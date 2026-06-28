"""
UDS ボタン送信ツール (GUI)

Cangaroo で都度ペイロードを手入力・FC を手動送信する代わりに、config.json に
定義した UDS コマンドをボタン1つで送信する。複数フレーム応答時の Flow
Control 送信、SecurityAccess の seed->key 計算も自動化する。

使い方:
    pip install -r requirements.txt
    python app.py [--config config.json]
"""
from __future__ import annotations

import argparse
import json
import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, scrolledtext, ttk

import can

import uds_link


def parse_payload(items) -> bytes:
    return bytes(int(x, 16) if isinstance(x, str) else int(x) for x in items)


class App(tk.Tk):
    def __init__(self, config_path: str):
        super().__init__()
        self.title("UDS Button Tester")
        self.geometry("900x600")

        with open(config_path, "r", encoding="utf-8") as f:
            self.cfg = json.load(f)

        self.bus: can.BusABC | None = None
        self.bus_lock = threading.Lock()
        self.log_queue: "queue.Queue[str]" = queue.Queue()
        self.state_queue: "queue.Queue[tuple[str, str]]" = queue.Queue()
        self.tester_present_stop = threading.Event()

        self._build_widgets()
        self.after(100, self._poll_queues)

    # ------------------------------------------------------------------
    # UI 構築
    # ------------------------------------------------------------------
    def _build_widgets(self):
        conn = ttk.LabelFrame(self, text="接続")
        conn.pack(fill="x", padx=8, pady=4)

        ttk.Label(conn, text="interface").grid(row=0, column=0, padx=4, pady=4)
        self.interface_var = tk.StringVar(value=self.cfg["can"]["interface"])
        ttk.Entry(conn, textvariable=self.interface_var, width=12).grid(row=0, column=1)

        ttk.Label(conn, text="channel").grid(row=0, column=2, padx=4)
        self.channel_var = tk.StringVar(value=str(self.cfg["can"]["channel"]))
        ttk.Entry(conn, textvariable=self.channel_var, width=8).grid(row=0, column=3)

        ttk.Label(conn, text="bitrate").grid(row=0, column=4, padx=4)
        self.bitrate_var = tk.StringVar(value=str(self.cfg["can"]["bitrate"]))
        ttk.Entry(conn, textvariable=self.bitrate_var, width=10).grid(row=0, column=5)

        self.connect_btn = ttk.Button(conn, text="Connect", command=self._toggle_connect)
        self.connect_btn.grid(row=0, column=6, padx=8)

        self.status_var = tk.StringVar(value="● Disconnected")
        self.status_label = ttk.Label(conn, textvariable=self.status_var, foreground="red")
        self.status_label.grid(row=0, column=7, padx=8)

        state = ttk.LabelFrame(self, text="トラッキング状態（本ツールが応答から推測した参考表示）")
        state.pack(fill="x", padx=8, pady=4)

        self.session_var = tk.StringVar(value="Session: unknown")
        ttk.Label(state, textvariable=self.session_var).grid(row=0, column=0, padx=8, pady=4)

        self.security_var = tk.StringVar(value="Security: unknown")
        ttk.Label(state, textvariable=self.security_var).grid(row=0, column=1, padx=8, pady=4)

        self.tester_present_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            state,
            text="Tester Present 自動送信 (2秒毎、S3タイマ維持)",
            variable=self.tester_present_var,
            command=self._toggle_tester_present,
        ).grid(row=0, column=2, padx=8, pady=4)

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True, padx=8, pady=4)

        btn_frame = ttk.LabelFrame(body, text="UDS コマンド")
        btn_frame.pack(side="left", fill="y", padx=(0, 8))

        cols = 2
        for i, btn_cfg in enumerate(self.cfg["buttons"]):
            b = ttk.Button(
                btn_frame,
                text=btn_cfg["label"],
                width=22,
                command=lambda c=btn_cfg: self._on_button_click(c),
            )
            b.grid(row=i // cols, column=i % cols, padx=4, pady=4, sticky="ew")

        log_frame = ttk.LabelFrame(body, text="ログ")
        log_frame.pack(side="left", fill="both", expand=True)

        self.log_text = scrolledtext.ScrolledText(
            log_frame, font=("Consolas", 10), state="disabled", wrap="word"
        )
        self.log_text.pack(fill="both", expand=True)

    # ------------------------------------------------------------------
    # 接続管理
    # ------------------------------------------------------------------
    def _toggle_connect(self):
        if self.bus is None:
            self._connect()
        else:
            self._disconnect()

    def _connect(self):
        channel_raw = self.channel_var.get()
        try:
            channel = int(channel_raw)
        except ValueError:
            channel = channel_raw  # COM ポート文字列等 (SLCAN 等)
        try:
            bus = uds_link.create_bus(
                self.interface_var.get(), channel, int(self.bitrate_var.get())
            )
        except Exception as exc:  # noqa: BLE001 - 接続失敗内容をそのままユーザーに見せる
            messagebox.showerror("接続失敗", str(exc))
            return
        self.bus = bus
        self.status_var.set("● Connected")
        self.status_label.configure(foreground="green")
        self.connect_btn.configure(text="Disconnect")
        self._log("接続しました")

    def _disconnect(self):
        self.tester_present_var.set(False)
        self.tester_present_stop.set()
        # python-can の gs_usb バックエンドは shutdown() 内部でデバイスの
        # 再スキャンを行うが、これを明示的に呼ぶと（特に複数回呼ばれた場合に）
        # libusb 側で access violation を起こすことを確認済み。
        # 参照を破棄するだけにし、後始末は BusABC.__del__ の best-effort
        # 処理（例外を抑制しつつ shutdown を1回だけ試みる）に委ねる。
        self.bus = None
        self.status_var.set("● Disconnected")
        self.status_label.configure(foreground="red")
        self.connect_btn.configure(text="Connect")
        self._log("切断しました")

    # ------------------------------------------------------------------
    # ボタン送信 (バックグラウンドスレッドで実行し、結果は queue 経由で GUI に反映)
    # ------------------------------------------------------------------
    def _on_button_click(self, btn_cfg):
        if self.bus is None:
            messagebox.showwarning("未接続", "先に Connect してください")
            return
        threading.Thread(target=self._send_worker, args=(btn_cfg,), daemon=True).start()

    def _send_worker(self, btn_cfg):
        label = btn_cfg["label"].replace("\n", " ")
        with self.bus_lock:
            try:
                if btn_cfg["type"] == "security_access_auto":
                    result = uds_link.security_access_auto(self.bus)
                    self.log_queue.put(f"[{label}] {result}")
                    if "成功" in result or "済み" in result:
                        self.state_queue.put(("security", "Security: Unlocked"))
                elif btn_cfg["type"] == "raw":
                    payload = parse_payload(btn_cfg["payload"])
                    self.log_queue.put(
                        f"[{label}] TX " + " ".join(f"{b:02X}" for b in payload)
                    )
                    uds_link.send_raw(self.bus, payload)
                    resp = uds_link.receive_uds_response(self.bus)
                    self.log_queue.put(f"[{label}] RX " + self._decode_response(payload, resp))
                    self._queue_tracking_update(payload, resp)
                else:
                    self.log_queue.put(f"[{label}] 未知のボタン種別: {btn_cfg['type']}")
            except uds_link.UdsTimeoutError as exc:
                self.log_queue.put(f"[{label}] {exc}")
            except Exception as exc:  # noqa: BLE001 - 想定外のエラーもログに出して継続する
                self.log_queue.put(f"[{label}] エラー: {exc}")

    def _decode_response(self, sent: bytes, resp: uds_link.UdsResponse) -> str:
        if resp.is_negative:
            return resp.describe()
        sid = sent[1] if len(sent) > 1 else None
        raw = resp.raw
        if sid == 0x22 and len(raw) >= 3 and raw[0] == 0x62:
            did = (raw[1] << 8) | raw[2]
            return uds_link.decode_did_value(did, raw[3:])
        if sid == 0x19 and len(raw) >= 2 and raw[0] == 0x59:
            return self._decode_dtc_response(raw)
        return resp.describe()

    def _decode_dtc_response(self, raw: bytes) -> str:
        sub = raw[1]
        if sub == 0x01 and len(raw) >= 3:
            return f"DTC count = {raw[2]}"
        if sub == 0x02:
            entries = []
            i = 2
            while i + 4 <= len(raw):
                dtc = (raw[i] << 16) | (raw[i + 1] << 8) | raw[i + 2]
                status = raw[i + 3]
                entries.append(f"{uds_link.dtc_name(dtc)} (status=0x{status:02X})")
                i += 4
            return "; ".join(entries) if entries else "(no DTC)"
        if sub == 0x04 and len(raw) >= 7:
            dtc = (raw[2] << 16) | (raw[3] << 8) | raw[4]
            data = " ".join(f"{b:02X}" for b in raw[6:])
            return f"FreezeFrame {uds_link.dtc_name(dtc)} record={raw[5]} data={data}"
        return " ".join(f"{b:02X}" for b in raw)

    def _queue_tracking_update(self, sent: bytes, resp: uds_link.UdsResponse):
        """Session/Security の参考表示を更新する。あくまで本ツールが送受信した
        フレームから推測したものであり、ECU 内部の正式な状態ではない
        （例: ECUReset は応答有無に関わらずリセットされるとみなして即時反映する）。"""
        if len(sent) < 2:
            return
        sid = sent[1]
        if sid == 0x11:
            self.state_queue.put(("session", "Session: Default (reset)"))
            self.state_queue.put(("security", "Security: Locked (reset)"))
            return
        if resp.is_negative:
            return
        if sid == 0x10 and len(resp.raw) >= 1 and resp.raw[0] == 0x50:
            sub = sent[2] if len(sent) > 2 else 0
            label = "Extended" if sub == 0x03 else "Default"
            self.state_queue.put(("session", f"Session: {label}"))

    # ------------------------------------------------------------------
    # Tester Present 自動送信
    # ------------------------------------------------------------------
    def _toggle_tester_present(self):
        if self.tester_present_var.get():
            self.tester_present_stop.clear()
            threading.Thread(target=self._tester_present_loop, daemon=True).start()
        else:
            self.tester_present_stop.set()

    def _tester_present_loop(self):
        while not self.tester_present_stop.wait(2.0):
            if self.bus is None:
                continue
            with self.bus_lock:
                try:
                    uds_link.send_raw(self.bus, bytes([0x02, 0x3E, 0x00]))
                    uds_link.receive_uds_response(self.bus, timeout=1.0)
                except Exception:  # noqa: BLE001 - バックグラウンド送信の失敗は致命的でない
                    pass

    # ------------------------------------------------------------------
    # ログ・状態表示の更新 (メインスレッドからのみ tkinter 変数を更新する)
    # ------------------------------------------------------------------
    def _log(self, text: str):
        self.log_queue.put(text)

    def _poll_queues(self):
        while True:
            try:
                text = self.log_queue.get_nowait()
            except queue.Empty:
                break
            self.log_text.configure(state="normal")
            self.log_text.insert("end", f"[{time.strftime('%H:%M:%S')}] {text}\n")
            self.log_text.see("end")
            self.log_text.configure(state="disabled")

        while True:
            try:
                kind, value = self.state_queue.get_nowait()
            except queue.Empty:
                break
            if kind == "session":
                self.session_var.set(value)
            elif kind == "security":
                self.security_var.set(value)

        self.after(100, self._poll_queues)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="config.json")
    args = parser.parse_args()
    App(args.config).mainloop()


if __name__ == "__main__":
    main()

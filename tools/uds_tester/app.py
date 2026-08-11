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
import gc
import json
import os
import queue
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

import can
from Crypto.Cipher import AES
from Crypto.Hash import CMAC

import capl_api
import capl_dsl
import uds_link


def parse_payload(items) -> bytes:
    return bytes(int(x, 16) if isinstance(x, str) else int(x) for x in items)


class App(tk.Tk):
    def __init__(self, config_path: str):
        super().__init__()
        self.title("UDS Button Tester")
        self.geometry("1100x650")

        with open(config_path, "r", encoding="utf-8") as f:
            self.cfg = json.load(f)

        self.bus: can.BusABC | None = None
        self.bus_lock = threading.Lock()
        self.log_queue: "queue.Queue[str]" = queue.Queue()
        self.state_queue: "queue.Queue[tuple]" = queue.Queue()
        self.tester_present_stop = threading.Event()
        self._periodic_stops: "dict[int, threading.Event]" = {}
        self._entry_vars: "dict[int, dict[str, tk.StringVar]]" = {}
        self._response_vars: "dict[int, tk.StringVar]" = {}
        self._periodic_btn_vars: "dict[int, tk.StringVar]" = {}
        self._log_visible = tk.BooleanVar(value=False)
        self._rx_monitor_vars: "dict[int, tk.StringVar]" = {}
        self._rx_monitor_ids: "dict[int, int]" = {}
        self._rx_monitor_decode: "dict[int, str]" = {}
        self._rx_monitor_secoc_verify: "dict[int, dict]" = {}
        self._rx_monitor_name_vars: "dict[int, tk.StringVar]" = {}
        self._rx_monitor_stop = threading.Event()
        self._rx_monitor_thread: "threading.Thread | None" = None
        self.script_stop_event = threading.Event()
        self._script_thread: threading.Thread | None = None
        # _rx_monitor_worker がバスの唯一の「常時ポーリングする」読み取り役であり、
        # 受信した全フレームをここにも流す (ファンアウト)。capl_dsl.py の on message は
        # 自前で bus.recv() せず、このキューから消費することで _rx_monitor_worker との
        # フレーム奪い合いを避ける。maxsize は誰も消費していない間に無制限に溜め込まない
        # ための保険 (満杯時は新しいフレームを黙って捨てる)。
        self._message_dispatch_queue: "queue.Queue" = queue.Queue(maxsize=64)

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

        ttk.Checkbutton(
            state,
            text="ログ",
            variable=self._log_visible,
            command=self._toggle_log,
        ).grid(row=0, column=3, padx=8, pady=4)

        meter = ttk.LabelFrame(
            self, text="仮想メータ表示（MeterStatus 0x200、実機に物理表示器が無いための可視化）")
        meter.pack(fill="x", padx=8, pady=4)

        ttk.Label(meter, text="RPM").grid(row=0, column=0, padx=8, pady=4)
        self.meter_rpm_bar = ttk.Progressbar(
            meter, orient="horizontal", length=300, maximum=8000, mode="determinate")
        self.meter_rpm_bar.grid(row=0, column=1, padx=4, pady=4)
        self.meter_rpm_var = tk.StringVar(value="---- rpm")
        ttk.Label(meter, textvariable=self.meter_rpm_var, width=10).grid(
            row=0, column=2, padx=8, pady=4)

        self.meter_run_var = tk.StringVar(value="RUN")
        self.meter_run_lbl = tk.Label(
            meter, textvariable=self.meter_run_var, width=8, relief="raised", bg="gray85")
        self.meter_run_lbl.grid(row=0, column=3, padx=4, pady=4)

        self.meter_fault_var = tk.StringVar(value="FAULT")
        self.meter_fault_lbl = tk.Label(
            meter, textvariable=self.meter_fault_var, width=8, relief="raised", bg="gray85")
        self.meter_fault_lbl.grid(row=0, column=4, padx=4, pady=4)

        self.meter_abs_var = tk.StringVar(value="ABS")
        self.meter_abs_lbl = tk.Label(
            meter, textvariable=self.meter_abs_var, width=8, relief="raised", bg="gray85")
        self.meter_abs_lbl.grid(row=0, column=5, padx=4, pady=4)

        script = ttk.LabelFrame(self, text="スクリプト (CAPL風)")
        script.pack(fill="x", padx=8, pady=4)

        ttk.Button(script, text="スクリプト実行...", command=self._open_script).grid(
            row=0, column=0, padx=8, pady=4)
        ttk.Button(script, text="停止", command=self._stop_script).grid(
            row=0, column=1, padx=8, pady=4)
        self.script_status_var = tk.StringVar(value="")
        ttk.Label(script, textvariable=self.script_status_var).grid(
            row=0, column=2, padx=8, pady=4, sticky="w")

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True, padx=8, pady=4)

        # ---- コマンドリスト (スクロール可能・1列) ----
        cmd_outer = ttk.LabelFrame(body, text="コマンド")
        cmd_outer.pack(side="left", fill="both", expand=True, padx=(0, 8))

        canvas = tk.Canvas(cmd_outer, width=500, highlightthickness=0)
        vsb = ttk.Scrollbar(cmd_outer, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=vsb.set)
        vsb.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)

        inner = ttk.Frame(canvas)
        win_id = canvas.create_window((0, 0), window=inner, anchor="nw")

        inner.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.bind("<Configure>", lambda e: canvas.itemconfig(win_id, width=e.width))

        def _scroll(event):
            canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        canvas.bind("<MouseWheel>", _scroll)
        inner.bind("<MouseWheel>", _scroll)

        # ヘッダ行
        ttk.Label(inner, text="コマンド", font=("", 9, "bold")).grid(
            row=0, column=0, padx=(4, 2), pady=(4, 1), sticky="w")
        ttk.Label(inner, text="CAN ID", font=("", 9, "bold")).grid(
            row=0, column=1, padx=(2, 4), pady=(4, 1), sticky="w")
        hdr_cell = ttk.Frame(inner)
        hdr_cell.grid(row=0, column=2, padx=(0, 4), pady=(4, 1), sticky="w")
        ttk.Label(hdr_cell, text="データ (hex)", font=("", 9, "bold"),
                  width=30, anchor="w").pack(side="left")
        ttk.Label(hdr_cell, text="説明", font=("", 9, "bold"),
                  width=40, anchor="w").pack(side="left", padx=(4, 0))
        ttk.Label(inner, text="送信", font=("", 9, "bold")).grid(
            row=0, column=3, padx=(4, 4), pady=(4, 1), sticky="w")
        ttk.Label(inner, text="定期", font=("", 9, "bold")).grid(
            row=0, column=4, padx=(4, 4), pady=(4, 1), sticky="w")
        ttk.Separator(inner, orient="horizontal").grid(
            row=1, column=0, columnspan=5, sticky="ew", padx=4, pady=(0, 2))

        def _hex_str(items) -> str:
            return " ".join(
                f"{int(x, 16) if isinstance(x, str) else int(x):02X}" for x in items
            )

        current_row = 2
        for i, btn_cfg in enumerate(self.cfg["buttons"]):
            row = current_row
            t = btn_cfg.get("type")

            if t == "group_header":
                sep = ttk.Label(inner, text=f"── {btn_cfg['label']} {'─' * 40}",
                                font=("", 9, "bold"), foreground="#555555", anchor="w")
                sep.grid(row=row, column=0, columnspan=5,
                         padx=(4, 4), pady=(6, 2), sticky="ew")
                sep.bind("<MouseWheel>", _scroll)
                current_row += 1
                continue

            if t == "rx_monitor":
                raw_id = btn_cfg.get("can_id", "0x000")
                can_id_int = int(raw_id, 0) if isinstance(raw_id, str) else int(raw_id)
                cmd_lbl = ttk.Label(inner, text=btn_cfg["label"], width=20,
                                    anchor="w", justify="left")
                cmd_lbl.grid(row=row, column=0, padx=(4, 2), pady=2, sticky="nsew")
                cmd_lbl.bind("<MouseWheel>", _scroll)
                id_lbl = ttk.Label(inner, text=f"0x{can_id_int:03X}",
                                   font=("Consolas", 9), foreground="#2a7a2a")
                id_lbl.grid(row=row, column=1, padx=(2, 6), pady=2, sticky="w")
                id_lbl.bind("<MouseWheel>", _scroll)
                rx_var = tk.StringVar(value="")
                name_var = tk.StringVar(value="")
                cell = ttk.Frame(inner)
                cell.grid(row=row, column=2, columnspan=3,
                          padx=(0, 4), pady=2, sticky="ew")
                cell.bind("<MouseWheel>", _scroll)
                rx_lbl = ttk.Label(cell, textvariable=rx_var,
                                   font=("Consolas", 9), foreground="#2a7a2a",
                                   anchor="w", width=30)
                rx_lbl.pack(side="left")
                rx_lbl.bind("<MouseWheel>", _scroll)
                name_lbl = ttk.Label(cell, textvariable=name_var,
                                     font=("", 9), foreground="#2a7a2a",
                                     anchor="w", width=40)
                name_lbl.pack(side="left", padx=(4, 0))
                name_lbl.bind("<MouseWheel>", _scroll)
                self._rx_monitor_vars[i] = rx_var
                self._rx_monitor_name_vars[i] = name_var
                self._rx_monitor_ids[i] = can_id_int
                if btn_cfg.get("decode"):
                    self._rx_monitor_decode[i] = btn_cfg["decode"]
                if btn_cfg.get("secoc_verify"):
                    self._rx_monitor_secoc_verify[i] = btn_cfg["secoc_verify"]
                current_row += 1
                continue

            is_uds = t in ("raw", "multiframe", "security_access_auto",
                           "security_seed", "security_key")

            # ---- 説明ラベル (col 0) ----
            rs = 2 if is_uds else 1
            cmd_lbl = ttk.Label(inner, text=btn_cfg["label"], width=20,
                                anchor="w", justify="left")
            cmd_lbl.grid(row=row, column=0, rowspan=rs,
                         padx=(4, 2), pady=2, sticky="nsew")
            cmd_lbl.bind("<MouseWheel>", _scroll)

            if is_uds:
                # TX 行 (上段): CAN ID=0x7E0 + 送信データ(編集可)
                tx_id = ttk.Label(inner, text="0x7E0", font=("Consolas", 9),
                                  foreground="#555555")
                tx_id.grid(row=row, column=1, padx=(2, 6), pady=(3, 1), sticky="w")
                tx_id.bind("<MouseWheel>", _scroll)

                if t == "security_access_auto":
                    tx_data = ttk.Label(inner, text="(seed→key 自動計算)",
                                        font=("Consolas", 9))
                    tx_data.grid(row=row, column=2, padx=(0, 4), pady=(3, 1), sticky="w")
                    tx_data.bind("<MouseWheel>", _scroll)
                else:
                    default_hex = _hex_str(btn_cfg.get("payload", []))
                    data_var = tk.StringVar(value=default_hex)
                    presets = btn_cfg.get("presets", [])
                    cell = ttk.Frame(inner)
                    cell.grid(row=row, column=2, padx=(0, 4), pady=(3, 1), sticky="w")
                    cell.bind("<MouseWheel>", _scroll)
                    data_entry = ttk.Entry(cell, textvariable=data_var, width=30,
                                           font=("Consolas", 9))
                    data_entry.pack(side="left")
                    data_entry.bind("<MouseWheel>", _scroll)
                    combo_vals = [p["label"] for p in presets] if presets else ["-"]
                    combo = ttk.Combobox(cell, values=combo_vals,
                                         state="readonly", width=40, font=("", 9))
                    combo.pack(side="left", padx=(4, 0))
                    combo.bind("<MouseWheel>", _scroll)
                    if presets:
                        def _on_preset(event, var=data_var, ps=presets, cb=combo):
                            sel = cb.current()
                            if sel >= 0:
                                vals = ps[sel].get("payload") or ps[sel].get("data") or []
                                var.set(_hex_str(vals))
                            cb.selection_clear()
                        combo.bind("<<ComboboxSelected>>", _on_preset)
                    self._entry_vars.setdefault(i, {})["data"] = data_var

                # RX 行 (下段): CAN ID=0x7E8 + 受信データ(自動更新)
                rx_id = ttk.Label(inner, text="0x7E8", font=("Consolas", 9),
                                  foreground="#3a7ebf")
                rx_id.grid(row=row + 1, column=1, padx=(2, 6), pady=(1, 3), sticky="w")
                rx_id.bind("<MouseWheel>", _scroll)

                resp_var = tk.StringVar(value="")
                resp_lbl = ttk.Label(inner, textvariable=resp_var,
                                     font=("Consolas", 9), foreground="#3a7ebf",
                                     anchor="w")
                resp_lbl.grid(row=row + 1, column=2, padx=(0, 4), pady=(1, 3), sticky="ew")
                resp_lbl.bind("<MouseWheel>", _scroll)
                self._response_vars[i] = resp_var

                # 送信ボタン (col 3, rowspan=2)
                send_btn = ttk.Button(
                    inner, text="送信", width=5,
                    command=lambda c=btn_cfg, idx=i: self._on_send_click(c, idx),
                )
                send_btn.grid(row=row, column=3, rowspan=2,
                              padx=(4, 4), pady=2, sticky="nsew")
                send_btn.bind("<MouseWheel>", _scroll)

                current_row += 2
            else:
                # can_frame: 1行のみ
                raw_id = btn_cfg.get("can_id", "0x000")
                can_id_val = int(raw_id, 0) if isinstance(raw_id, str) else int(raw_id)
                can_id_var = tk.StringVar(value=f"0x{can_id_val:03X}")
                id_widget = ttk.Entry(inner, textvariable=can_id_var, width=9,
                                      font=("Consolas", 9))
                id_widget.grid(row=row, column=1, padx=(2, 6), pady=2, sticky="w")
                id_widget.bind("<MouseWheel>", _scroll)
                self._entry_vars.setdefault(i, {})["can_id"] = can_id_var

                _e2e_cfg_btn = btn_cfg.get("e2e")
                _secoc_cfg_btn = btn_cfg.get("secoc")
                if _e2e_cfg_btn:
                    _raw_bytes = parse_payload(btn_cfg.get("data", []))
                    default_hex = " ".join(
                        f"{b:02X}" for b in App._apply_e2e(_raw_bytes, _e2e_cfg_btn, 0)
                    )
                elif _secoc_cfg_btn:
                    _raw_bytes = parse_payload(btn_cfg.get("data", []))
                    default_hex = " ".join(
                        f"{b:02X}" for b in App._apply_secoc(_raw_bytes, _secoc_cfg_btn, 0)
                    )
                else:
                    default_hex = _hex_str(btn_cfg.get("data", []))
                data_var = tk.StringVar(value=default_hex)
                presets = btn_cfg.get("presets", [])
                cell = ttk.Frame(inner)
                cell.grid(row=row, column=2, padx=(0, 4), pady=2, sticky="w")
                cell.bind("<MouseWheel>", _scroll)
                data_entry = ttk.Entry(cell, textvariable=data_var, width=30,
                                       font=("Consolas", 9))
                data_entry.pack(side="left")
                data_entry.bind("<MouseWheel>", _scroll)
                combo_vals = [p["label"] for p in presets] if presets else ["-"]
                combo = ttk.Combobox(cell, values=combo_vals,
                                     state="readonly", width=40, font=("", 9))
                combo.pack(side="left", padx=(4, 0))
                combo.bind("<MouseWheel>", _scroll)
                if presets:
                    def _on_preset(event, var=data_var, ps=presets, cb=combo,
                                   ecfg=_e2e_cfg_btn, scfg=_secoc_cfg_btn):
                        sel = cb.current()
                        if sel >= 0:
                            vals = ps[sel].get("data") or ps[sel].get("payload") or []
                            # カウンタ/フレッシュネスは Entry に現在入っている値
                            # （直前の送信で自動的に進んだ値、または初期値）から
                            # 引き継ぐ。ここで 0 に決め打ちすると、プリセットを
                            # 切り替えるだけで（例: UNLOCK→LOCK）まだ使っていない
                            # はずの値がリプレイ扱いされてしまう
                            # （SecOC の単調増加チェック、E2E の Counter 不整合検知
                            # いずれも直前値との連続性を見ているため）。
                            try:
                                cur = App._parse_hex_bytes(var.get())
                            except ValueError:
                                cur = b""
                            if ecfg:
                                counter = App._read_e2e_counter(cur, ecfg)
                                _pb = parse_payload(vals)
                                var.set(" ".join(
                                    f"{b:02X}" for b in App._apply_e2e(_pb, ecfg, counter)
                                ))
                            elif scfg:
                                fo = scfg["freshness_offset"]
                                freshness = cur[fo] if len(cur) > fo else 0
                                _pb = parse_payload(vals)
                                var.set(" ".join(
                                    f"{b:02X}" for b in App._apply_secoc(_pb, scfg, freshness)
                                ))
                            else:
                                var.set(_hex_str(vals))
                        cb.selection_clear()
                    combo.bind("<<ComboboxSelected>>", _on_preset)
                self._entry_vars.setdefault(i, {})["data"] = data_var

                # 送信ボタン (col 3)
                send_btn = ttk.Button(
                    inner, text="送信", width=5,
                    command=lambda c=btn_cfg, idx=i: self._on_send_click(c, idx),
                )
                send_btn.grid(row=row, column=3, padx=(4, 2), pady=2)
                send_btn.bind("<MouseWheel>", _scroll)

                # 定期送信ボタン (col 4)
                periodic_var = tk.StringVar(value="定期")
                periodic_btn = ttk.Button(
                    inner, textvariable=periodic_var, width=5,
                    command=lambda c=btn_cfg, idx=i: self._on_periodic_click(c, idx),
                )
                periodic_btn.grid(row=row, column=4, padx=(2, 4), pady=2)
                periodic_btn.bind("<MouseWheel>", _scroll)
                self._periodic_btn_vars[i] = periodic_var

                current_row += 1
        # ---- ログ ----
        self.log_frame = ttk.LabelFrame(body, text="ログ")
        # initially hidden; shown by _toggle_log when checkbox is checked

        self.log_text = scrolledtext.ScrolledText(
            self.log_frame, font=("Consolas", 10), state="disabled", wrap="word"
        )
        self.log_text.pack(fill="both", expand=True)

    def _toggle_log(self):
        if self._log_visible.get():
            self.log_frame.pack(side="left", fill="both", expand=True)
        else:
            self.log_frame.pack_forget()

    # ------------------------------------------------------------------
    # E2E P01/P05 送信サポート
    #
    # config.json の "e2e" ブロックに "profile": "p05" があれば Profile05
    # (CRC16+8bitカウンタ)、無ければ既定で Profile01 (CRC8+4bitカウンタ) を
    # 適用する。EngineInfo/AbsInfo は Profile05 へ移行済み
    # (src/Bsw/E2EXf/E2EXf_PBCfg.c 参照)。
    # ------------------------------------------------------------------
    @staticmethod
    def _crc8_sae_j1850(data: bytes) -> int:
        """CRC8 SAE J1850 (poly=0x1D, init=0x00, finalXOR=0x00)。
        AUTOSAR SWS_E2E_00083 は開始値・最終XOR値とも 0x00 と規定している
        (E2E_P01.c と同一のアルゴリズム。詳細はそちらのファイル冒頭コメント参照)。"""
        crc = 0x00
        for b in data:
            crc ^= b
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1D) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
        return crc

    @staticmethod
    def _crc16_e2e_p05(data: bytes, crc: int = 0xFFFF) -> int:
        """CRC16 (poly=0x1021, MSB-first、開始値0xFFFF、最終補正なし)。
        SWS_E2E_00406 の擬似コードと同一のアルゴリズム
        (E2E_P05.c の E2E_CalcCrc16() 参照)。"""
        for b in data:
            crc ^= (b << 8) & 0xFFFF
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
        return crc

    @staticmethod
    def _next_e2e_counter(counter: int, e2e_cfg: dict | None = None) -> int:
        """E2E Counter の次回送信値を返す。
        Profile05 (e2e_cfg["profile"]=="p05") は 8bit フルレンジ、予約値なしの
        単純なラップアラウンド。Profile01 (既定) は SWS_E2E_00075 により
        14 (0xE) に達したら次は 0 に戻る（15=0xF はスキップ、予約値）。
        単純な mod-16 (`(counter + 1) & 0x0F`) では 15 を経由してしまい仕様違反になる。"""
        if e2e_cfg and e2e_cfg.get("profile") == "p05":
            return (counter + 1) & 0xFF
        return 0 if counter >= 14 else counter + 1

    @staticmethod
    def _read_e2e_counter(frame: bytes, e2e_cfg: dict) -> int:
        """既存フレームから現在の E2E Counter 値を読み出す。
        Profile01 は counter_offset バイトの下位4bitのみ、Profile05 は
        counter_offset バイト全体がカウンタ値（フル0-255）。"""
        co = e2e_cfg["counter_offset"]
        if len(frame) <= co:
            return 0
        if e2e_cfg.get("profile") == "p05":
            return frame[co]
        return frame[co] & 0x0F

    @staticmethod
    def _apply_e2e_p05(payload: bytes, e2e_cfg: dict, counter: int) -> bytes:
        """E2E Profile05 保護バイト (Counter 1byte フル値 + CRC16 2byte LE) を
        付加した完全フレームを返す。payload はシグナルバイト列のみ
        (Counter/CRC 未付加)。

        frame_length / counter_offset / crc_offset / payload_offset は
        _apply_e2e() と同じ意味（付加後の完全フレーム上の位置）。CRC16 は
        Counter を含みユーザーデータ末尾までの範囲 → DataID（下位→上位バイト
        の順）の順で計算する（Profile01 と異なり DataID は「データの後」に
        投入する。E2E_P05.c の E2E_CalcCrc16Body() と同一のアルゴリズム）。"""
        data_id: int = e2e_cfg["data_id"]
        frame_length: int = e2e_cfg["frame_length"]
        counter_offset: int = e2e_cfg["counter_offset"]
        crc_offset: int = e2e_cfg["crc_offset"]
        payload_offset: int = e2e_cfg["payload_offset"]
        frame = bytearray(frame_length)
        frame[payload_offset:payload_offset + len(payload)] = payload
        frame[counter_offset] = counter & 0xFF
        crc_input = bytes(frame[counter_offset:])
        crc_input += bytes([data_id & 0xFF, (data_id >> 8) & 0xFF])
        crc = App._crc16_e2e_p05(crc_input)
        frame[crc_offset] = crc & 0xFF
        frame[crc_offset + 1] = (crc >> 8) & 0xFF
        return bytes(frame)

    @staticmethod
    def _apply_e2e(payload: bytes, e2e_cfg: dict, counter: int) -> bytes:
        """E2E 保護バイトを付加した完全フレームを返す。e2e_cfg["profile"]=="p05" なら
        _apply_e2e_p05() (Profile05: CRC16+8bitカウンタ) へ委譲し、それ以外
        (既定) は以下の Profile01 (CRC8+4bitカウンタ) を適用する。

        Profile01: payload はシグナルバイト列のみ (Counter/CRC 未付加)。
        frame_length / counter_offset / crc_offset / payload_offset は
        付加後の完全フレーム上の位置（AUTOSAR 標準バリアント1A、SWS_E2E_00227:
        CRC=byte0, Counter=byte1 下位4bit。シグナルは payload_offset から）。
        CRC は DataID に続けて、CRC バイト自身を除く全バイト（CRC より前 + 後の
        2 区間）を対象に計算する（E2E_P01.c の実装と同一のアルゴリズム）。"""
        if e2e_cfg.get("profile") == "p05":
            return App._apply_e2e_p05(payload, e2e_cfg, counter)
        data_id: int = e2e_cfg["data_id"]
        frame_length: int = e2e_cfg["frame_length"]
        counter_offset: int = e2e_cfg["counter_offset"]
        crc_offset: int = e2e_cfg["crc_offset"]
        payload_offset: int = e2e_cfg["payload_offset"]
        frame = bytearray(frame_length)
        frame[payload_offset:payload_offset + len(payload)] = payload
        frame[counter_offset] = (frame[counter_offset] & 0xF0) | (counter & 0x0F)
        crc_input = bytearray([data_id & 0xFF, (data_id >> 8) & 0xFF])
        crc_input += frame[:crc_offset]
        crc_input += frame[crc_offset + 1:]
        frame[crc_offset] = App._crc8_sae_j1850(bytes(crc_input))
        return bytes(frame)

    # ------------------------------------------------------------------
    # SecOC (Secure Onboard Communication) 送信サポート
    # ------------------------------------------------------------------
    @staticmethod
    def _secoc_cmac_truncated(key: bytes, data: bytes, trunc_len: int) -> bytes:
        """AES-128-CMAC (NIST SP 800-38B) を計算し、上位 trunc_len バイトを返す。
        pycryptodome の実装を使う（Arduino 側は src/Bsw/SecOC/SecOC_Cmac.c に
        自前実装があり、RFC 4493 の公式テストベクタで本実装と一致することを
        開発時に確認済み。詳細は README.md の「SecOC」節を参照）。"""
        mac = CMAC.new(key, ciphermod=AES)
        mac.update(data)
        return mac.digest()[:trunc_len]

    @staticmethod
    def _apply_secoc(payload: bytes, secoc_cfg: dict, freshness: int) -> bytes:
        """SecOC Profile 1 (24Bit-CMAC-8Bit-FV) 保護バイト
        (Freshness Value + 切り詰め MAC) を付加した Secured I-PDU を返す。
        payload は Authentic Payload のみ（Freshness/MAC 未付加）。

        DataToAuthenticator = DataId(2byte, Big Endian) | Authentic Payload |
        Complete Freshness Value
        (docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf [7.1.1.2]、
        Big Endian は [SWS_SecOC_00011]。Arduino 側の SecOC_IfRxIndication()
        と同一のアルゴリズム)。

        secoc_cfg キー: data_id, frame_length（Secured I-PDU 全体長）,
        auth_len（Authentic Payload 長）, freshness_offset, mac_offset,
        mac_len（切り詰め MAC 長、Profile 1 は 3）, key（32桁 hex 文字列、
        16 バイト AES-128 鍵）。"""
        data_id: int = secoc_cfg["data_id"]
        frame_length: int = secoc_cfg["frame_length"]
        auth_len: int = secoc_cfg["auth_len"]
        freshness_offset: int = secoc_cfg["freshness_offset"]
        mac_offset: int = secoc_cfg["mac_offset"]
        mac_len: int = secoc_cfg["mac_len"]
        key = bytes.fromhex(secoc_cfg["key"])

        frame = bytearray(frame_length)
        frame[0:auth_len] = payload[:auth_len]
        frame[freshness_offset] = freshness & 0xFF

        auth_input = bytearray([(data_id >> 8) & 0xFF, data_id & 0xFF])
        auth_input += frame[0:auth_len]
        auth_input += bytes([frame[freshness_offset]])

        mac = App._secoc_cmac_truncated(key, bytes(auth_input), mac_len)
        frame[mac_offset:mac_offset + mac_len] = mac
        return bytes(frame)

    @staticmethod
    def _verify_secoc(data: bytes, secoc_cfg: dict) -> bool:
        """受信した Secured I-PDU の MAC を検証する（_apply_secoc の逆方向）。
        Arduino 側 SecOC_MainFunction() が計算した MAC と、pycryptodome で
        独立に再計算した MAC が一致するかを確認する（Arduino の自前 AES-CMAC
        実装が TX 方向でも正しく動作していることの実機確認手段）。"""
        data_id: int = secoc_cfg["data_id"]
        auth_len: int = secoc_cfg["auth_len"]
        freshness_offset: int = secoc_cfg["freshness_offset"]
        mac_offset: int = secoc_cfg["mac_offset"]
        mac_len: int = secoc_cfg["mac_len"]
        key = bytes.fromhex(secoc_cfg["key"])
        if len(data) < mac_offset + mac_len:
            return False
        auth_input = bytearray([(data_id >> 8) & 0xFF, data_id & 0xFF])
        auth_input += data[0:auth_len]
        auth_input += bytes([data[freshness_offset]])
        expected = App._secoc_cmac_truncated(key, bytes(auth_input), mac_len)
        return expected == data[mac_offset:mac_offset + mac_len]

    @staticmethod
    def _btn_meta(btn_cfg) -> tuple[str, str]:
        """ボタン設定から (CAN ID 文字列, データ hex 文字列) を返す。ヘッダ列の表示用。"""
        def _hex_list(items) -> str:
            return " ".join(
                f"{int(x, 16) if isinstance(x, str) else int(x):02X}" for x in items
            )

        t = btn_cfg.get("type")
        if t == "can_frame":
            raw_id = btn_cfg.get("can_id", "?")
            can_id = int(raw_id, 0) if isinstance(raw_id, str) else int(raw_id)
            data_str = _hex_list(btn_cfg.get("data", []))
            interval = btn_cfg.get("interval_ms")
            if interval:
                data_str += f"  [{interval}ms周期]"
            return f"0x{can_id:03X}", data_str
        if t in ("raw", "multiframe"):
            return "0x7E0→7E8", _hex_list(btn_cfg.get("payload", []))
        if t == "security_access_auto":
            return "0x7E0→7E8", "(seed→key 自動計算)"
        return "", ""

    @staticmethod
    def _parse_hex_bytes(text: str) -> bytes:
        """スペース区切り hex 文字列をバイト列に変換する。失敗時は ValueError。"""
        tokens = text.strip().split()
        if not tokens:
            return b""
        return bytes(int(tok, 16) for tok in tokens)

    @staticmethod
    def _parse_can_id(text: str) -> int:
        """0x プレフィックス付き/なし hex または 10進数文字列を整数に変換する。"""
        return int(text.strip(), 0)

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
        self._rx_monitor_stop.clear()
        self._rx_monitor_thread = threading.Thread(
            target=self._rx_monitor_worker,
            args=(self._rx_monitor_stop,), daemon=True)
        self._rx_monitor_thread.start()

    def _disconnect(self):
        self.script_stop_event.set()
        if self._script_thread is not None and self._script_thread.is_alive():
            # スクリプトが送受信のブロッキング呼び出し中 (デフォルト timeout=2s 等) だと
            # stop_event はすぐには効かないため、その呼び出しが終わって次の
            # _check_stop() に到達するまで待つ。ここで待たずに bus を None にすると、
            # スクリプト側が古い bus への呼び出しを続けたまま新しい Bus オブジェクトが
            # 生成され、同一デバイスへの二重オープンが起こり得る。
            self._script_thread.join(timeout=3.0)
            if self._script_thread.is_alive():
                self.log_queue.put("[script] 停止待ちタイムアウト (バックグラウンドで終了処理中)")
        self.tester_present_var.set(False)
        self.tester_present_stop.set()
        for pidx, stop_ev in self._periodic_stops.items():
            stop_ev.set()
            if pidx in self._periodic_btn_vars:
                self._periodic_btn_vars[pidx].set("定期")
        self._periodic_stops.clear()
        # python-can の gs_usb バックエンドは shutdown() 内部でデバイスの
        # 再スキャンを行うが、これを明示的に呼ぶと（特に複数回呼ばれた場合に）
        # libusb 側で access violation を起こすことを確認済み。
        # 参照を破棄するだけにし、後始末は BusABC.__del__ の best-effort
        # 処理（例外を抑制しつつ shutdown を1回だけ試みる）に委ねる。
        self._rx_monitor_stop.set()
        # _rx_monitor_worker はループ先頭で bus = self.bus をローカル変数に
        # キャプチャしてから bus.recv(timeout=0.05) をブロッキング呼び出しする
        # ため、self.bus = None にした直後でもワーカースレッドがまだ古い
        # GsUsbBus を参照し続けている可能性がある。その状態で gc.collect() を
        # 呼んでも古いオブジェクトは回収されない（参照が生きているため）ので、
        # _script_thread と同様にワーカースレッドの終了を待ってから
        # bus 参照を破棄する。
        if self._rx_monitor_thread is not None and self._rx_monitor_thread.is_alive():
            self._rx_monitor_thread.join(timeout=1.0)
            if self._rx_monitor_thread.is_alive():
                self.log_queue.put("[RXモニタ] 停止待ちタイムアウト (バックグラウンドで終了処理中)")
        self._rx_monitor_thread = None
        self.bus = None
        # 参照を破棄しただけでは GC 実行タイミングが不定で、古い GsUsbBus が
        # USB デバイスを掴んだままの状態が続くことがある（すぐ Connect
        # し直すと「Entity not found」で失敗する原因になりうる）。shutdown()
        # を直接呼ぶのは上記コメントの通り access violation リスクがあるため
        # 避け、代わりに gc.collect() で GC を即座に走らせることで、
        # BusABC.__del__ の best-effort shutdown をこの場で確定的に
        # 発生させる。ワーカースレッドを join 済みなので、ここに来た時点で
        # 古い bus への参照はこのメソッドのローカル変数以外に残っていない。
        gc.collect()
        self.status_var.set("● Disconnected")
        self.status_label.configure(foreground="red")
        self.connect_btn.configure(text="Connect")
        self._log("切断しました")

    # ------------------------------------------------------------------
    # ボタン送信 (バックグラウンドスレッドで実行し、結果は queue 経由で GUI に反映)
    # ------------------------------------------------------------------
    def _require_connected(self) -> bool:
        """未接続なら警告を出して False を返す。GUI（メイン）スレッドの
        クリックハンドラからのみ呼ぶこと（messagebox はメインスレッド専用）。"""
        if self.bus is None:
            messagebox.showwarning("未接続", "先に Connect してください")
            return False
        return True

    def _on_send_click(self, btn_cfg, idx: int):
        if not self._require_connected():
            return
        entry_data = {k: v.get() for k, v in self._entry_vars.get(idx, {}).items()}
        threading.Thread(
            target=self._send_worker, args=(btn_cfg, entry_data, idx), daemon=True
        ).start()

    def _on_periodic_click(self, btn_cfg, idx: int):
        if not self._require_connected():
            return
        entry_data = {k: v.get() for k, v in self._entry_vars.get(idx, {}).items()}
        label = btn_cfg["label"].replace("\n", " ")
        threading.Thread(
            target=self._handle_periodic_can_toggle,
            args=(btn_cfg, idx, label, entry_data),
            daemon=True,
        ).start()

    def _send_worker(self, btn_cfg, entry_data: dict, idx: int):
        """entry_data: GUI スレッドで読み取った入力フィールドの文字列 {"data": "...", "can_id": "..."}"""
        label = btn_cfg["label"].replace("\n", " ")

        # エントリからバイト列 / CAN ID を取得するヘルパー (パース失敗でログして終了)
        def get_payload(cfg_key: str) -> bytes | None:
            if "data" in entry_data:
                try:
                    return self._parse_hex_bytes(entry_data["data"])
                except ValueError:
                    self.log_queue.put(
                        f"[{label}] データ形式エラー (スペース区切り hex 例: 02 10 01)"
                    )
                    return None
            return parse_payload(btn_cfg.get(cfg_key, []))

        def get_can_id() -> int | None:
            if "can_id" in entry_data:
                try:
                    return self._parse_can_id(entry_data["can_id"])
                except ValueError:
                    self.log_queue.put(
                        f"[{label}] CAN ID 形式エラー (例: 0x100 または 256)"
                    )
                    return None
            raw = btn_cfg.get("can_id", "0")
            return int(raw, 0) if isinstance(raw, str) else int(raw)

        with self.bus_lock:
            try:
                if btn_cfg["type"] == "security_access_auto":
                    result = uds_link.security_access_auto(self.bus)
                    self.log_queue.put(f"[{label}] {result}")
                    self.state_queue.put(("resp", (idx, result)))
                    if "成功" in result or "済み" in result:
                        self.state_queue.put(("security", "Security: Unlocked"))
                elif btn_cfg["type"] == "security_seed":
                    payload = get_payload("payload")
                    if payload is None:
                        return
                    self.state_queue.put(("resp", (idx, "")))
                    self.log_queue.put(
                        f"[{label}] TX " + " ".join(f"{b:02X}" for b in payload)
                    )
                    uds_link.send_raw(self.bus, payload)
                    resp = uds_link.receive_uds_response(self.bus)
                    self.state_queue.put(("resp", (idx, self._rx_display(resp))))
                    if (not resp.is_negative and len(resp.raw) >= 4
                            and resp.raw[0] == 0x67):
                        seed = (resp.raw[2] << 8) | resp.raw[3]
                        if seed == 0:
                            self.log_queue.put(f"[{label}] RX {self._rx_display(resp)}  既にアンロック済み (allZeroSeed)")
                        else:
                            key = seed ^ uds_link.SECURITY_KEY_MASK
                            key_hex = (f"04 27 02 {(key >> 8) & 0xFF:02X}"
                                       f" {key & 0xFF:02X}")
                            key_idx = next(
                                (j for j, c in enumerate(self.cfg["buttons"])
                                 if c.get("type") == "security_key"),
                                None,
                            )
                            if key_idx is not None:
                                self.state_queue.put(("key_fill", (key_idx, key_hex)))
                            self.log_queue.put(
                                f"[{label}] RX {self._rx_display(resp)}"
                                f"  seed=0x{seed:04X} key=0x{key:04X}"
                                + (" → Step2 に入力済み" if key_idx is not None else "")
                            )
                    else:
                        self.log_queue.put(f"[{label}] RX " + self._decode_response(payload, resp))
                elif btn_cfg["type"] in ("raw", "security_key"):
                    payload = get_payload("payload")
                    if payload is None:
                        return
                    self.state_queue.put(("resp", (idx, "")))
                    self.log_queue.put(
                        f"[{label}] TX " + " ".join(f"{b:02X}" for b in payload)
                    )
                    uds_link.send_raw(self.bus, payload)
                    resp = uds_link.receive_uds_response(self.bus)
                    decoded = self._decode_response(payload, resp)
                    self.log_queue.put(f"[{label}] RX " + decoded)
                    self.state_queue.put(("resp", (idx, self._rx_display(resp))))
                    self._queue_tracking_update(payload, resp)
                    if (btn_cfg["type"] == "security_key"
                            and not resp.is_negative
                            and len(resp.raw) >= 2 and resp.raw[0] == 0x67):
                        self.state_queue.put(("security", "Security: Unlocked"))
                elif btn_cfg["type"] == "multiframe":
                    uds_payload = get_payload("payload")
                    if uds_payload is None:
                        return
                    self.state_queue.put(("resp", (idx, "")))
                    self.log_queue.put(
                        f"[{label}] TX (FF+CF, {len(uds_payload)}B) "
                        + " ".join(f"{b:02X}" for b in uds_payload)
                    )
                    uds_link.send_multiframe_request(self.bus, uds_payload)
                    resp = uds_link.receive_uds_response(self.bus)
                    sent = bytes([0]) + uds_payload  # _decode_response は sent[1]=SID を見る
                    decoded = self._decode_response(sent, resp)
                    self.log_queue.put(f"[{label}] RX " + decoded)
                    self.state_queue.put(("resp", (idx, self._rx_display(resp))))
                    self._queue_tracking_update(sent, resp)
                elif btn_cfg["type"] == "can_frame":
                    can_id = get_can_id()
                    data = get_payload("data")
                    if can_id is None or data is None:
                        return
                    # E2E 付きの場合でも、送信するバイト列は Entry の内容をそのまま使う
                    # （CRC・Counter いずれも再計算しない）。Counter だけでなく CRC も
                    # 手入力した値をそのまま送れないと、意図的に不正な CRC を送って
                    # WRONGCRC 挙動を検証することができない
                    # （以前は CRC のみ常に再計算しており、手入力した不正な CRC が
                    # 送信直前に正しい値へ上書きされてしまうバグがあった）。
                    e2e_cfg = btn_cfg.get("e2e")
                    secoc_cfg = btn_cfg.get("secoc")
                    uds_link.send_can_frame(self.bus, can_id, data)
                    self.log_queue.put(
                        f"[{label}] TX ID=0x{can_id:03X} " + " ".join(f"{b:02X}" for b in data)
                    )
                    # E2E 付きの場合、次回送信に備えて Counter を進め、CRC を再計算した
                    # 「正常なフレーム」を Entry へ書き戻す（今回実際に送信した内容
                    # そのものには影響しない）。SecOC も同様に Freshness Value を
                    # 進めて MAC を再計算する（意図的に不正な MAC・古い Freshness を
                    # 手入力して検証失敗・リプレイ検知を試すことも今回の送信内容には
                    # 影響しない）。
                    if e2e_cfg:
                        po = e2e_cfg["payload_offset"]
                        next_counter = App._next_e2e_counter(App._read_e2e_counter(data, e2e_cfg), e2e_cfg)
                        next_frame = self._apply_e2e(data[po:], e2e_cfg, next_counter)
                        self.state_queue.put(
                            ("entry_update", (idx, " ".join(f"{b:02X}" for b in next_frame)))
                        )
                    elif secoc_cfg:
                        fo = secoc_cfg["freshness_offset"]
                        al = secoc_cfg["auth_len"]
                        next_freshness = ((data[fo] + 1) & 0xFF) if len(data) > fo else 0
                        next_frame = self._apply_secoc(data[:al], secoc_cfg, next_freshness)
                        self.state_queue.put(
                            ("entry_update", (idx, " ".join(f"{b:02X}" for b in next_frame)))
                        )
                else:
                    self.log_queue.put(f"[{label}] 未知のボタン種別: {btn_cfg['type']}")
            except uds_link.UdsTimeoutError as exc:
                self.log_queue.put(f"[{label}] {exc}")
                if btn_cfg.get("type") in ("raw", "multiframe", "security_access_auto",
                                            "security_seed", "security_key"):
                    self.state_queue.put(("resp", (idx, "タイムアウト")))
            except Exception as exc:  # noqa: BLE001 - 想定外のエラーもログに出して継続する
                self.log_queue.put(f"[{label}] エラー: {exc}")

    @staticmethod
    def _rx_display(resp) -> str:
        raw_hex = f"{len(resp.raw):02X} " + " ".join(f"{b:02X}" for b in resp.raw)
        if resp.is_negative and resp.nrc is not None:
            nrc_name = uds_link.NRC_NAMES.get(resp.nrc, "unknown NRC")
            return f"{raw_hex}  ({nrc_name})"
        return raw_hex

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
        if sid == 0x2F and len(raw) >= 5 and raw[0] == 0x6F:
            did = (raw[1] << 8) | raw[2]
            did_name = uds_link.DID_NAMES.get(did, f"DID 0x{did:04X}")
            opt_name = uds_link.IOCTRL_OPTION_NAMES.get(raw[3], f"0x{raw[3]:02X}")
            return f"{did_name} {opt_name} -> level={raw[4]}"
        if sid == 0x31 and len(raw) >= 4 and raw[0] == 0x71:
            return self._decode_routine_response(raw)
        return resp.describe()

    def _decode_routine_response(self, raw: bytes) -> str:
        sub_names = {0x01: "startRoutine", 0x02: "stopRoutine", 0x03: "requestRoutineResults"}
        sub = raw[1]
        rid = (raw[2] << 8) | raw[3]
        sub_name = sub_names.get(sub, f"0x{sub:02X}")
        if sub == 0x03 and len(raw) >= 5:
            if raw[4] == 0x00:
                return f"{sub_name} RID={rid:04X}: 実行中 (running)"
            if raw[4] == 0x01 and len(raw) >= 6:
                verdict = "PASS" if raw[5] == 0x01 else "FAIL"
                return f"{sub_name} RID={rid:04X}: 完了 ({verdict})"
        return f"{sub_name} RID={rid:04X}"

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
        if sub == 0x06 and len(raw) >= 8:
            dtc = (raw[2] << 16) | (raw[3] << 8) | raw[4]
            return f"ExtendedData {uds_link.dtc_name(dtc)} record={raw[5]} occurrence={raw[7]}"
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
    # 周期 CAN フレーム送信 (can_frame + interval_ms)
    # ------------------------------------------------------------------
    def _handle_periodic_can_toggle(self, btn_cfg, idx: int, label: str, entry_data: dict):
        """周期送信の開始/停止をトグルする。バスロック不要のためスレッドで直接呼ぶ。"""
        stop_ev = self._periodic_stops.get(idx)
        if stop_ev is not None and not stop_ev.is_set():
            stop_ev.set()
            self.log_queue.put(f"[{label}] 周期送信 停止")
            self.state_queue.put(("periodic_btn", (idx, "定期")))
        else:
            if self.bus is None:
                self.log_queue.put(f"[{label}] 未接続")
                return
            try:
                if "can_id" in entry_data:
                    can_id = self._parse_can_id(entry_data["can_id"])
                else:
                    raw = btn_cfg.get("can_id", "0")
                    can_id = int(raw, 0) if isinstance(raw, str) else int(raw)
                if "data" in entry_data:
                    data = self._parse_hex_bytes(entry_data["data"])
                else:
                    data = parse_payload(btn_cfg["data"])
            except ValueError as exc:
                self.log_queue.put(f"[{label}] 入力エラー: {exc}")
                return
            e2e_cfg_p = btn_cfg.get("e2e")
            secoc_cfg_p = btn_cfg.get("secoc")
            if e2e_cfg_p:
                # Entry には E2E バイト込みの完全フレームが入っているが、
                # 定期送信ではカウンタをインクリメントするためシグナル部分のみ抽出する
                data = data[e2e_cfg_p["payload_offset"]:]
            elif secoc_cfg_p:
                # SecOC も同様に、定期送信では Freshness Value を毎回進めるため
                # Authentic Payload 部分のみ抽出する。
                data = data[:secoc_cfg_p["auth_len"]]
            interval_ms = btn_cfg.get("interval_ms", 100)
            new_stop = threading.Event()
            self._periodic_stops[idx] = new_stop
            self.state_queue.put(("periodic_btn", (idx, "停止")))
            self.log_queue.put(
                f"[{label}] 周期送信 開始 ({interval_ms}ms 間隔)"
                f"  ID=0x{can_id:03X} DATA=" + " ".join(f"{b:02X}" for b in data)
            )
            threading.Thread(
                target=self._periodic_can_worker,
                args=(label, can_id, data, interval_ms / 1000.0, new_stop, e2e_cfg_p, secoc_cfg_p),
                daemon=True,
            ).start()

    # 送信直後に UDS が続いても間隔を保てるよう、送信後にロックを保持する時間 (秒)
    _PERIODIC_POST_SEND_HOLD_S = 0.010  # 10ms

    def _periodic_can_worker(self, label, can_id, data, interval_s, stop_ev,
                              e2e_cfg=None, secoc_cfg=None):
        """interval_s ごとに CAN フレームを送り続ける。stop_ev がセットされたら終了。

        bus_lock をノンブロッキングで取得し、送信後 _PERIODIC_POST_SEND_HOLD_S (10ms)
        ロックを保持してから解放する。

        【なぜ 10ms 保持するか】
        EngineInfo 送信の直後に UDS request が送られると、Arduino MCP2515 の
        受信バッファ (RXB0/RXB1) に 2 フレームが数 μs の間隔で詰まり、
        Can_MainFunction が呼ばれる前にバッファが埋まって UDS request が
        取りこぼされることがある（結果: receive_uds_response が 2s タイムアウト）。
        10ms 保持することで、UDS ワーカーがロックを取得するのは EngineInfo 送信
        から最低 10ms 後になり、Arduino が EngineInfo を読み出す時間を確保できる。

        UDS 処理中 (bus_lock 保持中) は blocking=False で即スキップする。
        スキップが 2s 続いても COM_TIMEOUT_ENGINE_INFO_MS (5000ms) 以内なので
        ECU 側のタイムアウトは発生しない。

        e2e_cfg が指定された場合は送信ごとにカウンタをインクリメントして
        E2E 保護バイト (Counter + CRC) を付加する
        (Profile01/05 いずれも e2e_cfg["profile"] に応じて自動判別)。"""
        e2e_counter = 0
        secoc_freshness = 0
        while True:
            if self.bus is not None:
                if self.bus_lock.acquire(blocking=False):
                    try:
                        if e2e_cfg:
                            send_data = self._apply_e2e(data, e2e_cfg, e2e_counter)
                            e2e_counter = App._next_e2e_counter(e2e_counter, e2e_cfg)
                        elif secoc_cfg:
                            send_data = self._apply_secoc(data, secoc_cfg, secoc_freshness)
                            secoc_freshness = (secoc_freshness + 1) & 0xFF
                        else:
                            send_data = data
                        uds_link.send_can_frame(self.bus, can_id, send_data)
                        time.sleep(self._PERIODIC_POST_SEND_HOLD_S)
                    except Exception:  # noqa: BLE001 - 周期送信中の一時エラーは無視して継続する
                        pass
                    finally:
                        self.bus_lock.release()
            if stop_ev.wait(interval_s):
                break

    # ------------------------------------------------------------------
    # 受信モニター (rx_monitor)
    # ------------------------------------------------------------------
    _ENGINE_STATE_NAMES = {0: "OFF", 1: "STARTING", 2: "RUNNING", 3: "FAULT"}
    _NM_SOURCE_NODE_NAMES = {1: "MeterEcu", 2: "VirtualPeerEcu"}

    @staticmethod
    def _decode_warning_status(byte0: int) -> str:
        """WarningStatus (CAN 0x210) byte[0] を RUN/FAULT/ABS/upd の4ビットへデコードする。
        Com Signal Group のビット配置: bit7=RunLamp, bit6=FaultLamp, bit5=AbsLamp,
        bit4=update-bit (Com_Cfg.h の BitPosition 0/1/2/3、ネットワークビット順)。
        upd はグループ単位の update-bit（SWS_Com_00801）: 実際に警告灯が変化した
        送信では 1、TMS=true 時の MIXED 周期フロア再送（変化なし）では 0 になる。"""
        run = (byte0 >> 7) & 1
        fault = (byte0 >> 6) & 1
        abs_ = (byte0 >> 5) & 1
        upd = (byte0 >> 4) & 1
        return f"(RUN:{run} FAULT:{fault} ABS:{abs_} upd={upd})"

    def _update_virtual_meter(self, data: bytes) -> None:
        """MeterStatus (CAN 0x200, DLC=5) の byte[2]=警告灯3bitミラー・
        byte[3-4]=EngineSpeedミラーから仮想メータ表示を更新する。
        ビット配置は WarningStatus と同じ MSB 起点（bit0=MSB）:
        byte[2] bit0=RunLamp, bit1=FaultLamp, bit2=AbsLamp。
        byte[3-4] はビッグエンディアン 16bit（EngineInfo の EngineSpeed と同じ単位・rpm）。"""
        run = (data[2] >> 7) & 1
        fault = (data[2] >> 6) & 1
        abs_ = (data[2] >> 5) & 1
        rpm = (data[3] << 8) | data[4]

        self.meter_rpm_bar["value"] = min(rpm, 8000)
        self.meter_rpm_var.set(f"{rpm} rpm")
        self.meter_run_lbl.configure(bg="green" if run else "gray85")
        self.meter_fault_lbl.configure(bg="red" if fault else "gray85")
        self.meter_abs_lbl.configure(bg="orange" if abs_ else "gray85")

    def _rx_monitor_worker(self, stop_ev: threading.Event):
        """bus_lock をノンブロッキングで取得し、rx_monitor CAN ID の受信フレームを表示する。
        UDS 処理中 (bus_lock 保持中) はスキップして干渉を避ける。

        このワーカーが「バスを常時ポーリングする」唯一の読み取り役であり、受信した
        フレームは rx_monitor 表示用に使うだけでなく _message_dispatch_queue にも
        流す (ファンアウト)。capl_dsl.py の on message ディスパッチ (CaplContext.try_recv())
        はここへは自前で bus.recv() せずこのキューを消費する側に回ることで、両者が
        同じフレームを奪い合ってどちらも取りこぼす、という競合を避けている。"""
        while not stop_ev.is_set():
            bus = self.bus
            if bus is None:
                stop_ev.wait(0.1)
                continue
            if not self.bus_lock.acquire(blocking=False):
                stop_ev.wait(0.02)
                continue
            error: Exception | None = None
            try:
                msg = bus.recv(timeout=0.05)
            except Exception as exc:  # noqa: BLE001 - 実エラーはログに出してワーカーを止める (下記参照)
                msg = None
                error = exc
            finally:
                self.bus_lock.release()
            if error is not None:
                # python-can の bus.recv() は仕様上タイムアウトでは None を返すだけで、
                # 例外はアダプタ切断等の実エラー。以前はここで一律無視していたため、
                # デッドなバスに対して無言で無限ポーリングし続けてしまっていた。
                # send()/wait_response() 等と同様にエラーを可視化し、このワーカー自体を
                # 停止する（GUI の Connected 表示自体は変えないが、以後 RX モニタ表示も
                # on message へのフレーム供給も止まる）。
                self.log_queue.put(f"[RXモニタ] 受信エラーのため監視を停止しました: {error}")
                return
            if msg is None:
                # フレーム未受信時は次の取得まで待機し、他スレッドがロックを取れる窓を設ける
                stop_ev.wait(0.1)
                continue
            try:
                self._message_dispatch_queue.put_nowait(msg)
            except queue.Full:
                pass  # 誰も消費していない (スクリプト未実行等) 場合は単に捨てる
            for idx, monitor_id in self._rx_monitor_ids.items():
                if msg.arbitration_id == monitor_id:
                    self.state_queue.put(("rx_mon", (idx, bytes(msg.data))))

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
    # スクリプト実行 (CAPL風 API 、capl_api.py 参照)
    # ------------------------------------------------------------------
    def _open_script(self):
        if not self._require_connected():
            return
        if self._script_thread is not None and self._script_thread.is_alive():
            messagebox.showwarning("実行中", "スクリプトは既に実行中です")
            return
        path = filedialog.askopenfilename(
            title="CAPL風スクリプトを選択",
            filetypes=[
                ("スクリプト", "*.py *.capl"),
                ("Python スクリプト", "*.py"),
                ("CAPL風 DSL スクリプト", "*.capl"),
                ("すべてのファイル", "*.*"),
            ],
        )
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                source = f.read()
        except OSError as exc:
            messagebox.showerror("読み込み失敗", str(exc))
            return

        # _message_dispatch_queue は Connect 中ずっと _rx_monitor_worker が貯め続けている
        # 共有キューなので、前のスクリプト実行や接続後のアイドル時間に溜まった「古い」
        # フレームが残っている場合がある。ここで捨てておかないと、on message が今回の
        # スクリプト開始より前に届いていたフレームをまとめて受け取ってしまい、
        # 「開始直後にバックログが一気に発火し、その後は静かに見える」という
        # 紛らわしい挙動になる。
        while True:
            try:
                self._message_dispatch_queue.get_nowait()
            except queue.Empty:
                break

        self.script_stop_event.clear()
        self.script_status_var.set(f"実行中: {os.path.basename(path)}")
        self._script_thread = threading.Thread(
            target=self._script_worker, args=(source, path), daemon=True
        )
        self._script_thread.start()

    def _stop_script(self):
        self.script_stop_event.set()

    def _script_worker(self, source: str, path: str):
        label = os.path.basename(path)
        self.log_queue.put(f"[script:{label}] 開始")
        # 拡張子で Python (.py, exec() ベース) / CAPL 風 DSL (.capl, 自作パーサ+インタプリタ)
        # を自動判別する。両者は run_script()/run_dsl_script() の引数の並びを揃えてあり、
        # どちらも内部で capl_api.CaplContext をランタイムとして使うため、GUI 側はこの分岐
        # だけで済む。
        is_dsl = path.lower().endswith(".capl")
        run = capl_dsl.run_dsl_script if is_dsl else capl_api.run_script
        try:
            run(
                source, path, lambda: self.bus, self.bus_lock,
                lambda text: self.log_queue.put(f"[script:{label}] {text}"),
                self.script_stop_event, self._message_dispatch_queue,
            )
            self.log_queue.put(f"[script:{label}] 完了")
        except capl_api.ScriptStopped:
            self.log_queue.put(f"[script:{label}] 停止されました")
        except capl_api.ScriptAbort as exc:
            self.log_queue.put(f"[script:{label}] 中断: {exc}")
        except capl_dsl.DslSyntaxError as exc:
            self.log_queue.put(f"[script:{label}] 構文エラー: {exc}")
        except Exception as exc:  # noqa: BLE001 - スクリプト内の想定外エラーもログに出して継続する
            self.log_queue.put(f"[script:{label}] エラー: {exc}")
        finally:
            self.state_queue.put(("script_done", None))

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
            elif kind == "resp":
                resp_idx, resp_text = value
                if resp_idx in self._response_vars:
                    self._response_vars[resp_idx].set(resp_text)
            elif kind == "key_fill":
                fill_idx, fill_text = value
                ev = self._entry_vars.get(fill_idx, {})
                if "data" in ev:
                    ev["data"].set(fill_text)
            elif kind == "entry_update":
                upd_idx, upd_text = value
                ev = self._entry_vars.get(upd_idx, {})
                if "data" in ev:
                    ev["data"].set(upd_text)
            elif kind == "script_done":
                self.script_status_var.set("")
            elif kind == "periodic_btn":
                btn_idx, text = value
                if btn_idx in self._periodic_btn_vars:
                    self._periodic_btn_vars[btn_idx].set(text)
            elif kind == "rx_mon":
                mon_idx, data = value
                if mon_idx in self._rx_monitor_vars:
                    raw_hex = " ".join(f"{b:02X}" for b in data)
                    self._rx_monitor_vars[mon_idx].set(raw_hex)
                    decode = self._rx_monitor_decode.get(mon_idx, "")
                    if decode == "engine_state" and len(data) >= 1:
                        # byte[0]=EngineState（E2E保護なし）
                        # byte[1] bit0（ネットワークビット8 = byte[1]のMSB）=
                        # EngineState シグナル単体の update-bit（SWS_Com_00061/00062）。
                        # 値変化時送信（Com_SendSignal 呼び出し）では 1、
                        # MIXED の周期フロア再送（値変化なし）では 0 になる。
                        name = self._ENGINE_STATE_NAMES.get(data[0], f"0x{data[0]:02X}")
                        if len(data) >= 2:
                            upd = (data[1] >> 7) & 1
                            name = f"{name} upd={upd}"
                        self._rx_monitor_name_vars[mon_idx].set(f"({name})")
                        if len(data) >= 5:
                            # byte[2] bit0-2 = RunLamp/FaultLamp/AbsLamp ミラー（本プロジェクト
                            # 独自拡張。WarningStatus(0x210) と同値）、byte[3-4] = EngineSpeed
                            # ミラー（ビッグエンディアン、EngineInfo(0x100) の検証済み値と同値）。
                            # 実機に物理表示器が無いための仮想メータ表示。
                            self._update_virtual_meter(data)
                    elif decode == "nm_status" and len(data) >= 2:
                        # byte[0]=Control Bit Vector（bit0=Repeat Message Request、
                        # 他ビットは本プロジェクトでは未使用）、
                        # byte[1]=Source Node Identifier
                        name = self._NM_SOURCE_NODE_NAMES.get(data[1], f"node=0x{data[1]:02X}")
                        repeat = " RepeatMsgReq" if (data[0] & 0x01) else ""
                        self._rx_monitor_name_vars[mon_idx].set(f"(alive: {name}{repeat})")
                    elif decode == "immobilizer_status" and len(data) >= 1:
                        # byte[0]=ImmobilizerStatus（Com Signal Gateway が
                        # ImmobilizerCmd(RX, SecOC検証済み)から直接転送。
                        # 0x00=LOCK, 0x01=UNLOCK。ImmobilizerCmd ボタンで
                        # UNLOCK/LOCK を送信した直後にここが追従して更新される
                        # ことを確認できる）
                        name = "UNLOCK" if data[0] == 0x01 else "LOCK"
                        self._rx_monitor_name_vars[mon_idx].set(f"({name})")
                    elif decode == "warning_status" and len(data) >= 1:
                        self._rx_monitor_name_vars[mon_idx].set(
                            self._decode_warning_status(data[0])
                        )
                    elif decode == "e2e_health" and len(data) >= 5:
                        # byte[0-1]=CRC16 (E2E Profile05、リトルエンディアン),
                        # byte[2]=Counter (E2E Profile05、8bitフル値)
                        # byte[3]=E2E CRCエラー累積数, byte[4]=E2E シーケンスエラー累積数
                        # (EngineInfo/AbsInfo 受信側の合算、0-255で飽和。E2EMon CDD相当モジュールが
                        #  Com経由でPERIODIC送信するネットワーク健全性テレメトリ。テレメトリ自体も
                        #  E2E保護されている)
                        # 以前は E2E Profile01+SecOC の二重保護(DLC=8)だったが、SecOC は撤去し
                        # E2E Profile05 単体保護(DLC=5)に切り替えた。
                        self._rx_monitor_name_vars[mon_idx].set(
                            f"(crcErr={data[3]} seqErr={data[4]})"
                        )
                    elif mon_idx in self._rx_monitor_name_vars:
                        self._rx_monitor_name_vars[mon_idx].set("")

        self.after(100, self._poll_queues)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="config.json")
    args = parser.parse_args()
    App(args.config).mainloop()


if __name__ == "__main__":
    main()

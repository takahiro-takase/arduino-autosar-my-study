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
import math
import os
import queue
import re
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

import can
import serial
import serial.tools.list_ports
from Crypto.Cipher import AES
from Crypto.Hash import CMAC

import capl_api
import capl_dsl
import uds_link


def parse_payload(items) -> bytes:
    return bytes(int(x, 16) if isinstance(x, str) else int(x) for x in items)


# tools/uds_tester/src の絶対パス。__file__ は tools/uds_tester/src/app.py。
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))

# data/can_signals.json（CANフレームのビットレイアウト定義。tools/can_signal_editor/
# 参照）へのパス。3階層上がリポジトリルート
# （tools/can_signal_editor/src/app.py の DEFAULT_DATA_PATH と同じ規約）。
DEFAULT_SIGNAL_DEFS_PATH = os.path.normpath(
    os.path.join(_THIS_DIR, "..", "..", "..", "data", "can_signals.json")
)

# config.json（このツール自身の設定）へのパス。以前は cwd 相対の "config.json" が
# 既定値で、run.bat の cd /d "%~dp0" に解決を頼っていた。tools/can_tool/（統合
# ランチャー）は tools/uds_tester/ へ cd しないため、DEFAULT_SIGNAL_DEFS_PATH と
# 同じ __file__ 基準の絶対パス規約に揃える（単体起動時の挙動は変わらない）。
DEFAULT_CONFIG_PATH = os.path.normpath(os.path.join(_THIS_DIR, "..", "config.json"))


class UdsTesterFrame(ttk.Frame):
    def _load_signal_defs(self, path: str) -> "dict[int, dict]":
        """data/can_signals.json を読み込み、{CAN ID(int): フレーム定義} の辞書を返す。
        RXモニタのデコードはこの辞書を情報源にする（tools/can_signal_editor/ で
        編集した内容がそのまま反映される）。ファイル自体が無い・JSONとして壊れて
        いる場合は空辞書にフォールバックする（全フレーム分のデコードが無効化
        されるのは避けられない）。一方、canId が数値型・null 等でフレーム1件
        だけが壊れている場合は、そのフレームだけをスキップし他の正常なフレームは
        引き続き使えるようにする（1件の入力ミスで全フレームのデコードが
        道連れで無効化されるのを避けるため）。UDS診断というこのツール本来の
        機能はどちらのケースでも止めない。"""
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError) as exc:
            self.log_queue.put(f"[起動] {path} の読み込みに失敗しました（RXモニタのデコードは無効化されます）: {exc}")
            return {}

        result: "dict[int, dict]" = {}
        for fr in data.get("frames", []):
            try:
                result[int(fr["canId"], 0)] = fr
            except Exception as exc:  # noqa: BLE001 - フレーム1件分の壊れ方は事前に列挙しきれない
                self.log_queue.put(
                    f"[起動] {path} のフレーム定義 {fr.get('name', '?')!r} を読み飛ばしました: {exc}"
                )
        return result

    def __init__(self, master: tk.Misc, config_path: str):
        super().__init__(master)

        with open(config_path, "r", encoding="utf-8") as f:
            self.cfg = json.load(f)

        self.bus: can.BusABC | None = None
        self.bus_lock = threading.Lock()
        self.log_queue: "queue.Queue[str]" = queue.Queue()
        self.state_queue: "queue.Queue[tuple]" = queue.Queue()
        self.signal_defs = self._load_signal_defs(DEFAULT_SIGNAL_DEFS_PATH)
        self._periodic_stops: "dict[int, threading.Event]" = {}
        self._entry_vars: "dict[int, dict[str, tk.StringVar]]" = {}
        self._response_vars: "dict[int, tk.StringVar]" = {}
        self._periodic_btn_vars: "dict[int, tk.StringVar]" = {}
        self._log_visible = tk.BooleanVar(value=False)
        self._rx_monitor_vars: "dict[int, tk.StringVar]" = {}
        self._rx_monitor_ids: "dict[int, int]" = {}
        self._rx_monitor_secoc_verify: "dict[int, dict]" = {}
        self._rx_monitor_name_vars: "dict[int, tk.StringVar]" = {}
        self._rx_monitor_stop = threading.Event()
        self._rx_monitor_thread: "threading.Thread | None" = None
        self.script_stop_event = threading.Event()
        self._script_thread: threading.Thread | None = None
        # Arduino の USB シリアルデバッグログ (Serial.println、Det_Hw.cpp 参照) を
        # 読むための接続。CAN 接続 (self.bus) とは完全に独立しており、
        # 別デバイス（同じ USB ケーブルの COM ポート）・別スレッド・別キューで扱う。
        self.serial_port: "serial.Serial | None" = None
        self._serial_stop = threading.Event()
        self._serial_thread: threading.Thread | None = None
        self.serial_log_queue: "queue.Queue[str]" = queue.Queue()
        self._serial_log_visible = tk.BooleanVar(value=False)
        # CanSM/EcuM の最新状態表示用 StringVar（ログ行から正規表現で抽出、
        # _parse_serial_state 参照）。tk.StringVar は Tk ルート作成後にしか
        # 生成できないためインスタンス属性だが、キー・表示順・ラベルは
        # クラス属性 _STATE_DEFS（下記 _parse_serial_state 付近）が単一の情報源。
        self._serial_state_vars = {
            tag: tk.StringVar(value="?") for tag, _label, _rules in self._STATE_DEFS
        }
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
        # CAN 接続と Serial 接続は完全に独立した別デバイス（self.bus / self.serial_port）
        # への接続のため、別パネルに分ける（1つの「接続」パネルに両方を詰め込むと
        # どちらの設定/ボタンか分かりにくいため）。
        conn = ttk.LabelFrame(self, text="CAN 接続")
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

        # 「CANログ」表示切替は接続状態や応答からの推測値ではなくGUI自体の表示設定
        # のため、トラッキング状態ではなく接続パネル側に置く。
        ttk.Checkbutton(
            conn,
            text="CANログ",
            variable=self._log_visible,
            command=lambda: self._toggle_panel(self.log_frame, self._log_visible),
        ).grid(row=0, column=8, padx=8)

        # CAPL風スクリプトの実行/停止（CAN 送信操作）も、コマンド一覧のボタンや
        # トラッキング状態とは性質が異なる常設の操作のため、CAN 接続パネル側に
        # 置く（ログと同じ理由）。
        ttk.Button(conn, text="スクリプト実行...", command=self._open_script).grid(
            row=0, column=9, padx=(8, 2))
        ttk.Button(conn, text="停止", command=self._stop_script).grid(
            row=0, column=10, padx=(2, 8))
        self.script_status_var = tk.StringVar(value="")
        ttk.Label(conn, textvariable=self.script_status_var).grid(
            row=0, column=11, padx=(0, 8), sticky="w")

        # ---- Serial 接続（CAN 接続 self.bus とは別の独立した接続。実体は Arduino
        # だが、シリアルログ読み取り自体は特定デバイスに依存しない汎用機能のため
        # 「Arduino」ではなく「Serial」と呼ぶ） ----
        conn_serial = ttk.LabelFrame(self, text="Serial 接続")
        conn_serial.pack(fill="x", padx=8, pady=4)

        ttk.Label(conn_serial, text="port").grid(row=0, column=0, padx=4, pady=4)
        self.serial_port_var = tk.StringVar(value="")
        self.serial_port_combo = ttk.Combobox(
            conn_serial, textvariable=self.serial_port_var, width=10, state="readonly")
        self.serial_port_combo.grid(row=0, column=1)
        self._refresh_serial_ports()

        ttk.Button(conn_serial, text="更新", command=self._refresh_serial_ports).grid(
            row=0, column=2, padx=4)

        ttk.Label(conn_serial, text="baud").grid(row=0, column=3, padx=4)
        self.serial_baud_var = tk.StringVar(
            value=str(self.cfg.get("serial", {}).get("baudrate", 115200)))
        ttk.Entry(conn_serial, textvariable=self.serial_baud_var, width=8).grid(row=0, column=4)

        self.serial_connect_btn = ttk.Button(
            conn_serial, text="Connect", command=self._toggle_serial_connect)
        self.serial_connect_btn.grid(row=0, column=6, padx=8)

        self.serial_status_var = tk.StringVar(value="● Disconnected")
        self.serial_status_label = ttk.Label(
            conn_serial, textvariable=self.serial_status_var, foreground="red")
        self.serial_status_label.grid(row=0, column=7, padx=8)

        # 「CANログ」と同じ理由（GUI 表示設定であって接続状態からの推測値ではない）
        # で Serial 接続パネル側に置く。
        ttk.Checkbutton(
            conn_serial,
            text="Serialログ",
            variable=self._serial_log_visible,
            command=lambda: self._toggle_panel(self.serial_log_frame, self._serial_log_visible),
        ).grid(row=0, column=8, padx=8)

        # ---- ECU 状態（シリアルログから抽出。接続パネルとは別枠にする理由:
        # 旧「トラッキング状態」パネル（CAN 送受信の観測からの推測値、S3 タイマ等で
        # サイレントに古くなりうるため撤去済み）と違い、ここは ECU 自身のログという
        # 一次情報源に基づく値。接続の可否とは別の関心事であり、今後 Dcm セッション/
        # SecurityAccess レベル等を追加しやすいよう、専用パネルとして独立させる） ----
        state_frame = ttk.LabelFrame(self, text="ECU 状態（シリアルログより）")
        state_frame.pack(fill="x", padx=8, pady=4)
        for tag, label, _rules in self._STATE_DEFS:
            ttk.Label(state_frame, text=f"{label}:",
                      font=("", 9, "bold")).pack(side="left", padx=(8, 2), pady=4)
            ttk.Label(state_frame, textvariable=self._serial_state_vars[tag],
                      foreground="#2a7a2a", font=("Consolas", 9, "bold"),
                      width=self._STATE_VALUE_WIDTH,
                      anchor="w").pack(side="left", padx=(0, 12), pady=4)

        meter = ttk.LabelFrame(
            self, text="仮想メータ表示（MeterStatus 0x200、実機に物理表示器が無いための可視化）")
        meter.pack(fill="x", padx=8, pady=4)

        # タコメータ風の円形ゲージ（実車寄りの240°弧、8時位置=0rpm～12時位置～
        # 4時位置=meter_rpm_max を時計回りにスイープ）。tkinter標準のCanvasのみで
        # 描画し、matplotlib等の追加依存は増やさない（フレーム受信毎の高頻度更新でも
        # 軽い。static部分は _draw_tacho_gauge_static() で一度だけ描き、rpm更新時は
        # _update_tacho_needle() が針の座標と数値表示だけ更新する）。
        self.meter_rpm_max = 8000
        self._tacho_cx, self._tacho_cy, self._tacho_r = 75, 68, 55
        self.meter_tacho_canvas = tk.Canvas(
            meter, width=150, height=120, highlightthickness=0)
        self.meter_tacho_canvas.grid(row=0, column=0, columnspan=2, padx=8, pady=4)
        self.meter_tacho_needle = None
        self.meter_tacho_rpm_text = None
        self._draw_tacho_gauge_static()
        self._update_tacho_needle(0)

        # 水温ゲージ（タコメータと同じ240°の丸形メータ。実車でも水温計はタコ/
        # スピードと同じ丸形で配置されることが多く、この形自体は不自然ではない。
        # タコメータより小さいサイズにして「主/副」の見た目の違いを付けている）。
        # タコメータの直後（列2）に置き、2つの丸形ゲージを隣り合わせにする。
        self.meter_coolant_max = 120
        self._coolant_cx, self._coolant_cy, self._coolant_r = 45, 42, 32
        self.meter_coolant_canvas = tk.Canvas(
            meter, width=95, height=80, highlightthickness=0)
        self.meter_coolant_canvas.grid(row=0, column=2, padx=8, pady=4)
        self.meter_coolant_needle = None
        self.meter_coolant_gauge_text = None
        self._draw_coolant_gauge_static()
        self._update_coolant_needle(0)

        # 警告灯（テルテール）。実車のクラスタは縦積みではなく、メータ盤の下に
        # 横一列の帯として配置されることが多いため、2つの丸ゲージの下の行
        # （row=1、ゲージと同じ列0-2）に、平坦な小型ランプとして並べる
        # （実車のテルテールはボタンではなくフラットな点灯/消灯ランプのため、
        # relief="raised" は使わない）。
        self.meter_run_var = tk.StringVar(value="RUN")
        self.meter_run_lbl = tk.Label(
            meter, textvariable=self.meter_run_var, width=6, relief="flat",
            borderwidth=1, bg="gray85")
        self.meter_run_lbl.grid(row=1, column=0, padx=4, pady=(0, 6))

        self.meter_fault_var = tk.StringVar(value="FAULT")
        self.meter_fault_lbl = tk.Label(
            meter, textvariable=self.meter_fault_var, width=6, relief="flat",
            borderwidth=1, bg="gray85")
        self.meter_fault_lbl.grid(row=1, column=1, padx=4, pady=(0, 6))

        self.meter_abs_var = tk.StringVar(value="ABS")
        self.meter_abs_lbl = tk.Label(
            meter, textvariable=self.meter_abs_var, width=6, relief="flat",
            borderwidth=1, bg="gray85")
        self.meter_abs_lbl.grid(row=1, column=2, padx=4, pady=(0, 6))

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
        ttk.Label(inner, text="周期(ms)", font=("", 9, "bold")).grid(
            row=0, column=5, padx=(2, 4), pady=(4, 1), sticky="w")
        ttk.Separator(inner, orient="horizontal").grid(
            row=1, column=0, columnspan=6, sticky="ew", padx=4, pady=(0, 2))

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
                sep.grid(row=row, column=0, columnspan=6,
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
                                     anchor="w", width=60)
                name_lbl.pack(side="left", padx=(4, 0))
                name_lbl.bind("<MouseWheel>", _scroll)
                self._rx_monitor_vars[i] = rx_var
                self._rx_monitor_name_vars[i] = name_var
                self._rx_monitor_ids[i] = can_id_int
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
                    # 送信データ入力欄: Entry+読み取り専用Comboboxだったものを、
                    # 直接編集も▼からのプリセット選択も両方できる1つの
                    # editable Combobox に統合（state を指定しない = 既定 "normal"）。
                    combo_vals = [p["label"] for p in presets]
                    data_combo = ttk.Combobox(cell, textvariable=data_var,
                                              values=combo_vals, width=30,
                                              font=("Consolas", 9))
                    data_combo.pack(side="left")
                    data_combo.bind("<MouseWheel>", _scroll)
                    if presets:
                        def _on_preset(event, var=data_var, ps=presets, cb=data_combo):
                            sel = cb.current()
                            if sel >= 0:
                                vals = ps[sel].get("payload") or ps[sel].get("data") or []
                                var.set(_hex_str(vals))
                            cb.selection_clear()
                        data_combo.bind("<<ComboboxSelected>>", _on_preset)
                    self._entry_vars.setdefault(i, {})["data"] = data_var

                    # 送信データのライブ説明（旧: 読み取り専用Comboboxがあった場所）。
                    # 受信データ説明 (_decode_response) と対になる送信側表示。
                    # multiframe は PCIバイトを持たない生UDSバイト列（send_multiframe_
                    # request() 参照）、それ以外(raw/security_*)は byte0=PCI。
                    has_pci = t != "multiframe"
                    tx_desc_var = tk.StringVar(value="")
                    tx_desc_lbl = ttk.Label(cell, textvariable=tx_desc_var,
                                            font=("Consolas", 9),
                                            foreground="#3a7ebf", width=40, anchor="w")
                    tx_desc_lbl.pack(side="left", padx=(6, 0))
                    tx_desc_lbl.bind("<MouseWheel>", _scroll)

                    def _update_tx_desc(*_args, var=data_var, dvar=tx_desc_var,
                                        has_pci=has_pci):
                        try:
                            payload = self._parse_hex_bytes(var.get())
                        except ValueError:
                            dvar.set("")
                            return
                        uds = payload[1:] if has_pci else payload
                        try:
                            dvar.set(self._decode_request(uds))
                        except (IndexError, ValueError):
                            dvar.set("")

                    data_var.trace_add("write", _update_tx_desc)
                    _update_tx_desc()

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

                if t == "raw":
                    # 定期送信ボタン (col 4, rowspan=2)。Tester Present 等、状態を
                    # 持たない単純な UDS request のみサポートする（multiframe/
                    # security_* は対象外。_on_periodic_click() 参照）。
                    periodic_var = tk.StringVar(value="定期")
                    periodic_btn = ttk.Button(
                        inner, textvariable=periodic_var, width=5,
                        command=lambda c=btn_cfg, idx=i: self._on_periodic_click(c, idx),
                    )
                    periodic_btn.grid(row=row, column=4, rowspan=2,
                                      padx=(2, 4), pady=2, sticky="nsew")
                    periodic_btn.bind("<MouseWheel>", _scroll)
                    self._periodic_btn_vars[i] = periodic_var

                    # 周期(ms) 入力欄 (col 5, rowspan=2)。既定 2000ms
                    # （旧 Tester Present 自動送信チェックボックスの 2 秒毎と同じ）。
                    interval_var = tk.StringVar(value=str(btn_cfg.get("interval_ms", 2000)))
                    interval_entry = ttk.Entry(inner, textvariable=interval_var, width=6,
                                               font=("Consolas", 9))
                    interval_entry.grid(row=row, column=5, rowspan=2,
                                        padx=(2, 4), pady=2, sticky="ns")
                    interval_entry.bind("<MouseWheel>", _scroll)
                    self._entry_vars.setdefault(i, {})["interval_ms"] = interval_var

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
                        f"{b:02X}" for b in UdsTesterFrame._apply_e2e(_raw_bytes, _e2e_cfg_btn, 0)
                    )
                elif _secoc_cfg_btn:
                    _raw_bytes = parse_payload(btn_cfg.get("data", []))
                    default_hex = " ".join(
                        f"{b:02X}" for b in UdsTesterFrame._apply_secoc(_raw_bytes, _secoc_cfg_btn, 0)
                    )
                else:
                    default_hex = _hex_str(btn_cfg.get("data", []))
                data_var = tk.StringVar(value=default_hex)
                presets = btn_cfg.get("presets", [])
                cell = ttk.Frame(inner)
                cell.grid(row=row, column=2, padx=(0, 4), pady=2, sticky="w")
                cell.bind("<MouseWheel>", _scroll)
                # 送信データ入力欄: Entry+読み取り専用Comboboxだったものを、
                # 直接編集も▼からのプリセット選択も両方できる1つの
                # editable Combobox に統合（state を指定しない = 既定 "normal"）。
                combo_vals = [p["label"] for p in presets]
                data_combo = ttk.Combobox(cell, textvariable=data_var,
                                          values=combo_vals, width=30,
                                          font=("Consolas", 9))
                data_combo.pack(side="left")
                data_combo.bind("<MouseWheel>", _scroll)
                if presets:
                    def _on_preset(event, var=data_var, ps=presets, cb=data_combo,
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
                                cur = UdsTesterFrame._parse_hex_bytes(var.get())
                            except ValueError:
                                cur = b""
                            if ecfg:
                                counter = UdsTesterFrame._read_e2e_counter(cur, ecfg)
                                _pb = parse_payload(vals)
                                var.set(" ".join(
                                    f"{b:02X}" for b in UdsTesterFrame._apply_e2e(_pb, ecfg, counter)
                                ))
                            elif scfg:
                                fo = scfg["freshness_offset"]
                                freshness = cur[fo] if len(cur) > fo else 0
                                _pb = parse_payload(vals)
                                var.set(" ".join(
                                    f"{b:02X}" for b in UdsTesterFrame._apply_secoc(_pb, scfg, freshness)
                                ))
                            else:
                                var.set(_hex_str(vals))
                        cb.selection_clear()
                    data_combo.bind("<<ComboboxSelected>>", _on_preset)
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

                # 周期(ms) 入力欄 (col 5)。優先順位: config.json の interval_ms
                # （明示指定）→ data/can_signals.json の txPeriodMs（この can_id を
                # 持つ RX/TX-RX 方向のフレーム定義があれば、そこに記録された送信
                # 周期。can_frame ボタンは基本的に外部ECUからの受信を模擬する用途
                # のため、対応するRXフレームの txPeriodMs がそのまま「相手ECUが
                # 送るべき周期」の妥当な初期値になる。direction=TX のフレーム
                # （メータECU自身が送るMeterStatus等）の txPeriodMs はメータECU
                # 自身の送信周期であり「相手ECUが送るべき周期」ではないため、
                # ここでは対象外にする）→ 既定100ms、の順で決める。実行時には
                # この Entry の値を変更できる。
                _default_interval = btn_cfg.get("interval_ms")
                if _default_interval is None:
                    _frame_def = self.signal_defs.get(can_id_val)
                    if _frame_def is not None and _frame_def.get("direction") in ("RX", "TX/RX"):
                        _default_interval = _frame_def.get("txPeriodMs")
                if _default_interval is None:
                    _default_interval = 100
                interval_var = tk.StringVar(value=str(_default_interval))
                interval_entry = ttk.Entry(inner, textvariable=interval_var, width=6,
                                           font=("Consolas", 9))
                interval_entry.grid(row=row, column=5, padx=(2, 4), pady=2)
                interval_entry.bind("<MouseWheel>", _scroll)
                self._entry_vars.setdefault(i, {})["interval_ms"] = interval_var

                current_row += 1
        # ---- ログ ----
        self.log_frame = ttk.LabelFrame(body, text="ログ")
        # initially hidden; shown by _toggle_panel when checkbox is checked

        self.log_text = scrolledtext.ScrolledText(
            self.log_frame, font=("Consolas", 10), state="disabled", wrap="word"
        )
        self.log_text.pack(fill="both", expand=True)

        # ---- Serial ログ（Serial.println 出力。上記「CANログ」＝ツール自身の
        # CAN 送受信ログとは別物のため、別パネルとして分ける） ----
        self.serial_log_frame = ttk.LabelFrame(body, text="Serialログ")
        # initially hidden; shown by _toggle_panel when checkbox is checked
        # CanSM/EcuM の状態表示は専用の「ECU 状態」パネルに常設したため、ここには
        # 置かない（_serial_state_vars の Label は state_frame 側を参照）。

        self.serial_log_text = scrolledtext.ScrolledText(
            self.serial_log_frame, font=("Consolas", 9), state="disabled", wrap="word"
        )
        self.serial_log_text.pack(fill="both", expand=True, padx=4, pady=4)

    @staticmethod
    def _toggle_panel(frame, visible_var):
        """チェックボックス連動で LabelFrame の表示/非表示を切り替える共通処理
        （「ログ」「Serialログ」パネルで同一の4行を2回書いていたのを統合）。"""
        if visible_var.get():
            frame.pack(side="left", fill="both", expand=True)
        else:
            frame.pack_forget()

    def _refresh_serial_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.serial_port_combo["values"] = ports
        if ports and self.serial_port_var.get() not in ports:
            self.serial_port_var.set(ports[0])

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
        crc = UdsTesterFrame._crc16_e2e_p05(crc_input)
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
            return UdsTesterFrame._apply_e2e_p05(payload, e2e_cfg, counter)
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
        frame[crc_offset] = UdsTesterFrame._crc8_sae_j1850(bytes(crc_input))
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

        mac = UdsTesterFrame._secoc_cmac_truncated(key, bytes(auth_input), mac_len)
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
        expected = UdsTesterFrame._secoc_cmac_truncated(key, bytes(auth_input), mac_len)
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
    # Serial 接続（CAN 接続とは独立。pyserial で COM ポートを開き、
    # Serial.println() の出力を行単位で読む。gs_usb と異なり明示 close() で
    # 素直に解放できるため、CAN 側のような gc.collect() 越しの後始末は不要）
    # ------------------------------------------------------------------
    def _toggle_serial_connect(self):
        if self.serial_port is None:
            self._serial_connect()
        else:
            self._serial_disconnect()

    def _serial_connect(self):
        port = self.serial_port_var.get()
        if not port:
            messagebox.showerror("接続失敗", "COM ポートを選択してください")
            return
        try:
            baud = int(self.serial_baud_var.get())
        except ValueError:
            messagebox.showerror("接続失敗", f"baud の形式エラー: {self.serial_baud_var.get()!r}")
            return
        try:
            ser = serial.Serial(port, baud, timeout=0.2)
        except Exception as exc:  # noqa: BLE001 - 接続失敗内容をそのままユーザーに見せる
            messagebox.showerror("接続失敗", str(exc))
            return
        self.serial_port = ser
        self.serial_status_var.set("● Connected")
        self.serial_status_label.configure(foreground="green")
        self.serial_connect_btn.configure(text="Disconnect")
        self.serial_log_queue.put(f"接続しました ({port} @ {baud}bps)")
        self._serial_stop.clear()
        self._serial_thread = threading.Thread(
            target=self._serial_reader_worker, args=(ser, self._serial_stop), daemon=True)
        self._serial_thread.start()

    def _serial_disconnect(self):
        self._serial_stop.set()
        if self._serial_thread is not None and self._serial_thread.is_alive():
            self._serial_thread.join(timeout=1.0)
        self._serial_thread = None
        self._serial_mark_disconnected()
        self.serial_log_queue.put("切断しました")

    def _serial_mark_disconnected(self):
        """serial_port を close して GUI を切断状態に戻す（表示のリセットのみ、
        メインスレッドから呼ぶこと）。ユーザーの Disconnect クリック
        （_serial_disconnect）と、ワーカースレッドの受信エラー通知
        （_poll_queues の "serial_error" ハンドラ）の両方から使う共通処理。
        後者を追加した理由: 以前はエラー時にログへ1行出すだけで self.serial_port/
        ステータス表示/Connect ボタンをリセットしておらず、USB 切断等で
        ワーカーが static に終了した後も GUI が「● Connected」のまま固着し、
        ECU 状態パネルも直前の値が更新されず古いまま残り続けるバグがあった
        （2026-08 のレビューで発見）。"""
        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except Exception:  # noqa: BLE001 - 切断処理は best-effort
                pass
        self.serial_port = None
        self.serial_status_var.set("● Disconnected")
        self.serial_status_label.configure(foreground="red")
        self.serial_connect_btn.configure(text="Connect")

    def _serial_reader_worker(self, ser: "serial.Serial", stop_ev: threading.Event):
        """シリアルログを行単位で読み、生ログは serial_log_queue へ、
        CanSM/EcuM の状態変化は state_queue へ流す。timeout=0.2s で定期的に
        stop_ev を確認することで、切断時に確実に終了する
        （_rx_monitor_worker と同じ設計、bus_lock 相当の排他は不要
        ―― CAN と違い他のワーカーとこのシリアルポートを取り合わないため）。

        ser.readline() を使わず read() + 手動でのバッファリングにしているのは、
        readline() だと 1 行の送信が timeout（0.2s）より長くかかった場合に
        改行未到達のまま打ち切られ、分断された断片をそれぞれ別の「1行」として
        扱ってしまう（ログが文字化けし、分断点が状態判定キーワードにかかると
        _parse_serial_state が遷移を見逃す）ため。バッファに溜めて実際に
        改行が見つかった分だけを1行として確定させれば、この問題は原理的に
        起こらない（2026-08 のレビューで発見・対応）。"""
        buffer = b""
        while not stop_ev.is_set():
            try:
                chunk = ser.read(max(1, ser.in_waiting))
            except Exception as exc:  # noqa: BLE001 - 実エラーはログに出してワーカーを止める
                self.serial_log_queue.put(f"[Serialログ] 受信エラーのため監視を停止しました: {exc}")
                self.state_queue.put(("serial_error", None))
                return
            if not chunk:
                continue  # timeout（0.2s）。何も届かなかっただけ。
            buffer += chunk
            while True:
                raw_line, sep, buffer = buffer.partition(b"\n")
                if not sep:
                    buffer = raw_line  # 改行未到達分。次の read() 分と合わせて再試行
                    break
                line = raw_line.decode("utf-8", errors="replace").rstrip("\r")
                if not line:
                    continue
                self.serial_log_queue.put(line)
                parsed = self._parse_serial_state(line)
                if parsed is not None:
                    self.state_queue.put(("serial_state", parsed))

    # ログ行 "[<ms>ms] LEVEL TAG: func: message" から TAG と残りを取り出す
    # （Det_Hw.cpp の出力フォーマット。README「シリアルモニタ出力例」参照）。
    _SERIAL_LOG_RE = re.compile(r"^\[\d+ms\]\s+\S+\s+(\w+):\s+(.*)$")

    # CanSM/EcuM の状態遷移ログに現れる部分文字列 → 表示する状態名。
    # 上から順に最初に一致したものを採用するため、より具体的な文字列
    # （例: "awaiting Nm Bus-Sleep Mode" を含む NO_COM_PENDING_SLEEP 用の行は
    # 素の "->NO_COM" も含んでしまう）を先に置く。CanSM.c/EcuM.c の
    # DET_LOGI/DET_LOGW 呼び出し文字列と対応（docs/modules/CanSM_Notes.md
    # 「状態遷移」の Mermaid 図と同じ6状態）。
    _CANSM_STATE_RULES = (
        ("awaiting Nm Bus-Sleep Mode", "NO_COM_PENDING_SLEEP"),
        ("->FULL_COM", "FULL_COM"),
        ("->SILENT_COM", "SILENT_COM"),
        ("Bus-Sleep Mode -> CAN controller SLEEP", "NO_COM"),
        ("->NO_COM", "NO_COM"),
        ("Wakeup detected -> validating", "WAKEUP_VALIDATING"),
        ("back to SLEEP", "NO_COM"),
        ("BusOff detected", "BUS_OFF"),
    )
    _ECUM_STATE_RULES = (
        ("->RUN", "RUN"),
        ("->POST_RUN", "POST_RUN"),
        ("->SHUTDOWN", "SHUTDOWN"),
    )

    # ComM.c の ComM_BusSMIndication() が CanSM からのモード変化通知を受けるたびに
    # 必ず出す "ch%u ->mode=%u" ログ（ComM.c 参照）から ComM_ModeType を判定する。
    # ComM_ModeType は 0/1/2 の3値しか取らない（ComM.h 参照）ため、CanSM/EcuM と
    # 同じ部分文字列一致ルールで表現でき、専用の正規表現・分岐は不要。
    _COMM_STATE_RULES = (
        ("->mode=0", "NO_COM"),
        ("->mode=1", "SILENT_COM"),
        ("->mode=2", "FULL_COM"),
    )

    # 「ECU 状態」パネルの単一の情報源: (ログの TAG 文字列, 表示ラベル, 判定ルール)。
    # ウィジェットの並び順（EcuM → ComM → CanSM の、上位から下位への順）・表示
    # ラベル文字列・ログ判定ルールを、ここ1箇所にまとめる（以前は3つの辞書に
    # 分散しており、新しい TAG を追加する際に更新箇所を3つ揃える必要があった）。
    #
    # 表示ラベルは対応する AUTOSAR の公式型がある場合はそれに合わせる（EcuM→
    # "ECU State"、ComM→"Comm Mode"）。CanSM だけ公式型がなく「CanSM」のまま
    # な理由は README「UDS ボタン送信ツール」節の該当パラグラフを参照。
    #
    _STATE_DEFS = (
        ("EcuM", "ECU State", _ECUM_STATE_RULES),
        ("ComM", "Comm Mode", _COMM_STATE_RULES),
        ("CanSM", "CanSM", _CANSM_STATE_RULES),
    )
    _TAG_STATE_RULES = {tag: rules for tag, _label, rules in _STATE_DEFS}
    # 値の表示幅（文字数）。実行中に値が変わっても左右の他要素の位置が動かない
    # よう固定する。全状態を通じて最長の値は "NO_COM_PENDING_SLEEP"（20文字）。
    _STATE_VALUE_WIDTH = 20

    @classmethod
    def _parse_serial_state(cls, line: str):
        """ログ1行から EcuM/ComM/CanSM の状態遷移を抽出する。
        該当なしなら None、該当すれば (tag, state) を返す。
        完全な状態機械の再現ではなく、DET_LOGI/DET_LOGW に実際に出てくる
        文字列だけを頼りにしたベストエフォートの表示用。Bus-Off 回復成功時
        （CanSM_MainFunction() の復帰分岐、CanSM.c 参照）のように、その場では
        専用の状態変化ログを出さない遷移もあり、その場合は次に別の遷移ログが
        出るまで表示が古いままになる。"""
        m = cls._SERIAL_LOG_RE.match(line)
        if not m:
            return None
        tag, rest = m.group(1), m.group(2)
        rules = cls._TAG_STATE_RULES.get(tag)
        if rules is None:
            return None
        for substr, state in rules:
            if substr in rest:
                return (tag, state)
        return None

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
        """定期送信ボタンのトグル判定は GUI(Tk) メインスレッドの command
        コールバックからここで直接（バックグラウンドスレッドを介さず）行う。
        以前はここを threading.Thread でラップしていたが、Tk のボタン
        ダブルクリック等で _on_periodic_click が短時間に2回呼ばれると、
        2つのスレッドが self._periodic_stops.get(idx) をどちらも書き込み前に
        読んでしまい、両方が「未起動」と誤判定してワーカーを二重起動する
        競合があった（2番目の self._periodic_stops[idx] 書き込みが1番目を
        上書きし、1番目の stop_ev が孤立して二度と停止できなくなる）。
        このトグル判定自体（辞書の読み書き・Event生成・スレッド起動）は
        CAN送受信のような時間のかかるI/Oを一切行わないため、Tkのイベント
        ディスパッチ（シングルスレッドで直列化される）に乗せてしまうのが
        最も簡単で確実な排他になる。実際の周期送信ループ（_periodic_can_worker/
        _periodic_uds_worker）は引き続き別スレッドで動くため、GUIは
        ブロックされない。"""
        if not self._require_connected():
            return
        entry_data = {k: v.get() for k, v in self._entry_vars.get(idx, {}).items()}
        label = btn_cfg["label"].replace("\n", " ")
        if btn_cfg["type"] == "raw":
            self._handle_periodic_uds_toggle(btn_cfg, idx, label, entry_data)
        else:
            self._handle_periodic_can_toggle(btn_cfg, idx, label, entry_data)

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
                        next_counter = UdsTesterFrame._next_e2e_counter(UdsTesterFrame._read_e2e_counter(data, e2e_cfg), e2e_cfg)
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
        if sub in (0x02, 0x0A):
            # 応答: [0x59, subFunc, statusAvailMask, (DTC_H,DTC_M,DTC_L,status) x N]
            # DTCレコードは raw[3] から始まる（raw[2] は statusAvailMask であり
            # DTCの一部ではない。以前は raw[2] から読んでおり1バイトずれていた）。
            entries = []
            i = 3
            while i + 4 <= len(raw):
                dtc = (raw[i] << 16) | (raw[i + 1] << 8) | raw[i + 2]
                status = raw[i + 3]
                entries.append(f"{uds_link.dtc_name(dtc)} (status=0x{status:02X})")
                i += 4
            label = "no supported DTC" if sub == 0x0A else "no DTC"
            return "; ".join(entries) if entries else f"({label})"
        if sub == 0x04 and len(raw) >= 7:
            dtc = (raw[2] << 16) | (raw[3] << 8) | raw[4]
            data = " ".join(f"{b:02X}" for b in raw[6:])
            return f"FreezeFrame {uds_link.dtc_name(dtc)} record={raw[5]} data={data}"
        if sub == 0x06 and len(raw) >= 8:
            dtc = (raw[2] << 16) | (raw[3] << 8) | raw[4]
            return f"ExtendedData {uds_link.dtc_name(dtc)} record={raw[5]} occurrence={raw[7]}"
        return " ".join(f"{b:02X}" for b in raw)

    def _decode_request(self, uds: bytes) -> str:
        """送信データボックスの内容（PCIバイトを除いた UDS ペイロード、
        uds[0]=SID）を、_decode_response と対になる簡易説明文へ変換する
        （送信データボックス脇のライブ表示用）。config.json に現れる
        SID/subFunc のみ対応する網羅的でないデコーダで、対応外は
        SID名（不明なら "SID 0xXX"）のみを返す。"""
        if not uds:
            return ""
        sid = uds[0]
        name = uds_link.SID_NAMES.get(sid, f"SID 0x{sid:02X}")
        if sid == 0x10 and len(uds) >= 2:
            return f"{name}: {uds_link.SESSION_NAMES.get(uds[1], f'0x{uds[1]:02X}')}"
        if sid == 0x11 and len(uds) >= 2:
            return f"{name}: {uds_link.RESET_TYPE_NAMES.get(uds[1], f'0x{uds[1]:02X}')}"
        if sid == 0x14 and len(uds) >= 4:
            grp = (uds[1] << 16) | (uds[2] << 8) | uds[3]
            suffix = " (all)" if grp == 0xFFFFFF else ""
            return f"{name}: group=0x{grp:06X}{suffix}"
        if sid == 0x19 and len(uds) >= 2:
            return self._decode_dtc_request(uds)
        if sid == 0x22 and len(uds) >= 3:
            did = (uds[1] << 8) | uds[2]
            return f"{name}: {uds_link.DID_NAMES.get(did, f'DID 0x{did:04X}')}"
        if sid == 0x27 and len(uds) >= 2:
            sub_names = {0x01: "requestSeed", 0x02: "sendKey"}
            sub_name = sub_names.get(uds[1], f"0x{uds[1]:02X}")
            extra = ""
            if uds[1] == 0x02 and len(uds) >= 4:
                extra = f" key=0x{(uds[2] << 8) | uds[3]:04X}"
            return f"{name}: {sub_name}{extra}"
        if sid == 0x28 and len(uds) >= 3:
            ctrl = uds_link.COMM_CONTROL_TYPE_NAMES.get(uds[1], f"0x{uds[1]:02X}")
            comm = uds_link.COMM_TYPE_NAMES.get(uds[2], f"0x{uds[2]:02X}")
            return f"{name}: {ctrl} / {comm}"
        if sid == 0x2E and len(uds) >= 3:
            did = (uds[1] << 8) | uds[2]
            # WriteDataByIdentifier の要求値は ReadDataByIdentifier 応答と
            # 同じ DID→表示ロジック（uds_link.decode_did_value）を流用できる。
            return uds_link.decode_did_value(did, uds[3:])
        if sid == 0x2F and len(uds) >= 4:
            did = (uds[1] << 8) | uds[2]
            did_name = uds_link.DID_NAMES.get(did, f"DID 0x{did:04X}")
            opt_name = uds_link.IOCTRL_OPTION_NAMES.get(uds[3], f"0x{uds[3]:02X}")
            level = f" level={uds[4]}" if len(uds) >= 5 else ""
            return f"{did_name} {opt_name}{level}"
        if sid == 0x31 and len(uds) >= 4:
            sub_name = uds_link.ROUTINE_SUBFUNC_NAMES.get(uds[1], f"0x{uds[1]:02X}")
            rid = (uds[2] << 8) | uds[3]
            return f"{sub_name} RID={rid:04X}"
        if sid == 0x36 and len(uds) >= 2:
            return f"{name}: counter={uds[1]} data={len(uds) - 2}byte"
        return name

    def _decode_dtc_request(self, uds: bytes) -> str:
        sub = uds[1]
        sub_name = uds_link.DTC_SUBFUNC_NAMES.get(sub, f"0x{sub:02X}")
        if sub in (0x01, 0x02) and len(uds) >= 3:
            return f"ReadDTCInformation: {sub_name} mask=0x{uds[2]:02X}"
        if sub in (0x04, 0x06) and len(uds) >= 6:
            dtc = (uds[2] << 16) | (uds[3] << 8) | uds[4]
            return f"ReadDTCInformation: {sub_name} {uds_link.dtc_name(dtc)} record={uds[5]}"
        return f"ReadDTCInformation: {sub_name}"

    def _parse_interval_ms(self, entry_data: dict, btn_cfg, label: str,
                           default: int) -> "int | None":
        """周期(ms)入力欄の値を解釈する。GUI の Entry（entry_data["interval_ms"]）を
        優先し、未入力/パース不能なら btn_cfg の interval_ms、なければ default を使う。
        0 以下や非数値はエラーとしてログへ出し None を返す（呼び出し側は送信開始を
        中止すること）。"""
        raw = entry_data.get("interval_ms")
        if raw is None or raw == "":
            return btn_cfg.get("interval_ms", default)
        try:
            interval_ms = int(raw)
            if interval_ms <= 0:
                raise ValueError
        except ValueError:
            self.log_queue.put(f"[{label}] 周期(ms)の形式エラー: {raw!r}（正の整数を入力してください）")
            return None
        return interval_ms

    # ------------------------------------------------------------------
    # 周期送信の開始/停止トグル（can_frame / raw(UDS) 共通）
    # ------------------------------------------------------------------
    def _toggle_periodic(self, idx: int, label: str, build_worker) -> None:
        """定期送信ボタンの開始/停止判定・_periodic_stops の読み書き・ログ出力・
        ボタン表示更新を1箇所にまとめた共通処理（_handle_periodic_can_toggle/
        _handle_periodic_uds_toggle 双方から呼ぶ）。以前はこの判定ロジックを
        それぞれの関数が個別に実装しており、修正時に片方へだけ適用してしまう
        リスクがあった（コードレビューで指摘）。

        build_worker() は「これから開始する」場合のみ呼ばれ、
        (worker_target, worker_args, start_log_text) を返す。入力エラー等で
        開始できない場合は None を返す（build_worker 内でログ済みのこと）。
        worker_target は最後の引数として stop_ev (threading.Event) を受け取る
        呼び出し規約とする（_periodic_can_worker/_periodic_uds_worker 参照）。

        呼び出し元は GUI(Tk) メインスレッドから直接呼ぶこと（_on_periodic_click
        参照）。バックグラウンドスレッド経由で呼ぶと、ダブルクリック等で
        self._periodic_stops.get(idx) をどちらの呼び出しも書き込み前に読んで
        しまい、両方が「未起動」と誤判定してワーカーを二重起動する競合が
        発生する（2番目の書き込みが1番目を上書きし、1番目の stop_ev が孤立して
        二度と停止できなくなる）。"""
        stop_ev = self._periodic_stops.get(idx)
        if stop_ev is not None and not stop_ev.is_set():
            stop_ev.set()
            self.log_queue.put(f"[{label}] 周期送信 停止")
            self.state_queue.put(("periodic_btn", (idx, "定期")))
            return
        built = build_worker()
        if built is None:
            return
        worker_target, worker_args, start_log_text = built
        new_stop = threading.Event()
        self._periodic_stops[idx] = new_stop
        self.state_queue.put(("periodic_btn", (idx, "停止")))
        self.log_queue.put(start_log_text)
        threading.Thread(
            target=worker_target,
            args=(*worker_args, new_stop),
            daemon=True,
        ).start()

    # ------------------------------------------------------------------
    # 周期 CAN フレーム送信 (can_frame + interval_ms)
    # ------------------------------------------------------------------
    def _handle_periodic_can_toggle(self, btn_cfg, idx: int, label: str, entry_data: dict):
        def build_worker():
            if self.bus is None:
                self.log_queue.put(f"[{label}] 未接続")
                return None
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
                return None
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
            interval_ms = self._parse_interval_ms(entry_data, btn_cfg, label, default=100)
            if interval_ms is None:
                return None
            log_text = (
                f"[{label}] 周期送信 開始 ({interval_ms}ms 間隔)"
                f"  ID=0x{can_id:03X} DATA=" + " ".join(f"{b:02X}" for b in data)
            )
            return (self._periodic_can_worker,
                    (label, can_id, data, interval_ms / 1000.0, e2e_cfg_p, secoc_cfg_p),
                    log_text)
        self._toggle_periodic(idx, label, build_worker)

    # 送信直後に UDS が続いても間隔を保てるよう、送信後にロックを保持する時間 (秒)
    _PERIODIC_POST_SEND_HOLD_S = 0.010  # 10ms

    def _periodic_can_worker(self, label, can_id, data, interval_s,
                              e2e_cfg, secoc_cfg, stop_ev):
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
                            e2e_counter = UdsTesterFrame._next_e2e_counter(e2e_counter, e2e_cfg)
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
    # 周期 UDS request 送信 (raw タイプ、Tester Present 等)
    # ------------------------------------------------------------------
    def _handle_periodic_uds_toggle(self, btn_cfg, idx: int, label: str, entry_data: dict):
        """UDS raw タイプボタン用の周期送信トグル。Tester Present はこの仕組みに
        統合されており、専用のチェックボックスは持たない（コマンド一覧の他の
        ボタンと同じ「定期」トグル+周期(ms)入力欄に一本化した、2026-08）。
        E2E/SecOC カウンタのような複雑な状態は持たない単純な UDS request の
        周期送信専用（multiframe/security_* には使わない）。"""
        def build_worker():
            if self.bus is None:
                self.log_queue.put(f"[{label}] 未接続")
                return None
            try:
                if "data" in entry_data:
                    payload = self._parse_hex_bytes(entry_data["data"])
                else:
                    payload = parse_payload(btn_cfg.get("payload", []))
            except ValueError as exc:
                self.log_queue.put(f"[{label}] 入力エラー: {exc}")
                return None
            interval_ms = self._parse_interval_ms(entry_data, btn_cfg, label, default=2000)
            if interval_ms is None:
                return None
            log_text = (
                f"[{label}] 周期送信 開始 ({interval_ms}ms 間隔)"
                f"  DATA=" + " ".join(f"{b:02X}" for b in payload)
            )
            return self._periodic_uds_worker, (payload, interval_ms / 1000.0), log_text
        self._toggle_periodic(idx, label, build_worker)

    def _periodic_uds_worker(self, payload: bytes, interval_s: float,
                              stop_ev: threading.Event) -> None:
        """interval_s ごとに UDS request を送り応答を読み捨て続ける
        （_periodic_can_worker() と同じ send-then-wait の順序）。
        _periodic_can_worker() と異なり bus_lock を完全に取得してブロッキングで
        送信+応答受信する（低頻度前提のため非blocking排他は不要で、応答を
        受信/破棄しておくことで他の RX 処理と取りこぼしを起こさない）。"""
        while True:
            if self.bus is not None:
                with self.bus_lock:
                    try:
                        uds_link.send_raw(self.bus, payload)
                        uds_link.receive_uds_response(self.bus, timeout=1.0)
                    except Exception:  # noqa: BLE001 - 周期送信中の一時エラーは無視して継続する
                        pass
            if stop_ev.wait(interval_s):
                break

    # ------------------------------------------------------------------
    # 受信モニター (rx_monitor)
    #
    # デコードは data/can_signals.json（tools/can_signal_editor/ で編集する、
    # CANフレームのビットレイアウト定義）を情報源にする。以前はフレームごとに
    # 手書きのビット演算デコード関数を持っていたが、フレーム追加のたびに
    # ここへも手を入れる必要があった。can_signals.json に定義さえあれば
    # 自動でデコードされるようにし、その二重管理を無くす。
    # ------------------------------------------------------------------
    @staticmethod
    def _extract_bits(data: bytes, bit_position: int, bit_size: int) -> int:
        """can_signals.json のビット位置規約（ビット0=byte[0]のMSB、AUTOSAR Com の
        ビッグエンディアン規約と同じ）で data から bit_size 分を抽出する。
        バイト境界をまたぐフィールド（16bit のシグナル等）にも対応する。"""
        value = 0
        for i in range(bit_size):
            byte_idx, bit_in_byte = divmod(bit_position + i, 8)
            if byte_idx >= len(data):
                break
            value = (value << 1) | ((data[byte_idx] >> (7 - bit_in_byte)) & 1)
        return value

    # can_signals.json 上でプロトコルオーバーヘッド（アプリケーションデータでは
    # ない）として扱う型。raw hex 表示（呼び出し元が別途出す）だけで十分なため
    # _decode_frame_generic() では除外する。ここに無い型（"number" は当然、
    # 将来 tools/can_signal_editor/ の TYPE_SCHEMA に新しい型が増えた場合も
    # 含む）は、専用の表示ロジックが無くても生値だけは出す（下記フォール
    # バック）。allowlist（既知の型だけ表示）にすると、新しい型が増えるたびに
    # ここも直さない限りサイレントに何も表示されなくなり、まさにこの統合が
    # 解消しようとした「2箇所を手で同期させる」問題が型の次元で再発するため。
    _HIDDEN_FIELD_TYPES = frozenset({"e2e_crc", "e2e_counter", "secoc_freshness", "secoc_mac"})

    @staticmethod
    def _decode_frame_generic(frame_def: dict, data: bytes) -> str:
        """can_signals.json のフレーム定義1件から、_HIDDEN_FIELD_TYPES 以外の
        フィールドを "name=値" の列挙文字列にデコードする。

        data がフレーム定義の DLC より短い（バス上の異常・誤配線等でフレームが
        欠落/切り詰められた）場合、_extract_bits() は範囲外のバイトを単に
        シフトしないため、欠けたフィールドはあたかも 0 が届いたかのような
        値になってしまう（例: 本来のエラーカウンタが実は届いていないのに
        "0件" と表示され、バスの異常自体を見えなくしてしまう）。それを避けるため、
        (1) フレーム全体の長さが DLC 未満なら先頭にその旨を明示し、
        (2) 個々のフィールドも data に収まりきらない場合はデコードせず省く
        （0 埋めで存在するかのような値を返さない）。"""
        parts = []
        expected_dlc = frame_def.get("dlc")
        if isinstance(expected_dlc, int) and len(data) < expected_dlc:
            parts.append(f"!DLC不足 期待={expected_dlc}byte 実際={len(data)}byte!")
        for f in frame_def.get("fields", []):
            t = f.get("type")
            if t in UdsTesterFrame._HIDDEN_FIELD_TYPES:
                continue
            bit_position, bit_size = f["bitPosition"], f["bitSize"]
            if bit_position + bit_size > len(data) * 8:
                continue  # data がここまで届いていない（0埋めで偽の値を出さない）
            raw = UdsTesterFrame._extract_bits(data, bit_position, bit_size)
            if t == "enum":
                label = next((e["label"] for e in f.get("enum", []) if e["value"] == raw), f"0x{raw:X}")
                parts.append(f"{f['name']}={label}")
            else:
                # "number" 型、および未知の型（将来 TYPE_SCHEMA が増えた場合）の
                # フォールバック。:g で末尾の ".0" だけ落とす（int/float どちらでも動く）。
                val = raw * (f.get("scale") or 1)
                parts.append(f"{f['name']}={val:g}{f.get('unit', '')}")
        return "(" + " ".join(parts) + ")" if parts else ""

    @staticmethod
    def _draw_round_gauge_face(canvas: tk.Canvas, cx: float, cy: float, r: float) -> None:
        """円形ゲージの固定部分（円弧＋目盛り＋ピボット）を一度だけ描く汎用ヘルパー
        （タコメータ・水温ゲージで共通）。

        実車の多くのアナログ式メータ（タコ/スピード/水温いずれも）は、半円(180°)でも
        全円(360°)でもなく、文字盤下寄りに軸(ピボット)を置いた240°程度の弧
        （時計の8時位置=最小値 → 12時位置=中間値 → 4時位置=最大値、と時計回りに
        上を通ってスイープする）が一般的（全円だと最小値と最大値が隣接して見分け
        づらくなるため、ニードル式では基本的に採用されない。水温計は実車でも
        タコ/スピードと同じ丸形メータとして配置されることが多く、この形自体は
        不自然ではない）。本実装も同じ配置にし、8時位置から4時位置まで240°の弧を
        描く。

        数学角度（反時計回り、東=0°、y上向き）では 8時位置=210°、12時位置=90°、
        4時位置=-30° に対応する。tkinter の create_arc は start/extent を見た目の
        反時計回り角度として扱う（内部の y 下向き座標系との差はキャンバス自身が
        吸収する）ため、start=-30, extent=240 とすれば 4時位置から反時計回りに
        （見た目は3,2,1,12,11,10,9時の順に）210°(8時位置)まで弧が続く
        （弧そのものは対称なので、どちらの端を start にしても同じ弧になる）。
        針や目盛りは create_line で座標を直接計算するため、y だけ符号を反転して
        数学角度→キャンバス座標に変換する必要がある（_gauge_needle_endpoint() も
        同じ変換を使う）。"""
        canvas.create_arc(cx - r, cy - r, cx + r, cy + r, start=-30, extent=240,
                           style="arc", width=8, outline="#333333")
        for i in range(9):
            theta = math.radians(210 - i * 30)
            x0 = cx + (r - 10) * math.cos(theta)
            y0 = cy - (r - 10) * math.sin(theta)
            x1 = cx + r * math.cos(theta)
            y1 = cy - r * math.sin(theta)
            canvas.create_line(x0, y0, x1, y1, fill="#333333", width=2)
        canvas.create_oval(cx - 4, cy - 4, cx + 4, cy + 4, fill="#333333", outline="")

    @staticmethod
    def _gauge_needle_endpoint(cx: float, cy: float, length: float,
                                value: float, value_max: float) -> tuple[float, float]:
        """value(0～value_max)に対応する針の先端座標を返す。
        _draw_round_gauge_face() と同じ配置（210°=最小値(8時位置) ～
        -30°=最大値(4時位置)、時計回りに12時位置を通ってスイープ＝画面上は
        左から右へ移動して見える）。value は呼び出し側で 0～value_max に
        クランプ済みであること。"""
        theta = math.radians(210 - (value / value_max) * 240)
        return cx + length * math.cos(theta), cy - length * math.sin(theta)

    def _draw_tacho_gauge_static(self) -> None:
        """タコメータ用ゲージの固定部分を描き、数値表示テキスト項目を作成する。"""
        cx, cy, r = self._tacho_cx, self._tacho_cy, self._tacho_r
        c = self.meter_tacho_canvas
        self._draw_round_gauge_face(c, cx, cy, r)
        # 数値表示（実車の多くのアナログメータが文字盤内の下寄りにデジタル/文字
        # 表示を持つのに合わせ、ピボット直下・弧が開いている領域に置く）。
        self.meter_tacho_rpm_text = c.create_text(
            cx, cy + r * 0.55, text="---- rpm", font=("", 11, "bold"), fill="#222222")

    def _update_tacho_needle(self, rpm: int) -> None:
        """rpm(0～meter_rpm_max)に応じて針の先端座標と中央下の数値表示を更新する
        （弧・目盛りは再描画しない）。数値表示は針のクランプ前の実際の rpm を
        示す（針の位置だけ範囲外をクランプする）。"""
        rpm_clamped = max(0, min(rpm, self.meter_rpm_max))
        cx, cy, r = self._tacho_cx, self._tacho_cy, self._tacho_r
        x1, y1 = self._gauge_needle_endpoint(cx, cy, r - 12, rpm_clamped, self.meter_rpm_max)
        c = self.meter_tacho_canvas
        if self.meter_tacho_needle is None:
            self.meter_tacho_needle = c.create_line(cx, cy, x1, y1, fill="red", width=3)
        else:
            c.coords(self.meter_tacho_needle, cx, cy, x1, y1)
        if self.meter_tacho_rpm_text is not None:
            c.itemconfig(self.meter_tacho_rpm_text, text=f"{rpm} rpm")

    def _draw_coolant_gauge_static(self) -> None:
        """水温ゲージ用の固定部分を描き、数値表示テキスト項目を作成する
        （_draw_tacho_gauge_static() と同じ構成、タコメータより小さいゲージ）。"""
        cx, cy, r = self._coolant_cx, self._coolant_cy, self._coolant_r
        c = self.meter_coolant_canvas
        self._draw_round_gauge_face(c, cx, cy, r)
        self.meter_coolant_gauge_text = c.create_text(
            cx, cy + r * 0.55, text="-- °C", font=("", 10, "bold"), fill="#222222")

    def _update_coolant_needle(self, temp: int) -> None:
        """temp(0～meter_coolant_max)に応じて水温ゲージの針と数値表示を更新する
        （_update_tacho_needle() と同じ考え方）。"""
        temp_clamped = max(0, min(temp, self.meter_coolant_max))
        cx, cy, r = self._coolant_cx, self._coolant_cy, self._coolant_r
        x1, y1 = self._gauge_needle_endpoint(cx, cy, r - 8, temp_clamped, self.meter_coolant_max)
        c = self.meter_coolant_canvas
        if self.meter_coolant_needle is None:
            self.meter_coolant_needle = c.create_line(cx, cy, x1, y1, fill="red", width=2)
        else:
            c.coords(self.meter_coolant_needle, cx, cy, x1, y1)
        if self.meter_coolant_gauge_text is not None:
            c.itemconfig(self.meter_coolant_gauge_text, text=f"{temp} °C")

    def _update_virtual_meter(self, data: bytes) -> None:
        """MeterStatus (CAN 0x200, DLC=6) の byte[2]=警告灯3bitミラー・
        byte[3-4]=EngineSpeedミラー・byte[5]=CoolantTempミラーから仮想メータ
        表示を更新する。ビット配置は WarningStatus と同じ MSB 起点（bit0=MSB）:
        byte[2] bit0=RunLamp, bit1=FaultLamp, bit2=AbsLamp。
        byte[3-4] はビッグエンディアン 16bit（EngineInfo の EngineSpeed と同じ単位・rpm）。
        byte[5] は EngineInfo の CoolantTemp と同じ単位（°C）。"""
        run = (data[2] >> 7) & 1
        fault = (data[2] >> 6) & 1
        abs_ = (data[2] >> 5) & 1
        rpm = (data[3] << 8) | data[4]

        self._update_tacho_needle(rpm)
        self.meter_run_lbl.configure(bg="green" if run else "gray85")
        self.meter_fault_lbl.configure(bg="red" if fault else "gray85")
        self.meter_abs_lbl.configure(bg="orange" if abs_ else "gray85")
        self._update_coolant_needle(data[5])

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

        serial_lines = []
        while True:
            try:
                serial_lines.append(self.serial_log_queue.get_nowait())
            except queue.Empty:
                break
        if serial_lines:
            # Arduino 側が既に [<ms>ms] 形式のタイムスタンプを付けているため、
            # 上記「ログ」（ツール自身の送受信ログ）と違い PC 側の時刻は付けない。
            # まとめて1回で insert することで、Arduino起動時の大量ログ等
            # バーストで届いた場合に Tk ウィジェット操作を行単位で繰り返さない。
            self.serial_log_text.configure(state="normal")
            self.serial_log_text.insert("end", "\n".join(serial_lines) + "\n")
            self.serial_log_text.see("end")
            self.serial_log_text.configure(state="disabled")

        while True:
            try:
                kind, value = self.state_queue.get_nowait()
            except queue.Empty:
                break
            if kind == "resp":
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
            elif kind == "serial_state":
                tag, state = value
                if tag in self._serial_state_vars:
                    self._serial_state_vars[tag].set(state)
            elif kind == "serial_error":
                # ワーカースレッド自身は Tk ウィジェットに触れないため、
                # GUI リセットはここ（メインスレッド）でまとめて行う
                # （_serial_mark_disconnected 参照）。ECU 状態パネルの値は
                # 「最後に確認できていた状態」として意図的にそのまま残す
                # （切断自体はステータス表示の赤/Connect ボタンで分かるため）。
                self._serial_thread = None
                self._serial_mark_disconnected()
            elif kind == "rx_mon":
                mon_idx, data = value
                if mon_idx in self._rx_monitor_vars:
                    # _rx_monitor_vars/_rx_monitor_name_vars/_rx_monitor_ids は
                    # ボタン構築時（_build_widgets 内、rx_monitor 種別）に同じ
                    # インデックスへ常に3つ揃って登録される（別々に存在することは
                    # ない）ため、ここに来た時点で他の2つの存在確認は不要。
                    raw_hex = " ".join(f"{b:02X}" for b in data)
                    self._rx_monitor_vars[mon_idx].set(raw_hex)
                    can_id = self._rx_monitor_ids[mon_idx]
                    frame_def = self.signal_defs.get(can_id)
                    self._rx_monitor_name_vars[mon_idx].set(
                        self._decode_frame_generic(frame_def, data) if frame_def is not None else ""
                    )
                    # MeterStatus (CAN 0x200) の仮想メータ（丸ゲージ）表示だけは
                    # can_signals.json 上の汎用デコードとは別物（テキストではなく
                    # 針の描画）で、この機能自体は元々 JSON に依存していなかった。
                    # signal_defs の読み込み成否（上の if/elif）とは独立に判定する
                    # ことで、can_signals.json が読めない/MeterStatus定義が
                    # 一時的に無い場合でもゲージ表示だけは動き続けるようにする。
                    if can_id == 0x200 and len(data) >= 6:
                        self._update_virtual_meter(data)

        self.after(100, self._poll_queues)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH)
    args = parser.parse_args()

    root = tk.Tk()
    root.title("UDS Button Tester")
    root.geometry("1100x650")
    frame = UdsTesterFrame(root, args.config)
    frame.pack(fill=tk.BOTH, expand=True)
    root.mainloop()


if __name__ == "__main__":
    main()

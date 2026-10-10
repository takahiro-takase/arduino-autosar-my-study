"""
コンフィグレータ（tools/configurator/）の実行パネル

信号定義エディタと設定 JSON エディタの両方に置く、「検査 / 同期の確認 / 同期 / 生成」のボタンと
結果ログの欄。スクリプトは別プロセスで実行し、結果を色付きでログへ出す。
"""
from __future__ import annotations

import os
import queue
import subprocess
import sys
import threading
import tkinter as tk
from typing import Callable
from tkinter import ttk

# コンフィグレータ（tools/configurator/）。信号表の変更に合わせて、各モジュールの設定 json の追加と、
# Cfg ソースの生成を行う。
CONFIGURATOR_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "configurator")
)
# 設定データ（config/data/*.json）とそのスキーマ（config/schema/*.schema.json）は、リポジトリ直下の config/ にある
REPO_ROOT = os.path.normpath(os.path.join(CONFIGURATOR_DIR, "..", ".."))
CONFIG_DIR = os.path.join(REPO_ROOT, "config", "data")
SCHEMA_DIR = os.path.join(REPO_ROOT, "config", "schema")

# 実行結果の行頭に応じた色（先頭一致）
LOG_TAGS = (
    ("整合性エラー", "error"), ("設定エラー", "error"), ("生成エラー", "error"), ("エラー", "error"),
    ("  - ", "error"), ("警告", "warn"), ("要確認", "note"), ("追加", "ok"),
)


class ConfiguratorPanel(ttk.LabelFrame):
    """検査・同期・生成を実行して、結果をログ欄に出すパネル。

    get_signals_path: 信号表（config/data/can_signals.json）のパスを返す関数。
    before_run:       実行前に呼ぶ関数。False を返すと実行を取りやめる（未保存の変更を確認するために使う）。
    """

    def __init__(self, master: tk.Misc, get_signals_path: Callable[[], str],
                 before_run: Callable[[], bool] | None = None, *, log_height: int = 8):
        super().__init__(master, text="コンフィグレータ（信号表を保存 → 同期 → 生成）", padding=4)
        self._get_signals_path = get_signals_path
        self._before_run = before_run or (lambda: True)
        self._queue: queue.Queue = queue.Queue()
        self._running = False

        btns = ttk.Frame(self)
        btns.pack(fill=tk.X)
        self._buttons = [
            ttk.Button(btns, text="検査", command=lambda: self.run("検査", "gen_cfg.py", ["--check"])),
            ttk.Button(btns, text="同期の確認 (dry-run)", command=lambda: self.run("同期の確認", "sync_cfg.py", ["--dry-run"])),
            ttk.Button(btns, text="同期", command=lambda: self.run("同期", "sync_cfg.py", [])),
            ttk.Button(btns, text="生成", command=lambda: self.run("生成", "gen_cfg.py", [])),
        ]
        for i, btn in enumerate(self._buttons):
            btn.pack(side=tk.LEFT, padx=(0, 4) if i < len(self._buttons) - 1 else 0)
        self.status_var = tk.StringVar(value="")
        ttk.Label(btns, textvariable=self.status_var, foreground="#666").pack(side=tk.LEFT, padx=12)

        wrap = ttk.Frame(self)
        wrap.pack(fill=tk.BOTH, expand=True, pady=(4, 0))
        self.log = tk.Text(wrap, height=log_height, wrap="word", state="disabled")
        scroll = ttk.Scrollbar(wrap, orient=tk.VERTICAL, command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scroll.pack(side=tk.LEFT, fill=tk.Y)
        self.log.tag_configure("error", foreground="#B00")
        self.log.tag_configure("warn", foreground="#A60")
        self.log.tag_configure("note", foreground="#06C")
        self.log.tag_configure("ok", foreground="#070")
        self.log.tag_configure("head", font=("TkDefaultFont", 9, "bold"))

    @property
    def running(self) -> bool:
        return self._running

    def log_text(self) -> str:
        return self.log.get("1.0", tk.END)

    # ------------------------------------------------------------------
    def command(self, script: str, extra: list[str]) -> list[str]:
        """tools/configurator/ のスクリプトを、指定の信号表に対して実行するコマンド。"""
        return [sys.executable, os.path.join(CONFIGURATOR_DIR, script), "--signals", self._get_signals_path()] + extra

    @staticmethod
    def run_command(cmd: list[str]) -> tuple[int, str]:
        """コマンドを実行し、(終了コード, 標準出力+標準エラー) を返す。出力は UTF-8 で受け取る。"""
        env = dict(os.environ, PYTHONIOENCODING="utf-8")
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        try:
            proc = subprocess.run(cmd, capture_output=True, env=env, creationflags=flags, timeout=120)
        except (OSError, subprocess.SubprocessError) as e:
            return 1, "実行できませんでした: %s\n" % e
        out = proc.stdout.decode("utf-8", errors="replace") + proc.stderr.decode("utf-8", errors="replace")
        return proc.returncode, out

    def append(self, text: str, *, head: bool = False) -> None:
        self.log.configure(state="normal")
        for line in text.splitlines():
            tag = "head" if head else next((tg for prefix, tg in LOG_TAGS if line.startswith(prefix)), None)
            self.log.insert(tk.END, line + "\n", (tag,) if tag else ())
        self.log.see(tk.END)
        self.log.configure(state="disabled")

    def run(self, title: str, script: str, extra: list[str]) -> None:
        """同期 / 生成 / 検査をバックグラウンドで実行し、結果をログ欄へ出す。"""
        if self._running:
            return
        if not self._before_run():
            return
        self._running = True
        for btn in self._buttons:
            btn.state(["disabled"])
        self.status_var.set("実行中: %s ..." % title)
        self.append("=== %s ===" % title, head=True)
        cmd = self.command(script, extra)

        def worker() -> None:
            self._queue.put((title, *self.run_command(cmd)))

        threading.Thread(target=worker, daemon=True).start()
        self.after(100, self._poll)

    def _poll(self) -> None:
        try:
            title, rc, out = self._queue.get_nowait()
        except queue.Empty:
            self.after(100, self._poll)
            return
        self.append(out)
        if rc == 0:
            self.status_var.set("完了: %s" % title)
        elif rc == 1:
            self.status_var.set("要対応（差分または警告あり）: %s" % title)
        else:
            self.status_var.set("エラー（終了コード %d）: %s" % (rc, title))
        self._running = False
        for btn in self._buttons:
            btn.state(["!disabled"])

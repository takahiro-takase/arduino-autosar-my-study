"""
CAN Tool（統合ランチャー）

uds_tester_app.py（UDS診断GUI）、can_signal_editor_app.py（CAN信号定義エディタ）、
config_editor_app.py（設定 JSON エディタ）を、1つのウィンドウにまとめたもの。いずれも ttk.Frame ベースで、
本ファイルの 1 ページとして埋め込む前提の同一パッケージ内モジュールとして同居する（単体起動は廃止済み）。

画面の左側にナビゲーション（縦に並んだ短い一覧）を置き、選んだ項目のページを右側に表示する。
設定 JSON は、通信スタックの上位から下位へ（Com → PduR → CanIf → Can）など、全体の流れが分かる順に並べ、
ブロックの間に区切り線を入れる。各行の日本語の説明は、右側のページの上部に出す。

使い方:
    python src/app.py [--data ../../config/data/can_signals.json] [--config ../config.json]
"""
from __future__ import annotations

import argparse
import os
import tkinter as tk
from tkinter import messagebox, ttk
from typing import NamedTuple

import can_signal_editor_app
import config_editor_app
import configurator_panel
import uds_tester_app

NAV_WIDTH = 190  # 左側のナビゲーションの幅 [px]
SEPARATOR = "─" * 12
START_PAGE = "Tester"  # 起動直後に表示するページ


class NavRow(NamedTuple):
    """左側のナビゲーションの、設定 JSON の行。"""
    file: str | None        # 設定 json のファイル名。None は json を持たない行（補足。選べない）
    label: str              # 一覧に出す短い名前
    caption: str            # 右側のページの上部に出す日本語の説明
    focus: tuple = ()       # 開いたときに選ぶ項目の候補（先頭から探す）。1 つの json を複数の行から開ける


# 設定 json の並べ方。上位のレイヤから下位へ、全体の流れが分かる順にし、ブロックの間に区切り線を入れる。
NAV_BLOCKS = (
    (
        NavRow("Com.json", "Com", "Com — 信号・I-PDU・送信モード・コールバック"),
        NavRow("PduR.json", "PduR", "PduR — PDU のルーティング"),
        NavRow("CanIf.json", "CanIf", "CanIf — PDU と CAN ID の対応"),
        NavRow(None, "Can", "Can — コントローラ設定（json なし。手書き）"),
    ),
    (
        NavRow("E2EXf.json", "E2EXf", "E2EXf — E2E 保護（Profile05）"),
        NavRow("SecOC.json", "SecOC", "SecOC — 認証（Secured I-PDU）"),
    ),
    (
        NavRow("CryptoStack.json", "Csm", "Csm — 暗号サービス（ジョブ）。Crypto・KeyM と同じ設定ファイル", (("jobs",),)),
        NavRow("CryptoStack.json", "Crypto", "Crypto — 暗号ドライバ（鍵）。Csm・KeyM と同じ設定ファイル", (("keys",),)),
        NavRow("CryptoStack.json", "KeyM", "KeyM — 鍵名の管理。Csm・Crypto と同じ設定ファイル", (("keys", 0, "keyM"), ("keys",))),
    ),
    (NavRow("BswM.json", "BswM", "BswM — モード遷移のルール・I-PDU グループの起動/停止"),),
)


def nav_blocks(names: list[str]) -> list[tuple[NavRow, ...]]:
    """実在する設定 json の行だけを残したブロックの列。NAV_BLOCKS に無い設定 json は、末尾のブロックにまとめる。"""
    known = {row.file for block in NAV_BLOCKS for row in block}
    blocks = []
    for block in NAV_BLOCKS:
        rows = tuple(r for r in block if r.file is None or r.file in names)
        if any(r.file for r in rows):  # json の行が 1 つも無いブロックは、補足の行も含めて出さない
            blocks.append(rows)
    rest = []
    for n in names:
        if n not in known:
            stem = os.path.splitext(n)[0]
            rest.append(NavRow(n, stem, stem))
    if rest:
        blocks.append(tuple(rest))
    return blocks


class CanToolApp:
    """左側のナビゲーションと、右側のページ（Tester / Signals / 設定 JSON）。"""

    def __init__(self, root: tk.Tk, data_path: str, config_path: str):
        self.root = root
        self._config_rows: dict[str, NavRow] = {}   # 設定 json の行 ID → 行の定義
        self._current: str | None = None            # いま表示している行 ID

        style = ttk.Style(root)
        style.configure("Nav.Treeview", rowheight=28, indent=4)

        paned = ttk.PanedWindow(root, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)
        nav_wrap = ttk.Frame(paned, width=NAV_WIDTH)
        nav_wrap.pack_propagate(False)
        paned.add(nav_wrap, weight=0)
        self.container = ttk.Frame(paned)
        self.container.grid_rowconfigure(0, weight=1)
        self.container.grid_columnconfigure(0, weight=1)
        paned.add(self.container, weight=1)

        self.nav = ttk.Treeview(nav_wrap, show="tree", selectmode="browse", style="Nav.Treeview")
        self.nav.pack(fill=tk.BOTH, expand=True)
        self.nav.column("#0", width=NAV_WIDTH - 4, stretch=True)
        self.nav.tag_configure("dim", foreground="#999")

        # --- ページ ---
        # 各ページは、未保存かどうかが変わるたびに on_title_change を呼ぶ（コンストラクタの完了後）。
        # ここでは、その知らせを合図に、各ページの dirty を読んで行の印（*）を付け直す。
        # ensure_all_saved は、コンフィグレータの実行前に、どちらのページの未保存の変更も確認するための関数
        # （ツールは保存済みのファイルを読むため）。
        self.editor_frame = can_signal_editor_app.CanSignalEditorFrame(
            self.container, data_path, on_title_change=lambda _t: self._refresh_marks(),
            ensure_all_saved=self._ensure_all_saved)
        self.config_frame = config_editor_app.ConfigEditorFrame(
            self.container, configurator_panel.CONFIG_DIR, lambda: data_path,
            on_title_change=lambda _t: self._refresh_marks(), ensure_all_saved=self._ensure_all_saved)
        self.tester_frame = uds_tester_app.UdsTesterFrame(self.container, config_path)
        self._frames = (self.editor_frame, self.config_frame, self.tester_frame)
        self.pages = {"Tester": self.tester_frame, "Signals": self.editor_frame}
        for page in self._frames:
            page.grid(row=0, column=0, sticky="nsew")

        # --- ナビゲーション ---
        for name in self.pages:
            self.nav.insert("", tk.END, iid=name, text=name)
        for block in nav_blocks(self.config_frame.names):
            self.nav.insert("", tk.END, text=SEPARATOR, tags=("dim",))
            for row in block:
                if row.file is None:
                    self.nav.insert("", tk.END, text=row.label, tags=("dim",))
                else:
                    iid = "%s#%s" % (row.file, row.label)
                    self.nav.insert("", tk.END, iid=iid, text=row.label)
                    self._config_rows[iid] = row
        self.nav.bind("<<TreeviewSelect>>", self._on_select)

    # ------------------------------------------------------------------
    def _ensure_all_saved(self) -> bool:
        return self.editor_frame.ensure_saved() and self.config_frame.ensure_saved()

    def _set_mark(self, iid: str, dirty: bool) -> None:
        base = self.nav.item(iid, "text").removesuffix(" *")
        self.nav.item(iid, text=base + (" *" if dirty else ""))

    def _refresh_marks(self) -> None:
        """各ページの未保存の状態を、行の表示名の印（*）に反映する。設定 JSON は、開いているファイルの行すべてに付ける。"""
        self._set_mark("Signals", self.editor_frame.dirty)
        current = self.config_frame.current_file
        for iid, row in self._config_rows.items():
            self._set_mark(iid, self.config_frame.dirty and row.file == current)

    def select(self, iid: str) -> None:
        """行を選んで、対応するページを表示する（プログラムからの切り替えにも使う）。"""
        self.nav.selection_set(iid)

    def _on_select(self, _event: object = None) -> None:
        sel = self.nav.selection()
        if not sel or sel[0] == self._current:   # 選択を元へ戻したときの再通知も、ここで止まる
            return
        iid = sel[0]
        row = self._config_rows.get(iid)
        if row is not None:
            try:
                shown = self.config_frame.show(row.file, row.caption, row.focus)
            except (OSError, ValueError, KeyError) as e:
                # 設定 json またはスキーマの読み込みに失敗した（壊れている、消えているなど）。表示は元のページのまま
                messagebox.showerror("エラー", "設定を開けませんでした: %s\n%s: %s" % (row.file, type(e).__name__, e))
                shown = False
            if not shown:                          # 失敗、または未保存の変更の破棄を取りやめた
                self._restore_selection()
                return
            self.config_frame.tkraise()
            self._refresh_marks()
        elif iid in self.pages:
            self.pages[iid].tkraise()
        else:
            self._restore_selection()              # 区切り線や補足の行は、ページを持たない
            return
        self._current = iid

    def _restore_selection(self) -> None:
        """選択を、いま表示している行へ戻す（まだ何も表示していなければ、選択を外す）。"""
        if self._current:
            self.nav.selection_set(self._current)
        else:
            self.nav.selection_remove(self.nav.selection())

    def confirm_close(self) -> bool:
        # 各ページは confirm_close() を実装していれば、閉じてよいかを判断できる
        return all(getattr(page, "confirm_close", lambda: True)() for page in self._frames)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=can_signal_editor_app.DEFAULT_DATA_PATH)
    parser.add_argument("--config", default=uds_tester_app.DEFAULT_CONFIG_PATH)
    args = parser.parse_args()

    root = tk.Tk()
    root.title("CAN Tool")
    root.geometry("1500x850")

    app = CanToolApp(root, args.data, args.config)
    app.select(START_PAGE)

    def _on_close() -> None:
        if app.confirm_close():
            root.destroy()

    root.protocol("WM_DELETE_WINDOW", _on_close)
    root.mainloop()


if __name__ == "__main__":
    main()

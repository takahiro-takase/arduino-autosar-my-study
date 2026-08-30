"""
CAN Tool（統合ランチャー）

can_signal_editor_app.py（CAN信号定義エディタ）と uds_tester_app.py（UDS診断GUI）を
1つのウィンドウの2タブにまとめたもの。両ツールは元々 tk.Tk を継承した単体アプリ
だったが、それぞれ ttk.Frame ベースに変換済みで、本ファイルの1タブとして
埋め込む前提の同一パッケージ内モジュールとして同居する（単体起動は廃止済み）。

使い方:
    python src/app.py [--data ../../data/can_signals.json] [--config ../config.json]
"""
from __future__ import annotations

import argparse
import tkinter as tk
from tkinter import ttk

import can_signal_editor_app
import uds_tester_app


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=can_signal_editor_app.DEFAULT_DATA_PATH)
    parser.add_argument("--config", default=uds_tester_app.DEFAULT_CONFIG_PATH)
    args = parser.parse_args()

    root = tk.Tk()
    root.title("CAN Tool")
    root.geometry("1400x850")

    notebook = ttk.Notebook(root)
    notebook.pack(fill=tk.BOTH, expand=True)

    # このコールバックは editor_frame（この代入文の左辺）自身を参照するが、
    # CanSignalEditorFrame は初回のタイトル通知を __init__ 内で同期的に呼ばず
    # after_idle() で遅延させているため、実際に呼ばれる時点では代入は完了して
    # おり問題ない（コンストラクタ引数として直接渡してよい）。
    # ttk.Notebook.tab() の tab_id には管理下の子ウィジェット自身を渡せる
    # （文字列パス名を自前で払い出す必要はない）。
    editor_frame = can_signal_editor_app.CanSignalEditorFrame(
        notebook, args.data,
        on_title_change=lambda text: notebook.tab(editor_frame, text=text),
    )
    notebook.add(editor_frame)

    tester_frame = uds_tester_app.UdsTesterFrame(notebook, args.config)
    notebook.add(tester_frame, text="UDS Tester")
    notebook.select(tester_frame)  # 起動直後に表示するタブ（UDS Tester を優先）

    tabs = (editor_frame, tester_frame)

    def _on_close() -> None:
        # 各タブは confirm_close() を実装していれば閉じてよいかを判断できる
        # （現状はeditor_frameのみ未保存確認を持つが、将来他のタブが同種の
        # 確認を持っても、ここを個別に書き換えずに済むようにする）。
        if all(getattr(tab, "confirm_close", lambda: True)() for tab in tabs):
            root.destroy()

    root.protocol("WM_DELETE_WINDOW", _on_close)
    root.mainloop()


if __name__ == "__main__":
    main()

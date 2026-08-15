"""
CAN Tool（統合ランチャー）

tools/can_signal_editor（CAN信号定義エディタ）と tools/uds_tester（UDS診断GUI）を
1つのウィンドウの2タブにまとめたもの。両ツールは元々 tk.Tk を継承した単体アプリ
だったが、それぞれ ttk.Frame ベースに変換済みで、単体起動（各ツールの
src/app.py 直接実行）と、このランチャーからの埋め込み起動の両方に対応する。

使い方:
    python src/app.py [--data ../../../data/can_signals.json] [--config ../../uds_tester/config.json]
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import sys
import tkinter as tk
from tkinter import ttk
from types import ModuleType

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_TOOLS_DIR = os.path.normpath(os.path.join(_THIS_DIR, "..", ".."))
_EDITOR_SRC_DIR = os.path.join(_TOOLS_DIR, "can_signal_editor", "src")
_TESTER_SRC_DIR = os.path.join(_TOOLS_DIR, "uds_tester", "src")


def _load_module(name: str, src_dir: str) -> ModuleType:
    """src_dir/app.py を name というモジュール名でロードする。can_signal_editor と
    uds_tester はどちらもファイル名が app.py で、それぞれパッケージ化されて
    いない（uds_tester 側は `import capl_api` 等の兄弟インポート前提）ため、
    通常の `import app` では名前が衝突する。spec_from_file_location で別名
    ロードし、かつロード前に src_dir を sys.path に加えることで、uds_tester の
    兄弟インポートも解決できるようにする。"""
    if src_dir not in sys.path:
        sys.path.insert(0, src_dir)
    spec = importlib.util.spec_from_file_location(name, os.path.join(src_dir, "app.py"))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def main() -> None:
    editor_mod = _load_module("can_signal_editor_app", _EDITOR_SRC_DIR)
    tester_mod = _load_module("uds_tester_app", _TESTER_SRC_DIR)

    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=editor_mod.DEFAULT_DATA_PATH)
    parser.add_argument("--config", default=tester_mod.DEFAULT_CONFIG_PATH)
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
    editor_frame = editor_mod.CanSignalEditorFrame(
        notebook, args.data,
        on_title_change=lambda text: notebook.tab(editor_frame, text=text),
    )
    notebook.add(editor_frame)

    tester_frame = tester_mod.UdsTesterFrame(notebook, args.config)
    notebook.add(tester_frame, text="UDS Tester")

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

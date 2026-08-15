"""
CAN 信号定義エディタ (GUI)

data/can_signals.json（README.md の「CAN フレーム仕様」表を一元管理するための
データソース）を Excel 風の表形式で表示・編集する。フレーム一覧とフィールド
一覧を2段のグリッドで表示し、セルのダブルクリックでインライン編集する。

使い方:
    python src/app.py [--data ../../data/can_signals.json]
"""
from __future__ import annotations

import argparse
import json
import os
import tkinter as tk
from tkinter import messagebox, ttk

# type ごとに保持しうる専用キー。field_detail_text()/_edit_field_detail()/
# _on_field_cell_commit() の型変更クリーンアップが、この1箇所を共通の情報源とする。
TYPE_SCHEMA: dict[str, tuple[str, ...]] = {
    "enum": ("enum",),
    "number": ("unit", "scale", "range"),
    "e2e_crc": (),
    "e2e_counter": (),
    "secoc_freshness": (),
    "secoc_mac": (),
}
FIELD_TYPES = list(TYPE_SCHEMA)
DIRECTIONS = ["TX", "RX", "TX/RX"]  # Nm 等、自ノード送信・他ノード受信の両方が同一CAN IDを使うフレーム用

DEFAULT_DATA_PATH = os.path.normpath(
    # __file__ は tools/can_signal_editor/src/app.py なので、3階層上がリポジトリルート
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "data", "can_signals.json")
)

# (キー, 見出し, 列幅) のリストを唯一の情報源とし、キー一覧・見出し・幅の
# 3つのdict/tupleを個別に手で同期させない。
FRAME_COLUMNS_DEF = [
    ("name", "Frame", 160), ("direction", "Dir", 50),
    ("canId", "CAN ID", 70), ("dlc", "DLC", 40),
    ("txPeriodMs", "TxPeriod(ms)", 90), ("rxTimeoutMs", "RxTimeout(ms)", 100),
    ("note", "Note", 400),
]
FRAME_COLUMNS = tuple(key for key, _, _ in FRAME_COLUMNS_DEF)

# フレーム一覧の数値系列の検証方法（FRAME_COLUMNS_DEF と同じく、ここが唯一の
# 情報源。ここに無い列（name/direction/canId/note）はプレーンな文字列として
# そのまま扱う）。
#   "int"          : 必須の整数（dlc。空欄は不可、負値チェックは無い＝既存挙動のまま）
#   "optional_int" : 任意の非負整数（txPeriodMs/rxTimeoutMs）。空欄にした場合は
#                    0 や null ではなくキー自体を削除する（欠落 = このフレームには
#                    周期/タイムアウトの概念が無い、という意図的な未設定。
#                    data/can_signals.json の $note 参照）。
# 以前は dlc 用の素の if 文と、txPeriodMs/rxTimeoutMs 用の frozenset 判定という
# 独立した2つの仕組みがあった。数値系の列が増えるたびにどちらに追加すべきか
# 迷い、書き忘れると無検証のプレーン文字列として扱われてしまう（silent
# fallthrough）ため、1つの表に統合する。
FRAME_COLUMN_KINDS = {
    "dlc": "int",
    "txPeriodMs": "optional_int",
    "rxTimeoutMs": "optional_int",
}

FIELD_COLUMNS_DEF = [
    ("name", "Signal", 160), ("bitPosition", "Bit", 40), ("bitSize", "Size", 40),
    ("type", "Type", 90), ("detail", "Detail", 220), ("note", "Note", 340),
]
FIELD_COLUMNS = tuple(key for key, _, _ in FIELD_COLUMNS_DEF)


def _fmt_range(rng: dict) -> str:
    """range の min/max のうち片方だけ未設定でも 'None' という文字列を出さない表示。"""
    lo, hi = rng.get("min"), rng.get("max")
    if lo is not None and hi is not None:
        return f"[{lo}, {hi}]"
    if lo is not None:
        return f"[{lo}, )"
    if hi is not None:
        return f"(, {hi}]"
    return ""


def field_detail_text(field: dict) -> str:
    """Detail 列に表示する要約文字列（実データは field 自体が保持する）。"""
    t = field.get("type")
    if t == "enum":
        return ", ".join(f"{e['value']}={e['label']}" for e in field.get("enum", []))
    if t == "number":
        parts = []
        if field.get("unit"):
            parts.append(str(field["unit"]))
        if field.get("scale") not in (None, 1):
            parts.append(f"scale={field['scale']}")
        rng = field.get("range")
        if rng:
            parts.append(_fmt_range(rng))
        return " ".join(parts)
    return ""


def bit_range_conflicts(fields: list[dict], dlc: int) -> list[str]:
    """同一フレーム内のビット範囲重複・DLC超過を検出する。空リストなら問題なし。

    重複判定は「開始位置でソートし、それまでに見た区間の最大終端（running max-end）
    と比較する」走査にする必要がある。単純に直前の要素とだけ比較すると、
    A(0-10) の中に B(2-3)・C(5-6) が両方収まっているような「隣接しないが
    包含関係にある」重複を見逃す（A-B は検出できても A-C は sorted 順で
    B の直後に来るため prev=B との比較しかされず、A との重複が漏れる）。
    """
    problems = []
    spans = []
    for f in fields:
        try:
            pos = int(f.get("bitPosition", 0))
            size = int(f.get("bitSize", 0))
        except (TypeError, ValueError):
            problems.append(f"{f.get('name', '?')}: bitPosition/bitSize が数値ではありません")
            continue
        if pos < 0:
            problems.append(f"{f.get('name', '?')}: bitPosition は 0 以上である必要があります")
            continue
        if size <= 0:
            problems.append(f"{f.get('name', '?')}: bitSize は 1 以上である必要があります")
            continue
        end = pos + size - 1
        if end >= dlc * 8:
            problems.append(f"{f.get('name', '?')}: bit {pos}-{end} が DLC={dlc}（{dlc * 8}bit）を超えています")
        spans.append((pos, end, f.get("name", "?")))

    spans.sort()
    max_end = None
    max_end_pos = None
    max_end_name = None
    for pos, end, name in spans:
        if max_end is not None and pos <= max_end:
            problems.append(
                f"{max_end_name}(bit {max_end_pos}-{max_end}) と {name}(bit {pos}-{end}) が重複しています"
            )
        if max_end is None or end > max_end:
            max_end, max_end_pos, max_end_name = end, pos, name
    return problems


class EnumEditorDialog(tk.Toplevel):
    """enum フィールドの value/label 一覧を編集するモーダルダイアログ。"""

    def __init__(self, parent: tk.Tk, enum_list: list[dict]):
        super().__init__(parent)
        self.title("enum を編集")
        self.resizable(False, False)
        self.result: list[dict] | None = None
        self._rows: list[tuple[tk.StringVar, tk.StringVar]] = []

        self.body = ttk.Frame(self, padding=8)
        self.body.pack(fill=tk.BOTH, expand=True)
        ttk.Label(self.body, text="value").grid(row=0, column=0, padx=4)
        ttk.Label(self.body, text="label").grid(row=0, column=1, padx=4)

        for e in enum_list:
            self._add_row(str(e.get("value", "")), str(e.get("label", "")))
        if not enum_list:
            self._add_row("", "")

        btns = ttk.Frame(self, padding=8)
        btns.pack(fill=tk.X)
        ttk.Button(btns, text="+ 行追加", command=lambda: self._add_row("", "")).pack(side=tk.LEFT)
        ttk.Button(btns, text="OK", command=self._on_ok).pack(side=tk.RIGHT)
        ttk.Button(btns, text="キャンセル", command=self.destroy).pack(side=tk.RIGHT, padx=4)

        self.transient(parent)
        self.grab_set()
        self.wait_window(self)

    def _add_row(self, value: str, label: str) -> None:
        r = len(self._rows) + 1
        v = tk.StringVar(value=value)
        l = tk.StringVar(value=label)
        ttk.Entry(self.body, textvariable=v, width=8).grid(row=r, column=0, padx=4, pady=2)
        ttk.Entry(self.body, textvariable=l, width=24).grid(row=r, column=1, padx=4, pady=2)
        self._rows.append((v, l))

    def _on_ok(self) -> None:
        result = []
        for v, l in self._rows:
            if v.get().strip() == "" and l.get().strip() == "":
                continue
            try:
                value = int(v.get(), 0)
            except ValueError:
                messagebox.showerror("エラー", f"value '{v.get()}' は整数ではありません", parent=self)
                return
            result.append({"value": value, "label": l.get()})
        self.result = result
        self.destroy()


class NumberDetailDialog(tk.Toplevel):
    """number フィールドの unit/scale/range を編集するモーダルダイアログ。"""

    def __init__(self, parent: tk.Tk, field: dict):
        super().__init__(parent)
        self.title("number 詳細を編集")
        self.resizable(False, False)
        self.result: dict | None = None

        rng = field.get("range") or {}
        self.unit_var = tk.StringVar(value=str(field.get("unit", "")))
        self.scale_var = tk.StringVar(value=str(field.get("scale", "")))
        self.min_var = tk.StringVar(value=str(rng.get("min", "")))
        self.max_var = tk.StringVar(value=str(rng.get("max", "")))

        body = ttk.Frame(self, padding=8)
        body.pack(fill=tk.BOTH, expand=True)
        for i, (label, var) in enumerate(
            [("unit", self.unit_var), ("scale（空欄=1）", self.scale_var),
             ("range min", self.min_var), ("range max", self.max_var)]
        ):
            ttk.Label(body, text=label).grid(row=i, column=0, sticky="w", padx=4, pady=2)
            ttk.Entry(body, textvariable=var, width=20).grid(row=i, column=1, padx=4, pady=2)

        btns = ttk.Frame(self, padding=8)
        btns.pack(fill=tk.X)
        ttk.Button(btns, text="OK", command=self._on_ok).pack(side=tk.RIGHT)
        ttk.Button(btns, text="キャンセル", command=self.destroy).pack(side=tk.RIGHT, padx=4)

        self.transient(parent)
        self.grab_set()
        self.wait_window(self)

    def _on_ok(self) -> None:
        result: dict = {}
        if self.unit_var.get().strip():
            result["unit"] = self.unit_var.get().strip()
        if self.scale_var.get().strip():
            try:
                result["scale"] = float(self.scale_var.get())
            except ValueError:
                messagebox.showerror("エラー", "scale は数値ではありません", parent=self)
                return
        min_s, max_s = self.min_var.get().strip(), self.max_var.get().strip()
        if min_s or max_s:
            try:
                result["range"] = {
                    "min": float(min_s) if min_s else None,
                    "max": float(max_s) if max_s else None,
                }
            except ValueError:
                messagebox.showerror("エラー", "range min/max は数値ではありません", parent=self)
                return
        self.result = result
        self.destroy()


class EditableTreeview(ttk.Treeview):
    """ダブルクリックしたセルを Entry/Combobox で上書きしてインライン編集する Treeview。

    editable_columns で指定した列のみ直接編集可能。combobox_columns に含まれる列は
    ドロップダウンで選択させる。それ以外の列（detail 等）はダブルクリック時に
    on_detail_edit コールバックを呼ぶだけで、テキスト編集はしない。
    """

    def __init__(self, master, columns, editable_columns, combobox_values=None,
                 on_commit=None, on_detail_edit=None, **kwargs):
        super().__init__(master, columns=columns, show="headings", **kwargs)
        self.editable_columns = set(editable_columns)
        self.combobox_values = combobox_values or {}
        self.on_commit = on_commit
        self.on_detail_edit = on_detail_edit
        # 編集中のセル (row_id, col_name, editor widget)。常に3つ揃って
        # 設定・参照・破棄されるため、別々の属性にせず1つのタプルにまとめる。
        self._editing: tuple[str, str, tk.Widget] | None = None
        self.bind("<Double-1>", self._begin_edit)

    def _begin_edit(self, event: tk.Event) -> None:
        # 別セルの編集を開始する前に、開きっぱなしのエディタがあれば確定させる
        # （readonly Combobox はドロップダウンを開くだけで FocusOut が発火する
        # ことがあるため、Combobox には FocusOut を結び付けない。代わりに
        # 次の編集開始時・フォーカス喪失時にここで確実に後始末する）。
        if self._editing is not None:
            row_id, col_name, editor = self._editing
            self._commit_edit(row_id, col_name, editor)

        row_id = self.identify_row(event.y)
        col_id = self.identify_column(event.x)
        if not row_id or not col_id:
            return
        col_index = int(col_id.replace("#", "")) - 1
        col_name = self["columns"][col_index]

        if col_name not in self.editable_columns:
            if self.on_detail_edit:
                self.on_detail_edit(row_id, col_name)
            return

        x, y, w, h = self.bbox(row_id, col_id)
        current = self.set(row_id, col_name)

        if col_name in self.combobox_values:
            editor = ttk.Combobox(self, values=self.combobox_values[col_name], state="readonly")
            editor.set(current)
            editor.bind("<<ComboboxSelected>>", lambda e: self._commit_edit(row_id, col_name, editor))
            # 注意: Combobox には <FocusOut> を結び付けない。readonly Combobox は
            # ドロップダウンを開いた時点で（実際に選択される前に）FocusOut が
            # 発火する環境があり、ここで destroy してしまうと、その後ユーザーが
            # ドロップダウンの項目をクリックした際に <<ComboboxSelected>> が
            # 破棄済みウィジェットに対して発火し TclError になる。
        else:
            editor = ttk.Entry(self)
            editor.insert(0, current)
            editor.select_range(0, tk.END)
            editor.bind("<FocusOut>", lambda e: self._commit_edit(row_id, col_name, editor))

        editor.place(x=x, y=y, width=w, height=h)
        editor.focus_set()
        editor.bind("<Return>", lambda e: self._commit_edit(row_id, col_name, editor))
        editor.bind("<Escape>", lambda e: self._cancel_edit())
        self._editing = (row_id, col_name, editor)

    def _commit_edit(self, row_id: str, col_name: str, editor: tk.Widget) -> None:
        if self._editing is None:
            return  # 既に確定済み（FocusOut が Return の後に二重発火するのを防ぐ）
        value = editor.get()
        self._cancel_edit()
        self.set(row_id, col_name, value)
        if self.on_commit:
            self.on_commit(row_id, col_name, value)

    def _cancel_edit(self) -> None:
        if self._editing is not None:
            self._editing[2].destroy()
            self._editing = None


class App(tk.Tk):
    def __init__(self, data_path: str):
        super().__init__()
        self.data_path = data_path
        self.title("CAN Signal Editor")
        self.geometry("1200x720")

        self.data = self._load()
        self.dirty = False
        self.current_frame_id: str | None = None

        self._build_ui()
        self._refresh_frame_tree()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ------------------------------------------------------------------
    # データ入出力
    # ------------------------------------------------------------------
    def _load(self) -> dict:
        with open(self.data_path, "r", encoding="utf-8") as f:
            return json.load(f)

    def _save(self) -> None:
        with open(self.data_path, "w", encoding="utf-8") as f:
            json.dump(self.data, f, ensure_ascii=False, indent=2)
            f.write("\n")
        self.dirty = False
        self._update_title()
        self.status_var.set(f"保存しました: {self.data_path}")

    def _mark_dirty(self) -> None:
        self.dirty = True
        self._update_title()

    def _update_title(self) -> None:
        mark = "*" if self.dirty else ""
        self.title(f"CAN Signal Editor{mark} - {os.path.basename(self.data_path)}")

    def _on_close(self) -> None:
        if self.dirty and not messagebox.askyesno(
            "確認", "未保存の変更があります。保存せずに終了しますか？"
        ):
            return
        self.destroy()

    # ------------------------------------------------------------------
    # UI構築
    # ------------------------------------------------------------------
    def _build_ui(self) -> None:
        toolbar = ttk.Frame(self, padding=4)
        toolbar.pack(fill=tk.X)
        ttk.Button(toolbar, text="保存 (Ctrl+S)", command=self._save).pack(side=tk.LEFT)
        ttk.Button(toolbar, text="再読み込み", command=self._reload).pack(side=tk.LEFT, padx=4)
        self.bind_all("<Control-s>", lambda e: self._save())

        self.status_var = tk.StringVar(value="")
        ttk.Label(toolbar, textvariable=self.status_var, foreground="#666").pack(side=tk.LEFT, padx=12)

        paned = ttk.PanedWindow(self, orient=tk.VERTICAL)
        paned.pack(fill=tk.BOTH, expand=True)

        # --- フレーム一覧 ---
        frame_pane = ttk.Frame(paned, padding=4)
        paned.add(frame_pane, weight=1)
        ttk.Label(frame_pane, text="フレーム一覧").pack(anchor="w")

        frame_tree_wrap = ttk.Frame(frame_pane)
        frame_tree_wrap.pack(fill=tk.BOTH, expand=True)
        self.frame_tree = EditableTreeview(
            frame_tree_wrap, FRAME_COLUMNS, editable_columns=FRAME_COLUMNS,
            combobox_values={"direction": DIRECTIONS},
            on_commit=self._on_frame_cell_commit, height=6,
        )
        for key, heading, width in FRAME_COLUMNS_DEF:
            self.frame_tree.heading(key, text=heading)
            self.frame_tree.column(key, width=width, anchor="w")
        frame_scroll = ttk.Scrollbar(frame_tree_wrap, orient=tk.VERTICAL, command=self.frame_tree.yview)
        self.frame_tree.configure(yscrollcommand=frame_scroll.set)
        self.frame_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        frame_scroll.pack(side=tk.LEFT, fill=tk.Y)
        self.frame_tree.bind("<<TreeviewSelect>>", self._on_frame_select)

        frame_btns = ttk.Frame(frame_pane)
        frame_btns.pack(fill=tk.X, pady=4)
        ttk.Button(frame_btns, text="+ フレーム追加", command=self._add_frame).pack(side=tk.LEFT)
        ttk.Button(frame_btns, text="- フレーム削除", command=self._delete_frame).pack(side=tk.LEFT, padx=4)

        # --- フィールド一覧 ---
        field_pane = ttk.Frame(paned, padding=4)
        paned.add(field_pane, weight=2)
        self.field_pane_label_var = tk.StringVar(value="フィールド一覧（フレームを選択してください）")
        ttk.Label(field_pane, textvariable=self.field_pane_label_var).pack(anchor="w")

        field_tree_wrap = ttk.Frame(field_pane)
        field_tree_wrap.pack(fill=tk.BOTH, expand=True)
        self.field_tree = EditableTreeview(
            field_tree_wrap, FIELD_COLUMNS,
            editable_columns=("name", "bitPosition", "bitSize", "type", "note"),
            combobox_values={"type": FIELD_TYPES},
            on_commit=self._on_field_cell_commit,
            on_detail_edit=self._on_field_detail_edit,
        )
        for key, heading, width in FIELD_COLUMNS_DEF:
            self.field_tree.heading(key, text=heading)
            self.field_tree.column(key, width=width, anchor="w")
        field_scroll = ttk.Scrollbar(field_tree_wrap, orient=tk.VERTICAL, command=self.field_tree.yview)
        self.field_tree.configure(yscrollcommand=field_scroll.set)
        self.field_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        field_scroll.pack(side=tk.LEFT, fill=tk.Y)

        field_btns = ttk.Frame(field_pane)
        field_btns.pack(fill=tk.X, pady=4)
        ttk.Button(field_btns, text="+ フィールド追加", command=self._add_field).pack(side=tk.LEFT)
        ttk.Button(field_btns, text="- フィールド削除", command=self._delete_field).pack(side=tk.LEFT, padx=4)
        ttk.Button(field_btns, text="詳細を編集... (enum/number)",
                   command=self._edit_selected_field_detail).pack(side=tk.LEFT, padx=12)

        # --- 検証結果 ---
        problems_pane = ttk.Frame(self, padding=4)
        problems_pane.pack(fill=tk.X)
        ttk.Label(problems_pane, text="ビット範囲チェック:").pack(anchor="w")
        self.problems_var = tk.StringVar(value="")
        ttk.Label(problems_pane, textvariable=self.problems_var, foreground="#B00").pack(anchor="w")

    # ------------------------------------------------------------------
    # フレーム一覧
    # ------------------------------------------------------------------
    def _refresh_frame_tree(self) -> None:
        self.frame_tree.delete(*self.frame_tree.get_children())
        for i, fr in enumerate(self.data["frames"]):
            iid = str(i)
            self.frame_tree.insert("", tk.END, iid=iid,
                                    values=tuple(fr.get(col, "") for col in FRAME_COLUMNS))

    def _on_frame_cell_commit(self, row_id: str, col_name: str, value: str) -> None:
        fr = self.data["frames"][int(row_id)]
        kind = FRAME_COLUMN_KINDS.get(col_name)
        if kind == "optional_int" and value.strip() == "":
            # 空欄 = このフレームには周期/タイムアウトの概念が無い、を意味する。
            # 0 を書き込むのではなくキー自体を削除する（欠落 = 意図的な未設定、
            # という data/can_signals.json の既存の表現に合わせる）。
            fr.pop(col_name, None)
            self._mark_dirty()
            self.frame_tree.set(row_id, col_name, "")
            if row_id == self.current_frame_id:
                self._refresh_problems()
            return
        if kind is not None:
            try:
                value = int(value)
                if kind == "optional_int" and value < 0:
                    raise ValueError
            except ValueError:
                hint = "0以上の整数、または空欄（値なし）" if kind == "optional_int" else "整数"
                messagebox.showerror("エラー", f"{col_name} は{hint}で入力してください")
                self._refresh_frame_tree()
                return
        fr[col_name] = value
        self._mark_dirty()
        # EditableTreeview._commit_edit は on_commit 呼び出し前に「入力された
        # 生の文字列」をセルへ書き込んでいる（前後の空白・先頭ゼロ・"-0" 等が
        # そのまま残る）。int系の列（FRAME_COLUMN_KINDS 参照）は int() で正規化
        # した値を fr に格納するため、セル表示もその正規化後の値に上書きし直して
        # 一致させる（name/direction/canId/note は正規化しないため実質 no-op）。
        self.frame_tree.set(row_id, col_name, value)
        if row_id == self.current_frame_id:
            self._refresh_problems()

    def _add_frame(self) -> None:
        self.data["frames"].append({
            "name": "NewFrame", "direction": "TX", "canId": "0x000", "dlc": 8,
            "note": "", "fields": [],
        })
        self._mark_dirty()
        self._refresh_frame_tree()

    def _delete_frame(self) -> None:
        sel = self.frame_tree.selection()
        if not sel:
            return
        if not messagebox.askyesno("確認", "選択したフレームを削除しますか？"):
            return
        idx = int(sel[0])
        del self.data["frames"][idx]
        self.current_frame_id = None
        self._mark_dirty()
        self._refresh_frame_tree()
        self._reset_field_pane()

    def _reset_field_pane(self) -> None:
        """フィールド一覧を「未選択」状態に戻す（フレーム削除時・再読み込み時に使う）。"""
        self.field_tree.delete(*self.field_tree.get_children())
        self.field_pane_label_var.set("フィールド一覧（フレームを選択してください）")
        self.problems_var.set("")

    def _on_frame_select(self, event: tk.Event) -> None:
        sel = self.frame_tree.selection()
        if not sel:
            return
        self.current_frame_id = sel[0]
        fr = self.data["frames"][int(self.current_frame_id)]
        self.field_pane_label_var.set(f"フィールド一覧: {fr.get('name', '')}")
        self._refresh_field_tree()
        self._refresh_problems()

    # ------------------------------------------------------------------
    # フィールド一覧
    # ------------------------------------------------------------------
    def _current_fields(self) -> list[dict]:
        fr = self.data["frames"][int(self.current_frame_id)]
        return fr.setdefault("fields", [])

    def _refresh_field_tree(self) -> None:
        self.field_tree.delete(*self.field_tree.get_children())
        for i, f in enumerate(self._current_fields()):
            iid = str(i)
            self.field_tree.insert("", tk.END, iid=iid, values=(
                f.get("name", ""), f.get("bitPosition", ""), f.get("bitSize", ""),
                f.get("type", ""), field_detail_text(f), f.get("note", ""),
            ))

    def _sync_field_row(self, idx: int) -> None:
        """フィールド1件分だけ表示を更新する。追加・削除は行数自体が変わるため
        _refresh_field_tree() が必要だが、既存1行の内容変更はこちらで十分
        （毎セル編集ごとに全行 delete+insert し直す無駄を避ける）。"""
        f = self._current_fields()[idx]
        self.field_tree.item(str(idx), values=(
            f.get("name", ""), f.get("bitPosition", ""), f.get("bitSize", ""),
            f.get("type", ""), field_detail_text(f), f.get("note", ""),
        ))

    def _on_field_cell_commit(self, row_id: str, col_name: str, value: str) -> None:
        idx = int(row_id)
        f = self._current_fields()[idx]
        if col_name in ("bitPosition", "bitSize"):
            try:
                value = int(value)
            except ValueError:
                messagebox.showerror("エラー", f"{col_name} は整数で入力してください")
                self._sync_field_row(idx)
                return
            if value < 0 or (col_name == "bitSize" and value == 0):
                messagebox.showerror("エラー", f"{col_name} は0以上（bitSize は1以上）で入力してください")
                self._sync_field_row(idx)
                return
        if col_name == "type" and value != f.get("type"):
            # 型変更時は前の型専用フィールドを破棄する（TYPE_SCHEMA が唯一の情報源）
            for keys in TYPE_SCHEMA.values():
                for k in keys:
                    f.pop(k, None)
        f[col_name] = value
        self._mark_dirty()
        self._sync_field_row(idx)
        self._refresh_problems()

    def _add_field(self) -> None:
        if self.current_frame_id is None:
            messagebox.showinfo("案内", "先にフレームを選択してください")
            return
        self._current_fields().append({
            "name": "NewSignal", "bitPosition": 0, "bitSize": 1, "type": "enum",
            "enum": [], "note": "",
        })
        self._mark_dirty()
        self._refresh_field_tree()
        self._refresh_problems()

    def _delete_field(self) -> None:
        sel = self.field_tree.selection()
        if not sel or self.current_frame_id is None:
            return
        idx = int(sel[0])
        del self._current_fields()[idx]
        self._mark_dirty()
        self._refresh_field_tree()
        self._refresh_problems()

    def _on_field_detail_edit(self, row_id: str, col_name: str) -> None:
        if col_name != "detail":
            return
        self._edit_field_detail(int(row_id))

    def _edit_selected_field_detail(self) -> None:
        sel = self.field_tree.selection()
        if not sel:
            messagebox.showinfo("案内", "先にフィールドを選択してください")
            return
        self._edit_field_detail(int(sel[0]))

    def _edit_field_detail(self, idx: int) -> None:
        f = self._current_fields()[idx]
        t = f.get("type")
        if t == "enum":
            dlg = EnumEditorDialog(self, f.get("enum", []))
            if dlg.result is not None:
                f["enum"] = dlg.result
                self._mark_dirty()
                self._sync_field_row(idx)
        elif t == "number":
            dlg = NumberDetailDialog(self, f)
            if dlg.result is not None:
                for k in TYPE_SCHEMA["number"]:
                    f.pop(k, None)
                f.update(dlg.result)
                self._mark_dirty()
                self._sync_field_row(idx)
        else:
            messagebox.showinfo("案内", f"type='{t}' は詳細編集の対象外です（note のみで説明する構造的フィールド）")

    # ------------------------------------------------------------------
    # 検証・その他
    # ------------------------------------------------------------------
    def _refresh_problems(self) -> None:
        if self.current_frame_id is None:
            self.problems_var.set("")
            return
        fr = self.data["frames"][int(self.current_frame_id)]
        problems = bit_range_conflicts(fr.get("fields", []), int(fr.get("dlc", 0) or 0))
        self.problems_var.set(" / ".join(problems) if problems else "問題なし")

    def _reload(self) -> None:
        if self.dirty and not messagebox.askyesno(
            "確認", "未保存の変更を破棄して再読み込みしますか？"
        ):
            return
        self.data = self._load()
        self.dirty = False
        self.current_frame_id = None
        self._update_title()
        self._refresh_frame_tree()
        self._reset_field_pane()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=DEFAULT_DATA_PATH)
    args = parser.parse_args()
    app = App(args.data)
    app.mainloop()


if __name__ == "__main__":
    main()

"""
設定 JSON エディタ（スキーマ駆動）

config/data/*.json（CanIf、Com、PduR などの設定データ）を、対応する JSON スキーマ
（config/schema/*.schema.json）に従ってツリー表示・編集する。画面はスキーマから作るため、
設定 json にモジュールが増えても、このファイルを書き換えずに使える。

  - 値の編集: 列挙は選択肢、整数は範囲、文字列は形式（パターン）に従う
  - 追加・削除・並べ替え: 配列の要素、オブジェクトの項目（スキーマにある未設定の項目を選べる）
  - 検証: 編集のたびにスキーマ検証し、違反を一覧に出す。保存すれば gen_cfg.py で生成できる
  - 保存形式: tools/configurator/sync_cfg.py と同じ（インデント 2、日本語はそのまま、CRLF）

can_tool（app.py）の 1 タブとして埋め込む。
"""
from __future__ import annotations

import copy
import glob
import json
import os
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Any, Callable

import jsonschema

import configurator_panel

SCALAR_KINDS = ("string", "integer", "number", "boolean", "enum", "const", "union")
KIND_LABEL = {
    "object": "オブジェクト", "array": "配列", "textlist": "文章（複数行）", "string": "文字列", "integer": "整数",
    "number": "数値", "boolean": "真偽", "enum": "列挙", "const": "固定値", "union": "整数/名前", "any": "?",
}
# 配列の要素の見出しに使う項目（先頭から探す）
LABEL_KEYS = ("name", "macro", "title", "frame", "summary")


# ----------------------------------------------------------------------
# スキーマの扱い
# ----------------------------------------------------------------------
class SchemaNav:
    """JSON スキーマをたどる（$ref の解決、種類の判定、新規要素の既定値）。"""

    def __init__(self, root: dict):
        self.root = root

    def resolve(self, s: Any) -> dict:
        if not isinstance(s, dict):
            return {}
        while "$ref" in s:
            ref = s["$ref"]
            node: Any = self.root
            for part in ref[2:].split("/"):
                node = node[part]
            s = {**node, **{k: v for k, v in s.items() if k != "$ref"}}
        return s

    def kind(self, s: Any) -> str:
        s = self.resolve(s)
        if "enum" in s:
            return "enum"
        if "const" in s:
            return "const"
        alts = s.get("oneOf")
        if alts:
            types = [self.resolve(a).get("type") for a in alts]
            if all(t in ("integer", "string", "number", "boolean") for t in types):
                return "union"
        t = s.get("type")
        if t == "object":
            return "object"
        if t == "array":
            return "textlist" if self.kind(s.get("items", {})) == "string" else "array"
        if t in ("string", "integer", "number", "boolean"):
            return t
        return "any"

    def prop_schema(self, s: Any, key: str) -> dict:
        """オブジェクトのスキーマ s における、項目 key のスキーマ。"""
        s = self.resolve(s)
        props = s.get("properties", {})
        if key in props:
            return self.resolve(props[key])
        add = s.get("additionalProperties")
        if isinstance(add, dict):
            return self.resolve(add)
        return {}

    def item_schema(self, s: Any) -> dict:
        return self.resolve(self.resolve(s).get("items", {}))

    def required(self, s: Any) -> list[str]:
        return list(self.resolve(s).get("required", []))

    def addable_keys(self, s: Any, present: Any) -> list[str]:
        """オブジェクトに追加できる項目名（スキーマにあって、まだ無いもの）。"""
        props = self.resolve(s).get("properties", {})
        return [k for k in props if k not in present and k != "$schema"]

    def allows_free_keys(self, s: Any) -> bool:
        return isinstance(self.resolve(s).get("additionalProperties"), dict)

    def default(self, s: Any) -> Any:
        """新しい要素の初期値。必須の項目は、型に応じた仮の値で埋める（検証で気づけるようにする）。"""
        s = self.resolve(s)
        k = self.kind(s)
        if "default" in s:
            return copy.deepcopy(s["default"])
        if k == "enum":
            return s["enum"][0]
        if k == "const":
            return s["const"]
        if k == "object":
            return {key: self.default(self.prop_schema(s, key)) for key in self.required(s)}
        if k in ("array", "textlist"):
            n = int(s.get("minItems", 0))
            return [self.default(s.get("items", {})) for _ in range(n)]
        if k == "integer":
            return int(s.get("minimum", 0))
        if k == "number":
            return s.get("minimum", 0)
        if k == "boolean":
            return False
        if k == "union":
            return 0
        return "x" if int(s.get("minLength", 0)) > 0 else ""


def parse_scalar(text: str, kind: str, schema: dict) -> Any:
    """入力文字列を、スキーマの種類に応じた値へ変換する。変換できなければ ValueError。"""
    if kind == "integer":
        return int(text, 0)
    if kind == "number":
        return float(text)
    if kind == "boolean":
        if text not in ("true", "false"):
            raise ValueError("true または false を選んでください")
        return text == "true"
    if kind == "union":
        try:
            return int(text, 0)
        except ValueError:
            return text
    return text


def display_value(v: Any) -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def item_label(i: int, v: Any) -> str:
    if isinstance(v, dict):
        for k in LABEL_KEYS:
            if isinstance(v.get(k), (str, int)):
                return "[%d] %s" % (i, v[k])
    return "[%d]" % i


def save_json(path: str, data: Any) -> None:
    """tools/configurator/sync_cfg.py と同じ書式（インデント 2、日本語そのまま、CRLF）で保存する。"""
    text = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    with open(path, "wb") as f:
        f.write(text.replace("\n", "\r\n").encode("utf-8"))


# ----------------------------------------------------------------------
# ダイアログ
# ----------------------------------------------------------------------
class ValueDialog(tk.Toplevel):
    """スカラー値（文字列・整数・列挙・真偽）を 1 つ入力するダイアログ。"""

    def __init__(self, parent: tk.Misc, title: str, kind: str, schema: dict, value: Any):
        super().__init__(parent)
        self.title(title)
        self.transient(parent.winfo_toplevel())
        self.result: tuple[Any] | None = None
        self._kind = kind
        self._schema = schema
        frm = ttk.Frame(self, padding=8)
        frm.pack(fill=tk.BOTH, expand=True)
        desc = schema.get("description")
        if desc:
            ttk.Label(frm, text=desc, wraplength=420, foreground="#555").pack(anchor="w", pady=(0, 6))
        self.var = tk.StringVar(value=display_value(value))
        if kind in ("enum", "boolean"):
            choices = schema["enum"] if kind == "enum" else ["true", "false"]
            self.entry = ttk.Combobox(frm, textvariable=self.var, values=[display_value(c) for c in choices],
                                      state="readonly", width=50)
        else:
            self.entry = ttk.Entry(frm, textvariable=self.var, width=60)
        self.entry.pack(fill=tk.X)
        hint = []
        if kind == "integer" or (kind == "union"):
            if "minimum" in schema or "maximum" in schema:
                hint.append("範囲: %s 〜 %s" % (schema.get("minimum", ""), schema.get("maximum", "")))
        if schema.get("pattern"):
            hint.append("形式: " + schema["pattern"])
        if kind == "union":
            hint.append("整数（16 進は 0x…）または名前（マクロ）")
        if hint:
            ttk.Label(frm, text=" / ".join(hint), foreground="#555").pack(anchor="w", pady=(4, 0))
        self.err = tk.StringVar(value="")
        ttk.Label(frm, textvariable=self.err, foreground="#B00").pack(anchor="w")
        btns = ttk.Frame(frm)
        btns.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(btns, text="OK", command=self._ok).pack(side=tk.RIGHT)
        ttk.Button(btns, text="キャンセル", command=self.destroy).pack(side=tk.RIGHT, padx=4)
        self.bind("<Return>", lambda _e: self._ok())
        self.bind("<Escape>", lambda _e: self.destroy())
        self.entry.focus_set()
        self.grab_set()

    def _ok(self) -> None:
        try:
            self.result = (parse_scalar(self.var.get(), self._kind, self._schema),)
        except ValueError as e:
            self.err.set(str(e))
            return
        self.destroy()


class TextListDialog(tk.Toplevel):
    """文字列の配列（説明文など）を、1 行 1 要素で編集するダイアログ。"""

    def __init__(self, parent: tk.Misc, title: str, lines: list[str], description: str = ""):
        super().__init__(parent)
        self.title(title)
        self.transient(parent.winfo_toplevel())
        self.result: list[str] | None = None
        frm = ttk.Frame(self, padding=8)
        frm.pack(fill=tk.BOTH, expand=True)
        if description:
            ttk.Label(frm, text=description, wraplength=560, foreground="#555").pack(anchor="w", pady=(0, 6))
        self.text = tk.Text(frm, width=90, height=14, wrap="none")
        self.text.pack(fill=tk.BOTH, expand=True)
        self.text.insert("1.0", "\n".join(lines))
        ttk.Label(frm, text="1 行が 1 要素になります（コメントの行）。", foreground="#555").pack(anchor="w", pady=(4, 0))
        btns = ttk.Frame(frm)
        btns.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(btns, text="OK", command=self._ok).pack(side=tk.RIGHT)
        ttk.Button(btns, text="キャンセル", command=self.destroy).pack(side=tk.RIGHT, padx=4)
        self.text.focus_set()
        self.grab_set()

    def _ok(self) -> None:
        raw = self.text.get("1.0", tk.END).rstrip("\n")
        self.result = raw.split("\n") if raw else []
        self.destroy()


class ChoiceDialog(tk.Toplevel):
    """選択肢から選ぶ、または新しい名前を入力するダイアログ。"""

    def __init__(self, parent: tk.Misc, title: str, prompt: str, choices: list[str], free_entry: bool = False):
        super().__init__(parent)
        self.title(title)
        self.transient(parent.winfo_toplevel())
        self.result: str | None = None
        frm = ttk.Frame(self, padding=8)
        frm.pack(fill=tk.BOTH, expand=True)
        ttk.Label(frm, text=prompt).pack(anchor="w")
        self.var = tk.StringVar(value=choices[0] if choices else "")
        self.box = ttk.Combobox(frm, textvariable=self.var, values=choices, width=50,
                                state="normal" if free_entry else "readonly")
        self.box.pack(fill=tk.X, pady=4)
        btns = ttk.Frame(frm)
        btns.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(btns, text="OK", command=self._ok).pack(side=tk.RIGHT)
        ttk.Button(btns, text="キャンセル", command=self.destroy).pack(side=tk.RIGHT, padx=4)
        self.bind("<Return>", lambda _e: self._ok())
        self.box.focus_set()
        self.grab_set()

    def _ok(self) -> None:
        if self.var.get():
            self.result = self.var.get()
            self.destroy()


# ----------------------------------------------------------------------
# エディタ本体
# ----------------------------------------------------------------------
class ConfigEditorFrame(ttk.Frame):
    """設定 JSON エディタ。ウィンドウを閉じる判断は呼び出し元の責務で、confirm_close() だけを提供する。"""

    def __init__(self, master: tk.Misc, config_dir: str, get_signals_path: Callable[[], str],
                 on_title_change: Callable[[str], None] | None = None):
        super().__init__(master)
        self.config_dir = config_dir
        self._get_signals_path = get_signals_path
        self._on_title_change = on_title_change or (lambda _t: None)
        self.path: str | None = None
        self.data: Any = {}
        self.nav = SchemaNav({})
        self.schema: dict = {}
        self.validator: jsonschema.Draft202012Validator | None = None
        self.dirty = False
        self._nodes: dict[str, tuple[tuple, dict]] = {}  # 行 ID → (path, schema)
        self._build_ui()
        names = self.file_names()
        if names:
            self.file_var.set(names[0])
            self.open_file(names[0], notify=False)
        # on_title_change が呼び出し元の変数（Notebook のタブを指す Frame 自身）を参照する場合に備え、
        # コンストラクタの完了後（イベントループの開始後）に通知する（信号定義エディタと同じ方式）。
        self.after_idle(self._update_title)

    # ------------------------------------------------------------------
    # ファイル
    # ------------------------------------------------------------------
    def file_names(self) -> list[str]:
        """編集できる設定 json の名前。スキーマのあるものだけを並べる
        （信号表 can_signals.json は専用の信号定義エディタで編集するため、ここには出さない）。"""
        names = []
        for p in sorted(glob.glob(os.path.join(self.config_dir, "*.json"))):
            try:
                with open(p, "r", encoding="utf-8") as f:
                    data = json.load(f)
            except (OSError, ValueError):
                continue
            if os.path.isfile(self._schema_path(p, data)):
                names.append(os.path.basename(p))
        return names

    def open_file(self, name: str, *, notify: bool = True) -> bool:
        """name の設定 json とスキーマを読み込む。notify=False は、コンストラクタからの呼び出し用
        （on_title_change が呼び出し元の変数を参照する場合に、代入前に呼ばないため）。"""
        path = os.path.join(self.config_dir, name)
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        schema_path = self._schema_path(path, data)
        with open(schema_path, "r", encoding="utf-8") as f:
            schema = json.load(f)
        self.path, self.data, self.schema = path, data, schema
        self.file_var.set(name)
        self.nav = SchemaNav(schema)
        self.validator = jsonschema.Draft202012Validator(schema)
        self.dirty = False
        self.refresh()
        if notify:
            self._update_title()
        self.status_var.set("開きました: %s" % path)
        return True

    @staticmethod
    def _schema_path(path: str, data: Any) -> str:
        ref = data.get("$schema") if isinstance(data, dict) else None
        if ref:
            return os.path.normpath(os.path.join(os.path.dirname(path), ref))
        stem = os.path.splitext(os.path.basename(path))[0]
        return os.path.join(configurator_panel.SCHEMA_DIR, stem + ".schema.json")

    def save(self) -> bool:
        if self.path is None:
            return False
        save_json(self.path, self.data)
        self.dirty = False
        self._update_title()
        self.status_var.set("保存しました: %s" % self.path)
        return True

    def reload(self) -> None:
        if self.path is None:
            return
        if self.dirty and not messagebox.askyesno("確認", "未保存の変更を破棄して再読み込みしますか？"):
            return
        self.open_file(os.path.basename(self.path))

    def confirm_close(self) -> bool:
        if self.dirty and not messagebox.askyesno("確認", "設定 JSON に未保存の変更があります。保存せずに終了しますか？"):
            return False
        return True

    def _update_title(self) -> None:
        mark = "*" if self.dirty else ""
        self._on_title_change("設定 JSON%s - %s" % (mark, os.path.basename(self.path) if self.path else ""))

    def _mark_dirty(self) -> None:
        self.dirty = True
        self._update_title()

    def _on_file_selected(self, _event: Any = None) -> None:
        name = self.file_var.get()
        if self.path and os.path.basename(self.path) == name:
            return
        if self.dirty and not messagebox.askyesno("確認", "未保存の変更を破棄して、別のファイルを開きますか？"):
            self.file_var.set(os.path.basename(self.path) if self.path else "")
            return
        self.open_file(name)

    def _ensure_saved_before_run(self) -> bool:
        if self.dirty:
            if not messagebox.askyesno("確認", "設定 JSON に未保存の変更があります。保存してから実行しますか？"):
                return False
            self.save()
        return True

    # ------------------------------------------------------------------
    # UI
    # ------------------------------------------------------------------
    def _build_ui(self) -> None:
        bar = ttk.Frame(self, padding=4)
        bar.pack(fill=tk.X)
        ttk.Label(bar, text="設定ファイル:").pack(side=tk.LEFT)
        self.file_var = tk.StringVar()
        box = ttk.Combobox(bar, textvariable=self.file_var, values=self.file_names(), state="readonly", width=24)
        box.pack(side=tk.LEFT, padx=4)
        box.bind("<<ComboboxSelected>>", self._on_file_selected)
        ttk.Button(bar, text="保存", command=self.save).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(bar, text="再読み込み", command=self.reload).pack(side=tk.LEFT, padx=4)
        self.status_var = tk.StringVar(value="")
        ttk.Label(bar, textvariable=self.status_var, foreground="#666").pack(side=tk.LEFT, padx=12)

        paned = ttk.PanedWindow(self, orient=tk.VERTICAL)
        paned.pack(fill=tk.BOTH, expand=True)

        top = ttk.Frame(paned, padding=4)
        paned.add(top, weight=3)
        wrap = ttk.Frame(top)
        wrap.pack(fill=tk.BOTH, expand=True)
        self.tree = ttk.Treeview(wrap, columns=("value", "type"), selectmode="browse", height=18)
        self.tree.heading("#0", text="項目")
        self.tree.heading("value", text="値")
        self.tree.heading("type", text="型")
        self.tree.column("#0", width=380, anchor="w")
        self.tree.column("value", width=520, anchor="w")
        self.tree.column("type", width=120, anchor="w")
        scroll = ttk.Scrollbar(wrap, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scroll.pack(side=tk.LEFT, fill=tk.Y)
        self.tree.bind("<Double-1>", lambda _e: self.edit_selected())
        self.tree.bind("<<TreeviewSelect>>", lambda _e: self._on_select())
        self.tree.tag_configure("bad", foreground="#B00")

        btns = ttk.Frame(top)
        btns.pack(fill=tk.X, pady=4)
        ttk.Button(btns, text="値を編集...", command=self.edit_selected).pack(side=tk.LEFT)
        ttk.Button(btns, text="+ 追加", command=self.add_to_selected).pack(side=tk.LEFT, padx=4)
        ttk.Button(btns, text="- 削除", command=self.delete_selected).pack(side=tk.LEFT)
        ttk.Button(btns, text="↑", width=3, command=lambda: self.move_selected(-1)).pack(side=tk.LEFT, padx=(12, 0))
        ttk.Button(btns, text="↓", width=3, command=lambda: self.move_selected(1)).pack(side=tk.LEFT, padx=2)
        self.desc_var = tk.StringVar(value="")
        ttk.Label(top, textvariable=self.desc_var, wraplength=1200, foreground="#555").pack(anchor="w")

        bottom = ttk.Frame(paned, padding=4)
        paned.add(bottom, weight=1)
        ttk.Label(bottom, text="スキーマ検証:").pack(anchor="w")
        vwrap = ttk.Frame(bottom)
        vwrap.pack(fill=tk.BOTH, expand=True)
        self.problems = tk.Text(vwrap, height=5, wrap="word", state="disabled")
        self.problems.tag_configure("bad", foreground="#B00")
        self.problems.tag_configure("good", foreground="#070")
        vscroll = ttk.Scrollbar(vwrap, orient=tk.VERTICAL, command=self.problems.yview)
        self.problems.configure(yscrollcommand=vscroll.set)
        self.problems.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        vscroll.pack(side=tk.LEFT, fill=tk.Y)

        self.configurator = configurator_panel.ConfiguratorPanel(
            self, self._get_signals_path, self._ensure_saved_before_run, log_height=6)
        self.configurator.pack(fill=tk.BOTH)

    # ------------------------------------------------------------------
    # ツリー
    # ------------------------------------------------------------------
    def get(self, path: tuple) -> Any:
        v = self.data
        for p in path:
            v = v[p]
        return v

    def refresh(self) -> None:
        """データからツリーを作り直す。展開状態と選択は、できるだけ保つ。"""
        expanded = {self._nodes[i][0] for i in self._nodes if self.tree.item(i, "open")}
        selected = self._nodes[self.tree.selection()[0]][0] if self.tree.selection() else None
        self.tree.delete(*self.tree.get_children())
        self._nodes.clear()
        root = self.tree.insert("", tk.END, text=os.path.basename(self.path or ""), open=True)
        self._nodes[root] = ((), self.nav.resolve(self.schema))
        self._add_children(root, (), self.schema, expanded)
        if selected is not None:
            for iid, (p, _s) in self._nodes.items():
                if p == selected:
                    self.tree.selection_set(iid)
                    self.tree.see(iid)
                    break
        self._validate()

    def _summary(self, v: Any, kind: str) -> str:
        if kind == "object":
            return "{ %d 項目 }" % len(v)
        if kind == "array":
            return "[ %d 件 ]" % len(v)
        if kind == "textlist":
            if not v:
                return "(空)"
            return v[0] + ("  …(%d 行)" % len(v) if len(v) > 1 else "")
        return display_value(v)

    def _insert(self, parent: str, label: str, path: tuple, schema: dict, required: bool, expanded: set) -> str:
        v = self.get(path)
        kind = self.nav.kind(schema)
        shown = self._summary(v, kind) if kind in ("object", "array", "textlist") or kind in SCALAR_KINDS else str(v)
        iid = self.tree.insert(parent, tk.END, text=label, values=(shown, KIND_LABEL.get(kind, "?") + (" *" if required else "")),
                               open=path in expanded)
        self._nodes[iid] = (path, schema)
        if kind in ("object", "array"):
            self._add_children(iid, path, schema, expanded)
        return iid

    def _add_children(self, parent: str, path: tuple, schema: dict, expanded: set) -> None:
        v = self.get(path)
        s = self.nav.resolve(schema)
        kind = self.nav.kind(s)
        if kind == "object" and isinstance(v, dict):
            req = set(self.nav.required(s))
            for k in v:
                if k == "$schema":
                    continue
                self._insert(parent, str(k), path + (k,), self.nav.prop_schema(s, k), k in req, expanded)
        elif kind == "array" and isinstance(v, list):
            item_schema = self.nav.item_schema(s)
            for i, item in enumerate(v):
                self._insert(parent, item_label(i, item), path + (i,), item_schema, False, expanded)

    def _selected(self) -> tuple[str, tuple, dict] | None:
        sel = self.tree.selection()
        if not sel:
            return None
        path, schema = self._nodes[sel[0]]
        return sel[0], path, schema

    def _on_select(self) -> None:
        sel = self._selected()
        if sel is None:
            self.desc_var.set("")
            return
        _iid, path, schema = sel
        s = self.nav.resolve(schema)
        parts = [s.get("description", "")]
        if "default" in s:
            parts.append("既定値: %s" % s["default"])
        self.desc_var.set(" ".join(p for p in parts if p))

    # ------------------------------------------------------------------
    # 編集
    # ------------------------------------------------------------------
    def set_value(self, path: tuple, value: Any) -> None:
        parent = self.get(path[:-1])
        parent[path[-1]] = value
        self._mark_dirty()
        self.refresh()

    def edit_selected(self) -> None:
        sel = self._selected()
        if sel is None or not sel[1]:
            return
        _iid, path, schema = sel
        s = self.nav.resolve(schema)
        kind = self.nav.kind(s)
        value = self.get(path)
        title = " / ".join(str(p) for p in path)
        if kind == "textlist":
            dlg = TextListDialog(self, title, list(value), s.get("description", ""))
            self.wait_window(dlg)
            if dlg.result is not None:
                self.set_value(path, dlg.result)
        elif kind in ("string", "integer", "number", "boolean", "enum", "union"):
            dlg = ValueDialog(self, title, kind, s, value)
            self.wait_window(dlg)
            if dlg.result is not None:
                self.set_value(path, dlg.result[0])
        elif kind == "const":
            messagebox.showinfo("案内", "固定値のため編集できません")
        else:
            messagebox.showinfo("案内", "この項目は子要素を選んで編集してください")

    def add_to_selected(self) -> None:
        """選択中の配列に要素を足す。要素を選んでいれば、その次に挿入する。オブジェクトなら項目を足す。"""
        sel = self._selected()
        if sel is None:
            return
        _iid, path, schema = sel
        s = self.nav.resolve(schema)
        kind = self.nav.kind(s)
        # 配列の要素を選んでいる場合は、親の配列を対象にする
        if path and isinstance(path[-1], int):
            arr_path = path[:-1]
            arr = self.get(arr_path)
            arr_schema = self._schema_at(arr_path)
            new = self.nav.default(self.nav.item_schema(arr_schema))
            arr.insert(path[-1] + 1, new)
            self._mark_dirty()
            self.refresh()
            return
        if kind == "array":
            self.get(path).append(self.nav.default(self.nav.item_schema(s)))
            self._mark_dirty()
            self.refresh()
        elif kind == "object":
            obj = self.get(path)
            keys = self.nav.addable_keys(s, obj)
            free = self.nav.allows_free_keys(s)
            if not keys and not free:
                messagebox.showinfo("案内", "追加できる項目はありません")
                return
            dlg = ChoiceDialog(self, "項目を追加", "追加する項目名:" if not free else "項目名（自由入力できます）:", keys, free_entry=free)
            self.wait_window(dlg)
            if dlg.result is None:
                return
            key = dlg.result
            if key in obj:
                messagebox.showinfo("案内", "その項目はすでにあります")
                return
            obj[key] = self.nav.default(self.nav.prop_schema(s, key))
            self._mark_dirty()
            self.refresh()
        else:
            messagebox.showinfo("案内", "配列かオブジェクトを選んでください")

    def _schema_at(self, path: tuple) -> dict:
        s = self.nav.resolve(self.schema)
        for p in path:
            s = self.nav.item_schema(s) if isinstance(p, int) else self.nav.prop_schema(s, p)
        return s

    def delete_selected(self) -> None:
        sel = self._selected()
        if sel is None or not sel[1]:
            return
        _iid, path, _schema = sel
        if not messagebox.askyesno("確認", "'%s' を削除しますか？" % " / ".join(str(p) for p in path)):
            return
        self.delete_path(path)

    def delete_path(self, path: tuple) -> None:
        parent = self.get(path[:-1])
        if isinstance(parent, list):
            parent.pop(path[-1])
        else:
            del parent[path[-1]]
        self._mark_dirty()
        self.refresh()

    def move_selected(self, delta: int) -> None:
        sel = self._selected()
        if sel is None or not sel[1] or not isinstance(sel[1][-1], int):
            return
        self.move_path(sel[1], delta)

    def move_path(self, path: tuple, delta: int) -> None:
        arr = self.get(path[:-1])
        i, j = path[-1], path[-1] + delta
        if not 0 <= j < len(arr):
            return
        arr[i], arr[j] = arr[j], arr[i]
        self._mark_dirty()
        self.refresh()
        # 移動後の要素を選び直す
        target = path[:-1] + (j,)
        for iid, (p, _s) in self._nodes.items():
            if p == target:
                self.tree.selection_set(iid)
                break

    # ------------------------------------------------------------------
    # 検証
    # ------------------------------------------------------------------
    def validation_errors(self) -> list[tuple[tuple, str]]:
        if self.validator is None:
            return []
        errs = sorted(self.validator.iter_errors(self.data), key=lambda e: [str(x) for x in e.absolute_path])
        return [(tuple(e.absolute_path), e.message) for e in errs]

    def _validate(self) -> None:
        errs = self.validation_errors()
        self.problems.configure(state="normal")
        self.problems.delete("1.0", tk.END)
        if errs:
            for path, msg in errs[:200]:
                self.problems.insert(tk.END, "%s: %s\n" % (" / ".join(str(p) for p in path) or "(全体)", msg), ("bad",))
        else:
            self.problems.insert(tk.END, "問題なし（スキーマに適合しています）", ("good",))
        self.problems.configure(state="disabled")
        bad = {p for p, _m in errs}
        for iid, (path, _s) in self._nodes.items():
            if path in bad:
                self.tree.item(iid, tags=("bad",))
        self.status_var.set(("スキーマ違反 %d 件" % len(errs)) if errs else self.status_var.get())

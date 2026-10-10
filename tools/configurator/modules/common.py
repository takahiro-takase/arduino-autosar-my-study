"""各モジュールの生成関数が共有する、コメントと構造体フィールドの整形。"""


def block_comment(lines, indent=0, star="/*", cont=" * "):
    """複数行コメント。1 行なら /* text */、複数行なら /* l0 / * l1 ... */。"""
    pad = " " * indent
    if len(lines) == 1:
        return [pad + star + " " + lines[0] + " */"]
    out = [pad + star + " " + lines[0]]
    for ln in lines[1:-1]:
        out.append(pad + cont + ln)
    out.append(pad + cont + lines[-1] + " */")
    return out


def doc_comment(lines):
    """Doxygen の /** ... */（2 行目以降は ' *  ' で始める）。"""
    return block_comment(lines, 0, star="/**", cont=" *  ")


def ruled_comment(lines, indent=0, width=63):
    """罫線付きの見出しコメント（エントリごとの見出しに使う）。"""
    pad = " " * indent
    rule = "-" * width
    return [pad + "/* " + rule] + [pad + " * " + ln for ln in lines] + [pad + " * " + rule + " */"]


def note_lines(base, extra=None):
    """DaVinci 対応コメント。base の次の行から extra を続ける（空なら base のみ）。"""
    return [base] + list(extra or [])


def struct_fields(fields, indent):
    """fields: [(名前, 値, 注釈行のリスト or None)]。名前を揃え、注釈を桁揃えで末尾に付ける。"""
    pad = " " * indent
    width = max(len(n) for n, _, _ in fields)
    rows = []
    for i, (name, value, note) in enumerate(fields):
        comma = "," if i < len(fields) - 1 else ""
        rows.append(("%s.%s = %s%s" % (pad, name.ljust(width), value, comma), note))
    col = max((len(t) for t, n in rows if n), default=0) + 1
    out = []
    for text, note in rows:
        if not note:
            out.append(text)
            continue
        head = text.ljust(col)
        out.append(head + "/* " + note[0] + (" */" if len(note) == 1 else ""))
        hang = " " * 9 if note[0].startswith("DaVinci: ") else ""
        for k, ln in enumerate(note[1:], start=1):
            tail = " */" if k == len(note) - 1 else ""
            out.append(" " * (col + 1) + "* " + hang + ln + tail)
    return out


def hex_literal(text):
    """'0x200' → '0x200U'。"""
    return text + "U"

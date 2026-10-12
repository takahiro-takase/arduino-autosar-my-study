"""設定値の #define を、説明コメント付きで並べる共通の整形（CanNm・CanTp など、1 項目ずつ説明を持つ定義用）。"""

from . import symbols
from .common import block_comment, doc_comment

RULE = "-" * 71


def section(lines):
    """見出しコメント（罫線で囲む）。"""
    return ["/* " + RULE] + [(" * " + ln).rstrip() for ln in lines] + [" * " + RULE + " */"]


def define(name, value, doc, unused=False):
    """説明コメント + #define。unused が真なら MISRA 2.5（未使用マクロ）の抑止コメントを付ける。"""
    out = doc_comment(doc)
    if unused:
        out.append(symbols.MISRA_UNUSED_MACRO)
    out.append("#define %s  %s" % (name, value))
    return out


def render_layout(cfg, layout, overrides=None):
    """layout に沿って見出しと定義を並べる。layout の要素は ("section", キー) か ("define", キー, マクロ名, 書式)。
    書式は "%dU" のように値（cfg[キー]["value"]）へ適用する文字列。overrides[キー] があれば書式を使わずその文字列を値にする。"""
    overrides = overrides or {}
    blocks = []
    for item in layout:
        if item[0] == "section":
            blocks.append(section(cfg[item[1]]))
        else:
            _, key, macro, fmt = item
            value = overrides[key] if key in overrides else fmt % cfg[key]["value"]
            blocks.append(define(macro, value, cfg[key]["doc"], cfg[key].get("unused", False)))
    return join(blocks)


def join(blocks):
    """ブロックの間を空行 1 つで区切る。"""
    out = []
    for i, b in enumerate(blocks):
        if i:
            out.append("")
        out += b
    return "\n".join(ln.rstrip() for ln in out)

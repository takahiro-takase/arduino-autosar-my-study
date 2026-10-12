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


def join(blocks):
    """ブロックの間を空行 1 つで区切る。"""
    out = []
    for i, b in enumerate(blocks):
        if i:
            out.append("")
        out += b
    return "\n".join(ln.rstrip() for ln in out)

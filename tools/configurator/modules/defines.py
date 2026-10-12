"""設計値の #define を並べるだけの、単純なモジュール（Port・Dio・Adc・EcuM など）の生成の共通部分。

設定 JSON の `items[]` に、ファイルに出す順で書く。1 要素は次のどちらか。
  {"section": [行...]}                                  見出しコメント（罫線で囲む）
  {"doc": [行...], "defines": [{"macro": 名前, "value": 値の文字列}, ...]}
                                                         説明コメント（Doxygen）と、それに続く 1 つ以上の #define
`value` は C のリテラルや他のマクロ名を、そのまま文字列で書く（"6U"、"5000UL"、"0x00000020UL"、"DIO_PORT_LED_GROUP" など）。
`doc` を省略すると、直前の要素の #define に続けて出す。`unused` が真の要素は、MISRA C:2012 Rule 2.5（未使用マクロ）の抑止コメントを付ける。

各モジュールの検査（値の範囲、他モジュールとの一致）は、そのモジュールの validate() と checks.py で行う。
"""

import re

from . import symbols
from .common import doc_comment
from .defs import join, section

INT_RE = re.compile(r"^(0[xX][0-9A-Fa-f]+|\d+)[uUlL]*$")


def number(value_text):
    """"6U" や "0x20UL" の数値部分を整数にする。数値のリテラルでなければ ValueError。"""
    m = INT_RE.match(value_text)
    if not m:
        raise ValueError("数値のリテラルではない: %s" % value_text)
    return int(m.group(1), 0)


def values(cfg):
    """{マクロ名: 値の文字列} を、items の出現順で返す。"""
    out = {}
    for it in cfg["items"]:
        for d in it.get("defines", []):
            out[d["macro"]] = d["value"]
    return out


def validate_items(cfg):
    errors = []
    names = []
    for i, it in enumerate(cfg["items"]):
        if ("section" in it) == ("defines" in it):
            errors.append("items[%d]: section と defines のどちらか一方だけを書く" % i)
        for d in it.get("defines", []):
            names.append(d["macro"])
    if len(set(names)) != len(names):
        errors.append("macro が重複している")
    return errors


def render_items(cfg, ctx=None):
    blocks = []
    for it in cfg["items"]:
        if "section" in it:
            blocks.append(section(it["section"]))
            continue
        defines = it["defines"]
        width = max(len(d["macro"]) for d in defines)
        out = []
        if "doc" in it:
            out += doc_comment(it["doc"])
        if it.get("unused", False):
            out.append(symbols.MISRA_UNUSED_MACRO)
        out += ["#define %s  %s" % (d["macro"].ljust(width), d["value"]) for d in defines]
        blocks.append(out)
    return join(blocks)


def make_spec(module, header, region_id, validate=None):
    """module（例 "Port"）の SPEC。header は生成先、region_id は @@GEN-BEGIN の id。"""

    def check(cfg):
        errors = validate_items(cfg)
        if validate is not None:
            errors += validate(cfg)
        return errors

    return {
        "config": "config/data/%s.json" % module,
        "schema": "config/schema/%s.schema.json" % module,
        "validate": check,
        "targets": [(header, region_id, render_items, None)],
    }

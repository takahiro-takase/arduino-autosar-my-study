"""NvM（NvM_Cfg.h / NvM_PBCfg.c）の生成。

NvM.json に書く値: ブロックの名前・サイズ、冗長ブロックかどうか、CRC 比較による書き込みスキップの有無、
ROM デフォルト値（なし／1 バイトの定数／同じ式の繰り返し）、各項目の説明。
生成する値:
  - ブロック ID（並び順がそのまま ID）、ブロック数
  - EEPROM の先頭アドレス（前のブロックの末尾＋CRC 1 バイトから連鎖。冗長ブロックはミラー面の分も確保する）
  - RAM ミラーバッファ、ROM デフォルト値、ブロック記述子テーブル
  - 最大ブロック長（NVM_MAX_BLOCK_LENGTH）、EEPROM レイアウトのコメント
Dem のイベント数に長さを合わせるブロック（`demEventCount`）は、Dem_Cfg.h の DEM_EVENT_COUNT と一致するか検査する。
"""

import pathlib
import re

from .common import block_comment, doc_comment

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
RULE = "-" * 71


def validate(cfg):
    errors = []
    names = [b["name"] for b in cfg["blocks"]]
    if len(set(names)) != len(names):
        errors.append("blocks: name が重複している")
    for b in cfg["blocks"]:
        d = b["default"]
        if d["kind"] == "repeat":
            idx = [c["index"] for c in d.get("elementComments", [])]
            if idx and max(idx) >= b["length"]:
                errors.append("blocks %s: default.elementComments の index が length 以上" % b["name"])
            if len(set(idx)) != len(idx):
                errors.append("blocks %s: default.elementComments の index が重複している" % b["name"])
        if d["kind"] != "none" and "value" not in d:
            errors.append("blocks %s: default.kind=%s には value が必要" % (b["name"], d["kind"]))
        if d["kind"] == "scalar" and b["length"] != 1:
            errors.append("blocks %s: default.kind=scalar は length=1 のブロックだけ" % b["name"])
    return errors


# ----------------------------------------------------------------------
def camel(name):
    """DEM_MAGIC → DemMagic。"""
    return "".join(p[0].upper() + p[1:].lower() for p in name.split("_") if p)


def hex4(v):
    return "0x%04XU" % v


def layout(cfg):
    """各ブロックの (EEPROM 先頭アドレス, ミラー面の先頭アドレス or None) と、使用バイト数。"""
    addr = cfg.get("baseAddress", 0)
    result = []
    for b in cfg["blocks"]:
        primary = addr
        addr += b["length"] + 1
        mirror = None
        if b.get("redundant"):
            mirror = addr
            addr += b["length"] + 1
        result.append((primary, mirror))
    return result, addr - cfg.get("baseAddress", 0)


def check_dem(cfg, ctx):
    """Dem のイベント数に合わせるブロックの長さを、Dem_Cfg.h の DEM_EVENT_COUNT と突き合わせる。"""
    root = pathlib.Path(getattr(ctx, "repo_root", None) or REPO_ROOT)
    text = (root / "src" / "Bsw" / "Dem" / "Dem_Cfg.h").read_bytes().decode("utf-8", errors="replace")
    m = re.search(r"#define\s+DEM_EVENT_COUNT\s+(\d+)U", text)
    if m is None:
        raise ValueError("Dem_Cfg.h に DEM_EVENT_COUNT が見つからない")
    count = int(m.group(1))
    for b in cfg["blocks"]:
        if b.get("demEventCount") and b["length"] != count:
            raise ValueError("blocks %s: length=%d が Dem の DEM_EVENT_COUNT=%d と一致しない" % (b["name"], b["length"], count))


def ruled(lines):
    return ["/* " + RULE] + [" * " + ln if ln else " *" for ln in lines] + [" * " + RULE + " */"]


def plural(n):
    return "%d byte%s" % (n, "" if n == 1 else "s")


# ----------------------------------------------------------------------
def render_cfg(cfg, ctx):
    check_dem(cfg, ctx)
    blocks = cfg["blocks"]
    addrs, total = layout(cfg)
    base = cfg.get("baseAddress", 0)
    out = []

    # EEPROM レイアウト
    width = max(len("NVM_BLOCK_ID_" + b["name"]) for b in blocks)
    lay = ["EEPROM レイアウト（先頭 %d バイト）" % total,
           "各ブロックはデータ本体直後に CRC8 (SAE J1850) を 1 バイト付加する",
           "(NvM.c の NvM_CalcCrc8() / NvM_CrcAddressForBase() 参照)。"]
    for b, (p, m) in zip(blocks, addrs):
        ident = ("NVM_BLOCK_ID_" + b["name"]).ljust(width)
        face = "（プライマリ面）" if m is not None else ""
        lay.append("  Addr 0x%04X: %s データ (%s) — %s%s" % (p, ident, plural(b["length"]), b["title"], face))
        lay.append("  Addr 0x%04X: %s CRC   (1 byte)%s" % (p + b["length"], ident, face and " — プライマリ面"))
        if m is not None:
            lay.append("  Addr 0x%04X: %s データ (%s) — %s（ミラー面）" % (m, ident, plural(b["length"]), b["title"]))
            lay.append("  Addr 0x%04X: %s CRC   (1 byte) — ミラー面" % (m + b["length"], ident))
    out += ruled(lay) + [""]

    # ブロック ID
    out += ruled(["ブロック ID", "NvM_ReadBlock() / NvM_WriteBlock() の第 1 引数に渡す。"])
    w = max(len("NVM_BLOCK_ID_" + b["name"]) for b in blocks)
    for i, b in enumerate(blocks):
        out.append("#define %s  %dU  /**< %s (%s) */" % (("NVM_BLOCK_ID_" + b["name"]).ljust(max(w, len("NVM_BLOCK_COUNT"))), i,
                                                       b["title"], plural(b["length"])))
    out.append("#define %s  %dU  /**< 管理ブロック総数 */" % ("NVM_BLOCK_COUNT".ljust(max(w, len("NVM_BLOCK_COUNT"))), len(blocks)))
    out.append("")

    # ブロックサイズ
    out += ruled(["ブロックサイズ (bytes)"])
    lw = max(len("NVM_BLOCK_%s_LENGTH" % b["name"]) for b in blocks)
    for b in blocks:
        out.append("#define %s  %dU  /**< %s */" % (("NVM_BLOCK_%s_LENGTH" % b["name"]).ljust(lw), b["length"], b["lengthDoc"]))
    out.append("")

    # EEPROM 先頭アドレス
    out += ruled(["EEPROM 先頭アドレス (各ブロックの物理格納先)",
                  "各ブロックのデータ本体直後 (BaseNumber + Length) に CRC 1 バイトが入るため、",
                  "後続ブロックの先頭アドレスは前ブロックの (データ長+1) ずつ後ろにずれる。",
                  "ここでは前ブロックのマクロからの連鎖式にして、プリプロセッサに計算させる",
                  "（CRC アドレス自体は NvM.c の NvM_CrcAddressForBase() が base+length から実行時に求める）。"])
    prev = None
    for b, (p, m) in zip(blocks, addrs):
        name = "NVM_BLOCK_%s_EEPROM_ADDR" % b["name"]
        if prev is None:
            out.append("#define %s  %s  /**< %s */" % (name, hex4(base), b["addrDoc"]))
        else:
            out.append("#define %s \\" % name)
            out.append("    (%s + %s + 1U)  /**< %s */" % (prev[0], prev[1], b["addrDoc"]))
        prev = (name, "NVM_BLOCK_%s_LENGTH" % b["name"])
        if m is not None:
            mname = "NVM_BLOCK_%s_MIRROR_EEPROM_ADDR" % b["name"]
            out.append("#define %s \\" % mname)
            out.append("    (%s + %s + 1U)  /**< %s */" % (prev[0], prev[1], b["mirrorAddrDoc"]))
            prev = (mname, prev[1])
    out.append("")

    mx = max(b["length"] for b in blocks)
    out += doc_comment(["設定済みブロックの中で最大の NvMNvBlockLength（生成値）。",
                        "冗長ブロックの読み込み検証（NvM.c の NvM_LoadAndVerifyBlock()）がミラー面の内容を",
                        "一時的に保持するスタック上のスクラッチバッファのサイズとして使う。"])
    out.append("#define NVM_MAX_BLOCK_LENGTH  %dU" % mx)
    return "\n".join(out)


# ----------------------------------------------------------------------
def default_ident(b):
    return "NvM_Default_" + camel(b["name"])


def ram_ident(b):
    return "NvM_Ram_" + camel(b["name"])


def render_pbcfg(cfg, ctx):
    check_dem(cfg, ctx)
    blocks = cfg["blocks"]
    addrs, _ = layout(cfg)
    out = []

    out += ruled(["RAM ミラーバッファ (NvM が管理; 上位モジュールは直接参照しない)",
                  "NvM_Init() で EEPROM 内容がここへ展開される。"])
    for b in blocks:
        out.append("static uint8 %s[NVM_BLOCK_%s_LENGTH];" % (ram_ident(b), b["name"]))
    out.append("")

    out += ruled(["ROM デフォルト値 (CRC 不一致時に NvM_Init()/NvM_RestoreBlockDefaults() が",
                  "RAM ミラーへ適用し、CRC を付け直して EEPROM へ書き戻す内容)"])
    for b in blocks:
        d = b["default"]
        doc = d.get("doc")
        if d["kind"] == "none":
            if doc:
                out += block_comment(doc)
            continue
        if doc:
            out += doc_comment(doc)
        if d["kind"] == "scalar":
            out.append("static const uint8 %s = %s;" % (default_ident(b), d["value"]))
        else:
            out.append("static const uint8 %s[NVM_BLOCK_%s_LENGTH] =" % (default_ident(b), b["name"]))
            out.append("{")
            comments = {str(c["index"]): c["text"] for c in d.get("elementComments", [])}
            for i in range(b["length"]):
                comma = "," if i < b["length"] - 1 else " "
                line = "    %s%s" % (d["value"], comma)
                if str(i) in comments:
                    line += " /* %s */" % comments[str(i)]
                out.append(line)
            out.append("};")
        out.append("")
    if out[-1] != "":
        out.append("")

    out += ruled(["ブロック記述子テーブル", "インデックスがそのまま NvM_BlockIdType の値に対応する。"])
    out.append("static const NvM_BlockDescriptorType NvM_BlockTable[NVM_BLOCK_COUNT] =")
    out.append("{")
    for i, (b, (p, m)) in enumerate(zip(blocks, addrs)):
        red = bool(b.get("redundant"))
        d = b["default"]
        if d["kind"] == "none":
            rom, rom_note = "NULL", "RomBlockDataAddress: 未設定→全0で代替"
        elif d["kind"] == "scalar":
            rom, rom_note = "&" + default_ident(b), "RomBlockDataAddress  "
        else:
            rom, rom_note = default_ident(b), "RomBlockDataAddress  "
        crc = b.get("useCrcCompMechanism", not red)
        rows = [
            ("NVM_BLOCK_%s_EEPROM_ADDR" % b["name"], "NvMNvBlockBaseNumber"),
            ("NVM_BLOCK_%s_LENGTH" % b["name"], "NvMNvBlockLength"),
            (ram_ident(b), "RamBlockDataAddress"),
            (rom, rom_note.strip()),
            ("1U" if red else "0U", "Redundant: 冗長ブロック" if red else "Redundant: 非冗長"),
            ("NVM_BLOCK_%s_MIRROR_EEPROM_ADDR" % b["name"] if red else "0U",
             "NvMNvBlockBaseNumberMirror" if red else "NvMNvBlockBaseNumberMirror: 非冗長のため未使用"),
            ("1U" if crc else "0U", "UseCrcCompMechanism" + ("" if crc else ": 使わない")),
        ]
        col = max(len(v) for v, _ in rows) + 3
        entry = ["    {"]
        for k, (v, note) in enumerate(rows):
            text = v + ("," if k < len(rows) - 1 else "")
            entry.append("        " + text.ljust(col) + "/* " + note + " */")
        entry.append("    }" + ("," if i < len(blocks) - 1 else ""))
        out += block_comment(["NVM_BLOCK_ID_" + b["name"]] + b.get("note", []), indent=4)
        out += entry
    out.append("};")
    return "\n".join(out)


def check_includes(cfg, text):
    messages = []
    if any("DEM_" in b["default"].get("value", "") for b in cfg["blocks"]) and '"Dem_Cfg.h"' not in text:
        messages.append('ROM デフォルト値が DEM_* を使っているのに、"Dem_Cfg.h" をインクルードしていない')
    return messages


SPEC = {
    "config": "config/data/NvM.json",
    "schema": "config/schema/NvM.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/NvM/NvM_Cfg.h", "nvm-blocks", render_cfg, None),
        ("src/Bsw/NvM/NvM_PBCfg.c", "nvm-pbcfg", render_pbcfg, check_includes),
    ],
}

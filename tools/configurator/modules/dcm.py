"""Dcm（Dcm_Cfg.h / Dcm_PBCfg.h / Dcm_PBCfg.c）の DID 定義の生成。

Dcm.json の dids[] に書く値: DID の名前・番号・データ長、対応するサービス（0x22 の読み出し関数、0x2E の書き込み関数、
0x2F で制御するランプ）、各項目の説明。読み出し・書き込みの関数の中身は Dcm.c に手書きする（名前だけを書く）。
生成する値（並び順がそのまま表の順序）:
  - Dcm_Cfg.h: DID のマクロ（DCM_DID_<name>）、データ長のマクロ（lengthMacro を指定したものだけ）、DID の数、
    読み出し対応の DID の最大データ長
  - Dcm_PBCfg.h: データ関数（readFct / writeFct）の宣言
  - Dcm_PBCfg.c: DID の表（Dcm_DidTable）。Dcm.c は表を引いて 0x22 / 0x2E / 0x2F を処理する
  - SID ごとの許可セッションの表（Dcm_SidSessionTable、sidSessions[]）と、その行数（Dcm_Cfg.h の DCM_SID_SESSION_COUNT）
検査: DID・名前・データ長マクロの重複、サービスが 1 つも無い DID。
"""

import pathlib
import re

from .dem import define_block


def validate(cfg):
    errors = []
    names = [d["name"] for d in cfg["dids"]]
    if len(set(names)) != len(names):
        errors.append("dids: name が重複している")
    nums = [int(d["did"], 16) for d in cfg["dids"]]
    if len(set(nums)) != len(nums):
        errors.append("dids: did が重複している")
    for d in cfg["dids"]:
        if "readFct" not in d and "writeFct" not in d and "ioControlLamp" not in d:
            errors.append("dids %s: readFct / writeFct / ioControlLamp のどれか 1 つは指定する" % d["name"])
        if "ioControlLamp" in d and ("readFct" in d or "writeFct" in d):
            errors.append("dids %s: ioControlLamp は readFct / writeFct と併用しない（0x2F 専用の DID）" % d["name"])
        if ("readFct" in d or "writeFct" in d) and "lengthMacro" not in d and d["length"] > 8:
            errors.append("dids %s: 長いデータの DID は lengthMacro を指定し、手書きの関数と同じ値を使う" % d["name"])
        if "lengthDoc" in d and "lengthMacro" not in d:
            errors.append("dids %s: lengthDoc は lengthMacro を指定したときだけ書く" % d["name"])
    sids = [s["sid"] for s in cfg["sidSessions"]]
    if len(set(sids)) != len(sids):
        errors.append("sidSessions: sid が重複している")
    macros = [d["lengthMacro"] for d in cfg["dids"] if "lengthMacro" in d]
    if len(set(macros)) != len(macros):
        errors.append("dids: lengthMacro が重複している")
    return errors


def read_max_length(cfg):
    return max([d["length"] for d in cfg["dids"] if "readFct" in d] or [1])


def render_cfg_dids(cfg, ctx):
    entries = []
    for d in cfg["dids"]:
        entries.append(("DCM_DID_" + d["name"], "0x%04XU" % int(d["did"], 16), d["doc"]))
        if "lengthMacro" in d:
            entries.append((d["lengthMacro"], "%dU" % d["length"], d.get("lengthDoc", ["データ長 [byte]"])))
    entries.append(("DCM_DID_COUNT", "%dU" % len(cfg["dids"]), ["Dcm_DidTable[] のエントリ数"]))
    entries.append(("DCM_DID_READ_MAX_LENGTH", "%dU" % read_max_length(cfg),
                    ["0x22 で読み出せる DID の最大データ長（応答用バッファの大きさ）"]))
    return "\n".join(define_block(entries))


def render_cfg_sid_count(cfg, ctx):
    return "\n".join(define_block([("DCM_SID_SESSION_COUNT", "%dU" % len(cfg["sidSessions"]),
                                    ["Dcm_SidSessionTable[] のエントリ数"])]))


def render_sid_table(cfg, ctx):
    """SID ごとの許可セッションの表。SID は Dcm_Cfg.h の DCM_SID_<sid> で指す（無ければ生成エラー）。"""
    cfg_h = (pathlib.Path(ctx.repo_root) / "src/Bsw/Dcm/Dcm_Cfg.h").read_bytes().decode("utf-8")
    out = ["/** " + "\n *  ".join(cfg["sidSessionDoc"]) + " */",
           "const Dcm_SidSessionRowType Dcm_SidSessionTable[DCM_SID_SESSION_COUNT] =", "{"]
    n = len(cfg["sidSessions"])
    for i, s in enumerate(cfg["sidSessions"]):
        if not re.search(r"^#define DCM_SID_%s\b" % s["sid"], cfg_h, re.M):
            raise ValueError("sidSessions: DCM_SID_%s が Dcm_Cfg.h に無い" % s["sid"])
        mask = " | ".join("DCM_SESSION_MASK_" + x for x in s["sessions"])
        out.append("    { DCM_SID_%s, %s }%s" % (s["sid"], mask, "," if i < n - 1 else ""))
    out.append("};")
    return "\n".join(out)


def functions(cfg):
    """(関数名, 種類) を、出てきた順に重複なく返す。"""
    seen, result = set(), []
    for d in cfg["dids"]:
        for key, kind in (("readFct", "read"), ("writeFct", "write")):
            if key in d and d[key] not in seen:
                seen.add(d[key])
                result.append((d[key], kind))
    return result


def render_cfg_prototypes(cfg, ctx):
    out = ["/* DID のデータ関数（Dcm.c に手書きする。名前は config/data/Dcm.json の readFct / writeFct） */"]
    for name, kind in functions(cfg):
        arg = "uint8* Data" if kind == "read" else "const uint8* Data"
        out.append("Std_ReturnType %s(%s);" % (name, arg))
    return "\n".join(out)


def render_table(cfg, ctx):
    out = ["/** DID の表（0x22 / 0x2E / 0x2F が引く）。Dcm.c の Dcm_FindDid() が先頭から探索する */",
           "const Dcm_DidConfigType Dcm_DidTable[DCM_DID_COUNT] =", "{"]
    n = len(cfg["dids"])
    for i, d in enumerate(cfg["dids"]):
        length = d["lengthMacro"] if "lengthMacro" in d else "%dU" % d["length"]
        comma = "," if i < n - 1 else ""
        out.append("    /* DCM_DID_%s */" % d["name"])
        out.append("    { DCM_DID_%s, %s, %s, %s, %s }%s" % (
            d["name"], length, d.get("readFct", "NULL"), d.get("writeFct", "NULL"),
            d.get("ioControlLamp", "DCM_DID_IOCONTROL_NONE"), comma))
    out.append("};")
    return "\n".join(out)


SPEC = {
    "config": "config/data/Dcm.json",
    "schema": "config/schema/Dcm.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Dcm/Dcm_Cfg.h", "dcm-dids", render_cfg_dids, None),
        ("src/Bsw/Dcm/Dcm_Cfg.h", "dcm-sid-session-count", render_cfg_sid_count, None),
        ("src/Bsw/Dcm/Dcm_PBCfg.c", "dcm-sid-session-table", render_sid_table, None),
        ("src/Bsw/Dcm/Dcm_PBCfg.h", "dcm-did-functions", render_cfg_prototypes, None),
        ("src/Bsw/Dcm/Dcm_PBCfg.c", "dcm-did-table", render_table, None),
    ],
}

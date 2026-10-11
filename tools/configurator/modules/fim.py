"""FiM（FiM_Cfg.h / FiM_PBCfg.c）の生成。

FiM.json に書く値: 機能（FID）の名前・説明、監視する Dem イベント、抑止するステータスのビット、
テーブルの見出しコメント。
生成する値: 機能 ID（並び順がそのまま FID）、機能数（FiM_Cfg.h）と、FID と Dem イベントの対応テーブル（FiM_PBCfg.c）。
検査: Dem のイベント・ステータスのマクロが Dem_Cfg.h に実在し、イベント ID が DEM_EVENT_COUNT 未満であること。
"""

import pathlib
import re

from .common import block_comment

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
RULE = "-" * 71


def validate(cfg):
    errors = []
    names = [f["name"] for f in cfg["functions"]]
    if len(set(names)) != len(names):
        errors.append("functions: name が重複している")
    pairs = [(f["event"], f["statusMask"]) for f in cfg["functions"]]
    return errors


def dem_macros(ctx):
    """Dem_Cfg.h の #define を {名前: 数値 or None} で返す。"""
    root = pathlib.Path(getattr(ctx, "repo_root", None) or REPO_ROOT)
    text = (root / "src" / "Bsw" / "Dem" / "Dem_Cfg.h").read_bytes().decode("utf-8", errors="replace")
    result = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)U?\b", text, re.M):
        result[m.group(1)] = int(m.group(2), 0)
    return result


def check_dem(cfg, ctx):
    macros = dem_macros(ctx)
    count = macros.get("DEM_EVENT_COUNT")
    for f in cfg["functions"]:
        for key in ("event", "statusMask"):
            if f[key] not in macros:
                raise ValueError("functions %s: %s '%s' が Dem_Cfg.h に無い" % (f["name"], key, f[key]))
        if count is not None and macros[f["event"]] >= count:
            raise ValueError("functions %s: event '%s'(%d) が DEM_EVENT_COUNT=%d 以上" % (f["name"], f["event"], macros[f["event"]], count))


def ruled(lines):
    return ["/* " + RULE] + [" * " + ln if ln else " *" for ln in lines] + [" * " + RULE + " */"]


def render_cfg(cfg, ctx):
    check_dem(cfg, ctx)
    fns = cfg["functions"]
    w = max(len("FIM_FID_" + f["name"]) for f in fns)
    w = max(w, len("FIM_FUNCTION_COUNT"))
    out = ruled(["機能 ID (FID) 定義",
                 "ASW (App_EngineManager / App_WarningIndicator) が",
                 "Rte_Call_FiM_GetFunctionPermission() の引数として使用する。",
                 "FID と監視対象イベントの対応は FiM_PBCfg.c の FiM_Functions[] （FiM.json から生成）。"])
    for i, f in enumerate(fns):
        out.append("#define %s  %dU  /**< %s */" % (("FIM_FID_" + f["name"]).ljust(w), i, f["title"]))
    out.append("#define %s  %dU  /**< 管理する機能の総数 */" % ("FIM_FUNCTION_COUNT".ljust(w), len(fns)))
    return "\n".join(out)


def render_pbcfg(cfg, ctx):
    check_dem(cfg, ctx)
    fns = cfg["functions"]
    out = ruled(["FID × Dem イベント対応テーブル"])
    out.append("static const FiM_FunctionCfgType FiM_Functions[FIM_FUNCTION_COUNT] =")
    out.append("{")
    for i, f in enumerate(fns):
        out += block_comment(["FID %d: %s" % (i, f["title"])] + f["note"], indent=4)
        comma = "," if i < len(fns) - 1 else ""
        out.append("    { FIM_FID_%s, %s, %s }%s" % (f["name"], f["event"], f["statusMask"], comma))
        if i < len(fns) - 1:
            out.append("")
    out.append("};")
    return "\n".join(out)


def check_includes(cfg, text):
    return [] if '"Dem_Cfg.h"' in text else ['FiM_PBCfg.c が "Dem_Cfg.h" をインクルードしていない']


SPEC = {
    "config": "config/data/FiM.json",
    "schema": "config/schema/FiM.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/FiM/FiM_Cfg.h", "fim-functions", render_cfg, None),
        ("src/Bsw/FiM/FiM_PBCfg.c", "fim-table", render_pbcfg, check_includes),
    ],
}

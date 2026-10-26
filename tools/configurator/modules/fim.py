"""FiM（FiM_Cfg.h / FiM_PBCfg.c）の生成。

FiM.json に書く値: 機能（FID）の名前・説明、抑止の条件（1 つ以上）、複数のイベントをまとめた名前（summaryEvents、省略可）、
テーブルの見出しコメント。
抑止の条件は、対象（event か summary）と、条件（statusMask か type）の組:
  - event: 監視する Dem イベント 1 つ。summary: summaryEvents[] で、まとめたイベントすべて（イベントごとの行へ展開する）
  - statusMask: 抑止するステータスのビット（独自拡張。いずれか 1 ビットでも立っていれば抑止）
  - type: 仕様の条件（FIM_LAST_FAILED / FIM_NOT_TESTED / FIM_TESTED / FIM_TESTED_AND_FAILED）
生成する値: 機能 ID（並び順がそのまま FID）、機能数と抑止の条件の数（FiM_Cfg.h）と、FID と Dem イベントの対応テーブル
（FiM_PBCfg.c。抑止の条件 1 つが 1 行。同じ FID の行は、どれか 1 つが成り立てば抑止する）。
検査: Dem のイベント・ステータスのマクロが Dem_Cfg.h に実在し、イベント ID が DEM_EVENT_COUNT 未満であること。
event / summary、statusMask / type は、それぞれ、ちょうど 1 つを書くこと。summary の名前が実在すること。
同じ機能の中で、同じ（イベント、条件）の組が重複しないこと。
"""

import pathlib
import re

from .common import block_comment
from .dem import EVENT_SYMBOL

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
RULE = "-" * 71


def condition_of(inh):
    """抑止の条件 1 件の、条件の部分（type か statusMask）を (列挙の名前, マスクの文字列) で返す。"""
    if "type" in inh:
        return inh["type"], "0U"
    return "FIM_INH_STATUS_MASK", inh["statusMask"]


def expand(cfg):
    """機能ごとの抑止の条件を、summary をイベントごとに展開して、{event, cond, mask, summary} の行の列にする。"""
    summaries = {s["name"]: s["events"] for s in cfg.get("summaryEvents", [])}
    result = []
    for f in cfg["functions"]:
        rows = []
        for inh in f["inhibitions"]:
            events = summaries[inh["summary"]] if "summary" in inh else [inh["event"]]
            cond, mask = condition_of(inh)
            for ev in events:
                rows.append({"event": ev, "cond": cond, "mask": mask, "summary": inh.get("summary")})
        result.append(rows)
    return result


def validate(cfg):
    errors = []
    names = [f["name"] for f in cfg["functions"]]
    if len(set(names)) != len(names):
        errors.append("functions: name が重複している")
    summary_names = [s["name"] for s in cfg.get("summaryEvents", [])]
    if len(set(summary_names)) != len(summary_names):
        errors.append("summaryEvents: name が重複している")
    for f in cfg["functions"]:
        for k, inh in enumerate(f["inhibitions"]):
            where = "functions %s: inhibitions[%d]" % (f["name"], k)
            if ("event" in inh) == ("summary" in inh):
                errors.append("%s: event と summary のどちらか一方だけを指定する" % where)
            if ("statusMask" in inh) == ("type" in inh):
                errors.append("%s: statusMask と type のどちらか一方だけを指定する" % where)
            if "summary" in inh and inh["summary"] not in summary_names:
                errors.append("%s: summary '%s' が summaryEvents に無い" % (where, inh["summary"]))
    if errors:
        return errors   # 形が崩れていると、下の展開ができない
    for f, rows in zip(cfg["functions"], expand(cfg)):
        pairs = [(r["event"], r["cond"], r["mask"]) for r in rows]
        if len(set(pairs)) != len(pairs):
            errors.append("functions %s: inhibitions の（event, 条件）の組が重複している（summary の展開後を含む）" % f["name"])
    return errors


def dem_macros(ctx):
    """Dem_Cfg.h の #define を {名前: 数値} で返す（ステータスのビットの存在確認用）。"""
    root = pathlib.Path(getattr(ctx, "repo_root", None) or REPO_ROOT)
    text = (root / "src" / "Bsw" / "Dem" / "Dem_Cfg.h").read_bytes().decode("utf-8", errors="replace")
    result = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)U?\b", text, re.M):
        result[m.group(1)] = int(m.group(2), 0)
    return result


def check_dem(cfg, ctx):
    """監視するイベントが Dem.json に実在し、ステータスのビットが Dem_Cfg.h に実在することを確認する。"""
    events = {e["name"] for e in ctx.configs["Dem"]["events"]}
    macros = dem_macros(ctx)
    for f, rows in zip(cfg["functions"], expand(cfg)):
        for r in rows:
            if r["event"] not in events:
                raise ValueError("functions %s: event '%s' が Dem.json のイベント名に無い" % (f["name"], r["event"]))
            if r["cond"] == "FIM_INH_STATUS_MASK" and r["mask"] not in macros:
                raise ValueError("functions %s: statusMask '%s' が Dem_Cfg.h に無い" % (f["name"], r["mask"]))


def ruled(lines):
    return ["/* " + RULE] + [" * " + ln if ln else " *" for ln in lines] + [" * " + RULE + " */"]


def render_cfg(cfg, ctx):
    check_dem(cfg, ctx)
    fns = cfg["functions"]
    w = max(len("FIM_FID_" + f["name"]) for f in fns)
    w = max(w, len("FIM_FUNCTION_COUNT"), len("FIM_INHIBITION_COUNT"))
    rows = sum(len(r) for r in expand(cfg))
    out = ruled(["機能 ID (FID) 定義",
                 "ASW (App_EngineManager / App_WarningIndicator) が",
                 "Rte_Call_FiM_GetFunctionPermission() の引数として使用する。",
                 "FID と監視対象イベントの対応（抑止の条件）は FiM_PBCfg.c の FiM_Functions[] （FiM.json から生成）。"])
    for i, f in enumerate(fns):
        out.append("#define %s  %dU  /**< %s */" % (("FIM_FID_" + f["name"]).ljust(w), i, f["title"]))
    out.append("#define %s  %dU  /**< 管理する機能（FID）の総数 */" % ("FIM_FUNCTION_COUNT".ljust(w), len(fns)))
    out.append("#define %s  %dU  /**< 抑止の条件（FiM_Functions[] の行）の総数。1 つの FID に複数あってよい */"
               % ("FIM_INHIBITION_COUNT".ljust(w), rows))
    return "\n".join(out)


def render_pbcfg(cfg, ctx):
    check_dem(cfg, ctx)
    fns = cfg["functions"]
    out = ruled(["FID × Dem イベント対応テーブル（抑止の条件 1 つが 1 行。同じ FID の行は、どれか 1 つが成り立てば抑止する）"])
    out.append("static const FiM_FunctionCfgType FiM_Functions[FIM_INHIBITION_COUNT] =")
    out.append("{")
    expanded = expand(cfg)
    for i, (f, rows) in enumerate(zip(fns, expanded)):
        out += block_comment(["FID %d: %s" % (i, f["title"])] + f["note"], indent=4)
        for j, r in enumerate(rows):
            last = (i == len(fns) - 1) and (j == len(rows) - 1)
            note = "  /* %s */" % r["summary"] if r["summary"] else ""
            out.append("    { FIM_FID_%s, %s, %s, %s }%s%s" % (f["name"], EVENT_SYMBOL + r["event"], r["mask"], r["cond"],
                                                              "" if last else ",", note))
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

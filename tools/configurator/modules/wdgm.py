"""WdgM（WdgM_Cfg.h / WdgM_PBCfg.c）の生成。

WdgM.json に書く値: 判定サイクル、監視対象エンティティ（監視する Os のタスク、Alive の期待回数、チェックポイント、
Logical 監視の許可遷移、Deadline 監視の許容時間）と、各項目の説明。
生成する値:
  - エンティティ数・ID、判定サイクル、Alive の期待回数のマクロ（WdgM_Cfg.h）
  - チェックポイント ID と Deadline の許容時間のマクロ（WdgM_Cfg.h）
  - 許可遷移テーブル、Deadline テーブル、エンティティテーブル（WdgM_PBCfg.c）
検査する内容（validate）:
  - チェックポイント ID が全エンティティで重複せず、WDGM_CP_INITIAL（0xFF）と衝突しない
  - 遷移・Deadline が、そのエンティティのチェックポイントを指している
  - Deadline の min <= max
  - 監視するタスク（osTask）の周期を Os.json のタスク表から引き、「終了→開始」の許容範囲に入っている
  - Alive の期待回数が、判定サイクルの間に実際に呼ばれる回数（サイクル ÷ タスク周期）を超えない
WdgM_Cfg.h の他の定数（EXPIRED の許容サイクル数、HW ウォッチドッグの周期とタイムアウト）は手書きのまま。
"""

from .common import doc_comment, os_periods


INITIAL = "INITIAL"
INITIAL_ID = 0xFF
RULE = "-" * 71


def camel(name):
    return "".join(p[0].upper() + p[1:].lower() for p in name.split("_") if p)


def validate(cfg):
    errors = []
    cycle = cfg["supervisionCycleMs"]
    names = [e["name"] for e in cfg["entities"]]
    if len(set(names)) != len(names):
        errors.append("entities: name が重複している")
    ids = {}
    for e in cfg["entities"]:
        cps = {c["name"]: c["id"] for c in e["checkpoints"]}
        if len(cps) != len(e["checkpoints"]):
            errors.append("entities %s: checkpoints の name が重複している" % e["name"])
        for c in e["checkpoints"]:
            if c["id"] == INITIAL_ID:
                errors.append("entities %s: チェックポイント %s の id が WDGM_CP_INITIAL (0xFF) と同じ" % (e["name"], c["name"]))
            if c["id"] in ids:
                errors.append("entities %s: チェックポイント %s の id=%d が %s と重複している" % (e["name"], c["name"], c["id"], ids[c["id"]]))
            ids[c["id"]] = "%s.%s" % (e["name"], c["name"])
        for t in e["transitions"]:
            if t["from"] != INITIAL and t["from"] not in cps:
                errors.append("entities %s: transitions の from '%s' が checkpoints に無い" % (e["name"], t["from"]))
            if t["to"] not in cps:
                errors.append("entities %s: transitions の to '%s' が checkpoints に無い" % (e["name"], t["to"]))
        pairs = set()
        for d in e["deadlines"]:
            for k in ("from", "to"):
                if d[k] not in cps:
                    errors.append("entities %s: deadlines の %s '%s' が checkpoints に無い" % (e["name"], k, d[k]))
            if d["minMs"] > d["maxMs"]:
                errors.append("entities %s: deadlines %s→%s の minMs が maxMs を超えている" % (e["name"], d["from"], d["to"]))
            if (d["from"], d["to"]) in pairs:
                errors.append("entities %s: deadlines %s→%s が重複している" % (e["name"], d["from"], d["to"]))
            pairs.add((d["from"], d["to"]))
    return errors


# ----------------------------------------------------------------------
def cp_macro(e, name):
    return "WDGM_CP_INITIAL" if name == INITIAL else "WDGM_CP_%s_%s" % (e["name"], name)


def ruled(lines):
    return ["/* " + RULE] + [" * " + ln if ln else " *" for ln in lines] + [" * " + RULE + " */"]


def deadline_macro(e, d, which):
    return "WDGM_%s_DEADLINE_%s_TO_%s_%s_MS" % (e["name"], d["from"], d["to"], which)


def check_periods(cfg, ctx):
    """監視するタスク（osTask）の周期を Os.json から引き、Alive の期待回数と Deadline の許容範囲と矛盾しないか検査する。"""
    periods = os_periods(ctx)
    cycle = cfg["supervisionCycleMs"]
    errors = []
    for e in cfg["entities"]:
        period = periods.get(e["osTask"])
        if period is None:
            errors.append("entities %s: osTask '%s' が Os.json の tasks に無い" % (e["name"], e["osTask"]))
            continue
        per_cycle = cycle // period
        if e["expectedAliveIndications"] > per_cycle:
            errors.append("entities %s: expectedAliveIndications=%d が、判定サイクルの間に呼ばれる回数 %d（%dms ÷ %s の周期 %dms）を超えている"
                          % (e["name"], e["expectedAliveIndications"], per_cycle, cycle, e["osTask"], period))
        first, last = e["checkpoints"][0]["name"], e["checkpoints"][-1]["name"]
        for d in e["deadlines"]:
            if d["to"] == first and d["from"] == last and not (d["minMs"] <= period <= d["maxMs"]):
                errors.append("entities %s: %s の周期 %dms が deadlines %s→%s の許容範囲 %d〜%dms に入っていない"
                              % (e["name"], e["osTask"], period, d["from"], d["to"], d["minMs"], d["maxMs"]))
    if errors:
        raise ValueError("; ".join(errors))


def render_cfg_entities(cfg, ctx):
    check_periods(cfg, ctx)
    out = doc_comment(["監視対象エンティティ総数"])
    out.append("#define WDGM_SUPERVISED_ENTITY_COUNT     %dU" % len(cfg["entities"]))
    for i, e in enumerate(cfg["entities"]):
        out.append("")
        out += doc_comment(["監視対象エンティティ ID: " + e["idDoc"]])
        out.append("#define %s %dU" % (("WDGM_ENTITY_" + e["name"]).ljust(32), i))
    out.append("")
    out += doc_comment(cfg["supervisionCycleDoc"])
    out.append("#define WDGM_SUPERVISION_CYCLE_MS        %dUL" % cfg["supervisionCycleMs"])
    for e in cfg["entities"]:
        out.append("")
        out += doc_comment(e["aliveDoc"])
        out.append("#define WDGM_%s_EXPECTED_ALIVE_INDICATIONS  %dU" % (e["name"], e["expectedAliveIndications"]))
    return "\n".join(out)


def render_cfg_checkpoints(cfg, ctx):
    check_periods(cfg, ctx)
    out = []
    for i, e in enumerate(cfg["entities"]):
        out += ruled(["論理監視 (Logical Supervision) チェックポイント ID"] + e["checkpointsDoc"])
        for c in e["checkpoints"]:
            out += doc_comment(c["doc"])
            out.append("#define %s  %dU" % (cp_macro(e, c["name"]), c["id"]))
        out.append("")
    out += doc_comment(["「まだ一度もチェックポイントが来ていない」ことを示す遷移元の特別値。",
                        "起動直後のみ有効な遷移元として許可テーブルに登場する。"])
    out.append("#define WDGM_CP_INITIAL       0x%02XU" % INITIAL_ID)
    for i, e in enumerate(cfg["entities"]):
        out.append("")
        out += ruled(["Deadline Supervision 許容経過時間 (Entity %d: %s 用)" % (i, e["name"])])
        for d in e["deadlines"]:
            out += doc_comment(d["doc"])
            out.append("#define %s   %dUL" % (deadline_macro(e, d, "MIN"), d["minMs"]))
            out.append("#define %s   %dUL" % (deadline_macro(e, d, "MAX"), d["maxMs"]))
    return "\n".join(out)


# ----------------------------------------------------------------------
def table_ident(e, kind):
    return "WdgM_%s%s" % (camel(e["name"]), kind)


def render_pbcfg(cfg, ctx):
    check_periods(cfg, ctx)
    out = []
    for i, e in enumerate(cfg["entities"]):
        if e["transitions"]:
            out += ruled(["論理監視 (Logical Supervision) 許可遷移テーブル — Entity %d 用" % i])
            out += doc_comment(e["transitionsDoc"])
            out.append("static const WdgM_TransitionCfgType %s[] =" % table_ident(e, "Transitions"))
            out.append("{")
            rows = []
            for t in e["transitions"]:
                rows.append(("{ %s, %s }" % (cp_macro(e, t["from"]), cp_macro(e, t["to"])), t.get("comment")))
            width = max(len(r[0]) for r in rows) + 2
            for k, (text, comment) in enumerate(rows):
                comma = "," if k < len(rows) - 1 else " "
                head = "    " + text + comma
                out.append((head.ljust(4 + width + 1) + (" /* " + comment + " */" if comment else "")).rstrip())
            out.append("};")
            out.append("")
        if e["deadlines"]:
            out += ruled(["時間監視 (Deadline Supervision) 許容テーブル — Entity %d 用" % i])
            out += doc_comment(e["deadlinesDoc"])
            out.append("static const WdgM_DeadlineCfgType %s[] =" % table_ident(e, "Deadlines"))
            out.append("{")
            for k, d in enumerate(e["deadlines"]):
                comma = "," if k < len(e["deadlines"]) - 1 else ""
                out.append("    { %s, %s," % (cp_macro(e, d["from"]), cp_macro(e, d["to"])))
                out.append("      %s, %s }%s" % (deadline_macro(e, d, "MIN"), deadline_macro(e, d, "MAX"), comma))
            out.append("};")
            out.append("")
    out += ruled(["Supervised Entity テーブル"])
    out.append("static const WdgM_EntityCfgType WdgM_Entities[WDGM_SUPERVISED_ENTITY_COUNT] =")
    out.append("{")
    for i, e in enumerate(cfg["entities"]):
        out.append("    /* Entity %d: %s" % (i, e["runnable"]))
        for k, ln in enumerate(e["note"]):
            out.append("     * " + ln + (" */" if k == len(e["note"]) - 1 else ""))
        if not e["note"]:
            out[-1] += " */"
        tr = table_ident(e, "Transitions")
        dl = table_ident(e, "Deadlines")
        out.append("    {")
        out.append("        WDGM_ENTITY_%s," % e["name"])
        out.append("        WDGM_SUPERVISION_CYCLE_MS,")
        out.append("        WDGM_%s_EXPECTED_ALIVE_INDICATIONS," % e["name"])
        if e["transitions"]:
            out.append("        %s," % tr)
            out.append("        (uint8)(sizeof(%s) / sizeof(%s[0]))," % (tr, tr))
        else:
            out += ["        NULL,", "        0U,"]
        if e["deadlines"]:
            out.append("        %s," % dl)
            out.append("        (uint8)(sizeof(%s) / sizeof(%s[0]))" % (dl, dl))
        else:
            out += ["        NULL,", "        0U"]
        out.append("    }" + ("," if i < len(cfg["entities"]) - 1 else ""))
    out.append("};")
    return "\n".join(out)


SPEC = {
    "config": "config/data/WdgM.json",
    "schema": "config/schema/WdgM.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/WdgM/WdgM_Cfg.h", "wdgm-entities", render_cfg_entities, None),
        ("src/Bsw/WdgM/WdgM_Cfg.h", "wdgm-checkpoints", render_cfg_checkpoints, None),
        ("src/Bsw/WdgM/WdgM_PBCfg.c", "wdgm-tables", render_pbcfg, None),
    ],
}

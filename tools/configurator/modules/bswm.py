"""BswM（BswM_PBCfg.c / BswM_Cfg.h）のルールテーブルの生成。

ルールは json に書いた順に並ぶ（添字がルール番号）。`perValue` を持つルールは、値ごとに 1 本ずつ展開する
（Dcm_CommunicationModeType の全通りに対する DCM_COMM_APPLY など）。
I-PDU グループは Com の `ipduGroups` の macro 名で参照する。
"""

from .common import block_comment, doc_comment

ACTIONS = {
    "ACTIVATE": "BSWM_ACTION_ACTIVATE",
    "DEACTIVATE": "BSWM_ACTION_DEACTIVATE",
    "PDU_GROUP_START": "BSWM_ACTION_PDU_GROUP_START",
    "PDU_GROUP_STOP": "BSWM_ACTION_PDU_GROUP_STOP",
    "DCM_COMM_APPLY": "BSWM_ACTION_DCM_COMM_APPLY",
}
# モードソースごとの値の書き方（EcuM / ComM の値は uint8 へキャストして渡す）
SOURCE_CAST = {"ECUM": "(uint8)", "COMM": "(uint8)", "DCM_COMM": ""}


def validate(cfg):
    errors = []
    for i, r in enumerate(cfg["rules"]):
        where = "rules[%d]" % i
        act = r["action"]
        if ("perValue" in r) == ("conditions" in r):
            errors.append("%s: conditions と perValue のどちらか一方だけを指定する" % where)
        if act in ("ACTIVATE", "DEACTIVATE") and "taskMask" not in r:
            errors.append("%s: %s には taskMask が必要" % (where, act))
        if act in ("PDU_GROUP_START", "PDU_GROUP_STOP") and "ipduGroup" not in r:
            errors.append("%s: %s には ipduGroup が必要" % (where, act))
        if act == "PDU_GROUP_START" and "initialize" not in r:
            errors.append("%s: PDU_GROUP_START には initialize が必要" % where)
        if act != "PDU_GROUP_START" and "initialize" in r:
            errors.append("%s: initialize は PDU_GROUP_START のときだけ指定する" % where)
        if act not in ("ACTIVATE", "DEACTIVATE") and "taskMask" in r:
            errors.append("%s: taskMask は ACTIVATE / DEACTIVATE のときだけ指定する" % where)
        if act not in ("PDU_GROUP_START", "PDU_GROUP_STOP") and "ipduGroup" in r:
            errors.append("%s: ipduGroup は PDU_GROUP_START / PDU_GROUP_STOP のときだけ指定する" % where)
        if r["operator"] == "OR" and "perValue" in r:
            errors.append("%s: perValue のルールは operator=AND（単一条件）にする" % where)
    return errors


def expand(cfg):
    """perValue を展開した (ルール, 条件のリスト, 先頭にコメントを出すか) の列。"""
    out = []
    for r in cfg["rules"]:
        if "perValue" in r:
            pv = r["perValue"]
            for j, v in enumerate(pv["values"]):
                out.append((r, [{"source": pv["source"], "value": v}], j == 0))
        else:
            out.append((r, r["conditions"], True))
    return out


def rule_lines(r, conds):
    label_w = len(".ConditionCount")
    pad = lambda name: ("." + name).ljust(label_w)
    cond_items = ["{ BSWM_MODE_SRC_%s, %s%s }" % (c["source"], SOURCE_CAST[c["source"]], c["value"]) for c in conds]
    prefix = "        %s = " % pad("Condition")
    cond_text = chr(44) + chr(10) + " " * (len(prefix) + 1)
    cond_text = cond_text.join(cond_items)
    rows = [
        "        %s = BSWM_OP_%s," % (pad("Operator"), r["operator"]),
        "%s{%s}," % (prefix, cond_text),
        "        %s = %dU," % (pad("ConditionCount"), len(conds)),
    ]
    tail = ["        %s = %s" % (pad("Action"), ACTIONS[r["action"]])]
    if "taskMask" in r:
        tail.append("        %s = %s" % (pad("TaskMask"), r["taskMask"]))
    if "ipduGroup" in r:
        tail.append("        %s = %s" % (pad("IpduGroupId"), r["ipduGroup"]))
    if "initialize" in r:
        tail.append("        %s = %s" % (pad("Initialize"), "TRUE" if r["initialize"] else "FALSE"))
    # 最後のフィールド以外にカンマを付ける
    tail = [t + "," for t in tail[:-1]] + [tail[-1]]
    return rows + tail


def render_rules(cfg, ctx):
    items = expand(cfg)
    out = []
    for i, (r, conds, with_comment) in enumerate(items):
        if with_comment and r.get("comment"):
            out += block_comment(r["comment"], 4)
        out.append("    {")
        out += rule_lines(r, conds)
        out.append("    }" + ("," if i < len(items) - 1 else ""))
    return "\n".join(out)


def render_count(cfg, ctx):
    out = block_comment(cfg["ruleCountDoc"])
    out.append("#define BSWM_RULE_COUNT  %dU" % len(expand(cfg)))
    return "\n".join(out)


SPEC = {
    "config": "config/data/BswM.json",
    "schema": "config/schema/BswM.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/BswM/BswM_PBCfg.c", "bswm-rules", render_rules, None),
        ("src/Bsw/BswM/BswM_Cfg.h", "bswm-rule-count", render_count, None),
    ],
}

"""Os（Os_PBCfg.c / Os_Cfg.h）のタスク表の生成。

Os.json の tasks[] に書く値: タスクとして呼ぶ関数、周期 [ms]、関数のヘッダ、1 行の説明、補足（任意）。
生成する値（並び順がそのままタスク ID と実行順序）:
  - Os_PBCfg.c: タスク表（Os_TaskTable）
  - Os_Cfg.h: タスク数（OS_TASK_COUNT）と、タスク ID のシンボリック名（OsConf_OsTask_<function>）
WdgM・CanNm・CanSM は、タスクの周期をこの json から引いて、自分の設定が周期と矛盾しないかを検査する（common.os_periods）。
"""

from . import symbols
from .common import block_comment, doc_comment

TASK_SYMBOL = "OsConf_OsTask_"


def validate(cfg):
    errors = []
    names = [t["function"] for t in cfg["tasks"]]
    if len(set(names)) != len(names):
        errors.append("tasks: function が重複している")
    return errors


def render_table(cfg, ctx):
    tasks = cfg["tasks"]
    label_w = len("Task %d" % (len(tasks) - 1))
    fn_w = max(len(t["function"]) for t in tasks) + 1
    out = ["static const Os_TaskType Os_TaskTable[OS_TASK_COUNT] =", "{"]
    for i, t in enumerate(tasks):
        if "note" in t:
            out += block_comment(t["note"], 4)
        last = i == len(tasks) - 1
        entry = "{ %s %5s }%s" % ((t["function"] + ",").ljust(fn_w), "%dU" % t["periodMs"], " " if last else ",")
        out.append("    /* %s */ %s  /* %d ms : %s */" % (("Task %d" % i).ljust(label_w), entry, t["periodMs"], t["summary"]))
    out.append("};")
    return "\n".join(out)


def render_cfg(cfg, ctx):
    out = doc_comment(cfg["taskCountDoc"])
    out.append("#define OS_TASK_COUNT  %dU" % len(cfg["tasks"]))
    out += ["", "/* シンボリック名: OsConf_OsTask_<function>（値はタスク表の添字 = タスク ID） */"]
    out += symbols.define_lines([[(TASK_SYMBOL + t["function"], i, True) for i, t in enumerate(cfg["tasks"])]])
    return "\n".join(out)


def check_includes(cfg, text):
    return ["%s がインクルードされていない（%s を使用）" % (h, "、".join(t["function"] for t in cfg["tasks"] if t["header"] == h))
            for h in sorted({t["header"] for t in cfg["tasks"]}) if '#include "%s"' % h not in text]


SPEC = {
    "config": "config/data/Os.json",
    "schema": "config/schema/Os.schema.json",
    "validate": validate,
    "targets": [
        ("src/Os/Os_PBCfg.c", "os-tasks", render_table, check_includes),
        ("src/Os/Os_Cfg.h", "os-tasks", render_cfg, None),
    ],
}

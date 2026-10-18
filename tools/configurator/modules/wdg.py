"""Wdg（Wdg_PBCfg.c）の Wdg_Config（ウォッチドッグドライバのポストビルド設定）の生成。

Wdg.json に書く値:
  defaultTimeout.macro : HW ウォッチドッグのタイムアウト [ms] を表すマクロ名（Wdg_ConfigType の DefaultTimeoutMs に入れる）。
                         値の実体は WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS の 1 か所だけに置く（Wdg.c は WdgM_Cfg.h を知らない）。
  defaultTimeout.doc   : 説明（コメントとして出す）
検査:
  - macro が WdgM_Cfg.h に実在し、値が 1〜65535（uint16）の整数であること（checks.py の check_wdg_timeout）。
生成する領域: Wdg_Config 全体（Wdg_PBCfg.c の wdg-config）。
"""

from .common import block_comment

MACRO_FILE = "src/Bsw/WdgM/WdgM_Cfg.h"


def validate(cfg):
    return []


def render_config(cfg, ctx=None):
    t = cfg["defaultTimeout"]
    out = block_comment(t["doc"], indent=0) if t.get("doc") else []
    out += [
        "const Wdg_ConfigType Wdg_Config =",
        "{",
        "    (uint16)%s  /* DefaultTimeoutMs */" % t["macro"],
        "};",
    ]
    return "\n".join(out)


SPEC = {
    "config": "config/data/Wdg.json",
    "schema": "config/schema/Wdg.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Wdg/Wdg_PBCfg.c", "wdg-config", render_config, None),
    ],
}

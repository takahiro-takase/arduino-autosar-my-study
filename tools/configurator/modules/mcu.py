"""Mcu（Mcu_PBCfg.c）の Mcu_Config（MCU ドライバのポストビルド設定）の生成。

本プロジェクトの Mcu は、クロック初期化を Arduino フレームワークに任せ、RAM セクション初期化・複数電源モードも
モデル化しない（Mcu.h 冒頭のコメント参照）。そのため Mcu_ConfigType は実フィールドを持たないプレースホルダ（Dummy）である。
Mcu.json は、AUTOSAR の McuModuleConfiguration コンテナの置き場所として用意したもので、
将来クロック設定・リセット要因の扱いなどを足すときに、ここへ項目を足していく。

Mcu.json に書く値:
  placeholder.value : プレースホルダ（Dummy）に入れる値。意味は無い（0）。
  placeholder.doc   : 説明（コメントとして出す）
生成する領域: Mcu_Config 全体（Mcu_PBCfg.c の mcu-config）。
"""

from .common import block_comment


def validate(cfg):
    return []


def render_config(cfg, ctx=None):
    p = cfg["placeholder"]
    out = block_comment(p["doc"], indent=0) if p.get("doc") else []
    out += [
        "const Mcu_ConfigType Mcu_Config =",
        "{",
        "    %dU  /* Dummy */" % p["value"],
        "};",
    ]
    return "\n".join(out)


SPEC = {
    "config": "config/data/Mcu.json",
    "schema": "config/schema/Mcu.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Mcu/Mcu_PBCfg.c", "mcu-config", render_config, None),
    ],
}

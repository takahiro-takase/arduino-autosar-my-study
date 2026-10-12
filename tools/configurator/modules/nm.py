"""Nm（Nm_Cfg.h）の NM チャネルのハンドルの定義の生成。

Nm.json に書く値: NM チャネルのハンドル値と、その説明。
検査: ハンドルが ComM.json のチャネル 0 の ID と一致すること（Nm は ComM のチャネルと同じ番号で呼ばれるため。checks.py で検査する）。
"""

from .defs import render_layout

LAYOUT = (("define", "mainNetworkHandle", "NM_MAIN_NETWORK_HANDLE", "%dU"),)


def validate(cfg):
    return []


def render(cfg, ctx):
    return render_layout(cfg, LAYOUT)


SPEC = {
    "config": "config/data/Nm.json",
    "schema": "config/schema/Nm.schema.json",
    "validate": validate,
    "targets": [("src/Bsw/Nm/Nm_Cfg.h", "nm-params", render, None)],
}

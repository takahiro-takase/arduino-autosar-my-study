"""ComM（ComM_Cfg.h）のチャネル数・ユーザ数・ID の定義の生成。

ComM.json に書く値: チャネル数、ユーザ数、チャネル 0 とユーザ 0 の ID、各項目の説明。
検査: チャネル数とユーザ数は 1 のみ対応。ID がそれぞれの数より小さいこと。チャネル数が CanSM.json と一致することは checks.py で検査する。
"""

from .defs import render_layout

LAYOUT = (
    ("define", "channelCount", "COMM_CHANNEL_COUNT", "%dU"),
    ("define", "userCount", "COMM_USER_COUNT", "%dU"),
    ("define", "channel0", "COMM_CHANNEL_0", "%dU"),
    ("define", "user0", "COMM_USER_0", "%dU"),
)


def validate(cfg):
    errors = []
    if cfg["channelCount"]["value"] != 1 or cfg["userCount"]["value"] != 1:
        errors.append("channelCount と userCount は 1 のみ対応（ComM.c が 1 チャネル・1 ユーザ固定で、COMM_CHANNEL_1 などは生成しないため）")
    if cfg["channel0"]["value"] >= cfg["channelCount"]["value"]:
        errors.append("channel0 は channelCount より小さくする")
    if cfg["user0"]["value"] >= cfg["userCount"]["value"]:
        errors.append("user0 は userCount より小さくする")
    return errors


def render(cfg, ctx):
    return render_layout(cfg, LAYOUT)


SPEC = {
    "config": "config/data/ComM.json",
    "schema": "config/schema/ComM.schema.json",
    "validate": validate,
    "targets": [("src/Bsw/ComM/ComM_Cfg.h", "comm-params", render, None)],
}

"""Gpt（Gpt_PBCfg.h / Gpt_PBCfg.c）のチャネル数・チャネル ID の定義と、チャネルテーブルの生成。

Gpt.json に書く値:
  items[]    : チャネル数 GPT_CHANNEL_COUNT と、チャネル ID GPT_CHANNEL_* の #define（形式は defines.py 参照）
  channels[] : チャネルテーブル Gpt_ChannelTable の各行。id（GPT_CHANNEL_* のマクロ名）、mode（CONTINUOUS / ONESHOT）、
               tickFrequencyHz、tickValueMax（C のリテラル）、notification（通知関数名。無ければ null）、
               見出し heading と、各列の説明 notes
検査: GPT_CHANNEL_COUNT が channels の行数と一致する。行の id が、items で定義した GPT_CHANNEL_* で、その値が行の添字と一致する。
"""

from . import defines
from .defs import join

MODES = {"CONTINUOUS": "GPT_CH_MODE_CONTINUOUS", "ONESHOT": "GPT_CH_MODE_ONESHOT"}


def validate(cfg):
    errors = []
    vals = defines.values(cfg)
    if "GPT_CHANNEL_COUNT" not in vals:
        return ["GPT_CHANNEL_COUNT が無い"]
    if defines.number(vals["GPT_CHANNEL_COUNT"]) != len(cfg["channels"]):
        errors.append("GPT_CHANNEL_COUNT=%d が channels の行数 %d と一致しない" % (defines.number(vals["GPT_CHANNEL_COUNT"]), len(cfg["channels"])))
    for i, ch in enumerate(cfg["channels"]):
        if ch["id"] not in vals:
            errors.append("channels[%d]: id '%s' が items に定義されていない" % (i, ch["id"]))
        elif defines.number(vals[ch["id"]]) != i:
            errors.append("channels[%d]: %s=%d が行の添字と一致しない（Gpt_ChannelTable の添字がそのままチャネル番号）" % (i, ch["id"], defines.number(vals[ch["id"]])))
        if ch["tickFrequencyHz"] <= 0:
            errors.append("channels[%d]: tickFrequencyHz は 0 より大きくする" % i)
    return errors


def render_table(cfg, ctx=None):
    out = [
        "/* -----------------------------------------------------------------------",
        " * チャネルテーブル",
        " * インデックスがそのままチャネル番号 (Gpt_ChannelType) に対応する。",
        " * ----------------------------------------------------------------------- */",
        "static const Gpt_ChannelConfigType Gpt_ChannelTable[GPT_CHANNEL_COUNT] =",
        "{",
    ]
    n = len(cfg["channels"])
    for i, ch in enumerate(cfg["channels"]):
        nt = ch.get("notes", {})
        out.append("    /* %s */" % ch["heading"])
        out.append("    {")
        fields = (
            ("ChannelId", ch["id"], None),
            ("Mode", MODES[ch["mode"]], None),
            ("TickFrequencyHz", "%dU" % ch["tickFrequencyHz"], nt.get("tickFrequencyHz")),
            ("TickValueMax", ch["tickValueMax"], nt.get("tickValueMax")),
            ("Notification", ch["notification"] or "NULL", nt.get("notification")),
        )
        for j, (label, value, note) in enumerate(fields):
            comma = "," if j < len(fields) - 1 else ""
            cell = (value + comma).ljust(24)
            comment = "/* %s%s */" % (label.ljust(17), (": " + note) if note else "")
            if not note:
                comment = "/* %s*/" % label.ljust(17)
            out.append(("        %s%s" % (cell, comment)).rstrip())
        out.append("    }" + ("," if i < n - 1 else ""))
    out.append("};")
    return join([out])


SPEC = {
    "config": "config/data/Gpt.json",
    "schema": "config/schema/Gpt.schema.json",
    "validate": lambda cfg: defines.validate_items(cfg) + validate(cfg),
    "targets": [
        ("src/Bsw/Gpt/Gpt_PBCfg.h", "gpt-channels", defines.render_items, None),
        ("src/Bsw/Gpt/Gpt_PBCfg.c", "gpt-table", render_table, None),
    ],
}

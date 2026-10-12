"""CanNm（CanNm_Cfg.h）の NM フレーム・タイマの定義の生成。

CanNm.json に書く値: NM の周期・フレーム長・ノード識別子・各タイマ [ms]、NM フレームの CanIf の PDU、各項目の説明。
NM フレームの CanIf の PDU は CanIf.json の名前で指し、生成物ではシンボリック名（CanIfConf_*_<name>）にする。
検査: 周期が Os.json の CanNm_MainFunction の周期と一致すること、CanIf の PDU の名前と上位層の種類。
"""

from . import symbols
from .common import os_periods
from .defs import render_layout

MAIN_FUNCTION = "CanNm_MainFunction"

# 並び順どおりに出力する（形式は defs.render_layout を参照）。値を計算で作る項目は render() の overrides で上書きする。
LAYOUT = (
    ("section", "frameSection"),
    ("define", "cycleMs", "CANNM_CYCLE_MS", "%dUL"),
    ("define", "dlc", "CANNM_DLC", "%dU"),
    ("define", "sourceNodeId", "CANNM_SOURCE_NODE_ID", "%s"),
    ("define", "canifTxPdu", "CANNM_CANIF_TX_PDU_ID", "%s"),
    ("define", "canifRxPdu", "CANNM_CANIF_RX_PDU_ID", "%s"),
    ("section", "timerSection"),
    ("define", "timeoutMs", "CANNM_TIMEOUT_MS", "%dUL"),
    ("define", "repeatMessageMs", "CANNM_REPEAT_MESSAGE_MS", "%dUL"),
    ("define", "waitBusSleepMs", "CANNM_WAIT_BUS_SLEEP_MS", "%dUL"),
)


def canif_pdu(cfg, ctx, key, table, prefix, upper):
    name = cfg[key]["pdu"]
    for p in ctx.configs["CanIf"][table]:
        if p["name"] == name:
            if p[upper] != "CANNM":
                raise ValueError("CanNm.json %s: CanIf の PDU '%s' の上位層は CANNM ではない（%s）" % (key, name, p[upper]))
            return prefix + name
    raise ValueError("CanNm.json %s: CanIf.json の %s に '%s' が無い" % (key, table, name))


def check_cycle(cfg, ctx):
    period = os_periods(ctx).get(MAIN_FUNCTION)
    if period is None:
        return ["Os.json の tasks に %s が無い" % MAIN_FUNCTION]
    if period != cfg["cycleMs"]["value"]:
        return ["cycleMs=%d が Os.json の %s の周期 %d ms と一致しない" % (cfg["cycleMs"]["value"], MAIN_FUNCTION, period)]
    return []


def validate(cfg):
    errors = []
    if cfg["timeoutMs"]["value"] <= cfg["cycleMs"]["value"]:
        errors.append("timeoutMs は cycleMs より長くする（周期内に送受信できず誤ってタイムアウトするため）")
    for key in ("repeatMessageMs", "waitBusSleepMs"):
        if cfg[key]["value"] < cfg["cycleMs"]["value"]:
            errors.append("%s は cycleMs 以上にする（NM フレームを 1 回も送らずに状態が終わるため）" % key)
    return errors


def render(cfg, ctx):
    errors = check_cycle(cfg, ctx)
    if errors:
        raise ValueError("CanNm.json: " + " / ".join(errors))
    return render_layout(cfg, LAYOUT, {
        "sourceNodeId": "0x%02XU" % int(cfg["sourceNodeId"]["value"], 16),
        "canifTxPdu": canif_pdu(cfg, ctx, "canifTxPdu", "txPdus", symbols.CANIF_TX, "txConfirm"),
        "canifRxPdu": canif_pdu(cfg, ctx, "canifRxPdu", "rxPdus", symbols.CANIF_RX, "rxIndication"),
    })


SPEC = {
    "config": "config/data/CanNm.json",
    "schema": "config/schema/CanNm.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/CanNm/CanNm_Cfg.h", "cannm-params", render, lambda cfg, text: symbols.include_warnings(text)),
    ],
}

"""CanNm（CanNm_Cfg.h）の NM フレーム・タイマの定義の生成。

CanNm.json に書く値: NM の周期・フレーム長・ノード識別子・各タイマ [ms]、NM フレームの CanIf の PDU、各項目の説明。
NM フレームの CanIf の PDU は CanIf.json の名前で指し、生成物ではシンボリック名（CanIfConf_*_<name>）にする。
検査: 周期が Os_PBCfg.c の CanNm_MainFunction の周期と一致すること、CanIf の PDU の名前と上位層の種類。
"""

from . import symbols
from .common import os_periods
from .defs import define, join, section

MAIN_FUNCTION = "CanNm_MainFunction"


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
        return ["Os_PBCfg.c の Os_TaskTable に %s が無い" % MAIN_FUNCTION]
    if period != cfg["cycleMs"]["value"]:
        return ["cycleMs=%d が Os_PBCfg.c の %s の周期 %d ms と一致しない" % (cfg["cycleMs"]["value"], MAIN_FUNCTION, period)]
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
    tx = canif_pdu(cfg, ctx, "canifTxPdu", "txPdus", symbols.CANIF_TX, "txConfirm")
    rx = canif_pdu(cfg, ctx, "canifRxPdu", "rxPdus", symbols.CANIF_RX, "rxIndication")
    g = lambda key, name, value: define(name, value, cfg[key]["doc"], cfg[key].get("unused", False))
    return join([
        section(cfg["frameSection"]),
        g("cycleMs", "CANNM_CYCLE_MS", "%dUL" % cfg["cycleMs"]["value"]),
        g("dlc", "CANNM_DLC", "%dU" % cfg["dlc"]["value"]),
        g("sourceNodeId", "CANNM_SOURCE_NODE_ID", "0x%02XU" % int(cfg["sourceNodeId"]["value"], 16)),
        g("canifTxPdu", "CANNM_CANIF_TX_PDU_ID", tx),
        g("canifRxPdu", "CANNM_CANIF_RX_PDU_ID", rx),
        section(cfg["timerSection"]),
        g("timeoutMs", "CANNM_TIMEOUT_MS", "%dUL" % cfg["timeoutMs"]["value"]),
        g("repeatMessageMs", "CANNM_REPEAT_MESSAGE_MS", "%dUL" % cfg["repeatMessageMs"]["value"]),
        g("waitBusSleepMs", "CANNM_WAIT_BUS_SLEEP_MS", "%dUL" % cfg["waitBusSleepMs"]["value"]),
    ])


SPEC = {
    "config": "config/data/CanNm.json",
    "schema": "config/schema/CanNm.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/CanNm/CanNm_Cfg.h", "cannm-params", render, lambda cfg, text: symbols.include_warnings(text)),
    ],
}

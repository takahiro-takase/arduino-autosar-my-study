"""CanTp（CanTp_Cfg.h）のバッファ・フロー制御・タイムアウト・ID の定義の生成。

CanTp.json に書く値: RX/TX バッファの大きさ、ブロックサイズと STmin、各タイムアウト [ms]、N-SDU の ID、
診断応答の PduR の経路、各項目の説明。PduR の経路は PduR.json の txPaths の名前で指し、
生成物ではシンボリック名（PduRConf_PduRSrcPdu_<name>）にする。
"""

from . import symbols
from .defs import define, join, section

# 並び順どおりに出力する。("section", キー) は見出し、("define", キー, マクロ名, 書式) は定義。書式が None のものは PduR のシンボル。
LAYOUT = (
    ("section", "bufferSection"),
    ("define", "rxBufferSize", "CANTP_RX_BUFFER_SIZE", "%dU"),
    ("define", "txBufferSize", "CANTP_TX_BUFFER_SIZE", "%dU"),
    ("section", "flowControlSection"),
    ("define", "blockSize", "CANTP_BLOCK_SIZE", "%dU"),
    ("define", "stMin", "CANTP_ST_MIN", "%dU"),
    ("section", "timeoutSection"),
    ("define", "nCrTimeoutMs", "CANTP_N_CR_TIMEOUT_MS", "%dUL"),
    ("define", "nBsTimeoutMs", "CANTP_N_BS_TIMEOUT_MS", "%dUL"),
    ("define", "nAsTimeoutMs", "CANTP_N_AS_TIMEOUT_MS", "%dUL"),
    ("section", "idSection"),
    ("define", "pdurTxSrcPdu", "CANTP_PDUR_TX_SDU_ID", None),
    ("define", "txSduId", "CANTP_TX_SDU_ID", "%dU"),
    ("define", "rxSduId", "CANTP_RX_SDU_ID", "%dU"),
)


def pdur_symbol(cfg, ctx):
    name = cfg["pdurTxSrcPdu"]["pdu"]
    for p in ctx.configs["PduR"]["txPaths"]:
        if p["name"] == name:
            if p.get("confModule") != "CANTP":
                raise ValueError("CanTp.json pdurTxSrcPdu: PduR の TX 経路 '%s' の上位層は CANTP ではない（%s）" % (name, p.get("confModule")))
            return symbols.PDUR_SRC + name
    raise ValueError("CanTp.json pdurTxSrcPdu: PduR.json の txPaths に '%s' が無い" % name)


def render(cfg, ctx):
    pdur = pdur_symbol(cfg, ctx)
    blocks = []
    for item in LAYOUT:
        if item[0] == "section":
            blocks.append(section(cfg[item[1]]))
        else:
            _, key, macro, fmt = item
            blocks.append(define(macro, pdur if fmt is None else fmt % cfg[key]["value"], cfg[key]["doc"]))
    return join(blocks)


SPEC = {
    "config": "config/data/CanTp.json",
    "schema": "config/schema/CanTp.schema.json",
    "validate": lambda cfg: [],
    "targets": [
        ("src/Bsw/CanTp/CanTp_Cfg.h", "cantp-params", render, lambda cfg, text: symbols.include_warnings(text)),
    ],
}

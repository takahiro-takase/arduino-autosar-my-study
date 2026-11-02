"""CanTp（CanTp_Cfg.h）のバッファ・フロー制御・タイムアウト・ID の定義の生成。

CanTp.json に書く値: ブロックサイズと STmin、各タイムアウト [ms]、N-SDU の ID、Dcm の I-PDU の ID、
診断応答を送る CanIf の TX PDU、N-PDU の ID（CanIf が CanTp へ渡す）、各項目の説明（CanTp は組み立て・分割のバッファを持たず、Dcm の受信・送信バッファを使う）。
CanIf の TX PDU は CanIf.json の txPdus の名前で指し、生成物ではシンボリック名（CanIfConf_CanIfTxPduCfg_<name>）にする。
"""

from . import symbols
from .defs import render_layout

# 並び順どおりに出力する。("section", キー) は見出し、("define", キー, マクロ名, 書式) は定義。canIfTxPdu は値を CanIf のシンボルで上書きする。
LAYOUT = (
    ("section", "flowControlSection"),
    ("define", "blockSize", "CANTP_BLOCK_SIZE", "%dU"),
    ("define", "stMin", "CANTP_ST_MIN", "%dU"),
    ("section", "timeoutSection"),
    ("define", "nCrTimeoutMs", "CANTP_N_CR_TIMEOUT_MS", "%dUL"),
    ("define", "nBsTimeoutMs", "CANTP_N_BS_TIMEOUT_MS", "%dUL"),
    ("define", "nAsTimeoutMs", "CANTP_N_AS_TIMEOUT_MS", "%dUL"),
    ("section", "idSection"),
    ("define", "canIfTxPdu", "CANTP_CANIF_TX_PDU_ID", "%s"),
    ("define", "txNPduId", "CANTP_TX_NPDU_ID", "%dU"),
    ("define", "rxNPduId", "CANTP_RX_NPDU_ID", "%dU"),
    ("define", "txSduId", "CANTP_TX_SDU_ID", "%dU"),
    ("define", "rxSduId", "CANTP_RX_SDU_ID", "%dU"),
    ("define", "dcmRxPduId", "CANTP_DCM_RX_PDU_ID", "%dU"),
    ("define", "dcmTxPduId", "CANTP_DCM_TX_PDU_ID", "%dU"),
)


def canif_symbol(cfg, ctx):
    name = cfg["canIfTxPdu"]["pdu"]
    for p in ctx.configs["CanIf"]["txPdus"]:
        if p["name"] == name:
            if p.get("txConfirm") != "CANTP":
                raise ValueError("CanTp.json canIfTxPdu: CanIf の TX PDU '%s' の上位層は CANTP ではない（%s）" % (name, p.get("txConfirm")))
            return symbols.CANIF_TX + name
    raise ValueError("CanTp.json canIfTxPdu: CanIf.json の txPdus に '%s' が無い" % name)


def render(cfg, ctx):
    return render_layout(cfg, LAYOUT, {"canIfTxPdu": canif_symbol(cfg, ctx)})


SPEC = {
    "config": "config/data/CanTp.json",
    "schema": "config/schema/CanTp.schema.json",
    "validate": lambda cfg: [],
    "targets": [
        ("src/Bsw/CanTp/CanTp_Cfg.h", "cantp-params", render, lambda cfg, text: symbols.include_warnings(text)),
    ],
}

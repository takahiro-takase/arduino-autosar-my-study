"""CanIf（CanIf_PBCfg.c / CanIf_Cfg.h）の生成。

CAN ID と DLC は、`frame` が指すフレームの定義（config/data/can_signals.json）から引く。
信号表に無いフレーム（UDS 診断など）だけ、`canId` と `dlc` を直接書く。
"""

from .common import doc_comment, hex_literal, note_lines, ruled_comment, struct_fields

TX_CONFIRM_FCT = {"PDUR": "PduR_CanIfTxConfirmation", "CANNM": "CanNm_TxConfirmation"}
RX_IND_FCT = {"PDUR": "PduR_CanIfRxIndication", "CANNM": "CanNm_RxIndication"}
MODULE_HEADER = {"PDUR": "PduR_CanIf.h", "CANNM": "CanNm.h"}
DAVINCI_TX = "/ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg/"
DAVINCI_RX = "/ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg/"


def validate(cfg):
    errors = []
    for key in ("txPdus", "rxPdus"):
        names = [p["name"] for p in cfg[key]]
        if len(set(names)) != len(names):
            errors.append("%s: name が重複している" % key)
    for p in cfg["txPdus"] + cfg["rxPdus"]:
        if ("frame" in p) == ("canId" in p):
            errors.append("%s: frame と canId のどちらか一方だけを指定する" % p["name"])
        if "frame" not in p and "dlc" not in p:
            errors.append("%s: frame を使わない場合は dlc が必要" % p["name"])
        if "frame" in p and "dlc" in p:
            errors.append("%s: frame を使う場合、dlc は信号表から引くため指定しない" % p["name"])
    return errors


def resolve(pdu, ctx):
    """(CAN ID の文字列, DLC) を返す。frame 指定なら信号表から引く。"""
    if "frame" in pdu:
        f = ctx.frames[pdu["frame"]]
        return f["canId"], f["dlc"]
    return pdu["canId"], pdu["dlc"]


def tx_table(cfg, ctx):
    out = [
        "/* -----------------------------------------------------------------------",
        " * TX PDU ルーティングテーブル",
        " * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg",
        " * TxPduId をインデックスとして CanIf_Transmit() が参照する。",
        " * ----------------------------------------------------------------------- */",
        "static const CanIf_TxPduConfigType CanIf_TxPduConfigData[CANIF_TX_PDU_COUNT] = {",
    ]
    n = len(cfg["txPdus"])
    for i, p in enumerate(cfg["txPdus"]):
        can_id, dlc = resolve(p, ctx)
        nt = p.get("notes", {})
        out.append("    {")
        out += ruled_comment(
            ["TxPduId=%d: %s" % (i, p["title"]), "DaVinci: " + DAVINCI_TX + p["name"]] + p.get("note", []), 8
        )
        out += struct_fields(
            [
                ("UpperLayerTxPduId", "%dU" % p["upperLayerTxPduId"],
                 note_lines("DaVinci: CanIfTxPduId", nt.get("upperLayerTxPduId"))),
                ("CanId", hex_literal(can_id), note_lines("DaVinci: CanIfTxPduCanId")),
                ("Dlc", "%dU" % dlc, note_lines("DaVinci: CanIfTxPduDlc", nt.get("dlc"))),
                ("Hth", "%dU" % p["hth"], note_lines("DaVinci: CanIfTxPduHthIdRef")),
                ("TxConfirmFct", TX_CONFIRM_FCT[p["txConfirm"]],
                 note_lines("DaVinci: CanIfTxPduUserTxConfirmationName", nt.get("txConfirm"))),
            ],
            8,
        )
        out.append("    }" + ("," if i < n - 1 else ""))
    out += ["};", ""]
    return out


def rx_table(cfg, ctx):
    out = [
        "/* -----------------------------------------------------------------------",
        " * RX PDU ルーティングテーブル",
        " * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg",
        " * HOH と CAN ID の組み合わせで CanIf_RxIndication() が検索する。",
        " * ----------------------------------------------------------------------- */",
        "static const CanIf_RxPduConfigType CanIf_RxPduConfigData[CANIF_RX_PDU_COUNT] = {",
    ]
    n = len(cfg["rxPdus"])
    for i, p in enumerate(cfg["rxPdus"]):
        can_id, dlc = resolve(p, ctx)
        nt = p.get("notes", {})
        up_extra = []
        if p["rxIndication"] == "PDUR":
            up_extra.append("→ PduR RX パス %d へのリンク" % p["upperLayerRxPduId"])
        up_extra += nt.get("upperLayerRxPduId", [])
        out.append("    {")
        out += ruled_comment(
            ["RxPduId=%d: %s" % (i, p["title"]), "DaVinci: " + DAVINCI_RX + p["name"]] + p.get("note", []), 8
        )
        out += struct_fields(
            [
                ("CanId", hex_literal(can_id), note_lines("DaVinci: CanIfRxPduCanId")),
                ("Hrh", "%dU" % p["hrh"], note_lines("DaVinci: CanIfRxPduHrhIdRef")),
                ("UpperLayerRxPduId", "%dU" % p["upperLayerRxPduId"],
                 note_lines("DaVinci: CanIfRxPduUpperLayerPduId", up_extra)),
                ("Dlc", "%dU" % dlc, note_lines("DaVinci: CanIfRxPduDataLength", nt.get("dlc"))),
                ("RxIndicationFct", RX_IND_FCT[p["rxIndication"]],
                 note_lines("DaVinci: CanIfRxPduUserRxIndicationName")),
            ],
            8,
        )
        out.append("    }" + ("," if i < n - 1 else ""))
    out += ["};", ""]
    return out


def config_instance(cfg, ctx):
    return [
        "/* -----------------------------------------------------------------------",
        " * CanIf ポストビルド設定インスタンス",
        " * CanIf_Init() の引数として渡す。",
        " * ----------------------------------------------------------------------- */",
        "const CanIf_ConfigType CanIf_Config = {",
        "    .TxPduConfig = CanIf_TxPduConfigData,",
        "    .TxPduCount  = CANIF_TX_PDU_COUNT,",
        "    .RxPduConfig = CanIf_RxPduConfigData,",
        "    .RxPduCount  = CANIF_RX_PDU_COUNT",
        "};",
    ]


def render_pbcfg(cfg, ctx):
    return "\n".join(tx_table(cfg, ctx) + rx_table(cfg, ctx) + config_instance(cfg, ctx))


def render_cfg_h(cfg, ctx):
    out = doc_comment(cfg["txPduCountDoc"])
    out.append("#define CANIF_TX_PDU_COUNT  %dU" % len(cfg["txPdus"]))
    out.append("")
    out += doc_comment(cfg["rxPduCountDoc"])
    out.append("#define CANIF_RX_PDU_COUNT  %dU" % len(cfg["rxPdus"]))
    return "\n".join(out)


def check_includes(cfg, text):
    used = {p["txConfirm"] for p in cfg["txPdus"]} | {p["rxIndication"] for p in cfg["rxPdus"]}
    return [
        "%s がインクルードされていない（%s を使用）" % (MODULE_HEADER[m], m)
        for m in sorted(used)
        if '#include "%s"' % MODULE_HEADER[m] not in text
    ]


SPEC = {
    "config": "config/data/CanIf.json",
    "schema": "config/schema/CanIf.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/CanIf/CanIf_PBCfg.c", "canif-pdus", render_pbcfg, check_includes),
        ("src/Bsw/CanIf/CanIf_Cfg.h", "canif-counts", render_cfg_h, None),
    ],
}

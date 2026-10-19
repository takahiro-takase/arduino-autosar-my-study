"""CanIf（CanIf_PBCfg.c / CanIf_Cfg.h）の生成。

CAN ID と DLC は、`frame` が指すフレームの定義（config/data/can_signals.json）から引く。
信号表に無いフレーム（UDS 診断など）だけ、`canId` と `dlc` を直接書く。
"""

from . import symbols
from .common import doc_comment, hex_literal, note_lines, ruled_comment, struct_fields

TX_CONFIRM_FCT = {"PDUR": "PduR_CanIfTxConfirmation", "CANNM": "CanNm_TxConfirmation",
                  "CANTSYN": "CanTSyn_TxConfirmation"}
RX_IND_FCT = {"PDUR": "PduR_CanIfRxIndication", "CANNM": "CanNm_RxIndication",
              "CANTSYN": "CanTSyn_RxIndication"}
MODULE_HEADER = {"PDUR": "PduR_CanIf.h", "CANNM": "CanNm.h", "CANTSYN": "CanTSyn.h"}
DAVINCI_TX = "/ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg/"
DAVINCI_RX = "/ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg/"


def validate(cfg):
    errors = []
    for key in ("txPdus", "rxPdus"):
        names = [p["name"] for p in cfg[key]]
        if len(set(names)) != len(names):
            errors.append("%s: name が重複している" % key)
    for p in cfg["txPdus"]:
        # 上位層へ返す ID は、PduR 宛てのときだけ数値で指定する（NM 宛ては CanNm.json の名前から引く）
        if (p["txConfirm"] == "PDUR") != ("upperLayerTxPduId" in p):
            errors.append("%s: upperLayerTxPduId は txConfirm が PDUR のときだけ指定する" % p["name"])
    for p in cfg["rxPdus"]:
        if (p["rxIndication"] == "PDUR") != ("upperLayerRxPduId" in p):
            errors.append("%s: upperLayerRxPduId は rxIndication が PDUR のときだけ指定する" % p["name"])
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


def upper_tx_value(p, index, ctx):
    """上位層へ返す送信確認の ID。PduR 宛てなら PduR の送信元 PDU、NM 宛てなら CanNm の TX PDU のシンボリック名、
    時刻同期宛ては CanTSyn_Cfg.h の CANTSYN_TX_PDU_ID。"""
    if p["txConfirm"] == "PDUR":
        return symbols.pdur_src_for_canif_tx(ctx, p, index)
    if p["txConfirm"] == "CANTSYN":
        return "CANTSYN_TX_PDU_ID"
    return symbols.cannm_tx_for_canif(ctx, p)


def upper_rx_value(p, ctx):
    if p["rxIndication"] == "PDUR":
        return symbols.pdur_src_for_canif_rx(ctx, p)
    if p["rxIndication"] == "CANTSYN":
        return "CANTSYN_RX_PDU_ID"
    return symbols.cannm_rx_for_canif(ctx, p)


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
                ("UpperLayerTxPduId", upper_tx_value(p, i, ctx),
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
                ("UpperLayerRxPduId", upper_rx_value(p, ctx),
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


def render_symbols(cfg, ctx):
    """CanIf が提供するハンドル ID のシンボリック名（[TPS_ECUC_02108]）。値はテーブルの添字。"""
    symbols.check_unique([p["name"] for p in cfg["txPdus"]], "CanIf txPdus")
    symbols.check_unique([p["name"] for p in cfg["rxPdus"]], "CanIf rxPdus")
    out = ["/* シンボリック名: CanIfConf_<コンテナ定義>_<name>（値は各テーブルの添字）。PduR などの上位層が参照する。 */"]
    # 使うかどうかは PduR の設定しだいだが、CanIf の出力が PduR の設定に左右されないよう、全部に
    # MISRA 2.5（未使用マクロ）の抑止コメントを付ける（使われている名前に付けても指摘は出ない）
    out += symbols.define_lines([
        [(symbols.CANIF_TX + p["name"], i, True) for i, p in enumerate(cfg["txPdus"])],
        [(symbols.CANIF_RX + p["name"], i, True) for i, p in enumerate(cfg["rxPdus"])],
    ])
    return "\n".join(out)


def check_includes(cfg, text):
    used = {p["txConfirm"] for p in cfg["txPdus"]} | {p["rxIndication"] for p in cfg["rxPdus"]}
    msgs = [
        "%s がインクルードされていない（%s を使用）" % (MODULE_HEADER[m], m)
        for m in sorted(used)
        if '#include "%s"' % MODULE_HEADER[m] not in text
    ]
    return msgs + symbols.include_warnings(text)


SPEC = {
    "config": "config/data/CanIf.json",
    "schema": "config/schema/CanIf.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/CanIf/CanIf_PBCfg.c", "canif-pdus", render_pbcfg, check_includes),
        ("src/Bsw/CanIf/CanIf_Cfg.h", "canif-counts", render_cfg_h, None),
        ("src/Bsw/CanIf/CanIf_Cfg.h", "canif-symbols", render_symbols, None),
    ],
}

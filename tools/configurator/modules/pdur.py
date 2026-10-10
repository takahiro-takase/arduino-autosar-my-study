"""PduR（PduR_PBCfg.c / PduR_Cfg.h）の生成。"""

from .common import block_comment, doc_comment, struct_fields

RX_FCT = {"COM": "Com_RxIndication", "CANTP": "CanTp_RxIndication", "SECOC": "SecOC_RxIndication"}
TX_FCT = {"COM": "Com_TxConfirmation", "CANTP": "CanTp_TxConfirmation"}
MODULE_HEADER = {"COM": "Com.h", "CANTP": "CanTp.h", "SECOC": "SecOC.h"}
DAVINCI_ROUTING = "/ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/"


def validate(cfg):
    """スキーマでは書けない、設定の整合性（重複）を検査する。"""
    errors = []
    for key in ("rxPaths", "txPaths"):
        names = [p["name"] for p in cfg[key]]
        srcs = [p["srcPduId"] for p in cfg[key]]
        if len(set(names)) != len(names):
            errors.append("%s: name が重複している" % key)
        if len(set(srcs)) != len(srcs):
            errors.append("%s: srcPduId が重複している" % key)
    for p in cfg["rxPaths"]:
        for d in p["dests"]:
            if ("ipdu" in d) != (d["module"] == "COM"):
                errors.append("rxPaths %s: ipdu は COM 宛て（module=COM）のときだけ指定する" % p["name"])
            if ("secocPdu" in d) != (d["module"] == "SECOC"):
                errors.append("rxPaths %s: secocPdu は SecOC 宛て（module=SECOC）のときだけ指定する" % p["name"])
    for p in cfg["txPaths"]:
        if ("confIpdu" in p) != (p["confModule"] == "COM"):
            errors.append("txPaths %s: confIpdu は confModule=COM のときだけ指定する" % p["name"])
    return errors


def com_index(ctx, direction, name, where):
    """Com の設定における I-PDU の添字（ID）。"""
    key = "rxIpdus" if direction == "RX" else "txIpdus"
    for i, p in enumerate(ctx.configs["Com"][key]):
        if p["name"] == name:
            return i
    raise ValueError("%s: Com の %s I-PDU '%s' が存在しない" % (where, direction, name))


def secoc_index(ctx, name, where):
    for i, p in enumerate(ctx.configs["SecOC"]["rxPdus"]):
        if p["name"] == name:
            return i
    raise ValueError("%s: SecOC の RX PDU '%s' が存在しない" % (where, name))


def dest_pdu_id(d, path, ctx):
    if "ipdu" in d:
        return com_index(ctx, "RX", d["ipdu"], "PduR " + path["name"])
    if "secocPdu" in d:
        return secoc_index(ctx, d["secocPdu"], "PduR " + path["name"])
    return d["destPduId"]


def conf_dest_pdu_id(p, ctx):
    if "confIpdu" in p:
        return com_index(ctx, "TX", p["confIpdu"], "PduR " + p["name"])
    return p["confDestPduId"]


def subst(text, path, ctx):
    """見出し中の {canId} を、frame が指す CAN ID で置き換える。"""
    if "{canId}" not in text:
        return text
    frame = path.get("frame")
    if frame is None or frame not in ctx.frames:
        raise ValueError("%s: {canId} を使うには、実在する frame の指定が必要" % path["name"])
    return text.replace("{canId}", ctx.frames[frame]["canId"])


def rx_dest_tables(cfg, ctx):
    out = [
        "/* -----------------------------------------------------------------------",
        " * RX 配信先テーブル（パスごと）",
        " * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/[PduRRoutingPath]/",
        " *           PduRDestPdu",
        " * ----------------------------------------------------------------------- */",
    ]
    for i, p in enumerate(cfg["rxPaths"]):
        out += block_comment(
            ["パス %d: %s" % (i, subst(p["tableSummary"], p, ctx)), "DaVinci: " + DAVINCI_ROUTING + p["name"]]
        )
        out.append("static const PduR_RxDestType PduR_RxDests_Path%d[PDUR_RX_DEST_COUNT_PATH%d] = {" % (i, i))
        for j, d in enumerate(p["dests"]):
            mod = d["module"]
            dest_id = dest_pdu_id(d, p, ctx)
            id_note = ["DaVinci: PduRDestPdu/PduRDestPduHandleId"]
            if "ipdu" in d:
                id_note.append("→ COM RX IPduId=%d (%s)" % (dest_id, d["ipdu"]))
            if "secocPdu" in d:
                title = next(s["title"] for s in ctx.configs["SecOC"]["rxPdus"] if s["name"] == d["secocPdu"])
                id_note.append("→ SecOC RX (SecOCRxPduId=%d: %s)" % (dest_id, title))
            if "destPduIdNote" in d:
                id_note.append(d["destPduIdNote"])
            out.append("    {")
            out += struct_fields(
                [
                    ("Module", "PDUR_MODULE_" + mod, ["DaVinci: PduRDestPdu/PduRDestModule = " + mod]),
                    ("DestPduId", "%dU" % dest_id, id_note),
                    ("RxIndFct", RX_FCT[mod], ["DaVinci: 自動解決（PduRDestModule=%s）" % mod]),
                ],
                8,
            )
            out.append("    }" + ("," if j < len(p["dests"]) - 1 else ""))
        out.append("};")
        out.append("")
    return out


def rx_paths(cfg, ctx):
    out = [
        "/* -----------------------------------------------------------------------",
        " * RX ルーティングパステーブル",
        " * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/[PduRRoutingPath]",
        " * CanIf からの RxPduId に対してどのパスを使うかを定義する。",
        " * ----------------------------------------------------------------------- */",
        "static const PduR_RxRoutingPathType PduR_RxPaths[PDUR_RX_PATH_COUNT] = {",
    ]
    n = len(cfg["rxPaths"])
    for i, p in enumerate(cfg["rxPaths"]):
        out.append("    {")
        out += block_comment(
            [
                "パス %d: %s" % (i, subst(p["summary"], p, ctx)),
                "DaVinci: PduRSrcPdu/PduRSrcPduHandleId = %d" % p["srcPduId"],
                "         (CanIf_PBCfg の RxPduId=%d と一致)" % p["srcPduId"],
            ],
            8,
        )
        out += struct_fields(
            [
                ("SrcPduId", "%dU" % p["srcPduId"], None),
                ("Dests", "PduR_RxDests_Path%d" % i, None),
                ("DestCount", "PDUR_RX_DEST_COUNT_PATH%d" % i, None),
            ],
            8,
        )
        out.append("    }" + ("," if i < n - 1 else ""))
    out += ["};", ""]
    return out


def tx_paths(cfg, ctx):
    out = [
        "/* -----------------------------------------------------------------------",
        " * TX ルーティングパステーブル",
        " * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/[PduRRoutingPath]",
        " * COM からの送信要求を CanIf へ転送するルートを定義する。",
        " * ----------------------------------------------------------------------- */",
        "static const PduR_TxRoutingPathType PduR_TxPaths[PDUR_TX_PATH_COUNT] = {",
    ]
    n = len(cfg["txPaths"])
    for i, p in enumerate(cfg["txPaths"]):
        out.append("    {")
        out += block_comment(
            ["パス %d: %s" % (i, subst(p["summary"], p, ctx)), "DaVinci: PduRRoutingPath/" + p["name"]]
            + p.get("note", []),
            8,
        )
        fields = [
            ("SrcPduId", "%dU" % p["srcPduId"], ["DaVinci: PduRSrcPdu/PduRSrcPduHandleId"]),
            ("CanIfTxPduId", "%dU" % p["canIfTxPduId"], ["DaVinci: PduRDestPdu/PduRDestPduHandleId"]),
            ("ConfDestPduId", "%dU" % conf_dest_pdu_id(p, ctx), None),
            ("ConfFct", TX_FCT[p["confModule"]], ["DaVinci: PduRTxConfirmation"]),
        ]
        ov = p.get("transmitOverride")
        if ov:
            fields += [
                ("TransmitOverrideFct", ov["fct"], ["本プロジェクト独自拡張（TX 経路上の中間モジュール）"]),
                ("TransmitOverrideId", "%dU" % ov["id"], None),
            ]
        out += struct_fields(fields, 8)
        out.append("    }" + ("," if i < n - 1 else ""))
    out += ["};", ""]
    return out


def config_instance(cfg, ctx):
    return [
        "/* -----------------------------------------------------------------------",
        " * PduR ポストビルド設定インスタンス",
        " * PduR_Init() の引数として渡す。",
        " * ----------------------------------------------------------------------- */",
        "const PduR_PBConfigType PduR_Config = {",
        "    .RxPaths     = PduR_RxPaths,",
        "    .RxPathCount = PDUR_RX_PATH_COUNT,",
        "    .TxPaths     = PduR_TxPaths,",
        "    .TxPathCount = PDUR_TX_PATH_COUNT",
        "};",
    ]


def render_pbcfg(cfg, ctx):
    return "\n".join(rx_dest_tables(cfg, ctx) + rx_paths(cfg, ctx) + tx_paths(cfg, ctx) + config_instance(cfg, ctx))


def render_cfg_h(cfg, ctx):
    out = doc_comment(cfg["rxPathCountDoc"])
    out.append("#define PDUR_RX_PATH_COUNT   %dU" % len(cfg["rxPaths"]))
    for i, p in enumerate(cfg["rxPaths"]):
        out.append("")
        out += doc_comment(["RX パス %d の配信先数（%s）" % (i, p["destCountNote"])])
        out.append("#define PDUR_RX_DEST_COUNT_PATH%d  %dU" % (i, len(p["dests"])))
    out.append("")
    out += doc_comment(cfg["txPathCountDoc"])
    out.append("#define PDUR_TX_PATH_COUNT   %dU" % len(cfg["txPaths"]))
    return "\n".join(out)


def check_includes(cfg, text):
    """使うモジュールのヘッダが C ソースにインクルードされているかを確認する。"""
    used = {d["module"] for p in cfg["rxPaths"] for d in p["dests"]}
    used |= {p["confModule"] for p in cfg["txPaths"]}
    return [
        "%s がインクルードされていない（モジュール %s を使用）" % (MODULE_HEADER[m], m)
        for m in sorted(used)
        if '#include "%s"' % MODULE_HEADER[m] not in text
    ]


SPEC = {
    "config": "config/data/PduR.json",
    "schema": "config/schema/PduR.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/PduR/PduR_PBCfg.c", "pdur-routing", render_pbcfg, check_includes),
        ("src/Bsw/PduR/PduR_Cfg.h", "pdur-counts", render_cfg_h, None),
    ],
}

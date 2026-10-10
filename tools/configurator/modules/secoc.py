"""SecOC（SecOC_PBCfg.c / SecOC_Cfg.h）の生成。

信号表（config/data/can_signals.json）から引く値（Secured I-PDU のレイアウト）:
  - FreshnessOffset / FreshnessLength: `secoc_freshness` フィールドの位置・長さ（byte 単位）
  - MacOffset / MacTxLength: `secoc_mac` フィールドの位置・長さ（byte 単位）
  - AuthenticPduLength: Freshness の手前までのバイト数
  - SecuredPduLength: フレームの DLC、DataId: 既定は CAN ID
SecOC の設定 json に書く値: Csm のジョブ、通知関数、Com の I-PDU との対応など。
TX の Secured I-PDU は未対応（現状 TX 方向で SecOC を使う PDU は無い）。
"""

from .common import doc_comment, ruled_comment, struct_fields

FIELDS = [
    ("SecOCRxPduId", "secOCRxPduId"), ("DataId", "dataId"), ("AuthenticPduLength", "authenticPduLength"),
    ("FreshnessOffset", "freshnessOffset"), ("FreshnessLength", "freshnessLength"), ("MacOffset", "macOffset"),
    ("MacTxLength", "macTxLength"), ("SecuredPduLength", "securedPduLength"), ("CsmJobId", "csmJobId"),
    ("ComRxPduId", "comRxPduId"), ("VerificationStatusCallout", "verificationStatusCallout"),
    ("VerificationStatusPropagationMode", "verificationStatusPropagationMode"),
]


def validate(cfg):
    errors = []
    names = [p["name"] for p in cfg["rxPdus"]]
    if len(set(names)) != len(names):
        errors.append("rxPdus: name が重複している")
    ips = [p["comIpdu"] for p in cfg["rxPdus"]]
    if len(set(ips)) != len(ips):
        errors.append("rxPdus: comIpdu が重複している")
    return errors


def layout(p, ctx):
    """Secured I-PDU のレイアウトを信号表から求める。"""
    fr = ctx.frames[p["frame"]]
    fresh = next((f for f in fr["fields"] if f["type"] == "secoc_freshness"), None)
    mac = next((f for f in fr["fields"] if f["type"] == "secoc_mac"), None)
    if fresh is None or mac is None:
        raise ValueError("SecOC %s: フレーム '%s' に secoc_freshness / secoc_mac のフィールドがない" % (p["name"], p["frame"]))
    for f in (fresh, mac):
        if f["bitPosition"] % 8 or f["bitSize"] % 8:
            raise ValueError("SecOC %s: フィールド '%s' がバイト境界にない" % (p["name"], f["name"]))
    return {
        "dataId": p["dataId"] if "dataId" in p else int(fr["canId"], 16),
        "authenticPduLength": min(fresh["bitPosition"], mac["bitPosition"]) // 8,
        "freshnessOffset": fresh["bitPosition"] // 8,
        "freshnessLength": fresh["bitSize"] // 8,
        "macOffset": mac["bitPosition"] // 8,
        "macTxLength": mac["bitSize"] // 8,
        "securedPduLength": fr["dlc"],
    }


def com_index(ctx, name, where):
    for i, ip in enumerate(ctx.configs["Com"]["rxIpdus"]):
        if ip["name"] == name:
            return i
    raise ValueError("%s: Com の RX I-PDU '%s' が存在しない" % (where, name))


def render_pbcfg(cfg, ctx):
    items = cfg["rxPdus"]
    out = [
        "static const SecOC_RxPduConfigType SecOC_RxPduConfigData[SECOC_RX_PDU_COUNT] = {",
    ]
    for i, p in enumerate(items):
        lay = layout(p, ctx)
        vals = {
            "secOCRxPduId": "%dU" % i,
            "dataId": "0x%04XU" % lay["dataId"],
            "authenticPduLength": "%dU" % lay["authenticPduLength"],
            "freshnessOffset": "%dU" % lay["freshnessOffset"],
            "freshnessLength": "%dU" % lay["freshnessLength"],
            "macOffset": "%dU" % lay["macOffset"],
            "macTxLength": "%dU" % lay["macTxLength"],
            "securedPduLength": "%dU" % lay["securedPduLength"],
            "csmJobId": p["csmJobId"],
            "comRxPduId": "%dU" % com_index(ctx, p["comIpdu"], "SecOC " + p["name"]),
        }
        if "verificationStatusCallout" in p:
            vals["verificationStatusCallout"] = p["verificationStatusCallout"]
        if "verificationStatusPropagationMode" in p:
            vals["verificationStatusPropagationMode"] = p["verificationStatusPropagationMode"]
        cm = p.get("comments", {})
        rows = [(c, vals[k], cm.get(k)) for c, k in FIELDS if k in vals]
        out.append("    {")
        out += ruled_comment(["RX Secured I-PDU %d: %s" % (i, p["title"])], 8)
        out += struct_fields(rows, 8)
        out.append("    }" + ("," if i < len(items) - 1 else ""))
    out += ["};", ""]
    out += [
        "/* TX 方向で SecOC を使う PDU は現在無い (SECOC_TX_PDU_COUNT=0、SecOC_Cfg.h 参照)。",
        " * サイズ0の配列宣言 (`T arr[0]`) は ISO C が認めていない GNU 拡張であり、",
        " * 空初期化子 `={}` はさらに C23 相当で、より厳格な C dialect や別コンパイラでの",
        " * ビルドに対して脆い。配列自体を宣言せず、ポインタを NULL・件数を 0 にする",
        " * （SecOC.c 側は TxPduCount を上限にループするため NULL を参照することはない）。",
        " * TX PDU が実際に追加された時点で、配列宣言とこの初期化子を書き戻すこと。 */",
        "const SecOC_ConfigType SecOC_Config = {",
        "    .RxPdus     = SecOC_RxPduConfigData,",
        "    .RxPduCount = SECOC_RX_PDU_COUNT,",
        "    .TxPdus     = NULL,",
        "    .TxPduCount = SECOC_TX_PDU_COUNT",
        "};",
    ]
    return "\n".join(out)


def render_cfg_counts(cfg, ctx):
    out = doc_comment(cfg["rxPduCountDoc"])
    out.append("#define SECOC_RX_PDU_COUNT  %dU" % len(cfg["rxPdus"]))
    out.append("")
    out += doc_comment(cfg["txPduCountDoc"])
    out.append("#define SECOC_TX_PDU_COUNT  0U")
    return "\n".join(out)


SPEC = {
    "config": "config/data/SecOC.json",
    "schema": "config/schema/SecOC.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/SecOC/SecOC_PBCfg.c", "secoc-config", render_pbcfg, None),
        ("src/Bsw/SecOC/SecOC_Cfg.h", "secoc-counts", render_cfg_counts, None),
    ],
}

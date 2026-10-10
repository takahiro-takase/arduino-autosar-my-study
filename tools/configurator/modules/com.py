"""Com（Com_PBCfg.c / Com_Cfg.h）の生成。

信号表（config/data/can_signals.json）から引く値:
  - I-PDU の DLC（`dlc` で上書きした場合を除く）、update-bit の位置（`(update-bit)` フィールド）
  - シグナルのビット位置・ビット長（`ipdu` が指すフレームの `field`）、エンディアン（信号表のバイトオーダー）
  - 周期・タイムアウトの定数（`constants` の `fromFrame`）
Com の設定 json に書く値: ID の割り当て、送信モード、フィルタ、タイムアウト時の動作、コールバック名など。
"""

from .common import block_comment, doc_comment, ruled_comment, struct_fields
from .secoc import layout as secoc_layout

DAVINCI_COM = "/ActiveEcuC/Com/ComConfig/"

# 構造体のフィールド順（C 名, json キー）
IPDU_FIELDS = [
    ("IPduId", "ipduId"), ("DLC", "dlc"), ("PduRId", "pduRId"), ("FirstTimeoutMs", "firstTimeoutMs"),
    ("TimeoutMs", "timeoutMs"), ("IsSignalGroup", "isSignalGroup"), ("TxModeMode", "txModeMode"),
    ("TxPeriodMs", "txPeriodMs"), ("TxModeModeTrue", "txModeModeTrue"), ("TxPeriodMsTrue", "txPeriodMsTrue"),
    ("MinDelayMs", "minDelayMs"), ("UpdateBitPosition", "updateBitPosition"), ("IpduGroupId", "ipduGroupId"),
    ("RxIndicationCbk", "rxIndicationCbk"), ("TxTransformCbk", "txTransformCbk"), ("TxAckCbk", "txAckCbk"),
    ("TxErrCbk", "txErrCbk"), ("RxAckCbk", "rxAckCbk"), ("NumberOfRepetitions", "numberOfRepetitions"),
    ("RepetitionPeriodMs", "repetitionPeriodMs"), ("TxFirstTimeoutMs", "txFirstTimeoutMs"),
    ("TxTimeoutMs", "txTimeoutMs"), ("TxTOutCbk", "txTOutCbk"), ("RxTOutCbk", "rxTOutCbk"),
    ("RxIpduCalloutCbk", "rxIpduCalloutCbk"), ("TxIpduCalloutCbk", "txIpduCalloutCbk"),
]
SIGNAL_FIELDS = [
    ("SignalId", "signalId"), ("Direction", "direction"), ("IPduId", "ipduId"), ("BitPosition", "bitPosition"),
    ("BitSize", "bitSize"), ("Endian", "endian"), ("InitValue", "initValue"), ("FilterAlgorithm", "filterAlgorithm"),
    ("Mask", "mask"), ("FilterX", "filterX"), ("FilterMin", "filterMin"), ("FilterMax", "filterMax"),
    ("FilterRejectCbk", "filterRejectCbk"), ("TmsContributor", "tmsContributor"),
    ("UpdateBitContributor", "updateBitContributor"), ("TransferProperty", "transferProperty"),
    ("RxDataTimeoutAction", "rxDataTimeoutAction"), ("TimeoutSubstitutionValue", "timeoutSubstitutionValue"),
    ("DataInvalidAction", "dataInvalidAction"), ("InvalidValue", "invalidValue"),
    ("InvalidNotificationCbk", "invalidNotificationCbk"), ("FirstTimeoutMs", "firstTimeoutMs"),
    ("TimeoutMs", "timeoutMs"), ("RxTOutCbk", "rxTOutCbk"), ("TxAckCbk", "txAckCbk"), ("TxErrCbk", "txErrCbk"),
    ("RxAckCbk", "rxAckCbk"), ("TxTOutCbk", "txTOutCbk"), ("InvalidValueConfigured", "invalidValueConfigured"),
]
HEX_KEYS = {"mask", "filterX", "invalidValue", "timeoutSubstitutionValue", "updateBitPosition"}
UPDATE_BIT_NONE = 255

# json で省略したときの既定値。ここに無い項目は、C の初期化子と同じく 0（コールバックは NULL）になる。
# C の 0 では困る項目（0 が別の意味を持つもの）だけを、ここで明示して生成する。
#   - ipduGroupId: 0 は COM_IPDU_GROUP_TELEMETRY。どのグループにも属さない値は COM_IPDU_GROUP_NONE（0xFF）
#   - updateBitPosition: 0 は bit0。update-bit なしは 255（信号表に (update-bit) フィールドがあればその位置）
#   - txModeMode（TX の I-PDU のみ）: 0 は COM_TX_MODE_MIXED。既定は COM_TX_MODE_DIRECT
DEFAULT_IPDU_GROUP = "COM_IPDU_GROUP_NONE"
DEFAULT_TX_MODE = "COM_TX_MODE_DIRECT"
# 値が 0 の列挙値の名前（省略したときの値）。注釈だけ書いたフィールドを出力するときに使う。
ENUM_ZERO = {
    "filterAlgorithm": "COM_FILTER_ALWAYS",
    "transferProperty": "COM_TRANSFER_PROPERTY_PENDING",
    "rxDataTimeoutAction": "COM_RX_TIMEOUT_ACTION_NONE",
    "dataInvalidAction": "COM_DATA_INVALID_ACTION_NONE",
    "txModeModeTrue": "COM_TX_MODE_MIXED",
}

# 注釈が無いときの既定の注釈（信号の標準フィールド）
SIGNAL_DEFAULT_NOTE = {
    "signalId": "DaVinci: ComHandleId",
    "direction": "本プロジェクト独自拡張。Com_SignalDirectionType 参照",
    "bitPosition": "DaVinci: ComBitPosition",
    "bitSize": "DaVinci: ComBitSize",
    "endian": "DaVinci: ComSignalEndianness = OPAQUE",
}
# 定数マクロを参照しうる、時間を表すフィールド
TIME_KEYS = ["firstTimeoutMs", "timeoutMs", "txPeriodMs", "txPeriodMsTrue", "minDelayMs",
             "txFirstTimeoutMs", "txTimeoutMs", "repetitionPeriodMs"]


# ----------------------------------------------------------------------
def validate(cfg):
    errors = []
    for key in ("rxIpdus", "txIpdus", "signals", "gateway"):
        names = [p["name"] for p in cfg[key]]
        if len(set(names)) != len(names):
            errors.append("%s: name が重複している" % key)
    macros = [s["macro"] for s in cfg["signals"]]
    if len(set(macros)) != len(macros):
        errors.append("signals: macro が重複している")
    cm = [c["macro"] for c in cfg["constants"]]
    if len(set(cm)) != len(cm):
        errors.append("constants: macro が重複している")
    ipdus = {("RX", p["name"]) for p in cfg["rxIpdus"]} | {("TX", p["name"]) for p in cfg["txIpdus"]}
    for s in cfg["signals"]:
        if (s["direction"], s["ipdu"]) not in ipdus:
            errors.append("signals %s: ipdu '%s' が %s 側の I-PDU に存在しない" % (s["name"], s["ipdu"], s["direction"]))
    names = {s["name"] for s in cfg["signals"]}
    for g in cfg["gateway"]:
        for k in ("src", "dest"):
            if g[k] not in names:
                errors.append("gateway %s: %s '%s' が signals に存在しない" % (g["name"], k, g[k]))
    for c in cfg["constants"]:
        if ("value" in c) == ("fromFrame" in c):
            errors.append("constants %s: value と fromFrame のどちらか一方だけを指定する" % c["macro"])
    # 時間を表すフィールドが文字列なら、constants に存在するマクロであること
    groups = {g["macro"] for g in cfg.get("ipduGroups", [])}
    if len(groups) != len(cfg.get("ipduGroups", [])):
        errors.append("ipduGroups: macro が重複している")
    for p in cfg["rxIpdus"] + cfg["txIpdus"]:
        g = p.get("ipduGroupId")
        if g is not None and g != DEFAULT_IPDU_GROUP and g not in groups:
            errors.append("ipdus %s: ipduGroupId '%s' が ipduGroups に存在しない" % (p["name"], g))
    known = set(cm)
    for p in cfg["rxIpdus"] + cfg["txIpdus"] + cfg["signals"]:
        for k in TIME_KEYS:
            v = p.get(k)
            if isinstance(v, str) and v not in known:
                errors.append("%s %s: %s の '%s' が constants に存在しない" % ("signals" if "macro" in p else "ipdus", p["name"], k, v))
    return errors


# ----------------------------------------------------------------------
def fmt(key, v):
    if isinstance(v, int):
        if key in HEX_KEYS and v > 9:
            return "0x%XU" % v
        return "%dU" % v
    return v


def frame_of(ipdu, ctx):
    return ctx.frames[ipdu["frame"]]


def frame_field(frame, name):
    for f in frame["fields"]:
        if f["name"] == name:
            return f
    return None


def derive_pdur_id(p, direction, idx, ctx):
    """ComIPduPduRef 相当の値を、PduR の設定から引く。"""
    pdur = ctx.configs["PduR"]
    if direction == "TX":
        for path in pdur["txPaths"]:
            if path.get("confIpdu") == p["name"]:
                return path["srcPduId"]
    else:
        for path in pdur["rxPaths"]:
            if any(d.get("ipdu") == p["name"] for d in path["dests"]):
                return idx
        if secoc_pdu_of(p, ctx) is not None:
            return idx  # SecOC が検証後に Com_RxIndication() へ渡す ID（= Com の RX I-PDU の ID）
    raise ValueError("Com %s: pduRId が未指定で、PduR の設定にも SecOC の設定にもこの I-PDU を指す経路がない" % p["name"])


def secoc_pdu_of(p, ctx):
    """この Com の RX I-PDU へ検証済みペイロードを渡す SecOC の RX PDU（無ければ None）。"""
    sec = ctx.configs.get("SecOC")
    if not sec:
        return None
    return next((s for s in sec["rxPdus"] if s["comIpdu"] == p["name"]), None)


def ipdu_values(p, ctx, idx, direction):
    """I-PDU の (json キー → 値) を、信号表と PduR の設定から引く値を補って返す。"""
    fr = frame_of(p, ctx)
    vals = {"ipduId": idx}
    vals["pduRId"] = p["pduRId"] if "pduRId" in p else derive_pdur_id(p, direction, idx, ctx)
    sec = secoc_pdu_of(p, ctx) if direction == "RX" else None
    if "dlc" in p:
        vals["dlc"] = p["dlc"]
    elif sec is not None:
        vals["dlc"] = secoc_layout(sec, ctx)["authenticPduLength"]  # Com が見るのは認証対象のペイロードだけ
    else:
        vals["dlc"] = fr["dlc"]
    for k in [k for _, k in IPDU_FIELDS if k not in ("ipduId", "dlc", "pduRId")]:
        if k in p:
            vals[k] = p[k]
    if "updateBitPosition" not in p:
        ub = frame_field(fr, "(update-bit)")
        vals["updateBitPosition"] = ub["bitPosition"] if ub else UPDATE_BIT_NONE
    vals.setdefault("ipduGroupId", DEFAULT_IPDU_GROUP)
    if direction == "TX":
        vals.setdefault("txModeMode", DEFAULT_TX_MODE)
    return vals


def signal_values(s, cfg, ctx):
    ipdus = cfg["rxIpdus"] if s["direction"] == "RX" else cfg["txIpdus"]
    pos = next(i for i, p in enumerate(ipdus) if p["name"] == s["ipdu"])
    fr = frame_of(ipdus[pos], ctx)
    f = frame_field(fr, s["field"])
    vals = {
        "signalId": s["macro"],
        "direction": "COM_SIGNAL_DIRECTION_" + s["direction"],
        "ipduId": pos,
        "bitPosition": s["bitPosition"] if "bitPosition" in s else f["bitPosition"],
        "bitSize": s["bitSize"] if "bitSize" in s else f["bitSize"],
        "endian": "COM_BIG_ENDIAN" if ctx.byte_order.startswith("big-endian") else "COM_LITTLE_ENDIAN",
    }
    for _, k in SIGNAL_FIELDS:
        if k in s and k not in vals:
            vals[k] = s[k]
    return vals


def fields_for(table, vals, comments, ipdu_note=None):
    """構造体のフィールド行を作る。値を省略していても、注釈があるフィールドは既定値で出力する。"""
    rows = []
    for cname, key in table:
        if key not in vals and key in comments:
            vals = dict(vals)
            vals[key] = ENUM_ZERO.get(key, 0)
        if key not in vals:
            continue
        note = comments.get(key)
        if note is None and ipdu_note:
            note = ipdu_note.get(key)
        rows.append((cname, fmt(key, vals[key]), note))
    return rows


# ----------------------------------------------------------------------
def ipdu_table(cfg, ctx, key, direction):
    items = cfg[key]
    cname = "Com_%sIPduConfigData" % direction.title()
    macro = "COM_%s_IPDU_COUNT" % direction
    out = [
        "/* -----------------------------------------------------------------------",
        " * %s I-PDU テーブル" % direction,
        " * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu] (Direction=%s)" % ("RECEIVE" if direction == "RX" else "SEND"),
    ]
    if direction == "RX":
        out += [" * Com_RxIndication() が受信 PDU をバッファに格納する際に参照する。"]
    else:
        out += [
            " * Com_MainFunctionTx()（DIRECT/MIXED の変化時送信・PERIODIC/MIXED の周期送信、",
            " * いずれもこの関数のみが実送信を行う）が送信要求を PduR へ転送する際に参照する。",
        ]
    out += [
        " * ----------------------------------------------------------------------- */",
        "static const Com_IPduConfigType %s[%s] = {" % (cname, macro),
    ]
    for i, p in enumerate(items):
        vals = ipdu_values(p, ctx, i, direction)
        out.append("    {")
        out += ruled_comment(
            ["%s IPduId=%d: %s" % (direction, i, p["title"]), "DaVinci: " + DAVINCI_COM + p["name"]] + p.get("note", []), 8
        )
        out += struct_fields(fields_for(IPDU_FIELDS, vals, p.get("comments", {})), 8)
        out.append("    }" + ("," if i < len(items) - 1 else ""))
    out += ["};", ""]
    return out


def signal_table(cfg, ctx):
    out = [
        "/* -----------------------------------------------------------------------",
        " * シグナルテーブル（RX + TX 共通）",
        " * DaVinci: /ActiveEcuC/Com/ComConfig/[ComSignal]",
        " * Com_ReceiveSignal() / Com_SendSignal() がビットパック・アンパックに使用する。",
        " * シグナル ID は Com_Cfg.h の COM_SIGNAL_* 定数と対応している。",
        " *",
        " * ComBitPosition の数え方（BigEndian/OPAQUE）:",
        " *   byte[0] の MSB = ビット位置 0、byte[0] の LSB = ビット位置 7",
        " *   byte[1] の MSB = ビット位置 8 ...",
        " * ----------------------------------------------------------------------- */",
        "static const Com_SignalConfigType Com_SignalConfigData[COM_SIGNAL_COUNT] = {",
    ]
    n = len(cfg["signals"])
    for i, s in enumerate(cfg["signals"]):
        vals = signal_values(s, cfg, ctx)
        comments = dict(s.get("comments", {}))
        for k, text in SIGNAL_DEFAULT_NOTE.items():
            comments.setdefault(k, [text])
        comments.setdefault("ipduId", ["DaVinci: ComIPduRef → " + s["ipdu"]])
        head = ["Signal %d: %s" % (i, s["title"])] + s.get("headNote", [])
        head += ["DaVinci: " + DAVINCI_COM + s["name"]] + s.get("note", [])
        out.append("    {")
        out += ruled_comment(head, 8)
        out += struct_fields(fields_for(SIGNAL_FIELDS, vals, comments), 8)
        out.append("    }" + ("," if i < n - 1 else ""))
    out += ["};", ""]
    return out


def gateway_table(cfg, ctx):
    by_name = {s["name"]: s["macro"] for s in cfg["signals"]}
    out = [
        "/* -----------------------------------------------------------------------",
        " * Signal Gateway ルーティングテーブル",
        " * DaVinci: /ActiveEcuC/Com/ComConfig/[ComGwMapping]",
        " * Com_GatewayRoute() が RX I-PDU 受信の都度、この表を走査して転送する。",
        " * 詳細は Com_Types.h の Com_GwMappingType コメント参照。",
        " * ----------------------------------------------------------------------- */",
        "static const Com_GwMappingType Com_GwMappingData[COM_GW_MAPPING_COUNT] = {",
    ]
    n = len(cfg["gateway"])
    for i, g in enumerate(cfg["gateway"]):
        out.append("    {")
        out += block_comment(g.get("note", []) + ["DaVinci: " + DAVINCI_COM + g["name"]], 8)
        out += struct_fields([("SrcSignalId", by_name[g["src"]], None), ("DestSignalId", by_name[g["dest"]], None)], 8)
        out.append("    }" + ("," if i < n - 1 else ""))
    out += ["};", ""]
    return out


def config_instance():
    return [
        "/* -----------------------------------------------------------------------",
        " * COM ポストビルド設定インスタンス",
        " * Com_Init() の引数として渡す。",
        " * ----------------------------------------------------------------------- */",
        "const Com_ConfigType Com_Config = {",
        "    .RxIPdus     = Com_RxIPduConfigData,",
        "    .RxIPduCount = COM_RX_IPDU_COUNT,",
        "    .TxIPdus     = Com_TxIPduConfigData,",
        "    .TxIPduCount = COM_TX_IPDU_COUNT,",
        "    .Signals     = Com_SignalConfigData,",
        "    .SignalCount  = COM_SIGNAL_COUNT,",
        "    .GwMappings     = Com_GwMappingData,",
        "    .GwMappingCount = COM_GW_MAPPING_COUNT",
        "};",
    ]


def render_pbcfg(cfg, ctx):
    out = ipdu_table(cfg, ctx, "rxIpdus", "RX") + ipdu_table(cfg, ctx, "txIpdus", "TX")
    out += signal_table(cfg, ctx) + gateway_table(cfg, ctx) + config_instance()
    return "\n".join(out)


# ----------------------------------------------------------------------
def constant_value(c, ctx):
    if "value" in c:
        return c["value"]
    f = c["fromFrame"]
    return ctx.frames[f["frame"]][f["key"]]


def render_cfg_counts(cfg, ctx):
    """個数マクロと、周期・タイムアウトなどの定数。"""
    out = []
    out += doc_comment(cfg["rxIpduCountDoc"])
    out.append("#define COM_RX_IPDU_COUNT   %dU" % len(cfg["rxIpdus"]))
    out.append("")
    out += doc_comment(cfg["txIpduCountDoc"])
    out.append("#define COM_TX_IPDU_COUNT   %dU" % len(cfg["txIpdus"]))
    out.append("")
    out += doc_comment(cfg["signalCountDoc"])
    out.append("#define COM_SIGNAL_COUNT    %dU" % len(cfg["signals"]))
    out.append("")
    out += doc_comment(cfg["gwMappingCountDoc"])
    out.append("#define COM_GW_MAPPING_COUNT  %dU" % len(cfg["gateway"]))
    for c in cfg["constants"]:
        out.append("")
        out += doc_comment(c["doc"])
        out.append("#define %s  %dU" % (c["macro"], constant_value(c, ctx)))
    return "\n".join(out)


def render_cfg_signal_ids(cfg, ctx):
    out = []
    for i, s in enumerate(cfg["signals"]):
        if i:
            out.append("")
        out += doc_comment(s["idDoc"])
        out.append("#define %s  %dU" % (s["macro"], i))
    return "\n".join(out)


def render_cfg_ipdu_groups(cfg, ctx):
    out = []
    for i, g in enumerate(cfg.get("ipduGroups", [])):
        if i:
            out.append("")
        out += doc_comment(g["doc"])
        out.append("#define %s  %dU" % (g["macro"], i))
    return chr(10).join(out)


def check_includes(cfg, text):
    return []


SPEC = {
    "config": "config/data/Com.json",
    "schema": "config/schema/Com.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Com/Com_PBCfg.c", "com-config", render_pbcfg, check_includes),
        ("src/Bsw/Com/Com_Cfg.h", "com-ipdu-groups", render_cfg_ipdu_groups, None),
        ("src/Bsw/Com/Com_Cfg.h", "com-counts", render_cfg_counts, None),
        ("src/Bsw/Com/Com_Cfg.h", "com-signal-ids", render_cfg_signal_ids, None),
    ],
}

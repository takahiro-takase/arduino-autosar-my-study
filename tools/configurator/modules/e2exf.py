"""E2EXf（E2EXf_PBCfg.c / E2EXf_PBCfg.h）の生成。

信号表（config/data/can_signals.json）から引く値:
  - DataLength: フレームの DLC
  - Offset: `e2e_crc` フィールドのビット位置 / 8（E2E ヘッダ = CRC + Counter の先頭）
  - DataID: 既定は CAN ID（`dataId` で上書き）
E2EXf の設定 json に書く値: インスタンスの識別（stem）、MaxDeltaCounter、Dem イベント、ステートマシンのしきい値など。
"""

from .common import block_comment

DAVINCI_E2EXF = "/ActiveEcuC/E2EXf/"
SUPPRESS = "/* cppcheck-suppress misra-c2012-8.9 */"
RULE = "-" * 71


def validate(cfg):
    errors = []
    names = [p["name"] for p in cfg["instances"]]
    stems = [p["stem"] for p in cfg["instances"]]
    if len(set(names)) != len(names):
        errors.append("instances: name が重複している")
    if len(set(stems)) != len(stems):
        errors.append("instances: stem が重複している")
    for p in cfg["instances"]:
        if p["direction"] == "RX" and "demEvent" not in p:
            errors.append("instances %s: RX には demEvent が必要" % p["name"])
        if p["direction"] == "TX" and ("demEvent" in p or "maxDeltaCounter" in p):
            errors.append("instances %s: TX には demEvent / maxDeltaCounter を指定しない" % p["name"])
    return errors


def instance_values(p, ctx):
    """(DataID, DataLength, MaxDeltaCounter, Offset) を信号表から補って返す。"""
    fr = ctx.frames[p["frame"]]
    crc = next(f for f in fr["fields"] if f["type"] == "e2e_crc")
    data_id = p["dataId"] if "dataId" in p else int(fr["canId"], 16)
    return data_id, fr["dlc"], p.get("maxDeltaCounter", 0), crc["bitPosition"] // 8


def com_index(ctx, direction, frame):
    key = "rxIpdus" if direction == "RX" else "txIpdus"
    for i, ip in enumerate(ctx.configs["Com"][key]):
        if ip["frame"] == frame:
            return i
    raise ValueError("E2EXf: フレーム '%s' に対応する Com の %s I-PDU がない" % (frame, direction))


def header_comment(p, ctx):
    fr = ctx.frames[p["frame"]]
    idx = com_index(ctx, p["direction"], p["frame"])
    lines = [
        "%s (%s IPduId=%d, CAN %s)" % (p["frame"], p["direction"], idx, fr["canId"]),
        "DaVinci: " + DAVINCI_E2EXF + p["name"],
    ]
    out = ["/* " + RULE]
    out += [" * " + ln for ln in lines]
    if p.get("note"):
        out.append(" *")
        out += [" * " + ln for ln in p["note"]]
    out.append(" * " + RULE + " */")
    return out


def cfg_struct(p, ctx):
    data_id, length, max_delta, offset = instance_values(p, ctx)
    rx = p["direction"] == "RX"
    delta_note = "許容カウンタ飛び幅" + (" (1=連続受信を前提)" if max_delta == 1 else "") if rx else "Protect 側では未使用"
    offset_note = "E2E ヘッダ(CRC16+Counter)は PDU 先頭" if offset == 0 else "E2E ヘッダ(CRC16+Counter)は PDU の byte[%d] から" % offset
    rows = [
        ("0x%04XU," % data_id, "DataID          : PDU 識別子 (CAN ID と一致させるのが一般的)"),
        ("%dU," % length, "DataLength      : " + p["lengthNote"]),
        ("%dU," % max_delta, "MaxDeltaCounter : " + delta_note),
        ("%dU" % offset, "Offset          : " + offset_note),
    ]
    out = ["static const E2E_P05ConfigType E2EXf_%sCfgP05 = {" % p["stem"]]
    out += ["    %s/* %s */" % (v.ljust(10), c) for v, c in rows]
    out.append("};")
    return out


def sm_config(cfg):
    sm = cfg["smConfig"]
    return [
        SUPPRESS,
        "static const E2E_SMConfigType E2EXf_SMConfigDefault = {",
        "    E2EXF_SM_WINDOW_SIZE, /* WindowSize */",
        "    %dU, /* MinOkStateInit       */" % sm["minOkStateInit"],
        "    %dU, /* MaxErrorStateInit    */" % sm["maxErrorStateInit"],
        "    %dU, /* MinOkStateValid      */" % sm["minOkStateValid"],
        "    %dU, /* MaxErrorStateValid   */" % sm["maxErrorStateValid"],
        "    %dU, /* MinOkStateInvalid    */" % sm["minOkStateInvalid"],
        "    %dU  /* MaxErrorStateInvalid */" % sm["maxErrorStateInvalid"],
        "};",
    ]


def instance_block(p, ctx):
    out = header_comment(p, ctx) + cfg_struct(p, ctx)
    s = p["stem"]
    if p["direction"] == "RX":
        out += [SUPPRESS, "static E2E_P05CheckStateType E2EXf_%sStateP05;" % s]
        out += block_comment(p["waitNote"]) if p.get("waitNote") else []
        out += [SUPPRESS, "static uint8 E2EXf_%sWaitForFirstDataP05;" % s]
        out += block_comment(p["smNote"]) if p.get("smNote") else []
        out += [
            "static uint8 E2EXf_%sSMWindow[E2EXF_SM_WINDOW_SIZE];" % s,
            SUPPRESS,
            "static E2E_SMCheckStateType E2EXf_%sSMState = { E2EXf_%sSMWindow, 0U, 0U, 0U, E2E_SM_DEINIT };" % (s, s),
            "",
            "const E2EXf_RxConfigTypeP05 E2EXf_%sRxCfg = {" % s,
            "    .E2EConfig        = &E2EXf_%sCfgP05," % s,
            "    .CheckState       = &E2EXf_%sStateP05," % s,
            "    .DemEventId       = %s," % p["demEvent"],
            "    .WaitForFirstData = &E2EXf_%sWaitForFirstDataP05," % s,
            "    .SMConfig         = &E2EXf_SMConfigDefault,",
            "    .SMState          = &E2EXf_%sSMState" % s,
            "};",
        ]
    else:
        out += [
            SUPPRESS,
            "static E2E_P05ProtectStateType E2EXf_%sStateP05;" % s,
            "",
            "const E2EXf_TxConfigTypeP05 E2EXf_%sTxCfgP05 = {" % s,
            "    .E2EConfig    = &E2EXf_%sCfgP05," % s,
            "    .ProtectState = &E2EXf_%sStateP05" % s,
            "};",
        ]
    return out


def render_sm_define(cfg, ctx):
    out = ["/* " + RULE] + [" * " + ln for ln in cfg["smConfig"]["doc"]] + [" * " + RULE + " */"]
    out.append("#define E2EXF_SM_WINDOW_SIZE %dU" % cfg["smConfig"]["windowSize"])
    return "\n".join(out)


def render_instances(cfg, ctx):
    out = sm_config(cfg)
    for p in cfg["instances"]:
        out.append("")
        out += instance_block(p, ctx)
    return "\n".join(out)


def render_init(cfg, ctx):
    rx = [p["stem"] for p in cfg["instances"] if p["direction"] == "RX"]
    tx = [p["stem"] for p in cfg["instances"] if p["direction"] == "TX"]
    out = ["    (void)E2E_P05CheckInit(&E2EXf_%sStateP05);" % s for s in rx]
    names = ["E2EXf_%sWaitForFirstDataP05" % s for s in rx]
    width = max(len(n) for n in names) if names else 0
    out += ["    %s = 1U;" % n.ljust(width) for n in names]
    out += [
        "    /* [SWS_E2E_00353]: E2E_SMCheckInit() を明示的に呼ぶ（呼ばないまま",
        "     * ゼロ初期化のみに頼ると E2E_SM_VALID(0x00) と誤認する、E2E.h の",
        "     * E2E_SMCheck() 宣言側コメント参照）。 */",
    ]
    out += ["    (void)E2E_SMCheckInit(&E2EXf_%sSMState, &E2EXf_SMConfigDefault);" % s for s in rx]
    out += ["    (void)E2E_P05ProtectInit(&E2EXf_%sStateP05);" % s for s in tx]
    return "\n".join(out)


def render_externs(cfg, ctx):
    out = []
    for p in cfg["instances"]:
        if p["direction"] == "RX":
            out.append("extern const E2EXf_RxConfigTypeP05 E2EXf_%sRxCfg;" % p["stem"])
        else:
            out.append("extern const E2EXf_TxConfigTypeP05 E2EXf_%sTxCfgP05;" % p["stem"])
    return "\n".join(out)


SPEC = {
    "config": "config/data/E2EXf.json",
    "schema": "config/schema/E2EXf.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/E2EXf/E2EXf_PBCfg.c", "e2exf-sm-define", render_sm_define, None),
        ("src/Bsw/E2EXf/E2EXf_PBCfg.c", "e2exf-instances", render_instances, None),
        ("src/Bsw/E2EXf/E2EXf_PBCfg.c", "e2exf-init", render_init, None),
        ("src/Bsw/E2EXf/E2EXf_PBCfg.h", "e2exf-externs", render_externs, None),
    ],
}

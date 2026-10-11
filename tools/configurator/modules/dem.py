"""Dem（Dem_Cfg.h / Dem.c）のイベント定義の生成。

Dem.json の events[] に書く値: イベント名、DTC コード、デバウンスの確定閾値、経年回復（Aging）の閾値、各項目の説明。
生成する値（並び順がそのままイベント ID）:
  - Dem_Cfg.h: イベント ID（DemConf_DemEventParameter_<name>。AUTOSAR のシンボリック名の規則）とイベント数（DEM_EVENT_COUNT）、DTC コード（DEM_DTC_<name>）、
    デバウンスの閾値（DEM_DEBOUNCE_LIMIT_<name>）、Aging の閾値（DEM_AGING_THRESHOLD_<name>）
  - Dem_PBCfg.c: イベント ID から DTC・デバウンス閾値・Aging 閾値を引く 3 つの表（宣言は Dem_PBCfg.h）
NvM（ブロック長・初期値の注釈）と FiM（監視するイベント）は、この Dem.json を参照して整合性を検査する。
"""

import re

IDENT_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
EVENT_SYMBOL = "DemConf_DemEventParameter_"  # [TPS_ECUC_02108]: <モジュール略称>Conf_<コンテナ定義>_<コンテナ名>


def validate(cfg):
    errors = []
    names = [e["name"] for e in cfg["events"]]
    if len(set(names)) != len(names):
        errors.append("events: name が重複している")
    dtcs = [int(e["dtc"], 16) for e in cfg["events"]]
    if len(set(dtcs)) != len(dtcs):
        errors.append("events: dtc が重複している")
    for e in cfg["events"]:
        if not IDENT_RE.match(e["name"]):
            errors.append("events %s: name は大文字の英数字と _ だけ（先頭は英字）" % e["name"])
        if not 1 <= int(e["dtc"], 16) <= 0xFFFFFF:
            errors.append("events %s: dtc は 24 bit の範囲（0x000001〜0xFFFFFF）" % e["name"])
        if e["dtc"].upper() == "0XFFFFFF":
            errors.append("events %s: dtc 0xFFFFFF は「全 DTC」を表すグループコード（DEM_GROUP_ALL_DTCS）と同じため使えない" % e["name"])
    return errors


def event_symbol(ctx, name, where):
    """他のモジュールが Dem のイベントを指すときの C 表記（シンボリック名）。名前が Dem.json に無ければエラーにする。"""
    if name not in {e["name"] for e in ctx.configs["Dem"]["events"]}:
        raise ValueError("%s: Dem イベント '%s' が Dem.json の events に無い" % (where, name))
    return EVENT_SYMBOL + name


# ----------------------------------------------------------------------
def define_block(entries):
    """#define の行を作る。entries は [(名前, 値の文字列, 説明の行のリスト)]。名前と値の幅をそろえ、説明を行末に付ける。"""
    name_w = max(len(n) for n, _, _ in entries)
    val_w = max(len(v) for _, v, _ in entries)
    out = []
    for name, value, doc in entries:
        head = "#define %s  %s  " % (name.ljust(name_w), value.ljust(val_w))
        pad = " " * (len(head) + 1)
        for k, line in enumerate(doc):
            last = k == len(doc) - 1
            if k == 0:
                out.append(head + "/**< " + line + (" */" if last else ""))
            else:
                out.append(pad + "*   " + line + (" */" if last else ""))
    return out


def render_events(cfg, ctx):
    events = cfg["events"]
    entries = [(EVENT_SYMBOL + e["name"], "%dU" % i, e["idDoc"]) for i, e in enumerate(events)]
    entries.append(("DEM_EVENT_COUNT", "%dU" % len(events), ["イベント総数"]))
    return "\n".join(define_block(entries))


def render_dtcs(cfg, ctx):
    return "\n".join(define_block([("DEM_DTC_" + e["name"], "0x%06XUL" % int(e["dtc"], 16), e["dtcDoc"]) for e in cfg["events"]]))


def render_debounce(cfg, ctx):
    return "\n".join(define_block([("DEM_DEBOUNCE_LIMIT_" + e["name"], "%d" % e["debounceLimit"], e["debounceDoc"]) for e in cfg["events"]]))


def render_aging(cfg, ctx):
    return "\n".join(define_block([("DEM_AGING_THRESHOLD_" + e["name"], "%dU" % e["agingThreshold"], e["agingDoc"]) for e in cfg["events"]]))


# ----------------------------------------------------------------------
def table(cfg, doc, decl, prefix):
    events = cfg["events"]
    names = [prefix + e["name"] for e in events]
    width = max(len(n) for n in names) + 1
    out = [doc, decl + " = {"]
    for i, n in enumerate(names):
        comma = "," if i < len(names) - 1 else " "
        out.append("    %s /* event %d */" % ((n + comma).ljust(width), i))
    out.append("};")
    return out


def render_tables(cfg, ctx):
    """イベント ID から DTC・デバウンス閾値・Aging 閾値を引く 3 つの表（宣言は Dem_PBCfg.h）。"""
    out = table(cfg, "/** イベント ID → DTC コード (24-bit) 変換テーブル */",
                "const uint32 Dem_DtcTable[DEM_EVENT_COUNT]", "DEM_DTC_")
    out += [""] + table(cfg, "/** イベント ID → デバウンス確定閾値 変換テーブル (Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*) */",
                        "const sint8 Dem_DebounceLimitTable[DEM_EVENT_COUNT]", "DEM_DEBOUNCE_LIMIT_")
    out += [""] + table(cfg, "/** イベント ID → 経年回復(Aging)閾値 変換テーブル (Dem_Cfg.h の DEM_AGING_THRESHOLD_*) */",
                        "const uint8 Dem_AgingThresholdTable[DEM_EVENT_COUNT]", "DEM_AGING_THRESHOLD_")
    return "\n".join(out)


SPEC = {
    "config": "config/data/Dem.json",
    "schema": "config/schema/Dem.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Dem/Dem_Cfg.h", "dem-events", render_events, None),
        ("src/Bsw/Dem/Dem_Cfg.h", "dem-dtcs", render_dtcs, None),
        ("src/Bsw/Dem/Dem_Cfg.h", "dem-debounce-limits", render_debounce, None),
        ("src/Bsw/Dem/Dem_Cfg.h", "dem-aging-thresholds", render_aging, None),
        ("src/Bsw/Dem/Dem_PBCfg.c", "dem-tables", render_tables, None),
    ],
}

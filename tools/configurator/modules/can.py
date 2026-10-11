"""Can（Can_Cfg.h / Can_PBCfg.c）の生成。

Can.json に書く値: コントローラの種類、SPI のチップセレクトと割り込みのピン番号、ボーレート、水晶の周波数、
CAN FD の設定（有効かどうか、データ部のビットレート、ビットレート切替）、受信フィルタ、各項目の説明。
生成する値: ピン番号のマクロ（Can_Cfg.h）と、Can_Config（Can_PBCfg.c）。
検査:
  - ボーレート・水晶の指定が、Can_Cfg.h の CAN_BAUDRATE_* / CAN_CRYSTAL_*MHZ に実在すること。
  - コントローラの種類に対して、水晶の周波数と FD の指定が成り立つこと（CONTROLLERS 参照）。
  - FD を有効にするときは、データ部のビットレートが通常のビットレート（アービトレーション）の整数倍であること。
Can_Cfg.h の他の定数（エラーコード、ApiId、ボーレートの数値表など）は手書きのまま。
"""

import pathlib
import re

from .common import block_comment, doc_comment

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]

# コントローラの種類ごとの制約。MCP2515 は CAN 2.0 まで。MCP2518FD は CAN FD に対応する。
# 水晶の周波数は、使うライブラリ（MCP2515: mcp_can、MCP2518FD: ACAN2517FD）が扱える値。
CONTROLLERS = {
    "MCP2515": {"fd": False, "crystals": (8, 16, 20)},
    "MCP2518FD": {"fd": True, "crystals": (20, 40)},
}


def nominal_bps(name):
    """'500KBPS' → 500000、'31K25BPS' → 31250（Can_Cfg.h の CAN_BAUDRATE_<name> の名前から）。"""
    body = name[:-3]
    whole, _, frac = body.partition("K")
    return int(whole) * 1000 + (int(frac.ljust(3, "0")) if frac else 0)


def validate(cfg):
    errors = []
    if cfg["csPin"] == cfg["intPin"]:
        errors.append("csPin と intPin が同じピン番号")
    ctl = CONTROLLERS[cfg["controller"]]
    if cfg["crystalMhz"] not in ctl["crystals"]:
        errors.append("crystalMhz=%d は %s では使えない（%s）" % (cfg["crystalMhz"], cfg["controller"], ", ".join(str(c) for c in ctl["crystals"])))
    fd = cfg["fd"]
    if fd["enabled"]:
        if not ctl["fd"]:
            errors.append("fd.enabled=true だが、%s は CAN FD に対応していない" % cfg["controller"])
        nominal = nominal_bps(cfg["baudrate"])
        if fd["dataBitrate"] < nominal or fd["dataBitrate"] % nominal != 0:
            errors.append("fd.dataBitrate=%d は、通常のビットレート %d の整数倍（以上）でなければならない" % (fd["dataBitrate"], nominal))
    for k in ("id", "mask"):
        v = int(cfg["filter"][k], 16)
        if v > 0x1FFFFFFF:
            errors.append("filter.%s が 29 bit を超えている" % k)
    return errors


def cfg_header_text(ctx):
    root = pathlib.Path(getattr(ctx, "repo_root", None) or REPO_ROOT)
    return (root / "src" / "Bsw" / "Can" / "Can_Cfg.h").read_bytes().decode("utf-8", errors="replace")


def check_names(cfg, ctx):
    text = cfg_header_text(ctx)
    baud = "CAN_BAUDRATE_" + cfg["baudrate"]
    crystal = "CAN_CRYSTAL_%dMHZ" % cfg["crystalMhz"]
    for name in (baud, crystal):
        if not re.search(r"#define\s+%s\b" % name, text):
            raise ValueError("Can_Cfg.h に %s が無い" % name)
    return baud, crystal


def hex_value(s):
    return "0x%03XU" % int(s, 16)


def render_pins(cfg, ctx):
    out = doc_comment([cfg["csPinDoc"]])
    out.append("#define CAN_CS_PIN             %dU" % cfg["csPin"])
    out.append("")
    out += doc_comment([cfg["intPinDoc"]])
    out.append("#define CAN_INT_PIN            %dU" % cfg["intPin"])
    return "\n".join(out)


def render_config(cfg, ctx):
    baud, crystal = check_names(cfg, ctx)
    out = ["const Can_ConfigType Can_Config = {"]
    out += block_comment(cfg["filterNote"], indent=4)
    out.append("    .filter      = { .filterId = %s, .mask = %s }," % (hex_value(cfg["filter"]["id"]), hex_value(cfg["filter"]["mask"])))
    out.append("    .csPin       = CAN_CS_PIN,")
    out.append("    .intPin      = CAN_INT_PIN,")
    out.append("    .baudrate    = %s," % baud)
    out.append("    .crystalFreq = %s," % crystal)
    fd = cfg["fd"]
    out += block_comment(fd["note"], indent=4)
    out.append("    .fdEnabled       = %dU," % (1 if fd["enabled"] else 0))
    out.append("    .fdBaudrate      = %dUL," % fd["dataBitrate"])
    out.append("    .txBitRateSwitch = %dU" % (1 if fd["txBitRateSwitch"] else 0))
    out.append("};")
    return "\n".join(out)


SPEC = {
    "config": "config/data/Can.json",
    "schema": "config/schema/Can.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Can/Can_Cfg.h", "can-pins", render_pins, None),
        ("src/Bsw/Can/Can_PBCfg.c", "can-config", render_config, None),
    ],
}

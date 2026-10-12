"""Port（Port_Cfg.h）の管理ピンの定義の生成。

Port.json に書く値: 管理ピン総数 PORT_PIN_COUNT と、各ピンの番号 PORT_PIN_*、それぞれの説明（形式は defines.py 参照）。
検査: ピンの番号が重複しない。PORT_PIN_COUNT が、PORT_PIN_* の数と一致する。Dio のチャネルが Port のピンに含まれることは checks.py で検査する。
"""

from . import defines


def pins(cfg):
    """{PORT_PIN_* のマクロ名: ピン番号}（PORT_PIN_COUNT を除く）。"""
    return {m: defines.number(v) for m, v in defines.values(cfg).items() if m.startswith("PORT_PIN_") and m != "PORT_PIN_COUNT"}


def validate(cfg):
    errors = []
    vals = defines.values(cfg)
    p = pins(cfg)
    if "PORT_PIN_COUNT" not in vals:
        return ["PORT_PIN_COUNT が無い"]
    if defines.number(vals["PORT_PIN_COUNT"]) != len(p):
        errors.append("PORT_PIN_COUNT=%d が PORT_PIN_* の数 %d と一致しない" % (defines.number(vals["PORT_PIN_COUNT"]), len(p)))
    if len(set(p.values())) != len(p):
        errors.append("PORT_PIN_* の番号が重複している")
    return errors


SPEC = defines.make_spec("Port", "src/Bsw/Port/Port_Cfg.h", "port-pins", validate)

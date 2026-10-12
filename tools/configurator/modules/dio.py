"""Dio（Dio_Cfg.h）のチャネル・ポート・チャネルグループの定義の生成。

Dio.json に書く値: チャネル番号 DIO_CHANNEL_*、仮想ポート DIO_PORT_*、ポートの総数 DIO_PORT_COUNT、チャネルグループ
DIO_CHANNELGROUP_*_PORT / _MASK / _OFFSET、それぞれの説明（形式は defines.py 参照）。
検査: チャネル番号が重複しない。ポートの番号が DIO_PORT_COUNT 未満。チャネルグループの PORT が、定義済みのポートを指す。
Dio のチャネルが Port のピンに含まれることは checks.py で検査する。
"""

from . import defines


def channels(cfg):
    """{DIO_CHANNEL_* のマクロ名: チャネル番号}。"""
    return {m: defines.number(v) for m, v in defines.values(cfg).items() if m.startswith("DIO_CHANNEL_") and m.isupper() and not m.startswith("DIO_CHANNELGROUP_")}


def validate(cfg):
    errors = []
    vals = defines.values(cfg)
    ch = channels(cfg)
    if len(set(ch.values())) != len(ch):
        errors.append("DIO_CHANNEL_* の番号が重複している")
    if "DIO_PORT_COUNT" not in vals:
        return errors + ["DIO_PORT_COUNT が無い"]
    count = defines.number(vals["DIO_PORT_COUNT"])
    ports = {m for m in vals if m.startswith("DIO_PORT_") and m != "DIO_PORT_COUNT"}
    for m in ports:
        if defines.number(vals[m]) >= count:
            errors.append("%s=%d が DIO_PORT_COUNT=%d 未満でない" % (m, defines.number(vals[m]), count))
    if len(ports) != count:
        errors.append("DIO_PORT_COUNT=%d が DIO_PORT_* の数 %d と一致しない" % (count, len(ports)))
    for m, v in vals.items():
        if m.startswith("DIO_CHANNELGROUP_") and m.endswith("_PORT") and v not in ports:
            errors.append("%s の値 %s が、定義済みのポート（DIO_PORT_*）ではない" % (m, v))
    return errors


SPEC = defines.make_spec("Dio", "src/Bsw/Dio/Dio_Cfg.h", "dio-channels", validate)

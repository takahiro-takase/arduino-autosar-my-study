"""CryIf（CryIf_Cfg.h）の Crypto チャネル ID の定義の生成。

CryIf.json に書く値: CRYIF_CHANNEL_ID とその説明（形式は defines.py 参照）。
検査: CRYIF_CHANNEL_ID が数値のリテラルであること。
CryIf_Cfg.h の他の定数（ModuleId、エラーコード、ApiId、バージョン）は AUTOSAR の仕様で決まる固定値のため、手書きのまま。
"""

from . import defines


def validate(cfg):
    vals = defines.values(cfg)
    if "CRYIF_CHANNEL_ID" not in vals:
        return ["CRYIF_CHANNEL_ID が無い"]
    try:
        defines.number(vals["CRYIF_CHANNEL_ID"])
    except ValueError as e:
        return ["CRYIF_CHANNEL_ID: %s" % e]
    return []


SPEC = defines.make_spec("CryIf", "src/Bsw/CryIf/CryIf_Cfg.h", "cryif-channel", validate)

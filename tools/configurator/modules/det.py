"""Det（Det_Cfg.h）のログレベルの既定値の生成。

Det.json に書く値:
  logLevel.value : 既定のログレベル。LOG_T / LOG_E / LOG_W / LOG_I / LOG_D のどれか（Det.h の LogLevel）。
                   ビルド設定（-DDET_LOG_LEVEL=...）で上書きできるよう、生成するのは `#ifndef DET_LOG_LEVEL` の中の既定値。
  logLevel.doc   : 既定値の説明（行末のコメントとして出す）
Det_Cfg.h の他の定数（ModuleId、エラーコード、ApiId、バージョン）は AUTOSAR の仕様で決まる固定値のため、手書きのまま。
"""

LEVELS = ("LOG_T", "LOG_E", "LOG_W", "LOG_I", "LOG_D")


def validate(cfg):
    if cfg["logLevel"]["value"] not in LEVELS:
        return ["logLevel.value '%s' は %s のどれかでなければならない" % (cfg["logLevel"]["value"], ", ".join(LEVELS))]
    return []


def render_log_level(cfg, ctx=None):
    lv = cfg["logLevel"]
    return "\n".join([
        "#ifndef DET_LOG_LEVEL",
        "#  define DET_LOG_LEVEL  %s  /**< %s */" % (lv["value"], lv["doc"]),
        "#endif",
    ])


SPEC = {
    "config": "config/data/Det.json",
    "schema": "config/schema/Det.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Det/Det_Cfg.h", "det-log-level", render_log_level, None),
    ],
}

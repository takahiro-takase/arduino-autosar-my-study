"""CanSM（CanSM_Cfg.h）のチャネル数・Bus-Off 回復時間・ウェイクアップ検証時間の定義の生成。

CanSM.json に書く値: チャネル数、Bus-Off 回復待機時間 L1/L2 [ms]、L1 から L2 へ切り替える試行回数、
Bus-Off 回復の確定条件（TX 確認のポーリングを使うか、使わないときの待ち時間 [ms]）、ウェイクアップ検証時間 [ms]、各項目の説明。
検査: L2 は L1 より長いこと。時間は CanSM_MainFunction() の周期（Os.json）の倍数であること
（周期の途中の値は、次の周期まで丸められて意図より長くなるため）。チャネル数は 1 のみ対応（CanSM.c が 1 チャネル固定のため）。ComM.json との一致は checks.py で検査する。
"""

from .common import os_periods
from .defs import render_layout

MAIN_FUNCTION = "CanSM_MainFunction"

LAYOUT = (
    ("define", "channelCount", "CANSM_CHANNEL_COUNT", "%dU"),
    ("define", "busOffRecoveryL1Ms", "CANSM_BUSOFF_RECOVERY_L1_MS", "%dUL"),
    ("define", "busOffRecoveryL2Ms", "CANSM_BUSOFF_RECOVERY_L2_MS", "%dUL"),
    ("define", "busOffL1ToL2Count", "CANSM_BUSOFF_L1_TO_L2_COUNT", "%dU"),
    ("define", "borTxConfirmationPolling", "CANSM_BOR_TX_CONFIRMATION_POLLING", "%dU"),
    ("define", "borTimeTxEnsuredMs", "CANSM_BOR_TIME_TX_ENSURED_MS", "%dUL"),
    ("define", "wakeupValidationMs", "CANSM_WAKEUP_VALIDATION_MS", "%dUL"),
)


def validate(cfg):
    errors = []
    if cfg["channelCount"]["value"] != 1:
        errors.append("channelCount は 1 のみ対応（CanSM.c が 1 チャネル固定のため）")
    if cfg["busOffRecoveryL2Ms"]["value"] <= cfg["busOffRecoveryL1Ms"]["value"]:
        errors.append("busOffRecoveryL2Ms は busOffRecoveryL1Ms より長くする")
    return errors


def render(cfg, ctx):
    period = os_periods(ctx).get(MAIN_FUNCTION)
    if period is None:
        raise ValueError("CanSM.json: Os.json の tasks に %s が無い" % MAIN_FUNCTION)
    for key in ("busOffRecoveryL1Ms", "busOffRecoveryL2Ms", "borTimeTxEnsuredMs", "wakeupValidationMs"):
        if cfg[key]["value"] % period:
            raise ValueError("CanSM.json: %s=%d は %s の周期 %d ms の倍数ではない" % (key, cfg[key]["value"], MAIN_FUNCTION, period))
    return render_layout(cfg, LAYOUT)


SPEC = {
    "config": "config/data/CanSM.json",
    "schema": "config/schema/CanSM.schema.json",
    "validate": validate,
    "targets": [("src/Bsw/CanSM/CanSM_Cfg.h", "cansm-params", render, None)],
}

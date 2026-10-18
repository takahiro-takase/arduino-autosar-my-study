"""IoHwAb（IoHwAb_Cfg.h）のボタンのデバウンス・固着判定と、ADC 電圧低下の閾値の生成。

IoHwAb.json に書く値:
  osTask                    : IoHwAb_MainFunction を呼ぶタスクの関数名（Os.json の tasks の function）。サンプルの周期を引くのに使う。
  button.debounceMs         : ボタンの生レベルが連続して変わり続けたら、確定値を変える時間 [ms]
  button.stuckMs            : 押下が確定してから、固着と判定するまでの時間 [ms]
  adc.lowVoltageThresholdMv : ADC の電圧がこの値を下回ったら、電圧低下とみなす閾値 [mV]
生成する値: IOHWAB_BUTTON_DEBOUNCE_COUNT（= debounceMs ÷ タスクの周期）、IOHWAB_BUTTON_STUCK_COUNT（= stuckMs ÷ 周期）、
            IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV。
検査:
  - osTask が Os.json に実在し、debounceMs・stuckMs がその周期の整数倍であること（サンプル数が割り切れる）。
  - デバウンスのサンプル数が 1〜255（カウンタは uint8）、固着のサンプル数が 1〜65535（カウンタは uint16）に収まること。
  - 電圧低下の閾値が 1 以上で、Adc.json の基準電圧 ADC_REF_VOLTAGE_MV 以下であること。
IoHwAb.c の他の定数（ModuleId、エラーコード、ApiId）は、手書きのままである。
"""

from . import defines
from .common import doc_comment, os_periods


def counts(cfg, ctx):
    """(デバウンスのサンプル数, 固着のサンプル数, サンプル周期 [ms])。割り切れない・周期が引けないときは ValueError。"""
    periods = os_periods(ctx)
    task = cfg["osTask"]
    if task not in periods:
        raise ValueError("osTask '%s' が Os.json の tasks に無い" % task)
    period = periods[task]
    out = []
    for key in ("debounceMs", "stuckMs"):
        ms = cfg["button"][key]
        if ms % period != 0:
            raise ValueError("button.%s=%d が、%s の周期 %d ms の整数倍ではない" % (key, ms, task, period))
        out.append(ms // period)
    return out[0], out[1], period


def validate(cfg):
    return []


def check_ranges(cfg, ctx):
    errors = []
    try:
        debounce, stuck, _ = counts(cfg, ctx)
    except ValueError as e:
        return [str(e)]
    if not 1 <= debounce <= 255:
        errors.append("デバウンスのサンプル数 %d が 1〜255（uint8 のカウンタ）に収まらない" % debounce)
    if not 1 <= stuck <= 65535:
        errors.append("固着のサンプル数 %d が 1〜65535（uint16 のカウンタ）に収まらない" % stuck)
    threshold = cfg["adc"]["lowVoltageThresholdMv"]
    adc = ctx.configs.get("Adc")
    if adc is not None:
        vals = defines.values(adc)
        if "ADC_REF_VOLTAGE_MV" in vals:
            ref = defines.number(vals["ADC_REF_VOLTAGE_MV"])
            if threshold > ref:
                errors.append("adc.lowVoltageThresholdMv=%d が基準電圧 ADC_REF_VOLTAGE_MV=%d を超えている" % (threshold, ref))
    return errors


def render_params(cfg, ctx):
    debounce, stuck, period = counts(cfg, ctx)
    out = doc_comment(["ボタン確定に必要な連続一致サンプル数 (%d × %dms = %dms)" % (debounce, period, debounce * period)])
    out.append("#define IOHWAB_BUTTON_DEBOUNCE_COUNT  %dU" % debounce)
    out.append("")
    out += doc_comment(["ボタン固着と判定するまでの確定押下継続サンプル数 (%d × %dms = %dms)" % (stuck, period, stuck * period)])
    out.append("#define IOHWAB_BUTTON_STUCK_COUNT     %dU" % stuck)
    out.append("")
    out += doc_comment(["ADC 電圧低下と判定する閾値 [mV]"])
    out.append("#define IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV  %dU" % cfg["adc"]["lowVoltageThresholdMv"])
    return "\n".join(out)


SPEC = {
    "config": "config/data/IoHwAb.json",
    "schema": "config/schema/IoHwAb.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/IoHwAb/IoHwAb_Cfg.h", "iohwab-params", render_params, None),
    ],
}

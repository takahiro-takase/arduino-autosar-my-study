"""Adc（Adc_Cfg.h）のチャネル・グループ・分解能・基準電圧の定義の生成。

Adc.json に書く値: ADC_CHANNEL_*、ADC_GROUP_*、ADC_GROUP_COUNT、ADC_RESOLUTION_MAX、ADC_REF_VOLTAGE_MV、それぞれの説明（形式は defines.py 参照）。
検査: グループの ID が ADC_GROUP_COUNT 未満で、重複しない。分解能の最大値が 2^n - 1。基準電圧が 0 より大きい。
"""

from . import defines


def validate(cfg):
    errors = []
    vals = defines.values(cfg)
    for need in ("ADC_GROUP_COUNT", "ADC_RESOLUTION_MAX", "ADC_REF_VOLTAGE_MV"):
        if need not in vals:
            return ["%s が無い" % need]
    count = defines.number(vals["ADC_GROUP_COUNT"])
    groups = {m: defines.number(v) for m, v in vals.items() if m.startswith("ADC_GROUP_") and m != "ADC_GROUP_COUNT"}
    for m, v in groups.items():
        if v >= count:
            errors.append("%s=%d が ADC_GROUP_COUNT=%d 未満でない" % (m, v, count))
    if len(set(groups.values())) != len(groups):
        errors.append("ADC_GROUP_* の ID が重複している")
    res = defines.number(vals["ADC_RESOLUTION_MAX"])
    if res < 1 or (res + 1) & res != 0:
        errors.append("ADC_RESOLUTION_MAX=%d が 2^n - 1 ではない" % res)
    if defines.number(vals["ADC_REF_VOLTAGE_MV"]) <= 0:
        errors.append("ADC_REF_VOLTAGE_MV は 0 より大きくする")
    return errors


SPEC = defines.make_spec("Adc", "src/Bsw/Adc/Adc_Cfg.h", "adc-channels", validate)

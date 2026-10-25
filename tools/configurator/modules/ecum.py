"""EcuM（EcuM_Cfg.h）の RUN 要求ユーザ・POST_RUN タイムアウト・ウェイクアップ要因の定義の生成。

EcuM.json に書く値: ECUM_USER_COUNT、ECUM_USER_*、ECUM_POST_RUN_TIMEOUT_MS、ECUM_VALIDATION_TIMEOUT_MS、ECUM_WKSOURCE_*、
それぞれの説明（形式は defines.py 参照）。
検査: ユーザの ID が ECUM_USER_COUNT 未満で、重複しない。ウェイクアップ要因が 1 ビットで、仕様で予約済みのビット 0〜4 を使わない。
"""

from . import defines

RESERVED_WAKEUP_BITS = 5  # [SWS_EcuM_04040] ビット 0〜4 は仕様で予約済み


def validate(cfg):
    errors = []
    vals = defines.values(cfg)
    for need in ("ECUM_USER_COUNT", "ECUM_POST_RUN_TIMEOUT_MS", "ECUM_VALIDATION_TIMEOUT_MS"):
        if need not in vals:
            return ["%s が無い" % need]
    count = defines.number(vals["ECUM_USER_COUNT"])
    users = {m: defines.number(v) for m, v in vals.items() if m.startswith("ECUM_USER_") and m != "ECUM_USER_COUNT"}
    for m, v in users.items():
        if v >= count:
            errors.append("%s=%d が ECUM_USER_COUNT=%d 未満でない" % (m, v, count))
    if len(set(users.values())) != len(users):
        errors.append("ECUM_USER_* の ID が重複している")
    if defines.number(vals["ECUM_POST_RUN_TIMEOUT_MS"]) <= 0:
        errors.append("ECUM_POST_RUN_TIMEOUT_MS は 0 より大きくする")
    if defines.number(vals["ECUM_VALIDATION_TIMEOUT_MS"]) <= 0:
        errors.append("ECUM_VALIDATION_TIMEOUT_MS は 0 より大きくする")
    for m, v in vals.items():
        if m.startswith("ECUM_WKSOURCE_"):
            n = defines.number(v)
            if n == 0 or (n & (n - 1)) != 0:
                errors.append("%s=0x%X が 1 ビットではない" % (m, n))
            elif n < (1 << RESERVED_WAKEUP_BITS):
                errors.append("%s=0x%X が、仕様で予約済みのビット 0〜4 にかかっている" % (m, n))
    return errors


SPEC = defines.make_spec("EcuM", "src/Bsw/EcuM/EcuM_Cfg.h", "ecum-params", validate)

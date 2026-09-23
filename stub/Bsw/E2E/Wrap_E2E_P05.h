/**
 * \file    Wrap_E2E_P05.h
 * \brief   `src/Bsw/E2E/E2E_P05.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/Can/Wrap_Can.h` 冒頭コメント参照
 *          （「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `E2E_P05.h` が宣言する公開API全5関数（ProtectInit/Protect/
 *          CheckInit/Check/MapStatusToSM）を wrap 対象とする。5関数すべてが
 *          戻り値を持つため、全関数に「指定した呼び出し回数以降は常に失敗を
 *          返す」という回数閾値方式の故障注入を実装する。
 *          `FailFromCallCount_E2E_P05Xxx`（既定
 *          `WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_E2E_P05Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_E2E_P05Xxx` を返すようになる（本物は
 *          一切呼ばず、出力引数にも触れない）。
 *
 *          変数名・故障注入方式・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_E2E_P05_H
#define WRAP_E2E_P05_H

#include "Std_Types.h"
#include "E2E_P05.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_E2E_P05Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（公開 API 全5関数共通）
 * ---------------------------------------------------------------------- */

extern uint32 CallCount_E2E_P05Protect;
extern uint32 CallCount_E2E_P05ProtectInit;
extern uint32 CallCount_E2E_P05Check;
extern uint32 CallCount_E2E_P05CheckInit;
extern uint32 CallCount_E2E_P05MapStatusToSM;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（全5関数）
 * ---------------------------------------------------------------------- */

/** `WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_E2E_P05Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_E2E_P05Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_E2E_P05Xxx` を返す。 */
extern uint32 FailFromCallCount_E2E_P05Protect;
extern uint32 FailFromCallCount_E2E_P05ProtectInit;
extern uint32 FailFromCallCount_E2E_P05Check;
extern uint32 FailFromCallCount_E2E_P05CheckInit;
extern uint32 FailFromCallCount_E2E_P05MapStatusToSM;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（全5関数）
 * ---------------------------------------------------------------------- */

extern Std_ReturnType       ForcedReturn_E2E_P05Protect;        /**< 既定 E2E_E_INPUTERR_NULL */
extern Std_ReturnType       ForcedReturn_E2E_P05ProtectInit;    /**< 既定 E2E_E_INPUTERR_NULL */
extern Std_ReturnType       ForcedReturn_E2E_P05Check;          /**< 既定 E2E_E_INPUTERR_NULL */
extern Std_ReturnType       ForcedReturn_E2E_P05CheckInit;      /**< 既定 E2E_E_INPUTERR_NULL */
extern E2E_PCheckStatusType ForcedReturn_E2E_P05MapStatusToSM;  /**< 既定 E2E_P_ERROR */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapE2EP05_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_E2E_P05_H */

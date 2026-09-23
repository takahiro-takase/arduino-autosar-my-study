/**
 * \file    Wrap_CanSM.h
 * \brief   `src/Bsw/CanSM/CanSM.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/Can/Wrap_Can.h` 冒頭コメント参照
 *          （「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `CanSM.h` が宣言する AUTOSAR IF 全9関数を wrap 対象とする。
 *
 *          戻り値を持つ2関数（RequestComMode/GetCurrentComMode）には
 *          「指定した呼び出し回数以降は常に失敗を返す」という回数閾値方式の
 *          故障注入を実装する。`FailFromCallCount_CanSM_Xxx`（既定
 *          `WRAP_CANSM_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_CanSM_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_CanSM_Xxx` を返すようになる。
 *
 *          戻り値を持たない7関数（Init/DeInit/ControllerBusOff/
 *          ControllerModeIndication/RxIndication/MainFunction/
 *          GetVersionInfo）は故障注入する戻り値が無いため、呼び出し回数のみを
 *          記録する。
 *
 *          変数名・故障注入方式・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_CANSM_H
#define WRAP_CANSM_H

#include "Std_Types.h"
#include "CanSM.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_CanSM_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_CANSM_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（AUTOSAR IF 全9関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_CanSM_Init;
extern uint32 CallCount_CanSM_DeInit;
extern uint32 CallCount_CanSM_RequestComMode;
extern uint32 CallCount_CanSM_GetCurrentComMode;
extern uint32 CallCount_CanSM_ControllerBusOff;
extern uint32 CallCount_CanSM_ControllerModeIndication;
extern uint32 CallCount_CanSM_RxIndication;
extern uint32 CallCount_CanSM_MainFunction;
extern uint32 CallCount_CanSM_GetVersionInfo;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ2関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_CANSM_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_CanSM_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_CanSM_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_CanSM_Xxx` を返す。 */
extern uint32 FailFromCallCount_CanSM_RequestComMode;
extern uint32 FailFromCallCount_CanSM_GetCurrentComMode;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ2関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_CanSM_RequestComMode;    /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanSM_GetCurrentComMode; /**< 既定 E_NOT_OK */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapCanSM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_CANSM_H */

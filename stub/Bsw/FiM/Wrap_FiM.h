/**
 * \file    Wrap_FiM.h
 * \brief   `src/Bsw/FiM/FiM.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h` 冒頭コメント
 *          参照（「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `FiM.h` が宣言する関数のうち実装済みの5関数を wrap 対象とする
 *          （`DemTriggerOnMonitorStatus`/`DemTriggerOnComponentStatus`/
 *          `DemInit` は未実装のため wrap 対象外）。
 *
 *          戻り値を持つ2関数（GetFunctionPermission/SetFunctionAvailable）
 *          には「指定した呼び出し回数以降は常に失敗を返す」という回数閾値
 *          方式の故障注入を実装する。`FailFromCallCount_FiM_Xxx`（既定
 *          `WRAP_FIM_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_FiM_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_FiM_Xxx` を返すようになる。
 *
 *          戻り値を持たない3関数（Init/GetVersionInfo/MainFunction）は
 *          故障注入する戻り値が無いため、呼び出し回数のみ記録する。
 *
 *          変数名・故障注入方式・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_FIM_H
#define WRAP_FIM_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "FiM.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** `FailFromCallCount_FiM_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_FIM_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * 呼び出し回数（実装済み全5関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_FiM_Init;
extern uint32 CallCount_FiM_GetFunctionPermission;
extern uint32 CallCount_FiM_SetFunctionAvailable;
extern uint32 CallCount_FiM_GetVersionInfo;
extern uint32 CallCount_FiM_MainFunction;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ2関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_FIM_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_FiM_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_FiM_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_FiM_Xxx` を返す。 */
extern uint32 FailFromCallCount_FiM_GetFunctionPermission;
extern uint32 FailFromCallCount_FiM_SetFunctionAvailable;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ2関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_FiM_GetFunctionPermission;  /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_FiM_SetFunctionAvailable;   /**< 既定 E_NOT_OK */

/* ======================================================================
 * Functions
 * ====================================================================== */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapFiM_Reset(void);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* WRAP_FIM_H */

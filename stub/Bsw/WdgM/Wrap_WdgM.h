/**
 * \file    Wrap_WdgM.h
 * \brief   `src/Bsw/WdgM/WdgM.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h` 冒頭コメント
 *          参照（「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `WdgM.h` が宣言する公開API全15関数（AUTOSAR IF 11関数 +
 *          本プロジェクト独自拡張4関数: EnableHwWatchdog/DisableHwWatchdog/
 *          ResumeSupervision/TriggerHwWatchdog）を wrap 対象とする
 *          （`WdgM_Test_SetFirstExpiredSEIDRaw` はテスト専用の拡張関数であり
 *          `WDGM_UNIT_TEST` ビルドでのみ存在するため対象外）。
 *
 *          戻り値を持つ6関数（SetMode/GetMode/CheckpointReached/
 *          GetLocalStatus/GetGlobalStatus/GetFirstExpiredSEID）には
 *          「指定した呼び出し回数以降は常に失敗を返す」という回数閾値方式の
 *          故障注入を実装する。`FailFromCallCount_WdgM_Xxx`（既定
 *          `WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_WdgM_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_WdgM_Xxx` を返すようになる。
 *
 *          戻り値を持たない9関数（Init/DeInit/EnableHwWatchdog/
 *          DisableHwWatchdog/ResumeSupervision/MainFunction/
 *          TriggerHwWatchdog/PerformReset/GetVersionInfo）は故障注入する
 *          戻り値が無いため、呼び出し回数のみ記録する。
 *
 *          変数名・故障注入方式・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_WDGM_H
#define WRAP_WDGM_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "WdgM.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** `FailFromCallCount_WdgM_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

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
 * 呼び出し回数（公開API全15関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_WdgM_Init;
extern uint32 CallCount_WdgM_DeInit;
extern uint32 CallCount_WdgM_GetVersionInfo;
extern uint32 CallCount_WdgM_SetMode;
extern uint32 CallCount_WdgM_GetMode;
extern uint32 CallCount_WdgM_EnableHwWatchdog;
extern uint32 CallCount_WdgM_DisableHwWatchdog;
extern uint32 CallCount_WdgM_ResumeSupervision;
extern uint32 CallCount_WdgM_CheckpointReached;
extern uint32 CallCount_WdgM_GetLocalStatus;
extern uint32 CallCount_WdgM_GetGlobalStatus;
extern uint32 CallCount_WdgM_MainFunction;
extern uint32 CallCount_WdgM_TriggerHwWatchdog;
extern uint32 CallCount_WdgM_PerformReset;
extern uint32 CallCount_WdgM_GetFirstExpiredSEID;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ6関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_WdgM_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_WdgM_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_WdgM_Xxx` を返す。 */
extern uint32 FailFromCallCount_WdgM_SetMode;
extern uint32 FailFromCallCount_WdgM_GetMode;
extern uint32 FailFromCallCount_WdgM_CheckpointReached;
extern uint32 FailFromCallCount_WdgM_GetLocalStatus;
extern uint32 FailFromCallCount_WdgM_GetGlobalStatus;
extern uint32 FailFromCallCount_WdgM_GetFirstExpiredSEID;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ6関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_WdgM_SetMode;                /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_WdgM_GetMode;                 /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_WdgM_CheckpointReached;       /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_WdgM_GetLocalStatus;          /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_WdgM_GetGlobalStatus;         /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_WdgM_GetFirstExpiredSEID;     /**< 既定 E_NOT_OK */

/* ======================================================================
 * Functions
 * ====================================================================== */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapWdgM_Reset(void);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* WRAP_WDGM_H */

/**
 * \file    Wrap_MemIf.h
 * \brief   `src/Bsw/MemIf/MemIf.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照。既定動作は `__real_...` へのパススルー。
 *
 *          MemIf.h の公開 API 全9関数を wrap 対象とする。NvM は MemIf 経由で Fee を使うため、
 *          NvM の「MemIf が書き込みを受け付けない」「ジョブが失敗する」「いつまでも終わらない」
 *          という異常系（実体の Fee では起こせない）を、ここで再現する。
 *
 *          戻り値を持つ4関数（Read/Write/GetStatus/GetJobResult）には、
 *          「指定した呼び出し回数以降は常に強制値を返す」という回数閾値方式の
 *          故障注入を実装する（`stub/Bsw/PduR/Wrap_PduR.h` 参照）。
 *          `FailFromCallCount_MemIf_Xxx`（既定 `WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED`）に
 *          N を設定すると、`CallCount_MemIf_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_MemIf_Xxx` を返す。
 *          戻り値を持たない5関数は、呼び出し回数のみを記録する。
 *
 *          全状態を一括で初期状態へ戻す `WrapMemIf_Reset()` を1つ持つ。各テストケースは
 *          SetUp() と TearDown() で呼び、`FailFromCallCount_MemIf_Xxx` を Arrange 区間でのみ立てること
 *          （MemIf は Fee・NvM を使う他のテストからも呼ばれるため、設定を残さない）。
 */
#ifndef WRAP_MEMIF_H
#define WRAP_MEMIF_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "MemIf_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** `FailFromCallCount_MemIf_Xxx` の「無効（常にパススルー）」を表す番兵値。 */
#define WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* 呼び出し回数（公開 API 全9関数） */
extern uint32 CallCount_MemIf_Init;
extern uint32 CallCount_MemIf_GetVersionInfo;
extern uint32 CallCount_MemIf_SetMode;
extern uint32 CallCount_MemIf_Read;
extern uint32 CallCount_MemIf_Write;
extern uint32 CallCount_MemIf_Cancel;
extern uint32 CallCount_MemIf_GetStatus;
extern uint32 CallCount_MemIf_GetJobResult;
extern uint32 CallCount_MemIf_MainFunction;

/* 回数閾値故障注入（戻り値を持つ4関数のみ） */
extern uint32 FailFromCallCount_MemIf_Read;
extern uint32 FailFromCallCount_MemIf_Write;
extern uint32 FailFromCallCount_MemIf_GetStatus;
extern uint32 FailFromCallCount_MemIf_GetJobResult;

/* 閾値到達後の強制戻り値（戻り値を持つ4関数のみ） */
extern Std_ReturnType      ForcedReturn_MemIf_Read;         /**< 既定 E_NOT_OK */
extern Std_ReturnType      ForcedReturn_MemIf_Write;        /**< 既定 E_NOT_OK */
extern MemIf_StatusType    ForcedReturn_MemIf_GetStatus;    /**< 既定 MEMIF_BUSY（終わらないジョブ） */
extern MemIf_JobResultType ForcedReturn_MemIf_GetJobResult; /**< 既定 MEMIF_JOB_FAILED */

/* ======================================================================
 * Functions
 * ====================================================================== */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。 */
void WrapMemIf_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_MEMIF_H */

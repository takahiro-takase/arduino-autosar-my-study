/**
 * \file    Wrap_CanNm.h
 * \brief   `src/Bsw/CanNm/CanNm.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/Can/Wrap_Can.h` 冒頭コメント参照
 *          （「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `CanNm.h` が宣言する AUTOSAR IF 全14関数を wrap 対象とする。
 *
 *          戻り値を持つ8関数（NetworkRequest/NetworkRelease/
 *          RepeatMessageRequest/DisableCommunication/EnableCommunication/
 *          GetLocalNodeIdentifier/GetNodeIdentifier/GetState）には
 *          「指定した呼び出し回数以降は常に失敗を返す」という回数閾値方式の
 *          故障注入を実装する。`FailFromCallCount_CanNm_Xxx`（既定
 *          `WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_CanNm_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_CanNm_Xxx` を返すようになる。
 *
 *          戻り値を持たない6関数（Init/DeInit/RxIndication/TxConfirmation/
 *          MainFunction/GetVersionInfo）は故障注入する戻り値が無いため、
 *          呼び出し回数のみを記録する。
 *
 *          変数名・故障注入方式・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_CANNM_H
#define WRAP_CANNM_H

#include "Std_Types.h"
#include "CanNm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_CanNm_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（AUTOSAR IF 全14関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_CanNm_Init;
extern uint32 CallCount_CanNm_DeInit;
extern uint32 CallCount_CanNm_NetworkRequest;
extern uint32 CallCount_CanNm_NetworkRelease;
extern uint32 CallCount_CanNm_DisableCommunication;
extern uint32 CallCount_CanNm_EnableCommunication;
extern uint32 CallCount_CanNm_GetNodeIdentifier;
extern uint32 CallCount_CanNm_GetLocalNodeIdentifier;
extern uint32 CallCount_CanNm_RepeatMessageRequest;
extern uint32 CallCount_CanNm_GetState;
extern uint32 CallCount_CanNm_GetVersionInfo;
extern uint32 CallCount_CanNm_TxConfirmation;
extern uint32 CallCount_CanNm_RxIndication;
extern uint32 CallCount_CanNm_MainFunction;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ8関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_CanNm_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_CanNm_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_CanNm_Xxx` を返す。 */
extern uint32 FailFromCallCount_CanNm_NetworkRequest;
extern uint32 FailFromCallCount_CanNm_NetworkRelease;
extern uint32 FailFromCallCount_CanNm_DisableCommunication;
extern uint32 FailFromCallCount_CanNm_EnableCommunication;
extern uint32 FailFromCallCount_CanNm_GetNodeIdentifier;
extern uint32 FailFromCallCount_CanNm_GetLocalNodeIdentifier;
extern uint32 FailFromCallCount_CanNm_RepeatMessageRequest;
extern uint32 FailFromCallCount_CanNm_GetState;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ8関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_CanNm_NetworkRequest;          /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanNm_NetworkRelease;          /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanNm_DisableCommunication;    /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanNm_EnableCommunication;     /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanNm_GetNodeIdentifier;       /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanNm_GetLocalNodeIdentifier;  /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanNm_RepeatMessageRequest;    /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_CanNm_GetState;                /**< 既定 E_NOT_OK */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapCanNm_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_CANNM_H */

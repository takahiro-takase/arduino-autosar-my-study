/**
 * \file    Wrap_Dcm.h
 * \brief   `src/Bsw/Dcm/Dcm_Cbk.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `test/stub/` 配下の構成規則は `test/stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *          命名・故障注入方式は [[reference_wrap_stub_naming_convention]] の
 *          標準テンプレートに従う。
 *
 *          AUTOSAR 仕様（SWS_Dcm）が定義する `Dcm.h` の公開 IF 関数8個に加え、
 *          本プロジェクト独自の受信コールバック `Dcm_ComIndication()`
 *          （`Dcm_Cbk.h` 参照）も対象に含める（いずれも実体は `Dcm_Cbk.c`
 *          1ファイルに定義されている）。DcmStack コールチェーンテストで、
 *          CanTp から Dcm への境界（`Dcm_ComIndication()`）を検証する用途を
 *          想定する。
 *
 *          戻り値を持つ5関数（GetVin/GetSesCtrlType/GetSecurityLevel/
 *          GetActiveProtocol/ResetToDefaultSession）には「指定した呼び出し
 *          回数以降は常に強制値を返す」という回数閾値方式の故障注入を実装
 *          する。`FailFromCallCount_Dcm_Xxx`
 *          （既定 `WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定
 *          すると、`CallCount_Dcm_Xxx` が N 以上になった回から（その回を
 *          含め、以降ずっと）`ForcedReturn_Dcm_Xxx` を返すようになる。
 *
 *          戻り値を持たない4関数（Init/MainFunction/GetVersionInfo/
 *          ComIndication）は故障注入する戻り値が無いため、呼び出し回数の
 *          みを記録する。
 *
 *          9関数すべて本ファイル1つが対象のため、他 wrap ファイルのような
 *          関数ごとの個別 `Reset()` ではなく、全状態を一括で初期状態へ戻す
 *          `WrapDcm_Reset()` を1つだけ持つ。各テストケースは SetUp() で
 *          `WrapDcm_Reset()` を1回呼び、`FailFromCallCount_Dcm_Xxx` を
 *          Arrange 区間でのみ立てること（他のテストケースの挙動を暗黙に
 *          変えないため）。
 */
#ifndef WRAP_DCM_H
#define WRAP_DCM_H

#include "Std_Types.h"
#include "Dcm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_Dcm_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（AUTOSAR IF + Dcm_ComIndication 全9関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_Dcm_Init;
extern uint32 CallCount_Dcm_MainFunction;
extern uint32 CallCount_Dcm_GetVin;
extern uint32 CallCount_Dcm_GetSesCtrlType;
extern uint32 CallCount_Dcm_GetSecurityLevel;
extern uint32 CallCount_Dcm_GetActiveProtocol;
extern uint32 CallCount_Dcm_ResetToDefaultSession;
extern uint32 CallCount_Dcm_GetVersionInfo;
extern uint32 CallCount_Dcm_ComIndication;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ5関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_Dcm_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_Dcm_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_Dcm_Xxx` を返す。 */
extern uint32 FailFromCallCount_Dcm_GetVin;
extern uint32 FailFromCallCount_Dcm_GetSesCtrlType;
extern uint32 FailFromCallCount_Dcm_GetSecurityLevel;
extern uint32 FailFromCallCount_Dcm_GetActiveProtocol;
extern uint32 FailFromCallCount_Dcm_ResetToDefaultSession;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ5関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_Dcm_GetVin;                 /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dcm_GetSesCtrlType;         /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dcm_GetSecurityLevel;       /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dcm_GetActiveProtocol;      /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dcm_ResetToDefaultSession;  /**< 既定 E_NOT_OK */

/* ----------------------------------------------------------------------
 * 直近の呼び出し内容のキャプチャ（Dcm_ComIndication のみ）
 * ---------------------------------------------------------------------- */
/** 直近の `Dcm_ComIndication()` 呼び出しで渡された `RxPduId`。 */
extern PduIdType LastRxPduId_Dcm_ComIndication;

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapDcm_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_DCM_H */

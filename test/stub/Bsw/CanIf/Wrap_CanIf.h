/**
 * \file    Wrap_CanIf.h
 * \brief   `src/Bsw/CanIf/CanIf.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `test/stub/` は env に依存しない共有のフォールトインジェクション
 *          ツリーで、`src/` と同じディレクトリ構成を持つ
 *          （`src/Bsw/CanIf/CanIf.c` に対する本ファイルは
 *          `test/stub/Bsw/CanIf/Wrap_CanIf.h`）。「wrap 対象の関数が定義されて
 *          いる元の src ファイル 1 つにつき 1 ファイル」を単位とし、
 *          CanIf.c 内の複数関数を wrap したくなった場合も新規ファイルを
 *          作らず本ファイルへ追記していく（2026-09 導入。複数 env に
 *          同じ wrap ファイルが重複する問題と、テスト環境の env 統合を
 *          見据えた構成）。
 *
 *          GNU ld の `--wrap` は、最終リンク後のバイナリ内で対象シンボルへの
 *          全呼び出し元を `__wrap_<symbol>()` へ差し替える。本物の定義は
 *          `__real_<symbol>()` という名前で引き続き呼び出せる。リンク単位
 *          全体に効く一括置換であり特定のテストケースだけを狙って差し替える
 *          仕組みではないため、既定動作は `__real_...` への単純な委譲
 *          （パススルー）とし、他のテストケースの挙動を暗黙に変えないように
 *          している。
 *
 *          2026-09-21、`test/stub/Bsw/Can/Wrap_Can.h` で確立した標準テンプレート
 *          （[[reference_wrap_stub_naming_convention]]）へ統一した。故障注入は
 *          単純な bool `ForceFail` ではなく回数閾値方式の
 *          `FailFromCallCount_CanIf_SetControllerMode`（既定
 *          `WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED`）を使う。
 *          `CallCount_CanIf_SetControllerMode` がこの値に達した回から
 *          （以降ずっと）`ForcedReturn_CanIf_SetControllerMode` を返す。
 *          N=1 を設定すれば即時強制失敗としても使える。変数名は
 *          `<種類>_<Module>_<関数名>`（種類=CallCount/FailFromCallCount/
 *          ForcedReturn/Last* を先頭側に置く）で統一する。個々のテストは
 *          `FailFromCallCount_CanIf_SetControllerMode` を Arrange 区間でのみ
 *          立て、`WrapCanIf_Reset()` で Act 直後に倒すこと。
 *
 * \par CanIf_SetControllerMode
 * `CanSM_MainFunction()` の Bus-Off 回復リトライ（`CanIf_SetControllerMode
 * (CAN_CS_STARTED)` 失敗時の分岐、CanSM.c 参照）を検証するために導入
 * （2026-09、`test/test_chain/Bsw_CanSM_BusOffRecovery_test.cpp` 参照。
 * 元は試作環境 `[env:native_chain_wrap]` の `Wrap_CanIf_SetControllerMode.h`
 * として新設し、本ファイルへ統合した）。CanIf.c/Can.c 自体はフェイクに
 * 置き換えず実体のまま、CanSM.c から見た戻り値だけをピンポイントで
 * 差し替える。
 *
 * \par CanIf_RxIndication / CanIf_TxConfirmation / CanIf_ControllerBusOff
 * 2026-09、test_native を native_chain へ統合した際に追加。Can.c がこの3関数を
 * 上位層通知として呼ぶ（旧 `test/test_native/Bsw_CanIf_fake.h` が単純な
 * 呼び出し記録フェイクとして丸ごと差し替えていた境界）。native_chain は
 * 既に CanIf.c を実体でリンクしており（Tx/Rx チェーン検証用）、
 * Bsw_Can_test.cpp（Can.c 単体検証）は CanIf の実際のPDUルーティングまでは
 * 対象外で、呼び出し回数・引数だけを検証したいため、CanIf_SetControllerMode
 * と同じ考え方で境界だけをピンポイントで観測する。既定は
 * `__real_...` へのパススルー（CanIf/CanSM 側に未初期化ガードがあり、
 * Bsw_Can_test.cpp は CanIf_Init()/CanSM_Init() を呼ばないため、実体へ渡っても
 * 静かに no-op になるだけで既存挙動に影響しない）。故障注入は誰も必要と
 * していないため、この3関数には呼び出し回数・引数キャプチャのみを持つ。
 */
#ifndef WRAP_CANIF_H
#define WRAP_CANIF_H

#include "Std_Types.h"
#include "CanIf_Types.h"
#include "CanIf.h"
#include "Can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_CanIf_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ====================================================================
 * External Variables
 * ==================================================================== */
extern uint32 CallCount_CanIf_Init;
extern uint32 CallCount_CanIf_DeInit;
extern uint32 CallCount_CanIf_SetControllerMode;
extern uint32 CallCount_CanIf_GetControllerMode;
extern uint32 CallCount_CanIf_GetControllerErrorState;
extern uint32 CallCount_CanIf_Transmit;
extern uint32 CallCount_CanIf_ReadRxPduData;
extern uint32 CallCount_CanIf_ReadTxNotifStatus;
extern uint32 CallCount_CanIf_ReadRxNotifStatus;
extern uint32 CallCount_CanIf_SetPduMode;
extern uint32 CallCount_CanIf_GetPduMode;
extern uint32 CallCount_CanIf_GetVersionInfo;
extern uint32 CallCount_CanIf_GetTxConfirmationState;
extern uint32 CallCount_CanIf_TxConfirmation;
extern uint32 CallCount_CanIf_RxIndication;
extern uint32 CallCount_CanIf_ControllerBusOff;

extern uint32 FailFromCallCount_CanIf_SetControllerMode;
extern uint32 FailFromCallCount_CanIf_GetControllerMode;
extern uint32 FailFromCallCount_CanIf_GetControllerErrorState;
extern uint32 FailFromCallCount_CanIf_Transmit;
extern uint32 FailFromCallCount_CanIf_ReadRxPduData;
extern uint32 FailFromCallCount_CanIf_ReadTxNotifStatus;
extern uint32 FailFromCallCount_CanIf_ReadRxNotifStatus;
extern uint32 FailFromCallCount_CanIf_SetPduMode;
extern uint32 FailFromCallCount_CanIf_GetPduMode;
extern uint32 FailFromCallCount_CanIf_GetTxConfirmationState;

/** 閾値到達後に返す戻り値（既定 E_NOT_OK）。 */
extern Std_ReturnType ForcedReturn_CanIf_SetControllerMode;
extern Std_ReturnType ForcedReturn_CanIf_GetControllerMode;
extern Std_ReturnType ForcedReturn_CanIf_GetControllerErrorState;
extern Std_ReturnType ForcedReturn_CanIf_Transmit;
extern Std_ReturnType ForcedReturn_CanIf_ReadRxPduData;
extern CanIf_NotifStatusType ForcedReturn_CanIf_ReadTxNotifStatus;
extern CanIf_NotifStatusType ForcedReturn_CanIf_ReadRxNotifStatus;
extern Std_ReturnType ForcedReturn_CanIf_SetPduMode;
extern Std_ReturnType ForcedReturn_CanIf_GetPduMode;
extern CanIf_NotifStatusType ForcedReturn_CanIf_GetTxConfirmationState;

extern uint8         LastData_CanIf_RxIndication[8];
extern Can_HwType    LastMailbox_CanIf_RxIndication;
extern PduLengthType LastLength_CanIf_RxIndication;
extern PduIdType     LastPduId_CanIf_TxConfirmation;
extern uint8         LastControllerId_CanIf_ControllerBusOff;

/** すべての関数呼び出し回数・回数閾値・強制戻り値・キャプチャ済み引数を
 *  初期状態へ戻す。各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapCanIf_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_CANIF_H */

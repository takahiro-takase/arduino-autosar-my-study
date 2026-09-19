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
 *          している。個々のテストは `ForceFail` 系フラグを Arrange 区間でのみ
 *          立てて狙った箇所だけ故障注入し、Act 直後に倒す。
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
 * 静かに no-op になるだけで既存挙動に影響しない。ForceFail 相当のトグルは
 * 無い（誰も故障注入を必要としていないため）。
 */
#ifndef WRAP_CANIF_H
#define WRAP_CANIF_H

#include "Std_Types.h"
#include "CanIf.h"
#include "Can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `__wrap_CanIf_SetControllerMode()` が呼ばれた回数。 */
extern uint32 WrapCanIfSetControllerMode_CallCount;

/** 0（既定）: `__real_CanIf_SetControllerMode()` へパススルー。
 *  0 以外: 呼び出し元へ即座に E_NOT_OK を返す（本物は一切呼ばない）。 */
extern uint8 WrapCanIfSetControllerMode_ForceFail;

/** 各テストケースの開始時に呼び、呼び出し回数・強制失敗フラグを初期状態へ戻す。 */
void WrapCanIfSetControllerMode_Reset(void);

extern uint32    WrapCanIfRxIndication_CallCount;
extern Can_HwType    WrapCanIfRxIndication_LastMailbox;
extern uint8         WrapCanIfRxIndication_LastData[8];
extern PduLengthType WrapCanIfRxIndication_LastLength;
void WrapCanIfRxIndication_Reset(void);

extern uint32    WrapCanIfTxConfirmation_CallCount;
extern PduIdType WrapCanIfTxConfirmation_LastPduId;
void WrapCanIfTxConfirmation_Reset(void);

extern uint32 WrapCanIfControllerBusOff_CallCount;
extern uint8  WrapCanIfControllerBusOff_LastControllerId;
void WrapCanIfControllerBusOff_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_CANIF_H */

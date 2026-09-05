/**
 * \file    Bsw_CanIf_fake.h
 * \brief   CanIf.h のテスト用フェイク実装の宣言（Can.c から見た上位層コールバック）
 * \details Can.c は CanIf_TxConfirmation/ControllerBusOff/RxIndication を
 *          上位層通知として呼ぶ（ウェイクアップ通知は 2026-09-05 是正で
 *          EcuM_CheckWakeup() 経由に変更されたため対象外、Bsw_EcuM_fake.h
 *          参照）。CanIf を本物でリンクすると PduR 以降まで芋づる式に
 *          必要になってしまうため、HAL 層のフェイクと同じ考え方で、Can.c が
 *          実際に呼ぶこの 3 関数だけを呼び出し記録付きのフェイクに差し替える
 *          （CanIf_Init/DeInit/Transmit/GetVersionInfo は Can.c から
 *          呼ばれないため未実装）。
 *
 *          Com→PduR→CanIf→Can のコールチェーンを CanIf の CanId/Dlc/Hth
 *          変換まで含めて検証したい場合は、CanIf の実体をリンクする必要が
 *          あり、この フェイクとシンボルが多重定義になるため両立できない
 *          （1 native バイナリに CanIf の実装を2種類同時に持てない）。
 *          そのようなテストは `test/test_chain/`（`[env:native_chain]`）という
 *          別のテスト環境・別バイナリに分離してある（`Bsw_TxChain_test.cpp`
 *          参照）。
 */
#ifndef BSW_CANIF_FAKE_H
#define BSW_CANIF_FAKE_H

#include "CanIf.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeCanIf_TxConfirmationCount;
extern uint32 FakeCanIf_ControllerBusOffCount;
extern uint32 FakeCanIf_RxIndicationCount;

/** 直近の呼び出し引数。 */
extern PduIdType FakeCanIf_LastTxConfirmationPduId;
extern uint8      FakeCanIf_LastControllerBusOffId;
extern Can_HwType FakeCanIf_LastRxMailbox;
extern uint8      FakeCanIf_LastRxData[8];
extern PduLengthType FakeCanIf_LastRxLength;

/** 各テストケースの開始時に呼び、記録をすべてクリアする。 */
void FakeCanIf_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSW_CANIF_FAKE_H */

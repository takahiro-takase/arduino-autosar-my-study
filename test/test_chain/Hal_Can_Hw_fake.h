/**
 * \file    Hal_Can_Hw_fake.h
 * \brief   Can_Hw.h（MCP2515 境界）のテスト用フェイク実装の宣言
 * \details Can.c のロジックだけを検証したいので、実 HW（MCP2515/SPI）は使わず、
 *          呼び出し回数・引数を記録するだけのフェイクに差し替える。
 *          各関数の戻り値は `FakeCanHw_*Return` で個別に制御でき、既定は
 *          すべて `CAN_HW_OK`（成功）にしてある。
 */
#ifndef HAL_CAN_HW_FAKE_H
#define HAL_CAN_HW_FAKE_H

#include "Std_Types.h"
#include "Can_Hw.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeCanHw_InitCount;
extern uint32 FakeCanHw_SendCount;
extern uint32 FakeCanHw_ReadCount;
extern uint32 FakeCanHw_InitMaskCount;
extern uint32 FakeCanHw_InitFilterCount;
extern uint32 FakeCanHw_SetModeCount;
extern uint32 FakeCanHw_CheckReceiveCount;
extern uint32 FakeCanHw_IsBusOffCount;
extern uint32 FakeCanHw_GetErrorStateCount;
extern uint32 FakeCanHw_IsWakeupPendingCount;
extern uint32 FakeCanHw_AttachRxIsrCount;

/** 直近の Can_Hw_Send() の引数（送信内容の検証用）。 */
extern uint32_t FakeCanHw_LastSendId;
extern uint8_t  FakeCanHw_LastSendDlc;
extern uint8_t  FakeCanHw_LastSendData[8];

/** 直近の Can_Hw_SetMode() の引数。 */
extern Can_Hw_Mode FakeCanHw_LastMode;

/** Can_Hw_AttachRxIsr() に渡された ISR 関数ポインタ（テストから直接呼び出し、
 *  真の割り込みを模擬するために使う）。 */
extern void (*FakeCanHw_AttachedIsr)(void);

/**
 * \brief   受信フレームの模擬キュー件数。
 * \details Can_MainFunction_Read() は `while (Can_Hw_CheckReceive() == CAN_HW_OK)`
 *          でドレインし続けるため、CheckReceive の戻り値を固定フラグにすると
 *          テストが無限ループする。本フェイクは MCP2515 の実際の挙動と同じく
 *          「件数が尽きるまで OK」を模擬する: CheckReceive はこの値が 0 より
 *          大きい間 CAN_HW_OK を返す（減算しない）。Read は 1 件消費して
 *          （FakeCanHw_RxId/RxDlc/RxData の内容を返しつつ）この値を 1 減らす。
 *          テストではこの値を「受信させたいフレーム数」として設定する
 *          （既定 0 = 受信フレームなし）。
 */
extern uint32_t FakeCanHw_RxPendingCount;

/** Can_Hw_Read() が返すフレーム内容（受信フレームのシミュレート用、全件共通）。 */
extern uint32_t FakeCanHw_RxId;
extern uint8_t  FakeCanHw_RxDlc;
extern uint8_t  FakeCanHw_RxData[8];

/** 各関数の戻り値（既定 CAN_HW_OK）。失敗系のテストで CAN_HW_FAIL に変更する。
 *  CheckReceive/Read は上記 FakeCanHw_RxPendingCount で制御するため対象外。 */
extern Can_Hw_ReturnType FakeCanHw_InitReturn;
extern Can_Hw_ReturnType FakeCanHw_SendReturn;
extern Can_Hw_ReturnType FakeCanHw_IsBusOffReturn;
extern Can_Hw_ReturnType FakeCanHw_IsWakeupPendingReturn;

/** Can_Hw_GetErrorState() が *errorStateOut へ書き込む値（既定 0 = Active）と
 *  戻り値（既定 CAN_HW_OK）。 */
extern uint8_t            FakeCanHw_ErrorState;
extern Can_Hw_ReturnType  FakeCanHw_GetErrorStateReturn;

/** 各テストケースの開始時に呼び、記録・戻り値設定をすべて初期状態に戻す。 */
void FakeCanHw_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_CAN_HW_FAKE_H */

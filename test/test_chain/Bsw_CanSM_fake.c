/**
 * \file    Bsw_CanSM_fake.c
 * \brief   CanSM.h（CanIf.c が include）が要求する3関数の no-op スタブ。
 * \details Com→PduR→CanIf→Can のコールチェーンテスト（README「Tx処理」
 *          「Rx処理」節）は CanIf.c を実体としてリンクする。CanIf.c は
 *          CanSM_RxIndication()/CanSM_ControllerBusOff()/CanSM_ControllerWakeup()
 *          をハードコードで呼ぶため（Com/PduR とは異なり関数ポインタ経由ではない）、
 *          CanSM.c 自体をリンクしなくてもシンボルだけは解決できるよう、
 *          このファイルで何もしないスタブを提供する。CanSM 自身のロジック
 *          （Bus-Off 回復・ウェイクアップ検証、README「ECU管理層」節）は
 *          このコールチェーンテストの対象外（別の call chain のため）。
 */
#include "CanSM.h"

void CanSM_RxIndication(uint8 ControllerId)
{
    (void)ControllerId;
}

void CanSM_ControllerBusOff(uint8 ControllerId)
{
    (void)ControllerId;
}

void CanSM_ControllerWakeup(uint8 ControllerId)
{
    (void)ControllerId;
}

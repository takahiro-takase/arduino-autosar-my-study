/**
 * \file    Com_fake.c
 * \brief   Dcm_Cbk.c の Com 依存（CommunicationControl、SID 0x28 相当）を
 *          満たすだけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 */
#include "Com.h"

void Com_SetCommunicationEnabled(uint8 RxEnabled, uint8 TxEnabled)
{
    (void)RxEnabled;
    (void)TxEnabled;
}

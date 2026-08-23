/**
 * \file    Nm_fake.c
 * \brief   Dcm_Cbk.c の Nm 依存（CommunicationControl 経由の Tx 抑制）を
 *          満たすだけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 */
#include "Nm.h"

void Nm_SetTxEnabled(uint8 Enabled)
{
    (void)Enabled;
}

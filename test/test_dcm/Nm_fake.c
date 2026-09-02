/**
 * \file    Nm_fake.c
 * \brief   Dcm_Cbk.c の Nm 依存（CommunicationControl 経由の Tx 抑制）を
 *          満たすだけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 */
#include "Nm.h"

Std_ReturnType Nm_EnableCommunication(NetworkHandleType Channel)
{
    (void)Channel;
    return E_OK;
}

Std_ReturnType Nm_DisableCommunication(NetworkHandleType Channel)
{
    (void)Channel;
    return E_OK;
}

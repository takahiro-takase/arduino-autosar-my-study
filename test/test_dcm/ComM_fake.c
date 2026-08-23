/**
 * \file    ComM_fake.c
 * \brief   Dcm_Cbk.c の ComM 依存（extendedSession 中の通信維持要求）を
 *          満たすだけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 */
#include "ComM.h"

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode)
{
    (void)User;
    (void)ComMode;
    return E_OK;
}

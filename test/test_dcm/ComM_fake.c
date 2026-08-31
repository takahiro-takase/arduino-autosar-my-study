/**
 * \file    ComM_fake.c
 * \brief   Dcm_Cbk.c の ComM 依存（extendedSession 中の通信維持要求。
 *          ComM_DCM_ActiveDiagnostic/InactiveDiagnostic 経由、旧 COMM_USER_1
 *          方式から2026-08のシグネチャ準拠サーベイで置き換え済み）を
 *          満たすだけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 */
#include "ComM.h"

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode)
{
    (void)User;
    (void)ComMode;
    return E_OK;
}

void ComM_DCM_ActiveDiagnostic(uint8 Channel)
{
    (void)Channel;
}

void ComM_DCM_InactiveDiagnostic(uint8 Channel)
{
    (void)Channel;
}

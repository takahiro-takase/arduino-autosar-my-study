/**
 * \file    Bsw_EcuM_fake.c
 * \brief   EcuM.h のテスト用スパイ実装
 * \details Bsw_EcuM_fake.h 冒頭のコメント参照。
 */
#include "Bsw_EcuM_fake.h"

uint32 FakeEcuM_RequestRUNCount = 0U;
uint32 FakeEcuM_ReleaseRUNCount = 0U;

EcuM_UserType FakeEcuM_LastRequestUser = 0xFFU;
EcuM_UserType FakeEcuM_LastReleaseUser = 0xFFU;

void FakeEcuM_Reset(void)
{
    FakeEcuM_RequestRUNCount  = 0U;
    FakeEcuM_ReleaseRUNCount  = 0U;
    FakeEcuM_LastRequestUser  = 0xFFU;
    FakeEcuM_LastReleaseUser  = 0xFFU;
}

Std_ReturnType EcuM_RequestRUN(EcuM_UserType user)
{
    FakeEcuM_RequestRUNCount++;
    FakeEcuM_LastRequestUser = user;
    return E_OK;
}

Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user)
{
    FakeEcuM_ReleaseRUNCount++;
    FakeEcuM_LastReleaseUser = user;
    return E_OK;
}

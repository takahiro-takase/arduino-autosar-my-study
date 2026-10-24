/**
 * \file    NvM_TestUtil.h
 * \brief   NvM_Init() の起動時診断（NVM_INTEGRITY_FAILED / NVM_LOSS_OF_REDUNDANCY）のテスト共通の補助関数
 * \details 空の EEPROM（全 0xFF）で NvM_Init() を呼ぶと、全ブロックを復元して正しい CRC 付きで
 *          書き戻す。その有効な EEPROM の一部のバイトを壊してから、もう一度 NvM_Init() と最初の
 *          NvM_MainFunction()（起動時診断の Dem への報告）を呼び、報告内容を確認する。
 */
#ifndef NVM_TESTUTIL_H
#define NVM_TESTUTIL_H

#include "NvM.h"
#include "NvM_Cfg.h"
#include "MemIf.h"
#include "Fee_Hw.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 空の EEPROM から NvM を初期化する（Dcm_Init() が SecurityAccess の失敗回数を NvM から復元するため、Dcm を使うテストの SetUp() で呼ぶ）。 */
static inline void NvMTest_ColdBoot(void)
{
    MemIf_Init();
    NvM_Init(NULL);
}

/** NvM を未初期化へ戻す（NvM を初期化したテストの TearDown() で呼び、次のテストへ状態を残さない）。 */
static inline void NvMTest_Shutdown(void)
{
    NvM_Test_ResetInitState();
}

/** 全ブロックが有効な EEPROM を用意する（空の EEPROM への NvM_Init() が復元して書き戻す）。 */
static inline void NvMTest_PrepareValidEeprom(void)
{
    MemIf_Init();
    NvM_Init(NULL);
}

/** EEPROM の指定アドレスの 1 バイトを反転して壊す（CRC の不一致を起こす）。 */
static inline void NvMTest_CorruptByte(uint16 address)
{
    uint8 value = 0U;
    Fee_Hw_ReadBlock(&value, address, 1U);
    Fee_Hw_WriteByte(address, (uint8)(value ^ 0xFFU));
}

/** 壊した EEPROM から NvM_Init() をやり直し、最初の NvM_MainFunction() で起動時診断を Dem へ報告させる。 */
static inline void NvMTest_ReinitAndReport(void)
{
    NvM_Test_ResetInitState();
    NvM_Init(NULL);
    WrapDem_Reset();
    NvM_MainFunction();
}

#ifdef __cplusplus
}
#endif

#endif /* NVM_TESTUTIL_H */

/**
 * \file    Hal_Det_Hw_fake.c
 * \brief   Det_Hw.h（Arduino Serial 出力）のテスト用スパイ実装
 * \details Hal_Det_Hw_fake.h 冒頭のコメント参照。
 */
#include "Hal_Det_Hw_fake.h"
#include "Det_Hw.h"

uint16 FakeDetHw_LastModuleId = 0xFFFFU;
uint8  FakeDetHw_LastApiId    = 0xFFU;
uint8  FakeDetHw_LastErrorId  = 0xFFU;
uint32 FakeDetHw_ReportCount  = 0U;

void FakeDetHw_Reset(void)
{
    FakeDetHw_LastModuleId = 0xFFFFU;
    FakeDetHw_LastApiId    = 0xFFU;
    FakeDetHw_LastErrorId  = 0xFFU;
    FakeDetHw_ReportCount  = 0U;
}

void Det_Hw_PrintLogLine(LogLevel lvl, const char* tag, const char* msg)
{
    /* テスト実行のノイズになるため標準出力へは書かない。DET_LOGx はここへ
     * 到達するだけで、内容の検証は Det_Hw_PrintDetError() 側のスパイで行う。 */
    (void)lvl;
    (void)tag;
    (void)msg;
}

void Det_Hw_PrintDetError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)InstanceId;
    FakeDetHw_LastModuleId = ModuleId;
    FakeDetHw_LastApiId    = ApiId;
    FakeDetHw_LastErrorId  = ErrorId;
    FakeDetHw_ReportCount++;
}

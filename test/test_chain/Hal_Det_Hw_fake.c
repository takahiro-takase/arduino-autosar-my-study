/**
 * \file    Hal_Det_Hw_fake.c
 * \brief   Det_Hw.h（Arduino Serial 出力）のテスト用スパイ実装
 * \details Hal_Det_Hw_fake.h 冒頭のコメント参照。
 *
 *          既定では標準出力へ書かない（テスト実行のノイズになるため）。
 *          DET_LOG_VERBOSE 環境変数を 0 以外に設定して実行した場合のみ、
 *          実 Det_Hw.cpp と同じ書式でログを標準出力へ出す（コールチェーンの
 *          途中で実際にどの関数が呼ばれたかをテスト実行結果から確認する用途）。
 *          値の検証自体は引き続き Det_Hw_PrintDetError() 側のスパイで行う。
 */
#include "Hal_Det_Hw_fake.h"
#include "Det_Hw.h"
#include "Hal_Millis_fake.h"
#include <stdio.h>
#include <stdlib.h>

uint16 FakeDetHw_LastModuleId  = 0xFFFFU;
uint8  FakeDetHw_LastApiId     = 0xFFU;
uint8  FakeDetHw_LastErrorId   = 0xFFU;
uint32 FakeDetHw_ReportCount   = 0U;
uint8  FakeDetHw_LogSuppressed = 1U;

static int FakeDetHw_IsVerbose(void)
{
    if (FakeDetHw_LogSuppressed) return 0;
    const char* v = getenv("DET_LOG_VERBOSE");
    return (v != NULL) && (v[0] != '\0') && (v[0] != '0');
}

static const char* FakeDetHw_LevelName(LogLevel lvl)
{
    switch (lvl)
    {
        case LOG_E: return "ERROR";
        case LOG_W: return "WARN ";
        case LOG_I: return "INFO ";
        case LOG_T: return "TRACE";
        default:    return "DEBUG";
    }
}

void FakeDetHw_Reset(void)
{
    FakeDetHw_LastModuleId  = 0xFFFFU;
    FakeDetHw_LastApiId     = 0xFFU;
    FakeDetHw_LastErrorId   = 0xFFU;
    FakeDetHw_ReportCount   = 0U;
    FakeDetHw_LogSuppressed = 1U;
}

void Det_Hw_PrintLogLine(LogLevel lvl, const char* tag, const char* func, const char* msg)
{
    if (FakeDetHw_IsVerbose())
    {
        printf("[%lums] %s %s/%s: %s\n", FakeMillis_Value, FakeDetHw_LevelName(lvl), tag, func, msg);
    }
}

void Det_Hw_PrintDetError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)InstanceId;
    FakeDetHw_LastModuleId = ModuleId;
    FakeDetHw_LastApiId    = ApiId;
    FakeDetHw_LastErrorId  = ErrorId;
    FakeDetHw_ReportCount++;

    if (FakeDetHw_IsVerbose())
    {
        printf("[%lums] DET M=%u I=%u API=0x%02X ERR=0x%02X\n",
               FakeMillis_Value, (unsigned)ModuleId, (unsigned)InstanceId,
               (unsigned)ApiId, (unsigned)ErrorId);
    }
}

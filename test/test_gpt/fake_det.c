/**
 * \file    fake_det.c
 * \brief   Det.h（DET_LOGx / Det_ReportError）のテスト用スパイ実装
 * \details fake_det.h 冒頭のコメント参照。
 */
#include "fake_det.h"
#include "Det.h"

uint16 FakeDet_LastModuleId = 0xFFFFU;
uint8  FakeDet_LastApiId    = 0xFFU;
uint8  FakeDet_LastErrorId  = 0xFFU;
uint32 FakeDet_ReportCount  = 0U;

void FakeDet_Reset(void)
{
    FakeDet_LastModuleId = 0xFFFFU;
    FakeDet_LastApiId    = 0xFFU;
    FakeDet_LastErrorId  = 0xFFU;
    FakeDet_ReportCount  = 0U;
}

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)InstanceId;
    FakeDet_LastModuleId = ModuleId;
    FakeDet_LastApiId    = ApiId;
    FakeDet_LastErrorId  = ErrorId;
    FakeDet_ReportCount++;
    return E_OK;
}

void Log_Write(LogLevel lvl, PGM_P tag_P, PGM_P fmt_P, ...)
{
    /* テスト実行のノイズになるため標準出力へは書かない。DET_LOGx はここへ
     * 到達するだけで、内容の検証は Det_ReportError() 側のスパイで行う。 */
    (void)lvl;
    (void)tag_P;
    (void)fmt_P;
}

void Log_HexStr(char* dst, uint8_t dstSize, const uint8_t* src, uint8_t srcLen)
{
    (void)src;
    (void)srcLen;
    if ((dst != NULL) && (dstSize > 0U))
    {
        dst[0] = '\0';
    }
}

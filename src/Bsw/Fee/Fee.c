/**
 * \file    Fee.c
 * \brief   Flash EEPROM Emulation 実装 (AUTOSAR SWS_Fee 準拠)
 * \details 実際のフラッシュエミュレーション EEPROM (Renesas RA EEPROM.h) への
 *          バイト/ブロックアクセスは Fee_Hw 層に委譲し、本ファイルは
 *          MCU 固有のヘッダを直接知らない (NvM.c が NvM_Hw 経由でのみ
 *          EEPROM にアクセスしていた旧構成と同じ境界の引き方)。
 *
 *          非同期書き込みジョブ:
 *            Fee_Write() はジョブ記述 (アドレス・データポインタ・長さ・進捗)
 *            を記録するだけで即座に返る。Fee_MainFunction() が呼ばれるたびに
 *            未書き込みの 1 バイトだけを Fee_Hw_WriteByte() で書く。
 *            ジョブは同時に 1 個のみ（優先度・複数ジョブの並行処理なし、
 *            学習用簡略化）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Fee.h"
#include "Fee_Hw.h"
#include "Det.h"

#define TAG "Fee"

static uint8 Fee_Initialized = 0U;

/** 処理中の非同期書き込みジョブ。DataPtr はジョブ完了まで呼び出し元
 *  (NvM.c) が寿命を保証する（Fee.h の Fee_Write() 説明参照）。 */
typedef struct
{
    uint16       Address;
    const uint8* DataPtr;
    uint16       Length;
    uint16       Pos;
    uint8        Active;
} Fee_JobType;

static Fee_JobType Fee_Job;

/** 直近のジョブ結果。Fee_Write() 受付時に MEMIF_JOB_PENDING、
 *  Fee_MainFunction() が最終バイトを書いた時点で MEMIF_JOB_OK、
 *  Fee_Cancel() で MEMIF_JOB_CANCELED になる。 */
static MemIf_JobResultType Fee_LastResult = MEMIF_JOB_OK;

void Fee_Init(void)
{
    Fee_Job.Active = 0U;
    Fee_LastResult = MEMIF_JOB_OK;
    Fee_Initialized = 1U;
    DET_LOGI(TAG, "Init ok");
}

void Fee_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_GET_VERSION_INFO, FEE_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = FEE_VENDOR_ID;
    versioninfo->moduleID         = FEE_MODULE_ID;
    versioninfo->sw_major_version = FEE_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = FEE_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = FEE_SW_PATCH_VERSION;
}

Std_ReturnType Fee_Read(uint16 Address, uint8* DataBufferPtr, uint16 Length)
{
    if (!Fee_Initialized)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_READ, FEE_E_UNINIT);
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_READ, FEE_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (Length == 0U)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_READ, FEE_E_INVALID_BLOCK_LEN);
        return E_NOT_OK;
    }

    Fee_Hw_ReadBlock(DataBufferPtr, Address, Length);
    return E_OK;
}

Std_ReturnType Fee_Write(uint16 Address, const uint8* DataBufferPtr, uint16 Length)
{
    if (!Fee_Initialized)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE, FEE_E_UNINIT);
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE, FEE_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (Length == 0U)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE, FEE_E_INVALID_BLOCK_LEN);
        return E_NOT_OK;
    }
    if (Fee_Job.Active)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE, FEE_E_BUSY);
        return E_NOT_OK;
    }

    Fee_Job.Address = Address;
    Fee_Job.DataPtr = DataBufferPtr;
    Fee_Job.Length  = Length;
    Fee_Job.Pos     = 0U;
    Fee_Job.Active  = 1U;
    Fee_LastResult  = MEMIF_JOB_PENDING;
    return E_OK;
}

Std_ReturnType Fee_WriteImmediate(uint16 Address, const uint8* DataBufferPtr, uint16 Length)
{
    if (!Fee_Initialized)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE_IMMEDIATE, FEE_E_UNINIT);
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE_IMMEDIATE, FEE_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (Length == 0U)
    {
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE_IMMEDIATE, FEE_E_INVALID_BLOCK_LEN);
        return E_NOT_OK;
    }
    if (Fee_Job.Active)
    {
        /* Fee_Write() の非同期ジョブと物理アドレス空間が重なりうるため、
         * Fee_Write() と同じ排他を取る（Fee.h の Fee_WriteImmediate() 説明
         * 参照）。呼び出し元は「Os スケジューラ開始前のみ」という運用規約に
         * 加え、この戻り値でも保護される。 */
        Det_ReportError(FEE_MODULE_ID, 0U, FEE_API_ID_WRITE_IMMEDIATE, FEE_E_BUSY);
        return E_NOT_OK;
    }

    Fee_Hw_WriteBlock(DataBufferPtr, Address, Length);
    return E_OK;
}

Std_ReturnType Fee_Cancel(void)
{
    Fee_Job.Active = 0U;
    Fee_LastResult = MEMIF_JOB_CANCELED;
    return E_OK;
}

MemIf_StatusType Fee_GetStatus(void)
{
    if (!Fee_Initialized)
        return MEMIF_UNINIT;
    return Fee_Job.Active ? MEMIF_BUSY : MEMIF_IDLE;
}

MemIf_JobResultType Fee_GetJobResult(void)
{
    return Fee_LastResult;
}

void Fee_MainFunction(void)
{
    if (!Fee_Initialized || !Fee_Job.Active)
        return;

    Fee_Hw_WriteByte((uint16)(Fee_Job.Address + Fee_Job.Pos), Fee_Job.DataPtr[Fee_Job.Pos]);
    Fee_Job.Pos++;

    if (Fee_Job.Pos >= Fee_Job.Length)
    {
        Fee_Job.Active = 0U;
        Fee_LastResult = MEMIF_JOB_OK;
    }
}

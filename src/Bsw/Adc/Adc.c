/**
 * \file    Adc.c
 * \brief   ADC ドライバ 実装 (AUTOSAR SWS_ADC 準拠)
 * \details 公開 API（Adc_SetupResultBuffer / Adc_StartGroupConversion / Adc_ReadGroup /
 *          Adc_GetGroupStatus）を実装する。
 *          Arduino API への直接依存を避けるため、ハードウェアアクセスは
 *          Adc_Hw_ReadChannel() へ委譲する（依存逆転）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Adc.h"
#include "Adc_Hw.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Adc"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static uint8                    Adc_Initialized = 0U;
static Adc_StatusType           Adc_GroupStatus[ADC_GROUP_COUNT];
static const Adc_ValueGroupType* Adc_ResultBuffer[ADC_GROUP_COUNT];  /* Adc_SetupResultBuffer() の登録（登録済みかの確認用） */
static Adc_ValueGroupType       Adc_GroupResult[ADC_GROUP_COUNT];    /* 直近に完了した変換の結果 */

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Adc_Init
 * ---------------------------------------------------------------------- */

void Adc_Init(const Adc_ConfigType* ConfigPtr)
{
    (void)ConfigPtr; /* 本プロジェクトは post-build 設定を持たない（Adc.h 参照） */

    for (uint8 g = 0U; g < ADC_GROUP_COUNT; g++)
    {
        Adc_GroupStatus[g]  = ADC_IDLE;
        Adc_ResultBuffer[g] = NULL;
        Adc_GroupResult[g]  = 0U;
    }
    Adc_Initialized = 1U;

    DET_LOGI(TAG, "Init ok");
}

/* ----------------------------------------------------------------------
 * Adc_GetVersionInfo
 * ---------------------------------------------------------------------- */

void Adc_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_GET_VERSION_INFO, ADC_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = ADC_VENDOR_ID;
    versioninfo->moduleID         = ADC_MODULE_ID;
    versioninfo->sw_major_version = ADC_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = ADC_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = ADC_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * Adc_SetupResultBuffer
 * ---------------------------------------------------------------------- */

/**
 * \brief   グループの結果バッファを登録する（変換を始める前に必要）。
 *
 * \details [SWS_Adc_00420]/[SWS_Adc_00421]: 登録後でなければ、そのグループの変換は始められない
 *          （Adc_StartGroupConversion() が ADC_E_BUFFER_UNINIT を報告する）。
 *          グループが ADC_IDLE でないときは、実行時エラー ADC_E_BUSY を報告して何もしない
 *          （[SWS_Adc_00433]）。
 *
 * \note    本プロジェクトは 1 グループ・1 チャネル・ストリーミングなしのため、変換結果は Adc.c 内部の
 *          領域に保持し、Adc_ReadGroup() が呼び出し元の DataBufferPtr へコピーして返す。
 *          ここで登録したポインタは、登録済みかどうかの確認だけに使う（引数が const のため、
 *          仕様どおりドライバから書き込まない）。
 *
 * \param[in]  Group          対象の ADC チャネルグループ（ADC_GROUP_*）。
 * \param[in]  DataBufferPtr  結果バッファの先頭アドレス。NULL 禁止（[SWS_Adc_00457]）。
 *
 * \retval  E_OK      登録した。
 * \retval  E_NOT_OK  未初期化・グループが範囲外・ポインタが NULL・グループが ADC_IDLE でない。
 *
 * \AUTOSARReq     {SWS_Adc_00419, SWS_Adc_00420, SWS_Adc_00421, SWS_Adc_00423, SWS_Adc_00433, SWS_Adc_00457}
 * \ServiceID      {0x0c}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Adc_SetupResultBuffer(Adc_GroupType Group, const Adc_ValueGroupType* DataBufferPtr)
{
    if (Adc_Initialized == 0U)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_SETUP_RESULT_BUFFER, ADC_E_UNINIT);
        return E_NOT_OK;
    }

    if (Group >= ADC_GROUP_COUNT)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_SETUP_RESULT_BUFFER, ADC_E_PARAM_GROUP);
        return E_NOT_OK;
    }

    if (DataBufferPtr == NULL)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_SETUP_RESULT_BUFFER, ADC_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (Adc_GroupStatus[Group] != ADC_IDLE)
    {
        (void)Det_ReportRuntimeError(ADC_MODULE_ID, 0U, ADC_API_ID_SETUP_RESULT_BUFFER, ADC_E_BUSY);
        return E_NOT_OK;
    }

    Adc_ResultBuffer[Group] = DataBufferPtr;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Adc_DeInit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_StartGroupConversion
 * ---------------------------------------------------------------------- */

/**
 * \brief   グループの変換を開始する。
 *
 * \details [SWS_Adc_00061]: 要求されたグループの全チャネルの変換を開始する。本プロジェクトのグループは、
 *          ソフトウェアトリガ・ワンショット・単一アクセスで、Adc_Hw_ReadChannel()（Arduino の
 *          analogRead()）の呼び出しが同期的に完了するため、戻ったときには状態が
 *          ADC_STREAM_COMPLETED になっている。
 *          エラー時は、何もせず戻る（結果は返さない）。
 *          ・未初期化: ADC_E_UNINIT（[SWS_Adc_00295]）
 *          ・グループが範囲外: ADC_E_PARAM_GROUP（[SWS_Adc_00125]）
 *          ・結果バッファが未登録: ADC_E_BUFFER_UNINIT（[SWS_Adc_00424]）
 *          ・グループが ADC_IDLE でない: 実行時エラー ADC_E_BUSY（[SWS_Adc_00346]）
 *
 * \param[in]  Group  変換を開始するグループ（ADC_GROUP_*）。
 *
 * \AUTOSARReq     {SWS_Adc_00367, SWS_Adc_00061, SWS_Adc_00125, SWS_Adc_00295, SWS_Adc_00346, SWS_Adc_00424}
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void Adc_StartGroupConversion(Adc_GroupType Group)
{
    /* 各グループが変換するチャネル（Adc_GroupType で添字付け。ADC_GROUP_COUNT を増やしたら、ここも増やすこと） */
    static const uint8 groupChannel[ADC_GROUP_COUNT] = { ADC_CHANNEL_SENSOR };

    if (Adc_Initialized == 0U)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_START_GROUP_CONVERSION, ADC_E_UNINIT);
        return;
    }

    if (Group >= ADC_GROUP_COUNT)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_START_GROUP_CONVERSION, ADC_E_PARAM_GROUP);
        return;
    }

    if (Adc_ResultBuffer[Group] == NULL)
    {
        (void)Det_ReportRuntimeError(ADC_MODULE_ID, 0U, ADC_API_ID_START_GROUP_CONVERSION, ADC_E_BUFFER_UNINIT);
        return;
    }

    if (Adc_GroupStatus[Group] != ADC_IDLE)
    {
        (void)Det_ReportRuntimeError(ADC_MODULE_ID, 0U, ADC_API_ID_START_GROUP_CONVERSION, ADC_E_BUSY);
        return;
    }

    Adc_GroupStatus[Group] = ADC_BUSY;
    Adc_GroupResult[Group] = Adc_Hw_ReadChannel(groupChannel[Group]);
    Adc_GroupStatus[Group] = ADC_STREAM_COMPLETED;
}

/* ----------------------------------------------------------------------
 * Adc_StopGroupConversion
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_ReadGroup
 * ---------------------------------------------------------------------- */

/**
 * \brief   グループの直近の変換結果を読み出す。
 *
 * \details [SWS_Adc_00075]: 直近に完了した変換の結果を、DataBufferPtr へ書き込む。
 *          グループが ADC_STREAM_COMPLETED のとき、読み出しで ADC_IDLE へ戻る
 *          （ソフトウェアトリガのワンショットで、変換が自動的に止まるグループ、[SWS_Adc_00330]）。
 *          グループが ADC_IDLE（変換を始めていない、または読み出し済み）のときは、
 *          実行時エラー ADC_E_IDLE（[SWS_Adc_00388]）を報告して E_NOT_OK を返す。
 *          未初期化は ADC_E_UNINIT、グループが範囲外は ADC_E_PARAM_GROUP（[SWS_Adc_00152]）、
 *          ポインタが NULL は ADC_E_PARAM_POINTER を報告する。
 *
 * \param[in]  Group          読み出すグループ（ADC_GROUP_*）。
 * \param[out] DataBufferPtr  変換結果（ADC 生値 0〜1023）の格納先。NULL 禁止。
 *
 * \retval  E_OK      結果を書き込んだ。
 * \retval  E_NOT_OK  結果がない、またはエラー。
 *
 * \AUTOSARReq     {SWS_Adc_00369, SWS_Adc_00075, SWS_Adc_00152, SWS_Adc_00296, SWS_Adc_00330, SWS_Adc_00388}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType* DataBufferPtr)
{
    if (Adc_Initialized == 0U)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_READ_GROUP, ADC_E_UNINIT);
        return E_NOT_OK;
    }

    if (Group >= ADC_GROUP_COUNT)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_READ_GROUP, ADC_E_PARAM_GROUP);
        return E_NOT_OK;
    }

    if (DataBufferPtr == NULL)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_READ_GROUP, ADC_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (Adc_GroupStatus[Group] != ADC_STREAM_COMPLETED)
    {
        (void)Det_ReportRuntimeError(ADC_MODULE_ID, 0U, ADC_API_ID_READ_GROUP, ADC_E_IDLE);
        return E_NOT_OK;
    }

    *DataBufferPtr = Adc_GroupResult[Group];
    Adc_GroupStatus[Group] = ADC_IDLE;   /* [SWS_Adc_00330]: ワンショットは、読み出しで ADC_IDLE へ戻る */
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Adc_EnableHardwareTrigger
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_DisableHardwareTrigger
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_EnableGroupNotification
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_DisableGroupNotification
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_GetGroupStatus
 * ---------------------------------------------------------------------- */

/**
 * \brief   グループの変換の状態を返す。
 *
 * \details [SWS_Adc_00220]/[SWS_Adc_00221]: 変換を始める前、または結果を読み出した後は ADC_IDLE、
 *          変換が完了して結果を読み出す前は ADC_STREAM_COMPLETED を返す。
 *          未初期化（ADC_E_UNINIT）またはグループが範囲外（ADC_E_PARAM_GROUP）のときは、
 *          エラーを報告して ADC_IDLE を返す。
 *
 * \param[in]  Group  状態を知りたいグループ（ADC_GROUP_*）。
 *
 * \return  グループの変換の状態（Adc_StatusType）。
 *
 * \AUTOSARReq     {SWS_Adc_00374, SWS_Adc_00220, SWS_Adc_00221}
 * \ServiceID      {0x09}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group)
{
    if (Adc_Initialized == 0U)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_GET_GROUP_STATUS, ADC_E_UNINIT);
        return ADC_IDLE;
    }

    if (Group >= ADC_GROUP_COUNT)
    {
        (void)Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_GET_GROUP_STATUS, ADC_E_PARAM_GROUP);
        return ADC_IDLE;
    }

    return Adc_GroupStatus[Group];
}

/* ----------------------------------------------------------------------
 * Adc_GetStreamLastPointer
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_SetPowerState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_GetCurrentPowerState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_GetTargetPowerState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_PreparePowerState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Adc_Main_PowerTransitionManager
 * ---------------------------------------------------------------------- */

/* 未実装 */

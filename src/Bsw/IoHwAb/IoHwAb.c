/**
 * \file    IoHwAb.c
 * \brief   I/O ハードウェア抽象化層 実装 (AUTOSAR IoHwAb 準拠)
 * \details MCAL Dio モジュールをラップし、上位層（RTE 経由 SW-C）に
 *          ハードウェア非依存の I/O インタフェースを提供する。
 *          本プロジェクトでは警告灯 LED (DIO_CHANNEL_LED_WARNING) のみを管理する。
 *
 *          本ファイルが Dio.h / Dio_Cfg.h を参照する唯一の非 MCAL ファイルである。
 *          SW-C は本層を直接インクルードせず、RTE C/S ポートを通じてのみアクセスする。
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

#include "IoHwAb.h"
#include "IoHwAb_Cfg.h"
#include "Dio.h"
#include "Dio_Cfg.h"
#include "Adc.h"
#include "Adc_Cfg.h"
#include "Det.h"
#include "Dem.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "IoHwAb"

/* ModuleId・エラーコード・ApiId・バージョン情報（IOHWAB_MODULE_ID 等）は IoHwAb_Cfg.h で定義する。 */
/* ボタンのデバウンス・固着判定のサンプル数と、ADC 電圧低下の閾値（IOHWAB_BUTTON_DEBOUNCE_COUNT 等）も
 * IoHwAb_Cfg.h（config/data/IoHwAb.json から生成）で定義する。 */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static uint8  s_confirmedLevel  = 0U;  /* デバウンス確定値 (0=解放, 1=押下) */
static uint8  s_debounceCounter = 0U;  /* 未確定サンプル積算カウンタ */
static uint16 s_stuckCounter    = 0U;  /* 確定押下継続サンプル数 */
static uint16 s_adcMv           = 0U;  /* ADC 変換済み電圧値 [mV] */

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * IoHwAb_Init
 * ---------------------------------------------------------------------- */

/**
 * \brief   IoHwAb モジュールを初期化する。
 *
 * \details 警告灯 LED を消灯状態で起動する。
 *          ピン方向設定は Port_Init() が担うため、本関数では行わない。
 *          チャネル番号 (DIO_CHANNEL_LED_WARNING) は Dio_Cfg.h で一元管理し、
 *          上位層には公開しない。
 *
 * \pre        Port_Init() が正常完了していること。
 *
 * \ServiceID      {0x01}（[SWS_IoHwAb_00119] の Service ID[hex] と一致。
 *                 他の I/O 個別関数 (Led_SetLevel 等) は仕様に対応する
 *                 API 定義自体が無いため、0xC1〜0xC6 を本プロジェクト
 *                 独自に割り当てている）
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void IoHwAb_Init(const IoHwAb_ConfigType* ConfigPtr)
{
    static Adc_ValueGroupType adcResultBuffer = 0U;  /* ADC グループの結果バッファ（Adc_SetupResultBuffer() へ登録） */

    (void)ConfigPtr; /* 本プロジェクトは post-build 設定を持たない（IoHwAb.h 参照） */
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, DIO_LOW);  /* 消灯状態で起動 */
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT,   DIO_LOW);  /* 消灯状態で起動 */
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING,  DIO_LOW);  /* 消灯状態で起動 */
    s_confirmedLevel  = 0U;
    s_debounceCounter = 0U;
    s_stuckCounter    = 0U;
    s_adcMv           = 0U;
    if (Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &adcResultBuffer) != E_OK)  /* Adc_Init() の後であること（EcuM_Init() の呼び出し順） */
    {
        DET_LOGE(TAG, "Init E: Adc_SetupResultBuffer failed (ADC conversion unavailable)");
    }
    DET_LOGI(TAG, "Init");
}

/* ----------------------------------------------------------------------
 * IoHwAb_GetVersionInfo
 * ---------------------------------------------------------------------- */

/**
 * \brief   IoHwAb モジュールのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL を渡すと
 *                           IOHWAB_E_PARAM_POINTER を DET へ報告し、何も書かない。
 *
 * \AUTOSARReq     {SWS_IoHwAb_00120}
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void IoHwAb_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(IOHWAB_MODULE_ID, 0U, IOHWAB_API_ID_GET_VERSION_INFO, IOHWAB_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = IOHWAB_VENDOR_ID;
    versioninfo->moduleID         = IOHWAB_MODULE_ID;
    versioninfo->sw_major_version = IOHWAB_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = IOHWAB_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = IOHWAB_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * IoHwAb_Led_SetLevel
 * ---------------------------------------------------------------------- */

/**
 * \brief   警告灯 LED の出力レベルを設定する。
 *
 * \details Dio_WriteChannel() へ委譲する。チャネル ID は本関数内で解決し、
 *          呼び出し元（RTE / SW-C）にピン番号を公開しない。
 *
 * \param[in]  level  出力レベル。0 = 消灯 (LOW)、1 = 点灯 (HIGH)。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Led_SetLevel(uint8 level)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, (Dio_LevelType)level);
    return E_OK;
}

/* ----------------------------------------------------------------------
 * IoHwAb_LedRunning_SetLevel
 * ---------------------------------------------------------------------- */

/**
 * \brief   RUNNING LED (D6) の出力レベルを設定する。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC3}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_LedRunning_SetLevel(uint8 level)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, (Dio_LevelType)level);
    return E_OK;
}

/* ----------------------------------------------------------------------
 * IoHwAb_LedFault_SetLevel
 * ---------------------------------------------------------------------- */

/**
 * \brief   FAULT LED (D7) の出力レベルを設定する。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_LedFault_SetLevel(uint8 level)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT, (Dio_LevelType)level);
    return E_OK;
}

/* ----------------------------------------------------------------------
 * IoHwAb_MainFunction
 * ---------------------------------------------------------------------- */

/**
 * \brief   ボタンのデバウンス処理を実行する周期関数。
 *
 * \details 積分カウンタ方式:
 *          生レベルが確定値と異なれば s_debounceCounter をインクリメントし、
 *          IOHWAB_BUTTON_DEBOUNCE_COUNT に達した時点で確定値を更新する。
 *          生レベルが確定値と一致すればカウンタをリセットする。
 *          Dio_ReadChannel の呼び出しは本関数に集約し、
 *          IoHwAb_Button_GetLevel は確定値を返すだけにする。
 *
 * \ServiceID      {0xC5}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void IoHwAb_MainFunction(void)
{
    /* INPUT_PULLUP: LOW = 押下（GND接続）、HIGH = 解放（プルアップ電位）*/
    const uint8 rawLevel = (Dio_ReadChannel(DIO_CHANNEL_BUTTON) == DIO_LOW) ? 1U : 0U;

    if (rawLevel == s_confirmedLevel)
    {
        s_debounceCounter = 0U;  /* 安定、カウンタリセット */
    }
    else
    {
        s_debounceCounter++;
        if (s_debounceCounter >= IOHWAB_BUTTON_DEBOUNCE_COUNT)
        {
            s_confirmedLevel  = rawLevel;
            s_debounceCounter = 0U;
            DET_LOGI(TAG, "Button confirmed level=%u", (unsigned)s_confirmedLevel);
        }
    }

    /* ボタン固着検出 (デバウンス確定値を使用) */
    if (s_confirmedLevel == 1U)
    {
        if (s_stuckCounter < IOHWAB_BUTTON_STUCK_COUNT)
        {
            s_stuckCounter++;
            if (s_stuckCounter == IOHWAB_BUTTON_STUCK_COUNT)
            {
                /* 5 秒固着判定済み（十分な持続性チェック）のため、
                 * Dem 側は DEM_DEBOUNCE_LIMIT_BUTTON_STUCK=1 で 1 回の報告で確定する */
                (void)Dem_SetEventStatus(DemConf_DemEventParameter_BUTTON_STUCK, DEM_EVENT_STATUS_FAILED);
                DET_LOGW(TAG, "Button stuck dtc=0x%06lX", (unsigned long)DEM_DTC_BUTTON_STUCK);
            }
        }
    }
    else
    {
        if (s_stuckCounter >= IOHWAB_BUTTON_STUCK_COUNT)
        {
            /* 固着から解放 → PASSED 報告 */
            (void)Dem_SetEventStatus(DemConf_DemEventParameter_BUTTON_STUCK, DEM_EVENT_STATUS_PASSED);
            DET_LOGI(TAG, "Button stuck cleared");
        }
        s_stuckCounter = 0U;
    }

    /* ADC サンプリング: 生値を mV へ変換し、電圧低下を DEM 報告 */
    {
        Adc_ValueGroupType raw = 0U;
        Adc_StartGroupConversion(ADC_GROUP_SENSOR);
        if (Adc_ReadGroup(ADC_GROUP_SENSOR, &raw) == E_OK)
        {
            s_adcMv = (uint16)((uint32)raw * ADC_REF_VOLTAGE_MV / ADC_RESOLUTION_MAX);
            if (s_adcMv < IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV)
            {
                (void)Dem_SetEventStatus(DemConf_DemEventParameter_ADC_VOLT_LOW, DEM_EVENT_STATUS_FAILED);
            }
            else
            {
                (void)Dem_SetEventStatus(DemConf_DemEventParameter_ADC_VOLT_LOW, DEM_EVENT_STATUS_PASSED);
            }
        }
        /* else: 変換できなかった（結果なし）。0 V の測定値と誤認して電圧低下を報告しないよう、
         * 前回値のまま、Dem へは報告しない（原因は Adc 側が DET へ報告済み）。 */
    }
}

/* ----------------------------------------------------------------------
 * IoHwAb_Button_GetLevel
 * ---------------------------------------------------------------------- */

/**
 * \brief   警告確認ボタンの押下状態を取得する。
 *
 * \details IoHwAb_MainFunction が確定したデバウンス済み状態を返す。
 *          Dio_ReadChannel は呼び出さず、静的変数 s_confirmedLevel を参照するだけ。
 *
 * \param[out] level  押下状態 (0=解放, 1=押下)。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Button_GetLevel(uint8* level)
{
    if (level == NULL)
    {
        (void)Det_ReportError(IOHWAB_MODULE_ID, 0U, IOHWAB_API_ID_BUTTON_GET_LEVEL, IOHWAB_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    *level = s_confirmedLevel;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * IoHwAb_Adc_GetValue_mV
 * ---------------------------------------------------------------------- */

/**
 * \brief   ADC センサ電圧値を取得する。
 *
 * \details IoHwAb_MainFunction が計算した最新の s_adcMv を返す。
 *          Adc_ReadGroup は呼び出さず、静的変数を参照するだけ。
 *
 * \param[out] mv  変換済み電圧値 [mV]。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC6}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Adc_GetValue_mV(uint16* mv)
{
    if (mv == NULL)
    {
        (void)Det_ReportError(IOHWAB_MODULE_ID, 0U, IOHWAB_API_ID_ADC_GET_VALUE_MV, IOHWAB_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    *mv = s_adcMv;
    return E_OK;
}

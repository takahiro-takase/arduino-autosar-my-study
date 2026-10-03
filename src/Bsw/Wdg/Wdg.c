/**
 * \file    Wdg.c
 * \brief   Watchdog Driver 実装 (AUTOSAR SWS_Wdg 準拠)
 * \details 実際の HW ウォッチドッグ（Renesas RA WDT ライブラリ）への
 *          Enable/Disable/Refresh は Wdg_Hw 層に委譲し、本ファイルは
 *          MCU 固有のヘッダを直接知らない（旧 WdgM_Hw と同じ境界の引き方。
 *          詳細は Wdg_Hw.h 参照）。
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

#include "Wdg.h"
#include "Wdg_Hw.h"
#include "Det.h"
#include "Dem.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Wdg"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static uint8 Wdg_Initialized = 0U;
static uint16 Wdg_ConfiguredTimeoutMs = 0U;

/* 関数をまたいで状態を保持するラッチのため、ブロックスコープ化できない（テスト用リセットからも参照） */
/* cppcheck-suppress misra-c2012-8.9 */
/** Wdg_SetTriggerCondition(0) を受けてトリガを止めたか（ラッチ。[SWS_Wdg_00140]） */
static uint8 Wdg_TriggerStopped = 0U;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Wdg_Init
 * ---------------------------------------------------------------------- */

void Wdg_Init(const Wdg_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init: NULL ConfigPtr");
        (void)Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_INIT, WDG_E_PARAM_POINTER);
        return;
    }

    Wdg_ConfiguredTimeoutMs = ConfigPtr->DefaultTimeoutMs;
    Wdg_TriggerStopped      = 0U;  /* 初期化し直したら、timeout=0 によるトリガ停止（ラッチ）も解除する */
    Wdg_Initialized         = 1U;

    DET_LOGI(TAG, "Init ok timeout=%ums", (unsigned)Wdg_ConfiguredTimeoutMs);
}

/* ----------------------------------------------------------------------
 * Wdg_SetMode
 * ---------------------------------------------------------------------- */

Std_ReturnType Wdg_SetMode(WdgIf_ModeType Mode)
{
    if (!Wdg_Initialized)
    {
        (void)Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_MODE, WDG_E_DRIVER_STATE);
        return E_NOT_OK;
    }

    switch (Mode)
    {
    case WDGIF_FAST_MODE:
        Wdg_Hw_Enable(Wdg_ConfiguredTimeoutMs);
        return E_OK;

    case WDGIF_OFF_MODE:
        /* Renesas RA4M1 の IWDT は一度有効化すると FSP からの無効化手段が
         * ないため（Wdg_Hw.cpp 参照）、この要求は物理的に受理できない
         * ([SWS_Wdg_00026]: モード切替を実行せず WDG_E_DISABLE_REJECTED を
         * 発生させ E_NOT_OK を返す。経緯は Wdg_Cfg.h 冒頭のコメント参照)。
         * 対になる PASSED（[SWS_Wdg_00183]）は、本プロジェクトの Wdg が
         * 無効化に成功する経路を持たないため報告しない。 */
        Wdg_Hw_Disable();
        (void)Dem_SetEventStatus(DEM_EVENT_WDG_DISABLE_REJECTED, DEM_EVENT_STATUS_FAILED);
        DET_LOGW(TAG, "SetMode(OFF) rejected - HW cannot be disabled once armed");
        return E_NOT_OK;

    case WDGIF_SLOW_MODE:
    default:
        (void)Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_MODE, WDG_E_PARAM_MODE);
        return E_NOT_OK;
    }
}

/* ----------------------------------------------------------------------
 * Wdg_SetTriggerCondition
 * ---------------------------------------------------------------------- */

void Wdg_SetTriggerCondition(uint16 timeout)
{
    if (!Wdg_Initialized)
    {
        (void)Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_TRIGGER_CONDITION, WDG_E_DRIVER_STATE);
        return;
    }

    if (timeout > Wdg_ConfiguredTimeoutMs)
    {
        (void)Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_TRIGGER_CONDITION, WDG_E_PARAM_TIMEOUT);
        return;
    }

    /* [SWS_Wdg_00140]: 既に timeout=0 でトリガを止めている場合は何もしない
     * （以降に渡された timeout は無視する）。 */
    if (Wdg_TriggerStopped != 0U)
    {
        return;
    }

    /* [SWS_Wdg_00140]: timeout=0 は「トリガの（ほぼ）即時停止と ECU の
     * （ほぼ）即時リセット」の要求。以前は timeout の値によらずリフレッシュ
     * していたため、0 を渡すと逆にウォッチドッグがリフレッシュされていた。 */
    if (timeout == 0U)
    {
        Wdg_TriggerStopped = 1U;
        DET_LOGE(TAG, "SetTriggerCondition(0): trigger stopped, forcing ECU reset");
        Wdg_Hw_ForceReset();
        return;
    }

    /* 本プロジェクトの HW は API 経由でのタイムアウト窓の動的変更に対応
     * しないため、timeout が 0 以外なら、その値によらずリフレッシュのみ行う
     * （Wdg.h 冒頭のコメント参照）。現在のモードの確認も行わない
     * （WdgM_TriggerHwWatchdog() は WdgM_SupervisionSuppressed 中も含め常に
     * リフレッシュを要求し続ける設計のため、Wdg 側で再度モードを判定する
     * 必要はない）。 */
    Wdg_Hw_Refresh();
}

/* ----------------------------------------------------------------------
 * Wdg_GetVersionInfo
 * ---------------------------------------------------------------------- */

void Wdg_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_GET_VERSION_INFO, WDG_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = WDG_VENDOR_ID;
    versioninfo->moduleID         = WDG_MODULE_ID;
    versioninfo->sw_major_version = WDG_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = WDG_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = WDG_SW_PATCH_VERSION;
}

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef WDG_UNIT_TEST
void Wdg_Test_ResetInitState(void)
{
    Wdg_Initialized         = 0U;
    Wdg_ConfiguredTimeoutMs = 0U;
    Wdg_TriggerStopped      = 0U;
}
#endif

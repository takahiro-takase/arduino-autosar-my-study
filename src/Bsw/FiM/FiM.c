/**
 * \file    FiM.c
 * \brief   機能抑止マネージャ 実装 (AUTOSAR SWS_FiM 準拠)
 * \details Dem が確定した DTC のステータスをもとに、関連するアプリ機能 (FID)
 *          の実行許可を判定する。
 *
 *          判定アルゴリズム:
 *            1. FiM_MainFunction() (100 ms 周期) が FiM_Functions[] を
 *               先頭から走査する。
 *            2. 各行について Dem_GetEventUdsStatus(EventId) を取得し、
 *               InhibitStatusMask とのビット AND が非ゼロなら抑止、
 *               ゼロなら許可と判定する。
 *            3. 判定結果が前回から変化した場合のみログを出力する
 *               (毎サイクルのログ過多を防ぐ、Can_Hw_IsBusOff と同様の方式)。
 *
 *          ASW は Rte_Call_FiM_GetFunctionPermission() 経由で許可状態を取得する。
 *          BSW モジュール同士（FiM → Dem）は RTE を介さず直接呼び出す
 *          （本プロジェクトの確立された層分離: ASW↔BSW 境界のみ RTE が仲介する）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "FiM.h"
#include "Dem.h"
#include "Det.h"

#define TAG "FiM"

/** ポストビルドコンフィグへのポインタ (FiM_Init で設定) */
static const FiM_ConfigType* FiM_Cfg = NULL;

/** FID ごとの現在の許可状態 (1=許可, 0=抑止、Dem イベントステータスに基づく) */
static uint8 FiM_Permitted[FIM_FUNCTION_COUNT];

/** FID ごとの利用可否 (1=利用可能, 0=利用不可、FiM_SetFunctionAvailable() が設定。
 *  FiM.h の FiM_SetFunctionAvailable() Doxygen 参照)。 */
static uint8 FiM_Available[FIM_FUNCTION_COUNT];

/**
 * \brief   単一 FID の許可状態を Dem の現在のイベントステータスから評価する。
 *
 * \details FiM_Init() と FiM_MainFunction() の共通ロジック（2026-09 追加、
 *          自己/simplify 指摘: 両者が同一の判定式を重複して持っていたため
 *          抽出）。InhibitStatusMask のいずれかのビットが立っていれば抑止
 *          (0)、立っていなければ許可 (1) を返す。
 */
static uint8 FiM_EvaluatePermission(const FiM_FunctionCfgType* fn)
{
    Dem_UdsStatusByteType status = 0U;
    (void)Dem_GetEventUdsStatus(fn->EventId, &status);
    return ((status & fn->InhibitStatusMask) != 0U) ? 0U : 1U;
}

/**
 * \brief   FiM モジュールを初期化する。
 *
 * \details 全 FID の許可状態を、Dem の現在のイベントステータス
 *          （NvM 復元済みの、前回起動までの確定 DTC を含む）から直接評価して
 *          初期化する（[SWS_Fim_00102]/[SWS_Fim_00104]、2026-09 是正）。
 *
 *          実仕様は「FiM_Init() → Dem_Init() → (Dem_Init() 内部から)
 *          FiM_DemInit() が Dem_GetMonitorStatus() を全 FID についてループし
 *          最終的な許可状態を確定する」という3段階の初期化シーケンスを規定
 *          するが、本プロジェクトの起動順序は EcuM.c で
 *          「Dem_Init() → FiM_Init()」と逆（Dem 側が先に確定済み）のため、
 *          FiM_DemInit() 相当の別関数を新設せず、FiM_Init() 自身が
 *          （FiM_MainFunction() と同じ）Dem 参照ロジックをその場で使うことで
 *          等価な結果を得ている。
 *
 *          以前は全 FID を無条件で「許可」初期化しており、起動直後から
 *          最初の FiM_MainFunction() 呼び出しまでの間、既に確定済みの DTC が
 *          あっても誤って許可扱いになる乖離があった（本来 FiM_Init() 完了
 *          時点で正しい許可状態が確定しているべき、[SWS_Fim_00104]違反）。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_Init(const FiM_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init: NULL ConfigPtr");
        Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_INIT, FIM_E_PARAM_POINTER);
        return;
    }

    FiM_Cfg = ConfigPtr;

    for (uint8 i = 0U; i < ConfigPtr->FunctionCount; i++)
    {
        const FiM_FunctionCfgType* fn = &ConfigPtr->Functions[i];
        const uint8 permitted = FiM_EvaluatePermission(fn);

        FiM_Permitted[fn->FunctionId] = permitted;
        FiM_Available[fn->FunctionId] = 1U;

        if (permitted == 0U)
        {
            DET_LOGW(TAG, "FID%u inhibited at init (ev=%u)",
                     (unsigned)fn->FunctionId, (unsigned)fn->EventId);
        }
    }

    DET_LOGI(TAG, "Init ok functions=%u", (unsigned)ConfigPtr->FunctionCount);
}

/**
 * \brief   FiM 周期処理。各 FID の許可状態を再評価する。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_MainFunction(void)
{
    DET_LOGT(TAG, "called");
    if (FiM_Cfg == NULL)
        return;

    for (uint8 i = 0U; i < FiM_Cfg->FunctionCount; i++)
    {
        const FiM_FunctionCfgType* fn = &FiM_Cfg->Functions[i];
        const uint8 newPermitted = FiM_EvaluatePermission(fn);

        if (newPermitted != FiM_Permitted[fn->FunctionId])
        {
            FiM_Permitted[fn->FunctionId] = newPermitted;

            if (newPermitted == 0U)
            {
                DET_LOGW(TAG, "FID%u inhibited (ev=%u)",
                         (unsigned)fn->FunctionId, (unsigned)fn->EventId);
            }
            else
            {
                DET_LOGI(TAG, "FID%u permitted again", (unsigned)fn->FunctionId);
            }
        }
    }
}

/**
 * \brief   指定 FID が現在許可されているかを取得する。
 *
 * \AUTOSARReq     {SWS_Fim_00011}
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType FiM_GetFunctionPermission(FiM_FunctionIdType FunctionId, boolean* Permission)
{
    DET_LOGT(TAG, "called");
    if (Permission == NULL)
    {
        Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_FUNCTION_PERMISSION, FIM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (FiM_Cfg == NULL)
    {
        *Permission = FALSE;  /* フェールセーフ: 未初期化中は抑止扱いとする */
        Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_FUNCTION_PERMISSION, FIM_E_UNINIT);
        return E_NOT_OK;
    }

    if (FunctionId >= FiM_Cfg->FunctionCount)
    {
        *Permission = FALSE;  /* フェールセーフ: 不明な FID は抑止扱いとする */
        Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_FUNCTION_PERMISSION, FIM_E_FID_OUT_OF_RANGE);
        return E_NOT_OK;
    }

    /* [SWS_Fim_00105]: Availability=0 (利用不可) の FID は Dem ベースの判定に
     * 関わらず常に抑止扱いとする (FiM_SetFunctionAvailable() の Doxygen 参照)。 */
    *Permission = (FiM_Available[FunctionId] != 0U) ? (FiM_Permitted[FunctionId] != 0U) : FALSE;
    return E_OK;
}

/**
 * \brief   指定 FID の利用可否を外部から強制設定する（[SWS_Fim_00106]）。
 *
 * \details 実仕様は `FiMAvailabilitySupport` が configured=True の場合のみ
 *          有効な任意サービスだが、本プロジェクトはそのようなビルド時
 *          コンフィグ切替を持たないため常に有効とする（学習用簡略化）。
 *
 *          [SWS_Fim_00105]: Availability=0（利用不可）に設定した FID は、
 *          `FiM_MainFunction()` が Dem のイベントステータスから判定する
 *          抑止状態に関わらず、`FiM_GetFunctionPermission()` が常に
 *          「抑止」を返すようになる（本実装では読み出し側
 *          `FiM_GetFunctionPermission()` で両方の条件の論理積を取る形で
 *          実現し、`FiM_MainFunction()` の DTC ベースの判定ロジック自体は
 *          変更しない）。
 *
 * \param[in]  FID           機能 ID (FIM_FID_*)。
 * \param[in]  Availability  TRUE: 利用可能。FALSE: 利用不可（強制抑止）。
 *
 * \retval  E_OK      正常に設定した。
 * \retval  E_NOT_OK  未初期化、または FID が範囲外。
 *
 * \AUTOSARReq     {SWS_Fim_00106, SWS_Fim_00105}
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType FiM_SetFunctionAvailable(FiM_FunctionIdType FID, boolean Availability)
{
    DET_LOGT(TAG, "called");
    if (FiM_Cfg == NULL)
    {
        Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_SET_FUNCTION_AVAILABLE, FIM_E_UNINIT);
        return E_NOT_OK;
    }

    if (FID >= FiM_Cfg->FunctionCount)
    {
        Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_SET_FUNCTION_AVAILABLE, FIM_E_FID_OUT_OF_RANGE);
        return E_NOT_OK;
    }

    FiM_Available[FID] = Availability ? 1U : 0U;

    if (!Availability)
        DET_LOGW(TAG, "FID%u made unavailable (forced)", (unsigned)FID);
    else
        DET_LOGI(TAG, "FID%u made available again", (unsigned)FID);

    return E_OK;
}

void FiM_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_VERSION_INFO, FIM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = FIM_VENDOR_ID;
    versioninfo->moduleID         = FIM_MODULE_ID;
    versioninfo->sw_major_version = FIM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = FIM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = FIM_SW_PATCH_VERSION;
}

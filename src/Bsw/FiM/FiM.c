/**
 * \file    FiM.c
 * \brief   機能抑止マネージャ 実装 (AUTOSAR SWS_FiM 準拠)
 * \details Dem が確定した DTC のステータスをもとに、関連するアプリ機能 (FID)
 *          の実行許可を判定する。
 *
 *          判定アルゴリズム:
 *            1. FiM_MainFunction() (100 ms 周期) が FiM_Functions[]（抑止の条件の配列）を
 *               先頭から走査する。
 *            2. 各行について Dem_GetEventUdsStatus(EventId) を取得し、
 *               InhibitStatusMask とのビット AND が非ゼロなら、その条件は成り立つ。
 *               FID は、成り立つ条件が 1 つでもあれば抑止、1 つも無ければ許可と判定する
 *               （同じ FID の複数の行の OR、[SWS_Fim_00064]）。
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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "FiM.h"
#include "Dem.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "FiM"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/** ポストビルドコンフィグへのポインタ (FiM_Init で設定) */
static const FiM_ConfigType* FiM_Cfg = NULL;

/** FID ごとの現在の許可状態 (1=許可, 0=抑止、Dem イベントステータスに基づく) */
static uint8 FiM_Permitted[FIM_FUNCTION_COUNT];

/** FID ごとの利用可否 (1=利用可能, 0=利用不可、FiM_SetFunctionAvailable() が設定。
 *  FiM.h の FiM_SetFunctionAvailable() Doxygen 参照)。 */
static uint8 FiM_Available[FIM_FUNCTION_COUNT];

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * FiM_EvaluatePermission
 * ---------------------------------------------------------------------- */

/**
 * \brief   設定が持つ機能（FID）の数を返す（内部状態の配列の大きさ FIM_FUNCTION_COUNT を上限とする）。
 *
 * \details 内部状態（FiM_Permitted[] / FiM_Available[]）は FIM_FUNCTION_COUNT 要素なので、設定の
 *          FunctionCount が大きくても、それ以上は扱わない。設定の FunctionCount が小さければ、
 *          その範囲だけを扱う（API の範囲検査 [FiM_GetFunctionPermission] と同じ範囲）。
 *
 * \return  扱う FID の数
 */
static uint8 FiM_FidCount(void)
{
    return (FiM_Cfg->FunctionCount < FIM_FUNCTION_COUNT) ? FiM_Cfg->FunctionCount : (uint8)FIM_FUNCTION_COUNT;
}

/**
 * \brief   抑止の条件 1 行が、Dem のイベントの現在のステータスに対して成り立つかを調べる。
 *
 * \details 条件の種類ごとの判定は、FiM_InhibitionType の説明（FiM_PBCfg.h）と、[SWS_Fim_00012] を参照。
 *          仕様の 4 つの条件（FIM_LAST_FAILED など）は、Dem_GetMonitorStatus() の監視ステータス
 *          （TestFailed、TestNotCompletedThisOperationCycle）で判定する（UDS ステータスの bit6 は、
 *          Dem_GetEventUdsStatus() では隠れて見えないため）。独自拡張の FIM_INH_STATUS_MASK は、
 *          Dem_GetEventUdsStatus() の UDS ステータスで判定する。
 *
 * \param[in]   cond        抑止の条件の行。
 * \param[out]  inhibiting  条件が成り立つとき TRUE（その行の FID は抑止される）。戻り値が E_NOT_OK のときは不定。
 *
 * \retval  E_OK      判定できた。
 * \retval  E_NOT_OK  Dem からステータスを読めなかった、または条件の種類が未知（その行は、計算に含めない。[SWS_Fim_00097]）。
 */
static Std_ReturnType FiM_CheckCondition(const FiM_FunctionCfgType* cond, boolean* inhibiting)
{
    Std_ReturnType ret = E_NOT_OK;

    if (cond->Inhibition == FIM_INH_STATUS_MASK)
    {
        Dem_UdsStatusByteType status = 0U;

        ret = Dem_GetEventUdsStatus(cond->EventId, &status);
        *inhibiting = ((status & cond->InhibitStatusMask) != 0U) ? TRUE : FALSE;
    }
    else
    {
        /* 仕様の 4 つの条件は、監視ステータスのビットが「立っているべき」ものと「立っていてはならない」ものの組で表せる。 */
        Dem_MonitorStatusType mustBeSet   = 0U;
        Dem_MonitorStatusType mustBeClear = 0U;

        switch (cond->Inhibition)
        {
            case FIM_LAST_FAILED:
                mustBeSet = DEM_MONITOR_STATUS_TF;
                break;
            case FIM_NOT_TESTED:
                mustBeSet = DEM_MONITOR_STATUS_TNCTOC;
                break;
            case FIM_TESTED:
                mustBeClear = DEM_MONITOR_STATUS_TNCTOC;
                break;
            case FIM_TESTED_AND_FAILED:
                mustBeSet   = DEM_MONITOR_STATUS_TF;
                mustBeClear = DEM_MONITOR_STATUS_TNCTOC;
                break;
            default:
                break;   /* 未知の種類: 組が空のまま。下で、その行を計算に含めない */
        }

        if ((mustBeSet | mustBeClear) != 0U)
        {
            Dem_MonitorStatusType monitor = 0U;

            ret = Dem_GetMonitorStatus(cond->EventId, &monitor);
            *inhibiting = (((monitor & mustBeSet) == mustBeSet) && ((monitor & mustBeClear) == 0U)) ? TRUE : FALSE;
        }
    }
    return ret;
}

/**
 * \brief   全 FID の許可状態を、Dem の現在のイベントステータスから評価する。
 *
 * \details FiM_Init() と FiM_MainFunction() の共通ロジック（2026-09 追加、
 *          自己/simplify 指摘: 両者が同一の判定式を重複して持っていたため
 *          抽出）。FiM_Functions[] の各行（抑止の条件）について、FiM_CheckCondition() が成り立てば、
 *          その行の FID を抑止 (0) にする。
 *          1 つの FID に複数の行があれば、どれか 1 つでも成り立てば抑止になる（OR、
 *          [SWS_Fim_00064]）。1 つも成り立たない FID は許可 (1)。
 *          Dem_GetEventUdsStatus() が失敗した行は、計算に含めない（[SWS_Fim_00097]。成り立たない条件として扱う）。
 *          ステータスを 0 として判定すると、FIM_TESTED（bit6 が立っていない）が成り立って、誤って抑止するため。
 *          FunctionId が FiM_FidCount() 以上の行は、無視する（配列の範囲外を書かないための防御。通常は、
 *          コンフィギュレータが保証する）。
 *
 * \param[out]  permitted    FID ごとの許可状態の格納先（FIM_FUNCTION_COUNT 要素）。
 * \param[out]  causeEvent   FID ごとの、最初に抑止した行の Dem イベント ID の格納先（ログ用。
 *                          許可の FID は 0。FIM_FUNCTION_COUNT 要素）。
 */
static void FiM_EvaluateAll(uint8* permitted, uint8* causeEvent)
{
    const uint8 fidCount = FiM_FidCount();

    for (uint8 fid = 0U; fid < fidCount; fid++)
    {
        permitted[fid]  = 1U;
        causeEvent[fid] = 0U;
    }

    for (uint8 i = 0U; i < FiM_Cfg->InhibitionCount; i++)
    {
        const FiM_FunctionCfgType* fn = &FiM_Cfg->Functions[i];
        boolean inhibiting = FALSE;

        if ((fn->FunctionId >= fidCount) || (permitted[fn->FunctionId] == 0U))
        {
            continue;   /* 範囲外の行、または、すでに抑止と決まった FID の行は、Dem へ問い合わせない */
        }

        if (FiM_CheckCondition(fn, &inhibiting) != E_OK)
        {
            continue;
        }

        if (inhibiting == TRUE)
        {
            permitted[fn->FunctionId]  = 0U;
            causeEvent[fn->FunctionId] = fn->EventId;
        }
    }
}

/**
 * \brief   全 FID の許可状態を再評価し、変わった FID をログに出して、内部状態へ反映する。
 *
 * \details FiM_MainFunction() と FiM_DemTriggerOnMonitorStatus() の共通の処理。
 *
 * \pre     FiM_Init() 済み（FiM_Cfg が NULL でない）。
 */
static void FiM_Refresh(void)
{
    uint8 newPermitted[FIM_FUNCTION_COUNT];
    uint8 causeEvent[FIM_FUNCTION_COUNT];
    FiM_EvaluateAll(newPermitted, causeEvent);

    const uint8 fidCount = FiM_FidCount();

    for (uint8 fid = 0U; fid < fidCount; fid++)
    {
        if (newPermitted[fid] != FiM_Permitted[fid])
        {
            FiM_Permitted[fid] = newPermitted[fid];

            if (newPermitted[fid] == 0U)
            {
                DET_LOGW(TAG, "FID%u inhibited (ev=%u)", (unsigned)fid, (unsigned)causeEvent[fid]);
            }
            else
            {
                DET_LOGI(TAG, "FID%u permitted again", (unsigned)fid);
            }
        }
    }
}

/* ----------------------------------------------------------------------
 * FiM_Init
 * ---------------------------------------------------------------------- */

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
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init: NULL ConfigPtr");
        (void)Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_INIT, FIM_E_PARAM_POINTER);
        return;
    }

    FiM_Cfg = ConfigPtr;

    uint8 causeEvent[FIM_FUNCTION_COUNT];
    FiM_EvaluateAll(FiM_Permitted, causeEvent);

    const uint8 fidCount = FiM_FidCount();

    for (uint8 fid = 0U; fid < fidCount; fid++)
    {
        FiM_Available[fid] = 1U;

        if (FiM_Permitted[fid] == 0U)
        {
            DET_LOGW(TAG, "FID%u inhibited at init (ev=%u)", (unsigned)fid, (unsigned)causeEvent[fid]);
        }
    }

    DET_LOGI(TAG, "Init ok functions=%u inhibitions=%u",
             (unsigned)ConfigPtr->FunctionCount, (unsigned)ConfigPtr->InhibitionCount);
}

/* ----------------------------------------------------------------------
 * FiM_GetFunctionPermission
 * ---------------------------------------------------------------------- */

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
    if (Permission == NULL)
    {
        (void)Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_FUNCTION_PERMISSION, FIM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (FiM_Cfg == NULL)
    {
        *Permission = FALSE;  /* フェールセーフ: 未初期化中は抑止扱いとする */
        (void)Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_FUNCTION_PERMISSION, FIM_E_UNINIT);
        return E_NOT_OK;
    }

    if (FunctionId >= FiM_Cfg->FunctionCount)
    {
        *Permission = FALSE;  /* フェールセーフ: 不明な FID は抑止扱いとする */
        (void)Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_FUNCTION_PERMISSION, FIM_E_FID_OUT_OF_RANGE);
        return E_NOT_OK;
    }

    /* [SWS_Fim_00105]: Availability=0 (利用不可) の FID は Dem ベースの判定に
     * 関わらず常に抑止扱いとする (FiM_SetFunctionAvailable() の Doxygen 参照)。 */
    *Permission = (FiM_Available[FunctionId] != 0U) ? (FiM_Permitted[FunctionId] != 0U) : FALSE;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * FiM_SetFunctionAvailable
 * ---------------------------------------------------------------------- */

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
    if (FiM_Cfg == NULL)
    {
        (void)Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_SET_FUNCTION_AVAILABLE, FIM_E_UNINIT);
        return E_NOT_OK;
    }

    if (FID >= FiM_Cfg->FunctionCount)
    {
        (void)Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_SET_FUNCTION_AVAILABLE, FIM_E_FID_OUT_OF_RANGE);
        return E_NOT_OK;
    }

    FiM_Available[FID] = Availability ? 1U : 0U;

    if (!Availability)
    {
        DET_LOGW(TAG, "FID%u made unavailable (forced)", (unsigned)FID);
    }
    else
    {
        DET_LOGI(TAG, "FID%u made available again", (unsigned)FID);
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * FiM_DemTriggerOnMonitorStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * FiM_DemTriggerOnComponentStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * FiM_DemInit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * FiM_GetVersionInfo
 * ---------------------------------------------------------------------- */

void FiM_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(FIM_MODULE_ID, 0U, FIM_API_ID_GET_VERSION_INFO, FIM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = FIM_VENDOR_ID;
    versioninfo->moduleID         = FIM_MODULE_ID;
    versioninfo->sw_major_version = FIM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = FIM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = FIM_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * FiM_MainFunction
 * ---------------------------------------------------------------------- */

/**
 * \brief   FiM 周期処理。各 FID の許可状態を再評価する。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_MainFunction(void)
{
    if (FiM_Cfg == NULL)
    {
        return;
    }

    FiM_Refresh();
}

/* ----------------------------------------------------------------------
 * FiM_DemTriggerOnMonitorStatus
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem から、イベントの監視ステータスが変わったことを知らされ、そのイベントを使う機能の許可状態を、すぐに更新する。
 *
 * \details [SWS_Fim_00021]、[SWS_Fim_00020]（機能の許可は、すぐに制御する）。Dem_SetEventStatus() が、ステータスが
 *          変わったときに呼ぶ。EventId を条件に使う行が無ければ、何もしない（Dem への問い合わせもしない）。
 *          あれば、FiM_MainFunction() と同じ再評価を、周期を待たずに行う。
 *          実仕様は、抑止しているイベントの数（抑止カウンタ）を増減して許可状態を決めるが、本実装は、全行を
 *          評価し直す方式（FiM_MainFunction() と同じ）で、結果は同じ。FiM_MainFunction() の周期の再評価も残す
 *          （Dem_ClearDTC() など、本関数が呼ばれない経路での、ステータスの変化を拾うため）。
 *          簡略化: FiM_Init() の前に呼ばれたときは、何もしない（DET も報告しない）。Dem は、FiM の初期化より前にも
 *          イベントを報告するため。
 *
 * \param[in]  EventId  監視ステータスが変わった Dem イベントの ID。
 *
 * \AUTOSARReq     {SWS_Fim_00021, SWS_Fim_00020}
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_DemTriggerOnMonitorStatus(Dem_EventIdType EventId)
{
    if (FiM_Cfg == NULL)
    {
        return;
    }

    for (uint8 i = 0U; i < FiM_Cfg->InhibitionCount; i++)
    {
        if (FiM_Cfg->Functions[i].EventId == EventId)
        {
            FiM_Refresh();
            return;
        }
    }
}

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef FIM_UNIT_TEST
void FiM_Test_ResetInitState(void)
{
    FiM_Cfg = NULL;
}
#endif

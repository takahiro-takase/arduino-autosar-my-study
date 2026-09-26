/**
 * \file    Nm.c
 * \brief   ネットワークマネジメントインタフェース実装
 *          (AUTOSAR SWS_NetworkManagementInterface 準拠)
 * \details ComM と CanNm の間を中継する薄い層（Nm.h 冒頭コメント参照）。
 *          NM Coordinator 機能は対応除外のため、大半の公開 API は
 *          「初期化状態・Channel を検証した上でそのまま CanNm_Xxx() へ委譲する」
 *          だけの単純な実装になる。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Nm.h"
#include "Nm_Cfg.h"
#include "CanNm.h"
#include "ComM.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Nm"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* Nm モジュール自身の初期化状態。CanNm_Initialized（CanNm.c）とは別に、
 * Nm 層自身が「Nm_Init() が呼ばれたか」を保持する
 * （[SWS_Nm_00127]/[SWS_BSW_00416]、CanNm.c の同名フラグと同じ設計方針）。 */
static uint8 Nm_Initialized = 0U;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static Nm_StateType Nm_MapCanNmState(CanNm_StateType canNmState);
static Nm_ModeType Nm_MapCanNmMode(CanNm_ModeType canNmMode);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ======================================================================
 * Standard services provided by NM Interface
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_Init
 * ---------------------------------------------------------------------- */

void Nm_Init(const Nm_ConfigType* ConfigPtr)
{
    (void)ConfigPtr;  /* 常に NULL（[SWS_Nm_00283]、post-build 設定を持たないため） */
    Nm_Initialized = 1U;
    DET_LOGI(TAG, "Init ok");
}

/* ----------------------------------------------------------------------
 * Nm_PassiveStartUp
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_NetworkRequest
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_NetworkRequest(NetworkHandleType Channel)
{
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_REQUEST, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_REQUEST, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    Std_ReturnType ret = CanNm_NetworkRequest(CANNM_MAIN_NETWORK_HANDLE);
    if (ret == E_OK)
    {
        DET_LOGI(TAG, "NetworkRequest ok");
    }
    return ret;
}

/* ----------------------------------------------------------------------
 * Nm_NetworkRelease
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_NetworkRelease(NetworkHandleType Channel)
{
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_RELEASE, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_RELEASE, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    Std_ReturnType ret = CanNm_NetworkRelease(CANNM_MAIN_NETWORK_HANDLE);
    if (ret == E_OK)
    {
        DET_LOGI(TAG, "NetworkRelease ok");
    }
    return ret;
}

/* ======================================================================
 * Communication control services provided by NM Interface
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_DisableCommunication
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_DisableCommunication(NetworkHandleType Channel)
{
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_DISABLE_COMMUNICATION, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_DISABLE_COMMUNICATION, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    Std_ReturnType ret = CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE);
    if (ret == E_OK)
    {
        DET_LOGI(TAG, "DisableCommunication ok");
    }
    return ret;
}

/* ----------------------------------------------------------------------
 * Nm_EnableCommunication
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_EnableCommunication(NetworkHandleType Channel)
{
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_ENABLE_COMMUNICATION, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_ENABLE_COMMUNICATION, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    Std_ReturnType ret = CanNm_EnableCommunication(CANNM_MAIN_NETWORK_HANDLE);
    if (ret == E_OK)
    {
        DET_LOGI(TAG, "EnableCommunication ok");
    }
    return ret;
}

/* ======================================================================
 * Extra services provided by NM Interface
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_SetUserData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_GetUserData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_GetPduData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_RepeatMessageRequest
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_RepeatMessageRequest(NetworkHandleType Channel)
{
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_REPEAT_MESSAGE_REQUEST, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_REPEAT_MESSAGE_REQUEST, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    Std_ReturnType ret = CanNm_RepeatMessageRequest(CANNM_MAIN_NETWORK_HANDLE);
    if (ret == E_OK)
    {
        DET_LOGI(TAG, "RepeatMessageRequest ok");
    }
    return ret;
}

/* ----------------------------------------------------------------------
 * Nm_GetNodeIdentifier
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    if (nmNodeIdPtr == NULL)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_NODE_IDENTIFIER, NM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_NODE_IDENTIFIER, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_NODE_IDENTIFIER, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    return CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, nmNodeIdPtr);
}

/* ----------------------------------------------------------------------
 * Nm_GetLocalNodeIdentifier
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    if (nmNodeIdPtr == NULL)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_LOCAL_NODE_IDENTIFIER, NM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_LOCAL_NODE_IDENTIFIER, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_LOCAL_NODE_IDENTIFIER, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    return CanNm_GetLocalNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, nmNodeIdPtr);
}

/* ----------------------------------------------------------------------
 * Nm_CheckRemoteSleepIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_GetState
 * ---------------------------------------------------------------------- */

Std_ReturnType Nm_GetState(NetworkHandleType Channel, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr)
{
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_STATE, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_STATE, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    CanNm_StateType canNmState;
    CanNm_ModeType  canNmMode;
    if (CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &canNmState, &canNmMode) != E_OK)
    {
        return E_NOT_OK;
    }

    if (nmStatePtr != NULL)
    {
        *nmStatePtr = Nm_MapCanNmState(canNmState);
    }
    if (nmModePtr != NULL)
    {
        *nmModePtr = Nm_MapCanNmMode(canNmMode);
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * Nm_GetVersionInfo
 * ---------------------------------------------------------------------- */

void Nm_GetVersionInfo(Std_VersionInfoType* nmVerInfoPtr)
{
    if (nmVerInfoPtr == NULL)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_VERSION_INFO, NM_E_PARAM_POINTER);
        return;
    }

    nmVerInfoPtr->vendorID         = NM_VENDOR_ID;
    nmVerInfoPtr->moduleID         = NM_MODULE_ID;
    nmVerInfoPtr->sw_major_version = NM_SW_MAJOR_VERSION;
    nmVerInfoPtr->sw_minor_version = NM_SW_MINOR_VERSION;
    nmVerInfoPtr->sw_patch_version = NM_SW_PATCH_VERSION;
}

/* ======================================================================
 * Call-back Notifications
 * ====================================================================== */

/* ======================================================================
 * Standard Call-back notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_NetworkStartIndication
 * ---------------------------------------------------------------------- */

void Nm_NetworkStartIndication(NetworkHandleType Channel)
{
    /* [SWS_Nm_00155]: 単純に ComM へ転送する。単一チャネル構成のため
     * Channel 値の変換は不要（CanNm 側も ComM 側も同じ 0 を使う）。 */
    DET_LOGI(TAG, "NetworkStartIndication -> ComM_Nm_NetworkStartIndication()");
    ComM_Nm_NetworkStartIndication(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_NetworkMode
 * ---------------------------------------------------------------------- */

void Nm_NetworkMode(NetworkHandleType Channel)
{
    /* [SWS_Nm_00158] */
    DET_LOGI(TAG, "NetworkMode -> ComM_Nm_NetworkMode()");
    ComM_Nm_NetworkMode(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_BusSleepMode
 * ---------------------------------------------------------------------- */

void Nm_BusSleepMode(NetworkHandleType Channel)
{
    /* [SWS_Nm_00163] */
    DET_LOGI(TAG, "BusSleepMode -> ComM_Nm_BusSleepMode()");
    ComM_Nm_BusSleepMode(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_PrepareBusSleepMode
 * ---------------------------------------------------------------------- */

void Nm_PrepareBusSleepMode(NetworkHandleType Channel)
{
    /* [SWS_Nm_00161] */
    DET_LOGI(TAG, "PrepareBusSleepMode -> ComM_Nm_PrepareBusSleepMode()");
    ComM_Nm_PrepareBusSleepMode(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_RemoteSleepIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_RemoteSleepCancellation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_SynchronizationPoint
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_CoordReadyToSleepIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_CoordReadyToSleepCancellation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Extra Call-back notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_PduRxIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_StateChangeNotification
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_RepeatMessageIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_TxTimeoutException
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_CarWakeUpIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Scheduled Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_MainFunction
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_MapCanNmState
 * ---------------------------------------------------------------------- */

/**
 * \brief   `CanNm_StateType`（5値、CANNM_STATE_BUS_SLEEP=0起点）を
 *          `Nm_StateType`（8値、NM_STATE_UNINIT=0x00起点）へ写像する
 *          （Nm.h 冒頭コメント参照）。
 *
 * \param[in]  canNmState  CanNm_GetState() が返した内部状態。
 *
 * \return  対応する Nm_StateType。
 */
static Nm_StateType Nm_MapCanNmState(CanNm_StateType canNmState)
{
    switch (canNmState)
    {
        case CANNM_STATE_BUS_SLEEP:         return NM_STATE_BUS_SLEEP;
        case CANNM_STATE_PREPARE_BUS_SLEEP: return NM_STATE_PREPARE_BUS_SLEEP;
        case CANNM_STATE_REPEAT_MESSAGE:    return NM_STATE_REPEAT_MESSAGE;
        case CANNM_STATE_NORMAL_OPERATION:  return NM_STATE_NORMAL_OPERATION;
        case CANNM_STATE_READY_SLEEP:       return NM_STATE_READY_SLEEP;
        default:
            /* CanNm_StateType はこの5値のみ取るため到達しないはず。
             * 安全側として最も保守的な Bus-Sleep 相当を返す。 */
            return NM_STATE_BUS_SLEEP;
    }
}

/* ----------------------------------------------------------------------
 * Nm_MapCanNmMode
 * ---------------------------------------------------------------------- */

/**
 * \brief   `CanNm_ModeType`（3値）を `Nm_ModeType`（4値、NM Coordinator専用の
 *          NM_MODE_SYNCHRONIZE を追加で持つ）へ写像する（Nm.h 冒頭コメント
 *          参照）。
 *
 * \param[in]  canNmMode  CanNm_GetState() が返した操作モード。
 *
 * \return  対応する Nm_ModeType。
 */
static Nm_ModeType Nm_MapCanNmMode(CanNm_ModeType canNmMode)
{
    switch (canNmMode)
    {
        case CANNM_MODE_BUS_SLEEP:         return NM_MODE_BUS_SLEEP;
        case CANNM_MODE_PREPARE_BUS_SLEEP: return NM_MODE_PREPARE_BUS_SLEEP;
        case CANNM_MODE_NETWORK:           return NM_MODE_NETWORK;
        default:
            /* CanNm_ModeType はこの3値のみ取るため到達しないはず。 */
            return NM_MODE_BUS_SLEEP;
    }
}

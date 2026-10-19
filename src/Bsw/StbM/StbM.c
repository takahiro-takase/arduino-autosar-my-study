/**
 * \file    StbM.c
 * \brief   同期時間ベースマネージャ 実装 (AUTOSAR SWS_StbM 準拠、最小版)
 * \details 公開インタフェースの説明（時刻の考え方・使い方）は StbM.h、対応範囲は
 *          StbM_Cfg.h を参照。
 *
 *          内部の時刻は 64bit の ns で持つ:
 *            仮想ローカル時間 [ns] = Os カウンタ [ms] × 10^6
 *            全域時間 [ns]       = 仮想ローカル時間 + オフセット（時間ベースごと、符号付き）
 *          時刻を設定するときは、そのときの全域時間が与えた値になるようオフセットを決める。
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

#include <stdint.h>

#include "StbM.h"
#include "StbM_Cfg.h"
#include "Det.h"
#include "Os.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "StbM"

/** 1 ms あたりの ns 数。 */
#define STBM_NS_PER_MS   1000000ULL

/** 1 s あたりの ns 数。 */
#define STBM_NS_PER_SEC  1000000000ULL

/** 設定できる時刻の秒の上限（ns に換算して符号付き 64bit のオフセットへ収まる範囲。約 146 年）。 */
#define STBM_MAX_SECONDS 4611686017ULL

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/** 時間ベースごとの状態。 */
typedef struct
{
    int64_t                 offsetNs;      /**< 全域時間 - 仮想ローカル時間 [ns] */
    StbM_TimeBaseStatusType status;        /**< 時間ベースの状態（STBM_TIMEBASE_STATUS_*） */
    uint64_t                lastUpdateMs;  /**< 最後に時刻を設定した時刻 [ms]（タイムアウトの起点。StbM_GetNowMs の値） */
} StbM_TimeBaseType;

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static uint8 StbM_Initialized = 0U;

/** Os カウンタ（32bit）の直近の値と、その一巡（約 49.7 日）の回数。StbM_GetNowMs が 64bit へ広げるのに使う。 */
static uint32 StbM_LastTick  = 0U;
static uint32 StbM_TickWraps = 0U;

static StbM_TimeBaseType StbM_TimeBase[STBM_TIME_BASE_COUNT];

/** 時間ベースごとの同期喪失のタイムアウト [ms]（0 なら監視しない）。添字は時間ベースの ID。 */
static const unsigned long StbM_SyncLossTimeoutMs[STBM_TIME_BASE_COUNT] = {
    STBM_SYNC_LOSS_TIMEOUT_MS_MASTER,
    STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE
};

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static uint64_t StbM_GetNowMs(void);
static uint64_t StbM_LocalNs(void);
static uint64_t StbM_GlobalNs(const StbM_TimeBaseType* base);
static void StbM_SetGlobalNs(StbM_TimeBaseType* base, uint64_t globalNs);
static uint64_t StbM_TimeStampToNs(const StbM_TimeStampType* ts);
static boolean  StbM_TimeStampInRange(const StbM_TimeStampType* ts, uint8 apiId);
static boolean StbM_CheckTimeBase(StbM_SynchronizedTimeBaseType timeBaseId, uint8 apiId);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * StbM_Init
 * ---------------------------------------------------------------------- */

/**
 * \brief   StbM を初期化する。マスターの時間ベースは、全域時間 = 仮想ローカル時間（ECU の
 *          稼働時間）で同期済み（GLOBAL_TIME_BASE）になる。スレーブの時間ベースは未同期。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を持たないため）。
 *
 * \AUTOSARReq     {SWS_StbM_00052}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void StbM_Init(const StbM_ConfigType* ConfigPtr)
{
    (void)ConfigPtr;  /* 常に NULL（post-build 設定を持たないため。StbM.h 参照） */

    StbM_LastTick  = 0U;
    StbM_TickWraps = 0U;

    for (uint16 id = 0U; id < STBM_TIME_BASE_COUNT; id++)
    {
        StbM_TimeBase[id].offsetNs     = 0;
        StbM_TimeBase[id].status       = 0U;
        StbM_TimeBase[id].lastUpdateMs = StbM_GetNowMs();
    }

    /* 時間マスター: 全域時間 = 仮想ローカル時間（オフセット 0）で、起動時から同期済みとする */
    StbM_TimeBase[STBM_TIME_BASE_MASTER].status = STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE;

    StbM_Initialized = 1U;   /* [SWS_StbM_00121] */

    DET_LOGI(TAG, "Init ok bases=%u", (unsigned)STBM_TIME_BASE_COUNT);
}

/* ----------------------------------------------------------------------
 * StbM_GetVersionInfo
 * ---------------------------------------------------------------------- */

/**
 * \brief   StbM のバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL を渡すと STBM_E_PARAM_POINTER を
 *                           DET へ報告し、何も書かない。
 *
 * \AUTOSARReq     {SWS_StbM_00066}
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void StbM_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_GET_VERSION_INFO, STBM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = STBM_VENDOR_ID;
    versioninfo->moduleID         = STBM_MODULE_ID;
    versioninfo->sw_major_version = STBM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = STBM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = STBM_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * StbM_GetCurrentTime
 * ---------------------------------------------------------------------- */

/**
 * \brief   時間ベースの現在の全域時間を取得する。
 *
 * \param[in]  timeBaseId  時間ベースの ID（STBM_TIME_BASE_COUNT 未満）。範囲外は STBM_E_PARAM。
 * \param[out] timeStamp   現在の時刻と、時間ベースの状態。NULL は STBM_E_PARAM_POINTER。
 * \param[out] userData    ユーザデータ。NULL は STBM_E_PARAM_POINTER。本実装はユーザデータを
 *                         保持しないため、長さ 0 を返す。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00195}
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetCurrentTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                   StbM_TimeStampType* timeStamp,
                                   StbM_UserDataType* userData)
{
    if (!StbM_CheckTimeBase(timeBaseId, STBM_API_ID_GET_CURRENT_TIME))
    {
        return E_NOT_OK;
    }

    if ((timeStamp == NULL) || (userData == NULL))
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_GET_CURRENT_TIME, STBM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const StbM_TimeBaseType* base = &StbM_TimeBase[timeBaseId];
    uint64_t globalNs = StbM_GlobalNs(base);
    uint64_t seconds  = globalNs / STBM_NS_PER_SEC;

    timeStamp->timeBaseStatus = base->status;
    timeStamp->nanoseconds    = (uint32)(globalNs % STBM_NS_PER_SEC);
    timeStamp->seconds        = (uint32)(seconds & 0xFFFFFFFFULL);
    timeStamp->secondsHi      = (uint16)((seconds >> 32U) & 0xFFFFULL);

    userData->userDataLength = 0U;
    userData->userByte0      = 0U;
    userData->userByte1      = 0U;
    userData->userByte2      = 0U;

    return E_OK;
}

/* ----------------------------------------------------------------------
 * StbM_GetCurrentTimeRaw
 * ---------------------------------------------------------------------- */

/**
 * \brief   時間ベースの仮想ローカル時間の ns 部分を取得する（時間差の測定の起点に使う）。
 *
 * \param[in]  timeBaseId      時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[out] timeStampRawPtr 仮想ローカル時間の ns 部分（約 4.29 秒で循環）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00205}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetCurrentTimeRaw(StbM_SynchronizedTimeBaseType timeBaseId,
                                      StbM_TimeStampRawType* timeStampRawPtr)
{
    if (!StbM_CheckTimeBase(timeBaseId, STBM_API_ID_GET_CURRENT_TIME_RAW))
    {
        return E_NOT_OK;
    }

    if (timeStampRawPtr == NULL)
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_GET_CURRENT_TIME_RAW, STBM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *timeStampRawPtr = (StbM_TimeStampRawType)StbM_LocalNs();   /* 下位 32bit */
    return E_OK;
}

/* ----------------------------------------------------------------------
 * StbM_GetCurrentTimeDiff
 * ---------------------------------------------------------------------- */

/**
 * \brief   現在の仮想ローカル時間と、StbM_GetCurrentTimeRaw() で取った時刻との差 [ns] を取得する。
 *
 * \param[in]  timeBaseId        時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[in]  givenTimeStamp    差の基準にする時刻（StbM_GetCurrentTimeRaw() の値）。
 * \param[out] timeStampDiffPtr  現在 - 基準 [ns]（循環を考慮した符号なしの差）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00209}
 * \ServiceID      {0x0A}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetCurrentTimeDiff(StbM_SynchronizedTimeBaseType timeBaseId,
                                       StbM_TimeStampRawType givenTimeStamp,
                                       StbM_TimeStampRawType* timeStampDiffPtr)
{
    if (!StbM_CheckTimeBase(timeBaseId, STBM_API_ID_GET_CURRENT_TIME_DIFF))
    {
        return E_NOT_OK;
    }

    if (timeStampDiffPtr == NULL)
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_GET_CURRENT_TIME_DIFF, STBM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* uint32 の引き算で、仮想ローカル時間の ns 部分の循環（約 4.29 秒）を自然に吸収する */
    *timeStampDiffPtr = (StbM_TimeStampRawType)((StbM_TimeStampRawType)StbM_LocalNs() - givenTimeStamp);
    return E_OK;
}

/* ----------------------------------------------------------------------
 * StbM_SetGlobalTime
 * ---------------------------------------------------------------------- */

/**
 * \brief   時間マスターが、時間ベースの全域時間を設定する。
 *
 * \details 設定した時点の全域時間が timeStamp になるようにオフセットを更新し、GLOBAL_TIME_BASE を
 *          立て、TIMEOUT を下ろす。
 *
 * \param[in]  timeBaseId  時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[in]  timeStamp   設定する時刻。NULL は STBM_E_PARAM_POINTER。
 * \param[in]  userData    ユーザデータ（本実装は保持しない。NULL でもよい）。
 * \return     E_OK=設定できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00213}
 * \ServiceID      {0x0B}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_SetGlobalTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                  const StbM_TimeStampType* timeStamp,
                                  const StbM_UserDataType* userData)
{
    (void)userData;   /* ユーザデータは保持しない（StbM.h 参照） */

    if (!StbM_CheckTimeBase(timeBaseId, STBM_API_ID_SET_GLOBAL_TIME))
    {
        return E_NOT_OK;
    }

    if (timeStamp == NULL)
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_SET_GLOBAL_TIME, STBM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (!StbM_TimeStampInRange(timeStamp, STBM_API_ID_SET_GLOBAL_TIME))
    {
        return E_NOT_OK;
    }

    StbM_SetGlobalNs(&StbM_TimeBase[timeBaseId], StbM_TimeStampToNs(timeStamp));
    return E_OK;
}

/* ----------------------------------------------------------------------
 * StbM_BusSetGlobalTime
 * ---------------------------------------------------------------------- */

/**
 * \brief   バスから受信した全域時間を、時間ベースへ反映する（時間スレーブ側のモジュールが呼ぶ）。
 *
 * \details 受信した時刻に伝搬遅延 pathDelay を足した値が、いまの全域時間になるようにオフセットを
 *          更新し、GLOBAL_TIME_BASE を立て、TIMEOUT を下ろす。timeStamp->timeBaseStatus の
 *          SYNC_TO_GATEWAY は、時間ベースの状態へ引き継ぐ。
 *
 * \param[in]  timeBaseId     時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[in]  timeStampPtr   受信した時刻。NULL は STBM_E_PARAM_POINTER。
 * \param[in]  userDataPtr    ユーザデータ（本実装は保持しない。NULL でもよい）。
 * \param[in]  measureDataPtr 追加情報（伝搬遅延）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=反映できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00233}
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_BusSetGlobalTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                     const StbM_TimeStampType* timeStampPtr,
                                     const StbM_UserDataType* userDataPtr,
                                     const StbM_MeasurementType* measureDataPtr)
{
    (void)userDataPtr;   /* ユーザデータは保持しない（StbM.h 参照） */

    if (!StbM_CheckTimeBase(timeBaseId, STBM_API_ID_BUS_SET_GLOBAL_TIME))
    {
        return E_NOT_OK;
    }

    if ((timeStampPtr == NULL) || (measureDataPtr == NULL))
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_BUS_SET_GLOBAL_TIME, STBM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    StbM_TimeBaseType* base = &StbM_TimeBase[timeBaseId];

    if (!StbM_TimeStampInRange(timeStampPtr, STBM_API_ID_BUS_SET_GLOBAL_TIME))
    {
        return E_NOT_OK;
    }

    StbM_SetGlobalNs(base, StbM_TimeStampToNs(timeStampPtr) + (uint64_t)measureDataPtr->pathDelay);

    if ((timeStampPtr->timeBaseStatus & STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY) != 0U)
    {
        base->status |= STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY;
    }
    else
    {
        base->status &= (StbM_TimeBaseStatusType)(~STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY);
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * StbM_GetTimeBaseStatus
 * ---------------------------------------------------------------------- */

/**
 * \brief   時間ベースの状態を取得する。
 *
 * \param[in]  timeBaseId         時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[out] syncTimeBaseStatus 同期時間ベースの状態。NULL は STBM_E_PARAM_POINTER。
 * \param[out] offsetTimeBaseStatus オフセット時間ベースの状態（本実装はオフセット時間ベースを
 *                         持たないため、常に 0）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00263}
 * \ServiceID      {0x14}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetTimeBaseStatus(StbM_SynchronizedTimeBaseType timeBaseId,
                                      StbM_TimeBaseStatusType* syncTimeBaseStatus,
                                      StbM_TimeBaseStatusType* offsetTimeBaseStatus)
{
    if (!StbM_CheckTimeBase(timeBaseId, STBM_API_ID_GET_TIME_BASE_STATUS))
    {
        return E_NOT_OK;
    }

    if ((syncTimeBaseStatus == NULL) || (offsetTimeBaseStatus == NULL))
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_GET_TIME_BASE_STATUS, STBM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *syncTimeBaseStatus   = StbM_TimeBase[timeBaseId].status;
    *offsetTimeBaseStatus = 0U;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * StbM_MainFunction
 * ---------------------------------------------------------------------- */

/**
 * \brief   同期喪失のタイムアウトを監視する。
 *
 * \details GLOBAL_TIME_BASE が立っていて、タイムアウトが設定された時間ベース
 *          （STBM_SYNC_LOSS_TIMEOUT_MS_*）が、最後の更新から設定時間以上更新されていなければ、
 *          TIMEOUT ビットを立てる（[SWS_StbM_00179]）。次の更新で下りる。
 *
 * \AUTOSARReq     {SWS_StbM_00057}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void StbM_MainFunction(void)
{
    if (StbM_Initialized == 0U)
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, STBM_API_ID_MAIN_FUNCTION, STBM_E_NOT_INITIALIZED);
        return;
    }

    uint64_t nowMs = StbM_GetNowMs();

    for (uint16 id = 0U; id < STBM_TIME_BASE_COUNT; id++)
    {
        StbM_TimeBaseType* base = &StbM_TimeBase[id];

        if ((StbM_SyncLossTimeoutMs[id] != 0UL)
            && ((base->status & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE) != 0U)
            && ((base->status & STBM_TIMEBASE_STATUS_TIMEOUT) == 0U)
            && ((nowMs - base->lastUpdateMs) >= StbM_SyncLossTimeoutMs[id]))
        {
            base->status |= STBM_TIMEBASE_STATUS_TIMEOUT;
            DET_LOGW(TAG, "base=%u sync loss timeout", (unsigned)id);
        }
    }
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * StbM_GetNowMs
 * ---------------------------------------------------------------------- */

/**
 * \brief   現在時刻 [ms] を 64bit で返す（Os カウンタ SYSTEM_COUNTER の tick 値を、一巡ごとに広げる）。
 * \details millis() を直接呼ばず Os の GetCounterValue() を介する理由は CanNm.c の同名関数と同じ。
 *          Os カウンタは 32bit（約 49.7 日で一巡）で、そのままでは仮想ローカル時間（ns）が一巡の
 *          たびに戻り、全域時間が飛ぶ。直近の値より小さくなったら一巡したとみなして上位を足す
 *          （一巡より短い間隔で呼ばれることが前提。StbM_MainFunction が 100ms ごとに呼ぶ）。
 */
static uint64_t StbM_GetNowMs(void)
{
    TickType now = 0U;
    (void)GetCounterValue(SYSTEM_COUNTER, &now);

    if ((uint32)now < StbM_LastTick)
    {
        StbM_TickWraps++;
    }
    StbM_LastTick = (uint32)now;

    return ((uint64_t)StbM_TickWraps << 32U) | (uint64_t)StbM_LastTick;
}

/* ----------------------------------------------------------------------
 * StbM_LocalNs
 * ---------------------------------------------------------------------- */

/**
 * \brief   仮想ローカル時間 [ns]（Os カウンタ [ms] × 10^6）を返す。
 */
static uint64_t StbM_LocalNs(void)
{
    return StbM_GetNowMs() * STBM_NS_PER_MS;
}

/* ----------------------------------------------------------------------
 * StbM_GlobalNs
 * ---------------------------------------------------------------------- */

/**
 * \brief   時間ベースの現在の全域時間 [ns] を返す（仮想ローカル時間 + オフセット。負なら 0）。
 *
 * \param[in]  base  時間ベース。
 */
static uint64_t StbM_GlobalNs(const StbM_TimeBaseType* base)
{
    int64_t globalNs = (int64_t)StbM_LocalNs() + base->offsetNs;
    return (globalNs < 0) ? 0ULL : (uint64_t)globalNs;
}

/* ----------------------------------------------------------------------
 * StbM_SetGlobalNs
 * ---------------------------------------------------------------------- */

/**
 * \brief   いまの全域時間が globalNs になるよう、時間ベースのオフセットを更新する。
 *
 * \details 同期済み（GLOBAL_TIME_BASE）にし、TIMEOUT を下ろして、最終更新時刻を現在にする。
 *          同期済みでなかった（初回、またはタイムアウト後）の更新は INFO ログに出す（受信側の実機確認用）。
 *
 * \param[in,out] base      時間ベース。
 * \param[in]     globalNs  いまの全域時間 [ns]。
 */
static void StbM_SetGlobalNs(StbM_TimeBaseType* base, uint64_t globalNs)
{
    boolean wasSynced = ((base->status & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE) != 0U)
                        && ((base->status & STBM_TIMEBASE_STATUS_TIMEOUT) == 0U);

    base->offsetNs     = (int64_t)globalNs - (int64_t)StbM_LocalNs();
    base->status       = (StbM_TimeBaseStatusType)((base->status | STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE)
                                                   & (StbM_TimeBaseStatusType)(~STBM_TIMEBASE_STATUS_TIMEOUT));
    base->lastUpdateMs = StbM_GetNowMs();

    if (!wasSynced)
    {
        DET_LOGI(TAG, "base=%u synchronized", (unsigned)(base - StbM_TimeBase));
    }
}

/* ----------------------------------------------------------------------
 * StbM_TimeStampToNs
 * ---------------------------------------------------------------------- */

/**
 * \brief   StbM_TimeStampType を ns に換算する（秒は 48bit）。
 *
 * \param[in]  ts  時刻。
 */
static uint64_t StbM_TimeStampToNs(const StbM_TimeStampType* ts)
{
    uint64_t seconds = ((uint64_t)ts->secondsHi << 32U) | (uint64_t)ts->seconds;
    return (seconds * STBM_NS_PER_SEC) + (uint64_t)ts->nanoseconds;
}

/* ----------------------------------------------------------------------
 * StbM_TimeStampInRange
 * ---------------------------------------------------------------------- */

/**
 * \brief   設定しようとする時刻が保持できる範囲（秒が STBM_MAX_SECONDS 以下、ns が 10^9 未満）か確認する。
 *
 * \details 範囲外なら STBM_E_PARAM を DET へ報告する（ns への換算やオフセットの計算があふれるのを防ぐ）。
 *
 * \param[in]  ts     確認する時刻。
 * \param[in]  apiId  DET へ報告するときの ApiId。
 * \return     TRUE=範囲内。FALSE=範囲外（報告済み。呼び出し元は E_NOT_OK を返す）。
 */
static boolean StbM_TimeStampInRange(const StbM_TimeStampType* ts, uint8 apiId)
{
    uint64_t seconds = ((uint64_t)ts->secondsHi << 32U) | (uint64_t)ts->seconds;

    if ((seconds > STBM_MAX_SECONDS) || (ts->nanoseconds >= (uint32)STBM_NS_PER_SEC))
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, apiId, STBM_E_PARAM);
        return FALSE;
    }

    return TRUE;
}

/* ----------------------------------------------------------------------
 * StbM_CheckTimeBase
 * ---------------------------------------------------------------------- */

/**
 * \brief   API 共通の入口チェック（初期化済みか、時間ベースの ID が範囲内か）。
 *
 * \details 未初期化なら STBM_E_NOT_INITIALIZED、範囲外なら STBM_E_PARAM を DET へ報告する。
 *
 * \param[in]  timeBaseId  時間ベースの ID。
 * \param[in]  apiId       DET へ報告するときの ApiId。
 * \return     TRUE=続行してよい。FALSE=エラーを報告済み（呼び出し元は E_NOT_OK を返す）。
 */
static boolean StbM_CheckTimeBase(StbM_SynchronizedTimeBaseType timeBaseId, uint8 apiId)
{
    if (StbM_Initialized == 0U)
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, apiId, STBM_E_NOT_INITIALIZED);
        return FALSE;
    }

    if (timeBaseId >= STBM_TIME_BASE_COUNT)
    {
        (void)Det_ReportError(STBM_MODULE_ID, 0U, apiId, STBM_E_PARAM);
        return FALSE;
    }

    return TRUE;
}

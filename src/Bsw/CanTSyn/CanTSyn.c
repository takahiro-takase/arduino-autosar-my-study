/**
 * \file    CanTSyn.c
 * \brief   CAN 時刻同期 実装 (AUTOSAR SWS_CanTSyn 準拠、時間マスターのみ)
 * \details 公開インタフェースの説明（シーケンス・フォーマット・対応範囲）は
 *          CanTSyn.h / CanTSyn_Cfg.h を参照。
 *
 *          内部状態は3つ:
 *            IDLE            : 次の SYNC 送信周期を待っている。
 *            WAIT_SYNC_CONF  : SYNC を CanIf へ渡した。送信完了通知待ち。
 *            WAIT_FUP_CONF   : FUP を CanIf へ渡した。送信完了通知待ち。
 *          [SWS_CanTSyn_00029]: SYNC と FUP の1組の送信が終わる（または
 *          タイムアウトで破棄される）まで、次の SYNC は送らない。
 *
 *          時刻源は Os カウンタ（ECU の稼働時間 [ms]）。T0 の秒部分は
 *          T0[ms]/1000、ns 部分は (T0[ms]%1000)*10^6 [ns]。FUP の時刻 T4 は
 *          「T0 の ns 部分 + SYNC を CanIf へ渡してから送信完了通知までの
 *          経過時間」で、ms 単位の分解能で配信する。
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

#include "CanTSyn.h"
#include "CanTSyn_Cfg.h"
#include "CanIf.h"
#include "Det.h"
#include "Os.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "CanTSyn"

/** 1 ms あたりの ns 数。 */
#define CANTSYN_NS_PER_MS  1000000UL

/** 1 s あたりの ns 数。 */
#define CANTSYN_NS_PER_SEC  1000000000UL

/** 1 s あたりの ms 数。 */
#define CANTSYN_MS_PER_SEC  1000UL

/** SC（シーケンスカウンタ）の値域（4bit、[SWS_CanTSyn_00048]）。 */
#define CANTSYN_SC_MASK  0x0FU

/** OVS（秒のオーバーフロー、FUP Byte3 の Bit1-0）の最大値。 */
#define CANTSYN_OVS_MAX  3UL

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/** 送信シーケンスの内部状態。 */
typedef enum
{
    CANTSYN_STATE_IDLE = 0,       /**< 次の SYNC 送信周期を待っている */
    CANTSYN_STATE_WAIT_SYNC_CONF, /**< SYNC の送信完了通知待ち */
    CANTSYN_STATE_WAIT_FUP_CONF   /**< FUP の送信完了通知待ち */
} CanTSyn_StateType;

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static uint8 CanTSyn_Initialized = 0U;

/** 送信機能のオン/オフ（CanTSyn_SetTransmissionMode() で更新）。 */
static uint8 CanTSyn_TxOn;

/** 現在の送信シーケンスの状態。 */
static CanTSyn_StateType CanTSyn_State;

/** 直近に SYNC を送信要求した時刻 [ms]（送信周期の判定用）。 */
static unsigned long CanTSyn_LastSyncMs;

/** 現在待っている送信完了通知の起点時刻 [ms]。WAIT_SYNC_CONF の間は T0raw
 *  （SYNC を CanIf へ渡した時刻）、WAIT_FUP_CONF の間は FUP を渡した時刻。 */
static unsigned long CanTSyn_WaitStartMs;

/** 現在の送信シーケンス（SYNC と対の FUP）のシーケンスカウンタ（0〜15）。
 *  SYNC を送信要求するたびに +1 する（SendSync）。FUP は同じ値を使う。 */
static uint8 CanTSyn_Sc;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static unsigned long CanTSyn_GetNowMs(void);
static void CanTSyn_SendSync(unsigned long nowMs);
static void CanTSyn_SendFup(unsigned long nowMs);
static void CanTSyn_WriteBe32(uint8* dst, uint32 value);
static Std_ReturnType CanTSyn_TransmitPdu(uint8* pdu);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTSyn_Init
 * ---------------------------------------------------------------------- */

/**
 * \brief   CanTSyn モジュールを初期化する。送信機能はオンで、最初の SYNC は
 *          次の CanTSyn_MainFunction() 呼び出しで送る。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build
 *                        設定を持たないため）。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00093}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_Init(const CanTSyn_ConfigType* ConfigPtr)
{
    (void)ConfigPtr;  /* 常に NULL（post-build 設定を持たないため。CanTSyn.h 参照） */
    CanTSyn_TxOn          = 1U;
    CanTSyn_State         = CANTSYN_STATE_IDLE;
    CanTSyn_Sc            = CANTSYN_SC_MASK;   /* [SWS_CanTSyn_00007]: 最初の SYNC が SC=0 になるよう、1 つ前の値（15）から始める */
    CanTSyn_WaitStartMs   = 0UL;
    /* 初回の呼び出しで SYNC を送るため、前回送信時刻を1周期前にしておく
     * （unsigned の引き算の折り返しで、経過時間は CANTSYN_TX_PERIOD_MS になる） */
    CanTSyn_LastSyncMs    = CanTSyn_GetNowMs() - CANTSYN_TX_PERIOD_MS;
    CanTSyn_Initialized   = 1U;

    DET_LOGI(TAG, "Init ok period=%lums", (unsigned long)CANTSYN_TX_PERIOD_MS);
}

/* ----------------------------------------------------------------------
 * CanTSyn_GetVersionInfo
 * ---------------------------------------------------------------------- */

/**
 * \brief   CanTSyn のバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL を渡すと
 *                           CANTSYN_E_NULL_POINTER を DET へ報告し、何も書かない。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00094}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_GET_VERSION_INFO, CANTSYN_E_NULL_POINTER);
        return;
    }

    versioninfo->vendorID         = CANTSYN_VENDOR_ID;
    versioninfo->moduleID         = CANTSYN_MODULE_ID;
    versioninfo->sw_major_version = CANTSYN_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = CANTSYN_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = CANTSYN_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * CanTSyn_SetTransmissionMode
 * ---------------------------------------------------------------------- */

/**
 * \brief   CanTSyn の送信機能をオン/オフする。
 *
 * \details オフにすると、途中の SYNC/FUP の送信シーケンスも破棄して待機に戻る。
 *          オンに戻すと、次の CanTSyn_MainFunction() で、前回の SYNC から送信周期が
 *          経過していれば SYNC を送る（オフの間に周期が過ぎていればすぐに送る）。
 *
 * \param[in]  CtrlIdx  CAN チャネル番号（単一チャネル構成のため 0 のみ有効）。
 *                      それ以外は CANTSYN_E_INV_CTRL_IDX を DET へ報告し何もしない。
 * \param[in]  Mode     CANTSYN_TX_OFF / CANTSYN_TX_ON。それ以外は
 *                      CANTSYN_E_PARAM を DET へ報告し何もしない。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00095}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_SetTransmissionMode(uint8 CtrlIdx, CanTSyn_TransmissionModeType Mode)
{
    if (CanTSyn_Initialized == 0U)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_SET_TRANSMISSION_MODE, CANTSYN_E_UNINIT);
        return;
    }

    if (CtrlIdx != 0U)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_SET_TRANSMISSION_MODE, CANTSYN_E_INV_CTRL_IDX);
        return;
    }

    if ((Mode != CANTSYN_TX_OFF) && (Mode != CANTSYN_TX_ON))
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_SET_TRANSMISSION_MODE, CANTSYN_E_PARAM);
        return;
    }

    CanTSyn_TxOn = (uint8)((Mode == CANTSYN_TX_ON) ? 1U : 0U);
    if (CanTSyn_TxOn == 0U)
    {
        CanTSyn_State = CANTSYN_STATE_IDLE;   /* 途中のシーケンスは破棄する */
    }
}

/* ----------------------------------------------------------------------
 * CanTSyn_MainFunction
 * ---------------------------------------------------------------------- */

/**
 * \brief   時刻同期メッセージの周期送信と、送信完了待ちの監視を行う。
 *
 * \details 待機中に CANTSYN_TX_PERIOD_MS が経過していれば SYNC を送信する。
 *          SYNC/FUP の送信完了待ちが CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS を
 *          超えたら、送信を取り消して待機に戻る（[SWS_CanTSyn_00033]）。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00102}
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_MainFunction(void)
{
    if (CanTSyn_Initialized == 0U)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_MAIN_FUNCTION, CANTSYN_E_UNINIT);
        return;
    }

    unsigned long nowMs = CanTSyn_GetNowMs();

    if (CanTSyn_State == CANTSYN_STATE_IDLE)
    {
        if ((CanTSyn_TxOn != 0U) && ((nowMs - CanTSyn_LastSyncMs) >= CANTSYN_TX_PERIOD_MS))
        {
            CanTSyn_SendSync(nowMs);
        }
    }
    else if ((nowMs - CanTSyn_WaitStartMs) >= CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS)
    {
        /* [SWS_CanTSyn_00027]/[SWS_CanTSyn_00033]: 送信完了通知の待ちがタイムアウトした。
         * 送信を取り消し、次の周期で SYNC からやり直す（FUP は送らない）。 */
        DET_LOGW(TAG, "TX confirmation timeout state=%u", (unsigned)CanTSyn_State);
        CanTSyn_State = CANTSYN_STATE_IDLE;
    }
    else
    {
        /* 送信完了通知を待っている間は何もしない */
    }
}

/* ======================================================================
 * Call-back Notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTSyn_TxConfirmation
 * ---------------------------------------------------------------------- */

/**
 * \brief   SYNC/FUP の送信完了を通知する（CanIf から呼ばれる）。
 *
 * \details SYNC の送信完了なら、送信遅延を含む T4 を計算して FUP を送信する
 *          （[SWS_CanTSyn_00045]）。ただし SYNC を渡してから CANTSYN_MASTER_
 *          CONFIRMATION_TIMEOUT_MS 以上経っていたら、T4 が信用できないため FUP は
 *          送らず待機に戻る。FUP の送信完了なら待機に戻る。
 *          result が E_NOT_OK の場合は、その送信シーケンスを破棄して待機に戻る。
 *
 * \param[in]  TxPduId  送信完了した PDU の ID（CANTSYN_TX_PDU_ID 以外は
 *                      CANTSYN_E_INVALID_PDUID を DET へ報告し何もしない）。
 * \param[in]  result   E_OK=送信成功。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00099}
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    if (CanTSyn_Initialized == 0U)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_TX_CONFIRMATION, CANTSYN_E_UNINIT);
        return;
    }

    if (TxPduId != CANTSYN_TX_PDU_ID)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_TX_CONFIRMATION, CANTSYN_E_INVALID_PDUID);
        return;
    }

    if (CanTSyn_State == CANTSYN_STATE_WAIT_SYNC_CONF)
    {
        unsigned long nowMs = CanTSyn_GetNowMs();

        if ((nowMs - CanTSyn_WaitStartMs) >= CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS)
        {
            /* 送信完了通知が待ち時間を超えて届いた（MainFunction より先に通知が来た場合）。
             * 送信遅延が大きすぎて T4 が信用できないため、FUP は送らず次の周期でやり直す
             * （[SWS_CanTSyn_00033]、MainFunction のタイムアウトと同じ扱い）。 */
            DET_LOGW(TAG, "SYNC confirmation too late elapsed=%lums", (unsigned long)(nowMs - CanTSyn_WaitStartMs));
            CanTSyn_State = CANTSYN_STATE_IDLE;
        }
        else if (result == E_OK)
        {
            CanTSyn_SendFup(nowMs);
        }
        else
        {
            CanTSyn_State = CANTSYN_STATE_IDLE;   /* SYNC が送れなかったので FUP は送らない */
        }
    }
    else if (CanTSyn_State == CANTSYN_STATE_WAIT_FUP_CONF)
    {
        CanTSyn_State = CANTSYN_STATE_IDLE;       /* SYNC/FUP の1組が完了 */
    }
    else
    {
        /* 待機中の通知（タイムアウトで破棄した後に遅れて届いたもの）は無視する */
    }
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTSyn_GetNowMs
 * ---------------------------------------------------------------------- */

/**
 * \brief   現在時刻 [ms] を返す（Os カウンタ SYSTEM_COUNTER の tick 値）。
 * \details StbM を実装した場合は StbM_GetCurrentTime() への置き換え箇所
 *          （CanTSyn_Cfg.h 参照）。millis() を直接呼ばず Os の GetCounterValue() を
 *          介する理由は CanNm.c の同名関数と同じ。
 */
static unsigned long CanTSyn_GetNowMs(void)
{
    TickType now = 0U;
    (void)GetCounterValue(SYSTEM_COUNTER, &now);
    return (unsigned long)now;
}

/* ----------------------------------------------------------------------
 * CanTSyn_WriteBe32
 * ---------------------------------------------------------------------- */

/**
 * \brief   32bit 値をビッグエンディアンで 4 バイトへ書く（[SWS_CanTSyn_00008]）。
 *
 * \param[out] dst    書き込み先（4 バイト以上）。
 * \param[in]  value  書く値。
 */
static void CanTSyn_WriteBe32(uint8* dst, uint32 value)
{
    dst[0] = (uint8)(value >> 24U);
    dst[1] = (uint8)(value >> 16U);
    dst[2] = (uint8)(value >> 8U);
    dst[3] = (uint8)(value & 0xFFU);
}

/* ----------------------------------------------------------------------
 * CanTSyn_SendSync
 * ---------------------------------------------------------------------- */

/**
 * \brief   SYNC メッセージを組み立てて CanIf_Transmit() へ渡す。
 *
 * \details T0 = 現在時刻。秒部分を SyncTimeSec へ書く（ns 部分は FUP の T4 計算で
 *          CanTSyn_LastSyncMs から求める）。SC は送信要求のたびに1進める（[SWS_CanTSyn_00048]。送信に
 *          失敗した場合も進める）。渡せた場合は送信完了通知待ちへ移り、渡せな
 *          かった場合は待機のまま、次の周期でやり直す。
 *
 * \param[in]  nowMs  現在時刻 [ms]（T0）。
 */
static void CanTSyn_SendSync(unsigned long nowMs)
{
    uint8  pdu[CANTSYN_DLC] = { 0U };
    uint32 secPart = (uint32)(nowMs / CANTSYN_MS_PER_SEC);

    CanTSyn_Sc         = (uint8)((CanTSyn_Sc + 1U) & CANTSYN_SC_MASK);
    CanTSyn_LastSyncMs = nowMs;

    pdu[0] = CANTSYN_TYPE_SYNC;
    /* pdu[1] = User Byte 1（既定値 0） */
    pdu[2] = (uint8)((CANTSYN_TIME_DOMAIN_ID << 4U) | CanTSyn_Sc);
    /* pdu[3] = User Byte 0（既定値 0） */
    CanTSyn_WriteBe32(&pdu[4], secPart);

    if (CanTSyn_TransmitPdu(pdu) == E_OK)
    {
        CanTSyn_WaitStartMs = nowMs;
        CanTSyn_State       = CANTSYN_STATE_WAIT_SYNC_CONF;
        DET_LOGD(TAG, "SYNC sc=%u sec=%lu", (unsigned)CanTSyn_Sc, (unsigned long)secPart);
    }
    else
    {
        DET_LOGW(TAG, "SYNC transmit rejected sc=%u", (unsigned)CanTSyn_Sc);
    }
}

/* ----------------------------------------------------------------------
 * CanTSyn_SendFup
 * ---------------------------------------------------------------------- */

/**
 * \brief   FUP メッセージを組み立てて CanIf_Transmit() へ渡す。
 *
 * \details T4 = (T0 の ns 部分) + (SYNC を CanIf へ渡してからの経過時間)
 *          （[SWS_CanTSyn_00045]）。T4 が 1 秒以上なら、超えた秒数を OVS
 *          （最大 3）へ、残りを SyncTimeNSec へ書く（[SWS_CanTSyn_00047]）。
 *          SC は SYNC と同じ値にする（[SWS_CanTSyn_00049]）。渡せなかった場合は
 *          待機へ戻り、次の周期で SYNC からやり直す。
 *
 * \param[in]  nowMs  現在時刻 [ms]（SYNC の送信完了通知を受けた時刻）。
 */
static void CanTSyn_SendFup(unsigned long nowMs)
{
    uint8   pdu[CANTSYN_DLC] = { 0U };
    uint32  elapsedMs = (uint32)(nowMs - CanTSyn_WaitStartMs);
    /* T0 の ns 部分。SYNC の送信要求時刻 T0 は CanTSyn_LastSyncMs に残っている。 */
    uint32  t0Ns      = (uint32)((CanTSyn_LastSyncMs % CANTSYN_MS_PER_SEC) * CANTSYN_NS_PER_MS);
    uint32  t4Ns      = t0Ns + (elapsedMs * (uint32)CANTSYN_NS_PER_MS);
    uint32  ovs       = t4Ns / (uint32)CANTSYN_NS_PER_SEC;
    uint32  nsPart    = t4Ns % (uint32)CANTSYN_NS_PER_SEC;

    if (ovs > CANTSYN_OVS_MAX)
    {
        ovs = CANTSYN_OVS_MAX;
    }

    pdu[0] = CANTSYN_TYPE_FUP;
    /* pdu[1] = User Byte 2（既定値 0） */
    pdu[2] = (uint8)((CANTSYN_TIME_DOMAIN_ID << 4U) | CanTSyn_Sc);
    pdu[3] = (uint8)(ovs & 0x03U);   /* SGW=0（SyncToGTM）、OVS=Bit1-0 */
    CanTSyn_WriteBe32(&pdu[4], nsPart);

    if (CanTSyn_TransmitPdu(pdu) == E_OK)
    {
        CanTSyn_WaitStartMs = nowMs;
        CanTSyn_State       = CANTSYN_STATE_WAIT_FUP_CONF;
        DET_LOGD(TAG, "FUP sc=%u ns=%lu ovs=%lu", (unsigned)CanTSyn_Sc,
                 (unsigned long)nsPart, (unsigned long)ovs);
    }
    else
    {
        DET_LOGW(TAG, "FUP transmit rejected sc=%u", (unsigned)CanTSyn_Sc);
        CanTSyn_State = CANTSYN_STATE_IDLE;
    }
}

/* ----------------------------------------------------------------------
 * CanTSyn_TransmitPdu
 * ---------------------------------------------------------------------- */

/**
 * \brief   組み立てた SYNC/FUP（CANTSYN_DLC バイト）を CanIf_Transmit() へ渡す。
 *
 * \param[in]  pdu  送信するデータ（CANTSYN_DLC バイト）。
 * \return     CanIf_Transmit() の戻り値（E_OK=受け付けられた）。
 */
static Std_ReturnType CanTSyn_TransmitPdu(uint8* pdu)
{
    PduInfoType pduInfo = {
        .SduDataPtr = pdu,
        .SduLength  = CANTSYN_DLC
    };

    return CanIf_Transmit(CANTSYN_CANIF_TX_PDU_ID, &pduInfo);
}

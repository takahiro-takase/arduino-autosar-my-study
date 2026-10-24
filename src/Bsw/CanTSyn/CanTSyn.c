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

#include <stdint.h>

#include "CanTSyn.h"
#include "CanTSyn_Cfg.h"
#include "CanIf.h"
#include "StbM.h"
#include "Det.h"
#include "Os.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "CanTSyn"

/** 1 s あたりの ns 数。 */
#define CANTSYN_NS_PER_SEC  1000000000UL

/** SC（シーケンスカウンタ）の値域（4bit、[SWS_CanTSyn_00048]）。 */
#define CANTSYN_SC_MASK  0x0FU

/** SYNC 送信時に T0 と T0raw を同じ瞬間に読めるまでの最大試行回数（tick をまたいだ場合に読み直す）。 */
#define CANTSYN_T0_READ_TRIES  3U

/** OVS（秒のオーバーフロー、FUP Byte3 の Bit1-0）の最大値。 */
#define CANTSYN_OVS_MAX  3UL

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/** 受信（時間スレーブ）シーケンスの内部状態。 */
typedef enum
{
    CANTSYN_RX_STATE_IDLE = 0,   /**< SYNC を待っている */
    CANTSYN_RX_STATE_WAIT_FUP    /**< SYNC を受けた。対の FUP を待っている */
} CanTSyn_RxStateType;

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

/** 現在の SYNC に載せた T0 の ns 部分 [ns]（StbM の全域時間から取る。FUP の T4 計算用）。 */
static uint32 CanTSyn_T0Ns;

/** SYNC を CanIf へ渡した時点の仮想ローカル時間（StbM_GetCurrentTimeRaw()、T0raw）。 */
static StbM_TimeStampRawType CanTSyn_T0Raw;

/** 受信（時間スレーブ）の状態。 */
static CanTSyn_RxStateType CanTSyn_RxState;

/** 受け付けた直近の SYNC の SC（SC の増分の検証用）。CanTSyn_RxHaveSc が 0 の間は無効。 */
static uint8 CanTSyn_RxLastSc;
static uint8 CanTSyn_RxHaveSc;

/** TIMEOUT の間に、SC の飛び幅の検査を省略する権利を既に使ったか（[SWS_CanTSyn_00079]。時間ベースが
 *  TIMEOUT から復帰したら、また使えるようにする）。 */
static boolean CanTSyn_RxTimeoutSkipUsed;

/** 待っている FUP の対になる SYNC の SC、秒（T0）、受信時の仮想ローカル時間（T2raw）、受信時刻 [ms]。 */
static uint8                 CanTSyn_RxSyncSc;
static uint32                CanTSyn_RxSyncSec;
static StbM_TimeStampRawType CanTSyn_RxT2Raw;
static unsigned long         CanTSyn_RxWaitStartMs;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static unsigned long CanTSyn_GetNowMs(void);
static void CanTSyn_SendSync(unsigned long nowMs);
static void CanTSyn_SendFup(unsigned long nowMs);
static void CanTSyn_ClearPdu(uint8* pdu);
static void CanTSyn_WriteBe32(uint8* dst, uint32 value);
static Std_ReturnType CanTSyn_TransmitPdu(uint8* pdu);
static uint8 CanTSyn_Crc8H2FUpdate(uint8 crc, uint8 data);
static uint8 CanTSyn_CalcFrameCrc(const uint8* frame, uint8 dataId);
static void CanTSyn_SetCrc(uint8* pdu, uint8 dataId);
static uint32 CanTSyn_ReadBe32(const uint8* src);
static boolean CanTSyn_RxCrcOk(const uint8* data, uint8 dataId);
static void CanTSyn_RxHandleSync(const uint8* data, unsigned long nowMs);
static void CanTSyn_RxHandleFup(const uint8* data, unsigned long nowMs);

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
    CanTSyn_T0Ns          = 0U;
    CanTSyn_T0Raw         = 0U;
    CanTSyn_RxState       = CANTSYN_RX_STATE_IDLE;
    CanTSyn_RxHaveSc      = 0U;
    CanTSyn_RxTimeoutSkipUsed = FALSE;
    CanTSyn_RxLastSc      = 0U;
    CanTSyn_RxSyncSc      = 0U;
    CanTSyn_RxSyncSec     = 0U;
    CanTSyn_RxT2Raw       = 0U;
    CanTSyn_RxWaitStartMs = 0UL;
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

    if ((CanTSyn_RxState == CANTSYN_RX_STATE_WAIT_FUP)
        && ((nowMs - CanTSyn_RxWaitStartMs) >= CANTSYN_RX_FOLLOW_UP_TIMEOUT_MS))
    {
        /* [SWS_CanTSyn_00063]: SYNC のあと FUP が待ち時間内に来なかった。その SYNC を捨てる。 */
        DET_LOGD(TAG, "RX FUP timeout sc=%u", (unsigned)CanTSyn_RxSyncSc);
        CanTSyn_RxState = CANTSYN_RX_STATE_IDLE;
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

/* ----------------------------------------------------------------------
 * CanTSyn_RxIndication
 * ---------------------------------------------------------------------- */

/**
 * \brief   時刻同期メッセージ（SYNC/FUP）の受信を通知する（CanIf から呼ばれる、時間スレーブ側）。
 *
 * \details SYNC を受けると、StbM の時間ベース 1 の仮想ローカル時間 T2raw を控えて FUP を待つ。
 *          対応する FUP（SC が同じ）を待ち時間内に受けると、
 *          全域時間 = T0（SYNC の秒）+ T4（FUP の OVS とナノ秒）+ T3diff（T2raw からの経過時間）を
 *          求め、StbM_BusSetGlobalTime() で時間ベース 1 へ反映する（[SWS_CanTSyn_00073]）。
 *          次のものは無視する: 時間領域が違うフレーム、Type が CANTSYN_RX_CRC_VALIDATED の設定と
 *          合わないもの、CRC が合わないもの、SC の増分が 0 または CANTSYN_RX_SC_JUMP_WIDTH を
 *          超える SYNC、SC が対の SYNC と違う FUP、待ち時間（CANTSYN_RX_FOLLOW_UP_TIMEOUT_MS）を
 *          超えて届いた FUP、SYNC を受けていない FUP。
 *
 * \param[in]  RxPduId     受信した PDU の ID（CANTSYN_RX_PDU_ID 以外は
 *                         CANTSYN_E_INVALID_PDUID を DET へ報告し何もしない）。
 * \param[in]  PduInfoPtr  受信データ。NULL またはデータポインタが NULL なら
 *                         CANTSYN_E_NULL_POINTER を DET へ報告する。DLC が足りないものは無視する。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00096}
 * \ServiceID      {0x42}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (CanTSyn_Initialized == 0U)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_RX_INDICATION, CANTSYN_E_UNINIT);
        return;
    }

    if (RxPduId != CANTSYN_RX_PDU_ID)
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_RX_INDICATION, CANTSYN_E_INVALID_PDUID);
        return;
    }

    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL))
    {
        (void)Det_ReportError(CANTSYN_MODULE_ID, 0U, CANTSYN_API_ID_RX_INDICATION, CANTSYN_E_NULL_POINTER);
        return;
    }

    const uint8* data = PduInfoPtr->SduDataPtr;

    if ((PduInfoPtr->SduLength < CANTSYN_DLC)
        || ((uint8)(data[2] >> 4U) != CANTSYN_RX_TIME_DOMAIN_ID))
    {
        return;   /* DLC が足りない、または別の時間領域のフレーム */
    }

    if (data[0] == CANTSYN_RX_TYPE_SYNC)
    {
        CanTSyn_RxHandleSync(data, CanTSyn_GetNowMs());
    }
    else if (data[0] == CANTSYN_RX_TYPE_FUP)
    {
        CanTSyn_RxHandleFup(data, CanTSyn_GetNowMs());
    }
    else
    {
        /* CANTSYN_RX_CRC_VALIDATED の設定と合わない Type は受け付けない（[SWS_CanTSyn_00057]/00058） */
    }
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTSyn_RxHandleSync
 * ---------------------------------------------------------------------- */

/**
 * \brief   受信した SYNC を検証し、FUP を待つ状態へ移る。
 *
 * \details 設定で CRC を検証する場合は CRC（Byte2〜Byte7 + DataID）が合うことを確認する。
 *          SC の増分（直前に受け付けた SYNC との差）が 1〜CANTSYN_RX_SC_JUMP_WIDTH でなければ
 *          無視する（[SWS_CanTSyn_00078]）。ただし起動直後と、時間ベース 1 が TIMEOUT の後の
 *          最初の SYNC（1 回だけ。TIMEOUT から復帰するまでの 2 つ目以降は検査する）は検査しない
 *          （[SWS_CanTSyn_00079]）。直前の SC は、無視した SYNC でも更新する
 *          （1 回取りこぼしても、次の SYNC から復帰できるようにするため）。
 *          受け付けたら、秒（T0）と、StbM の時間ベース 1 の仮想ローカル時間 T2raw を控える。
 *
 * \param[in]  data   受信した SYNC（CANTSYN_DLC バイト）。
 * \param[in]  nowMs  現在時刻 [ms]（FUP の待ち時間の起点）。
 */
static void CanTSyn_RxHandleSync(const uint8* data, unsigned long nowMs)
{
    uint8 sc = (uint8)(data[2] & CANTSYN_SC_MASK);

    if ((CANTSYN_RX_CRC_VALIDATED != 0U) && (!CanTSyn_RxCrcOk(data, CANTSYN_RX_DATA_ID_SYNC(sc))))
    {
        return;
    }

    /* [SWS_CanTSyn_00079]: 起動直後、または StbM の時間ベースが TIMEOUT（更新が途絶えた）の後の
     * 最初の SYNC は、SC の飛び幅を検査しない（SC の再同期のため。途絶はタイムアウト監視で検出済み） */
    StbM_TimeBaseStatusType syncStatus   = 0U;
    StbM_TimeBaseStatusType offsetStatus = 0U;
    boolean skipJumpCheck = (CanTSyn_RxHaveSc == 0U);
    if (StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &syncStatus, &offsetStatus) == E_OK)
    {
        if ((syncStatus & STBM_TIMEBASE_STATUS_TIMEOUT) == 0U)
        {
            CanTSyn_RxTimeoutSkipUsed = FALSE;   /* TIMEOUT から復帰した。次の TIMEOUT でまた使える */
        }
        else if (!CanTSyn_RxTimeoutSkipUsed)
        {
            skipJumpCheck             = TRUE;    /* TIMEOUT 後の最初の SYNC だけ、検査を省略する */
            CanTSyn_RxTimeoutSkipUsed = TRUE;
        }
        else
        {
            /* 省略は使い切った。FUP が揃わず TIMEOUT のままの間は、通常どおり検査する */
        }
    }

    uint8 jump = (uint8)((sc - CanTSyn_RxLastSc) & CANTSYN_SC_MASK);
    boolean jumpOk = skipJumpCheck || ((jump >= 1U) && (jump <= CANTSYN_RX_SC_JUMP_WIDTH));

    CanTSyn_RxLastSc = sc;
    CanTSyn_RxHaveSc = 1U;

    if ((!jumpOk) || (StbM_GetCurrentTimeRaw(STBM_TIME_BASE_SLAVE, &CanTSyn_RxT2Raw) != E_OK))
    {
        CanTSyn_RxState = CANTSYN_RX_STATE_IDLE;
        return;
    }

    CanTSyn_RxSyncSc      = sc;
    CanTSyn_RxSyncSec     = CanTSyn_ReadBe32(&data[4]);
    CanTSyn_RxWaitStartMs = nowMs;
    CanTSyn_RxState       = CANTSYN_RX_STATE_WAIT_FUP;
}

/* ----------------------------------------------------------------------
 * CanTSyn_RxHandleFup
 * ---------------------------------------------------------------------- */

/**
 * \brief   受信した FUP を検証し、全域時間を求めて StbM の時間ベース 1 へ反映する。
 *
 * \details SYNC を受けていない、CRC が合わない（設定で検証する場合）、SC が対の SYNC と
 *          違う、待ち時間を超えた、ナノ秒が 10^9 以上（範囲外）、のいずれかなら捨てる（SC が違うときは、保留中の SYNC も捨てる、
 *          [SWS_CanTSyn_00076]）。受け付けたら、
 *            全域時間 = T0（SYNC の秒）+ T4（OVS の秒 + FUP のナノ秒）+ T3diff（T2raw からの経過時間）
 *          を StbM_BusSetGlobalTime() で時間ベース 1 へ反映する（[SWS_CanTSyn_00073]/00075/00064）。
 *          FUP の SGW は SYNC_TO_GATEWAY ビットに反映する（[SWS_CanTSyn_00133]）。
 *
 * \param[in]  data   受信した FUP（CANTSYN_DLC バイト）。
 * \param[in]  nowMs  現在時刻 [ms]（FUP の待ち時間の判定用）。
 */
static void CanTSyn_RxHandleFup(const uint8* data, unsigned long nowMs)
{
    uint8 sc = (uint8)(data[2] & CANTSYN_SC_MASK);

    if (CanTSyn_RxState != CANTSYN_RX_STATE_WAIT_FUP)
    {
        return;   /* 対の SYNC を受けていない FUP */
    }

    CanTSyn_RxState = CANTSYN_RX_STATE_IDLE;   /* この SYNC は、この FUP で使い切る（捨てる場合も含む） */

    if ((CANTSYN_RX_CRC_VALIDATED != 0U) && (!CanTSyn_RxCrcOk(data, CANTSYN_RX_DATA_ID_FUP(sc))))
    {
        return;
    }

    if ((sc != CanTSyn_RxSyncSc)
        || ((nowMs - CanTSyn_RxWaitStartMs) >= CANTSYN_RX_FOLLOW_UP_TIMEOUT_MS))
    {
        return;
    }

    /* [SWS_CanTSyn_00086] 項目 5: FUP のナノ秒は、StbM_TimeStampType.nanoseconds の範囲（10^9 未満）に収まること */
    if (CanTSyn_ReadBe32(&data[4]) >= (uint32)CANTSYN_NS_PER_SEC)
    {
        return;
    }

    StbM_TimeStampRawType t3Diff = 0U;
    if (StbM_GetCurrentTimeDiff(STBM_TIME_BASE_SLAVE, CanTSyn_RxT2Raw, &t3Diff) != E_OK)
    {
        return;
    }

    uint64_t totalNs = ((uint64_t)CanTSyn_RxSyncSec * (uint64_t)CANTSYN_NS_PER_SEC)
                     + ((uint64_t)(data[3] & 0x03U) * (uint64_t)CANTSYN_NS_PER_SEC)   /* OVS */
                     + (uint64_t)CanTSyn_ReadBe32(&data[4])                          /* T4 のナノ秒 */
                     + (uint64_t)t3Diff;
    uint64_t seconds = totalNs / (uint64_t)CANTSYN_NS_PER_SEC;

    StbM_TimeStampType   ts;
    StbM_MeasurementType measure;

    ts.timeBaseStatus = (uint8)(((data[3] & 0x04U) != 0U) ? STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY : 0U);
    ts.nanoseconds    = (uint32)(totalNs % (uint64_t)CANTSYN_NS_PER_SEC);
    ts.seconds        = (uint32)(seconds & 0xFFFFFFFFULL);
    ts.secondsHi      = (uint16)((seconds >> 32U) & 0xFFFFULL);
    measure.pathDelay = 0U;   /* [SWS_CanTSyn_00115] */

    if (StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &ts, NULL, &measure) == E_OK)
    {
        DET_LOGD(TAG, "RX time updated sc=%u sec=%lu", (unsigned)sc, (unsigned long)ts.seconds);
    }
}

/* ----------------------------------------------------------------------
 * CanTSyn_ReadBe32
 * ---------------------------------------------------------------------- */

/**
 * \brief   4 バイトのビッグエンディアンの値を読む（[SWS_CanTSyn_00008]）。
 *
 * \param[in]  src  読み出し元（4 バイト以上）。
 * \return     値。
 */
static uint32 CanTSyn_ReadBe32(const uint8* src)
{
    return ((uint32)src[0] << 24U) | ((uint32)src[1] << 16U) | ((uint32)src[2] << 8U) | (uint32)src[3];
}

/* ----------------------------------------------------------------------
 * CanTSyn_RxCrcOk
 * ---------------------------------------------------------------------- */

/**
 * \brief   受信した SYNC/FUP の CRC（Byte1）が正しいか確認する。
 *
 * \details CRC は Byte2〜Byte7 に DataID を続けて計算する（送信側の CanTSyn_SetCrc と同じ、
 *          [SWS_CanTSyn_00055]）。
 *
 * \param[in]  data    受信したフレーム（CANTSYN_DLC バイト）。
 * \param[in]  dataId  この SC・メッセージ種別の DataID（CANTSYN_RX_DATA_ID_SYNC/FUP）。
 * \return     TRUE=CRC が一致した。FALSE=一致しない。
 */
static boolean CanTSyn_RxCrcOk(const uint8* data, uint8 dataId)
{
    return (CanTSyn_CalcFrameCrc(data, dataId) == data[1]) ? TRUE : FALSE;
}

/* ----------------------------------------------------------------------
 * CanTSyn_CalcFrameCrc
 * ---------------------------------------------------------------------- */

/**
 * \brief   SYNC/FUP の CRC（Byte1 に載せる値）を計算する。送信（SetCrc）と受信（RxCrcOk）で共通。
 *
 * \details Byte2〜Byte7 に続けて DataID を投入し、最後に 0xFF と XOR する（[SWS_CanTSyn_00055]）。
 *
 * \param[in]  frame   SYNC/FUP（CANTSYN_DLC バイト。Byte1 は読まない）。
 * \param[in]  dataId  この SC・メッセージ種別の DataID。
 * \return     CRC。
 */
static uint8 CanTSyn_CalcFrameCrc(const uint8* frame, uint8 dataId)
{
    uint8 crc = 0xFFU;   /* 初期値 */

    for (uint8 i = 2U; i < CANTSYN_DLC; i++)   /* Byte2〜Byte7 */
    {
        crc = CanTSyn_Crc8H2FUpdate(crc, frame[i]);
    }
    crc = CanTSyn_Crc8H2FUpdate(crc, dataId);  /* DataID を最後に */

    return (uint8)(crc ^ 0xFFU);               /* 最終 XOR */
}

/* ----------------------------------------------------------------------
 * CanTSyn_GetNowMs
 * ---------------------------------------------------------------------- */

/**
 * \brief   現在時刻 [ms] を返す（Os カウンタ SYSTEM_COUNTER の tick 値）。
 * \details 送信周期・各種タイムアウトの判定に使う ms（StbM の全域時間とは別）。
 *          millis() を直接呼ばず Os の GetCounterValue() を
 *          介する理由は CanNm.c の同名関数と同じ。
 */
static unsigned long CanTSyn_GetNowMs(void)
{
    TickType now = 0U;
    (void)GetCounterValue(SYSTEM_COUNTER, &now);
    return (unsigned long)now;
}

/* ----------------------------------------------------------------------
 * CanTSyn_ClearPdu
 * ---------------------------------------------------------------------- */

/**
 * \brief   SYNC/FUP 用のバッファ（CANTSYN_DLC バイト）を 0 で埋める。
 *
 * \details User Byte など、値を決めていないバイトを既定値 0 にしておくために、組み立ての前に呼ぶ。
 *          配列の一部だけを初期化子で書く方法は、MISRA C:2012 Rule 9.3 に反するため、ループで埋める。
 *
 * \param[out] pdu  埋めるバッファ（CANTSYN_DLC バイト）。
 */
static void CanTSyn_ClearPdu(uint8* pdu)
{
    for (uint8 i = 0U; i < CANTSYN_DLC; i++)
    {
        pdu[i] = 0U;
    }
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
 * \details T0 = StbM の時間ベース 0 の現在の全域時間（[SWS_CanTSyn_00045]）。秒部分を SyncTimeSec へ書き、
 *          ns 部分と、そのときの仮想ローカル時間 T0raw を FUP の T4 計算用に保持する。時間ベースが
 *          全域時間に同期済みでなければ送らない（[SWS_CanTSyn_00028]）。SC は送信要求のたびに
 *          1進める（[SWS_CanTSyn_00048]。送信に失敗した場合も進める）。渡せた場合は送信完了通知待ちへ
 *          移り、渡せなかった場合は待機のまま、次の周期でやり直す。
 *
 * \param[in]  nowMs  現在時刻 [ms]（T0）。
 */
static void CanTSyn_SendSync(unsigned long nowMs)
{
    uint8              pdu[CANTSYN_DLC];
    StbM_TimeStampType t0;
    StbM_UserDataType  userData;

    CanTSyn_ClearPdu(pdu);
    CanTSyn_LastSyncMs = nowMs;

    /* [SWS_CanTSyn_00028]: 時間ベースが全域時間に同期済み（GLOBAL_TIME_BASE）のときだけ送る */
    /* T0 と T0raw は同じ瞬間でなければ T4 がずれる。tick をまたいだ読み取り（前後の Raw が違う）は読み直す */
    StbM_TimeStampRawType rawBefore = 0U;
    StbM_TimeStampRawType rawAfter  = 0U;
    uint8                 tries     = 0U;
    do
    {
        if ((StbM_GetCurrentTimeRaw(STBM_TIME_BASE_MASTER, &rawBefore) != E_OK)
            || (StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &t0, &userData) != E_OK)
            || (StbM_GetCurrentTimeRaw(STBM_TIME_BASE_MASTER, &rawAfter) != E_OK))
        {
            return;
        }
        tries++;
    } while ((rawBefore != rawAfter) && (tries < CANTSYN_T0_READ_TRIES));

    CanTSyn_T0Raw = rawBefore;

    if ((t0.timeBaseStatus & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE) == 0U)
    {
        return;
    }

    uint32 secPart = t0.seconds;   /* 秒の下位 32bit を載せる（[SWS_CanTSyn_00015]） */

    CanTSyn_Sc = (uint8)((CanTSyn_Sc + 1U) & CANTSYN_SC_MASK);
    CanTSyn_T0Ns = t0.nanoseconds;

    pdu[0] = CANTSYN_TX_TYPE_SYNC;
    /* pdu[1] = CRC なしなら User Byte 1（既定値 0）、CRC 付きなら CRC（下の CanTSyn_SetCrc） */
    pdu[2] = (uint8)((CANTSYN_TIME_DOMAIN_ID << 4U) | CanTSyn_Sc);
    /* pdu[3] = User Byte 0（既定値 0） */
    CanTSyn_WriteBe32(&pdu[4], secPart);
    CanTSyn_SetCrc(pdu, CANTSYN_DATA_ID_SYNC(CanTSyn_Sc));

    if (CanTSyn_TransmitPdu(pdu) == E_OK)
    {
        CanTSyn_WaitStartMs = nowMs;
        CanTSyn_State       = CANTSYN_STATE_WAIT_SYNC_CONF;
        DET_LOGD(TAG, "SYNC sc=%u sec=%lu", (unsigned)CanTSyn_Sc, (unsigned long)secPart);
    }
    else
    {
        /* SILENT_COM（Bus-Off 中・バススリープの準備中）は CanIf が TX を拒否するのが正常のため、WARN にしない */
        DET_LOGD(TAG, "SYNC transmit rejected sc=%u", (unsigned)CanTSyn_Sc);
    }
}

/* ----------------------------------------------------------------------
 * CanTSyn_SendFup
 * ---------------------------------------------------------------------- */

/**
 * \brief   FUP メッセージを組み立てて CanIf_Transmit() へ渡す。
 *
 * \details T4 = (T0 の ns 部分) + (SYNC を CanIf へ渡してからの経過時間。StbM_GetCurrentTimeDiff() で測る)
 *          （[SWS_CanTSyn_00045]）。T4 が 1 秒以上なら、超えた秒数を OVS
 *          （最大 3）へ、残りを SyncTimeNSec へ書く（[SWS_CanTSyn_00047]）。
 *          SC は SYNC と同じ値にする（[SWS_CanTSyn_00049]）。渡せなかった場合は
 *          待機へ戻り、次の周期で SYNC からやり直す。
 *
 * \param[in]  nowMs  現在時刻 [ms]（SYNC の送信完了通知を受けた時刻）。
 */
static void CanTSyn_SendFup(unsigned long nowMs)
{
    uint8                 pdu[CANTSYN_DLC];
    StbM_TimeStampRawType t0Diff = 0U;

    CanTSyn_ClearPdu(pdu);

    /* [SWS_CanTSyn_00045]: T4 = T0 の ns 部分 + (SYNC を渡してからの経過時間)。経過時間は StbM が測る。 */
    if (StbM_GetCurrentTimeDiff(STBM_TIME_BASE_MASTER, CanTSyn_T0Raw, &t0Diff) != E_OK)
    {
        CanTSyn_State = CANTSYN_STATE_IDLE;
        return;
    }

    uint32  t4Ns   = CanTSyn_T0Ns + t0Diff;
    uint32  ovs    = t4Ns / (uint32)CANTSYN_NS_PER_SEC;
    uint32  nsPart = t4Ns % (uint32)CANTSYN_NS_PER_SEC;

    if (ovs > CANTSYN_OVS_MAX)
    {
        ovs = CANTSYN_OVS_MAX;
    }

    pdu[0] = CANTSYN_TX_TYPE_FUP;
    /* pdu[1] = CRC なしなら User Byte 2（既定値 0）、CRC 付きなら CRC（下の CanTSyn_SetCrc） */
    pdu[2] = (uint8)((CANTSYN_TIME_DOMAIN_ID << 4U) | CanTSyn_Sc);
    pdu[3] = (uint8)(ovs & 0x03U);   /* SGW=0（SyncToGTM）、OVS=Bit1-0 */
    CanTSyn_WriteBe32(&pdu[4], nsPart);
    CanTSyn_SetCrc(pdu, CANTSYN_DATA_ID_FUP(CanTSyn_Sc));

    if (CanTSyn_TransmitPdu(pdu) == E_OK)
    {
        CanTSyn_WaitStartMs = nowMs;
        CanTSyn_State       = CANTSYN_STATE_WAIT_FUP_CONF;
        DET_LOGD(TAG, "FUP sc=%u ns=%lu ovs=%lu", (unsigned)CanTSyn_Sc,
                 (unsigned long)nsPart, (unsigned long)ovs);
    }
    else
    {
        DET_LOGD(TAG, "FUP transmit rejected sc=%u", (unsigned)CanTSyn_Sc);
        CanTSyn_State = CANTSYN_STATE_IDLE;
    }
}

/* ----------------------------------------------------------------------
 * CanTSyn_Crc8H2F
 * ---------------------------------------------------------------------- */

/**
 * \brief   CRC-8H2F（多項式 0x2F、反転なし）へ 1 バイト分を投入する。
 *
 * \details 呼び出し側が、初期値 0xFF から始め、全バイトを投入し終えたあとに最終 XOR 0xFF を
 *          掛ける（CanTSyn_SetCrc 参照）。この手順は AUTOSAR の Crc_CalculateCRC8H2F() と
 *          同じ結果になる（[SWS_CanTSyn_00050]。検査値: "123456789" → 0xDF）。本プロジェクトには
 *          Crc モジュールが無いため、本モジュール内に持つ。
 *
 * \param[in]  crc   投入前の CRC（途中の値）。
 * \param[in]  data  投入する 1 バイト。
 * \return     投入後の CRC（途中の値）。
 */
static uint8 CanTSyn_Crc8H2FUpdate(uint8 crc, uint8 data)
{
    uint8 result = (uint8)(crc ^ data);   /* [MISRA 17.8] 引数 crc は書き換えず、ローカル変数で計算する */
    for (uint8 bit = 0U; bit < 8U; bit++)
    {
        result = ((result & 0x80U) != 0U) ? (uint8)((uint8)(result << 1U) ^ 0x2FU) : (uint8)(result << 1U);
    }

    return result;
}

/* ----------------------------------------------------------------------
 * CanTSyn_SetCrc
 * ---------------------------------------------------------------------- */

/**
 * \brief   CRC 付きの設定なら、SYNC/FUP の Byte1 へ CRC を書く（なしなら何もしない）。
 *
 * \details CRC は Byte2〜Byte7 に続けて DataID を最後に計算する（[SWS_CanTSyn_00055]）。
 *
 * \param[in,out] pdu     組み立て済みの SYNC/FUP（Byte1 以外が完成していること）。
 * \param[in]     dataId  この SC・メッセージ種別の DataID（CANTSYN_DATA_ID_SYNC/FUP）。
 */
static void CanTSyn_SetCrc(uint8* pdu, uint8 dataId)
{
#if (CANTSYN_TX_CRC_SECURED != 0U)
    pdu[1] = CanTSyn_CalcFrameCrc(pdu, dataId);
#else
    (void)pdu;
    (void)dataId;
#endif
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

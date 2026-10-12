/**
 * \file    CanTp.c
 * \brief   CAN トランスポートプロトコル実装 (AUTOSAR SWS_CanTp 準拠, ISO 15765-2)
 * \details CanIf/PduR と DCM の間で ISO 15765-2 フレームの組立・分割を担う。
 *
 *          対応フレームタイプ:
 *            SF (Single Frame)      PCI 上位ニブル = 0x0
 *            FF (First Frame)       PCI 上位ニブル = 0x1
 *            CF (Consecutive Frame) PCI 上位ニブル = 0x2
 *            FC (Flow Control)      PCI 上位ニブル = 0x3
 *
 *          上位層（Dcm）との受け渡しは TP の API で行う。CanTp は組み立て・分割用の
 *          バッファを持たず、フレームごとに上位層とやり取りする（仕様では PduR を経由する
 *          が、本プロジェクトは PduR の TP の経路を持たないので、Dcm を直接呼ぶ）。
 *            受信: Dcm_StartOfReception()（SF/FF の受信時）→ Dcm_CopyRxData()（各フレームの
 *                  データ）→ Dcm_TpRxIndication()（完了・中断）
 *            送信: CanTp_Transmit()（長さだけを受け取る）→ Dcm_CopyTxData()（SF/FF/各 CF の
 *                  データ）→ Dcm_TpTxConfirmation()（完了・中断）
 *
 *          RX 状態マシン:
 *            IDLE → SF 受信 → StartOfReception/CopyRxData/TpRxIndication(E_OK) → IDLE
 *            IDLE → FF 受信 → StartOfReception → FC 送信 → CopyRxData（FF のデータ）→ WAIT_CF
 *            WAIT_CF → CF 受信 → CopyRxData → 完成なら TpRxIndication(E_OK) → IDLE
 *                             → 未完成なら WAIT_CF (N_Cr リセット)
 *            WAIT_CF → N_Cr タイムアウト → TpRxIndication(E_NOT_OK) → IDLE (中断)
 *            WAIT_CF → 別の SF/FF 受信 → 進行中の受信を TpRxIndication(E_NOT_OK) で中断し、
 *                                     新規受信として処理する (SWS_CanTp_00124)
 *
 *          TX 状態マシン:
 *            IDLE → CanTp_Transmit (<=7B) → CopyTxData → SF 送信 → TpTxConfirmation(E_OK)
 *                                          （失敗なら E_NOT_OK を返し IDLE のまま。確認通知は出さない）
 *            IDLE → CanTp_Transmit (>7B) → CopyTxData → FF 送信 → WAIT_FC（送信失敗なら E_NOT_OK を返し中断）
 *            WAIT_FC → FC(CTS) 受信 → SEND_CF
 *            WAIT_FC → N_Bs タイムアウト → TpTxConfirmation(E_NOT_OK) → IDLE (中断)
 *            SEND_CF → CopyTxData → CF 送信 (CanTp_MainFunction) → 完成なら TpTxConfirmation(E_OK) → IDLE
 *                    → BS 消費なら WAIT_FC
 *            SEND_CF → CF 送信が同期的に失敗し続け N_As タイムアウト → TpTxConfirmation(E_NOT_OK) → IDLE (中断)
 *                      （送信に失敗した CF は、CopyTxData をやり直さず、同じフレームを再送する）
 *
 *          CanTp_TxConfirmation（CanIf からの送信確認）について: 本実装は Can.c/CanIf.c の非同期 TX 確認経路が
 *          常に E_OK 固定（真の送信失敗を表現できない）であるため、
 *          CanTp_TxConfirmation() 自体は no-op のままとし、代わりに
 *          CanTp_SendFrame() の同期的な戻り値（PduR/CanIf/Can_Write が
 *          その場で拒否した場合）と上記 N_As タイムアウトで送信失敗を検出する。
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

#include "CanTp.h"
#include "PduR.h"
#include "Dcm_Cbk.h"
#include "Det.h"
#include "Os.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

 #define TAG "CanTp"

 /* -----------------------------------------------------------------------
 * ISO 15765-2 フレームタイプ定数
 * ----------------------------------------------------------------------- */
#define CANTP_FRAME_SF    0x0U   /**< Single Frame       */
#define CANTP_FRAME_FF    0x1U   /**< First Frame        */
#define CANTP_FRAME_CF    0x2U   /**< Consecutive Frame  */
#define CANTP_FRAME_FC    0x3U   /**< Flow Control       */

#define CANTP_FC_CTS      0x0U   /**< Continue To Send   */
#define CANTP_FC_WAIT     0x1U   /**< Wait               */
#define CANTP_FC_OVFLW    0x2U   /**< Overflow           */

#define CANTP_SF_MAX_DATA 7U     /**< SF: 最大 UDS ペイロード長    */
#define CANTP_FF_DATA     6U     /**< FF: 先頭データバイト数       */
#define CANTP_CF_DATA     7U     /**< CF: データバイト数           */
#define CANTP_MAX_MSG_LEN 4095U  /**< FF の長さ欄（12 bit）で表せる N-SDU の最大長 */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * RX/TX 状態型
 * ----------------------------------------------------------------------- */
typedef enum
{
    CANTP_RX_IDLE    = 0,
    CANTP_RX_WAIT_CF = 1
} CanTp_RxStateType;

typedef enum
{
    CANTP_TX_IDLE    = 0,
    CANTP_TX_WAIT_FC = 1,
    CANTP_TX_SEND_CF = 2
} CanTp_TxStateType;

/* -----------------------------------------------------------------------
 * モジュール内部状態
 * ----------------------------------------------------------------------- */

/** RX チャネル状態（データは持たず、フレームごとに Dcm_CopyRxData() で渡す） */
static struct {
    uint16            msgLen;    /**< FF で通知された総メッセージ長 */
    uint16            pos;       /**< Dcm へコピー済みのバイト数    */
    uint8             sn;        /**< 次に受信すべき CF シーケンス番号 */
    CanTp_RxStateType state;
    unsigned long     timer;     /**< N_Cr タイムアウト基点 (ms)   */
} CanTp_Rx;

/** TX チャネル状態（データは持たず、フレームごとに Dcm_CopyTxData() で取り出す） */
static struct {
    uint16            msgLen;    /**< 送信する総メッセージ長        */
    uint16            pos;       /**< 送信済み（フレームに詰めた）バイト数 */
    uint8             frameLen;  /**< 組み立て済みで未送信の CF のデータバイト数 */
    uint8             frameReady;/**< 1 = CanTp_TxFrameBuf に未送信の CF が組み立て済み
                                  *   （送信に失敗した CF を、CopyTxData をやり直さず再送する） */
    uint8             sn;        /**< 次の CF シーケンス番号 (1-15, 0...) */
    uint8             bs;        /**< FC で受信したブロックサイズ   */
    uint8             bsCnt;     /**< 現ブロック内残り送信数        */
    uint8             stMin;     /**< FC で受信した STmin (ms)      */
    CanTp_TxStateType state;
    unsigned long     bsTimer;   /**< N_Bs タイムアウト基点 (ms)   */
    unsigned long     cfTimer;   /**< STmin タイマ基点 (ms)        */
    unsigned long     asFailTimer; /**< N_As: CF 送信が連続失敗し始めた時刻 (ms)。
                                     *   0 = 現在失敗継続中ではない */
} CanTp_Tx;

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/** [SWS_CanTp_00031] 用の初期化済みフラグ。本実装は CanTp_ConfigType を
 *  保持しない簡略設計のため、他モジュールの ConfigPtr==NULL 相当として
 *  このフラグを使う。 */
static uint8 CanTp_Initialized = 0U;

/** TX フレーム送信用バッファ (SF/FF/CF 共用)。FC は、CF の組み立て済みフレーム（再送待ち）を壊さないよう、
 *  CanTp_SendFlowControl() の中の別のバッファを使う */
static uint8 CanTp_TxFrameBuf[8];

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static unsigned long CanTp_GetNowMs(void);

/* -----------------------------------------------------------------------
 * 内部関数プロトタイプ
 * ----------------------------------------------------------------------- */
static Std_ReturnType CanTp_SendFrame(uint8* frame);
static boolean CanTp_TxBusy(void);
static boolean CanTp_TxStallTimedOut(void);
static void CanTp_RxAbort(void);
static BufReq_ReturnType CanTp_CopyToUpper(const uint8* src, uint8 len);
static void CanTp_TxFinish(Std_ReturnType result);
static void CanTp_SendFlowControl(uint8 fs, uint8 bs, uint8 stMin);
static void CanTp_SendNextCF(void);
static uint8 CanTp_DecodeStMin(uint8 raw);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * CanTp_Init
 * ----------------------------------------------------------------------- */

/**
 * \brief   CanTp モジュールを初期化する。
 *
 * \details RX/TX チャネルの状態を IDLE にリセットする。
 *
 * \AUTOSARReq     {SWS_CanTp_00208}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_Init(const CanTp_ConfigType* CfgPtr)
{
    (void)CfgPtr; /* 本プロジェクトは post-build 設定を持たない（CanTp.h 参照） */
    CanTp_Rx.state  = CANTP_RX_IDLE;
    CanTp_Rx.msgLen = 0U;
    CanTp_Rx.pos    = 0U;
    CanTp_Tx.state       = CANTP_TX_IDLE;
    CanTp_Tx.msgLen      = 0U;
    CanTp_Tx.pos         = 0U;
    CanTp_Tx.frameReady  = 0U;
    CanTp_Tx.asFailTimer = 0UL;
    CanTp_Initialized = 1U;
    DET_LOGI(TAG, "Init ok");
}

/* -----------------------------------------------------------------------
 * CanTp_GetVersionInfo
 * ----------------------------------------------------------------------- */

void CanTp_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_GET_VERSION_INFO, CANTP_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = CANTP_VENDOR_ID;
    versioninfo->moduleID         = CANTP_MODULE_ID;
    versioninfo->sw_major_version = CANTP_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = CANTP_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = CANTP_SW_PATCH_VERSION;
}

/* -----------------------------------------------------------------------
 * CanTp_Shutdown
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_Transmit
 * ----------------------------------------------------------------------- */

/**
 * \brief   DCM の UDS 応答を CAN フレームに分割して送信する。
 *
 * \details PduInfoPtr->SduLength だけを使う（SduDataPtr は NULL でもよい）。データは、フレームを
 *          組み立てる時に Dcm_CopyTxData() で Dcm から取り出す。長さ <= 7 なら、取り出して SF を
 *          送信し、送信できたら Dcm_TpTxConfirmation(E_OK) を呼ぶ。>= 8 なら FF の分を取り出して
 *          FF 送信し、WAIT_FC へ遷移する（残りは各 CF の組み立ての時に取り出す）。
 *
 * \param[in]  TxSduId     TX N-SDU ID (CANTP_TX_SDU_ID)。
 * \param[in]  PduInfoPtr  送信する UDS 応答の長さ（SduLength）。NULL 禁止。
 *
 * \retval  E_OK      送信受け付け済み。
 * \retval  E_NOT_OK  NULL ポインタ、長さ 0 または超過、チャネルビジー、データの取り出し・フレームの送信の失敗。
 *                     （この場合、Dcm_TpTxConfirmation() は呼ばない）
 *
 * \AUTOSARReq     {SWS_CanTp_00212}
 * \ServiceID      {0x49}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanTp_Transmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr)
{
    if (!CanTp_Initialized)
    {
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_TRANSMIT, CANTP_E_UNINIT);
        return E_NOT_OK;
    }

    if (TxSduId != CANTP_TX_SDU_ID)
    {
        DET_LOGE(TAG, "TX E: invalid TxSduId=%u", (unsigned)TxSduId);
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_TRANSMIT, CANTP_E_INVALID_TX_ID);
        return E_NOT_OK;
    }

    /* データ（SduDataPtr）は使わない。送る長さだけを受け取り、データは Dcm_CopyTxData() で取り出す。 */
    if (PduInfoPtr == NULL)
    {
        DET_LOGE(TAG, "TX E: NULL PduInfoPtr");
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_TRANSMIT, CANTP_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    uint16 msgLen = (uint16)PduInfoPtr->SduLength;

    if ((msgLen == 0U) || (msgLen > (uint16)CANTP_MAX_MSG_LEN))
    {
        DET_LOGE(TAG, "TX E: invalid len");
        return E_NOT_OK;
    }

    /* [SWS_CanTp_00123]: TX チャネルがビジー（マルチフレーム送信が
     * CANTP_TX_WAIT_FC/CANTP_TX_SEND_CF で進行中）なら、フレーム種別
     * （SF/FF）に関わらず新規送信要求を拒否する。CanTp_TxFrameBuf は
     * SF/FF/CF が共有する単一の送信バッファであり、ここで SF だけ
     * チェックを素通りさせると、進行中の CF ストリームの最中に無関係な SF
     * フレームがそのままバスへ出てしまい、受信側の再組み立てを破壊する
     * （2026-08 のスペック監査で発見・修正。以前は FF 分岐にのみこの
     * チェックがあった）。 */
    if (CanTp_TxBusy())
    {
        DET_LOGE(TAG, "TX E: busy");
        return E_NOT_OK;
    }

    PduLengthType avail = 0U;

    if (msgLen <= (uint16)CANTP_SF_MAX_DATA)
    {
        /* ---- Single Frame ---- */
        PduInfoType src;
        src.SduDataPtr = &CanTp_TxFrameBuf[1];
        src.SduLength  = (PduLengthType)msgLen;
        if (Dcm_CopyTxData(CANTP_DCM_TX_PDU_ID, &src, NULL, &avail) != BUFREQ_OK)
        {
            DET_LOGE(TAG, "TX SF: CopyTxData failed");
            return E_NOT_OK;
        }
        CanTp_TxFrameBuf[0] = (uint8)msgLen;   /* SF PCI: 0x0N */
        uint8 i;
        for (i = (uint8)msgLen; i < CANTP_SF_MAX_DATA; i++)
        {
            CanTp_TxFrameBuf[1U + i] = 0x00U;
        }

        DET_LOGI(TAG, "TX SF len=%u", (unsigned)msgLen);

        if (CanTp_SendFrame(CanTp_TxFrameBuf) != E_OK)
        {
            /* FF 分岐と同様に送信失敗を呼び出し元(Dcm)へ伝える
             * (SWS_CanTp_00212 の定義どおり、要求が受理されなかった場合は
             * E_NOT_OK を返す。無条件に E_OK を返してはならない)。 */
            DET_LOGE(TAG, "TX SF FAIL");
            return E_NOT_OK;
        }
        Dcm_TpTxConfirmation(CANTP_DCM_TX_PDU_ID, E_OK);   /* SF は、送ったら終わり */
        return E_OK;
    }

    /* ---- Multi Frame: First Frame ---- */
    PduInfoType src;
    src.SduDataPtr = &CanTp_TxFrameBuf[2];
    src.SduLength  = (PduLengthType)CANTP_FF_DATA;
    if (Dcm_CopyTxData(CANTP_DCM_TX_PDU_ID, &src, NULL, &avail) != BUFREQ_OK)
    {
        DET_LOGE(TAG, "TX FF: CopyTxData failed");
        return E_NOT_OK;
    }

    CanTp_Tx.msgLen     = msgLen;
    CanTp_Tx.pos        = (uint16)CANTP_FF_DATA;   /* FF で送信済みバイト数 */
    CanTp_Tx.sn         = 1U;
    CanTp_Tx.frameReady = 0U;

    /* FF PCI: [0x1H, 0xLL] + 先頭 6 バイト（データは CopyTxData で書き込み済み） */
    CanTp_TxFrameBuf[0] = (uint8)(0x10U | (uint8)((msgLen >> 8U) & 0x0FU));
    CanTp_TxFrameBuf[1] = (uint8)(msgLen & 0xFFU);

    DET_LOGI(TAG, "TX FF len=%u", (unsigned)msgLen);

    if (CanTp_SendFrame(CanTp_TxFrameBuf) != E_OK)
    {
        DET_LOGE(TAG, "TX FF FAIL abort");
        CanTp_Tx.state = CANTP_TX_IDLE;
        return E_NOT_OK;
    }

    CanTp_Tx.state   = CANTP_TX_WAIT_FC;
    CanTp_Tx.bsTimer = CanTp_GetNowMs();

    return E_OK;
}

/* -----------------------------------------------------------------------
 * CanTp_CancelTransmit
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_CancelReceive
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_ChangeParameter
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_ReadParameter
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_MainFunction
 * ----------------------------------------------------------------------- */

/**
 * \brief   タイムアウト監視と CF 送信を周期的に処理する。
 *
 * \details EcuM_MainFunction から毎ループ呼び出す。
 *
 * \AUTOSARReq     {SWS_CanTp_00213}
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_MainFunction(void)
{
    if (!CanTp_Initialized)
    {
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_MAIN_FUNCTION, CANTP_E_UNINIT);
        return;
    }

    unsigned long now = CanTp_GetNowMs();

    /* ---- TX: N_Bs タイムアウト (WAIT_FC) ---- */
    if (CanTp_Tx.state == CANTP_TX_WAIT_FC)
    {
        if ((now - CanTp_Tx.bsTimer) >= CANTP_N_BS_TIMEOUT_MS)
        {
            DET_LOGE(TAG, "TX N_Bs timeout abort");
            CanTp_TxFinish(E_NOT_OK);
        }
    }

    /* ---- TX: CF 送信 (SEND_CF) ---- */
    if (CanTp_Tx.state == CANTP_TX_SEND_CF)
    {
        if ((now - CanTp_Tx.cfTimer) >= (unsigned long)CanTp_Tx.stMin)
        {
            CanTp_SendNextCF();
            CanTp_Tx.cfTimer = now;
        }
    }

    /* ---- RX: N_Cr タイムアウト (WAIT_CF) ---- */
    if (CanTp_Rx.state == CANTP_RX_WAIT_CF)
    {
        if ((now - CanTp_Rx.timer) >= CANTP_N_CR_TIMEOUT_MS)
        {
            DET_LOGE(TAG, "RX N_Cr timeout abort");
            CanTp_RxAbort();
        }
    }
}

/* ======================================================================
 * Call-back notifications
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * CanTp_RxIndication
 * ----------------------------------------------------------------------- */

/**
 * \brief   PduR から配信された受信 CAN フレームを処理する。
 *
 * \details フレームタイプを判定して SF/FF/CF/FC それぞれを処理する。
 *          SF/FF/CF のデータは、Dcm_StartOfReception()/Dcm_CopyRxData()/Dcm_TpRxIndication() で
 *          Dcm へ渡す（組み立ては Dcm の受信バッファの上で行う）。
 *
 * \param[in]  RxPduId     受信 PDU ID (単一チャネルのため未使用)。
 * \param[in]  PduInfoPtr  受信した 8 バイト CAN フレーム。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_CanTp_00214}
 * \ServiceID      {0x42}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (!CanTp_Initialized)
    {
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_RX_INDICATION, CANTP_E_UNINIT);
        return;
    }

    if (RxPduId != CANTP_RX_SDU_ID)
    {
        DET_LOGE(TAG, "RX E: invalid RxPduId=%u", (unsigned)RxPduId);
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_RX_INDICATION, CANTP_E_INVALID_RX_ID);
        return;
    }

    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL))
    {
        DET_LOGE(TAG, "RX E: NULL PduInfoPtr");
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_RX_INDICATION, CANTP_E_PARAM_POINTER);
        return;
    }

    if (PduInfoPtr->SduLength == 0U)
    {
        return;  /* 空フレームは DET 対象外（NULL ポインタではないため） */
    }

    const uint8* data      = PduInfoPtr->SduDataPtr;
    uint8        pci       = data[0];
    uint8        frameType = (uint8)((pci >> 4U) & 0x0FU);

    switch (frameType)
    {
    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_SF:
    {
        uint8 udsLen = pci & 0x0FU;

        if ((udsLen == 0U) || (udsLen > CANTP_SF_MAX_DATA)
            || (PduInfoPtr->SduLength < (PduLengthType)((PduLengthType)udsLen + 1U)))
        {
            break;
        }

        CanTp_RxAbort();   /* 受信中の MF があれば中断（受信中でなければ何もしない） */

        DET_LOGI(TAG, "RX SF len=%u", (unsigned)udsLen);

        PduLengthType bufSize = 0U;
        if (Dcm_StartOfReception(CANTP_DCM_RX_PDU_ID, NULL, (PduLengthType)udsLen, &bufSize) != BUFREQ_OK)
        {
            DET_LOGW(TAG, "RX SF rejected by upper layer");
            break;   /* 受け入れられなかった（Dcm_TpRxIndication() は呼ばない） */
        }

        Std_ReturnType result = (CanTp_CopyToUpper(&data[1], (uint8)udsLen) == BUFREQ_OK) ? E_OK : E_NOT_OK;
        Dcm_TpRxIndication(CANTP_DCM_RX_PDU_ID, result);
        break;
    }

    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_FF:
    {
        /* SWS_CanTp_00124: 同一コネクション（本プロジェクトは診断 RX N-SDU が
         * 1 系統のみのため常に同一 N_AI）で受信中に新しい FF を受信した場合、
         * 進行中の受信を中断し、受信したフレームを新規受信の開始として
         * 処理しなければならない（無視して古いセッションの N_Cr タイムアウトを
         * 待たせてはならない）。以前は CanTp_Transmit() の busy 判定と対称的な
         * 設計のつもりで新しい FF を無視していたが、TX の「送信中は新規送信を
         * 拒否する」とは非対称であるべき仕様であり誤りだった。
         * 以降の処理が CanTp_Rx の各フィールドを無条件に上書きするため、
         * ここで明示的に何もしなくても「中断して新規受信として処理」は
         * 自然に実現される。 */
        if (CanTp_Rx.state == CANTP_RX_WAIT_CF)
        {
            DET_LOGW(TAG, "RX FF aborts in-progress reception, starting new");
            CanTp_RxAbort();   /* 古い受信を、上位層へ中断として通知する */
        }

        uint16 msgLen = (uint16)((((uint16)pci & 0x0FU) << 8U)
                                 | (uint16)data[1]);

        if ((msgLen <= CANTP_SF_MAX_DATA) || (PduInfoPtr->SduLength < 8U))
        {
            break;
        }

        PduLengthType bufSize = 0U;
        BufReq_ReturnType started = Dcm_StartOfReception(CANTP_DCM_RX_PDU_ID, NULL, (PduLengthType)msgLen, &bufSize);

        if (started == BUFREQ_E_OVFL)
        {
            DET_LOGE(TAG, "RX FF overflow FC_OVFLW");
            CanTp_SendFlowControl(CANTP_FC_OVFLW, 0U, 0U);
            break;
        }

        if (started != BUFREQ_OK)
        {
            DET_LOGW(TAG, "RX FF rejected by upper layer");
            break;   /* 受け入れられなかった（Dcm_TpRxIndication() は呼ばない） */
        }

        CanTp_Rx.msgLen = msgLen;
        CanTp_Rx.sn     = 1U;
        CanTp_Rx.state  = CANTP_RX_WAIT_CF;
        CanTp_Rx.timer  = CanTp_GetNowMs();

        DET_LOGI(TAG, "RX FF len=%u", (unsigned)msgLen);

        /* FF の先頭 6 バイトを、上位層へ渡す（msgLen > 7 は上で確認済み） */
        if (CanTp_CopyToUpper(&data[2], (uint8)CANTP_FF_DATA) != BUFREQ_OK)
        {
            CanTp_RxAbort();
            break;
        }
        CanTp_Rx.pos = (uint16)CANTP_FF_DATA;

        CanTp_SendFlowControl(CANTP_FC_CTS, CANTP_BLOCK_SIZE, CANTP_ST_MIN);
        break;
    }

    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_CF:
    {
        if (CanTp_Rx.state != CANTP_RX_WAIT_CF)
        {
            break;
        }

        /* CanTp_SendNextCF() は常に 8 バイトへパディングして送信する規約のため、
         * 受信側もこれ未満は不正フレームとして破棄する（SF/FF と同様の防御。
         * これが無いと、短い CF で data[1..7] に古い/未初期化のバイトが
         * 混入したまま再組立バッファへコピーされてしまう）。 */
        if (PduInfoPtr->SduLength < 8U)
        {
            DET_LOGE(TAG, "RX CF too short len=%u", (unsigned)PduInfoPtr->SduLength);
            CanTp_RxAbort();
            break;
        }

        uint8 sn = pci & 0x0FU;

        if (sn != (CanTp_Rx.sn & 0x0FU))
        {
            DET_LOGE(TAG, "RX CF SN err exp=%u got=%u",
                     (unsigned)(CanTp_Rx.sn & 0x0FU), (unsigned)sn);
            CanTp_RxAbort();
            break;
        }

        uint16 remaining = CanTp_Rx.msgLen - CanTp_Rx.pos;
        uint8  copyLen   = (remaining > (uint16)CANTP_CF_DATA)
                           ? CANTP_CF_DATA : (uint8)remaining;
        if (CanTp_CopyToUpper(&data[1], copyLen) != BUFREQ_OK)
        {
            CanTp_RxAbort();
            break;
        }

        CanTp_Rx.pos += (uint16)copyLen;
        CanTp_Rx.sn   = (uint8)((CanTp_Rx.sn + 1U) & 0x0FU);
        CanTp_Rx.timer = CanTp_GetNowMs();

        DET_LOGI(TAG, "RX CF sn=%u pos=%u", (unsigned)sn, (unsigned)CanTp_Rx.pos);

        if (CanTp_Rx.pos >= CanTp_Rx.msgLen)
        {
            CanTp_Rx.state = CANTP_RX_IDLE;

            DET_LOGI(TAG, "RX MF done len=%u", (unsigned)CanTp_Rx.msgLen);

            Dcm_TpRxIndication(CANTP_DCM_RX_PDU_ID, E_OK);
        }
        break;
    }

    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_FC:
    {
        if (CanTp_Tx.state != CANTP_TX_WAIT_FC)
        {
            break;
        }

        /* CanTp_SendFlowControl() は常に 8 バイトへパディングして送信する規約
         * のため、受信側もこれ未満は不正フレームとして破棄する（SF/FF/CF と
         * 同様の防御。これが無いと、短い FC で data[1](BS)/data[2](STmin) に
         * 古い/未初期化のバイトが混入したまま採用されてしまう）。 */
        if (PduInfoPtr->SduLength < 8U)
        {
            DET_LOGE(TAG, "RX FC too short len=%u", (unsigned)PduInfoPtr->SduLength);
            break;
        }

        uint8 fs = pci & 0x0FU;

        DET_LOGI(TAG, "RX FC fs=%u", (unsigned)fs);

        if (fs == CANTP_FC_CTS)
        {
            CanTp_Tx.bs      = data[1];
            CanTp_Tx.stMin   = CanTp_DecodeStMin(data[2]);
            CanTp_Tx.bsCnt   = CanTp_Tx.bs;
            CanTp_Tx.state   = CANTP_TX_SEND_CF;
            CanTp_Tx.cfTimer = CanTp_GetNowMs();
        }
        else if (fs == CANTP_FC_WAIT)
        {
            CanTp_Tx.bsTimer = CanTp_GetNowMs();   /* N_Bs タイマリセット */
        }
        else   /* OVFLW */
        {
            DET_LOGE(TAG, "TX FC OVFLW abort");
            CanTp_TxFinish(E_NOT_OK);
        }
        break;
    }

    default:
        break;
    }
}


/* -----------------------------------------------------------------------
 * CanTp_TxConfirmation
 * ----------------------------------------------------------------------- */

/**
 * \brief   PduR からの送信完了通知を受け取る。
 *
 * \details 現状 no-op（意図的）。SWS_CanTp_00355 は本来
 *          「result==E_NOT_OK ならセッションを中断する」ことを要求するが、
 *          本プロジェクトの非同期 TX 確認経路（Can.c の TX 確認保留キュー →
 *          CanIf_TxConfirmation()）は物理送信が同期的に失敗した場合そもそも
 *          キューに積まれず、かつ CanIf_TxConfirmation() 自体が E_OK を
 *          固定で返すため、result にはこの経路では実質的に E_NOT_OK が
 *          渡ってこない（真に非同期の送信失敗を表現する手段がない）。
 *          実際に到達可能な送信失敗（PduR/CanIf/Can_Write が同期的に拒否する
 *          ケース）は CanTp_SendFrame() の戻り値と、CF 送信の N_As 相当
 *          タイムアウト（CanTp_SendNextCF() 参照）で別途検出している。
 *
 * \param[in]  TxPduId  完了した PDU ID (未使用)。
 * \param[in]  result   送信結果 (未使用。上記理由により本経路では常に E_OK)。
 *
 * \AUTOSARReq     {SWS_CanTp_00215}
 * \ServiceID      {0x40}
 */
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)TxPduId;
    (void)result;

    if (!CanTp_Initialized)
    {
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_TX_CONFIRMATION, CANTP_E_UNINIT);
        return;
    }
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTp_GetNowMs
 * ---------------------------------------------------------------------- */

/**
 * \brief   現在時刻 [ms] を返す（Os カウンタ SYSTEM_COUNTER の tick 値）。
 * \details millis() を直接呼ばず Os の GetCounterValue() を介する。値は Os_Init() の
 *          前後や millis() フォールバックの前後でも飛ばない（Os.h 参照）ため、
 *          本モジュールが保持する「前回時刻」との差分計算にそのまま使える。
 */
static unsigned long CanTp_GetNowMs(void)
{
    TickType now = 0U;
    (void)GetCounterValue(SYSTEM_COUNTER, &now);
    return (unsigned long)now;
}

/* -----------------------------------------------------------------------
 * 内部ヘルパー: フレーム送信
 * ----------------------------------------------------------------------- */

/** frame（8 バイト）を PduR 経由で CAN 0x7E8 に送信する。
 *  \param[in] frame  送信する 8 バイトのフレーム（CanTp_TxFrameBuf か fcFrame）。
 *  \retval E_OK     送信受け付け成功。
 *  \retval E_NOT_OK PduR / CanIf / Can_Write が失敗（TX バッファビジー等）。 */
static Std_ReturnType CanTp_SendFrame(uint8* frame)
{
    static PduInfoType pdu;
    pdu.SduDataPtr = frame;
    pdu.SduLength  = 8U;
    return PduR_CanTpTransmit(CANTP_PDUR_TX_SDU_ID, &pdu);
}

/** TX チャネルが、送信の途中（FC 待ち、または CF の送信中）か。 */
static boolean CanTp_TxBusy(void)
{
    return (boolean)(CanTp_Tx.state != CANTP_TX_IDLE);
}

/** 送信が進まない状態（CF の送信失敗、または Dcm の送信データ待ち）が、CANTP_N_AS_TIMEOUT_MS を
 *  超えて続いたか。呼ぶたびに、進まない状態の継続を測る（最初に呼んだ時刻を基点に記録する）。
 *  進んだ時は、呼び出し側が asFailTimer を 0 へ戻す。
 *  @retval TRUE   タイムアウト。
 *  @retval FALSE  まだ。 */
static boolean CanTp_TxStallTimedOut(void)
{
    unsigned long now = CanTp_GetNowMs();

    if (CanTp_Tx.asFailTimer == 0UL)
    {
        CanTp_Tx.asFailTimer = now;   /* 継続の開始時刻を記録 */
    }
    return (boolean)((now - CanTp_Tx.asFailTimer) >= CANTP_N_AS_TIMEOUT_MS);
}

/** 受信中の N-SDU を中断する。上位層（Dcm）へ Dcm_TpRxIndication(E_NOT_OK) を通知し、RX を IDLE へ戻す。 */
static void CanTp_RxAbort(void)
{
    if (CanTp_Rx.state == CANTP_RX_WAIT_CF)
    {
        CanTp_Rx.state = CANTP_RX_IDLE;
        Dcm_TpRxIndication(CANTP_DCM_RX_PDU_ID, E_NOT_OK);
    }
}

/** 送信を終える（正常終了、または中断）。TX を IDLE へ戻し、上位層（Dcm）へ Dcm_TpTxConfirmation() を通知する。 */
static void CanTp_TxFinish(Std_ReturnType result)
{
    CanTp_Tx.state       = CANTP_TX_IDLE;
    CanTp_Tx.frameReady  = 0U;
    CanTp_Tx.asFailTimer = 0UL;
    Dcm_TpTxConfirmation(CANTP_DCM_TX_PDU_ID, result);
}

/** 受信したフレームのデータ（len バイト、最大 CANTP_CF_DATA）を、上位層の受信バッファへコピーする。
 *  CopyRxData() は書き込み可能なポインタを取るので、受信フレームを一度、内部の作業領域へ写してから渡す。 */
static BufReq_ReturnType CanTp_CopyToUpper(const uint8* src, uint8 len)
{
    static uint8 work[CANTP_CF_DATA];
    PduInfoType  info;
    PduLengthType remaining = 0U;
    uint8 k;

    for (k = 0U; k < len; k++)
    {
        work[k] = src[k];
    }
    info.SduDataPtr = work;
    info.SduLength  = (PduLengthType)len;
    return Dcm_CopyRxData(CANTP_DCM_RX_PDU_ID, &info, &remaining);
}

/**
 * \brief   ISO 15765-2 の STmin バイト値をミリ秒へデコードする。
 *
 * \details 0x00-0x7F はそのまま ms（0-127ms）。0xF1-0xF9 は 100-900µs 刻みだが、
 *          本実装は CanTp_MainFunction() が ms 分解能でしか駆動されないため
 *          （Os スケジューラの最小周期が 1ms）、サブミリ秒の要求は「追加の
 *          待機なし」として 0ms に切り下げる。0x80-0xF0 / 0xFA-0xFF は予約値
 *          のため、誤った短い遅延でバスを圧迫しないよう安全側（規格上の
 *          直接値レンジの最大値 127ms）に倒す。
 *
 * \param[in]  raw  受信した FC フレームの STmin 生バイト値。
 * \return     ミリ秒に換算した STmin。
 */
static uint8 CanTp_DecodeStMin(uint8 raw)
{
    if (raw <= 0x7FU)
    {
        return raw;
    }

    if ((raw >= 0xF1U) && (raw <= 0xF9U))
    {
        return 0U;
    }

    DET_LOGW(TAG, "STmin reserved value 0x%02X -> clamp 127ms", (unsigned)raw);
    return 127U;
}

/**
 * \brief   Flow Control フレームを組立て送信する。
 *
 * \param[in]  fs     フローステータス (CTS=0 / WAIT=1 / OVFLW=2)。
 * \param[in]  bs     ブロックサイズ。
 * \param[in]  stMin  最小分離時間 (ms)。
 */
static void CanTp_SendFlowControl(uint8 fs, uint8 bs, uint8 stMin)
{
    static uint8 fcFrame[8];

    fcFrame[0] = (uint8)(0x30U | (fs & 0x0FU));
    fcFrame[1] = bs;
    fcFrame[2] = stMin;
    fcFrame[3] = 0x00U;
    fcFrame[4] = 0x00U;
    fcFrame[5] = 0x00U;
    fcFrame[6] = 0x00U;
    fcFrame[7] = 0x00U;

    DET_LOGI(TAG, "TX FC fs=%u bs=%u", (unsigned)fs, (unsigned)bs);

    (void)CanTp_SendFrame(fcFrame);   /* FC 送信失敗は N_Cr タイムアウトで上位が再試行 */
}

/**
 * \brief   次の Consecutive Frame を組立て送信する。
 *
 * \details CanTp_Tx.buf からデータを取り出し CF PCI を付けて送信する。
 *          送信後、位置・シーケンス番号を更新する。
 *          全バイト送信完了なら IDLE へ遷移し、BS を消費したなら WAIT_FC へ遷移する。
 *
 *          N_As タイムアウト（CanTp_Cfg.h 参照）: 送信が同期的に失敗し続ける間、
 *          最初の失敗時刻から CANTP_N_AS_TIMEOUT_MS 経過してもまだ失敗していれば
 *          セッションを中断する。これが無いと、バス障害等で送信が失敗し続ける
 *          場合に無期限にリトライし続け、CanTp_Transmit() の busy 判定により
 *          新規送信が永久に拒否され続けてしまう。
 */
static void CanTp_SendNextCF(void)
{
    if (CanTp_Tx.frameReady == 0U)
    {
        /* 新しい CF を組み立てる: データは、上位層（Dcm）から取り出す。 */
        uint16 remaining = CanTp_Tx.msgLen - CanTp_Tx.pos;
        uint8  copyLen   = (remaining > (uint16)CANTP_CF_DATA)
                           ? CANTP_CF_DATA : (uint8)remaining;

        PduInfoType   src;
        PduLengthType avail = 0U;
        src.SduDataPtr = &CanTp_TxFrameBuf[1];
        src.SduLength  = (PduLengthType)copyLen;

        BufReq_ReturnType got = Dcm_CopyTxData(CANTP_DCM_TX_PDU_ID, &src, NULL, &avail);

        if (got == BUFREQ_E_BUSY)
        {
            /* データがまだ無い。次の CanTp_MainFunction 呼び出しで再試行する（[SWS_CanTp_00184]）。
             * ただし、送信データが用意されないまま続く場合に備え、N_Cs 相当のタイムアウトで中断する
             * （送信失敗の継続を測る N_As 用のタイマと、同じ時間・同じタイマを使う）。 */
            if (CanTp_TxStallTimedOut())
            {
                DET_LOGE(TAG, "TX CF: no data from upper layer (N_Cs) abort sn=%u", (unsigned)CanTp_Tx.sn);
                CanTp_TxFinish(E_NOT_OK);
            }
            return;
        }

        if (got != BUFREQ_OK)
        {
            DET_LOGE(TAG, "TX CF: CopyTxData failed abort sn=%u", (unsigned)CanTp_Tx.sn);
            CanTp_TxFinish(E_NOT_OK);
            return;
        }

        CanTp_TxFrameBuf[0] = (uint8)(0x20U | (CanTp_Tx.sn & 0x0FU));
        uint8 i;
        for (i = copyLen; i < CANTP_CF_DATA; i++)
        {
            CanTp_TxFrameBuf[1U + i] = 0x00U;
        }
        CanTp_Tx.frameLen   = copyLen;
        CanTp_Tx.frameReady = 1U;
    }

    DET_LOGI(TAG, "TX CF sn=%u pos=%u", (unsigned)CanTp_Tx.sn, (unsigned)CanTp_Tx.pos);

    if (CanTp_SendFrame(CanTp_TxFrameBuf) != E_OK)
    {
        if (CanTp_TxStallTimedOut())
        {
            DET_LOGE(TAG, "TX N_As timeout abort sn=%u", (unsigned)CanTp_Tx.sn);
            CanTp_TxFinish(E_NOT_OK);
            return;
        }

        /* 送信失敗: pos/sn を進めず、組み立て済みの同じ CF を、次の CanTp_MainFunction 呼び出しで再送する */
        DET_LOGE(TAG, "TX CF FAIL sn=%u retry", (unsigned)CanTp_Tx.sn);
        return;
    }

    CanTp_Tx.asFailTimer = 0UL;   /* 送信成功: 失敗継続の記録をクリア */
    CanTp_Tx.frameReady  = 0U;
    CanTp_Tx.pos += (uint16)CanTp_Tx.frameLen;
    CanTp_Tx.sn   = (uint8)((CanTp_Tx.sn + 1U) & 0x0FU);

    if (CanTp_Tx.pos >= CanTp_Tx.msgLen)
    {
        DET_LOGI(TAG, "TX done");
        CanTp_TxFinish(E_OK);
        return;
    }

    /* ブロックサイズ管理 (bs=0 なら無制限) */
    if (CanTp_Tx.bs > 0U)
    {
        CanTp_Tx.bsCnt--;
        if (CanTp_Tx.bsCnt == 0U)
        {
            DET_LOGI(TAG, "TX block done wait FC");
            CanTp_Tx.state   = CANTP_TX_WAIT_FC;
            CanTp_Tx.bsTimer = CanTp_GetNowMs();
        }
    }
}

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef CANTP_UNIT_TEST
void CanTp_Test_ResetInitState(void)
{
    CanTp_Initialized = 0U;
}

boolean CanTp_Test_IsTxBusy(void)
{
    return CanTp_TxBusy();
}
#endif

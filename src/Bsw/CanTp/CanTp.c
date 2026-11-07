/**
 * \file    CanTp.c
 * \brief   CAN トランスポートプロトコル実装 (AUTOSAR SWS_CanTp 準拠, ISO 15765-2)
 * \details CanIf（下位層）と PduR（上位層への仲介）の間で ISO 15765-2 フレームの組立・分割を担う。
 *
 *          対応フレームタイプ:
 *            SF (Single Frame)      PCI 上位ニブル = 0x0
 *            FF (First Frame)       PCI 上位ニブル = 0x1
 *            CF (Consecutive Frame) PCI 上位ニブル = 0x2
 *            FC (Flow Control)      PCI 上位ニブル = 0x3
 *
 *          下位層（CanIf）とは、PduR を経由せず直接やり取りする（SWS_CanTp の N-PDU。送信は CanIf_Transmit()、
 *          受信は CanIf の CanTp_RxIndication() 呼び出し、送信確認は CanIf の CanTp_TxConfirmation() 呼び出し）。
 *          上位層（Dcm）との受け渡しは、PduR の TP の API（PduR_CanTp*()）で行う。PduR が、TP の
 *          ルーティングパスで上位層（Dcm）の PDU ID へ変換して、Dcm のコールバックへ転送する。
 *          CanTp は組み立て・分割用のバッファを持たず、フレームごとに上位層とやり取りする。
 *            受信: PduR_CanTpStartOfReception()（SF/FF の受信時）→ PduR_CanTpCopyRxData()（各フレームの
 *                  データ）→ PduR_CanTpRxIndication()（完了・中断）
 *            送信: CanTp_Transmit()（長さだけを受け取る）→ PduR_CanTpCopyTxData()（SF/FF/各 CF の
 *                  データ）→ PduR_CanTpTxConfirmation()（完了・中断）
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
 *          CanTp_SendFrame() の同期的な戻り値（CanIf/Can_Write が
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
#include "CanIf.h"
#include "PduR.h"
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
#define CANTP_MAX_MSG_LEN 4095U  /**< FF の長さ欄（12 bit）で表せる N-SDU の最大長 */

/* CAN FD のフレーム（N-PDU の CAN_DL が 8 を超える）では、PCI の置き方とデータの量が変わる（ISO 15765-2:2016）。
 *   SF: PCI が 2 バイト（0x00 + SF_DL。「エスケープ」形式）、データは CAN_DL - 2 バイトまで
 *   FF: PCI は 2 バイトのまま、データは CAN_DL - 2 バイト
 *   CF: PCI は 1 バイトのまま、データは CAN_DL - 1 バイトまで */
#define CANTP_FD_SF_PCI   2U     /**< FD の SF: PCI のバイト数 */
#define CANTP_FF_PCI      2U     /**< FF: PCI のバイト数（クラシックも FD も同じ） */
#define CANTP_CF_PCI      1U     /**< CF: PCI のバイト数（クラシックも FD も同じ） */
#define CANTP_MAX_FRAME_DATA  (CANTP_FD_FRAME_LEN - CANTP_CF_PCI)   /**< 1 フレームのデータの最大バイト数（FD の CF） */

/** CAN FD のフレームの最大長が、CanIf の PDU の最大長（COMSTACK_PDU_MAX_LENGTH）に収まることの静的アサート。
 *  CanIf の診断応答の PDU の DLC（config/data/CanIf.json の DiagResp_Tx）は、これ以上にすること（CanTp_Cfg.h 参照）。 */
typedef char CanTp_FdFrame_Must_Fit_CanIfMaxDlc[(CANTP_FD_FRAME_LEN <= CANIF_MAX_DLC) ? 1 : -1];

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
    CANTP_TX_SEND_CF = 2,
    CANTP_TX_START   = 3   /**< 最初のフレーム（SF/FF）に使う、Dcm の送信データを待っている */
} CanTp_TxStateType;

/** CanTp_TxStart() の結果 */
typedef enum
{
    CANTP_START_SF_SENT = 0,   /**< SF を送信した（これで送信は終わり） */
    CANTP_START_FF_SENT = 1,   /**< FF を送信した（FC 待ちへ進んだ） */
    CANTP_START_BUSY    = 2,   /**< Dcm の送信データがまだ無い（後で再試行する） */
    CANTP_START_FAIL    = 3    /**< データの取り出し、またはフレームの送信に失敗した */
} CanTp_StartResultType;

/* -----------------------------------------------------------------------
 * モジュール内部状態
 * ----------------------------------------------------------------------- */

/** RX チャネル状態（データは持たず、フレームごとに PduR_CanTpCopyRxData() で渡す） */
static struct {
    uint8             dl;        /**< この受信の RX_DL（最初のフレームの CAN_DL。クラシックは 8、FD は 12〜64）。CF のデータは dl - 1 バイトまで */
    uint16            msgLen;    /**< FF で通知された総メッセージ長 */
    uint16            pos;       /**< Dcm へコピー済みのバイト数    */
    uint8             sn;        /**< 次に受信すべき CF シーケンス番号 */
    uint8             bs;        /**< この受信で FC に載せるブロックサイズ。FF の受信時に CanTp_RxBlockSize から固定する（[SWS_CanTp_00067]: 途中で変えない） */
    uint8             bsCnt;     /**< 現ブロックで、FC を送るまでに受信する CF の残り数（bs が 0 のときは使わない） */
    CanTp_RxStateType state;
    unsigned long     timer;     /**< N_Cr タイムアウト基点 (ms)   */
} CanTp_Rx;

/** TX チャネル状態（データは持たず、フレームごとに PduR_CanTpCopyTxData() で取り出す） */
static struct {
    uint8             dl;        /**< この送信の TX_DL（フレームの最大長。クラシックは 8、FD は CANTP_FD_FRAME_LEN） */
    uint16            msgLen;    /**< 送信する総メッセージ長        */
    uint16            pos;       /**< 送信済み（フレームに詰めた）バイト数 */
    uint8             frameLen;  /**< 組み立て済みで未送信の CF のデータバイト数 */
    uint8             frameSize; /**< 組み立て済みで未送信の CF の、フレームの長さ（パディング後。送信にそのまま渡す） */
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

/** 受信側が FC で送るブロックサイズ（BS。0 = ブロックごとの FC なし）。CanTp_Init() が CANTP_BLOCK_SIZE へ戻す。
 *  BS は上限で、受信の状況に合わせて、設定値以下へ変えてよい（[SWS_CanTp_00091]）ため、定数ではなく変数で持つ。 */
static uint8 CanTp_RxBlockSize = CANTP_BLOCK_SIZE;

/** TX フレーム送信用バッファ (SF/FF/CF 共用)。FC は、CF の組み立て済みフレーム（再送待ち）を壊さないよう、
 *  CanTp_SendFlowControl() の中の別のバッファを使う */
static uint8 CanTp_TxFrameBuf[CANTP_FD_FRAME_LEN];

/** 直近に受け付けた要求（SF/FF）が、CAN FD のフレームだったか。真なら、応答も FD のフレームで返す
 *  （クラシック CAN 専用の相手には、これまでどおりクラシックで返す）。 */
static boolean CanTp_PeerFd = FALSE;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static unsigned long CanTp_GetNowMs(void);

/* -----------------------------------------------------------------------
 * 内部関数プロトタイプ
 * ----------------------------------------------------------------------- */
static Std_ReturnType CanTp_SendFrame(uint8* frame, uint8 length);
static uint8 CanTp_RoundUpDl(uint8 n);
static void CanTp_ReportRuntimeError(uint8 apiId, uint8 errorId);
static CanTp_StartResultType CanTp_TxStart(void);
static BufReq_ReturnType CanTp_StartOfReception(const uint8* src, uint8 len, PduLengthType total, PduLengthType* bufSize);
static uint8* CanTp_StageRxData(const uint8* src, uint8 len);
static boolean CanTp_TxBusy(void);
static boolean CanTp_TxStallTimedOut(void);
static void CanTp_RxAbort(void);
static BufReq_ReturnType CanTp_CopyToUpper(const uint8* src, uint8 len);
static void CanTp_TxFinish(Std_ReturnType result);
static Std_ReturnType CanTp_SendFlowControl(uint8 fs, uint8 bs, uint8 stMin);
static boolean CanTp_SendRxFlowControl(void);
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
 *
 * \param[in]  CfgPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                     持たないため）。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_Init(const CanTp_ConfigType* CfgPtr)
{
    (void)CfgPtr; /* 本プロジェクトは post-build 設定を持たない（CanTp.h 参照） */
    CanTp_Rx.state  = CANTP_RX_IDLE;
    CanTp_Rx.dl     = CANTP_FRAME_LEN;
    CanTp_Rx.msgLen = 0U;
    CanTp_Rx.pos    = 0U;
    CanTp_Rx.bs     = 0U;
    CanTp_Rx.bsCnt  = 0U;
    CanTp_RxBlockSize = CANTP_BLOCK_SIZE;
    CanTp_Tx.state       = CANTP_TX_IDLE;
    CanTp_Tx.dl          = CANTP_FRAME_LEN;
    CanTp_Tx.msgLen      = 0U;
    CanTp_Tx.pos         = 0U;
    CanTp_Tx.frameReady  = 0U;
    CanTp_Tx.asFailTimer = 0UL;
    CanTp_PeerFd         = FALSE;
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
 *          組み立てる時に PduR_CanTpCopyTxData() で Dcm から取り出す。長さ <= 7 なら、取り出して SF を
 *          送信し、送信できたら PduR_CanTpTxConfirmation(E_OK) を呼ぶ。>= 8 なら FF の分を取り出して
 *          FF 送信し、WAIT_FC へ遷移する（残りは各 CF の組み立ての時に取り出す）。
 *
 * \param[in]  TxSduId     TX N-SDU ID (CANTP_TX_SDU_ID)。
 * \param[in]  PduInfoPtr  送信する UDS 応答の長さ（SduLength）。NULL 禁止。
 *
 * \retval  E_OK      送信要求を受け付けた。Dcm の送信データがまだ無いとき（CopyTxData が BUSY）も、受け付けて、
 *                     CanTp_MainFunction() で再試行する（[SWS_CanTp_00184]。N_Cs 相当のタイムアウトで中断）。
 * \retval  E_NOT_OK  NULL ポインタ、長さ 0 または超過、チャネルビジー、データの取り出し・フレームの送信の失敗。
 *                     （この場合、PduR_CanTpTxConfirmation() は呼ばない）
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

    /* データ（SduDataPtr）は使わない。送る長さだけを受け取り、データは PduR_CanTpCopyTxData() で取り出す。 */
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

    CanTp_Tx.msgLen = msgLen;
    CanTp_Tx.dl     = (CanTp_PeerFd != FALSE) ? (uint8)CANTP_FD_FRAME_LEN : (uint8)CANTP_FRAME_LEN;

    switch (CanTp_TxStart())
    {
    case CANTP_START_SF_SENT:
        PduR_CanTpTxConfirmation(CANTP_TX_SDU_ID, E_OK);   /* SF は、送ったら終わり */
        return E_OK;

    case CANTP_START_FF_SENT:
        return E_OK;

    case CANTP_START_BUSY:
        /* [SWS_CanTp_00167]/[SWS_CanTp_00184]: Dcm の送信データがまだ無い。要求は受け付け、データが用意されるまで
         * CanTp_MainFunction() で再試行する（N_Cs 相当のタイムアウトで中断する）。 */
        CanTp_Tx.state = CANTP_TX_START;
        (void)CanTp_TxStallTimedOut();   /* N_Cs の起点を記録する */
        return E_OK;

    default:
        return E_NOT_OK;   /* データの取り出し、またはフレームの送信の失敗。この場合、確認通知は出さない */
    }
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
 * \par 仕様
 *          EcuM_MainFunction から毎ループ呼び出す。以下を担当する:
 *          - TX WAIT_FC 状態: N_Bs タイムアウト検出 → TX 中断
 *          - TX SEND_CF 状態: STmin 経過後に次の CF を送信
 *          - RX WAIT_CF 状態: N_Cr タイムアウト検出 → RX 中断
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

    /* ---- TX: 最初のフレーム（SF/FF）の再試行 (START) ---- */
    if (CanTp_Tx.state == CANTP_TX_START)
    {
        switch (CanTp_TxStart())
        {
        case CANTP_START_SF_SENT:
            CanTp_TxFinish(E_OK);   /* SF は、送ったら終わり */
            break;

        case CANTP_START_FF_SENT:
            break;

        case CANTP_START_BUSY:
            if (CanTp_TxStallTimedOut())
            {
                DET_LOGE(TAG, "TX first frame: no data from upper layer (N_Cs) abort");
                CanTp_ReportRuntimeError(CANTP_API_ID_MAIN_FUNCTION, CANTP_E_TX_COM);
                CanTp_TxFinish(E_NOT_OK);
            }
            break;

        default:
            CanTp_ReportRuntimeError(CANTP_API_ID_MAIN_FUNCTION, CANTP_E_COM);
            CanTp_TxFinish(E_NOT_OK);
            break;
        }
    }

    /* ---- TX: N_Bs タイムアウト (WAIT_FC) ---- */
    if (CanTp_Tx.state == CANTP_TX_WAIT_FC)
    {
        if ((now - CanTp_Tx.bsTimer) >= CANTP_N_BS_TIMEOUT_MS)
        {
            DET_LOGE(TAG, "TX N_Bs timeout abort");
            CanTp_ReportRuntimeError(CANTP_API_ID_MAIN_FUNCTION, CANTP_E_TX_COM);
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
            CanTp_ReportRuntimeError(CANTP_API_ID_MAIN_FUNCTION, CANTP_E_RX_COM);
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
 * \brief   CanIf から配信された受信 CAN フレーム（N-PDU）を処理する。
 *
 * \details フレームタイプを判定して SF/FF/CF/FC それぞれを処理する。
 *          SF/FF/CF のデータは、PduR_CanTpStartOfReception()/PduR_CanTpCopyRxData()/PduR_CanTpRxIndication() で
 *          Dcm へ渡す（組み立ては Dcm の受信バッファの上で行う）。
 *
 * \param[in]  RxPduId     受信 N-PDU の ID（CANTP_RX_NPDU_ID。CanIf の設定が渡す）。
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

    if (RxPduId != CANTP_RX_NPDU_ID)
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

    /* 8 バイトを超えるフレームは、CAN FD の有効な長さ（12、16、20、24、32、48、64）だけを扱う。
     * 実機の配線では、CanIf が無効な長さを捨てるため、ここへは届かない（二重の防御）。 */
    if ((PduInfoPtr->SduLength > CANTP_FRAME_LEN) &&
        (CanTp_RoundUpDl((uint8)PduInfoPtr->SduLength) != (uint8)PduInfoPtr->SduLength))
    {
        DET_LOGW(TAG, "RX frame with invalid FD length=%u ignored", (unsigned)PduInfoPtr->SduLength);
        return;
    }

    const uint8* data      = PduInfoPtr->SduDataPtr;
    uint8        pci       = data[0];
    uint8        frameType = (uint8)((pci >> 4U) & 0x0FU);

    switch (frameType)
    {
    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_SF:
    {
        /* フレームが CAN FD のものか（CAN_DL が 8 を超える）。FD の SF は、PCI が 2 バイト（0x00 + SF_DL）で、
         * データは [2] から。クラシックの SF は、PCI が 1 バイト（下位 4 bit が長さ）で、データは [1] から。 */
        boolean isFd   = (boolean)(PduInfoPtr->SduLength > CANTP_FRAME_LEN);
        uint8   hdrLen = (isFd != FALSE) ? (uint8)CANTP_FD_SF_PCI : 1U;
        uint8   udsLen = (isFd != FALSE) ? data[1] : (uint8)(pci & 0x0FU);

        /* [SWS_CanTp_00344]/[SWS_CanTp_00345]: 受信パディングは常に ON（送信側テスターは 8 バイトへ
         * パディングする前提）。DLC が 8 未満の SF は、PCI の内容にかかわらず受け付けず、ランタイム
         * エラー CANTP_E_PADDING を報告する（宣言された長さの検査より先に行う）。なお実機の配線では、
         * DLC が設定（8）と一致しないフレームは、ここへ届く前に CanIf が捨てる（二重の防御）。 */
        if (PduInfoPtr->SduLength < CANTP_FRAME_LEN)
        {
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_PADDING);
            break;
        }

        /* ペイロード長が 0、または 1 フレームに収まる最大値（クラシックは 7、FD は CAN_DL - 2）を超える SF は不正。
         * FD の SF で、PCI の 1 バイト目の下位 4 bit が 0 でないものも不正（エスケープ形式は 0x00）。 */
        uint8 maxLen = (isFd != FALSE) ? (uint8)(PduInfoPtr->SduLength - CANTP_FD_SF_PCI) : (uint8)CANTP_SF_MAX_DATA;
        if ((udsLen == 0U) || (udsLen > maxLen) || ((isFd != FALSE) && ((pci & 0x0FU) != 0U)))
        {
            break;
        }

        CanTp_RxAbort();   /* 受信中の MF があれば中断（受信中でなければ何もしない） */

        DET_LOGD(TAG, "RX SF len=%u%s", (unsigned)udsLen, (isFd != FALSE) ? " (FD)" : "");

        PduLengthType bufSize = 0U;
        if (CanTp_StartOfReception(&data[hdrLen], udsLen, (PduLengthType)udsLen, &bufSize) != BUFREQ_OK)
        {
            DET_LOGW(TAG, "RX SF rejected by upper layer");
            break;   /* 受け入れられなかった（PduR_CanTpRxIndication() は呼ばない） */
        }

        /* [SWS_CanTp_00339]: 受け入れられても、空きが、受信済みのデータより小さければ、受信を中断する。 */
        if (bufSize < (PduLengthType)udsLen)
        {
            DET_LOGE(TAG, "RX SF buffer too small avail=%u", (unsigned)bufSize);
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
            PduR_CanTpRxIndication(CANTP_RX_SDU_ID, E_NOT_OK);
            break;
        }

        CanTp_PeerFd = isFd;   /* 受け付けた要求のフレーム形式。応答も同じ形式で返す */

        Std_ReturnType result = (CanTp_CopyToUpper(&data[hdrLen], udsLen) == BUFREQ_OK) ? E_OK : E_NOT_OK;
        PduR_CanTpRxIndication(CANTP_RX_SDU_ID, result);
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

        /* FF の先頭データの量。クラシックは 6 バイト、FD は CAN_DL - 2 バイト。 */
        boolean isFd   = (boolean)(PduInfoPtr->SduLength > CANTP_FRAME_LEN);
        uint8   ffData = (isFd != FALSE) ? (uint8)(PduInfoPtr->SduLength - CANTP_FF_PCI) : (uint8)CANTP_FF_DATA;

        /* SF で送れる長さの N-SDU は FF にしない（クラシック: 7 以下）。FD は、FF のデータより長いときだけ FF にする。 */
        if ((msgLen <= ((isFd != FALSE) ? (uint16)ffData : (uint16)CANTP_SF_MAX_DATA)) ||
            (PduInfoPtr->SduLength < CANTP_FRAME_LEN))
        {
            break;
        }

        PduLengthType bufSize = 0U;
        BufReq_ReturnType started = CanTp_StartOfReception(&data[CANTP_FF_PCI], ffData, (PduLengthType)msgLen, &bufSize);

        if (started == BUFREQ_E_OVFL)
        {
            DET_LOGE(TAG, "RX FF overflow FC_OVFLW");
            (void)CanTp_SendFlowControl(CANTP_FC_OVFLW, 0U, 0U);
            break;
        }

        if (started != BUFREQ_OK)
        {
            DET_LOGW(TAG, "RX FF rejected by upper layer");
            break;   /* 受け入れられなかった（PduR_CanTpRxIndication() は呼ばない） */
        }

        /* [SWS_CanTp_00339]: 受け入れられても、空きが、FF で受信済みのデータより小さければ、受信を中断する。 */
        if (bufSize < (PduLengthType)ffData)
        {
            DET_LOGE(TAG, "RX FF buffer too small avail=%u", (unsigned)bufSize);
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
            PduR_CanTpRxIndication(CANTP_RX_SDU_ID, E_NOT_OK);
            break;
        }

        CanTp_PeerFd    = isFd;   /* 受け付けた要求のフレーム形式。応答も同じ形式で返す */
        CanTp_Rx.dl     = (isFd != FALSE) ? (uint8)PduInfoPtr->SduLength : (uint8)CANTP_FRAME_LEN;
        CanTp_Rx.msgLen = msgLen;
        CanTp_Rx.sn     = 1U;
        CanTp_Rx.bs     = CanTp_RxBlockSize;
        CanTp_Rx.bsCnt  = CanTp_Rx.bs;
        CanTp_Rx.state  = CANTP_RX_WAIT_CF;
        CanTp_Rx.timer  = CanTp_GetNowMs();

        DET_LOGD(TAG, "RX FF len=%u%s", (unsigned)msgLen, (isFd != FALSE) ? " (FD)" : "");

        /* FF の先頭データ（クラシックは 6 バイト、FD は CAN_DL - 2 バイト）を、上位層へ渡す（msgLen が、それより長いことは上で確認済み） */
        if (CanTp_CopyToUpper(&data[CANTP_FF_PCI], ffData) != BUFREQ_OK)
        {
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
            CanTp_RxAbort();
            break;
        }
        CanTp_Rx.pos = (uint16)ffData;

        (void)CanTp_SendRxFlowControl();
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
        /* FD では、最後の CF は、データが入る分だけの有効な長さでよい（8 を超える有効な DLC は 12、16、…、64 のため、
         * パディングで 8 バイトに揃える対象ではない）。クラシックは、常に 8 バイト以上。 */
        uint16 remaining = CanTp_Rx.msgLen - CanTp_Rx.pos;
        uint8  copyLen   = (remaining > (uint16)(CanTp_Rx.dl - CANTP_CF_PCI))
                           ? (uint8)(CanTp_Rx.dl - CANTP_CF_PCI) : (uint8)remaining;
        uint8  minLen    = (CanTp_Rx.dl == CANTP_FRAME_LEN) ? (uint8)CANTP_FRAME_LEN : (uint8)(copyLen + CANTP_CF_PCI);

        /* ISO 15765-2: CF は、最後のものを除き、長さが RX_DL（最初のフレームの CAN_DL）に等しい。RX_DL より長い CF は、
         * 余りを切り捨てると、データが壊れる（コピーが進む量とずれる）ため、受信を中断する。 */
        if (PduInfoPtr->SduLength > CanTp_Rx.dl)
        {
            DET_LOGE(TAG, "RX CF longer than RX_DL len=%u dl=%u", (unsigned)PduInfoPtr->SduLength, (unsigned)CanTp_Rx.dl);
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
            CanTp_RxAbort();
            break;
        }

        if (PduInfoPtr->SduLength < minLen)
        {
            DET_LOGE(TAG, "RX CF too short len=%u", (unsigned)PduInfoPtr->SduLength);
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_PADDING);   /* [SWS_CanTp_00346] */
            CanTp_RxAbort();
            break;
        }

        uint8 sn = pci & 0x0FU;

        if (sn != (CanTp_Rx.sn & 0x0FU))
        {
            DET_LOGE(TAG, "RX CF SN err exp=%u got=%u",
                     (unsigned)(CanTp_Rx.sn & 0x0FU), (unsigned)sn);
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
            CanTp_RxAbort();
            break;
        }

        if (CanTp_CopyToUpper(&data[CANTP_CF_PCI], copyLen) != BUFREQ_OK)
        {
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
            CanTp_RxAbort();
            break;
        }

        CanTp_Rx.pos += (uint16)copyLen;
        CanTp_Rx.sn   = (uint8)((CanTp_Rx.sn + 1U) & 0x0FU);
        CanTp_Rx.timer = CanTp_GetNowMs();

        DET_LOGD(TAG, "RX CF sn=%u pos=%u", (unsigned)sn, (unsigned)CanTp_Rx.pos);

        if (CanTp_Rx.pos >= CanTp_Rx.msgLen)
        {
            CanTp_Rx.state = CANTP_RX_IDLE;

            DET_LOGD(TAG, "RX MF done len=%u", (unsigned)CanTp_Rx.msgLen);

            PduR_CanTpRxIndication(CANTP_RX_SDU_ID, E_OK);
        }
        else if (CanTp_Rx.bs > 0U)
        {
            /* [SWS_CanTp_00278]: ブロック（BS 本の CF）の最後の CF の後で、まだ続きがあれば、FC を送る
             * （BS・STmin は、どの FC も同じ値。[SWS_CanTp_00067]）。 */
            CanTp_Rx.bsCnt--;
            if (CanTp_Rx.bsCnt == 0U)
            {
                CanTp_Rx.bsCnt = CanTp_Rx.bs;

                if (!CanTp_SendRxFlowControl())
                {
                    break;
                }
                CanTp_Rx.timer = CanTp_GetNowMs();   /* 次のブロックの先頭の CF を、N_Cr で待つ */
            }
        }
        else
        {
            /* ブロックサイズ 0: FC は FF の後の 1 回だけ */
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
         * 古い/未初期化のバイトが混入したまま採用されてしまう）。
         * [SWS_CanTp_00349]: 送信中の FC が 8 バイト未満なら、送信セッションを E_NOT_OK で中断し、
         * ランタイムエラー CANTP_E_PADDING を報告する（以前は無視していたため、N_Bs タイムアウトの
         * 5 秒後にしか失敗が上位層へ伝わらなかった）。 */
        if (PduInfoPtr->SduLength < CANTP_FRAME_LEN)
        {
            DET_LOGE(TAG, "RX FC too short len=%u", (unsigned)PduInfoPtr->SduLength);
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_PADDING);
            CanTp_TxFinish(E_NOT_OK);
            break;
        }

        uint8 fs = pci & 0x0FU;

        DET_LOGD(TAG, "RX FC fs=%u", (unsigned)fs);

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
            CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
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
 * \brief   CanIf からの送信完了通知を受け取る。
 *
 * \details 現状、ID の検査以外は no-op（意図的）。SWS_CanTp_00355 は本来
 *          「result==E_NOT_OK ならセッションを中断する」ことを要求するが、
 *          本プロジェクトの非同期 TX 確認経路（Can.c の TX 確認保留キュー →
 *          CanIf_TxConfirmation()）は物理送信が同期的に失敗した場合そもそも
 *          キューに積まれず、かつ CanIf_TxConfirmation() 自体が E_OK を
 *          固定で返すため、result にはこの経路では実質的に E_NOT_OK が
 *          渡ってこない（真に非同期の送信失敗を表現する手段がない）。
 *          実際に到達可能な送信失敗（CanIf/Can_Write が同期的に拒否する
 *          ケース）は CanTp_SendFrame() の戻り値と、CF 送信の N_As 相当
 *          タイムアウト（CanTp_SendNextCF() 参照）で別途検出している。
 *
 * \param[in]  TxPduId  完了した TX N-PDU の ID（CANTP_TX_NPDU_ID。CanIf の設定が渡す）。
 * \param[in]  result   送信結果 (未使用。上記理由により本経路では常に E_OK)。
 *
 * \AUTOSARReq     {SWS_CanTp_00215}
 * \ServiceID      {0x40}
 */
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)result;

    if (!CanTp_Initialized)
    {
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_TX_CONFIRMATION, CANTP_E_UNINIT);
        return;
    }

    if (TxPduId != CANTP_TX_NPDU_ID)
    {
        DET_LOGE(TAG, "TxConf E: invalid TxPduId=%u", (unsigned)TxPduId);
        (void)Det_ReportError(CANTP_MODULE_ID, 0U, CANTP_API_ID_TX_CONFIRMATION, CANTP_E_INVALID_TX_ID);
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

/** frame（length バイト）を CanIf 経由で CAN 0x7E8 に送信する。
 *  \param[in] frame   送信するフレーム（CanTp_TxFrameBuf か fcFrame）。
 *  \param[in] length  フレームの長さ（クラシックは 8、FD は 12〜64 の有効な長さ。パディング済みのもの）。
 *  \retval E_OK     送信受け付け成功。
 *  \retval E_NOT_OK CanIf / Can_Write が失敗（TX バッファビジー等）。 */
static Std_ReturnType CanTp_SendFrame(uint8* frame, uint8 length)
{
    static PduInfoType pdu;
    pdu.SduDataPtr = frame;
    pdu.SduLength  = length;
    return CanIf_Transmit(CANTP_CANIF_TX_PDU_ID, &pdu);
}

/** フレームの長さ n を、送れる有効な長さへ切り上げる（[SWS_CanTp_00351]）。
 *  クラシックの長さ（8 以下）は、パディング ON のため 8 にする。FD では 12、16、20、24、32、48、64。
 *  \param[in] n  フレームに入れる長さ（PCI を含む。CANTP_FD_FRAME_LEN 以下）。
 *  \return    切り上げた有効な長さ。 */
static uint8 CanTp_RoundUpDl(uint8 n)
{
    uint8 r;

    if (n <= CANTP_FRAME_LEN)    { r = CANTP_FRAME_LEN; }
    else if (n <= 12U)           { r = 12U; }
    else if (n <= 16U)           { r = 16U; }
    else if (n <= 20U)           { r = 20U; }
    else if (n <= 24U)           { r = 24U; }
    else if (n <= 32U)           { r = 32U; }
    else if (n <= 48U)           { r = 48U; }
    else                         { r = CANTP_FD_FRAME_LEN; }
    return r;
}

/** 実行時エラーを DET へ報告する（モジュール ID・インスタンス ID は固定）。
 *  \param[in] apiId    報告元の API の ID（CANTP_API_ID_*）。
 *  \param[in] errorId  実行時エラーのコード（CANTP_E_PADDING、CANTP_E_COM、CANTP_E_RX_COM、CANTP_E_TX_COM）。 */
static void CanTp_ReportRuntimeError(uint8 apiId, uint8 errorId)
{
    (void)Det_ReportRuntimeError(CANTP_MODULE_ID, 0U, apiId, errorId);
}

/** 送信の最初のフレーム（SF、または FF）を、Dcm の送信データで組み立てて送信する。
 *  CanTp_Tx.msgLen に、送信する N-SDU の長さを設定しておくこと。
 *  \retval CANTP_START_SF_SENT  SF を送信した（確認通知は、呼び出し側が出す）。
 *  \retval CANTP_START_FF_SENT  FF を送信し、TX を WAIT_FC にした。
 *  \retval CANTP_START_BUSY     Dcm の送信データがまだ無い（[SWS_CanTp_00184]。呼び出し側が再試行する）。
 *  \retval CANTP_START_FAIL     データの取り出し、またはフレームの送信に失敗した。 */
static CanTp_StartResultType CanTp_TxStart(void)
{
    PduLengthType avail  = 0U;
    uint16        msgLen = CanTp_Tx.msgLen;
    uint8         dl     = CanTp_Tx.dl;
    PduInfoType   src;

    /* フレームの種類。SF: クラシックの形（PCI 1 バイト）、FD の形（PCI 2 バイト）。長さが SF に収まらなければ FF。
     * 7 バイト以下の N-SDU は、相手が FD でも、クラシックの SF で送る。 */
    boolean isSf   = (boolean)((msgLen <= (uint16)CANTP_SF_MAX_DATA) ||
                               ((dl > CANTP_FRAME_LEN) && (msgLen <= (uint16)(dl - CANTP_FD_SF_PCI))));
    boolean fdSf   = (boolean)((isSf != FALSE) && (msgLen > (uint16)CANTP_SF_MAX_DATA));
    uint8   hdrLen = (isSf == FALSE) ? (uint8)CANTP_FF_PCI : ((fdSf != FALSE) ? (uint8)CANTP_FD_SF_PCI : 1U);
    uint8   dataLen = (isSf != FALSE) ? (uint8)msgLen : (uint8)(dl - CANTP_FF_PCI);   /* FF: クラシック 6、FD は dl - 2 */
    uint8   frameSize = (isSf == FALSE) ? dl : ((fdSf != FALSE) ? CanTp_RoundUpDl((uint8)(msgLen + CANTP_FD_SF_PCI)) : (uint8)CANTP_FRAME_LEN);

    src.SduDataPtr = &CanTp_TxFrameBuf[hdrLen];
    src.SduLength  = (PduLengthType)dataLen;

    BufReq_ReturnType got = PduR_CanTpCopyTxData(CANTP_TX_SDU_ID, &src, NULL, &avail);
    if (got == BUFREQ_E_BUSY)
    {
        return CANTP_START_BUSY;
    }
    if (got != BUFREQ_OK)
    {
        DET_LOGE(TAG, "TX %s: CopyTxData failed", (isSf != FALSE) ? "SF" : "FF");
        return CANTP_START_FAIL;
    }

    if (isSf != FALSE)
    {
        /* ---- Single Frame ---- */
        if (fdSf != FALSE)
        {
            CanTp_TxFrameBuf[0] = 0x00U;            /* FD の SF: PCI は 0x00（エスケープ）+ SF_DL */
            CanTp_TxFrameBuf[1] = (uint8)msgLen;
        }
        else
        {
            CanTp_TxFrameBuf[0] = (uint8)msgLen;    /* SF PCI: 0x0N */
        }
        uint8 i;
        for (i = (uint8)(hdrLen + dataLen); i < frameSize; i++)
        {
            CanTp_TxFrameBuf[i] = CANTP_PADDING_BYTE;
        }

        DET_LOGD(TAG, "TX SF len=%u%s", (unsigned)msgLen, (fdSf != FALSE) ? " (FD)" : "");

        if (CanTp_SendFrame(CanTp_TxFrameBuf, frameSize) != E_OK)
        {
            DET_LOGE(TAG, "TX SF FAIL");
            return CANTP_START_FAIL;
        }
        return CANTP_START_SF_SENT;
    }

    /* ---- Multi Frame: First Frame ---- */
    CanTp_Tx.pos        = (uint16)dataLen;   /* FF で送信済みバイト数 */
    CanTp_Tx.sn         = 1U;
    CanTp_Tx.frameReady = 0U;

    /* FF PCI: [0x1H, 0xLL] + 先頭データ（クラシックは 6 バイト、FD は dl - 2 バイト。データは CopyTxData で書き込み済み） */
    CanTp_TxFrameBuf[0] = (uint8)(0x10U | (uint8)((msgLen >> 8U) & 0x0FU));
    CanTp_TxFrameBuf[1] = (uint8)(msgLen & 0xFFU);

    DET_LOGD(TAG, "TX FF len=%u%s", (unsigned)msgLen, (dl > CANTP_FRAME_LEN) ? " (FD)" : "");

    if (CanTp_SendFrame(CanTp_TxFrameBuf, frameSize) != E_OK)
    {
        DET_LOGE(TAG, "TX FF FAIL abort");
        return CANTP_START_FAIL;
    }

    CanTp_Tx.state       = CANTP_TX_WAIT_FC;
    CanTp_Tx.bsTimer     = CanTp_GetNowMs();
    CanTp_Tx.asFailTimer = 0UL;   /* 最初のフレームの待ちは、ここで終わり */
    return CANTP_START_FF_SENT;
}

/** SF/FF の受信を、上位層（Dcm）へ通知する。受け取ったデータ（src、len バイト）は、
 *  PduR_CanTpStartOfReception() の info（[SWS_CanTp_00329]）として渡す。データは、続く CopyRxData() で渡す。
 *  \param[in]  src      SF/FF のデータ（プロトコルの情報を除く）。
 *  \param[in]  len      src の長さ（最大 CANTP_MAX_FRAME_DATA）。
 *  \param[in]  total    N-SDU 全体の長さ。
 *  \param[out] bufSize  上位層の空き。 */
static BufReq_ReturnType CanTp_StartOfReception(const uint8* src, uint8 len, PduLengthType total, PduLengthType* bufSize)
{
    PduInfoType info;

    info.SduDataPtr = CanTp_StageRxData(src, len);
    info.SduLength  = (PduLengthType)len;
    return PduR_CanTpStartOfReception(CANTP_RX_SDU_ID, &info, total, bufSize);
}

/** 受信フレームのデータ（len バイト、最大 CANTP_MAX_FRAME_DATA。超える分は切り捨てる）を、内部の作業領域へ写し、その先頭を返す。
 *  上位層の関数は書き込み可能なポインタを取る（CopyRxData）、または受信フレームそのものを渡したくない（StartOfReception）ため、
 *  一度、写してから渡す。作業領域は 1 つで、渡した呼び出しが終わるまでだけ有効。 */
static uint8* CanTp_StageRxData(const uint8* src, uint8 len)
{
    static uint8 work[CANTP_MAX_FRAME_DATA];
    uint8 n = (len > CANTP_MAX_FRAME_DATA) ? (uint8)CANTP_MAX_FRAME_DATA : len;
    uint8 k;

    for (k = 0U; k < n; k++)
    {
        work[k] = src[k];
    }
    return work;
}

/** TX チャネルが、送信の途中（最初のフレームの待ち、FC 待ち、または CF の送信中）か。 */
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

/** 受信中の N-SDU を中断する。上位層（Dcm）へ PduR_CanTpRxIndication(E_NOT_OK) を通知し、RX を IDLE へ戻す。 */
static void CanTp_RxAbort(void)
{
    if (CanTp_Rx.state == CANTP_RX_WAIT_CF)
    {
        CanTp_Rx.state = CANTP_RX_IDLE;
        PduR_CanTpRxIndication(CANTP_RX_SDU_ID, E_NOT_OK);
    }
}

/** 送信を終える（正常終了、または中断）。TX を IDLE へ戻し、上位層（Dcm）へ PduR_CanTpTxConfirmation() を通知する。 */
static void CanTp_TxFinish(Std_ReturnType result)
{
    CanTp_Tx.state       = CANTP_TX_IDLE;
    CanTp_Tx.frameReady  = 0U;
    CanTp_Tx.asFailTimer = 0UL;
    PduR_CanTpTxConfirmation(CANTP_TX_SDU_ID, result);
}

/** 受信したフレームのデータ（len バイト、最大 CANTP_MAX_FRAME_DATA）を、上位層の受信バッファへコピーする。
 *  CopyRxData() は書き込み可能なポインタを取るので、受信フレームを一度、内部の作業領域へ写してから渡す。 */
static BufReq_ReturnType CanTp_CopyToUpper(const uint8* src, uint8 len)
{
    PduInfoType  info;
    PduLengthType remaining = 0U;

    info.SduDataPtr = CanTp_StageRxData(src, len);
    info.SduLength  = (PduLengthType)len;
    return PduR_CanTpCopyRxData(CANTP_RX_SDU_ID, &info, &remaining);
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
 *
 * \retval  E_OK      送信を受け付けた。
 * \retval  E_NOT_OK  CanIf / Can_Write が失敗した。
 */
static Std_ReturnType CanTp_SendFlowControl(uint8 fs, uint8 bs, uint8 stMin)
{
    static uint8 fcFrame[8];

    fcFrame[0] = (uint8)(0x30U | (fs & 0x0FU));
    fcFrame[1] = bs;
    fcFrame[2] = stMin;
    fcFrame[3] = CANTP_PADDING_BYTE;
    fcFrame[4] = CANTP_PADDING_BYTE;
    fcFrame[5] = CANTP_PADDING_BYTE;
    fcFrame[6] = CANTP_PADDING_BYTE;
    fcFrame[7] = CANTP_PADDING_BYTE;

    DET_LOGD(TAG, "TX FC fs=%u bs=%u", (unsigned)fs, (unsigned)bs);

    return CanTp_SendFrame(fcFrame, CANTP_FRAME_LEN);
}

/** 受信中の N-SDU の次のブロックのために、FC(CTS)（この受信で固定した BS、STmin）を送る。
 *  送信に失敗したら、受信を終了し（[SWS_CanTp_00342]。上位層へは E_NOT_OK を通知する）、DET の実行時エラーを報告する。
 *  \retval TRUE   送信を受け付けた。
 *  \retval FALSE  送信に失敗し、受信を終了した。 */
static boolean CanTp_SendRxFlowControl(void)
{
    if (CanTp_SendFlowControl(CANTP_FC_CTS, CanTp_Rx.bs, CANTP_ST_MIN) != E_OK)
    {
        DET_LOGE(TAG, "RX: FC send failed abort");
        CanTp_ReportRuntimeError(CANTP_API_ID_RX_INDICATION, CANTP_E_COM);
        CanTp_RxAbort();
        return FALSE;
    }
    return TRUE;
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
        uint8  copyLen   = (remaining > (uint16)(CanTp_Tx.dl - CANTP_CF_PCI))
                           ? (uint8)(CanTp_Tx.dl - CANTP_CF_PCI) : (uint8)remaining;

        PduInfoType   src;
        PduLengthType avail = 0U;
        src.SduDataPtr = &CanTp_TxFrameBuf[1];
        src.SduLength  = (PduLengthType)copyLen;

        BufReq_ReturnType got = PduR_CanTpCopyTxData(CANTP_TX_SDU_ID, &src, NULL, &avail);

        if (got == BUFREQ_E_BUSY)
        {
            /* データがまだ無い。次の CanTp_MainFunction 呼び出しで再試行する（[SWS_CanTp_00184]）。
             * ただし、送信データが用意されないまま続く場合に備え、N_Cs 相当のタイムアウトで中断する
             * （送信失敗の継続を測る N_As 用のタイマと、同じ時間・同じタイマを使う）。 */
            if (CanTp_TxStallTimedOut())
            {
                DET_LOGE(TAG, "TX CF: no data from upper layer (N_Cs) abort sn=%u", (unsigned)CanTp_Tx.sn);
                CanTp_ReportRuntimeError(CANTP_API_ID_MAIN_FUNCTION, CANTP_E_TX_COM);
                CanTp_TxFinish(E_NOT_OK);
            }
            return;
        }

        if (got != BUFREQ_OK)
        {
            DET_LOGE(TAG, "TX CF: CopyTxData failed abort sn=%u", (unsigned)CanTp_Tx.sn);
            CanTp_ReportRuntimeError(CANTP_API_ID_MAIN_FUNCTION, CANTP_E_COM);
            CanTp_TxFinish(E_NOT_OK);
            return;
        }

        CanTp_TxFrameBuf[0] = (uint8)(0x20U | (CanTp_Tx.sn & 0x0FU));

        /* フレームの長さ: クラシックは常に 8。FD は、PCI + データを、有効な長さへ切り上げる（余りはパディング）。 */
        uint8 frameSize = (CanTp_Tx.dl == CANTP_FRAME_LEN) ? (uint8)CANTP_FRAME_LEN
                                                           : CanTp_RoundUpDl((uint8)(copyLen + CANTP_CF_PCI));
        uint8 i;
        for (i = (uint8)(CANTP_CF_PCI + copyLen); i < frameSize; i++)
        {
            CanTp_TxFrameBuf[i] = CANTP_PADDING_BYTE;
        }
        CanTp_Tx.frameLen   = copyLen;
        CanTp_Tx.frameSize  = frameSize;
        CanTp_Tx.frameReady = 1U;
    }

    DET_LOGD(TAG, "TX CF sn=%u pos=%u", (unsigned)CanTp_Tx.sn, (unsigned)CanTp_Tx.pos);

    if (CanTp_SendFrame(CanTp_TxFrameBuf, CanTp_Tx.frameSize) != E_OK)
    {
        if (CanTp_TxStallTimedOut())
        {
            DET_LOGE(TAG, "TX N_As timeout abort sn=%u", (unsigned)CanTp_Tx.sn);
            CanTp_ReportRuntimeError(CANTP_API_ID_MAIN_FUNCTION, CANTP_E_TX_COM);
            CanTp_TxFinish(E_NOT_OK);
            return;
        }

        /* 送信失敗: pos/sn を進めず、組み立て済みの同じ CF を、次の CanTp_MainFunction 呼び出しで再送する。
         * [SWS_CanTp_00343] は、CanIf_Transmit() が E_NOT_OK のとき、接続を打ち切ることを求めるが、本実装は意図的に再送する
         * （N_As 相当の時間まで）。E_NOT_OK の主な原因は、Can_Write() が同期（mcp_can の sendMsg() が、TXREQ のクリアまで最大 2.5 ms 待つ）で、
         * バスの混雑による時間切れを E_NOT_OK にすること。仕様では、この「混んでいるだけ」の状態は、Can_Write() が CAN_BUSY を返し、
         * CanIf が送信バッファ（CanIfTxBuffer）で溜めて再送するため、CanTp には見えない。本プロジェクトは CAN_BUSY を返さず、
         * CanIf にも送信バッファが無いので、CanTp がその代わりをする（Arduino のライブラリの制約が理由の補い）。 */
        DET_LOGE(TAG, "TX CF FAIL sn=%u retry", (unsigned)CanTp_Tx.sn);
        return;
    }

    CanTp_Tx.asFailTimer = 0UL;   /* 送信成功: 失敗継続の記録をクリア */
    CanTp_Tx.frameReady  = 0U;
    CanTp_Tx.pos += (uint16)CanTp_Tx.frameLen;
    CanTp_Tx.sn   = (uint8)((CanTp_Tx.sn + 1U) & 0x0FU);

    if (CanTp_Tx.pos >= CanTp_Tx.msgLen)
    {
        DET_LOGD(TAG, "TX done");
        CanTp_TxFinish(E_OK);
        return;
    }

    /* ブロックサイズ管理 (bs=0 なら無制限) */
    if (CanTp_Tx.bs > 0U)
    {
        CanTp_Tx.bsCnt--;
        if (CanTp_Tx.bsCnt == 0U)
        {
            DET_LOGD(TAG, "TX block done wait FC");
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

/**
 * \brief   [テスト専用] TX チャネルを、Flow Control（FC）の待ち状態にする。
 *
 * \details マルチフレーム送信を実際に始めるには、上位層（Dcm）の送信データと CAN の送信が必要になる。
 *          受信した FC フレームの解釈（STmin の換算など）だけを調べるテストが、それを用意せずに
 *          FC 待ちの状態から始められるようにする。状態以外（送信済みバイト数、タイマーなど）は変えない
 *          （標準外の関数。CanNm_Test_SetState() と同じ設計方針）。
 */
void CanTp_Test_SetTxWaitFc(void)
{
    CanTp_Tx.state = CANTP_TX_WAIT_FC;
}

/**
 * \brief   [テスト専用] 受信側が FC で送るブロックサイズ（BS）を設定する。
 *
 * \details 設定値（CANTP_BLOCK_SIZE）は 0 のため、ブロックごとの FC の送信を確かめるために、実行時の値を変える。
 *          CanTp_Init() が、設定値へ戻す。
 *
 * \param[in]  bs  ブロックサイズ。
 */
void CanTp_Test_SetRxBlockSize(uint8 bs)
{
    CanTp_RxBlockSize = bs;
}

/**
 * \brief   [テスト専用] 相手が CAN FD のフレームで要求してきたことにする（応答を FD のフレームで返す）。
 *
 * \details 実際には、FD の SF/FF を受け付けたときに決まる。受信の処理（と、Dcm の応答）を経ずに、
 *          送信側（CanTp_Transmit）の FD の分岐だけを確かめるために、直接設定する（標準外の関数）。
 *
 * \param[in]  fd  TRUE なら FD の相手、FALSE ならクラシックの相手。
 */
void CanTp_Test_SetPeerFd(boolean fd)
{
    CanTp_PeerFd = fd;
}

/**
 * \brief   [テスト専用] 直近の FC フレームで受信し、換算した STmin（ms）を返す。
 *
 * \details CTS の FC フレームを受信すると、CanTp が、FC の STmin の生の値を ms に換算して保持する。
 *          その換算結果を、テストが確認するためだけに公開している（標準外の関数）。
 *
 * \return  換算後の STmin（ms）。
 */
uint8 CanTp_Test_GetTxStMin(void)
{
    return CanTp_Tx.stMin;
}
#endif

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
 *          RX 状態マシン:
 *            IDLE → SF 受信 → Dcm_ComIndication → IDLE
 *            IDLE → FF 受信 → FC 送信 → WAIT_CF
 *            WAIT_CF → CF 受信 → 完成なら Dcm_ComIndication → IDLE
 *                             → 未完成なら WAIT_CF (N_Cr リセット)
 *            WAIT_CF → N_Cr タイムアウト → IDLE (中断)
 *
 *          TX 状態マシン:
 *            IDLE → CanTp_Transmit (<=7B) → SF 送信 → IDLE
 *            IDLE → CanTp_Transmit (>7B) → FF 送信 → WAIT_FC
 *            WAIT_FC → FC(CTS) 受信 → SEND_CF
 *            WAIT_FC → N_Bs タイムアウト → IDLE (中断)
 *            SEND_CF → CF 送信 (CanTp_MainFunction) → 完成なら IDLE
 *                    → BS 消費なら WAIT_FC
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "CanTp.h"
#include "PduR.h"
#include "Dcm_Cbk.h"
#include "Det.h"

extern unsigned long millis(void);

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

/** RX チャネル状態 (FF/CF 組立バッファ含む) */
static struct {
    uint8             buf[CANTP_RX_BUFFER_SIZE];
    uint16            msgLen;    /**< FF で通知された総メッセージ長 */
    uint16            pos;       /**< buf への書き込み済みバイト数  */
    uint8             sn;        /**< 次に受信すべき CF シーケンス番号 */
    CanTp_RxStateType state;
    unsigned long     timer;     /**< N_Cr タイムアウト基点 (ms)   */
} CanTp_Rx;

/** TX チャネル状態 (FF/CF 分割バッファ含む) */
static struct {
    uint8             buf[CANTP_TX_BUFFER_SIZE];
    uint16            msgLen;    /**< 送信する総メッセージ長        */
    uint16            pos;       /**< buf からの読み出し済みバイト数 */
    uint8             sn;        /**< 次の CF シーケンス番号 (1-15, 0...) */
    uint8             bs;        /**< FC で受信したブロックサイズ   */
    uint8             bsCnt;     /**< 現ブロック内残り送信数        */
    uint8             stMin;     /**< FC で受信した STmin (ms)      */
    CanTp_TxStateType state;
    unsigned long     bsTimer;   /**< N_Bs タイムアウト基点 (ms)   */
    unsigned long     cfTimer;   /**< STmin タイマ基点 (ms)        */
} CanTp_Tx;

/** TX フレーム送信用バッファ (SF/FF/CF/FC 共用) */
static uint8 CanTp_TxFrameBuf[8];

/* -----------------------------------------------------------------------
 * 内部関数プロトタイプ
 * ----------------------------------------------------------------------- */
static void CanTp_SendFrame(void);
static void CanTp_SendFlowControl(uint8 fs, uint8 bs, uint8 stMin);
static void CanTp_SendNextCF(void);

/* -----------------------------------------------------------------------
 * CanTp_Init
 * ----------------------------------------------------------------------- */

/**
 * \brief   CanTp モジュールを初期化する。
 *
 * \details RX/TX チャネルの状態を IDLE にリセットする (SWS_CanTp_00208)。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_Init(void)
{
    CanTp_Rx.state  = CANTP_RX_IDLE;
    CanTp_Tx.state  = CANTP_TX_IDLE;
    Det_LogP(PSTR("[CanTp_Init] initialized"));
}

/* -----------------------------------------------------------------------
 * 内部ヘルパー: フレーム送信
 * ----------------------------------------------------------------------- */

/** CanTp_TxFrameBuf の内容 (8 バイト) を PduR 経由で CAN 0x7E8 に送信する。 */
static void CanTp_SendFrame(void)
{
    static PduInfoType pdu;
    pdu.SduDataPtr = CanTp_TxFrameBuf;
    pdu.SduLength  = 8U;
    PduR_Transmit(CANTP_PDUR_TX_SDU_ID, &pdu);
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
    CanTp_TxFrameBuf[0] = (uint8)(0x30U | (fs & 0x0FU));
    CanTp_TxFrameBuf[1] = bs;
    CanTp_TxFrameBuf[2] = stMin;
    CanTp_TxFrameBuf[3] = 0x00U;
    CanTp_TxFrameBuf[4] = 0x00U;
    CanTp_TxFrameBuf[5] = 0x00U;
    CanTp_TxFrameBuf[6] = 0x00U;
    CanTp_TxFrameBuf[7] = 0x00U;

    Det_PrintP(PSTR("[CanTp_Tx] FC fs="));
    Det_PrintDec(fs);
    Det_PrintP(PSTR(" bs="));
    Det_PrintDec(bs);
    Det_Newline();

    CanTp_SendFrame();
}

/**
 * \brief   次の Consecutive Frame を組立て送信する。
 *
 * \details CanTp_Tx.buf からデータを取り出し CF PCI を付けて送信する。
 *          送信後、位置・シーケンス番号を更新する。
 *          全バイト送信完了なら IDLE へ遷移し、BS を消費したなら WAIT_FC へ遷移する。
 */
static void CanTp_SendNextCF(void)
{
    uint16 remaining = CanTp_Tx.msgLen - CanTp_Tx.pos;
    uint8  copyLen   = (remaining > (uint16)CANTP_CF_DATA)
                       ? CANTP_CF_DATA : (uint8)remaining;

    CanTp_TxFrameBuf[0] = (uint8)(0x20U | (CanTp_Tx.sn & 0x0FU));
    uint8 i;
    for (i = 0U; i < copyLen; i++)
        CanTp_TxFrameBuf[1U + i] = CanTp_Tx.buf[CanTp_Tx.pos + i];
    for (; i < CANTP_CF_DATA; i++)
        CanTp_TxFrameBuf[1U + i] = 0x00U;

    Det_PrintP(PSTR("[CanTp_Tx] CF sn="));
    Det_PrintDec(CanTp_Tx.sn);
    Det_PrintP(PSTR(" pos="));
    Det_PrintDec(CanTp_Tx.pos);
    Det_Newline();

    CanTp_SendFrame();

    CanTp_Tx.pos += (uint16)copyLen;
    CanTp_Tx.sn   = (uint8)((CanTp_Tx.sn + 1U) & 0x0FU);

    if (CanTp_Tx.pos >= CanTp_Tx.msgLen)
    {
        Det_LogP(PSTR("[CanTp_Tx] complete"));
        CanTp_Tx.state = CANTP_TX_IDLE;
        return;
    }

    /* ブロックサイズ管理 (bs=0 なら無制限) */
    if (CanTp_Tx.bs > 0U)
    {
        CanTp_Tx.bsCnt--;
        if (CanTp_Tx.bsCnt == 0U)
        {
            Det_LogP(PSTR("[CanTp_Tx] block done, wait FC"));
            CanTp_Tx.state   = CANTP_TX_WAIT_FC;
            CanTp_Tx.bsTimer = millis();
        }
    }
}

/* -----------------------------------------------------------------------
 * CanTp_Transmit
 * ----------------------------------------------------------------------- */

/**
 * \brief   DCM からの UDS ペイロードを CAN フレームに分割して送信する。
 *
 * \details ペイロード長 <= 7 なら SF 送信、>= 8 なら FF 送信 + WAIT_FC へ遷移
 *          (SWS_CanTp_00218)。
 *
 * \param[in]  TxSduId     TX N-SDU ID (単一チャネルのため未使用)。
 * \param[in]  PduInfoPtr  UDS ペイロード (PCI バイトを含まない)。
 *
 * \retval  E_OK      送信受け付け済み。
 * \retval  E_NOT_OK  NULL ポインタ、長さ超過、またはチャネルビジー。
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanTp_Transmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr)
{
    (void)TxSduId;

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
        return E_NOT_OK;

    uint16 msgLen = (uint16)PduInfoPtr->SduLength;

    if (msgLen == 0U || msgLen > (uint16)CANTP_TX_BUFFER_SIZE)
    {
        Det_LogP(PSTR("[CanTp_Tx] E: invalid length"));
        return E_NOT_OK;
    }

    if (msgLen <= (uint16)CANTP_SF_MAX_DATA)
    {
        /* ---- Single Frame ---- */
        CanTp_TxFrameBuf[0] = (uint8)msgLen;   /* SF PCI: 0x0N */
        uint8 i;
        for (i = 0U; i < (uint8)msgLen; i++)
            CanTp_TxFrameBuf[1U + i] = PduInfoPtr->SduDataPtr[i];
        for (; i < CANTP_SF_MAX_DATA; i++)
            CanTp_TxFrameBuf[1U + i] = 0x00U;

        Det_PrintP(PSTR("[CanTp_Tx] SF len="));
        Det_PrintDec((uint8)msgLen);
        Det_Newline();

        CanTp_SendFrame();
        return E_OK;
    }

    /* ---- Multi Frame: First Frame ---- */
    if (CanTp_Tx.state != CANTP_TX_IDLE)
    {
        Det_LogP(PSTR("[CanTp_Tx] E: TX busy"));
        return E_NOT_OK;
    }

    /* メッセージ全体を TX バッファへコピー */
    uint16 i;
    for (i = 0U; i < msgLen; i++)
        CanTp_Tx.buf[i] = PduInfoPtr->SduDataPtr[i];
    CanTp_Tx.msgLen = msgLen;
    CanTp_Tx.pos    = (uint16)CANTP_FF_DATA;   /* FF で送信済みバイト数 */
    CanTp_Tx.sn     = 1U;

    /* FF PCI: [0x1H, 0xLL] + 先頭 6 バイト */
    CanTp_TxFrameBuf[0] = (uint8)(0x10U | (uint8)((msgLen >> 8U) & 0x0FU));
    CanTp_TxFrameBuf[1] = (uint8)(msgLen & 0xFFU);
    uint8 k;
    for (k = 0U; k < CANTP_FF_DATA; k++)
        CanTp_TxFrameBuf[2U + k] = CanTp_Tx.buf[k];

    Det_PrintP(PSTR("[CanTp_Tx] FF len="));
    Det_PrintDec((uint8)msgLen);
    Det_Newline();

    CanTp_SendFrame();

    CanTp_Tx.state   = CANTP_TX_WAIT_FC;
    CanTp_Tx.bsTimer = millis();

    return E_OK;
}

/* -----------------------------------------------------------------------
 * CanTp_RxIndication
 * ----------------------------------------------------------------------- */

/**
 * \brief   PduR から配信された受信 CAN フレームを処理する。
 *
 * \details フレームタイプを判定して SF/FF/CF/FC それぞれを処理する
 *          (SWS_CanTp_00214)。SF または完全に組み立てた UDS メッセージは
 *          Dcm_ComIndication へ渡す。
 *
 * \param[in]  RxPduId     受信 PDU ID (単一チャネルのため未使用)。
 * \param[in]  PduInfoPtr  受信した 8 バイト CAN フレーム。NULL 禁止。
 *
 * \ServiceID      {0x42}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)RxPduId;

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL
        || PduInfoPtr->SduLength == 0U)
        return;

    const uint8* data      = PduInfoPtr->SduDataPtr;
    uint8        pci       = data[0];
    uint8        frameType = (uint8)((pci >> 4U) & 0x0FU);

    switch (frameType)
    {
    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_SF:
    {
        uint8 udsLen = pci & 0x0FU;

        if (udsLen == 0U || udsLen > CANTP_SF_MAX_DATA
            || PduInfoPtr->SduLength < (PduLengthType)(1U + udsLen))
            break;

        uint8 i;
        for (i = 0U; i < udsLen; i++)
            CanTp_Rx.buf[i] = data[1U + i];

        CanTp_Rx.state = CANTP_RX_IDLE;   /* 受信中の MF があれば中断 */

        static PduInfoType sfUds;
        sfUds.SduDataPtr = CanTp_Rx.buf;
        sfUds.SduLength  = (PduLengthType)udsLen;

        Det_PrintP(PSTR("[CanTp_Rx] SF udsLen="));
        Det_PrintDec(udsLen);
        Det_Newline();

        Dcm_ComIndication(0U, &sfUds);
        break;
    }

    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_FF:
    {
        uint16 msgLen = (uint16)(((uint16)(pci & 0x0FU) << 8U)
                                 | (uint16)data[1]);

        if (msgLen <= CANTP_SF_MAX_DATA || PduInfoPtr->SduLength < 8U)
            break;

        if (msgLen > (uint16)CANTP_RX_BUFFER_SIZE)
        {
            Det_LogP(PSTR("[CanTp_Rx] FF: overflow, send FC(OVFLW)"));
            CanTp_SendFlowControl(CANTP_FC_OVFLW, 0U, 0U);
            break;
        }

        CanTp_Rx.msgLen = msgLen;
        CanTp_Rx.sn     = 1U;
        CanTp_Rx.state  = CANTP_RX_WAIT_CF;
        CanTp_Rx.timer  = millis();

        /* FF の先頭 6 バイトをコピー */
        uint16 copyLen = (msgLen < (uint16)CANTP_FF_DATA)
                         ? msgLen : (uint16)CANTP_FF_DATA;
        uint8 i;
        for (i = 0U; i < (uint8)copyLen; i++)
            CanTp_Rx.buf[i] = data[2U + i];
        CanTp_Rx.pos = copyLen;

        Det_PrintP(PSTR("[CanTp_Rx] FF msgLen="));
        Det_PrintDec(msgLen);
        Det_Newline();

        CanTp_SendFlowControl(CANTP_FC_CTS, CANTP_BLOCK_SIZE, CANTP_ST_MIN);
        break;
    }

    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_CF:
    {
        if (CanTp_Rx.state != CANTP_RX_WAIT_CF)
            break;

        uint8 sn = pci & 0x0FU;

        if (sn != (CanTp_Rx.sn & 0x0FU))
        {
            Det_PrintP(PSTR("[CanTp_Rx] CF SN err exp="));
            Det_PrintDec(CanTp_Rx.sn & 0x0FU);
            Det_PrintP(PSTR(" got="));
            Det_PrintDec(sn);
            Det_Newline();
            CanTp_Rx.state = CANTP_RX_IDLE;
            break;
        }

        uint16 remaining = CanTp_Rx.msgLen - CanTp_Rx.pos;
        uint8  copyLen   = (remaining > (uint16)CANTP_CF_DATA)
                           ? CANTP_CF_DATA : (uint8)remaining;
        uint8 i;
        for (i = 0U; i < copyLen; i++)
            CanTp_Rx.buf[CanTp_Rx.pos + i] = data[1U + i];

        CanTp_Rx.pos += (uint16)copyLen;
        CanTp_Rx.sn   = (uint8)((CanTp_Rx.sn + 1U) & 0x0FU);
        CanTp_Rx.timer = millis();

        Det_PrintP(PSTR("[CanTp_Rx] CF sn="));
        Det_PrintDec(sn);
        Det_PrintP(PSTR(" pos="));
        Det_PrintDec(CanTp_Rx.pos);
        Det_Newline();

        if (CanTp_Rx.pos >= CanTp_Rx.msgLen)
        {
            CanTp_Rx.state = CANTP_RX_IDLE;

            static PduInfoType mfUds;
            mfUds.SduDataPtr = CanTp_Rx.buf;
            mfUds.SduLength  = (PduLengthType)CanTp_Rx.msgLen;

            Det_PrintP(PSTR("[CanTp_Rx] MF complete msgLen="));
            Det_PrintDec(CanTp_Rx.msgLen);
            Det_Newline();

            Dcm_ComIndication(0U, &mfUds);
        }
        break;
    }

    /* ------------------------------------------------------------------ */
    case CANTP_FRAME_FC:
    {
        if (CanTp_Tx.state != CANTP_TX_WAIT_FC)
            break;

        uint8 fs = pci & 0x0FU;

        Det_PrintP(PSTR("[CanTp_Rx] FC fs="));
        Det_PrintDec(fs);
        Det_Newline();

        if (fs == CANTP_FC_CTS)
        {
            CanTp_Tx.bs      = data[1];
            CanTp_Tx.stMin   = data[2];
            CanTp_Tx.bsCnt   = CanTp_Tx.bs;
            CanTp_Tx.state   = CANTP_TX_SEND_CF;
            CanTp_Tx.cfTimer = millis();
        }
        else if (fs == CANTP_FC_WAIT)
        {
            CanTp_Tx.bsTimer = millis();   /* N_Bs タイマリセット */
        }
        else   /* OVFLW */
        {
            Det_LogP(PSTR("[CanTp_Tx] FC OVFLW: abort TX"));
            CanTp_Tx.state = CANTP_TX_IDLE;
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
 * \details CF 送信は CanTp_MainFunction のタイマドリブンで行うため、現在 no-op
 *          (SWS_CanTp_00236)。
 *
 * \param[in]  TxPduId  完了した PDU ID (未使用)。
 * \param[in]  result   送信結果 (未使用)。
 *
 * \ServiceID      {0x48}
 */
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)TxPduId;
    (void)result;
}

/* -----------------------------------------------------------------------
 * CanTp_MainFunction
 * ----------------------------------------------------------------------- */

/**
 * \brief   タイムアウト監視と CF 送信を周期的に処理する。
 *
 * \details EcuM_MainFunction から毎ループ呼び出す (SWS_CanTp_00236 相当)。
 *
 * \ServiceID      {0x1B}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_MainFunction(void)
{
    unsigned long now = millis();

    /* ---- TX: N_Bs タイムアウト (WAIT_FC) ---- */
    if (CanTp_Tx.state == CANTP_TX_WAIT_FC)
    {
        if (now - CanTp_Tx.bsTimer >= CANTP_N_BS_TIMEOUT_MS)
        {
            Det_LogP(PSTR("[CanTp_Tx] N_Bs timeout: abort TX"));
            CanTp_Tx.state = CANTP_TX_IDLE;
        }
    }

    /* ---- TX: CF 送信 (SEND_CF) ---- */
    if (CanTp_Tx.state == CANTP_TX_SEND_CF)
    {
        if (now - CanTp_Tx.cfTimer >= (unsigned long)CanTp_Tx.stMin)
        {
            CanTp_SendNextCF();
            CanTp_Tx.cfTimer = now;
        }
    }

    /* ---- RX: N_Cr タイムアウト (WAIT_CF) ---- */
    if (CanTp_Rx.state == CANTP_RX_WAIT_CF)
    {
        if (now - CanTp_Rx.timer >= CANTP_N_CR_TIMEOUT_MS)
        {
            Det_LogP(PSTR("[CanTp_Rx] N_Cr timeout: abort RX"));
            CanTp_Rx.state = CANTP_RX_IDLE;
        }
    }
}

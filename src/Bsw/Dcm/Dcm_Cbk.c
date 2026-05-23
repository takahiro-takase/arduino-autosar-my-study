/**
 * \file    Dcm_Cbk.c
 * \brief   DCM モジュール実装 (AUTOSAR SWS_DCM 準拠, UDS ISO 14229-1)
 * \details PduR から配信された UDS 診断フレームを解析し、
 *          以下の UDS サービスを処理して CAN 0x7E8 で応答する。
 *
 *          対応サービス:
 *            0x10 DiagnosticSessionControl — Default / Extended セッション切替
 *            0x11 ECUReset               — hardReset / softReset (応答後セッションリセット)
 *            0x22 ReadDataByIdentifier   — DID 0x0101/0x0102/0x0103
 *            0x3E TesterPresent          — セッション維持
 *
 *          ISO 15765-2 (CAN TP) は Single Frame (SF) のみ対応。
 *          マルチフレーム (FF/CF) は本実装の範囲外。
 *
 *          応答送信フロー:
 *            Dcm → PduR_Transmit(DCM_TX_PDUR_ID) → CanIf_Transmit(1)
 *            → Can_Write → MCP2515 → CAN 0x7E8
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "PduR.h"
#include "Rte.h"
#include "Det.h"

/* -----------------------------------------------------------------------
 * モジュール内部状態
 * ----------------------------------------------------------------------- */

/** 現在の診断セッション (DCM_SESSION_DEFAULT / DCM_SESSION_EXTENDED) */
static uint8 Dcm_CurrentSession;

/** CAN 送信応答バッファ (DLC=8 固定, ISO 15765-2 Single Frame) */
static uint8 Dcm_TxBuf[8];

/** PduR_Transmit に渡す PDU 情報構造体 */
static PduInfoType Dcm_TxPdu;

/* -----------------------------------------------------------------------
 * 内部関数プロトタイプ
 * ----------------------------------------------------------------------- */
static void Dcm_Transmit(void);
static void Dcm_SendNegativeResponse(uint8 sid, uint8 nrc);
static void Dcm_HandleSessionControl(const uint8* uds, uint8 udsLen);
static void Dcm_HandleEcuReset(const uint8* uds, uint8 udsLen);
static void Dcm_HandleClearDtc(const uint8* uds, uint8 udsLen);
static void Dcm_HandleReadDtcInfo(const uint8* uds, uint8 udsLen);
static void Dcm_HandleReadDtcCount(const uint8* uds, uint8 udsLen);
static void Dcm_HandleReadDtcByMask(const uint8* uds, uint8 udsLen);
static void Dcm_HandleReadDataById(const uint8* uds, uint8 udsLen);
static void Dcm_HandleTesterPresent(const uint8* uds, uint8 udsLen);
static Std_ReturnType Dcm_ReadDid(uint16 did, uint8* buf, uint8* dataLen);

/* -----------------------------------------------------------------------
 * Dcm_Init
 * ----------------------------------------------------------------------- */

/**
 * \brief   DCM モジュールを初期化する。
 *
 * \details セッション状態を defaultSession にリセットし、
 *          TX バッファポインタを設定する (SWS_Dcm_00769)。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_Init(void)
{
    Dcm_CurrentSession   = DCM_SESSION_DEFAULT;
    Dcm_TxPdu.SduDataPtr = Dcm_TxBuf;
    Dcm_TxPdu.SduLength  = (PduLengthType)sizeof(Dcm_TxBuf);
    Det_LogP(PSTR("[Dcm_Init] session=Default"));
}

/* -----------------------------------------------------------------------
 * 応答送信ヘルパー
 * ----------------------------------------------------------------------- */

/** Dcm_TxBuf の内容を PduR 経由で CAN 0x7E8 に送信する。 */
static void Dcm_Transmit(void)
{
    PduR_Transmit(DCM_TX_PDUR_ID, &Dcm_TxPdu);
}

/**
 * \brief   UDS 否定応答 (NRC) フレームを送信する。
 *
 * \details ISO 14229-1 に従い [0x03, 0x7F, SID, NRC] を送信する。
 *
 * \param[in]  sid  失敗したサービス ID。
 * \param[in]  nrc  否定応答コード (DCM_NRC_*)。
 */
static void Dcm_SendNegativeResponse(uint8 sid, uint8 nrc)
{
    Dcm_TxBuf[0] = 0x03U;               /* PCI: SF, UDS length=3 */
    Dcm_TxBuf[1] = DCM_SID_NEGATIVE_RESP;
    Dcm_TxBuf[2] = sid;
    Dcm_TxBuf[3] = nrc;
    Dcm_TxBuf[4] = 0x00U;
    Dcm_TxBuf[5] = 0x00U;
    Dcm_TxBuf[6] = 0x00U;
    Dcm_TxBuf[7] = 0x00U;

    Det_PrintP(PSTR("[Dcm] NRC sid=0x"));
    Det_PrintHex(sid);
    Det_PrintP(PSTR(" nrc=0x"));
    Det_PrintHex(nrc);
    Det_Newline();

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * 0x10 DiagnosticSessionControl
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x10 DiagnosticSessionControl を処理する。
 *
 * \details subFunction に応じてセッションを切り替え、
 *          P2/P2* タイミングパラメータを含む正応答を返す。
 *          対応: 0x01=defaultSession, 0x03=extendedDiagnosticSession
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ (uds[0]=SID)。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleSessionControl(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 2U)
    {
        Dcm_SendNegativeResponse(DCM_SID_SESSION_CTRL, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    /* bit7: suppressPosRspMsgIndicationBit (本実装では無視) */
    uint8 subFunc = uds[1] & 0x7FU;

    if (subFunc != DCM_SESSION_DEFAULT && subFunc != DCM_SESSION_EXTENDED)
    {
        Dcm_SendNegativeResponse(DCM_SID_SESSION_CTRL, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
        return;
    }

    Dcm_CurrentSession = subFunc;

    Det_PrintP(PSTR("[Dcm 0x10] session=0x"));
    Det_PrintHex(subFunc);
    Det_Newline();

    /* 正応答: [0x06, 0x50, subFunc, P2_H, P2_L, P2X_H, P2X_L] */
    Dcm_TxBuf[0] = 0x06U;
    Dcm_TxBuf[1] = 0x50U;               /* SID + 0x40 */
    Dcm_TxBuf[2] = subFunc;
    Dcm_TxBuf[3] = DCM_SESSION_P2_HIGH;
    Dcm_TxBuf[4] = DCM_SESSION_P2_LOW;
    Dcm_TxBuf[5] = DCM_SESSION_P2X_HIGH;
    Dcm_TxBuf[6] = DCM_SESSION_P2X_LOW;
    Dcm_TxBuf[7] = 0x00U;

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * 0x11 ECUReset
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x11 ECUReset を処理する。
 *
 * \details 正応答送信後、セッションを defaultSession に戻す。
 *          Arduino UNO にはウォッチドッグタイマがあるが、本実装では
 *          ハードウェアリセットを発行せずログのみで代替する（学習用簡略化）。
 *          対応: 0x01=hardReset, 0x03=softReset
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleEcuReset(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 2U)
    {
        Dcm_SendNegativeResponse(DCM_SID_ECU_RESET, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint8 subFunc = uds[1];

    if (subFunc != DCM_RESET_HARD && subFunc != DCM_RESET_SOFT)
    {
        Dcm_SendNegativeResponse(DCM_SID_ECU_RESET, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
        return;
    }

    Det_PrintP(PSTR("[Dcm 0x11] ECUReset sub=0x"));
    Det_PrintHex(subFunc);
    Det_Newline();

    /* 正応答: [0x02, 0x51, subFunc] */
    Dcm_TxBuf[0] = 0x02U;
    Dcm_TxBuf[1] = 0x51U;               /* SID + 0x40 */
    Dcm_TxBuf[2] = subFunc;
    Dcm_TxBuf[3] = 0x00U;
    Dcm_TxBuf[4] = 0x00U;
    Dcm_TxBuf[5] = 0x00U;
    Dcm_TxBuf[6] = 0x00U;
    Dcm_TxBuf[7] = 0x00U;

    Dcm_Transmit();

    /* リセット後処理: セッションをデフォルトに戻す */
    Dcm_CurrentSession = DCM_SESSION_DEFAULT;
    Det_LogP(PSTR("[Dcm 0x11] session reset to Default"));
}

/* -----------------------------------------------------------------------
 * 0x14 ClearDiagnosticInformation
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x14 ClearDiagnosticInformation を処理する。
 *
 * \details groupOfDTC=0xFFFFFF (全 DTC クリア) のみ対応する。
 *          Dem_ClearAllDTCs() を呼び出して DTC ステータスと EEPROM をリセットし、
 *          正応答 [0x01, 0x54] を返す。
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ (uds[0]=SID 0x14)。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleClearDtc(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 4U)
    {
        Dcm_SendNegativeResponse(DCM_SID_CLEAR_DTC, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint32 group = ((uint32)uds[1] << 16U)
                 | ((uint32)uds[2] <<  8U)
                 | (uint32)uds[3];

    if (group != DEM_GROUP_ALL_DTCS)
    {
        /* 本実装はグループ指定クリアを未対応 */
        Dcm_SendNegativeResponse(DCM_SID_CLEAR_DTC, DCM_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    Det_LogP(PSTR("[Dcm 0x14] ClearAllDTCs"));
    Dem_ClearAllDTCs();

    /* 正応答: [0x01, 0x54] */
    Dcm_TxBuf[0] = 0x01U;
    Dcm_TxBuf[1] = 0x54U;               /* SID 0x14 + 0x40 */
    Dcm_TxBuf[2] = 0x00U;
    Dcm_TxBuf[3] = 0x00U;
    Dcm_TxBuf[4] = 0x00U;
    Dcm_TxBuf[5] = 0x00U;
    Dcm_TxBuf[6] = 0x00U;
    Dcm_TxBuf[7] = 0x00U;

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * 0x19 ReadDTCInformation — サブ機能ディスパッチ
 * ----------------------------------------------------------------------- */

/**
 * \brief   SID 0x19 subFunc 0x01 reportNumberOfDTCByStatusMask を処理する。
 *
 * \details statusMask に一致する DTC の件数を返す。
 *          応答: [0x06, 0x59, 0x01, statusAvailMask, dtcFormat, countH, countL]
 *          (7 バイト UDS ペイロード = 1 SF に収まる)
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleReadDtcCount(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 3U)
    {
        Dcm_SendNegativeResponse(DCM_SID_READ_DTC_INFO, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint8  statusMask = uds[2];
    uint32 dtcBuf[DEM_EVENT_COUNT];
    uint8  statusBuf[DEM_EVENT_COUNT];
    uint8  count = 0U;

    Dem_GetAllDTCs(dtcBuf, statusBuf, &count, statusMask);

    Det_PrintP(PSTR("[Dcm 0x19/01] mask=0x"));
    Det_PrintHex(statusMask);
    Det_PrintP(PSTR(" count="));
    Det_PrintDec(count);
    Det_Newline();

    /* 正応答: [0x06, 0x59, 0x01, statusAvailMask, dtcFormat, countH, countL] */
    Dcm_TxBuf[0] = 0x06U;
    Dcm_TxBuf[1] = 0x59U;                        /* SID 0x19 + 0x40 */
    Dcm_TxBuf[2] = DCM_DTC_SUBFUNC_REPORT_COUNT;
    Dcm_TxBuf[3] = DEM_STATUS_AVAILABILITY_MASK;
    Dcm_TxBuf[4] = DCM_DTC_FORMAT_ISO15031;
    Dcm_TxBuf[5] = 0x00U;                        /* countH */
    Dcm_TxBuf[6] = count;                        /* countL */
    Dcm_TxBuf[7] = 0x00U;

    Dcm_Transmit();
}

/**
 * \brief   SID 0x19 subFunc 0x02 reportDTCByStatusMask を処理する。
 *
 * \details statusMask に一致する DTC を返す。
 *          Single Frame (最大 7 バイト UDS ペイロード) の制約上、
 *          1 件の DTC (3 バイト DTC + 1 バイトステータス = 4 バイト) のみ返す。
 *          複数件ある場合は先頭 1 件のみ返し、残りはシリアルログに記録する。
 *          応答 (0 件): [0x03, 0x59, 0x02, statusAvailMask]
 *          応答 (1 件): [0x07, 0x59, 0x02, statusAvailMask, D1, D2, D3, status]
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleReadDtcByMask(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 3U)
    {
        Dcm_SendNegativeResponse(DCM_SID_READ_DTC_INFO, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint8  statusMask = uds[2];
    uint32 dtcBuf[DEM_EVENT_COUNT];
    uint8  statusBuf[DEM_EVENT_COUNT];
    uint8  count = 0U;

    Dem_GetAllDTCs(dtcBuf, statusBuf, &count, statusMask);

    Det_PrintP(PSTR("[Dcm 0x19/02] mask=0x"));
    Det_PrintHex(statusMask);
    Det_PrintP(PSTR(" found="));
    Det_PrintDec(count);
    Det_Newline();

    if (count == 0U)
    {
        /* DTC なし: statusAvailabilityMask のみ返す */
        Dcm_TxBuf[0] = 0x03U;
        Dcm_TxBuf[1] = 0x59U;
        Dcm_TxBuf[2] = DCM_DTC_SUBFUNC_REPORT_BY_MASK;
        Dcm_TxBuf[3] = DEM_STATUS_AVAILABILITY_MASK;
        Dcm_TxBuf[4] = 0x00U;
        Dcm_TxBuf[5] = 0x00U;
        Dcm_TxBuf[6] = 0x00U;
        Dcm_TxBuf[7] = 0x00U;
    }
    else
    {
        /* SF 制約: 先頭 1 件のみ返す (マルチフレーム未対応) */
        if (count > 1U)
        {
            Det_LogP(PSTR("[Dcm 0x19/02] truncated to 1 DTC (SF limit)"));
        }

        Dcm_TxBuf[0] = 0x07U;
        Dcm_TxBuf[1] = 0x59U;
        Dcm_TxBuf[2] = DCM_DTC_SUBFUNC_REPORT_BY_MASK;
        Dcm_TxBuf[3] = DEM_STATUS_AVAILABILITY_MASK;
        Dcm_TxBuf[4] = (uint8)(dtcBuf[0] >> 16U);   /* DTC 上位バイト */
        Dcm_TxBuf[5] = (uint8)(dtcBuf[0] >>  8U);   /* DTC 中位バイト */
        Dcm_TxBuf[6] = (uint8)(dtcBuf[0]);           /* DTC 下位バイト */
        Dcm_TxBuf[7] = statusBuf[0];                 /* DTC ステータス */
    }

    Dcm_Transmit();
}

/**
 * \brief   UDS 0x19 ReadDTCInformation のサブ機能をディスパッチする。
 *
 * \details 対応サブ機能:
 *            0x01 reportNumberOfDTCByStatusMask → Dcm_HandleReadDtcCount()
 *            0x02 reportDTCByStatusMask         → Dcm_HandleReadDtcByMask()
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleReadDtcInfo(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 2U)
    {
        Dcm_SendNegativeResponse(DCM_SID_READ_DTC_INFO, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint8 subFunc = uds[1] & 0x7FU;   /* bit7: suppressPosRsp (本実装では無視) */

    switch (subFunc)
    {
    case DCM_DTC_SUBFUNC_REPORT_COUNT:
        Dcm_HandleReadDtcCount(uds, udsLen);
        break;
    case DCM_DTC_SUBFUNC_REPORT_BY_MASK:
        Dcm_HandleReadDtcByMask(uds, udsLen);
        break;
    default:
        Dcm_SendNegativeResponse(DCM_SID_READ_DTC_INFO, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
        break;
    }
}

/* -----------------------------------------------------------------------
 * 0x22 ReadDataByIdentifier — DID ディスパッチ
 * ----------------------------------------------------------------------- */

/**
 * \brief   DID に対応するデータを読み出す。
 *
 * \param[in]   did      データ識別子 (DCM_DID_*)。
 * \param[out]  buf      読み出したデータの格納先。
 * \param[out]  dataLen  読み出したデータのバイト数。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  DID 不明またはデータ取得失敗。
 */
static Std_ReturnType Dcm_ReadDid(uint16 did, uint8* buf, uint8* dataLen)
{
    switch (did)
    {
    case DCM_DID_ENGINE_SPEED:
    {
        EngineSpeed_t speed = 0U;
        Rte_Read_SpeedSensor_EngineSpeed(&speed);
        buf[0]   = (uint8)(speed >> 8U);   /* MSB first (big-endian) */
        buf[1]   = (uint8)(speed & 0xFFU);
        *dataLen = 2U;
        return E_OK;
    }
    case DCM_DID_COOLANT_TEMP:
    {
        CoolantTemp_t temp = 0U;
        Rte_Read_TempSensor_CoolantTemp(&temp);
        buf[0]   = temp;
        *dataLen = 1U;
        return E_OK;
    }
    case DCM_DID_ENGINE_STATE:
    {
        EngineState_t state = ENGINE_STATE_OFF;
        Rte_Read_EngineStatus_EngineState(&state);
        buf[0]   = (uint8)state;
        *dataLen = 1U;
        return E_OK;
    }
    default:
        return E_NOT_OK;
    }
}

/**
 * \brief   UDS 0x22 ReadDataByIdentifier を処理する。
 *
 * \details 要求フレームから DID を抽出し、対応するデータを読み出して
 *          正応答 [PCI, 0x62, DID_H, DID_L, data...] を返す。
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleReadDataById(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 3U)
    {
        Dcm_SendNegativeResponse(DCM_SID_READ_DATA, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint16 did = ((uint16)uds[1] << 8U) | (uint16)uds[2];

    uint8 dataBuf[4];
    uint8 dataLen = 0U;

    if (Dcm_ReadDid(did, dataBuf, &dataLen) != E_OK)
    {
        Dcm_SendNegativeResponse(DCM_SID_READ_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    Det_PrintP(PSTR("[Dcm 0x22] DID=0x"));
    Det_PrintHex(did);
    Det_PrintP(PSTR(" len="));
    Det_PrintDec(dataLen);
    Det_Newline();

    /* 正応答: [PCI, 0x62, DID_H, DID_L, data...] */
    uint8 udsRspLen = (uint8)(3U + dataLen);   /* 0x62(1) + DID(2) + data */
    Dcm_TxBuf[0] = udsRspLen;                  /* PCI: SF, length */
    Dcm_TxBuf[1] = 0x62U;                      /* SID + 0x40 */
    Dcm_TxBuf[2] = (uint8)(did >> 8U);
    Dcm_TxBuf[3] = (uint8)(did & 0xFFU);

    uint8 i;
    for (i = 0U; i < dataLen; i++)
        Dcm_TxBuf[4U + i] = dataBuf[i];
    for (; 4U + i < 8U; i++)
        Dcm_TxBuf[4U + i] = 0x00U;             /* padding */

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * 0x3E TesterPresent
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x3E TesterPresent を処理する。
 *
 * \details セッションタイムアウトをリセットし (本実装ではタイムアウト未実装)、
 *          正応答 [0x02, 0x7E, subFunc] を返す。
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleTesterPresent(const uint8* uds, uint8 udsLen)
{
    uint8 subFunc = (udsLen >= 2U) ? (uds[1] & 0x7FU) : 0x00U;

    Det_LogP(PSTR("[Dcm 0x3E] TesterPresent"));

    /* 正応答: [0x02, 0x7E, subFunc] */
    Dcm_TxBuf[0] = 0x02U;
    Dcm_TxBuf[1] = 0x7EU;               /* SID + 0x40 */
    Dcm_TxBuf[2] = subFunc;
    Dcm_TxBuf[3] = 0x00U;
    Dcm_TxBuf[4] = 0x00U;
    Dcm_TxBuf[5] = 0x00U;
    Dcm_TxBuf[6] = 0x00U;
    Dcm_TxBuf[7] = 0x00U;

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * Dcm_ComIndication — PduR から呼び出されるエントリポイント
 * ----------------------------------------------------------------------- */

/**
 * \brief   PduR から配信された UDS フレームを処理する。
 *
 * \details ISO 15765-2 の PCI バイトを解析し、Single Frame (SF) であれば
 *          UDS サービス ID に応じてハンドラへディスパッチする。
 *          マルチフレームは本実装の範囲外のためスキップする。
 *
 * \param[in]  RxPduId     受信 PDU ID（未使用; 本実装では DCM 専用パス固定）。
 * \param[in]  PduInfoPtr  受信データへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_ComIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)RxPduId;

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL
        || PduInfoPtr->SduLength == 0U)
        return;

    /* --- ISO 15765-2 PCI 解析 --- */
    uint8 pci = PduInfoPtr->SduDataPtr[0];

    if ((pci & DCM_ISOTP_SF_PCI_MASK) != DCM_ISOTP_SF_PCI_TYPE)
    {
        /* マルチフレーム (FF/CF/FC) は未対応 */
        Det_LogP(PSTR("[Dcm] Non-SF: not supported"));
        return;
    }

    uint8 udsLen = pci & 0x0FU;   /* SF: 下位ニブルが UDS ペイロード長 */

    if (udsLen == 0U || udsLen > DCM_ISOTP_SF_MAX_LEN
        || PduInfoPtr->SduLength < (PduLengthType)(1U + udsLen))
        return;

    /* UDS ペイロード = PCI バイトの直後 */
    const uint8* uds = &PduInfoPtr->SduDataPtr[1];
    uint8 sid = uds[0];

    Det_PrintP(PSTR("[Dcm] SID=0x"));
    Det_PrintHex(sid);
    Det_Newline();

    /* --- UDS サービスディスパッチ --- */
    switch (sid)
    {
    case DCM_SID_SESSION_CTRL:
        Dcm_HandleSessionControl(uds, udsLen);
        break;
    case DCM_SID_ECU_RESET:
        Dcm_HandleEcuReset(uds, udsLen);
        break;
    case DCM_SID_CLEAR_DTC:
        Dcm_HandleClearDtc(uds, udsLen);
        break;
    case DCM_SID_READ_DTC_INFO:
        Dcm_HandleReadDtcInfo(uds, udsLen);
        break;
    case DCM_SID_READ_DATA:
        Dcm_HandleReadDataById(uds, udsLen);
        break;
    case DCM_SID_TESTER_PRESENT:
        Dcm_HandleTesterPresent(uds, udsLen);
        break;
    default:
        Dcm_SendNegativeResponse(sid, DCM_NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

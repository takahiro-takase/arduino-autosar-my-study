/**
 * \file    Dcm_Cbk.c
 * \brief   DCM モジュール実装 (AUTOSAR SWS_DCM 準拠, UDS ISO 14229-1)
 * \details CanTp から配信された UDS 診断ペイロードを解析し、
 *          以下の UDS サービスを処理して CAN 0x7E8 で応答する。
 *
 *          対応サービス:
 *            0x10 DiagnosticSessionControl — Default / Extended セッション切替
 *            0x11 ECUReset               — hardReset / softReset (応答後セッションリセット)
 *            0x14 ClearDiagnosticInformation — 全 DTC クリア／DTC 指定で 1 件クリア
 *            0x19 ReadDTCInformation     — subFunc 0x01/0x02/0x04
 *            0x22 ReadDataByIdentifier   — DID 0x0101/0x0102/0x0103
 *            0x3E TesterPresent          — セッション維持 (S3 タイマリセット)
 *
 *          ISO 15765-2 (CAN TP) は CanTp モジュールが担当する。
 *          本モジュールは UDS ペイロード (PCI バイトなし) のみを扱う。
 *          マルチフレーム対応により SID 0x19/02 は複数 DTC を一度に返せる。
 *
 *          S3 タイマ:
 *            defaultSession 以外の間、Dcm_ComIndication() が呼ばれるたびに
 *            最終活動時刻を更新する (TesterPresent に限らず全サービスが対象)。
 *            Dcm_MainFunction() が周期的に経過時間を確認し、
 *            DCM_S3_TIMEOUT_MS 以上要求が来なければ defaultSession へ復帰する。
 *
 *          応答送信フロー:
 *            Dcm → CanTp_Transmit(CANTP_TX_SDU_ID) → PduR_Transmit → CanIf
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
#include "CanTp.h"
#include "Rte.h"
#include "Det.h"

/* millis() is declared in Arduino wiring.c with C linkage. */
extern unsigned long millis(void);

#define TAG "Dcm"

/* -----------------------------------------------------------------------
 * モジュール内部状態
 * ----------------------------------------------------------------------- */

/** 現在の診断セッション (DCM_SESSION_DEFAULT / DCM_SESSION_EXTENDED) */
static uint8 Dcm_CurrentSession;

/** 最後に診断要求を受信した時刻 (S3 タイマの基準点) */
static unsigned long Dcm_LastActivityMs;

/** UDS 応答バッファ (PCI バイトなし; CanTp がトランスポート層を付加する)
 *  最大サイズ: 0x19/02 応答 (6 DTC) = 3 + 6×4 = 27 バイト → 32 バイトで余裕を持たせる */
static uint8 Dcm_TxBuf[32];

/** CanTp_Transmit に渡す PDU 情報構造体 */
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
static void Dcm_HandleReadDtcSnapshot(const uint8* uds, uint8 udsLen);
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
    Dcm_TxPdu.SduLength  = 0U;   /* 各ハンドラで送信長を設定する */
    Dcm_LastActivityMs   = millis();
    DET_LOGI(TAG, "Init ok");
}

/**
 * \brief   DCM 周期処理。S3 タイマ (セッションタイムアウト) を監視する。
 *
 * \details defaultSession 中は何もしない。それ以外の間、最後に診断要求を
 *          受信してから DCM_S3_TIMEOUT_MS 以上経過していれば
 *          defaultSession へ復帰させる (ISO 14229-1 の S3 タイマに相当)。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_MainFunction(void)
{
    if (Dcm_CurrentSession == DCM_SESSION_DEFAULT)
        return;

    if ((millis() - Dcm_LastActivityMs) >= DCM_S3_TIMEOUT_MS)
    {
        DET_LOGI(TAG, "S3 timeout -> session=Default");
        Dcm_CurrentSession = DCM_SESSION_DEFAULT;
    }
}

/* -----------------------------------------------------------------------
 * 応答送信ヘルパー
 * ----------------------------------------------------------------------- */

/**
 * \brief   Dcm_TxBuf の内容を CanTp 経由で CAN 0x7E8 に送信する。
 *
 * \details CanTp がペイロード長に応じて SF/FF+CF を選択する。
 *          呼び出し前に Dcm_TxPdu.SduLength を設定すること。
 */
static void Dcm_Transmit(void)
{
    CanTp_Transmit(CANTP_TX_SDU_ID, &Dcm_TxPdu);
}

/**
 * \brief   UDS 否定応答 (NRC) フレームを送信する。
 *
 * \details ISO 14229-1 に従い [0x7F, SID, NRC] を CanTp へ渡す。
 *
 * \param[in]  sid  失敗したサービス ID。
 * \param[in]  nrc  否定応答コード (DCM_NRC_*)。
 */
static void Dcm_SendNegativeResponse(uint8 sid, uint8 nrc)
{
    Dcm_TxBuf[0] = DCM_SID_NEGATIVE_RESP;
    Dcm_TxBuf[1] = sid;
    Dcm_TxBuf[2] = nrc;
    Dcm_TxPdu.SduLength = 3U;

    DET_LOGE(TAG, "NRC sid=0x%02X nrc=0x%02X", (unsigned)sid, (unsigned)nrc);

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

    DET_LOGI(TAG, "10 session=0x%02X", (unsigned)subFunc);

    /* 正応答: [0x50, subFunc, P2_H, P2_L, P2X_H, P2X_L] */
    Dcm_TxBuf[0] = 0x50U;               /* SID + 0x40 */
    Dcm_TxBuf[1] = subFunc;
    Dcm_TxBuf[2] = DCM_SESSION_P2_HIGH;
    Dcm_TxBuf[3] = DCM_SESSION_P2_LOW;
    Dcm_TxBuf[4] = DCM_SESSION_P2X_HIGH;
    Dcm_TxBuf[5] = DCM_SESSION_P2X_LOW;
    Dcm_TxPdu.SduLength = 6U;

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

    DET_LOGI(TAG, "11 sub=0x%02X", (unsigned)subFunc);

    /* 正応答: [0x51, subFunc] */
    Dcm_TxBuf[0] = 0x51U;               /* SID + 0x40 */
    Dcm_TxBuf[1] = subFunc;
    Dcm_TxPdu.SduLength = 2U;

    Dcm_Transmit();

    /* リセット後処理: セッションをデフォルトに戻す */
    Dcm_CurrentSession = DCM_SESSION_DEFAULT;
    DET_LOGI(TAG, "11 session->Default");
}

/* -----------------------------------------------------------------------
 * 0x14 ClearDiagnosticInformation
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x14 ClearDiagnosticInformation を処理する。
 *
 * \details groupOfDTC=0xFFFFFF なら Dem_ClearAllDTCs() で全 DTC をクリアする。
 *          それ以外の値は特定の DTC コードとみなし、Dem_GetEventIdOfDTC() で
 *          一致するイベントを探して Dem_ClearDTC() で 1 件だけクリアする
 *          （該当イベントがなければ NRC 0x31）。
 *          いずれの場合も正応答 [0x54] を返す。
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

    if (group == DEM_GROUP_ALL_DTCS)
    {
        DET_LOGI(TAG, "14 ClearAllDTC");
        Dem_ClearAllDTCs();
    }
    else
    {
        Dem_EventIdType eventId = 0U;

        if (Dem_GetEventIdOfDTC(group, &eventId) != E_OK)
        {
            /* 指定された DTC コードに一致するイベントがない */
            Dcm_SendNegativeResponse(DCM_SID_CLEAR_DTC, DCM_NRC_REQUEST_OUT_OF_RANGE);
            return;
        }

        DET_LOGI(TAG, "14 ClearDTC dtc=0x%06lX", (unsigned long)group);
        Dem_ClearDTC(eventId);
    }

    /* 正応答: [0x54] */
    Dcm_TxBuf[0] = 0x54U;               /* SID 0x14 + 0x40 */
    Dcm_TxPdu.SduLength = 1U;

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * 0x19 ReadDTCInformation — サブ機能ディスパッチ
 * ----------------------------------------------------------------------- */

/**
 * \brief   SID 0x19 subFunc 0x01 reportNumberOfDTCByStatusMask を処理する。
 *
 * \details statusMask に一致する DTC の件数を返す。
 *          応答: [0x59, 0x01, statusAvailMask, dtcFormat, countH, countL]
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

    DET_LOGI(TAG, "19/01 mask=0x%02X cnt=%u", (unsigned)statusMask, (unsigned)count);

    /* 正応答: [0x59, 0x01, statusAvailMask, dtcFormat, countH, countL] */
    Dcm_TxBuf[0] = 0x59U;                        /* SID 0x19 + 0x40 */
    Dcm_TxBuf[1] = DCM_DTC_SUBFUNC_REPORT_COUNT;
    Dcm_TxBuf[2] = DEM_STATUS_AVAILABILITY_MASK;
    Dcm_TxBuf[3] = DCM_DTC_FORMAT_ISO15031;
    Dcm_TxBuf[4] = 0x00U;                        /* countH */
    Dcm_TxBuf[5] = count;                        /* countL */
    Dcm_TxPdu.SduLength = 6U;

    Dcm_Transmit();
}

/**
 * \brief   SID 0x19 subFunc 0x02 reportDTCByStatusMask を処理する。
 *
 * \details statusMask に一致する DTC をすべて返す。
 *          CanTp がマルチフレームを担当するため、SF の 7 バイト制限がなくなった。
 *          応答 (0 件): [0x59, 0x02, statusAvailMask]
 *          応答 (n 件): [0x59, 0x02, statusAvailMask,
 *                        DTC1_H, DTC1_M, DTC1_L, S1,
 *                        DTC2_H, DTC2_M, DTC2_L, S2, ...]
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

    DET_LOGI(TAG, "19/02 mask=0x%02X found=%u", (unsigned)statusMask, (unsigned)count);

    Dcm_TxBuf[0] = 0x59U;
    Dcm_TxBuf[1] = DCM_DTC_SUBFUNC_REPORT_BY_MASK;
    Dcm_TxBuf[2] = DEM_STATUS_AVAILABILITY_MASK;

    uint8 offset = 3U;
    uint8 i;
    for (i = 0U; i < count; i++)
    {
        Dcm_TxBuf[offset++] = (uint8)(dtcBuf[i] >> 16U);   /* DTC 上位バイト */
        Dcm_TxBuf[offset++] = (uint8)(dtcBuf[i] >>  8U);   /* DTC 中位バイト */
        Dcm_TxBuf[offset++] = (uint8)(dtcBuf[i]);           /* DTC 下位バイト */
        Dcm_TxBuf[offset++] = statusBuf[i];                 /* DTC ステータス */
    }
    Dcm_TxPdu.SduLength = (PduLengthType)offset;

    Dcm_Transmit();
}

/**
 * \brief   SID 0x19 subFunc 0x04 reportDTCSnapshotRecordByDTCNumber を処理する。
 *
 * \details 要求された DTC に一致する FreezeFrame を Dem から取得し、
 *          EngineSpeed (DID 0x0101) / CoolantTemp (0x0102) / EngineState (0x0103)
 *          の 3 DID 固定フォーマットで返す。
 *          本実装はレコード番号 0x01 のみ対応する（イベントごとに 1 スナップショット）。
 *          応答は 18 バイトで SF の 7 バイト制限を超えるため、CanTp が FF+CF に分割する。
 *
 *          要求: [0x19, 0x04, DTC_H, DTC_M, DTC_L, recordNumber]
 *          応答: [0x59, 0x04, DTC_H, DTC_M, DTC_L, status, recordNumber, numDID,
 *                 DID1_H, DID1_L, EngineSpeed_H, EngineSpeed_L,
 *                 DID2_H, DID2_L, CoolantTemp,
 *                 DID3_H, DID3_L, EngineState]
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleReadDtcSnapshot(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 6U)
    {
        Dcm_SendNegativeResponse(DCM_SID_READ_DTC_INFO, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint32 dtc          = ((uint32)uds[2] << 16U) | ((uint32)uds[3] << 8U) | (uint32)uds[4];
    uint8  recordNumber = uds[5];

    Dem_EventIdType     eventId = 0U;
    Dem_FreezeFrameType frame;

    if (Dem_GetEventIdOfDTC(dtc, &eventId) != E_OK
        || recordNumber != DCM_FREEZEFRAME_RECORD_NUMBER
        || Dem_GetFreezeFrameOfEvent(eventId, &frame) != E_OK)
    {
        /* DTC 不明・レコード番号不一致・FreezeFrame 未記録 (一度も FAILED していない) */
        Dcm_SendNegativeResponse(DCM_SID_READ_DTC_INFO, DCM_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    DET_LOGI(TAG, "19/04 dtc=0x%06lX rec=%u", (unsigned long)dtc, (unsigned)recordNumber);

    Dcm_TxBuf[0]  = 0x59U;                              /* SID 0x19 + 0x40 */
    Dcm_TxBuf[1]  = DCM_DTC_SUBFUNC_REPORT_SNAPSHOT;
    Dcm_TxBuf[2]  = (uint8)(dtc >> 16U);
    Dcm_TxBuf[3]  = (uint8)(dtc >>  8U);
    Dcm_TxBuf[4]  = (uint8)(dtc);
    Dcm_TxBuf[5]  = Dem_GetStatusOfEvent(eventId);
    Dcm_TxBuf[6]  = recordNumber;
    Dcm_TxBuf[7]  = DCM_FREEZEFRAME_DID_COUNT;
    Dcm_TxBuf[8]  = (uint8)(DCM_DID_ENGINE_SPEED >> 8U);
    Dcm_TxBuf[9]  = (uint8)(DCM_DID_ENGINE_SPEED & 0xFFU);
    Dcm_TxBuf[10] = (uint8)(frame.EngineSpeed >> 8U);
    Dcm_TxBuf[11] = (uint8)(frame.EngineSpeed & 0xFFU);
    Dcm_TxBuf[12] = (uint8)(DCM_DID_COOLANT_TEMP >> 8U);
    Dcm_TxBuf[13] = (uint8)(DCM_DID_COOLANT_TEMP & 0xFFU);
    Dcm_TxBuf[14] = frame.CoolantTemp;
    Dcm_TxBuf[15] = (uint8)(DCM_DID_ENGINE_STATE >> 8U);
    Dcm_TxBuf[16] = (uint8)(DCM_DID_ENGINE_STATE & 0xFFU);
    Dcm_TxBuf[17] = frame.EngineState;
    Dcm_TxPdu.SduLength = 18U;

    Dcm_Transmit();
}

/**
 * \brief   UDS 0x19 ReadDTCInformation のサブ機能をディスパッチする。
 *
 * \details 対応サブ機能:
 *            0x01 reportNumberOfDTCByStatusMask         → Dcm_HandleReadDtcCount()
 *            0x02 reportDTCByStatusMask                 → Dcm_HandleReadDtcByMask()
 *            0x04 reportDTCSnapshotRecordByDTCNumber    → Dcm_HandleReadDtcSnapshot()
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
    case DCM_DTC_SUBFUNC_REPORT_SNAPSHOT:
        Dcm_HandleReadDtcSnapshot(uds, udsLen);
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
 *          正応答 [0x62, DID_H, DID_L, data...] を返す。
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

    DET_LOGI(TAG, "22 did=0x%04X len=%u", (unsigned)did, (unsigned)dataLen);

    /* 正応答: [0x62, DID_H, DID_L, data...] */
    Dcm_TxBuf[0] = 0x62U;                      /* SID + 0x40 */
    Dcm_TxBuf[1] = (uint8)(did >> 8U);
    Dcm_TxBuf[2] = (uint8)(did & 0xFFU);

    uint8 i;
    for (i = 0U; i < dataLen; i++)
        Dcm_TxBuf[3U + i] = dataBuf[i];

    Dcm_TxPdu.SduLength = (PduLengthType)(3U + dataLen);

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * 0x3E TesterPresent
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x3E TesterPresent を処理する。
 *
 * \details セッションタイムアウトをリセットし (本実装ではタイムアウト未実装)、
 *          正応答 [0x7E, subFunc] を返す。
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleTesterPresent(const uint8* uds, uint8 udsLen)
{
    uint8 subFunc = (udsLen >= 2U) ? (uds[1] & 0x7FU) : 0x00U;

    DET_LOGI(TAG, "3E TesterPresent");

    /* 正応答: [0x7E, subFunc] */
    Dcm_TxBuf[0] = 0x7EU;               /* SID + 0x40 */
    Dcm_TxBuf[1] = subFunc;
    Dcm_TxPdu.SduLength = 2U;

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * Dcm_ComIndication — CanTp から呼び出されるエントリポイント
 * ----------------------------------------------------------------------- */

/**
 * \brief   CanTp から配信された UDS ペイロードを処理する。
 *
 * \details CanTp が ISO 15765-2 トランスポート層を処理済みであるため、
 *          PduInfoPtr には PCI バイトを含まない生 UDS ペイロードが格納されている。
 *          先頭バイト (uds[0]) が UDS サービス ID (SID) となる。
 *
 * \param[in]  RxPduId     受信 PDU ID（未使用; CanTp からの単一チャネル固定）。
 * \param[in]  PduInfoPtr  組立済み UDS ペイロードへのポインタ。NULL 禁止。
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

    /* CanTp が PCI を除去済み: 先頭バイトは UDS SID */
    const uint8* uds    = PduInfoPtr->SduDataPtr;
    uint8        udsLen = (uint8)PduInfoPtr->SduLength;
    uint8        sid    = uds[0];

    /* 診断要求を受信した時点で S3 タイマをリセットする（NRC になる要求も対象） */
    Dcm_LastActivityMs = millis();

    DET_LOGI(TAG, "req SID=0x%02X", (unsigned)sid);

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

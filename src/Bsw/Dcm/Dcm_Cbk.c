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
 *                                              (SecurityAccess Level1 必須)
 *            0x19 ReadDTCInformation     — subFunc 0x01/0x02/0x04
 *            0x22 ReadDataByIdentifier   — DID 0x0101/0x0102/0x0103/0x0104
 *            0x27 SecurityAccess         — subFunc 0x01 requestSeed / 0x02 sendKey
 *            0x2E WriteDataByIdentifier  — DID 0x0104 (TestPattern) のみ。
 *                                          SecurityAccess Level1 必須
 *            0x3E TesterPresent          — セッション維持 (S3 タイマリセット)
 *
 *          ISO 15765-2 (CAN TP) は CanTp モジュールが担当する。
 *          本モジュールは UDS ペイロード (PCI バイトなし) のみを扱う。
 *          マルチフレーム対応により SID 0x19/02 は複数 DTC を一度に返せる
 *          (応答方向)。0x2E は要求が 11 バイトとなり SF 上限を超えるため、
 *          CanTp の複数フレーム要求受信 (FF+CF) を実機検証する唯一の SID。
 *
 *          S3 タイマ:
 *            defaultSession 以外の間、Dcm_ComIndication() が呼ばれるたびに
 *            最終活動時刻を更新する (TesterPresent に限らず全サービスが対象)。
 *            Dcm_MainFunction() が周期的に経過時間を確認し、
 *            DCM_S3_TIMEOUT_MS 以上要求が来なければ defaultSession へ復帰する。
 *
 *          SecurityAccess (Level1) 状態機械:
 *            1. requestSeed (27/01): ロック中なら seed を生成して応答し、
 *               「seed 発行済み・key 未受信」状態にする。アンロック済みなら
 *               ISO 14229-1 の作法通り allZeroSeed (seed=0x0000) を返す。
 *            2. sendKey (27/02): 直前に発行した seed から期待される key
 *               (seed XOR DCM_SECURITY_KEY_MASK) と比較する。
 *               一致 → Level1 アンロック。不一致 → NRC 0x35、
 *               DCM_SECURITY_MAX_ATTEMPTS 回連続失敗で NRC 0x36 を返し
 *               DCM_SECURITY_DELAY_MS の間 requestSeed 自体を NRC 0x37 で拒否する
 *               (ブルートフォース対策、ISO 14229-1 準拠)。
 *            3. defaultSession へ遷移（明示要求または S3 タイムアウト）すると
 *               Level1 は再ロックされる。失敗回数・待機時間はセッションをまたいで
 *               維持する（再ロックでブルートフォース対策を回避できないようにする）。
 *            4. ClearDiagnosticInformation (0x14) は Level1 アンロック済みでなければ
 *               NRC 0x33 securityAccessDenied を返す。
 *
 *          SID × セッション許可 (Dcm_SidSessionTable[], AUTOSAR DcmDspSessionRow 相当):
 *            Dcm_ComIndication() が SID ディスパッチの前に全 SID 共通で判定する。
 *            テーブルに掲載のない SID はセッション制約なし。現在は 0x14・0x27・0x2E
 *            のみ extendedSession 限定とし、defaultSession では NRC 0x7F
 *            serviceNotSupportedInActiveSession で拒否する。各ハンドラ個別に
 *            セッション判定を埋め込むのではなく、一元管理することで
 *            「どの SID がどのセッションで使えるか」を一覧できるようにしている。
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

/** SecurityAccess Level1 状態 (0=Locked, 1=Unlocked) */
static uint8 Dcm_SecurityLevel;

/** requestSeed 発行済みで対応する sendKey をまだ受信していないか (1=待機中) */
static uint8 Dcm_SecuritySeedPending;

/** 直前に発行した seed 値 (sendKey 受信時の期待 key 計算に使用) */
static uint16 Dcm_SecuritySeed;

/** sendKey の連続失敗回数 (DCM_SECURITY_MAX_ATTEMPTS でロックアウト) */
static uint8 Dcm_SecurityAttemptCount;

/** ロックアウト中か (1=ロックアウト中)。(millis() - Dcm_SecurityLockoutStartMs)
 *  が DCM_SECURITY_DELAY_MS 以上経過するまで requestSeed を NRC 0x37 で拒否する。
 *  millis() オーバフロー (約49.7日) でも正しく動作する差分計算にするため、
 *  絶対時刻ではなく開始時刻+フラグで持つ。 */
static uint8 Dcm_SecurityLockoutActive;

/** ロックアウト開始時刻 [ms] */
static unsigned long Dcm_SecurityLockoutStartMs;

/** UDS 応答バッファの最大サイズ。0x19/02 (reportDTCByStatusMask) が
 *  DEM_EVENT_COUNT 件全てに一致した場合が最大: [0x59,subFunc,availMask] (3)
 *  + DEM_EVENT_COUNT 件 × 4 バイト。
 *  以前は固定値 32 で確保していたが、DEM_EVENT_COUNT が 6→8 に増えた際に
 *  追従しておらず 3+8×4=35 バイトが 32 バイトのバッファに収まらず
 *  オーバーフローしていた。DEM_EVENT_COUNT の変化に自動追従する数式に変更。 */
#define DCM_TX_BUF_SIZE  (3U + (DEM_EVENT_COUNT * 4U))

/** UDS 応答バッファ (PCI バイトなし; CanTp がトランスポート層を付加する) */
static uint8 Dcm_TxBuf[DCM_TX_BUF_SIZE];

/** DID 0x0104 (TestPattern) の格納領域。CanTp の複数フレーム要求受信を
 *  検証するための学習用データ（実際の車両データではない）。 */
static uint8 Dcm_TestPattern[DCM_DID_TEST_PATTERN_LENGTH];

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
static void Dcm_HandleWriteDataById(const uint8* uds, uint8 udsLen);
static void Dcm_HandleSecurityAccess(const uint8* uds, uint8 udsLen);
static void Dcm_HandleSecurityRequestSeed(uint8 subFunc);
static void Dcm_HandleSecuritySendKey(uint8 subFunc, const uint8* uds, uint8 udsLen);
static uint16 Dcm_ComputeSecurityKey(uint16 seed);
static void Dcm_SecurityLock(void);
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

    Dcm_SecurityLevel         = 0U;
    Dcm_SecuritySeedPending   = 0U;
    Dcm_SecurityAttemptCount  = 0U;
    Dcm_SecurityLockoutActive = 0U;

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
        Dcm_SecurityLock();
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

    if (subFunc == DCM_SESSION_DEFAULT)
        Dcm_SecurityLock();

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
 *          DTC 履歴の消去は誤操作・悪用の影響が大きいため二重に保護する:
 *            ・extendedSession 限定（Dcm_ComIndication の Dcm_SidSessionTable[] が判定、
 *              defaultSession では本関数に到達する前に NRC 0x7F で拒否される）
 *            ・SecurityAccess Level1 アンロック必須（本関数内で判定、未アンロックは NRC 0x33）
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ (uds[0]=SID 0x14)。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleClearDtc(const uint8* uds, uint8 udsLen)
{
    if (Dcm_SecurityLevel == 0U)
    {
        Dcm_SendNegativeResponse(DCM_SID_CLEAR_DTC, DCM_NRC_SECURITY_ACCESS_DENIED);
        return;
    }

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
        if ((offset + 4U) > DCM_TX_BUF_SIZE)
        {
            /* DEM_EVENT_COUNT に対して DCM_TX_BUF_SIZE を正しく計算しているため
             * 通常は到達しないが、将来の設定変更で再びサイズ計算がずれた場合に
             * メモリ破壊ではなく安全な切り詰めで応答するための防御的チェック。 */
            DET_LOGE(TAG, "19/02 TxBuf full, truncating at %u/%u DTCs",
                     (unsigned)i, (unsigned)count);
            break;
        }
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
    case DCM_DID_TEST_PATTERN:
    {
        uint8 i;
        for (i = 0U; i < DCM_DID_TEST_PATTERN_LENGTH; i++)
            buf[i] = Dcm_TestPattern[i];
        *dataLen = DCM_DID_TEST_PATTERN_LENGTH;
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

    uint8 dataBuf[DCM_DID_TEST_PATTERN_LENGTH];
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
 * 0x2E WriteDataByIdentifier
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x2E WriteDataByIdentifier を処理する。
 *
 * \details DCM_DID_TEST_PATTERN (0x0104, 固定長 DCM_DID_TEST_PATTERN_LENGTH
 *          バイト) のみ書き込みに対応する。要求ペイロードは
 *          SID(1)+DID(2)+data(8)=11 バイトとなり CanTp の SF 上限 (7 バイト)
 *          を超えるため、CanTp_RxIndication() の FF+CF 受信パスが実際に
 *          動作する（これまで一度も実機を通っていなかったコードパスを
 *          検証する目的の学習用 DID。実際の車両データではない）。
 *          0x14 ClearDiagnosticInformation と同じ方針で、extendedSession
 *          （Dcm_SidSessionTable[] で判定）かつ SecurityAccess Level1
 *          アンロック済みでなければ NRC 0x33 を返す。
 *
 *          要求: [0x2E, DID_H, DID_L, data0..data7]
 *          応答: [0x6E, DID_H, DID_L]
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ (uds[0]=SID 0x2E)。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleWriteDataById(const uint8* uds, uint8 udsLen)
{
    if (Dcm_SecurityLevel == 0U)
    {
        Dcm_SendNegativeResponse(DCM_SID_WRITE_DATA, DCM_NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    if (udsLen < 3U)
    {
        Dcm_SendNegativeResponse(DCM_SID_WRITE_DATA, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint16 did = ((uint16)uds[1] << 8U) | (uint16)uds[2];

    if (did != DCM_DID_TEST_PATTERN)
    {
        Dcm_SendNegativeResponse(DCM_SID_WRITE_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    if (udsLen != (uint8)(3U + DCM_DID_TEST_PATTERN_LENGTH))
    {
        Dcm_SendNegativeResponse(DCM_SID_WRITE_DATA, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8 i;
    for (i = 0U; i < DCM_DID_TEST_PATTERN_LENGTH; i++)
        Dcm_TestPattern[i] = uds[3U + i];

    DET_LOGI(TAG, "2E did=0x%04X len=%u (multi-frame request)",
             (unsigned)did, (unsigned)DCM_DID_TEST_PATTERN_LENGTH);

    /* 正応答: [0x6E, DID_H, DID_L] */
    Dcm_TxBuf[0] = 0x6EU;               /* SID + 0x40 */
    Dcm_TxBuf[1] = uds[1];
    Dcm_TxBuf[2] = uds[2];
    Dcm_TxPdu.SduLength = 3U;

    Dcm_Transmit();
}

/* -----------------------------------------------------------------------
 * 0x27 SecurityAccess
 * ----------------------------------------------------------------------- */

/**
 * \brief   seed から期待される key を計算する。
 *
 * \details 学習用の単純な固定 XOR マスクによる変換。
 *          実際の量産 ECU は暗号学的アルゴリズムや OEM 固有の非公開アルゴリズムを
 *          用いるため、本実装をそのまま実運用に転用しないこと。
 *
 * \param[in]  seed  requestSeed で発行した seed 値。
 * \return     対応する期待 key 値。
 */
static uint16 Dcm_ComputeSecurityKey(uint16 seed)
{
    return (uint16)(seed ^ DCM_SECURITY_KEY_MASK);
}

/**
 * \brief   SecurityAccess Level1 を再ロックする。
 *
 * \details defaultSession への遷移（明示要求・S3 タイムアウトいずれも）で呼ぶ。
 *          連続失敗回数・ロックアウト解除時刻はあえてリセットしない
 *          （セッション往復によるブルートフォース対策の回避を防ぐため）。
 */
static void Dcm_SecurityLock(void)
{
    if (Dcm_SecurityLevel != 0U)
        DET_LOGI(TAG, "27 Security locked (session->Default)");

    Dcm_SecurityLevel       = 0U;
    Dcm_SecuritySeedPending = 0U;
}

/**
 * \brief   SID 0x27 subFunc 0x01 requestSeed を処理する。
 *
 * \details Locked 中はロックアウト中でなければ新しい seed (millis() 由来) を発行し、
 *          「seed 発行済み・key 未受信」状態にする。ロックアウト中は NRC 0x37。
 *          Unlocked 済みの場合は ISO 14229-1 の作法に従い allZeroSeed
 *          (seed=0x0000) を返し、sendKey が不要であることを示す。
 *
 * \param[in]  subFunc  要求されたサブ機能 (0x01、suppressPosRsp ビット除去済み)。
 */
static void Dcm_HandleSecurityRequestSeed(uint8 subFunc)
{
    if (Dcm_SecurityLevel != 0U)
    {
        DET_LOGI(TAG, "27/%02X already unlocked -> allZeroSeed", (unsigned)subFunc);

        Dcm_TxBuf[0] = 0x67U;               /* SID + 0x40 */
        Dcm_TxBuf[1] = subFunc;
        Dcm_TxBuf[2] = 0x00U;
        Dcm_TxBuf[3] = 0x00U;
        Dcm_TxPdu.SduLength = 4U;

        Dcm_SecuritySeedPending = 0U;
        Dcm_Transmit();
        return;
    }

    if (Dcm_SecurityLockoutActive != 0U)
    {
        if ((millis() - Dcm_SecurityLockoutStartMs) < DCM_SECURITY_DELAY_MS)
        {
            Dcm_SendNegativeResponse(DCM_SID_SECURITY_ACCESS, DCM_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
            return;
        }
        Dcm_SecurityLockoutActive = 0U;  /* 待機時間経過、ロックアウト解除 */
    }

    Dcm_SecuritySeed        = (uint16)millis();
    Dcm_SecuritySeedPending = 1U;

    DET_LOGI(TAG, "27/%02X seed=0x%04X", (unsigned)subFunc, (unsigned)Dcm_SecuritySeed);

    /* 正応答: [0x67, subFunc, seedH, seedL] */
    Dcm_TxBuf[0] = 0x67U;
    Dcm_TxBuf[1] = subFunc;
    Dcm_TxBuf[2] = (uint8)(Dcm_SecuritySeed >> 8U);
    Dcm_TxBuf[3] = (uint8)(Dcm_SecuritySeed & 0xFFU);
    Dcm_TxPdu.SduLength = 4U;

    Dcm_Transmit();
}

/**
 * \brief   SID 0x27 subFunc 0x02 sendKey を処理する。
 *
 * \details 直前の requestSeed で発行した seed から期待 key を計算し、
 *          受信した key と比較する。requestSeed なしでの sendKey は
 *          NRC 0x24 requestSequenceError。鍵不一致は NRC 0x35、
 *          DCM_SECURITY_MAX_ATTEMPTS 回連続失敗で NRC 0x36 を返し
 *          DCM_SECURITY_DELAY_MS の間ロックアウトする。
 *          1 つの seed は 1 回の sendKey でのみ有効（再試行には新しい
 *          requestSeed が必要）。
 *
 * \param[in]  subFunc  要求されたサブ機能 (0x02、suppressPosRsp ビット除去済み)。
 * \param[in]  uds      UDS ペイロード先頭ポインタ ([0x27, 0x02, keyH, keyL])。
 * \param[in]  udsLen   UDS ペイロード長。
 */
static void Dcm_HandleSecuritySendKey(uint8 subFunc, const uint8* uds, uint8 udsLen)
{
    if (udsLen < 4U)
    {
        Dcm_SendNegativeResponse(DCM_SID_SECURITY_ACCESS, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    if (Dcm_SecuritySeedPending == 0U)
    {
        Dcm_SendNegativeResponse(DCM_SID_SECURITY_ACCESS, DCM_NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }
    Dcm_SecuritySeedPending = 0U;  /* この seed は 1 回限り; 再試行は requestSeed からやり直す */

    uint16 key      = ((uint16)uds[2] << 8U) | (uint16)uds[3];
    uint16 expected = Dcm_ComputeSecurityKey(Dcm_SecuritySeed);

    if (key != expected)
    {
        Dcm_SecurityAttemptCount++;
        DET_LOGW(TAG, "27/%02X invalid key attempt=%u", (unsigned)subFunc, (unsigned)Dcm_SecurityAttemptCount);

        if (Dcm_SecurityAttemptCount >= DCM_SECURITY_MAX_ATTEMPTS)
        {
            Dcm_SecurityLockoutActive  = 1U;
            Dcm_SecurityLockoutStartMs = millis();
            Dcm_SecurityAttemptCount   = 0U;
            DET_LOGW(TAG, "27 lockout %lums", DCM_SECURITY_DELAY_MS);
            Dcm_SendNegativeResponse(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_NUM_ATTEMPTS);
        }
        else
        {
            Dcm_SendNegativeResponse(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);
        }
        return;
    }

    Dcm_SecurityLevel        = 1U;
    Dcm_SecurityAttemptCount = 0U;
    DET_LOGI(TAG, "27/%02X unlocked", (unsigned)subFunc);

    /* 正応答: [0x67, subFunc] */
    Dcm_TxBuf[0] = 0x67U;
    Dcm_TxBuf[1] = subFunc;
    Dcm_TxPdu.SduLength = 2U;

    Dcm_Transmit();
}

/**
 * \brief   UDS 0x27 SecurityAccess のサブ機能をディスパッチする。
 *
 * \details extendedSession 限定であることは Dcm_ComIndication の
 *          Dcm_SidSessionTable[] チェックで一元的に保証済みのため、
 *          本関数ではセッション判定を行わない。
 *
 * \param[in]  uds     UDS ペイロード先頭ポインタ (uds[0]=SID 0x27)。
 * \param[in]  udsLen  UDS ペイロード長。
 */
static void Dcm_HandleSecurityAccess(const uint8* uds, uint8 udsLen)
{
    if (udsLen < 2U)
    {
        Dcm_SendNegativeResponse(DCM_SID_SECURITY_ACCESS, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    uint8 subFunc = uds[1] & 0x7FU;   /* bit7: suppressPosRsp (本実装では無視) */

    switch (subFunc)
    {
    case DCM_SEC_SUBFUNC_REQUEST_SEED:
        Dcm_HandleSecurityRequestSeed(subFunc);
        break;
    case DCM_SEC_SUBFUNC_SEND_KEY:
        Dcm_HandleSecuritySendKey(subFunc, uds, udsLen);
        break;
    default:
        Dcm_SendNegativeResponse(DCM_SID_SECURITY_ACCESS, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
        break;
    }
}

/* -----------------------------------------------------------------------
 * 0x3E TesterPresent
 * ----------------------------------------------------------------------- */

/**
 * \brief   UDS 0x3E TesterPresent を処理する。
 *
 * \details S3 タイマをリセットする (Dcm_ComIndication で全 SID 共通リセット済みのため
 *          本関数では追加処理不要)。正応答 [0x7E, subFunc] を返す。
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
 * SID × セッション許可テーブル (AUTOSAR DcmDspSessionRow に相当)
 * ----------------------------------------------------------------------- */

/** SID ごとに許可されるセッションを表す 1 行。 */
typedef struct
{
    uint8 Sid;
    uint8 AllowedSessionMask;  /**< DCM_SESSION_MASK_* の組み合わせ */
} Dcm_SidSessionRowType;

/** テーブルに掲載のない SID はセッション制約なし（全セッションで許可）とみなす。
 *  DTC 履歴の消去 (0x14)・セキュリティ認証 (0x27)・データ書き込み (0x2E) のみ、
 *  誤操作・悪用の影響が大きいため extendedSession 限定とする学習用の最小構成。
 *  実際の AUTOSAR では DcmDspSessionRow がサービス・サブ機能単位で
 *  この表をコンフィギュレーションツールから生成する。 */
static const Dcm_SidSessionRowType Dcm_SidSessionTable[] =
{
    { DCM_SID_CLEAR_DTC,       DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_SECURITY_ACCESS, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_WRITE_DATA,      DCM_SESSION_MASK_EXTENDED },
};

/**
 * \brief   SID が現在のセッションで許可されているかを判定する。
 *
 * \details Dcm_SidSessionTable[] を先頭から探索し、一致する行があれば
 *          AllowedSessionMask と現在のセッションのビットを照合する。
 *          一致する行がなければセッション制約なし（常に許可）として扱う。
 *
 * \param[in]  sid      判定対象の UDS サービス ID。
 * \param[in]  session  現在のセッション (DCM_SESSION_DEFAULT / _EXTENDED)。
 *
 * \retval  1  現在のセッションで許可されている。
 * \retval  0  現在のセッションでは許可されない。
 */
static uint8 Dcm_IsServiceAllowedInSession(uint8 sid, uint8 session)
{
    const uint8 sessionMask = (session == DCM_SESSION_EXTENDED)
                               ? DCM_SESSION_MASK_EXTENDED
                               : DCM_SESSION_MASK_DEFAULT;
    const uint8 rowCount = (uint8)(sizeof(Dcm_SidSessionTable) / sizeof(Dcm_SidSessionTable[0]));
    uint8 i;

    for (i = 0U; i < rowCount; i++)
    {
        if (Dcm_SidSessionTable[i].Sid == sid)
        {
            return ((Dcm_SidSessionTable[i].AllowedSessionMask & sessionMask) != 0U) ? 1U : 0U;
        }
    }

    return 1U;  /* テーブル未掲載 = 制約なし */
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

    /* SID × セッション許可チェック (Dcm_SidSessionTable[]) を全 SID 共通で先に行う。
     * 各ハンドラ個別の特例チェックに分散させず、ここで一元的に拒否する。 */
    if (Dcm_IsServiceAllowedInSession(sid, Dcm_CurrentSession) == 0U)
    {
        Dcm_SendNegativeResponse(sid, DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
        return;
    }

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
    case DCM_SID_WRITE_DATA:
        Dcm_HandleWriteDataById(uds, udsLen);
        break;
    case DCM_SID_SECURITY_ACCESS:
        Dcm_HandleSecurityAccess(uds, udsLen);
        break;
    case DCM_SID_TESTER_PRESENT:
        Dcm_HandleTesterPresent(uds, udsLen);
        break;
    default:
        Dcm_SendNegativeResponse(sid, DCM_NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

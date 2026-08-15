/**
 * \file    Nm.c
 * \brief   ネットワークマネジメント実装 (AUTOSAR SWS_CANNM 準拠)
 * \details CanNm 状態機械（[SWS_CanNm_00089] のUML状態図）を実装する。
 *
 *          状態遷移の概要（詳細は各関数のコメント参照）:
 *
 *            Bus-Sleep Mode ──NetworkRequest()/RxIndication(PrepareBusSleep中)──┐
 *                 ↑ WaitBusSleepTime満了                                       │
 *            Prepare Bus-Sleep Mode                                            │
 *                 ↑ NM-Timeout Timer満了(Ready Sleepから)                       ▼
 *            Network Mode: Ready Sleep State ←NetworkRelease()── Normal Operation State
 *                 │                                                    ↑
 *                 └─────────────RepeatMessageTime満了(要求あり)────────┘
 *                                        ↑
 *                              Repeat Message State（新規/再エントリ時は必ずここ）
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Nm.h"
#include "Nm_Cfg.h"
#include "CanIf.h"
#include "CanSM.h"
#include "Det.h"

#define TAG "Nm"

/* Arduino wiring.c（C リンケージ）で定義 */
extern unsigned long millis(void);

static uint8 Nm_Initialized = 0U;

/** 現在の内部状態（Nm_StateType）。 */
static Nm_StateType Nm_State;

/** ComM から見て「通信が必要」かどうか（Nm_NetworkRequest/Release で更新）。 */
static uint8 Nm_NetworkRequested;

/** 診断 CommunicationControl (UDS SID 0x28) からの送信有効/無効状態。既定は有効。 */
static uint8 Nm_TxEnabled = 1U;

/** 送信する NM フレームの CBV Bit0 (Repeat Message Request)。
 *  Nm_RepeatMessageRequest() が呼ばれた場合のみ 1 になり、Repeat Message
 *  State を離れるときに 0 へ戻す（[SWS_CanNm_00107]）。 */
static uint8 Nm_RepeatMessageBitSet;

/** NM-Timeout Timer の起点時刻。Network Mode の3内部状態すべてで使う
 *  （[SWS_CanNm_00096]/[SWS_CanNm_00098]/[SWS_CanNm_00099]/[SWS_CanNm_00109]）。 */
static unsigned long Nm_TimeoutTimerMs;

/** Repeat Message State / Prepare Bus-Sleep Mode の滞在時間タイマ起点。
 *  状態ごとに意味が異なる単一目的タイマ（同時に両方使うことはない）。 */
static unsigned long Nm_StateTimerMs;

static void Nm_TransmitPdu(void);
static void Nm_EnterRepeatMessage(void);
static void Nm_EnterNormalOperation(void);
static void Nm_EnterReadySleep(void);
static void Nm_EnterPrepareBusSleep(void);
static void Nm_EnterBusSleep(void);

void Nm_Init(void)
{
    DET_LOGT(TAG, "called");
    Nm_State               = NM_STATE_BUS_SLEEP;
    Nm_NetworkRequested     = 0U;
    Nm_TxEnabled            = 1U;
    Nm_RepeatMessageBitSet  = 0U;
    Nm_TimeoutTimerMs       = millis();
    Nm_StateTimerMs         = millis();
    Nm_Initialized          = 1U;
    DET_LOGI(TAG, "Init ok node=0x%02X (Bus-Sleep Mode)", (unsigned)NM_SOURCE_NODE_ID);
}

void Nm_DeInit(void)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_DEINIT, NM_E_UNINIT);
        return;
    }

    Nm_Initialized = 0U;
    DET_LOGI(TAG, "DeInit ok");
}

/**
 * \brief   NM フレーム（CBV + Source Node ID）を組み立てて CanIf_Transmit() へ渡す。
 *
 * \details PduR/Com を経由せず CanIf_Transmit() を直接呼び出す（実車の CanNm
 *          と同じ構造）。送信成功時の NM-Timeout Timer 再起動は
 *          Nm_TxConfirmation() 側で行うが、`Can.c` の TX 確認は Can_Write() と
 *          同一スタックフレームでは完了しない非同期設計（TX 確認保留キュー
 *          → 別タスク Can_MainFunction_Write() まで遅延。送信失敗時やキュー
 *          満杯時は確認自体が来ないこともある）。そのため呼び出し元
 *          （Nm_EnterRepeatMessage() 等）は、この呼び出しの完了だけを頼りに
 *          NM-Timeout Timer が再起動された前提を置いてはならない。呼び出し元
 *          自身が状態進入時点で明示的にタイマを起動/再起動すること。
 */
static void Nm_TransmitPdu(void)
{
    DET_LOGT(TAG, "called");
    uint8 pdu[NM_DLC];
    pdu[0] = Nm_RepeatMessageBitSet ? NM_CBV_BIT_REPEAT_MESSAGE_REQUEST : 0x00U;
    pdu[1] = NM_SOURCE_NODE_ID;

    PduInfoType pduInfo = {
        .SduDataPtr = pdu,
        .SduLength  = NM_DLC
    };

    (void)CanIf_Transmit(NM_CANIF_TX_PDU_ID, &pduInfo);
}

/**
 * \brief   Bus-Sleep/Prepare Bus-Sleep Mode から Network Mode (Repeat Message
 *          State) へ入る（[SWS_CanNm_00314]/[SWS_CanNm_00315]）。
 *
 * \details [SWS_CanNm_00096]: Network Mode 進入時に NM-Timeout Timer を起動。
 *          [SWS_CanNm_00100]: 送信有効なら NM フレームの (再)送信を開始する。
 */
static void Nm_EnterRepeatMessage(void)
{
    DET_LOGT(TAG, "called");
    Nm_State          = NM_STATE_REPEAT_MESSAGE;
    Nm_StateTimerMs    = millis();
    Nm_TimeoutTimerMs  = millis();
    DET_LOGI(TAG, "-> Network Mode: Repeat Message State");

    if (Nm_TxEnabled)
        Nm_TransmitPdu();
}

/**
 * \brief   Ready Sleep State から Normal Operation State へ入る（[SWS_CanNm_00116]）。
 *
 * \details Nm_EnterRepeatMessage() と同様、状態進入時点で NM-Timeout Timer を
 *          明示的に再武装する。TX 確認（Nm_TxConfirmation() 経由）に頼ると、
 *          `Can.c` の TX 確認が非同期（別タスクへ遅延、失敗時やキュー満杯時は
 *          確認自体が来ない）であるため、進入直後に送信した PDU の確認が
 *          届かない間タイマが古いまま残り、本来より早く NM-Timeout Timer が
 *          満了したと誤判定してしまう（Nm_TransmitPdu() のコメント参照）。
 */
static void Nm_EnterNormalOperation(void)
{
    DET_LOGT(TAG, "called");
    Nm_State          = NM_STATE_NORMAL_OPERATION;
    Nm_TimeoutTimerMs  = millis();
    DET_LOGI(TAG, "-> Network Mode: Normal Operation State");

    if (Nm_TxEnabled)
        Nm_TransmitPdu();
}

/** Repeat Message/Normal Operation State から Ready Sleep State へ入る
 *  （[SWS_CanNm_00106]/[SWS_CanNm_00118]）。[SWS_CanNm_00108]: 送信を停止する
 *  （以降 Nm_TransmitPdu() を呼ばないだけで実現する）。 */
static void Nm_EnterReadySleep(void)
{
    DET_LOGT(TAG, "called");
    Nm_State = NM_STATE_READY_SLEEP;
    DET_LOGI(TAG, "-> Network Mode: Ready Sleep State (tx stopped)");
}

/** Ready Sleep State から Prepare Bus-Sleep Mode へ入る（[SWS_CanNm_00109]）。 */
static void Nm_EnterPrepareBusSleep(void)
{
    DET_LOGT(TAG, "called");
    Nm_State       = NM_STATE_PREPARE_BUS_SLEEP;
    Nm_StateTimerMs = millis();
    DET_LOGI(TAG, "-> Prepare Bus-Sleep Mode");
}

/**
 * \brief   Prepare Bus-Sleep Mode から Bus-Sleep Mode へ入る（[SWS_CanNm_00115]）。
 *
 * \details [SWS_CanNm_00126]: 上位層（本プロジェクトでは CanSM）への通知
 *          相当として CanSM_NmBusSleepMode() を直接呼ぶ。CanSM はこれを
 *          受けて初めて CAN コントローラを実際にスリープさせる
 *          （協調スリープ。詳細は CanSM.c 参照）。
 */
static void Nm_EnterBusSleep(void)
{
    DET_LOGT(TAG, "called");
    Nm_State = NM_STATE_BUS_SLEEP;
    DET_LOGI(TAG, "-> Bus-Sleep Mode");
    CanSM_NmBusSleepMode();
}

Std_ReturnType Nm_NetworkRequest(void)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_REQUEST, NM_E_UNINIT);
        return E_NOT_OK;
    }

    Nm_NetworkRequested = 1U;

    switch (Nm_State)
    {
        case NM_STATE_BUS_SLEEP:
        case NM_STATE_PREPARE_BUS_SLEEP:
            /* [SWS_CanNm_00123]（Prepare Bus-Sleepから）。本プロジェクトは
             * Bus-Sleepからの能動的ウェイクアップにも同じ扱いを適用する
             * （[SWS_CanNm_00401]のActive Wakeup Bit自体は対応除外）。 */
            Nm_EnterRepeatMessage();
            break;

        case NM_STATE_READY_SLEEP:
            Nm_EnterNormalOperation();  /* [SWS_CanNm_00110] */
            break;

        default:
            /* Repeat Message/Normal Operation State: 既に要求済みのため冪等 */
            break;
    }

    DET_LOGI(TAG, "NetworkRequest ok (state=%u)", (unsigned)Nm_State);
    return E_OK;
}

Std_ReturnType Nm_NetworkRelease(void)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_RELEASE, NM_E_UNINIT);
        return E_NOT_OK;
    }

    Nm_NetworkRequested = 0U;

    if (Nm_State == NM_STATE_NORMAL_OPERATION)
        Nm_EnterReadySleep();  /* [SWS_CanNm_00118] */

    DET_LOGI(TAG, "NetworkRelease ok (state=%u)", (unsigned)Nm_State);
    return E_OK;
}

Std_ReturnType Nm_RepeatMessageRequest(void)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_REPEAT_MESSAGE_REQUEST, NM_E_UNINIT);
        return E_NOT_OK;
    }

    /* [SWS_CanNm_00137]: Repeat Message State / Prepare Bus-Sleep / Bus-Sleep
     * からの呼び出しは受理しない。 */
    if (Nm_State == NM_STATE_REPEAT_MESSAGE
        || Nm_State == NM_STATE_PREPARE_BUS_SLEEP
        || Nm_State == NM_STATE_BUS_SLEEP)
    {
        DET_LOGW(TAG, "RepeatMessageRequest W: rejected in state=%u", (unsigned)Nm_State);
        return E_NOT_OK;
    }

    Nm_RepeatMessageBitSet = 1U;  /* [SWS_CanNm_00113]（Ready Sleepから）/[SWS_CanNm_00121]（Normal Operationから） */
    Nm_EnterRepeatMessage();      /* [SWS_CanNm_00112]（Ready Sleepから）/[SWS_CanNm_00120]（Normal Operationから） */
    return E_OK;
}

void Nm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    DET_LOGT(TAG, "called");
    (void)RxPduId;

    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_RX_INDICATION, NM_E_UNINIT);
        return;
    }

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_RX_INDICATION, NM_E_PARAM_POINTER);
        return;
    }

    const uint8 cbv          = (PduInfoPtr->SduLength >= 1U) ? PduInfoPtr->SduDataPtr[0] : 0U;
    const uint8 sourceNodeId = (PduInfoPtr->SduLength >= 2U) ? PduInfoPtr->SduDataPtr[1] : 0U;

    switch (Nm_State)
    {
        case NM_STATE_BUS_SLEEP:
            /* [SWS_CanNm_00127]/[SWS_CanNm_00336]: 状態遷移
             * はせず DET へ通知するのみ。実際にネットワークへ復帰するか
             * どうかは上位層（本プロジェクトでは CanSM のウェイクアップ
             * 検証経由の ComM）が別途 Nm_NetworkRequest() を呼んで決める。 */
            DET_LOGW(TAG, "RxIndication W: NM PDU received in Bus-Sleep Mode (node=0x%02X)",
                     (unsigned)sourceNodeId);
            Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_RX_INDICATION, NM_E_NET_START_IND);
            break;

        case NM_STATE_PREPARE_BUS_SLEEP:
            /* [SWS_CanNm_00124]: 自動的に Network Mode (Repeat Message State) へ */
            DET_LOGI(TAG, "RxIndication: node=0x%02X woke us from Prepare Bus-Sleep", (unsigned)sourceNodeId);
            Nm_EnterRepeatMessage();
            break;

        case NM_STATE_REPEAT_MESSAGE:
        case NM_STATE_NORMAL_OPERATION:
        case NM_STATE_READY_SLEEP:
            /* [SWS_CanNm_00098]: Network Mode 中は NM-Timeout Timer を再起動
             * （＝他ノードがまだ通信中なら自ノードは眠れない、の核心部分）。 */
            Nm_TimeoutTimerMs = millis();

            if ((cbv & NM_CBV_BIT_REPEAT_MESSAGE_REQUEST) != 0U && Nm_State != NM_STATE_REPEAT_MESSAGE)
            {
                /* [SWS_CanNm_00111]/[SWS_CanNm_00119]: 他ノードの Repeat
                 * Message Request Bit を受信 -> 自ノードも再announce する
                 * （node detection）。ただし自ノード自身の CBV bit0 は
                 * 立てない（無限伝播を避ける簡略化）。 */
                DET_LOGI(TAG, "RxIndication: repeat message bit from node=0x%02X -> re-announce",
                         (unsigned)sourceNodeId);
                Nm_EnterRepeatMessage();
            }
            break;
    }
}

void Nm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    DET_LOGT(TAG, "called");
    (void)TxPduId;

    if (!Nm_Initialized || result != E_OK)
        return;

    /* [SWS_CanNm_00099]: Network Mode（Repeat Message/Normal Operation State）
     * での送信成功時に NM-Timeout Timer を再起動する。Ready Sleep State は
     * 送信自体を行わないため対象外。 */
    if (Nm_State == NM_STATE_REPEAT_MESSAGE || Nm_State == NM_STATE_NORMAL_OPERATION)
        Nm_TimeoutTimerMs = millis();
}

void Nm_MainFunction(void)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_MAIN_FUNCTION, NM_E_UNINIT);
        return;
    }

    const unsigned long now = millis();

    switch (Nm_State)
    {
        case NM_STATE_BUS_SLEEP:
            /* Nm_NetworkRequest() または Nm_RxIndication() 経由でのみ抜けられる */
            break;

        case NM_STATE_PREPARE_BUS_SLEEP:
            if ((now - Nm_StateTimerMs) >= NM_WAIT_BUS_SLEEP_MS)
                Nm_EnterBusSleep();  /* [SWS_CanNm_00115] */
            break;

        case NM_STATE_REPEAT_MESSAGE:
            if ((now - Nm_TimeoutTimerMs) >= NM_TIMEOUT_MS)
            {
                /* [SWS_CanNm_00193]/[SWS_CanNm_00101]: NM-Timeout Timer 満了時は
                 * タイマーの再起動と DET 報告のみを行う（PDU の (再)送信は
                 * 下記の Message Cycle Timer が独立して駆動するため、ここでは
                 * 行わない。[SWS_CanNm_00032]/[SWS_CanNm_00040] 参照）。
                 * 正常運転中は [SWS_CanNm_00099] によりほぼ毎周期の送信成功で
                 * このタイマーが先に再起動されるため、通常はここへ到達しない
                 * （到達した場合は Bus-Off 等の異常を示す。本ファイル冒頭の
                 * NM_E_NETWORK_TIMEOUT の説明参照）。 */
                Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_MAIN_FUNCTION, NM_E_NETWORK_TIMEOUT);
                Nm_TimeoutTimerMs = now;
            }

            /* [SWS_CanNm_00237]/[SWS_CanNm_00032]/[SWS_CanNm_00040]: Message
             * Cycle Timer（本プロジェクトでは Nm_MainFunction() 自体の呼び出し
             * 周期 NM_CYCLE_MS をそのまま周期として使う簡略化）による周期送信。
             * NM-Timeout Timer とは独立に、Repeat Message/Normal Operation
             * State の間は毎周期送信する。 */
            if (Nm_TxEnabled)
                Nm_TransmitPdu();

            if ((now - Nm_StateTimerMs) >= NM_REPEAT_MESSAGE_MS)
            {
                /* [SWS_CanNm_00102]/[SWS_CanNm_00103]/[SWS_CanNm_00106] */
                Nm_RepeatMessageBitSet = 0U;  /* [SWS_CanNm_00107] */
                if (Nm_NetworkRequested)
                    Nm_EnterNormalOperation();
                else
                    Nm_EnterReadySleep();
            }
            break;

        case NM_STATE_NORMAL_OPERATION:
            if ((now - Nm_TimeoutTimerMs) >= NM_TIMEOUT_MS)
            {
                /* [SWS_CanNm_00194]/[SWS_CanNm_00117]: 上記 Repeat Message State
                 * と同じ理由で、ここでは送信を行わない。 */
                Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_MAIN_FUNCTION, NM_E_NETWORK_TIMEOUT);
                Nm_TimeoutTimerMs = now;
            }

            /* Message Cycle Timer による周期送信（上記 Repeat Message State と同じ）。 */
            if (Nm_TxEnabled)
                Nm_TransmitPdu();
            break;

        case NM_STATE_READY_SLEEP:
            if ((now - Nm_TimeoutTimerMs) >= NM_TIMEOUT_MS)
                Nm_EnterPrepareBusSleep();  /* [SWS_CanNm_00109] */
            break;
    }
}

void Nm_SetTxEnabled(uint8 Enabled)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_SET_TX_ENABLED, NM_E_UNINIT);
        return;
    }

    if (Nm_TxEnabled != Enabled)
        DET_LOGI(TAG, "CommunicationControl tx=%u->%u", (unsigned)Nm_TxEnabled, (unsigned)Enabled);
    Nm_TxEnabled = Enabled;
}

Std_ReturnType Nm_GetState(Nm_StateType* StatePtr, Nm_ModeType* ModePtr)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_STATE, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (StatePtr != NULL)
        *StatePtr = Nm_State;

    if (ModePtr != NULL)
    {
        if (Nm_State == NM_STATE_BUS_SLEEP)
            *ModePtr = NM_MODE_BUS_SLEEP;
        else if (Nm_State == NM_STATE_PREPARE_BUS_SLEEP)
            *ModePtr = NM_MODE_PREPARE_BUS_SLEEP;
        else
            *ModePtr = NM_MODE_NETWORK;
    }

    return E_OK;
}

void Nm_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_VERSION_INFO, NM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = NM_VENDOR_ID;
    versioninfo->moduleID         = NM_MODULE_ID;
    versioninfo->sw_major_version = NM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = NM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = NM_SW_PATCH_VERSION;
}

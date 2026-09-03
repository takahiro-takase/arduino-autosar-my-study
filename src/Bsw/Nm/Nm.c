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
#include "ComM.h"
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

/** 直近に受信した NM フレームの送信元ノード ID（`Nm_GetNodeIdentifier()`
 *  [SWS_CanNm_00219] 用キャッシュ）。一度も受信していない間は 0。
 *  `Nm_RxIndication()` が受信の都度更新する。 */
static uint8 Nm_LastRxNodeId;

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

/**
 * \brief   Nm モジュールを初期化する。Bus-Sleep Mode から開始する。
 *
 * \pre        CanIf_Init() / ComM_Init() が正常に完了していること。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build
 *                        設定を持たないため。実 AUTOSAR 仕様は
 *                        SWS_CanNm_00208 で `CanNm_Init(const CanNm_ConfigType*
 *                        cannmConfigPtr)` を要求する）。
 *
 * \AUTOSARReq     {SWS_CanNm_00208}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_Init(const Nm_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    (void)ConfigPtr;  /* 常に NULL（post-build 設定を持たないため。Nm.h 参照） */
    Nm_State               = NM_STATE_BUS_SLEEP;
    Nm_NetworkRequested     = 0U;
    Nm_TxEnabled            = 1U;
    Nm_RepeatMessageBitSet  = 0U;
    Nm_LastRxNodeId         = 0U;
    Nm_TimeoutTimerMs       = millis();
    Nm_StateTimerMs         = millis();
    Nm_Initialized          = 1U;
    DET_LOGI(TAG, "Init ok node=0x%02X (Bus-Sleep Mode)", (unsigned)NM_SOURCE_NODE_ID);
}

/**
 * \brief   Nm モジュールを未初期化状態に戻す。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
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
 *
 *          上位層（本プロジェクトでは ComM）への通知として
 *          ComM_Nm_NetworkMode()（[SWS_ComM_00296]）を呼ぶ。呼び出す順序が
 *          2 つの理由で重要（変更する場合は両方を再検証すること）:
 *            1. 送信の正しさ: ComM_Nm_NetworkMode() は CanSM_RequestComMode()
 *               経由で同期的に物理コントローラを再起動しうる（Prepare
 *               Bus-Sleep Mode 中で SILENT_COM だった場合）。下の
 *               Nm_TransmitPdu() より後に置くと、この直後の
 *               (再)アナウンスフレームの送信が（コントローラがまだ
 *               Listen-Only のため）静かに失敗する。
 *            2. 再入安全性: ComM_Nm_NetworkMode() → CanSM_RequestComMode()
 *               → ComM_BusSM_ModeIndication() → Nm_NetworkRequest() という経路で
 *               本ファイルへ同期的に再入しうる。Nm_State を先に
 *               NM_STATE_REPEAT_MESSAGE へ更新済みだからこそ、再入した
 *               Nm_NetworkRequest() は「既に要求済み」の default 分岐に
 *               落ちて Nm_EnterRepeatMessage() への再帰を起こさない。
 */
static void Nm_EnterRepeatMessage(void)
{
    DET_LOGT(TAG, "called");
    Nm_State          = NM_STATE_REPEAT_MESSAGE;
    Nm_StateTimerMs    = millis();
    Nm_TimeoutTimerMs  = millis();
    DET_LOGI(TAG, "-> Network Mode: Repeat Message State");

    ComM_Nm_NetworkMode(0U);

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

/**
 * \brief   Ready Sleep State から Prepare Bus-Sleep Mode へ入る（[SWS_CanNm_00109]）。
 *
 * \details 上位層（本プロジェクトでは ComM）への通知として
 *          ComM_Nm_PrepareBusSleepMode()（[SWS_ComM_00826]）を呼ぶ。ComM は
 *          これを受けて CanSM_RequestComMode(SILENT_COM) を呼び、CAN
 *          コントローラを受信専用（Listen-Only）へ切り替える（ComM.c ファイル
 *          冒頭コメント参照）。
 */
static void Nm_EnterPrepareBusSleep(void)
{
    DET_LOGT(TAG, "called");
    Nm_State       = NM_STATE_PREPARE_BUS_SLEEP;
    Nm_StateTimerMs = millis();
    DET_LOGI(TAG, "-> Prepare Bus-Sleep Mode");
    ComM_Nm_PrepareBusSleepMode(0U);
}

/**
 * \brief   Prepare Bus-Sleep Mode から Bus-Sleep Mode へ入る（[SWS_CanNm_00115]）。
 *
 * \details [SWS_CanNm_00126]: 上位層（本プロジェクトでは ComM）への通知として
 *          ComM_Nm_BusSleepMode()（[SWS_ComM_00392]）を呼ぶ。ComM は
 *          ComM_RequestComMode(FULL_COM->NO_COM) の時点では Nm_NetworkRelease()
 *          を送るのみでチャネルを FULL_COM のまま据え置いており（協調スリープの
 *          起点、ComM.c ファイル冒頭コメント参照）、この通知を受けて初めて
 *          CanSM へ NO_COM を伝え、CAN コントローラを実際にスリープさせる。
 */
static void Nm_EnterBusSleep(void)
{
    DET_LOGT(TAG, "called");
    Nm_State = NM_STATE_BUS_SLEEP;
    DET_LOGI(TAG, "-> Bus-Sleep Mode");
    ComM_Nm_BusSleepMode(0U);
}

/**
 * \brief   通信が必要であることを Nm へ伝える（[SWS_CanNm_00104] 相当）。
 *
 * \details Bus-Sleep/Prepare Bus-Sleep Mode から呼ばれた場合は Repeat Message
 *          State へ、Ready Sleep State から呼ばれた場合は Normal Operation
 *          State へ遷移する。既に Repeat Message/Normal Operation State なら
 *          何もしない（冪等）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00213, SWS_CanNm_00192}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_NetworkRequest(NetworkHandleType Channel)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_REQUEST, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_REQUEST, NM_E_INVALID_CHANNEL);
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

/**
 * \brief   通信が不要になったことを Nm へ伝える（[SWS_CanNm_00105] 相当）。
 *
 * \details Normal Operation State から呼ばれた場合は Ready Sleep State へ
 *          遷移する（NM フレーム送信を停止するが、NM-Timeout Timer が
 *          満了するまでは Prepare Bus-Sleep Mode へは移行しない）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00214, SWS_CanNm_00192}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_NetworkRelease(NetworkHandleType Channel)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_RELEASE, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_NETWORK_RELEASE, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    Nm_NetworkRequested = 0U;

    if (Nm_State == NM_STATE_NORMAL_OPERATION)
        Nm_EnterReadySleep();  /* [SWS_CanNm_00118] */

    DET_LOGI(TAG, "NetworkRelease ok (state=%u)", (unsigned)Nm_State);
    return E_OK;
}

/**
 * \brief   Repeat Message State への遷移を要求する（[SWS_CanNm_00120] 相当）。
 *
 * \details 本プロジェクトでは診断・デバッグ用途を想定するのみで、通常の
 *          運用フローからは呼ばない。Repeat Message State/Prepare
 *          Bus-Sleep Mode/Bus-Sleep Mode から呼ばれた場合は無視する
 *          （[SWS_CanNm_00137]）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      Repeat Message State へ遷移した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または現在の状態では受理できない。
 *
 * \AUTOSARReq     {SWS_CanNm_00221, SWS_CanNm_00192}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_RepeatMessageRequest(NetworkHandleType Channel)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_REPEAT_MESSAGE_REQUEST, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_REPEAT_MESSAGE_REQUEST, NM_E_INVALID_CHANNEL);
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

/**
 * \brief   NM フレームの受信を通知する（CanIf から呼ばれる）。
 *
 * \details Network Mode 中は NM-Timeout Timer を再起動する
 *          （[SWS_CanNm_00098]）。Prepare Bus-Sleep Mode 中は Network Mode
 *          （Repeat Message State）へ自動遷移する（[SWS_CanNm_00124]）。
 *          Bus-Sleep Mode 中は状態遷移せず NM_E_NET_START_IND を DET へ
 *          報告するのみ（[SWS_CanNm_00127]/[SWS_CanNm_00336]。実際に
 *          ネットワークへ復帰するかどうかは上位層（本プロジェクトでは
 *          CanSM のウェイクアップ検証経由）が別途 Nm_NetworkRequest() を
 *          呼んで決める）。
 *
 * \param[in]  RxPduId     受信 PDU ID（本プロジェクトでは単一チャネルのため未使用）。
 * \param[in]  PduInfoPtr  受信データ。NULL 禁止。
 *
 * \ServiceID      {0x42}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
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

    /* [SWS_CanNm_00219] Nm_GetNodeIdentifier() 用キャッシュ。状態に関わらず
     * 受信の都度更新する（Bus-Sleep Mode 中の受信も含む）。 */
    Nm_LastRxNodeId = sourceNodeId;

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

/**
 * \brief   NM フレームの送信完了を通知する（CanIf から呼ばれる）。
 *
 * \details 送信成功時、Network Mode 中は NM-Timeout Timer を再起動する
 *          （[SWS_CanNm_00099]）。
 *
 * \param[in]  TxPduId  送信完了した PDU ID（本プロジェクトでは単一チャネルのため未使用）。
 * \param[in]  result   E_OK=送信成功。
 *
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
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

/**
 * \brief   Nm の周期処理。タイマ満了判定と NM フレームの（再）送信を行う。
 *
 * \ServiceID      {0x13}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
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

/**
 * \brief   診断 CommunicationControl (UDS SID 0x28) からの NM PDU 送信無効化要求を反映する
 *          （[SWS_CanNm_00215] 相当）。
 *
 * \details 無効化中は Repeat Message/Normal Operation State でも NM フレームを
 *          送信しない（[SWS_CanNm_00100] の passive mode 相当の抑制。状態機械
 *          自体は通常どおり遷移する）。実仕様（[SWS_CanNm_00172]）は現在
 *          Network Mode でない場合に E_NOT_OK を要求するが、本プロジェクトは
 *          そのゲートを実装しない（Bus-Sleep 中に呼ばれても抑制フラグ自体は
 *          そのまま更新して良く、次回 Network Mode 復帰時に正しく反映される
 *          ため。Nm_NetworkRequest/Release と同じ「現在の状態に関わらず常に
 *          受理する」簡略方針）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00215, SWS_CanNm_00192}
 * \ServiceID      {0x0C}
 * \Reentrancy     {Reentrant (but not for the same NM-channel)}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_DisableCommunication(NetworkHandleType Channel)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_DISABLE_COMMUNICATION, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_DISABLE_COMMUNICATION, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    if (Nm_TxEnabled != 0U)
        DET_LOGI(TAG, "CommunicationControl tx=%u->0", (unsigned)Nm_TxEnabled);
    Nm_TxEnabled = 0U;
    return E_OK;
}

/**
 * \brief   診断 CommunicationControl (UDS SID 0x28) からの NM PDU 送信再有効化要求を反映する
 *          （[SWS_CanNm_00216] 相当）。
 *
 * \details `Nm_DisableCommunication()` で立てた抑制を解除する。ゲート省略の
 *          方針は同関数のコメントを参照。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00216, SWS_CanNm_00192}
 * \ServiceID      {0x0D}
 * \Reentrancy     {Reentrant (but not for the same NM-channel)}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_EnableCommunication(NetworkHandleType Channel)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_ENABLE_COMMUNICATION, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_ENABLE_COMMUNICATION, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    if (Nm_TxEnabled != 1U)
        DET_LOGI(TAG, "CommunicationControl tx=%u->1", (unsigned)Nm_TxEnabled);
    Nm_TxEnabled = 1U;
    return E_OK;
}

/**
 * \brief   自ノードに設定されたノード識別子を取得する（[SWS_CanNm_00220]）。
 *
 * \details 送信する NM フレームに乗せる自ノードの ID（`NM_SOURCE_NODE_ID`）を
 *          そのまま返す。受信した NM フレームの送信元 ID を返す
 *          `Nm_GetNodeIdentifier()` とは区別されるので注意。
 *
 * \param[in]   Channel      NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  nmNodeIdPtr  自ノードの ID の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または nmNodeIdPtr が NULL。
 *
 * \AUTOSARReq     {SWS_CanNm_00220, SWS_CanNm_00192}
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_LOCAL_NODE_IDENTIFIER, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_LOCAL_NODE_IDENTIFIER, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    if (nmNodeIdPtr == NULL)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_LOCAL_NODE_IDENTIFIER, NM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *nmNodeIdPtr = NM_SOURCE_NODE_ID;
    return E_OK;
}

/**
 * \brief   直近に受信した NM フレームの送信元ノード識別子を取得する（[SWS_CanNm_00219]）。
 *
 * \details `Nm_RxIndication()` が受信の都度更新するキャッシュ値をそのまま
 *          返す。一度も NM フレームを受信していない場合は 0 を返す
 *          （実仕様は「未設定/取得失敗時は E_NOT_OK」も許容するが、本
 *          プロジェクトは他の getter 系 API と同じく既定値を返すだけの
 *          簡略実装とする）。
 *
 * \param[in]   Channel      NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  nmNodeIdPtr  直近受信 NM フレームの送信元 ID の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または nmNodeIdPtr が NULL。
 *
 * \AUTOSARReq     {SWS_CanNm_00219, SWS_CanNm_00192}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_NODE_IDENTIFIER, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_NODE_IDENTIFIER, NM_E_INVALID_CHANNEL);
        return E_NOT_OK;
    }

    if (nmNodeIdPtr == NULL)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_NODE_IDENTIFIER, NM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *nmNodeIdPtr = Nm_LastRxNodeId;
    return E_OK;
}

/**
 * \brief   現在の CanNm 状態とモードを取得する（[SWS_CanNm_00091] 相当）。
 *
 * \param[in]   Channel   NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  StatePtr  現在の内部状態の格納先。NULL 可（不要なら渡さなくてよい）。
 * \param[out]  ModePtr   現在の操作モードの格納先。NULL 可。
 *
 * \retval  E_OK      取得した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00223, SWS_CanNm_00192}
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_GetState(NetworkHandleType Channel, Nm_StateType* StatePtr, Nm_ModeType* ModePtr)
{
    DET_LOGT(TAG, "called");
    if (!Nm_Initialized)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_STATE, NM_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel != NM_MAIN_NETWORK_HANDLE)
    {
        Det_ReportError(NM_MODULE_ID, 0U, NM_API_ID_GET_STATE, NM_E_INVALID_CHANNEL);
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

/**
 * \brief   Nm モジュールのバージョン情報を取得する。
 *
 * \details Nm_Init と並び、未初期化時でも NM_E_UNINIT を報告しない例外 API
 *          （他 BSW モジュールと共通の慣例）のため、初期化状態は確認せず
 *          NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
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

/**
 * \file    Com.c
 * \brief   通信マネージャ (AUTOSAR SWS_COM 準拠)
 * \details シグナルベースの通信を行う AUTOSAR COM API 層を実装する。
 *          RX/TX I-PDU バッファを管理し、設定可能なビットエンディアン
 *          (Motorola/Intel) でシグナルのパック・アンパックを行う。
 *          AUTOSAR 4.3.1 SWS_COM 仕様に準拠し、Arduino UNO 向けに
 *          バッファ数固定・更新ビットなしに簡略化している。
 *          受信デッドライン監視 (SWS_Com_00398) を実装しており、
 *          Com_MainFunctionRx() が周期的にタイムアウトを検出する。
 *          タイムアウト中の I-PDU に属するシグナルは Com_ReceiveSignal() が
 *          E_NOT_OK を返し、上位層（RTE/ASW）がフェイルセーフ処理を行う。
 *
 *          E2E は関知しない（E2E Transformer 方式）:
 *          本モジュールは E2E Profile 1 の存在を一切知らない。E2E 保護は
 *          Com_IPduConfigType.RxIndicationCbk / TxTransformCbk という汎用
 *          フック経由で、Com の外側（Rte 層 + E2EXf モジュール）が担う。
 *          これは AUTOSAR が定義する 3 つの E2E 統合方式のうち「E2E
 *          Transformer」（docs/AUTOSAR_SWS_E2ELibrary.pdf 12.4 節）に相当し、
 *          Com に E2E ロジックを直接埋め込む「COM E2E Callout」方式（かつて
 *          このファイルが採用していた設計）とは異なる。詳細は
 *          src/Bsw/E2EXf/E2EXf.c のファイル冒頭コメントを参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Com.h"
#include "PduR.h"
#include "Det.h"

#define TAG "Com"

#define COM_IPDU_MAX_DLC  8U
#define COM_RX_IPDU_MAX   COM_RX_IPDU_COUNT  /* Com_Cfg.h の設定値に連動 */
#define COM_TX_IPDU_MAX   COM_TX_IPDU_COUNT  /* Com_Cfg.h の設定値に連動 */

/* millis() は Arduino wiring.c で C リンケージ定義されている */
extern unsigned long millis(void);

/* Signal Gateway（Com_GwMappingType 参照）。Com_UnpackSignal() を使うため
 * 定義は同関数より後に置くが、Com_RxIndication() から呼ぶため前方宣言する
 * （本ファイルの他の static ヘルパーとは異なり、定義順を呼び出し順と
 * 一致させられないための例外）。 */
static void Com_GatewayRoute(Com_IPduIdType rxIPduId);

/* Com_Init()/Com_IpduGroupStart() の I-PDU バッファ初期化（ComSignalInitValue
 * のビット単位パック）で使うため、定義（Com_PackSignal より後）より前に
 * 前方宣言する。Com_GatewayRoute と同じ理由の例外。 */
static void Com_PackInitValues(uint8* buf, Com_IPduIdType id, Com_SignalDirectionType dir);

static const Com_ConfigType* Com_ConfigPtr = NULL;
static uint8         Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8         Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];
static unsigned long Com_RxLastMs[COM_RX_IPDU_MAX];   /* 最終受信時刻 [ms] */
static uint8         Com_RxTimedOut[COM_RX_IPDU_MAX];  /* 1 = タイムアウト中 */

/* 1 = Com_Init()/Com_IpduGroupStart()/Com_SetCommunicationEnabled() による
 * 監視（再）開始以降、まだ一度も実受信していない（= デッドライン判定に
 * ComFirstTimeout 相当の Com_IPduConfigType.FirstTimeoutMs を使うべき）状態。
 * 0 = 既に 1 回以上受信済みで、以降は定常状態の TimeoutMs（ComTimeout 相当）
 * を使う（[SWS_Com_00787] 項目2・[SWS_Com_00716]・[SWS_Com_00879] 相当の
 * 「初回は FirstTimeout、以降は Timeout」を受信デッドライン監視にも適用）。 */
static uint8         Com_RxUsingFirstTimeout[COM_RX_IPDU_MAX];

/* シグナル単位のデッドライン監視ラッチ（1 = このシグナルのタイムアウトを
 * 検出済み）。Com_SignalConfigType.FirstTimeoutMs/TimeoutMs を設定した
 * 非 Signal Group の RX シグナルにのみ意味を持つ（Signal Group メンバーは
 * 引き続き Com_RxTimedOut/Com_RxShadowTimedOut（I-PDU/グループ単位）を使う
 * ため、このシグナルについては本配列は常に 0 のまま参照されない）。
 * タイマーの起点は所属 I-PDU の Com_RxLastMs[]・フェーズ判定は
 * Com_RxUsingFirstTimeout[] を共有する（Com_SignalConfigType の
 * FirstTimeoutMs/TimeoutMs 宣言コメント参照）。 */
static uint8         Com_SigTimedOut[COM_SIGNAL_COUNT];

/* -----------------------------------------------------------------------
 * RX Signal Group（ComIPduConfigType.IsSignalGroup = 1、RX I-PDU 側）関連の
 * 内部状態。TX 側のシャドウバッファ（Com_TxShadowBuffer 等、下記）の対称。
 * Com_ReceiveSignalGroup() が Com_RxBuffer から確定コピーし、以降
 * Com_ReceiveSignal() はグループメンバーに対してこちらを読む。
 * ----------------------------------------------------------------------- */
static uint8 Com_RxShadowBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];

/* Com_ReceiveSignalGroup() 実行時点の Com_RxTimedOut[] のスナップショット。
 * Com_ReceiveSignal() はグループメンバーの読み取り可否をこちらで判定する
 * （ライブの Com_RxTimedOut[] を都度見ると、同じグループの複数メンバーを
 * 読む間にタイムアウト判定が変化してしまい、スナップショットの一貫性が
 * 崩れるため）。既定値 1（未コミット = 利用不可、Com_RxTimedOut の既定 0 とは
 * 意図的に異なる。詳細は Com_Init() 参照）。 */
static uint8 Com_RxShadowTimedOut[COM_RX_IPDU_MAX];

/* COM_TX_MODE_PERIODIC/MIXED の周期フロア判定用、最終送信時刻 [ms]。
 * Com_MainFunctionTx() が実送信するたび（Com_TxPending 経由・周期フロア
 * 経由いずれも）更新することで、MIXED の周期フロアが直近の送信からの
 * 経過時間を基準に動く（COM_TX_MODE_DIRECT の I-PDU では未使用）。 */
static unsigned long Com_TxLastSentMs[COM_TX_IPDU_MAX];

/* 診断 CommunicationControl (UDS SID 0x28) からの通信有効/無効状態。
 * 既定は両方とも有効 (1)。Com_SetCommunicationEnabled() 参照。
 *
 * Com_Rx/TxIPduStarted[]（下記）とは独立した、直交する抑制機構である点に
 * 注意: こちらは「全 I-PDU 一括」の診断用スイッチ、Started は「I-PDU Group
 * 単位」の起動/停止状態。実際に送受信処理が行われるのは両方が真の場合のみ
 * （AND 条件）。 */
static uint8 Com_RxEnabled = 1U;
static uint8 Com_TxEnabled = 1U;

/* I-PDU Group（Com_IPduConfigType.IpduGroupId）の起動/停止状態
 * （Com_IpduGroupStart()/Com_IpduGroupStop() 参照）。IPduId を添字として使う
 * （Com_RxBuffer/Com_TxBuffer 等、既存の配列と同じ規約）。
 * IpduGroupId==COM_IPDU_GROUP_NONE の I-PDU は Com_Init() で常に 1（起動済み）
 * のまま変化しない（[SWS_Com_00840]）。それ以外は Com_Init() で既定 0（停止）
 * となり（[SWS_Com_00444]）、Com_IpduGroupStart() が呼ばれるまで有効にならない。 */
static uint8 Com_RxIPduStarted[COM_RX_IPDU_MAX];
static uint8 Com_TxIPduStarted[COM_TX_IPDU_MAX];

/* Com_EnableReceptionDM()/Com_DisableReceptionDM()（SRS_Com_00192、2026-08
 * 追加）用。Com_RxIPduStarted とは独立した軸で、I-PDU Group の起動/停止
 * （送受信処理そのもの）とは別に、受信デッドライン監視の評価だけを
 * 個別に止められるようにする。既定は 1（有効、Com_Init() 参照）。 */
static uint8 Com_RxDmEnabled[COM_RX_IPDU_MAX];

/* Com_ConfigPtr->RxIPdus[].IsSignalGroup を IPduId 添字で引けるようにした
 * キャッシュ（Com_Init() で一度だけ設定、以降不変。/simplify で指摘された
 * 「Com_MainFunctionRx() のシグナル単位ループが、直前の I-PDU 単位ループで
 * 既に読んだ IsSignalGroup を、毎 tick Com_FindRxIPdu() の線形探索で
 * 再導出していた」重複を解消するために追加）。 */
static uint8 Com_RxIPduIsGroup[COM_RX_IPDU_MAX];

/* -----------------------------------------------------------------------
 * TX シグナルフィルタ（ComFilterAlgorithm）関連の内部状態
 * ----------------------------------------------------------------------- */
static uint32 Com_FilterLastValue[COM_SIGNAL_COUNT];  /* シグナルごとの直近フィルタ比較値 */

/* ComDataInvalidAction=COM_DATA_INVALID_ACTION_NOTIFY の RX シグナル用、
 * 直近の有効値（InvalidValue と一致しなかった、最後に受理した値）。
 * SWS_Com_00717: 無効値受信中は Com_ReceiveSignal() がこれを返し続け、
 * 実バッファ/シャドウバッファの中身（無効値そのもの）は返さない。 */
static uint32 Com_RxLastValidValue[COM_SIGNAL_COUNT];

/* Com_ReceiveSignal() が無効値を検知した際に立てる、「次回 Com_MainFunctionRx()
 * で InvalidNotificationCbk を呼ぶべき」フラグ。実際のコールバック呼び出しは
 * 必ず Com_MainFunctionRx() 側へディスパッチし、Com_ReceiveSignal() の呼び出し
 * スタックフレームでは行わない。
 *
 * 理由（実機で確認済みの障害）: Com_ReceiveSignal() は Rte 層の
 * SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()（実体は noInterrupts()/
 * interrupts()、グローバル割り込み禁止）の内側から呼ばれることがある
 * （Rte_COMRxInd_EngineInfo() 等）。この区間内でコールバックを直接呼び、
 * コールバックが Serial 出力のような割り込み駆動の I/O を行うと、
 * 割り込み禁止中は UART TX バッファが空かず実質無限ループとなり、
 * WDT リセットを引き起こす。Com_TxPending と同じ設計思想（実処理を
 * ASW/Rte のスタックフレームから切り離し、必ず Com_MainFunctionRx() 側で
 * 行う）でこれを回避する。 */
static uint8 Com_RxInvalidNotifyPending[COM_SIGNAL_COUNT];

/* ComFilterAlgorithm=COM_FILTER_NEW_IS_WITHIN の RX シグナル用、
 * Com_RxInvalidNotifyPending と全く同じ理由・同じ仕組みの遅延ディスパッチ
 * フラグ（「次回 Com_MainFunctionRx() で FilterRejectCbk を呼ぶべき」）。
 * Com_ReceiveSignal() が範囲外の値を検知した際にここを立てるだけに留め、
 * 実際のコールバック呼び出しは必ず Com_MainFunctionRx() 側で行う。 */
static uint8 Com_RxFilterRejectPending[COM_SIGNAL_COUNT];

/* DIRECT/MIXED I-PDU 用、「次回 Com_MainFunctionTx() で送信すべき変化あり」フラグ。
 * 実送信（PduR_ComTransmit → ... → MCP2515 への SPI 送信）を ASW Runnable の
 * スタックフレームから切り離し、必ず Com_MainFunctionTx()（Os の 100ms タスク、
 * WdgM 非監視）側で行うためのディスパッチ機構。COM_TX_MODE_PERIODIC の
 * I-PDU では未使用（常に 0）。 */
static uint8 Com_TxPending[COM_TX_IPDU_MAX];

/* Com_TriggerIPDUSend()（SWS_Com_00861/00388、2026-08 追加）専用のフラグ。
 * Com_TxPending とは別軸で持つ理由: Com_TxPending は COM_TX_MODE_PERIODIC の
 * I-PDU では一切参照されない（Com_RequestTxOnChange() が PERIODIC I-PDU に
 * 対して早期 return するため、上記コメント参照）。しかし
 * Com_TriggerIPDUSend は「I-PDU が started であれば TxModeMode によらず
 * 送信をトリガーする」仕様（[SWS_Com_00861] に TxModeMode による除外の
 * 記述はない）のため、PERIODIC I-PDU に対しても効かせる必要がある。
 * Com_MainFunctionTx() の due 判定に、Com_TxPending とは独立した OR 項として
 * 追加する（[SWS_Com_00388]: MDT のみ尊重し、ComTxModeNumberOfRepetitions
 * 等の他の TxMode パラメータは考慮しない——そのため
 * ComTxModeNumberOfRepetitions の残り再送回数デクリメント判定
 * （`repeatDue && !changeDue`）にはあえて混ぜていない）。 */
static uint8 Com_TxTriggerPending[COM_TX_IPDU_MAX];

/* 「PduR_ComTransmit() には渡した（実送信済み）が、対応する Com_TxConfirmation()
 * がまだ届いていない」ことを示すフラグ（TX I-PDU 単位）。Com_TxPending が
 * 「まだ送信要求すら出していない」を表すのに対し、こちらは送信要求は完了して
 * 確認応答だけを待っている状態を表す（両者は排他ではなく別軸）。
 * Com_DoTransmit() が PduR_ComTransmit() 成功時にセットし、
 * Com_TxConfirmation()（成功/失敗を問わず）が届いた時点でクリアする。
 * [SWS_Com_00479]/[SWS_Com_00491]: Com_IpduGroupStop() 実行時にこのフラグが
 * 立ったままの I-PDU があれば、確認されないまま停止されたとみなし
 * TxErrCbk（Com_CbkTxErr 相当）を即座に呼んでからクリアする。 */
static uint8 Com_TxConfPending[COM_TX_IPDU_MAX];

/* TX 送信デッドライン監視（Com_CbkTxTOut、SWS_Com_00878/00879/00880/00304/
 * 00554）用の状態。Com_TxConfPendingSinceMs は Com_TxConfPending が 0→1 に
 * 遷移した時刻（Com_DoTransmit() が記録）。Com_TxTimedOut はエッジラッチ
 * （Com_RxTimedOut と同じく、タイムアウト検出のたびに 1 回だけ通知する）。
 * Com_TxUsingFirstTimeout は 1 = 再始動後まだ一度も Com_TxConfirmation()
 * が届いていない（TxFirstTimeoutMs を使う）、Com_TxConfirmation() 到達で
 * 0 へ（以降は steady TxTimeoutMs を使う、Com_RxUsingFirstTimeout と対称）。 */
static unsigned long Com_TxConfPendingSinceMs[COM_TX_IPDU_MAX];
static uint8         Com_TxTimedOut[COM_TX_IPDU_MAX];
static uint8         Com_TxUsingFirstTimeout[COM_TX_IPDU_MAX];

/* TMS（Transmission Mode Selector）評価結果。1 = true（TxModeModeTrue/
 * TxPeriodMsTrue を使う）、0 = false（TxModeMode/TxPeriodMs を使う）。
 * Com_RecalcTms() が Com_SendSignal()/Com_SendSignalGroup() のたびに
 * 再評価する（SWS_Com_00245）。TmsContributor を持つシグナルが存在しない
 * I-PDU では常に 0 のまま変化しない（＝常に false 側のみを使う、既存の
 * 単一モード I-PDU と同じ挙動）。 */
static uint8 Com_TmsState[COM_TX_IPDU_MAX];

/* ComTxModeNumberOfRepetitions（SWS_Com_00305）の残り再送回数。
 * Com_RequestTxOnChange() が新規要求のたびに ipdu->NumberOfRepetitions で
 * 上書き、Com_MainFunctionTx() が実際の再送 dispatch のたびに 1 減らす
 * （確認 (Com_TxConfirmation) 到達時ではなく dispatch 時点で減らす理由は
 * Com_MainFunctionTx() のコメント・docs/modules/Com_Notes.md 参照）。0 なら
 * 再送なし。Com_IpduGroupStart()/Com_IpduGroupStop() でも Com_TxPending
 * 同様にゼロへリセットする（[SWS_Com_00392]）。 */
static uint8 Com_TxRepeatsRemaining[COM_TX_IPDU_MAX];

/* -----------------------------------------------------------------------
 * Signal Group（ComIPduConfigType.IsSignalGroup = 1）関連の内部状態
 * Com_SendSignal() は Signal Group メンバーをここへ書き込み、実バッファ
 * (Com_TxBuffer) へは反映しない。Com_SendSignalGroup() が呼ばれた時点で
 * まとめて実バッファへ確定コミットする。
 * ----------------------------------------------------------------------- */
static uint8 Com_TxShadowBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

/* ComTransferProperty=COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE のメンバーが
 * 変化を検知するたび Com_SendSignal() が立てる、「このグループの送信を
 * 引き起こす」フラグ。COM_TRANSFER_PROPERTY_PENDING のメンバーはここへ
 * 一切書き込まない（＝自身の変化だけでは送信を引き起こさない）。
 * Com_SendSignalGroup() が読み取ってクリアする。 */
static uint8 Com_GroupTriggerPending[COM_TX_IPDU_MAX];

/**
 * \brief   COM モジュールを初期化し、すべての I-PDU バッファをクリアする。
 *
 * \details 設定ポインタを保存し、RX/TX の全 I-PDU バッファをまずゼロクリアし、
 *          続けて各シグナルの ComSignalInitValue（Com_SignalConfigType.
 *          InitValue）をビット単位でパックする（[SWS_Com_00217]/
 *          [SWS_Com_00098]）。TX フィルタの old_value（Com_FilterLastValue）・
 *          RX の「直近の合格値」（Com_RxLastValidValue）も同じ InitValue から
 *          始める（[SWS_Com_00603]/[SWS_Com_00717]）。シグナル設定テーブルを
 *          ログ出力する。RX または TX の I-PDU 数がコンパイル時上限を超える
 *          場合は初期化を中断する。
 *
 * \param[in]  config  COM 設定構造体へのポインタ。NULL 禁止。
 *
 * \pre        PduR_Init() が正常に完了していること。
 * \note       COM_RX_IPDU_MAX / COM_TX_IPDU_MAX はコンパイル時定数で 1 に固定。
 *             それを超える設定は拒否される。
 *
 * \AUTOSARReq     {SWS_Com_00432, SWS_Com_00217, SWS_Com_00098, SWS_Com_00603}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_Init(const Com_ConfigType* config)
{
    DET_LOGT(TAG, "called");

    if (config == NULL)
    {
        DET_LOGE(TAG, "Init E: config NULL");
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INIT, COM_E_PARAM_POINTER);
        return;
    }
    if (config->RxIPduCount > COM_RX_IPDU_MAX)
    {
        DET_LOGE(TAG, "Init E: RxIPduCount>max");
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INIT, COM_E_INIT_FAILED);
        return;
    }
    if (config->TxIPduCount > COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "Init E: TxIPduCount>max");
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INIT, COM_E_INIT_FAILED);
        return;
    }

    Com_ConfigPtr = config;
    Com_RxEnabled = 1U;
    Com_TxEnabled = 1U;

    const unsigned long now = millis();
    for (uint8 i = 0; i < COM_RX_IPDU_MAX; i++)
    {
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
        {
            Com_RxBuffer[i][j]       = 0U;
            Com_RxShadowBuffer[i][j] = 0U;
        }
        Com_RxLastMs[i]  = now;  /* タイムアウト計測を Init 時刻から開始 */
        Com_RxTimedOut[i] = 0U;
        Com_RxUsingFirstTimeout[i] = 1U;  /* 起動直後は ComFirstTimeout 相当を使う */
        /* RX Signal Group 未コミット状態。Com_ReceiveSignalGroup() が一度も
         * 呼ばれていないグループメンバーを、ゼロクリアされたシャドウバッファ
         * を「正常な値」として誤って返さないよう、利用不可扱いにしておく。 */
        Com_RxShadowTimedOut[i] = 1U;
        Com_RxIPduStarted[i] = 1U;  /* IPduId 範囲外の添字保護のため、まず全域を
                                     * 起動済みにしておき、下の実ループで
                                     * I-PDU Group 所属分のみ上書きする */
        Com_RxIPduIsGroup[i] = 0U;  /* 同上、下の実ループで実際の値を設定する */
        Com_RxDmEnabled[i]   = 1U;  /* デッドライン監視は既定で有効
                                     * （Com_DisableReceptionDM() を明示的に
                                     * 呼ぶまで無効化されない） */
    }

    for (uint8 i = 0; i < COM_TX_IPDU_MAX; i++)
    {
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
        {
            Com_TxBuffer[i][j]       = 0U;
            Com_TxShadowBuffer[i][j] = 0U;
        }
        Com_TxLastSentMs[i]      = now;  /* PERIODIC/MIXED の周期計測を Init 時刻から開始 */
        Com_TxPending[i]         = 0U;
        Com_TxTriggerPending[i]  = 0U;
        Com_TxConfPending[i]     = 0U;
        Com_TmsState[i]          = 0U;   /* 既定 false（ゼロクリアされたバッファと整合） */
        Com_TxRepeatsRemaining[i] = 0U;
        Com_TxConfPendingSinceMs[i] = now;
        Com_TxTimedOut[i]           = 0U;
        Com_TxUsingFirstTimeout[i]  = 1U;
        Com_GroupTriggerPending[i] = 0U;
        Com_TxIPduStarted[i] = 1U;  /* 上と同じ理由 */
    }

    /* I-PDU Group に所属する I-PDU は既定で停止状態にする（[SWS_Com_00444]:
     * 全 I-PDU Group は既定で停止。[SWS_Com_00840]: 所属しない I-PDU は常に
     * 起動済みのまま）。設定テーブルを実際に走査する必要があるため、上の
     * ゼロクリアループとは別に行う（IpduGroupId は Com_IPduConfigType 側に
     * あり、添字だけでは判定できないため）。 */
    for (uint8 i = 0; i < config->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &config->RxIPdus[i];
        if (ipdu->IpduGroupId != COM_IPDU_GROUP_NONE)
            Com_RxIPduStarted[ipdu->IPduId] = 0U;
        Com_RxIPduIsGroup[ipdu->IPduId] = ipdu->IsSignalGroup;
    }
    for (uint8 i = 0; i < config->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &config->TxIPdus[i];
        if (ipdu->IpduGroupId != COM_IPDU_GROUP_NONE)
            Com_TxIPduStarted[ipdu->IPduId] = 0U;
    }

    /* [SWS_Com_00217]: 上のゼロクリアに続けて、I-PDU バッファをビット単位で
     * 各シグナルの ComSignalInitValue（Com_SignalConfigType.InitValue）で
     * 上書きする。既定（未設定）の InitValue=0 なら、直前のゼロクリアと
     * 結果が変わらないため、既存の全シグナルの挙動には影響しない。
     * Signal Group のシャドウバッファも同様に初期化するが、
     * Com_RxShadowTimedOut=1U（上のループで既定設定済み）により
     * Com_ReceiveSignal() から実際に読まれるのは Com_ReceiveSignalGroup() が
     * 一度確定コピーした後のみである。 */
    for (uint8 i = 0; i < config->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &config->RxIPdus[i];
        Com_PackInitValues(Com_RxBuffer[ipdu->IPduId], ipdu->IPduId, COM_SIGNAL_DIRECTION_RX);
        if (ipdu->IsSignalGroup != 0U)
            Com_PackInitValues(Com_RxShadowBuffer[ipdu->IPduId], ipdu->IPduId, COM_SIGNAL_DIRECTION_RX);
    }
    for (uint8 i = 0; i < config->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &config->TxIPdus[i];
        Com_PackInitValues(Com_TxBuffer[ipdu->IPduId], ipdu->IPduId, COM_SIGNAL_DIRECTION_TX);
        if (ipdu->IsSignalGroup != 0U)
            Com_PackInitValues(Com_TxShadowBuffer[ipdu->IPduId], ipdu->IPduId, COM_SIGNAL_DIRECTION_TX);
    }

    for (uint8 s = 0; s < config->SignalCount; s++)
    {
        /* [SWS_Com_00603]: フィルタ old_value の起動時初期値も InitValue に
         * 揃える（COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD が、起動直後に
         * InitValue と同じ値を送っただけで誤って「変化あり」と判定しない
         * ようにするため）。Com_RxLastValidValue も同じ理由（SWS_Com_00717/
         * 00718: 「まだ一度も受信していない場合は InitValue を返す」）で
         * InitValue から始める。 */
        const uint32 initVal = config->Signals[s].InitValue;
        Com_FilterLastValue[s]         = initVal;
        Com_RxLastValidValue[s]        = initVal;
        Com_RxInvalidNotifyPending[s]  = 0U;
        Com_RxFilterRejectPending[s]   = 0U;
        Com_SigTimedOut[s]             = 0U;
    }

    DET_LOGI(TAG, "Init ok RX=%u TX=%u sig=%u",
             (unsigned)config->RxIPduCount,
             (unsigned)config->TxIPduCount,
             (unsigned)config->SignalCount);
}

/**
 * \brief   COM モジュールを未初期化状態に戻す。
 *
 * \details [SWS_Com_00129]: 起動中の全 I-PDU を停止済み状態にする。
 *          `COM_IPDU_GROUP_NONE` 所属（常時有効、Com_IpduGroupStart/Stop() の
 *          対象外）の I-PDU も含め、モジュール全体が未初期化に戻る以上
 *          無条件に全エントリを停止する。最後に `Com_ConfigPtr` を NULL へ
 *          戻すことで、以降 `Com_GetStatus()` は `COM_UNINIT` を返し、他の
 *          全 API は既存の `Com_ConfigPtr == NULL` チェックにより
 *          `COM_E_UNINIT` を報告するようになる（[SWS_Com_00804]）。
 *          Com_TxPending 等の一時状態は明示的にクリアしない
 *          （次回 Com_Init() が全状態を再初期化するため、ここでの二重クリアは
 *          不要）。
 *
 * \AUTOSARReq     {SWS_Com_00129, SWS_Com_00130, SWS_Com_00804}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_DeInit(void)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_DEINIT, COM_E_UNINIT);
        return;
    }

    for (uint8 i = 0U; i < COM_RX_IPDU_MAX; i++)
        Com_RxIPduStarted[i] = 0U;
    for (uint8 i = 0U; i < COM_TX_IPDU_MAX; i++)
        Com_TxIPduStarted[i] = 0U;

    Com_ConfigPtr = NULL;

    DET_LOGI(TAG, "DeInit ok");
}

/**
 * \brief   COM モジュールの初期化状態を返す。
 *
 * \details [SWS_Com_00804] が「Com_GetStatus を除く他の全 API は未初期化時に
 *          COM_E_UNINIT を報告する」と規定しているとおり、本関数のみが
 *          唯一の例外として未初期化状態でも安全に呼べる（開発エラー報告を
 *          行わない）。
 *
 * \AUTOSARReq     {SWS_Com_00194}
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Com_StatusType Com_GetStatus(void)
{
    DET_LOGT(TAG, "called");
    return (Com_ConfigPtr != NULL) ? COM_INIT : COM_UNINIT;
}

/**
 * \brief   COM モジュールのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_Com_00426}
 * \ServiceID      {0x09}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_GET_VERSION_INFO, COM_E_UNINIT);
        return;
    }

    if (versioninfo == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_GET_VERSION_INFO, COM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID          = COM_VENDOR_ID;
    versioninfo->moduleID          = COM_MODULE_ID;
    versioninfo->sw_major_version  = COM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version  = COM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version  = COM_SW_PATCH_VERSION;
}

/**
 * \brief   受信した I-PDU ペイロードを内部 RX バッファへコピーする。
 *
 * \details PduR がフレームを受信した際に呼び出される。
 *          RxPduId（PduR 名前空間）に一致する RX I-PDU エントリを検索し、
 *          ペイロードを対応する RX バッファスロットへコピーしてログ出力する。
 *          この呼び出し後、Com_ReceiveSignal() でシグナル値を取得できる。
 *
 *          受信長 (PduInfoPtr->SduLength) が設定 DLC (ipdu->DLC) に満たない
 *          場合の扱いは Signal Group かどうかで異なる（詳細は本体コメント
 *          参照）:
 *            - Signal Group（IsSignalGroup=1）: I-PDU 全体を棄却する
 *              （[SWS_Com_00575]: 部分受信したグループを一貫性のないまま
 *              公開しない）。
 *            - 非 Signal Group: 実際に受信できたバイト数の範囲内に収まる
 *              シグナルのみを部分的に受理する（[SWS_Com_00574]/
 *              [SWS_Com_00870]）。範囲外のシグナルは前回受信値のまま
 *              据え置かれる。
 *          DLC を超える分（CAN フレームが 8 バイト固定でパディングされている
 *          場合の末尾バイト等）は許容し、先頭 DLC バイトのみを読み取る。
 *
 *          上記のいずれよりも前に、受信デッドライン監視タイマ（Com_RxLastMs
 *          等）をまずリセットする（[SWS_Com_00872] 段階1）。続けて
 *          `ipdu->RxIpduCalloutCbk` が設定されていれば Com_RxIpduCallout
 *          （[SWS_Com_00700]、段階2）が呼ばれる。0 を返せばこの受信は
 *          以降一切処理しない（バッファ更新・通知は行わないが、タイマは
 *          既にリセット済みのまま——詳細は Com_Types.h の RxIpduCalloutCbk
 *          コメント参照）。
 *
 *          注意（実機非到達の既知の制約）: 本プロジェクトは「多層防御」として
 *          CanIf_RxIndication() 自身も独立した受信長チェックを持ち
 *          （CanIf.c 参照、SWS_CANIF_00026/00168 相当）、CanIf_PBCfg.c の
 *          該当 RxPdu の `.Dlc` は現状 Com 側の `ipdu->DLC` と同じ値（例:
 *          EngineInfo は双方とも 6）に設定されている。そのため
 *          `SduLength < ipdu->DLC` の短小フレームは CanIf 層で先に棄却され、
 *          本関数（の非 Signal Group 部分受理パス）へは実運用上到達しない
 *          （2026-07 時点で実機確認済み。VehicleSpeed の
 *          RxDataTimeoutAction=SUBSTITUTE が実際には発動しないのと同種の
 *          制約）。本パスを実機で最後まで検証するには、CanIf 側の `.Dlc` を
 *          Com 側より緩く設定する必要がある。
 *
 * \param[in]  RxPduId     受信 I-PDU の PduR 層 PDU ID。
 *                         Com_IPduConfigType エントリの検索に使用する。
 * \param[in]  PduInfoPtr  受信 PDU のデータと長さへのポインタ。
 *                         NULL 禁止。SduDataPtr も NULL 禁止
 *                         （本関数が直接 SduDataPtr[b] を参照するため必須）。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Com_00123, SWS_Com_00574, SWS_Com_00575, SWS_Com_00870,
 *                  SWS_Com_00555, SWS_Com_00700, SWS_Com_00816, SWS_Com_00872,
 *                  SWS_Com_00715, SWS_Com_00738}
 * \ServiceID      {0x42}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RX_INDICATION, COM_E_UNINIT);
        return;
    }
    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RX_INDICATION, COM_E_PARAM_POINTER);
        return;
    }

    if (Com_RxEnabled == 0U)
    {
        /* 診断 CommunicationControl (UDS 0x28) による受信抑制中。バッファ・
         * タイムアウトタイマとも更新しない（受信長チェックと同じ「何もしない」
         * 扱い）。頻繁に呼ばれるため DEBUG レベルに留める。 */
        DET_LOGD(TAG, "RX suppressed (CommunicationControl) src=%u", (unsigned)RxPduId);
        return;
    }

    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->PduRId != RxPduId)
            continue;

        /* I-PDU Group が停止中（Com_IpduGroupStop() 参照）: 受信処理自体を
         * 無効化する（[SWS_Com_00684]）。バッファ・タイムアウトタイマとも
         * 更新しない（上の Com_RxEnabled==0 の場合と同じ「何もしない」扱い）。 */
        if (!Com_RxIPduStarted[ipdu->IPduId])
        {
            DET_LOGD(TAG, "RX suppressed (I-PDU Group stopped) iPdu=%u", (unsigned)ipdu->IPduId);
            return;
        }

        /* [SWS_Com_00872] 段階1（デッドライン監視タイマ再始動）を、段階2
         * （I-PDU callout、下記）や短小フレーム破棄より先に行う。
         * [SWS_Com_00715]: "the AUTOSAR COM module shall reset the
         * reception deadline monitoring timer ... at invocation of the
         * function Com_RxIndication"——つまりリセットは Com_RxIndication()
         * が呼ばれたという事実だけに懸かり、中身の受理可否には懸からない。
         * [SWS_Com_00738]: "shall not take the values of the signals into
         * account"（無効なシグナル/シグナルグループ受信時もタイマは
         * 再始動される、と明記）。以前はこの後段の callout/短小フレーム
         * 破棄より後ろでリセットしていたが、/code-review で「実 AUTOSAR
         * ならタイムアウトしない状況でタイムアウトしてしまう」と指摘され
         * 是正した（詳細は docs/modules/Com_Notes.md 参照）。この是正は
         * 副次的に2つの挙動変化を伴う（いずれも上記2つの SWS 要求に
         * 沿った、意図した変化——回帰テスト参照）:
         *   (1) 下記 [SWS_Com_00575] 短小フレーム破棄（Signal Group）も
         *       今後はタイマをリセットする（従来は破棄側が先に return する
         *       ため素通りしていた）。
         *   (2) reject/破棄された受信であっても Com_RxUsingFirstTimeout は
         *       steady 状態へ遷移する（初回受信の定義がバッファ格納の成否
         *       ではなく「Com_RxIndication() が呼ばれたこと」であるため）。
         * バッファの更新はここでは行わない（＝「受信の事実」と「値の反映」
         * は別軸）。 */
        Com_RxLastMs[ipdu->IPduId]  = millis();
        Com_RxTimedOut[ipdu->IPduId] = 0U;
        Com_RxUsingFirstTimeout[ipdu->IPduId] = 0U;

        /* [SWS_Com_00700]/[SWS_Com_00816] (Com_RxIpduCallout): バッファ書き込み・
         * RxAckCbk/RxIndicationCbk のいずれよりも前に、PduR から渡された
         * 生バイト列をそのまま渡して呼ぶ。FALSE を返したら、
         * この受信は以降一切処理しない（デッドライン監視タイマは上で
         * 既にリセット済みのため、これ以降の reject では巻き戻さない）。 */
        if (ipdu->RxIpduCalloutCbk != NULL &&
            !ipdu->RxIpduCalloutCbk(PduInfoPtr->SduDataPtr, (uint8)PduInfoPtr->SduLength))
        {
            /* 具体的な拒否理由は RxIpduCalloutCbk 自身が WARN で既に出力
             * 済みのはず（Rte_COMRxIpduCallout_SecureCommand 等）。ここでは
             * 二重の WARN ログを避け、フレームワーク側の通過点としてのみ
             * DEBUG で記録する（Com_RxEnabled==0 等、他の「抑制」経路と
             * 同じ扱い）。 */
            DET_LOGD(TAG, "RX iPdu=%u rejected by RxIpduCallout", (unsigned)ipdu->IPduId);
            return;
        }

        /* [SWS_Com_00575]: Signal Group は受信できたバイト数が DLC に満たない
         * 場合、グループ全体（含まれる全メンバー）を丸ごと不採用にする。
         * バッファは更新しない。デッドライン監視タイマは上で既にリセット
         * 済み（[SWS_Com_00738]: 無効なシグナル/シグナルグループ受信時も
         * タイマは再始動される、という要求どおり）。これは「一部の
         * メンバーだけ新しい値、残りは古い値」という一貫性のない
         * スナップショットを公開しないための要求であり、非 Signal Group の
         * 部分受理（下記）とは意図的に異なる扱いとなる。 */
        if (ipdu->IsSignalGroup != 0U && PduInfoPtr->SduLength < ipdu->DLC)
        {
            DET_LOGW(TAG, "RX iPdu=%u(group) length mismatch got=%u exp=%u -> discarded",
                     (unsigned)ipdu->IPduId, (unsigned)PduInfoPtr->SduLength,
                     (unsigned)ipdu->DLC);
            return;
        }

        /* [SWS_Com_00574]/[SWS_Com_00870]: 非 Signal Group の I-PDU は、
         * 実際に受信できたバイト数（recvLen）分だけバッファを更新する。
         * DLC 未満の場合、recvLen 以降のバイトには触れない（＝前回受信値の
         * まま据え置かれる）ことで、新旧データの混在を「シグナル単位の
         * 部分受理」として意図的に許容する（recvLen に収まらないシグナルの
         * 扱いは下記 Com_SigTimedOut ループ参照）。DLC を超える分（CAN
         * フレームの末尾パディング等）はこれまでどおり読み捨てる。 */
        const uint8 recvLen = (PduInfoPtr->SduLength < (PduLengthType)ipdu->DLC)
                              ? (uint8)PduInfoPtr->SduLength
                              : ipdu->DLC;
        if (recvLen < ipdu->DLC)
        {
            DET_LOGW(TAG, "RX iPdu=%u partial length got=%u exp=%u -> signals within range only",
                     (unsigned)ipdu->IPduId, (unsigned)PduInfoPtr->SduLength,
                     (unsigned)ipdu->DLC);
        }

        /* Com は E2E 等のペイロード内容には一切関知せず、無条件にバッファを
         * 更新する（E2E Transformer 方式。Com_Types.h の RxIndicationCbk
         * 説明参照）。デッドライン監視タイマは本関数の冒頭で既にリセット
         * 済み（[SWS_Com_00872] 段階1、上記参照）。ペイロードの妥当性検証・
         * 破棄判断はすべて RxIndicationCbk 側（例: RTE 経由の
         * E2EXf_InverseTransform）の責務であり、Com はそれがあることすら
         * 知らない。E2E 保護された I-PDU では、部分受信で新旧バイトが
         * 混在した内容はほぼ確実に CRC 不一致となり、E2EXf 側で別途
         * 棄却される。Com 層の部分受理（本要求）と E2E 層の整合性検証は
         * 独立した層であり、両者が別々の役割を担う構造は実 AUTOSAR と
         * 同じである。 */
        for (uint8 b = 0; b < recvLen; b++)
            Com_RxBuffer[ipdu->IPduId][b] = PduInfoPtr->SduDataPtr[b];

        /* Com_CbkRxAck（SWS_Com_00555）: Signal Group はメンバー単位ではなく
         * グループ単位で 1 回だけ呼ぶ（詳細は docs/modules/Com_Notes.md 参照）。
         * 短フレーム破棄（[SWS_Com_00575]、上の return）を通過していれば
         * 必ずグループ全体が格納済みのため、ここで無条件に呼んでよい。 */
        if (ipdu->IsSignalGroup != 0U && ipdu->RxAckCbk != NULL)
            ipdu->RxAckCbk();

        /* シグナル単位のデッドライン監視（非 Signal Group のみ意味を持つ）は、
         * このシグナルの全ビット範囲が recvLen バイト以内に収まっている
         * 場合のみリセットする（[SWS_Com_00574]: 完全に受信できたシグナルの
         * みを「受信した」とみなす）。範囲外のシグナルには新しいデータが
         * 届いていないので、前回のタイムアウト状態のまま据え置く（Signal
         * Group メンバーは Com_SigTimedOut を参照しないため、ここで一律に
         * 回しても実害はない）。Com_CbkRxAck（SWS_Com_00555、非 Signal Group
         * のみ）も「recvLen 以内に収まっているか」という全く同じ判定を使う
         * ため、同じループ内でまとめて処理する（2026-08 の /code-review で、
         * 別関数に分離した初期実装がこの判定を重複計算していたと指摘され、
         * 統合した）。 */
        for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
        {
            const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
            if (sig->Direction != COM_SIGNAL_DIRECTION_RX || sig->IPduId != ipdu->IPduId)
                continue;

            const uint8 lastByte = (uint8)((sig->BitPosition + sig->BitSize + 7U) / 8U);
            if (lastByte <= recvLen)
            {
                Com_SigTimedOut[s] = 0U;
                if (ipdu->IsSignalGroup == 0U && sig->RxAckCbk != NULL)
                    sig->RxAckCbk();
            }
        }

        char hexbuf[25];
        Log_HexStr(hexbuf, sizeof(hexbuf), Com_RxBuffer[ipdu->IPduId], ipdu->DLC);
        DET_LOGI(TAG, "RX iPdu=%u [%s]", (unsigned)ipdu->IPduId, hexbuf);

        /* フレーム受信の都度呼ばれる汎用フック（E2E Transformer 等）。
         * バッファ更新後・return 前に呼ぶことで、上位層が最新データを
         * 参照できる状態にしてから通知する。 */
        if (ipdu->RxIndicationCbk != NULL)
            ipdu->RxIndicationCbk();

        /* Signal Gateway（[SWS_Com_00872]: I-PDU callout の次の処理段階）。
         * SWC/Rte を一切介さず、この I-PDU に紐づくゲートウェイ設定があれば
         * 直接 TX シグナルへ転送する。 */
        Com_GatewayRoute(ipdu->IPduId);

        return;
    }

    DET_LOGW(TAG, "RX no iPdu src=%u", (unsigned)RxPduId);
}

/* -----------------------------------------------------------------------
 * 内部ヘルパー — AUTOSAR COM 公開 API の範囲外
 * ----------------------------------------------------------------------- */

/**
 * \brief   ネットワークビット順でバイトバッファからビットフィールドを取り出す。
 *
 * \details ビット番号の定義: bit 0 = byte[0] の MSB、bit 7 = byte[0] の LSB、
 *          bit 8 = byte[1] の MSB、...（ネットワーク / Motorola 順）。
 *          COM_BIG_ENDIAN では最初に読んだビットが結果の MSB になり、
 *          COM_LITTLE_ENDIAN では最初に読んだビットが LSB になる。
 *
 * \param[in]  buf      読み取り元バイトバッファ。
 * \param[in]  bitPos   開始ビット位置（ネットワークビット順）。
 * \param[in]  bitSize  取り出すビット数（1〜32）。
 * \param[in]  endian   ビット重みの方向 (COM_BIG_ENDIAN / COM_LITTLE_ENDIAN)。
 *
 * \return  アンパックしたシグナル値（uint32）。
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static uint32 Com_UnpackSignal(const uint8* buf,
                                uint8 bitPos,
                                uint8 bitSize,
                                Com_SignalEndianType endian)
{
    DET_LOGT(TAG, "called");
    uint32 value = 0U;
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 pos = bitPos + i;
        const uint8 bit = (buf[pos / 8U] >> (7U - (pos % 8U))) & 1U;
        if (endian == COM_BIG_ENDIAN)
            value = (value << 1U) | bit;
        else
            value |= ((uint32)bit << i);
    }
    return value;
}

/**
 * \brief   ネットワークビット順でバイトバッファのビットフィールドに値を書き込む。
 *
 * \details Com_UnpackSignal() と同じネットワークビット番号定義に従い、
 *          bitPos から bitSize ビット分の value を buf へ書き込む。
 *          対象ビット以外の buf の内容は保持される。
 *
 * \param[in,out] buf      書き込み先バイトバッファ。
 * \param[in]     bitPos   開始ビット位置（ネットワークビット順）。
 * \param[in]     bitSize  書き込むビット数（1〜32）。
 * \param[in]     endian   ビット重みの方向 (COM_BIG_ENDIAN / COM_LITTLE_ENDIAN)。
 * \param[in]     value    パックするシグナル値。下位 bitSize ビットのみ使用する。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_PackSignal(uint8* buf,
                            uint8 bitPos,
                            uint8 bitSize,
                            Com_SignalEndianType endian,
                            uint32 value)
{
    DET_LOGT(TAG, "called");
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 bit   = (endian == COM_BIG_ENDIAN)
                            ? (uint8)((value >> (bitSize - 1U - i)) & 1U)
                            : (uint8)((value >> i) & 1U);
        const uint8 pos   = bitPos + i;
        const uint8 shift = 7U - (pos % 8U);
        if (bit)
            buf[pos / 8U] |=  (uint8)(1U << shift);
        else
            buf[pos / 8U] &= (uint8)~(1U << shift);
    }
}

/**
 * \brief   value の下位 byteCount バイトを、リトルエンディアンで dataPtr へ書き出す。
 *
 * \details Com_ReceiveSignal() が呼び出し元の SignalDataPtr（BitSize に応じた
 *          uint8/uint16/uint32 変数）へ値を返す際の共通処理。常に 4 バイト
 *          書き込むと 8bit/16bit の呼び出し元でスタック上の隣接領域を
 *          破壊するため、byteCount 分だけを書き込む。
 *
 * \param[out] dataPtr    書き込み先。byteCount バイト以上必要。
 * \param[in]  byteCount  書き込むバイト数（1〜4）。
 * \param[in]  value      書き込む値。
 */
static void Com_WriteSignalBytes(uint8* dataPtr, uint8 byteCount, uint32 value)
{
    DET_LOGT(TAG, "called");
    for (uint8 b = 0U; b < byteCount; b++)
        dataPtr[b] = (uint8)(value >> (8U * b));
}

/**
 * \brief   指定 I-PDU バッファへ、所属する全シグナルの ComSignalInitValue を
 *          ビット単位でパックする。
 *
 * \details [SWS_Com_00217]/[SWS_Com_00222] 項目1・2: I-PDU のデータ初期化は
 *          まずバイト単位でゼロクリアし（ComTxIPduUnusedAreasDefault 相当、
 *          本実装は常に 0）、その後ビット単位で各シグナルの InitValue を
 *          上書きする、という 2 段階の手順で行う。本関数は後段（ビット単位
 *          の上書き）のみを担う。呼び出し元が先にバイト単位のゼロクリアを
 *          済ませておくこと。RX/TX 両方の I-PDU バッファ・シャドウバッファ
 *          初期化（Com_Init()/Com_IpduGroupStart()）で共用する。
 *
 * \param[in,out] buf  初期化対象のバッファ（Com_RxBuffer[id] 等）。
 *                      COM_IPDU_MAX_DLC バイト以上必要。
 * \param[in]     id   対象 I-PDU の ID（dir 側の値空間、Com_SignalConfigType
 *                      の IPduId と同じ規約）。
 * \param[in]     dir  対象シグナルの方向（RX/TX）。この I-PDU 自体の
 *                      RX/TX は呼び出し元が Com_RxBuffer/Com_TxBuffer の
 *                      どちらを渡すかで決まるため、ここでは対象シグナルの
 *                      絞り込みにのみ使う。
 *
 * \pre        Com_ConfigPtr が NULL でないこと。
 *
 * \ServiceID      {0xF6}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_PackInitValues(uint8* buf, Com_IPduIdType id, Com_SignalDirectionType dir)
{
    DET_LOGT(TAG, "called");
    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->Direction == dir && sig->IPduId == id)
        {
            Com_PackSignal(buf, sig->BitPosition, sig->BitSize, sig->Endian, sig->InitValue);
        }
    }
}

/**
 * \brief   I-PDU バッファ 1 本を [SWS_Com_00217]/[SWS_Com_00222] 項目1・2の
 *          2 段階手順（バイト単位ゼロクリア → Com_PackInitValues()）で
 *          初期値へリセットする。
 *
 * \details Com_IpduGroupStart() が RX/TX バッファ・シャドウバッファの
 *          計 4 箇所で共通して行う手順をまとめたもの。
 *
 * \param[in,out] buf  初期化対象のバッファ（Com_RxBuffer[id] 等）。
 *                      COM_IPDU_MAX_DLC バイト以上必要。
 * \param[in]     id   対象 I-PDU の ID。
 * \param[in]     dir  対象シグナルの方向（RX/TX）。
 *
 * \pre        Com_ConfigPtr が NULL でないこと。
 */
static void Com_ResetBufferToInitValues(uint8* buf, Com_IPduIdType id, Com_SignalDirectionType dir)
{
    DET_LOGT(TAG, "called");
    for (uint8 b = 0U; b < COM_IPDU_MAX_DLC; b++)
        buf[b] = 0U;
    Com_PackInitValues(buf, id, dir);
}

/**
 * \brief   Signal Gateway: RX I-PDU の受信を機に、紐づく TX シグナルへ値を転送する。
 *
 * \details Com_RxIndication() が RX バッファを更新した直後に呼ばれる。
 *          `Com_ConfigPtr->GwMappings[]` を線形検索し、`SrcSignalId` が
 *          rxIPduId に属するエントリごとに、RX バッファから生値を直接
 *          アンパックして `Com_SendSignal(DestSignalId, ...)` を呼ぶ
 *          （[SWS_Com_00357]/[SWS_Com_00377]）。Com_ReceiveSignal() を経由
 *          しないため、ComRxDataTimeoutAction・ComDataInvalidAction・
 *          ComFilterAlgorithm(NEW_IS_WITHIN) はいずれも評価しない
 *          （[SWS_Com_00872] の RX 側処理段階に、これらは含まれていない）。
 *          転送先の実際の送信要否・タイミング判定は Com_SendSignal() 自身が
 *          行う（SWC が直接呼ぶ場合と全く同じ経路。7.2.5 節 "the signal
 *          processing does not differ ..." のとおり）。
 *
 * \param[in]  rxIPduId  受信した RX I-PDU の ID（Com_IPduConfigType.IPduId）。
 *
 * \pre        Com_ConfigPtr が NULL でないこと（Com_RxIndication() が保証する）。
 * \pre        Com_RxBuffer[rxIPduId] が最新の受信データで更新済みであること。
 *
 * \AUTOSARReq     {SWS_Com_00357, SWS_Com_00360, SWS_Com_00377, SWS_Com_00701}
 * \ServiceID      {0xF5}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_GatewayRoute(Com_IPduIdType rxIPduId)
{
    DET_LOGT(TAG, "called");
    for (uint8 g = 0U; g < Com_ConfigPtr->GwMappingCount; g++)
    {
        const Com_GwMappingType* gw = &Com_ConfigPtr->GwMappings[g];

        /* ゲートウェイ元シグナルの設定を検索し、rxIPduId に属するかを確認する
         * （SrcSignalId 自体は RX/TX 共通のシグナル ID 空間の値のため、
         * Direction も確認して RX シグナルであることを保証する。
         * Com_SignalDirectionType の宣言コメント参照）。 */
        const Com_SignalConfigType* srcSig = NULL;
        for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
        {
            const Com_SignalConfigType* cand = &Com_ConfigPtr->Signals[s];
            if (cand->SignalId == gw->SrcSignalId && cand->Direction == COM_SIGNAL_DIRECTION_RX)
            {
                srcSig = cand;
                break;
            }
        }
        if (srcSig == NULL || srcSig->IPduId != rxIPduId)
            continue;

        /* [SWS_Com_00360]: エンディアン変換はアンパック（Src の Endian）と
         * パック（Com_SendSignal() 内、Dest の Endian）をそれぞれ独立に
         * 行うだけで自然に達成される（本実装は元々シグナルごとに Endian を
         * 個別設定できる設計のため、ゲートウェイ専用の変換処理は不要）。 */
        const uint32 value = Com_UnpackSignal(
            Com_RxBuffer[rxIPduId], srcSig->BitPosition, srcSig->BitSize, srcSig->Endian);

        DET_LOGI(TAG, "Gateway src=%u -> dst=%u value=%lu",
                 (unsigned)gw->SrcSignalId, (unsigned)gw->DestSignalId, (unsigned long)value);

        /* SWC が Com_SendSignal() を直接呼ぶのと全く同じ経路（7.2.5 節）。
         * value は uint32 のローカル変数のため、その先頭アドレスを渡せば
         * Com_SendSignal() 内部が DestSignalId の BitSize に応じて必要な
         * バイト数だけリトルエンディアンで読み取る（既存の呼び出し規約と同じ）。 */
        (void)Com_SendSignal(gw->DestSignalId, &value);
    }
}

/**
 * \brief   TX I-PDU 設定テーブルから IPduId に一致するエントリを検索する。
 *
 * \details Com_SendSignal() / Com_SendSignalGroup() が、シグナルの所属する
 *          I-PDU が Signal Group（IsSignalGroup=1）かどうかを判定するために使う。
 *
 * \param[in]  IPduId  検索する TX I-PDU の ID。
 *
 * \return  一致するエントリへのポインタ。見つからない場合は NULL。
 *
 * \pre        Com_ConfigPtr が NULL でないこと（呼び出し元で保証する）。
 *
 * \ServiceID      {0xF2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static const Com_IPduConfigType* Com_FindTxIPdu(Com_IPduIdType IPduId)
{
    DET_LOGT(TAG, "called");
    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        if (Com_ConfigPtr->TxIPdus[i].IPduId == IPduId)
            return &Com_ConfigPtr->TxIPdus[i];
    }
    return NULL;
}

/**
 * \brief   RX I-PDU 設定テーブルから IPduId に一致するエントリを検索する。
 *
 * \details Com_ReceiveSignal() / Com_ReceiveSignalGroup() が、シグナルの
 *          所属する I-PDU が RX Signal Group（IsSignalGroup=1）かどうかを
 *          判定するために使う（Com_FindTxIPdu() の RX 側対称）。
 *
 * \param[in]  IPduId  検索する RX I-PDU の ID。
 *
 * \return  一致するエントリへのポインタ。見つからない場合は NULL。
 *
 * \pre        Com_ConfigPtr が NULL でないこと（呼び出し元で保証する）。
 *
 * \ServiceID      {0xF4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static const Com_IPduConfigType* Com_FindRxIPdu(Com_IPduIdType IPduId)
{
    DET_LOGT(TAG, "called");
    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        if (Com_ConfigPtr->RxIPdus[i].IPduId == IPduId)
            return &Com_ConfigPtr->RxIPdus[i];
    }
    return NULL;
}

/**
 * \brief   シグナル設定テーブルから SignalId に一致するエントリの添字を検索する。
 *
 * \details Com_ReceiveSignal() / Com_SendSignal() が共通で使う、
 *          Signals[] を SignalId で線形探索する処理をまとめたもの。
 *          見つかった後の処理が Com_RxLastValidValue[s]/Com_FilterLastValue[s]
 *          等、添字 s を要する並行配列を参照するため、ポインタではなく
 *          添字を返す。
 *
 * \param[in]  SignalId  検索するシグナル ID。
 *
 * \return  一致するエントリの添字。見つからない場合は Com_ConfigPtr->SignalCount
 *          （＝配列の範囲外を示す番兵値）。
 *
 * \pre        Com_ConfigPtr が NULL でないこと（呼び出し元で保証する）。
 */
static uint8 Com_FindSignalIndex(Com_SignalIdType SignalId)
{
    DET_LOGT(TAG, "called");
    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        if (Com_ConfigPtr->Signals[s].SignalId == SignalId)
            return s;
    }
    return Com_ConfigPtr->SignalCount;
}

/**
 * \brief   TX I-PDU バッファを実際に PduR_ComTransmit() へ渡す共通処理。
 *
 * \details TxTransformCbk が設定されていれば送信直前に呼び出し（E2E
 *          Transformer 等、送信直前の最終変換用の汎用フック。Com はここで
 *          何が実行されるか一切関知しない）、その後 TX バッファの内容を
 *          ログ出力して PduR_ComTransmit() を呼ぶ（PduR→CanIf→Can_Write と
 *          MCP2515 への SPI 送信までブロッキングで完了する）。
 *          `Com_MainFunctionTx()` からのみ呼ばれる。DIRECT/MIXED I-PDU の
 *          イベント駆動送信であっても実送信は必ず `Com_MainFunctionTx()`
 *          （Os の 100ms タスク）側で行う設計とし、WdgM の Deadline
 *          Supervision 対象である ASW Runnable（`App_EngineManager_Run()`
 *          等）のスタックフレーム内で SPI 送信がブロッキングしないようにする
 *          （バス輻輳時に `sendMsgBuf()` の TX バッファ空き待ちが伸びても、
 *          Runnable 自体の実行時間には影響しない）。
 *          「送信すべきかどうかの判断」は呼び出し元（`Com_MainFunctionTx()`）が
 *          既に済ませてから呼ぶ。
 *
 *          update-bit クリア（ipdu->UpdateBitPosition が 0xFF 以外の場合、
 *          SWS_Com_00062: ComTxIPduClearUpdateBit=Transmit 相当）: PduR_ComTransmit()
 *          が E_OK を返した場合のみクリアする（SWS_Com_00062 原文 "after this
 *          I-PDU was sent out via PduR_ComTransmit and PduR_ComTransmit
 *          returned E_OK" のとおり）。失敗時はクリアせず、次回の再送で
 *          update-bit ごと正しく伝わるようにする。
 *
 *          TxIpduCalloutCbk（[SWS_Com_00346]/[SWS_Com_00719]）: TxTransformCbk
 *          適用後・PduR_ComTransmit() 呼び出し直前に、実際に送信される最終
 *          バイト列を渡して呼ぶ。戻り値 0（false）ならこの送信は行わず
 *          即座に E_NOT_OK を返す。「実際には PduR へ渡していない」ため、
 *          PduR_ComTransmit() 自体が失敗した場合と同様に Com_TxConfPending は
 *          セットしない・update-bit もクリアしない（詳細は下記コメント・
 *          Com_Types.h の TxIpduCalloutCbk 参照）。
 *
 * \param[in]  ipdu  送信する TX I-PDU 設定。NULL 禁止（呼び出し元で保証する）。
 * \param[in]  now   Com_MainFunctionTx() が計算済みの現在時刻 [ms]（millis()
 *                   を再度呼ばず再利用する。TX 送信デッドライン監視の
 *                   アーム時刻記録に使う）。
 *
 * \retval  E_OK      PduR_ComTransmit() が成功した。
 * \retval  E_NOT_OK  PduR_ComTransmit() が失敗した、または TxIpduCalloutCbk が
 *                     送信を拒否した（この場合 PduR_ComTransmit() 自体を呼ばない）。
 *
 * \AUTOSARReq     {SWS_Com_00062, SWS_Com_00878, SWS_Com_00346, SWS_Com_00719,
 *                   SWS_Com_00381}
 * \ServiceID      {0xF3}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static Std_ReturnType Com_DoTransmit(const Com_IPduConfigType* ipdu, unsigned long now)
{
    if (ipdu->TxTransformCbk != NULL)
        ipdu->TxTransformCbk(Com_TxBuffer[ipdu->IPduId], ipdu->DLC);

    if (ipdu->TxIpduCalloutCbk != NULL &&
        !ipdu->TxIpduCalloutCbk(Com_TxBuffer[ipdu->IPduId], ipdu->DLC))
    {
        /* [SWS_Com_00346] false: 送信そのものを行わない。具体的な拒否理由は
         * TxIpduCalloutCbk 自身が WARN で既に出力している想定のため、ここは
         * DET_LOGD に留める（RxIpduCalloutCbk と同じ二重ログ回避の方針）。 */
        DET_LOGD(TAG, "TX iPdu=%u rejected by TxIpduCallout", (unsigned)ipdu->IPduId);
        return E_NOT_OK;
    }

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), Com_TxBuffer[ipdu->IPduId], ipdu->DLC);
    DET_LOGI(TAG, "TX iPdu=%u [%s]", (unsigned)ipdu->IPduId, hexbuf);

    PduInfoType pduInfo = {
        .SduDataPtr = Com_TxBuffer[ipdu->IPduId],
        .SduLength  = ipdu->DLC
    };
    const Std_ReturnType ret = PduR_ComTransmit(ipdu->PduRId, &pduInfo);

    /* [SWS_Com_00479]/[SWS_Com_00491]: PduR への引き渡しが成功した時点で
     * 「送信済み・未確認」とマークする。対応する Com_TxConfirmation() が
     * 届くまでの間に Com_IpduGroupStop() が呼ばれたら TxErrCbk の対象となる
     * （詳細は Com_TxConfPending[] の宣言コメント参照）。
     * [SWS_Com_00878] "unless already running": TX 送信デッドライン監視の
     * アーム時刻は 0→1 遷移の瞬間のみ記録する。MIXED 周期フロアや再送
     * （NumberOfRepetitions）による同一 I-PDU の重複ディスパッチ（既に
     * Com_TxConfPending==1）はタイマを延命しない。 */
    if (ret == E_OK)
    {
        if (Com_TxConfPending[ipdu->IPduId] == 0U)
            Com_TxConfPendingSinceMs[ipdu->IPduId] = now;
        Com_TxConfPending[ipdu->IPduId] = 1U;
    }

    /* update-bit クリア（SWS_Com_00062: ComTxIPduClearUpdateBit=Transmit 相当。
     * Confirmation/TriggerTransmit の 2 択は未実装）。原文は "after this I-PDU
     * was sent out via PduR_ComTransmit and PduR_ComTransmit returned E_OK"
     * であり、ret==E_OK のときのみクリアする（ret を無視して無条件にクリア
     * していた過去の実装は誤り。失敗時にもクリアすると、update-bit だけが
     * 消えてバッファのデータは残ったまま次回再送されるため、実際には
     * 初めて正常配信される新データが受信側に「未更新」として誤って破棄
     * されうる）。PduR_ComTransmit() はこの呼び出し内で同期的に SPI 送信まで
     * 完了しているため、この時点で Com_TxBuffer を書き換えても既に送信済み
     * のバイト列には影響しない。
     * Signal Group（SWS_Com_00801）・非 Signal Group（SWS_Com_00061、
     * Com_SendSignal() 参照）いずれの update-bit も、クリア自体は本関数で
     * 同じ処理を行う（SWS_Com_00062 はどちらの場合も区別しない）。
     * `UpdateBitPosition != 0xFFU` の判定のみで十分であり IsSignalGroup は
     * 見ない。ただしこれは「update-bit を使わない I-PDU は必ず
     * `.UpdateBitPosition = 0xFFU` を明示設定する」という Com_PBCfg.c 側の
     * 規約が守られていることが前提（C の既定初期化 0 のまま放置すると、
     * その I-PDU のバッファ bit0 を毎回誤ってクリアし、シグナル値を破壊する）。 */
    if (ret == E_OK && ipdu->UpdateBitPosition != 0xFFU)
        Com_PackSignal(Com_TxBuffer[ipdu->IPduId], ipdu->UpdateBitPosition, 1U, COM_BIG_ENDIAN, 0U);

    return ret;
}

/**
 * \brief   TMS（Transmission Mode Selector）評価に基づく実効 TxModeMode を返す。
 *
 * \details `Com_TmsState[]` が true なら `TxModeModeTrue`、false なら
 *          `TxModeMode` を返す（SWS_Com_00032/00799）。TMS を持たない
 *          （TmsContributor なシグナルが存在せず、Com_TmsState が常に 0 の）
 *          I-PDU では常に `TxModeMode` を返すため、既存の単一モード I-PDU の
 *          挙動に影響しない。
 *
 * \param[in]  ipdu  対象 TX I-PDU 設定。NULL 禁止。
 *
 * \return  現在有効な Com_TxModeModeType。
 *
 * \ServiceID      {0x1C}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static Com_TxModeModeType Com_EffectiveTxModeMode(const Com_IPduConfigType* ipdu)
{
    DET_LOGT(TAG, "called");
    return Com_TmsState[ipdu->IPduId] ? ipdu->TxModeModeTrue : ipdu->TxModeMode;
}

/**
 * \brief   TMS 評価に基づく実効 TxPeriodMs を返す。
 *
 * \details Com_EffectiveTxModeMode() と対になる周期値のペア選択。
 *
 * \param[in]  ipdu  対象 TX I-PDU 設定。NULL 禁止。
 *
 * \return  現在有効な TxPeriodMs [ms]。
 *
 * \ServiceID      {0x1D}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static uint16 Com_EffectiveTxPeriodMs(const Com_IPduConfigType* ipdu)
{
    DET_LOGT(TAG, "called");
    return Com_TmsState[ipdu->IPduId] ? ipdu->TxPeriodMsTrue : ipdu->TxPeriodMs;
}

/**
 * \brief   ComTxModeNumberOfRepetitions（SWS_Com_00305）が現在の実効モードで
 *          適用対象かどうかを返す。
 *
 * \details TxModeMode==DIRECT の I-PDU のみを対象とする。MIXED の周期フロアや
 *          TMS との相互作用を避けるための設計上の制約であり、
 *          Com_RequestTxOnChange()（残り回数のセット/クリア）と
 *          Com_MainFunctionTx()（repeatDue 判定）の両方から同一の predicate を
 *          呼ぶことで、判定条件が2箇所で食い違わないようにする（詳細は
 *          docs/modules/Com_Notes.md 参照）。
 *
 * \param[in]  mode  Com_EffectiveTxModeMode() が返した実効 TxModeMode。
 *
 * \return  1 = 対象（DIRECT）、0 = 対象外。
 *
 * \ServiceID      {0x1F}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static uint8 Com_TxRepeatApplicable(Com_TxModeModeType mode)
{
    return (mode == COM_TX_MODE_DIRECT) ? 1U : 0U;
}

/**
 * \brief   デッドライン監視の「初回猶予期間か定常状態か」に応じて閾値を選ぶ。
 *
 * \details RX I-PDU 単位・RX シグナル単位・TX（Com_CbkTxTOut）の3箇所が
 *          同じ形の判定（`usingFirst ? firstMs : steadyMs`）を必要とするため
 *          共通化した（/code-review で重複を指摘）。呼び出し元ごとに対象と
 *          なる配列・フィールドが異なる（Com_RxUsingFirstTimeout[]/
 *          Com_TxUsingFirstTimeout[]、FirstTimeoutMs/TxFirstTimeoutMs 等）
 *          ため、値だけを受け取る薄いヘルパーとする。
 *
 * \param[in]  usingFirst  1 = 初回猶予期間中（firstMs を使う）。
 * \param[in]  firstMs     初回猶予期間の閾値 [ms]。
 * \param[in]  steadyMs    定常状態の閾値 [ms]。
 *
 * \return  適用すべき閾値 [ms]。
 *
 * \ServiceID      {0x21}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static uint16 Com_SelectTimeoutThreshold(uint8 usingFirst, uint16 firstMs, uint16 steadyMs)
{
    return usingFirst ? firstMs : steadyMs;
}

/**
 * \brief   TMS（Transmission Mode Selector）を再評価する。
 *
 * \details 指定 I-PDU に属するシグナルのうち `TmsContributor=1` のものについて、
 *          `Com_TxBuffer[ipduId]` から現在値をアンパックし、
 *          `(値 & Mask) != FilterX` を TMC（Transmission Mode Condition）として
 *          評価する。1 つでも真なら TMS = true（SWS_Com_00678）、
 *          寄与するシグナルは存在するがどれも偽なら TMS = false
 *          （SWS_Com_00679）。
 *
 *          仕様との既知の相違点（意図的、未修正）: 寄与するシグナルが
 *          1 つも無い I-PDU について、仕様は TMS = true と規定する
 *          （SWS_Com_00677）が、本実装は false のまま（tmsTrue の初期値
 *          0 が変化しない）とする。これは、本プロジェクトの `Com_PBCfg.c`
 *          が「TmsContributor を持たない I-PDU では TxModeModeTrue/
 *          TxPeriodMsTrue を設定しない（0 のまま）」という前提で
 *          `TxModeMode`/`TxPeriodMs` 側のみを実際の意図した値に設定して
 *          いるため（例: E2EHealthStatus は PERIODIC、ImmobilizerStatus は
 *          DIRECT）、仕様どおり TMS=true にすると `Com_EffectiveTxModeMode()`/
 *          `Com_EffectiveTxPeriodMs()` が未設定（0 = MIXED/0ms）の True 側
 *          フィールドを返してしまい、実際に意図した送信モードが壊れる。
 *          TMS の True/False 切り替えを実際に使う I-PDU
 *          （WarningStatus、FaultLamp/AbsLamp が TmsContributor）は
 *          TxModeModeTrue/TxPeriodMsTrue を明示的に設定済みのため、この
 *          相違の影響を受けない。結果を `Com_TmsState[ipduId]` へ保存する。
 *
 *          Com_SendSignal()（Signal Group でない場合）と
 *          Com_SendSignalGroup() の確定コミット後、いずれも実バッファへの
 *          反映が完了した時点で呼ぶこと（SWS_Com_00245: 値の更新のたびに
 *          TMS を再計算する）。
 *
 *          戻り値は「この呼び出しで Com_TmsState[ipduId] が変化したか」。
 *          SWS_Com_00495（TMS の遷移によってモードが切り替わったら、その
 *          変化を起こしたシグナルの ComTransferProperty によらず無条件に
 *          即座に送信しなければならない）を呼び出し元が実装するために使う。
 *
 * \param[in]  ipduId  再評価する TX I-PDU の ID。
 *
 * \retval  1  Com_TmsState[ipduId] が今回の呼び出しで変化した（true⇔false）。
 * \retval  0  変化しなかった。
 *
 * \pre        Com_ConfigPtr が NULL でないこと（呼び出し元で保証する）。
 * \pre        `Com_TxBuffer[ipduId]` が最新値へ更新済みであること。
 *
 * \AUTOSARReq     {SWS_Com_00245, SWS_Com_00495, SWS_Com_00676, SWS_Com_00677,
 *                  SWS_Com_00678, SWS_Com_00679}
 * \ServiceID      {0x1E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static uint8 Com_RecalcTms(Com_IPduIdType ipduId)
{
    DET_LOGT(TAG, "called");
    uint8 tmsTrue = 0U;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->IPduId != ipduId || sig->TmsContributor == 0U)
            continue;

        const uint32 value = Com_UnpackSignal(Com_TxBuffer[ipduId],
                                               sig->BitPosition, sig->BitSize, sig->Endian);
        if ((value & sig->Mask) != sig->FilterX)
            tmsTrue = 1U;
    }

    const uint8 changed = (Com_TmsState[ipduId] != tmsTrue) ? 1U : 0U;
    Com_TmsState[ipduId] = tmsTrue;
    return changed;
}

/**
 * \brief   ComFilterAlgorithm を通過した変化を「次回送信あり」として記録する。
 *
 * \details Com_SendSignal() / Com_SendSignalGroup() が変化を検知した際に
 *          呼ばれる。ここでは `Com_TxPending[]` を立てるだけで、実際の
 *          PduR_ComTransmit() 呼び出し（ひいては MCP2515 への SPI 送信）は一切
 *          行わない（SWS_Com_00734/00742/00743 の要求"shall immediately
 *          (within the next main function at the latest) initiate..." の
 *          うち、「次回メイン関数まで」の猶予を使い、実送信は必ず
 *          `Com_MainFunctionTx()` 側にディスパッチする設計にしている。
 *          呼び出しスタックと同一フレームで SPI 送信までブロッキングすると、
 *          WdgM の Deadline Supervision 対象である ASW Runnable
 *          （App_EngineManager_Run 等）の実行時間がバス輻輳時の SPI 遅延に
 *          左右されてしまうため）。
 *
 *          実効 TxModeMode（`Com_EffectiveTxModeMode()`、TMS 評価済み）が
 *          `COM_TX_MODE_PERIODIC` の I-PDU では何もしない（PERIODIC I-PDU は
 *          Com_MainFunctionTx() の周期タスクのみが送信を担い、値の変化そのものは
 *          送信タイミングに影響しない）。
 *
 *          診断 CommunicationControl (UDS 0x28) による送信抑制中でも
 *          ここではフラグを立てるだけとする（実際に送信を抑制するかどうかの
 *          判断は Com_MainFunctionTx() 側で行う。SWS_Com_00777/SWS_Com_00334
 *          が要求する「停止中に発生した送信要求は保持されず、再開しても
 *          古いトリガーで即座に送信してはならない」は、Com_MainFunctionTx()
 *          が抑制中にこのフラグを見つけ次第、実送信せずに破棄することで
 *          満たす）。
 *
 * \param[in]  ipdu  対象 TX I-PDU 設定。NULL 禁止（呼び出し元で保証する）。
 *
 *          あわせて ComTxModeNumberOfRepetitions（SWS_Com_00305）の残り
 *          再送回数を ipdu->NumberOfRepetitions で無条件上書きする
 *          （[SWS_Com_00279]: 新規送信要求は進行中の再送をキャンセルして
 *          再スタートする）。
 *
 *          \note   本関数は static な内部ヘルパーであり、Det_ReportError() を
 *          直接呼ぶ公開 API ではないため、他の静的ヘルパー（Com_FindSignalIndex
 *          等）と同様に \ServiceID/\Reentrancy/\Synchronicity タグは付与しない
 *          （旧コメントには誤って \ServiceID{0x17} が付いていたが、これは
 *          本来 Com_TriggerIPDUSend の実 Service ID であり、本関数のものでは
 *          ない。2026-08 の Com_TriggerIPDUSend 追加を機に是正した）。
 *
 * \AUTOSARReq     {SWS_Com_00734, SWS_Com_00742, SWS_Com_00743, SWS_Com_00279}
 */
static void Com_RequestTxOnChange(const Com_IPduConfigType* ipdu)
{
    DET_LOGT(TAG, "called");
    const Com_TxModeModeType mode = Com_EffectiveTxModeMode(ipdu);
    if (mode == COM_TX_MODE_PERIODIC)
        return;

    Com_TxPending[ipdu->IPduId] = 1U;
    /* [SWS_Com_00279]: 新規送信要求は進行中の再送シーケンスをキャンセルして
     * 再スタートする（NumberOfRepetitions=0 の I-PDU では no-op）。DIRECT
     * 以外では明示的に 0 へクリアする（Com_TxRepeatApplicable() 参照。MIXED
     * の間の古い残り回数が、後で TMS が DIRECT へ戻った際に不意の再送として
     * 復活するのを防ぐ）。 */
    Com_TxRepeatsRemaining[ipdu->IPduId] = Com_TxRepeatApplicable(mode) ? ipdu->NumberOfRepetitions : 0U;
}

/**
 * \brief   RX I-PDU バッファからシグナル値を取り出す。
 *
 * \details シグナル設定テーブルの SignalId に一致するエントリを検索し、
 *          ビット位置・サイズ・エンディアンに従って内部 RX バッファから
 *          アンパックする。アンパックした値は BitSize にかかわらず、
 *          常に 4 バイトのリトルエンディアン整数として SignalDataPtr へ
 *          書き込む。
 *
 * \param[in]  SignalId      読み取るシグナルの ID。
 *                           シグナル設定テーブルのエントリと一致すること。
 * \param[out] SignalDataPtr 出力バッファへのポインタ。4 バイト以上必要。
 *                           リトルエンディアン uint32 として書き込まれる。
 *                           NULL 禁止。
 *
 * \retval  E_OK      シグナルが見つかり、SignalDataPtr へ値を書き込んだ
 *                    （実データ、当該 I-PDU がタイムアウト中かつ
 *                    RxDataTimeoutAction=SUBSTITUTE の場合は
 *                    TimeoutSubstitutionValue、RxDataTimeoutAction=REPLACE
 *                    または受信値が InvalidValue と一致し
 *                    DataInvalidAction=REPLACE の場合は InitValue、
 *                    受信値が InvalidValue と一致し DataInvalidAction=NOTIFY
 *                    の場合、または FilterAlgorithm=NEW_IS_WITHIN の範囲外の
 *                    場合は直近の合格値）。
 * \retval  E_NOT_OK  COM 未初期化、SignalDataPtr が NULL、
 *                    シグナル設定テーブルに SignalId が存在しない、
 *                    または当該 I-PDU がタイムアウト中かつ
 *                    RxDataTimeoutAction=NONE（既定）。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        このシグナルが属する I-PDU で Com_RxIndication() が
 *             少なくとも 1 回呼ばれていること。
 * \pre        このシグナルが RX Signal Group（所属 I-PDU の IsSignalGroup=1）の
 *             メンバーである場合は、あわせて Com_ReceiveSignalGroup() が
 *             少なくとも 1 回呼ばれていること（呼ばれるまでは初期値 = 安全値の
 *             まま更新されない。Com_ReceiveSignalGroup() 参照）。
 * \note       戻り値型は仕様に従い uint8。E_OK / E_NOT_OK の値（0x00 / 0x01）は
 *             RTE が使う Std_ReturnType と互換性がある。
 *
 * \AUTOSARReq     {SWS_Com_00198, SWS_Com_00500, SWS_Com_00875, SWS_Com_00876,
 *                  SWS_Com_00470, SWS_Com_00680, SWS_Com_00681, SWS_Com_00717,
 *                  SWS_Com_00273, SWS_Com_00303, SWS_Com_00695}
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL, COM_E_UNINIT);
        return E_NOT_OK;
    }
    if (SignalDataPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL, COM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    uint8* dataPtr = (uint8*)SignalDataPtr;

    const uint8 s = Com_FindSignalIndex(SignalId);
    if (s < Com_ConfigPtr->SignalCount)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];

        /* 範囲チェック: Signal 設定テーブルの IPduId をそのまま Com_RxBuffer[]
         * 等の配列添字として使うため、設定ミス（存在しない I-PDU を指す
         * IPduId 等）で範囲外の値が来ると隣接するグローバル変数を破壊する
         * バッファオーバーランになる。MPU のない AVR/Renesas RA では
         * これを検出する手段がハードウェアにないため、ここで明示的に
         * 検査する。 */
        if (sig->IPduId >= COM_RX_IPDU_MAX)
        {
            DET_LOGE(TAG, "ReceiveSignal E: sig=%u IPduId=%u out of range (max=%u)",
                     (unsigned)SignalId, (unsigned)sig->IPduId, (unsigned)COM_RX_IPDU_MAX);
            Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL, COM_E_PARAM);
            return E_NOT_OK;
        }

        const Com_IPduConfigType* ipdu = Com_FindRxIPdu(sig->IPduId);
        if (ipdu == NULL)
        {
            DET_LOGE(TAG, "ReceiveSignal E: sig=%u IPduId=%u not a registered RX I-PDU",
                     (unsigned)SignalId, (unsigned)sig->IPduId);
            Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL, COM_E_PARAM);
            return E_NOT_OK;
        }

        /* RX Signal Group メンバーは Com_ReceiveSignalGroup() が確定コピーした
         * シャドウバッファ・タイムアウトスナップショットを読む（Com_RxBuffer/
         * Com_RxTimedOut を直接見ない）。これにより、同じグループの複数
         * メンバーを読む間に新しいフレームが届いても一貫した値が返る
         * （[7.3.6] "handled like a signal" のとおり、グループはこの
         * I-PDU/グループ単位の判定を使う）。
         * 非 Signal Group のシグナルは、このシグナル自身の
         * FirstTimeoutMs/TimeoutMs に基づく Com_SigTimedOut[]（シグナル単位、
         * Com_MainFunctionRx() 参照）を使う。 */
        const uint8 timedOut = (ipdu->IsSignalGroup != 0U)
                               ? Com_RxShadowTimedOut[sig->IPduId]
                               : Com_SigTimedOut[s];
        const uint8* srcBuf  = (ipdu->IsSignalGroup != 0U)
                               ? Com_RxShadowBuffer[sig->IPduId]
                               : Com_RxBuffer[sig->IPduId];

        /* SignalDataPtr は呼び出し元が BitSize に応じた幅の変数
         * (uint8/uint16/uint32) を渡す。常に 4 バイト書き込むと、
         * 8bit/16bit の呼び出し元ではスタック上の隣接領域を破壊する。
         * BitSize から必要バイト数だけを書き込む。 */
        const uint8 byteCount = (uint8)((sig->BitSize + 7U) / 8U);

        if (timedOut)
        {
            /* ComRxDataTimeoutAction（Com_RxDataTimeoutActionType 参照）:
             * NONE（既定）なら、値を書き込まず E_NOT_OK を返す
             * （呼び出し元の初期値=安全値を使用、既存の既定動作）。
             * SUBSTITUTE なら I-PDU バッファ/シャドウバッファは読まず、
             * 設定済みの TimeoutSubstitutionValue を代わりに書き込んで
             * E_OK を返す（実データが古いまま返ることを防ぐ）。
             * REPLACE なら同様にバッファは読まず、InitValue を書き込んで
             * E_OK を返す（[SWS_Com_00470]）。あわせて Com_RxLastValidValue[s]
             * も InitValue で上書きする（同要求の "the last received value is
             * overwritten and gets lost" のとおり、新しい値を受信するまで
             * InitValue を返し続けさせるため）。 */
            if (sig->RxDataTimeoutAction == COM_RX_TIMEOUT_ACTION_SUBSTITUTE)
            {
                Com_WriteSignalBytes(dataPtr, byteCount, sig->TimeoutSubstitutionValue);
                return E_OK;
            }
            if (sig->RxDataTimeoutAction == COM_RX_TIMEOUT_ACTION_REPLACE)
            {
                Com_RxLastValidValue[s] = sig->InitValue;
                Com_WriteSignalBytes(dataPtr, byteCount, sig->InitValue);
                return E_OK;
            }
            return E_NOT_OK;
        }

        uint32 value = Com_UnpackSignal(
            srcBuf,
            sig->BitPosition, sig->BitSize, sig->Endian);

        /* ComDataInvalidAction（Com_DataInvalidActionType 参照）: 受信値が
         * InvalidValue と一致する場合の振る舞い。
         * NOTIFY: 「シグナルオブジェクトへ格納しない」（SWS_Com_00717）。
         * すなわち Com_RxLastValidValue[s] を更新せず、直近の有効値をそのまま
         * 返す。通知コールバックの実呼び出しはここでは行わず、
         * Com_RxInvalidNotifyPending[s] を立てるだけに留める（Com_MainFunctionRx()
         * へディスパッチする理由は Com_RxInvalidNotifyPending の宣言コメント
         * 参照）。
         * REPLACE: 受信値を InitValue に置き換えたうえで、以降の
         * フィルタ処理・格納処理へそのまま合流させる（[SWS_Com_00681]:
         * "the normal signal processing like filtering and notification
         * shall take place as if the ComSignalInitValue would have been
         * received"。NOTIFY と異なり InvalidNotificationCbk は呼ばない）。 */
        if (value == sig->InvalidValue)
        {
            if (sig->DataInvalidAction == COM_DATA_INVALID_ACTION_NOTIFY)
            {
                Com_RxInvalidNotifyPending[s] = 1U;

                Com_WriteSignalBytes(dataPtr, byteCount, Com_RxLastValidValue[s]);
                return E_OK;
            }
            if (sig->DataInvalidAction == COM_DATA_INVALID_ACTION_REPLACE)
            {
                value = sig->InitValue;
            }
        }

        /* RX ComFilterAlgorithm（Com_FilterAlgorithmType の用途 (3) 参照）:
         * COM_FILTER_NEW_IS_WITHIN の場合、値が [FilterMin, FilterMax] の
         * 範囲外ならフィルタ条件は偽となり、このシグナルを「破棄」する
         * （SWS_Com_00273: 処理しない。SWS_Com_00303: old_value も更新しない）。
         * DataInvalidAction と同じ Com_RxLastValidValue[s] を「直近の合格値」
         * として使い回す（両者は同じ「格納しない」意味論のため、実質的に
         * 同じ状態を指す。1 つのシグナルに両方を設定する構成は想定していない）。
         * FilterRejectCbk の実呼び出しは Com_RxInvalidNotifyPending と同じ理由
         * で次回 Com_MainFunctionRx() まで遅延する。 */
        if (sig->FilterAlgorithm == COM_FILTER_NEW_IS_WITHIN
            && (value < sig->FilterMin || value > sig->FilterMax))
        {
            Com_RxFilterRejectPending[s] = 1U;

            Com_WriteSignalBytes(dataPtr, byteCount, Com_RxLastValidValue[s]);
            return E_OK;
        }

        Com_RxLastValidValue[s] = value;
        Com_WriteSignalBytes(dataPtr, byteCount, value);
        return E_OK;
    }

    DET_LOGE(TAG, "ReceiveSignal E: sig=%u not found", (unsigned)SignalId);
    Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL, COM_E_PARAM);
    return E_NOT_OK;
}

/**
 * \brief   RX Signal Group を I-PDU バッファから RX シャドウバッファへ確定コピーする。
 *
 * \details Com_SendSignalGroup()（TX 側）の対称版。SignalGroupId が RX Signal
 *          Group（IsSignalGroup=1）であれば、Com_RxBuffer[SignalGroupId] の
 *          内容を Com_RxShadowBuffer[SignalGroupId] へバイト単位でコピーし、
 *          あわせてその時点の Com_RxTimedOut[SignalGroupId] を
 *          Com_RxShadowTimedOut[SignalGroupId]
 *          へスナップショットする。以降 Com_ReceiveSignal() は、このグループに
 *          属するシグナルに対してこのスナップショットを読む（次に
 *          Com_ReceiveSignalGroup() が呼ばれるまで更新されない）。
 *
 *          コピー自体は、現在タイムアウト中かどうかに関わらず常に行う
 *          （SWS_Com_00461: I-PDU が停止/タイムアウト中でも既知の最新値を
 *          シャドウバッファへ反映すること、という実 AUTOSAR の要求に合わせた）。
 *          ただし本実装は Com_ReceiveSignal() の非グループ経路と同じ簡略化
 *          （タイムアウト中かどうかを E_OK/E_NOT_OK の 2 値にまとめる）を
 *          踏襲しており、実 AUTOSAR の COM_SERVICE_NOT_AVAILABLE や
 *          ComSignalInitValue によるフォールバックといった細分化は行わない。
 *
 *          ComRxDataTimeoutAction=SUBSTITUTE（Com_RxDataTimeoutActionType 参照）
 *          との関係: このグループのメンバーに対する SUBSTITUTE 判定
 *          （SWS_Com_00876「...when the reception deadline monitoring timer
 *          of a signal group expires」）は、この関数が Com_RxTimedOut[GroupId]
 *          を読むこの瞬間にのみライブに評価される。この呼び出し以降、次に
 *          本関数が呼ばれるまでの間にタイムアウトが新規発生しても、
 *          Com_ReceiveSignal() はこの時点のスナップショット
 *          （Com_RxShadowTimedOut[GroupId]）しか見ないため、SUBSTITUTE は
 *          即座には反映されない。これは呼び出し側の都合ではなく、Signal
 *          Group が「Com_ReceiveSignal() はシャドウバッファのみを読む」
 *          という設計だからである。
 *
 *          update-bit（ipdu->UpdateBitPosition が 0xFF 以外の場合、
 *          SWS_Com_00324/00802）: I-PDU バッファ内のこのビットが 0（未更新）
 *          なら、確定コピー・タイムアウトスナップショット更新のいずれも
 *          行わずに戻る（SWS_Com_00802: "shall discard this signal/ signal
 *          group... It will only be discarded"）。1（更新済み、SWS_Com_00067）
 *          の場合のみ、以下の通常の確定コピー処理を行う。
 *
 * \param[in]  SignalGroupId  確定コピーする RX Signal Group の ID（所属する
 *                            RX I-PDU の ID と同じ、Com_Types.h 参照）。
 *
 * \retval  E_OK      SignalGroupId が見つかり、コピー時点でタイムアウト中で
 *                    なかった（または update-bit=0 のため何もせず破棄した）。
 * \retval  E_NOT_OK  COM 未初期化、SignalGroupId が RX I-PDU 設定テーブルに
 *                    存在しない、IsSignalGroup=0 の I-PDU を指定した、
 *                    またはコピーは行ったがコピー時点でタイムアウト中だった。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Com_00201, SWS_Com_00051, SWS_Com_00638, SWS_Com_00461,
 *                  SWS_Com_00876, SWS_Com_00324, SWS_Com_00802, SWS_Com_00067}
 * \ServiceID      {0x0e}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_ReceiveSignalGroup(Com_SignalGroupIdType SignalGroupId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL_GROUP, COM_E_UNINIT);
        return E_NOT_OK;
    }

    /* 範囲チェック: SignalGroupId をそのまま Com_RxBuffer[] 等の配列添字として
     * 使うため、RX I-PDU 設定テーブル自体に範囲外の IPduId が設定される
     * 事態に備えて明示的に検査する（Com_ReceiveSignal/Com_SendSignalGroup と
     * 同じ方針）。 */
    if (SignalGroupId >= COM_RX_IPDU_MAX)
    {
        DET_LOGE(TAG, "ReceiveSignalGroup E: SignalGroupId=%u out of range (max=%u)",
                 (unsigned)SignalGroupId, (unsigned)COM_RX_IPDU_MAX);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL_GROUP, COM_E_PARAM);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindRxIPdu(SignalGroupId);
    if (ipdu == NULL || ipdu->IsSignalGroup == 0U)
    {
        DET_LOGE(TAG, "ReceiveSignalGroup E: SignalGroupId=%u not found or not a Signal Group",
                 (unsigned)SignalGroupId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL_GROUP, COM_E_PARAM);
        return E_NOT_OK;
    }

    /* update-bit（SWS_Com_00324/00802）: 設定されており、かつ 0（未更新）の
     * 場合、受信データを破棄する。シャドウバッファ・タイムアウトスナップ
     * ショットとも直近の状態のまま更新しない（＝前回 update-bit=1 で確定
     * コピーした内容を Com_ReceiveSignal() が返し続ける）。 */
    if (ipdu->UpdateBitPosition != 0xFFU)
    {
        const uint32 updateBit = Com_UnpackSignal(Com_RxBuffer[SignalGroupId],
                                                    ipdu->UpdateBitPosition, 1U, COM_BIG_ENDIAN);
        if (updateBit == 0U)
            return E_OK;
    }

    for (uint8 b = 0U; b < ipdu->DLC; b++)
        Com_RxShadowBuffer[SignalGroupId][b] = Com_RxBuffer[SignalGroupId][b];

    Com_RxShadowTimedOut[SignalGroupId] = Com_RxTimedOut[SignalGroupId];

    return Com_RxShadowTimedOut[SignalGroupId] ? E_NOT_OK : E_OK;
}

/**
 * \brief   RX I-PDU の生バイト列をそのままコピーする。
 *
 * \details Com_ReceiveSignal() のようなビット単位アンパックを行わず、
 *          I-PDU バッファの内容を DataPtr へそのまま（先頭 DLC バイト分）
 *          コピーする。E2E Transformer（RxIndicationCbk 経由で呼ばれる
 *          InverseTransform 等）が、CRC/Counter 検証のために I-PDU 全体の
 *          バイト列を必要とする用途を想定している（実 AUTOSAR の
 *          Com_ReceiveSignalGroupArray に相当する簡略版）。
 *
 *          Com_ReceiveSignal() と異なり、Com_RxTimedOut は見ない
 *          （RxIndicationCbk はフレーム受信直後、タイムアウト判定より前に
 *          呼ばれるため、このコピー自体は常に「最新の受信データ」を指す）。
 *
 * \param[in]  SignalGroupId  読み取る Signal Group（RX I-PDU）の ID。本プロジェクトは
 *                            Signal Group を専用の ID 空間に持たず所属 I-PDU の ID を
 *                            そのまま使う簡略設計のため、Com_SignalGroupIdType は
 *                            Com_IPduIdType と同じ uint8 の別名（Com_Types.h 参照）。
 * \param[out] DataPtr        コピー先バッファへのポインタ。ipdu->DLC バイト以上
 *                            必要。NULL 禁止。
 *
 * \retval  E_OK      SignalGroupId が見つかり、DataPtr へコピーした。
 * \retval  E_NOT_OK  COM 未初期化、DataPtr が NULL、
 *                    または SignalGroupId が RX I-PDU 設定に存在しない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \note    実仕様([SWS_Com_00854])は戻り値型 uint8・引数型 Com_SignalGroupIdType
 *          だが、以前は Com_SendSignalGroup/Com_ReceiveSignalGroup(PR#192で修正済み)
 *          と同じ乖離が残っていた。今回まとめて修正。
 * \ServiceID      {0x24}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_ReceiveSignalGroupArray(Com_SignalGroupIdType SignalGroupId, uint8* DataPtr)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL_GROUP_ARRAY, COM_E_UNINIT);
        return E_NOT_OK;
    }
    if (DataPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL_GROUP_ARRAY, COM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindRxIPdu(SignalGroupId);
    if (ipdu == NULL)
    {
        DET_LOGE(TAG, "ReceiveSignalGroupArray E: SignalGroupId=%u not found", (unsigned)SignalGroupId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_RECEIVE_SIGNAL_GROUP_ARRAY, COM_E_PARAM);
        return E_NOT_OK;
    }

    for (uint8 b = 0; b < ipdu->DLC; b++)
        DataPtr[b] = Com_RxBuffer[SignalGroupId][b];
    return E_OK;
}

/**
 * \brief   RX I-PDU が現在タイムアウト中かどうかを返す。
 *
 * \details Com_RxTimedOut[IPduId] をそのまま返す軽量アクセサ。
 *          Rte 層が Com_ReceiveSignal() を介さずに、E_NOT_OK 判定の
 *          ゲートとして直接参照する用途を想定している
 *          （E2E Transformer 方式では Rte がミラーから値を読むため、
 *          Com_ReceiveSignal() のタイムアウトチェックを経由しない）。
 *
 * \param[in]  IPduId  確認する RX I-PDU の ID。
 *
 * \retval  1  タイムアウト中（IPduId が範囲外の場合も安全側でこちらを返す）。
 * \retval  0  タイムアウトしていない（正常受信中）。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \note    本プロジェクト独自 API（実 AUTOSAR に対応関数なし）のため、
 *          ServiceID は Dcm_ComIndication 等と同じ非標準値 0xF0 を踏襲する
 *          （Com_Cfg.h 参照）。
 * \ServiceID      {0xF0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_IsRxTimedOut(Com_IPduIdType IPduId)
{
    DET_LOGT(TAG, "called");

    if (IPduId >= COM_RX_IPDU_MAX)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_IS_RX_TIMED_OUT, COM_E_PARAM);
        return 1U;
    }
    return Com_RxTimedOut[IPduId];
}

/**
 * \brief   TX I-PDU バッファへシグナル値をパックする。
 *
 * \details シグナル設定テーブルの SignalId に一致するエントリを検索し、
 *          ビット位置・サイズ・エンディアンに従って内部 TX バッファへ
 *          パックする。SignalDataPtr から 4 バイトのリトルエンディアン整数として
 *          値を読み取り、BitSize に関係なく該当ビットのみ書き換える。
 *          送信要否・タイミングの判断は本関数内で完結する
 *          （ComFilterAlgorithm 通過時、DIRECT/MIXED I-PDU なら即座に
 *          送信する。呼び出し元が別途送信をトリガする必要はない）。
 *
 * \param[in]  SignalId      書き込むシグナルの ID。
 *                           シグナル設定テーブルのエントリと一致すること。
 * \param[in]  SignalDataPtr シグナル値へのポインタ。4 バイト以上で
 *                           リトルエンディアン順。NULL 禁止。
 *
 * \retval  E_OK      シグナルが見つかり、TX バッファへ値をパックした。
 * \retval  E_NOT_OK  COM 未初期化、SignalDataPtr が NULL、
 *                    またはシグナル設定テーブルに SignalId が存在しない。
 *
 * \details ComFilterAlgorithm:
 *          値をバッファへパックした後、シグナルの FilterAlgorithm を評価する。
 *          COM_FILTER_ALWAYS なら常に、COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD
 *          なら (新値 & Mask) が前回のフィルタ比較値と異なる場合のみ、
 *          「送信すべき変化あり」とみなして Com_RequestTxOnChange() を呼ぶ
 *          （TxModeMode が DIRECT/MIXED の I-PDU なら次回 Com_MainFunctionTx()
 *          で送信される。本関数自体は PduR_ComTransmit() を呼ばない）。これとは
 *          独立に、Com_RecalcTms() が TMS の遷移を検出した場合も
 *          ComFilterAlgorithm の判定結果によらず Com_RequestTxOnChange() を
 *          呼ぶ（SWS_Com_00495。非 Signal Group のシグナルに TmsContributor=1
 *          を設定した場合に備える。現状の設定ではこの経路は通らない）。
 *
 *          Signal Group（詳細は Com_SendSignalGroup() の \AUTOSARReq 参照）:
 *          所属する I-PDU が IsSignalGroup=1 の場合、値は実 TX バッファ
 *          (Com_TxBuffer) ではなくシャドウバッファ (Com_TxShadowBuffer) へ
 *          パックするのみとし、ComFilterAlgorithm の判定も行わない
 *          （Signal Group メンバーの送信要否は ComFilterAlgorithm ではなく
 *          ComTransferProperty が決める。Com_TransferPropertyType 参照）。
 *          Com_SendSignalGroup() が呼ばれるまで実バッファへは反映されない
 *          （グループの複数メンバーを不整合な状態で送信しないため）。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \note       戻り値型は仕様に従い uint8。E_OK / E_NOT_OK の値（0x00 / 0x01）は
 *             RTE が使う Std_ReturnType と互換性がある。
 *
 * \AUTOSARReq     {SWS_Com_00197, SWS_Com_00742, SWS_Com_00743, SWS_Com_00061,
 *                  SWS_Com_00495}
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL, COM_E_UNINIT);
        return E_NOT_OK;
    }
    if (SignalDataPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL, COM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const uint8* dataPtr = (const uint8*)SignalDataPtr;

    const uint8 s = Com_FindSignalIndex(SignalId);
    if (s < Com_ConfigPtr->SignalCount)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];

        /* 範囲チェック + 登録確認: sig->IPduId をそのまま Com_TxBuffer[] 等の
         * 配列添字として使う前に、(1) 配列範囲内であること、
         * (2) TX I-PDU 設定テーブルに実際に登録された IPduId であることを
         * 確認する。Com_FindTxIPdu() が NULL を返す（設定ミスで存在しない
         * I-PDU を指している）場合に以前は判定を素通りしてしまい、範囲外の
         * IPduId であれば隣接するグローバル変数を破壊するバッファオーバーラン
         * になり得た。 */
        if (sig->IPduId >= COM_TX_IPDU_MAX)
        {
            DET_LOGE(TAG, "SendSignal E: sig=%u IPduId=%u out of range (max=%u)",
                     (unsigned)SignalId, (unsigned)sig->IPduId, (unsigned)COM_TX_IPDU_MAX);
            Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL, COM_E_PARAM);
            return E_NOT_OK;
        }

        const Com_IPduConfigType* ipdu = Com_FindTxIPdu(sig->IPduId);
        if (ipdu == NULL)
        {
            DET_LOGE(TAG, "SendSignal E: sig=%u IPduId=%u not a registered TX I-PDU",
                     (unsigned)SignalId, (unsigned)sig->IPduId);
            Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL, COM_E_PARAM);
            return E_NOT_OK;
        }

        /* SignalDataPtr は呼び出し元が BitSize に応じた幅の変数
         * (uint8/uint16/uint32) を渡す。常に 4 バイト読み込むと、
         * 8bit/16bit の呼び出し元ではスタック上の隣接領域を読んでしまう。
         * BitSize から必要バイト数だけを読み込む。 */
        const uint8 byteCount = (uint8)((sig->BitSize + 7U) / 8U);
        uint32 value = 0U;
        for (uint8 b = 0U; b < byteCount; b++)
        {
            value |= ((uint32)dataPtr[b]) << (8U * b);
        }

        if (ipdu->IsSignalGroup != 0U)
        {
            /* Signal Group メンバー: シャドウバッファへ書き込むのみ。
             * 実バッファへの反映は Com_SendSignalGroup() が行う。
             *
             * ComTransferProperty（SWS_Com_00742/00743、Com_TransferPropertyType
             * 参照）: TRIGGERED_ON_CHANGE のメンバーのみ、前回値との比較で
             * このグループの送信を引き起こすかどうかを判定する。この比較は
             * ComFilterAlgorithm/Mask/FilterX とは独立しており、マスクなしの
             * 生値同士を比較する（TmsContributor=1 として同じシグナルが
             * COM_FILTER_MASKED_NEW_DIFFERS_X を TMS 評価に使っていても競合
             * しない。TMS 再評価は Com_SendSignalGroup() 側で行う）。
             * PENDING のメンバーは Com_GroupTriggerPending へ一切書き込まない
             * （＝自身の変化だけでは送信を引き起こさない。SWS_Com_00743）。 */
            if (sig->TransferProperty == COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE
                && value != Com_FilterLastValue[s])
            {
                Com_GroupTriggerPending[sig->IPduId] = 1U;
            }
            Com_FilterLastValue[s] = value;

            Com_PackSignal(Com_TxShadowBuffer[sig->IPduId],
                           sig->BitPosition, sig->BitSize, sig->Endian, value);
            return E_OK;
        }

        Com_PackSignal(Com_TxBuffer[sig->IPduId],
                       sig->BitPosition, sig->BitSize, sig->Endian, value);

        /* TMS 再評価（SWS_Com_00245）。Com_SendSignalGroup() と同様、実バッファへの
         * 反映後・Com_RequestTxOnChange() 呼び出し前に行う（Com_RequestTxOnChange()
         * が Com_EffectiveTxModeMode() 経由で Com_TmsState を参照するため）。
         * 現状 TmsContributor=1 を設定しているシグナルは Signal Group
         * （WarningStatus）にしか存在しないためこの呼び出しがなくても実害はないが、
         * 非 Signal Group のシグナルに TmsContributor=1 を設定した場合に備える。
         * 戻り値（TMS が今回変化したか）は下記 SWS_Com_00495 対応で使う。 */
        const uint8 tmsChanged = Com_RecalcTms(sig->IPduId);

        /* ComFilterAlgorithm 評価: 送信すべき更新かどうかは Com 自身が判断する
         * (ASW は値をセットするだけで、送信要否には関与しない) */
        uint8 passesFilter = 1U;
        if (sig->FilterAlgorithm == COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD)
        {
            passesFilter = ((value & sig->Mask) != (Com_FilterLastValue[s] & sig->Mask)) ? 1U : 0U;
        }
        Com_FilterLastValue[s] = value;

        /* SWS_Com_00495: TMS の遷移によって送信モードが切り替わった場合は、
         * この変化を起こしたシグナルの ComFilterAlgorithm 判定によらず無条件に
         * 即座に送信しなければならない。passesFilter とは独立の判断軸として
         * OR で合成する（詳細は Com_RecalcTms() のドキュメント参照）。 */
        if (passesFilter || tmsChanged)
        {
            Com_RequestTxOnChange(ipdu);
        }

        /* update-bit セット（SWS_Com_00061 相当）。仕様原文は「Com_SendSignal
         * が呼ばれるたびに無条件でセットする」だが、本プロジェクトの ASW は
         * 毎サイクル無条件に Com_SendSignal() を呼び、「値が実際に変化したか」
         * の判定は Com の ComFilterAlgorithm に委ねる設計（README「責務分離の
         * 効果」参照）。そのため文字どおり無条件にセットすると、次の実送信
         * （周期フロア含む）までの間に必ず ASW が再度 Com_SendSignal() を
         * 呼んでビットを再セットしてしまい、update-bit が常に 1 のまま
         * 「実際に変化したか」を一切表せなくなる（2026-07 時点で実機確認済み
         * の不具合）。そこで本実装は、このシグナルの送信要否判定
         * （passesFilter、Com_RequestTxOnChange() と同じ判断軸）に合わせて
         * セットする。ASW 側の「常に書き込む」設計を変えずに、update-bit
         * 本来の目的（このシグナルが実際に更新されたかどうかを示す）を
         * 満たすための、本プロジェクト固有の解釈である。TMS 遷移のみによる
         * 即時送信（tmsChanged）はこのシグナル自体の値更新を意味しないため、
         * update-bit の条件には含めない（passesFilter のみで判定する）。 */
        /* UpdateBitContributor（Com_Types.h 参照）: I-PDU に複数の非 Signal
         * Group TX シグナルが同居する場合、update-bit を「このシグナルの
         * 変化」専用に保つため、寄与するシグナルのみに絞る（TmsContributor
         * と同じパターン。2026-08 コードレビューで、MeterStatus に
         * EngineSpeed/RunLamp 等のミラーシグナルを追加した際、それらの
         * 変化だけで EngineState 用の update-bit が誤って立つ不具合が
         * 見つかり対応した）。 */
        if (passesFilter && ipdu->UpdateBitPosition != 0xFFU && sig->UpdateBitContributor == 1U)
            Com_PackSignal(Com_TxBuffer[sig->IPduId], ipdu->UpdateBitPosition, 1U, COM_BIG_ENDIAN, 1U);

        return E_OK;
    }

    DET_LOGE(TAG, "SendSignal E: sig=%u not found", (unsigned)SignalId);
    Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL, COM_E_PARAM);
    return E_NOT_OK;
}

/**
 * \brief   Signal Group メンバーをシャドウバッファから実 TX バッファへ確定コミットする。
 *
 * \details Com_SendSignal() が Signal Group（IsSignalGroup=1）のメンバーを
 *          書き込んだシャドウバッファ (Com_TxShadowBuffer) を、実 TX バッファ
 *          (Com_TxBuffer) へまとめてコピーする（PENDING/TRIGGERED_ON_CHANGE
 *          いずれのメンバーの値も分け隔てなくコピーする）。
 *          送信を引き起こすかどうかは、バイト単位の変化比較ではなく
 *          Com_GroupTriggerPending[GroupId]（ComTransferProperty=
 *          TRIGGERED_ON_CHANGE のメンバーが Com_SendSignal() 内で変化検知した
 *          際に立てるフラグ。Com_TransferPropertyType 参照）で判定する。
 *          立っていれば Com_RequestTxOnChange() を呼ぶ（TxModeMode が
 *          DIRECT/MIXED の I-PDU なら次回 Com_MainFunctionTx() で送信される）。
 *          これとは独立に、Com_RecalcTms() が TMS（Transmission Mode
 *          Selector）の遷移（true⇔false）を検出した場合も、
 *          Com_GroupTriggerPending の状態によらず Com_RequestTxOnChange() を
 *          呼ぶ（SWS_Com_00495: TMS 遷移によるモード切り替えは、それを
 *          起こしたシグナルの ComTransferProperty によらず無条件に即座に
 *          送信しなければならない）。
 *
 *          update-bit（IPduId->UpdateBitPosition が 0xFF 以外の場合、
 *          SWS_Com_00801）: 呼ばれるたびに無条件でこのビットをセットする
 *          （値が実際に変化したかどうかは問わない。Com_GroupTriggerPending
 *          とは独立の判断軸）。クリアは Com_DoTransmit() 側で行う。
 *
 * \param[in]  SignalGroupId  コミットする Signal Group の ID（所属する TX
 *                            I-PDU の ID と同じ、Com_Types.h 参照）。
 *
 * \retval  E_OK      SignalGroupId が見つかり、コミット処理を行った。
 * \retval  E_NOT_OK  COM 未初期化、SignalGroupId が TX I-PDU 設定テーブルに
 *                    存在しない、または IsSignalGroup=0 の I-PDU を指定した。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        コミット前に、このグループに属する全メンバーを
 *             Com_SendSignal() で設定しておくこと。
 *
 * \AUTOSARReq     {SWS_Com_00200, SWS_Com_00050, SWS_Com_00742, SWS_Com_00743,
 *                  SWS_Com_00801, SWS_Com_00055, SWS_Com_00495}
 * \ServiceID      {0x0d}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_SendSignalGroup(Com_SignalGroupIdType SignalGroupId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL_GROUP, COM_E_UNINIT);
        return E_NOT_OK;
    }

    /* 範囲チェック: SignalGroupId をそのまま Com_TxBuffer[] 等の配列添字として
     * 使うため、TX I-PDU 設定テーブル自体に範囲外の IPduId が設定される
     * 事態に備えて明示的に検査する（Com_ReceiveSignal/Com_SendSignal と
     * 同じ方針）。 */
    if (SignalGroupId >= COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "SendSignalGroup E: SignalGroupId=%u out of range (max=%u)",
                 (unsigned)SignalGroupId, (unsigned)COM_TX_IPDU_MAX);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL_GROUP, COM_E_PARAM);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindTxIPdu(SignalGroupId);
    if (ipdu == NULL || ipdu->IsSignalGroup == 0U)
    {
        DET_LOGE(TAG, "SendSignalGroup E: SignalGroupId=%u not found or not a Signal Group",
                 (unsigned)SignalGroupId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL_GROUP, COM_E_PARAM);
        return E_NOT_OK;
    }

    /* PENDING/TRIGGERED_ON_CHANGE を問わず、シャドウバッファの値はすべて
     * 実バッファへコピーする（SWS_Com_00743: PENDING メンバーも、他の
     * メンバーが引き起こした送信に便乗して最新値が運ばれる）。 */
    for (uint8 b = 0U; b < ipdu->DLC; b++)
    {
        Com_TxBuffer[SignalGroupId][b] = Com_TxShadowBuffer[SignalGroupId][b];
    }

    /* TMS 再評価（SWS_Com_00245）。Com_RequestTxOnChange() が
     * Com_EffectiveTxModeMode() 経由で Com_TmsState を参照するため、
     * その呼び出しより前に確定させる。TMS 寄与シグナルが PENDING の場合、
     * 「送信は引き起こさないが TMS だけは変化する」こともあり得るが、
     * これは仕様上の矛盾ではない（TMS は「次に送信するときどのモードを
     * 使うか」を決めるだけで、それ自体が送信のトリガーではないため）。
     * 戻り値（TMS が今回変化したか）は下記 SWS_Com_00495 対応で使う。 */
    const uint8 tmsChanged = Com_RecalcTms(SignalGroupId);

    /* 送信を引き起こすかどうかは、ComTransferProperty=TRIGGERED_ON_CHANGE の
     * メンバーが Com_SendSignal() 内で変化検知して立てたフラグのみで判定する
     * （バイト単位の生比較はしない。PENDING メンバーだけが変化した場合は
     * このフラグは立たず、コミットはされても送信は引き起こされない）。 */
    const uint8 groupTriggered = Com_GroupTriggerPending[SignalGroupId];
    Com_GroupTriggerPending[SignalGroupId] = 0U;

    /* SWS_Com_00495: TMS の遷移によって送信モードが切り替わった場合は、
     * その変化を起こしたシグナルの ComTransferProperty（TRIGGERED_ON_CHANGE/
     * PENDING）によらず無条件に即座に送信しなければならない。groupTriggered
     * （通常のトリガー）とは独立の判断軸として OR で合成する。これにより、
     * TMS 寄与シグナルが PENDING のみで構成される場合でも（groupTriggered が
     * 立たないため）TMS 遷移そのものが確実に送信を引き起こすようになる
     * （詳細は Com_RecalcTms() のドキュメント参照）。 */
    if (groupTriggered || tmsChanged)
    {
        Com_RequestTxOnChange(ipdu);
    }

    /* update-bit セット（SWS_Com_00801 相当）。仕様原文は「
     * Com_SendSignalGroup が呼ばれるたびに無条件でセットする」だが、
     * MeterStatus/EngineState（Com_SendSignal 側）で実機確認済みの
     * 不具合と同じ理由により、本実装では Com_GroupTriggerPending
     * （＝ TRIGGERED_ON_CHANGE メンバーが実際に変化したかどうか、
     * Com_RequestTxOnChange() を呼ぶかどうかと同じ判断軸）に条件づける。
     * App_WarningIndicator_Run() は毎サイクル無条件に
     * Rte_SendSignalGroup_WarningStatus()（→本関数）を呼ぶ設計（ASW は
     * 値を書くだけ、Com が送信要否を判断する責務分離。README「責務分離
     * の効果」参照）のため、無条件セットのままだと次の実送信までの間に
     * 必ず ASW が本関数を再度呼んでビットを再セットしてしまい、
     * update-bit が常に 1 のままになる。詳細は Com_SendSignal() の
     * 同種コメント・README「Update Bit」節参照。TMS 遷移のみによる即時送信
     * （tmsChanged）はグループメンバーの値更新を意味しないため、update-bit
     * の条件には含めない（groupTriggered のみで判定する）。 */
    if (groupTriggered && ipdu->UpdateBitPosition != 0xFFU)
    {
        Com_PackSignal(Com_TxBuffer[SignalGroupId], ipdu->UpdateBitPosition, 1U, COM_BIG_ENDIAN, 1U);
    }

    return E_OK;
}

/**
 * \brief   TX I-PDU へ生バイト列をそのままコミットする（Signal Group 単位）。
 *
 * \details Com_SendSignal() を1本ずつ呼んでシャドウバッファ (Com_TxShadowBuffer)
 *          へ書き込み、Com_SendSignalGroup() でまとめてコミットする通常経路の
 *          代わりに、I-PDU 全体のバイト列を1回で TX バッファ (Com_TxBuffer) へ
 *          直接書き込む（実 AUTOSAR の Com_SendSignalGroupArray に相当する
 *          簡略版。Com_ReceiveSignalGroupArray と対称——あちらは I-PDU から
 *          呼び出し元へ、こちらは呼び出し元から I-PDU への一括コピー）。
 *
 *          Com_SendSignalGroup() と異なりシャドウバッファへの書き込みは
 *          経由しないが、以降に通常経路（Com_SendSignal()+
 *          Com_SendSignalGroup()）と混在して使われた場合に古い状態で
 *          上書きされないよう、シャドウバッファ・Com_GroupTriggerPending・
 *          各メンバーの変化検知ベースライン（Com_FilterLastValue）は
 *          いずれも今回の書き込み内容に同期する（/code-review で
 *          指摘: 同期しないと、後で Com_SendSignalGroup() が呼ばれた際に
 *          古いシャドウバッファ内容で今回のコミットを黙って巻き戻す、
 *          または古い Com_GroupTriggerPending が残ったまま次回変化なしで
 *          誤発火する、といった状態不整合が起こり得た）。
 *          TMS 再評価（Com_RecalcTms()）は Com_TxBuffer から直接読むため、
 *          この直接書き込みでも正しく動作する（Com.c 該当関数参照）。
 *
 *          個々のシグナル単位の変化検知（Com_GroupTriggerPending、
 *          ComTransferProperty=TRIGGERED_ON_CHANGE のメンバーが
 *          Com_SendSignal() 内で検知するもの）を経由しないため、本関数は
 *          呼ばれるたびに常に「新しいデータがある」ものとして扱い、無条件で
 *          送信要求（Com_RequestTxOnChange()）・update-bit セットを行う
 *          （[SWS_Com_00801] 原文どおり「呼ばれるたびに無条件でセットする」
 *          という素直な実装。Com_SendSignal()/Com_SendSignalGroup() 側で
 *          これを Com_GroupTriggerPending に条件づけているのは、ASW が
 *          毎サイクル無条件に呼ぶ既存の呼び出しパターン（App_WarningIndicator_Run
 *          等）に合わせた対策であり、本関数は呼び出し側が明示的に「新しい
 *          データがある」ときのみ呼ぶ想定の別 API のため、その対策は不要）。
 *
 * \param[in]  SignalGroupId  コミットする Signal Group（TX I-PDU）の ID。本プロジェクトは
 *                            Signal Group を専用の ID 空間に持たず所属 I-PDU の ID を
 *                            そのまま使う簡略設計のため、Com_SignalGroupIdType は
 *                            Com_IPduIdType と同じ uint8 の別名（Com_Types.h 参照）。
 * \param[in]  DataPtr        書き込む生バイト列。ipdu->DLC バイト以上必要。NULL 禁止。
 *
 * \retval  E_OK      SignalGroupId が見つかり、書き込み・コミット処理を行った。
 * \retval  E_NOT_OK  COM 未初期化、DataPtr が NULL、SignalGroupId が TX I-PDU 設定
 *                    テーブルに存在しない、または IsSignalGroup=0 の I-PDU
 *                    を指定した。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \note    実仕様([SWS_Com_00851])は戻り値型 uint8・引数型 Com_SignalGroupIdType
 *          だが、以前は Com_SendSignalGroup/Com_ReceiveSignalGroup(PR#192で修正済み)
 *          と同じ乖離が残っていた。今回まとめて修正。
 * \AUTOSARReq     {SWS_Com_00851, SWS_Com_00852, SWS_Com_00853}
 * \ServiceID      {0x23}
 * \Reentrancy     {Non Reentrant for the same signal group. Reentrant for
 *                  different signal groups.}
 * \Synchronicity  {Asynchronous}
 */
uint8 Com_SendSignalGroupArray(Com_SignalGroupIdType SignalGroupId, const uint8* DataPtr)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL_GROUP_ARRAY, COM_E_UNINIT);
        return E_NOT_OK;
    }
    if (DataPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL_GROUP_ARRAY, COM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* 範囲チェック: SignalGroupId をそのまま Com_TxBuffer[] 等の配列添字として
     * 使うため、TX I-PDU 設定テーブル自体に範囲外の IPduId が設定される事態に
     * 備えて明示的に検査する（Com_SendSignalGroup() と同じ方針）。 */
    if (SignalGroupId >= COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "SendSignalGroupArray E: SignalGroupId=%u out of range (max=%u)",
                 (unsigned)SignalGroupId, (unsigned)COM_TX_IPDU_MAX);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL_GROUP_ARRAY, COM_E_PARAM);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindTxIPdu(SignalGroupId);
    if (ipdu == NULL || ipdu->IsSignalGroup == 0U)
    {
        DET_LOGE(TAG, "SendSignalGroupArray E: SignalGroupId=%u not found or not a Signal Group",
                 (unsigned)SignalGroupId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SEND_SIGNAL_GROUP_ARRAY, COM_E_PARAM);
        return E_NOT_OK;
    }

    /* シャドウバッファ・保留フラグ・変化検知ベースラインの同期理由は
     * 上の \details 参照。 */
    for (uint8 b = 0U; b < ipdu->DLC; b++)
    {
        Com_TxBuffer[SignalGroupId][b]       = DataPtr[b];
        Com_TxShadowBuffer[SignalGroupId][b] = DataPtr[b];
    }

    Com_GroupTriggerPending[SignalGroupId] = 0U;

    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->Direction == COM_SIGNAL_DIRECTION_TX && sig->IPduId == SignalGroupId)
        {
            Com_FilterLastValue[s] = Com_UnpackSignal(Com_TxBuffer[SignalGroupId],
                                                        sig->BitPosition, sig->BitSize, sig->Endian);
        }
    }

    /* TMS 再評価（SWS_Com_00245、本関数でも正しく動く理由は上の \details
     * 参照）。戻り値（TMS が今回変化したか）は使わない: 本関数は常に無条件で
     * 送信要求するため、TMS 遷移かどうかで分岐する必要がない
     * （SWS_Com_00495 が要求する「TMS 遷移は無条件で即座に送信」も、
     * この無条件送信要求に自然に含まれる）。 */
    (void)Com_RecalcTms(SignalGroupId);
    Com_RequestTxOnChange(ipdu);

    if (ipdu->UpdateBitPosition != 0xFFU)
    {
        Com_PackSignal(Com_TxBuffer[SignalGroupId], ipdu->UpdateBitPosition, 1U, COM_BIG_ENDIAN, 1U);
    }

    return E_OK;
}

/**
 * \brief   シグナルを、設定済みの ComSignalDataInvalidValue で無効化する。
 *
 * \details [SWS_Com_00099]/[SWS_Com_00642]: 内部的に Com_SendSignal() を
 *          InvalidValue で呼ぶだけであり、独自の送信ロジックは持たない。
 *          SignalId が Signal Group メンバーであっても Com_SendSignal()
 *          自身がシャドウバッファへの書き込みに正しく分岐するため
 *          （7.4.2 章）、本関数側で Signal Group か否かを判定する必要はない。
 *
 *          [SWS_Com_00643]: ComSignalDataInvalidValue が未設定
 *          （Com_SignalConfigType.InvalidValueConfigured=0）の場合は
 *          COM_SERVICE_NOT_AVAILABLE 相当として拒否する。この条件は仕様上
 *          「開発エラーによる失敗」とは別区分のため、Det_ReportError() は
 *          呼ばない（DET ログのみ）。
 *
 * \param[in]  SignalId  無効化する TX シグナルの ID。
 *
 * \retval  E_OK      SignalId が見つかり、InvalidValue が設定済みで、
 *                    Com_SendSignal() が成功した。
 * \retval  E_NOT_OK  COM 未初期化、SignalId が存在しない、SignalId が TX
 *                    シグナルでない、ComSignalDataInvalidValue が未設定、
 *                    または内部の Com_SendSignal() が失敗した。
 *
 * \AUTOSARReq     {SWS_Com_00099, SWS_Com_00642, SWS_Com_00643}
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant for the same signal. Reentrant for different signals.}
 * \Synchronicity  {Asynchronous}
 */
uint8 Com_InvalidateSignal(Com_SignalIdType SignalId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INVALIDATE_SIGNAL, COM_E_UNINIT);
        return E_NOT_OK;
    }

    const uint8 s = Com_FindSignalIndex(SignalId);
    if (s >= Com_ConfigPtr->SignalCount)
    {
        /* 未知の SignalId。下の InvalidValueConfigured 確認のためにここで
         * シグナルを解決する必要があり、Com_SendSignal() 側の同種チェックを
         * 先取りする形になる（DET 報告の内容は Com_SendSignal() と同じ）。 */
        DET_LOGE(TAG, "InvalidateSignal E: sig=%u not found", (unsigned)SignalId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INVALIDATE_SIGNAL, COM_E_PARAM);
        return E_NOT_OK;
    }

    const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
    if (sig->Direction != COM_SIGNAL_DIRECTION_TX)
    {
        /* RX/TX の IPduId は別々の配列だが同じ数値空間を共有するため
         * （Com_FindTxIPdu() は数値が一致する限り RX シグナルの IPduId とも
         * 偶然マッチしてしまいうる）、Direction を明示的に確認しないまま
         * Com_SendSignal() に委譲すると、誤って RX シグナルに
         * InvalidValueConfigured=1 を設定した場合に無関係な TX I-PDU を
         * 静かに破壊しかねない（DET エラーなし）。Com_InvalidateSignalGroup()
         * 側は元々メンバー走査時に Direction==TX で絞っているため、この
         * チェックはそちらと対称にするための是正（/code-review 指摘）。 */
        DET_LOGE(TAG, "InvalidateSignal E: sig=%u is not a TX signal", (unsigned)SignalId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INVALIDATE_SIGNAL, COM_E_PARAM);
        return E_NOT_OK;
    }
    if (sig->InvalidValueConfigured == 0U)
    {
        DET_LOGW(TAG, "InvalidateSignal: sig=%u has no ComSignalDataInvalidValue configured",
                 (unsigned)SignalId);
        return E_NOT_OK;
    }

    return Com_SendSignal(SignalId, &sig->InvalidValue);
}

/**
 * \brief   Signal Group の全メンバーを、各々の ComSignalDataInvalidValue で無効化する。
 *
 * \details [SWS_Com_00557]: グループメンバーのいずれか 1 つでも
 *          ComSignalDataInvalidValue が未設定なら、書き込みを一切行わず
 *          全体を E_NOT_OK とする（all-or-nothing。副作用を起こす前に
 *          全メンバーを検証してから実際の書き込みへ進む）。
 *
 *          [SWS_Com_00099]/[SWS_Com_00645]: 各メンバーごとに
 *          Com_SendSignal() を InvalidValue で呼んでシャドウバッファへ
 *          書き込んだのち、内部的に Com_SendSignalGroup() を呼んで実
 *          バッファへ確定コミットする（Com_InvalidateSignal() と同じ
 *          「内部的に対応する送信 API を呼ぶ」構造の Signal Group 版）。
 *
 * \param[in]  SignalGroupId  無効化する Signal Group（TX I-PDU）の ID。
 *
 * \retval  E_OK      全メンバーの InvalidValue が設定済みで、コミットまで成功した。
 * \retval  E_NOT_OK  COM 未初期化、SignalGroupId が TX I-PDU 設定テーブルに
 *                    存在しない、IsSignalGroup=0 の I-PDU を指定した、
 *                    またはいずれかのメンバーの ComSignalDataInvalidValue が
 *                    未設定。
 *
 * \AUTOSARReq     {SWS_Com_00557, SWS_Com_00645}
 * \ServiceID      {0x1B}
 * \Reentrancy     {Non Reentrant for the same signal group. Reentrant for different signal groups.}
 * \Synchronicity  {Asynchronous}
 */
uint8 Com_InvalidateSignalGroup(Com_SignalGroupIdType SignalGroupId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INVALIDATE_SIGNAL_GROUP, COM_E_UNINIT);
        return E_NOT_OK;
    }

    if (SignalGroupId >= COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "InvalidateSignalGroup E: SignalGroupId=%u out of range (max=%u)",
                 (unsigned)SignalGroupId, (unsigned)COM_TX_IPDU_MAX);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INVALIDATE_SIGNAL_GROUP, COM_E_PARAM);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindTxIPdu(SignalGroupId);
    if (ipdu == NULL || ipdu->IsSignalGroup == 0U)
    {
        DET_LOGE(TAG, "InvalidateSignalGroup E: SignalGroupId=%u not found or not a Signal Group",
                 (unsigned)SignalGroupId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INVALIDATE_SIGNAL_GROUP, COM_E_PARAM);
        return E_NOT_OK;
    }

    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->Direction == COM_SIGNAL_DIRECTION_TX && sig->IPduId == SignalGroupId
            && sig->InvalidValueConfigured == 0U)
        {
            DET_LOGW(TAG, "InvalidateSignalGroup: SignalGroupId=%u member sig=%u has no "
                     "ComSignalDataInvalidValue configured",
                     (unsigned)SignalGroupId, (unsigned)sig->SignalId);
            return E_NOT_OK;
        }
    }

    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->Direction == COM_SIGNAL_DIRECTION_TX && sig->IPduId == SignalGroupId)
        {
            (void)Com_SendSignal(sig->SignalId, &sig->InvalidValue);
        }
    }

    return Com_SendSignalGroup(SignalGroupId);
}

/**
 * \brief   TX I-PDU を、値の変化や送信モードに関わらず今すぐ送信要求する。
 *
 * \details [SWS_Com_00861]: 対象 I-PDU が started の場合のみトリガーする。
 *          stopped の場合は E_NOT_OK を返すのみで、後で started になっても
 *          自動的には実行されない（トリガー自体を憶えておく仕組みはない）。
 *
 *          [SWS_Com_00388]: MDT（`ipdu->MinDelayMs`）のみを尊重し、
 *          `ComTxModeNumberOfRepetitions` 等、他の TxMode 関連パラメータは
 *          考慮しない。実際の送信は本関数内では行わず、既存の
 *          `Com_TxTriggerPending[]` フラグを立てるだけで
 *          `Com_MainFunctionTx()` のディスパッチへ委ねる（`Com_SendSignal()`
 *          が `Com_TxPending[]` を立てるのと同じ設計——実送信を ASW の
 *          呼び出しスタックから切り離し、WdgM の Deadline Supervision から
 *          保護するため。Com_MainFunctionTx() の Doxygen コメント参照）。
 *          `Com_TxTriggerPending[]` は `Com_TxPending[]` と異なり
 *          COM_TX_MODE_PERIODIC の I-PDU でも効く（詳細は同フラグの宣言
 *          コメント参照）。
 *
 *          [SWS_Com_00492]: 設定済みの TxIpduCalloutCbk は、既存の
 *          `Com_DoTransmit()` が呼ぶため、本関数側で別途呼ぶ必要はない。
 *
 * \note    診断 CommunicationControl (UDS 0x28) による送信抑制中
 *          （`Com_TxEnabled==0`）に due 判定を満たしても、
 *          `Com_MainFunctionTx()` はトリガーを消費するだけで実送信は行わない
 *          （`Com_TxPending[]` の既存挙動と同じ。SWS_Com_00777/
 *          SWS_Com_00334: 抑制解除後に「溜まった分」を即座に送らないため）。
 *          この場合本関数の戻り値自体は E_OK のままであり、トリガーが
 *          後で自動的に再送されることもない——呼び出し元が抑制解除後に
 *          必要なら改めて呼び直すこと（/code-review 指摘）。
 *
 * \param[in]  PduId  即時送信をトリガーする TX I-PDU の ID。
 *
 * \retval  E_OK      I-PDU が見つかり、started であり、トリガーを受け付けた
 *                    （実際に送信されるとは限らない。上記 \note 参照）。
 * \retval  E_NOT_OK  COM 未初期化、PduId が TX I-PDU 設定テーブルに
 *                    存在しない、または I-PDU が stopped。
 *
 * \AUTOSARReq     {SWS_Com_00861, SWS_Com_00388, SWS_Com_00492}
 * \ServiceID      {0x17}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType PduId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_TRIGGER_IPDU_SEND, COM_E_UNINIT);
        return E_NOT_OK;
    }

    if (PduId >= COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "TriggerIPDUSend E: PduId=%u out of range (max=%u)",
                 (unsigned)PduId, (unsigned)COM_TX_IPDU_MAX);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_TRIGGER_IPDU_SEND, COM_E_PARAM);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindTxIPdu(PduId);
    if (ipdu == NULL)
    {
        DET_LOGE(TAG, "TriggerIPDUSend E: PduId=%u not a registered TX I-PDU", (unsigned)PduId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_TRIGGER_IPDU_SEND, COM_E_PARAM);
        return E_NOT_OK;
    }

    if (!Com_TxIPduStarted[PduId])
    {
        /* [SWS_Com_00861]: stopped I-PDU は単に E_NOT_OK。開発エラーによる
         * 失敗とは別区分のため Det_ReportError() は呼ばない（DET ログのみ、
         * Com_InvalidateSignal() の InvalidValueConfigured==0 判定と同じ
         * 方針）。 */
        DET_LOGW(TAG, "TriggerIPDUSend: PduId=%u is stopped", (unsigned)PduId);
        return E_NOT_OK;
    }

    Com_TxTriggerPending[PduId] = 1U;
    return E_OK;
}

/**
 * \brief   TX I-PDU の TMS（Transmission Mode Selector）状態を明示的に切り替える。
 *
 * \details `Com_TmsState[PduId]` を直接書き換える、シグナル値に基づく自動
 *          評価（`Com_RecalcTms()`）とは独立したもう一つの TMS 変更経路。
 *          要求済みの Mode が既に現在の状態と同じ場合は何もしない（spec 原文
 *          "the call will have no effect"）。DIRECT/MIXED/PERIODIC 遷移ごとの
 *          即時送信・周期タイマ再始動の詳細、自動評価と混在させる場合の注意、
 *          `ComTxModeTimeOffset` 省略の理由は
 *          docs/modules/Com_Notes.md「Com_SwitchIpduTxMode」参照。
 *
 * \param[in]  PduId  TMS 状態を切り替える TX I-PDU の ID。
 * \param[in]  Mode   新しい TMS 状態（TRUE/FALSE）。
 *
 * \AUTOSARReq     {SWS_Com_00881, SWS_Com_00239, SWS_Com_00244}
 * \ServiceID      {0x27}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
void Com_SwitchIpduTxMode(Com_IPduIdType PduId, boolean Mode)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SWITCH_IPDU_TX_MODE, COM_E_UNINIT);
        return;
    }

    if (PduId >= COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "SwitchIpduTxMode E: PduId=%u out of range (max=%u)",
                 (unsigned)PduId, (unsigned)COM_TX_IPDU_MAX);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SWITCH_IPDU_TX_MODE, COM_E_PARAM);
        return;
    }

    const Com_IPduConfigType* ipdu = Com_FindTxIPdu(PduId);
    if (ipdu == NULL)
    {
        DET_LOGE(TAG, "SwitchIpduTxMode E: PduId=%u not a registered TX I-PDU", (unsigned)PduId);
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_SWITCH_IPDU_TX_MODE, COM_E_PARAM);
        return;
    }

    const uint8 newState = Mode ? 1U : 0U;
    if (Com_TmsState[PduId] == newState)
        return;  /* spec 原文: "the call will have no effect" */

    Com_TmsState[PduId] = newState;

    /* [SWS_Com_00244] 周期タイマ再始動。PERIODIC のみここで直接
     * Com_TxLastSentMs を更新する理由は docs/modules/Com_Notes.md
     * 「Com_SwitchIpduTxMode」参照。DIRECT/MIXED 側で触らない理由（非自明）:
     * MinDelayMs>0 の I-PDU では、ここでリセットすると直後の
     * Com_RequestTxOnChange() による「即時」送信要求が MDT 未経過と
     * 誤判定されて遅延してしまうため。 */
    if (Com_EffectiveTxModeMode(ipdu) == COM_TX_MODE_PERIODIC)
    {
        Com_TxLastSentMs[PduId] = millis();
    }
    else
    {
        Com_RequestTxOnChange(ipdu);
    }
}

typedef void (*Com_VoidCbkType)(void);

/* Com_InvokeTxNotification() が「TxAckCbk・TxErrCbk・TxTOutCbk のどれを
 * 配送するか」を選ぶための判別子。2026-08 のレビューでは「呼び出し先が
 * 2 種類しかないため三項演算子で十分、関数ポインタテーブルは過剰な抽象化」
 * と判断したが、Com_CbkTxTOut（TX 送信デッドライン監視）追加で 3 種類に
 * なった。3 種とも呼び出し側がコンパイル時に知っている固定種別のままで
 * あることは変わらないため、関数ポインタテーブルへは寄せず、三項演算子を
 * switch 文に置き換えるだけで対応する（同じ判断基準の延長）。 */
typedef enum
{
    COM_TX_NOTIFY_ACK  = 0,
    COM_TX_NOTIFY_ERR  = 1,
    COM_TX_NOTIFY_TOUT = 2
} Com_TxNotifyKindType;

/**
 * \brief   TxAckCbk/TxErrCbk/TxTOutCbk（Com_CbkTxAck/Com_CbkTxErr/
 *          Com_CbkTxTOut、SWS_Com_00468/SWS_Com_00491/SWS_Com_00554）
 *          共通の配送ロジック。
 *
 * \details 実 AUTOSAR はいずれのコールバックも signal 単位/signal group
 *          単位で別々のコールバック名（`Rte_COMCbkTAck_<sn>`/`<sg>`、
 *          `Rte_COMCbkTErr_<sn>`/`<sg>`、`Rte_COMCbkTxTOut_<sn>`/`<sg>`）を
 *          持てる。`Com_TxConfirmation()`（TxAckCbk/TxTOutCbk 解除側）・
 *          `Com_IpduGroupStop()`（TxErrCbk 側）・`Com_MainFunctionTx()`
 *          （TxTOutCbk 発火側）は「どのコールバックか」以外は完全に同じ
 *          配送ロジック（Signal Group ならグループ単位で 1 回、そうでなければ
 *          この I-PDU に属する TX シグナルのうち該当コールバックが設定
 *          されているものすべてを呼ぶ）のため、2026-08 のレビューで指摘
 *          された重複をここへ集約した。
 *
 * \param[in]  ipdu   対象 TX I-PDU 設定。NULL 可（NULL は「Signal Group では
 *                    ない」扱いとし、下記シグナル走査へ進む。Com_FindTxIPdu()
 *                    が見つけられなかった場合に備える、Com_TxConfirmation()
 *                    参照）。
 * \param[in]  TxPduId 対象 TX I-PDU の ID（シグナル走査時の `sig->IPduId`
 *                    一致判定に使う）。
 * \param[in]  kind   配送するコールバックの種別。
 *
 * \pre        Com_ConfigPtr が NULL でないこと（呼び出し元で保証する）。
 *
 * \ServiceID      {0xF2}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_InvokeTxNotification(const Com_IPduConfigType* ipdu,
                                      Com_IPduIdType TxPduId,
                                      Com_TxNotifyKindType kind)
{
    DET_LOGT(TAG, "called");

    if (ipdu != NULL && ipdu->IsSignalGroup != 0U)
    {
        Com_VoidCbkType groupCbk = NULL;
        switch (kind)
        {
        case COM_TX_NOTIFY_ACK:  groupCbk = ipdu->TxAckCbk;  break;
        case COM_TX_NOTIFY_ERR:  groupCbk = ipdu->TxErrCbk;  break;
        case COM_TX_NOTIFY_TOUT: groupCbk = ipdu->TxTOutCbk; break;
        }
        if (groupCbk != NULL)
            groupCbk();
        return;
    }

    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        /* Direction のチェックが必須: RX I-PDU と TX I-PDU の IPduId は
         * 別々の値空間（どちらも 0 始まり）のため、IPduId の一致だけでは
         * 方向を判別できない（例: RX の EngineInfo=0 と TX の
         * MeterStatus=0）。詳細は Com_SignalDirectionType の宣言コメント参照。 */
        if (sig->Direction != COM_SIGNAL_DIRECTION_TX || sig->IPduId != TxPduId)
            continue;

        Com_VoidCbkType cbk = NULL;
        switch (kind)
        {
        case COM_TX_NOTIFY_ACK:  cbk = sig->TxAckCbk;  break;
        case COM_TX_NOTIFY_ERR:  cbk = sig->TxErrCbk;  break;
        case COM_TX_NOTIFY_TOUT: cbk = sig->TxTOutCbk; break;
        }
        if (cbk != NULL)
            cbk();
    }
}

/**
 * \brief   TX I-PDU の送信完了を COM へ通知し、ComNotification（TxAck）を配送する。
 *
 * \details CAN フレームの送信完了後に PduR から呼び出される。まずログ出力を
 *          行い、result==E_OK（送信成功）であれば、この I-PDU（TxPduId、
 *          Com_IPduIdType と同一の値空間。PduR_PBCfg.c の ConfDestPduId 参照）
 *          の TxAck 通知を行う（Com_CbkTxAck、SWS_Com_00468: "called
 *          immediately after successful transmission of the I-PDU
 *          containing the message"）。
 *
 *          実 AUTOSAR は signal 単位/signal group 単位で別々のコールバック名
 *          （Rte_COMCbkTAck_<sn>/<sg>）を持てる（SWS_Com_00468 "It can be
 *          configured for signals and signal groups"）。この区別（Signal
 *          Group ならグループ単位で 1 回、そうでなければ TX シグナル単位で
 *          走査）の実体は `Com_IpduGroupStop()` の TxErrCbk 配送と共通の
 *          `Com_InvokeTxNotification()` に集約されている（詳細は同関数の
 *          コメント参照。2026-08 に個別実装→統一実装→共通ヘルパー化と
 *          段階的に是正した経緯は docs/modules/Com_Notes.md 参照）。
 *          いずれの場合も、値がこの送信で実際に変化したかどうかは問わない
 *          （I-PDU が送信されたという事実だけで通知する）。
 *
 *          呼び出しコンテキストについて（Rx 無効値検知の実機障害を踏まえた
 *          確認事項）: この関数は Can_MainFunction_Write()（Os の 100ms
 *          タスク）から CanIf_TxConfirmation() → PduR_CanIfTxConfirmation()
 *          経由で同期的に呼ばれる。この経路上に SchM 排他エリア（割り込み
 *          禁止区間）は存在しないため、`TxAckCbk` 内で Serial 出力等の
 *          ブロッキング処理を行っても、Rx 無効値検知（ComInvalidNotification）
 *          で発生した WDT リセット障害と同じ問題は起きない（呼び出しチェーンを
 *          実際にたどって確認済み）。
 *
 * \param[in]  TxPduId  送信が完了した TX I-PDU の PduR 層 PDU ID
 *                      （= Com_IPduIdType と同一の値空間）。
 * \param[in]  result   CanIf から転送された送信結果。
 *                      E_OK = 成功、E_NOT_OK = 失敗。成功/失敗いずれの場合も
 *                      「送信済み・未確認」状態（Com_TxConfPending[]）は解除
 *                      する（確認自体は届いたため）。TX リトライやエラー
 *                      カウンタは実装しない。ComTxModeNumberOfRepetitions
 *                      （SWS_Com_00305）の残り再送回数は本関数ではなく
 *                      Com_MainFunctionTx() が dispatch 時点で減らす（理由は
 *                      同関数のコメント参照）。Com_CbkTxErr（SWS_Com_00491）は
 *                      本関数ではなく Com_IpduGroupStop() 側で、確認が届く
 *                      前に I-PDU Group が停止された場合にのみ呼ばれる
 *                      （SWS_Com_00491 原文 "called in case the transmission
 *                      is not possible because the corresponding I-PDU group
 *                      is stopped" のとおり、result==E_NOT_OK 自体は
 *                      Com_CbkTxErr の発火条件ではない）。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \note       result が実際に E_NOT_OK になる経路は現状存在しない。呼び出し元の
 *             CanIf_TxConfirmation() が result を受け取らない 1 引数 API で、
 *             内部で常に E_OK 決め打ちで呼び出すため（さらにその手前の
 *             Can_Write() は送信成功時のみ CanIf_TxConfirmation() を呼ぶ。
 *             MCP2515 との SPI 通信が同期的なため）。
 *
 * \AUTOSARReq     {SWS_Com_00124, SWS_Com_00468, SWS_Com_00880}
 * \ServiceID      {0x40}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    DET_LOGI(TAG, "TxConf id=%u", (unsigned)TxPduId);

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_TX_CONFIRMATION, COM_E_UNINIT);
        return;
    }

    /* [SWS_Com_00800]: 停止中の I-PDU に対する送信確認は無視する。
     * TxPduId は Com の TX IPduId 空間（Com_FindTxIPdu() 参照）。
     * Com_IpduGroupStop() が既に Com_TxConfPending[] をクリアして
     * TxErrCbk 発火済みのため、ここでは重ねて処理しない。 */
    if (TxPduId < COM_TX_IPDU_MAX && !Com_TxIPduStarted[TxPduId])
    {
        DET_LOGD(TAG, "TxConf ignored (I-PDU Group stopped) iPdu=%u", (unsigned)TxPduId);
        return;
    }

    /* 確認が到達した（成功/失敗を問わず）ため「送信済み・未確認」を解除する。
     * [SWS_Com_00880]: 確認到達時は TX 送信デッドライン監視タイマも解除する
     * （成功/失敗を問わない、原文に「成功時のみ」という限定は無い）。
     * Com_TxUsingFirstTimeout も同時に false へ倒す（確認到達＝1サイクル
     * 完了とみなし、以降は steady TxTimeoutMs を使う。
     * Com_RxIndication() 側の Com_RxUsingFirstTimeout クリアと対称）。 */
    if (TxPduId < COM_TX_IPDU_MAX)
    {
        Com_TxConfPending[TxPduId]       = 0U;
        Com_TxTimedOut[TxPduId]          = 0U;
        Com_TxUsingFirstTimeout[TxPduId] = 0U;
    }

    if (result != E_OK)
        return;

    const Com_IPduConfigType* ipdu = Com_FindTxIPdu(TxPduId);
    Com_InvokeTxNotification(ipdu, TxPduId, COM_TX_NOTIFY_ACK);
}

/**
 * \brief   受信デッドライン監視タイムアウトを周期的に検出する。
 *
 * \details Os の 100 ms タスクから呼び出される。
 *          TimeoutMs > 0 の各 RX I-PDU について、最終受信からの経過時間を確認し、
 *          設定値を超えた場合に Com_RxTimedOut フラグを立てて WARN ログを出力する。
 *          その後 Com_ReceiveSignal() は当該 I-PDU のシグナルに対して E_NOT_OK を返し、
 *          上位層（RTE → ASW）がフェイルセーフ処理（FAULT 遷移など）を実施する。
 *
 *          タイムアウトは Com_RxIndication() でフレームを受信するまで継続する。
 *
 *          診断 CommunicationControl (UDS 0x28) による受信抑制中
 *          (Com_RxEnabled==0) はデッドライン監視自体を評価しない
 *          （SWS_Com_00684/SWS_Com_00685: I-PDU が停止された間は受信処理・
 *          デッドライン監視の両方を無効化する要求に対応。抑制中に
 *          Com_RxTimedOut を新規に立ててしまうと、意図的に止めているだけの
 *          通信を「通信異常」として誤って上位層へ伝えてしまう）。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 *          ComInvalidNotification のディスパッチ（Com_RxInvalidNotifyPending
 *          参照）: Com_ReceiveSignal() が ComDataInvalidAction=NOTIFY の
 *          無効値受信を検知した際、その場ではフラグを立てるだけで
 *          InvalidNotificationCbk() を直接呼ばない。実際の呼び出しは必ず
 *          本関数の冒頭で行う。理由: Com_ReceiveSignal() は Rte 層の
 *          SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()（グローバル割り込み
 *          禁止）の内側から呼ばれることがあり、そこでコールバックを直接
 *          呼ぶと、コールバックが Serial 出力のような割り込み駆動の I/O を
 *          行った場合に割り込み禁止のまま停止し続け、WDT リセットを
 *          引き起こしうる（実機で確認済み）。
 *
 * \note    実仕様は本関数（Com_MainFunctionRx）と Com_MainFunctionTx に
 *          分かれている（単体の Com_MainFunction は4.3.1仕様書に存在しない。
 *          2026-08、シグネチャ準拠のため分割。旧実装は両者を1関数に
 *          まとめていた経緯があり、TX側の設計判断（実送信を本関数側へ
 *          一元化する理由等）は Com_MainFunctionTx() の Doxygen コメントを
 *          参照）。
 *
 * \AUTOSARReq     {SWS_Com_00398, SWS_Com_00684, SWS_Com_00685}
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_MainFunctionRx(void)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_MAIN_FUNCTION_RX, COM_E_UNINIT);
        return;
    }

    const unsigned long now = millis();

    /* ComInvalidNotification のディスパッチ（Com_RxInvalidNotifyPending 参照）。
     * Com_ReceiveSignal() が割り込み禁止区間から呼ばれた場合でも安全なように、
     * 実際のコールバック呼び出しは必ずここ（Os の 100ms タスク、割り込み
     * 禁止区間の外）で行う。 */
    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        if (!Com_RxInvalidNotifyPending[s])
            continue;

        Com_RxInvalidNotifyPending[s] = 0U;
        if (Com_ConfigPtr->Signals[s].InvalidNotificationCbk != NULL)
            Com_ConfigPtr->Signals[s].InvalidNotificationCbk();
    }

    /* RX ComFilterAlgorithm=NEW_IS_WITHIN の FilterRejectCbk ディスパッチ
     * （Com_RxFilterRejectPending 参照）。上記 InvalidNotificationCbk と
     * 全く同じ理由・同じ仕組みで、必ずここ（割り込み禁止区間の外）で呼ぶ。 */
    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        if (!Com_RxFilterRejectPending[s])
            continue;

        Com_RxFilterRejectPending[s] = 0U;
        if (Com_ConfigPtr->Signals[s].FilterRejectCbk != NULL)
            Com_ConfigPtr->Signals[s].FilterRejectCbk();
    }

    if (Com_RxEnabled != 0U)
    {
        for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
        {
            const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
            if (ipdu->TimeoutMs == 0U)
                continue;  /* [SWS_Com_00333]: 監視無効（FirstTimeoutMs も無視） */

            /* I-PDU Group が停止中はデッドライン監視自体を評価しない
             * （[SWS_Com_00685]。Com_RxEnabled==0 の場合と同じ理由：意図的に
             * 止めているだけの通信を「通信異常」として誤って伝えないため）。
             * Com_DisableReceptionDM() による個別無効化も同じ扱い
             * （SRS_Com_00192、2026-08 追加）。 */
            if (!Com_RxIPduStarted[ipdu->IPduId] || !Com_RxDmEnabled[ipdu->IPduId])
                continue;

            const Com_IPduIdType id = ipdu->IPduId;

            /* [SWS_Com_00787] 項目2/[SWS_Com_00716]/[SWS_Com_00879] 相当:
             * 再始動以降まだ受信していない間は FirstTimeoutMs（ComFirstTimeout）、
             * 1 回でも受信済みなら定常状態の TimeoutMs（ComTimeout）を使う。
             * FirstTimeoutMs==0 は「初回受信までは監視しない」を意味する
             * （[SWS_Com_00716]。TimeoutMs 自体は非 0 のためこの分岐にのみ
             * 適用され、初回受信後の定常監視には影響しない）。 */
            const uint16 threshold = Com_SelectTimeoutThreshold(
                Com_RxUsingFirstTimeout[id], ipdu->FirstTimeoutMs, ipdu->TimeoutMs);
            if (threshold == 0U)
                continue;

            if (!Com_RxTimedOut[id] &&
                (now - Com_RxLastMs[id]) >= (unsigned long)threshold)
            {
                Com_RxTimedOut[id] = 1U;
                DET_LOGW(TAG, "RX timeout iPdu=%u (%ums, %s)",
                         (unsigned)id, (unsigned)threshold,
                         Com_RxUsingFirstTimeout[id] ? "first" : "steady");

                /* [SWS_Com_00536]/[SWS_Com_00556] (Com_CbkRxTOut)、Signal
                 * Group 単位のみ（[7.3.6]: グループ全体で1つのデッドライン
                 * として扱われるため、このI-PDU単位ループがグループの
                 * 発火点になる。非 Signal Group の発火は下のシグナル単位
                 * ループが別途担う）。 */
                if (ipdu->IsSignalGroup != 0U && ipdu->RxTOutCbk != NULL)
                    ipdu->RxTOutCbk();
            }
        }

        /* シグナル単位のデッドライン監視（[7.3.6]、Com_SignalConfigType の
         * FirstTimeoutMs/TimeoutMs 宣言コメント参照）。Signal Group メンバー
         * は対象外（設定上は TimeoutMs=0 の既定のままにする規約だが、それ
         * だけに頼らず Com_RxIPduIsGroup[] をランタイムガードとしても
         * 確認する——Com_CbkRxAck の ipdu->IsSignalGroup チェックや
         * NumberOfRepetitions の TxModeMode==DIRECT チェックと同じ「設定
         * 判別フィールドに対する実行時ガード」の考え方。グループの
         * deadline は上の I-PDU 単位ループが別途担う）。I-PDU Group 停止中は
         * 上と同じ理由でスキップする。 */
        for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
        {
            const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
            if (sig->Direction != COM_SIGNAL_DIRECTION_RX || sig->TimeoutMs == 0U)
                continue;  /* [SWS_Com_00333]: 監視無効（FirstTimeoutMs も無視） */

            if (sig->IPduId >= COM_RX_IPDU_MAX || !Com_RxIPduStarted[sig->IPduId]
                || !Com_RxDmEnabled[sig->IPduId])
                continue;

            if (Com_RxIPduIsGroup[sig->IPduId] != 0U)
                continue;  /* Signal Group メンバーは対象外（上記コメント参照） */

            const uint16 sigThreshold = Com_SelectTimeoutThreshold(
                Com_RxUsingFirstTimeout[sig->IPduId], sig->FirstTimeoutMs, sig->TimeoutMs);
            if (sigThreshold == 0U)
                continue;  /* [SWS_Com_00716]: 初回受信までは監視しない */

            if (!Com_SigTimedOut[s] &&
                (now - Com_RxLastMs[sig->IPduId]) >= (unsigned long)sigThreshold)
            {
                Com_SigTimedOut[s] = 1U;
                DET_LOGW(TAG, "RX timeout sig=%u iPdu=%u (%ums, %s)",
                         (unsigned)sig->SignalId, (unsigned)sig->IPduId,
                         (unsigned)sigThreshold,
                         Com_RxUsingFirstTimeout[sig->IPduId] ? "first" : "steady");
                /* [SWS_Com_00536]/[SWS_Com_00556] (Com_CbkRxTOut)、非 Signal
                 * Group のシグナル単位。 */
                if (sig->RxTOutCbk != NULL)
                    sig->RxTOutCbk();
            }
        }
    }
    /* Com_RxEnabled==0 の間はデッドライン監視自体を無効化する
     * (SWS_Com_00684/00685)。 */
}

/**
 * \brief   送信スケジューリング（周期送信・変化時送信・再送）と送信確認
 *          デッドライン監視を周期的に処理する。
 *
 * \details Os の 100 ms タスクから呼び出される（Com_MainFunctionRx() とは
 *          独立したタスクとして登録される。Os_PBCfg.c 参照）。
 *
 *          TX I-PDU の送信ディスパッチ（DIRECT/MIXED/PERIODIC 共通）:
 *          実際に PduR_ComTransmit()（→ MCP2515 への SPI 送信）を呼ぶのはこの
 *          関数だけである。判定に使う `TxModeMode`/`TxPeriodMs` は
 *          `Com_EffectiveTxModeMode()`/`Com_EffectiveTxPeriodMs()` 経由で
 *          TMS（Transmission Mode Selector、`Com_TmsState[]`）評価済みの
 *          実効値を使う（TMS を持たない I-PDU は常に基本の TxModeMode/
 *          TxPeriodMs のまま）。DIRECT/MIXED の変化時送信は
 *          `Com_RequestTxOnChange()` が立てた `Com_TxPending[]` を、
 *          MIXED/PERIODIC の周期送信は実効 TxPeriodMs からの経過時間を
 *          それぞれ判定材料にする:
 *            - DIRECT   : Com_TxPending[] が立っており、かつ MDT
 *                          （ComMinimumDelayTime、下記）を満たせば送信
 *            - MIXED    : (Com_TxPending[] が立っており、かつ MDT を満たす)、
 *                          または経過時間が実効 TxPeriodMs（周期フロア間隔）
 *                          を超えたら送信（周期フロアには MDT を適用しない）
 *            - PERIODIC : 経過時間が実効 TxPeriodMs を超えたら常に送信
 *                          （Com_TxPending[]・MDT のいずれも使用しない）
 *          上記いずれのモードでも、`Com_TriggerIPDUSend()` が立てた
 *          `Com_TxTriggerPending[]`（MDT のみ尊重、[SWS_Com_00861]/
 *          [SWS_Com_00388]）が独立した OR 項として効く（詳細は同フラグの
 *          宣言コメント参照）。
 *
 *          MDT（`ipdu->MinDelayMs`、DaVinci: ComMinimumDelayTime）: DIRECT/
 *          MIXED I-PDU の変化時送信について、直近の実送信から MinDelayMs
 *          未満しか経過していなければ送信を保留する（Com_TxPending[] は
 *          立てたまま破棄しない。次回以降の呼び出しで経過時間を満たし次第
 *          送信する）。MIXED の周期フロアには適用しない
 *          （SWS_Com_00789 の既定動作 [ComEnableMDTForCyclicTransmission=false]
 *          に合わせている）。MinDelayMs=0 の I-PDU は常に満了扱いのため、
 *          MDT 未設定の I-PDU の挙動に影響しない（SWS_Com_00471）。
 *
 *          実送信を Com_SendSignal()/Com_SendSignalGroup() の呼び出し元
 *          （ASW Runnable）ではなく本関数（Os の 100ms タスク、WdgM 非監視）
 *          側に一元化することで、バス輻輳時に `sendMsgBuf()` の TX バッファ
 *          空き待ちが伸びても、WdgM の Deadline Supervision 対象である
 *          ASW Runnable の実行時間には影響しない。ASW/CDD は
 *          `Com_SendSignal()` で値を更新するだけでよく、送信タイミング・
 *          TMS のいずれにも一切関与しない（実車の Com と同じ責務分離）。
 *          診断 CommunicationControl (UDS 0x28) による送信抑制中
 *          (Com_TxEnabled==0) は送信自体を行わないが、`Com_TxPending[]` の
 *          クリアと `Com_TxLastSentMs` の更新は行う（SWS_Com_00777/
 *          SWS_Com_00334: 停止中に発生した送信要求は保持されず、再開しても
 *          古いトリガーで即座に送信されることはない。抑制解除直後に
 *          「抑制中に溜まった分」を connectivity 復帰の合図として即座に
 *          送ってしまわないようにするため）。
 *
 *          ComTxModeNumberOfRepetitions（`ipdu->NumberOfRepetitions`/
 *          `RepetitionPeriodMs`、SWS_Com_00305）: Com_TxRepeatApplicable() が
 *          真の I-PDU のみ対象。残り再送回数（`Com_TxRepeatsRemaining[]`）が
 *          0 より大きく、かつ直近送信から RepetitionPeriodMs 以上経過して
 *          いれば再送する（repeatDue、changeDue/floorDue と OR）。残り回数を
 *          いつ・どう減らすかの詳細（オフバイワン対策・CommunicationControl
 *          無効中の扱い）は下の実装コメント・docs/modules/Com_Notes.md 参照。
 *
 *          TX 送信デッドライン監視（`ipdu->TxFirstTimeoutMs`/`TxTimeoutMs`/
 *          `TxTOutCbk`、Com_CbkTxTOut、SWS_Com_00878/00879/00880/00304/
 *          00554）: TX ディスパッチループの後段で、`Com_TxConfPending[]`が
 *          立ったまま`TxTimeoutMs`（初回は`TxFirstTimeoutMs`）を超えた
 *          I-PDU を検出し`TxTOutCbk`を発火する。実機では発動しない
 *          （理由は docs/modules/Com_Notes.md 参照）。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Com_00399, SWS_Com_00734, SWS_Com_00742, SWS_Com_00743,
 *                  SWS_Com_00777, SWS_Com_00032, SWS_Com_00799, SWS_Com_00471,
 *                  SWS_Com_00698, SWS_Com_00789, SWS_Com_00305, SWS_Com_00467,
 *                  SWS_Com_00392, SWS_Com_00878, SWS_Com_00879, SWS_Com_00880,
 *                  SWS_Com_00304, SWS_Com_00554, SWS_Com_00861, SWS_Com_00388}
 * \ServiceID      {0x19}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_MainFunctionTx(void)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_MAIN_FUNCTION_TX, COM_E_UNINIT);
        return;
    }

    const unsigned long now = millis();

    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        const Com_IPduIdType      id   = ipdu->IPduId;

        /* I-PDU Group が停止中は due 判定自体を行わない。保留中の送信要求は
         * Com_IpduGroupStop() が既にキャンセル済み（[SWS_Com_00777]）のため、
         * ここで due=1 になることはないはずだが、防御的に早期 continue する。 */
        if (!Com_TxIPduStarted[id])
            continue;

        const Com_TxModeModeType  mode   = Com_EffectiveTxModeMode(ipdu);
        const uint16              period = Com_EffectiveTxPeriodMs(ipdu);

        uint8 due;
        /* ComTxModeNumberOfRepetitions（SWS_Com_00305）用。この due 判定が
         * changeDue/repeatDue どちらに由来するかを、下の dispatch 後の
         * 残り回数デクリメント判定で使うため if/else の外に出しておく。 */
        uint8 changeDue = 0U;
        uint8 repeatDue = 0U;
        if (mode == COM_TX_MODE_PERIODIC)
        {
            const unsigned long elapsed = now - Com_TxLastSentMs[id];
            /* Com_TriggerIPDUSend()（[SWS_Com_00861]/[SWS_Com_00388]）は
             * TxModeMode によらず効く必要があるため、PERIODIC I-PDU でも
             * MDT のみ尊重して OR する（Com_TxTriggerPending 宣言コメント
             * 参照）。 */
            const uint8 triggerDue = Com_TxTriggerPending[id]
                                      && (elapsed >= (unsigned long)ipdu->MinDelayMs);
            due = (elapsed >= (unsigned long)period) || triggerDue;
        }
        else
        {
            const unsigned long elapsed  = now - Com_TxLastSentMs[id];
            const uint8         floorDue = (mode == COM_TX_MODE_MIXED)
                                    && (elapsed >= (unsigned long)period);
            /* MDT（ComMinimumDelayTime）: 変化時送信（Com_TxPending 経由）にのみ
             * 適用し、MIXED の周期フロア（floorDue）には適用しない
             * （SWS_Com_00789 の既定動作。MinDelayMs=0 なら常に満了扱いのため
             * MDT 未設定の I-PDU では以前と同じ挙動になる）。満了前に変化検知が
             * あっても Com_TxPending は立てたまま保持し、破棄しない
             * （次回 Com_MainFunctionTx() で再判定する）。 */
            const uint8 mdtElapsed = elapsed >= (unsigned long)ipdu->MinDelayMs;
            changeDue = (Com_TxPending[id] != 0U) && mdtElapsed;
            /* ComTxModeNumberOfRepetitions（SWS_Com_00305）。再送専用の
             * タイマーは持たず、changeDue/floorDue と同じ Com_TxLastSentMs/
             * elapsed を流用する。減算条件の詳細は下のコメント参照。 */
            repeatDue = Com_TxRepeatApplicable(mode)
                        && (Com_TxRepeatsRemaining[id] > 0U)
                        && (elapsed >= (unsigned long)ipdu->RepetitionPeriodMs);
            /* changeDue には混ぜない — 宣言コメント（本ファイル冒頭の
             * Com_TxTriggerPending）の repeatDue/changeDue 分離理由を参照。 */
            const uint8 triggerDue = Com_TxTriggerPending[id] && mdtElapsed;
            due = changeDue || floorDue || repeatDue || triggerDue;
        }

        if (!due)
            continue;

        Com_TxPending[id]    = 0U;
        Com_TxTriggerPending[id] = 0U;
        Com_TxLastSentMs[id] = now;

        if (Com_TxEnabled == 0U)
        {
            DET_LOGD(TAG, "TX skip iPdu=%u (CommunicationControl disabled)", (unsigned)id);
            continue;
        }

        /* [SWS_Com_00305] 残り再送回数のデクリメント。dispatch 直前（＝実際に
         * Com_DoTransmit() を呼ぶ場合）のみ行う: CommunicationControl 無効中
         * （上で continue した場合）に空費すると、再開後に本来送るべき再送が
         * 残っていない事態になりかねない（/code-review で指摘）。
         *
         * `repeatDue && !changeDue` の `!changeDue` も /code-review で指摘
         * されたオフバイワン対策: elapsed（直近の実送信からの経過時間）は
         * 新規送信要求の時刻ではリセットしないため、前回の実送信から
         * RepetitionPeriodMs 以上経ってから新しい変化が来ると、その「初回」
         * 送信の時点で偶然 repeatDue も真になりうる（changeDue と同時に真）。
         * これを再送1回分と誤カウントすると、計 N+1 回ではなく N 回で
         * 止まってしまう。 */
        if (repeatDue && !changeDue)
            Com_TxRepeatsRemaining[id]--;

        (void)Com_DoTransmit(ipdu, now);
    }

    /* TX 送信デッドライン監視（Com_CbkTxTOut）。ディスパッチループの後段に
     * 置くのは、この呼び出し内でそのループが今まさに作った
     * Com_TxConfPending/Com_TxConfPendingSinceMs を読むため。
     * Com_TxIPduStarted[id] のチェックは「停止中のグループを評価しない」
     * という RX 側と同じ目的で、TxErrCbk との二重発火防止そのものは
     * Com_IpduGroupStop() 側が Com_TxConfPending を無条件クリアすることで
     * 担っている（同関数のコメント参照）。設計の詳細・SWS 引用・
     * Com_TxEnabled ゲートの経緯は docs/modules/Com_Notes.md 参照。 */
    if (Com_TxEnabled != 0U)
    {
        for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
        {
            const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
            if (ipdu->TxTimeoutMs == 0U)
                continue;

            const Com_IPduIdType id = ipdu->IPduId;
            if (!Com_TxIPduStarted[id] || Com_TxConfPending[id] == 0U)
                continue;  /* 未送信、または既に確認済みの I-PDU は対象外 */

            const uint16 threshold = Com_SelectTimeoutThreshold(
                Com_TxUsingFirstTimeout[id], ipdu->TxFirstTimeoutMs, ipdu->TxTimeoutMs);
            if (threshold == 0U)
                continue;

            if (!Com_TxTimedOut[id] &&
                (now - Com_TxConfPendingSinceMs[id]) >= (unsigned long)threshold)
            {
                Com_TxTimedOut[id] = 1U;
                DET_LOGW(TAG, "TX confirmation timeout iPdu=%u (%ums, %s)",
                         (unsigned)id, (unsigned)threshold,
                         Com_TxUsingFirstTimeout[id] ? "first" : "steady");
                Com_InvokeTxNotification(ipdu, id, COM_TX_NOTIFY_TOUT);
            }
        }
    }
}

/**
 * \brief   RX I-PDU 1 本分のデッドライン監視タイマを再始動する。
 *
 * \details Com_SetCommunicationEnabled() の受信再開時と Com_IpduGroupStart() が
 *          共通して行う手順（[SWS_Com_00787] 相当）をまとめたもの。
 *          Com_RxLastMs を現在時刻へリセットしないと、TimeoutMs 以上の時間
 *          受信を抑制していた場合、再有効化した直後（次の Com_MainFunctionRx()
 *          呼び出し）で古い Com_RxLastMs のまま即座にタイムアウト判定されて
 *          しまう。既に立っていた Com_RxTimedOut/Com_SigTimedOut も、抑制中の
 *          「経過時間」を理由に上位層へ通信異常と伝え続けないよう、あわせて
 *          クリアする。再始動直後は ComFirstTimeout 相当（FirstTimeoutMs）
 *          から監視を始める。
 *
 * \param[in]  id   対象 RX I-PDU の ID。
 * \param[in]  now  基準時刻（millis()）。
 *
 * \pre        Com_ConfigPtr が NULL でないこと。
 */
static void Com_ResetRxDeadlineMonitoring(Com_IPduIdType id, unsigned long now)
{
    DET_LOGT(TAG, "called");
    Com_RxLastMs[id]   = now;
    Com_RxTimedOut[id] = 0U;
    Com_RxUsingFirstTimeout[id] = 1U;

    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        if (Com_ConfigPtr->Signals[s].Direction == COM_SIGNAL_DIRECTION_RX
            && Com_ConfigPtr->Signals[s].IPduId == id)
        {
            Com_SigTimedOut[s] = 0U;
        }
    }
}

void Com_SetCommunicationEnabled(uint8 RxEnabled, uint8 TxEnabled)
{
    DET_LOGT(TAG, "called");

    if (Com_RxEnabled != RxEnabled || Com_TxEnabled != TxEnabled)
    {
        DET_LOGI(TAG, "CommunicationControl rx=%u->%u tx=%u->%u",
                 (unsigned)Com_RxEnabled, (unsigned)RxEnabled,
                 (unsigned)Com_TxEnabled, (unsigned)TxEnabled);
    }

    if (Com_RxEnabled == 0U && RxEnabled != 0U && Com_ConfigPtr != NULL)
    {
        const unsigned long now = millis();
        for (uint8 i = 0U; i < Com_ConfigPtr->RxIPduCount; i++)
        {
            Com_ResetRxDeadlineMonitoring(Com_ConfigPtr->RxIPdus[i].IPduId, now);
        }
    }

    Com_RxEnabled = RxEnabled;
    Com_TxEnabled = TxEnabled;
}

void Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean initialize)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_IPDU_GROUP_START, COM_E_UNINIT);
        return;
    }

    const unsigned long now = millis();

    for (uint8 i = 0U; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->IpduGroupId != IpduGroupId)
            continue;

        const Com_IPduIdType id = ipdu->IPduId;
        Com_RxIPduStarted[id] = 1U;

        /* [SWS_Com_00787] 項目2: 受信デッドライン監視タイマを再始動する
         * （Com_SetCommunicationEnabled() の再開時と同じ理由）。 */
        Com_ResetRxDeadlineMonitoring(id, now);

        if (initialize)
        {
            /* [SWS_Com_00222] 項目1: I-PDU のデータを ComSignalInitValue で
             * 初期化する（Com_Init() と同じ手順: バイト単位ゼロクリア →
             * ビット単位で InitValue 上書き。[SWS_Com_00217]）。 */
            Com_ResetBufferToInitValues(Com_RxBuffer[id], id, COM_SIGNAL_DIRECTION_RX);

            /* Com_RxLastValidValue も InitValue へ戻す（[SWS_Com_00228]:
             * 起動時点でまだ実際に受信していないシグナルは InitValue を
             * 返すべきという要求に対応。Com_Init() の該当コメント参照）。 */
            for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
            {
                const Com_SignalConfigType* rsig = &Com_ConfigPtr->Signals[s];
                if (rsig->Direction == COM_SIGNAL_DIRECTION_RX && rsig->IPduId == id)
                    Com_RxLastValidValue[s] = rsig->InitValue;
            }

            if (ipdu->IsSignalGroup != 0U)
            {
                /* [SWS_Com_00222] 項目2: Signal Group のシャドウバッファも
                 * 同じ手順で初期化する。未コミット状態（利用不可）へ戻す。 */
                Com_ResetBufferToInitValues(Com_RxShadowBuffer[id], id, COM_SIGNAL_DIRECTION_RX);
                Com_RxShadowTimedOut[id] = 1U;
            }
        }

        DET_LOGI(TAG, "IpduGroupStart grp=%u iPdu=%u(RX) init=%u",
                 (unsigned)IpduGroupId, (unsigned)id, (unsigned)initialize);
    }

    for (uint8 i = 0U; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if (ipdu->IpduGroupId != IpduGroupId)
            continue;

        const Com_IPduIdType id = ipdu->IPduId;
        Com_TxIPduStarted[id] = 1U;

        /* [SWS_Com_00787] 項目1/3: MDT・周期タイマの基準時刻を再始動する
         * （再開直後に「積み残し」として即座に送信されないようにする。
         * Com_SetCommunicationEnabled() の既存コメントと同じ考え方）。 */
        Com_TxLastSentMs[id] = now;
        Com_TxPending[id]    = 0U;
        Com_TxTriggerPending[id] = 0U;
        /* 起動直後は必ず「送信済み・未確認」状態もクリアしておく（前回の
         * Stop() で既にクリア済みのはずだが、初回 Start() 時の保険）。 */
        Com_TxConfPending[id] = 0U;
        /* ComTxModeNumberOfRepetitions（SWS_Com_00305）の残り再送回数も同様に
         * クリアする（前回 Stop() 時点の再送シーケンスを持ち越さない）。 */
        Com_TxRepeatsRemaining[id] = 0U;
        /* TX 送信デッドライン監視（SWS_Com_00878 等）も同様に再初期化する
         * （Com_ResetRxDeadlineMonitoring() の RX 側と対称）。 */
        Com_TxConfPendingSinceMs[id] = now;
        Com_TxTimedOut[id]           = 0U;
        Com_TxUsingFirstTimeout[id]  = 1U;

        /* [SWS_Com_00787] 項目4: update-bit をクリアする。 */
        if (ipdu->UpdateBitPosition != 0xFFU)
            Com_PackSignal(Com_TxBuffer[id], ipdu->UpdateBitPosition, 1U, COM_BIG_ENDIAN, 0U);

        if (initialize)
        {
            /* [SWS_Com_00222] 項目1: I-PDU のデータを ComSignalInitValue で
             * 初期化する（RX 側と同じ手順）。 */
            Com_ResetBufferToInitValues(Com_TxBuffer[id], id, COM_SIGNAL_DIRECTION_TX);

            /* [SWS_Com_00222] 項目3: フィルタの old_value も InitValue へ戻す
             * （Com_Init() の該当コメント参照。COM_FILTER_MASKED_NEW_DIFFERS_
             * MASKED_OLD が、再起動直後に InitValue と同じ値を送っただけで
             * 誤って「変化あり」と判定しないようにするため）。 */
            for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
            {
                const Com_SignalConfigType* tsig = &Com_ConfigPtr->Signals[s];
                if (tsig->Direction == COM_SIGNAL_DIRECTION_TX && tsig->IPduId == id)
                    Com_FilterLastValue[s] = tsig->InitValue;
            }

            if (ipdu->IsSignalGroup != 0U)
            {
                Com_ResetBufferToInitValues(Com_TxShadowBuffer[id], id, COM_SIGNAL_DIRECTION_TX);
            }
        }

        /* [SWS_Com_00223] I-PDU 起動時、現在のデータ内容から TMS を再評価する
         * （initialize の有無に関わらず。ゼロ初期化直後でも、TmsContributor
         * シグナルの初期値に基づいて正しく再評価される）。起動時の再評価は
         * SWS_Com_00495（送信トリガー）の対象ではないため戻り値は使わない。 */
        (void)Com_RecalcTms(id);

        DET_LOGI(TAG, "IpduGroupStart grp=%u iPdu=%u(TX) init=%u",
                 (unsigned)IpduGroupId, (unsigned)id, (unsigned)initialize);
    }
}

void Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_IPDU_GROUP_STOP, COM_E_UNINIT);
        return;
    }

    for (uint8 i = 0U; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->IpduGroupId != IpduGroupId)
            continue;

        /* [SWS_Com_00684]/[SWS_Com_00685]: 受信処理・デッドライン監視の両方を
         * 無効化する。Com_RxTimedOut は意図的にクリアしない（Started==0 の間
         * Com_MainFunctionRx() 側の評価自体を止めるため、値は参照されない）。 */
        Com_RxIPduStarted[ipdu->IPduId] = 0U;

        DET_LOGI(TAG, "IpduGroupStop grp=%u iPdu=%u(RX)",
                 (unsigned)IpduGroupId, (unsigned)ipdu->IPduId);
    }

    for (uint8 i = 0U; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if (ipdu->IpduGroupId != IpduGroupId)
            continue;

        const Com_IPduIdType id = ipdu->IPduId;
        Com_TxIPduStarted[id] = 0U;

        /* [SWS_Com_00479]/[SWS_Com_00491]: PduR へは渡した（実送信済み）が
         * 対応する Com_TxConfirmation() がまだ届いていない（＝未確認の）
         * I-PDU がこの停止時点で存在すれば、TxErrCbk（Com_CbkTxErr 相当）を
         * 即座に呼ぶ（signal/signal group 単位の配送は Com_TxConfirmation()
         * の TxAckCbk と共通の Com_InvokeTxNotification() を使う、同関数の
         * コメント参照）。呼び出し後は確認待ちでなくなるためフラグをクリア
         * する（後から届く Com_TxConfirmation() は Com_TxIPduStarted[id]==0
         * により無視される、上記参照）。 */
        if (Com_TxConfPending[id])
        {
            Com_InvokeTxNotification(ipdu, id, COM_TX_NOTIFY_ERR);
            Com_TxConfPending[id] = 0U;

            DET_LOGW(TAG, "IpduGroupStop grp=%u iPdu=%u(TX) unconfirmed at stop -> TxErrCbk",
                     (unsigned)IpduGroupId, (unsigned)id);
        }
        /* Com_TxTimedOut/Com_TxUsingFirstTimeout/Com_TxConfPendingSinceMs
         * （TX 送信デッドライン監視）は意図的にクリアしない（上の RX 側
         * Com_RxTimedOut と同じ理由: Started==0 の間は Com_MainFunctionTx()
         * 側の監視ループ自体が評価しないため値は参照されず、再開時は
         * Com_IpduGroupStart() が無条件で再初期化する）。この停止時点で
         * 確認待ちだった I-PDU が TxTOutCbk と二重発火しないのは、直上で
         * Com_TxConfPending[id] を無条件でクリアしているため（TX 監視
         * ループは Com_TxConfPending[id]==0 を見た時点でこの I-PDU を
         * 対象外にする、Com_MainFunctionTx() 参照）。Com_TxIPduStarted[id]==0
         * はあくまで「停止中は評価しない」という独立した目的であり、この
         * 二重発火防止自体の担い手ではない。 */

        /* [SWS_Com_00777]: 保留中の送信要求をキャンセルする。再開時に
         * 「停止中に溜まった分」が積み残しとして即座に送信されないようにする
         * （Com_SetCommunicationEnabled() の既存コメントと同じ考え方）。 */
        Com_TxPending[id] = 0U;
        Com_TxTriggerPending[id] = 0U;

        /* [SWS_Com_00392]: I-PDU Group の停止は ComTxModeNumberOfRepetitions
         * の再送シーケンスもキャンセルする。本番設定では対象 I-PDU
         * （ImmobilizerStatus）が IpduGroupId=COM_IPDU_GROUP_NONE のため
         * このパスは実機では到達しないが、防御的にクリアしておく。 */
        Com_TxRepeatsRemaining[id] = 0U;

        DET_LOGI(TAG, "IpduGroupStop grp=%u iPdu=%u(TX)",
                 (unsigned)IpduGroupId, (unsigned)id);
    }
}

/**
 * \brief   指定した I-PDU Group に TX I-PDU が1本でも含まれるか判定する。
 *
 * \details [SWS_Com_00534]: `Com_EnableReceptionDM()`/`Com_DisableReceptionDM()`
 *          は、対象 I-PDU Group が1本でも TX I-PDU を含む場合、要求全体を
 *          黙って無視しなければならない（RX/TX が混在するグループへ「受信」
 *          デッドライン監視の有効/無効化を適用することは意味を持たないため）。
 *          本プロジェクトの `COM_IPDU_GROUP_NONE` は実際に RX/TX 混在グループ
 *          （Com_PBCfg.c 参照: RX 1本・TX 3本が同じ `COM_IPDU_GROUP_NONE` に
 *          属する）であり、この要求は現実に到達しうる。
 *
 * \param[in]  IpduGroupId  判定対象の I-PDU Group の ID。
 *
 * \retval  1  1本でも所属 TX I-PDU があった。
 * \retval  0  所属 TX I-PDU が無かった。
 *
 * \pre        Com_ConfigPtr が NULL でないこと。
 */
static uint8 Com_IpduGroupHasTxMember(Com_IpduGroupIdType IpduGroupId)
{
    for (uint8 i = 0U; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        if (Com_ConfigPtr->TxIPdus[i].IpduGroupId == IpduGroupId)
            return 1U;
    }
    return 0U;
}

void Com_EnableReceptionDM(Com_IpduGroupIdType IpduGroupId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_ENABLE_RECEPTION_DM, COM_E_UNINIT);
        return;
    }

    if (Com_IpduGroupHasTxMember(IpduGroupId))
    {
        /* [SWS_Com_00534]: 要求全体を無視する（RX 側も一切変更しない）。 */
        DET_LOGW(TAG, "EnableReceptionDM grp=%u ignored: group contains TX I-PDU(s)",
                 (unsigned)IpduGroupId);
        return;
    }

    const unsigned long now = millis();

    for (uint8 i = 0U; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->IpduGroupId != IpduGroupId)
            continue;

        Com_RxDmEnabled[ipdu->IPduId] = 1U;

        /* Com_IpduGroupStart() の [SWS_Com_00787] 項目2と同じ理由:
         * 無効化していた間の経過時間を理由に、再有効化した直後で即座に
         * タイムアウト判定されてしまうのを防ぐ。 */
        Com_ResetRxDeadlineMonitoring(ipdu->IPduId, now);

        DET_LOGI(TAG, "EnableReceptionDM grp=%u iPdu=%u",
                 (unsigned)IpduGroupId, (unsigned)ipdu->IPduId);
    }
}

void Com_DisableReceptionDM(Com_IpduGroupIdType IpduGroupId)
{
    DET_LOGT(TAG, "called");

    if (Com_ConfigPtr == NULL)
    {
        Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_DISABLE_RECEPTION_DM, COM_E_UNINIT);
        return;
    }

    if (Com_IpduGroupHasTxMember(IpduGroupId))
    {
        /* [SWS_Com_00534]: 要求全体を無視する（RX 側も一切変更しない）。 */
        DET_LOGW(TAG, "DisableReceptionDM grp=%u ignored: group contains TX I-PDU(s)",
                 (unsigned)IpduGroupId);
        return;
    }

    for (uint8 i = 0U; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->IpduGroupId != IpduGroupId)
            continue;

        Com_RxDmEnabled[ipdu->IPduId] = 0U;

        DET_LOGI(TAG, "DisableReceptionDM grp=%u iPdu=%u",
                 (unsigned)IpduGroupId, (unsigned)ipdu->IPduId);
    }
}

#ifdef COM_UNIT_TEST
uint8 Com_Test_GetTxPending(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return 0U;
    return Com_TxPending[ipduId];
}

uint8 Com_Test_GetTxTriggerPending(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return 0U;
    return Com_TxTriggerPending[ipduId];
}

uint8 Com_Test_GetTmsState(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return 0U;
    return Com_TmsState[ipduId];
}

const uint8* Com_Test_GetTxBuffer(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return NULL;
    return Com_TxBuffer[ipduId];
}

uint8 Com_Test_GetSigTimedOut(Com_SignalIdType SignalId)
{
    /* Com_SigTimedOut[] は SignalId ではなく Com_ConfigPtr->Signals[] 上の
     * 位置で添字付けされている（Com_ReceiveSignal() と同じく
     * Com_FindSignalIndex() で変換が必要。Com_Test_GetTxPending() 等の
     * IPduId 直接添字とは事情が異なる。IPduId は Com_TxBuffer[] 等の
     * 添字として直接使う設計だが、SignalId は配列内位置とは無関係な ID）。 */
    if (Com_ConfigPtr == NULL)
        return 0U;
    const uint8 s = Com_FindSignalIndex(SignalId);
    if (s >= Com_ConfigPtr->SignalCount)
        return 0U;
    return Com_SigTimedOut[s];
}

void Com_Test_SetTxConfPending(Com_IPduIdType ipduId, uint8 value)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return;
    Com_TxConfPending[ipduId] = value;
}

uint8 Com_Test_GetTxRepeatsRemaining(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return 0U;
    return Com_TxRepeatsRemaining[ipduId];
}

void Com_Test_SetTxRepeatsRemaining(Com_IPduIdType ipduId, uint8 value)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return;
    Com_TxRepeatsRemaining[ipduId] = value;
}

uint8 Com_Test_GetTxTimedOut(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return 0U;
    return Com_TxTimedOut[ipduId];
}

uint8 Com_Test_GetTxConfPending(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return 0U;
    return Com_TxConfPending[ipduId];
}

void Com_Test_SetTxConfPendingSinceMs(Com_IPduIdType ipduId, unsigned long value)
{
    if (ipduId >= COM_TX_IPDU_MAX)
        return;
    Com_TxConfPendingSinceMs[ipduId] = value;
}

uint8 Com_Test_GetRxDmEnabled(Com_IPduIdType ipduId)
{
    if (ipduId >= COM_RX_IPDU_MAX)
        return 0U;
    return Com_RxDmEnabled[ipduId];
}
#endif

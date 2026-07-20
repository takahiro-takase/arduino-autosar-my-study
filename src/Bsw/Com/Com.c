/**
 * \file    Com.c
 * \brief   通信マネージャ (AUTOSAR SWS_COM 準拠)
 * \details シグナルベースの通信を行う AUTOSAR COM API 層を実装する。
 *          RX/TX I-PDU バッファを管理し、設定可能なビットエンディアン
 *          (Motorola/Intel) でシグナルのパック・アンパックを行う。
 *          AUTOSAR 4.3.1 SWS_COM 仕様に準拠し、Arduino UNO 向けに
 *          バッファ数固定・更新ビットなしに簡略化している。
 *          受信デッドライン監視 (SWS_Com_00398) を実装しており、
 *          Com_MainFunction() が周期的にタイムアウトを検出する。
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

static const Com_ConfigType* Com_ConfigPtr = NULL;
static uint8         Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8         Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];
static unsigned long Com_RxLastMs[COM_RX_IPDU_MAX];   /* 最終受信時刻 [ms] */
static uint8         Com_RxTimedOut[COM_RX_IPDU_MAX];  /* 1 = タイムアウト中 */

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
 * Com_MainFunction() が実送信するたび（Com_TxPending 経由・周期フロア
 * 経由いずれも）更新することで、MIXED の周期フロアが直近の送信からの
 * 経過時間を基準に動く（COM_TX_MODE_DIRECT の I-PDU では未使用）。 */
static unsigned long Com_TxLastSentMs[COM_TX_IPDU_MAX];

/* 診断 CommunicationControl (UDS SID 0x28) からの通信有効/無効状態。
 * 既定は両方とも有効 (1)。Com_SetCommunicationEnabled() 参照。 */
static uint8 Com_RxEnabled = 1U;
static uint8 Com_TxEnabled = 1U;

/* -----------------------------------------------------------------------
 * TX シグナルフィルタ（ComFilterAlgorithm）関連の内部状態
 * ----------------------------------------------------------------------- */
static uint32 Com_FilterLastValue[COM_SIGNAL_COUNT];  /* シグナルごとの直近フィルタ比較値 */

/* ComDataInvalidAction=COM_DATA_INVALID_ACTION_NOTIFY の RX シグナル用、
 * 直近の有効値（InvalidValue と一致しなかった、最後に受理した値）。
 * SWS_Com_00717: 無効値受信中は Com_ReceiveSignal() がこれを返し続け、
 * 実バッファ/シャドウバッファの中身（無効値そのもの）は返さない。 */
static uint32 Com_RxLastValidValue[COM_SIGNAL_COUNT];

/* Com_ReceiveSignal() が無効値を検知した際に立てる、「次回 Com_MainFunction()
 * で InvalidNotificationCbk を呼ぶべき」フラグ。実際のコールバック呼び出しは
 * 必ず Com_MainFunction() 側へディスパッチし、Com_ReceiveSignal() の呼び出し
 * スタックフレームでは行わない。
 *
 * 理由（実機で確認済みの障害）: Com_ReceiveSignal() は Rte 層の
 * SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()（実体は noInterrupts()/
 * interrupts()、グローバル割り込み禁止）の内側から呼ばれることがある
 * （Rte_COMCbk_EngineInfo() 等）。この区間内でコールバックを直接呼び、
 * コールバックが Serial 出力のような割り込み駆動の I/O を行うと、
 * 割り込み禁止中は UART TX バッファが空かず実質無限ループとなり、
 * WDT リセットを引き起こす。Com_TxPending と同じ設計思想（実処理を
 * ASW/Rte のスタックフレームから切り離し、必ず Com_MainFunction() 側で
 * 行う）でこれを回避する。 */
static uint8 Com_RxInvalidNotifyPending[COM_SIGNAL_COUNT];

/* DIRECT/MIXED I-PDU 用、「次回 Com_MainFunction() で送信すべき変化あり」フラグ。
 * 実送信（PduR_Transmit → ... → MCP2515 への SPI 送信）を ASW Runnable の
 * スタックフレームから切り離し、必ず Com_MainFunction()（Os の 100ms タスク、
 * WdgM 非監視）側で行うためのディスパッチ機構。COM_TX_MODE_PERIODIC の
 * I-PDU では未使用（常に 0）。 */
static uint8 Com_TxPending[COM_TX_IPDU_MAX];

/* TMS（Transmission Mode Selector）評価結果。1 = true（TxModeModeTrue/
 * TxPeriodMsTrue を使う）、0 = false（TxModeMode/TxPeriodMs を使う）。
 * Com_RecalcTms() が Com_SendSignal()/Com_SendSignalGroup() のたびに
 * 再評価する（SWS_Com_00245）。TmsContributor を持つシグナルが存在しない
 * I-PDU では常に 0 のまま変化しない（＝常に false 側のみを使う、既存の
 * 単一モード I-PDU と同じ挙動）。 */
static uint8 Com_TmsState[COM_TX_IPDU_MAX];

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
 * \details 設定ポインタを保存し、RX/TX の全 I-PDU バッファをゼロクリアして
 *          シグナル設定テーブルをログ出力する。RX または TX の I-PDU 数が
 *          コンパイル時上限を超える場合は初期化を中断する。
 *
 * \param[in]  config  COM 設定構造体へのポインタ。NULL 禁止。
 *
 * \pre        PduR_Init() が正常に完了していること。
 * \note       COM_RX_IPDU_MAX / COM_TX_IPDU_MAX はコンパイル時定数で 1 に固定。
 *             それを超える設定は拒否される。
 *
 * \AUTOSARReq     {SWS_Com_00432}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_Init(const Com_ConfigType* config)
{
    if (config == NULL)
    {
        DET_LOGE(TAG, "Init E: config NULL");
        return;
    }
    if (config->RxIPduCount > COM_RX_IPDU_MAX)
    {
        DET_LOGE(TAG, "Init E: RxIPduCount>max");
        return;
    }
    if (config->TxIPduCount > COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "Init E: TxIPduCount>max");
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
        /* RX Signal Group 未コミット状態。Com_ReceiveSignalGroup() が一度も
         * 呼ばれていないグループメンバーを、ゼロクリアされたシャドウバッファ
         * を「正常な値」として誤って返さないよう、利用不可扱いにしておく。 */
        Com_RxShadowTimedOut[i] = 1U;
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
        Com_TmsState[i]          = 0U;   /* 既定 false（ゼロクリアされたバッファと整合） */
        Com_GroupTriggerPending[i] = 0U;
    }

    for (uint8 s = 0; s < COM_SIGNAL_COUNT; s++)
    {
        Com_FilterLastValue[s]         = 0U;
        Com_RxLastValidValue[s]        = 0U;
        Com_RxInvalidNotifyPending[s]  = 0U;
    }

    DET_LOGI(TAG, "Init ok RX=%u TX=%u sig=%u",
             (unsigned)config->RxIPduCount,
             (unsigned)config->TxIPduCount,
             (unsigned)config->SignalCount);
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
 *          場合はエラー扱いとし、バッファ・タイムアウトタイマのいずれも
 *          更新せずに処理を打ち切る（ショート/破損フレームによる新旧データ
 *          混在を防ぐ。AUTOSAR 本来のシグナル単位部分受理との違いは後述）。
 *          DLC を超える分（CAN フレームが 8 バイト固定でパディングされている
 *          場合の末尾バイト等）は許容し、先頭 DLC バイトのみを読み取る。
 *
 * \param[in]  RxPduId     受信 I-PDU の PduR 層 PDU ID。
 *                         Com_IPduConfigType エントリの検索に使用する。
 * \param[in]  PduInfoPtr  受信 PDU のデータと長さへのポインタ。
 *                         NULL 禁止。SduDataPtr も NULL 禁止
 *                         （本関数が直接 SduDataPtr[b] を参照するため必須）。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Com_00123}
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (Com_ConfigPtr == NULL || PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
        return;

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

        /* 受信長チェック: 設定 DLC に満たないフレーム（ショート/破損フレーム等）
         * はここで棄却し、バッファ・タイムアウトタイマのいずれも更新せずに
         * 処理を打ち切る。これを行わないと、短いフレームが届いた際に
         * Com_RxBuffer の一部だけが新しい値で上書きされ、残りバイトは前回値が
         * 残ったまま（新旧混在の破損データ）「正常受信」として扱われてしまう。
         * 逆に DLC を超える分（末尾のパディングバイト）は許容する。CAN フレームは
         * 実際には 8 バイト固定で送信されることが多く（uds_tester の
         * send_can_frame() 等、DLC 未満は 0x00 で埋めて送る）、DLC より短い
         * 論理 I-PDU（例: AbsInfo DLC=5）でも物理フレームは 8 バイトで届くのが
         * 通常運用のため、完全一致を要求すると正常フレームまで拒否してしまう。
         *
         * AUTOSAR 実装との違い: 本来 AUTOSAR COM は I-PDU 全体ではなく
         * シグナル単位で部分受理を行う（SWS_Com_00574: 完全に受信できた
         * シグナルのみアンパック・通知する。SWS_Com_00575: 部分受信の
         * シグナルグループは丸ごと不採用。SWS_Com_00870: 部分受信 I-PDU でも
         * 受信済み範囲に収まる位置のシグナルは受理してよい）。本実装は
         * この部分受理を行わず I-PDU 全体を丸ごと棄却する、より単純で
         * 安全側の簡略化を採用している（学習用途では実装・検証が容易なため）。 */
        if (PduInfoPtr->SduLength < ipdu->DLC)
        {
            DET_LOGW(TAG, "RX iPdu=%u length mismatch got=%u exp=%u",
                     (unsigned)ipdu->IPduId, (unsigned)PduInfoPtr->SduLength,
                     (unsigned)ipdu->DLC);
            return;
        }

        /* この時点で PduInfoPtr->SduLength >= ipdu->DLC が確定している
         * (冒頭の受信長チェック参照)。DLC を超える分（末尾パディング）は
         * このシグナルグループの対象外のため読み捨てる。
         *
         * Com は E2E 等のペイロード内容には一切関知せず、無条件にバッファ・
         * タイムアウトタイマを更新する（E2E Transformer 方式。Com_Types.h の
         * RxIndicationCbk 説明参照）。ペイロードの妥当性検証・破棄判断は
         * すべて RxIndicationCbk 側（例: RTE 経由の E2EXf_InverseTransform）
         * の責務であり、Com はそれがあることすら知らない。 */
        for (uint8 b = 0; b < ipdu->DLC; b++)
            Com_RxBuffer[ipdu->IPduId][b] = PduInfoPtr->SduDataPtr[b];

        /* 受信成功 → タイムアウトタイマをリセット */
        Com_RxLastMs[ipdu->IPduId]  = millis();
        Com_RxTimedOut[ipdu->IPduId] = 0U;

        char hexbuf[25];
        Log_HexStr(hexbuf, sizeof(hexbuf), Com_RxBuffer[ipdu->IPduId], ipdu->DLC);
        DET_LOGI(TAG, "RX iPdu=%u [%s]", (unsigned)ipdu->IPduId, hexbuf);

        /* フレーム受信の都度呼ばれる汎用フック（E2E Transformer 等）。
         * バッファ更新後・return 前に呼ぶことで、上位層が最新データを
         * 参照できる状態にしてから通知する。 */
        if (ipdu->RxIndicationCbk != NULL)
            ipdu->RxIndicationCbk();

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
    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        if (Com_ConfigPtr->RxIPdus[i].IPduId == IPduId)
            return &Com_ConfigPtr->RxIPdus[i];
    }
    return NULL;
}

/**
 * \brief   TX I-PDU バッファを実際に PduR_Transmit() へ渡す共通処理。
 *
 * \details TxTransformCbk が設定されていれば送信直前に呼び出し（E2E
 *          Transformer 等、送信直前の最終変換用の汎用フック。Com はここで
 *          何が実行されるか一切関知しない）、その後 TX バッファの内容を
 *          ログ出力して PduR_Transmit() を呼ぶ（PduR→CanIf→Can_Write と
 *          MCP2515 への SPI 送信までブロッキングで完了する）。
 *          `Com_MainFunction()` からのみ呼ばれる。DIRECT/MIXED I-PDU の
 *          イベント駆動送信であっても実送信は必ず `Com_MainFunction()`
 *          （Os の 100ms タスク）側で行う設計とし、WdgM の Deadline
 *          Supervision 対象である ASW Runnable（`App_EngineManager_Run()`
 *          等）のスタックフレーム内で SPI 送信がブロッキングしないようにする
 *          （バス輻輳時に `sendMsgBuf()` の TX バッファ空き待ちが伸びても、
 *          Runnable 自体の実行時間には影響しない）。
 *          「送信すべきかどうかの判断」は呼び出し元（`Com_MainFunction()`）が
 *          既に済ませてから呼ぶ。
 *
 * \param[in]  ipdu  送信する TX I-PDU 設定。NULL 禁止（呼び出し元で保証する）。
 *
 * \retval  E_OK      PduR_Transmit() が成功した。
 * \retval  E_NOT_OK  PduR_Transmit() が失敗した。
 *
 * \ServiceID      {0xF3}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static Std_ReturnType Com_DoTransmit(const Com_IPduConfigType* ipdu)
{
    if (ipdu->TxTransformCbk != NULL)
        ipdu->TxTransformCbk(Com_TxBuffer[ipdu->IPduId], ipdu->DLC);

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), Com_TxBuffer[ipdu->IPduId], ipdu->DLC);
    DET_LOGI(TAG, "TX iPdu=%u [%s]", (unsigned)ipdu->IPduId, hexbuf);

    PduInfoType pduInfo = {
        .SduDataPtr = Com_TxBuffer[ipdu->IPduId],
        .SduLength  = ipdu->DLC
    };
    return PduR_Transmit(ipdu->PduRId, &pduInfo);
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
    return Com_TmsState[ipdu->IPduId] ? ipdu->TxPeriodMsTrue : ipdu->TxPeriodMs;
}

/**
 * \brief   TMS（Transmission Mode Selector）を再評価する。
 *
 * \details 指定 I-PDU に属するシグナルのうち `TmsContributor=1` のものについて、
 *          `Com_TxBuffer[ipduId]` から現在値をアンパックし、
 *          `(値 & Mask) != FilterX` を TMC（Transmission Mode Condition）として
 *          評価する。1 つでも真なら TMS = true（SWS_Com_00678）、
 *          TmsContributor が 1 つも無い、またはどれも偽なら TMS = false
 *          （SWS_Com_00679）。結果を `Com_TmsState[ipduId]` へ保存する。
 *
 *          Com_SendSignal()（Signal Group でない場合）と
 *          Com_SendSignalGroup() の確定コミット後、いずれも実バッファへの
 *          反映が完了した時点で呼ぶこと（SWS_Com_00245: 値の更新のたびに
 *          TMS を再計算する）。
 *
 * \param[in]  ipduId  再評価する TX I-PDU の ID。
 *
 * \pre        Com_ConfigPtr が NULL でないこと（呼び出し元で保証する）。
 * \pre        `Com_TxBuffer[ipduId]` が最新値へ更新済みであること。
 *
 * \AUTOSARReq     {SWS_Com_00245, SWS_Com_00676, SWS_Com_00677, SWS_Com_00678, SWS_Com_00679}
 * \ServiceID      {0x1E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_RecalcTms(Com_IPduIdType ipduId)
{
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

    Com_TmsState[ipduId] = tmsTrue;
}

/**
 * \brief   ComFilterAlgorithm を通過した変化を「次回送信あり」として記録する。
 *
 * \details Com_SendSignal() / Com_SendSignalGroup() が変化を検知した際に
 *          呼ばれる。ここでは `Com_TxPending[]` を立てるだけで、実際の
 *          PduR_Transmit() 呼び出し（ひいては MCP2515 への SPI 送信）は一切
 *          行わない（SWS_Com_00734/00742/00743 の要求"shall immediately
 *          (within the next main function at the latest) initiate..." の
 *          うち、「次回メイン関数まで」の猶予を使い、実送信は必ず
 *          `Com_MainFunction()` 側にディスパッチする設計にしている。
 *          呼び出しスタックと同一フレームで SPI 送信までブロッキングすると、
 *          WdgM の Deadline Supervision 対象である ASW Runnable
 *          （App_EngineManager_Run 等）の実行時間がバス輻輳時の SPI 遅延に
 *          左右されてしまうため）。
 *
 *          実効 TxModeMode（`Com_EffectiveTxModeMode()`、TMS 評価済み）が
 *          `COM_TX_MODE_PERIODIC` の I-PDU では何もしない（PERIODIC I-PDU は
 *          Com_MainFunction() の周期タスクのみが送信を担い、値の変化そのものは
 *          送信タイミングに影響しない）。
 *
 *          診断 CommunicationControl (UDS 0x28) による送信抑制中でも
 *          ここではフラグを立てるだけとする（実際に送信を抑制するかどうかの
 *          判断は Com_MainFunction() 側で行う。SWS_Com_00777/SWS_Com_00334
 *          が要求する「停止中に発生した送信要求は保持されず、再開しても
 *          古いトリガーで即座に送信してはならない」は、Com_MainFunction()
 *          が抑制中にこのフラグを見つけ次第、実送信せずに破棄することで
 *          満たす）。
 *
 * \param[in]  ipdu  対象 TX I-PDU 設定。NULL 禁止（呼び出し元で保証する）。
 *
 * \AUTOSARReq     {SWS_Com_00734, SWS_Com_00742, SWS_Com_00743}
 * \ServiceID      {0x17}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_RequestTxOnChange(const Com_IPduConfigType* ipdu)
{
    if (Com_EffectiveTxModeMode(ipdu) == COM_TX_MODE_PERIODIC)
        return;

    Com_TxPending[ipdu->IPduId] = 1U;
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
 *                    TimeoutSubstitutionValue、または受信値が InvalidValue と
 *                    一致し DataInvalidAction=NOTIFY の場合は直近の有効値）。
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
 *                  SWS_Com_00680, SWS_Com_00717}
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    uint8* dataPtr = (uint8*)SignalDataPtr;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

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
            return E_NOT_OK;
        }

        const Com_IPduConfigType* ipdu = Com_FindRxIPdu(sig->IPduId);
        if (ipdu == NULL)
        {
            DET_LOGE(TAG, "ReceiveSignal E: sig=%u IPduId=%u not a registered RX I-PDU",
                     (unsigned)SignalId, (unsigned)sig->IPduId);
            return E_NOT_OK;
        }

        /* RX Signal Group メンバーは Com_ReceiveSignalGroup() が確定コピーした
         * シャドウバッファ・タイムアウトスナップショットを読む（Com_RxBuffer/
         * Com_RxTimedOut を直接見ない）。これにより、同じグループの複数
         * メンバーを読む間に新しいフレームが届いても一貫した値が返る。 */
        const uint8 timedOut = (ipdu->IsSignalGroup != 0U)
                               ? Com_RxShadowTimedOut[sig->IPduId]
                               : Com_RxTimedOut[sig->IPduId];
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
             * SUBSTITUTE でなければ、値を書き込まず E_NOT_OK を返す
             * （呼び出し元の初期値=安全値を使用、既存の既定動作）。
             * SUBSTITUTE なら I-PDU バッファ/シャドウバッファは読まず、
             * 設定済みの TimeoutSubstitutionValue を代わりに書き込んで
             * E_OK を返す（実データが古いまま返ることを防ぐ）。 */
            if (sig->RxDataTimeoutAction != COM_RX_TIMEOUT_ACTION_SUBSTITUTE)
                return E_NOT_OK;

            for (uint8 b = 0U; b < byteCount; b++)
                dataPtr[b] = (uint8)(sig->TimeoutSubstitutionValue >> (8U * b));
            return E_OK;
        }

        const uint32 value = Com_UnpackSignal(
            srcBuf,
            sig->BitPosition, sig->BitSize, sig->Endian);

        /* ComDataInvalidAction（Com_DataInvalidActionType 参照）: 受信値が
         * InvalidValue と一致する場合、NOTIFY なら「シグナルオブジェクトへ
         * 格納しない」（SWS_Com_00717）。すなわち Com_RxLastValidValue[s]
         * を更新せず、直近の有効値をそのまま返す。通知コールバックの実呼び出し
         * はここでは行わず、Com_RxInvalidNotifyPending[s] を立てるだけに留める
         * （Com_MainFunction() へディスパッチする理由は
         * Com_RxInvalidNotifyPending の宣言コメント参照）。 */
        if (sig->DataInvalidAction == COM_DATA_INVALID_ACTION_NOTIFY
            && value == sig->InvalidValue)
        {
            Com_RxInvalidNotifyPending[s] = 1U;

            const uint32 lastValid = Com_RxLastValidValue[s];
            for (uint8 b = 0U; b < byteCount; b++)
                dataPtr[b] = (uint8)(lastValid >> (8U * b));
            return E_OK;
        }

        Com_RxLastValidValue[s] = value;
        for (uint8 b = 0U; b < byteCount; b++)
        {
            dataPtr[b] = (uint8)(value >> (8U * b));
        }
        return E_OK;
    }
    return E_NOT_OK;
}

/**
 * \brief   RX Signal Group を I-PDU バッファから RX シャドウバッファへ確定コピーする。
 *
 * \details Com_SendSignalGroup()（TX 側）の対称版。GroupId が RX Signal Group
 *          （IsSignalGroup=1）であれば、Com_RxBuffer[GroupId] の内容を
 *          Com_RxShadowBuffer[GroupId] へバイト単位でコピーし、あわせて
 *          その時点の Com_RxTimedOut[GroupId] を Com_RxShadowTimedOut[GroupId]
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
 * \param[in]  GroupId  確定コピーする RX Signal Group（RX I-PDU）の ID。
 *
 * \retval  E_OK      GroupId が見つかり、コピー時点でタイムアウト中でなかった。
 * \retval  E_NOT_OK  COM 未初期化、GroupId が RX I-PDU 設定テーブルに
 *                    存在しない、IsSignalGroup=0 の I-PDU を指定した、
 *                    またはコピーは行ったがコピー時点でタイムアウト中だった。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Com_00201, SWS_Com_00051, SWS_Com_00638, SWS_Com_00461, SWS_Com_00876}
 * \ServiceID      {0x19}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_ReceiveSignalGroup(Com_IPduIdType GroupId)
{
    if (Com_ConfigPtr == NULL)
        return E_NOT_OK;

    /* 範囲チェック: GroupId をそのまま Com_RxBuffer[] 等の配列添字として
     * 使うため、RX I-PDU 設定テーブル自体に範囲外の IPduId が設定される
     * 事態に備えて明示的に検査する（Com_ReceiveSignal/Com_SendSignalGroup と
     * 同じ方針）。 */
    if (GroupId >= COM_RX_IPDU_MAX)
    {
        DET_LOGE(TAG, "ReceiveSignalGroup E: GroupId=%u out of range (max=%u)",
                 (unsigned)GroupId, (unsigned)COM_RX_IPDU_MAX);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindRxIPdu(GroupId);
    if (ipdu == NULL || ipdu->IsSignalGroup == 0U)
        return E_NOT_OK;

    for (uint8 b = 0U; b < ipdu->DLC; b++)
        Com_RxShadowBuffer[GroupId][b] = Com_RxBuffer[GroupId][b];

    Com_RxShadowTimedOut[GroupId] = Com_RxTimedOut[GroupId];

    return Com_RxShadowTimedOut[GroupId] ? E_NOT_OK : E_OK;
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
 * \param[in]  IPduId   読み取る RX I-PDU の ID。
 * \param[out] DataPtr  コピー先バッファへのポインタ。ipdu->DLC バイト以上
 *                      必要。NULL 禁止。
 *
 * \retval  E_OK      IPduId が見つかり、DataPtr へコピーした。
 * \retval  E_NOT_OK  COM 未初期化、DataPtr が NULL、
 *                    または IPduId が RX I-PDU 設定に存在しない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0x1A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_ReceiveSignalGroupArray(Com_IPduIdType IPduId, uint8* DataPtr)
{
    if (Com_ConfigPtr == NULL || DataPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->IPduId != IPduId)
            continue;

        for (uint8 b = 0; b < ipdu->DLC; b++)
            DataPtr[b] = Com_RxBuffer[IPduId][b];
        return E_OK;
    }
    return E_NOT_OK;
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
 * \ServiceID      {0x1B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_IsRxTimedOut(Com_IPduIdType IPduId)
{
    if (IPduId >= COM_RX_IPDU_MAX)
        return 1U;
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
 *          （TxModeMode が DIRECT/MIXED の I-PDU なら次回 Com_MainFunction()
 *          で送信される。本関数自体は PduR_Transmit() を呼ばない）。
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
 * \AUTOSARReq     {SWS_Com_00197, SWS_Com_00742, SWS_Com_00743}
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    const uint8* dataPtr = (const uint8*)SignalDataPtr;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

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
            return E_NOT_OK;
        }

        const Com_IPduConfigType* ipdu = Com_FindTxIPdu(sig->IPduId);
        if (ipdu == NULL)
        {
            DET_LOGE(TAG, "SendSignal E: sig=%u IPduId=%u not a registered TX I-PDU",
                     (unsigned)SignalId, (unsigned)sig->IPduId);
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
         * 非 Signal Group のシグナルに TmsContributor=1 を設定した場合に備える。 */
        Com_RecalcTms(sig->IPduId);

        /* ComFilterAlgorithm 評価: 送信すべき更新かどうかは Com 自身が判断する
         * (ASW は値をセットするだけで、送信要否には関与しない) */
        uint8 passesFilter = 1U;
        if (sig->FilterAlgorithm == COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD)
        {
            passesFilter = ((value & sig->Mask) != (Com_FilterLastValue[s] & sig->Mask)) ? 1U : 0U;
        }
        Com_FilterLastValue[s] = value;

        if (passesFilter)
            Com_RequestTxOnChange(ipdu);

        return E_OK;
    }
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
 *          DIRECT/MIXED の I-PDU なら次回 Com_MainFunction() で送信される）。
 *
 * \param[in]  GroupId  コミットする Signal Group（TX I-PDU）の ID。
 *
 * \retval  E_OK      GroupId が見つかり、コミット処理を行った。
 * \retval  E_NOT_OK  COM 未初期化、GroupId が TX I-PDU 設定テーブルに
 *                    存在しない、または IsSignalGroup=0 の I-PDU を指定した。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        コミット前に、このグループに属する全メンバーを
 *             Com_SendSignal() で設定しておくこと。
 *
 * \AUTOSARReq     {SWS_Com_00200, SWS_Com_00050, SWS_Com_00742, SWS_Com_00743}
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_SendSignalGroup(Com_IPduIdType GroupId)
{
    if (Com_ConfigPtr == NULL)
        return E_NOT_OK;

    /* 範囲チェック: GroupId をそのまま Com_TxBuffer[] 等の配列添字として
     * 使うため、TX I-PDU 設定テーブル自体に範囲外の IPduId が設定される
     * 事態に備えて明示的に検査する（Com_ReceiveSignal/Com_SendSignal と
     * 同じ方針）。 */
    if (GroupId >= COM_TX_IPDU_MAX)
    {
        DET_LOGE(TAG, "SendSignalGroup E: GroupId=%u out of range (max=%u)",
                 (unsigned)GroupId, (unsigned)COM_TX_IPDU_MAX);
        return E_NOT_OK;
    }

    const Com_IPduConfigType* ipdu = Com_FindTxIPdu(GroupId);
    if (ipdu == NULL || ipdu->IsSignalGroup == 0U)
        return E_NOT_OK;

    /* PENDING/TRIGGERED_ON_CHANGE を問わず、シャドウバッファの値はすべて
     * 実バッファへコピーする（SWS_Com_00743: PENDING メンバーも、他の
     * メンバーが引き起こした送信に便乗して最新値が運ばれる）。 */
    for (uint8 b = 0U; b < ipdu->DLC; b++)
    {
        Com_TxBuffer[GroupId][b] = Com_TxShadowBuffer[GroupId][b];
    }

    /* TMS 再評価（SWS_Com_00245）。Com_RequestTxOnChange() が
     * Com_EffectiveTxModeMode() 経由で Com_TmsState を参照するため、
     * その呼び出しより前に確定させる。TMS 寄与シグナルが PENDING の場合、
     * 「送信は引き起こさないが TMS だけは変化する」こともあり得るが、
     * これは仕様上の矛盾ではない（TMS は「次に送信するときどのモードを
     * 使うか」を決めるだけで、それ自体が送信のトリガーではないため）。 */
    Com_RecalcTms(GroupId);

    /* 送信を引き起こすかどうかは、ComTransferProperty=TRIGGERED_ON_CHANGE の
     * メンバーが Com_SendSignal() 内で変化検知して立てたフラグのみで判定する
     * （バイト単位の生比較はしない。PENDING メンバーだけが変化した場合は
     * このフラグは立たず、コミットはされても送信は引き起こされない）。 */
    if (Com_GroupTriggerPending[GroupId])
    {
        Com_GroupTriggerPending[GroupId] = 0U;
        Com_RequestTxOnChange(ipdu);
    }

    return E_OK;
}

/**
 * \brief   TX I-PDU の送信完了を COM へ通知し、ComNotification（TxAck）を配送する。
 *
 * \details CAN フレームの送信完了後に PduR から呼び出される。まずログ出力を
 *          行い、result==E_OK（送信成功）であれば、この I-PDU（TxPduId、
 *          Com_IPduIdType と同一の値空間。PduR_PBCfg.c の ConfDestPduId 参照）
 *          に属する TX シグナル（Direction==COM_SIGNAL_DIRECTION_TX。RX I-PDU
 *          と TX I-PDU の IPduId は別値空間で数値が重複しうるため、この
 *          Direction チェックが方向誤認を防ぐために必須。Com_SignalDirectionType
 *          参照）のうち `TxAckCbk` が設定されているものすべてを呼び出す
 *          （Com_CbkTxAck、SWS_Com_00468: "called immediately after
 *          successful transmission of the I-PDU containing the message"）。
 *          Signal Group のメンバーかどうかは問わず、シグナル単位に統一して
 *          扱う（実 AUTOSAR は signal 単位/signal group 単位で別々の
 *          コールバック名 Rte_COMCbkTAck_<sn>/<sg> を持てるが、本実装は
 *          シグナル単位の TxAckCbk のみのシンプルな簡略版）。値がこの送信で
 *          実際に変化したかどうかは問わない（I-PDU が送信されたという事実
 *          だけで、含まれる全シグナルの TxAckCbk が呼ばれる）。
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
 *                      E_OK = 成功、E_NOT_OK = 失敗。
 *                      TX リトライやエラーカウンタ、Com_CbkTxErr
 *                      （SWS_Com_00491、失敗時通知）は実装しない。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \note       result が実際に E_NOT_OK になる経路は現状存在しない。呼び出し元の
 *             CanIf_TxConfirmation() が result を受け取らない 1 引数 API で、
 *             内部で常に E_OK 決め打ちで呼び出すため（さらにその手前の
 *             Can_Write() は送信成功時のみ CanIf_TxConfirmation() を呼ぶ。
 *             MCP2515 との SPI 通信が同期的なため）。
 *
 * \AUTOSARReq     {SWS_Com_00124, SWS_Com_00468}
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    DET_LOGI(TAG, "TxConf id=%u", (unsigned)TxPduId);

    if (result != E_OK || Com_ConfigPtr == NULL)
        return;

    for (uint8 s = 0U; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        /* Direction のチェックが必須: RX I-PDU と TX I-PDU の IPduId は
         * 別々の値空間（どちらも 0 始まり）のため、IPduId の一致だけでは
         * 方向を判別できない（例: RX の EngineInfo=0 と TX の
         * MeterStatus=0）。Direction を見ずに sig->IPduId == TxPduId だけで
         * 判定すると、EngineInfo（RX）に属する EngineSpeed 等が
         * MeterStatus（TX）の送信確認のたびに誤って候補に入ってしまう
         * （TxAckCbk が NULL でない限り誤発火する）。詳細は
         * Com_SignalDirectionType の宣言コメント参照。 */
        if (sig->Direction == COM_SIGNAL_DIRECTION_TX
            && sig->TxAckCbk != NULL
            && sig->IPduId == TxPduId)
        {
            sig->TxAckCbk();
        }
    }
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
 *          TX I-PDU の送信ディスパッチ（DIRECT/MIXED/PERIODIC 共通）:
 *          実際に PduR_Transmit()（→ MCP2515 への SPI 送信）を呼ぶのはこの
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
 * \AUTOSARReq     {SWS_Com_00398, SWS_Com_00684, SWS_Com_00685, SWS_Com_00734,
 *                  SWS_Com_00742, SWS_Com_00743, SWS_Com_00777, SWS_Com_00032,
 *                  SWS_Com_00799, SWS_Com_00471, SWS_Com_00698, SWS_Com_00789}
 * \ServiceID      {0x20}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_MainFunction(void)
{
    if (Com_ConfigPtr == NULL)
        return;

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

    if (Com_RxEnabled != 0U)
    {
        for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
        {
            const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
            if (ipdu->TimeoutMs == 0U)
                continue;  /* 監視無効 */

            if (!Com_RxTimedOut[ipdu->IPduId] &&
                (now - Com_RxLastMs[ipdu->IPduId]) >= (unsigned long)ipdu->TimeoutMs)
            {
                Com_RxTimedOut[ipdu->IPduId] = 1U;
                DET_LOGW(TAG, "RX timeout iPdu=%u (%ums)",
                         (unsigned)ipdu->IPduId, (unsigned)ipdu->TimeoutMs);
            }
        }
    }
    /* Com_RxEnabled==0 の間はデッドライン監視自体を無効化する
     * (SWS_Com_00684/00685)。TX 送信は Rx とは独立した機能のため、
     * ここで return せず以下へ続ける。 */

    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        const Com_IPduIdType      id   = ipdu->IPduId;
        const Com_TxModeModeType  mode   = Com_EffectiveTxModeMode(ipdu);
        const uint16              period = Com_EffectiveTxPeriodMs(ipdu);

        uint8 due;
        if (mode == COM_TX_MODE_PERIODIC)
        {
            due = ((now - Com_TxLastSentMs[id]) >= (unsigned long)period) ? 1U : 0U;
        }
        else
        {
            const uint8 floorDue = (mode == COM_TX_MODE_MIXED)
                                    && ((now - Com_TxLastSentMs[id]) >= (unsigned long)period);
            /* MDT（ComMinimumDelayTime）: 変化時送信（Com_TxPending 経由）にのみ
             * 適用し、MIXED の周期フロア（floorDue）には適用しない
             * （SWS_Com_00789 の既定動作。MinDelayMs=0 なら常に満了扱いのため
             * MDT 未設定の I-PDU では以前と同じ挙動になる）。満了前に変化検知が
             * あっても Com_TxPending は立てたまま保持し、破棄しない
             * （次回 Com_MainFunction() で再判定する）。 */
            const uint8 mdtElapsed = (now - Com_TxLastSentMs[id]) >= (unsigned long)ipdu->MinDelayMs;
            const uint8 changeDue  = (Com_TxPending[id] != 0U) && mdtElapsed;
            due = changeDue || floorDue;
        }

        if (!due)
            continue;

        Com_TxPending[id]    = 0U;
        Com_TxLastSentMs[id] = now;

        if (Com_TxEnabled == 0U)
        {
            DET_LOGD(TAG, "TX skip iPdu=%u (CommunicationControl disabled)", (unsigned)id);
            continue;
        }

        (void)Com_DoTransmit(ipdu);
    }
}

void Com_SetCommunicationEnabled(uint8 RxEnabled, uint8 TxEnabled)
{
    if (Com_RxEnabled != RxEnabled || Com_TxEnabled != TxEnabled)
    {
        DET_LOGI(TAG, "CommunicationControl rx=%u->%u tx=%u->%u",
                 (unsigned)Com_RxEnabled, (unsigned)RxEnabled,
                 (unsigned)Com_TxEnabled, (unsigned)TxEnabled);
    }

    if (Com_RxEnabled == 0U && RxEnabled != 0U && Com_ConfigPtr != NULL)
    {
        /* SWS_Com_00787 相当: 受信再開時はデッドライン監視タイマを再始動する。
         * Com_RxLastMs を現在時刻へリセットしないと、TimeoutMs 以上の時間
         * 受信を抑制していた場合、再有効化した直後（次の Com_MainFunction()
         * 呼び出し）で古い Com_RxLastMs のまま即座にタイムアウト判定されて
         * しまう。既に立っていた Com_RxTimedOut も、抑制中の「経過時間」を
         * 理由に上位層へ通信異常と伝え続けないよう、あわせてクリアする。 */
        const unsigned long now = millis();
        for (uint8 i = 0U; i < Com_ConfigPtr->RxIPduCount; i++)
        {
            const Com_IPduIdType id = Com_ConfigPtr->RxIPdus[i].IPduId;
            Com_RxLastMs[id]   = now;
            Com_RxTimedOut[id] = 0U;
        }
    }

    Com_RxEnabled = RxEnabled;
    Com_TxEnabled = TxEnabled;
}

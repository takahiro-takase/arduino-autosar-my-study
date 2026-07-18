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

/* 診断 CommunicationControl (UDS SID 0x28) からの通信有効/無効状態。
 * 既定は両方とも有効 (1)。Com_SetCommunicationEnabled() 参照。 */
static uint8 Com_RxEnabled = 1U;
static uint8 Com_TxEnabled = 1U;

/* -----------------------------------------------------------------------
 * TX シグナルフィルタ（ComFilterAlgorithm）関連の内部状態
 * ----------------------------------------------------------------------- */
static uint32 Com_FilterLastValue[COM_SIGNAL_COUNT];  /* シグナルごとの直近フィルタ比較値 */
static uint8  Com_TxUpdatePending[COM_TX_IPDU_MAX];   /* 1 = フィルタを通過した未送信の更新あり */
static uint8  Com_TxCyclesSinceSent[COM_TX_IPDU_MAX]; /* Com_TriggerIPDUSend 呼び出し回数（周期フロア用） */

/* -----------------------------------------------------------------------
 * Signal Group（ComIPduConfigType.IsSignalGroup = 1）関連の内部状態
 * Com_SendSignal() は Signal Group メンバーをここへ書き込み、実バッファ
 * (Com_TxBuffer) へは反映しない。Com_SendSignalGroup() が呼ばれた時点で
 * まとめて実バッファへ確定コミットする。
 * ----------------------------------------------------------------------- */
static uint8 Com_TxShadowBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8 Com_GroupFilterLastBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

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
            Com_RxBuffer[i][j] = 0U;
        Com_RxLastMs[i]  = now;  /* タイムアウト計測を Init 時刻から開始 */
        Com_RxTimedOut[i] = 0U;
    }

    for (uint8 i = 0; i < COM_TX_IPDU_MAX; i++)
    {
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
        {
            Com_TxBuffer[i][j]             = 0U;
            Com_TxShadowBuffer[i][j]        = 0U;
            Com_GroupFilterLastBuffer[i][j] = 0U;
        }
        Com_TxUpdatePending[i]   = 0U;
        Com_TxCyclesSinceSent[i] = 0U;
    }

    for (uint8 s = 0; s < COM_SIGNAL_COUNT; s++)
        Com_FilterLastValue[s] = 0U;

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
 * \retval  E_OK      シグナルが見つかり、SignalDataPtr へ値を書き込んだ。
 * \retval  E_NOT_OK  COM 未初期化、SignalDataPtr が NULL、
 *                    またはシグナル設定テーブルに SignalId が存在しない。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        このシグナルが属する I-PDU で Com_RxIndication() が
 *             少なくとも 1 回呼ばれていること。
 * \note       戻り値型は仕様に従い uint8。E_OK / E_NOT_OK の値（0x00 / 0x01）は
 *             RTE が使う Std_ReturnType と互換性がある。
 *
 * \AUTOSARReq     {SWS_Com_00198}
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

        /* タイムアウト中は値を書き込まず E_NOT_OK を返す（呼び出し元の初期値=安全値を使用） */
        if (Com_RxTimedOut[sig->IPduId])
            return E_NOT_OK;

        const uint32 value = Com_UnpackSignal(
            Com_RxBuffer[sig->IPduId],
            sig->BitPosition, sig->BitSize, sig->Endian);

        /* SignalDataPtr は呼び出し元が BitSize に応じた幅の変数
         * (uint8/uint16/uint32) を渡す。常に 4 バイト書き込むと、
         * 8bit/16bit の呼び出し元ではスタック上の隣接領域を破壊する。
         * BitSize から必要バイト数だけを書き込む。 */
        const uint8 byteCount = (uint8)((sig->BitSize + 7U) / 8U);
        for (uint8 b = 0U; b < byteCount; b++)
        {
            dataPtr[b] = (uint8)(value >> (8U * b));
        }
        return E_OK;
    }
    return E_NOT_OK;
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
 *          I-PDU は即座には送信されない。送信するには
 *          Com_TriggerIPDUSend() を呼び出すこと。
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
 *          所属する TX I-PDU に「送信要求あり」を立てる
 *          (Com_TxUpdatePending[])。実際に送信するかどうかは
 *          Com_TriggerIPDUSend() が判断する。
 *
 *          Signal Group（詳細は Com_SendSignalGroup() の \AUTOSARReq 参照）:
 *          所属する I-PDU が IsSignalGroup=1 の場合、値は実 TX バッファ
 *          (Com_TxBuffer) ではなくシャドウバッファ (Com_TxShadowBuffer) へ
 *          パックするのみとし、ComFilterAlgorithm の判定も行わない。
 *          Com_SendSignalGroup() が呼ばれるまで実バッファへは反映されない
 *          （グループの複数メンバーを不整合な状態で送信しないため）。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \note       戻り値型は仕様に従い uint8。E_OK / E_NOT_OK の値（0x00 / 0x01）は
 *             RTE が使う Std_ReturnType と互換性がある。
 *
 * \AUTOSARReq     {SWS_Com_00197}
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
             * 実バッファへの反映とフィルタ判定は Com_SendSignalGroup() が行う。 */
            Com_PackSignal(Com_TxShadowBuffer[sig->IPduId],
                           sig->BitPosition, sig->BitSize, sig->Endian, value);
            return E_OK;
        }

        Com_PackSignal(Com_TxBuffer[sig->IPduId],
                       sig->BitPosition, sig->BitSize, sig->Endian, value);

        /* ComFilterAlgorithm 評価: 送信すべき更新かどうかは Com 自身が判断する
         * (ASW は値をセットするだけで、送信要否には関与しない) */
        uint8 passesFilter = 1U;
        if (sig->FilterAlgorithm == COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD)
        {
            passesFilter = ((value & sig->Mask) != (Com_FilterLastValue[s] & sig->Mask)) ? 1U : 0U;
        }
        Com_FilterLastValue[s] = value;

        if (passesFilter)
            Com_TxUpdatePending[sig->IPduId] = 1U;

        return E_OK;
    }
    return E_NOT_OK;
}

/**
 * \brief   Signal Group メンバーをシャドウバッファから実 TX バッファへ確定コミットする。
 *
 * \details Com_SendSignal() が Signal Group（IsSignalGroup=1）のメンバーを
 *          書き込んだシャドウバッファ (Com_TxShadowBuffer) を、実 TX バッファ
 *          (Com_TxBuffer) へまとめてコピーする。個々のメンバー単位ではなく、
 *          このコミット単位で
 *          前回コミット値とのバイト比較により変化の有無を判定し、
 *          変化があれば所属 I-PDU に「送信要求あり」を立てる
 *          (Com_TxUpdatePending[])。実際に送信するかどうか（周期フロアを
 *          含む）は Com_TriggerIPDUSend() が判断する。
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
 * \AUTOSARReq     {SWS_Com_00200, SWS_Com_00050}
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

    uint8 changed = 0U;
    for (uint8 b = 0U; b < ipdu->DLC; b++)
    {
        if (Com_TxShadowBuffer[GroupId][b] != Com_GroupFilterLastBuffer[GroupId][b])
            changed = 1U;
        Com_TxBuffer[GroupId][b]             = Com_TxShadowBuffer[GroupId][b];
        Com_GroupFilterLastBuffer[GroupId][b] = Com_TxShadowBuffer[GroupId][b];
    }

    if (changed)
        Com_TxUpdatePending[GroupId] = 1U;

    return E_OK;
}

/**
 * \brief   TX I-PDU を PduR 経由で即座に送信する。
 *
 * \details PduId で TX I-PDU 設定を検索し、内部 TX バッファを指す
 *          PduInfoType を構築して PduR_Transmit() を呼び出す。
 *          転送前に TX バッファの内容をログ出力する。
 *          TxTransformCbk が設定された I-PDU は、転送直前にこのフックへ
 *          実 TX バッファへのポインタと長さを渡して呼び出す（E2E Transformer
 *          等、送信直前の最終変換用の汎用フック。Com はここで何が実行される
 *          か一切関知しない）。
 *
 *          送信要否の判定（MIXED 送信モード相当）:
 *          Com_SendSignal() が ComFilterAlgorithm を通過した更新（
 *          Com_TxUpdatePending[]）があれば送信する。無くても
 *          COM_TX_PERIODIC_FLOOR_CYCLES 回呼ばれたら周期フロアとして
 *          強制送信する（実車が変化なしでも一定周期で再送し続けるのと同じ理由:
 *          新規に参加した受信側や、直前のフレームを取りこぼした受信側への配慮）。
 *          いずれも満たさない場合は PduR_Transmit() を呼ばず E_OK を返す
 *          （ASW から見れば「送信要求は正常に受け付けたが、Com が不要と判断した」）。
 *
 *          診断 CommunicationControl (UDS 0x28) による送信抑制中
 *          (Com_TxEnabled==0) は、送信要求があっても実送信は行わず、かつ
 *          その送信トリガー（Com_TxUpdatePending/Com_TxCyclesSinceSent）を
 *          破棄する（SWS_Com_00777: 停止中の I-PDU の送信要求はキャンセル
 *          しなければならない。SWS_Com_00334 の説明文も同旨）。再開直後に
 *          抑制中の古いトリガーで即座に送信されることはない
 *          （TX バッファの値自体は Com_SendSignal() が既に更新済みのため
 *          失われない）。
 *
 * \param[in]  PduId  送信する I-PDU の COM ハンドル。
 *                    TX I-PDU 設定テーブルの IPduId と一致すること。
 *
 * \retval  E_OK      I-PDU が PduR_Transmit() に正常に転送された、
 *                    または Com のフィルタ判定により送信不要と判断された。
 * \retval  E_NOT_OK  COM 未初期化、PduId が見つからない、
 *                    または PduR_Transmit() が失敗した。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        送信前に Com_SendSignal() で TX バッファへ値を設定しておくこと。
 *
 * \AUTOSARReq     {SWS_Com_00348, SWS_Com_00334, SWS_Com_00777}
 * \ServiceID      {0x17}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    if (Com_ConfigPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if ((PduIdType)ipdu->IPduId != PduId)
            continue;

        Com_TxCyclesSinceSent[PduId]++;
        const uint8 shouldSend = (Com_TxUpdatePending[PduId] != 0U)
                                  || (Com_TxCyclesSinceSent[PduId] >= COM_TX_PERIODIC_FLOOR_CYCLES);

        if (!shouldSend)
        {
            DET_LOGD(TAG, "TX skip iPdu=%u (no change)", (unsigned)PduId);
            return E_OK;
        }

        if (Com_TxEnabled == 0U)
        {
            /* 診断 CommunicationControl (UDS 0x28) による送信抑制中。
             * SWS_Com_00334 の説明文・SWS_Com_00777 により、停止中に発生した
             * 送信トリガーは保持せず破棄することが要求されている
             * （「再開しても古いトリガーで即座に送信してはならない」）。
             * そのため Com_TxUpdatePending/Com_TxCyclesSinceSent をここで
             * リセットする。バッファの値自体は Com_SendSignal() が既に
             * 更新済みのため失われない。再開後に実際に値が変化した時、
             * または通常の周期フロアに新たに達した時に初めて送信される。 */
            Com_TxUpdatePending[PduId]   = 0U;
            Com_TxCyclesSinceSent[PduId] = 0U;
            DET_LOGD(TAG, "TX skip iPdu=%u (CommunicationControl disabled)", (unsigned)PduId);
            return E_OK;
        }

        /* 実際に送信を決定したときだけカウンタをリセットする
         * (送信を見送った呼び出しでリセットすると、周期フロアが永遠に
         * 到達できなくなってしまう)。 */
        Com_TxUpdatePending[PduId]   = 0U;
        Com_TxCyclesSinceSent[PduId] = 0U;

        /* 送信直前の最終変換フック（E2E Transformer 等）。
         * PduR_Transmit() へ渡す実 TX バッファへ直接書き込ませる。 */
        if (ipdu->TxTransformCbk != NULL)
            ipdu->TxTransformCbk(Com_TxBuffer[PduId], ipdu->DLC);

        char hexbuf[25];
        Log_HexStr(hexbuf, sizeof(hexbuf), Com_TxBuffer[PduId], ipdu->DLC);
        DET_LOGI(TAG, "TX iPdu=%u [%s]", (unsigned)PduId, hexbuf);

        PduInfoType pduInfo = {
            .SduDataPtr = Com_TxBuffer[PduId],
            .SduLength  = ipdu->DLC
        };
        return PduR_Transmit(ipdu->PduRId, &pduInfo);
    }

    DET_LOGW(TAG, "TX no iPdu=%u", (unsigned)PduId);
    return E_NOT_OK;
}

/**
 * \brief   TX I-PDU の送信完了を COM へ通知する。
 *
 * \details CAN フレームの送信完了後に PduR から呼び出される。
 *          この実装ではログ出力のみ行い、リトライや締め切り処理は行わない。
 *
 * \param[in]  TxPduId  送信が完了した TX I-PDU の PduR 層 PDU ID。
 * \param[in]  result   CanIf から転送された送信結果。
 *                      E_OK = 成功、E_NOT_OK = 失敗。
 *                      TX リトライやエラーカウンタを実装しないため未使用。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \note       result を使用しない理由: Com_TxConfirmation() 自体は SWS_Com_00124
 *             どおり result 引数を持つが、本実装では呼び出し元の CanIf_TxConfirmation()
 *             が result を受け取らない 1 引数 API で、内部で常に E_OK 決め打ちで
 *             呼び出す。さらにその手前の Can_Write()（Can.c）は送信成功時のみ
 *             CanIf_TxConfirmation() を呼び、失敗時は即座に CAN_NOT_OK を同期的な
 *             戻り値で返す（TxConfirmation 自体を呼ばない）。MCP2515 との SPI 通信が
 *             同期的で非同期の送信完了割り込みを使っていないため、TX 失敗は既に
 *             呼び出し元への同期戻り値で伝わっており、この result が実際に
 *             E_NOT_OK になる経路は現状存在しない。
 *
 * \AUTOSARReq     {SWS_Com_00124}
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)result;
    DET_LOGI(TAG, "TxConf id=%u", (unsigned)TxPduId);
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
 * \AUTOSARReq     {SWS_Com_00398, SWS_Com_00684, SWS_Com_00685}
 * \ServiceID      {0x20}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_MainFunction(void)
{
    if (Com_ConfigPtr == NULL)
        return;

    if (Com_RxEnabled == 0U)
        return;  /* 受信抑制中はデッドライン監視自体を無効化する (SWS_Com_00684/00685) */

    const unsigned long now = millis();

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

/**
 * \file    CanIf.c
 * \brief   CAN インタフェース (AUTOSAR SWS_CANInterface 準拠)
 * \details CAN ドライバ (Can.c) と上位通信層 (PduR, DCM) の間に位置する
 *          AUTOSAR CanIf API を実装する。
 *          AUTOSAR 4.3.1 SWS_CANInterface 仕様に準拠し、
 *          Arduino UNO 向けに一部を簡略化している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "CanIf.h"
#include "Can.h"
#include "CanSM.h"
#include "Det.h"

#define TAG "CanIf"

static const CanIf_ConfigType* CanIf_ConfigPtr = NULL;

/* CanIf_ReadRxPduData()（[SWS_CANIF_00194]、2026-08 追加）用の内部バッファ。
 * CanIf_RxPduConfigType.ReadRxPduDataEnabled=1 の RX PDU についてのみ、
 * CanIf_RxIndication() が受信データをここへ複製する。添字は
 * CanIf_ConfigPtr->RxPduConfig[] 上の位置（CanIfRxSduId、実 AUTOSAR の
 * CanIf 内部ハンドルに相当）であり、UpperLayerRxPduId とは別の名前空間
 * （Com_RxBuffer[] 等が Com の IPduId 空間を使うのと同じ考え方）。 */
static uint8 CanIf_RxPduDataBuffer[CANIF_RX_PDU_MAX][CANIF_MAX_DLC];
static uint8 CanIf_RxPduDataLength[CANIF_RX_PDU_MAX];
static uint8 CanIf_RxPduDataValid[CANIF_RX_PDU_MAX];  /* 一度でも受信したか */

/* CanIf_SetPduMode()/CanIf_GetPduMode()（[SWS_CANIF_00137]、2026-08 追加）の
 * コントローラ単位の状態。CanIf_Transmit() はここが CANIF_ONLINE のときのみ
 * Can_Write() まで到達させる。RX 側（CanIf_RxIndication() の上位層通知）は
 * 意図的にこの状態でゲートしない: 本プロジェクトが必要とする唯一の用途
 * （CanSM の SILENT_COMMUNICATION、Nm/Com の受信処理は継続させたい）では
 * CANIF_OFFLINE を実際には使わないため（CanIf_RxIndication() の doc コメント
 * 参照）。 */
static CanIf_PduModeType CanIf_ControllerPduMode[CANIF_CONTROLLER_MAX];

/* CanIf_SetControllerMode()/CanIf_GetControllerMode()（[SWS_CANIF_00003]/
 * [SWS_CANIF_00229]、2026-08 追加）が追跡するコントローラ状態。
 * 実 AUTOSAR は CanIf_ControllerModeIndication()（Can からの非同期通知）で
 * 実際の遷移完了を知るが、本プロジェクトの Can_SetControllerMode() は
 * 同期的に完了するため、要求成功時点で直接更新する形に簡略化している。
 * CanIf_SetControllerMode(CAN_CS_STOPPED) を要求されたとき、その場に応じて
 * Can_StateTransitionType(CAN_T_STOP か CAN_T_WAKEUP か) のどちらを
 * Can_SetControllerMode() に渡すべきかは遷移元状態に依存する（Can.c の
 * Can_SetControllerMode() 実装コメント参照）ため、この配列が無いと
 * 判断できない。 */
static Can_ControllerStateType CanIf_ControllerMode[CANIF_CONTROLLER_MAX];

/**
 * \brief   CAN インタフェースモジュールを初期化する。
 *
 * \details 設定ポインタを保存し、TX/RX PDU 数をログ出力する。
 *          Can_Init() の呼び出し後、他のすべての CanIf_* API より
 *          先に 1 回だけ呼び出すこと。
 *
 * \param[in]  ConfigPtr  CanIf 設定構造体へのポインタ。NULL 禁止。
 *
 * \pre        Can_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_CANIF_00001}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");

    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init: NULL ConfigPtr");
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_INIT, CANIF_E_PARAM_POINTER);
        return;
    }

    /* CanIf_RxPduDataBuffer[]/Length[]/Valid[] は CANIF_RX_PDU_MAX
     * （native_chain バイナリ全体で共有される固定サイズ）でしか確保されて
     * いない。ConfigPtr->RxPduCount がこれを超えると
     * CanIf_RxIndication()/CanIf_ReadRxPduData() が範囲外書き込み/読み出しを
     * 起こしうるため、Com_Init() の RxIPduCount/TxIPduCount ガードと同じ
     * 方針で初期化自体を拒否する（/code-review・/simplify 双方の指摘で
     * 発覚: この配列は元々存在せず、本チェックも今回追加するまで無かった）。 */
    if (ConfigPtr->RxPduCount > CANIF_RX_PDU_MAX)
    {
        DET_LOGE(TAG, "Init E: RxPduCount>max");
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_INIT, CANIF_E_INIT_FAILED);
        return;
    }

    CanIf_ConfigPtr = ConfigPtr;

    /* CanIf_ReadRxPduData() 用バッファの初期化（[SWS_CANIF_00194]）。
     * CanIf_RxPduDataValid をクリアすることが本質で、Buffer/Length は
     * Valid=0 の間は参照されないため実際にはゼロクリア不要だが、
     * Com_Init() の同種ループと同じく再初期化を明示しておく。 */
    for (uint8 i = 0U; i < CANIF_RX_PDU_MAX; i++)
    {
        CanIf_RxPduDataLength[i] = 0U;
        CanIf_RxPduDataValid[i]  = 0U;
    }

    for (uint8 i = 0U; i < CANIF_CONTROLLER_MAX; i++)
    {
        /* [SWS_CANIF_00137] の初期値: Init 直後は配下のコントローラもまだ
         * 起動していない（CanIf_Init() の Note 参照）ため CANIF_OFFLINE とする。
         * 実際に FULL_COM へ遷移する際に CanSM が CanIf_SetPduMode(CANIF_ONLINE)
         * を呼ぶ。 */
        CanIf_ControllerPduMode[i] = CANIF_OFFLINE;

        /* Can_Init() 完了直後の実際のコントローラ状態（CAN_CS_STOPPED、Can.c の
         * Can_Init() 参照）に合わせる。CanIf は Can_Init() の後に初期化される
         * 前提（本関数の \pre 参照）。 */
        CanIf_ControllerMode[i] = CAN_CS_STOPPED;
    }

    DET_LOGI(TAG, "Init ok TX=%u RX=%u",
             (unsigned)ConfigPtr->TxPduCount, (unsigned)ConfigPtr->RxPduCount);
}

/**
 * \brief   CAN インタフェースモジュールを未初期化状態に戻す。
 *
 * \details 設定ポインタを NULL に戻す。CanIf の他 API 同様、未初期化状態
 *          チェックに Det_ReportError は用いない（本ファイル冒頭のコメント
 *          参照）。
 *
 * \AUTOSARReq     {SWS_CANIF_91002}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_DeInit(void)
{
    CanIf_ConfigPtr = NULL;
    DET_LOGI(TAG, "DeInit ok");
}

/**
 * \brief   CAN ドライバ経由で PDU の送信を要求する。
 *
 * \details TxPduId で TX PDU 設定を検索し、PDU 長を設定 DLC と照合したうえで
 *          Can_PduType を構築して Can_Write() を呼び出す。
 *
 * \param[in]  TxPduId     送信する TX PDU の ID。
 *                         設定済み TxPduCount 未満であること。
 * \param[in]  PduInfoPtr  送信するデータと長さへのポインタ。
 *                         NULL 禁止。SduDataPtr も NULL 禁止。
 *
 * \retval  E_OK      PDU が Can_Write() に正常に渡された。
 * \retval  E_NOT_OK  CanIf 未初期化、TxPduId 不正、NULL ポインタ、
 *                    SduLength が設定 DLC を超過、PDU チャネルが
 *                    CANIF_ONLINE でない（CanIf_SetPduMode() 参照）、
 *                    または Can_Write() 失敗。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 * \pre        CAN コントローラが CAN_CS_STARTED 状態であること。
 *
 * \AUTOSARReq     {SWS_CANIF_00005}
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        DET_LOGE(TAG, "TX E: invalid TxPduId");
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_TRANSMIT, CANIF_E_INVALID_TXPDUID);
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        DET_LOGE(TAG, "TX E: PduInfoPtr NULL");
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_TRANSMIT, CANIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    if (PduInfoPtr->SduLength > txCfg->Dlc)
    {
        DET_LOGE(TAG, "TX E: SduLength>DLC");
        return E_NOT_OK;
    }

    /* [SWS_CANIF_00137]/[SWS_CANIF_00074] 相当: PDU チャネルが CANIF_ONLINE
     * でなければ送信しない（CANIF_OFFLINE/CANIF_TX_OFFLINE いずれも TX 禁止）。
     * 本プロジェクトは単一コントローラのため添字は固定で 0。DET は報告しない
     * （TxIpduCalloutCbk による拒否と同じ扱い、呼び出し元の CanSM が既に
     * 意図して TX_OFFLINE にしている想定のため）。 */
    if (CanIf_ControllerPduMode[0] != CANIF_ONLINE)
    {
        DET_LOGD(TAG, "TX iPdu=%u rejected: PduMode not ONLINE", (unsigned)TxPduId);
        return E_NOT_OK;
    }

    Can_PduType canPdu = {
        .swPduHandle = TxPduId,
        .id          = txCfg->CanId,
        .length      = (uint8)PduInfoPtr->SduLength,
        .sdu         = PduInfoPtr->SduDataPtr
    };

    DET_LOGI(TAG, "TX id=%u can=0x%lX", (unsigned)TxPduId, (unsigned long)txCfg->CanId);

    Can_ReturnType ret = Can_Write(txCfg->Hth, &canPdu);

    if (ret == CAN_BUSY)
        DET_LOGW(TAG, "TX BUSY");

    return (ret == CAN_OK) ? E_OK : E_NOT_OK;
}

/**
 * \brief   CAN ドライバから受信フレームを上位層へ通知する。
 *
 * \details CAN ドライバがフレームを受信した際に呼び出される。
 *          RX PDU テーブルから HOH と CAN ID が一致するエントリを検索し、
 *          設定された上位層の RxIndication コールバックへ転送する。
 *          一致するエントリが存在しない場合はフレームを破棄してログを出力する。
 *          一致したエントリの設定 DLC に満たない L-PDU も上位層へ渡さず棄却する
 *          （データ長チェック、違反時は CANIF_E_INVALID_DATA_LENGTH 相当）。
 *
 *          上位 PDU への振り分け結果に関わらず、CanSM_RxIndication() を
 *          呼び出して「有効なフレームを受信した」ことを CanSM へ通知する
 *          (AUTOSAR SWS_CanSM の CanSMRxIndicationUsed に相当)。通常運用中は
 *          無害だが、ウェイクアップ検証中はこれが検証成功の唯一の合図になる
 *          （詳細は CanSM_RxIndication() を参照）。
 *
 *          `ReadRxPduDataEnabled=1` の RX PDU では、上位層コールバックの
 *          呼び出しに加えて内部バッファも更新し、`CanIf_ReadRxPduData()`
 *          （[SWS_CANIF_00194]）でのポーリング取得に備える。
 *
 * \param[in]  Mailbox     受信 CAN ID・HOH・コントローラ ID を格納した
 *                         ハードウェアメールボックス記述子へのポインタ。
 *                         NULL 禁止。
 * \param[in]  PduInfoPtr  受信 PDU のデータと長さへのポインタ。
 *                         NULL 禁止。SduDataPtr も NULL 禁止
 *                         （CanIf_Transmit と対称の入力検証）。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_CANIF_00415, SWS_CANIF_00026, SWS_CANIF_00168}
 * \ServiceID      {0x14}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return;  /* [SWS_CANIF_00421]: 未初期化時は黙って何もしない（DET 報告なし） */

    if (Mailbox == NULL || PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        DET_LOGE(TAG, "RX: NULL Mailbox/PduInfoPtr");
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_RX_INDICATION, CANIF_E_PARAM_POINTER);
        return;
    }

    CanSM_RxIndication(Mailbox->ControllerId);

    /* SWS_CANIF_00416/00417: Hoh 自体が未設定なのか（HOH エラー）、Hoh は
     * 設定済みだが CanId が想定と異なるのか（CanId エラー）を区別して
     * 報告するため、ループ内で Hoh 一致の有無を別途記録する。 */
    uint8 hohMatched = 0U;

    for (uint8 i = 0; i < CanIf_ConfigPtr->RxPduCount; i++)
    {
        const CanIf_RxPduConfigType* rxCfg = &CanIf_ConfigPtr->RxPduConfig[i];

        if (rxCfg->Hrh != Mailbox->Hoh)
            continue;

        hohMatched = 1U;

        if (rxCfg->CanId != Mailbox->CanId)
            continue;

        /* SWS_CANIF_00026/00168 相当: 設定 DLC に満たない L-PDU は
         * 上位層へ渡さず棄却する。Com/CanTp 側にも独自の受信長チェックが
         * あるが、本来この責務は CanIf 層にある。CanIf にチェックがないと、
         * 将来 PduR に新しいルートが追加された際、上位層側でチェックを
         * 入れ忘れるリスクを CanIf 一層で防げなくなる。 */
        if (PduInfoPtr->SduLength < rxCfg->Dlc)
        {
            DET_LOGW(TAG, "RX can=0x%lX length mismatch got=%u exp=%u",
                     (unsigned long)Mailbox->CanId,
                     (unsigned)PduInfoPtr->SduLength, (unsigned)rxCfg->Dlc);
            return;
        }

        DET_LOGI(TAG, "RX can=0x%lX pdu=%u",
                 (unsigned long)Mailbox->CanId,
                 (unsigned)rxCfg->UpperLayerRxPduId);

        /* CanIf_ReadRxPduData() 用バッファ更新（[SWS_CANIF_00194]、
         * ReadRxPduDataEnabled=1 の RX PDU のみ）。上のデータ長チェックは
         * SduLength < rxCfg->Dlc（不足）のみを棄却し、超過は素通りするため、
         * ここでは rxCfg->Dlc（この PDU 自身の設定値）でクランプする
         * （CANIF_MAX_DLC ではない——CanIf_ReadRxPduData() の呼び出し元が
         * 「この PDU の設定 Dlc 分だけ確保すれば十分」と信頼できるように
         * するため。/code-review 指摘: 当初 CANIF_MAX_DLC でクランプしており、
         * Dlc より大きい異常フレームを受けた場合に契約を超えるデータ長を
         * 返しうる状態だった）。 */
        if (rxCfg->ReadRxPduDataEnabled != 0U)
        {
            const uint8 copyLen = (PduInfoPtr->SduLength <= rxCfg->Dlc)
                                       ? (uint8)PduInfoPtr->SduLength
                                       : rxCfg->Dlc;
            for (uint8 b = 0U; b < copyLen; b++)
                CanIf_RxPduDataBuffer[i][b] = PduInfoPtr->SduDataPtr[b];
            CanIf_RxPduDataLength[i] = copyLen;
            CanIf_RxPduDataValid[i]  = 1U;
        }

        if (rxCfg->RxIndicationFct != NULL)
            rxCfg->RxIndicationFct(rxCfg->UpperLayerRxPduId, PduInfoPtr);

        return;
    }

    DET_LOGW(TAG, "RX no match can=0x%lX", (unsigned long)Mailbox->CanId);
    if (hohMatched)
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_RX_INDICATION, CANIF_E_PARAM_CANID);
    else
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_RX_INDICATION, CANIF_E_PARAM_HOH);
}

/**
 * \brief   RX PDU の直近受信データをポーリングで取得する。
 *
 * \details `CanIf_RxIndication()` が上位層コールバックへプッシュ配送する
 *          経路とは独立した、ポーリングによる受信データ取得経路
 *          （実 AUTOSAR の I-PDU callout 等とも異なる、CanIf 自身が保持する
 *          内部バッファへのアクセサ）。対象 RX PDU は
 *          `CanIf_RxPduConfigType.ReadRxPduDataEnabled=1` で事前に
 *          opt-in されている必要がある（[SWS_CANIF_00325]、既定は 0
 *          ＝バッファリングなし）。
 *
 *          `CanIfRxSduId` は `CanIf_ConfigPtr->RxPduConfig[]` 上の位置
 *          （`CanIf_RxIndication()` のループ添字 `i` と同じ名前空間）であり、
 *          上位層へ渡す `UpperLayerRxPduId` とは別の ID 空間である点に注意
 *          （実 AUTOSAR の "CanIf 内部ハンドル" に相当）。
 *
 * \note    本プロジェクトは [SWS_CANIF_00324]（コントローラが
 *          CAN_CS_STARTED かつ受信パスが online でなければ E_NOT_OK）は
 *          実装しない。CanIf 自身はコントローラ状態を一切追跡しておらず
 *          （Can_MainFunction_Read() から渡されたフレームをそのまま
 *          振り分けるだけの設計）、この状態管理は CanSM の責務のため
 *          スコープ外とする。「一度も受信していない PDU は E_NOT_OK」
 *          （spec 原文 "No valid data has been received"）のみ実装する。
 *
 * \param[in]   CanIfRxSduId    データを取得する RX PDU の ID
 *                              （`CanIf_ConfigPtr->RxPduConfig[]` の添字）。
 * \param[out]  CanIfRxInfoPtr  取得したデータの格納先。`SduDataPtr` は
 *                              対象 PDU の設定 `Dlc` バイト分確保しておけば
 *                              十分（バッファ自体の格納長も `Dlc` でクランプ
 *                              される。詳細は `CanIf_RxIndication()` の
 *                              実装コメント参照）。NULL 禁止。
 *
 * \retval  E_OK      データを `CanIfRxInfoPtr` へ格納した。
 * \retval  E_NOT_OK  CanIf 未初期化、CanIfRxSduId が範囲外、対象 PDU が
 *                    `ReadRxPduDataEnabled=0`、`CanIfRxInfoPtr`/
 *                    `SduDataPtr` が NULL、またはこの PDU をまだ一度も
 *                    受信していない。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \note    本プロジェクトは単一の協調的スーパーループ（Os の各タスクが
 *          プリエンプションなしで順次実行される、`Os_PBCfg.c` 参照）で
 *          動作するため、`CanIf_RxIndication()`（Reentrant）と本関数
 *          （Non Reentrant）が実際に競合して同じ `CanIf_RxPduDataBuffer[]`
 *          要素を同時に読み書きすることはない（`CanIf_RxIndication()` 自体
 *          も真の割り込みコンテキストではなく `Can_MainFunction_Read()`
 *          からポーリングで呼ばれる、本ファイル冒頭の
 *          `CanIf_RxIndication()` の Doxygen コメント参照）。マルチコア化
 *          等でこの前提が崩れる場合は、Rte.c の
 *          `SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()` と同様の排他区間が
 *          必要になる（/code-review 指摘）。
 *
 * \AUTOSARReq     {SWS_CANIF_00194, SWS_CANIF_00325, SWS_CANIF_00326}
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanIf_ReadRxPduData(PduIdType CanIfRxSduId, PduInfoType* CanIfRxInfoPtr)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;  /* CanIf の他 API と同じ方針、CanIf_Cfg.h 冒頭コメント参照 */

    if (CanIfRxSduId >= CanIf_ConfigPtr->RxPduCount)
    {
        DET_LOGE(TAG, "ReadRxPduData E: invalid CanIfRxSduId");
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_READ_RX_PDU_DATA, CANIF_E_INVALID_RXPDUID);
        return E_NOT_OK;
    }

    if (CanIfRxInfoPtr == NULL || CanIfRxInfoPtr->SduDataPtr == NULL)
    {
        DET_LOGE(TAG, "ReadRxPduData E: NULL CanIfRxInfoPtr");
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_READ_RX_PDU_DATA, CANIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const CanIf_RxPduConfigType* rxCfg = &CanIf_ConfigPtr->RxPduConfig[CanIfRxSduId];
    if (rxCfg->ReadRxPduDataEnabled == 0U)
    {
        /* [SWS_CANIF_00325]: opt-in されていない PDU の要求も開発エラー */
        DET_LOGE(TAG, "ReadRxPduData E: CanIfRxSduId=%u not configured for buffering",
                 (unsigned)CanIfRxSduId);
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_READ_RX_PDU_DATA, CANIF_E_INVALID_RXPDUID);
        return E_NOT_OK;
    }

    if (CanIf_RxPduDataValid[CanIfRxSduId] == 0U)
    {
        /* spec 原文: "E_NOT_OK: No valid data has been received"。まだ一度も
         * 受信していないだけの正常な状態のため Det_ReportError() は呼ばない。 */
        return E_NOT_OK;
    }

    const uint8 len = CanIf_RxPduDataLength[CanIfRxSduId];
    for (uint8 b = 0U; b < len; b++)
        CanIfRxInfoPtr->SduDataPtr[b] = CanIf_RxPduDataBuffer[CanIfRxSduId][b];
    CanIfRxInfoPtr->SduLength = len;

    return E_OK;
}

/**
 * \brief   CAN フレームの送信完了を上位層へ通知する。
 *
 * \details CAN ドライバが送信完了を確認した後に呼び出される。
 *          CanTxPduId で TX PDU 設定を検索し、設定された上位層の
 *          TxConfirmation コールバックを呼び出す。
 *          CanTxPduId が範囲外の場合は処理を無視する。
 *
 * \param[in]  CanTxPduId  送信が完了した TX PDU の ID。
 *                         設定済み TxPduCount 未満であること。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_CANIF_00007}
 * \ServiceID      {0x13}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return;

    if (CanTxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_TX_CONFIRMATION, CANIF_E_PARAM_LPDU);
        return;
    }

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[CanTxPduId];

    DET_LOGI(TAG, "TxConf id=%u", (unsigned)CanTxPduId);

    if (txCfg->TxConfirmFct != NULL)
        txCfg->TxConfirmFct(txCfg->UpperLayerTxPduId, E_OK);
}

/**
 * \brief   CAN コントローラの Bus-Off 状態を上位層へ通知する。
 *
 * \details Can_MainFunction_BusOff() が Bus-Off を検出した際に呼び出される。
 *          CanSM_ControllerBusOff() へ委譲し、回復シーケンスを起動する。
 *
 * \param[in]  ControllerId  Bus-Off を検出したコントローラ ID。
 *
 * \AUTOSARReq     {SWS_CANIF_00218}
 * \ServiceID      {0x16}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_ControllerBusOff(uint8 ControllerId)
{
    DET_LOGT(TAG, "called");

    if (ControllerId != 0U)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_CONTROLLER_BUSOFF, CANIF_E_PARAM_CONTROLLERID);
        return;
    }

    DET_LOGW(TAG, "ControllerBusOff ch=%u", (unsigned)ControllerId);
    CanSM_ControllerBusOff(ControllerId);
}

/**
 * \brief   CAN コントローラのウェイクアップ（スリープからの復帰）を上位層へ通知する。
 *
 * \details Can_MainFunction_Wakeup() が CAN_CS_SLEEP 中に Can_Isr()（INT ピン
 *          立ち下がり割り込み）が検出したウェイクアップ（バス活動による
 *          MCP2515 の自律的なウェイクアップ）を確認した際に呼び出される。
 *          CanSM_ControllerWakeup() へ委譲する。
 *
 * \param[in]  ControllerId  ウェイクアップを検出したコントローラ ID。
 *
 * \ServiceID      {0x17}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_ControllerWakeup(uint8 ControllerId)
{
    DET_LOGI(TAG, "ControllerWakeup ch=%u", (unsigned)ControllerId);
    CanSM_ControllerWakeup(ControllerId);
}

/**
 * \brief   PDU チャネル（コントローラ単位）の送受信有効/無効状態を設定する。
 *
 * \details 実 AUTOSAR の主な用途は CanSM が SILENT_COMMUNICATION 等の
 *          通信モードを実現するための下位レイヤ操作。`CANIF_TX_OFFLINE`
 *          にすると、以後 `CanIf_Transmit()` は `E_NOT_OK` を返し
 *          `Can_Write()` まで到達しなくなる（コントローラ自体は
 *          `CAN_CS_STARTED` のまま、送信のみを禁止する）。
 *
 *          [SWS_CANIF_00874] は「対象コントローラが `CAN_CS_STARTED` でない
 *          場合は `E_NOT_OK`」と規定するが、本プロジェクトの CanIf は
 *          コントローラ状態を一切追跡しない既存方針（`CanIf_ReadRxPduData()`
 *          の doc コメント参照）のため、このチェックは実装しない。
 *
 *          RX 側（`CanIf_RxIndication()` の上位層通知抑制）は本 API では
 *          制御しない（本ファイル冒頭の `CanIf_ControllerPduMode` 宣言コメント
 *          参照）。
 *
 * \param[in]  ControllerId    対象コントローラの ID。
 * \param[in]  PduModeRequest  要求する PDU モード。
 *
 * \retval  E_OK      要求を受け付けた。
 * \retval  E_NOT_OK  ControllerId が範囲外、または PduModeRequest が
 *                     `CanIf_PduModeType` の定義値以外。
 *
 * \AUTOSARReq     {SWS_CANIF_00137, SWS_CANIF_00341, SWS_CANIF_00860}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, CanIf_PduModeType PduModeRequest)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (ControllerId >= CANIF_CONTROLLER_MAX)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_SET_PDU_MODE, CANIF_E_PARAM_CONTROLLERID);
        return E_NOT_OK;
    }

    if (PduModeRequest != CANIF_OFFLINE && PduModeRequest != CANIF_TX_OFFLINE && PduModeRequest != CANIF_ONLINE)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_SET_PDU_MODE, CANIF_E_PARAM_PDU_MODE);
        return E_NOT_OK;
    }

    DET_LOGI(TAG, "SetPduMode ch=%u mode=%u", (unsigned)ControllerId, (unsigned)PduModeRequest);
    CanIf_ControllerPduMode[ControllerId] = PduModeRequest;
    return E_OK;
}

/**
 * \brief   PDU チャネル（コントローラ単位）の現在の送受信有効/無効状態を取得する。
 *
 * \param[in]   ControllerId  対象コントローラの ID。
 * \param[out]  PduModePtr    現在の PDU モードの格納先。NULL 禁止。
 *
 * \retval  E_OK      PduModePtr へ格納した。
 * \retval  E_NOT_OK  ControllerId が範囲外、または PduModePtr が NULL。
 *
 * \AUTOSARReq     {SWS_CANIF_00009, SWS_CANIF_00346, SWS_CANIF_00657}
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant (Not for the same channel)}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType* PduModePtr)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (ControllerId >= CANIF_CONTROLLER_MAX)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_GET_PDU_MODE, CANIF_E_PARAM_CONTROLLERID);
        return E_NOT_OK;
    }

    if (PduModePtr == NULL)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_GET_PDU_MODE, CANIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *PduModePtr = CanIf_ControllerPduMode[ControllerId];
    return E_OK;
}

/**
 * \brief   CAN コントローラの動作モード（Started/Sleep/Stopped）の遷移を要求する。
 *
 * \details 下位の Can_SetControllerMode() を呼び出す（[SWS_CANIF_00308]）。
 *          実 AUTOSAR は目標モードから実際の Can_StateTransitionType への
 *          変換に Can 側の非同期通知（CanIf_ControllerModeIndication、
 *          本実装は未実装）で得た現在状態を使うが、本プロジェクトの
 *          Can_SetControllerMode() は同期的に完了するため、CanIf 自身が
 *          追跡する `CanIf_ControllerMode[]`（前回の成功要求の結果）だけで
 *          十分に判定できる。
 *
 *          CAN_CS_STARTED/CAN_CS_SLEEP は遷移元によらず常に単一の
 *          Transition（CAN_T_START/CAN_T_SLEEP）に対応する（Can.c の
 *          Can_SetControllerMode() が両方とも複数の遷移元を許容する設計の
 *          ため）。CAN_CS_STOPPED のみ遷移元に応じて CAN_T_STOP
 *          （CAN_CS_STARTED から）と CAN_T_WAKEUP（CAN_CS_SLEEP から）を
 *          使い分ける必要がある。
 *
 * \param[in]  ControllerId    対象コントローラの ID。
 * \param[in]  ControllerMode  要求する目標モード。CAN_CS_UNINIT は無効。
 *
 * \retval  E_OK      要求を受け付け、Can_SetControllerMode() が成功した。
 * \retval  E_NOT_OK  ControllerId が範囲外、ControllerMode が
 *                     CAN_CS_STARTED/SLEEP/STOPPED 以外、または
 *                     Can_SetControllerMode() が失敗した。
 *
 * \AUTOSARReq     {SWS_CANIF_00003, SWS_CANIF_00308, SWS_CANIF_00311, SWS_CANIF_00774}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant (Not for the same controller)}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (ControllerId >= CANIF_CONTROLLER_MAX)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_SET_CONTROLLER_MODE, CANIF_E_PARAM_CONTROLLERID);
        return E_NOT_OK;
    }

    Can_StateTransitionType transition;
    switch (ControllerMode)
    {
        case CAN_CS_STARTED:
            transition = CAN_T_START;
            break;
        case CAN_CS_SLEEP:
            transition = CAN_T_SLEEP;
            break;
        case CAN_CS_STOPPED:
            transition = (CanIf_ControllerMode[ControllerId] == CAN_CS_SLEEP) ? CAN_T_WAKEUP : CAN_T_STOP;
            break;
        default:
            Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_SET_CONTROLLER_MODE, CANIF_E_PARAM_CTRLMODE);
            return E_NOT_OK;
    }

    if (Can_SetControllerMode(ControllerId, transition) != CAN_OK)
        return E_NOT_OK;

    DET_LOGI(TAG, "SetControllerMode ch=%u mode=%u", (unsigned)ControllerId, (unsigned)ControllerMode);
    CanIf_ControllerMode[ControllerId] = ControllerMode;
    return E_OK;
}

/**
 * \brief   CAN コントローラの現在の動作モードを取得する。
 *
 * \param[in]   ControllerId     対象コントローラの ID。
 * \param[out]  ControllerModePtr  現在のモードの格納先。NULL 禁止。
 *
 * \retval  E_OK      ControllerModePtr へ格納した。
 * \retval  E_NOT_OK  ControllerId が範囲外、または ControllerModePtr が NULL。
 *
 * \AUTOSARReq     {SWS_CANIF_00229, SWS_CANIF_00313, SWS_CANIF_00656}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType* ControllerModePtr)
{
    DET_LOGT(TAG, "called");

    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (ControllerId >= CANIF_CONTROLLER_MAX)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_GET_CONTROLLER_MODE, CANIF_E_PARAM_CONTROLLERID);
        return E_NOT_OK;
    }

    if (ControllerModePtr == NULL)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_GET_CONTROLLER_MODE, CANIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *ControllerModePtr = CanIf_ControllerMode[ControllerId];
    return E_OK;
}

/**
 * \brief   CAN インタフェースモジュールのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");

    if (versioninfo == NULL)
    {
        Det_ReportError(CANIF_MODULE_ID, 0U, CANIF_API_ID_GET_VERSION_INFO, CANIF_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = CANIF_VENDOR_ID;
    versioninfo->moduleID         = CANIF_MODULE_ID;
    versioninfo->sw_major_version = CANIF_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = CANIF_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = CANIF_SW_PATCH_VERSION;
}

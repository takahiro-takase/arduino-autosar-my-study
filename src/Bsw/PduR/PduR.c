/**
 * \file    PduR.c
 * \brief   PDU ルータ (AUTOSAR SWS_PDURouter 準拠)
 * \details CanIf と上位通信モジュール (COM, DCM) の間に位置する
 *          AUTOSAR PduR ルーティング層を実装する。
 *          受信 PDU を 1 つ以上の上位層モジュールへ配信（マルチキャスト）し、
 *          送信要求と送信完了通知を COM と CanIf 間で転送する。
 *          AUTOSAR 4.3.1 SWS_PDURouter 仕様に準拠し、
 *          Arduino UNO 向けに一部を簡略化している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "PduR.h"
#include "CanIf.h"
#include "Det.h"

#define TAG "PduR"

static const PduR_PBConfigType* PduR_ConfigPtr = NULL;

/**
 * \brief   PDU ルータモジュールを初期化する。
 *
 * \details すべての RX ルーティングパスを検証し、ポストビルド設定ポインタを
 *          保存する (AUTOSAR SWS_PduR_00119)。
 *          TX/RX ルーティングテーブルの内容をログ出力する。
 *          いずれかの RX パスに転送先がない場合は初期化を中断する。
 *
 * \param[in]  ConfigPtr  PduR ポストビルド設定へのポインタ。NULL 禁止。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_Init(const PduR_PBConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init E: config NULL");
        return;
    }

    for (uint8 i = 0; i < ConfigPtr->RxPathCount; i++)
    {
        if (ConfigPtr->RxPaths[i].Dests == NULL || ConfigPtr->RxPaths[i].DestCount == 0)
        {
            DET_LOGE(TAG, "Init E: RxPath[%u] no dests", (unsigned)i);
            return;
        }
    }

    PduR_ConfigPtr = ConfigPtr;
    DET_LOGI(TAG, "Init ok RX=%u TX=%u",
             (unsigned)ConfigPtr->RxPathCount, (unsigned)ConfigPtr->TxPathCount);
}

/**
 * \brief   CanIf から受信した PDU をすべての上位層へルーティングする。
 *
 * \details PduR_CanIf.h の `#define PduR_CanIfRxIndication PduR_ComRxIndication`
 *          を経由して CanIf から間接的に呼び出される (AUTOSAR SWS_PduR_00369)。
 *          RxPduId に一致する RX ルーティングパスを検索し、設定された
 *          すべての転送先の RxIndFct コールバックを呼び出す（マルチキャスト）。
 *          一致するパスが存在しない場合は PDU を破棄する。
 *
 * \param[in]  RxPduId     CanIf が割り当てた送信元 PDU ID。
 *                         RX ルーティングパスの検索に使用する。
 * \param[in]  PduInfoPtr  受信 PDU のデータと長さへのポインタ。NULL 禁止。
 *
 * \pre        PduR_Init() が正常に完了していること。
 * \note       アプリケーションコードから PduR_ComRxIndication を直接呼び出さず、
 *             #define エイリアスの PduR_CanIfRxIndication 経由で使用すること。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < PduR_ConfigPtr->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &PduR_ConfigPtr->RxPaths[i];

        if (path->SrcPduId != RxPduId)
            continue;

        for (uint8 d = 0; d < path->DestCount; d++)
        {
            const PduR_RxDestType* dest = &path->Dests[d];

            DET_LOGD(TAG, "RxInd src=%u mod=%u dst=%u",
                     (unsigned)RxPduId, (unsigned)dest->Module,
                     (unsigned)dest->DestPduId);

            if (dest->RxIndFct != NULL)
                dest->RxIndFct(dest->DestPduId, PduInfoPtr);
        }
        return;
    }

    DET_LOGW(TAG, "RxInd no route src=%u", (unsigned)RxPduId);
}

/**
 * \brief   CanIf からの TX 送信完了通知を上位層へ転送する。
 *
 * \details CAN フレームの送信完了後に CanIf から呼び出される。
 *          TxPduId に一致する TX ルーティングパスを検索し、設定された
 *          上位層の確認コールバックを呼び出す (AUTOSAR SWS_PduR_00365)。
 *
 * \param[in]  TxPduId  送信が完了した PDU の ID（CanIf 名前空間）。
 *                      TX ルーティングパスの検索に使用する。
 * \param[in]  result   CanIf からの送信結果。
 *                      E_OK = 成功、E_NOT_OK = 失敗。
 *                      上位層（COM）の TxConfirmation へそのまま転送する。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    if (PduR_ConfigPtr == NULL)
        return;

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != TxPduId)
            continue;

        DET_LOGD(TAG, "TxConf src=%u dst=%u",
                 (unsigned)TxPduId, (unsigned)path->ConfDestPduId);

        if (path->ConfFct != NULL)
            path->ConfFct(path->ConfDestPduId, result);

        return;
    }

    DET_LOGW(TAG, "TxConf no route src=%u", (unsigned)TxPduId);
}

/**
 * \brief   上位層からの PDU 送信要求を CanIf へ転送する。
 *
 * \details COM 等の上位層から PDU 送信を要求された際に呼び出される。
 *          SrcPduId に一致する TX ルーティングパスを検索し、
 *          CanIf_Transmit() へ転送する (AUTOSAR SWS_PduR_00109)。
 *          一致するパスが存在しない場合は即座に E_NOT_OK を返す。
 *
 * \param[in]  SrcPduId    送信する上位層 PDU の ID。
 *                         TX ルーティングパスの検索に使用する。
 * \param[in]  PduInfoPtr  送信するデータと長さへのポインタ。NULL 禁止。
 *
 * \retval  E_OK      PDU が CanIf_Transmit() に正常に転送された。
 * \retval  E_NOT_OK  PduR 未初期化、PduInfoPtr が NULL、
 *                    一致する TX ルーティングパスなし、
 *                    または CanIf_Transmit() が失敗した。
 *
 * \pre        PduR_Init() が正常に完了していること。
 * \pre        CAN コントローラが CAN_CS_STARTED 状態であること。
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL || PduInfoPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != SrcPduId)
            continue;

        DET_LOGD(TAG, "TX src=%u canif=%u",
                 (unsigned)SrcPduId, (unsigned)path->CanIfTxPduId);

        return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
    }

    DET_LOGW(TAG, "TX no route src=%u", (unsigned)SrcPduId);
    return E_NOT_OK;
}

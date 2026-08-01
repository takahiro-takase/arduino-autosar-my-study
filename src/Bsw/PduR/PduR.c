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
 * \brief   SrcPduId に一致する TX ルーティングパスを検索する。
 *
 * \details PduR_CanIfTxConfirmation/PduR_Transmit/PduR_SecOCTransmit が
 *          共通で行う「TxPaths を SrcPduId で線形探索する」処理をまとめたもの。
 *
 * \param[in]  SrcPduId  検索する送信元 PDU ID。
 *
 * \return  一致した TX ルーティングパスへのポインタ。一致なしなら NULL。
 *
 * \pre        PduR_ConfigPtr != NULL であること（呼び出し元で保証済み）。
 */
static const PduR_TxRoutingPathType* PduR_FindTxPath(PduIdType SrcPduId)
{
    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        if (PduR_ConfigPtr->TxPaths[i].SrcPduId == SrcPduId)
            return &PduR_ConfigPtr->TxPaths[i];
    }
    return NULL;
}

/**
 * \brief   PDU ルータモジュールを初期化する。
 *
 * \details すべての RX ルーティングパスを検証し、ポストビルド設定ポインタを
 *          保存する。TX/RX ルーティングテーブルの内容をログ出力する。
 *          いずれかの RX パスに転送先がない場合は初期化を中断する。
 *
 * \param[in]  ConfigPtr  PduR ポストビルド設定へのポインタ。NULL 禁止。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_PduR_00334}
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_Init(const PduR_PBConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init E: config NULL");
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_INIT, PDUR_E_INIT_FAILED);
        return;
    }

    for (uint8 i = 0; i < ConfigPtr->RxPathCount; i++)
    {
        if (ConfigPtr->RxPaths[i].Dests == NULL || ConfigPtr->RxPaths[i].DestCount == 0)
        {
            DET_LOGE(TAG, "Init E: RxPath[%u] no dests", (unsigned)i);
            Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_INIT, PDUR_E_INIT_FAILED);
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
 *          を経由して CanIf から間接的に呼び出される。
 *          RxPduId に一致する RX ルーティングパスを検索し、設定された
 *          すべての転送先の RxIndFct コールバックを呼び出す（マルチキャスト）。
 *          一致するパスが存在しない場合は PDU を破棄する。
 *
 * \param[in]  RxPduId     CanIf が割り当てた送信元 PDU ID。
 *                         RX ルーティングパスの検索に使用する。
 * \param[in]  PduInfoPtr  受信 PDU のデータと長さへのポインタ。
 *                         NULL 禁止。SduDataPtr も NULL 禁止
 *                         （PduR_Transmit が委譲する CanIf_Transmit と
 *                         対称の入力検証。下流の RxIndFct がそのまま
 *                         SduDataPtr を参照するため、ここで検証しておく）。
 *
 * \pre        PduR_Init() が正常に完了していること。
 * \note       アプリケーションコードから PduR_ComRxIndication を直接呼び出さず、
 *             #define エイリアスの PduR_CanIfRxIndication 経由で使用すること。
 *
 * \AUTOSARReq     {SWS_PduR_00362}
 * \ServiceID      {0x42}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL)
    {
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_RX_INDICATION, PDUR_E_UNINIT);
        return;
    }

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        DET_LOGE(TAG, "RxInd E: PduInfoPtr NULL");
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_RX_INDICATION, PDUR_E_PARAM_POINTER);
        return;
    }

    for (uint8 i = 0; i < PduR_ConfigPtr->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &PduR_ConfigPtr->RxPaths[i];

        if (path->SrcPduId != RxPduId)
            continue;

        for (uint8 d = 0; d < path->DestCount; d++)
        {
            const PduR_RxDestType* dest = &path->Dests[d];

            DET_LOGI(TAG, "RxInd src=%u mod=%u dst=%u",
                     (unsigned)RxPduId, (unsigned)dest->Module,
                     (unsigned)dest->DestPduId);

            if (dest->RxIndFct != NULL)
                dest->RxIndFct(dest->DestPduId, PduInfoPtr);
        }
        return;
    }

    DET_LOGW(TAG, "RxInd no route src=%u", (unsigned)RxPduId);
    Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_RX_INDICATION, PDUR_E_PDU_ID_INVALID);
}

/**
 * \brief   CanIf からの TX 送信完了通知を上位層へ転送する。
 *
 * \details CAN フレームの送信完了後に CanIf から呼び出される。
 *          TxPduId に一致する TX ルーティングパスを検索し、設定された
 *          上位層の確認コールバックを呼び出す。
 *
 * \param[in]  TxPduId  送信が完了した PDU の ID（CanIf 名前空間）。
 *                      TX ルーティングパスの検索に使用する。
 * \param[in]  result   CanIf からの送信結果。
 *                      E_OK = 成功、E_NOT_OK = 失敗。
 *                      上位層（COM）の TxConfirmation へそのまま転送する。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_PduR_00365}
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    if (PduR_ConfigPtr == NULL)
    {
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TX_CONFIRMATION, PDUR_E_UNINIT);
        return;
    }

    const PduR_TxRoutingPathType* path = PduR_FindTxPath(TxPduId);

    if (path == NULL)
    {
        DET_LOGW(TAG, "TxConf no route src=%u", (unsigned)TxPduId);
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TX_CONFIRMATION, PDUR_E_PDU_ID_INVALID);
        return;
    }

    DET_LOGI(TAG, "TxConf src=%u dst=%u",
             (unsigned)TxPduId, (unsigned)path->ConfDestPduId);

    if (path->ConfFct != NULL)
        path->ConfFct(path->ConfDestPduId, result);
}

/**
 * \brief   上位層からの PDU 送信要求を CanIf へ転送する。
 *
 * \details COM 等の上位層から PDU 送信を要求された際に呼び出される。
 *          SrcPduId に一致する TX ルーティングパスを検索し、
 *          CanIf_Transmit() へ転送する。
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
 * \AUTOSARReq     {SWS_PduR_00406}
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL)
    {
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TRANSMIT, PDUR_E_UNINIT);
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL)
    {
        DET_LOGE(TAG, "TX E: PduInfoPtr NULL");
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TRANSMIT, PDUR_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const PduR_TxRoutingPathType* path = PduR_FindTxPath(SrcPduId);

    if (path == NULL)
    {
        DET_LOGW(TAG, "TX no route src=%u", (unsigned)SrcPduId);
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TRANSMIT, PDUR_E_PDU_ID_INVALID);
        return E_NOT_OK;
    }

    if (path->TransmitOverrideFct != NULL)
    {
        DET_LOGI(TAG, "TX src=%u -> override id=%u",
                 (unsigned)SrcPduId, (unsigned)path->TransmitOverrideId);
        return path->TransmitOverrideFct(path->TransmitOverrideId, PduInfoPtr);
    }

    DET_LOGI(TAG, "TX src=%u canif=%u",
             (unsigned)SrcPduId, (unsigned)path->CanIfTxPduId);

    return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
}

/**
 * \brief   SecOC が変換済みの Secured I-PDU を送信する際に呼ぶ、PduR_Transmit()
 *          とは別の TX エントリポイント。
 *
 * \details PduR_Transmit() と異なり TransmitOverrideFct を一切評価せず、常に
 *          CanIf_Transmit() へ直接転送する（SecOC は既に変換を終えた後にここへ
 *          来るため、再度 TransmitOverrideFct を評価すると SecOC_IfTransmit() を
 *          無限に呼び直すことになってしまう）。SrcPduId は元の Authentic I-PDU
 *          が使っていたのと同じ値（SecOC_TxPduConfigType.PduRSrcPduId）を渡す
 *          必要がある（同じ TX ルーティングパスを再検索して CanIfTxPduId を
 *          得るため）。
 *
 * \param[in]  SrcPduId    元の Authentic I-PDU の TX ルーティングパス ID。
 * \param[in]  PduInfoPtr  送信する Secured I-PDU のデータと長さ。NULL 禁止。
 *
 * \retval  E_OK      PDU が CanIf_Transmit() に正常に転送された。
 * \retval  E_NOT_OK  PduR 未初期化、PduInfoPtr が NULL、
 *                    一致する TX ルーティングパスなし、
 *                    または CanIf_Transmit() が失敗した。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_SecOC_00062}
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType PduR_SecOCTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL)
    {
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_SECOC_TRANSMIT, PDUR_E_UNINIT);
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL)
    {
        DET_LOGE(TAG, "TX(SecOC) E: PduInfoPtr NULL");
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_SECOC_TRANSMIT, PDUR_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const PduR_TxRoutingPathType* path = PduR_FindTxPath(SrcPduId);

    if (path == NULL)
    {
        DET_LOGW(TAG, "TX(SecOC) no route src=%u", (unsigned)SrcPduId);
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_SECOC_TRANSMIT, PDUR_E_PDU_ID_INVALID);
        return E_NOT_OK;
    }

    DET_LOGI(TAG, "TX(SecOC) src=%u canif=%u",
             (unsigned)SrcPduId, (unsigned)path->CanIfTxPduId);

    return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
}

/**
 * \brief   PDU ルータモジュールのバージョン情報を取得する。
 *
 * \details [SWS_PduR_00119] が明記するとおり、PduR_Init と並び本関数だけは
 *          未初期化状態で呼ばれても PDUR_E_UNINIT を報告しない例外 API である。
 *          そのため PduR_ConfigPtr の状態は確認しない。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_PduR_00338}
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_GET_VERSION_INFO, PDUR_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = PDUR_VENDOR_ID;
    versioninfo->moduleID         = PDUR_MODULE_ID;
    versioninfo->sw_major_version = PDUR_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = PDUR_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = PDUR_SW_PATCH_VERSION;
}

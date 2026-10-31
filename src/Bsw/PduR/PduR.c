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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "PduR.h"
#include "CanIf.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "PduR"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static const PduR_PBConfigType* PduR_ConfigPtr = NULL;

/** ルーティングパスグループごとの有効（1）・無効（0）の状態。PduR_Init() が PduRIsEnabledAtInit で設定し、
 *  PduR_EnableRouting()/PduR_DisableRouting() が変える。添字は PduR_RoutingPathGroupIdType。 */
static uint8 PduR_GroupEnabled[PDUR_ROUTING_PATH_GROUP_MAX];

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static const PduR_TxRoutingPathType* PduR_FindTxPath(PduIdType SrcPduId);
static boolean PduR_IsRoutingPathEnabled(boolean isRx, PduIdType srcPduId);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_Init
 * ---------------------------------------------------------------------- */

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
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_INIT, PDUR_E_INIT_FAILED);
        return;
    }

    for (uint8 i = 0; i < ConfigPtr->RxPathCount; i++)
    {
        if ((ConfigPtr->RxPaths[i].Dests == NULL) || (ConfigPtr->RxPaths[i].DestCount == 0U))
        {
            DET_LOGE(TAG, "Init E: RxPath[%u] no dests", (unsigned)i);
            (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_INIT, PDUR_E_INIT_FAILED);
            return;
        }
    }

    if ((ConfigPtr->RoutingPathGroupCount > PDUR_ROUTING_PATH_GROUP_MAX)
        || ((ConfigPtr->RoutingPathGroupCount > 0U) && (ConfigPtr->RoutingPathGroups == NULL)))
    {
        DET_LOGE(TAG, "Init E: routing path groups invalid (count=%u)", (unsigned)ConfigPtr->RoutingPathGroupCount);
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_INIT, PDUR_E_INIT_FAILED);
        return;
    }

    for (uint8 g = 0U; g < ConfigPtr->RoutingPathGroupCount; g++)
    {
        const PduR_RoutingPathGroupType* group = &ConfigPtr->RoutingPathGroups[g];

        /* メンバー数があるのに ID 配列が無い設定は、PduR_IsRoutingPathEnabled() が NULL を参照するため、ここで弾く。 */
        if (((group->RxPathCount > 0U) && (group->RxSrcPduIds == NULL))
            || ((group->TxPathCount > 0U) && (group->TxSrcPduIds == NULL)))
        {
            DET_LOGE(TAG, "Init E: group[%u] member ids NULL", (unsigned)g);
            (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_INIT, PDUR_E_INIT_FAILED);
            return;
        }
    }

    /* [SWS_PDUR_00827]: ルーティングパスグループの初期状態は、PduRIsEnabledAtInit で決まる。 */
    for (uint8 g = 0U; g < ConfigPtr->RoutingPathGroupCount; g++)
    {
        PduR_GroupEnabled[g] = (ConfigPtr->RoutingPathGroups[g].IsEnabledAtInit != FALSE) ? 1U : 0U;
    }

    PduR_ConfigPtr = ConfigPtr;
    DET_LOGI(TAG, "Init ok RX=%u TX=%u",
             (unsigned)ConfigPtr->RxPathCount, (unsigned)ConfigPtr->TxPathCount);
}

/* ----------------------------------------------------------------------
 * PduR_GetVersionInfo
 * ---------------------------------------------------------------------- */

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
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_GET_VERSION_INFO, PDUR_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = PDUR_VENDOR_ID;
    versioninfo->moduleID         = PDUR_MODULE_ID;
    versioninfo->sw_major_version = PDUR_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = PDUR_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = PDUR_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * PduR_GetConfigurationId
 * ---------------------------------------------------------------------- */

/**
 * \brief   使用中のポストビルド構成の識別子を返す。
 *
 * \details 本プロジェクトは、ポストビルドで選択できる構成を 1 つしか持たない。
 *          識別子は、PduR_PBConfigType.PBConfigId の値（既定は 0）。
 *
 * \return  ポストビルド構成の識別子（[SWS_PduR_00280]）。未初期化のときは、
 *          PDUR_E_UNINIT を報告して 0 を返す。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_PduR_00341, SWS_PduR_00280}
 * \ServiceID      {0xF2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
PduR_PBConfigIdType PduR_GetConfigurationId(void)
{
    if (PduR_ConfigPtr == NULL)
    {
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_GET_CONFIGURATION_ID, PDUR_E_UNINIT);
        return 0U;
    }

    return PduR_ConfigPtr->PBConfigId;
}

/* ----------------------------------------------------------------------
 * PduR_EnableRouting
 * ---------------------------------------------------------------------- */

/**
 * \brief   ルーティングパスグループを有効にする。
 *
 * \details 有効にしたグループに所属するルーティングパスは、すぐに（[SWS_PduR_00715]）、
 *          送信要求・受信通知を処理する。既に有効なグループへの呼び出しは、何も変えない。
 *          グループが存在しないときは、何もせず、DET へ報告する（[SWS_PduR_00647]/[SWS_PduR_00648]）。
 *
 * \param[in]  id  ルーティングパスグループの識別子（PduR_PBConfigType.RoutingPathGroups[] の添字）。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_PduR_00615, SWS_PduR_00647, SWS_PduR_00648, SWS_PduR_00715}
 * \ServiceID      {0xF3}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_EnableRouting(PduR_RoutingPathGroupIdType id)
{
    if (PduR_ConfigPtr == NULL)
    {
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_ENABLE_ROUTING, PDUR_E_UNINIT);
        return;
    }

    if (id >= PduR_ConfigPtr->RoutingPathGroupCount)
    {
        DET_LOGE(TAG, "EnableRouting E: group=%u not found", (unsigned)id);
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_ENABLE_ROUTING, PDUR_E_ROUTING_PATH_GROUP_ID_INVALID);
        return;
    }

    PduR_GroupEnabled[id] = 1U;
    DET_LOGI(TAG, "EnableRouting group=%u", (unsigned)id);
}

/* ----------------------------------------------------------------------
 * PduR_DisableRouting
 * ---------------------------------------------------------------------- */

/**
 * \brief   ルーティングパスグループを無効にする。
 *
 * \details 無効にしたグループにだけ所属するルーティングパスは、PduR_ComTransmit()/PduR_SecOCTransmit() が E_NOT_OK を、
 *          PduR_ComRxIndication() が何もせずに戻る（[SWS_PDUR_00805]。DET へは報告しない。[SWS_PduR_00717]）。
 *          送信の確認（PduR_CanIfTxConfirmation()）は、無効の間も上位層へ伝える。仕様の [SWS_PDUR_00805] の
 *          一覧に無く、無効にする前に送った PDU の確認を失わないため。
 *          グループが存在しないときは、何もせず、DET へ報告する（[SWS_PduR_00716]/[SWS_PduR_00649]）。
 *
 *          簡略化: 仕様は、無効にするとき、単一バッファの既定値への戻し（initialize が TRUE のとき。
 *          [SWS_PDUR_00810]）と、FIFO の破棄（[SWS_PduR_00663]）を求める。本実装は、ゲートウェイ用の
 *          バッファも FIFO も持たない（PDU は受信のたびに、その場で上位層へ渡す）ので、戻す対象が無く、
 *          initialize は使わない。
 *
 * \param[in]  id          ルーティングパスグループの識別子（PduR_PBConfigType.RoutingPathGroups[] の添字）。
 * \param[in]  initialize  TRUE=単一バッファを既定値へ戻す。FALSE=現在の値を保つ。本実装では無効（上記）。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_PduR_00617, SWS_PduR_00716, SWS_PduR_00649, SWS_PDUR_00805}
 * \ServiceID      {0xF4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_DisableRouting(PduR_RoutingPathGroupIdType id, boolean initialize)
{
    (void)initialize;   /* バッファを持たないので、使わない（Doxygen の「簡略化」参照） */

    if (PduR_ConfigPtr == NULL)
    {
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_DISABLE_ROUTING, PDUR_E_UNINIT);
        return;
    }

    if (id >= PduR_ConfigPtr->RoutingPathGroupCount)
    {
        DET_LOGE(TAG, "DisableRouting E: group=%u not found", (unsigned)id);
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_DISABLE_ROUTING, PDUR_E_ROUTING_PATH_GROUP_ID_INVALID);
        return;
    }

    PduR_GroupEnabled[id] = 0U;
    DET_LOGI(TAG, "DisableRouting group=%u", (unsigned)id);
}

/* ======================================================================
 * Configurable interfaces definitions for interaction with upper layer module
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_ComTransmit
 * ---------------------------------------------------------------------- */

/**
 * \brief   Com からの PDU 送信要求を CanIf へ転送する（[SWS_PduR_00406]、
 *          generic な PduR_<User:Up>Transmit テンプレートの Com 向け実体化）。
 *
 * \details Com から PDU 送信を要求された際に呼び出される。
 *          SrcPduId に一致する TX ルーティングパスを検索し、
 *          CanIf_Transmit() へ転送する。
 *          一致するパスが存在しない場合は即座に E_NOT_OK を返す。
 *          CanTp 向けの実体化である `PduR_CanTpTransmit()` は本関数と
 *          挙動が完全に同一のため、本関数へそのまま委譲する。
 *
 * \param[in]  SrcPduId    送信する Com 側 PDU の ID。
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
Std_ReturnType PduR_ComTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL)
    {
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TRANSMIT, PDUR_E_UNINIT);
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL)
    {
        DET_LOGE(TAG, "TX E: PduInfoPtr NULL");
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TRANSMIT, PDUR_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const PduR_TxRoutingPathType* path = PduR_FindTxPath(SrcPduId);

    if (path == NULL)
    {
        DET_LOGW(TAG, "TX no route src=%u", (unsigned)SrcPduId);
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TRANSMIT, PDUR_E_PDU_ID_INVALID);
        return E_NOT_OK;
    }
    if (!PduR_IsRoutingPathEnabled(FALSE, SrcPduId))
    {
        /* [SWS_PDUR_00805]/[SWS_PduR_00717]: 無効なルーティングパスグループの送信要求は、DET へ報告せず、E_NOT_OK を返す。 */
        DET_LOGD(TAG, "TX src=%u routing disabled", (unsigned)SrcPduId);
        return E_NOT_OK;
    }

    if (path->TransmitOverrideFct != NULL)
    {
        DET_LOGD(TAG, "TX src=%u -> override id=%u",
                 (unsigned)SrcPduId, (unsigned)path->TransmitOverrideId);
        return path->TransmitOverrideFct(path->TransmitOverrideId, PduInfoPtr);
    }

    DET_LOGD(TAG, "TX src=%u canif=%u",
             (unsigned)SrcPduId, (unsigned)path->CanIfTxPduId);

    return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * PduR_CanTpTransmit
 * ---------------------------------------------------------------------- */

/**
 * \brief   CanTp からの PDU 送信要求を CanIf へ転送する（[SWS_PduR_00406]、
 *          generic な PduR_<User:Up>Transmit テンプレートの CanTp 向け実体化）。
 *
 * \details Com 向けの `PduR_ComTransmit()` と挙動が完全に同一（同じ
 *          TX ルーティングテーブルを同じロジックで検索するだけで、CanTp
 *          固有の特別扱いは無い）ため、そのまま委譲する
 *          （`PduR_SecOCTxConfirmation()` が `PduR_CanIfTxConfirmation()` へ
 *          委譲するのと同じ方針。`PduR_SecOCTransmit()` が独自に本体を持つのは
 *          TransmitOverrideFct を評価しないという実際の挙動差があるためで、
 *          今回はそのような差が無い）。
 *
 * \param[in]  SrcPduId    送信する CanTp 側 PDU の ID。
 *                         TX ルーティングパスの検索に使用する。
 * \param[in]  PduInfoPtr  送信するデータと長さへのポインタ。NULL 禁止。
 *
 * \retval  E_OK      PDU が CanIf_Transmit() に正常に転送された。
 * \retval  E_NOT_OK  PduR 未初期化、PduInfoPtr が NULL、
 *                    一致する TX ルーティングパスなし、
 *                    または CanIf_Transmit() が失敗した。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_PduR_00406}
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType PduR_CanTpTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    return PduR_ComTransmit(SrcPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * PduR_SecOCTransmit
 * ---------------------------------------------------------------------- */

/**
 * \brief   SecOC が変換済みの Secured I-PDU を送信する際に呼ぶ、
 *          PduR_ComTransmit()/PduR_CanTpTransmit() とは別の TX エントリポイント。
 *
 * \details PduR_ComTransmit()/PduR_CanTpTransmit() と異なり
 *          TransmitOverrideFct を一切評価せず、常に
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
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_SECOC_TRANSMIT, PDUR_E_UNINIT);
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL)
    {
        DET_LOGE(TAG, "TX(SecOC) E: PduInfoPtr NULL");
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_SECOC_TRANSMIT, PDUR_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const PduR_TxRoutingPathType* path = PduR_FindTxPath(SrcPduId);

    if (path == NULL)
    {
        DET_LOGW(TAG, "TX(SecOC) no route src=%u", (unsigned)SrcPduId);
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_SECOC_TRANSMIT, PDUR_E_PDU_ID_INVALID);
        return E_NOT_OK;
    }
    if (!PduR_IsRoutingPathEnabled(FALSE, SrcPduId))
    {
        /* PduR_ComTransmit() と同じ（SecOC が変換している間に、グループが無効になった場合）。 */
        DET_LOGD(TAG, "TX(SecOC) src=%u routing disabled", (unsigned)SrcPduId);
        return E_NOT_OK;
    }

    DET_LOGD(TAG, "TX(SecOC) src=%u canif=%u",
             (unsigned)SrcPduId, (unsigned)path->CanIfTxPduId);

    return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * PduR_<User:Up>Transmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:Up>CancelTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:Up>ChangeParameter (obsolete)
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:Up>CancelReceive
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Configurable interfaces definitions for lower layer communication interface module interaction
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_ComRxIndication
 * ---------------------------------------------------------------------- */

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
 *                         （PduR_ComTransmit が委譲する CanIf_Transmit と
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
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_RX_INDICATION, PDUR_E_UNINIT);
        return;
    }

    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL))
    {
        DET_LOGE(TAG, "RxInd E: PduInfoPtr NULL");
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_RX_INDICATION, PDUR_E_PARAM_POINTER);
        return;
    }

    for (uint8 i = 0; i < PduR_ConfigPtr->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &PduR_ConfigPtr->RxPaths[i];

        if (path->SrcPduId != RxPduId)
        {
            continue;
        }

        if (!PduR_IsRoutingPathEnabled(TRUE, RxPduId))
        {
            /* [SWS_PDUR_00805]/[SWS_PduR_00717]: 無効なルーティングパスグループの受信は、DET へ報告せず、捨てる。 */
            DET_LOGD(TAG, "RxInd src=%u routing disabled", (unsigned)RxPduId);
            return;
        }

        for (uint8 d = 0; d < path->DestCount; d++)
        {
            const PduR_RxDestType* dest = &path->Dests[d];

            DET_LOGD(TAG, "RxInd src=%u mod=%u dst=%u",
                     (unsigned)RxPduId, (unsigned)dest->Module,
                     (unsigned)dest->DestPduId);

            if (dest->RxIndFct != NULL)
            {
                dest->RxIndFct(dest->DestPduId, PduInfoPtr);
            }
        }
        return;
    }

    DET_LOGW(TAG, "RxInd no route src=%u", (unsigned)RxPduId);
    (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_RX_INDICATION, PDUR_E_PDU_ID_INVALID);
}

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>RxIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_CanIfTxConfirmation
 * ---------------------------------------------------------------------- */

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
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TX_CONFIRMATION, PDUR_E_UNINIT);
        return;
    }

    const PduR_TxRoutingPathType* path = PduR_FindTxPath(TxPduId);

    if (path == NULL)
    {
        DET_LOGW(TAG, "TxConf no route src=%u", (unsigned)TxPduId);
        (void)Det_ReportError(PDUR_MODULE_ID, 0U, PDUR_API_ID_TX_CONFIRMATION, PDUR_E_PDU_ID_INVALID);
        return;
    }

    DET_LOGD(TAG, "TxConf src=%u dst=%u",
             (unsigned)TxPduId, (unsigned)path->ConfDestPduId);

    if (path->ConfFct != NULL)
    {
        path->ConfFct(path->ConfDestPduId, result);
    }
}

/* ----------------------------------------------------------------------
 * PduR_SecOCTxConfirmation
 * ---------------------------------------------------------------------- */

/**
 * \brief   SecOC からの TX 送信完了通知を上位層（Com 等）へ転送する。
 *
 * \details generic な PduR_<Lo>TxConfirmation テンプレートの SecOC 向け
 *          実体化（PduR_SecOC.h 冒頭コメント参照）。PDUR_API_ID_SECOC_TX_
 *          CONFIRMATION が PDUR_API_ID_TX_CONFIRMATION のエイリアスである
 *          （PduR_Cfg.h 参照）通り、内部動作は CanIf 向けと完全に同一の
 *          ため、`PduR_CanIfTxConfirmation()` へそのまま委譲する。
 *
 * \param[in]  SrcPduId  SecOC_TxPduConfigType.PduRSrcPduId と一致する、
 *                       元の送信元 PDU の ID。TX ルーティングパスの検索に
 *                       使用する。
 * \param[in]  result    下位層からの送信結果。
 *                       E_OK = 成功、E_NOT_OK = 失敗。
 *                       上位層（COM 等）の TxConfirmation へそのまま転送する。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_PduR_00365}
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_SecOCTxConfirmation(PduIdType SrcPduId, Std_ReturnType result)
{
    PduR_CanIfTxConfirmation(SrcPduId, result);
}

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>TxConfirmation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>TriggerTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Configurable interfaces definitions for lower layer transport protocol module interaction
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_<User:LoTp>CopyRxData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:LoTp>RxIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:LoTp>StartOfReception
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:LoTp>CopyTxData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:LoTp>TxConfirmation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_FindTxPath
 * ---------------------------------------------------------------------- */

/**
 * \brief   SrcPduId に一致する TX ルーティングパスを検索する。
 *
 * \details PduR_CanIfTxConfirmation/PduR_ComTransmit/PduR_SecOCTransmit が
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
        {
            return &PduR_ConfigPtr->TxPaths[i];
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------------
 * PduR_IsRoutingPathEnabled
 * ---------------------------------------------------------------------- */

/**
 * \brief   ルーティングパスが、今、有効かを返す。
 *
 * \details どのルーティングパスグループにも所属しないパスは、常に有効。所属するパスは、所属するグループの
 *          いずれかが有効なら、有効。すべてのグループが無効なら、無効。
 *
 * \param[in]  isRx       TRUE=RX パス（RxPaths[]）、FALSE=TX パス（TxPaths[]）。
 * \param[in]  srcPduId   パスの SrcPduId。
 *
 * \return  TRUE=有効、FALSE=無効。
 *
 * \pre     PduR_ConfigPtr が NULL でないこと。
 */
static boolean PduR_IsRoutingPathEnabled(boolean isRx, PduIdType srcPduId)
{
    boolean member = FALSE;

    for (uint8 g = 0U; g < PduR_ConfigPtr->RoutingPathGroupCount; g++)
    {
        const PduR_RoutingPathGroupType* group = &PduR_ConfigPtr->RoutingPathGroups[g];
        const PduIdType* ids = (isRx != FALSE) ? group->RxSrcPduIds : group->TxSrcPduIds;
        const uint8 count    = (isRx != FALSE) ? group->RxPathCount : group->TxPathCount;

        for (uint8 i = 0U; i < count; i++)
        {
            if (ids[i] == srcPduId)
            {
                member = TRUE;
                if (PduR_GroupEnabled[g] != 0U)
                {
                    return TRUE;
                }
            }
        }
    }

    return (member == FALSE) ? TRUE : FALSE;
}

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef PDUR_UNIT_TEST
void PduR_Test_ResetInitState(void)
{
    PduR_ConfigPtr = NULL;
}
#endif

/**
 * \file    Fake_PduR_PBCfg.c
 * \brief   native_chain 用の PduR ポストビルド設定 `PduR_Config`（`src/Bsw/PduR/PduR_PBCfg.c` の代替）
 * \details 本番の PduR_PBCfg.c は、CanIf・Com・SecOC など多数のモジュールの PBCfg に依存するため、
 *          native_chain へはリンクしない。代わりに、本ファイルが `PduR_Config` を定義する。
 *
 *          - IF のルーティング（RX/TX パス）は持たない（IF の経路を使うテストは、各自の設定を PduR_Init() へ渡す）。
 *          - TP のルーティングは、本番の PduR.json の tpRxPaths/tpTxPaths と同じ内容を持つ（CanTp ↔ Dcm の診断の
 *            経路。PduR_CanTp*() が Dcm のコールバックへ、PduR_DcmTransmit() が CanTp_Transmit() へ、転送する）。
 *            Dcm と CanTp を実体で結ぶテストは、PduR_Init(&PduR_Config) を呼ぶこと。
 *
 *          EcuM_Init() が PduR_Init(&PduR_Config) を呼ぶためのシンボルも兼ねる（EcuM_Init() 自体は、テストから呼ばない）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "Dcm_Cbk.h"
#include "PduR_PBCfg.h"
#include "Fake_PduR_PBCfg.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

const PduR_TpRxRoutingPathType Fake_PduR_TpRxPaths[FAKE_PDUR_TP_RX_PATH_COUNT] = {
    {
        .SrcPduId            = CANTP_RX_SDU_ID,
        .DestPduId           = CANTP_DCM_RX_PDU_ID,
        .StartOfReceptionFct = Dcm_StartOfReception,
        .CopyRxDataFct       = Dcm_CopyRxData,
        .RxIndicationFct     = Dcm_TpRxIndication
    }
};

const PduR_TpTxRoutingPathType Fake_PduR_TpTxPaths[FAKE_PDUR_TP_TX_PATH_COUNT] = {
    {
        .SrcPduId          = CANTP_DCM_TX_PDU_ID,
        .DestPduId         = CANTP_TX_SDU_ID,
        .TransmitFct       = CanTp_Transmit,
        .CopyTxDataFct     = Dcm_CopyTxData,
        .TxConfirmationFct = Dcm_TpTxConfirmation
    }
};

const PduR_PBConfigType PduR_Config = {
    .TpRxPaths     = Fake_PduR_TpRxPaths,
    .TpRxPathCount = FAKE_PDUR_TP_RX_PATH_COUNT,
    .TpTxPaths     = Fake_PduR_TpTxPaths,
    .TpTxPathCount = FAKE_PDUR_TP_TX_PATH_COUNT
};

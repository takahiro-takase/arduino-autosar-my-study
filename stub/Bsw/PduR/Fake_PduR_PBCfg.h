/**
 * \file    Fake_PduR_PBCfg.h
 * \brief   native_chain 用の PduR の TP ルーティング（CanTp ↔ Dcm の診断の経路）の定義
 * \details `Fake_PduR_PBCfg.c` が定義する。`PduR_Config`（既定の設定）が持つ TP のルーティングを、
 *          テスト固有の PduR_PBConfigType（IF のルーティングを持つもの）へ持たせるために公開する。
 */
#ifndef FAKE_PDUR_PBCFG_H
#define FAKE_PDUR_PBCFG_H

#include "PduR_Types.h"

#define FAKE_PDUR_TP_RX_PATH_COUNT  1U
#define FAKE_PDUR_TP_TX_PATH_COUNT  1U

#ifdef __cplusplus
extern "C" {
#endif

extern const PduR_TpRxRoutingPathType Fake_PduR_TpRxPaths[FAKE_PDUR_TP_RX_PATH_COUNT];
extern const PduR_TpTxRoutingPathType Fake_PduR_TpTxPaths[FAKE_PDUR_TP_TX_PATH_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* FAKE_PDUR_PBCFG_H */

/**
 * \file    Dcm_Cbk.h
 * \brief   DCM コールバック 公開インタフェース (AUTOSAR SWS_DCM 簡易実装)
 * \details PduR から呼び出される DCM 受信コールバックを宣言する。
 *          AUTOSAR では Dcm_ComIndication が SWS_Dcm で規定されており、
 *          PduR が受信 PDU を DCM へルーティングする際に呼び出す。
 *          本実装は学習用の簡易スタブであり、受信内容を Det 経由でログ出力する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DCM_CBK_H
#define DCM_CBK_H

#include "ComStack_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   PduR から呼び出される DCM 受信インジケーションコールバック。
 *
 * \param[in]  RxPduId     受信 PDU の ID（PduR 名前空間）。
 * \param[in]  PduInfoPtr  受信データと長さへのポインタ。NULL 禁止。
 */
void Dcm_ComIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* DCM_CBK_H */

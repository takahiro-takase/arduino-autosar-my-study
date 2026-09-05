/**
 * \file    Dcm_Cbk.h
 * \brief   DCM コールバック 公開インタフェース (AUTOSAR SWS_DCM 簡易実装)
 * \details PduR から呼び出される DCM 受信コールバックを宣言する。
 *          `Dcm_ComIndication` という名前・シグネチャ自体は SWS_Dcm 本文には
 *          存在しない、本プロジェクト独自の関数である（2026-09-05 是正前は
 *          誤って「AUTOSAR 規定」と記載していた）。実仕様は CanTp からの
 *          受信を `Dcm_StartOfReception`/`Dcm_CopyRxData`/`Dcm_TpRxIndication`
 *          の3関数へ分割し、ペイロードを逐次コピーさせる設計だが、本プロジェクトの
 *          CanTp は組み立て済みバッファを `PduInfoType*` で丸ごと1回で渡すため、
 *          単純な改名では実仕様に合わせられない（アーキテクチャ自体が異なる）。
 *          本実装は組み立て済み UDS ペイロードを受け取り、SID ディスパッチまで
 *          含めて処理する。
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

/**
 * \file    SecOC.h
 * \brief   Secure Onboard Communication 公開インタフェース (AUTOSAR SWS_SecOC 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SECOC_H
#define SECOC_H

#include "SecOC_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   SecOC モジュールを初期化する。
 *
 * \details 設定ポインタを保存し、各 Secured I-PDU のフレッシュネス状態
 *          （最後に受理した Freshness Value）を初期化する。あわせて
 *          AES-128 の自己診断（SecOC_Aes128_SelfTest()、FIPS-197 既知
 *          テストベクタ）を実行し、結果をログ出力する。
 *
 * \param[in]  config  SecOC 設定構造体。NULL 禁止。
 */
void SecOC_Init(const SecOC_ConfigType* config);

/**
 * \brief   PduR から呼ばれる、Secured I-PDU 受信時の検証エントリポイント。
 *
 * \details `PduR_RxIndicationFctType` と同じシグネチャを持ち、PduR の
 *          `PduR_RxDestType.RxIndFct` へ直接登録できる（Com_RxIndication /
 *          CanTp_RxIndication と同じ立ち位置の PduR 宛先モジュール）。
 *
 *          Secured I-PDU を Authentic Payload / Freshness Value / 切り詰め
 *          MAC に分離し、AES-128-CMAC を自前実装で再計算して MAC が一致するか
 *          検証する（[SWS_SecOC_00192] SecOC Profile 1）。続けてフレッシュネス
 *          が単調増加しているか（リプレイでないか）を確認する。両方成功した
 *          場合のみ、Authentic Payload を `Com_RxIndication()` へ転送する。
 *          いずれかに失敗した場合はログ出力のみ行い、Com へは一切転送しない
 *          （検証されていないデータを上位層へ渡さないことが本モジュールの
 *          存在意義そのものであるため）。
 *
 * \param[in]  RxPduId     検証対象の SecOC RX Secured I-PDU ID
 *                         （SecOC_RxPduConfigType.SecOCRxPduId と照合する）。
 * \param[in]  PduInfoPtr  受信した Secured I-PDU のデータと長さ。NULL 禁止。
 */
void SecOC_IfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* SECOC_H */

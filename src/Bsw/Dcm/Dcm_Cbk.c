/**
 * \file    Dcm_Cbk.c
 * \brief   DCM コールバック実装 (AUTOSAR SWS_DCM 簡易実装)
 * \details PduR から受信 PDU が配信されたとき、Det 経由でログ出力する
 *          学習用スタブ実装。完全な AUTOSAR DCM 実装では UDS サービス処理
 *          (0x10 DiagnosticSessionControl, 0x22 ReadDataByIdentifier 等) が
 *          ここで行われる。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Dcm_Cbk.h"
#include "Det.h"

/**
 * \brief   PduR から呼び出される DCM 受信インジケーションコールバック。
 *
 * \details 受信 PDU ID と先頭バイトを Det 経由でログ出力する。
 *          完全な DCM 実装では UDS サービスのデコードと応答生成を行う。
 *
 * \param[in]  RxPduId     受信 PDU の ID（PduR 名前空間）。
 * \param[in]  PduInfoPtr  受信データと長さへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_ComIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    Det_PrintP(PSTR("[Dcm_ComInd] RxPduId="));
    Det_PrintDec(RxPduId);
    Det_PrintP(PSTR(" first_byte=0x"));
    Det_PrintHex(PduInfoPtr->SduDataPtr[0]);
    Det_Newline();
}

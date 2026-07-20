/**
 * \file    SecOC_Types.h
 * \brief   SecOC 型定義 (AUTOSAR SWS_SecureOnboardCommunication 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SECOC_TYPES_H
#define SECOC_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "SecOC_Aes128.h"

/* -----------------------------------------------------------------------
 * RX Secured I-PDU 設定（1 エントリ = 1 つの Secured I-PDU）
 *
 *   DataId               : SecOCDataId（ECUC_SecOC_00043 相当）。Authenticator
 *                           計算対象データの先頭に連結する識別子
 *                           （[7.1.1.2] "DataToAuthenticator = Data Identifier |
 *                           secured part of the Authentic I-PDU | Complete
 *                           Freshness Value"）。Big Endian で連結する
 *                           （[SWS_SecOC_00011]）。
 *   AuthenticPduLength    : Authentic I-PDU（保護対象の元データ）のバイト長。
 *                           Secured I-PDU の先頭からこのバイト数が Authentic
 *                           Payload であり、そのまま Com へ転送される。
 *   FreshnessOffset/Length: Secured I-PDU 内での Freshness Value の位置・長さ。
 *                           本実装は SecOCFreshnessValueLength=8bit を採用し、
 *                           送信される 8bit がそのまま Complete Freshness Value
 *                           となる（Profile 1 の SecOCFreshnessValueTxLength=8bit
 *                           と一致させ、実車のような「送信されない上位ビットの
 *                           推定復元」を不要にする簡略化。詳細は README 参照）。
 *   MacOffset/TxLength    : Secured I-PDU 内での切り詰め MAC の位置・長さ。
 *                           SecOC Profile 1（[SWS_SecOC_00192]）に倣い
 *                           TxLength=3byte（24bit、AES-CMAC 128bit 出力の
 *                           上位ビット）を使用する。
 *   SecuredPduLength      : Secured I-PDU 全体の期待バイト長
 *                           （= AuthenticPduLength + FreshnessLength + MacTxLength）。
 *   Key                   : 16 バイト AES-128 鍵へのポインタ（SecOC_PBCfg.c の
 *                           固定配列を指す。実車は KeyM 等による鍵管理が必要だが、
 *                           本実装は学習用に固定鍵で簡略化する）。
 *   ComRxPduId            : 検証成功時に Com_RxIndication() へ渡す Com RX I-PDU ID。
 * ----------------------------------------------------------------------- */
typedef struct
{
    PduIdType    SecOCRxPduId;  /* PduR_RxDestType.DestPduId と一致させる検索キー
                                 * （Com の IPduId 検索と同じく、配列添字に暗黙依存
                                 * せず明示フィールドで検索する） */
    uint16       DataId;
    uint8        AuthenticPduLength;
    uint8        FreshnessOffset;
    uint8        FreshnessLength;
    uint8        MacOffset;
    uint8        MacTxLength;
    uint8        SecuredPduLength;
    const uint8* Key;
    PduIdType    ComRxPduId;
} SecOC_RxPduConfigType;

/* -----------------------------------------------------------------------
 * TX Secured I-PDU 設定（1 エントリ = 1 つの Secured I-PDU）
 *
 *   PduR の TX 経路上（PduR_TxRoutingPathType.TransmitOverrideFct）に
 *   挟まる中間モジュールとして動作する（[7.4.1] "Authentication during
 *   direct transmission" の ad-hoc transmission フロー相当）。
 *   Com が PduR_Transmit() を呼ぶと SecOC_IfTransmit() が Authentic I-PDU を
 *   内部バッファへコピーして即座に E_OK を返し（[SWS_SecOC_00058]）、
 *   実際の Freshness/MAC 計算と Secured I-PDU の組み立ては次回
 *   SecOC_MainFunction() で行う（[SWS_SecOC_00060]〜[SWS_SecOC_00062]）。
 *   計算完了後、SecOC 自身が PduR_SecOCTransmit() を呼んで CanIf まで
 *   到達させる。
 *
 *   DataId/AuthenticPduLength/FreshnessOffset/FreshnessLength/MacOffset/
 *   MacTxLength/SecuredPduLength/Key の意味は SecOC_RxPduConfigType と同じ
 *   （対称の TX 版）。
 *   PduRSrcPduId : SecOC_MainFunction() が変換完了後に PduR_SecOCTransmit()
 *                  へ渡す、元の Authentic I-PDU の TX ルーティングパス ID
 *                  （PduR_TxRoutingPathType.SrcPduId と一致させる）。
 * ----------------------------------------------------------------------- */
typedef struct
{
    PduIdType    SecOCTxPduId;  /* PduR_TxRoutingPathType.TransmitOverrideId と一致させる検索キー */
    uint16       DataId;
    uint8        AuthenticPduLength;
    uint8        FreshnessOffset;
    uint8        FreshnessLength;
    uint8        MacOffset;
    uint8        MacTxLength;
    uint8        SecuredPduLength;
    const uint8* Key;
    PduIdType    PduRSrcPduId;
} SecOC_TxPduConfigType;

typedef struct
{
    const SecOC_RxPduConfigType* RxPdus;
    uint8                        RxPduCount;
    const SecOC_TxPduConfigType* TxPdus;
    uint8                        TxPduCount;
} SecOC_ConfigType;

#endif /* SECOC_TYPES_H */

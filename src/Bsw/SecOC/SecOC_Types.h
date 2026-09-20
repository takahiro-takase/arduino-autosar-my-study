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
 *   CsmJobId              : Csm_MacVerify() に渡すジョブ ID（CSM_JOB_ID_* 定数）。
 *                           SecOC は鍵そのものへは一切アクセスしない
 *                           （どの鍵を使うかは Csm/Crypto レイヤの設定が決める。
 *                           Csm/CryIf/Crypto レイヤ分離の詳細は SecOC.c 参照）。
 *   ComRxPduId            : 検証成功時に Com_RxIndication() へ渡す Com RX I-PDU ID。
 * ----------------------------------------------------------------------- */
/**
 * \brief   検証結果の種別（[SWS_SecOC_00149]）。
 * \details `SECOC_AUTHENTICATIONBUILDFAILURE` は「Freshness 値の問い合わせ
 *          自体が失敗した」場合用だが、本プロジェクトの Freshness は受信
 *          バイト列から同期的に導出するだけで問い合わせ処理を持たないため
 *          （SecOC_Types.h 内 FreshnessOffset/Length コメント参照）、この値が
 *          実際に使われることはない（値の定義のみ）。
 */
typedef enum
{
    SECOC_VERIFICATIONSUCCESS        = 0x00U, /**< 検証成功 */
    SECOC_VERIFICATIONFAILURE        = 0x01U, /**< MAC不一致、または下位層(Csm等)自体の失敗 */
    SECOC_FRESHNESSFAILURE           = 0x02U, /**< MACは一致したがFreshness(リプレイ)検証で不合格 */
    SECOC_AUTHENTICATIONBUILDFAILURE = 0x03U  /**< 本実装では未到達（上記説明参照） */
} SecOC_VerificationResultType;

/**
 * \brief   `SecOC_VerificationStatusCallout()` へ渡す通知データ構造体
 *          （[SWS_SecOC_00160]）。
 * \details `freshnessValueID` は実仕様が持つ専用の Freshness Value ID 空間
 *          ではなく、`SecOC_VerifyStatusOverride()` と同じ学習用簡略化により
 *          対象の `SecOCRxPduId` をそのまま使う（同関数の Doxygen 参照）。
 */
typedef struct
{
    uint16                        freshnessValueID;
    SecOC_VerificationResultType  verificationStatus;
    uint16                        secOCDataId;
} SecOC_VerificationStatusType;

/**
 * \brief   検証ステータス伝播モード（[ECUC_SecOC_00046]
 *          SecOCVerificationStatusPropagationMode 相当）。RX Secured I-PDU
 *          ごとに設定する。
 */
typedef enum
{
    SECOC_VERIFICATION_STATUS_PROPAGATION_NONE = 0U,          /**< 通知しない（既定） */
    SECOC_VERIFICATION_STATUS_PROPAGATION_FAILURE_ONLY,       /**< 失敗時のみ通知 */
    SECOC_VERIFICATION_STATUS_PROPAGATION_BOTH                /**< 成功・失敗とも通知 */
} SecOC_VerificationStatusPropagationModeType;

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
    uint32       CsmJobId;
    PduIdType    ComRxPduId;
    /** [SWS_SecOC_00048]/[SWS_SecOC_00119]: 非NULLなら検証の都度、
     *  VerificationStatusPropagationMode に従って呼ぶ（[SWS_SecOC_00048]、
     *  詳細は SecOC.c の SecOC_RxIndication() 参照）。NULL 可（通知不要なら
     *  未設定でよい、他の Cbk フックと同じ規約）。 */
    void (*VerificationStatusCallout)(SecOC_VerificationStatusType status);
    SecOC_VerificationStatusPropagationModeType VerificationStatusPropagationMode;
} SecOC_RxPduConfigType;

/* -----------------------------------------------------------------------
 * TX Secured I-PDU 設定（1 エントリ = 1 つの Secured I-PDU）
 *
 *   PduR の TX 経路上（PduR_TxRoutingPathType.TransmitOverrideFct）に
 *   挟まる中間モジュールとして動作する（[7.4.1] "Authentication during
 *   direct transmission" の ad-hoc transmission フロー相当）。
 *   Com が PduR_ComTransmit() を呼ぶと SecOC_IfTransmit() が Authentic I-PDU を
 *   内部バッファへコピーして即座に E_OK を返し（[SWS_SecOC_00058]）、
 *   実際の Freshness/MAC 計算と Secured I-PDU の組み立ては次回
 *   SecOC_MainFunctionTx() で行う（[SWS_SecOC_00060]〜[SWS_SecOC_00062]）。
 *   計算完了後、SecOC 自身が PduR_SecOCTransmit() を呼んで CanIf まで
 *   到達させる。
 *
 *   DataId/AuthenticPduLength/FreshnessOffset/FreshnessLength/MacOffset/
 *   MacTxLength/SecuredPduLength/CsmJobId の意味は SecOC_RxPduConfigType と
 *   同じ（対称の TX 版。ただし CsmJobId は Csm_MacGenerate() 用のジョブを指す）。
 *   PduRSrcPduId : SecOC_MainFunctionTx() が変換完了後に PduR_SecOCTransmit()
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
    uint32       CsmJobId;
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

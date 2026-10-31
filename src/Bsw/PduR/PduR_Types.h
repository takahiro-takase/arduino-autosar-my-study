/**
 * \file    PduR_Types.h
 * \brief   PDU ルータ型定義 (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_TYPES_H
#define PDUR_TYPES_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

typedef enum
{
    PDUR_MODULE_COM   = 0,
    PDUR_MODULE_CANTP = 1,
    PDUR_MODULE_DCM   = 2,
    PDUR_MODULE_SECOC = 3,
} PduR_DestModuleType;

typedef void (*PduR_RxIndicationFctType)(PduIdType DestPduId, const PduInfoType* PduInfoPtr);

typedef void (*PduR_TxConfirmationFctType)(PduIdType DestPduId, Std_ReturnType result);

/* SecOC_IfTransmit() 等、TX 経路上の中間モジュールへの委譲に使う関数ポインタ型。
 * PduR_ComTransmit()/PduR_CanTpTransmit() 自身と同じシグネチャ
 * （Std_ReturnType(*)(PduIdType, const PduInfoType*)）を持つため、実 AUTOSAR の
 * SecOC_IfTransmit() のような「PduR_<Up>Transmit と同じ形の下位層エントリ
 * ポイント」をそのまま登録できる。 */
typedef Std_ReturnType (*PduR_TxTransmitFctType)(PduIdType DestPduId, const PduInfoType* PduInfoPtr);

typedef struct
{
    PduR_DestModuleType      Module;
    PduIdType                DestPduId;
    PduR_RxIndicationFctType RxIndFct;
} PduR_RxDestType;

typedef struct
{
    PduIdType               SrcPduId;
    const PduR_RxDestType*  Dests;
    uint8                   DestCount;
} PduR_RxRoutingPathType;

typedef struct
{
    PduIdType                 SrcPduId;
    PduIdType                 CanIfTxPduId;
    PduIdType                 ConfDestPduId;
    PduR_TxConfirmationFctType ConfFct;
    /* TX 経路上に中間モジュール（SecOC 等）を挟む場合のみ使用。NULL（既定）なら
     * PduR_ComTransmit()/PduR_CanTpTransmit() は従来どおり CanIf_Transmit() へ
     * 直接転送する（既存の全 TX パスはこのフィールドを設定しないため無変更）。
     * 非 NULL なら PduR_ComTransmit()/PduR_CanTpTransmit() は CanIf_Transmit()
     * の代わりにこの関数を TransmitOverrideId 付きで呼び、中間モジュールに変換
     * を委ねる。中間モジュールは変換完了後、PduR_SecOCTransmit()
     * （TransmitOverrideFct を評価しない専用エントリポイント）を呼んで
     * CanIf_Transmit() まで到達させる（[SWS_SecOC_00058]〜[SWS_SecOC_00062]、
     * 7.4.1 節の "ad-hoc transmission" フロー相当）。 */
    PduR_TxTransmitFctType    TransmitOverrideFct;
    PduIdType                 TransmitOverrideId;
} PduR_TxRoutingPathType;

/* SWS_PduR_00771: PduR_GetConfigurationId() が返す、ポストビルド構成の識別子 */
typedef uint16 PduR_PBConfigIdType;

/* ルーティングパスグループの識別子（PduR_PBConfigType.RoutingPathGroups[] の添字）。
 * PduR_EnableRouting()/PduR_DisableRouting() の引数（SWS_PduR_00615/00617）。 */
typedef uint16 PduR_RoutingPathGroupIdType;

/* ルーティングパスグループ 1 つ分の設定（PduRRoutingPathGroup）。
 * 所属するルーティングパスは、送信元 PDU の ID（RX は RxPaths[].SrcPduId、TX は TxPaths[].SrcPduId）で参照する。
 * どのグループにも所属しないルーティングパスは、常に有効である。
 * 1 つのパスが複数のグループに所属するときは、いずれかのグループが有効なら、有効である。 */
typedef struct
{
    boolean          IsEnabledAtInit;   /* PduRIsEnabledAtInit: PduR_Init() 直後の状態（TRUE=有効） */
    const PduIdType* RxSrcPduIds;       /* 所属する RX パスの SrcPduId の配列。所属が無ければ NULL */
    uint8            RxPathCount;
    const PduIdType* TxSrcPduIds;       /* 所属する TX パスの SrcPduId の配列。所属が無ければ NULL */
    uint8            TxPathCount;
} PduR_RoutingPathGroupType;

/* SWS_PduR_00241: post-build configuration type (PduR_PBConfigType)
 * 末尾の 3 項目は省略できる（省略すると 0 / NULL。グループなし、構成 ID 0）。 */
typedef struct
{
    const PduR_RxRoutingPathType* RxPaths;
    uint8                         RxPathCount;
    const PduR_TxRoutingPathType* TxPaths;
    uint8                         TxPathCount;
    PduR_PBConfigIdType               PBConfigId;            /* PduR_GetConfigurationId() が返す値（SWS_PduR_00280） */
    const PduR_RoutingPathGroupType*  RoutingPathGroups;     /* ルーティングパスグループの配列。無ければ NULL */
    uint8                             RoutingPathGroupCount; /* PDUR_ROUTING_PATH_GROUP_MAX 以下 */
} PduR_PBConfigType;

#endif

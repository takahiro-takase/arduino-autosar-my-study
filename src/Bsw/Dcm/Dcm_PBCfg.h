/**
 * \file    Dcm_PBCfg.h
 * \brief   DCM ポストビルドコンフィグ 型定義・外部宣言
 * \details DID の表（Dcm_DidTable）の型と外部宣言、DID のデータ関数の宣言を提供する。
 *          表の定義は Dcm_PBCfg.c にあり、config/data/Dcm.json から生成する
 *          （AUTOSAR の DcmDspData / DcmDspDid に相当）。データ関数の中身は Dcm.c に手書きする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DCM_PBCFG_H
#define DCM_PBCFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "Dcm_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** 0x2F InputOutputControlByIdentifier に対応しない DID の ioControlLamp の値 */
#define DCM_DID_IOCONTROL_NONE  0xFFU

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/** 0x22 のデータ読み出し関数。Data[0 .. Length-1] を埋める。 */
typedef Std_ReturnType (*Dcm_DidReadFctType)(uint8* Data);

/** 0x2E のデータ書き込み関数。Data[0 .. Length-1] を受け取る（長さは呼び出し元が検証済み）。 */
typedef Std_ReturnType (*Dcm_DidWriteFctType)(const uint8* Data);

/** SID ごとに許可されるセッションを表す 1 行 */
typedef struct
{
    uint8 Sid;
    uint8 AllowedSessionMask;  /**< DCM_SESSION_MASK_* の組み合わせ */
} Dcm_SidSessionRowType;

/** DID の表の 1 行 */
typedef struct
{
    uint16              Did;           /**< データ識別子 */
    uint8               Length;        /**< データ長 [byte] */
    Dcm_DidReadFctType  ReadFct;       /**< 0x22 の読み出し関数。NULL なら 0x22 非対応 */
    Dcm_DidWriteFctType WriteFct;      /**< 0x2E の書き込み関数。NULL なら 0x2E 非対応 */
    uint8               IoControlLamp; /**< 0x2F で制御するランプ (Rte_LampIdType)。DCM_DID_IOCONTROL_NONE なら 0x2F 非対応 */
    uint8               ReadSessionMask;     /**< 0x22 で読めるセッション (DCM_SESSION_MASK_* の組み合わせ。DcmDspDidReadSessionRef) */
    uint8               ReadSecurityRequired; /**< 1 なら 0x22 に SecurityAccess Level1 のアンロックが要る (DcmDspDidReadSecurityLevelRef) */
} Dcm_DidConfigType;

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/** DID の表（Dcm_PBCfg.c で定義） */
extern const Dcm_DidConfigType Dcm_DidTable[DCM_DID_COUNT];

/** SID × セッション許可の表（Dcm_PBCfg.c で定義） */
extern const Dcm_SidSessionRowType Dcm_SidSessionTable[DCM_SID_SESSION_COUNT];

/* ======================================================================
 * Functions
 * ====================================================================== */

/* @@GEN-BEGIN dcm-did-functions  生成元: config/data/Dcm.json  （自動生成: 手編集禁止） */
/* DID のデータ関数（Dcm.c に手書きする。名前は config/data/Dcm.json の readFct / writeFct） */
Std_ReturnType Dcm_ReadDidEngineSpeed(uint8* Data);
Std_ReturnType Dcm_ReadDidCoolantTemp(uint8* Data);
Std_ReturnType Dcm_ReadDidEngineState(uint8* Data);
Std_ReturnType Dcm_ReadDidTestPattern(uint8* Data);
Std_ReturnType Dcm_WriteDidTestPattern(const uint8* Data);
Std_ReturnType Dcm_WriteDidCryptoKeyUpdate(const uint8* Data);
Std_ReturnType Dcm_ReadDidSecureInfo(uint8* Data);
Std_ReturnType Dcm_ReadDidVin(uint8* Data);
/* @@GEN-END dcm-did-functions */

#endif /* DCM_PBCFG_H */

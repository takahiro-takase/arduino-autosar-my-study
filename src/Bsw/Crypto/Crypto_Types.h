/**
 * \file    Crypto_Types.h
 * \brief   Crypto スタック共通型定義 (AUTOSAR SWS_CryptoServiceManager/
 *          SWS_CryptoInterface/SWS_CryptoDriver 準拠)
 * \details Csm（Crypto Service Manager）・CryIf（Crypto Interface）・
 *          Crypto（Crypto Driver）の 3 層が共通で使う型を、Com スタックの
 *          `ComStack_Types.h` と同じ位置付けでここにまとめる。
 *
 *          `Crypto_JobType` は実 AUTOSAR では
 *          `Crypto_JobPrimitiveInputOutputType`/`Crypto_JobPrimitiveInfoType`/
 *          `Crypto_PrimitiveInfoType`/`Crypto_JobInfoType` という 4 段階の
 *          入れ子構造体（ARXML 設定生成を前提にした汎用ジョブ記述）だが、
 *          本プロジェクトは MAC 生成 (CRYPTO_MACGENERATE) と MAC 検証
 *          (CRYPTO_MACVERIFY) の 2 用途しか使わないため、実際に必要な
 *          フィールドだけを持つフラット構造体に簡略化している
 *          （`BswM_RuleType` が実 AUTOSAR の ActionList/LogicalExpression の
 *          汎用性を単一条件ルールへ削ったのと同じ簡略化方針）。
 *          Hash/Encrypt/Decrypt/Sign 等、他のプリミティブを追加する場合は
 *          このフラット構造体に新たなフィールドが必要になる点に注意。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CRYPTO_TYPES_H
#define CRYPTO_TYPES_H

#include "Platform_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   ジョブが実行するプリミティブ種別 (AUTOSAR Crypto_ServiceInfoType の抜粋)
 * \details 値は AUTOSAR_SWS_CryptoServiceManager.pdf 8.7.2 章（Crypto_ServiceInfoType
 *          Range 表）を実測して確認済み。本プロジェクトが使う 2 値のみ定義する。
 */
typedef enum
{
    CRYPTO_MACGENERATE = 0x01U,  /**< MAC 生成サービス */
    CRYPTO_MACVERIFY   = 0x02U   /**< MAC 検証サービス */
} Crypto_ServiceInfoType;

/**
 * \brief   ジョブの操作モード (AUTOSAR Crypto_OperationModeType の抜粋)
 * \details 値は AUTOSAR_SWS_CryptoServiceManager.pdf 8.7.2.3 章を実測して確認済み。
 *          実 AUTOSAR は START/UPDATE/STREAMSTART/FINISH の組み合わせで
 *          ストリーミング処理を表現できるが、本プロジェクトは常に
 *          CRYPTO_OPERATIONMODE_SINGLECALL（Start+Update+Finish 相当の
 *          一括呼び出し）のみを使う同期実装のため、他の値は定義しない。
 */
typedef enum
{
    CRYPTO_OPERATIONMODE_SINGLECALL = 0x07U
} Crypto_OperationModeType;

/**
 * \brief   MAC 検証結果 (AUTOSAR Crypto_VerifyResultType)
 * \details 値は AUTOSAR_SWS_CryptoServiceManager.pdf 8.7.2.6 章を実測して確認済み。
 */
typedef enum
{
    CRYPTO_E_VER_OK     = 0x00U,  /**< 検証対象と再計算値が一致した */
    CRYPTO_E_VER_NOT_OK = 0x01U   /**< 検証対象と再計算値が不一致だった */
} Crypto_VerifyResultType;

/**
 * \brief   Crypto ジョブ記述（簡略化フラット版、本ファイル冒頭コメント参照）
 *
 * \details Csm_MacGenerate()/Csm_MacVerify() が本構造体を組み立て、
 *          CryIf_ProcessJob() → Crypto_ProcessJob() へそのまま渡す
 *          （CryIf は本プロジェクトでは Crypto Driver が 1 個のみのため
 *          実質パススルーで、内容を書き換えない）。
 *
 *          macLength の単位はバイトで統一する。実 AUTOSAR は
 *          Csm_MacGenerate() の macLengthPtr がバイト単位、Csm_MacVerify()
 *          の macLength がビット単位という非対称な仕様になっているが
 *          （[SWS_Csm_00982]/[SWS_Csm_01050] を実測して確認済み）、
 *          Csm.c が Csm_MacVerify() 呼び出し時にビット→バイト変換を行った
 *          上でこの構造体へ積むため、Job 以降（CryIf/Crypto）は常にバイト
 *          単位として扱えばよい。
 */
typedef struct
{
    uint32                   jobId;           /**< Csm のジョブ ID（Csm_JobConfigType の検索キー） */
    Crypto_ServiceInfoType   service;         /**< 実行するプリミティブ種別 */
    Crypto_OperationModeType mode;            /**< 本プロジェクトは常に SINGLECALL */
    uint32                   cryptoKeyId;     /**< Crypto Driver 側の鍵テーブル添字 */
    const uint8*             inputPtr;        /**< MAC 計算対象データ */
    uint32                   inputLength;     /**< inputPtr のバイト長 */
    uint8*                   macPtr;          /**< MACGENERATE: 出力先 / MACVERIFY: 検証対象(受信側)MAC */
    uint32                   macLength;       /**< バイト単位。MACGENERATE は要求する切り詰め長(in)、
                                                *   MACVERIFY は比較するバイト数(in) */
    Crypto_VerifyResultType* verifyResultPtr; /**< MACVERIFY のときのみ使用。結果格納先 */
} Crypto_JobType;

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_TYPES_H */

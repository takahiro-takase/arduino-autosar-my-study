/**
 * \file    Csm_PBCfg.h
 * \brief   Crypto Service Manager ポストビルド設定 型・宣言
 * \details CsmJob 設定テーブルの型と外部参照を宣言する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する（DaVinci: Csm/CsmJobs/CsmJob コンテナ相当）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CSM_PBCFG_H
#define CSM_PBCFG_H

#include "Crypto_Types.h"
#include "Csm_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   CsmJob 1 件分の設定（DaVinci CsmJob コンテナの簡略版）。
 *
 * \details 実 AUTOSAR の CsmJob は CsmJobPrimitiveRef 経由でアルゴリズム・
 *          鍵参照・コールバック等を多段に参照するが、本プロジェクトは
 *          MACGENERATE/MACVERIFY の 2 種類しか使わないため、
 *          「どのプリミティブか」「どの鍵か」だけを持つフラット構造体に
 *          簡略化する（Crypto_Types.h 冒頭コメントと同じ方針）。
 */
typedef struct
{
    uint32                 JobId;       /**< CSM_JOB_ID_* 定数 */
    Crypto_ServiceInfoType Service;     /**< このジョブが実行するプリミティブ */
    uint32                 CryptoKeyId; /**< Crypto Driver 側の鍵テーブル添字 (CRYPTO_KEY_*) */
} Csm_JobConfigType;

/** ジョブ設定テーブル（Csm_PBCfg.c で定義）。CSM_JOB_COUNT 件。 */
extern const Csm_JobConfigType Csm_JobConfigData[CSM_JOB_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* CSM_PBCFG_H */

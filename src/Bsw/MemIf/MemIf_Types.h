/**
 * \file    MemIf_Types.h
 * \brief   Memory Abstraction Interface 共通型定義 (AUTOSAR SWS_MemIf 準拠)
 * \details Fee/MemIf/NvM が共通で使う型を 1 か所にまとめる
 *          (Com_Types.h / Crypto_Types.h と同じ「複数モジュールが参照する
 *          共通型は専用ヘッダに分離する」プロジェクトの慣例)。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MEMIF_TYPES_H
#define MEMIF_TYPES_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   下位ドライバ (Fee) の現在のビジー状態。
 * \details AUTOSAR MemIf_StatusType (SWS_MemIf) の一部に相当する。
 *          本実装で使用するのは以下の 3 値のみ（学習用簡略化。
 *          複数ジョブの内部キューイングを表す MEMIF_BUSY_INTERNAL は
 *          本プロジェクトが常に「同時に 1 ジョブのみ」しか扱わないため
 *          未使用）。
 */
typedef enum
{
    MEMIF_UNINIT = 0U,  /**< Fee_Init() 未実行                       */
    MEMIF_IDLE,         /**< 進行中のジョブなし。次のジョブを受付可能 */
    MEMIF_BUSY          /**< 非同期ジョブ (Write) 処理中              */
} MemIf_StatusType;

/**
 * \brief   直近のジョブの結果。
 * \details AUTOSAR MemIf_JobResultType (SWS_MemIf) の一部に相当する。
 *          本実装で使用するのは以下の 4 値のみ（学習用簡略化。
 *          MEMIF_BLOCK_INCONSISTENT/MEMIF_BLOCK_INVALID は Fee 自身の
 *          仮想ページ管理・ガベージコレクションに関わる状態のため、
 *          本実装はそれらを持たず対象外）。
 */
typedef enum
{
    MEMIF_JOB_OK = 0U,     /**< ジョブが正常完了した                       */
    MEMIF_JOB_FAILED,      /**< ジョブが失敗した（本実装では未使用の予約値） */
    MEMIF_JOB_PENDING,     /**< ジョブがまだ完了していない                 */
    MEMIF_JOB_CANCELED     /**< MemIf_Cancel()/Fee_Cancel() で中断された */
} MemIf_JobResultType;

/**
 * \brief   MemIf が振り分ける下位ドライバの識別子。
 * \details 実 AUTOSAR は複数の Fee/Ea インスタンスを同時に構成でき、
 *          MemIf はこの Device 引数でどのインスタンスへ振り分けるかを
 *          決める。本プロジェクトは対応 MCU が Renesas RA のみのため
 *          常にちょうど 1 個（Fee）しか存在せず、有効な値は
 *          MEMIF_DEVICE_0 のみ（詳細は MemIf.c 冒頭のコメント参照）。
 */
typedef uint8 MemIf_DeviceType;

#define MEMIF_DEVICE_0  0U

#ifdef __cplusplus
}
#endif

#endif /* MEMIF_TYPES_H */

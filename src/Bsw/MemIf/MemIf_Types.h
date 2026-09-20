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
 * \details [SWS_MemIf_00064] 準拠の4値全てを定義する。本プロジェクトの
 *          `MemIf_GetStatus()`（MemIf.c）は Fee の状態をそのまま返す
 *          薄い委譲のみで、Fee 自体は「同時に1ジョブのみ」しか扱わず
 *          複数ジョブの内部キューイングという概念を持たないため、
 *          `MEMIF_BUSY_INTERNAL` が実際に返ることはない（2026-09-20
 *          追加、値の定義のみで到達経路は無い）。
 */
typedef enum
{
    MEMIF_UNINIT = 0U,   /**< Fee_Init() 未実行                       */
    MEMIF_IDLE,          /**< 進行中のジョブなし。次のジョブを受付可能 */
    MEMIF_BUSY,          /**< 非同期ジョブ (Write) 処理中              */
    MEMIF_BUSY_INTERNAL  /**< 内部管理処理でビジー（本実装では未到達） */
} MemIf_StatusType;

/**
 * \brief   直近のジョブの結果。
 * \details [SWS_MemIf_00065] 準拠の6値全てを定義する。
 *          `MEMIF_BLOCK_INCONSISTENT`/`MEMIF_BLOCK_INVALID` は Fee 自身の
 *          仮想ページ管理・ガベージコレクションに関わる状態だが、本実装は
 *          それらの機構を持たないため実際に返ることはない（2026-09-20
 *          追加、値の定義のみで到達経路は無い）。
 */
typedef enum
{
    MEMIF_JOB_OK = 0U,        /**< ジョブが正常完了した                       */
    MEMIF_JOB_FAILED,         /**< ジョブが失敗した（本実装では未使用の予約値） */
    MEMIF_JOB_PENDING,        /**< ジョブがまだ完了していない                 */
    MEMIF_JOB_CANCELED,       /**< MemIf_Cancel()/Fee_Cancel() で中断された */
    MEMIF_BLOCK_INCONSISTENT, /**< ブロックが不整合（本実装では未到達）        */
    MEMIF_BLOCK_INVALID       /**< ブロックが無効化済み（本実装では未到達）    */
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

/**
 * \brief   下位ドライバの動作モード（[SWS_MemIf_00066]）。
 * \details 実 AUTOSAR は「フラッシュ書き込み電圧・タイミングを切り替えられる
 *          HW」を想定するが、本プロジェクトが対応する Renesas RA の EEPROM
 *          エミュレーションライブラリにそのような切り替え手段は無く、1 バイトの
 *          書き込みでも常に同じ消去・書き込みサイクルを要する（MemIf_SetMode()
 *          のドキュメント参照）。そのため本実装は Mode を受理するだけで
 *          状態を保持せず、実際の書き込みペースへは影響させない。
 */
typedef enum
{
    MEMIF_MODE_SLOW = 0U,  /**< 既定値。実効果は無い（型定義コメント参照）。 */
    MEMIF_MODE_FAST        /**< 実効果は無い（型定義コメント参照）。         */
} MemIf_ModeType;

#ifdef __cplusplus
}
#endif

#endif /* MEMIF_TYPES_H */

/**
 * \file    Rte_Type.h
 * \brief   RTE アプリケーション型定義 (AUTOSAR SWS_RTE 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef RTE_TYPE_H
#define RTE_TYPE_H

#include "Platform_Types.h"

// -------------------------------------------------------
// アプリケーション Signal の型エイリアス
//
// AUTOSAR では ARXML から自動生成される。
// SW-C は COM_SIGNAL_* の ID や uint16/uint8 の生の型を知らず、
// これらのアプリ型だけを使う。
// -------------------------------------------------------
typedef uint16 EngineSpeed_t;    /* rpm (0-15000) */
typedef uint8  CoolantTemp_t;    /* ℃  (0-255) */
typedef uint8  EngineOnFlag_t;   /* 0=OFF, 1=ON (1bit) */

/* ABS ECU シグナル型（ARXML で定義する ApplicationDataType に相当）*/
typedef uint16 VehicleSpeed_t;   /* 0.01 km/h (0-655.35 km/h) */
typedef uint8  BrakeActive_t;    /* 0=解除, 1=作動 (1bit) */
typedef uint8  AbsActive_t;      /* 0=非作動, 1=ABS 作動中 (1bit) */

/* ローカルボタン入力型（IoHwAb 経由で読み取る GPIO 入力）*/
typedef uint8  ButtonState_t;    /* 0=解放, 1=押下 */

typedef enum
{
    ENGINE_STATE_OFF      = 0,
    ENGINE_STATE_STARTING = 1,
    ENGINE_STATE_RUNNING  = 2,
    ENGINE_STATE_FAULT    = 3
} EngineState_t;

/* -------------------------------------------------------
 * Rte_IStatusType — データ変換 (E2E Transformer) を伴う Read ポートの
 * 拡張ステータス (AUTOSAR SWS_Rte の Rte_IStatusType に相当)。
 *
 * 実 AUTOSAR では Rte_Read/Rte_Receive がトランスフォーマチェーンを持つ
 * ポートに対して Std_ReturnType ではなくこの型を返し、複数要因が同時に
 * 起きた場合は優先順位（[SWS_Rte_08594] 等）に従って 1 つに絞り込む:
 * RTE_E_HARD_TRANSFORMER_ERROR > RTE_E_COM_STOPPED > RTE_E_SOFT_TRANSFORMER_ERROR。
 *
 * 本実装はこの優先順位をそのままでは適用しない。E2E チェックはフレーム
 * 受信時に非同期に行い結果をラッチする設計のため、RTE_E_HARD_TRANSFORMER_ERROR
 * は「過去の一時点（最後の受信）のラッチ」であり、RTE_E_COM_STOPPED
 * （Com_IsRxTimedOut()）は「現在も継続する物理層の状態」である。ラッチを
 * 無条件に優先すると、E2E エラーを起こしたフレームを最後に通信が本当に
 * 途絶えた場合、ラッチされた HARD_TRANSFORMER_ERROR が COM_STOPPED を
 * 永久にマスクしてしまう（コードレビューで発見・修正済み）。そのため
 * Rte_Read_*() は必ず Com_IsRxTimedOut()（生きている情報）を先に判定する
 * （詳細は Rte.c の「E2E Transformer 方式の RX ミラー」コメント参照）。
 *
 * 値そのものは本プロジェクトの簡略実装であり、AUTOSAR 公式のビット幅・
 * ビットパターンとは一致しない（本実装は 1 呼び出しにつき単一要因のみを
 * 返す前提のため、ビット OR による複数トランスフォーマの合成は行わない）。
 * ------------------------------------------------------- */
typedef uint8 Rte_IStatusType;
#define RTE_E_OK                      0x00U /**< 正常。データは最新かつ検証済み */
#define RTE_E_COM_STOPPED             0x01U /**< Com 受信デッドライン監視のタイムアウト中 */
#define RTE_E_HARD_TRANSFORMER_ERROR  0x02U /**< E2E: WRONGCRC/WRONGSEQUENCE/REPEATED/ERROR相当。
                                                  データは信頼できず、ミラーは前回の正常値のまま */
#define RTE_E_SOFT_TRANSFORMER_ERROR  0x03U /**< E2E: OKSOMELOST相当。データは使用可能だが
                                                  一部フレームの消失を検出した */

#endif

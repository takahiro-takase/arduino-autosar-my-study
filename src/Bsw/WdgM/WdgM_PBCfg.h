/**
 * \file    WdgM_PBCfg.h
 * \brief   ウォッチドッグマネージャ ポストビルドコンフィグ 型定義・外部宣言
 * \details WdgM_Init() に渡すコンフィグ構造体の型定義と
 *          WdgM_Config インスタンスの外部宣言を提供する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGM_PBCFG_H
#define WDGM_PBCFG_H

#include "Std_Types.h"
#include "WdgM_Cfg.h"

/**
 * \brief   論理監視 (Logical Supervision) で許可される遷移 1 件
 * \details AUTOSAR の WdgMCheckpointTransition コンテナに相当する。
 *          FromCheckpointId から ToCheckpointId への遷移のみを許可する
 *          有向グラフの辺を表す。FromCheckpointId には WDGM_CP_INITIAL も指定できる
 *          （起動直後に許可される最初のチェックポイントを表す）。
 *
 * \note    公開 API `WdgM_CheckpointReached()` の引数型は仕様準拠の
 *          `WdgM_CheckpointIdType`（uint16 由来、[SWS_WdgM_00357]）だが、
 *          本コンフィグテーブル自体はコンパイル時に確定する少数の固定値
 *          （WdgM_Cfg.h の `WDGM_CP_*`、現状 0〜3 と番兵値 `WDGM_CP_INITIAL`
 *          =0xFF のみ）しか保持しないため、あえて `uint8` のまま最小サイズで
 *          持たせている（Arduino の限られた RAM を無駄に消費しないための
 *          意図的な簡略化。方針上「シグネチャは仕様準拠、内部実装は Arduino
 *          で実現可能な範囲に簡略化」に該当。将来 256 種類を超える
 *          チェックポイントを設定する場合はこのテーブルも
 *          `WdgM_CheckpointIdType` へ拡張すること）。
 */
typedef struct
{
    uint8 FromCheckpointId;  /**< 遷移元チェックポイント ID（WDGM_CP_INITIAL 可） */
    uint8 ToCheckpointId;    /**< 遷移先チェックポイント ID */
} WdgM_TransitionCfgType;

/**
 * \brief   Deadline Supervision で監視する 1 件のチェックポイント間隔。
 * \details AUTOSAR の WdgMDeadlineSupervision コンテナに相当する。
 *          FromCheckpointId から ToCheckpointId への実際の経過時間が
 *          [MinMs, MaxMs] の範囲内であることを検証する。
 *
 * \note    `WdgM_TransitionCfgType` と同じ理由で FromCheckpointId/
 *          ToCheckpointId は `uint8` のまま（上記コメント参照）。
 */
typedef struct
{
    uint8  FromCheckpointId;  /**< 計測開始チェックポイント ID */
    uint8  ToCheckpointId;    /**< 計測終了チェックポイント ID */
    uint32 MinMs;             /**< 許容最小経過時間 (ms) */
    uint32 MaxMs;             /**< 許容最大経過時間 (ms) */
} WdgM_DeadlineCfgType;

/**
 * \brief   Supervised Entity 1 件の設定
 * \details AUTOSAR の WdgMSupervisedEntity コンテナに相当する。
 *
 * \note    公開 API の SEID 引数型は仕様準拠の `WdgM_SupervisedEntityIdType`
 *          （uint16 由来、[SWS_WdgM_00356]）だが、EntityId はコンパイル時に
 *          確定する少数の固定値（WdgM_Cfg.h の `WDGM_ENTITY_*`、現状 0/1 の
 *          み・`WDGM_SUPERVISED_ENTITY_COUNT`=2）しか取らないため、
 *          `WdgM_TransitionCfgType`/`WdgM_DeadlineCfgType` と同じ理由で
 *          `uint8` のまま最小サイズで持たせている。
 */
typedef struct
{
    uint8   EntityId;                  /**< エンティティ ID (WDGM_ENTITY_*) */
    uint32  SupervisionCycleMs;        /**< Alive Supervision サイクル時間 (ms) */
    uint8   ExpectedAliveIndications;  /**< サイクル内の最小 Checkpoint 呼び出し回数 */
    const WdgM_TransitionCfgType* Transitions;      /**< 許可された遷移テーブルの先頭 */
    uint8                          TransitionCount; /**< 遷移テーブルの要素数 */
    const WdgM_DeadlineCfgType*   Deadlines;        /**< Deadline テーブルの先頭 (NULL 可) */
    uint8                          DeadlineCount;   /**< Deadline テーブルの要素数 */
} WdgM_EntityCfgType;

/**
 * \brief   WdgM ポストビルドコンフィグ型
 * \details WdgM_Init() に渡す最上位コンフィグ構造体。WdgM_PBCfg.c でインスタンス化する。
 *
 * \note    EntityCount も `WdgM_EntityCfgType.EntityId` と同じ理由で `uint8`
 *          のまま（現状 `WDGM_SUPERVISED_ENTITY_COUNT`=2）。
 */
typedef struct
{
    const WdgM_EntityCfgType*  Entities;     /**< エンティティ設定配列の先頭 */
    uint8                       EntityCount;  /**< エンティティ数 */
} WdgM_ConfigType;

/** WdgM_PBCfg.c で定義されるポストビルドコンフィグインスタンス */
extern const WdgM_ConfigType WdgM_Config;

#endif /* WDGM_PBCFG_H */

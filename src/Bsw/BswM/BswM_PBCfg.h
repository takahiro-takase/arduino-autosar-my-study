/**
 * \file    BswM_PBCfg.h
 * \brief   BSW モードマネージャ ポストビルドコンフィグ 型定義
 * \details BswM ルールテーブルの型と外部参照を宣言する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイル。
 *
 *          ルール構造:
 *            BswM_RuleType — (モードソース, 期待値, アクション, タスクマスク) の 1 行
 *            BswM_ConfigType — ルールテーブルへのポインタとルール数
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef BSWM_PBCFG_H
#define BSWM_PBCFG_H

#include "Std_Types.h"
#include "BswM_Cfg.h"
#include "Com_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * ルール型定義
 * ----------------------------------------------------------------------- */

/** BswM アクション種別
 *
 *  実 AUTOSAR の BswM ActionList 項目種別のうち、本実装が持つのは以下の 2 系統
 *  （DaVinci: BswMGenericAction 相当）。
 *    - ACTIVATE/DEACTIVATE : TaskMask で指定した Os タスクの有効/無効化
 *      （本プロジェクト独自拡張。実 AUTOSAR には対応するアクション種別が
 *      無く、ModeSwitch 等で代用される）
 *    - PDU_GROUP_START/STOP : Com_IpduGroupStart()/Com_IpduGroupStop() の
 *      呼び出し（DaVinci: BswMPduGroupSwitch、[SWS_BswM_00273] "shall call
 *      Com_IpduGroupStart for each BswMEnabledPduGroupRef, and call
 *      Com_IpduGroupStop for each BswMDisabledPduGroupRef" 相当。実 AUTOSAR は
 *      1 つの BswMPduGroupSwitch アクションに複数の Enabled/Disabled 参照を
 *      持てるが、本実装は単一条件ルールと同じ簡略化方針で 1 ルール = 1 グループ
 *      のみ対応する） */
typedef enum
{
    BSWM_ACTION_ACTIVATE,         /**< 対象タスクを有効化 */
    BSWM_ACTION_DEACTIVATE,       /**< 対象タスクを無効化 */
    BSWM_ACTION_PDU_GROUP_START,  /**< Com_IpduGroupStart(IpduGroupId, Initialize) を呼ぶ */
    BSWM_ACTION_PDU_GROUP_STOP    /**< Com_IpduGroupStop(IpduGroupId) を呼ぶ */
} BswM_ActionType;

/** BswM モードソース種別 (どのモジュールからの通知か) */
typedef enum
{
    BSWM_MODE_SRC_ECUM,  /**< EcuM_CurrentState からの通知 */
    BSWM_MODE_SRC_COMM   /**< ComM_CurrentMode からの通知  */
} BswM_ModeSrcType;

/**
 * \brief   BswM ルール 1 行分の型
 *
 * \details 「IF ModeSrc のモードが ModeValue になったら、
 *            TaskMask で指定したタスクに対して Action を実行する」
 *          という単一条件ルールを表す。
 *
 *          複合条件 (AND/OR) が必要な場合は LogicalExpression を追加するが、
 *          本実装は単一条件のみとする (学習用簡略化)。
 *
 *          TaskMask は Action=ACTIVATE/DEACTIVATE のときのみ使用する。
 *          IpduGroupId/Initialize は Action=PDU_GROUP_START/STOP のときのみ
 *          使用する（PDU_GROUP_STOP は Initialize を参照しない。
 *          Com_IpduGroupStop() 自体がその引数を持たないため）。
 */
typedef struct
{
    BswM_ModeSrcType    ModeSrc;    /**< トリガとなるモードのソース */
    uint8               ModeValue;  /**< トリガとなるモード値 */
    BswM_ActionType     Action;     /**< 実行するアクション */
    uint16              TaskMask;   /**< 操作対象タスクのビットマスク (9 タスク分) */
    Com_IpduGroupIdType IpduGroupId; /**< 操作対象の I-PDU Group ID */
    uint8               Initialize;  /**< Com_IpduGroupStart() の initialize 引数 */
} BswM_RuleType;

/**
 * \brief   BswM ポストビルドコンフィグ型
 * \details BswM_Init() に渡すコンフィグ構造体。BswM_PBCfg.c でインスタンス化する。
 */
typedef struct
{
    const BswM_RuleType*  Rules;      /**< ルールテーブル先頭ポインタ */
    uint8                 RuleCount;  /**< ルール数 */
} BswM_ConfigType;

/** BswM_PBCfg.c で定義するコンフィグインスタンス */
extern const BswM_ConfigType BswM_Config;

#ifdef __cplusplus
}
#endif

#endif /* BSWM_PBCFG_H */

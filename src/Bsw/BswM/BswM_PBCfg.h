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
 * \brief   ルール条件の論理演算子（BswMLogicalOperator [ECUC_BswM_00814] の実測）。
 *
 * \details 実 AUTOSAR は BSWM_AND/BSWM_NAND/BSWM_NOT/BSWM_OR/BSWM_XOR の5種を
 *          定義するが、本プロジェクトは AND/OR のみ対応する（学習用簡略化。
 *          NAND/NOT/XOR は本プロジェクトのルールで使用実績がないため対応除外）。
 *          [ECUC_BswM_00814] に「論理演算子が BSWM_NOT 以外で条件が1つしかない
 *          場合、この演算子は無効（単一条件と同じ）」と明記されている実測結果
 *          どおり、単一条件ルールも BSWM_OP_AND（ConditionCount=1）で表現する
 *          （従来の「単一条件専用の型」を別途持たない）。
 */
typedef enum
{
    BSWM_OP_AND,  /**< [SWS_BswM_00245]: 全条件が真のときのみ真（条件数1なら単一条件と等価） */
    BSWM_OP_OR    /**< [SWS_BswM_00247]: いずれかの条件が真なら真 */
} BswM_LogicalOperatorType;

/**
 * \brief   単一のモード条件（BswMModeCondition [ECUC_BswM_00807] の簡略版）。
 *
 * \details 実 AUTOSAR の BswMModeCondition は BSWM_EQUALS/BSWM_EQUALS_NOT や
 *          イベントポートの SET/CLEARED も表現できるが、本プロジェクトは
 *          「ModeSrc の現在値が ModeValue と一致するか」（BSWM_EQUALS 相当）
 *          のみを扱う。
 */
typedef struct
{
    BswM_ModeSrcType ModeSrc;    /**< 比較対象のモードソース */
    uint8            ModeValue;  /**< 比較する値 */
} BswM_ConditionType;

/**
 * \brief   BswM ルール 1 行分の型
 *
 * \details 「IF (Condition[0] Operator Condition[1] ... ) が真になったら、
 *            TaskMask で指定したタスクに対して Action を実行する」という
 *          複合条件ルールを表す（BswMLogicalExpression [ECUC_BswM_00808] の
 *          簡略版。条件数は BSWM_RULE_MAX_CONDITIONS(=2) までで、任意の木構造
 *          （LogicalExpression の入れ子）ではなくフラットな配列とする）。
 *          Action は条件の評価結果が false→true へ遷移したときのみ実行する
 *          （BswM.c の BswM_ExecuteRules() 参照。同じ結果が続く間の重複実行や、
 *          true→false への遷移では Action を実行しない）。
 *
 *          TaskMask は Action=ACTIVATE/DEACTIVATE のときのみ使用する。
 *          IpduGroupId/Initialize は Action=PDU_GROUP_START/STOP のときのみ
 *          使用する（PDU_GROUP_STOP は Initialize を参照しない。
 *          Com_IpduGroupStop() 自体がその引数を持たないため）。
 */
typedef struct
{
    BswM_LogicalOperatorType Operator;        /**< Condition[] の組み合わせ方 */
    BswM_ConditionType       Condition[BSWM_RULE_MAX_CONDITIONS]; /**< 条件配列 */
    uint8                    ConditionCount;  /**< 実際に使う条件数 (1 または 2) */
    BswM_ActionType          Action;          /**< 実行するアクション */
    uint32                   TaskMask;        /**< 操作対象タスクのビットマスク (OS_TASK_COUNT タスク分。
                                                *   uint16 では 16 個までしか表現できず、Task 16
                                                *   (SecOC_MainFunctionTx) 追加時に uint32 へ拡張した） */
    Com_IpduGroupIdType      IpduGroupId;     /**< 操作対象の I-PDU Group ID */
    uint8                    Initialize;       /**< Com_IpduGroupStart() の initialize 引数 */
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

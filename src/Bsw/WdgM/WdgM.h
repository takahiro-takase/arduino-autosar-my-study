/**
 * \file    WdgM.h
 * \brief   ウォッチドッグマネージャ 公開インタフェース (AUTOSAR SWS_WdgM 準拠)
 * \details Supervised Entity の Alive Supervision / Logical Supervision インタフェース。
 *
 *          使い方:
 *            1. EcuM_Init 内で WdgM_Init(&WdgM_Config) を呼ぶ。
 *            2. 監視対象の Runnable 内のプログラムフロー上の各地点で
 *               WdgM_CheckpointReached(WDGM_ENTITY_*, WDGM_CP_*) を呼ぶ。
 *            3. Os スケジューラが WdgM_MainFunction() を定期実行する。
 *               CheckpointReached が WDGM_EXPECTED_ALIVE_INDICATIONS 回以上来ていれば OK、
 *               0 回のまま周期が来たら FAILED とみなしログで通知する (Alive Supervision)。
 *            4. WdgM_CheckpointReached() は呼ばれた瞬間にも、直前のチェックポイントから
 *               今回のチェックポイントへの遷移が許可されているかを即座に確認する
 *               (Logical Supervision)。許可されない順序が来た場合は即座に FAILED とする。
 *
 *          本プロジェクトでの失敗アクション:
 *            WdgM_LocalStatus を FAILED に更新し WARN ログを出力する。
 *            実装製品では WdgM がハードウェアウォッチドッグのリフレッシュを停止し
 *            システムリセットをトリガするが、Arduino UNO では HW ウォッチドッグ経由の
 *            強制リセットは未実装。
 *
 *          学習用簡略化:
 *            Alive と Logical の判定結果を 1 つの WdgM_LocalStatusType に統合している。
 *            そのため Logical Supervision が FAILED と判定した直後でも、次の
 *            WdgM_MainFunction サイクルで Alive 条件を満たせば「recovered」として
 *            OK に復帰する（AUTOSAR ではアルゴリズムごとに個別の判定結果を保持する）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGM_H
#define WDGM_H

#include "Std_Types.h"
#include "WdgM_Cfg.h"
#include "WdgM_PBCfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */

/** Supervised Entity ID 型 */
typedef uint8 WdgM_SupervisedEntityIdType;

/**
 * \brief   Supervised Entity のローカルステータス
 * \details AUTOSAR WdgM_LocalStatusType に相当する。
 */
typedef enum
{
    WDGM_LOCAL_STATUS_OK          = 0x00U,  /**< 正常: Checkpoint が期待回数以上届いた */
    WDGM_LOCAL_STATUS_FAILED      = 0x01U,  /**< 失敗: Checkpoint 不足 */
    WDGM_LOCAL_STATUS_DEACTIVATED = 0x0FU   /**< 無効: 初期化前または ID 不正 */
} WdgM_LocalStatusType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   WdgM モジュールを初期化する。
 *
 * \details 全エンティティの Alive カウンタとステータスを初期化する。
 *          EcuM_Init() の末尾（Os_Init より前）に呼び出すこと。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_Init(const WdgM_ConfigType* ConfigPtr);

/**
 * \brief   Supervised Entity がチェックポイントに到達したことを報告する。
 *
 * \details 監視対象の Runnable 内のプログラムフロー上の各地点から呼び出す。
 *          WdgM 内部の Alive カウンタをインクリメントし (Alive Supervision)、
 *          直前に報告されたチェックポイントから今回のチェックポイントへの遷移が
 *          許可テーブルに含まれるかを即座に検査する (Logical Supervision)。
 *          許可されない遷移の場合はローカルステータスを即座に FAILED にする。
 *
 * \param[in]  SEID          エンティティ ID (WdgM_Cfg.h の WDGM_ENTITY_*)。
 * \param[in]  CheckpointId  チェックポイント ID (WdgM_Cfg.h の WDGM_CP_*)。
 * \return     E_OK: 正常受付。E_NOT_OK: ID 不正。
 *
 * \ServiceID      {0x0E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, uint8 CheckpointId);

/**
 * \brief   Supervised Entity の現在のローカルステータスを取得する。
 *
 * \param[in]  SEID  エンティティ ID。
 * \return     WdgM_LocalStatusType 値。ID 不正の場合は DEACTIVATED。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
WdgM_LocalStatusType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID);

/**
 * \brief   WdgM 周期処理。Alive Supervision を評価する。
 *
 * \details Os スケジューラから WDGM_SUPERVISION_CYCLE_MS ごとに呼ばれる。
 *          各エンティティの Alive カウンタを検査し、期待回数を満たさない場合は
 *          ローカルステータスを FAILED に更新して WARN ログを出力する。
 *          検査後、カウンタは次のサイクルのためにリセットする。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* WDGM_H */

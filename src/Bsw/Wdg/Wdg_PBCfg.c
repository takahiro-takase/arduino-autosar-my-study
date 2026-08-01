/**
 * \file    Wdg_PBCfg.c
 * \brief   Watchdog Driver ポストビルドコンフィグ 定義
 * \details AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル
 *          に相当する。
 *
 *          WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS を参照している理由:
 *          以前は Wdg 相当の層が存在せず、WdgM_Hw.cpp が `WDT.begin(4000)` を
 *          直接ハードコードしていたため、「WdgM_Cfg.h の
 *          WDGM_HW_WATCHDOG_TIMEOUT_MS と手動で一致させること」という
 *          コメントだけが同期を保証する脆い状態だった。PBCfg はモジュール間の
 *          配線を行うコンフィグ層であり、複数モジュールの定義を参照してよい
 *          という本プロジェクトの確立された方針（NvM_PBCfg.c が Dem_Cfg.h を
 *          参照するのと同じ）に従い、ここで WDGM_HW_WATCHDOG_TIMEOUT_MS を
 *          直接使うことで、値の実体を 1 か所（WdgM_Cfg.h）だけに保ち、
 *          手動同期の必要性そのものをなくした。Wdg.c 自身は WdgM_Cfg.h を
 *          知らない（下位層が上位層の設定ヘッダへ依存しないという層構造は
 *          維持している）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Wdg_PBCfg.h"
#include "WdgM_Cfg.h"

const Wdg_ConfigType Wdg_Config =
{
    (uint16)WDGM_HW_WATCHDOG_TIMEOUT_MS  /* DefaultTimeoutMs */
};

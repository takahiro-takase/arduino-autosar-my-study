/**
 * \file    StbM_Cfg.h
 * \brief   同期時間ベースマネージャ (StbM) プリコンパイル設定 (AUTOSAR SWS_StbM 準拠、最小版)
 * \details 時間ベースの数と、同期喪失のタイムアウトを定義する。
 *
 *          本プロジェクトでの位置づけ:
 *            CanTSyn（CAN 時刻同期）が配信・受信する時刻の置き場。同期時間ベースを2つ持つ。
 *              - 時間ベース 0（マスター）: 本 ECU が時間マスター。全域時間 = ECU の稼働時間。
 *                CanTSyn が SYNC/FUP で CAN へ配信する。
 *              - 時間ベース 1（スレーブ）: 他ノードの時間マスターに同期する。CanTSyn が
 *                受信した SYNC/FUP から StbM_BusSetGlobalTime() で時刻を更新する。
 *
 *          対応範囲（簡略版）:
 *            - 同期時間ベースのみ（オフセット時間ベース、純ローカル時間ベース、レート補正、
 *              時刻の飛び（TIMELEAP）の検出、時刻記録、通知コールバック、タイマ、
 *              StbM_GetCurrentTimeExtended / SetUserData / UpdateGlobalTime 等は対象外）。
 *            - 仮想ローカル時間（Virtual Local Time）は Os カウンタ（ms）を ns に換算して使う。
 *              分解能は 1ms。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様（docs/autosar/4.3.1/AUTOSAR_SWS_SynchronizedTimeBaseManager.pdf）
 *          を参考にした学習用実装です。AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef STBM_CFG_H
#define STBM_CFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_StbM [SWS_StbM_00041] の開発エラー。ModuleId は AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1）で Synchronized Time-Base Manager (StbM) に割り当てられた固定値 160。
 * ----------------------------------------------------------------------- */
/** AUTOSAR StbM の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 160） */
#define STBM_MODULE_ID  160U

#define STBM_E_PARAM            0x0AU  /* [SWS_StbM_00041]: 不正なパラメータ（時間ベース ID 等） */
#define STBM_E_NOT_INITIALIZED  0x0BU  /* [SWS_StbM_00041]: 未初期化時の API 呼び出し */
#define STBM_E_PARAM_POINTER    0x10U  /* [SWS_StbM_00041]: NULL ポインタ */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。SWS_StbM 8.1.3 の Service ID） */
/* cppcheck-suppress misra-c2012-2.5 */
#define STBM_API_ID_INIT                  0x00U
#define STBM_API_ID_MAIN_FUNCTION         0x04U
#define STBM_API_ID_GET_VERSION_INFO      0x05U
#define STBM_API_ID_GET_CURRENT_TIME      0x07U
#define STBM_API_ID_GET_CURRENT_TIME_RAW  0x09U
#define STBM_API_ID_GET_CURRENT_TIME_DIFF 0x0AU
#define STBM_API_ID_SET_GLOBAL_TIME       0x0BU
#define STBM_API_ID_BUS_SET_GLOBAL_TIME   0x0FU
#define STBM_API_ID_GET_TIME_BASE_STATUS  0x14U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define STBM_VENDOR_ID          0U
#define STBM_SW_MAJOR_VERSION   1U
#define STBM_SW_MINOR_VERSION   0U
#define STBM_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * 時間ベース
 * ----------------------------------------------------------------------- */

/** 時間ベースの数（ID は 0 から連続）。 */
#define STBM_TIME_BASE_COUNT  2U

/** 時間ベース 0: 本 ECU が時間マスターの同期時間ベース（全域時間 = ECU の稼働時間）。 */
#define STBM_TIME_BASE_MASTER  0U

/** 時間ベース 1: 他ノードの時間マスターに同期する同期時間ベース（CanTSyn が更新する）。 */
#define STBM_TIME_BASE_SLAVE   1U

/** 時間ベースごとの同期喪失のタイムアウト [ms]（StbMSyncLossTimeout 相当、
 *  [SWS_StbM_00179]）。この時間 StbM_BusSetGlobalTime() で更新されないと TIMEOUT ビットを立てる。
 *  0 ならタイムアウトを監視しない（マスターは自分で時刻を決めるため 0）。 */
#define STBM_SYNC_LOSS_TIMEOUT_MS_MASTER  0UL
#define STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE   5000UL

/** 時間ベースごとの、起動時から同期済み（GLOBAL_TIME_BASE）にするか（1=同期済み、0=未同期）。
 *  時間マスターは自分で時刻を決めるため、全域時間 = 仮想ローカル時間で起動時から同期済み。 */
#define STBM_INITIALLY_SYNCED_MASTER  1U
#define STBM_INITIALLY_SYNCED_SLAVE   0U

/** StbM_MainFunction() の呼び出し周期 [ms]（StbMMainFunctionPeriod 相当、
 *  Os.json のタスク周期と一致させること）。 */
/* cppcheck-suppress misra-c2012-2.5 */
#define STBM_MAIN_FUNCTION_PERIOD_MS  100UL

#endif /* STBM_CFG_H */

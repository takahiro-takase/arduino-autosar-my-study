/**
 * \file    CanSM_Cfg.h
 * \brief   CanSM プリコンパイル設定 (AUTOSAR SWS_CanSM 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANSM_CFG_H
#define CANSM_CFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_CanSM_00654 の Development Errors 表に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * CAN State Manager (CanSM) に割り当てられた固定値 140 を使う。
 *
 * 本実装は CanSM_ConfigType を保持しない簡略設計（CanSM_Init は実際の
 * AUTOSAR 仕様では ConfigPtr を取るが、本プロジェクトは無引数）のため、
 * 初期化済みかどうかを示す専用フラグ（CanSM_Initialized）を別途持つ。
 * ----------------------------------------------------------------------- */
/** AUTOSAR CAN State Manager の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 140） */
#define CANSM_MODULE_ID  140U

/** 開発エラーコード（SWS_CanSM_00654 表より、実際に使用する分のみ） */
#define CANSM_E_UNINIT                   0x01U
#define CANSM_E_PARAM_POINTER            0x02U
#define CANSM_E_INVALID_NETWORK_HANDLE   0x03U
#define CANSM_E_PARAM_CONTROLLER         0x04U

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
/* cppcheck-suppress misra-c2012-2.5 */
#define CANSM_API_ID_INIT                  0x00U
#define CANSM_API_ID_REQUEST_COM_MODE       0x02U
#define CANSM_API_ID_GET_CURRENT_COM_MODE  0x03U
#define CANSM_API_ID_CONTROLLER_BUSOFF     0x04U
/** [SWS_CanSM_00396] `CanSM_ControllerModeIndication`（旧 `CanSM_ControllerWakeup`、
 *  2026-09-05 是正）。誤って 0x06（実仕様では別関数
 *  `CanSM_ConfirmPnAvailability`、Partial Networking 用で本プロジェクト未実装）
 *  を使っていたが、正しくは 0x07。 */
#define CANSM_API_ID_CONTROLLER_MODE_INDICATION  0x07U
#define CANSM_API_ID_MAIN_FUNCTION         0x05U
#define CANSM_API_ID_DEINIT                 0x14U
#define CANSM_API_ID_GET_VERSION_INFO       0x01U

/** バージョン情報（SWS_CANStateManager、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define CANSM_VENDOR_ID          0U
#define CANSM_SW_MAJOR_VERSION   1U
#define CANSM_SW_MINOR_VERSION   0U
#define CANSM_SW_PATCH_VERSION   0U

/* @@GEN-BEGIN cansm-params  生成元: config/data/CanSM.json  （自動生成: 手編集禁止） */
/** 管理ネットワーク（チャネル）数 */
#define CANSM_CHANNEL_COUNT  1U

/** Bus-Off 回復待機時間 L1 [ms]（CanSMBorTimeL1 相当）
 *  AUTOSAR T_REC: バスオフ後、コントローラを再起動するまでの待ち時間。
 *  実車では 100〜200 ms が一般的。CANSM_BUSOFF_L1_TO_L2_COUNT 回まではこの
 *  短い周期でリトライする（SWS_CanSM_00514）。 */
#define CANSM_BUSOFF_RECOVERY_L1_MS  200UL

/** Bus-Off 回復待機時間 L2 [ms]（CanSMBorTimeL2 相当）
 *  CANSM_BUSOFF_L1_TO_L2_COUNT 回のリトライを超えても回復しない場合、
 *  この長い周期へ切り替えて無期限にリトライを継続する（SWS_CanSM_00515）。
 *  「諦めて二度と復帰しない」状態は AUTOSAR 仕様に存在しないため、
 *  L1/L2 いずれの場合も回復試行そのものは打ち切らない。 */
#define CANSM_BUSOFF_RECOVERY_L2_MS  5000UL

/** Bus-Off 回復試行回数の L1→L2 切替閾値（CanSMBorCounterL1ToL2 相当）
 *  この回数を超えて回復しない場合、一時的なバス障害ではなく持続的な
 *  Bus-Off と判断し、リトライ周期を L2 へ切り替えて継続する（回復自体は続く）。
 *  DemConf_DemEventParameter_CAN_BUSOFF (DTC 0x000108) へは、Bus-Off の検出ごとに PREFAILED を
 *  報告する。Dem 側のしきい値 DEM_DEBOUNCE_LIMIT_CAN_BUSOFF は、この値 + 1（config/data/Dem.json）。
 *  両者のずれは CanSM.c の #error で検出する。 */
#define CANSM_BUSOFF_L1_TO_L2_COUNT  3U

/** Bus-Off 回復の確定条件（G_BUS_OFF_PASSIVE）に TX 確認のポーリングを使うか（CanSMBorTxConfirmationPolling 相当、
 *  1=使う、0=使わない）。1 なら、再起動後に CanIf_GetTxConfirmationState() が CANIF_TX_RX_NOTIFICATION
 *  （再起動後に TX 確認が 1 回でもあった）を返した時点で回復が確定する（SWS_CanSM_00497）。
 *  0 なら（SILENT_COM に戻った場合も）、再起動から CANSM_BOR_TIME_TX_ENSURED_MS が経過した時点で確定する（SWS_CanSM_00496）。
 *  確定すると、Dem へ PASSED を報告し、回復試行回数を 0 に戻す（SWS_CanSM_00498）。 */
#define CANSM_BOR_TX_CONFIRMATION_POLLING  1U

/** Bus-Off 回復の確定までの待ち時間 [ms]（CanSMBorTimeTxEnsured 相当）。
 *  CANSM_BOR_TX_CONFIRMATION_POLLING が 0 のとき（SWS_CanSM_00496）と、SILENT_COM に戻った場合（TX を抑制
 *  していて TX 確認が得られないため）に使う。この時間の間に再度 Bus-Off が起きなければ、回復が確定したとみなす。 */
#define CANSM_BOR_TIME_TX_ENSURED_MS  1000UL

/** ウェイクアップ検証タイムアウト [ms]（AUTOSAR EcuM Wakeup Validation
 *  Protocol 相当）。ボランタリスリープ中に MCP2515 の WAKIF が立ってから、
 *  この時間内に有効な CAN フレームを 1 つも受信できなければ、電気的ノイズ等
 *  による誤ウェイクアップとみなして再びスリープへ戻す。 */
#define CANSM_WAKEUP_VALIDATION_MS  2000UL
/* @@GEN-END cansm-params */

#endif /* CANSM_CFG_H */

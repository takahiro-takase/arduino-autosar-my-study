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

/** 管理ネットワーク（チャネル）数 */
#define CANSM_CHANNEL_COUNT        1U

/**
 * Bus-Off 回復待機時間 L1 [ms]（CanSMBorTimeL1 相当）
 * AUTOSAR T_REC: バスオフ後、コントローラを再起動するまでの待ち時間。
 * 実車では 100〜200 ms が一般的。CANSM_BUSOFF_L1_TO_L2_COUNT 回まではこの
 * 短い周期でリトライする（SWS_CanSM_00514）。
 */
#define CANSM_BUSOFF_RECOVERY_L1_MS   200UL

/**
 * Bus-Off 回復待機時間 L2 [ms]（CanSMBorTimeL2 相当）
 * CANSM_BUSOFF_L1_TO_L2_COUNT 回のリトライを超えても回復しない場合、
 * この長い周期へ切り替えて無期限にリトライを継続する（SWS_CanSM_00515）。
 * 「諦めて二度と復帰しない」状態は AUTOSAR 仕様に存在しないため、
 * L1/L2 いずれの場合も回復試行そのものは打ち切らない。
 */
#define CANSM_BUSOFF_RECOVERY_L2_MS  5000UL

/**
 * Bus-Off 回復試行回数の L1→L2 切替閾値（CanSMBorCounterL1ToL2 相当）
 * この回数を超えて回復しない場合、一時的なバス障害ではなく持続的な
 * Bus-Off と判断し、DEM_EVENT_CAN_BUSOFF (DTC 0x000108) を Dem へ FAILED
 * 報告した上で、リトライ周期を L2 へ切り替えて継続する（回復自体は続く）。
 * Dem 側は DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1（この報告は既に十分なリトライを
 * 経た後の確定情報のため、Dem では重ねてデバウンスしない）。
 */
#define CANSM_BUSOFF_L1_TO_L2_COUNT   3U

/**
 * ウェイクアップ検証タイムアウト [ms]（AUTOSAR EcuM Wakeup Validation
 * Protocol 相当）。ボランタリスリープ中に MCP2515 の WAKIF が立ってから、
 * この時間内に有効な CAN フレームを 1 つも受信できなければ、電気的ノイズ等
 * による誤ウェイクアップとみなして再びスリープへ戻す。
 */
#define CANSM_WAKEUP_VALIDATION_MS  2000UL

#endif /* CANSM_CFG_H */

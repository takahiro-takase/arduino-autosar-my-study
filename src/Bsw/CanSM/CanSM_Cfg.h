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
 * Bus-Off 回復待機時間 [ms]
 * AUTOSAR T_REC: バスオフ後、コントローラを再起動するまでの待ち時間。
 * 実車では 100〜200 ms が一般的。
 */
#define CANSM_BUSOFF_RECOVERY_MS  200U

/**
 * Bus-Off 最大回復試行回数
 * 連続バスオフがこの回数に達すると回復を停止し、
 * DEM_EVENT_CAN_BUSOFF (DTC 0x000108) として Dem へ報告する。
 * Dem 側は DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1（この断念報告は既に
 * 十分なリトライを経た後の確定情報のため、Dem では重ねてデバウンスしない）。
 */
#define CANSM_BUSOFF_MAX_RETRIES    3U

#endif /* CANSM_CFG_H */

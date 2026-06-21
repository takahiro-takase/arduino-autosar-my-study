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
 * Bus-Off 内側リトライ回数（1 回の回復サイクルあたり）
 * この回数を超えて再起動に失敗すると 1 回分の「回復断念」とみなし、
 * DEM_EVENT_CAN_BUSOFF (DTC 0x000108) に FAILED を報告したうえで
 * 内側リトライカウンタをリセットし、回復サイクルを再開する
 * （CANSM_BUSOFF_MAX_GIVEUP_CYCLES に達するまでは完全には断念しない）。
 */
#define CANSM_BUSOFF_MAX_RETRIES    3U

/**
 * Bus-Off 回復断念の許容回数（外側カウンタ）
 * 内側リトライが尽きるたびに 1 増える。この回数に達すると今度こそ
 * 通信を完全に断念し、ComM へ NO_COM を通知する（EcuM が POST_RUN へ移行）。
 * Dem のデバウンス確定 (DEM_DEBOUNCE_LIMIT) に達してから最終的に断念させる
 * ため、DEM_DEBOUNCE_LIMIT 以上の値を設定すること。
 */
#define CANSM_BUSOFF_MAX_GIVEUP_CYCLES  2U

#endif /* CANSM_CFG_H */

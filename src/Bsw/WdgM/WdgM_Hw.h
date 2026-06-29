/**
 * \file    WdgM_Hw.h
 * \brief   WdgM ハードウェア依存層 内部インタフェース
 * \details WdgM.c (純粋 C, AUTOSAR API 層) と、実際の HW ウォッチドッグ
 *          ペリフェラルとの境界を定義する。WdgM.c はこのヘッダ経由でのみ
 *          ウォッチドッグを操作し、MCU 固有のヘッダ (avr/wdt.h 等) を
 *          直接知らない。本ヘッダは WdgM.c と WdgM_Hw.c 以外から
 *          インクルードしないこと。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGM_HW_H
#define WDGM_HW_H

#ifdef __cplusplus
extern "C" {
#endif

/** \brief  HW ウォッチドッグを WDGM_HW_WATCHDOG_TIMEOUT_MS 相当のタイムアウトで有効化する。 */
void WdgM_Hw_Enable(void);

/** \brief  HW ウォッチドッグを無効化する (POST_RUN/SHUTDOWN 等、意図的な停止時)。 */
void WdgM_Hw_Disable(void);

/** \brief  HW ウォッチドッグのタイムアウトカウンタをリフレッシュする。 */
void WdgM_Hw_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* WDGM_HW_H */

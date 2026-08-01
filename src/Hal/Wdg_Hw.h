/**
 * \file    Wdg_Hw.h
 * \brief   Wdg ハードウェア依存層 内部インタフェース
 * \details Wdg.c (純粋 C, AUTOSAR API 層) と、実際の HW ウォッチドッグ
 *          ペリフェラルとの境界を定義する。Wdg.c はこのヘッダ経由でのみ
 *          ウォッチドッグを操作し、MCU 固有のヘッダ (RA の WDT ライブラリ等)
 *          を直接知らない。本ヘッダは Wdg.c と Wdg_Hw.cpp 以外から
 *          インクルードしないこと。
 *
 *          旧 WdgM_Hw.h/.cpp をリネームしたもの（WdgM が直接 HW を叩いていた
 *          構成から、WdgM → WdgIf → Wdg → Wdg_Hw の 4 層構成へ分離した際に、
 *          HW 境界の実体は Wdg.c の直下に置くよう移設した。詳細は
 *          Wdg.c 冒頭のコメント参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDG_HW_H
#define WDG_HW_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   HW ウォッチドッグを指定タイムアウトで有効化する。
 *
 * \param[in]  timeoutMs  タイムアウト値 [ms]（Wdg_ConfigType.DefaultTimeoutMs
 *                        経由で Wdg_PBCfg.c の値がそのまま渡される）。
 */
void Wdg_Hw_Enable(uint16 timeoutMs);

/** \brief  HW ウォッチドッグを無効化する (POST_RUN/SHUTDOWN 等、意図的な停止時)。 */
void Wdg_Hw_Disable(void);

/** \brief  HW ウォッチドッグのタイムアウトカウンタをリフレッシュする。 */
void Wdg_Hw_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* WDG_HW_H */

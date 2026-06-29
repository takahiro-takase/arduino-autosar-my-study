/**
 * \file    Mcu_Hw.h
 * \brief   Mcu ハードウェア依存層 内部インタフェース
 * \details main.cpp の起動シーケンス（リセット原因の取得・起動時 WDT 無効化）
 *          が MCU 種別に依存しないよう、AVR 固有 API をこの層の裏に隠す。
 *          本ヘッダは main.cpp と Mcu_Hw.c 以外からインクルードしないこと。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MCU_HW_H
#define MCU_HW_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   直前のリセット原因フラグを読み取り、レジスタをクリアする。
 *
 * \details AVR では MCUSR (bit3 WDRF/bit2 BORF/bit1 EXTRF/bit0 PORF) を読み、
 *          ブートローダの WDT 無限リセットループ対策として読み取り後にクリアする。
 *          クリア前の値を返すため、呼び出し元は起動直後に 1 回だけ呼ぶこと。
 *
 * \return  リセット原因フラグ（クリア前の値）。
 */
uint8 Mcu_Hw_ReadAndClearResetReason(void);

/**
 * \brief   起動直後に HW ウォッチドッグを無効化する。
 *
 * \details WdgM_Init() が必要なタイムアウトで後ほど再度有効化する前提。
 *          ブートローダ起因の WDT 無限リセットループを防ぐため、
 *          setup() の最初（Mcu_Hw_ReadAndClearResetReason() の直後）に呼ぶこと。
 */
void Mcu_Hw_DisableWatchdogAtBoot(void);

#ifdef __cplusplus
}
#endif

#endif /* MCU_HW_H */

/**
 * \file    Mcu_Hw.h
 * \brief   Mcu ハードウェア依存層 内部インタフェース
 * \details main.cpp の起動シーケンス（リセット原因の取得・起動時 WDT 無効化）
 *          が MCU 種別に依存しないよう、MCU 固有 API をこの層の裏に隠す。
 *          リセット原因のビット配置は MCU ごとに全く異なる（AVR の MCUSR と
 *          Renesas RA の RSTSR0/RSTSR1 はビット位置に互換性がない）ため、
 *          生のレジスタ値ではなく、デコード済みの Mcu_Hw_ResetReasonType を返す。
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
 * \brief   デコード済みのリセット原因。
 *
 * \details 各フィールドは該当する原因でリセットされていれば 1、それ以外は 0。
 *          複数同時に立つこともある。MCU によっては区別できない原因がある
 *          （例: Renesas RA は外部リセットピン専用の検出フラグを持たないため、
 *          External は常に 0 を返す）。
 */
typedef struct
{
    uint8 Watchdog;  /**< ウォッチドッグタイムアウトによるリセット */
    uint8 BrownOut;  /**< 電源電圧低下 (LVD) によるリセット        */
    uint8 External;  /**< 外部リセット (RESET ピン/ボタン等)。検出不可な MCU では常に 0 */
    uint8 PowerOn;   /**< 電源投入リセット                          */
} Mcu_Hw_ResetReasonType;

/**
 * \brief   直前のリセット原因を読み取り、レジスタをクリアする。
 *
 * \details ブートローダの WDT 無限リセットループ対策として、読み取り後に
 *          原因レジスタをクリアする（クリアしないと次回リセット時に古い値が
 *          残り原因判定を誤る）。呼び出し元は起動直後に 1 回だけ呼ぶこと。
 *
 * \return  リセット原因（クリア前の値をデコードしたもの）。
 */
Mcu_Hw_ResetReasonType Mcu_Hw_ReadAndClearResetReason(void);

/**
 * \brief   起動直後に HW ウォッチドッグを無効化する。
 *
 * \details WdgM_Init() が必要なタイムアウトで後ほど再度有効化する前提。
 *          ブートローダ起因の WDT 無限リセットループを防ぐため、
 *          setup() の最初（Mcu_Hw_ReadAndClearResetReason() の直後）に呼ぶこと。
 *          MCU によっては起動直後に WDT が有効化されていないことがあり、
 *          その場合は何もしない（例: Renesas RA は WDT を明示的に
 *          begin() するまで動作しないため、本関数は no-op）。
 */
void Mcu_Hw_DisableWatchdogAtBoot(void);

#ifdef __cplusplus
}
#endif

#endif /* MCU_HW_H */

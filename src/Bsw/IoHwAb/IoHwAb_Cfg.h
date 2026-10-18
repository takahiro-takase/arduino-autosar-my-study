/**
 * \file    IoHwAb_Cfg.h
 * \brief   IoHwAb プリコンパイル設定
 * \details ボタンのデバウンス・固着判定のサンプル数と、ADC 電圧低下の閾値を定義する。
 *          値は config/data/IoHwAb.json から生成する。サンプル数は、時間 [ms] を
 *          IoHwAb_MainFunction を呼ぶタスクの周期（config/data/Os.json）で割って求める。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * 
ote    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef IOHWAB_CFG_H
#define IOHWAB_CFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* @@GEN-BEGIN iohwab-params  生成元: config/data/IoHwAb.json  （自動生成: 手編集禁止） */
/** ボタン確定に必要な連続一致サンプル数 (4 × 10ms = 40ms) */
#define IOHWAB_BUTTON_DEBOUNCE_COUNT  4U

/** ボタン固着と判定するまでの確定押下継続サンプル数 (500 × 10ms = 5000ms) */
#define IOHWAB_BUTTON_STUCK_COUNT     500U

/** ADC 電圧低下と判定する閾値 [mV] */
#define IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV  1000U
/* @@GEN-END iohwab-params */

#endif /* IOHWAB_CFG_H */

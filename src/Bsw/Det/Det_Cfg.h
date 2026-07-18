/**
 * \file    Det_Cfg.h
 * \brief   Det プリコンパイル設定 (AUTOSAR SWS_Det 準拠)
 * \details 出力するログの最低重要度を定義する。
 *          LogLevel は数値が小さいほど重要度が高い (E=0 が最重要)。
 *          DET_LOG_LEVEL 以下の数値 (＝同等以上に重要) のログのみ出力する。
 *
 *          例: DET_LOG_LEVEL を LOG_I にすると ERROR/WARN/INFO のみ出力し、
 *              DEBUG は抑制する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DET_CFG_H
#define DET_CFG_H

#ifndef DET_LOG_LEVEL
#  define DET_LOG_LEVEL  LOG_I  /**< 既定値: ERROR/WARN/INFO を出力、DEBUG を抑制 */
#endif

#endif /* DET_CFG_H */

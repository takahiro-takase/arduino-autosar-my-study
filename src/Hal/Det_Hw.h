/**
 * \file    Det_Hw.h
 * \brief   Det ハードウェア依存層 内部インタフェース
 * \details Det.c (純粋 C, レベル判定・メッセージ整形層) と、実際の出力先
 *          (Arduino Serial) との境界を定義する。Det.c はこのヘッダ経由でのみ
 *          出力を行い、Arduino API を直接知らない。本ヘッダは Det.c と
 *          Det_Hw.cpp 以外からインクルードしないこと。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DET_HW_H
#define DET_HW_H

#include "Det.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   "[<ms>ms] LEVEL TAG: msg\r\n" 形式で 1 行出力する。
 *
 * \param[in]  lvl    ログレベル（出力する文字列 ERROR/WARN /INFO /DEBUG の選択に使う）。
 * \param[in]  tag_P  PROGMEM ポインタ（RAM へコピーせず直接出力する）。
 * \param[in]  msg    整形済みメッセージ本文（Det.c 側で vsnprintf 済み）。
 */
void Det_Hw_PrintLogLine(LogLevel lvl, PGM_P tag_P, const char* msg);

/** \brief  "[<ms>ms] DET M=.. I=.. API=0x.. ERR=0x.." 形式で 1 行出力する。 */
void Det_Hw_PrintDetError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);

#ifdef __cplusplus
}
#endif

#endif /* DET_HW_H */

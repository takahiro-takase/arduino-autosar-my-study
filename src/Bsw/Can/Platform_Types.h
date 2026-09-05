/**
 * \file    Platform_Types.h
 * \brief   プラットフォーム依存型定義 (AUTOSAR Platform_Types)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>
#include <stddef.h>   /* NULL */
#include <stdbool.h>  /* boolean 型の実体（下記コメント参照） */

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;

/**
 * \brief   AUTOSAR Platform_Types の boolean 型。
 *
 * \note    `docs/autosar/4.3.1/` に Platform_Types 単体の仕様書 PDF が無いため、
 *          具体的な要求番号(SWS_Platform_xxxxx)は本コメントに記載しない
 *          （ISO 14229-1 の subFunction 番号を記憶だけで実装して誤った過去の
 *          教訓を踏まえ、ローカルに裏取りできない番号を記憶だけで書かない
 *          方針）。`boolean`/`TRUE`/`FALSE` という識別子自体は AUTOSAR
 *          Platform_Types の公知の定義（実仕様は `unsigned char` ベース）。
 *
 * \details 実仕様は `unsigned char`(TRUE=1/FALSE=0)ベースの独自型だが、
 *          本プロジェクトは C99/C++ 標準の `bool`(`<stdbool.h>`)を実体に
 *          採用する。理由: 本プロジェクトがターゲットにする Arduino UNO R4
 *          用フレームワーク(`framework-arduinorenesas-uno`)の
 *          `cores/arduino/api/Common.h` が既に `typedef bool boolean;` を
 *          定義しており、`main.cpp`/`src/Hal/*.cpp`（`<Arduino.h>` を直接
 *          include する唯一の層）はこの定義とも同時にコンパイルされる。
 *          実体を `unsigned char` にすると、これらのファイルで
 *          「同名だが実体が異なる」再定義エラーになる。`bool` に合わせれば
 *          「同名かつ同一実体」の冗長な再宣言（C++ 上合法）となり衝突しない。
 *          2026-09-05、この構成で native/uno_r4 双方のビルドを実地検証済み。
 */
typedef bool boolean;
#define TRUE  true
#define FALSE false

#endif

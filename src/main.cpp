/**
 * \file    main.cpp
 * \brief   Arduino エントリポイント・ECU 起動
 *
 * \details EcuM (ECU ステートマネージャ) を通じて BSW スタック全体を
 *          起動・運転する。main.cpp は EcuM.h だけをインクルードすればよく、
 *          個々の BSW モジュール（Can / CanIf / PduR / Com）を
 *          直接参照しない。Arduino 固有の Serial 初期化だけ
 *          setup() に残し、それ以外はすべて EcuM へ委譲する。
 *
 *          HW ウォッチドッグ起因のブートループ対策:
 *            WdgM が <avr/wdt.h> の実ハードウェアウォッチドッグを使用するため、
 *            setup() の最初に MCUSR クリア + wdt_disable() を行う
 *            （Arduino/AVR の定番パターン）。これを怠ると、短いタイムアウトで
 *            WDT が有効なまま再起動した場合に、ブートローダの待機中に再度
 *            タイムアウトしてスケッチに到達できない無限リセットに陥る恐れがある。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include <avr/wdt.h>
#include "EcuM.h"

// -------------------------------------------------------
// Arduino setup()
// -------------------------------------------------------
void setup()
{
    /* ブートローダ起因の WDT 無限リセットループを防ぐため、
     * 何よりも先に MCUSR をクリアして WDT を無効化する。
     * WdgM_Init() が後で必要なタイムアウトで再度有効化する。 */
    MCUSR = 0;
    wdt_disable();

    Serial.begin(115200);
    EcuM_Init();
}

// -------------------------------------------------------
// Arduino loop()
// -------------------------------------------------------
void loop()
{
    EcuM_MainFunction();
}

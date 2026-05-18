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
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include "EcuM.h"

// -------------------------------------------------------
// Arduino setup()
// -------------------------------------------------------
void setup()
{
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

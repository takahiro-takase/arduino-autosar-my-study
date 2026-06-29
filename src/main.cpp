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
 *            MCUSR は読み取り後にクリアする必要があるため、クリア前の値を
 *            一時保存しておき、Serial 初期化後にリセット原因として
 *            ログ出力する（バスオフ/WDT リセット調査用の診断ログ。
 *            project_busoff_watchdog_reset_bug 参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include "EcuM.h"
#include "Det.h"

#define TAG "Main"

// -------------------------------------------------------
// Arduino setup()
// -------------------------------------------------------
void setup()
{
    /* ブートローダ起因の WDT 無限リセットループを防ぐため、
     * 何よりも先に MCUSR をクリアして WDT を無効化する。
     * WdgM_Init() が後で必要なタイムアウトで再度有効化する。
     * MCUSR は読み取り後に必ずクリアすること（クリアしないと次回リセット時に
     * 古い値が残り原因判定を誤る）。そのため、クリアする直前の値を保存して
     * おき、Serial が使えるようになった後でログ出力する。 */
    const uint8_t resetFlags = MCUSR;
    MCUSR = 0;
    wdt_disable();

    Serial.begin(115200);

    /* リセット原因の診断ログ (バスオフ/WDT リセット調査用):
     * bit3 WDRF=ウォッチドッグリセット、bit2 BORF=低電圧検出リセット、
     * bit1 EXTRF=外部(RESETボタン)リセット、bit0 PORF=電源投入リセット。
     * 複数同時に立つこともある（例: 電源投入直後は PORF のみが通常だが、
     * 環境によっては BORF も同時に立つことがある）。 */
    DET_LOGI(TAG, "MCUSR=0x%02X (WDRF=%u BORF=%u EXTRF=%u PORF=%u)",
             (unsigned)resetFlags,
             (unsigned)((resetFlags >> 3) & 1U),
             (unsigned)((resetFlags >> 2) & 1U),
             (unsigned)((resetFlags >> 1) & 1U),
             (unsigned)(resetFlags & 1U));

    /* NvM CRC 動作確認用 (一時的な動作確認コード):
     * コメントを外して再アップロードすると、NvM_Init() が EEPROM を読み込む
     * 直前に DEM_AGING ブロックの先頭バイトを直接破壊する。
     * NvM: "CRC mismatch" -> "defaults restored" ログが出ることを確認できる。
     * 確認後は必ずコメントアウトに戻して再アップロードすること。 */
    // eeprom_write_byte((uint8_t*)0x0BU, 0xFFU);

    EcuM_Init();
}

// -------------------------------------------------------
// Arduino loop()
// -------------------------------------------------------
void loop()
{
    EcuM_MainFunction();
}

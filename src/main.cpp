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
 *            WdgM が実ハードウェアウォッチドッグを使用するため、
 *            setup() の最初に Mcu_Hw 経由でリセット原因取得 + WDT 無効化を行う
 *            （Arduino/AVR の定番パターンを MCU 非依存の形でラップしたもの）。
 *            これを怠ると、短いタイムアウトで WDT が有効なまま再起動した場合に、
 *            ブートローダの待機中に再度タイムアウトしてスケッチに到達できない
 *            無限リセットに陥る恐れがある。リセット原因はクリア前の値を
 *            Serial 初期化後にログ出力する（バスオフ/WDT リセット調査用の診断ログ。
 *            project_busoff_watchdog_reset_bug 参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include "EcuM.h"
#include "Det.h"
#include "Mcu_Hw.h"

#define TAG "Main"

// -------------------------------------------------------
// Arduino setup()
// -------------------------------------------------------
void setup()
{
    /* ブートローダ起因の WDT 無限リセットループを防ぐため、
     * 何よりも先にリセット原因を読み取り(レジスタはクリアされる)、WDT を
     * 無効化する。WdgM_Init() が後で必要なタイムアウトで再度有効化する。
     * クリア前の値は Mcu_Hw_ReadAndClearResetReason() が返すので、
     * Serial が使えるようになった後でログ出力する。 */
    const Mcu_Hw_ResetReasonType resetReason = Mcu_Hw_ReadAndClearResetReason();
    Mcu_Hw_DisableWatchdogAtBoot();

    Serial.begin(115200);

    /* USB CDC (ネイティブ USB シリアル) は UART-USB 変換チップと異なり、
     * ホスト側がポートを開いて接続を確立するまで送信データをバッファしない。
     * このため Serial.begin() 直後にログを出すと、シリアルモニタの接続が
     * 間に合わずリセット直後のログを取りこぼす。最大 3 秒だけ接続を待つが、
     * 本機は PC 非接続でも車載 ECU として単体起動できる必要があるため、
     * 無条件に待ち続けはせずタイムアウトで打ち切って起動を継続する。 */
    const unsigned long serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart) < 3000U)
    {
        /* 接続待ち */
    }

    /* `Serial` (DTR 相当の tud_cdc_connected()) が true になった瞬間は、
     * OS のポートハンドルが確立されただけで、ホスト側のモニタアプリが
     * 実際に読み取りを開始しているとは限らない。特に書き込み直後の
     * USB 再列挙では、この2つのタイミングにずれが生じやすく、直後に
     * 出力した行だけが失われる。モニタが読み取りを開始するまでの
     * 猶予として追加で待つ (PC 非接続時もここは通過するだけなので
     * 起動遅延は許容範囲)。 */
    delay(1000);

    /* リセット原因の診断ログ (バスオフ/WDT リセット調査用)。
     * 複数同時に立つこともある（例: 電源投入直後は PowerOn のみが通常だが、
     * 環境によっては BrownOut も同時に立つことがある）。
     * External は MCU によっては検出できない (Mcu_Hw.h 参照)。 */
    DET_LOGI(TAG, "ResetReason WDT=%u BOR=%u EXT=%u POR=%u",
             (unsigned)resetReason.Watchdog,
             (unsigned)resetReason.BrownOut,
             (unsigned)resetReason.External,
             (unsigned)resetReason.PowerOn);

    EcuM_Init();
}

// -------------------------------------------------------
// Arduino loop()
// -------------------------------------------------------
void loop()
{
    EcuM_MainFunction();
}

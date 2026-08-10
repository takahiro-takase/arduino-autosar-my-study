# Mcu

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

`main.cpp` の `setup()` 冒頭（`Serial.begin()` より前）で `Mcu_Init()` を呼び、
起動直後のリセット原因（Watchdog/BrownOut/External/PowerOn）を一度だけ読み取って
キャッシュする（Mcu_Hw のレジスタ読み取りは 1 起動につき 1 回しか呼べないため）。
`Mcu_InitClock`/`Mcu_SetMode`/`Mcu_InitRamSection`/`Mcu_PerformReset` 等は
Arduino フレームワークがクロック初期化を担い複数電源モードもモデル化しないため
未実装。`Mcu_GetResetReason()`（単一の `Mcu_ResetType`）に加え、複数要因の同時
検出を診断できるよう `Mcu_GetResetRawValue()`（4 フラグをビット詰めした本
プロジェクト独自の生値）も提供する。

## Mcu_Hw（下位ドライバ実装）

リセット要因の読み取り（Renesas RA RSTSR0-1）・起動時ウォッチドッグ無効化。

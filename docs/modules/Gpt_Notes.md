# Gpt

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

HW タイマ（Renesas RA FspTimer）による周期割り込み駆動の General Purpose Timer
Driver。目標時間到達判定は HW コンペアマッチではなく `Gpt_OnTick()` 内の
ソフトウェア比較で行い（`GetTimeElapsed`/`GetTimeRemaining` を単純な整数演算で
正確に実現するため）、`Gpt_EnableNotification` された通知関数は ISR コンテキストから
直接呼ばれる。2 チャネル構成: Channel 0 は `App_GptDemo` の動作確認用
（1Hz Notification）、Channel 1 は Os 専用のスケジューラティック（Notification なし、
`Os` が `Gpt_GetTimeElapsed()` をポーリング。詳細は [`EcuM_Notes.md`](./EcuM_Notes.md)
の「Os のスケジューラティック」参照）。`Gpt_SetMode`/`Gpt_EnableWakeup`/
`Gpt_DisableWakeup`/`Gpt_CheckWakeup`/`Gpt_GetPredefTimerValue` は、EcuM が
SLEEP モードを持たないため仕様上のプリコンパイル設定（`GptWakeupFunctionalityApi` 等）
に沿って未実装。

## Gpt_Hw（下位ドライバ実装）

Renesas RA `FspTimer` ラッパー。`FspTimer::get_available_timer()` で AGT/GPT の
空きチャネルを実行時に自動選択し、`setup_overflow_irq()` で周期割り込みを有効化する。
`Gpt.c` と `Gpt_Hw.cpp` 以外からはインクルードしない内部境界。

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

## 開発の経緯（実機で見つかった不具合・設計変更）

> 現在の仕様を理解するだけなら読む必要はありません。実機検証で見つかった
> 不具合や、その結果としての設計変更の経緯を時系列でまとめています。

### Power-On Reset フラグ (PORF) が実機で検出されない（未解決）

Mcu モジュール導入（`Mcu_Init()`/`Mcu_GetResetRawValue()`）の実機検証中、
USB ケーブルの抜き差し（真の電源断→再投入）を行っても、診断ログ
（`ResetReason WDT=%u BOR=%u EXT=%u POR=%u`）の POR が常に 0 のままである
ことが判明した。この事象自体は Mcu モジュール導入以前から存在していた
（本セッション最初期の検証ログの時点で既に `POR=0` だった）ため、
Mcu.c/Mcu.h 側の新規ロジック（Mcu_Init() の呼び出し順序修正・
MCU_RAW_RESET_*_BIT の一元化等）が原因ではなく、`src/Hal/Mcu_Hw.c` の
`Mcu_Hw_ReadAndClearResetReason()`（`R_SYSTEM->RSTSR0_b.PORF` を読む）
側の問題と考えられる。同ファイルは元々のコメントで「現時点でハードウェア
未検証」と明記されており、今回はその未検証事項が実機で顕在化した形。

有力な仮説（未検証）:
  - UNO R4 の Arduino コアにはブートローダが組み込まれており、リセット
    直後にまずこれが起動し「ダブルタップでブートローダモードへ入るか」
    等の判定のためにリセット原因レジスタを参照・クリアする可能性がある。
    その場合、スケッチの `setup()`（`Mcu_Init()`）に処理が渡る頃には
    PORF が既に消費された後になる。
  - あるいはボードの電源回路（レギュレータの保持容量等）により、USB
    抜き差しでも RA4M1 の VCC が完全に 0V まで落ちきらず、ハードウェア
    的に真の POR 条件を満たしていない可能性もある。

対応は見送り、事象の記録のみ行う（2026-08）。原因調査（ブートローダの
ソース確認、電源電圧の実測等）が必要であり、判明すれば別途対応する。

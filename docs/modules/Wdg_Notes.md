# Wdg

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

Renesas RA の実 HW ウォッチドッグ（IWDT、RA WDT ライブラリ経由）向け下位
ドライバ。`Wdg_SetMode(WDGIF_FAST_MODE)` で 4000ms タイムアウトを有効化する。
`Wdg_SetMode(WDGIF_OFF_MODE)` は常に `E_NOT_OK` を返す（IWDT は一度有効化すると
無効化する手段がないため。実 AUTOSAR の拡張プロダクションエラー
`WDG_E_DISABLE_REJECTED` に相当する状況）。WdgIf 経由でのみ呼ばれ、WdgM から
直接見えることはない。

## Wdg_Hw（下位ドライバ実装）

実 HW ウォッチドッグの Enable / Disable / Refresh ラッパー。`Wdg.c` と
`Wdg_Hw.cpp` 以外からはインクルードしない内部境界。

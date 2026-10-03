# Wdg

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

Renesas RA の実 HW ウォッチドッグ（IWDT、RA WDT ライブラリ経由）向け下位
ドライバ。`Wdg_SetMode(WDGIF_FAST_MODE)` で 4000ms タイムアウトを有効化する。
`Wdg_SetMode(WDGIF_OFF_MODE)` は常に `E_NOT_OK` を返す（IWDT は一度有効化すると
無効化する手段がないため。実 AUTOSAR の拡張プロダクションエラー
`WDG_E_DISABLE_REJECTED` に相当する状況）。WdgIf 経由でのみ呼ばれ、WdgM から
直接見えることはない。

## Wdg_SetTriggerCondition のタイムアウト値

[SWS_Wdg_00140] のとおり、`timeout = 0` は「トリガを止めて ECU を（ほぼ）即座に
リセットする」要求です。本実装は `Wdg_Hw_ForceReset()`（ソフトウェアリセット。IWDT は
有効化後に強制発火する手段がないため）を呼んでトリガを止め、以後の呼び出しは
何もしません（同仕様の「カウンタ値が 0 のときは無視する」に相当）。

`timeout` が 0 以外の場合は、HW が API 経由でのタイムアウト窓の動的変更に対応しない
ため、値によらず単純にリフレッシュします（学習用簡略化。`Wdg_Config.DefaultTimeoutMs`
を超える値は `WDG_E_PARAM_TIMEOUT`）。以前の実装は値によらずリフレッシュしていたため、
0 を渡すと逆にウォッチドッグがリフレッシュされていました。現在この API を `0` で
呼ぶ呼び出し元は `WdgM_PerformReset()` だけです（WdgM_PerformReset → `WdgIf_SetTriggerCondition(0)`
→ `Wdg_SetTriggerCondition(0)` → ソフトウェアリセット）。通常のリフレッシュは
`WDGM_HW_WATCHDOG_TIMEOUT_MS` を渡します。

## Wdg_Hw（下位ドライバ実装）

実 HW ウォッチドッグの Enable / Disable / Refresh ラッパー。`Wdg.c` と
`Wdg_Hw.cpp` 以外からはインクルードしない内部境界。

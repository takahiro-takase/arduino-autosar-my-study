# Fee

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

フラッシュエミュレーション EEPROM（Renesas RA `EEPROM.h`）向けの下位ドライバ。
`Fee_Write()` は物理アドレス・データ・長さを受け取ってジョブを開始するだけで
即座に返り、実際の書き込みは `Fee_MainFunction()` が 1 回の呼び出しにつき
1 バイトだけ進める（消去・書き込みサイクルによるブロッキングで WdgM の
Deadline Supervision を巻き込んだ実機不具合への対策）。MemIf 経由でのみ呼ばれ、
NvM から直接見えることはない。

## Fee_Hw（下位ドライバ実装）

フラッシュエミュレーション EEPROM 読み書き（Renesas RA `EEPROM` ライブラリ）
ラッパー。`Fee.c` と `Fee_Hw.cpp` 以外からはインクルードしない内部境界。

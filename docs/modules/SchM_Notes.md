# SchM

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

排他エリアマクロ（`SchM_Enter` / `SchM_Exit`）で共有リソースを保護。実体は
`SchM_Hw`（`noInterrupts()`/`interrupts()`）で、Can の割り込みペンディングフラグ、
および Gpt のチャネル状態機械（`Gpt_ChannelState`/`Gpt_ElapsedTicks`、実 HW
割り込みとメインループの両方から読み書きされる）を実際に保護する。

## SchM_Hw（下位ドライバ実装）

Arduino `noInterrupts()`/`interrupts()` ラッパー。

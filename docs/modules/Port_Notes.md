# Port（ピン方向設定）

> [README](../../README.md) の「[IO スタック](../../README.md#io-stack)」節から分離。

`Port_Init` が起動時に一度だけ、[`Dio_Notes.md`](./Dio_Notes.md) のチャネル割り当て表の
「Port 方向」列に従って D6/D7/D8 を OUTPUT、D9 を INPUT_PULLUP に設定します（A0 は
アナログ専用ピンのため Port 設定不要）。以降 Port 自身が呼ばれることはありません。

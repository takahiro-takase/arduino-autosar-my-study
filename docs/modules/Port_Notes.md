# Port（ピン方向設定）

> [README](../../README.md) の「[IO スタック](../../README.md#io-stack)」節から分離。

`Port_Init` が起動時に一度だけ、[`Dio_Notes.md`](./Dio_Notes.md) のチャネル割り当て表の
「Port 方向」列に従って D6/D7/D8 を OUTPUT、D9 を INPUT_PULLUP に設定します（A0 は
アナログ専用ピンのため Port 設定不要）。`Port_RefreshPortDirection`（AUTOSAR
[SWS_Port_00142] 準拠、同じ設定を再適用するのみ）も公開していますが、本プロジェクトの
本番コードからは呼び出していません（IF シグネチャ準拠のための追加、テストのみで検証）。

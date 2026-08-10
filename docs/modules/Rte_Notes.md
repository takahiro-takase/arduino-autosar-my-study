# Rte

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

ポートベース S/R API。複数 SW-C が同一シグナルを独立ポートで受信する。E2E
Transformer を持つ Read ポートは `Std_ReturnType` ではなく `Rte_IStatusType` を
返し、E2E チェック結果（OK/ハードエラー/ソフトエラー）と Com タイムアウトを
区別して SWC へ伝える。

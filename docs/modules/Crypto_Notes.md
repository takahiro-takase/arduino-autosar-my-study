# Crypto

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

Csm/CryIf/Crypto レイヤの最下層。鍵テーブル（`Crypto_PBCfg.c`）と実際の暗号計算
（AES-128-CMAC、自前実装）を保持する唯一のモジュール。`Crypto_ProcessJob()` が
ジョブの `service`（`CRYPTO_MACGENERATE`/`CRYPTO_MACVERIFY`）に応じて MAC を生成、
または定数時間比較で検証する（MAC 検証のタイミングサイドチャネル対策はここに実装。
旧実装では SecOC.c 内にあったロジックを責務として正しい層へ移設した）。

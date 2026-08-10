# CryIf

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

Csm と Crypto Driver の間のルーティング層。`CryIf_ProcessJob()`（実 AUTOSAR は複数
Crypto Driver Object への振り分けを担う）は、本プロジェクトが Crypto Driver を
1 個しか持たないため実質パススルーで `Crypto_ProcessJob()` へ委譲する（CanIf が
単一 CAN コントローラに固定しているのと同じ簡略化）。

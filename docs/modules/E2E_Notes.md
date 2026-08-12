# E2E

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。
> CRC/カウンタアルゴリズム自体の学習ノートは
> [`docs/E2E_Profile1_Notes.md`](./E2E_Profile1_Notes.md) /
> [`docs/E2E_Profile5_Notes.md`](./E2E_Profile5_Notes.md) を参照。
> Com/Rte への実際の適用は [`E2EXf_Notes.md`](./E2EXf_Notes.md) を参照。

AUTOSAR E2E Profile 05 保護の実処理。DataID・CRC16（多項式 0x1021）・8bit
カウンタの 3 要素で、`E2E_P05Check` はデータ破壊・フレーム脱落・重複・誤ルー
ティングを検出し、`E2E_P05Protect` は Counter・CRC16 を付加する。Com/Rte の
どちらにも依存しない純粋な検証/付与ライブラリ（Profile01 版の `E2E_P01.c` も
参考実装として残っている）。

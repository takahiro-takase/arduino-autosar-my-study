# Csm

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

SecOC が唯一直接呼ぶ暗号スタックの入口。`Csm_MacGenerate()`/`Csm_MacVerify()` が
`jobId`（`Csm_PBCfg.c` の `Csm_JobConfigData`）から実行すべきプリミティブ種別と
鍵 ID を解決し、`Crypto_JobType` ジョブを組み立てて `CryIf_ProcessJob()` へ委譲する。
`Csm_MacVerify()` の `macLength` はビット単位（[SWS_Csm_01050]）、
`Csm_MacGenerate()` の `macLengthPtr` はバイト単位（[SWS_Csm_00982]）という実仕様の
非対称性を踏襲し、Csm 内でビット→バイト変換する。`Csm_KeyElementSet()`/
`Csm_KeySetValid()`（KeyM が呼ぶ鍵操作 API）は CryIf へのパススルー。

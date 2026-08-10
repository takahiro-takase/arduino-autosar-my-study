# KeyM

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

鍵更新セッション（`KeyM_Start`→`KeyM_Update`→`KeyM_Finalize`）を管理する Key
Manager。Dcm の WriteDataByIdentifier（DID 0x0108 CryptoKeyUpdate、詳細は
[`Dcm_Notes.md`](./Dcm_Notes.md) 参照）が模擬鍵マスターとして駆動する。
Certificate submodule・SHE 形式等は対応除外（詳細は [`SecOC_Notes.md`](./SecOC_Notes.md)
の「明示する簡略化」節参照）。ModuleId は Release 4.3.1 の
AUTOSAR_TR_BSWModuleList.pdf に KeyM 自体が未掲載のため未検証の暫定値。

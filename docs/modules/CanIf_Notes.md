# CanIf

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

CAN ID ↔ 論理 PDU のマッピングを担う。上位層は CAN ID を知らず PDU ID で通信する。
設定 DLC 未満の受信 L-PDU は上位層へ渡さず棄却する（SWS_CANIF_00026 のデータ長
チェック）。

## CanIf_ReadRxPduData（受信データのポーリング取得、2026-08 追加）

`CanIf_RxIndication()` による上位層コールバックへのプッシュ配送とは別に、実
AUTOSAR は `CanIf_ReadRxPduData`（[SWS_CANIF_00194]）というポーリング取得
API も提供する。CanIf 自身が RX PDU ごとの直近受信データを内部バッファに
保持し、任意のタイミングで読み出せるようにする仕組み。本プロジェクトには
存在しなかったため追加した。

```
CanIf_RxPduConfigType.ReadRxPduDataEnabled = 1（opt-in、既定 0）
  ↓
CanIf_RxIndication()  ← 通常のプッシュ配送に加え、内部バッファ
                          （CanIf_RxPduDataBuffer[]、CANIF_RX_PDU_MAX 本）
                          へ受信データを複製する
  ↓
CanIf_ReadRxPduData(CanIfRxSduId, &info)  ← いつでもポーリングで取得可能
```

**`CanIfRxSduId` は `UpperLayerRxPduId` とは別の ID 空間**: 前者は
`CanIf_ConfigPtr->RxPduConfig[]` 上の位置（CanIf 内部ハンドル）、後者は
上位層（PduR/Com）が使う ID。両者が同じ値になるとは限らない。

opt-in にした理由・[SWS_CANIF_00324]（コントローラ状態チェック）を
省略した理由など、詳細な設計判断は `CanIf.c` の `CanIf_ReadRxPduData()`/
`CanIf_RxIndication()` の Doxygen コメント参照。

**この機能は実際に発動するか**: 本番設定への配線は行っていない
（`CanIf_PBCfg.c` の RX PDU はいずれも `ReadRxPduDataEnabled` 未設定
＝既定 0 のまま）。ユニットテストでのみ検証している
（`test/test_chain/Bsw_RxChain_test.cpp` の `CanIfReadRxPduData_*`）。

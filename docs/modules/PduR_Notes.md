# PduR

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

受信 PDU を Com/CanTp/SecOC へ（1つの RxPduId から複数宛先への配信にも対応）、
送信 PDU を CanIf へルーティングする、通信スタックの配管役。TX 経路は既定では
`CanIf_Transmit()` へ直接転送するが、`PduR_TxRoutingPathType.TransmitOverrideFct`
が設定されている場合は中間モジュール（SecOC）へ委譲できるよう汎用化されている
（既存の全 TX パスはこのフィールドを使わないため無変更）。

## ルーティング設定（`PduR_PBCfg.c`）

| 方向 | SrcPduId | 経路 | 備考 |
|------|----------|------|------|
| RX | 0 | CanIf（CAN 0x100）→ `Com_RxIndication`（DestPduId=0: EngineInfo） | Com のみ |
| RX | 1 | CanIf（CAN 0x7E0）→ `CanTp_RxIndication` | 診断要求（CanTp から Dcm へ） |
| RX | 2 | CanIf（CAN 0x110）→ `Com_RxIndication`（DestPduId=1: AbsInfo） | Com のみ |
| RX | 3 | CanIf（CAN 0x120）→ `SecOC_RxIndication`（DestPduId=0: ImmobilizerCmd） | SecOC が検証後、自ら `Com_RxIndication()` を呼ぶ |
| TX | 0 | `CanIf_Transmit(TxPduId=0)`（CAN 0x200 MeterStatus）、確認は `Com_TxConfirmation` | |
| TX | 1 | `CanIf_Transmit(TxPduId=1)`（CAN 0x7E8 診断応答）、確認は `CanTp_TxConfirmation` | |
| TX | 2 | `CanIf_Transmit(TxPduId=3)`（CAN 0x210 WarningStatus）、確認は `Com_TxConfirmation` | |
| TX | 3 | `CanIf_Transmit(TxPduId=4)`（CAN 0x220 E2EHealthStatus）、確認は `Com_TxConfirmation` | 以前は SecOC を経由していた（撤去済み） |
| TX | 4 | `CanIf_Transmit(TxPduId=5)`（CAN 0x230 ImmobilizerStatus）、確認は `Com_TxConfirmation` | Com の Signal Gateway が転送 |

`SrcPduId` は上位層（Com / CanTp）が使う ID 空間で、CanIf の PDU ID とは別です。
Com と CanTp は同じ `SrcPduId` の空間を共有するため、衝突しないよう値を割り当てています
（WarningStatus が 2、CanTp の応答が 1）。

## RX / TX / 送信確認の流れ

- **RX**（`PduR_ComRxIndication()`。CanIf からは別名 `PduR_CanIfRxIndication` として呼ばれる
  マクロ）: `SrcPduId` に一致する経路を探し、その経路に登録された**全ての宛先**の
  `RxIndFct` を順に呼びます（1 つの受信 PDU を複数の上位層へ配信できる）。一致する経路が
  なければ `PDUR_E_PDU_ID_INVALID` を報告します。
- **TX**（`PduR_ComTransmit()`、`PduR_CanTpTransmit()`。後者は前者と完全に同じ）: `SrcPduId`
  に一致する経路を探し、`TransmitOverrideFct` が設定されていればそちら（中間モジュール）へ、
  無ければ `CanIf_Transmit(CanIfTxPduId)` へ転送します。現在 `TransmitOverrideFct` を使う
  経路はありません（SecOC の TX を撤去したため。機構自体は学習用に残している）。
  `PduR_SecOCTransmit()` は SecOC が変換を終えたあとの送信用で、`TransmitOverrideFct` を
  再評価せず `CanIf_Transmit()` へ進みます（再帰を防ぐため）。
- **送信確認**（`PduR_CanIfTxConfirmation()`）: `SrcPduId` の経路の `ConfFct` を、`ConfDestPduId`
  を添えて呼びます。SecOC 経由の送信確認は `PduR_SecOCTxConfirmation()` が元の送信元へ中継します。

## エラーと簡略化

- `PduR_Init()` と `PduR_GetVersionInfo()` 以外は、未初期化なら `PDUR_E_UNINIT` を報告します
  （[SWS_PduR_00338]）。NULL のポインタは `PDUR_E_PARAM_POINTER` です。
- ゲートウェイ（PDU を別のバスへ中継する機能）、`PduR_*StartOfReception` / `CopyRxData` などの
  トランスポートプロトコル用の段階受理 API、`PduR_*TriggerTransmit` は実装していません
  （CanTp との連携は、CanTp が組み立て済みの PDU を渡す簡略化した形）。
- 単体テストからだけ使える `PduR_Test_ResetInitState()`（`PDUR_UNIT_TEST`）で、未初期化状態へ
  戻せます（他の BSW モジュールと同じ設計）。

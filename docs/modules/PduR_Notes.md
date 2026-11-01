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

## ルーティングパスグループ（`PduR_EnableRouting()` / `PduR_DisableRouting()`）

ルーティング経路をまとめて有効・無効にする仕組みです（[SWS_PduR_00615]、[SWS_PduR_00617]。実際の AUTOSAR では、BswM のルールが呼ぶ想定）。

- **設定**: `PduR_PBConfigType.RoutingPathGroups[]`（`PduRRoutingPathGroup`）。グループの識別子は、配列の添字です。グループごとに、起動時の状態（`IsEnabledAtInit`、[SWS_PDUR_00827]）と、所属する RX・TX 経路の `SrcPduId` の一覧を持ちます。
- **本番の設定は、グループを使いません**（`RoutingPathGroupCount` = 0）。どの経路も、常に有効です。コンフィギュレータ（`PduR.json`）も、まだグループを生成しません。
- **どの経路が有効か**: どのグループにも属さない経路は、常に有効です。属する経路は、属するグループのいずれかが有効なら有効で、すべて無効なら無効です。
- **無効な経路の扱い**（[SWS_PDUR_00805]、[SWS_PduR_00717]）: `PduR_ComTransmit()` / `PduR_SecOCTransmit()` は、DET へ報告せずに `E_NOT_OK` を返します。`PduR_ComRxIndication()` は、DET へ報告せず、PDU を捨てます。
- **送信確認は、無効の間も上位層へ伝えます**。仕様の一覧にない処理で、無効にする前に送った PDU の確認を失わないためです。
- **存在しないグループ**: 何もせず、`PDUR_E_ROUTING_PATH_GROUP_ID_INVALID` を DET へ報告します（[SWS_PduR_00647]〜[00649]）。
- **簡略化**: 仕様は、無効にするとき、単一バッファの既定値への戻し（`initialize` が TRUE のとき。[SWS_PDUR_00810]）と、FIFO の破棄（[SWS_PduR_00663]）を求めます。本実装は、ゲートウェイ用のバッファも FIFO も持たないので、`initialize` は使いません。

`PduR_GetConfigurationId()`（[SWS_PduR_00341]）は、`PduR_PBConfigType.PBConfigId`（既定は 0）を返します。ポストビルドで選択できる構成は 1 つだけです。

## トリガ送信（`PduR_CanIfTriggerTransmit()`）

下位層（CanIf）が送信の直前にデータを取りに来たとき（[SWS_PduR_00369]）、TX ルーティングパス
（`PduR_TxRoutingPathType`）の `TriggerTransmitFct`（Com なら `Com_TriggerTransmit()`）を、パスの
`ConfDestPduId`（上位層の PDU ID）付きで呼びます。上位層の戻り値は、そのまま下位層へ返します
（[SWS_PduR_00430]/[SWS_PduR_00661]）。`PduR_CanIfTxConfirmation()` と同じ ID 空間で経路を引きます。

- 無効なルーティングパスグループのパスは、DET へ報告せず `E_NOT_OK`（[SWS_PDUR_00805]）。
- `TriggerTransmitFct` を持たないパスは、トリガ送信に対応しないので `E_NOT_OK`（DET へは報告しません）。
- 一致するパスが無いときは `PDUR_E_PDU_ID_INVALID`、NULL のポインタは `PDUR_E_PARAM_POINTER`、
  未初期化は `PDUR_E_UNINIT` を報告して `E_NOT_OK` です。

SecOC を経由する経路（`TransmitOverrideFct` を持つ経路）は、上位層の PDU ID に、`ConfDestPduId` ではなく、
SecOC の TX PDU の ID（`TransmitOverrideId`）を渡します。この経路の `TriggerTransmitFct` は `SecOC_TriggerTransmit()` で、
SecOC が組み立てて保持している Secured I-PDU を、下位層へ複製します（上位層の Com へは転送しません。
[SWS_SecOC_00068]。[`SecOC_Notes.md`](./SecOC_Notes.md) の「トリガ送信」参照）。

本番の設定は、どのパスも `TriggerTransmitFct` を持たないので、本番コードからは呼ばれません。
全体の流れは、[`Com_Notes.md`](./Com_Notes.md) の「Com_TriggerTransmit」を参照してください。

## エラーと簡略化

- `PduR_Init()` と `PduR_GetVersionInfo()` 以外は、未初期化なら `PDUR_E_UNINIT` を報告します
  （[SWS_PduR_00338]）。NULL のポインタは `PDUR_E_PARAM_POINTER` です。
- ゲートウェイ（PDU を別のバスへ中継する機能）、`PduR_*StartOfReception` / `CopyRxData` などの
  トランスポートプロトコル用の段階受理 API は実装していません
  （CanTp との連携は、CanTp が組み立て済みの PDU を渡す簡略化した形）。
  トリガ送信（`PduR_CanIfTriggerTransmit()`）は、上位層の送信データを取りに行く経路だけを持ちます
  （ゲートウェイの FIFO やバッファを返す動作は、持ちません）。
- 単体テストからだけ使える `PduR_Test_ResetInitState()`（`PDUR_UNIT_TEST`）で、未初期化状態へ
  戻せます（他の BSW モジュールと同じ設計）。

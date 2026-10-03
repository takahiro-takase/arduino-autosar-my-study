# CanIf

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

CAN ID ↔ 論理 PDU のマッピングを担う。上位層は CAN ID を知らず PDU ID で通信する。
設定 DLC 未満の受信 L-PDU は上位層へ渡さず棄却する（SWS_CANIF_00026 のデータ長
チェック）。

## PDU の対応表（`CanIf_PBCfg.c`）

| 方向 | CanIf の PDU ID | CAN ID | DLC | 上位層 |
|------|----------------|--------|-----|--------|
| TX | 0 | 0x200 | 6 | MeterStatus（Com、PduR 経由） |
| TX | 1 | 0x7E8 | 8 | UDS 診断応答（CanTp、PduR 経由） |
| TX | 2 | 0x400 | 2 | NM フレーム（CanNm が `CanIf_Transmit()` を直接呼ぶ。PduR/Com を経由しない） |
| TX | 3 | 0x210 | 1 | WarningStatus（Com） |
| TX | 4 | 0x220 | 5 | E2EHealthStatus（Com、E2E Profile05 保護） |
| TX | 5 | 0x230 | 1 | ImmobilizerStatus（Com、Signal Gateway の転送先） |
| RX | 0 | 0x100 | 7 | EngineInfo → PduR → Com |
| RX | 1 | 0x7E0 | 8 | UDS 診断要求 → PduR → CanTp |
| RX | 2 | 0x110 | 6 | AbsInfo → PduR → Com |
| RX | 3 | 0x120 | 6 | ImmobilizerCmd（SecOC 保護）→ PduR → SecOC → Com |
| RX | 4 | 0x400 | 2 | NM フレーム → `CanNm_RxIndication()`（PduR/Com を経由しない） |

DLC の値は `CanIf_PBCfg.c` の設定どおりです（RX の DLC は受信長チェックの基準でもある）。
送信 PDU 数・受信 PDU 数の上限は `CANIF_TX_PDU_COUNT`（6）・`CANIF_RX_PDU_COUNT`（5）です。

## 送受信の流れ

- **TX（`CanIf_Transmit()`）**: TxPduId の範囲・ポインタ・長さ（設定 DLC を超える長さは拒否）を確認し、
  **PDU モードが `CANIF_ONLINE` のときだけ** `Can_Write()` まで進めます（それ以外は `E_NOT_OK`）。
  `Can_Write()` が `CAN_OK` なら `E_OK`（`CAN_BUSY` は警告ログ付きで `E_NOT_OK`）。
- **RX（`CanIf_RxIndication()`）**: まず**どの CAN ID であっても** `CanSM_RxIndication()` を呼びます
  （ウェイクアップ検証中に「何かフレームを受信できた」ことを CanSM が知るため）。その後、
  HRH と CAN ID が一致する RX PDU を探し、設定 DLC 未満なら（`CANIF_E_INVALID_DATA_LENGTH` を
  ランタイムエラーとして報告して）棄却、足りていれば上位層の `RxIndicationFct`（PduR）へ渡します。
  一致する PDU が無いときは、HRH が一致していれば `CANIF_E_PARAM_CANID`、一致していなければ
  `CANIF_E_PARAM_HOH` を報告します。未初期化のときは何も報告せず黙って何もしません
  （[SWS_CANIF_00421]）。
- **TX 確認（`CanIf_TxConfirmation()`）**: Can ドライバの `Can_MainFunction_Write()` から呼ばれ、
  その PDU の通知状態と、コントローラの TX 確認状態（STARTED のときだけ）を更新して、
  上位層の `TxConfirmFct`（PduR）を `E_OK` で呼びます。
- **Bus-Off（`CanIf_ControllerBusOff()`）**: `CanSM_ControllerBusOff()` へ委譲するだけです。
- **コントローラモード（`CanIf_SetControllerMode()`）**: 要求を `Can_SetControllerMode()` の遷移
  （START / STOP / SLEEP、SLEEP からの STOPPED 要求は WAKEUP）へ変換し、成功したら現在のモードを
  記録します（CanSM が使う入口）。STARTED または STOPPED へ遷移したときは TX 確認状態をリセットします。

## PDU モード（`CanIf_SetPduMode()`）

| モード | TX | RX（上位層への通知） |
|--------|----|--------------------|
| `CANIF_OFFLINE`（初期値） | 拒否 | 継続（ゲートしない） |
| `CANIF_TX_OFFLINE` | 拒否 | 継続 |
| `CANIF_ONLINE` | 許可 | 継続 |

CanSM が SILENT_COMMUNICATION で `CANIF_TX_OFFLINE`、FULL_COMMUNICATION で `CANIF_ONLINE` に切り替えます
（[`CanSM_Notes.md`](./CanSM_Notes.md)）。**実装では TX だけを制御し、RX は PDU モードでは抑止しません**
（SILENT_COM 中も CanNm/Com の受信処理を続けたいため。`CANIF_OFFLINE` でも受信は止まらない）。
実仕様の `CANIF_TX_OFFLINE_ACTIVE`（送信を偽装して確認コールバックだけ返すモード）は、Com の
送信確認待ち管理と両立しないため実装していません。

## 通知状態の読み出し API

`CanIf_ReadTxNotifStatus()` / `CanIf_ReadRxNotifStatus()` は、その PDU の送信確認・受信が
あったかを返し（読むとクリア）、`CanIf_GetTxConfirmationState()` はコントローラが直近の起動以降に
一度でも TX 確認を受けたかを返します（読んでもクリアしない）。いずれも本番コードからは呼ばれて
いません（ユニットテストのみ）。

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
（`test/Bsw/ComStack/Bsw_ComStack_Signal_Rx_test.cpp` の `CanIfReadRxPduData_*`）。

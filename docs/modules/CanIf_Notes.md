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

## 未初期化の API 呼び出し（`CANIF_E_UNINIT`）

`CanIf_Init()` と `CanIf_GetVersionInfo()` 以外の全 API は、`CanIf_Init()` の前に呼ばれると、通常の動作を
実行せずに戻り（[SWS_CANIF_00661]。戻り値のある API は、`E_NOT_OK` や `CANIF_NO_NOTIFICATION`）、
DET へ `CANIF_E_UNINIT`（値 30）を報告します（[SWS_CANIF_00901]）。各 API の先頭の判定
（`CanIf_ConfigPtr == NULL`）から、`CanIf_ReportUninit()` を呼びます。`CanIf_DeInit()` も対象で、
すでに未初期化のときの呼び出しを報告します。以前は、DET へ報告せず黙って戻っていました。
本番では、コールバック（`CanIf_RxIndication()`、`CanIf_TxConfirmation()`、`CanIf_ControllerBusOff()` など）が
未初期化で呼ばれることはありません。理由は次のとおりです。

- 呼び出し元の `Can_MainFunction_Read/Write/BusOff()` は、`EcuM_Init()` の完了後に、メインループ（`loop()`）の
  タスクとして動きます。
- コントローラが STARTED になるのは、`CanIf_SetControllerMode()`（`CanIf_Init()` の後に CanSM が呼ぶ）の
  経由だけで、Bus-Off の検出と RX の読み出しは、STARTED のときだけ動きます。
- `Can_Isr()`（`Can_Init()` で登録するハードウェア割り込み）は、フラグを立てるだけで、CanIf を呼びません。
- 本番では `CanIf_DeInit()` を呼びません。

## 送受信の流れ

- **TX（`CanIf_Transmit()`）**: TxPduId の範囲・ポインタ・長さ（設定 DLC を超える長さは拒否）を確認し、
  **PDU モードが `CANIF_ONLINE` のときだけ** `Can_Write()` まで進めます（それ以外は `E_NOT_OK`）。
  `Can_Write()` が `CAN_OK` なら `E_OK`（`CAN_BUSY` は警告ログ付きで `E_NOT_OK`）。
- **RX（`CanIf_RxIndication()`）**: まず**どの CAN ID であっても**、最初の受信を「起床後の最初の受信」として記憶します
  （ウェイクアップ検証用。EcuM が `CanIf_CheckValidation()` で確認し、記憶があれば
  `EcuM_ValidateWakeupEvent()` で検証済みを伝えます。スリープへ入ると記憶は消えます）。その後、
  HRH と CAN ID が一致する RX PDU を探し、設定 DLC 未満なら（`CANIF_E_INVALID_DATA_LENGTH` を
  ランタイムエラーとして報告して）棄却、足りていれば上位層の `RxIndicationFct`（PduR）へ渡します。
  一致する PDU が無いときは、HRH が一致していれば `CANIF_E_PARAM_CANID`、一致していなければ
  `CANIF_E_PARAM_HOH` を報告します。未初期化のときは何もせず、`CANIF_E_UNINIT` を報告します
  （[SWS_CANIF_00421]/[SWS_CANIF_00901]）。
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

## CanIf_CancelTransmit（ダミー関数）

仕様は本 API を、PduR がバスに依存しない動作をするためだけに呼ぶ「機能を持たない関数」と定めています
（[SWS_CANIF_00520]）。TxPduId が範囲外なら `CANIF_E_INVALID_TXPDUID` を DET へ報告し（[SWS_CANIF_00652]）、
それ以外は常に `E_NOT_OK`（取り消しの拒否）を返します。CAN ドライバへ取り消しを伝える手段は無く、
送信済みのフレームを取り消すこともありません。構成項目 `CanIfPublicCancelTransmitSupport` による
有効・無効の切り替えは持たず、常に有効です。上位（`PduR_<User:Up>CancelTransmit()`、`Com_CancelTransmit` 相当）は
未実装で、本番コードからは呼ばれていません。

## CanIf_ControllerModeIndication（コントローラのモード遷移の通知）

CAN ドライバがコントローラの遷移の完了を知らせるコールバックで、CanIf は `CanSM_ControllerModeIndication()` へ
そのまま渡します（[SWS_CANIF_00711]）。CanIf が未初期化なら通知せず（[SWS_CANIF_00702]）、`CANIF_E_UNINIT` を報告し、
`ControllerId` が範囲外なら `CANIF_E_PARAM_CONTROLLERID` を報告して通知しません（[SWS_CANIF_00700]）。

**本番コードからは呼ばれません。** 本プロジェクトの `Can_SetControllerMode()` は要求の中で同期的に遷移を終え、
`CanIf_SetControllerMode()` が成功の時点で状態を更新するので、Can が通知を返す必要がありません。
起床の検出は、`EcuM_CheckWakeup()` が `CanSM_ControllerModeIndication()` を直接呼ぶ経路です
（[`CanSM_Notes.md`](./CanSM_Notes.md)）。本関数は `CanIf_ControllerMode[]` を更新しません
（`CanIf_SetControllerMode()` の側にある、起床時の通知・送信確認の状態などの付随処理と、ちぐはぐになるため）。

## CanIf_TriggerTransmit（送信データの要求の転送）

CAN ドライバが送信の直前にデータを取りに来たとき、TX PDU 設定（`CanIf_TxPduConfigType`）の
`TriggerTransmitFct`（PduR 経由なら `PduR_CanIfTriggerTransmit()`）を、`UpperLayerTxPduId` へ読み替えた
PDU ID 付きで呼び、その戻り値をそのまま返します（[SWS_CANIF_00885]）。`TriggerTransmitFct` が NULL の
TX PDU は、トリガ送信に対応しないので `E_NOT_OK` です（[SWS_CANIF_00888]。DET へは報告しません）。
`TxPduId` が範囲外なら `CANIF_E_INVALID_TXPDUID`、NULL のポインタは `CANIF_E_PARAM_POINTER` を報告します。
未初期化は、他の API と同じく、`CANIF_E_UNINIT` を報告して `E_NOT_OK` です。

**本番コードからは呼ばれません**: 本プロジェクトの Can ドライバ（MCP2515）は、送信の直前にデータを
取りに来ません。`Can_Write()` へデータを渡さずに送信を要求する経路もないので、`CanIf_Transmit()` が
NULL の `SduDataPtr` を受け付ける動作（[SWS_CANIF_00882]）も未実装です。構成項目
`CanIfTriggerTransmitSupport` による有効・無効の切り替えは持たず、TX PDU ごとの `TriggerTransmitFct` の
有無だけで決めます。全体の流れは、[`Com_Notes.md`](./Com_Notes.md) の「Com_TriggerTransmit」を参照してください。

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
（`test/CallChain/ComStack/ComStack_Signal_Rx_test.cpp` の `CanIfReadRxPduData_*`）。

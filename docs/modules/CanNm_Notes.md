# CanNm（ネットワークマネジメント）

> [CAN 通信スタック 詳細](../can_stack.md) の「[CAN 通信状態管理](../can_stack.md#can-comm-management)」節から分離
> （旧「ECU 管理層」節から移動。実 AUTOSAR では EcuM/BswM/WdgM とは別クラスタ
> [Communication Services] に属するため）。

CanNm (Network Management) は、実車の各 ECU がバス上に周期的な生存確認フレーム
（NM フレーム）を送信し、クラスタ内の全 ECU が送信を止めたときにのみバス
スリープへ移行できる、という合意形成（協調スリープ）の仕組みです。
CanNm 状態機械（Repeat Message/Normal Operation/Ready Sleep/Prepare
Bus-Sleep/Bus-Sleep）の状態機械図・タイマ設定・ComM/CanSM との連携・ログ例の
詳細を以下にまとめます。
本プロジェクトの `CanNm.c` は `docs/4.3.1/AUTOSAR_SWS_CANNetworkManagement.pdf`
の CanNm 状態機械をほぼそのまま実装しており、他ノード（uds_tester が模擬する
「仮想他ECU」）からの NM フレーム受信が自ノードのスリープ判断に反映される
ことを実機で確認できます。

## 状態機械

```
Bus-Sleep Mode ─────────CanNm_NetworkRequest()/RxIndication(Prepare Bus-Sleep中)──┐
     ↑ Wait-Bus-Sleep Timer満了                                                │
Prepare Bus-Sleep Mode                                                         │
     ↑ NM-Timeout Timer満了(Ready Sleepから)                                   ▼
Network Mode: Ready Sleep State ←─CanNm_NetworkRelease()── Normal Operation State
     │  (送信停止)                                              ↑ (送信継続)
     └──────Repeat Message Time満了(要求あり=Normal Operationへ)─┘
                        ↑
              Repeat Message State（Network Mode への進入は必ずここを経由）
```

図に無い遷移（実装済み）:

| 契機 | 遷移 |
|------|------|
| `CanNm_NetworkRequest()`（Bus-Sleep / Prepare Bus-Sleep 中） | → Repeat Message State |
| `CanNm_NetworkRequest()`（Ready Sleep 中） | → Normal Operation State（[SWS_CanNm_00110]） |
| `CanNm_NetworkRelease()`（Normal Operation 中） | → Ready Sleep State（[SWS_CanNm_00118]）。Repeat Message 中は要求フラグを下ろすだけで、Repeat Message Time 満了時に Ready Sleep へ進む |
| `CanNm_RepeatMessageRequest()`（Ready Sleep / Normal Operation 中） | → Repeat Message State。送信する NM フレームの Repeat Message Request ビットを立てる（[SWS_CanNm_00112]/[00120]）。Repeat Message / Prepare Bus-Sleep / Bus-Sleep 中は `E_NOT_OK` で拒否 |
| 他ノードの NM フレーム受信（Prepare Bus-Sleep 中） | → Repeat Message State（[SWS_CanNm_00124]） |
| 他ノードの NM フレーム受信で Repeat Message Request ビットが立っている（Normal Operation / Ready Sleep 中） | → Repeat Message State（再アナウンス） |

3つのタイマ（`CanNm_Cfg.h`）で駆動します。

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `CANNM_TIMEOUT_MS` | 3000 ms | NM-Timeout Timer。送信成功確認/受信のたびに再起動される「他ノードを含め通信が生きているか」の監視タイマ。Ready Sleep State でこれが満了すると Prepare Bus-Sleep Mode へ遷移。Repeat Message/Normal Operation State での満了は本来 Bus-Off 等の異常時にのみ起こる想定（[SWS_CanNm_00193]/[SWS_CanNm_00194]。再送信は伴わず、タイマ再起動と DET 報告のみ） |
| `CANNM_REPEAT_MESSAGE_MS` | 1500 ms | Repeat Message State の滞在時間 |
| `CANNM_WAIT_BUS_SLEEP_MS` | 1500 ms | Prepare Bus-Sleep Mode の滞在時間 |
| `CANNM_CYCLE_MS` | 1000 ms | `CanNm_MainFunction()` の呼び出し周期。実 CanNm の Message Cycle Timer（`CanNmMsgCycleTime`、[SWS_CanNm_00032]/[SWS_CanNm_00040]。NM-Timeout Timer とは独立に Repeat Message/Normal Operation State の周期送信を駆動する専用タイマ）をこの呼び出し周期自体で兼用する簡略化 |

時刻は `GetCounterValue(SYSTEM_COUNTER)`（Os のカウンタ、[`EcuM_Notes.md`](./EcuM_Notes.md) 参照）から取得します。
`CanNm_MainFunction()` は Os タスク（1000 ms 周期）で、BswM のどのモードでも無効化されません
（SHUTDOWN 中も動かし続けるタスク。協調スリープを進めるため）。

Message Cycle Timer と NM-Timeout Timer は独立している点に注意してください。
健全な通信中は毎周期の送信成功が NM-Timeout Timer を先回りして再起動し続ける
ため、`CANNM_E_NETWORK_TIMEOUT` は通常発生しません（実装当初この分離を誤り、
NM-Timeout Timer 満了そのものを再送信のトリガとしてしまっていたため、健全時
でも約 `CANNM_TIMEOUT_MS`〜`CANNM_TIMEOUT_MS+CANNM_CYCLE_MS` ごとに誤って
`CANNM_E_NETWORK_TIMEOUT` が発生し続け、かつそのせいで Ready Sleep State 進入
時点でタイマが既に古くなっており Prepare Bus-Sleep Mode へ異常に早く遷移する、
という2つの不具合が実機ログから見つかり修正した経緯があります）。

**対応除外**（実 AUTOSAR CanNm が持つが本プロジェクトでは実装しない機能）:
Partial Networking（7.11章）、NM Coordinator Sync（7.9.7章。`CanNm_RequestBusSynchronization` / `CanNm_SetSleepReadyBit`）、User Data
（7.9.2章。`CanNm_GetPduData`）、Remote Sleep Indication（`CanNm_CheckRemoteSleepIndication`、
7.9.1章）、Passive Mode（7.9.3章。`CanNm_PassiveStartUp`）。これらの関数は `CanNm.c` に宣言も
実装も無く、「未実装」のコメントだけが残っています。

**ノード識別子 API**: `CanNm_GetNodeIdentifier()` は「最後に受信した NM フレームの Source Node ID」
（他ノードの ID。受信前は 0）を返し、`CanNm_GetLocalNodeIdentifier()` は自ノードの
`CANNM_SOURCE_NODE_ID`（0x01）を返します。

## 通信の停止・再開（`CanNm_DisableCommunication()` / `CanNm_EnableCommunication()`）

BswM のルール（[`BswM_Notes.md`](./BswM_Notes.md) 参照）が `Nm_DisableCommunication()` /
`Nm_EnableCommunication()` 経由で呼びます（UDS 0x28 の CommunicationControl に連動）。
Disable 中は NM フレームを**送信せず**、NM-Timeout Timer の判定も止まります
（受信でもタイマを再起動しません）。そのため Ready Sleep で Disable されるとスリープにも
進まず、Enable した時点でタイマを起算し直して再開します。状態機械の状態自体は変わりません。

## MeterStatus との違い（なぜ Com を経由しないか）

`MeterStatus` は ASW → RTE → Com → PduR → CanIf → Can という
通常のシグナル送信経路を通ります。一方 `CanNm` はシグナル値を運ばず、実車の `CanNm` も
Com スタックを経由せず直接 `CanIf_Transmit()`/`CanIf_RxIndication()` をやり取り
するため、本プロジェクトの `CanNm.c` も同じ構造にしています。

```
MeterStatus: App_EngineManager → Com_SendSignal → Com_RequestTxOnChange（フラグのみ）
               … 次回 Com_MainFunctionTx() → PduR_ComTransmit → CanIf_Transmit → Can_Write

CanNm(TX):      CanNm_MainFunction/CanNm_NetworkRequest等 → CanIf_Transmit → Can_Write
CanNm(RX):      Can_Isr → CanIf_RxIndication → CanNm_RxIndication
             （いずれも PduR・Com を経由しない）
```

## フレームレイアウト（CAN ID 0x400 / DLC=2）

```
byte[0] : Control Bit Vector（Bit0=Repeat Message Request のみ使用。他ビットは
          対応除外の機能に対応するため常に 0）。Bit0 は `CanNm_RepeatMessageRequest()` で
          Repeat Message State へ入ったときだけ 1 になり、その Repeat Message Time 満了で 0 に戻る
byte[1] : Source Node Identifier（本 ECU は 0x01）
```

シグナル値ではなく生存確認そのものが目的のため、E2E 保護は付与していません
（実車でも NM フレームは通常 E2E 保護の対象にしません）。

## ComM との連携（エッジトリガ方式）

ComM は CanNm を直接ではなく **Nm 層（`Nm_NetworkRequest()` / `Nm_NetworkRelease()`）経由**で呼びます
（Nm はチャネルを確認して CanNm へ委譲するだけ。[Nm 層](#nm-層ネットワークマネジメントインタフェース) 参照）。

```
ComM_BusSM_ModeIndication() で FULL_COM が確定したとき:
  Nm_NetworkRequest()  → CanNm_NetworkRequest()
ComM_RequestComMode() で FULL_COM → NO_COM が要求されたとき（CanSM は呼ばない）:
  Nm_NetworkRelease()  → CanNm_NetworkRelease()      ← 協調スリープの起点
ComM_BusSM_ModeIndication() で SILENT_COM が確定したとき（Bus-Off 検出経路）:
  Nm_NetworkRelease()    ← Bus-Off 中の NM 送信リトライと CANNM_E_NETWORK_TIMEOUT 連発を防ぐ
```

NO_COM が確定した時点（`ComM_BusSM_ModeIndication(NO_COM)`）では Release を呼びません
（要求の時点で既に呼んでいるため）。

以前は `CanNm_MainFunction()` が毎周期 `ComM_GetCurrentComMode()` をポーリングして
送信可否だけを判断する簡易設計でしたが、現在は ComM からのエッジトリガ通知を
受けて CanNm 自身が状態機械とタイマを自律的に管理します。これにより、通信解放後も
すぐには送信を止めず（Ready Sleep State）、さらに NM-Timeout Timer +
Wait-Bus-Sleep Timer の間は状態機械上の待機を続けるという、実車と同じ「猶予期間」
が生まれます。

## ComM との連携（Prepare Bus-Sleep/Bus-Sleep、協調スリープ）

CanNm が Prepare Bus-Sleep Mode へ入ると `ComM_Nm_PrepareBusSleepMode()`
（`[SWS_ComM_00826]`、2026-08 追加）を呼びます。ComM はこれを受けて
`CanSM_RequestComMode(SILENT_COM)` を呼び、CAN コントローラを受信専用
（Listen-Only）へ切り替えます（まだ物理スリープはしない）。

CanNm が実際に Bus-Sleep Mode へ到達すると `ComM_Nm_BusSleepMode()`
（`[SWS_ComM_00392]`）を呼びます。CanSM はこの通知を受けて初めて
`Can_SetControllerMode(CAN_T_SLEEP)` を実行し、CAN コントローラを物理的に
スリープさせます（以前は `ComM_RequestComMode(NO_COM)` の時点で即座に
スリープしていましたが、CanNm 導入に伴い変更しました）。

途中で他ノード（仮想他ECU）から NM フレームを受信すると、Network Mode 中の
NM-Timeout Timer が再起動される（実質的にスリープが延期される）ため、
「他ノードがまだ通信中の間は実際にはスリープしない」という協調スリープの本質を
実機で確認できます。Prepare Bus-Sleep Mode（＝上記の受信専用状態）中に
他ノードの NM フレームを受信した場合は、CanNm が自律的に Repeat Message State へ
復帰すると同時に `ComM_Nm_NetworkMode()`（`[SWS_ComM_00296]`、2026-08 追加）が
呼ばれ、`CanSM_RequestComMode(FULL_COM)` でコントローラを送受信可能な状態へ
戻します。

## Nm 層（ネットワークマネジメントインタフェース）

<a id="nm-層ネットワークマネジメントインタフェース"></a>
`src/Bsw/Nm/Nm.c` は、ComM と CanNm の間に置く薄い仲介層です（実 AUTOSAR の Nm Interface。
本プロジェクトは CAN 1 チャネルのみで、`NM_MAIN_NETWORK_HANDLE` 以外のチャネルは
`NM_E_INVALID_CHANNEL` で拒否）。専用ノートは無く、役割は次の 2 方向の中継だけです。

| 方向 | 関数 | 中継先 |
|------|------|-------|
| 下向き（ComM/BswM → CanNm） | `Nm_NetworkRequest` / `Nm_NetworkRelease` / `Nm_DisableCommunication` / `Nm_EnableCommunication` / `Nm_RepeatMessageRequest` / `Nm_GetNodeIdentifier` / `Nm_GetLocalNodeIdentifier` / `Nm_GetState` | `CanNm_*` |
| 上向き（CanNm → ComM） | `Nm_NetworkMode` / `Nm_PrepareBusSleepMode` / `Nm_BusSleepMode` / `Nm_NetworkStartIndication` | `ComM_Nm_NetworkMode` / `ComM_Nm_PrepareBusSleepMode` / `ComM_Nm_BusSleepMode` / `ComM_Nm_NetworkStartIndication` |

上向きの `Nm_NetworkStartIndication()` は、CanNm が Bus-Sleep Mode 中に NM フレームを受信したとき
（`CANNM_E_NET_START_IND` を DET へ報告するのと同時に）呼ばれ、ComM へ「バス上で通信が始まった」
ことを知らせます（[SWS_ComM_00383]）。ComM 側は追加のアクションを取らず、物理ウェイクアップは
CanSM のウェイクアップ検証経路で処理されます。上向きの各関数は、呼ばれるたびに
`Nm: ... -> ComM_Nm_...()` のログを出します（下のログ例では省略しています）。

## ログ例（協調スリープにより物理スリープが延期される様子）

```
[30315ms] INFO  ComM: ch0 ->mode=0                          # ComM_BusSM_ModeIndication(NO_COM)
[30318ms] INFO  CanNm: -> Network Mode: Ready Sleep State (tx stopped)
[33320ms] INFO  CanNm: -> Prepare Bus-Sleep Mode                # NM-Timeout Timer(3000ms)満了
[33320ms] INFO  CanSM: ->SILENT_COM                          # ComM_Nm_PrepareBusSleepMode() 経由
[33850ms] INFO  CanIf: RX can=0x400                          # 仮想他ECU(node=0x02)のNMフレーム受信
[33853ms] INFO  CanNm: RxIndication: node=0x02 woke us from Prepare Bus-Sleep
[33854ms] INFO  CanNm: -> Network Mode: Repeat Message State    # スリープ延期
[33854ms] INFO  CanSM: ->FULL_COM                            # ComM_Nm_NetworkMode() 経由で復帰
[35360ms] INFO  CanNm: -> Network Mode: Ready Sleep State (tx stopped)
[38362ms] INFO  CanNm: -> Prepare Bus-Sleep Mode
[38362ms] INFO  CanSM: ->SILENT_COM
[39865ms] INFO  CanNm: -> Bus-Sleep Mode                        # 今度は他ノードのNMフレームが来なかった
[39866ms] INFO  CanSM: ->NO_COM (physical sleep)             # ComM_Nm_BusSleepMode() 経由
```

## 実機検証（uds_tester）

`tools/can_tool` の「周辺ECU」グループに「NM 仮想他ECU (0x400, node=0x02)」
ボタンがあります。「定期」送信を有効にした状態でエンジンを OFF のまま放置すると、
本 ECU がスリープへ向かう途中で仮想他ECUの NM フレームを受信し続けるため、
ログ上でスリープが延期され続けることを確認できます（周期送信を止めれば、
その後の Wait-Bus-Sleep Timer 満了で通常どおり Bus-Sleep Mode に到達します）。
「NM (0x400)」受信モニターで自ノード・仮想他ECU双方の NM フレーム（Repeat
Message Request ビットの有無を含む）を観測できます。

# Nm（ネットワークマネジメント）

> [README](../../README.md) の「[CAN 通信状態管理](../../README.md#can-comm-management)」節から分離
> （旧「ECU 管理層」節から移動。実 AUTOSAR では EcuM/BswM/WdgM とは別クラスタ
> [Communication Services] に属するため）。

Nm (Network Management) は、実車の各 ECU がバス上に周期的な生存確認フレーム
（NM フレーム）を送信し、クラスタ内の全 ECU が送信を止めたときにのみバス
スリープへ移行できる、という合意形成（協調スリープ）の仕組みです。
CanNm 状態機械（Repeat Message/Normal Operation/Ready Sleep/Prepare
Bus-Sleep/Bus-Sleep）の状態機械図・タイマ設定・ComM/CanSM との連携・ログ例の
詳細を以下にまとめます。
本プロジェクトの `Nm.c` は `docs/4.3.1/AUTOSAR_SWS_CANNetworkManagement.pdf`
の CanNm 状態機械をほぼそのまま実装しており、他ノード（uds_tester が模擬する
「仮想他ECU」）からの NM フレーム受信が自ノードのスリープ判断に反映される
ことを実機で確認できます。

## 状態機械

```
Bus-Sleep Mode ─────────Nm_NetworkRequest()/RxIndication(Prepare Bus-Sleep中)──┐
     ↑ Wait-Bus-Sleep Timer満了                                                │
Prepare Bus-Sleep Mode                                                         │
     ↑ NM-Timeout Timer満了(Ready Sleepから)                                   ▼
Network Mode: Ready Sleep State ←─Nm_NetworkRelease()── Normal Operation State
     │  (送信停止)                                              ↑ (送信継続)
     └──────Repeat Message Time満了(要求あり=Normal Operationへ)─┘
                        ↑
              Repeat Message State（Network Mode への進入は必ずここを経由）
```

3つのタイマ（`Nm_Cfg.h`）で駆動します。

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `NM_TIMEOUT_MS` | 3000 ms | NM-Timeout Timer。送信成功確認/受信のたびに再起動される「他ノードを含め通信が生きているか」の監視タイマ。Ready Sleep State でこれが満了すると Prepare Bus-Sleep Mode へ遷移。Repeat Message/Normal Operation State での満了は本来 Bus-Off 等の異常時にのみ起こる想定（[SWS_CanNm_00193]/[SWS_CanNm_00194]。再送信は伴わず、タイマ再起動と DET 報告のみ） |
| `NM_REPEAT_MESSAGE_MS` | 1500 ms | Repeat Message State の滞在時間 |
| `NM_WAIT_BUS_SLEEP_MS` | 1500 ms | Prepare Bus-Sleep Mode の滞在時間 |
| `NM_CYCLE_MS` | 1000 ms | `Nm_MainFunction()` の呼び出し周期。実 CanNm の Message Cycle Timer（`CanNmMsgCycleTime`、[SWS_CanNm_00032]/[SWS_CanNm_00040]。NM-Timeout Timer とは独立に Repeat Message/Normal Operation State の周期送信を駆動する専用タイマ）をこの呼び出し周期自体で兼用する簡略化 |

Message Cycle Timer と NM-Timeout Timer は独立している点に注意してください。
健全な通信中は毎周期の送信成功が NM-Timeout Timer を先回りして再起動し続ける
ため、`NM_E_NETWORK_TIMEOUT` は通常発生しません（実装当初この分離を誤り、
NM-Timeout Timer 満了そのものを再送信のトリガとしてしまっていたため、健全時
でも約 `NM_TIMEOUT_MS`〜`NM_TIMEOUT_MS+NM_CYCLE_MS` ごとに誤って
`NM_E_NETWORK_TIMEOUT` が発生し続け、かつそのせいで Ready Sleep State 進入
時点でタイマが既に古くなっており Prepare Bus-Sleep Mode へ異常に早く遷移する、
という2つの不具合が実機ログから見つかり修正した経緯があります）。

**対応除外**（実 AUTOSAR CanNm が持つが本プロジェクトでは実装しない機能）:
Partial Networking（7.11章）、NM Coordinator Sync（7.9.7章）、User Data
（7.9.2章）、Remote Sleep Indication（`CanNm_CheckRemoteSleepIndication`、
7.9.1章）、Passive Mode（7.9.3章）。

## MeterStatus との違い（なぜ Com を経由しないか）

`MeterStatus` は ASW → RTE → Com → PduR → CanIf → Can という
通常のシグナル送信経路を通ります。一方 `Nm` はシグナル値を運ばず、実車の `CanNm` も
Com スタックを経由せず直接 `CanIf_Transmit()`/`CanIf_RxIndication()` をやり取り
するため、本プロジェクトの `Nm.c` も同じ構造にしています。

```
MeterStatus: App_EngineManager → Com_SendSignal → Com_RequestTxOnChange（フラグのみ）
               … 次回 Com_MainFunction() → PduR_Transmit → CanIf_Transmit → Can_Write

Nm(TX):      Nm_MainFunction/Nm_NetworkRequest等 → CanIf_Transmit → Can_Write
Nm(RX):      Can_Isr → CanIf_RxIndication → Nm_RxIndication
             （いずれも PduR・Com を経由しない）
```

## フレームレイアウト（CAN ID 0x400 / DLC=2）

```
byte[0] : Control Bit Vector（Bit0=Repeat Message Request のみ使用。他ビットは
          対応除外の機能に対応するため常に 0）
byte[1] : Source Node Identifier（本 ECU は 0x01）
```

シグナル値ではなく生存確認そのものが目的のため、E2E 保護は付与していません
（実車でも NM フレームは通常 E2E 保護の対象にしません）。

## ComM との連携（エッジトリガ方式）

```
ComM_BusSMIndication() がチャネルモードを確定させるたびに:
  FULL_COM へ変化 → Nm_NetworkRequest()
  NO_COM   へ変化 → Nm_NetworkRelease()
```

以前は `Nm_MainFunction()` が毎周期 `ComM_GetCurrentComMode()` をポーリングして
送信可否だけを判断する簡易設計でしたが、現在は ComM からのエッジトリガ通知を
受けて Nm 自身が状態機械とタイマを自律的に管理します。これにより、通信解放後も
すぐには送信を止めず（Ready Sleep State）、さらに NM-Timeout Timer +
Wait-Bus-Sleep Timer の間は状態機械上の待機を続けるという、実車と同じ「猶予期間」
が生まれます。

## ComM との連携（Prepare Bus-Sleep/Bus-Sleep、協調スリープ）

Nm が Prepare Bus-Sleep Mode へ入ると `ComM_Nm_PrepareBusSleepMode()`
（`[SWS_ComM_00826]`、2026-08 追加）を呼びます。ComM はこれを受けて
`CanSM_RequestComMode(SILENT_COM)` を呼び、CAN コントローラを受信専用
（Listen-Only）へ切り替えます（まだ物理スリープはしない）。

Nm が実際に Bus-Sleep Mode へ到達すると `ComM_Nm_BusSleepMode()`
（`[SWS_ComM_00392]`）を呼びます。CanSM はこの通知を受けて初めて
`Can_SetControllerMode(CAN_T_SLEEP)` を実行し、CAN コントローラを物理的に
スリープさせます（以前は `ComM_RequestComMode(NO_COM)` の時点で即座に
スリープしていましたが、Nm 導入に伴い変更しました）。

途中で他ノード（仮想他ECU）から NM フレームを受信すると、Network Mode 中の
NM-Timeout Timer が再起動される（実質的にスリープが延期される）ため、
「他ノードがまだ通信中の間は実際にはスリープしない」という協調スリープの本質を
実機で確認できます。Prepare Bus-Sleep Mode（＝上記の受信専用状態）中に
他ノードの NM フレームを受信した場合は、Nm が自律的に Repeat Message State へ
復帰すると同時に `ComM_Nm_NetworkMode()`（`[SWS_ComM_00296]`、2026-08 追加）が
呼ばれ、`CanSM_RequestComMode(FULL_COM)` でコントローラを送受信可能な状態へ
戻します。

## ログ例（協調スリープにより物理スリープが延期される様子）

```
[30315ms] INFO  ComM: ch0 ->mode=0                          # ComM_BusSMIndication(NO_COM)
[30318ms] INFO  Nm: -> Network Mode: Ready Sleep State (tx stopped)
[33320ms] INFO  Nm: -> Prepare Bus-Sleep Mode                # NM-Timeout Timer(3000ms)満了
[33320ms] INFO  CanSM: ->SILENT_COM                          # ComM_Nm_PrepareBusSleepMode() 経由
[33850ms] INFO  CanIf: RX can=0x400                          # 仮想他ECU(node=0x02)のNMフレーム受信
[33853ms] INFO  Nm: RxIndication: node=0x02 woke us from Prepare Bus-Sleep
[33854ms] INFO  Nm: -> Network Mode: Repeat Message State    # スリープ延期
[33854ms] INFO  CanSM: ->FULL_COM                            # ComM_Nm_NetworkMode() 経由で復帰
[35360ms] INFO  Nm: -> Network Mode: Ready Sleep State (tx stopped)
[38362ms] INFO  Nm: -> Prepare Bus-Sleep Mode
[38362ms] INFO  CanSM: ->SILENT_COM
[39865ms] INFO  Nm: -> Bus-Sleep Mode                        # 今度は他ノードのNMフレームが来なかった
[39866ms] INFO  CanSM: ->NO_COM (physical sleep)             # ComM_Nm_BusSleepMode() 経由
```

## 実機検証（uds_tester）

`tools/uds_tester` の「周辺ECU」グループに「NM 仮想他ECU (0x400, node=0x02)」
ボタンがあります。「定期」送信を有効にした状態でエンジンを OFF のまま放置すると、
本 ECU がスリープへ向かう途中で仮想他ECUの NM フレームを受信し続けるため、
ログ上でスリープが延期され続けることを確認できます（周期送信を止めれば、
その後の Wait-Bus-Sleep Timer 満了で通常どおり Bus-Sleep Mode に到達します）。
「NM (0x400)」受信モニターで自ノード・仮想他ECU双方の NM フレーム（Repeat
Message Request ビットの有無を含む）を観測できます。

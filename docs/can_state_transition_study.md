# CanSM・ComM の状態遷移と、受信デッドライン監視の開始・停止

CAN の通信モードが、どのきっかけで、どう変わり、その結果、Com の受信デッドライン監視がいつ始まり、いつ止まるか、をまとめた勉強用の資料です。
チャネルは 1 本固定(コントローラ 0、`COMM_CHANNEL_0`)です。数値と処理の流れは、2026-10-31 時点の実装から取っています。

- CanSM: [CanSM.c](../src/Bsw/CanSM/CanSM.c)、設定は [CanSM_Cfg.h](../src/Bsw/CanSM/CanSM_Cfg.h)
- ComM: [ComM.c](../src/Bsw/ComM/ComM.c)
- Com: [Com.c](../src/Bsw/Com/Com.c)、グループとタイムアウトの設定は [Com_Cfg.h](../src/Bsw/Com/Com_Cfg.h) と [Com_PBCfg.c](../src/Bsw/Com/Com_PBCfg.c)
- BswM のルール: [BswM_PBCfg.c](../src/Bsw/BswM/BswM_PBCfg.c)

## 1. 全体像: 誰が誰を呼ぶか

```
 アプリ(App_EngineManager)  ComM_RequestComMode()          ┐
 Dcm(診断要求)             ComM_DCM_ActiveDiagnostic()      ├→ ComM ──→ CanSM ──→ CanIf / Can
 CanNm(NM の状態変化)       ComM_Nm_*()                     ┤           ↑
 EcuM(ウェイクアップ)        ComM_EcuM_WakeUpIndication()    ┘     CanSM が結果を ComM_BusSM_ModeIndication() で返す

 ComM のモードが変わる → BswM のルールが評価される → Com_IpduGroupStart() / Com_IpduGroupStop()
```

- ComM は、ユーザの要求を集約して(一番高いモードが勝つ)、CanSM へ要求します。コントローラを直接は操作しません。
- CanSM は、コントローラ(CanIf 経由)と、CanIf の PDU モードを操作して、結果を ComM へ通知します。
- Com の I-PDU Group を起動・停止するのは、Com 自身ではなく BswM のルールです(仕様の想定どおり)。

## 2. CanSM の状態遷移

| 状態 | 意味 | コントローラ / CanIf |
|---|---|---|
| NO_COM | 通信停止。物理スリープ中(または未起動) | `CAN_CS_SLEEP`(ウェイクアップ割り込みを武装) |
| WAKEUP_VALIDATING | ウェイクアップの検証中(ノイズによる誤起床を除く) | `CAN_CS_STOPPED`(Listen-Only。受信だけ) |
| FULL_COM | 通常の送受信 | `CAN_CS_STARTED` + `CANIF_ONLINE` |
| SILENT_COM | 受信専用 | `CAN_CS_STARTED` のまま、`CANIF_TX_OFFLINE`(送信だけ止める) |
| BUS_OFF | Bus-Off の回復中 | `CAN_CS_STOPPED` |

| 現在の状態 | きっかけ | 次の状態 | 行うこと |
|---|---|---|---|
| NO_COM | `CanSM_ControllerModeIndication()`(バス活動の検知) | WAKEUP_VALIDATING | `CAN_CS_STOPPED` にして、検証タイマを開始 |
| WAKEUP_VALIDATING | 有効な受信を確認し、ComM が `CanSM_RequestComMode(FULL_COM)` | FULL_COM | `CAN_CS_STARTED`、`CANIF_ONLINE`、Dem の Bus-Off を PASSED、ComM へ FULL_COM |
| WAKEUP_VALIDATING | 検証タイムアウト(`CANSM_WAKEUP_VALIDATION_MS` = 2000 ms) | NO_COM | `CAN_CS_SLEEP` へ戻す |
| NO_COM | `RequestComMode(FULL_COM)` | FULL_COM | 同上 |
| FULL_COM | `RequestComMode(SILENT_COM)` | SILENT_COM | `CanIf_SetPduMode(TX_OFFLINE)`、ComM へ SILENT_COM |
| SILENT_COM | `RequestComMode(FULL_COM)` | FULL_COM | `CAN_CS_STARTED`、`CANIF_ONLINE`、ComM へ FULL_COM |
| FULL_COM / SILENT_COM | `RequestComMode(NO_COM)` | NO_COM | `CAN_CS_SLEEP`、ComM へ NO_COM |
| FULL_COM / SILENT_COM | `CanSM_ControllerBusOff()` | BUS_OFF | `CAN_CS_STOPPED`、直前の状態を覚える、ComM へ SILENT_COM |
| BUS_OFF | `CanSM_MainFunction()` で、L1 / L2 の時間が経過 | 直前の状態(FULL_COM または SILENT_COM) | `CAN_CS_STARTED`、回復の確定待ちを開始、ComM へ通知 |
| BUS_OFF | `RequestComMode(NO_COM)` | NO_COM | 受け付けて回復を取り消し、`CAN_CS_SLEEP`、ComM へ NO_COM([SWS_CanSM_00182]) |
| BUS_OFF | `RequestComMode(FULL_COM / SILENT_COM)` | 変わらない | 受け付けて(`E_OK`)、回復の後に戻る状態だけを要求どおりに変える。SILENT_COM は、回復を待たずに TX を止める([SWS_CanSM_00369]) |

Bus-Off の回復の間隔(L1 / L2)は、次のとおりです。
- 回復の試行が 3 回(`CANSM_BUSOFF_L1_TO_L2_COUNT`)以内なら L1 = 200 ms ごと。それを超えると L2 = 5000 ms ごと。
- 再起動のあと、TX の確認を 1 回でも受け取ると(`CANSM_BOR_TX_CONFIRMATION_POLLING`)、回復が確定します。そこで試行回数を 0 に戻し、Dem の Bus-Off を PASSED にします([SWS_CanSM_00496]〜[00498])。
- Bus-Off を検出するたびに、Dem の Bus-Off へ PREFAILED を報告します([SWS_CanSM_00522]/[00605])。確定しないまま 4 回目の検出に入ると、Dem のデバウンスが DTC を確定します(持続的な Bus-Off と判断)。

## 3. ComM のチャネルの状態遷移

ComM は、`ComM_ChannelMode` として FULL_COM / SILENT_COM / NO_COM の 3 つを持ちます。FULL_COM → NO_COM の間は、CanNm の協調スリープを待つため、`ComM_ChannelMode` は FULL_COM のまま、「解放待ち」(`ComM_NmReleasePending`)になります。

| きっかけ | ComM の動き | CanSM への要求 |
|---|---|---|
| アプリが FULL_COM を要求(起動時など) | 集約して、FULL_COM へ(`CommunicationAllowed` が TRUE のとき) | `RequestComMode(FULL_COM)` |
| アプリが NO_COM を要求(エンジン OFF が続いた) | CanNm へ `NetworkRelease` だけ送って待つ。モードは FULL_COM のまま | なし |
| CanNm が Prepare Bus-Sleep Mode へ(`ComM_Nm_PrepareBusSleepMode`) | SILENT_COM へ | `RequestComMode(SILENT_COM)` |
| CanNm が Bus-Sleep Mode へ(`ComM_Nm_BusSleepMode`) | NO_COM へ。EcuM の RUN を解放 | `RequestComMode(NO_COM)` |
| 他ノードの NM フレームで、スリープを取りやめた(`ComM_Nm_NetworkMode`) | FULL_COM へ戻す | `RequestComMode(FULL_COM)` |
| Dcm の診断要求(`ComM_DCM_ActiveDiagnostic`) | 仮想のユーザとして FULL_COM を要求。解放待ちなら取り消す | `RequestComMode(FULL_COM)` |
| 診断の終了(`ComM_DCM_InactiveDiagnostic`) | 仮想のユーザの要求を取り下げ、再集約 | 結果しだい |
| 物理ウェイクアップの確定(`ComM_EcuM_WakeUpIndication`) | FULL_COM へ | `RequestComMode(FULL_COM)` |
| CanSM から `ComM_BusSM_ModeIndication` | 結果を反映。FULL_COM なら EcuM の RUN を要求、NO_COM なら解放 | なし |

CanSM が Bus-Off の間に出す SILENT_COM は、ComM から見ると、協調スリープの SILENT_COM と区別できません。（2026-11 までは、Bus-Off の回復中は、CanSM が ComM の要求を断る(`E_NOT_OK`)ことと合わせて、特別な扱い(`ComM_RetryNmReleaseAfterBusOff`)がありました。今は、CanSM が回復中の要求を受け付ける([SWS_CanSM_00182])ので、この扱いは不要になり、削除しました。）

## 4. 受信デッドライン監視の開始と停止

### 4.1 設定

| I-PDU | 内容 | I-PDU Group | 監視の時間 |
|---|---|---|---|
| EngineInfo(RX) | エンジン回転数、冷却水温、エンジン ON | `COM_IPDU_GROUP_SENSOR_RX`(1) | 初回も、通常も 5000 ms |
| AbsInfo(RX) | 車速、ブレーキ、ABS | `COM_IPDU_GROUP_SENSOR_RX`(1) | 初回も、通常も 5000 ms |
| ほかの RX | ImmobilizerCmd など | どのグループにも属さない | 監視しない(`TimeoutMs` = 0) |

TX の I-PDU は、受信の監視の対象外です。

### 4.2 開始(再始動)する条件

「開始」は、次の 3 つを同時に行うことです。(1) 最終受信の時刻を、今にする。(2) タイムアウトの印を消す。(3) 初回用の時間(`FirstTimeoutMs`)から数え直す。

| きっかけ | 対象 | 備考 |
|---|---|---|
| `Com_Init()` | グループに属さない I-PDU | すぐに監視が始まる。グループに属する I-PDU は、停止状態から始まる |
| `Com_IpduGroupStart(group)` | そのグループの RX I-PDU | BswM の Rule 6(EcuM が RUN、かつ ComM が FULL_COM)が呼ぶ |
| `Com_EnableReceptionDM(group)` | 無効だった I-PDU | 既に有効なら、何もしない([SWS_Com_00486]) |
| `Com_SetCommunicationEnabled(Rx: 0→1)` | すべての RX I-PDU | UDS 0x28(CommunicationControl)で、受信を戻したとき |

### 4.3 タイマを巻き戻す(受信した)条件

`Com_RxIndication()` が呼ばれたとき、次を行います([SWS_Com_00715]、[SWS_Com_00738])。
- 最終受信の時刻を今にし、タイムアウトの印を消し、通常用の時間(`TimeoutMs`)に切り替える。
- 受け入れの可否に関係しません。I-PDU のコールアウトが拒否しても、DLC が足りなくても、受信したという事実だけで巻き戻します。データの中身(無効値など)も見ません。

### 4.4 停止する(評価しない)条件

| きっかけ | 何が止まるか | 受信そのもの |
|---|---|---|
| `Com_IpduGroupStop(group)` | 受信と監視の両方([SWS_Com_00684]、[00685]) | 止まる |
| `Com_DisableReceptionDM(group)` | 監視だけ | 続く |
| `Com_SetCommunicationEnabled(Rx: 0)` | 全 RX の監視 | 無視する |
| `TimeoutMs` が 0 | その I-PDU の監視([SWS_Com_00333]) | 影響なし |
| `FirstTimeoutMs` が 0 | 初回の受信までの監視([SWS_Com_00716])。初回の受信後は、`TimeoutMs` で監視する | 影響なし |
| `Com_DeInit()` | すべて | 止まる |

BswM のルールは、RX のグループ(`SENSOR_RX`)を、**ComM が NO_COM のときだけ**止めます(Rule 7)。SILENT_COM(Bus-Off や協調スリープの途中)では、受信は生きているので、止めません。TX のテレメトリ(`TELEMETRY`)は、SILENT_COM でも止めます(Rule 5)。

### 4.5 タイムアウトの検出

`Com_MainFunctionRx()`(100 ms ごと)が、次の条件で判定します。
- 経過時間(今 − 最終受信)が、今使っている時間(初回なら `FirstTimeoutMs`、それ以外は `TimeoutMs`)以上になったら、タイムアウトです。
- タイムアウトの通知(`RxTOutCbk`)は、**1 回だけ**呼びます。印は、次に `Com_RxIndication()` が呼ばれるか、監視が再始動されるまで、残ります。
- 印が付いている間、`Com_ReceiveSignal()` は、`RxDataTimeoutAction`(NONE、REPLACE、SUBSTITUTE)に従って振る舞います。

### 4.6 実機ログの例(can_tool_log_20261031_101449)

相手のいないバスで、起動直後に Bus-Off を繰り返した場面です。

| 時刻 | ログ | 意味 |
|---|---|---|
| 14:14:52.172 | `IpduGroupStart grp=1 iPdu=0(RX)` / `iPdu=1(RX)` | Bus-Off の回復で FULL_COM に戻り、Rule 6 が RX のグループを起動。監視が(再)始動 |
| 14:14:53.644 | `BusOff detected! retry=3 (L2) recovery in 5000ms` | 4 回目から L2(5000 ms ごと)。回復までの間、SILENT_COM |
| 14:14:55.571 | `CanNm_EnterPrepareBusSleep` | 協調スリープの途中。ComM が SILENT_COM へ |
| 14:14:57.190 | `RX timeout iPdu=0 (5000ms, first)` | 再始動(52.172)から 5000 ms、一度も受信せず、初回のタイムアウト |
| 14:14:57.587 | `RequestComMode ignored: BusOff recovery in progress` | Bus-Off の回復中なので、CanSM が NO_COM の要求を断る(2026-11 の変更で、受け付けて、回復を取り消す動作に変わりました。このログの行は、変更前のものです) |
| 14:14:58.662 | `BusOff recovered -> FULL_COM` | L2 の回復。Rule 6 が再び RX のグループを起動 |
| 14:14:58.639〜 | `CAN: Rx 0x300 / 0x240 / 0x400` | テスターが接続され、受信が再開(`RxIndication` が、タイマを巻き戻す) |

## 5. 仕様との突き合わせの結果(2026-10-31)

### 5.1 直したもの

| 項目 | 仕様 | 以前の実装 | 対応 |
|---|---|---|---|
| `Com_EnableReceptionDM()` を、既に有効な I-PDU に呼ぶ | 黙って無視する([SWS_Com_00486]) | 監視タイマを再始動していた(動いている監視の経過時間が巻き戻り、タイムアウトの検出が遅れる) | 既に有効なら何もしない。テストを追加 |
| `Com_DisableReceptionDM()` を、既に無効な I-PDU に呼ぶ | 黙って無視する([SWS_Com_00225]) | 状態は変わらないが、ログが重複 | 既に無効なら何もしない |
| ComM.c の冒頭の状態図 | — | SILENT_COM、NO_COM を「`Can_SetControllerMode(CAN_T_STOP)`」としていた(古い設計) | 現在の動作(SILENT_COM は CanIf の TX 抑制、NO_COM は物理スリープ)に書き直した |

### 5.2 気づいたが、変えなかったもの(判断が必要)

| 項目 | 状況 | 変えない理由 |
|---|---|---|
| `Com_IpduGroupStart()` を、起動中のグループへ呼ぶと、受信の監視を再始動する | 仕様は「停止中のグループを起動する」と書くだけで、起動中への呼び出しは定めていない。BswM の Rule 6 は、Bus-Off の回復のたびに、一度も止めていない RX のグループへ、起動を呼ぶ(上の 4.6 の 14:14:52.172 など) | 仕様が曖昧で、実機の動作が変わる。Bus-Off が短い間隔で続くと、再始動のたびにタイムアウトが遅れる点は、設計の選択として残す |
| `BswM_CanSM_CurrentState()` が未実装 | 仕様の E_BUS_OFF / E_TX_ON の効果は、BswM への通知を求める。このプロジェクトでは、ComM 経由の通知で、同じルールを動かしている | 影響が小さく、規模が大きい(BswM の状態の入力元が増える) |
| `CanSM_StartWakeupSource` / `CanSM_StopWakeupSource` が未実装 | EcuM の `Start/StopWakeupSources` の前提 | 既知の残件(EcuM の項目) |

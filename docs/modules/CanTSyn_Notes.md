# CanTSyn（CAN 時刻同期、時間マスターのみ）

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

ECU の稼働時間（`millis()` 相当、Os カウンタの tick）を、AUTOSAR の CAN 時刻同期
（`docs/autosar/4.3.1/AUTOSAR_SWS_TimeSyncOverCAN.pdf` の SYNC/FUP の2段階）の
フォーマットで CAN バスへ周期配信します。受信側（CAN ツール）は、ECU のシリアルログの
`[<ms>ms]` と PC の時刻を対応づけられます。

## 作った理由

CAN ツールは、シリアルログ（ECU の `[<ms>ms]`）と CAN フレーム（PC の受信時刻）を1つの
ログへ統合します。2つの時計を揃えるのに、以前は ECU の CanIf の `TX`/`RX ... can=0x..` ログと
CAN フレームを突き合わせていました。しかしこの方法は、ECU のログ出力（`DET_LOG_LEVEL`）が
INFO 以上であることに依存します。ComStack の送受信ログを DEBUG へ下げると、同期が
できなくなります。そこで、ECU が CAN で時刻そのものを配信し、ツールは CAN の受信だけで
同期できるようにしました。

## シーケンス（[SWS_CanTSyn_00025]/[SWS_CanTSyn_00045]）

```
CanTSyn_MainFunction()（100ms 周期）
  待機中に CANTSYN_TX_PERIOD_MS（1000ms）が経過していたら SYNC を送る:
    T0 = 現在時刻
    SYNC(Type=0x10, 秒=T0 の秒部分) を CanIf_Transmit()
    SYNC の送信完了待ちへ（T0 を控える）

CanTSyn_TxConfirmation()（CanIf が送信完了を通知）
  SYNC の送信完了なら:
    T4 = (T0 の ns 部分) + (T0 からの経過時間)    ← 送信遅延を含む「SYNC を送った時刻」
    FUP(Type=0x18, ナノ秒=T4 の ns 部分, OVS=T4 の秒の繰り上がり) を CanIf_Transmit()
  FUP の送信完了なら待機へ戻る（SYNC/FUP の1組が完了）

送信完了通知が CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS（100ms）以内に来なければ、
送信を取り消して待機へ戻り、次の周期で SYNC からやり直す（FUP は送らない、[SWS_CanTSyn_00033]）。
```

SYNC と FUP は同じ CAN ID（0x240）・同じ PDU を使い、Byte0 の Type で区別します
（[SWS_CanTSyn_00008]）。PduR/Com を経由せず、`CanTSyn` が `CanIf_Transmit()` を直接
呼びます（`CanNm` と同じ構造）。

## フレームフォーマット（CRC なし、DLC=8、時刻はビッグエンディアン）

```
SYNC（[SWS_CanTSyn_00015]）
  Byte0   : Type = 0x10
  Byte1   : User Byte 1（既定 0）
  Byte2   : 時間領域 D（上位4bit、本 ECU は 0）+ シーケンスカウンタ SC（下位4bit）
  Byte3   : User Byte 0（既定 0）
  Byte4-7 : SyncTimeSec = T0 の秒（32bit）

FUP（[SWS_CanTSyn_00016]）
  Byte0   : Type = 0x18
  Byte1   : User Byte 2（既定 0）
  Byte2   : 時間領域 D + SC（対の SYNC と同じ値、[SWS_CanTSyn_00049]）
  Byte3   : SGW（Bit2、0 = SyncToGTM）+ OVS（Bit1-0、ns が 1 秒を超えた秒数）
  Byte4-7 : SyncTimeNSec = T4 の ns 部分（32bit）
```

SC は SYNC を送信要求するたびに +1 し、0〜15 で循環します（[SWS_CanTSyn_00048]。
送信が拒否された要求でも進めます）。

## 受信側（CAN ツール）の使い方

SYNC（秒）の受信時刻と、FUP の `(秒 + OVS) + ナノ秒` から求めた「ECU が SYNC を実際に送った
時刻」を対応づけて、PC 時刻と ECU 時刻のオフセットを求めます（直近16標本の中央値）。
ECU の再起動は、ECU 時刻が前回より小さくなることで検出します。ツールの詳細は
[tools/can_tool/README.md](../../tools/can_tool/README.md) を参照してください。

## 対応範囲と、将来の本格実装

| 項目 | 状況 |
| --- | --- |
| 時間マスター（SYNC/FUP の送信） | 実装済み |
| 時間スレーブ（`CanTSyn_RxIndication`、受信した時刻を StbM へ反映） | 未実装 |
| OFS/OFNS（オフセット時間ベース）、CRC 付きメッセージ（0x20/0x28）、CAN FD の拡張フォーマット | 未実装 |
| 時刻源 | StbM ではなく ECU の稼働時間（`CanTSyn_GetNowMs()`、Os カウンタ） |
| 時間領域 | 0 の1つ。時間ベースの状態（同期/ゲートウェイ）は持たない |
| ナノ秒の分解能 | ms（`1ms = 1,000,000ns` 単位） |

API は SWS_CanTSyn に合わせています（`CanTSyn_Init`/`GetVersionInfo`/`SetTransmissionMode`/
`MainFunction`/`TxConfirmation`、開発エラー `CANTSYN_E_*`）。StbM を実装する場合は、
`CanTSyn_GetNowMs()` を `StbM_GetCurrentTime()` / `StbM_GetCurrentTimeRaw()` /
`StbM_GetCurrentTimeDiff()` に置き換え、`CanTSyn_Cfg.h` の設定値（時間領域 ID・送信周期・
送信完了待ちの時間）を ECUC のパラメータ（`CanTSynGlobalTimeDomainId` など）へ対応づければ、
メッセージフォーマットと送信シーケンスはそのまま使えます。

## 配線

| 箇所 | 内容 |
| --- | --- |
| 信号表（`config/data/can_signals.json`） | フレーム `TimeSync`（0x240、DLC=8）。Com は経由しない（`Com.json` の `excludedFrames`） |
| `config/data/CanIf.json` | TX PDU `CanTSyn_Tx`（`txConfirm: "CANTSYN"`）。送信完了は `CanTSyn_TxConfirmation()` へ通知 |
| `config/data/Os.json` / `BswM.json` | タスク `CanTSyn_MainFunction`（100ms）。`CanNm` と同じく POST_RUN（バススリープの準備中）でも動き続け、SHUTDOWN（バススリープ）で BswM が止める。Com の I-PDU グループや ComM の通信許可は経由しない（`CanNm` と同じ。SILENT_COM 中の送信抑制は CanIf の PduMode で効く） |
| `EcuM_Init()` | `CanTSyn_Init(NULL)`（`ComM_RequestComMode()` の前） |

## 単体テスト

`test/CallChain/TimeSyncStack/` に、CanIf と Can を実体のまま使うコールチェーンのテストがあります。
SYNC が HW へ届き、`Can_MainFunction_Write()` の確認キューのドレインの中で FUP が送信・完了されること（ドレイン中の
再入）と、次の周期で SC が進むことを確認します。

`test/Bsw/CanTSyn/` に、`CanTSyn_Init`/`GetVersionInfo`/`SetTransmissionMode`/`MainFunction`/
`TxConfirmation` の各関数のテストがあります。`CanIf_Transmit()` は wrap（`Wrap_CanIf`）で
成功を強制し、渡されたフレームの中身（Type・SC・秒・ナノ秒・OVS）を検証します。
未初期化での呼び出しの DET 報告（`CANTSYN_E_UNINIT`）は、CanTSyn に DeInit が無く、
他のテストが先に初期化済みにするため検証できません（[docs/unit_test.md](../unit_test.md) 参照）。

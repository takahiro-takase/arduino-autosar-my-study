# CanTSyn（CAN 時刻同期、時間マスター・時間スレーブ）

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

## フレームフォーマット（DLC=8、時刻はビッグエンディアン）

CRC の有無は `CanTSyn_Cfg.h` の `CANTSYN_TX_CRC_SECURED`（1=CRC 付きが既定、0=CRC なし）で切り替えます。Type は、
CRC なしが SYNC=0x10 / FUP=0x18、CRC 付きが SYNC=0x20 / FUP=0x28 です（[SWS_CanTSyn_00031]）。

```
SYNC（[SWS_CanTSyn_00015]/[SWS_CanTSyn_00017]）
  Byte0   : Type = 0x10（CRC 付きは 0x20）
  Byte1   : User Byte 1（既定 0）。CRC 付きでは CRC
  Byte2   : 時間領域 D（上位4bit、本 ECU は 0）+ シーケンスカウンタ SC（下位4bit）
  Byte3   : User Byte 0（既定 0）
  Byte4-7 : SyncTimeSec = T0 の秒（32bit）

FUP（[SWS_CanTSyn_00016]/[SWS_CanTSyn_00018]）
  Byte0   : Type = 0x18（CRC 付きは 0x28）
  Byte1   : User Byte 2（既定 0）。CRC 付きでは CRC
  Byte2   : 時間領域 D + SC（対の SYNC と同じ値、[SWS_CanTSyn_00049]）
  Byte3   : SGW（Bit2、0 = SyncToGTM）+ OVS（Bit1-0、ns が 1 秒を超えた秒数）
  Byte4-7 : SyncTimeNSec = T4 の ns 部分（32bit）
```

SC は SYNC を送信要求するたびに +1 し、0〜15 で循環します（[SWS_CanTSyn_00048]。
送信が拒否された要求でも進めます）。

### CRC（CRC 付きのとき）

- CRC-8H2F（多項式 0x2F、初期値 0xFF、最終 XOR 0xFF、反転なし。AUTOSAR の `Crc_CalculateCRC8H2F()` と同じ。
  検査値 `"123456789"` → 0xDF）を、**Byte2〜Byte7 に DataID を続けて**計算し、Byte1 に入れます
  （[SWS_CanTSyn_00055]）。CRC を使うモジュールが無いため、`CanTSyn.c` 内に関数を持ちます。
- DataID は 8bit で、仕様では SC をインデックスにした表（`DataIDList[SC]`、[SWS_CanTSyn_00054]）で与えます。
  本プロジェクトは、E2E が DataID を CAN ID と一致させている慣習に倣い、表を **CAN ID から導きます**:
  SYNC の DataID = (CAN ID + SC) の下位 8bit、FUP の DataID = (CAN ID + 0x10 + SC) の下位 8bit
  （CAN ID 0x240 なら、SYNC は 0x40〜0x4F、FUP は 0x50〜0x5F）。SC ごと・SYNC/FUP ごとに値が違うため、
  別の SC や SYNC/FUP を取り違えたフレームは CRC が合いません。CAN ID は 11bit で 8bit に収まらないため、
  E2E のように CAN ID そのものは使えず、下位 8bit に SC を足す形にしています（表は持ちませんが、
  ECUC 上は「16 要素の表」と等価です）。受信側（CAN ツール）も同じ式で検証します。

## 受信側（CAN ツール）の使い方

SYNC（秒）の受信時刻と、FUP の `(秒 + OVS) + ナノ秒` から求めた「ECU が SYNC を実際に送った
時刻」を対応づけて（CRC 付きの場合は CRC を検証し、一致しないフレームは同期に使わず WARN を出します）、PC 時刻と ECU 時刻のオフセットを求めます（直近16標本の最小値。遅れて届いたフレームの標本は必ず大きくなるため）。
ECU の再起動は、ECU 時刻が前回より小さくなることで検出します。ツールの詳細は
[tools/can_tool/README.md](../../tools/can_tool/README.md) を参照してください。

## 対応範囲と、将来の本格実装

| 項目 | 状況 |
| --- | --- |
| 時間マスター（SYNC/FUP の送信） | 実装済み |
| 時間スレーブ（`CanTSyn_RxIndication`、受信した時刻を StbM の時間ベース 1 へ反映） | 実装済み（時間領域 1、CAN ID 0x241） |
| CRC 付きメッセージ（0x20/0x28）の送信 | 実装済み（`CANTSYN_TX_CRC_SECURED`、既定 1） |
| OFS/OFNS（オフセット時間ベース）、CAN FD の拡張フォーマット | 未実装 |
| 時刻源 | StbM（[StbM_Notes.md](./StbM_Notes.md)）。送信は時間ベース 0（本 ECU が時間マスター）、受信は時間ベース 1。`CanTSyn_GetNowMs()`（Os カウンタ）は、送信周期と待ち時間の計時だけに使う |
| 時間領域 | 送信が 0、受信が 1 |
| ナノ秒の分解能 | ms（`1ms = 1,000,000ns` 単位） |

API は SWS_CanTSyn に合わせています（`CanTSyn_Init`/`GetVersionInfo`/`SetTransmissionMode`/
`MainFunction`/`TxConfirmation`/`RxIndication`、開発エラー `CANTSYN_E_*`）。設定値（時間領域 ID・
送信周期・送信完了待ちの時間・受信の CRC 検証・FUP の待ち時間・SC の増分の上限）は、ECUC の
パラメータ（`CanTSynGlobalTimeDomainId`・`CanTSynGlobalTimeTxPeriod`・
`CanTSynMasterConfirmationTimeout`・`CanTSynRxCrcValidated`・`CanTSynGlobalTimeFollowUpTimeout`・
`CanTSynGlobalTimeSequenceCounterJumpWidth`）に対応します。

## 時間スレーブ（受信）

他ノードの時間マスターが時間領域 1・CAN ID 0x241 で送る SYNC/FUP を受け、StbM の時間ベース 1 へ
反映します。アルゴリズムは [SWS_CanTSyn_00073]:

```
SYNC 受信:  T0 = SYNC の秒、T2raw = StbM_GetCurrentTimeRaw()（受信時の仮想ローカル時間）を控える
FUP 受信:   T4 = OVS の秒 + FUP のナノ秒、T3diff = StbM_GetCurrentTimeDiff(T2raw)（SYNC を受けてからの経過時間）
            全域時間 = T0 + T4 + T3diff   → StbM_BusSetGlobalTime(時間ベース 1, 全域時間, 伝搬遅延 0)
```

検証（ひとつでも外れたら、その SYNC/FUP は使わない）:

| 検証 | 内容 |
| --- | --- |
| 時間領域 | `CANTSYN_RX_TIME_DOMAIN_ID`（1）と一致 |
| DLC | 8 以上 |
| Type と CRC | `CANTSYN_RX_CRC_VALIDATED`=1 なら Type 0x20/0x28 で CRC が正しいもの、0 なら 0x10/0x18 |
| SC の増分（SYNC） | 直前に受けた SYNC との差が 1〜`CANTSYN_RX_SC_JUMP_WIDTH`。外れた SYNC は無視するが、SC は記録する（1 回取りこぼしても、次の SYNC から復帰できる） |
| SC の一致（FUP） | 対の SYNC と同じ。違えば、保留中の SYNC も捨てる |
| FUP の待ち時間 | SYNC から `CANTSYN_RX_FOLLOW_UP_TIMEOUT_MS`（200ms）未満 |

## 配線

| 箇所 | 内容 |
| --- | --- |
| 信号表（`config/data/can_signals.json`） | フレーム `TimeSync`（0x240、DLC=8）。Com は経由しない（`Com.json` の `excludedFrames`） |
| `config/data/CanIf.json` | TX PDU `CanTSyn_Tx`（`txConfirm: "CANTSYN"`）。送信完了は `CanTSyn_TxConfirmation()` へ通知 |
| `config/data/Os.json` / `BswM.json` | タスク `CanTSyn_MainFunction`（100ms）。`CanNm` と同じく POST_RUN（バススリープの準備中）でも動き続け、SHUTDOWN（バススリープ）で BswM が止める。Com の I-PDU グループや ComM の通信許可は経由しない（`CanNm` と同じ。SILENT_COM 中の送信抑制は CanIf の PduMode で効く） |
| `config/data/CanIf.json`（受信） | RX PDU `CanTSyn_Rx`（`rxIndication: "CANTSYN"`、信号表 `TimeSyncIn` = 0x241）。受信は `CanTSyn_RxIndication()` へ通知 |
| `EcuM_Init()` | `StbM_Init(NULL)` → `CanTSyn_Init(NULL)`（`ComM_RequestComMode()` の前） |

## 単体テスト

`test/CallChain/TimeSyncStack/` に、CanIf と Can を実体のまま使うコールチェーンのテストがあります。
SYNC が HW へ届き、`Can_MainFunction_Write()` の確認キューのドレインの中で FUP が送信・完了されること（ドレイン中の
再入）と、次の周期で SC が進むことを確認します。

`test/Bsw/CanTSyn/` に、`CanTSyn_Init`/`GetVersionInfo`/`SetTransmissionMode`/`MainFunction`/
`TxConfirmation` の各関数のテストがあります。`CanIf_Transmit()` は wrap（`Wrap_CanIf`）で
成功を強制し、渡されたフレームの中身（Type・SC・秒・ナノ秒・OVS）を検証します。
未初期化での呼び出しの DET 報告（`CANTSYN_E_UNINIT`）は、CanTSyn に DeInit が無く、
他のテストが先に初期化済みにするため検証できません（[docs/unit_test.md](../unit_test.md) 参照）。

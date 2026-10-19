# StbM（同期時間ベースマネージャ、最小版）

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

時刻（全域時間）を保持し、上位・下位のモジュールへ提供します。時刻をどう同期するか
（CAN なら [`CanTSyn`](./CanTSyn_Notes.md)）は知りません。仕様は
`AUTOSAR_SWS_SynchronizedTimeBaseManager.pdf`（R4.3.1、docs/ には無く、autosar.org から取得）です。

## 時刻の考え方

```
仮想ローカル時間 [ns] = Os カウンタ [ms] × 10^6        （ECU の稼働時間。分解能 1ms）
全域時間 [ns]         = 仮想ローカル時間 + オフセット   （時間ベースごと、符号付き）
```

時刻を設定する（`StbM_SetGlobalTime` / `StbM_BusSetGlobalTime`）と、そのときの全域時間が与えた値に
なるようにオフセットを更新します。時刻が進めば、全域時間も同じだけ進みます。
`StbM_GetCurrentTimeRaw()` は仮想ローカル時間の ns の下位 32bit（約 4.29 秒で循環）を返し、
`StbM_GetCurrentTimeDiff()` はその差を循環を考慮して返します。CanTSyn は、SYNC を渡した時刻と
受信した時刻の差を測るのに使います（[SWS_CanTSyn_00045]/[SWS_CanTSyn_00073]）。

Os カウンタ（32bit、約 49.7 日で一巡）は、StbM が一巡を数えて 64bit に広げて使うため、全域時間は一巡をまたいでも飛びません
（一巡より短い間隔で呼ばれることが前提で、`StbM_MainFunction` が 100ms ごとに呼びます）。設定できる時刻は秒が
約 146 年分（4611686017 秒）までで、超えると `STBM_E_PARAM` を報告して `E_NOT_OK` を返します。
`StbM_GetCurrentTimeDiff()` は約 4.29 秒を超える間隔を区別できないため、呼び出し側が間隔を短く保つ必要があります
（CanTSyn は FUP の待ち時間 200ms で制限しています）。

## 時間ベース

| ID | 役割 | 起動時 |
| --- | --- | --- |
| 0（`STBM_TIME_BASE_MASTER`） | 本 ECU が時間マスター。全域時間 = ECU の稼働時間。CanTSyn が SYNC/FUP で配信する | 同期済み（GLOBAL_TIME_BASE） |
| 1（`STBM_TIME_BASE_SLAVE`） | 他ノードの時間マスターに同期する。CanTSyn が受信した SYNC/FUP から `StbM_BusSetGlobalTime()` で更新する | 未同期 |

時間ベース 1 は、`STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE`（5000ms）更新されないと、`StbM_MainFunction()` が
TIMEOUT ビットを立て（[SWS_StbM_00179]）、次の更新で下ろします。マスターは自分で時刻を決めるため、
タイムアウトを監視しません。

## 実装した API

`StbM_Init` / `StbM_GetVersionInfo` / `StbM_GetCurrentTime` / `StbM_GetCurrentTimeRaw` /
`StbM_GetCurrentTimeDiff` / `StbM_SetGlobalTime` / `StbM_BusSetGlobalTime` / `StbM_GetTimeBaseStatus` /
`StbM_MainFunction`。開発エラーは `STBM_E_PARAM`（0x0A）・`STBM_E_NOT_INITIALIZED`（0x0B）・
`STBM_E_PARAM_POINTER`（0x10）です。

## 対応範囲

| 項目 | 状況 |
| --- | --- |
| 同期時間ベース（時刻の保持、設定、取得、同期喪失のタイムアウト） | 実装済み |
| ユーザデータ | 保持しない（取得時は長さ 0） |
| オフセット時間ベース、純ローカル時間ベース | 未実装 |
| レート補正（`StbM_SetRateCorrection` など）、時刻の飛び（TIMELEAP）の検出 | 未実装 |
| 時刻記録、通知コールバック、タイマ（`StbM_StartTimer`）、`StbM_GetCurrentTimeExtended` ほか | 未実装 |
| 時刻源 | Os カウンタ（ms）。分解能 1ms |

## 配線

| 箇所 | 内容 |
| --- | --- |
| `EcuM_Init()` | `StbM_Init(NULL)`（`CanTSyn_Init()` より前） |
| `config/data/Os.json` / `BswM.json` | タスク `StbM_MainFunction`（100ms）。SHUTDOWN 中は BswM が止める |
| `CanTSyn` | マスターは時間ベース 0 から時刻を読んで SYNC/FUP を送る。スレーブは受信した時刻を時間ベース 1 へ反映する |

## 単体テスト

`test/Bsw/StbM/` に各関数のテストがあります。仮想ローカル時間の ns の循環をまたぐ時間差、48bit の秒、
伝搬遅延の加算、TIMEOUT の立ち下がりを確認します。未初期化での API 呼び出し（`STBM_E_NOT_INITIALIZED`）は、
StbM に DeInit が無く、他のテストが先に初期化済みにするため検証できません
（[docs/unit_test.md](../unit_test.md) 参照）。

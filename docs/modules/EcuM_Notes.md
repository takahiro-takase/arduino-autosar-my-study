# EcuM（ECU ステートマネージャ）

> [README](../../README.md) の「[ECU 管理層](../../README.md#ecu-management)」節から分離。

EcuM (ECU State Manager) は BSW スタック全体のライフサイクル（STARTUP/RUN/POST_RUN/
SHUTDOWN）を管理するモジュールです。`main.cpp` は `EcuM_Init()` と
`EcuM_MainFunction()` を呼ぶだけでよく、個々の BSW モジュールを直接参照しません。
状態マシン・Os スケジューラティック（Gpt 駆動）・RUN ユーザ管理の詳細を
以下にまとめます。

## EcuM 状態マシン

```
          EcuM_Init() 完了
STARTUP ──────────────────→ RUN ── 全 RUN ユーザが解放 ──→ POST_RUN
                             ↑                                  │
                    EcuM_RequestRUN が来たら ←──────────────────┘
                    (POST_RUN 中の場合のみ)       ECUM_POST_RUN_TIMEOUT_MS (5秒) 経過
                                                               ↓
                                                           SHUTDOWN
                            (WdgM_TriggerHwWatchdog / Can_MainFunction_Read /
                             Can_MainFunction_Wakeup / CanSM_MainFunction /
                             NvM_MainFunction / MemIf_MainFunction /
                             Nm_MainFunction 以外は停止)
                             ↑                                  │
                    CAN バスのウェイクアップ ←──────────────────┘
                    (EcuM_RequestRUN 経由)
```

| 状態 | `Os_SchedulerStep()` | 遷移条件 |
|------|:-------------------:|---------|
| STARTUP | 停止 | `EcuM_Init()` 末尾で RUN へ自動遷移 |
| RUN | **実行** | 全 RUN ユーザが `EcuM_ReleaseRUN` → POST_RUN |
| POST_RUN | **実行**（後処理継続） | タイムアウト → SHUTDOWN / `EcuM_RequestRUN` → RUN |
| SHUTDOWN | **実行**（`WdgM_TriggerHwWatchdog` / `Can_MainFunction_Read` / `Can_MainFunction_Wakeup` / `CanSM_MainFunction` / `NvM_MainFunction` / `MemIf_MainFunction` / `Nm_MainFunction` のみ有効） | Arduino では電源断不可のためアイドル待機するが、`EcuM_RequestRUN` が来れば RUN へ復帰できる（CAN バスのウェイクアップ経由）。`Os_SchedulerStep()` 自体は呼ばれ続けるが、BswM Rule 2 がこの 7 タスク以外を無効化するため実質アイドル。HW ウォッチドッグ維持のため `WdgM_TriggerHwWatchdog`、CAN ウェイクアップ検出・検証中フレーム処理のため `Can_MainFunction_Read`/`Can_MainFunction_Wakeup`、ウェイクアップ検証タイムアウト監視のため `CanSM_MainFunction`、保留中の DTC 永続化のため `NvM_MainFunction`/`MemIf_MainFunction`（NvM がジョブを開始するだけの `NvM_MainFunction` だけを動かしても、物理バイト書き込みを進める `MemIf_MainFunction` を止めてしまうとジョブが永久に完了しない）、Nm 状態機械（Bus-Sleep Mode への到達判定・他ノードの NM フレーム受信によるスリープ延期の継続処理）のため `Nm_MainFunction` だけは動き続ける（CAN 受信自体は真のハードウェア割り込み `Can_Isr()` のため、この無効化に関わらず常に起動する） |

SHUTDOWN は CAN バスのウェイクアップにより常に RUN へ復帰できます。実機リセットが
必要な終端状態は存在しません。Bus-Off 回復は後述の通り L1/L2 バックオフで無期限に
継続するため、Bus-Off の検出・回復だけを理由に新たに RUN が解放されて SHUTDOWN へ
向かうことはなく、SHUTDOWN は ComM の NO_COM 要求による正常系（ボランタリ）スリープ
からのみ到達します（NO_COM_PENDING_SLEEP 中に実際に Bus-Off が発生した場合、回復時に
`ComM_BusSMIndication(NO_COM)` が呼ばれ直すことはありますが、RUN は既にボランタリ
スリープ突入時点で解放済みのため、これによって新たに `EcuM_ReleaseRUN()` が呼ばれる
ことはありません。詳細は CanSM.c の `CanSM_BusOffFromPendingSleep` 参照）。

## Os のスケジューラティック（Gpt 駆動）

`Os_SchedulerStep()` の周期到来判定に使う時間源は、当初 Arduino コアの
`millis()` でしたが、2026-08 に Os 専用の Gpt チャネル（`GPT_CHANNEL_1`、
1000Hz=1ms 分解能、Notification なし）へ置き換えました。`Os_Init()` が
自らこのチャネルを `Gpt_StartTimer()` で起動し、以後は
`Gpt_GetTimeElapsed(GPT_CHANNEL_1)` を都度ポーリングします（本物の
AUTOSAR OS の OsCounter が HW タイマ割り込みで駆動される構成に近づける
ための変更。詳細は `src/Os/Os.c` 冒頭のコメント参照）。

`loop()` は従来どおり `EcuM_MainFunction()` を busy-spin で呼び続けます。
`Gpt_SetMode`/`Gpt_EnableWakeup` 系は本プロジェクトの EcuM が SLEEP モード
を持たないため未実装であり（Gpt モジュールの節参照）、CPU を寝かせる余地が
ないためです。つまりこの変更は「割り込みで CPU を起こす」設計ではなく、
「経過時間の計算に使う時計を millis() から Gpt の ISR 駆動ティックへ
差し替える」だけの、スコープを絞った変更です。

**millis() をフォールバック用に残した理由:** [DEVLOG](../DEVLOG.md#can-rx-割り込み化の実機検証で得られた教訓)
に記録のとおり、本プロジェクトは実機で割り込みが期待どおり発火しなかった
事象を CAN RX 割り込み化の際に一度経験しています。Os の時間源はスケジューラ
そのものであり、`WdgM_TriggerHwWatchdog` を含む全タスクの発火判定に使われる
ため、ここが完全に停止すると実 HW ウォッチドッグ（`WDGM_HW_WATCHDOG_TIMEOUT_MS`=
4000ms）でリセットされてしまいます。CAN フレーム 1 個の欠落よりも影響が
大きいため、単に「検知してログを残す」だけでは不十分です（ログを残しても
スケジューラ自体が止まったままでは結局リセットに至ってしまう）。

`Os_CrossCheckTickSource()` は Gpt とは別系統の HW タイマで駆動している
`millis()` との差分を `OS_TICK_CROSSCHECK_PERIOD_MS`（500ms、HW ウォッチドッグ
タイムアウトの 4000ms に対して 8 倍のマージン）ごとに突き合わせ、Gpt ティック
の進みが明らかに遅い（半分未満）場合は実際に時間源を `millis()` へ
フォールバックします（ラッチ式。一度切り替えたらその起動中は millis() を
使い続ける）。切り替える瞬間は全タスクの最終実行時刻を現在の `millis()` 値へ
リセットします（`Os_SetTaskActive()` が休止タスクを再開する際に行うのと
同じ考え方。リセットしないと基準時刻が「Gpt ティック（停止した値）」から
「millis()（現在の実時刻）」へ飛び、ほぼ全タスクが「周期を大幅に超過している」
と誤判定されて一斉に追いつき実行されてしまう。これは WdgM の Alive Supervision
が過去に繰り返し踏んだ「監視対象タスクに実行機会がほとんどないまま判定される」
誤検知と同種の事故になりうるため避けている）。

初版（2026-08 最初のコミット）ではクロスチェック周期を 5000ms、フォールバック
無しの「ログのみ」としていましたが、いずれも問題があるとレビューで指摘され
修正しました。5000ms は 4000ms の HW ウォッチドッグタイムアウトより長く、
最悪ケースでは診断ログさえリセット前に一度も出力されません。また「ログのみ」
ではスケジューラが止まったまま復旧しないため、結局リセットに至ることに
変わりありませんでした。

## RUN ユーザ

RUN フェーズを継続するために「誰かが使っている」ことを宣言するしくみです。
ユーザが全員解放したときに POST_RUN へ遷移します。

| ユーザ | 定数 | `EcuM_RequestRUN` タイミング | `EcuM_ReleaseRUN` タイミング |
|-------|------|--------------------------|--------------------------|
| ComM | `ECUM_USER_COMM` | CAN バスが FULL_COM になったとき（起動時 / Bus-Off 回復試行時 / ボランタリスリープからのウェイクアップ時） | CAN バスが NO_COM になったとき（エンジン OFF 継続によるボランタリスリープ突入時。NO_COM_PENDING_SLEEP 中に Bus-Off が発生し回復した場合も `ComM_BusSMIndication(NO_COM)` は呼ばれ直すが、RUN は既に解放済みのため `EcuM_ReleaseRUN()` が再度呼ばれることはない） |

**重複要求・対応しない解放の検知（SWS_EcuM_04125/04127）:**
`EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` は、AUTOSAR の実 EcuM と同様に
「同一ユーザからの要求はネストできない」ことを検知します。各ユーザの RUN 要求は
`EcuM_RunUsers` のビットマスクで管理しており、既に立っているビットへ重ねて
`EcuM_RequestRUN()` を呼ぶと DET 相当のログ（`ECUM_E_MULTIPLE_RUN_REQUESTS`）を
出力して `E_NOT_OK` を返します。同様に、立っていないビットに対して
`EcuM_ReleaseRUN()` を呼ぶと `ECUM_E_MISMATCHED_RUN_RELEASE` 相当のログを
出力して `E_NOT_OK` を返します。呼び出し元は必ず `void` キャストで戻り値を
捨てているため、この検知は実行時の挙動には影響しません（開発時の診断用途）。

この検知の追加に伴い、`ComM_BusSMIndication()` 側も、実際に EcuM の RUN 要求状態が
変化した時のみ `EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` を呼ぶよう変更しました。
当初はチャネルモード（`ComM_ChannelMode`）そのものの変化で判定していましたが、
Bus-Off 検出時に一時的に挟まる `COMM_SILENT_COMMUNICATION`（EcuM の RUN 状態には
無関係）を経由すると、回復時の FULL_COM/NO_COM 通知が「SILENT_COM からの変化」として
見えてしまい、EcuM 側で `ERR=0x20`（多重要求）/`ERR=0x21`（不整合解放）を誤検知する
不具合があった（2026-08 発見・修正）。現在は `ComM_EcuMRunMode`（EcuM へ最後に伝えた
FULL/NO_COM の別。SILENT_COM では更新しない）という専用の内部状態で判定しており、
Bus-Off 回復中に SILENT_COM を何度挟んでも、EcuM への再通知は本当に FULL⇔NO_COM が
変化したときだけに限られます。

## EcuM 設定（`EcuM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `ECUM_USER_COUNT` | 1 | RUN 要求できるユーザ数 |
| `ECUM_USER_COMM` | 0 | ComM のユーザ ID |
| `ECUM_POST_RUN_TIMEOUT_MS` | 5000 ms | POST_RUN タイムアウト |

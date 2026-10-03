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
                    (POST_RUN 中の場合のみ)       POST_RUN 要求ユーザが無く、かつ
                                                  ECUM_POST_RUN_TIMEOUT_MS (5秒) 経過
                                                               ↓
                                                           SHUTDOWN
                            (WdgM_TriggerHwWatchdog / Can_MainFunction_Read /
                             Can_MainFunction_Wakeup / CanSM_MainFunction /
                             NvM_MainFunction / MemIf_MainFunction /
                             CanNm_MainFunction 以外は停止)
                             ↑                                  │
                    CAN バスのウェイクアップ ←──────────────────┘
                    (EcuM_RequestRUN 経由)
```

| 状態 | `Os_SchedulerStep()` | 遷移条件 |
|------|:-------------------:|---------|
| STARTUP | 停止 | `EcuM_Init()` 末尾で RUN へ自動遷移 |
| RUN | **実行** | 全 RUN ユーザが `EcuM_ReleaseRUN` → POST_RUN |
| POST_RUN | **実行**（後処理継続） | POST_RUN 要求ユーザが無く、かつタイムアウト → SHUTDOWN / `EcuM_RequestRUN` → RUN |
| SHUTDOWN | **実行**（`WdgM_TriggerHwWatchdog` / `Can_MainFunction_Read` / `Can_MainFunction_Wakeup` / `CanSM_MainFunction` / `NvM_MainFunction` / `MemIf_MainFunction` / `CanNm_MainFunction` のみ有効） | Arduino では電源断不可のためアイドル待機するが、`EcuM_RequestRUN` が来れば RUN へ復帰できる（CAN バスのウェイクアップ経由）。`Os_SchedulerStep()` 自体は呼ばれ続けるが、BswM Rule 2 がこの 7 タスク以外を無効化するため実質アイドル（マスク対象外の `App_GptDemo_Run`/`ComM_MainFunction`/`SecOC_MainFunctionRx` を除く。詳細は [`BswM_Notes.md`](./BswM_Notes.md)）。HW ウォッチドッグ維持のため `WdgM_TriggerHwWatchdog`、CAN ウェイクアップ検出・検証中フレーム処理のため `Can_MainFunction_Read`/`Can_MainFunction_Wakeup`、ウェイクアップ検証タイムアウト監視のため `CanSM_MainFunction`、保留中の DTC 永続化のため `NvM_MainFunction`/`MemIf_MainFunction`（NvM がジョブを開始するだけの `NvM_MainFunction` だけを動かしても、物理バイト書き込みを進める `MemIf_MainFunction` を止めてしまうとジョブが永久に完了しない）、CanNm 状態機械（Bus-Sleep Mode への到達判定・他ノードの NM フレーム受信によるスリープ延期の継続処理）のため `CanNm_MainFunction` だけは動き続ける（CAN 受信自体は真のハードウェア割り込み `Can_Isr()` のため、この無効化に関わらず常に起動する） |

SHUTDOWN は CAN バスのウェイクアップにより常に RUN へ復帰できます。実機リセットが
必要な終端状態は存在しません。Bus-Off 回復は後述の通り L1/L2 バックオフで無期限に
継続するため、Bus-Off の検出・回復だけを理由に新たに RUN が解放されて SHUTDOWN へ
向かうことはなく、SHUTDOWN は ComM の NO_COM 要求による正常系（ボランタリ）スリープ
からのみ到達します（CanNm の協調スリープ待ち中に実際に Bus-Off が発生した場合、回復時に
ComM が `ComM_RetryNmReleaseAfterBusOff()` で解放をやり直します。ComM は RUN 要求状態
（`ComM_EcuMRunMode`）が実際に変化したときだけ `EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` を
呼ぶため、Bus-Off 回復によって `EcuM_ReleaseRUN()` が重複して呼ばれることはありません。
詳細は ComM.c の `ComM_BusSM_ModeIndication()` 参照）。

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

**millis() をフォールバック用に残した理由:** [`Can_Notes.md`](./Can_Notes.md#rx-割り込み化の実機検証で得られた教訓)
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

**各モジュールの時刻（Os のカウンタ API）:** スケジューラ以外のモジュール（CanSM/CanNm/CanTp/Com/
Dcm/WdgM/EcuM/App_EngineManager など）も、`millis()` を直接呼ばず `GetCounterValue(SYSTEM_COUNTER, &tick)`
（SWS_Os_00383）で時刻を得ます（各モジュールの `<Mod>_GetNowMs()`）。この値は `Os_Init()` より前は
`millis()` を返し、`Os_Init()` の時点で「Gpt 時間源と `millis()` の差」をオフセットとして確定するため、
`Os_Init()` の前後で値が飛びません。Gpt ティックの停滞を検知して `millis()` へフォールバックするときも、
オフセットを付け替えて値が連続するようにしています（上の「実機での確認」参照）。

初版（2026-08 最初のコミット）ではクロスチェック周期を 5000ms、フォールバック
無しの「ログのみ」としていましたが、いずれも問題があるとレビューで指摘され
修正しました。5000ms は 4000ms の HW ウォッチドッグタイムアウトより長く、
最悪ケースでは診断ログさえリセット前に一度も出力されません。また「ログのみ」
ではスケジューラが止まったまま復旧しないため、結局リセットに至ることに
変わりありませんでした。

**実機での確認（2026-10-03）:** 一時コードで起動 30 秒後に `Gpt_StopTimer(GPT_CHANNEL_1)` を呼んで
Gpt ティックを人為的に止め、フォールバックの動作を確認しました。

- 停止の約 316ms 後に `Gpt tick stalled ... -> fallback to millis()` が出力され、
  WdgM のエラーやリセットは発生せず、以後も正常に動作し続けました
- `GetCounterValue()` の値は切り替えの前後で飛ばず、後ろへ戻ることもありませんでした
- **注意点 1: カウンタは停止していた時間の分だけ実時間より遅れたままになる。**
  切り替えは「その時点のカウンタ値を保ったまま `millis()` 基準へ付け替える」ため、
  Gpt が止まってから検知するまでの時間（実測で約 312ms、最大でクロスチェック周期の
  500ms 程度）がカウンタに反映されず、以後も `millis()` より約 312ms 遅れた値を返します。
  カウンタの差分で時間を測るモジュール（NM、Dcm S3、COM のデッドライン監視など）は、
  その分だけ判定が余計に遅れうるだけで、値の逆行や急な進みは起きません
- **注意点 2: 全タスクの周期が切り替え時点から数え直される。** 追いつき実行の連発を避ける
  ためのリセットの副作用で、`CanNm_MainFunction` の送信間隔が 1 回だけ約 1741ms
  （通常 1000ms）に伸びましたが、その後は元の周期に戻りました

## RUN ユーザ

RUN フェーズを継続するために「誰かが使っている」ことを宣言するしくみです。
ユーザが全員解放したときに POST_RUN へ遷移します。

| ユーザ | 定数 | `EcuM_RequestRUN` タイミング | `EcuM_ReleaseRUN` タイミング |
|-------|------|--------------------------|--------------------------|
| ComM | `ECUM_USER_COMM` | CAN バスが FULL_COM になったとき（起動時 / Bus-Off 回復試行時 / ボランタリスリープからのウェイクアップ時） | CAN バスが NO_COM になったとき（エンジン OFF 継続によるボランタリスリープで、CanNm が Bus-Sleep Mode へ到達して `CanSM` が物理スリープした直後。CanNm の協調スリープ待ち中に Bus-Off が発生した場合は、回復時に ComM が解放をやり直す。RUN の要求状態が変化したときだけ呼ぶため、`EcuM_ReleaseRUN()` が重複して呼ばれることはない） |

**重複要求・対応しない解放の検知（SWS_EcuM_04125/04127）:**
`EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` は、AUTOSAR の実 EcuM と同様に
「同一ユーザからの要求はネストできない」ことを検知します。各ユーザの RUN 要求は
`EcuM_RunUsers` のビットマスクで管理しており、既に立っているビットへ重ねて
`EcuM_RequestRUN()` を呼ぶと DET 相当のログ（`ECUM_E_MULTIPLE_RUN_REQUESTS`）を
出力して `E_NOT_OK` を返します。同様に、立っていないビットに対して
`EcuM_ReleaseRUN()` を呼ぶと `ECUM_E_MISMATCHED_RUN_RELEASE` 相当のログを
出力して `E_NOT_OK` を返します。呼び出し元は必ず `void` キャストで戻り値を
捨てているため、この検知は実行時の挙動には影響しません（開発時の診断用途）。

この検知の追加に伴い、`ComM_BusSM_ModeIndication()` 側も、実際に EcuM の RUN 要求状態が
変化した時のみ `EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` を呼ぶよう変更しました。
当初はチャネルモード（`ComM_ChannelMode`）そのものの変化で判定していましたが、
Bus-Off 検出時に一時的に挟まる `COMM_SILENT_COMMUNICATION`（EcuM の RUN 状態には
無関係）を経由すると、回復時の FULL_COM/NO_COM 通知が「SILENT_COM からの変化」として
見えてしまい、EcuM 側で `ERR=0x20`（多重要求）/`ERR=0x21`（不整合解放）を誤検知する
不具合があった（2026-08 発見・修正）。現在は `ComM_EcuMRunMode`（EcuM へ最後に伝えた
FULL/NO_COM の別。SILENT_COM では更新しない）という専用の内部状態で判定しており、
Bus-Off 回復中に SILENT_COM を何度挟んでも、EcuM への再通知は本当に FULL⇔NO_COM が
変化したときだけに限られます。

## 状態遷移に伴う WdgM・BswM との連携

| 遷移 | EcuM が行うこと |
|------|----------------|
| STARTUP → RUN（`EcuM_Init()` 末尾） | `BswM_EcuM_CurrentState(RUN)`（Rule 0: 全タスク有効化） |
| RUN → POST_RUN（最後の RUN ユーザが解放） | POST_RUN の起算時刻を記録 → `WdgM_DisableHwWatchdog()`（アプリタスクが止まり Alive が必ず不足するため、SHUTDOWN を待たずここで HW ウォッチドッグを無効化扱いにする）→ `BswM_EcuM_CurrentState(POST_RUN)`（Rule 1: アプリタスク無効化） |
| POST_RUN → SHUTDOWN | `BswM_EcuM_CurrentState(SHUTDOWN)`（Rule 2: 動かし続けるタスク以外を無効化） |
| POST_RUN / SHUTDOWN → RUN（`EcuM_RequestRUN`） | `WdgM_ResumeSupervision()`（チェックポイント基準のリセット。止まっていた時間を Deadline 違反と誤認しないため）→ `WdgM_EnableHwWatchdog()` → `BswM_EcuM_CurrentState(RUN)`。POST_RUN 要求も全てクリアする（残すと次回の SHUTDOWN が永久に保留されるため） |

## POST_RUN 要求（`EcuM_RequestPOST_RUN` / `EcuM_ReleasePOST_RUN`）

[SWS_EcuM_04128]/[SWS_EcuM_04129]。RUN 要求とは別に、POST_RUN フェーズの継続を要求するしくみです
（`EcuM_PostRunUsers` のビットマスクで RUN 要求とは独立に管理）。POST_RUN 中に 1 件でも要求が
残っていると、`ECUM_POST_RUN_TIMEOUT_MS` が経過しても SHUTDOWN へ遷移しません。最後の 1 件を
解放した時点から、改めてタイムアウトを起算します。RUN 中に呼んだ場合はビットを記録するだけで、
その場での状態遷移は起きません。同一ユーザの重複要求・対応しない解放の扱いは、RUN 系と同じ
エラーコード（`ECUM_E_MULTIPLE_RUN_REQUESTS` / `ECUM_E_MISMATCHED_RUN_RELEASE`）を共用します。
**本プロジェクトの本番コードにこの 2 関数を呼ぶ箇所はありません**（ユニットテストのみ）。

## ウェイクアップ通知（`EcuM_CheckWakeup`）

[SWS_Can_00271]。Can ドライバの `Can_MainFunction_Wakeup()` が、スリープ中の CAN バス活動を
検出すると `EcuM_CheckWakeup(ECUM_WKSOURCE_CAN)` を呼びます（旧 `CanIf_ControllerWakeup()` の
役割を引き継ぐ）。EcuM は CAN 以外の要因を持たないため、`ECUM_WKSOURCE_CAN` 以外には
`ECUM_E_INVALID_PAR` を報告します。CAN の場合は `CanSM_ControllerModeIndication()` を呼んで
ウェイクアップ検証（CanSM の `WAKEUP_VALIDATING`）を始め、検証に成功すると ComM 経由で
`EcuM_RequestRUN()` が呼ばれて SHUTDOWN から RUN へ戻ります
（詳細は [`CanSM_Notes.md`](./CanSM_Notes.md) と [can_stack.md](../can_stack.md#can-controller-sleep)）。

## EcuM 設定（`EcuM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `ECUM_USER_COUNT` | 1 | RUN 要求できるユーザ数 |
| `ECUM_USER_COMM` | 0 | ComM のユーザ ID |
| `ECUM_POST_RUN_TIMEOUT_MS` | 5000 ms | POST_RUN タイムアウト |

## 開発の経緯（実機で見つかった不具合・設計変更）

> 現在の仕様を理解するだけなら読む必要はありません。実機検証で見つかった
> 不具合や、その結果としての設計変更の経緯を時系列でまとめています。

### スケジューラの時間源を millis() から Gpt 割り込み駆動へ変更した経緯

`Os_SchedulerStep()` の周期到来判定は、当初 Arduino コアの `millis()`
（フレームワーク内部の HW タイマ割り込みで駆動される、実装詳細を意識しない
時計）を時間源にしていた。実機の AUTOSAR OS では OsCounter を HW タイマ
割り込みで駆動するのが本来の姿であり、既に実装済みの Gpt Driver
（Renesas RA FspTimer 経由）をこの用途に使うことでより実機に近い構成に
できるため、2026-08 に Os 専用の Gpt チャネル（`GPT_CHANNEL_1`）を新設し、
時間源を `Gpt_GetTimeElapsed(GPT_CHANNEL_1)` に置き換えた。

**検討した設計の選択肢:** 当初「`loop()` 自体を Gpt 割り込みでイベント駆動
にする」フル置き換え案も検討したが、以下の理由で見送り、時間源の差し替え
のみに留めた（最小構成案を採用）。

- 本プロジェクトの EcuM は SLEEP モードを持たず（`Gpt_SetMode`/
  `Gpt_EnableWakeup` 系が未実装なのはこのため）、CPU を寝かせる余地が
  そもそも無い。`loop()` を busy-spin から「割り込みを待つ」構成に変えても
  省電力・レイテンシ上の実利が無く、複雑さだけが増える。
- `Os_SchedulerStep()` は既に CAN Rx・CanTp・Rte Runnable まで全ての周期
  処理を仲介しており（`EcuM_MainFunction()` はこれを呼ぶだけ）、独立した
  「割り込み化すべき別のポーリング処理」が他に残っていない。
- ISR 発火をまたいだ「未処理ティックの蓄積 or 破棄」という新しい設計判断
  が増えるだけで、本プロジェクトが繰り返し踏んできた
  [WdgM のタイミング関連リグレッション](./WdgM_Notes.md#post_runrun-復帰時の-alive-supervision-誤検出)
  と同種の事故を呼び込みやすい。

**millis() をフォールバック用に残した理由:** 本プロジェクトは
[CAN RX 割り込み化の実機検証で得られた教訓](./Can_Notes.md#rx-割り込み化の実機検証で得られた教訓)
のとおり、割り込みが実機で期待どおり発火しない事象を一度経験している。
今回 Gpt が駆動するのは Os スケジューラそのものであり、これが完全に止まると
`WdgM_TriggerHwWatchdog` を含む全タスクが二度と発火しなくなり、実 HW
ウォッチドッグ（`WDGM_HW_WATCHDOG_TIMEOUT_MS`=4000ms）でリセットされる。
CAN フレーム 1 個の欠落よりも影響が大きいため、Gpt とは別系統の HW タイマで
駆動している `millis()` との差分を `Os_CrossCheckTickSource()` で
`OS_TICK_CROSSCHECK_PERIOD_MS`（500ms）ごとに突き合わせ、Gpt ティックの
進みが明らかに遅ければ（半分未満）実際に時間源を `millis()` へフォールバック
する（ラッチ式。ログも 1 回だけ）。

**初版のレビューで発覚した2つの不備（2026-08）:**

1. **クロスチェック周期が HW ウォッチドッグより長かった。** 初版は
   `OS_TICK_CROSSCHECK_PERIOD_MS=5000ms` としていたが、`WdgM_Cfg.h` の
   `WDGM_HW_WATCHDOG_TIMEOUT_MS` は 4000ms しかない。最悪ケース
   （クロスチェック直後にストールが始まる場合）では、次のクロスチェックが
   来る前に HW ウォッチドッグがリセットしてしまい、まさに診断しようとしていた
   事象を一度も検知できないまま再起動する。500ms（4000ms に対して 8 倍の
   マージン）に修正した。
2. **検知しても「ログを残すだけ」で自動復旧しなかった。** 検知周期を
   どれだけ短くしても、ログを出すだけでは `Os_GetTimeMs()` が返す値は
   Gpt ティックのまま（停止した値）であり、スケジューラは動かない。
   結局 HW ウォッチドッグのリセットに至ることに変わりはなく、「起動失敗を
   自動復旧はしない」（Mcu の PORF 未解決事象と同じ「記録のみ」方針）を
   ここでも踏襲したのは誤りだった。PORF の場合と異なり、Os の時間源停止は
   スケジューラ全体（HW ウォッチドッグのリフレッシュを含む）を巻き込むため、
   「記録して残すだけ」では実害（無音のリセット）を防げない。検知したら
   `Os_UseMillisFallback` を立てて実際に `millis()` へ切り替えるよう修正した
   （切り替え時に全タスクの最終実行時刻を現在の `millis()` へリセットするのは
   [WdgM の resume 時刻ずれ](./WdgM_Notes.md#post_runrun-復帰時の-alive-supervision-誤検出)
   と同種の「基準時刻が飛んで一斉追いつき実行され誤検知する」事故を避けるため）。
   これは `Gpt_StartTimer()` が戻り値を持たず（AUTOSAR SWS_Gpt 準拠のため
   `void`）、`Os_Init()` 側で起動失敗を直接検出する手段が無いという設計上の
   制約に対する対策も兼ねる。起動失敗時も Gpt ティックは最初から一切
   進まないため、同じクロスチェック経路で（最初の 500ms 経過時点で）検知され、
   HW ウォッチドッグのタイムアウトよりずっと前に millis() へフォールバックする。

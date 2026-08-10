# WdgM（ウォッチドッグマネージャ）

> [README](../../README.md) の「[ECU 管理層](../../README.md#ecu-management)」節から分離。

WdgM (Watchdog Manager) は「ソフトウェアが本当に動いているか」を監視するモジュールです。
EcuM や BswM がフェーズ管理・タスク制御を担うのに対し、WdgM はタスク内部の実行を監視します。

CAN バスが正常でも、タスクが無限ループやスタック破壊で停止することがあります。
WdgM は監視対象（Supervised Entity）に「生存報告」を埋め込み、報告が途絶えたとき（Alive Supervision）、
報告が想定外の順序で来たとき（Logical Supervision）、報告の間隔が異常に長い・短いとき
（Deadline Supervision）に異常と判断します。AUTOSAR が定める 3 つの監視アルゴリズムです。

異常時の最終アクションは **実ハードウェアウォッチドッグによる本当の MCU リセット**です
（後述）。ログ出力だけのシミュレーションではなく、実機上で実際に再起動が発生します。

## Alive Supervision の仕組み

`App_EngineManager_Run()` は 1 回の実行で `WdgM_CheckpointReached` を 2 回呼ぶため
（後述の START/END チェックポイント）、AliveCount は 3000ms 周期の Run() 呼び出しごとに 2 ずつ増えます。

```
監視対象 Runnable が呼ぶ:
  App_EngineManager_Run()  (3000ms 周期)
    ├→ WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START)  ← 開始
    └→ WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END)    ← 終了（"私は正常に完了した"）

WdgM_MainFunction (6000ms 周期) が評価:
  AliveCount >= WDGM_ENGINE_EXPECTED_ALIVE_INDICATIONS (1) ?
    YES → LOCAL_STATUS_OK  → "SE0 OK alive=4"   ← 6000ms の間に Run() が 2 回 × 2 チェックポイント
    NO  → LOCAL_STATUS_FAILED → "SE0 FAILED alive=0 [HW WDT reset pending]"

評価後: AliveCount = 0 にリセットして次サイクル開始
  （HW ウォッチドッグへのリフレッシュはここでは行わない。別タスクの
   WdgM_TriggerHwWatchdog が 1000ms 周期で判定結果を見て行う。後述）
```

## Logical Supervision の仕組み

チェックポイントが「来たかどうか」だけでなく「正しい順序で来たか」を検査します。
`WdgM_CheckpointReached()` が呼ばれた瞬間に、直前のチェックポイントから今回のチェックポイントへの
遷移が許可遷移テーブル（`WdgM_PBCfg.c` の `WdgM_EngineTransitions[]`）に含まれるかを即座に確認するため、
`WdgM_MainFunction` の周期を待たずに違反を検出できます。

Entity 0（App_EngineManager_Run）で許可される遷移グラフ:

```
(起動直後)
    │ WDGM_CP_INITIAL → WDGM_CP_ENGINE_START
    ▼
┌─────────┐  START→END   ┌───────┐
│  START  │ ────────────→│  END  │
└─────────┘               └───────┘
    ▲                          │
    └────── END→START ─────────┘
          (次サイクル)
```

上記以外の遷移（例: START の連続呼び出し、起動直後に END が来る等）は順序違反として
即座に `LOCAL_STATUS_FAILED` にし、WARN ログを出力します。

## Deadline Supervision の仕組み

Alive（来たかどうか）・Logical（正しい順序か）に続く、AUTOSAR の 3 つ目の監視
アルゴリズムです。チェックポイント間の**実際の経過時間**が許容範囲内かを検査します。

`WdgM_CheckpointReached()` が呼ばれた瞬間に、直前のチェックポイントからの経過時間
(`millis()` の差分) を計算し、許容テーブル（`WdgM_PBCfg.c` の `WdgM_EngineDeadlines[]`）
に設定された `[MinMs, MaxMs]` と比較します。範囲外（遅すぎる、または速すぎる）なら
`Logical Supervision` と同様に `WdgM_MainFunction` の周期を待たず即座に検出します。

Entity 0（App_EngineManager_Run）で監視する 2 区間:

| 区間 | 許容範囲 | 検出する異常 |
|---|---|---|
| START→END | 0〜500ms | `Run()` 1 回分の処理が異常に遅い（無限ループ・ブロッキング処理） |
| END→START | 2500〜3500ms | 次サイクルの `Run()` 呼び出しがタスク周期 3000ms から大きくズレている |

START→END は通常 RTE 読み取り・状態遷移・CAN 送信トリガのみで数 ms 程度のはずなので、
下限 (MinMs) は実質的な意味を持たず 0 にしています。一方 END→START はタスク周期
3000ms を中心に ±500ms の許容幅を持たせており、協調スケジューラ（他タスクの実行で
多少のジッタが生じる）を前提にした現実的な範囲です。

> Alive Supervision との違い: Alive は「6000ms の間に 1 回以上呼ばれたか」という
> 粗い判定しかできませんが、Deadline は「正確に何 ms かかったか」を見るため、
> Alive では検出できない「動いてはいるが異常に遅い」状態を検出できます。

## 複数 Supervised Entity（Entity 0: ENGINE / Entity 1: WARNING）

これまで WdgM は `App_EngineManager_Run()`（Entity 0）1 つしか監視しておらず、
「エンティティごとの独立したローカル判定 → 全エンティティを見たグローバル判定」
という WdgM 本来の構成が実機で一度も動いていませんでした。`WdgM.c` のコアロジック
（`WdgM_MainFunction` / `WdgM_TriggerHwWatchdog` / `WdgM_CheckpointReached`）は
最初から `WdgM_Cfg->EntityCount` を見て汎用的にループする作りだったため、
`App_WarningIndicator_Run()`（500ms 周期の警告灯タスク）を Entity 1 として
追加登録するだけで、この構成を確認できます。

| | Entity 0 (ENGINE) | Entity 1 (WARNING) |
|---|---|---|
| 監視対象 Runnable | `App_EngineManager_Run` | `App_WarningIndicator_Run` |
| 周期 | 3000ms | 500ms |
| Alive 判定サイクル | 6000ms（共通） | 6000ms（共通） |
| サイクル内の期待呼び出し回数 | 期待値 約2回中 最小1回 | 期待値 約12回中 最小6回 |
| Deadline (START→END) | 0〜500ms | 0〜200ms |
| Deadline (END→START) | 2500〜4500ms | 300〜1500ms |

2 つのエンティティは周期もチェックポイント ID も完全に独立しており
（`WdgM_LastCheckpoint[]` 等はエンティティごとの配列）、どちらか一方の
Alive/Logical/Deadline Supervision が FAILED になっても、もう一方の判定には
一切影響しません。一方で **グローバル判定**（実際に HW ウォッチドッグを
リフレッシュするかどうか）は両エンティティの結果を集約します。

```
WdgM_TriggerHwWatchdog()（1000ms 周期）:
  for each entity (ENGINE, WARNING):
    WdgM_GetLocalStatus(entity) != OK ?
      YES → allOk = false; break
  allOk == true ?
    YES → WdgIf_SetTriggerCondition()   ← ENGINE・WARNING 両方が OK の場合のみリフレッシュ
    NO  → 何もしない                     ← どちらか一方でも FAILED ならリフレッシュを止める
```

> **END→START の許容上限には他モジュール由来の遅延を見込んだ余裕がある**:
> NvM の EEPROM 書き込み（`Dem_ReportErrorStatus()` からの DTC 確定時）は
> ブロッキング処理のため、DTC 確定のたびに協調スケジューラが数百ms 単位で
> 止まりえます。これは WARNING タスク自身の異常ではなく他 BSW モジュール
> （Dem/NvM）由来の遅延のため、ENGINE・WARNING 双方の END→START 上限
> （下表参照）に実測値の約2倍の余裕を持たせています。
> 500ms 周期の WARNING は 3000ms 周期の ENGINE よりチェックポイント報告の
> 間隔が短いため、この種の一時的なブロッキングの影響を相対的に受けやすい点に
> 留意してください。
>
> `Os_SchedulerStep()` は各タスクの周期判定のたびに時間源（Os 専用の Gpt
> チャネル、`Os_GetTimeMs()` 経由の `Gpt_GetTimeElapsed()`。2026-08 に
> `millis()` から置き換え、詳細は README の「Os のスケジューラティック」参照）を
> 都度取得し直します（ループ先頭で 1 回だけ取得して使い回す実装だと、同一
> スキャン内で他タスクがブロッキングした際に後続タスクの `Os_LastRunMs[]`
> へ不正確な時刻が記録され、Deadline 判定を誤らせます）。また `Os_SetTaskActive()` は
> タスクを無効→有効へ切り替える瞬間に `Os_LastRunMs[]` を現在時刻へリセット
> します。これにより、長時間無効化されていたどのタスク（SHUTDOWN 中に
> 停止していた `WdgM_MainFunction` を含む）も、再開直後は必ずフルの周期を
> 待ってから初めて実行・評価されます。これらの設計に至った実機不具合の経緯は
> [DEVLOG](../DEVLOG.md#wdgm-deadline-supervision-上限緩和と-os_schedulerstep-のバグ) を参照。

## HW ウォッチドッグ連携（実際の MCU リセット）

WdgM は実ハードウェアウォッチドッグと連携していますが、直接は触れません。
`WdgM → WdgIf（ディスパッチ層）→ Wdg（下位ドライバ）→ Wdg_Hw（Renesas RA
の WDT ライブラリをラップする HAL 層）` という 4 層構成を経由します
（NvM → MemIf → Fee → Fee_Hw と同じ構成。WdgIf は実 AUTOSAR 仕様上、
下位ドライバが Wdg 1 個のみの構成では単なるパススルーでよいとされる
（[SWS_WdgIf_00018]）が、MemIf と同じ理由でチェック自体は残している）。
シミュレーションではなく、実機上で実際にリセットが発生します。

判定（Alive/Logical/Deadline Supervision）とリフレッシュ（trigger）は
**意図的に別々の周期**で動きます。

```
EcuM_Init() 内、WdgM_Init() より前:
  Wdg_Init(&Wdg_Config)   ← コンフィグ（タイムアウト値）を記録するのみ。
                            HW にはまだ触れない（初期化処理自体が HW
                            ウォッチドッグのタイムアウトに巻き込まれないため）

WdgM_Init()（起動シーケンス末尾、Os_Init の直前）:
  WdgM_EnableHwWatchdog()
    → WdgIf_SetMode(WDGIF_DEVICE_0, WDGIF_FAST_MODE)
      → Wdg_SetMode(WDGIF_FAST_MODE)
        → Wdg_Hw_Enable(timeoutMs)   ← HW ウォッチドッグを 4000ms タイムアウトで有効化

WdgM_MainFunction()（6000ms 周期、判定のみ）:
  各エンティティの Alive/Logical/Deadline を評価し WdgM_AliveStatus 等に反映する。
  1 つでも FAILED な判定サイクルが続くたびに WdgM_ExpiredCycleCount を進め、
  WDGM_EXPIRED_SUPERVISION_CYCLE_TOL（既定 2）回を超えて初めて
  WdgM_GlobalStopped を立てる（詳細は次項）。ここでは HW ウォッチドッグに触れない。

WdgM_TriggerHwWatchdog()（1000ms 周期、リフレッシュのみ）:
  WdgM_GlobalStopped が立っていない ?
    YES → WdgIf_SetTriggerCondition(WDGIF_DEVICE_0, WDGM_HW_WATCHDOG_TIMEOUT_MS)
            → Wdg_SetTriggerCondition() → Wdg_Hw_Refresh()
            ← リフレッシュ。タイマが 0 から再カウント開始
    NO  → 何もしない              ← リフレッシュされず、カウントが進み続ける

リフレッシュされないまま 4000ms 経過 → HW が MCU を強制リセット
  → setup() から再起動（DET ログも最初から出力される）
```

**Wdg_SetMode(WDGIF_OFF_MODE) は常に失敗する**: `WdgM_DisableHwWatchdog()`
（POST_RUN 遷移時に呼ばれる）は内部で `WdgIf_SetMode(WDGIF_DEVICE_0,
WDGIF_OFF_MODE)` を呼ぶが、Renesas RA4M1 の IWDT は一度有効化すると
無効化する手段がないため、`Wdg_SetMode()` は常に `E_NOT_OK` を返す
（実 AUTOSAR の拡張プロダクションエラー `WDG_E_DISABLE_REJECTED` に相当する
状況。本プロジェクトはプロダクションエラーの仕組み自体を持たないため
`DET_LOGW` のみで通知する）。`WdgM_DisableHwWatchdog()` はこの戻り値を
無視し、`WdgM_SupervisionSuppressed` フラグを立てることで目的を達成する
（HW が物理的に無効化されたかどうかには依存しない設計。詳細は WdgM.c の
「HW ウォッチドッグ連携」コメント参照）。

**なぜ判定サイクルとリフレッシュ周期を分けているか:**
当初は AVR の `wdt_enable(WDTO_8S)`（8000ms）を前提に、`WdgM_MainFunction`
（6000ms 周期）が判定とリフレッシュを両方担っていました（タイムアウトが
監視サイクルより長ければそれで十分機能する）。しかし Arduino Uno R4 WiFi
（Renesas RA4M1）移行時、RA の IWDT（独立ウォッチドッグ）は最大タイムアウトが
約 5592ms しかなく、6000ms の判定サイクルに直接リフレッシュを同期させることが
仕様上不可能でした（判定が終わる前にタイムアウトしてしまう）。

これは実車の AUTOSAR WdgM が、Wdg への trigger 周期と `WdgMSupervisionCycle`
（判定周期）を別々に設定できる設計になっているのと同じ理由です。そこで本実装も
リフレッシュ専用の軽量タスク `WdgM_TriggerHwWatchdog`（1000ms 周期、判定結果を
参照するだけ）を新設し、判定は従来通り 6000ms 周期のまま、リフレッシュだけを
HW タイムアウト（4000ms）に対して十分短い周期で行うようにしました。
副次効果として、Logical/Deadline 違反発生からリフレッシュ停止までの遅延も
最大 6000ms → 最大 1000ms に縮まっています。

## グローバルレベルの EXPIRED 許容サイクル

AUTOSAR 仕様（`[SWS_WdgM_00119]`〜`[SWS_WdgM_00121]`）は、Global Supervision
Status が `WDGM_GLOBAL_STATUS_OK`・`FAILED`・`EXPIRED` のいずれであっても
`WdgIf_SetTriggerCondition`（リフレッシュ相当）を同一に呼び続けることを要求して
おり、リフレッシュを 0（停止）にしてよいのは `[SWS_WdgM_00122]`
`WDGM_GLOBAL_STATUS_STOPPED` に到達したときだけです。STOPPED に到達するには
`WdgMExpiredSupervisionCycleTol`（グローバルレベルの EXPIRED 許容サイクル数）
分の判定サイクルを消費する必要があり（`[SWS_WdgM_00216]`/`[SWS_WdgM_00217]`
等）、単発の異常でいきなりリフレッシュを止めることは想定されていません。

これを表現するため、`WdgM_ExpiredCycleCount`（グローバルレベルの連続 FAILED
判定サイクル数）と `WdgM_GlobalStopped`（AUTOSAR の `WDGM_GLOBAL_STATUS_STOPPED`
相当）を持たせています。本実装は Local Supervision Status を OK/FAILED の
2 値に簡略化しており（仕様本来の FAILED/EXPIRED の区別や、per-SE の
`WdgMFailedAliveSupervisionRefCycleTol` は実装していません）、その代わりに
この 1 段のグローバル許容サイクル数（`WDGM_EXPIRED_SUPERVISION_CYCLE_TOL`、
既定 2）だけを持たせています（この機構を追加するに至った実機不具合の経緯は
[DEVLOG](../DEVLOG.md#wdgm-グローバル-expired-許容サイクルの追加) 参照）。

```
WdgM_MainFunction()（6000ms 周期）:
  いずれかのエンティティが FAILED ?
    YES → WdgM_ExpiredCycleCount < TOL ?
            YES → WdgM_ExpiredCycleCount++          （猶予中、リフレッシュ継続）
            NO  → WdgM_GlobalStopped = 1             （猶予を使い切った）
    NO  → WdgM_ExpiredCycleCount = 0, WdgM_GlobalStopped = 0   （全回復）

WdgM_TriggerHwWatchdog()（1000ms 周期）:
  WdgM_GlobalStopped ?
    YES → リフレッシュしない（HW タイムアウト後に実際にリセット）
    NO  → リフレッシュする（FAILED 判定中でも、猶予の範囲内なら継続）
```

`WdgM_GetLocalStatus()` 自体（各エンティティの真の Supervision 結果）は
この猶予とは無関係に、これまで通り即座に正確な値を返します。変わるのは
「その判定結果を受けて実際に HW ウォッチドッグのリフレッシュを止めるまでの
猶予」だけです。

## グローバル猶予カウンタは resume でリセットしない

Logical/Deadline Supervision のステータスはそもそも `WdgM_Init` まで回復しない
ラッチ式の設計です。それを評価するグローバル猶予カウンタ
（`WdgM_ExpiredCycleCount`/`WdgM_GlobalStopped`）を RUN 復帰のたびに回復させて
しまうと、本プロジェクトのようにボランタリスリープが数十秒おきに発生する環境では、
恒久的な違反があっても `WdgM_GlobalStopped` に到達する前に必ず次のスリープが来て
猶予がリセットされ続け、フェイルセーフが実質的に機能しなくなります（この
非対称性が引き起こした実機不具合の経緯は
[DEVLOG](../DEVLOG.md#wdgm-グローバル猶予カウンタを-resume-でリセットしてはいけなかった) 参照）。

そのため `WdgM_ResumeSupervision()` はこの 2 つをリセットせず、真に全エンティティが
OK に戻ったとき（`WdgM_MainFunction()` 末尾の自然な回復判定）にのみクリアします。
これにより、恒久的な違反は何回スリープ/ウェイクアップを挟んでも判定サイクル換算で
着実に猶予を消費し続け、いずれ確実に `WdgM_GlobalStopped` に到達します。

あわせて、`WdgM_SupervisionSuppressed`（POST_RUN 中の想定内の Alive 不足を無視する
フラグ）が立っている間は、グローバル猶予カウンタの判定自体を凍結します（進めも
回復させもしない）。POST_RUN 中の Rte_Engine/Rte_Warning 停止による想定内の
Alive 不足が、POST_RUN の頻度や長さ次第でグローバル猶予を無関係に消費してしまう
ことを防ぐためです。

```
WdgM_ResumeSupervision()（RUN 復帰のたびに呼ばれる）:
  AliveCount/AliveStatus  ← リセットする（POST_RUN 中の想定内の不足のため）
  ExpiredCycleCount/GlobalStopped ← リセットしない（恒久的な違反を見逃さないため）

WdgM_MainFunction()（6000ms 周期の判定サイクル）:
  いずれかのエンティティが FAILED ?
    かつ WdgM_SupervisionSuppressed 中 → 猶予カウンタは凍結（進めない）
    かつ 抑制されていない            → 猶予カウンタを消費（上記の通常フロー）
    （全 OK）                        → 猶予カウンタをクリア
```

## ブートローダ起因の無限リセットループ対策

MCU によっては、短いタイムアウトで WDT が有効なまま再起動すると、ブートローダの
待機中に再度タイムアウトしてスケッチに到達できない「無限リセットループ」に陥る
既知の問題があります（AVR で顕著）。これを防ぐため `main.cpp` の `setup()` の
最初で `Mcu_Hw_ReadAndClearResetReason()`（リセット原因取得、レジスタはクリア
される）→ `Mcu_Hw_DisableWatchdogAtBoot()`（WDT 無効化）を実行し、
`WdgM_Init()` が後から安全なタイムアウトで再度有効化します。
Renesas RA は WDT が `WDT.begin()` を呼ぶまで動作しないため、
`Mcu_Hw_DisableWatchdogAtBoot()` は RA では no-op です。

## 意図的な POST_RUN 移行での無効化／RUN 復帰での再有効化

EcuM が RUN から POST_RUN へ遷移する際、`WdgM_DisableHwWatchdog()` を呼んで
HW ウォッチドッグを無効化します。POST_RUN では BswM Rule 1 によって Rte_Engine /
Rte_Warning タスク（WdgM の監視対象、Entity 0/1 双方）が意図的に停止するため、
両エンティティとも Alive Supervision は必ず FAILED になります。

無効化するタイミングを **SHUTDOWN ではなく POST_RUN 移行時**にしているのには理由が
あります。WdgM はタスクとしては POST_RUN 中も継続するため（CanTp/Com/IoHwAb と同じ
BSW タスク）、無効化しないと POST_RUN 中（最大 `ECUM_POST_RUN_TIMEOUT_MS`=5000ms）に
Alive Supervision が FAILED を検出し続け、リフレッシュが止まったままになります。
HW ウォッチドッグのタイムアウト（4000ms）は SHUTDOWN への遷移
（POST_RUN 開始から最大 5000ms 後）より短いため、無効化しなければほぼ確実に
「正常なシャットダウン処理中」のはずが予期しないリセットを起こしてしまいます。
POST_RUN への移行そのものを無効化のタイミングにすることで、この競合を避けています。

ボランタリスリープからのウェイクアップ等で POST_RUN/SHUTDOWN から RUN へ復帰した
場合は、`WdgM_EnableHwWatchdog()` で再度有効化し、Alive Supervision による監視を
再開します（Bus-Off 回復は L1/L2 バックオフで無期限に継続し RUN を解放しないため、
この経路で POST_RUN に入ることはありません）。

## RUN 復帰時のリセット（`WdgM_ResumeSupervision()`）

POST_RUN 中は Rte_Engine / Rte_Warning タスクが意図的に停止するため、この間の
チェックポイント未到達・Alive 不足はいずれも「想定内」です。しかし SHUTDOWN や
POST_RUN からそのまま RUN へ復帰した直後にこれを正しく扱わないと、停止していた
だけの期間を実際の違反と誤検出してしまいます。そのため `EcuM_RequestRUN()` が
POST_RUN/SHUTDOWN→RUN へ遷移する際に `WdgM_ResumeSupervision()` を呼び、
以下をリセットします。

```
WdgM_ResumeSupervision():
  WdgM_LastCheckpoint[]       ← WDGM_CP_INITIAL にリセット
                                （再開後最初の遷移を「基準なし」として扱い、
                                 POST_RUN の停止時間を Deadline 違反と誤検出しない）
  WdgM_LastCheckpointTimeMs[] ← 現在時刻にリセット
  WdgM_AliveCount[]           ← 0 にリセット
  WdgM_AliveStatus[]          ← WDGM_LOCAL_STATUS_OK にリセット
                                （POST_RUN 中に付いた FAILED ラッチを RUN 復帰後まで
                                 持ち越さない）
  WdgM_SkipNextAliveJudgment  ← 1
                                （次回の WdgM_MainFunction 呼び出し 1 回分だけ
                                 Alive 判定自体をスキップする。POST_RUN が
                                 WDGM_SUPERVISION_CYCLE_MS より大幅に短いと、
                                 各エンティティがまだ一度もチェックインできて
                                 いないうちに判定サイクルが来てしまうため）
```

Logical/Deadline のラッチ済み FAILED 状態、およびグローバル猶予カウンタ
（前述）はリセットしません。前者は「停止前に本当に違反していた事実」を、
後者は「恒久的な違反を見逃さない」ことをそれぞれ優先するためです。

この一連のリセット処理は、Deadline Supervision 追加時・CommunicationControl
実機検証時・短時間 POST_RUN のシナリオそれぞれで実際に HW ウォッチドッグ
リセットを引き起こした 3 件の不具合を経て現在の形になりました。詳しい経緯は
[DEVLOG: POST_RUN→RUN 復帰時の Deadline Supervision 誤検出](../DEVLOG.md#wdgm-post_runrun-復帰時の-deadline-supervision-誤検出)、
[Alive Supervision 誤検出](../DEVLOG.md#wdgm-post_runrun-復帰時の-alive-supervision-誤検出)、
[短時間 POST_RUN での誤検出](../DEVLOG.md#wdgm-短時間-post_run-での-alive-supervision-誤検出)
を参照してください。

## 本プロジェクトでの失敗アクション

| 環境 | 失敗時のアクション |
|---|---|
| 本プロジェクト（Arduino Uno R4 WiFi） | HW ウォッチドッグのリフレッシュが止まり、最大 4000ms 後に実際に MCU がリセットされる |
| 実機製品 | 同様に HW ウォッチドッグがリフレッシュを停止し、タイムアウト後にシステムリセット（本実装と同じ仕組み） |

Alive・Logical・Deadline の 3 つの Supervision は、それぞれ独立したステータス
（`WdgM_AliveStatus[]` / `WdgM_LogicalStatus[]` / `WdgM_DeadlineStatus[]`）で
管理されます。`WdgM_GetLocalStatus()` と HW ウォッチドッグの refresh 判定は、
いずれか一つでも FAILED なら FAILED として扱います。

- `WdgM_AliveStatus` は周期ごとに再評価され、Alive 条件を満たせば OK に戻ります。
- `WdgM_LogicalStatus` / `WdgM_DeadlineStatus` は `WdgM_Init()` までラッチされ、
  `WdgM_MainFunction` の周期処理では自動的に OK へ戻りません（違反が起きたという
  事実は、その後 Alive 条件を満たしても消えないため）。

Alive・Logical・Deadline を独立したステータス配列に分けているのは、単一の
統合ステータスにすると「Logical 違反の直後に Alive 条件さえ満たせば違反が
消えてしまう」問題が起こるためです（経緯は
[DEVLOG](../DEVLOG.md#wdgm-alive-と-logical-のステータス統合バグ) 参照）。
なお HW ウォッチドッグが有効な今は、いずれの FAILED も前述の通り通常
MainFunction の次サイクルを待たずにリセットに至るため、ステータスの遷移
そのものをログで観測できる場面は限られます。

## シリアルログ確認例

**正常時（6000ms ごと）:**
```
[19ms]    INFO  WdgM: HW watchdog enabled (4000ms)   ← WdgM_Init 内（起動時）
[1019ms]  DEBUG WdgM: HW watchdog refreshed          ← WdgM_TriggerHwWatchdog（1000ms 周期）
[2019ms]  DEBUG WdgM: HW watchdog refreshed
[3019ms]  DEBUG WdgM: HW watchdog refreshed
[4019ms]  DEBUG WdgM: HW watchdog refreshed
[6017ms]  DEBUG WdgM: SE0 alive OK alive=4   ← 3000ms 周期で Run() が 2 回、各 2 チェックポイント（WdgM_MainFunction、6000ms 周期）
```

**POST_RUN 移行時（意図的な停止。HW ウォッチドッグは無効化されるため実際のリセットは発生しない）:**
```
[30312ms] INFO  EcuM: ->POST_RUN timeout=5000ms
[30313ms] INFO  WdgM: HW watchdog disabled
[36312ms] WARN  WdgM: SE0 alive FAILED alive=0 (exp>=1) [HW WDT reset pending]  ← ソフト的には FAILED と記録されるが
                                                                                ← 無効化済みのため実際にはリセットしない
```

**Alive 失敗検知（RUN 中に実際の異常が起きた場合 — 後述の動作確認方法）:**
```
[5019ms]  DEBUG WdgM: HW watchdog refreshed          ← 最後に成功したリフレッシュ
[6017ms]  WARN  WdgM: SE0 alive FAILED alive=0 (exp>=1) [HW WDT reset pending]
[6018ms]  ERROR WdgM: HW watchdog NOT refreshed - reset imminent
                        ↑ 6000ms は 1000ms の倍数のため、同じ Os_SchedulerStep 内で
                          WdgM_MainFunction(Task7) → WdgM_TriggerHwWatchdog(Task10)
                          の順に実行され、直後に検知される
（最後の成功リフレッシュ(5019ms)から HW ウォッチドッグのタイムアウト 4000ms 後に到達）
[9019ms]  INFO  NvM: Init ok blocks=2     ← MCU が実際にリセットされ、setup() から再起動
[9020ms]  INFO  Port: Init pins=4
...
```

**Logical 失敗検知（後述の動作確認方法で START 呼び出しを止めた場合）:**
```
[3010ms] WARN  WdgM: SE0 logical FAILED cp 1->1 (unexpected) [HW WDT reset pending]
                                  └┘  └┘
                                  前回END  今回END（START がスキップされ END→END になった）
[4019ms] ERROR WdgM: HW watchdog NOT refreshed - reset imminent
            ↑ 次の WdgM_TriggerHwWatchdog（1000ms 周期）が最短 1000ms 以内に FAILED を
              検知して停止する（WdgM_MainFunction の 6000ms 周期を待たない）
（最後の成功リフレッシュから HW ウォッチドッグのタイムアウト 4000ms 後に MCU が実際に
 リセットされる。もし MainFunction の次サイクルがその前に実行されれば
 [6017ms] WARN WdgM: SE0 logical still FAILED (latched since violation) [HW WDT reset pending]
 も見えることがあるが、リフレッシュ停止の判定自体は WdgM_TriggerHwWatchdog が行う）
```

**Deadline 失敗検知（後述の動作確認方法で START→END に人為的な遅延を入れた場合）:**
```
[3011ms] WARN  WdgM: SE0 deadline FAILED cp 0->1 elapsed=1003 (exp 0..500) [HW WDT reset pending]
                                                  └┘            └──────┘
                                                  実際の経過時間   許容範囲 (START_TO_END)
（以降は Logical 失敗検知と同様、次の WdgM_TriggerHwWatchdog（最短 1000ms 以内）で
 リフレッシュが止まり、そこから HW ウォッチドッグのタイムアウト 4000ms 後に
 MCU が実際にリセットされる）
```

> **動作確認の前に**: 以下のテストは実機を**実際にリセット**させます。
> シリアルモニタには `EcuM: ->RUN` 等の起動ログが再び表示され、リセットされたことが
> わかります（EEPROM の DTC は NvM 経由で保持されるため消えません）。元に戻すには
> コメントを外して再度アップロードしてください。

**動作確認方法（Alive Supervision）:** `App_EngineManager.c` の END 側 `WdgM_CheckpointReached` をコメントアウト。
```c
/* (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END); */
```
起動後 6000ms で `alive=0` の FAILED ログが出てリフレッシュが止まり、最後の成功
リフレッシュから HW ウォッチドッグのタイムアウト（最大 4000ms）後に実際に MCU が
リセットされる。

**動作確認方法（Logical Supervision）:** START 側だけをコメントアウトすると、END→END の
順序違反が次の Run() 実行時に即座に検出される（MainFunction の周期を待たない）。
```c
/* (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START); */
```

**動作確認方法（Deadline Supervision）:** START チェックポイントの直後に `delay(1000)`
を追加すると、START→END の許容上限 500ms を超え、Run() 終了時の END チェックポイントで
即座に検出される（MainFunction の周期を待たない）。
```c
(void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);
delay(1000);  /* 動作確認用: 500ms の許容上限を超えさせる */
```

## WdgM 設定（`WdgM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `WDGM_SUPERVISED_ENTITY_COUNT` | 2 | 監視対象エンティティ数（ENGINE/WARNING） |
| `WDGM_ENTITY_ENGINE` / `WDGM_ENTITY_WARNING` | 0 / 1 | App_EngineManager_Run / App_WarningIndicator_Run のエンティティ ID |
| `WDGM_SUPERVISION_CYCLE_MS` | 6000 ms | Alive Supervision サイクル（WdgM_MainFunction 周期と一致、両エンティティ共通） |
| `WDGM_EXPECTED_ALIVE_INDICATIONS` (ENGINE) / `WDGM_WARNING_EXPECTED_ALIVE_INDICATIONS` | 1 / 6 | サイクル内の最小 CheckpointReached 呼び出し回数 |
| `WDGM_CP_ENGINE_START` / `_END` | 0 / 1 | ENGINE の Run() 開始直後・終了直前のチェックポイント ID |
| `WDGM_CP_WARNING_START` / `_END` | 0 / 1 | WARNING の Run() 開始直後・終了直前のチェックポイント ID |
| `WDGM_CP_INITIAL` | 0xFF | 起動直後（まだチェックポイント未報告）を示す特別な遷移元 ID |
| `WDGM_DEADLINE_START_TO_END_MIN_MS` / `_MAX_MS`（ENGINE） | 0 / 500 ms | START→END（Run() 1 回分）の許容経過時間 |
| `WDGM_DEADLINE_END_TO_START_MIN_MS` / `_MAX_MS`（ENGINE） | 2500 / 4500 ms | END→START（次サイクルまでの間隔）の許容経過時間 |
| `WDGM_WARNING_DEADLINE_START_TO_END_MIN_MS` / `_MAX_MS` | 0 / 200 ms | WARNING の START→END 許容経過時間 |
| `WDGM_WARNING_DEADLINE_END_TO_START_MIN_MS` / `_MAX_MS` | 300 / 1500 ms | WARNING の END→START 許容経過時間 |
| `WDGM_EXPIRED_SUPERVISION_CYCLE_TOL` | 2 | グローバルレベルの連続 FAILED 判定サイクル許容回数（超過で `WdgM_GlobalStopped`） |
| `WDGM_HW_TRIGGER_CYCLE_MS` | 1000 ms | HW ウォッチドッグへの実際のリフレッシュ周期（WdgM_TriggerHwWatchdog 周期と一致）。判定サイクル（`WDGM_SUPERVISION_CYCLE_MS`）とは意図的に分離 |
| `WDGM_HW_WATCHDOG_TIMEOUT_MS` | 4000 ms | 実 HW ウォッチドッグのタイムアウト。`Wdg_PBCfg.c` がこの値を直接引用して `Wdg_Config.DefaultTimeoutMs` を組み立て、`Wdg_Hw_Enable(timeoutMs)`（`Wdg_Hw.cpp`、`WDT.begin(timeoutMs)`）まで渡る。`WDGM_HW_TRIGGER_CYCLE_MS` より十分長く設定すること（RA4M1 の IWDT 最大タイムアウト ≒5592ms 未満という制約もある） |

許可遷移グラフは `WdgM_PBCfg.c` の `WdgM_EngineTransitions[]`、Deadline 許容範囲テーブルは
同ファイルの `WdgM_EngineDeadlines[]`（いずれもポストビルド設定）で管理します。

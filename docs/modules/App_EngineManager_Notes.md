# App_EngineManager（エンジン状態遷移 SW-C）

> [README](../../README.md) の「[アプリケーション](../../README.md#application)」節から分離。

エンジン状態遷移（OFF / STARTING / RUNNING / FAULT）・DTC 登録・CAN TX 要求を担う ASW
SW-Component。OFF 継続を検知して ComM へ通信不要（NO_COM）を要求するボランタリスリープ
判断も担う。

## エンジン状態遷移

```
          flag=1
  [OFF] ──────────> [STARTING]
    ^                  │  │  │  │
    │ flag=0           │  │  │  └── comm timeout ──> [FAULT]
    │                  │  │  └───── timeout(5s) ────> [FAULT]
    │                  │  └──────── flag=0 ──────────> [OFF]
    │        speed≥500 │
    │                  v
    │              [RUNNING]
    │                  │  │  │
    │    flag=0 ─────  ┘  │  └── comm timeout ──────> [FAULT]
    │                      └── temp≥100℃ or speed<100rpm
    │                                   ↓
    └──────── flag=0 ────────────── [FAULT]
                                      │
                              flag=0 or btn=1
                                      │
                                    [OFF]
```

| 状態 | 遷移条件 | 遷移先 |
|------|---------|--------|
| OFF | EngineOnFlag = 1 | STARTING |
| OFF | EngineOnFlag = 0 かつ EngineSpeed > 0 | FAULT（フラグなしで回転を検出） |
| STARTING | EngineSpeed ≥ 500 rpm | RUNNING |
| STARTING | 5 秒経過 | FAULT |
| STARTING | EngineOnFlag = 0 | OFF |
| STARTING | EngineInfo 受信タイムアウト（5 秒） | FAULT（通信断） |
| RUNNING | CoolantTemp ≥ 100 ℃ | FAULT（過熱） |
| RUNNING | EngineSpeed < 100 rpm | FAULT（エンスト） |
| RUNNING | EngineOnFlag = 0 | OFF |
| RUNNING | EngineInfo 受信タイムアウト（5 秒） | FAULT（通信断） |
| FAULT | EngineOnFlag = 0 | OFF |
| FAULT | 警告確認ボタン押下（D9） | OFF（`FIM_FID_BUTTON_ACK` 抑止中は無視） |

`App_EngineManager_Run`（3000ms 周期、Os Task 2）は、遷移を判定する前に
`EngineInfo` の受信タイムアウト（`RTE_E_COM_STOPPED`）を先に判定します。STARTING / RUNNING 中に
タイムアウトしていれば FAULT へ遷移して `DEM_EVENT_COMM_TIMEOUT` を報告し、その周期は状態ハンドラ
（上の表の遷移）を実行しません。OFF / FAULT 中は通信が無いのが正常（ボランタリスリープ等）なため、
タイムアウトしていても FAILED を報告せず遷移もしません。E2E ハードエラー
（`RTE_E_HARD_TRANSFORMER_ERROR`）は観測ログを出すだけで、遷移にも Dem にも影響させません
（Dem への報告は E2EXf が行う。[`E2EXf_Notes.md`](./E2EXf_Notes.md) 参照）。

## Dem への報告（遷移に伴うイベント）

| イベント（DTC） | FAILED になる条件 | PASSED になる条件 |
|---------------|-------------------|-------------------|
| `DEM_EVENT_ENGINE_OVERHEAT`（0x000101） | RUNNING で CoolantTemp ≥ 100 ℃ | RUNNING で正常、またはストール側で FAILED のとき |
| `DEM_EVENT_ENGINE_STALL`（0x000102） | RUNNING で EngineSpeed < 100 rpm | RUNNING で正常、または過熱側で FAILED のとき |
| `DEM_EVENT_ENGINE_SPEED_NO_FLAG`（0x000103） | OFF で EngineOnFlag=0 かつ EngineSpeed > 0 | OFF で回転なし、または OFF→STARTING |
| `DEM_EVENT_STARTING_TIMEOUT`（0x000104） | STARTING が 5 秒続いても RUNNING にならない | STARTING→RUNNING |
| `DEM_EVENT_COMM_TIMEOUT`（0x000105） | STARTING / RUNNING 中の EngineInfo 受信タイムアウト | 正常受信の周期ごと |

また毎周期 `Dem_SetFreezeFrameContext()` で現在の回転数・水温・状態を Dem へ渡し、FAILED 遷移時の
フリーズフレームとして記録されます。FAULT から OFF への復帰（`EngineOnFlag=0`、または警告確認ボタン）
では FAILED を取り消しません。過熱・ストール・起動タイムアウトの PASSED は RUNNING に入らないと
報告されないため、OFF に戻ってもこれらの TF ビットは次に RUNNING になるまで残ります。ボタンの受理は
`FIM_FID_BUTTON_ACK` が許可されているときだけで、ボタン固着（`DEM_EVENT_BUTTON_STUCK`）の確定中は
`FAULT->OFF btn=1 inhibited (FiM)` として無視します。ボタン判定は通信状態に依らず毎周期行います。

## ボランタリスリープ判断

OFF が `APP_ENGINE_SLEEP_OFF_CYCLES`（5 周期 = 約 15 秒）続くと、`ComM_RequestComMode(NO_COMMUNICATION)`
で通信不要を要求します（Dcm が extendedSession 中なら ComM の集約結果は FULL_COM のまま）。OFF 以外に
なったら即座に FULL_COM を要求し直します。ComM が NO_COM から FULL_COM へ復帰した周期は 1 サイクル分の
猶予として OFF カウンタを 0 に戻し、判断をスキップします（SILENT_COM からの復帰は対象外）。協調スリープの
続きは [`ComM_Notes.md`](./ComM_Notes.md) と [`CanNm_Notes.md`](./CanNm_Notes.md) を参照してください。

## その他の出力

- `Rte_Write_EngineStatus_EngineState()` で WarningIndicator 向けに状態を公開します。
- `MeterStatus`（CAN 0x200）へ EngineSpeed / CoolantTemp をミラー送信します（受信タイムアウト中も、
  最後に検証できた値をそのまま送る）。
- ADC 電圧は参考値としてデバッグログに出すだけです（異常判定は IoHwAb が Dem へ報告する）。
- 実行完了ごとに `WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END)` を呼び、
  WdgM の Alive / Logical Supervision の対象になります。

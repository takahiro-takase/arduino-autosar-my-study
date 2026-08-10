# App_WarningIndicator（警告灯 SW-C）

> [README](../../README.md) の「[アプリケーション](../../README.md#application)」節から分離。

`Rte_ScheduleWarningIndicator` タスクが 500ms 周期で `App_WarningIndicator_Run` を起動します。
3 つの LED は互いに独立して制御され、状態の組み合わせを同時に表現できます。

| LED | ピン | 点灯条件 | 制御 API |
|-----|------|---------|---------|
| RUNNING 灯 | D6 | `EngineState == RUNNING` かつ `FIM_FID_RUNNING_LED` 許可中 | `IoHwAb_LedRunning_SetLevel` |
| FAULT 灯 | D7 | `EngineState == FAULT`（500ms 点滅） | `IoHwAb_LedFault_SetLevel`（毎 Runnable でトグル） |
| ABS 灯 | D8 | `AbsActive == 1` | `IoHwAb_Led_SetLevel` |

FAULT 中に AbsActive=1 のフレームを受信すると D7 が点滅しつつ D8 も同時に点灯します。
POST_RUN 遷移後は Rte_Warning タスクが停止し、LED は消灯状態で固定されます。

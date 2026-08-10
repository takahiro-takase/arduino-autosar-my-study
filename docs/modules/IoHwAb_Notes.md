# IoHwAb（I/O ハードウェア抽象化）

> [README](../../README.md) の「[IO スタック](../../README.md#io-stack)」節から分離。

`IoHwAb_MainFunction`（10ms 周期）が Dio / Adc の生値取得からデバウンス・固着検出・
電圧監視までを一手に担い、SW-C へは確定済みの値だけを静的変数経由で返します。

<a id="debounce"></a>
## デバウンス（積分カウンタ方式）

`IoHwAb_MainFunction` が 10ms 周期で `Dio_ReadChannel` を呼び出し、生レベルを積算します。

```
10ms ごとに (IoHwAb_MainFunction):
  rawLevel = (Dio_ReadChannel(D9) == LOW) ? 1 : 0   ← INPUT_PULLUP 反転

  if rawLevel == s_confirmedLevel:
    s_debounceCounter = 0                             ← 安定、リセット
  else:
    s_debounceCounter++
    if s_debounceCounter >= 4:                        ← 4 × 10ms = 40ms 連続変化
      s_confirmedLevel = rawLevel
      INFO: "Button confirmed level=1"

App_EngineManager_Run が読み取る:
  Rte_Call_Button_GetLevel(&btn)
    → IoHwAb_Button_GetLevel()
        → s_confirmedLevel を返す（Dio_ReadChannel は呼ばない）
```

`Dio_ReadChannel` の呼び出しは `IoHwAb_MainFunction` に集中しているため、
`IoHwAb_Button_GetLevel` は静的変数を返すだけです。

<a id="button-stuck"></a>
## ボタン固着検出

確定押下状態（`s_confirmedLevel == 1`）が 5000ms（= 500 × 10ms）継続すると Dem にエラーを報告します。
この 5 秒間の固着判定そのものが十分な持続性チェックのため、Dem 側は
`DEM_DEBOUNCE_LIMIT_BUTTON_STUCK=1` で 1 回の報告を即座に確定します。

```
確定押下が継続するたびに (IoHwAb_MainFunction):
  s_stuckCounter++
  s_stuckCounter == 500?
    → Dem_ReportErrorStatus(DEM_EVENT_BUTTON_STUCK, FAILED)
    → WARN: "Button stuck dtc=0x000106"      ← DTC 0x000106 が即座に確定・EEPROM に保存

ボタン解放時:
  if s_stuckCounter >= 500:
    → Dem_ReportErrorStatus(DEM_EVENT_BUTTON_STUCK, PASSED)
    → INFO: "Button stuck cleared"            ← TF が即座にクリア（CDTC は残る）
  s_stuckCounter = 0
```

固着判定後にボタンを解放すると PASSED が報告され、TF ビットはクリアされます（CDTC は残る）。

<a id="adc-monitoring"></a>
## ADC センサ電圧監視

`IoHwAb_MainFunction` が 10ms 周期で `Adc_ReadChannel` を呼び出し、10-bit 生値を mV へ変換して
電圧低下を Dem へ報告します。`Dio_ReadChannel` と同様に、ADC アクセスも `IoHwAb_MainFunction` に
集約し、`IoHwAb_Adc_GetValue_mV` は変換済みの静的変数を返すだけにしています
（チャネル設定は [`Adc_Notes.md`](./Adc_Notes.md) の `Adc_Cfg.h` 参照）。

```
10ms ごとに (IoHwAb_MainFunction):
  raw = Adc_ReadChannel(ADC_CHANNEL_SENSOR)        ← 0〜1023
  mv  = (uint32)raw * ADC_REF_VOLTAGE_MV / ADC_RESOLUTION_MAX

  mv < 1000 (IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV)?
    YES → Dem_ReportErrorStatus(DEM_EVENT_ADC_VOLT_LOW, FAILED)
    NO  → Dem_ReportErrorStatus(DEM_EVENT_ADC_VOLT_LOW, PASSED)

App_EngineManager_Run が読み取る:
  Rte_Call_Adc_GetValue_mV(&mv)
    → IoHwAb_Adc_GetValue_mV()
        → s_adcMv を返す（Adc_ReadChannel は呼ばない）
```

`(uint32)raw * ADC_REF_VOLTAGE_MV` は最大 1023 × 5000 = 5,115,000 となり uint16 を超えるため、
乗算前に uint32 へキャストしてオーバーフローを防いでいます。

毎サイクル FAILED/PASSED いずれかを報告するため、Dem 側のデバウンス確定（カウンタ 2 回分）は
電圧低下発生から数十 ms 以内に完了します。

<a id="iohwab-api"></a>
## IoHwAb API 一覧（`IoHwAb.h`）

| 関数 | 呼び出し元（RTE 経由） | 動作 |
|------|----------------------|------|
| `IoHwAb_Init()` | EcuM_Init | 全 LED を消灯、カウンタをリセット |
| `IoHwAb_LedRunning_SetLevel(level)` | App_WarningIndicator | D6 を点灯 / 消灯 |
| `IoHwAb_LedFault_SetLevel(level)` | App_WarningIndicator | D7 を点灯 / 消灯 |
| `IoHwAb_Led_SetLevel(level)` | App_WarningIndicator | D8 (ABS LED) を点灯 / 消灯 |
| `IoHwAb_MainFunction()` | Os Task 6 (10ms) | デバウンスサンプリング・固着検出・ADC サンプリング |
| `IoHwAb_Button_GetLevel(&level)` | App_EngineManager | デバウンス済み押下状態を返す |
| `IoHwAb_Adc_GetValue_mV(&mv)` | App_EngineManager | 変換済み ADC 電圧値 [mV] を返す |

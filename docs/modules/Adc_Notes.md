# Adc（ADC ドライバ）

> [README](../../README.md) の「[IO スタック](../../README.md#io-stack)」節から分離。

MCAL。AUTOSAR の ADC のグループ・バッファ方式の API で、10-bit アナログ生値（0–1023）を読み取ります。
1 グループ・1 チャネル・ソフトウェアトリガのワンショット変換だけを扱います。

```
Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buf)   ← 起動時に 1 回（IoHwAb_Init）。登録しないと変換を始められない
Adc_StartGroupConversion(ADC_GROUP_SENSOR)       ← 変換開始。Arduino の analogRead() が同期的に完了し、状態が ADC_STREAM_COMPLETED になる
Adc_ReadGroup(ADC_GROUP_SENSOR, &raw)            ← 結果を読み出す。読み出すと ADC_IDLE へ戻る（[SWS_Adc_00330]）
Adc_GetGroupStatus(ADC_GROUP_SENSOR)             ← 状態（ADC_IDLE / ADC_STREAM_COMPLETED）を返す
```

以前は、実仕様に存在しない独自 API の `Adc_ReadChannel()` で即時に読み取っていました（2026-10 に置き換え）。
エラーは、未初期化（`ADC_E_UNINIT`）、グループが範囲外（`ADC_E_PARAM_GROUP`）、ポインタが NULL（`ADC_E_PARAM_POINTER`）を開発エラー、
結果バッファが未登録（`ADC_E_BUFFER_UNINIT`）、変換中の再開始（`ADC_E_BUSY`）、変換前の読み出し（`ADC_E_IDLE`）を実行時エラーとして DET へ報告します。

<a id="adc-channel-config"></a>
## チャネル設定（`Adc_Cfg.h`）

| 定数 | 値 | 意味 |
|------|-----|------|
| `ADC_CHANNEL_SENSOR` | 0（A0） | アナログセンサ入力チャネル |
| `ADC_GROUP_SENSOR` / `ADC_GROUP_COUNT` | 0 / 1 | センサ入力のグループ（`ADC_CHANNEL_SENSOR` を変換）/ グループ数 |
| `ADC_RESOLUTION_MAX` | 1023 | 10-bit ADC の最大生値 |
| `ADC_REF_VOLTAGE_MV` | 5000 | 基準電圧（5V） |

このチャネルを読み取り mV へスケーリングする処理（オーバーフロー対策込み）は
`IoHwAb_MainFunction` が行います。詳細は [`IoHwAb_Notes.md`](./IoHwAb_Notes.md) の
ADC センサ電圧監視を参照してください。

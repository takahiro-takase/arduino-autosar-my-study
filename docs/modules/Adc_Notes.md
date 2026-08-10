# Adc（ADC ドライバ）

> [README](../../README.md) の「[IO スタック](../../README.md#io-stack)」節から分離。

MCAL。`Adc_ReadChannel` で 10-bit アナログ生値（0–1023）を読み取るのみ。

<a id="adc-channel-config"></a>
## チャネル設定（`Adc_Cfg.h`）

| 定数 | 値 | 意味 |
|------|-----|------|
| `ADC_CHANNEL_SENSOR` | 0（A0） | アナログセンサ入力チャネル |
| `ADC_RESOLUTION_MAX` | 1023 | 10-bit ADC の最大生値 |
| `ADC_REF_VOLTAGE_MV` | 5000 | 基準電圧（5V） |

このチャネルを読み取り mV へスケーリングする処理（オーバーフロー対策込み）は
`IoHwAb_MainFunction` が行います。詳細は [`IoHwAb_Notes.md`](./IoHwAb_Notes.md) の
ADC センサ電圧監視を参照してください。

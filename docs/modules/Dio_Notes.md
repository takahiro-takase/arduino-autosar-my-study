# Dio（デジタル I/O 値読み書き）

> [README](../../README.md) の「[IO スタック](../../README.md#io-stack)」節から分離。

MCAL。方向設定は Port が担い、Dio は `Dio_WriteChannel` / `Dio_ReadChannel` による値の読み書きのみを行う。

<a id="channel-assignment"></a>
## チャネル割り当て（`Dio_Cfg.h`）

| 定数 | Dio チャネル | Arduino ピン | 機能 | Port 方向 |
|------|------------|-------------|------|----------|
| `DIO_CHANNEL_LED_RUNNING` | 6 | D6 | RUNNING 灯（RUNNING 中点灯） | OUTPUT |
| `DIO_CHANNEL_LED_FAULT` | 7 | D7 | FAULT 灯（FAULT 中 500ms 点滅） | OUTPUT |
| `DIO_CHANNEL_LED_WARNING` | 8 | D8 | ABS 警告灯（AbsActive=1 で点灯） | OUTPUT |
| `DIO_CHANNEL_BUTTON` | 9 | D9 | 警告確認ボタン（FAULT→OFF 遷移） | INPUT_PULLUP |

ピン番号の変更は `Dio_Cfg.h` の定数を変えるだけで完了します（IoHwAb や SW-C の変更不要）。

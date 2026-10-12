# test_scripts（実機の結合テストスクリプト）

実機（Arduino UNO R4）に CAN で接続して動かす、結合テスト用のスクリプトです。
`tools/can_tool` の「スクリプト実行...」から実行します（Connect 済みの状態で、`.capl` または `.py` を選びます）。
スクリプトの書き方は [tools/can_tool/README.md](../tools/can_tool/README.md) の「CAPL 風スクリプト機能」を参照してください。

`test/` との違い:

| フォルダ | 実行する場所 | 相手 | 実行方法 |
| --- | --- | --- | --- |
| `test/` | PC 上 | ホスト用にビルドした BSW（GoogleTest） | `cmake --build --preset native-chain` など |
| `test_scripts/` | 実機 | ECU（CAN 経由） | can_tool の「スクリプト実行...」 |

## スクリプト

| ファイル | 内容 |
| --- | --- |
| `001_ecu_reset.capl` | UDS 0x11 ECUReset を送り、ECU が実際にリセットされることを確認する |
| `100_verify_nvm_writeall_on_reset.capl` | 0x14 全クリアの直後に 0x11 を送り、リセット後にクリアした DTC が復活しないこと（ECUReset 直前の `NvM_WriteAll()`）を確認する |
| `000_regression_normal_operation.capl` | 通常動作のデグレ確認（約 20 秒）。DTC を消去したあと、E2E Profile05 で保護した EngineInfo / AbsInfo を周期送信し、ECU の出力（MeterStatus の EngineState・EngineSpeed・CoolantTemp、WarningStatus の各ランプ、E2EHealthStatus のエラーカウンタ、TestMsg の連続性）を判定して PASS / FAIL を出す。ECU をリセットした直後に実行する |
| `200_diag_request_during_nm_sleep.capl` | NM の協調スリープ（SILENT_COM）に入った ECU へ TesterPresent を送り、起こして応答すること（[SWS_Dcm_01376]）と、起床後に ECU の周期送信が再開することを確認する（約 20〜40 秒）。実行中は他のフレームを送らない |
| `201_wakeup_validation_after_bus_sleep.capl` | 完全なスリープ（Bus-Sleep Mode）に入った ECU へ、受信設定に無い ID のダミーフレームを 1 回送り、起床要因の検証（`CanIf_CheckValidation()` → `EcuM_ValidateWakeupEvent()` → `ComM_EcuM_WakeUpIndication()` → `CanSM_RequestComMode(FULL_COM)`）を経て、ECU の周期送信が再開し診断要求に応答することを確認する（約 30〜60 秒）。途中経路は ECU のシリアルログで確認する。実行中は他のフレームを送らない |
| `dcm_cantp_tx_busy_check.py` | CanTp の送信中に届いた新規要求を Dcm が無視すること（[SWS_Dcm_00557]）を確認する |

文法の説明用のサンプル（`example_*.capl` など）は、[tools/can_tool/capl_scripts/](../tools/can_tool/capl_scripts/) にあります。

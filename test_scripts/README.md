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
| `dcm_cantp_tx_busy_check.py` | CanTp の送信中に届いた新規要求を Dcm が無視すること（[SWS_Dcm_00557]）を確認する |

文法の説明用のサンプル（`example_*.capl` など）は、[tools/can_tool/capl_scripts/](../tools/can_tool/capl_scripts/) にあります。

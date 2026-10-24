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
| `101_verify_e2e_profile05.capl` | E2E Profile05 の実機検証（約 40 秒）。ECU が周期送信する 0x220（E2EHealthStatus）の CRC16 と Counter を検証し、正常な EngineInfo ではエラーカウンタが増えないこと、CRC16 を誤らせると crcErr が +1、Counter を飛ばすと seqErr が +1 になることを確認する。テスト対象以外のメッセージは `setMessageLog()` で実行中だけ統合ログへの記録を止める |
| `102_verify_dcm_did.capl` | Dcm の DID 表（`Dcm.json` から生成）経由の UDS 0x22 / 0x2E / 0x2F の実機検証。読み出し/書き込み/ランプ制御の正常系（TestPattern の書き込み→読み戻し、マルチフレーム応答を含む）と、NRC 0x31（DID なし・機能なし）/0x13（長さ違い）/0x33（セキュリティ未解除）/0x7F（defaultSession）を確認する。0x22 は、複数 DID の要求（未対応の DID は応答から外れる、5 個以上と長さ違いは NRC 0x13）と、DID ごとの条件つきの DID 0x0109（defaultSession は NRC 0x31、未解除は NRC 0x33、解除後は正応答）も確認する。最後にランプを `returnControlToECU` で戻す |
| `103_verify_cantsyn_slave.capl` | CanTSyn の時間スレーブ（受信）の実機検証（約 30 秒）。CAN ID 0x241・時間領域 1 の SYNC/FUP（CRC-8H2F 付き）を外部の時間マスターの役で送り、FUP の CRC 不正・SC 不一致・待ち時間超過・別の時間領域では ECU が同期しないこと、正しい組で `base=1 synchronized` が出ること、7 秒の途絶で `sync loss timeout` が出て、再開で再び同期することを確認する。ECU の内部状態は CAN で読めないため、判定は ECU のシリアルログ（StbM の行）で目視する。ECU をリセットした直後に実行する |
| `104_verify_cantp_padding.capl` | DLC が 8 未満の診断フレームを受け付けないことの実機検証（約 8 秒）。DLC=8 の TesterPresent には応答し、`send_can()` で送る DLC=3 の SF（`02 3E 00`、パディングなし）には応答しないことを確認する。実機の配線では、DLC が設定と合わないフレームは CanTp に届く前に CanIf が捨てるため、ECU のシリアルログには CanIf の `length mismatch` と DET（M=60、ERR=0x3D）が出る（CanTp の `CANTP_E_PADDING` ではない）。CAN ツールの UDS ボタンは 8 バイトへパディングして送るため、この確認には使えない |
| `200_diag_request_during_nm_sleep.capl` | NM の協調スリープ（SILENT_COM）に入った ECU へ TesterPresent を送り、起こして応答すること（[SWS_Dcm_01376]）と、起床後に ECU の周期送信が再開することを確認する（約 20〜40 秒）。実行中は他のフレームを送らない |
| `201_wakeup_validation_after_bus_sleep.capl` | 完全なスリープ（Bus-Sleep Mode）に入った ECU へ、受信設定に無い ID のダミーフレームを 1 回送り、起床要因の検証（`CanIf_CheckValidation()` → `EcuM_ValidateWakeupEvent()` → `ComM_EcuM_WakeUpIndication()` → `CanSM_RequestComMode(FULL_COM)`）を経て、ECU の周期送信が再開し診断要求に応答することを確認する（約 30〜60 秒）。途中経路は ECU のシリアルログで確認する。実行中は他のフレームを送らない |
| `dcm_cantp_tx_busy_check.py` | CanTp の送信中に届いた新規要求を Dcm が無視すること（[SWS_Dcm_00557]）を確認する |

文法の説明用のサンプル（`example_*.capl` など）は、[tools/can_tool/capl_scripts/](../tools/can_tool/capl_scripts/) にあります。

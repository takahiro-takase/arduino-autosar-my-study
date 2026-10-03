# 設計上の注意点

> [README](../README.md) の補足から分離しました。

<a id="design-notes"></a>
# 設計上の注意点

<a id="c-cpp-boundary"></a>
## C / C++ 言語境界

| ファイル | 言語 | 理由 |
|---------|------|------|
| `Adc_Hw.cpp` | C++ | Arduino の `analogRead` API を使用 |
| `Can_Hw.cpp` | C++ | MCP_CAN クラスのインスタンス化に placement new が必要 |
| `Det_Hw.cpp` | C++ | Arduino の `Serial` API を使用 |
| `Dio_Hw.cpp` | C++ | Arduino の `digitalWrite` API を使用 |
| `Fee_Hw.cpp` | C++ | `EEPROM.h` の `EEPROMClass`（C++ クラス）を使用 |
| `Gpt_Hw.cpp` | C++ | Renesas RA の `FspTimer` クラスを使用 |
| `Port_Hw.cpp` | C++ | Arduino の `pinMode` API を使用 |
| `SchM_Hw.cpp` | C++ | Arduino の `noInterrupts()` / `interrupts()` を使用 |
| `Wdg_Hw.cpp` | C++ | Renesas RA の WDT ライブラリ（`WDTimer` クラス、グローバルインスタンス `WDT`）を使用 |
| `main.cpp` | C++ | Arduino の `setup()` / `loop()` と `Serial.begin()` |
| その他すべて | C | AUTOSAR CP の標準に準拠 |

C ファイルから C++ 関数を呼ぶすべてのヘッダに `extern "C"` ガードを設けています。

`Det_Hw.cpp` が唯一 `Serial.print()` を呼ぶファイルです。他の `.c` ファイルは `DET_LOG*` マクロのみを使います。

<a id="log-level"></a>
## ログレベルの抑制 (Det_Cfg.h)

`Det_Cfg.h` の `DET_LOG_LEVEL`（既定値 `LOG_I`）以下の重要度のログのみ出力されます
（`LogLevel` は数値が小さいほど重要度が高い: `LOG_T`=0（テスト専用トレース）< `LOG_E` < `LOG_W` < `LOG_I` < `LOG_D`）。
既定では ERROR/WARN/INFO のみ出力し、DEBUG（例: IoHwAb の ADC 電圧低下デバウンス過程など、
毎サイクル出力されうる詳細ログ）を抑制します。全レベル出力したい場合は
`platformio.ini` の `build_flags` に `-D DET_LOG_LEVEL=LOG_D` を追加してください。

<a id="fixed-buffer-size"></a>
## 固定長バッファのサイズは設定定数から計算する

`Dcm.c`（旧 `Dcm_Cbk.c`）の UDS 応答バッファ `Dcm_TxBuf` は、当初 `DEM_EVENT_COUNT`（その時点では 6）
から手計算した値に余裕を持たせた固定値 32 バイトで確保していました。
その後 `DEM_EVENT_COUNT` が 8 に増えた際、最大応答サイズの計算（SID 0x19/02 が
全イベント一致した場合 `3 + DEM_EVENT_COUNT×4` バイト）を更新し忘れ、
35 バイトの応答を 32 バイトのバッファへ書き込む実際に到達可能なバッファ
オーバーフローになっていました（DTC を 8 件同時に確定させると再現する）。

```c
/* 修正後: DEM_EVENT_COUNT に自動追従する数式で確保 */
#define DCM_TX_BUF_SIZE  (3U + (DEM_EVENT_COUNT * 4U))
static uint8 Dcm_TxBuf[DCM_TX_BUF_SIZE];
```

固定長バッファのサイズを「その時点で必要な値」を手計算した定数にすると、
後から参照先の設定定数（ここでは `DEM_EVENT_COUNT`）だけが増えてもコンパイラは
何も警告してくれません。サイズは可能な限り設定定数からの数式で導出し、
書き込みループにも防御的な境界チェックを入れる（`Dcm_HandleReadDtcByMask()`
参照）、という二重の対策にしています。

<a id="rx-tx-symmetry"></a>
## RX/TX で対称な入力検証

`CanIf_Transmit()`（TX）は `PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL`
を検証してから送信データを参照していますが、対応する受信経路
`CanIf_RxIndication()` → `PduR_ComRxIndication()` → `Com_RxIndication()` は
`PduInfoPtr == NULL` だけを見て `SduDataPtr` を検証しないまま
`Com_RxIndication()` が `PduInfoPtr->SduDataPtr[b]` を直接参照していました。

現在の呼び出し元 (`Can.c` の RX 処理) は常にスタック上の有効なバッファを渡すため
今すぐ問題になるわけではありませんが、関数自身のドキュメント
（「SduDataPtr も NULL 禁止」）を実際にコードで保証していない状態でした。
TX 側と同じ検証を RX の各層境界（CanIf → PduR → Com）にも追加し、
「ドキュメントが約束している契約は、呼び出し元の現状に頼らず関数自身が
保証する」という、FiM のフェールセーフ修正のときと同じ考え方を踏襲しています。

<a id="config-table-centralization"></a>
## 設定テーブルの一元管理

各モジュールの設定は対応する `*_PBCfg.c` ファイルで管理しています。

| 変更したい内容 | 編集ファイル |
|---|---|
| CAN ID・DLC（EngineInfo / AbsInfo など） | `CanIf_PBCfg.c` + `CanIf_Cfg.h` |
| シグナルのビット位置・エンディアン | `Com_PBCfg.c` + `Com_Cfg.h` |
| PDU ルーティングパス（RX/TX の対応関係） | `PduR_PBCfg.c` + `PduR_Cfg.h` |
| RTE ポート API（SW-C から見えるシグナル名） | `Rte.h` / `Rte.c` / `Rte_Type.h` |
| E2E チェック結果を Rte_IStatusType へ写像する分類の変更 | `Rte.c` の `Rte_MapE2EStatusP05()` |
| Com TX I-PDU の送信モード変更（DIRECT/MIXED/PERIODIC）・周期変更 | `Com_PBCfg.c` の該当 IPdu の `TxModeMode`/`TxPeriodMs`、周期定数は `Com_Cfg.h` |
| Com TMS（TxModeModeTrue への自動切り替え）変更・対象シグナル変更 | `Com_PBCfg.c` の該当 IPdu の `TxModeModeTrue`/`TxPeriodMsTrue`、対象シグナルの `TmsContributor`/`FilterX`/`Mask` |
| Com MDT（変化時送信の最小送信間隔）変更 | `Com_PBCfg.c` の該当 IPdu の `MinDelayMs`、周期定数は `Com_Cfg.h` |
| E2EMon（ネットワーク健全性テレメトリ）の集計対象・カウンタ追加 | `E2EMon.c` の `E2EMon_NotifyCheckResultP05()`（Profile05 用。Profile01 用の `E2EMon_NotifyCheckResult()` は参考実装として残している） |
| EEPROM アドレス・ブロックサイズ | `NvM_PBCfg.c` / `NvM_Cfg.h` |
| NvM ブロックの冗長化（Redundant Block）追加・変更 | `NvM_PBCfg.c` の該当ブロックの `Redundant`/`NvMNvBlockBaseNumberMirror`、ミラーアドレスは `NvM_Cfg.h` |
| Dem デバウンス閾値の変更（イベントごと） | `Dem_Cfg.h` の `DEM_DEBOUNCE_LIMIT_*` |
| Dem 経年回復（Aging）の閾値変更（イベントごと） | `Dem_Cfg.h` の `DEM_AGING_THRESHOLD_*` |
| **タスク周期・タスク追加/削除** | **`Os_PBCfg.c`** |
| EcuM POST_RUN タイムアウト・RUN ユーザ追加 | `EcuM_Cfg.h` |
| BswM ルール追加・タスクマスク変更 | `BswM_PBCfg.c` / `BswM_Cfg.h` |
| WdgM 監視サイクル・期待回数の変更 | `WdgM_Cfg.h` |
| WdgM 監視対象エンティティの追加 | `WdgM_PBCfg.c` に行を追加し `WDGM_SUPERVISED_ENTITY_COUNT` を更新 |
| WdgM 論理監視（許可されるチェックポイント順序）の変更 | `WdgM_PBCfg.c` の `WdgM_EngineTransitions[]` / チェックポイント ID は `WdgM_Cfg.h` |
| WdgM 時間監視（チェックポイント間の許容経過時間）の変更 | `WdgM_PBCfg.c` の `WdgM_EngineDeadlines[]` / 閾値は `WdgM_Cfg.h` の `WDGM_ENGINE_DEADLINE_*` / `WDGM_WARNING_DEADLINE_*` |
| LED / ボタンのピン番号変更 | `Dio_Cfg.h`（`DIO_CHANNEL_LED_RUNNING` / `_LED_FAULT` / `_LED_WARNING` / `_BUTTON`） |
| ADC チャネル・分解能・基準電圧・電圧低下閾値 | `Adc_Cfg.h` / `IoHwAb.c`（`IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV`） |
| Dcm S3 タイマのタイムアウト時間変更 | `Dcm_Cfg.h` の `DCM_S3_TIMEOUT_MS` |
| Dcm SecurityAccess の鍵・試行回数・ロックアウト時間変更 | `Dcm_Cfg.h` の `DCM_SECURITY_KEY_MASK` / `DCM_SECURITY_MAX_ATTEMPTS` / `DCM_SECURITY_DELAY_MS` |
| SecurityAccess で保護するサービスの追加 | `Dcm.c` の各ハンドラ先頭で `Dcm_SecurityLevel == 0U` をチェック（`Dcm_HandleClearDtc` 参照） |
| SID にセッション制約を追加・変更 | `Dcm.c` の `Dcm_SidSessionTable[]` に行を追加（`DCM_SESSION_MASK_DEFAULT` / `_EXTENDED` / `_ALL`） |
| IOControl (0x2F) 対象ランプの追加 | `Dcm_Cfg.h` に `DCM_DID_*` を追加、`Dcm_LampIdOfDid()` に分岐を追加、`Rte.h` の `Rte_LampIdType` に列挙値を追加し `Rte_Call_*_SetLevel()` を `Rte_Lamp_ArbitrateAndWrite()` 経由にする |
| ComM ユーザの追加 | `ComM_Cfg.h` に `COMM_USER_*` を追加し `COMM_USER_COUNT` を更新。要求元モジュールから `ComM_RequestComMode(新ユーザID, モード)` を呼ぶだけで `ComM_RequestComMode()` の集約ロジックが自動的に対応する |
| FiM の抑止対象機能・イベントの追加・変更 | `FiM_Cfg.h` に `FIM_FID_*` を追加し、`FiM_PBCfg.c` の `FiM_Functions[]` に行を追加 |
| CanSM Bus-Off L1/L2 バックオフの変更 | `CanSM_Cfg.h` の `CANSM_BUSOFF_RECOVERY_L1_MS` / `_L2_MS` / `CANSM_BUSOFF_L1_TO_L2_COUNT` |
| ウェイクアップ検証タイムアウトの変更 | `CanSM_Cfg.h` の `CANSM_WAKEUP_VALIDATION_MS`（既定 2000ms） |
| ボランタリスリープに入るまでのエンジン OFF 継続時間の変更 | `App_EngineManager.c` の `APP_ENGINE_SLEEP_OFF_CYCLES`（Run 周期3000ms×既定5=15秒） |

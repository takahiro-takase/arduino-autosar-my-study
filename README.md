# arduino-autosar-my-study

Arduino UNO R4 WiFi + MCP2515 + TJA1050 を用いて 
AUTOSAR CP の BSW CAN スタックを学習目的で実装したプロジェクトです。
ARXML や設定ツールは使用せず、コードで階層構造・型定義・設定テーブルを再現しています。

## 目次

- [概要](#overview)
- [ハードウェア](#hardware)
  - [ハードウェア構成](#hw-configuration)
    - [配線図](#wiring-diagram)
    - [MCP2515 接続（Arduino UNO）](#mcp2515-connection)
  - [ビルド環境・設定](#build-environment)
  - [ビルドと書き込み](#build-and-flash)
- [ソフトウェア](#software)
  - [アーキテクチャ](#architecture)
    - [層構造](#layer-structure)
    - [モジュール一覧](#module-list)
  - [ディレクトリ構成](#directory-structure)
  - [CAN 通信スタック（Can_Hw / Can / CanIf / PduR / Com / E2E / E2EXf / E2EMon / Rte）](#can-stack)
    - [CAN フレーム仕様](#can-frame-spec)
    - [処理の流れ（関数コールチェーンと多層防御）](#processing-flow)
      - [Tx 処理（Com → PduR → CanIf → Can の順）](#tx-processing)
      - [Rx 処理（Can → CanIf → PduR → Com の順）](#rx-processing)
    - [Can](#can-module)
    - [Com](#com-module)
    - [DET 準拠（Det_ReportError による標準化エラー通知）](#det-compliance)
  - [E2E P01 保護（EngineInfo/AbsInfo 受信 / E2EHealthStatus 送信）](#e2e-p01)
    - [I-PDU Group（Com_IpduGroupStart/Stop、通信のライフサイクル制御）](#ipdu-group)
    - [呼び出し元は BswM（実 AUTOSAR の標準構成）](#ipdu-group-caller)
    - [Com_IpduGroupStart/Stop が実際に行うこと](#ipdu-group-behavior)
    - [動作確認方法](#ipdu-group-verification)
    - [E2E が保護する故障モデル](#e2e-fault-model)
    - [受信側（Check）— EngineInfo / AbsInfo](#e2e-check-rx)
    - [送信側（Protect）— E2EHealthStatus](#e2e-protect-tx)
    - [E2EMon（ネットワーク健全性モニタ、独自 CDD 相当）](#e2emon)
  - [SecOC（Secure Onboard Communication、メッセージ認証）](#secoc)
    - [アーキテクチャ — E2E Transformer 方式とは異なる理由](#secoc-architecture)
    - [Secured I-PDU バイトレイアウト（SecOC Profile 1 準拠）](#secoc-byte-layout)
    - [明示する簡略化](#secoc-simplifications)
    - [検証](#secoc-verification)
    - [意図的に応用範囲を限定した理由](#secoc-scope-limitation)
  - [Signal Gateway（Com_GatewayRoute、SWC を介さないシグナル転送）](#signal-gateway)
    - [適用例 — ImmobilizerCmd（SecOC 検証済み）→ ImmobilizerStatus](#gateway-example)
    - [RX 側処理段階と実装の対応](#gateway-rx-mapping)
    - [明示する簡略化](#gateway-simplifications)
    - [動作確認方法](#gateway-verification)
  - [診断スタック（CanTp / Dcm / Dem / FiM / NvM）](#diag-stack)
    - [UDS 診断通信（ISO 14229-1 / ISO 15765-2）](#uds-diag-comm)
    - [CanTp（ISO 15765-2 トランスポートプロトコル）](#cantp)
    - [NvM（Non-Volatile Memory Manager）](#nvm)
    - [DEM 診断イベント管理（AUTOSAR SWS_DEM）](#dem)
      - [FreezeFrame（故障時スナップショット）](#freezeframe)
      - [ExtendedData（故障確定回数）](#extendeddata)
    - [FiM（機能抑止マネージャ）](#fim)
  - [ECU 管理層（EcuM / BswM / WdgM / ComM / CanSM / Nm）](#ecu-management)
    - [処理の流れ（コールチェーン）](#processing-flow-ecu)
    - [EcuM（ECU ステートマネージャ）](#ecum)
    - [BswM（BSW モードマネージャ）](#bswm)
    - [ComM（通信マネージャ）](#comm)
    - [WdgM（ウォッチドッグマネージャ）](#wdgm)
    - [Nm（ネットワークマネジメント）](#nm)
  - [IO スタック（IoHwAb / Dio / Port / Adc）](#io-stack)
    - [IoHwAb](#iohwab-module)
      - [デバウンス（積分カウンタ方式）](#debounce)
      - [ボタン固着検出](#button-stuck)
      - [ADC センサ電圧監視](#adc-monitoring)
      - [IoHwAb API 一覧（`IoHwAb.h`）](#iohwab-api)
    - [Dio](#dio-module)
      - [チャネル割り当て（`Dio_Cfg.h`）](#channel-assignment)
    - [Port](#port-module)
    - [Adc](#adc-module)
      - [チャネル設定（`Adc_Cfg.h`）](#adc-channel-config)
  - [アプリケーション（App_EngineManager / App_WarningIndicator）](#application)
    - [エンジン状態遷移](#engine-state-machine)
    - [App_WarningIndicator（警告灯 SW-C）](#app-warning-indicator)
- [シリアルモニタ出力例](#serial-log-example)
- [設計上の注意点](#design-notes)
  - [C / C++ 言語境界](#c-cpp-boundary)
  - [ログレベルの抑制 (Det_Cfg.h)](#log-level)
  - [固定長バッファのサイズは設定定数から計算する](#fixed-buffer-size)
  - [RX/TX で対称な入力検証](#rx-tx-symmetry)
  - [設定テーブルの一元管理](#config-table-centralization)
  - [単体テスト（ホスト上でのロジック検証）](#unit-test)

<a id="overview"></a>
## 概要

本プロジェクトは、学習目的で AUTOSAR の ASW / RTE / BSW の 3 層アーキテクチャを
Arduino UNO 上に最小構成で再現し、その上でメータ ECU（インストルメントクラスタ）相当の
アプリケーションを動作させることを目的としています。

システムは、エンジン ECU（CAN 0x100）と ABS ECU（CAN 0x110）の 2 つの周辺 ECU から
CAN バス経由でデータを受信し、BSW（CanDrv / CanIf / PduR / Com）が信号を抽象化します。

抽象化されたデータは RTE を介して 2 つの ASW SW-Component に提供されます。
SW-C は RTE の Client/Server ポート経由で IoHwAb（I/O Hardware Abstraction）を呼び出し、
SW-C がピン番号などのハードウェア詳細を知ることなく警告灯（LED）を制御します。

これにより、実車のメータ ECU が持つ
「CAN 受信（複数 ECU）→ 状態判定 → 優先度付き警告灯制御」
という典型的な処理フローを、Arduino 上で簡易的に再現します。

- **RX（CAN ID 0x100）**: エンジン ECU から回転数・水温・ON フラグを受信（AUTOSAR E2E Profile 01 保護付き）
- **RX（CAN ID 0x110）**: ABS ECU から車速・ブレーキ作動・ABS 作動フラグを受信
- **TX（CAN ID 0x200）**: エンジン状態（OFF / STARTING / RUNNING / FAULT）を変化時送信＋周期フロア（ComFilterAlgorithm、E2E 保護なし）
- **TX（CAN ID 0x210）**: 3 本の警告灯状態（RUNNING/FAULT/ABS）を Com Signal Group として一括送信
- **TX（CAN ID 0x220）**: E2E 検証エラーの累積カウンタ（CRC不一致/シーケンス異常）を Com の PERIODIC 送信モードで 6000ms 周期送信（E2EMon、独自 CDD 相当、AUTOSAR E2E Profile 01 保護付き）
- **診断 RX（CAN ID 0x7E0）**: UDS 診断要求を受信（ISO 14229-1 / ISO 15765-2）
- **診断 TX（CAN ID 0x7E8）**: UDS 診断応答を送信（マルチフレーム対応）
- **RUNNING LED（D6）**: ENGINE_STATE_RUNNING のとき点灯
- **FAULT LED（D7）**: ENGINE_STATE_FAULT のとき 500ms 周期で点滅
- **ABS LED（D8）**: ABS 作動（AbsActive=1）のとき点灯
- **警告確認ボタン（D9）**: FAULT 状態でボタンを押すと FAULT→OFF に遷移（ドライバーが警告を確認したことを通知）
- **ボランタリスリープ/ウェイクアップ**: エンジン OFF が一定時間継続すると CAN バスを実際にスリープさせ、バス活動を検知すると自律的に復帰する

<a id="hardware"></a>
## ハードウェア

<a id="hw-configuration"></a>
### ハードウェア構成

| 機器 | 用途 |
|------|------|
| Arduino UNO R4 WiFi | マイコン本体 |
| MCP2515 + TJA1050 | CAN コントローラ + トランシーバ |
| LED + 抵抗（220〜470 Ω）× 3 | RUNNING 灯（D6）/ FAULT 灯（D7）/ ABS 灯（D8）各 1 本 |
| プッシュボタン | 警告確認ボタン（D9 と GND を接続・内部プルアップ使用） |
| USB-CAN アダプタ | PC との CAN バス接続（解析用） |
| Cangaroo 等 | CAN フレーム送受信ツール |
| tools/uds_tester（本リポジトリ同梱） | UDS コマンドのボタン送信・FC 自動応答（後述） |

<a id="wiring-diagram"></a>
#### 配線図

![Arduino UNO R4 WiFi + MCP2515+TJA1050 + USB-CAN 配線図](docs/images/wiring_diagram.svg)

<a id="mcp2515-connection"></a>
#### MCP2515 接続（Arduino UNO）

| Arduino ピン | MCP2515 ピン | 備考 |
|-------------|-------------|------|
| D10 | CS | SPI チップセレクト |
| D2 | INT | 受信割り込み（ポーリング）|
| D13 | SCK | SPI クロック |
| D11 | SI (MOSI) | SPI データ出力 |
| D12 | SO (MISO) | SPI データ入力 |

> **D13 は MCP2515 の SCK と共用されるため LED には使用できません。**
> LED は D6（RUNNING）・D7（FAULT）・D8（ABS）それぞれに 220〜470 Ω の抵抗を直列に挿入して接続してください。

> **警告確認ボタン（D9）** は D9 と GND の間にプッシュボタンを接続するだけです。
> Port が `INPUT_PULLUP` で初期化するため、外部プルアップ抵抗は不要です。
> ボタン押下時に D9 が GND と接続され `DIO_LOW` となり、`IoHwAb_Button_GetLevel()` 内で論理反転して「押下=1」に変換されます。

> CAN バスには両端に終端抵抗（120 Ω）が必要です。

<a id="build-environment"></a>
### ビルド環境・設定

| 項目 | 値 |
|------|-----|
| プラットフォーム | PlatformIO + Renesas RA (`platform = renesas-ra`) |
| ボード | Arduino UNO R4 WiFi (`board = uno_r4_wifi`) |
| フレームワーク | Arduino |
| 外部ライブラリ | coryjfowler/mcp_can @ ^1.5.1 |
| CAN ボーレート | 500 kbps |
| MCP2515 クリスタル | 8 MHz（`Can_CrystalFreqType` で変更可） |
| シリアルモニタ | 115200 bps |

<a id="build-and-flash"></a>
### ビルドと書き込み

```bash
# ビルド
pio run

# 書き込み
pio run --target upload

# シリアルモニタ
pio device monitor
```

<a id="software"></a>
## ソフトウェア

<a id="architecture"></a>
### アーキテクチャ

<a id="layer-structure"></a>
#### 層構造

```
ASW ─── App_EngineManager / App_WarningIndicator / App_GptDemo
RTE ─── Rte（ポートベース S/R API + E2E Transformer 呼び出しグルー）
OS  ─── Os（タイムトリガスケジューラ）
BSW ─── EcuM / BswM / WdgM / WdgIf / Wdg / ComM / CanSM / Nm / E2EXf / E2E / Com / PduR / SecOC / Csm / CryIf / Crypto / KeyM / CanIf / Can
        CanTp / Dcm / Dem / NvM / MemIf / Fee / IoHwAb / Dio / Port / Adc / SchM / Det / Mcu / Gpt
HAL ─── Can_Hw / Dio_Hw / Port_Hw / Adc_Hw / Mcu_Hw / Fee_Hw / Wdg_Hw / Gpt_Hw（src/Hal/ に集約）
```

各層は上位層のヘッダのみに依存し、下位層の実装詳細を知りません。

<a id="module-list"></a>
#### モジュール一覧

| 層 | モジュール | ModuleId | AUTOSAR 仕様 | 仕様準拠度 |
|---|---|---|---|---|
| ASW | App_EngineManager | — | — | — |
|  | App_WarningIndicator | — | — | — |
| RTE | Rte | — | — | — |
| OS | Os | — | SWS_Os | 主要機能実装<br>(一部意図的に簡略化) |
| BSW | Adc | 123 | SWS_Adc | 主要機能実装<br>(一部意図的に簡略化) |
|  | BswM | 42 | SWS_BswM | 主要機能実装<br>(一部意図的に簡略化) |
|  | Can | 80 | SWS_Can | 主要機能実装 |
|  | CanIf | 60 | SWS_CanIf | 主要機能実装 |
|  | CanSM | 140 | SWS_CanSM | 主要機能実装 |
|  | CanTp | 35 | SWS_CanTp | 主要機能実装<br>(一部意図的に簡略化) |
|  | Com | 50 | SWS_Com | 主要機能実装 |
|  | ComM | 12 | SWS_ComM | 主要機能実装 |
|  | CryIf | 112 | SWS_CryptoInterface | パススルー<br>(下位が1個のため) |
|  | Crypto | 114 | SWS_CryptoDriver | 主要機能実装<br>(一部意図的に簡略化) |
|  | Csm | 110 | SWS_CryptoServiceManager | 主要機能実装<br>(一部意図的に簡略化) |
|  | Dcm | 53 | SWS_Dcm | 主要機能実装<br>(一部意図的に簡略化) |
|  | Dem | 54 | SWS_Dem | 主要機能実装 |
|  | Det | — | SWS_Det | 主要機能実装<br>(一部意図的に簡略化) |
|  | Dio | — | SWS_Dio | 主要機能実装<br>(一部意図的に簡略化) |
|  | E2E | — | SWS_E2E | 主要機能実装<br>(一部意図的に簡略化) |
|  | E2EXf | 176 | SWS_E2ELibrary 12.4<br>(E2E Transformer) | 主要機能実装<br>(一部意図的に簡略化) |
|  | E2EMon | — | — (独自 CDD 相当) | — |
|  | EcuM | 10 | SWS_EcuStateManager | 主要機能実装 |
|  | Fee | 21 | SWS_Fee | 主要機能実装<br>(一部意図的に簡略化) |
|  | FiM | 11 | SWS_FiM | 主要機能実装<br>(一部意図的に簡略化) |
|  | Gpt | 100 | SWS_Gpt | 主要機能実装<br>(一部意図的に簡略化) |
|  | IoHwAb | 254 | AUTOSAR 抽象化層 | — |
|  | KeyM | 116<br>(暫定値) | SWS_KeyManager<br>(Release 4.4.0) | 主要機能実装<br>(一部意図的に簡略化) |
|  | Mcu | 101 | SWS_Mcu | 主要機能実装<br>(一部意図的に簡略化) |
|  | MemIf | 22 | SWS_MemIf | パススルー<br>(下位が1個のため) |
|  | Nm | 31 | SWS_CANNM | 主要機能実装 |
|  | NvM | 20 | SWS_NvM | 主要機能実装<br>(一部意図的に簡略化) |
|  | PduR | 51 | SWS_PduR | 主要機能実装<br>(一部意図的に簡略化) |
|  | Port | — | SWS_Port | 主要機能実装<br>(一部意図的に簡略化) |
|  | SchM | — | SWS_SchM | 主要機能実装<br>(一部意図的に簡略化) |
|  | SecOC | 150 | SWS_SecureOnboardCommunication | 主要機能実装<br>(一部意図的に簡略化) |
|  | Wdg | 102 | SWS_Wdg | 主要機能実装<br>(一部意図的に簡略化) |
|  | WdgIf | 43 | SWS_WdgIf | パススルー<br>(下位が1個のため) |
|  | WdgM | 13 | SWS_WdgM | 主要機能実装 |
| HAL | Can_Hw | — | — | — |
|  | Dio_Hw | — | — | — |
|  | Port_Hw | — | — | — |
|  | Adc_Hw | — | — | — |
|  | SchM_Hw | — | — | — |
|  | Mcu_Hw | — | — | — |
|  | Fee_Hw | — | — | — |
|  | Wdg_Hw | — | — | — |
|  | Gpt_Hw | — | — | — |

> 「仕様準拠度」の凡例: **主要機能実装**=対象 SWS 仕様の主要要求を実質的に満たす／**主要機能実装(一部意図的に簡略化)**=中核機能は実装済みだが特定の API・モードを対応除外／**パススルー**=下位ドライバが1個のみのため実質的に素通し／**—**=対応する AUTOSAR 仕様が無い（ASW・RTE・HAL 層、または独自 CDD 相当）。各モジュールの具体的な簡略化内容は下表または各モジュール詳細節を参照。

「本プロジェクトでの役割」のうち、複数モジュールをまとめた詳細節（／診断スタック／ECU 管理層／IO スタック／アプリケーション）を持つものは、その節の先頭に個別の表として移動しました。以下はそれ以外（詳細節を持たないモジュール）の一覧です。

| モジュール | 本プロジェクトでの役割 |
|---|---|
| Os | タイムトリガスケジューラ。タスクごとに周期を設定し `Os_SchedulerStep()` で到来タスクを順次実行。時間源は Os 専用の Gpt チャネル（`GPT_CHANNEL_1`、詳細は [Os のスケジューラティック](#os-のスケジューラティックgpt-駆動) 参照） |
| CryIf | Csm と Crypto Driver の間のルーティング層。`CryIf_ProcessJob()`（実 AUTOSAR は複数 Crypto Driver Object への振り分けを担う）は、本プロジェクトが Crypto Driver を1個しか持たないため実質パススルーで `Crypto_ProcessJob()` へ委譲する（CanIf が単一 CAN コントローラに固定しているのと同じ簡略化） |
| Crypto | Csm/CryIf/Crypto レイヤの最下層。鍵テーブル（`Crypto_PBCfg.c`）と実際の暗号計算（AES-128-CMAC、自前実装）を保持する唯一のモジュール。`Crypto_ProcessJob()` がジョブの `service`（`CRYPTO_MACGENERATE`/`CRYPTO_MACVERIFY`）に応じて MAC を生成、または定数時間比較で検証する（MAC 検証のタイミングサイドチャネル対策はここに実装。旧実装では SecOC.c 内にあったロジックを責務として正しい層へ移設した） |
| Csm | SecOC が唯一直接呼ぶ暗号スタックの入口。`Csm_MacGenerate()`/`Csm_MacVerify()` が `jobId`（`Csm_PBCfg.c` の `Csm_JobConfigData`）から実行すべきプリミティブ種別と鍵 ID を解決し、`Crypto_JobType` ジョブを組み立てて `CryIf_ProcessJob()` へ委譲する。`Csm_MacVerify()` の `macLength` はビット単位（[SWS_Csm_01050]）、`Csm_MacGenerate()` の `macLengthPtr` はバイト単位（[SWS_Csm_00982]）という実仕様の非対称性を踏襲し、Csm 内でビット→バイト変換する。`Csm_KeyElementSet()`/`Csm_KeySetValid()`（KeyM が呼ぶ鍵操作 API）は CryIf へのパススルー |
| Det | `DET_LOG*` マクロ経由でタイムスタンプ付きログを Serial に出力するデバッグ用ブリッジ。加えて標準準拠の `Det_ReportError()`（[SWS_Det_00009]）も実装。詳細は下記「[DET 準拠](#det-compliance)」節を参照 |
| Fee | フラッシュエミュレーション EEPROM（Renesas RA `EEPROM.h`）向けの下位ドライバ。`Fee_Write()` は物理アドレス・データ・長さを受け取ってジョブを開始するだけで即座に返り、実際の書き込みは `Fee_MainFunction()` が 1 回の呼び出しにつき 1 バイトだけ進める（消去・書き込みサイクルによるブロッキングで WdgM の Deadline Supervision を巻き込んだ実機不具合への対策）。MemIf 経由でのみ呼ばれ、NvM から直接見えることはない |
| Gpt | HW タイマ（Renesas RA FspTimer）による周期割り込み駆動の General Purpose Timer Driver。目標時間到達判定は HW コンペアマッチではなく `Gpt_OnTick()` 内のソフトウェア比較で行い（`GetTimeElapsed`/`GetTimeRemaining` を単純な整数演算で正確に実現するため）、`Gpt_EnableNotification` された通知関数は ISR コンテキストから直接呼ばれる。2 チャネル構成: Channel 0 は `App_GptDemo` の動作確認用（1Hz Notification）、Channel 1 は Os 専用のスケジューラティック（Notification なし、`Os` が `Gpt_GetTimeElapsed()` をポーリング。[Os のスケジューラティック](#os-のスケジューラティックgpt-駆動) 参照）。`Gpt_SetMode`/`Gpt_EnableWakeup`/`Gpt_DisableWakeup`/`Gpt_CheckWakeup`/`Gpt_GetPredefTimerValue` は、EcuM が SLEEP モードを持たないため仕様上のプリコンパイル設定（`GptWakeupFunctionalityApi` 等）に沿って未実装 |
| KeyM | 鍵更新セッション（`KeyM_Start`→`KeyM_Update`→`KeyM_Finalize`）を管理する Key Manager。Dcm の WriteDataByIdentifier（DID 0x0108 CryptoKeyUpdate、詳細は下記「[UDS 診断通信](#uds-diag-comm)」節参照）が模擬鍵マスターとして駆動する。Certificate submodule・SHE 形式等は対応除外（詳細は下記「[明示する簡略化](#secoc-simplifications)」節参照）。ModuleId は Release 4.3.1 の AUTOSAR_TR_BSWModuleList.pdf に KeyM 自体が未掲載のため未検証の暫定値 |
| Mcu | `main.cpp` の `setup()` 冒頭（`Serial.begin()` より前）で `Mcu_Init()` を呼び、起動直後のリセット原因（Watchdog/BrownOut/External/PowerOn）を一度だけ読み取ってキャッシュする（Mcu_Hw のレジスタ読み取りは 1 起動につき 1 回しか呼べないため）。`Mcu_InitClock`/`Mcu_SetMode`/`Mcu_InitRamSection`/`Mcu_PerformReset` 等は Arduino フレームワークがクロック初期化を担い複数電源モードもモデル化しないため未実装。`Mcu_GetResetReason()`（単一の `Mcu_ResetType`）に加え、複数要因の同時検出を診断できるよう `Mcu_GetResetRawValue()`（4 フラグをビット詰めした本プロジェクト独自の生値）も提供する |
| MemIf | NvM（上位）と Fee（下位ドライバ）の間のディスパッチ層。実 AUTOSAR は Device 引数で複数の Fee/Ea インスタンスへ振り分けるが、本プロジェクトは下位ドライバが Fee 1 個のみのため実質パススルー（CryIf → Crypto の関係と同様）。`MemIf_Init`/`MemIf_MainFunction` は実 AUTOSAR の SWS_MemIf には存在しない（[SWS_MemIf_00019] により、ドライバが1個の構成では EcuM/Os が Fee_Init/Fee_MainFunction を直接呼んでよいと規定されている）本プロジェクト独自の拡張で、プラットフォーム分岐をこの層に閉じ込めるために追加した |
| SchM | 排他エリアマクロ（`SchM_Enter` / `SchM_Exit`）で共有リソースを保護。実体は `SchM_Hw`（`noInterrupts()`/`interrupts()`）で、Can の割り込みペンディングフラグ、および Gpt のチャネル状態機械（`Gpt_ChannelState`/`Gpt_ElapsedTicks`、実 HW 割り込みとメインループの両方から読み書きされる）を実際に保護する |
| SecOC | メッセージ認証（AES-128-CMAC）とフレッシュネス管理によるリプレイ対策。E2E とは異なる軸（E2E=意図しない誤り検出、SecOC=意図的な改ざん・なりすまし検出）で、PduR のルーティング経路上に中間モジュールとして挟まる。詳細は下記「[SecOC](#secoc)」節を参照 |
| Wdg | Renesas RA の実 HW ウォッチドッグ（IWDT、RA WDT ライブラリ経由）向け下位ドライバ。`Wdg_SetMode(WDGIF_FAST_MODE)` で 4000ms タイムアウトを有効化する。`Wdg_SetMode(WDGIF_OFF_MODE)` は常に `E_NOT_OK` を返す（IWDT は一度有効化すると無効化する手段がないため。実 AUTOSAR の拡張プロダクションエラー `WDG_E_DISABLE_REJECTED` に相当する状況）。WdgIf 経由でのみ呼ばれ、WdgM から直接見えることはない |
| WdgIf | WdgM（上位）と Wdg（下位ドライバ）の間のディスパッチ層。実 AUTOSAR は Device 引数で複数の Wdg インスタンスへ振り分けるが、本プロジェクトは物理ウォッチドッグが Wdg 1 個のみのため実質パススルー（MemIf → Fee と同じ簡略化）。実 AUTOSAR の WdgIf に Init/MainFunction が存在しない（[SWS_WdgIf_00018] により、ドライバが1個の構成では WdgM が Wdg_Init() を直接呼んでよいと規定されている）点は MemIf と共通するが、WdgIf 自体には MemIf のような非標準の Init/MainFunction 拡張を追加していない（プラットフォーム分岐を隠す必要がないため） |
| Dio_Hw | Arduino `digitalWrite` / `digitalRead` ラッパー |
| Port_Hw | Arduino `pinMode` ラッパー |
| Adc_Hw | Arduino `analogRead` ラッパー |
| SchM_Hw | Arduino `noInterrupts()`/`interrupts()` ラッパー |
| Mcu_Hw | リセット要因の読み取り（Renesas RA RSTSR0-1）・起動時ウォッチドッグ無効化 |
| Fee_Hw | フラッシュエミュレーション EEPROM 読み書き（Renesas RA `EEPROM` ライブラリ）ラッパー。Fee.c と Fee_Hw.cpp 以外からはインクルードしない内部境界 |
| Wdg_Hw | 実 HW ウォッチドッグの Enable / Disable / Refresh ラッパー。Wdg.c と Wdg_Hw.cpp 以外からはインクルードしない内部境界 |
| Gpt_Hw | Renesas RA `FspTimer` ラッパー。`FspTimer::get_available_timer()` で AGT/GPT の空きチャネルを実行時に自動選択し、`setup_overflow_irq()` で周期割り込みを有効化する。Gpt.c と Gpt_Hw.cpp 以外からはインクルードしない内部境界 |

ModuleId の出典は `docs/AUTOSAR_TR_BSWModuleList.pdf`（Release 4.3.1、「List of
Basic Software Modules」表）。詳細は「CAN 通信スタック」セクションの「DET 準拠」
節を参照。

> 各モジュールの詳細（フレーム構造・状態マシン・設定値）は後続セクションを参照してください。

<a id="directory-structure"></a>
### ディレクトリ構成

```
├── src/
│   ├── main.cpp                  # EcuM_Init / EcuM_MainFunction を呼ぶだけのエントリポイント
│   ├── Asw/
│   │   ├── App_EngineManager.h
│   │   ├── App_EngineManager.c      # エンジン状態遷移・ボランタリスリープ判断（OFF継続でComMへNO_COM要求）
│   │   ├── App_WarningIndicator.h
│   │   └── App_WarningIndicator.c   # 3 LED 独立制御（D6=RUNNING / D7=FAULT 点滅 / D8=ABS）
│   ├── Rte/
│   │   ├── Rte_Type.h            # アプリ型エイリアス（ARXML 自動生成相当）
│   │   ├── Rte.h
│   │   └── Rte.c                 # ポート API（周期管理は Os へ移管済み）・ランプ IOControl 調停・ComM_USER_0 要求ポート
│   ├── Os/
│   │   ├── Os_Cfg.h              # タスク数定数
│   │   ├── Os.h / Os.c           # タイムトリガスケジューラ（Os_SchedulerStep）。時間源は専用 Gpt チャネル (GPT_CHANNEL_1)
│   │   ├── Os_PBCfg.h
│   │   └── Os_PBCfg.c            # タスクテーブル（周期・関数ポインタ）
│   ├── Bsw/
│   │   ├── Adc/                  # ADC ドライバ（AUTOSAR SWS_Adc 準拠 API）
│   │   │   ├── Adc_Cfg.h         # チャネル定義・分解能・基準電圧
│   │   │   ├── Adc.h             # 公開インタフェース（Adc_ReadChannel）
│   │   │   └── Adc.c             # AUTOSAR Adc モジュール（純粋 C、Adc_Hw へ委譲。境界は src/Hal/Adc_Hw.h）
│   │   ├── BswM/                 # BSW モードマネージャ（ルール駆動タスク制御）
│   │   │   ├── BswM_Cfg.h        # タスク ID 定数・タスクマスク定義
│   │   │   ├── BswM_PBCfg.h      # ルール構造体型定義・BswM_Config 宣言
│   │   │   ├── BswM_PBCfg.c      # ルールテーブル実体（6 ルール、うち2つは AND/OR 複合条件）
│   │   │   ├── BswM.h            # 公開インタフェース（Init / EcuM通知 / ComM通知）
│   │   │   └── BswM.c            # ルールエンジン実装・Os_SetTaskActive 呼び出し
│   │   ├── Can/                  # CAN ドライバ（AUTOSAR SWS_Can 準拠 API）
│   │   │   ├── Can.h             # 公開インタフェース
│   │   │   └── Can.c             # AUTOSAR Can モジュール（純粋 C、Can_Hw へ委譲。境界は src/Hal/Can_Hw.h）。Can_Isr は真の割り込みでペンディングフラグを立てるのみ、実処理は Can_MainFunction_Read/BusOff/Wakeup
│   │   ├── CanIf/                # CAN インタフェース
│   │   │   ├── CanIf_Types.h     # 型定義（Can_PduType 等）
│   │   │   ├── CanIf_Cfg.h       # TX/RX PDU テーブル数定数
│   │   │   ├── CanIf_PBCfg.h     # ポストビルド設定宣言（CanIf_Config）
│   │   │   ├── CanIf_PBCfg.c     # TX/RX PDU ルーティングテーブル実体
│   │   │   ├── CanIf.h           # 公開インタフェース（CanIf_Transmit / CanIf_RxIndication）
│   │   │   └── CanIf.c           # CAN ID ↔ 論理 PDU マッピング・Can/PduR 間の仲介・ControllerBusOff/Wakeup 通知の中継・全受信フレームを CanSM_RxIndication へ転送（ウェイクアップ検証用）
│   │   ├── CanSM/                # CAN ステートマネージャ（Bus-Off 回復シーケンス、L1/L2 バックオフ）
│   │   │   ├── CanSM_Cfg.h       # L1/L2 回復待機時間・L1→L2 切替閾値
│   │   │   ├── CanSM.h           # 公開インタフェース・CanSM_ControllerBusOff コールバック
│   │   │   └── CanSM.c           # 状態機械・Bus-Off L1/L2 バックオフ回復タイマ管理・NO_COM要求時に CAN コントローラを実際にスリープ・ウェイクアップ検証（CanSM_ControllerWakeup/CanSM_RxIndication/検証タイムアウト）
│   │   ├── CanTp/                # CAN トランスポートプロトコル（ISO 15765-2）
│   │   │   ├── CanTp_Cfg.h       # ブロックサイズ・STmin・タイムアウト設定
│   │   │   ├── CanTp.h           # 公開インタフェース（CanTp_Transmit / CanTp_RxIndication）
│   │   │   └── CanTp.c           # SF/FF/CF/FC 状態機械・マルチフレーム組立分割
│   │   ├── Com/                  # COM（シグナル管理、E2E には一切関知しない）
│   │   │   ├── Com_Types.h       # 型定義（Com_SignalIdType / Com_IPduIdType / Com_IPduConfigType の RxIndicationCbk/TxTransformCbk 等）
│   │   │   ├── Com_Cfg.h         # I-PDU・シグナル数定数・シグナル ID
│   │   │   ├── Com_PBCfg.h       # ポストビルド設定宣言（Com_Config）
│   │   │   ├── Com_PBCfg.c       # I-PDU/シグナルレイアウトテーブル実体（RxIndicationCbk/TxTransformCbk で Rte 層のグルー関数を紐付けるのみ、E2E の詳細は持たない）
│   │   │   ├── Com.h             # 公開インタフェース（Com_SendSignal / Com_ReceiveSignal / Com_ReceiveSignalGroupArray / Com_IsRxTimedOut）
│   │   │   └── Com.c             # シグナルパック/アンパック・受信デッドライン監視・ComFilterAlgorithm（送信要否判定）・Signal Group（Com_SendSignalGroup）
│   │   ├── ComM/                 # 通信マネージャ（CAN バス通信モード管理）
│   │   │   ├── ComM_Cfg.h        # チャネル数・ユーザ数定数
│   │   │   ├── ComM.h            # 公開インタフェース（NO_COM/SILENT_COM/FULL_COM）
│   │   │   └── ComM.c            # 複数ユーザ要求の集約（調停）・CanSM_RequestComMode へ委譲
│   │   ├── CryIf/                # Crypto Interface（Csm と Crypto Driver 間のルーティング層）
│   │   │   ├── CryIf_Cfg.h       # DET 定数・CRYIF_CHANNEL_ID
│   │   │   ├── CryIf.h           # 公開インタフェース（CryIf_ProcessJob）
│   │   │   └── CryIf.c           # 本プロジェクトは Crypto Driver が1個のみのため実質パススルー
│   │   ├── Crypto/                # Crypto Driver（Csm/CryIf/Crypto レイヤの最下層。鍵テーブルと実暗号計算を保持）
│   │   │   ├── Crypto_Types.h    # Crypto_JobType（簡略化フラット版）・Crypto_ServiceInfoType 等
│   │   │   ├── Crypto_Cfg.h      # DET 定数・CRYPTO_KEY_* 定数
│   │   │   ├── Crypto_PBCfg.h/.c # 鍵テーブル実体（元 SecOC_PBCfg.c から移設）
│   │   │   ├── Crypto.h          # 公開インタフェース（Crypto_ProcessJob）
│   │   │   ├── Crypto.c          # MAC 生成/検証の実処理（定数時間比較はここに実装）
│   │   │   ├── Crypto_Aes128.h/.c # AES-128 自前実装（FIPS-197 KAT セルフテスト付き。元 SecOC_Aes128.*）
│   │   │   └── Crypto_Cmac.h/.c   # AES-CMAC 自前実装（NIST SP 800-38B。元 SecOC_Cmac.*）
│   │   ├── Csm/                  # Crypto Service Manager（SecOC が唯一直接呼ぶ暗号スタックの入口）
│   │   │   ├── Csm_Cfg.h         # DET 定数・CSM_JOB_ID_* 定数
│   │   │   ├── Csm_PBCfg.h/.c    # CsmJob 設定テーブル（jobId → プリミティブ種別・鍵ID）
│   │   │   ├── Csm.h             # 公開インタフェース（Csm_MacGenerate/Csm_MacVerify、Csm_KeyElementSet/Csm_KeySetValid）
│   │   │   └── Csm.c             # ジョブ解決 → Crypto_JobType 組み立て → CryIf_ProcessJob 委譲。鍵操作 API は CryIf へパススルー
│   │   ├── KeyM/                 # Key Manager（鍵更新セッションを管理。Csm のみに依存）
│   │   │   ├── KeyM_Cfg.h        # DET 定数・鍵名定数（KEYM_CRYPTO_KEY_NAME_*）
│   │   │   ├── KeyM_PBCfg.h/.c   # 鍵名テーブル（鍵名 → Csm 側 keyId = CRYPTO_KEY_*）
│   │   │   ├── KeyM.h            # 公開インタフェース（KeyM_Start/KeyM_Update/KeyM_Finalize）
│   │   │   └── KeyM.c            # セッション状態管理 → 鍵名解決 → Csm_KeyElementSet/Csm_KeySetValid 呼び出し
│   │   ├── E2E/                  # AUTOSAR E2E Profile 01 保護（CRC8 SAE J1850 + 4bit カウンタ）
│   │   │   ├── E2E_P01.h         # 設定型・状態型・API 宣言（Check: E2E_P01Check/CheckInit、Protect: E2E_P01Protect/ProtectInit）
│   │   │   └── E2E_P01.c         # CRC8 計算・受信検証（カウンタデルタ判定）・送信保護（Counter更新+CRC付加）実装
│   │   ├── E2EXf/                # E2E Transformer（Com から E2E ロジックを切り離す統合層）
│   │   │   ├── E2EXf.h           # 汎用 API 宣言（E2EXf_Init/E2EXf_DeInit/E2EXf_InverseTransform/E2EXf_Transform/E2EXf_GetVersionInfo、E2E_P01 への薄いラッパー）
│   │   │   ├── E2EXf.c           # 上記実装（Dem_ReportErrorStatus への報告・モジュール自身の初期化状態ガードも含む）
│   │   │   ├── E2EXf_PBCfg.h     # ポストビルド設定宣言（E2EXf_*RxCfg/TxCfg、E2EXf_PBCfg_Init）
│   │   │   └── E2EXf_PBCfg.c     # I-PDU ごとの E2E P01 設定/状態実体（EngineInfo/AbsInfo/E2EHealthStatus）
│   │   ├── E2EMon/                # 独自 CDD 相当（標準 AUTOSAR モジュールではない、E2E 健全性監視の例）
│   │   │   ├── E2EMon.h          # 公開インタフェース（E2EMon_Init/E2EMon_NotifyCheckResult）
│   │   │   └── E2EMon.c          # E2E 検証結果の累積カウンタ・Com_SendSignal() での公開
│   │   ├── EcuM/                 # ECU ステートマネージャ（ライフサイクル管理）
│   │   │   ├── EcuM_Cfg.h        # RUN ユーザ定義・POST_RUN タイムアウト
│   │   │   ├── EcuM.h            # 公開インタフェース・EcuM_StateType 定義
│   │   │   └── EcuM.c            # 状態マシン・EcuM_RequestRUN / EcuM_ReleaseRUN（SHUTDOWN→RUNのCANウェイクアップ復帰対応）
│   │   ├── Dcm/                  # 診断通信マネージャ（UDS ISO 14229-1）
│   │   │   ├── Dcm_Cfg.h         # SID/NRC/DID 定数・セッション・S3 タイマ・SecurityAccess 設定
│   │   │   ├── Dcm_Cbk.h         # PduR から呼ばれる受信コールバック宣言（Dcm_ComIndication）
│   │   │   ├── Dcm.h             # 公開インタフェース（Dcm_Init / Dcm_MainFunction）
│   │   │   └── Dcm_Cbk.c         # UDS サービスディスパッチ実装・S3 タイマ監視・SecurityAccess/RoutineControl 状態機械
│   │   ├── Dem/                  # 診断イベントマネージャ（DTC 管理）
│   │   │   ├── Dem_Cfg.h         # イベント ID・DTC コード・ステータスビットマスク・デバウンス閾値
│   │   │   ├── Dem.h             # 公開インタフェース（ReportErrorStatus / GetAllDTCs / FreezeFrame）
│   │   │   └── Dem.c             # DTC ライフサイクル・デバウンス・FreezeFrame 記録・NvM 永続化
│   │   ├── Det/                  # Default Error Tracer（Serial ブリッジ）
│   │   │   ├── Det.h             # ログマクロ定義（DET_LOGI/W/E/D）
│   │   │   └── Det.c             # AUTOSAR Det モジュール（純粋 C、レベル判定・メッセージ整形。Det_Hw へ委譲。境界は src/Hal/Det_Hw.h）
│   │   ├── Dio/                  # デジタル I/O 値読み書き（MCAL・方向設定は Port が担う）
│   │   │   ├── Dio_Cfg.h         # チャネル ID 定義（D6=RUNNING / D7=FAULT / D8=ABS / D9=ボタン）
│   │   │   ├── Dio.h             # 公開インタフェース（Dio_WriteChannel / Dio_ReadChannel）
│   │   │   └── Dio.c             # AUTOSAR Dio モジュール（純粋 C、Dio_Hw へ委譲。境界は src/Hal/Dio_Hw.h）
│   │   ├── FiM/                  # 機能抑止マネージャ（DTC→機能抑止）
│   │   │   ├── FiM_Cfg.h         # 機能 ID (FID) 定義
│   │   │   ├── FiM_PBCfg.h       # FID×イベント設定構造体型定義・FiM_Config 宣言
│   │   │   ├── FiM_PBCfg.c       # FID×イベント対応テーブル実体
│   │   │   ├── FiM.h             # 公開インタフェース（FiM_Init / FiM_MainFunction / GetFunctionPermission）
│   │   │   └── FiM.c             # 許可状態の再評価・キャッシュ
│   │   ├── Nm/                   # ネットワークマネジメント（CanNm 状態機械）
│   │   │   ├── Nm_Cfg.h          # DET 定数・NM フレーム周期/DLC/ノードID・状態機械タイマ値
│   │   │   ├── Nm.h              # 公開インタフェース（Nm_NetworkRequest/Release/RxIndication/TxConfirmation 等）
│   │   │   └── Nm.c              # Network Mode(Repeat Message/Normal Operation/Ready Sleep)/Prepare Bus-Sleep/Bus-Sleep の状態機械。PduR/Com 非経由で CanIf と直接やり取り
│   │   ├── NvM/                  # Non-Volatile Memory Manager（EEPROM 抽象化）
│   │   │   ├── NvM_Cfg.h         # ブロック ID・EEPROM アドレス・ブロックサイズ定義
│   │   │   ├── NvM_PBCfg.h       # ブロック設定構造体型定義・NvM_Config 宣言
│   │   │   ├── NvM_PBCfg.c       # ブロック設定テーブル実体（DEM_MAGIC/STATUS/AGING/EXTENDED、EXTENDED は冗長ブロック）
│   │   │   ├── NvM.h             # 公開インタフェース（NvM_ReadBlock / NvM_WriteBlock / NvM_MainFunction / NvM_GetErrorStatus）
│   │   │   └── NvM.c             # RAM ミラー・ブロック/CRC/冗長化のオーケストレーション。物理バイト書き込みの進行は持たず MemIf へ委譲（境界は src/Bsw/MemIf/MemIf.h）
│   │   ├── Mcu/                  # MCU Driver（リセット原因の読み取り。クロック/RAMセクション初期化は未対応）
│   │   │   ├── Mcu_Cfg.h         # DET 定数・ApiId（docs/4.3.1/AUTOSAR_SWS_MCUDriver.pdf を実測して確認済み）
│   │   │   ├── Mcu_PBCfg.h       # コンフィグ型定義（プレースホルダ）・Mcu_Config 宣言
│   │   │   ├── Mcu_PBCfg.c       # コンフィグ実体
│   │   │   ├── Mcu.h             # 公開インタフェース（Mcu_Init/Mcu_GetResetReason/Mcu_GetResetRawValue）
│   │   │   └── Mcu.c             # Mcu_Hw への委譲。Mcu_Init() が起動直後に一度だけリセット原因を読み取りキャッシュする
│   │   ├── MemIf/                # Memory Abstraction Interface（NvM と Fee の間のディスパッチ層）
│   │   │   ├── MemIf_Types.h     # 共通型（MemIf_StatusType/MemIf_JobResultType/MemIf_DeviceType）
│   │   │   ├── MemIf_Cfg.h       # DET 定数・ApiId（Fee_Cfg.h と共に SWS 実測値へ差し替え済み）
│   │   │   ├── MemIf.h           # 公開インタフェース（MemIf_Read/Write/WriteImmediate/Cancel/GetStatus/GetJobResult/MainFunction）
│   │   │   └── MemIf.c           # 唯一の下位ドライバ (Fee) へのディスパッチ。`#if defined(__AVR__)` 分岐は歴史的にこのファイルだけに置く設計（現在は Fee 固定）
│   │   ├── Fee/                  # Flash EEPROM Emulation（Renesas RA フラッシュエミュレーション EEPROM 向け下位ドライバ）
│   │   │   ├── Fee_Cfg.h         # DET 定数・ApiId（docs/4.3.1/AUTOSAR_SWS_FlashEEPROMEmulation.pdf を実測して確認済み）
│   │   │   ├── Fee.h             # 公開インタフェース（Fee_Read/Write/WriteImmediate/Cancel/GetStatus/GetJobResult/MainFunction）
│   │   │   └── Fee.c             # 非同期書き込みジョブ（Fee_Write は即座に返り、Fee_MainFunction が 1 呼び出し 1 バイトだけ進める。境界は src/Hal/Fee_Hw.h）
│   │   ├── PduR/                 # PDU ルーター
│   │   │   ├── PduR_Types.h      # 型定義
│   │   │   ├── PduR_Cfg.h        # RX/TX ルーティングパス数定数
│   │   │   ├── PduR_PBCfg.h      # ポストビルド設定宣言（PduR_Config）
│   │   │   ├── PduR_PBCfg.c      # ルーティングテーブル実体
│   │   │   ├── PduR_COM.h        # COM 向けコールバック型定義
│   │   │   ├── PduR_CanIf.h      # CanIf 向けコールバック型定義
│   │   │   ├── PduR_SecOC.h      # SecOC 向けインタフェース定義（PduR_SecOCTransmit）
│   │   │   ├── PduR.h            # 公開インタフェース
│   │   │   └── PduR.c            # PDU マルチキャスト配信・送信完了通知の転送・TX 経路の中間モジュール委譲（TransmitOverrideFct）
│   │   ├── SecOC/                # メッセージ認証（Csm_MacGenerate/Csm_MacVerify を呼ぶのみ。鍵/AES/CMAC は一切知らない）
│   │   │   ├── SecOC_Types.h     # 型定義（SecOC_RxPduConfigType/SecOC_TxPduConfigType、Key ではなく CsmJobId を持つ）
│   │   │   ├── SecOC_Cfg.h       # RX/TX Secured I-PDU 数定数
│   │   │   ├── SecOC_PBCfg.h     # ポストビルド設定宣言（SecOC_Config）
│   │   │   ├── SecOC_PBCfg.c     # RX/TX Secured I-PDU 設定実体（DataId/オフセット/CsmJobId）
│   │   │   ├── SecOC.h           # 公開インタフェース（SecOC_IfRxIndication/SecOC_IfTransmit/SecOC_MainFunction）
│   │   │   └── SecOC.c           # フレッシュネス検証（RX）／Secured I-PDU 組み立て（TX）。MAC 自体は Csm 経由
│   │   ├── Port/                 # ピン方向設定（MCAL・Dio と責務を分離）
│   │   │   ├── Port_Cfg.h        # ピン番号定義（D6/D7/D8 OUTPUT / D9 INPUT_PULLUP）
│   │   │   ├── Port.h            # 公開インタフェース（Port_Init / Port_SetPinDirection）
│   │   │   └── Port.c            # AUTOSAR Port モジュール（純粋 C、Port_Hw へ委譲。境界は src/Hal/Port_Hw.h）
│   │   ├── Gpt/                  # General Purpose Timer Driver（Renesas RA FspTimer 向け HW タイマ抽象化）
│   │   │   ├── Gpt_Cfg.h         # DET 定数・ApiId・基本型（Gpt_ChannelType 等。docs/4.3.1/AUTOSAR_SWS_GPTDriver.pdf を実測して確認済み）
│   │   │   ├── Gpt_PBCfg.h       # チャネル設定構造体型定義・Gpt_Config 宣言
│   │   │   ├── Gpt_PBCfg.c       # チャネルテーブル実体（Channel 0: 1000Hz tick・App_GptDemo_OnTick 通知、Channel 1: 1000Hz tick・Os 専用・Notification なし）
│   │   │   ├── Gpt.h             # 公開インタフェース（Gpt_Init/StartTimer/StopTimer/GetTimeElapsed/GetTimeRemaining/Enable・DisableNotification）
│   │   │   └── Gpt.c             # チャネル状態機械。目標時間到達判定は HW コンペアマッチではなく Gpt_OnTick() 内のソフトウェア比較。境界は src/Hal/Gpt_Hw.h
│   │   ├── IoHwAb/               # I/O ハードウェア抽象化（MCAL と SW-C の境界）
│   │   │   ├── IoHwAb.h          # 公開インタフェース（RTE が参照）
│   │   │   └── IoHwAb.c          # Dio/Adc へ委譲・デバウンス（40ms）・固着検出・ADC 電圧低下検出（Dem 報告）
│   │   ├── SchM/                 # スケジュールマネージャ（排他エリアマクロ）
│   │   │   └── SchM.h            # SchM_Enter/Exit マクロ定義（全モジュール共通、実体は src/Hal/SchM_Hw.h）
│   │   ├── Wdg/                  # Watchdog Driver（Renesas RA 実 HW ウォッチドッグ向け下位ドライバ）
│   │   │   ├── Wdg_Cfg.h         # DET 定数・ApiId（docs/4.3.1/AUTOSAR_SWS_WatchdogDriver.pdf を実測して確認済み）
│   │   │   ├── Wdg_PBCfg.h       # コンフィグ型定義・Wdg_Config 宣言
│   │   │   ├── Wdg_PBCfg.c       # コンフィグ実体（WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS を直接引用し二重管理を解消）
│   │   │   ├── Wdg.h             # 公開インタフェース（Wdg_Init/Wdg_SetMode/Wdg_SetTriggerCondition）
│   │   │   └── Wdg.c             # Wdg_SetMode(WDGIF_OFF_MODE) は HW 制約により常に E_NOT_OK。境界は src/Hal/Wdg_Hw.h
│   │   ├── WdgIf/                # Watchdog Interface（WdgM と Wdg の間のディスパッチ層）
│   │   │   ├── WdgIf_Types.h     # 共通型（WdgIf_ModeType/WdgIf_DeviceType）
│   │   │   ├── WdgIf_Cfg.h       # DET 定数・ApiId（docs/4.3.1/AUTOSAR_SWS_WatchdogInterface.pdf を実測して確認済み）
│   │   │   ├── WdgIf.h           # 公開インタフェース（WdgIf_SetMode/WdgIf_SetTriggerCondition）
│   │   │   └── WdgIf.c           # 唯一の下位ドライバ (Wdg) へのディスパッチ
│   │   └── WdgM/                 # ウォッチドッグマネージャ（Alive + Logical Supervision）
│   │       ├── WdgM_Cfg.h        # エンティティ ID・チェックポイント ID・監視サイクル・期待回数定義
│   │       ├── WdgM_PBCfg.h      # エンティティ/遷移設定構造体型定義・WdgM_Config 宣言
│   │       ├── WdgM_PBCfg.c      # エンティティテーブル・許可遷移テーブル実体（App_EngineManager_Run / App_WarningIndicator_Run）
│   │       ├── WdgM.h            # 公開インタフェース（Init / CheckpointReached / MainFunction）
│   │       └── WdgM.c            # Alive/Logical/Deadline 判定(6000ms) + HW WDT trigger(1000ms、WdgIf 経由で Wdg へ委譲)
│   └── Hal/                      # ハードウェア依存層（Bsw 各モジュールの純粋 C 実装から分離）
│       ├── Can_Hw.h / Can_Hw.cpp        # MCP2515 / mcp_can C++ ラッパー（旧 Mcp2515_Wrapper.cpp）。SLEEP時にウェイクアップ割り込みを有効化
│       ├── Dio_Hw.h / Dio_Hw.cpp        # Arduino digitalWrite / digitalRead ラッパー
│       ├── Port_Hw.h / Port_Hw.cpp      # Arduino pinMode ラッパー
│       ├── Adc_Hw.h / Adc_Hw.cpp        # Arduino analogRead ラッパー
│       ├── Det_Hw.h / Det_Hw.cpp        # Arduino Serial 出力実装（Arduino API を呼ぶ唯一の場所）。Det.c と Det_Hw.cpp 以外からインクルードしない内部境界
│       ├── Mcu_Hw.h / Mcu_Hw.c          # リセット要因読み取り（RA RSTSR0-1）・起動時 WDT 無効化
│       ├── Fee_Hw.h / Fee_Hw.cpp        # フラッシュエミュレーション EEPROM 読み書き（RA EEPROM.h）。Fee.c と Fee_Hw.cpp 以外からインクルードしない内部境界
│       ├── Wdg_Hw.h / Wdg_Hw.cpp        # 実 HW ウォッチドッグ Enable / Disable / Refresh(RA WDTライブラリ)。Wdg.c と Wdg_Hw.cpp 以外からインクルードしない内部境界
│       ├── Gpt_Hw.h / Gpt_Hw.cpp        # FspTimer による周期 HW 割り込み(AGT/GPT 空きチャネルを実行時に自動選択)。Gpt.c と Gpt_Hw.cpp 以外からインクルードしない内部境界
│       └── SchM_Hw.h / SchM_Hw.cpp      # noInterrupts()/interrupts() ラッパー（SchM 排他エリアの実体）
├── dbc/
│   └── engine_manager.dbc        # CAN シグナル定義（Cangaroo 等で使用）
└── platformio.ini
```

---
<a id="can-stack"></a>
### CAN 通信スタック（Can_Hw / Can / CanIf / PduR / Com / E2E / E2EXf / E2EMon / Rte）

CAN ドライバ（Can / Can_Hw）から CanIf・PduR を経由して COM モジュールへ至るデータパスを担うスタックです。

```
TX（Arduino → 外部、下り）
  Rte → (E2EXf/E2E) → Com → PduR → CanIf → Can → Can_Hw → MCP2515

RX（外部 → Arduino、上り）
  MCP2515 → Can_Hw → Can → CanIf → PduR → Com → (E2EXf/E2E/E2EMon) → Rte
```

| 層 | モジュール | 本プロジェクトでの役割 |
|---|---|---|
| RTE | Rte | ポートベース S/R API。複数 SW-C が同一シグナルを独立ポートで受信。E2E Transformer を持つ Read ポートは `Std_ReturnType` ではなく `Rte_IStatusType` を返し、E2E チェック結果（OK/ハードエラー/ソフトエラー）と Com タイムアウトを区別して SWC へ伝える |
| BSW | E2EMon | 標準 AUTOSAR モジュールには存在しない、実務でよく見る「独自 CDD」パターンの例。EngineInfo/AbsInfo の E2E 検証結果を購読し、ネットワーク健全性テレメトリとして公開する。詳細は下記「[E2EMon](#e2emon)」節を参照 |
|  | E2EXf | Com と E2E の間を仲介する統合層。RX は `E2E_P01Check` でデータ破壊・フレーム脱落・重複・誤ルーティングを検出して Dem へ報告、TX は `E2E_P01Protect` で Counter・CRC8 を付加する。Com 自身はこの層の存在を知らない。詳細は下記「[E2E P01 保護](#e2e-p01)」節を参照 |
|  | E2E | AUTOSAR E2E Profile 01 保護の実処理。DataID・CRC8 (SAE J1850)・4bit カウンタの 3 要素で、`E2E_P01Check` はデータ破壊・フレーム脱落・重複・誤ルーティングを検出、`E2E_P01Protect` は Counter・CRC8 を付加。Com/Rte のどちらにも依存しない純粋な検証/付与ライブラリ |
|  | Com | シグナルのビット単位パック／アンパックと送受信タイミング制御を担う（TxModeMode: DIRECT/MIXED/PERIODIC・TMS・MDT・受信フィルタ・I-PDU Group・Signal Gateway 等）。E2E には一切関知しない（E2E Transformer 方式、Com.c 本体に E2E は埋め込まれない） |
|  | PduR | 受信 PDU を Com/CanTp/SecOC へ（1つの RxPduId から複数宛先への配信にも対応）、送信 PDU を CanIf へルーティング。通信スタックの配管役。TX 経路は既定では `CanIf_Transmit()` へ直接転送するが、`PduR_TxRoutingPathType.TransmitOverrideFct` が設定されている場合は中間モジュール（SecOC）へ委譲できるよう汎用化されている（既存の全 TX パスはこのフィールドを使わないため無変更） |
|  | CanIf | CAN ID ↔ 論理 PDU のマッピング。上位層は CAN ID を知らず PDU ID で通信。設定 DLC 未満の受信 L-PDU は上位層へ渡さず棄却する（SWS_CANIF_00026 のデータ長チェック） |
|  | Can | MCP2515 の送受信・Bus-Off 検出・CAN バス活動によるウェイクアップ検出を担う MCAL 最下層。HW を直接操作する唯一のモジュール |
| HAL | Can_Hw | MCP2515 / mcp_can C++ ラッパー（RX 割り込み登録 `Can_Hw_AttachRxIsr` を含む） |

以降、まず Tx/Rx 共通の CAN フレーム仕様を示し、続けて Tx/Rx それぞれの関数コールチェーン
（Tx: Com → PduR → CanIf → Can、Rx: Can → CanIf → PduR → Com）とレイヤ間の多層防御を
モジュール横断の内容として説明します。そのあと Can・Com それぞれのモジュール内で閉じた
詳細（実装判断の背景・設定・検証手順等）を章ごとにまとめます。Can/CanIf/PduR/Com に限らず
BSW 全体へ及ぶ DET 準拠（エラー通知の標準化）は最後にまとめます。

<a id="can-frame-spec"></a>
#### CAN フレーム仕様

エンディアンはすべてビッグエンディアン（Motorola / CAN 標準）。
ビット 0 = byte[0] の MSB、ビット 7 = byte[0] の LSB。

**Tx/Rx フレーム一覧**

| Tx/Rx | フレーム | CAN ID | DLC | ビット位置 | サイズ | シグナル | 単位・値域 |
|-------|---------|--------|-----|-----------|--------|---------|----------|
| Tx | MeterStatus | 0x200 | 2 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–7 | 8 bit | EngineState | 0=OFF<br>1=STARTING<br>2=RUNNING<br>3=FAULT<br>（E2E 保護なし） |
|  |  |  |  | 8 | 1 bit | (update-bit) | EngineState 単体の update-bit（SWS_Com_00061/00062）。値変化時送信=1、周期フロア再送=0 |
| Tx | WarningStatus | 0x210 | 1 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0 | 1 bit | RunLamp | 0=消灯<br>1=点灯<br>（RUNNING LED D6 と同値） |
|  |  |  |  | 1 | 1 bit | FaultLamp | 0=消灯<br>1=点灯<br>（FAULT LED D7 と同値、点滅中は 500ms ごとに反転） |
|  |  |  |  | 2 | 1 bit | AbsLamp | 0=消灯<br>1=点灯<br>（ABS LED D8 と同値） |
|  |  |  |  | 3 | 1 bit | (update-bit) | Signal Group 全体の update-bit（SWS_Com_00801）。値変化時送信=1、MIXED 周期フロア再送=0 |
| Tx | E2EHealthStatus | 0x220 | 4 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–7 | 8 bit | E2E CRC | CRC8 SAE J1850<br>（DataID=0x0220 + byte[1-3] を対象に計算。AUTOSAR 標準バリアント 1A 準拠でCRCは先頭バイト） |
|  |  |  |  | 12–15 | 4 bit | E2E Counter | 0–15 のリングカウンタ（送信のたびに +1） |
|  |  |  |  | 16–23 | 8 bit | E2ECrcErrCount | 0–255（飽和）<br>EngineInfo/AbsInfo受信のE2E CRC不一致累積数 |
|  |  |  |  | 24–31 | 8 bit | E2ESeqErrCount | 0–255（飽和）<br>EngineInfo/AbsInfo受信のE2Eシーケンス異常累積数 |
| Rx | EngineInfo | 0x100 | 6 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–7 | 8 bit | E2E CRC | CRC8 SAE J1850<br>（DataID=0x0100 + byte[1-5] を対象に計算。AUTOSAR 標準バリアント 1A 準拠でCRCは先頭バイト） |
|  |  |  |  | 12–15 | 4 bit | E2E Counter | 0–15 のリングカウンタ（フレーム脱落・重複検出用） |
|  |  |  |  | 16–31 | 16 bit | EngineSpeed | rpm（0–15000） |
|  |  |  |  | 32–39 | 8 bit | CoolantTemp | ℃（0–255） |
|  |  |  |  | 40 | 1 bit | EngineOnFlag | 0=OFF / 1=ON |
| Rx | AbsInfo | 0x110 | 5 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–7 | 8 bit | E2E CRC | CRC8 SAE J1850<br>（DataID=0x0110 + byte[1-4] を対象に計算。AUTOSAR 標準バリアント 1A 準拠でCRCは先頭バイト） |
|  |  |  |  | 12–15 | 4 bit | E2E Counter | 0–15 のリングカウンタ<br>（フレーム脱落・重複検出用） |
|  |  |  |  | 16–31 | 16 bit | VehicleSpeed | 0.01 km/h（raw 0x0064 = 1.00 km/h） |
|  |  |  |  | 32 | 1 bit | BrakeActive | 0=解除 / 1=作動 |
|  |  |  |  | 33 | 1 bit | AbsActive | 0=非作動 / 1=ABS 作動中 |

##### TX フレーム（Arduino → 外部）

**MeterStatus（メータ ECU / CAN ID 0x200 / DLC=2 / E2E 保護なし / TxModeMode=MIXED）**

`App_EngineManager_Run()`（3000ms 周期）は `Rte_Write_EngineStatus_EngineState()` で
値を書き込むだけで、送信自体は Com が判断します。`EngineState` が変化すると Com が
次回 `Com_MainFunction()`（Os の 100ms タスク）で送信し、変化がなくても一定間隔
（周期フロア、後述）で再送し続けます。実際の CAN 送信（SPI 通信）は必ず
`Com_MainFunction()` 側で行われるため、`App_EngineManager_Run()` 自身が SPI
送信でブロッキングすることはありません（詳細は「Com」の「ComFilterAlgorithm と TxModeMode」セクション参照）。E2E 保護は
付与していません（EngineInfo/AbsInfo を Com が既に検証した**後**にメータ ECU 自身が
導出する二次データであり、実車でも一次センサ値ほど厳密な保護が付与されないことが
多いため、素の（E2E 保護なしの）シグナル送信の実装例として意図的に残しています。
詳細は「E2E P01 保護」セクション参照）。MIXED を選んだ理由: `EngineState` は他 ECU
（盗難防止・ボディ制御等）が判断材料に使いうるデータのため、起動直後の受信側や
瞬断から復帰した受信側がいつまでも古い値を握り続けないよう、周期フロアによる
再送を残しています。

**WarningStatus（メータ ECU / CAN ID 0x210 / DLC=1 / Com Signal Group / TMS 付き DIRECT⇔MIXED）**

`App_WarningIndicator_Run()`（500ms 周期）が 3 本の LED レベルを計算した直後、同じ値を
Signal Group としてまとめて Com へコミットします。コミットで変化が検知されると
次回 `Com_MainFunction()` で送信されます。E2E 保護は付与していません
（ダッシュボード表示用の LED ミラー情報であり、他 ECU の制御判断に使う想定がないため）。

この I-PDU は固定の `TxModeMode` を 1 つだけ持つのではなく、TMS
（Transmission Mode Selector）により通常時と警告時で自動的にモードを
切り替えます。普段（FaultLamp/AbsLamp とも消灯）は DIRECT（周期フロアなし、
変化時のみ送信、他 ECU が制御判断に使わない表示専用データのため取りこぼしても
実害が小さい）ですが、FaultLamp または AbsLamp のいずれかが点灯すると MIXED
（周期フロア付き）へ自動的に切り替わります。これは、警告状態こそ途中から
参加した監視ツールにも確実に伝わってほしい、という実務的な判断です
（詳細は「TMS」セクション参照）。同じメータ ECU の 2 つの TX フレーム
（MeterStatus は固定 MIXED、WarningStatus は TMS 付き DIRECT⇔MIXED）で、
データの役割の違いに応じて異なる `TxModeMode` 戦略を選んでいる点が
実務的な設計判断の例です。

**E2EHealthStatus（メータ ECU / CAN ID 0x220 / DLC=4 / AUTOSAR E2E Profile 01 保護 / PERIODIC）**

`E2EMon`（CDD 相当モジュール）が EngineInfo/AbsInfo 受信側の E2E 検証エラー累積数を
集計し、Com の PERIODIC 送信モードにより 6000ms 周期で自動送信されるネットワーク
健全性テレメトリです。テレメトリ自体の破損を監視ツールが検出できるよう、AbsInfo と
同じ構成（データ＋Counter＋CRC）で E2E 保護しています。詳細は「E2E P01 保護」
セクションの E2EMon サブセクションを参照してください。

##### RX フレーム（外部 → Arduino）

**EngineInfo（エンジン ECU / CAN ID 0x100 / DLC=6 / AUTOSAR E2E Profile 01 保護）**

**RUNNING 状態に入るフレーム例（Speed=500rpm, Temp=0℃, EngineOnFlag=1, Counter=0）：**
```
byte[0] byte[1] byte[2] byte[3] byte[4] byte[5]
  XX      00      01      F4      00      80
  │       │       └─────┘         └──┘   └──── EngineOnFlag=1（bit40 = byte[5] の MSB）
  │       └─ Counter=0             Speed=500rpm    Temp=0℃
  └───────── CRC8（XX は自動計算）
```
**AbsInfo（ABS ECU / CAN ID 0x110 / DLC=5）**

**ABS 作動フレーム例（VehicleSpeed=100km/h, BrakeActive=1, AbsActive=1, Counter=0）：**
```
byte[0] byte[1] byte[2] byte[3] byte[4]
  XX      00      27      10      C0
  │       │       └─────┘         └──── BrakeActive=1（bit32）, AbsActive=1（bit33）
  │       └─ Counter=0             Speed=10000 (0x2710) → 100.00 km/h
  └───────── CRC8（XX は自動計算）
```

（AUTOSAR 標準バリアント 1A、SWS_E2E_00227 に準拠し、CRC を先頭バイト・Counter をそれに
続く 1 バイトの下位 4bit に配置している）

> E2E Counter と CRC は uds_tester ツールが自動計算して付加します。
> Cangaroo から手動送信する場合は byte[0]=CRC8 の計算値、byte[1]=Counter 値を手動で付加してください。

<a id="processing-flow"></a>
#### 処理の流れ（関数コールチェーンと多層防御）

<a id="tx-processing"></a>
##### Tx 処理（Com → PduR → CanIf → Can の順）

```
Com_SendSignal()/Com_SendSignalGroup()   ← ASW から呼ばれる。TX バッファへ pack するだけ
  ┊  (Com_TxPending 経由。次回 Com_MainFunction() の 100ms tick まで非同期に待機)
  ↓
Com_MainFunction()                        ← ここから下は同期呼び出し連鎖
  → TxTransformCbk があれば呼ぶ             ← Rte_COMTransform_*() → E2EXf_Transform() → E2E_P01Protect()
  → PduR_Transmit()
    → TransmitOverrideFct 未設定:
        CanIf_Transmit() → Can_Write()（SPI 送信完了までここで同期完了）
    → TransmitOverrideFct=SecOC_IfTransmit（E2EHealthStatus のみ）:
        SecOC_IfTransmit()                ← Authentic I-PDU をバッファへコピーするだけ
          ┊  (SecOC_TxPending 経由。次回 SecOC_MainFunction() の 100ms tick まで非同期に待機)
          ↓
        SecOC_MainFunction()              ← ここから下は同期呼び出し連鎖
          → Csm_MacGenerate() で Secured I-PDU を組み立て
            → PduR_SecOCTransmit() → CanIf_Transmit() → Can_Write()
```

<a id="rx-processing"></a>
##### Rx 処理（Can → CanIf → PduR → Com の順）

```
Can_Isr()                        ← 真の割り込み。ペンディングフラグを立てるだけ
  ┊  (Can_RxIrqPending 経由。次回 Os スケジューラ tick まで非同期に待機)
  ↓
Can_MainFunction_Read()          ← フラグをドレイン、SPI 読み出し（ここから下は同期呼び出し連鎖）
  → CanIf_RxIndication()         ← CAN ID → PduId（論理 PDU）へ変換
    → PduR_CanIfRxIndication() (= PduR_ComRxIndication())
      → 宛先ごとにマルチキャスト:
          Com_RxIndication()         ← EngineInfo/AbsInfo（RxIndicationCbk 経由）
            → Rte_COMCbk_EngineInfo/AbsInfo()
              → E2EXf_InverseTransform() → E2E_P01Check()
          CanTp_RxIndication()       ← UDS 診断要求（複数フレーム対応）
          SecOC_IfRxIndication()     ← ImmobilizerCmd
            → Csm_MacVerify() で認証成功時のみ Com_RxIndication()
```

##### 受信長チェックの多層防御

**受信長チェックの多層防御**: 設定 DLC に満たない短小フレームは、まず `CanIf_RxIndication()`
が棄却する（SWS_CANIF_00026 相当、本来この責務は CanIf 層にある）。仮に何らかの理由で
ここを通過しても、`Com_RxIndication()`・`CanTp_RxIndication()`（SF/FF/CF/FC 各フレーム）
がそれぞれ独立に自分の期待長を検証する。1 箇所だけに頼らず各層が自分の責務として
検証するのは、本プロジェクトで実際に発生した「短いフレームで上位層バッファに
新旧混在の破損データが残る」バグ（Com のシグナル固定長アクセス等）を踏まえた設計判断
である。`Com_RxIndication()` の受信長チェックは AUTOSAR 本来の仕様（SWS_Com_00574/
00575/00870）に準拠したシグナル単位の部分受理を実装している: Signal Group は
一貫性のないスナップショットを公開しないため受信できたバイト数が DLC 未満なら
グループ全体を棄却するが（SWS_Com_00575）、非 Signal Group の I-PDU は受信できた
バイト範囲に収まるシグナルのみを部分的に受理し、範囲外のシグナルは前回受信値の
まま据え置く（SWS_Com_00574/00870）。前述の「新旧混在の破損データ」バグは
Com_ReceiveSignal/Com_SendSignal が BitSize に関わらず常に 4 バイト読み書きして
呼び出し元のスタックを破壊していたことが根本原因であり、本対応（未受信バイトに
触れず前回値をそのまま保持する）とは別の問題である（詳細は `Com.c` のコメント参照）。
なお `CanIf_PBCfg.c` の各 RxPdu の `.Dlc` は現状 Com 側の `ipdu->DLC` と同じ値に
設定されているため、`Com_RxIndication()` の部分受理パスは短小フレームが CanIf 層で
先に棄却されることで実運用上到達しない（2026-07 時点で実機確認済み）。検証するには
CanIf 側の `.Dlc` を一時的に緩める必要がある。

<a id="can-module"></a>
#### Can

<a id="can-tx-async-confirm"></a>
##### TX 確認の非同期化（`Can_MainFunction_Write`）

**なぜ非同期化したか**: AUTOSAR の仕様 [SWS_Can_00016] は、`CanIf_TxConfirmation()` を
「TX 割り込みハンドラから」または「ポーリングモードでは `Can_MainFunction_Write()` の
中から」呼ぶことを求めている。しかし当初の実装では、`Can_Write()` が送信成功直後、
呼び出し元と同一スタックフレーム内でそのまま `CanIf_TxConfirmation()` を呼んでいた。

```
（修正前）
Com_MainFunction() → Com_DoTransmit() → PduR_Transmit() → CanIf_Transmit() → Can_Write()
                                                                                → CanIf_TxConfirmation()
                                                                                  → PduR_CanIfTxConfirmation()
                                                                                    → Com_TxConfirmation()
（すべて 1 回の呼び出しチェーン内で完結）
```

これ自体は現状害がないが、将来 `Com_TxConfirmation()` の延長線上（あるいは他の
`TxConfirmFct`）に「送信失敗を検知したら即座に再送する」ような処理が足された場合、
その再送呼び出しがそのまま `Can_Write()` の再帰呼び出しになってしまう。NvM の
非同期書き込みジョブキュー（[DEVLOG参照](docs/DEVLOG.md#nvm-非同期書き込みジョブキューへの変更経緯)）
と同じ「今は実害がないが将来の変更で踏み抜きやすいスタック深化の地雷」を避ける
考え方で、この結合を断ち切った。

**設計**:

```
Can_Write(Hth, PduInfo):
  Can_Hw_Send() が成功したら、PduInfo->swPduHandle を TX 確認保留キューへ積むだけで
  即座に E_OK 相当（CAN_OK）を返す（CanIf_TxConfirmation() はまだ呼ばない）

Can_MainFunction_Write()（1ms 周期、Os_PBCfg.c Task 13）:
  保留キューが空になるまで、投入順に CanIf_TxConfirmation() を呼び出す
```

NvM の非同期ジョブキュー（1 呼び出し 1 バイトずつ）とは異なり、こちらは
`CanIf_TxConfirmation()` 自体がハードウェアをブロックしないソフトウェア的な
コールバック転送のみのため、1 回の `Can_MainFunction_Write()` 呼び出しで
保留分を全件処理してよい。

**動作への影響**: `CanIf_TxConfirmation()` の呼び出しタイミングが `Can_Write()` から
最大 1ms（Task 13 の周期）遅延するようになるが、`Com_TxConfirmation()`・
`CanTp_TxConfirmation()` のいずれも受け取った結果を使わない no-op のため、
体感できる動作変化はない（この経路は常に E_OK 固定でもある。詳細は CanTp
セクションの N_As タイムアウトの説明を参照）。

<a id="can-rx-interrupt"></a>
##### RX の割り込み化（`Can_Isr` / `Can_MainFunction_Read/BusOff/Wakeup`）

**なぜ割り込み化したか**: 従来 `Can_Isr()` は Os スケジューラから 1ms ごとにポーリング
呼び出しされる「疑似 ISR」で、INT ピンを `digitalRead()` で確認していた。これは
「割り込み」と名乗りながら実態はポーリングであり、AUTOSAR OS が本来持つ「タスクと
ISR は実際にプリエンプトし合う」という関係を体験できていなかった。また、SchM の
排他エリアマクロ（`SchM_Enter_Com_SIGNAL_EXCLUSIVE_AREA()` 等）も「協調スケジューリング
なので NOP でよい」という理由でずっと未使用のまま残っていた。

本変更で `Can_Hw_AttachRxIsr()`（`Can_Init()` 内）が `attachInterrupt()` で INT ピンの
立ち下がりエッジに `Can_Isr()` を真のハードウェア割り込みとして登録し、Os スケジューラの
周期とは無関係に即座に起動されるようにした。

**ISR を最小限に保つ設計判断**: 素直に実装するなら「ISR の中で `CanIf_RxIndication()` まで
呼んでしまう」のが最も単純だが、本実装ではあえてそうしていない。`Can_Isr()` は
ペンディングフラグ（`Can_RxIrqPending` / `Can_WakeupIrqPending`）を立てるだけに留め、
SPI 通信・Serial ログ・CanIf 呼び出しは一切行わない。理由は 2 つ:

1. **SPI バス排他**: MCP2515 は SPI 接続のため、CS ピン制御を伴う複数バイトの読み書きが
   1 トランザクションとして完結する必要がある。メインループ側の `Can_Write()`（TX、SPI
   経由）がトランザクション途中で割り込みにプリエンプトされ、割り込み側が同じ SPI バスへ
   別トランザクションを割り込ませると、双方が破壊されうる。ISR 側で SPI を使わなければ
   この競合はそもそも発生しない。
2. **処理時間の上限**: `CanIf_RxIndication()` から先は PduR/Com/CanTp/Dcm まで連鎖し、
   UDS SID 処理（RoutineControl 等）まで含まれ得る。これを割り込みハンドラの中で行うと、
   ISR の実行時間が事実上無制限になりかねない（本 README で繰り返し出てくる「同期呼び出し
   連鎖のスタック/ブロッキングリスク」と同種の問題）。

実際の SPI 読み出しと `CanIf_RxIndication()` 呼び出しは、ペンディングフラグを見てメイン
ループのタスクが行う（AUTOSAR `SWS_Can_00396`・`SWS_Can_00012` 参照:「呼び出しコンテキストが
ISR か `Can_MainFunction_Read` かは実装依存であり、コールバックはいずれの場合も ISR から
呼ばれたかのように実装してよい」）。これは TX 確認の非同期化（`Can_MainFunction_Write`）と
対になる設計で、CAN モジュールの RX/TX 双方が「イベントは即座に検知するが、重い処理は
専用タスクへ委譲する」という同じパターンに統一されたことになる。

**関数の分離**: 旧 `Can_Isr()` は「RX ポーリング」「Bus-Off ポーリング」「SLEEP 中の
ウェイクアップ検出」の 3 役を 1 つの関数にまとめていたが、AUTOSAR は元々これらを
独立した `Can_MainFunction_xxx` として定義している。これに合わせて分離した。

```
Can_Isr()                     ← 真の割り込み。フラグを立てるだけ
Can_MainFunction_Read()       ← Can_RxIrqPending をドレインし RX 処理 (SWS_Can_00108)
Can_MainFunction_BusOff()     ← EFLG.TXBO を毎回ポーリング (SWS_Can_00109、割り込み非依存)
Can_MainFunction_Wakeup()     ← Can_WakeupIrqPending をドレインしウェイクアップ通知 (SWS_Can_00112)
```

`Can_MainFunction_Read()` のドレインループはフラグではなく `Can_Hw_CheckReceive()` が
NOT_OK を返すまで継続する。MCP2515 の INT はレベル方式（未読フレームが残る限り
アサートされ続ける）ため、連続到着した 2 フレーム目には新たなエッジが立たないことが
あるが、フラグは「立った」ことだけを覚えていれば十分で、実際に何件処理するかは
ドレインループがハードウェアの状態から判断する。

**SchM が初めて意味を持つ**: `Can_Isr()`（割り込みコンテキスト）と
`Can_MainFunction_Read()`/`Can_MainFunction_Wakeup()`（メインループのタスク）は、
ペンディングフラグを介して実際に競合しうる関係になった。フラグの読み出しとクリアを
アトミックに行わないと、その間に割り込みが発生した場合にフラグのセットが失われ、
受信フレーム・ウェイクアップ通知を取りこぼす。これを防ぐため `SchM.h` に新しい排他エリア
`SchM_Enter/Exit_Can_IRQFLAG_EXCLUSIVE_AREA()` を追加し、実体を
`SchM_Hw_EnterExclusiveArea()`/`ExitExclusiveArea()`（`src/Hal/SchM_Hw.cpp`、
`noInterrupts()`/`interrupts()` を呼ぶだけ）とした。既存の `Rte_MIRROR`・`Com_SIGNAL`
排他エリアも同じ実体を指すように変更し、NOP のままだった `SchM.h` が実際に機能するように
なった（Com の RX/TX バッファ自体は現状 `Can_MainFunction_Read()` というメインループの
タスクからのみ触られる設計にしたため、まだ割り込みと競合しないが、Rte 側と同様
将来のための保険として Enter/Exit を残してある）。

> **意図的な二重化**: `Can_MainFunction_Read()`/`Can_MainFunction_Wakeup()` は
> 「割り込みが本当に発火するか」に正しさを依存させない設計にしている。
> - `Can_MainFunction_Read()` は `Can_RxIrqPending` の有無に関わらず、毎回
>   無条件に `Can_Hw_CheckReceive()`（SPI 経由のステータスレジスタ読み出し。
>   INT ピンの実際の状態には依存しない）でドレインする。
> - `Can_MainFunction_Wakeup()` は `Can_WakeupIrqPending` に加えて
>   `digitalRead(intPin)` の直接ポーリングも併用する（旧実装と同じ
>   フォールバック）。
>
> `Can_Isr()`・ペンディングフラグ・`SchM_Enter/Exit_Can_IRQFLAG_EXCLUSIVE_AREA()`
> の構造はそのまま残り、割り込みが発火すればより低遅延に反応できる
> 「ボーナス経路」として機能するが、たとえ割り込みが何らかの理由で発火
> しなくてもポーリング側だけで正しく動作する。単一の検出経路（割り込みのみ）に
> 正しさを委ねず、独立したポーリングでも動作を保証する設計にした経緯は
> [DEVLOG: Can RX 割り込み化の実機検証で得られた教訓](docs/DEVLOG.md#can-rx-割り込み化の実機検証で得られた教訓) を参照。

<a id="com-module"></a>
#### Com

##### ComFilterAlgorithm と TxModeMode（送信要否・タイミングを Com 自身が判断する）

実車の AUTOSAR Com は、ASW が値をセットする（`Com_SendSignal`）ことと、実際に CAN へ
いつ送信すべきかを判断すること（`ComFilterAlgorithm` + `ComTxModeMode`）を分離しています。
本プロジェクトでも `EngineState` にこの分離を適用しました。

```
ComFilterAlgorithm = MASKED_NEW_DIFFERS_MASKED_OLD（Mask = 0xFF、8bit 全体を比較）
TxModeMode         = MIXED（MeterStatus）

Com_SendSignal(ENGINE_STATE, value):
  TX バッファへ value をパック（常に実行、ASW から見た値は常に最新）
  (value & Mask) != (前回のフィルタ比較値 & Mask) ?
    YES → Com_RequestTxOnChange(MeterStatus) を呼ぶ
            → Com_TxPending[MeterStatus] = 1 を立てるだけ
              （PduR_Transmit は呼ばない。呼び出し元の Runnable は
              ここで一切ブロッキングしない）
    NO  → 何もしない（バッファは更新済みだが送信要求は立たない）

Com_MainFunction()（Os の 100ms タスクから周期的に呼ばれる。WdgM 非監視）:
  MeterStatus について、
    Com_TxPending[MeterStatus] == 1 、
    または最終送信からの経過時間 >= TxPeriodMs（周期フロア間隔）？
      YES → CommunicationControl 抑制中でなければ送信する
              （TxTransformCbk があれば呼んだ上で PduR_Transmit
              → CanIf_Transmit → Can_Write の SPI 送信まで完了する）
      NO  → 何もしない
```

**なぜ実送信を `Com_MainFunction()` に一元化したか**: `Com_SendSignal()` の
呼び出し元は `App_EngineManager_Run()` であり、WdgM の Deadline Supervision
（実行時間の上限監視）対象の Runnable です。もし `Com_SendSignal()` の中で
`PduR_Transmit()`（→ MCP2515 への SPI 送信）まで同期的に呼び切ると、バス輻輳等で
SPI 送信が想定より長引いた場合に Runnable 自体の実行時間が伸び、Deadline
Supervision の誤検出につながりかねません。`Com_MainFunction()` は WdgM の
監視対象外のタスクのため、実送信をそちらへディスパッチすることで、SPI 送信の
所要時間が ASW Runnable の実行時間に影響しない設計にしています（SWS_Com_00734
等が要求する「次回メイン関数までに送信を開始する」という猶予の範囲内）。

**なぜ周期フロアが必要か**: 単純に「変化時のみ送信」（DIRECT）にすると、`EngineState` が
長時間同じ値（例: RUNNING が続く）のときフレームが完全に途絶えてしまいます。
実車でも同様の理由（新規に起動した受信 ECU や、直前のフレームを取りこぼした
受信 ECU への配慮）で、変化がなくても一定周期で再送し続ける「MIXED 送信モード」が
一般的です。本プロジェクトでは `COM_TX_PERIOD_METERSTATUS_FLOOR_MS`（既定 9000ms）で
これを実時間ベースに再現しています。

**責務分離の効果**: 以前は「変化したら送る」という判断を ASW（`App_EngineManager`）が
毎サイクル無条件に送信トリガ API を呼ぶことで暗黙的に実現していました
（実際には無条件に送信していただけで、判断はしていませんでした）。今は ASW は
「値をセットする」だけを行い（送信をトリガする API 自体が存在しない）、実際に
いつ送るかは Com 層が `ComFilterAlgorithm`/`TxModeMode` の設定だけで完結して決めます。
将来 ASW のロジックを変更しても Com の設定（`Com_PBCfg.c`）だけで送信頻度のポリシーを
調整できます。

##### Com 設定（`Com_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `COM_TX_PERIOD_METERSTATUS_FLOOR_MS` | 9000 | MeterStatus（MIXED）の周期フロア間隔 [ms]。変化がなくてもこの間隔で強制送信する |
| `COM_TX_PERIOD_E2EHEALTH_MS` | 6000 | E2EHealthStatus（PERIODIC）の送信周期 [ms] |
| `COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS` | 2000 | WarningStatus の TMS が true（FAULT/ABS 点灯中）のときの周期フロア間隔 [ms] |
| `COM_TX_MIN_DELAY_WARNINGSTATUS_MS` | 100 | WarningStatus の MDT（変化時送信の最小送信間隔）[ms]。周期フロアには適用しない |

##### Com Signal Group（複数シグナルの一括コミット）

`MeterStatus` の `EngineState` は 1 I-PDU に 1 シグナルしかありませんが、実車の Com には
「1 つの I-PDU に属する複数シグナルを、ASW が個別に `Com_SendSignal()` した瞬間ではなく、
すべて揃った時点でまとめて確定させたい」というニーズがあります（Signal Group、
SWS_Com_00050 相当）。バラバラのタイミングで実 TX バッファへ反映すると、途中経過の
不整合な組み合わせ（例: RunLamp だけ新しい値、FaultLamp は古い値のまま）が一瞬でも
CAN へ送信されうるためです。本プロジェクトでは `WarningStatus`（3 本の警告灯）で
これを実装しました。

```
Com_IPduConfigType.IsSignalGroup = 1（WarningStatus, IPduId=1、TMS 付き DIRECT⇔MIXED）

Com_SendSignal(RUN_LAMP/FAULT_LAMP/ABS_LAMP, value)  ← App_WarningIndicator が 3 回呼ぶ
  所属 I-PDU の IsSignalGroup を検索
    == 1 → Com_TxShadowBuffer[1] へパックするのみ（実 TX バッファは変更しない、
            ComFilterAlgorithm 判定もしない）
            ComTransferProperty=TRIGGERED_ON_CHANGE のメンバーのみ、前回値との
            比較（マスクなしの生値比較、Com_FilterLastValue[] を流用）で
            Com_GroupTriggerPending[1] を立てる（詳細は次項）
    == 0 → 従来どおり Com_TxBuffer へ直接パック（MeterStatus はこちら）

Com_SendSignalGroup(GroupId=1)   ← 3 シグナルすべて設定し終えた後に 1 回呼ぶ
  Com_TxShadowBuffer[1] を Com_TxBuffer[1] へバイト単位でコピー（確定コミット。
  PENDING/TRIGGERED_ON_CHANGE を問わず全メンバー分をコピーする）
  Com_GroupTriggerPending[1] を判定
    立っている → Com_RequestTxOnChange(WarningStatus) を呼び、フラグをクリア
                  → Com_TxPending[WarningStatus] = 1 を立てるだけ
                    （次回 Com_MainFunction() で送信、周期フロアなし）
    立っていない → 何もしない
```

**なぜシャドウバッファが必要か**: `Com_SendSignal()` を 3 回呼ぶ間、CAN 送信タスクが
（この実装は非プリエンプティブなので実際には割り込まれませんが）割り込んで
送信を行ってしまうと、3 本のうち更新済みのものと未更新のものが混在した状態で
送信されてしまいます。シャドウバッファへ一旦貯めてから `Com_SendSignalGroup()`
で一括コピーすることで、この不整合な中間状態が実 TX バッファ（延いては CAN バス）
へ現れることはありません。

##### ComTransferProperty（Signal Group メンバーごとの送信トリガー宣言）

「3 本のうちどれか 1 本でも変わればコミット全体を送信する」という要否判定を、
以前は `Com_SendSignalGroup()` が前回コミット値とのバイト単位比較（`changed` 判定）で
代用していました。この方式には概念上の欠陥があります。Signal Group の各メンバーには
本来、実 AUTOSAR の `ComTransferProperty`（SWS_Com_00742/00743）として「自分の変化が
送信の引き金になる（TRIGGERED_ON_CHANGE）か、値を保持するだけで他メンバーの送信に
便乗する（PENDING）か」を個別に宣言できるはずですが、バイト単位比較ではこの区別を
表現できず、グループ内の全メンバーが事実上 TRIGGERED_ON_CHANGE 相当になってしまいます。

本実装ではこれを是正し、メンバーごとに `Com_SignalConfigType.TransferProperty`
（`COM_TRANSFER_PROPERTY_PENDING` / `COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE` の
2 値。実 AUTOSAR は他に TRIGGERED/TRIGGERED_ON_CHANGE_WITHOUT_REPETITION も持つが、
本実装は WarningStatus の用途に必要な最小限のみ実装）を宣言できるようにしました。
`RunLamp`/`FaultLamp`/`AbsLamp` は現状すべて `TRIGGERED_ON_CHANGE` のため、挙動自体は
バイト比較方式のときと変わりません（動機は TMS/MDT と同じく、実利より仕様忠実性）。
`COM_TRANSFER_PROPERTY_PENDING` を使う具体例は本設定にはありませんが、「他メンバーの
送信には毎回便乗させたいが、自分の変化単独では送信させたくない」低優先度シグナルを
将来追加する際に使えます。

この判定は `ComFilterAlgorithm`（シグナル単位、1 シグナル = 1 I-PDU 向け）とは別の
独立した仕組みです。`Com_SendSignal()` は Signal Group メンバーに対して
`ComFilterAlgorithm` を一切評価せず、`ComTransferProperty` の判定だけを行います。

##### TMS（Transmission Mode Selector、I-PDU 単位で 2 つの送信モードを自動切り替え）

ここまでの `TxModeMode` は I-PDU ごとに 1 つだけ固定で設定していました。実車の
AUTOSAR Com は、1 つの I-PDU に **2 組**の送信モード設定（`ComTxModeTrue` /
`ComTxModeFalse`）を持たせ、どちらが有効かを TMS（Transmission Mode Selector）の
評価結果で自動的に切り替えられます（SWS_Com_00032/00799）。`WarningStatus` に
この仕組みを適用しました。

```
WarningStatus (IPduId=1):
  TxModeMode      = DIRECT （TMS=false のとき使う。ComTxModeFalse 相当）
  TxModeModeTrue  = MIXED  （TMS=true  のとき使う。ComTxModeTrue 相当）
  TxPeriodMsTrue  = COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS（既定 2000ms）

TMS 寄与シグナル（TmsContributor=1）:
  FaultLamp: ComFilterAlgorithm=MASKED_NEW_DIFFERS_X, Mask=0x01, FilterX=0
  AbsLamp  : ComFilterAlgorithm=MASKED_NEW_DIFFERS_X, Mask=0x01, FilterX=0
  （RunLamp は寄与しない = TmsContributor=0）

Com_RecalcTms(WarningStatus)  ← Com_SendSignalGroup() のコミット直後に毎回呼ばれる
  tmsTrue = false
  FaultLamp について: (値 & Mask) != FilterX  ← 値=1 なら true
    true なら tmsTrue = true
  AbsLamp について: 同様に評価、true なら tmsTrue = true
                     （SWS_Com_00678: 1 つでも真なら TMS 全体が true）
  Com_TmsState[WarningStatus] = tmsTrue

Com_EffectiveTxModeMode(WarningStatus):
  Com_TmsState[WarningStatus] ? TxModeModeTrue : TxModeMode
```

**なぜ FaultLamp/AbsLamp だけを TMS 対象にしたか**: RunLamp（エンジン稼働中の表示）は
正常運転そのものであり、途中参加した監視ツールが多少取りこぼしても実害がありません。
一方 FaultLamp/AbsLamp が示す異常状態は、途中参加したツールにも確実に伝わってほしい
という考えで、この 2 つだけを TMS の判定材料にしています。

**具体的に変わる挙動**: AbsActive が単独で立った（FAULT は起きていない）場合、
AbsLamp は点滅せず点灯しっぱなしになります。TMS が無ければ DIRECT のまま
なので、ABS 作動の立ち上がり時に 1 回フレームが送信された後は、AbsActive が
落ちるまで再送されません。この間に監視ツールを新規接続すると、ABS 作動中
であることを知る手段がありません。TMS により AbsLamp 点灯中は MIXED へ
自動的に切り替わるため、`COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS`
（既定 2000ms）ごとに再送され続け、途中参加したツールも確実に把握できます
（FaultLamp は 500ms 点滅中でありバイト自体が毎周期変化するため、この効果は
主に AbsLamp 単独点灯時に意味を持ちます）。

**TMS 変化時の即時送信について**: 実 AUTOSAR は「TMS の変化によりモードが
切り替わったら、その変化を起こしたシグナルの ComTransferProperty によらず
無条件に即座に送信する」ことを要求します（SWS_Com_00495）。本実装はこの
特別扱いを独立には実装していません。現状 `FaultLamp`/`AbsLamp` は TMS 寄与
シグナル（`TmsContributor=1`）であると同時に `ComTransferProperty=
TRIGGERED_ON_CHANGE` でもあるため、TMS を変化させる値変化は同じタイミングで
`Com_GroupTriggerPending` も立てる通常の送信トリガーに該当し、結果として
SWS_Com_00495 相当の即時送信が「たまたま」成立しています。

これは偶然の一致であり、一般には成立しません。もし将来 TMS 寄与シグナルを
`ComTransferProperty=PENDING` に設定した場合、そのシグナル単体の変化は
`Com_GroupTriggerPending` を立てないため、他の TRIGGERED_ON_CHANGE メンバーの
変化が同時に起きない限り、TMS だけが切り替わって送信は起きません
（SWS_Com_00495 の要求を満たさない状態になり得ます）。この限界は
`Com.c` の `Com_SendSignalGroup()` 内コメントにも明記しています。SWS_Com_00495
を厳密に満たすには、TMS 遷移（`Com_TmsState[]` の値そのものの変化）を検出して
`ComTransferProperty` の値によらず無条件に `Com_RequestTxOnChange()` を呼ぶ、
独立した経路が別途必要です。

**ログ例**:
```
[10123ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:1]     # ABS 作動開始
[10124ms] INFO  Com: TX iPdu=1 [04]                 # 即座に送信（TMS: false→true）
[12124ms] INFO  Com: TX iPdu=1 [04]                 # 周期フロア（2000ms 後、値は不変）
[14124ms] INFO  Com: TX iPdu=1 [04]                 # 周期フロア（継続中）
[15200ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:0]     # ABS 作動終了
[15201ms] INFO  Com: TX iPdu=1 [00]                 # 即座に送信（TMS: true→false）
                                                     # 以降は DIRECT に戻り、変化なしでは再送されない
```

##### MDT（ComMinimumDelayTime、変化時送信の最小送信間隔）

DIRECT/MIXED I-PDU は値が変化するたびに送信要求（`Com_TxPending[]`）が立ちますが、
信号源が高頻度で変化し続けると、その分だけ CAN バスへの送信も連続してしまいます。
実 AUTOSAR の Com は、I-PDU ごとに `ComMinimumDelayTime`（MDT）を設定でき、
直近の実送信からこの時間が経過するまでは変化時送信を保留するバス負荷保護機構を
持っています。`WarningStatus` にこれを適用しました。

```
WarningStatus (IPduId=1):
  MinDelayMs = COM_TX_MIN_DELAY_WARNINGSTATUS_MS（既定 100ms）

Com_MainFunction()（DIRECT/MIXED 共通、周期フロアには適用しない）:
  mdtElapsed = (経過時間 >= MinDelayMs)
  changeDue  = Com_TxPending[WarningStatus] && mdtElapsed
  due        = changeDue || floorDue（MIXED の周期フロア。MDT の影響を受けない）

  due さもなくば:
    何もしない（Com_TxPending は立てたまま破棄しない → 次回以降 MDT 満了時に送信）
```

**「破棄」ではなく「保留」であることが重要**: MDT 未満で変化検知があっても
`Com_TxPending[]` はクリアされません。次回以降の `Com_MainFunction()`
（100ms 周期）で MDT が満了していれば、そのとき初めて送信されます
（SWS_Com_00471/00698/00789 準拠。値そのものを取りこぼすわけではなく、
「送るタイミングを遅らせるだけ」という設計です）。

**なぜ周期フロアには適用しないか**: 実 AUTOSAR は既定
（`ComEnableMDTForCyclicTransmission=false`）で MIXED の周期部分・PERIODIC には
そもそも MDT タイマを起動しません（SWS_Com_00789）。周期送信は既に
`TxPeriodMs` 自身がバス負荷の上限を決めているため、MDT による追加の間引きは
不要という考え方です。本実装もこれに倣い、`floorDue` は `mdtElapsed` の
影響を受けません。

**この実装で MDT が実際に保留を発生させる場面はあるか**: 正直に言うと、
現状の ASW 呼び出しパターンでは稀です。`App_WarningIndicator_Run()` は
500ms 周期でしか `Com_SendSignalGroup()` を呼ばないため、
`COM_TX_MIN_DELAY_WARNINGSTATUS_MS`（100ms）を下回る間隔で変化が連続することは
起こりません。つまり本プロジェクトの現在の信号源だけを見れば、MDT は
「普段は効かない保護的な既定値」です。それでも、(1) 実車では複数の CDD や
より高頻度な信号源が同じ I-PDU に寄与しうるため Com 自身がこの保護を持つ
意味があること、(2) 本実装のロジック自体は正しく機能すること（`Com_TxPending`
の保留・次回満了時送信という一連の流れ）を確認する目的で、Com の標準機能として
実装しています。

##### ComNotification拡張（Tx確定コールバック、Com_CbkTxAck）

これまでの `Com_TxConfirmation()` は PduR から送信完了を受け取ってログ出力する
だけで、それより上位（ASW/Rte）へは何も伝えていませんでした。実 AUTOSAR の
Com は、I-PDU の送信が実際に成功した際、そこに含まれるシグナル/Signal Group
ごとに個別のコールバック（`Com_CbkTxAck`、SWS_Com_00468。実車の RTE 生成名は
`Rte_COMCbkTAck_<sn>`/`<sg>`）を呼び出せます。`ComRxDataTimeoutAction`/
`ComDataInvalidAction` が RX 側の「値の異常」を扱う機能だったのに対し、これは
TX 側の「送信できたことの確認」を扱う機能です。

本プロジェクトでは `MeterStatus` の `EngineState` に `TxAckCbk` を設定しました。

```
Com_SignalConfigType (EngineState):
  TxAckCbk = Rte_COMTxAck_EngineState

Com_TxConfirmation(TxPduId=0/*MeterStatus*/, result=E_OK)  ← PduR から呼ばれる
  Com_ConfigPtr->Signals[] を走査
    sig->Direction==TX かつ sig->IPduId == TxPduId かつ sig->TxAckCbk != NULL
    のものすべてについて sig->TxAckCbk() を呼ぶ  ← EngineState だけでなく、
                          同じ I-PDU の全 TX シグナルが対象（本設定では
                          EngineState のみ）
```

**シグナル単位に統一した理由**: 実 AUTOSAR は signal 単位・signal group 単位で
別々のコールバック名を持てますが、本実装は `TmsContributor`/`TransferProperty`/
`RxDataTimeoutAction` 等これまでのフィールドと同じく、シグナル単位の
`TxAckCbk` のみで統一しています。Signal Group（`WarningStatus`）のメンバーに
`TxAckCbk` を設定した場合も、`Com_ConfigPtr->Signals[]` を素直に走査するだけの
実装なので、そのメンバー個別に呼ばれます（実 AUTOSAR の「signal group 全体で
1 回」という意味論とは異なる簡略化です）。

**レビューで見つかった問題（RX/TX の方向誤認）**: 初期実装は
`sig->IPduId == TxPduId` だけで走査対象を絞っており、方向（RX/TX）を
確認していませんでした。RX I-PDU と TX I-PDU の `IPduId` は別々の値空間で
（どちらも 0 始まり）数値が重複します（例: RX の `EngineInfo=0` と TX の
`MeterStatus=0`）。そのため `Com_TxConfirmation(TxPduId=0, ...)`
（`MeterStatus` の送信確認）が呼ばれると、本来対象であるべき TX 側の
`EngineState` だけでなく、たまたま `IPduId=0` である RX 側の
`EngineInfo`（`EngineSpeed`/`CoolantTemp`/`EngineOnFlag`）まで走査対象に
入ってしまいます。当時はどの RX シグナルにも `TxAckCbk` が設定されていな
かったため実害はありませんでしたが、コード上は何も防いでおらず、将来
誰かが（コピペ等で）RX シグナルに `TxAckCbk` を設定してしまうと、無関係な
TX I-PDU の送信完了のたびにそのコールバックが静かに誤発火する
（クラッシュも DET ログも出ないため発見しづらい）バグになり得ました。

`Com_SendSignal()`/`Com_ReceiveSignal()` は `SignalId` で 1 エントリを検索
してから `Com_FindTxIPdu()`/`Com_FindRxIPdu()` で方向を確認する設計のため
この曖昧さの影響を受けませんが、`Com_TxConfirmation()` は逆に
「`Signals[]` 全体を `IPduId` だけで検索する」という、本実装で初めて
現れた走査パターンだったために問題が顕在化しました。

修正として `Com_SignalConfigType` に `Direction`（`Com_SignalDirectionType`:
`COM_SIGNAL_DIRECTION_RX`/`_TX`）フィールドを新設し、全 12 シグナルへ明示的に
設定したうえで、`Com_TxConfirmation()` の走査条件に
`sig->Direction == COM_SIGNAL_DIRECTION_TX` を追加しました。実 AUTOSAR には
対応パラメータがありません（ComSignal は必ずどちらか一方の ComIPdu に構造的に
含まれるため、そもそもこの曖昧さが発生しない）。本プロジェクトが RX/TX 共通の
1 本の配列に平坦化した簡略設計を採用したことで生じた、本プロジェクト固有の
補正です。

**このコールバックはこの送信でシグナル自体が変化したかどうかを問わない**:
`Com_CbkTxAck` は「I-PDU の送信が成功した」ことの通知であり、含まれる個々の
シグナルの値がこの送信で変化したかどうかとは無関係です（SWS_Com_00468:
"called immediately after successful transmission of the I-PDU containing
the message"）。`EngineState` が変化していなくても、`MeterStatus` が周期
フロア（MIXED モード）で再送されるたびに `Rte_COMTxAck_EngineState()` が
呼ばれます。

**割り込み安全性について（前節の教訓を踏まえた確認）**: `Com_TxConfirmation()`
は `Can_MainFunction_Write()`（Os の 100ms タスク）→ `CanIf_TxConfirmation()`
→ `PduR_CanIfTxConfirmation()` という経路で同期的に呼ばれます。前節の
`ComInvalidNotification` とは異なり、この経路上には SchM 排他エリア
（割り込み禁止区間）が存在しないことを実際にコードを辿って確認済みです。
したがって `TxAckCbk` の中で `DET_LOGI` のような Serial 出力を行っても、
前節の WDT リセット障害と同じ問題は起きません。

**この機能は実際に発動するか**: 発動します。`MeterStatus` は `TxModeMode=MIXED`
（値変化時の即時送信 + 周期フロア再送）のため、通常運用で確実に送信され続け、
そのたびに `Com_TxConfirmation()` → `Rte_COMTxAck_EngineState()` が呼ばれます。
ログに `Com: TxConf id=0` の直後に `Rte: MeterStatus TX ack (EngineState)`
が出力されることを実機で確認できます。

##### Update Bit（送信側が実際に更新したかを示す1ビット）

これまでの機能はいずれも「値そのもの」（無効値パターン、タイムアウト、送信成功）
に関するものでした。update-bit（実 AUTOSAR 7.8 章）はこれらとは別の軸で、
「送信側がこのシグナル/シグナルグループを実際に更新して送ったかどうか」を示す
1 ビットです（SWS_Com_00055: シグナル/グループの値そのものとは独立に、Com が
内部でのみ扱う）。共有 I-PDU に複数の送信元・複数のシナリオが値を書き込みうる
構成や、周期送信と変化時送信を併用する MIXED モードで、「このフィールドは今回の
フレームで本当に新しい値が入っているか」を受信側が区別したい場合に使います。

実 AUTOSAR の `ComUpdateBitPosition`（ECUC_Com_00257）は `ComSignal`
（非 Signal Group の単一シグナル、SWS_Com_00061）・`ComSignalGroup`
（グループ全体、SWS_Com_00801）双方に同名パラメータとして存在します。本実装は
これを `Com_IPduConfigType.UpdateBitPosition` という I-PDU 単位のフィールド 1 個に
簡略化していますが、値のセット元が「非 Signal Group なら `Com_SendSignal()`
（SWS_Com_00061）」「Signal Group なら `Com_SendSignalGroup()`（SWS_Com_00801）」の
どちらであっても、クリア（`Com_DoTransmit()`）は同じコードで扱います。

現状の適用状況:

- **`MeterStatus`/`EngineState`（TX、非 Signal Group）: 適用済み・実機検証済み**。
  `UpdateBitPosition=8`（byte[1] bit0）を設定しています。`TxModeMode=MIXED`
  のため、「今回の送信は実際の値変化によるものか、単なる周期フロア再送か」を
  受信側が区別できる、という update-bit 本来の動機がそのまま当てはまる例です。
- **`WarningStatus`（TX、Signal Group）: 適用済み・実機検証済み**。
  `UpdateBitPosition=3`（byte[0] bit3。RunLamp/FaultLamp/AbsLamp が使う
  bit0-2 の次の空きビットのため DLC 拡張は不要）を設定しています。
  TMS=true（FAULT/ABS 点灯中）時の MIXED 周期フロア再送と、実際に警告灯が
  変化したことによる送信とを区別できる、Signal Group 単位の実装例です。

```
送信側 非 Signal Group（Com_SendSignal）:
  実バッファへ反映（既存処理）
  ComFilterAlgorithm が「変化あり」と判定した場合のみ、
  UpdateBitPosition が 0xFF 以外ならそのビットをセット
  （SWS_Com_00061。本実装独自の条件付け、詳細は次項）
送信側 Signal Group（Com_SendSignalGroup）:
  実バッファへ反映（既存処理）
  Com_GroupTriggerPending（TRIGGERED_ON_CHANGE メンバーが実際に変化したか）
  が立った場合のみ、UpdateBitPosition が 0xFF 以外ならそのビットをセット
  （SWS_Com_00801。こちらも本実装独自の条件付け、詳細は次項）
送信側（Com_DoTransmit、Com_MainFunction() から。Signal Group かどうかを問わない）:
  PduR_Transmit() で実送信（この時点でビット=1 のまま送信される）
  ret==E_OK のときのみ、送信直後にビットをクリア
  （SWS_Com_00062: ComTxIPduClearUpdateBit=Transmit）
```

**動作確認方法**: `uds_tester` の「EngineStatus (0x200)」rx_monitor ボタンに
`upd=0/1` の表示を追加しました。ダッシュボードの RUNNING/STARTING/FAULT 等の
状態遷移で `EngineState` が変化した直後は `upd=1`、その後
`COM_TX_PERIOD_METERSTATUS_FLOOR_MS`（既定 9000ms）間隔で値が変化せずに再送
される周期フロアフレームでは `upd=0` になることを確認できます。同様に
「WarningStatus (0x210)」rx_monitor ボタンにも `upd=0/1` の表示を追加しました。
FAULT/ABS 点灯による TMS=true（MIXED 切り替え）中、警告灯が実際に変化した
送信は `upd=1`、`COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS` 間隔の周期フロア
再送は `upd=0` になります。Cangaroo で CAN 0x200 の byte[1] bit0（生の 2 バイト
目、MSB）・CAN 0x210 の byte[0] bit4 を直接観察することでも同様に確認できます。

**レビューで見つかった問題（非 Signal Group の update-bit が常に 1 のままだった）**:
初期実装は SWS_Com_00061 の原文どおり「`Com_SendSignal()` が呼ばれるたびに
無条件でセットする」としていましたが、実機検証で `upd` が常に `1` のままになる
不具合が見つかりました。

原因は ASW（`App_EngineManager_Run()`）の呼び出しパターンとの相互作用です。
本プロジェクトの ASW は「値が変わったかどうか」を判定せず、毎サイクル無条件に
`Rte_Write_EngineStatus_EngineState()`（→ `Com_SendSignal()`）を呼び、実際に
送信するかどうかの判断は Com の `ComFilterAlgorithm` に完全に委ねる設計です
（前述「責務分離の効果」）。SWS_Com_00061 を文字どおり実装すると、実送信
（イベント駆動・周期フロアいずれも）から次の実送信までの間に、ASW が
`Com_SendSignal()` を何度も（無変化のまま）呼び続けるため、`Com_DoTransmit()`
がクリアした直後には必ず再セットされてしまい、update-bit が「実際に変化した
かどうか」を一切表せなくなっていました。

対策として、非 Signal Group の update-bit セットを `ComFilterAlgorithm` の
「変化あり」判定（`passesFilter`、`Com_RequestTxOnChange()` を呼ぶかどうかと
同じ判断軸）に条件づけました。ASW 側の「常に書き込む」設計はそのまま変えず、
Com 側が既に持っている「これは実際の変化か」の判断をそのまま update-bit にも
使い回す形です。これは SWS_Com_00061 の文字どおりの実装ではありませんが、
本プロジェクトの ASW 呼び出し規約のもとで update-bit 本来の目的（実際に更新
されたかどうかを示す）を満たすための意図的な調整です。

Signal Group 側（`Com_SendSignalGroup()`）も同種の問題を抱えていました。
`App_WarningIndicator_Run()` も毎サイクル無条件に
`Rte_SendSignalGroup_WarningStatus()`（→ `Com_SendSignalGroup()`）を呼ぶ同じ
設計のため、`WarningStatus` に `UpdateBitPosition` を設定した時点で
（非 Signal Group と同じ理由により）update-bit が常に 1 のままになることは
実装前から予見できました。そのため `WarningStatus` へ update-bit を適用する
のと同時に、`Com_SendSignalGroup()` 側も対策済みです: セットの条件を
`Com_GroupTriggerPending`（`ComTransferProperty=TRIGGERED_ON_CHANGE` の
メンバーが実際に変化したかどうか、`Com_RequestTxOnChange()` を呼ぶかどうかと
同じ判断軸）に条件づけました。非 Signal Group 側の `passesFilter` と対になる
考え方です。

**なぜ ComTxIPduClearUpdateBit=Transmit のみか**: 実 AUTOSAR は Transmit
（`PduR_ComTransmit` 呼び出し直後にクリア）/ Confirmation（送信確認後にクリア）/
TriggerTransmit（トリガ送信後にクリア）の 3 択ですが、本実装は Transmit のみ
実装しています。本プロジェクトの送信経路は `Com_MainFunction()` →
`Com_DoTransmit()` → `PduR_Transmit()` が同期的に SPI 送信まで完了する
（実際の送信完了と `PduR_ComTransmit` 呼び出しの間に有意な時間差がない）ため、
3 つのタイミングの違いを意味のある形で再現できないという判断です。

**レビューで見つかった問題（update-bit クリアが送信結果を無視していた）**:
このコード自体は、実際に `WarningStatus` へ試験的に適用していた開発中の段階で
見つかった問題を修正済みです。初期実装は `Com_DoTransmit()` 内で
`PduR_Transmit()` の戻り値 `ret` を見ずに update-bit を無条件でクリアして
いました。しかし本節が引用する SWS_Com_00062 の原文は "after this I-PDU was
sent out via PduR_ComTransmit **and PduR_ComTransmit returned E_OK**" であり、
送信成功時のみクリアすべきと明記されています。当時のコードコメントは
「戻り値によらず無条件でクリアする」と、この食い違いを自覚的に開示しないまま
断定していました。

具体的な失敗シナリオ（`WarningStatus` に適用していた場合を想定）: この I-PDU
は `TxModeModeTrue=MIXED`（TMS=true 時に周期フロア再送あり）のため、
(1) `FaultLamp` 点灯で `Com_SendSignalGroup()` が update-bit をセット、
(2) バス輻輳等で最初の送信が失敗（`ret=E_NOT_OK`）しても update-bit だけが
クリアされてしまう、(3) データ自体はバッファに残ったまま次の周期フロアで
再送され、そちらは成功する——という流れになると、実際には初めて正常配信
された新データにもかかわらず、受信側から見ると update-bit が 0（＝「未更新」）
のフレームとして届いてしまいます。

同じファイル内の `Com_TxConfirmation()` は `if (result != E_OK ...) return;`
と正しく結果をチェックしており、この判定パターン自体は目新しいものでは
ありませんでした。修正は `if (ret == E_OK && ...)` という条件の追加のみです。

この修正は `MeterStatus`（非 Signal Group）の送信経路でも共通に使われるため、
Signal Group への適用有無に関わらず活きています。

##### RX Signal Group（複数シグナルの一貫したスナップショット読み出し）

ここまでの Signal Group（`Com Signal Group`/`ComTransferProperty` 節）は TX 側の
話でした。実 AUTOSAR の Com は RX 側にも対称の仕組みを持ちます:
`Com_ReceiveSignalGroup()`（SWS_Com_00201）が I-PDU バッファの内容を
「RX シャドウバッファ」へアトミックにコピーし（SWS_Com_00051/00638）、
以降の `Com_ReceiveSignal()` 呼び出しはこのシャドウバッファを読みます。
これにより、同じグループに属する複数シグナルを別々の `Com_ReceiveSignal()`
呼び出しで読む間に新しいフレームが届いても（`Com_RxIndication()` が
`Com_RxBuffer` を上書きしても）、読み取り側は常にこの呼び出し時点の一貫した
スナップショットを見ます。

本プロジェクトでは `AbsInfo`（RX IPduId=1、`VehicleSpeed`/`BrakeActive`/`AbsActive`
の 3 シグナル）に `IsSignalGroup=1` を設定し、これを実装しました。

```
Com_RxIndication(AbsInfo)  ← CAN フレーム受信のたびに呼ばれる
  Com_RxBuffer[1] を更新、Com_RxTimedOut[1] = 0 にリセット
  RxIndicationCbk（Rte_COMCbk_AbsInfo）を呼ぶ

Rte_COMCbk_AbsInfo()
  Com_ReceiveSignalGroupArray(1, buf) で生バイト列を取得 → E2E 検証
  検証 OK なら:
    Com_ReceiveSignalGroup(1U)
      Com_RxBuffer[1] を Com_RxShadowBuffer[1] へコピー（確定コピー）
      Com_RxShadowTimedOut[1] = Com_RxTimedOut[1]（この時点は 0）
    Com_ReceiveSignal(VEHICLE_SPEED, ...)  ← Com_RxShadowBuffer[1] から読む
    Com_ReceiveSignal(BRAKE_ACTIVE,  ...)  ← 同上
    Com_ReceiveSignal(ABS_ACTIVE,    ...)  ← 同上
```

**なぜ TX 側と対称の実装にしたか**: `Com_SendSignalGroup()`（TX 側）と同じ
「シャドウバッファ経由でしか実データへ触れない」設計を RX 側にも適用することで、
Signal Group という概念が送受信どちらの向きでも同じ形（アトミックなコミット/
コピーの単位）で表現できることを確認する目的です。実装は
`Com_FindTxIPdu()`/`Com_TxShadowBuffer` に対する `Com_FindRxIPdu()`/
`Com_RxShadowBuffer` というほぼ 1 対 1 の対称構造になっています。

**タイムアウト判定もスナップショットする理由**: `Com_ReceiveSignal()` が
グループメンバーに対して見るのは、ライブの `Com_RxTimedOut[]` ではなく
`Com_ReceiveSignalGroup()` 実行時点でスナップショットした
`Com_RxShadowTimedOut[]` です。ライブの値を都度参照すると、同じグループの
複数メンバーを読む間にデッドライン監視タイムアウトが発生した場合、
1 本目は成功・2 本目は `E_NOT_OK` という不整合が起こり得ます（せっかく
データはアトミックにコピーしても、可否判定だけが後から変わってしまう）。
スナップショットすることで、データと可否判定を 1 つの一貫した単位として
扱えます。

**この実装で実際に不整合が起きる場面はあるか**: 正直に言うと、現状はありません。
`Com_ReceiveSignalGroup(1U)` の呼び出しと、それに続く 3 回の `Com_ReceiveSignal()`
呼び出しはすべて `Rte_COMCbk_AbsInfo()` という 1 つの同期呼び出し列の中で行われ、
かつこの関数自体が `Com_RxIndication()` からフレーム受信直後・`Com_RxTimedOut`
リセット直後に呼ばれます。したがってこのシーケンスの実行中にタイムアウトが
新規発生することはなく（本実装は非プリエンプティブなため、他のタスクがこの間に
割り込むこともありません）、シャドウバッファなしでも同じ結果になります。
`TMS`/`MDT`/`ComTransferProperty` と同じく、動機は実利より仕様忠実性です。
シャドウバッファが実際に意味を持つのは、ASW Runnable が `Com_ReceiveSignal()`
をフレーム受信タイミングとは無関係な任意のタイミングで直接呼ぶような構成
（本プロジェクトの E2E Transformer 方式とは異なり、RTE ミラーを経由しない構成）
です。

##### ComRxDataTimeoutAction（受信タイムアウト時のシグナル値の扱い）

RX 受信デッドライン監視（次節）がタイムアウトを検出したあと、`Com_ReceiveSignal()`
がそのシグナルに対してどう振る舞うかは、実 AUTOSAR では `ComRxDataTimeoutAction`
（ECUC_Com_00412）で設定できます: 何もしない（NONE）、`ComSignalInitValue`
（起動時の初期値）で置き換える（REPLACE）、`ComTimeoutSubstitutionValue`
（このシグナル専用の代替値）で置き換える（SUBSTITUTE）の 3 択です。SUBSTITUTE の
根拠要求は対象が非グループシグナルか Signal Group メンバーかで異なり、前者は
SWS_Com_00875、後者（今回の VehicleSpeed）は SWS_Com_00876 です。

本プロジェクトはこれまで NONE 相当の動作（`Com_ReceiveSignal()` が値を書き込まず
`E_NOT_OK` を返すだけ）のみをサポートしており、「タイムアウト中は安全な代替値を
返す」判断は常に呼び出し元（ASW/RTE）任せでした。これに `COM_RX_TIMEOUT_ACTION_SUBSTITUTE`
を追加し、`AbsInfo` の `VehicleSpeed`（RX Signal Group メンバー）に適用しました。

```
Com_SignalConfigType (VehicleSpeed):
  RxDataTimeoutAction      = COM_RX_TIMEOUT_ACTION_SUBSTITUTE
  TimeoutSubstitutionValue = 0xFFFF  （655.35 km/h 相当、物理的にあり得ない値）

Com_ReceiveSignal(VEHICLE_SPEED, &out)  ← AbsInfo がタイムアウト中の場合
  RxDataTimeoutAction を確認
    COM_RX_TIMEOUT_ACTION_SUBSTITUTE → I-PDU バッファは読まず、
                                        out = 0xFFFF を書き込んで E_OK を返す
    COM_RX_TIMEOUT_ACTION_NONE（既定） → 何も書き込まず E_NOT_OK を返す（既存の全シグナルの挙動）
```

**なぜ REPLACE ではなく SUBSTITUTE を実装したか**: 本プロジェクトは
`ComSignalInitValue` という設定概念自体を持たず、RX バッファは `Com_Init()` で
単純にゼロクリアするのみです。REPLACE は「タイムアウト時 = 起動直後と同じ
初期値（0）」を返すだけなので、停車中で本当に速度が 0 の場合と区別がつきません。
SUBSTITUTE なら 0xFFFF のような、通常運用では絶対に出現しない値を割り当てられる
ため、「値が来ていない」ことを呼び出し元が確実に判別できます。

**この実装で実際に SUBSTITUTE が発動する場面はあるか**: 正直に言うと、
現状はありません。`VehicleSpeed` を読む `Com_ReceiveSignal()` 呼び出しは
`Rte_COMCbk_AbsInfo()` の中の 1 箇所のみで、これは `Com_RxIndication()` から
フレーム受信直後・`Com_RxTimedOut` リセット直後に同期的に呼ばれるため
（RX Signal Group 節で述べた理由と全く同じ）、この呼び出し自体がタイムアウト中に
実行されることはありません。加えて、この経路はそもそも E2E 検証済みの値を
`Rte_AbsInfoMirror` へコピーするだけで、ASW（`App_EngineManager` 等）は
`Rte_AbsInfoMirror` 経由でしか値を読まず、`Com_ReceiveSignal()` を直接呼ぶことも
ありません。

**「ASW の呼び出し方を変えれば発動するようになるか」— 実は Signal Group の場合は
それだけでは不十分**: `VehicleSpeed` は Signal Group メンバーのため、
`Com_ReceiveSignal()` が見る「タイムアウトしているか」は、呼び出し時点の
ライブな `Com_RxTimedOut` ではなく、直近の `Com_ReceiveSignalGroup()` 呼び出し
時点でスナップショットした `Com_RxShadowTimedOut` です（`Com_ReceiveSignalGroup()`
自身は呼び出し時点のライブな `Com_RxTimedOut` を見るため、SWS_Com_00876 の
「タイマ満了時」という要求はこの関数自身の呼び出し時点では正しく満たされます）。
つまり、たとえ将来 ASW が E2E Transformer のような中間ミラーを介さず
`Com_ReceiveSignal()` を直接、フレーム受信タイミングとは無関係な任意のタイミングで
呼ぶ構成に変えたとしても、その直前に `Com_ReceiveSignalGroup()` を呼ばない限り、
「タイムアウトが実際に発生した瞬間」ではなく「最後に `Com_ReceiveSignalGroup()`
を呼んだ時点」の状態しか観測できません。これは本プロジェクトの呼び出し方の
都合ではなく、Signal Group が「`Com_ReceiveSignal()` はシャドウバッファのみを
読み、ライブな I-PDU バッファ・ライブなタイムアウト状態には一切触れない」という
設計そのものに由来する構造的な性質です（実 AUTOSAR も同様。RX Signal Group 節で
引用した「the RTE accesses the group signals in the shadow buffer」参照）。

`TMS`/`MDT`/`ComTransferProperty`/RX Signal Group と同じく、動機は実利より
仕様忠実性です。

##### Rx無効値検知（ComSignalDataInvalidValue/ComDataInvalidAction）

`ComRxDataTimeoutAction`（前節）が「一定時間フレームが来ない」という**時間ベース**
の異常を扱うのに対し、こちらは「フレームは正常に届いているが、中身の値そのものが
送信元によって明示的に『無効』とマークされている」という**値ベース**の異常を
扱います。実車ではセンサ断線・短絡を検知した ECU が、物理的にありえない特定の
ビットパターン（例: 8bit センサ値の 0xFF）を意図的に送信する、という運用がよく
使われます。実 AUTOSAR は `ComSignalDataInvalidValue`（ECUC_Com_00391）で
この「無効値パターン」を、`ComDataInvalidAction`（ECUC_Com_00314、NOTIFY/REPLACE）
で受信時の振る舞いを設定できます。

本プロジェクトは `EngineInfo` の `CoolantTemp`（8bit、非 Signal Group）に
`COM_DATA_INVALID_ACTION_NOTIFY` を適用しました。

```
Com_SignalConfigType (CoolantTemp):
  DataInvalidAction      = COM_DATA_INVALID_ACTION_NOTIFY
  InvalidValue           = 0xFF  （水温センサ異常マーカー）
  InvalidNotificationCbk = Rte_COMInvalidNotify_CoolantTemp

Com_ReceiveSignal(COOLANT_TEMP, &out)  ← CoolantTemp のバイトが 0xFF の場合
  DataInvalidAction を確認
    COM_DATA_INVALID_ACTION_NOTIFY → 受信値を signal object へ格納しない
                                      （Com_RxLastValidValue を更新しない）。
                                      out = 直近の有効値を書き込んで E_OK。
                                      Com_RxInvalidNotifyPending[] を立てる
                                      （InvalidNotificationCbk はまだ呼ばない）
    COM_DATA_INVALID_ACTION_NONE（既定） → 無効値チェックをせず、受信値を
                                            そのまま返す（既存の全シグナルの挙動）

Com_MainFunction()  ← 次回の Os 100ms タスク呼び出し
  Com_RxInvalidNotifyPending[] が立っているシグナルについて
  InvalidNotificationCbk()（Rte_COMInvalidNotify_CoolantTemp）を呼ぶ
```

**なぜ REPLACE ではなく NOTIFY を実装したか**: `ComRxDataTimeoutAction` の
REPLACE を実装しなかった理由と全く同じです。実 AUTOSAR の REPLACE
（SWS_Com_00681）はシグナルを `ComSignalInitValue` へ置き換えますが、本
プロジェクトはその設定概念自体を持ちません。NOTIFY（SWS_Com_00680/00717）は
「シグナルオブジェクトへ格納しない＝直近の有効値を返し続ける」という、
`ComSignalInitValue` に依存しない動作のため、こちらのみ実装しています。

**なぜ「直近の有効値」を返せるのか（新規追加した内部状態）**: これまでの
`Com_ReceiveSignal()` は I-PDU バッファ（または RX シャドウバッファ）を毎回
その場でアンパックするだけで、シグナル単位の「最後に確定した値」を別途保持して
いませんでした。SWS_Com_00717 の「次回の Com_ReceiveSignal は直近の有効値を
返す」を実現するには、無効値を弾いたあとの値をどこかに憶えておく必要があるため、
`Com_FilterLastValue`（TX 側フィルタ用）と対になる `Com_RxLastValidValue`
（シグナルごとの直近有効値キャッシュ）を新設しました。無効値を検知した呼び出しは
このキャッシュを更新せず、そうでない呼び出しは毎回更新します。

**この機能は実際に発動するか**: これまでの `SUBSTITUTE`/RX Signal Group とは
違い、正直に言うと **これは実際に発動します**。`CoolantTemp` を読む
`Com_ReceiveSignal()` は `Rte_COMCbk_EngineInfo()`（RxIndicationCbk）内で、
E2E 検証に成功した**すべての** `EngineInfo` フレームに対して毎回呼ばれます。
すなわち、送信元が `CoolantTemp=0xFF` を含む（E2E CRC/Counter は正しい）
フレームを送信しさえすれば、この無効値検知ロジックは通常運用の経路で確実に
通過します（タイムアウト系の機能のように「このコールサイトは条件的に
到達しない」という限界がありません）。`tools/uds_tester/config.json` の
EngineInfo プリセットに「水温センサ異常 (0xFF)」を追加済みで、実機で
送信すると `Rte_EngineInfoMirror.temp` が更新されず、ログに
`Rte: CoolantTemp invalid value received (sensor fault pattern)` が出力される
ことを確認できます。

**実機検証で見つかった障害（コールバックを即時実行しない理由）**: 開発初期の
実装は、`Com_ReceiveSignal()` が無効値を検知した、まさにその場で
`InvalidNotificationCbk()`（`Rte_COMInvalidNotify_CoolantTemp()`、中身は
`DET_LOGW` による Serial 出力）を同期的に呼んでいました。実機で
`CoolantTemp=0xFF` のフレームを送信したところ、フレーム受信直後に
WDT（ウォッチドッグタイマ）リセットが発生しました。

原因は次の通りです。`CoolantTemp` を読む `Com_ReceiveSignal()` の呼び出しは、
`Rte_COMCbk_EngineInfo()` 内の `SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA()` /
`SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA()` の間（`Rte_EngineInfoMirror` を
`Rte_Read_*` との競合から守るための排他区間）で行われます。この排他エリアの
実体は Arduino の `noInterrupts()`/`interrupts()`、すなわち**グローバル割り込み
禁止**です（`SchM_Hw.cpp` 参照）。この区間の内側で `Serial.print()` 系の
`DET_LOGW` を呼んだ結果、UART 送信バッファが埋まった時点で「送信完了割り込みが
バッファを空けるのを待つ」ループに入りましたが、割り込みそのものが禁止されている
ため空きが永久に生まれず、事実上の無限ループになりました。CAN 受信処理や WdgM の
リフレッシュもすべて止まるため、最終的に WDT が満了してリセットに至りました。

修正として、`InvalidNotificationCbk()` の実呼び出しを `Com_ReceiveSignal()` の
呼び出しスタックフレームから切り離し、`Com_RxInvalidNotifyPending[]` フラグを
立てるだけにして、実際の呼び出しは必ず `Com_MainFunction()`（Os の 100ms
タスク、割り込み禁止区間の外）側で行うようにしました。これは `Com_TxPending`
（`Com_SendSignal()` から `PduR_Transmit()` の実行を切り離す、既存の仕組み）と
全く同じ設計思想です。この教訓は `ComInvalidNotification` に限らず一般化できます:
**Com のコールバック系フック（`RxIndicationCbk`/`TxTransformCbk`/
`InvalidNotificationCbk` 等）は、それ自身がどの呼び出しコンテキスト（割り込み
禁止区間の内側かどうか）から呼ばれるか呼び出し側の事情に左右されるため、
ブロッキングする可能性のある処理（Serial 出力、長時間のループ等）を含めては
ならない。**

##### RX ComFilterAlgorithm（受信フィルタ、プラウジビリティチェック）

`ComFilterAlgorithm` はこれまで TX シグナルの送信要否判定・TMS 評価にのみ
使っていました（`COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD`/
`COM_FILTER_MASKED_NEW_DIFFERS_X`）。実 AUTOSAR の仕様書を確認すると、
これらは「TMC（Transmission Mode Condition）の評価」であり値そのものは
破棄しないのに対し、値を実際に「破棄」するフィルタリングは RX 専用の概念だと
明記されています。

```
[SWS_Com_00695] The AUTOSAR COM module shall filter out signals only at
receiver side. (SRS_Com_02037)

[SWS_Com_00602] The AUTOSAR COM module shall use filtering mechanisms on
sender side for Transmission Mode Conditions (TMC) but it shall not filter
out signals on sender side. (SRS_Com_02083)
```

つまり、これまで TX 側で使っていた `ComFilterAlgorithm` は仕様上正しい用法
（TMC 評価）だった一方、「受信値そのものを検証して怪しければ捨てる」という
本来の意味でのフィルタリングは未実装でした。この非対称性を解消する形で、
RX シグナル向けに `COM_FILTER_NEW_IS_WITHIN` を追加しました。

###### 適用例 — EngineSpeed のプラウジビリティチェック

`EngineSpeed`（RX、非 Signal Group、CAN 0x100 経由）に
`FilterAlgorithm=COM_FILTER_NEW_IS_WITHIN`、`FilterMin=0`、`FilterMax=8000`
（本プロジェクト想定エンジンのレッドライン相当 rpm）を設定しました。
`VehicleSpeed`（`ComRxDataTimeoutAction`節で既出）ではなくこちらを選んだ
理由は、`VehicleSpeed` は RX Signal Group メンバーで `Com_ReceiveSignal()`
が `Rte_COMCbk_AbsInfo()`（フレーム受信直後の 1 箇所）でしか実際には
呼ばれず、既に「SUBSTITUTE は現状のアーキテクチャでは実際には発動しない」
という限界が記録済みだったのに対し、`EngineSpeed` は非グループシグナルで
`Rte_COMCbk_EngineInfo()` から毎フレーム実際に `Com_ReceiveSignal()` が
呼ばれるため、このフィルタが確実に効くからです。

```c
[SWS_Com_00273] If the AUTOSAR COM module filters out a signal on receiver
side, i.e. filter condition evaluates to false, the AUTOSAR COM module shall
discard that signal and shall not process it.
```

`Com_ReceiveSignal()` は範囲外の値を検知すると、`ComDataInvalidAction` と
同じ「シグナルオブジェクトへ格納しない」動作（`Com_RxLastValidValue[s]` を
更新せず、直近の合格値を返し続ける）を行います。通知コールバック
（`FilterRejectCbk`、実 AUTOSAR の `ComNotification` 相当）の実呼び出しは、
`ComDataInvalidAction`/`InvalidNotificationCbk` と全く同じ理由・同じ仕組みで
次回 `Com_MainFunction()` まで遅延します（`Com_ReceiveSignal()` は
`Rte_COMCbk_EngineInfo()` の `SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()`
内側から呼ばれるため、その場でコールバックを直接呼ぶと WDT リセットを
引き起こしうる教訓を踏襲）。

###### 明示する簡略化

- 実 AUTOSAR には他に `NEW_IS_OUTSIDE`/`MASKED_NEW_EQUALS_X`/`NEVER`/
  `ONE_EVERY_N` もありますが、「物理的にあり得ない受信値を弾く」という
  具体的なシナリオに最小限必要な `NEW_IS_WITHIN` のみ実装しています。
- `ComSignalInitValue`（`Com_SignalConfigType.InitValue`）は実装済みです。
  `[SWS_Com_00603]`（起動時 old_value を ComSignalInitValue にする）どおり、
  `Com_Init()`/`Com_IpduGroupStart(initialize=true)` で `Com_RxLastValidValue`
  を InitValue から初期化します（既定 0 のシグナルは従来どおりゼロクリアと
  同じ結果になります）。`ComRxDataTimeoutAction`/`ComDataInvalidAction` の
  `REPLACE` アクションもこの InitValue を使って実装済みです。
- RX Signal Group への適用（`[SWS_Com_00836]`: グループ全体を破棄する）は
  未実装です（動機は上記の通り、実際に効くシナリオが非グループシグナルに
  あったため）。

###### 動作確認方法

`uds_tester` の EngineInfo ボタンに「回転数センサ異常 (65535rpm、RXフィルタ
で拒否されるはず)」プリセットを追加しました。送信すると、Arduino ログに
次のように出力されます（E2E CRC/Counter はプリセット選択時に自動計算される
ため、E2E 検証自体は正常に通ります）。

```
[NNNNms] INFO  Com: RX iPdu=0 [.. .. FF FF 50 80]
[NNNNms] WARN  Rte: EngineSpeed out of plausible range, rejected by RX filter (kept last valid value)
```

このとき `App_EngineManager` が読む `EngineSpeed` は直前の正常値のまま
更新されないため、後続の `Dem: FreezeFrame ev=6 spd=...` ログの `spd` にも
`65535` は一切現れません（プリセットを正常値に戻して送信すれば、通常どおり
即座に反映されます）。

##### 受信デッドライン監視（COM Deadline Monitoring）

COM モジュールが各 RX I-PDU の受信間隔を監視し、設定タイムアウト内にフレームが届かない場合に
上位層へエラーを通知します（AUTOSAR SWS_COM_00398 準拠）。

```
エンジン ECU がフレームを送り続けている間
  ↓ 受信のたびに
  Com_RxIndication() → Com_RxLastMs[0] = millis()   ← タイマリセット

100 ms ごとに（Task 5）
  Com_MainFunction()
    now - Com_RxLastMs[0] >= 5000 ms?
      YES → Com_RxTimedOut[0] = 1
             WARN ログ出力

3000 ms ごとに（Task 2）
  App_EngineManager_Run()
    Rte_Read_SpeedSensor_EngineSpeed()
      → Com_ReceiveSignal()
          Com_RxTimedOut[0] == 1 → return E_NOT_OK
    E_NOT_OK を検知
      → DEM_EVENT_COMM_TIMEOUT FAILED 報告
      → ENGINE_STATE_FAULT 遷移
      → LED 点滅（App_WarningIndicator がそのまま動く）
```

###### タイムアウト設定値（`Com_Cfg.h`）

| I-PDU | 定数 | 既定値 | フォールバック動作 |
|-------|------|--------|-----------------|
| EngineInfo (0x100) | `COM_TIMEOUT_ENGINE_INFO_MS` | 5000 ms | STARTING/RUNNING → FAULT |
| AbsInfo (0x110) | `COM_TIMEOUT_ABS_INFO_MS` | 5000 ms | AbsActive が 0 に戻り ABS 警告消灯 |

###### タイムアウト確認手順

1. RUNNING 状態に遷移させてから EngineInfo の送信を止める
2. 5 秒後：`WARN Com: RX timeout iPdu=0 (5000ms)` が出力される
3. さらに最大 3 秒後（次の Runnable 起動時）：`WARN AppEng: ->FAULT comm timeout` が出力される
4. LED が点滅に変わる
5. UDS SID 0x19 で DTC 0x000105 (COMM_TIMEOUT) が取得できる
6. EngineInfo を再送すると Com_RxTimedOut がリセットされ、次の Runnable サイクルで復帰する

##### update-bit の受信側判定（discard）

update-bit の概要、および TX 側（非 Signal Group・Signal Group）のセット/クリアの
仕組みは「Com」の「Update Bit」を参照してください。ここでは Signal Group の
RX 側判定ロジック（discard）のみを説明します。

```
受信側（Com_ReceiveSignalGroup、Signal Group のみ）:
  UpdateBitPosition が 0xFF 以外なら、I-PDU バッファの該当ビットを確認
    0（未更新）→ 何もせず E_OK で戻る（SWS_Com_00802: "discard"。
                  シャドウバッファ・タイムアウトスナップショットとも
                  直近の確定コピー内容のまま）
    1（更新済み）→ 通常どおり確定コピー（既存処理）
```

**受信側の「discard」は E_NOT_OK ではなく E_OK を返す**: `Com_ReceiveSignalGroup()`
は update-bit=0 の場合、何も更新せずに `E_OK` を返します。これは
`ComDataInvalidAction=NOTIFY` の「無効値を検知しても直近の有効値を返して
`E_OK`」という設計判断と揃えたものです（`E_NOT_OK` は「呼び出し自体が失敗した/
現在タイムアウト中」を意味する既存の用法と一貫させるため）。

**適用状況**: `AbsInfo`（RX、Signal Group）は現状
`UpdateBitPosition = 0xFFU`（未適用）です。実車のこの種のフレームは高頻度・
固定周期で常に新鮮なセンサ値を送り続けるのが通常で、鮮度管理は既存の受信
デッドライン監視で十分カバーされるため、update-bit を使う動機が薄いという
従来からの判断は変わっていません。したがって上記の discard ロジックは今のところ
実機で経路を通りません（TX 側のセット/クリアのみが `MeterStatus`/`WarningStatus`
を通じて実機検証済みです）。

<a id="det-compliance"></a>
#### DET 準拠（Det_ReportError による標準化エラー通知）

これまで Com モジュールの NULL チェック・範囲チェック・未初期化チェックは、
すべて `DET_LOGE(TAG, "自由文字列")`（`src/Bsw/Det/`、Serial 出力用の自作ロガー）
のみで報告していました。しかし `docs/AUTOSAR_SWS_COM.pdf` `[SWS_Com_00442]`
（7.13 章 Error Notification）は次のように要求しています。

```
When a development error is detected, the function Det_ReportError of the
Default Error Tracer shall be called with:
     50 as the AUTOSAR COM's ModuleId
     0 as InstanceId
     the service ID of the AUTOSAR COM module's API in which the error was
         detected as ApiId
     the error ID as defined in Chapter 7.12.1 as ErrorId
```

自作ロガーの`DET_LOGE`はこの標準化された `Det_ReportError(ModuleId, InstanceId,
ApiId, ErrorId)` 呼び出し（`docs/AUTOSAR_SWS_DefaultErrorTracer.pdf`
`[SWS_Det_00009]`）とは全くの別物で、上位の診断ツール・DET フックから
機械可読な形で捕捉できる手段が存在していませんでした。この非適合は個々の
機能追加時には指摘してこなかった、Com スタック全体に及ぶ体系的なギャップ
でした。

##### 対応方針

情報を失わないよう、既存の `DET_LOGE(...)`（呼び出し元固有の自由文字列、
人間が読むための詳細情報）は**そのまま残し**、標準化された
`Det_ReportError(...)` を**並行して**呼ぶようにしました。

```c
if (config == NULL)
{
    DET_LOGE(TAG, "Init E: config NULL");                              // 既存
    Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INIT, COM_E_PARAM_POINTER); // 追加
    return;
}
```

`Det_ReportError()` 自体は `Det.h`/`Det.c` に新規実装し、`DET_LOGE` とは
独立したチャネルとして `[<ms>ms] DET M=<ModuleId> I=<InstanceId> API=0x<ApiId>
ERR=0x<ErrorId>` という 1 行を出力します（本実装は実 AUTOSAR のコールアウト
フック登録・実行停止等の高度な機能は持たない学習用の簡略実装）。

エラーコードは `docs/AUTOSAR_SWS_COM.pdf` 7.12.1 章から検証した値をそのまま
`Com_Cfg.h` に定数化しています。

| エラーコード | 値 | 意味（要求番号） |
|---|---|---|
| `COM_E_PARAM` | 0x01 | API に不正なパラメータで呼ばれた（`[SWS_Com_00803]`） |
| `COM_E_UNINIT` | 0x02 | `Com_Init` 前 / `Com_Deinit` 後に呼ばれた（`[SWS_Com_00804]`） |
| `COM_E_PARAM_POINTER` | 0x03 | NULL ポインタチェック（`[SWS_Com_00805]`） |
| `COM_E_INIT_FAILED` | 0x04 | 不正なコンフィグセット選択（`[SWS_Com_00837]`） |

ApiId は各関数の Doxygen `\ServiceID` タグ（以前から記録されていたが実際の
エラー通知には反映されていなかった値）をそのまま定数化して使っています。
`Com_ConfigPtr == NULL || 他の条件` のように複数の異常系を 1 つの `if` で
まとめていた箇所（`Com_RxIndication`/`Com_ReceiveSignal`/`Com_SendSignal`/
`Com_TxConfirmation`）は、正しいエラーコードを区別して報告するために条件を
分割しました（`Com_TxConfirmation` の `result != E_OK` のような「異常では
ない早期 return」までエラー報告してしまわないよう、原因ごとに分けています）。

##### 対応範囲：全 BSW モジュールへの展開

当初は Com モジュールのみの対応でしたが、その後全 BSW モジュール
（Can, CanIf, PduR, CanTp, Dcm, Dem, FiM, NvM, EcuM, BswM, WdgM, WdgIf, Wdg,
ComM, CanSM, Nm, IoHwAb, Dio, Port, Adc, SecOC, E2E, E2EXf, Fee, MemIf の
25 モジュール）へ同じ方針で展開しました。DET_LOGE で既に報告されていた
箇所（NULL/範囲/未登録チェック）に加え、ログを一切出していなかった暗黙の
NULL/未初期化チェックにも同じ基準で追加しています。

**ModuleId の出典**: Com 以外のほとんどのモジュールは SWS 本文に
ModuleId の固定値が明記されていません（Com の `[SWS_Com_00442]` は
例外的な明記）。そのため `docs/AUTOSAR_TR_BSWModuleList.pdf`
（Release 4.3.1、「List of Basic Software Modules」表）から各モジュールの
ModuleId を検証・転記しています。この値は Com=50 という既知の値と
独立に一致したため、出典として信頼できることを確認済みです。各モジュールの
ModuleId は前述の「モジュール一覧」表を参照してください。うち EcuM（10）は
エラーコード値自体が SWS 標準で未固定（`[SWS_EcuM_04032]`）のため本実装で
独自に割り当てたもの、IoHwAb（254）はエラー分類自体が実装者定義
（`SWS_IoHwAb_91001`）であることに注意してください。

**対象外・チェック追加なしと判断したモジュール**:

- **E2E（`src/Bsw/E2E/E2E_P01.c`）**: `docs/AUTOSAR_SWS_E2ELibrary.pdf`
  `[SWS_E2E_00216]` が「E2E Library は DET/DEM/RTE を一切呼び出しては
  ならない」と明記しており、エラーは戻り値（`E2E_P01STATUS_*`）のみで
  呼び出し元（E2EXf）に伝達する設計です。Det_ReportError の追加は仕様
  違反になるため行っていません。
- **Dio / Port**: `Dio_ChannelType`/`Port_PinType` は本プロジェクトでは
  Arduino の生ピン番号をそのまま渡す簡略設計で、設定済みチャネル一覧との
  照合テーブルを持ちません。両モジュールともポインタ引数もないため、
  AUTOSAR が定義する `DIO_E_PARAM_INVALID_CHANNEL_ID` 等のエラーは
  本実装の設計上そもそも検出対象が存在せず、追加していません
  （ServiceID タグの誤りのみ修正）。

**非標準の独自拡張関数（DET 対象外）**: `CanIf_ControllerWakeup` /
`CanSM_ControllerWakeup` / `CanSM_RxIndication` / `WdgM_EnableHwWatchdog` /
`WdgM_DisableHwWatchdog` / `WdgM_ResumeSupervision` / `Dcm_ComIndication` /
`Adc_ReadChannel` はいずれも実際の AUTOSAR SWS には存在しない、本プロジェクト
独自の簡略化・拡張関数のため、DET のエラーコード・ApiId は個々の実 SWS 関数
とは対応付けていません（ServiceID は既存の非標準値をそのまま踏襲）。

**副次的に発見・修正した ServiceID タグの誤り**: 対応作業中、各モジュールの
実 SWS「Service ID[hex]」記載と実測照合したところ、20 件以上の Doxygen
`\ServiceID` タグの誤りを発見し、あわせて修正しました（例:
`PduR_Init` 0x00→0xF0、`BswM_EcuM_CurrentState`/`BswM_ComM_CurrentMode` が
互いに入れ替わっていた、`NvM_*` 系はほぼ全関数が誤り、等）。これらは
Det_ReportError の ApiId 引数に直接使う値のため、誤ったまま報告していると
診断ツール側で API を取り違える実害がありました。

##### 検証方法について

`Det_ReportError()` が呼ばれるのは NULL ポインタ・範囲外 ID・未初期化状態と
いった、正常な CAN 通信では到達しない開発時の異常系のみです。したがって
UDS/CAN フレーム経由で外部から誘発することはできず、`uds_tester` 等での
実機確認の対象にはなりません（各モジュールについて `pio run -e uno_r4` の
ビルド成功と、エラーコード・ModuleId の割り当てが対応する SWS PDF の原文と
一致していることの確認に留めています）。

<a id="e2e-p01"></a>
### E2E P01 保護（EngineInfo/AbsInfo 受信 / E2EHealthStatus 送信）

AUTOSAR E2E (End-to-End) Profile 01 による保護です。CAN バスの電気的エラーでは検出できない
**データ破壊・フレーム脱落・フレーム重複・誤ルーティング**を、CRC と送信カウンタの 2 種類の
保護要素で検出します。本プロジェクトでは 3 方向に適用しています。

> **統合方式（E2E Transformer）:** Com は E2E の存在を一切関知しません。AUTOSAR が定義する
> 3 通りの E2E 統合方式のうち「E2E Transformer」（`docs/AUTOSAR_SWS_E2ELibrary.pdf` 12.4 節、
> R4.2.1 以降）を模しており、CRC/Counter の検証・付与は Com の外側（`Rte` 層 +
> `src/Bsw/E2EXf/`）が担います。以前は Com 自身が I-PDU ごとに E2E ロジックを直接埋め込む
> 「COM E2E Callout」に近い設計でしたが、Com から BSW 層をまたいだ責務を切り離すために
> 移行しました（詳細は本セクション内の「Com モジュールとの統合」を参照）。
>
> **E2EXf 自身の初期化状態ガード（SWS_E2EXf_00130/00133/00151）:** E2EXf は下位の
> `E2E_P01CheckStateType`/`E2E_P01ProtectStateType`（フレームごとの Check/Protect 状態）
> とは別に、「E2EXf_Init() が呼ばれたか」というモジュール自身の初期化状態を
> `E2EXf.c` の静的フラグで保持します。`E2EXf_PBCfg_Init()`（`EcuM_Init()` から
> `Com_Init()` の直後に呼ばれる）が各 I-PDU の State を初期化した最後に
> `E2EXf_Init()` を呼んでこのフラグを立てます。`E2EXf_InverseTransform()`/
> `E2EXf_Transform()` はこのフラグが立つ前に呼ばれると安全側（E_NOT_OK／no-op）で
> 早期 return するため、将来 `EcuM_Init()` の呼び出し順序が変わって初期化前に
> フレーム受信経路が有効になっても、未初期化 State（ゼロクリアされた BSS のまま）
> を使って誤判定することがありません。

- **EngineInfo（CAN 0x100、受信）**: エンジン ECU から受信するフレームを`E2E_P01Check`で検証
  （本セクション前半）。EngineSpeed（回転数）は実車ではメータ表示だけでなく変速制御・
  トラクションコントロール・オーバーレブ保護等、複数の機能が参照しうる値のため、
  一般的なエンジン ECU の周期送信フレームを模して保護を付与しています
- **AbsInfo（CAN 0x110、受信）**: ABS ECU から受信するフレームを`E2E_P01Check`で検証（本セクション前半）
- **E2EHealthStatus（CAN 0x220、送信）**: 本 ECU（メータ ECU）が送信する、EngineInfo/AbsInfo
  受信側の E2E 検証エラー累積数を伝えるネットワーク健全性テレメトリに `E2E_P01Protect`で
  Counter・CRC8 を付加（本セクション後半の E2EMon サブセクション参照）。
  監視ツールがこのテレメトリ自体の破損を検出できるようにするためです

> MeterStatus（CAN 0x200、送信）・WarningStatus（CAN 0x210、送信）には E2E 保護を
> 付与していません。MeterStatus は EngineInfo/AbsInfo を Com が既に検証した**後**に
> メータ ECU 自身が導出する二次データ（エンジン状態の要約）、WarningStatus も同様に
> 警告灯の点灯状態という二次データであり、実車でも一次センサ値ほど厳密な保護が
> 付与されないことが多いため、素の（E2E 保護なしの）シグナル送信の実装例として
> 意図的に残しています。

<a id="ipdu-group"></a>
#### I-PDU Group（Com_IpduGroupStart/Stop、通信のライフサイクル制御）

これまで通信の有効/無効制御は、診断 `CommunicationControl`（UDS 0x28）が呼ぶ
`Com_SetCommunicationEnabled()`（全 I-PDU 一括の ON/OFF スイッチ）のみでした。
実 AUTOSAR の `Com_IpduGroupStart()`/`Com_IpduGroupStop()`（`Com_IPduConfigType`
の設計コメント・`Com.h` の `Com_SetCommunicationEnabled()` ドキュメントに、
「本来は I-PDU Group 単位だが、本プロジェクトには I-PDU Group という設定概念が
ないため簡略化している」と以前から明記されていたギャップです）を実装し、
**個別の I-PDU 単位**で起動/停止できるようにしました。

```
[SWS_Com_00444] 既定では全 I-PDU Group は停止状態
[SWS_Com_00840] どの I-PDU Group にも属さない I-PDU は Com_Init() で常に
                起動済み扱いになり、二度と停止できない
```

`Com_IPduConfigType.IpduGroupId`（既定値 `COM_IPDU_GROUP_NONE`）で所属を設定します。
本プロジェクトでは **E2EHealthStatus のみ**を「テレメトリ」I-PDU Group
（`COM_IPDU_GROUP_TELEMETRY`）に所属させ、他の全 I-PDU（EngineInfo/AbsInfo/
MeterStatus/WarningStatus/ImmobilizerCmd）はどの I-PDU Group にも属させていません
（＝常に有効、`Com_IpduGroupStart/Stop()` の影響を受けない）。E2EHealthStatus は
診断監視用のネットワーク健全性テレメトリで、車両の基本動作には不要な「非重要」
通信であるため、独立して停止できる対象として選びました。

<a id="ipdu-group-caller"></a>
#### 呼び出し元は BswM（実 AUTOSAR の標準構成）

`docs/AUTOSAR_SWS_Com.pdf` [7.3.5.1] は次のように述べています。

```
Once again, the COM module does not know or handle any grouping of I-PDUs...
it is expected that the complete state handling of I-PDU groups is done
outside of the AUTOSAR COM module, e.g. within the Basic Software Mode Manager.
```

つまり「どの I-PDU がどの Group に属するか」は Com の設定（`Com_PBCfg.c`）が持ち、
「いつ Group を起動/停止するか」は **BswM が呼ぶ**、というのが実 AUTOSAR の標準的な
役割分担です。実際、`docs/AUTOSAR_SWS_BSWModeManager.pdf` にも
`BswMPduGroupSwitch` という専用の ActionList 項目種別が定義されています。

```
[SWS_BswM_00273] When a BswMPduGroupSwitch action is executed, the BswM
shall call Com_IpduGroupStart for each BswMEnabledPduGroupRef, and call
Com_IpduGroupStop for each BswMDisabledPduGroupRef.
```

これに倣い、`BswM_ActionType` に `BSWM_ACTION_PDU_GROUP_START`/`_STOP` を追加し
（既存の `BSWM_ACTION_ACTIVATE`/`_DEACTIVATE`——Os タスクの有効/無効化——とは別の、
2 つ目のアクション種別として）、以下の 2 ルールを追加しました
（`src/Bsw/BswM/BswM_PBCfg.c`）。

| Rule | トリガ | アクション |
|---|---|---|
| Rule 3 | EcuM → RUN | `Com_IpduGroupStart(TELEMETRY, initialize=false)` |
| Rule 4 | EcuM → POST_RUN | `Com_IpduGroupStop(TELEMETRY)` |

既存の Rule 0（RUN→全タスク有効化）・Rule 1（POST_RUN→アプリタスク無効化）と
同じトリガ（同じ条件）で新しいルールを追加しただけなので、
既存ルールへの変更は一切ありません（`BswM_ExecuteRules()` は一致する全ルールを
実行するため、Rule 0 と Rule 3 は RUN 遷移のたびに両方発火します）。

> **その後の変更**: Nm（CanNm 状態機械）導入に伴い、Rule 3 は単一条件
> （EcuM==RUN）から複合条件（EcuM==RUN `AND` ComM==FULL_COMMUNICATION）へ、
> Rule 4 と対になる停止条件として Rule 5（ComM==SILENT_COMMUNICATION `OR`
> ComM==NO_COMMUNICATION）が追加されています。詳細は前述の「BswM（BSW
> モードマネージャ）」セクションのルールテーブルを参照してください。

<a id="ipdu-group-behavior"></a>
#### Com_IpduGroupStart/Stop が実際に行うこと

- **Start**（[SWS_Com_00787]）: RX は受信デッドライン監視タイマを再始動
  （最終受信時刻を現在時刻へリセット）。TX は MDT/周期タイマの基準時刻を
  再始動し、update-bit をクリアし、現在のデータ内容から TMS を再評価する
  （[SWS_Com_00223]）。`initialize=true` の場合は追加で I-PDU バッファ・
  Signal Group のシャドウバッファをゼロ初期化する（[SWS_Com_00222]）。
- **Stop**（[SWS_Com_00684]/[SWS_Com_00685]）: RX は受信処理・デッドライン
  監視の両方を無効化する（`Com_RxIndication()` がフレームを受信してもバッファ
  を更新せずに棄却する）。TX は保留中の送信要求をキャンセルする
  （[SWS_Com_00777]）。停止中の TX 確認（`Com_TxConfirmation()`）も無視する
  （[SWS_Com_00800]）。
- **`Com_SendSignal()`/`Com_ReceiveSignal()` 自体は停止中でも内部バッファを
  更新・参照できる**（[SWS_Com_00334]）。「値をセットする」ことと「実際に
  送受信するタイミング」は独立した責務であり、E2EMon は E2EHealthStatus が
  現在起動中か停止中かを一切意識せず `Com_SendSignal()` を呼び続けます。

既存の `Com_SetCommunicationEnabled()`（診断 CommunicationControl 用）とは
**独立した、直交する抑制機構**です。実際に送受信処理が行われるのは両方が
有効な場合のみ（AND 条件）で、意図的に統合していません（本プロジェクトの
UDS 0x28 実装は「全 I-PDU 一括」のままの方が既存のテストが安全に保たれるため）。

<a id="ipdu-group-verification"></a>
#### 動作確認方法

実機ログで、EcuM が RUN → POST_RUN → SHUTDOWN → （ウェイクアップ）→ RUN と
遷移する様子を観察すると、以下が確認できます。

```
[1159ms]  INFO  EcuM: ->RUN
[1162ms]  INFO  BswM: Rule0 fired src=0 val=0x10 act=0 mask=0xFFFF
[1163ms]  INFO  BswM: Rule3 fired src=0 val=0x10 act=2 mask=0x000
[1164ms]  INFO  Com: IpduGroupStart grp=0 iPdu=2(TX) init=0
...
[7138ms]  INFO  Com: TX iPdu=2 [E6 00 00 00]        # テレメトリ、通常どおり送信される

# ボランタリスリープで POST_RUN へ入った直後
[16395ms] INFO  EcuM: ->POST_RUN timeout=5000ms
[16396ms] INFO  BswM: Rule1 fired src=0 val=0x20 act=1 mask=0x00C
[16397ms] INFO  BswM: Rule4 fired src=0 val=0x20 act=3 mask=0x000
[16398ms] INFO  Com: IpduGroupStop grp=0 iPdu=2(TX)
# 以降、EcuM: ->SHUTDOWN まで "Com: TX iPdu=2" が一切出力されなくなる
# （EngineInfo の受信・MeterStatus 等の送信は Rule1 の対象外なので継続する）

# CAN バスのウェイクアップで RUN へ復帰
[80647ms] INFO  EcuM: SHUTDOWN ->RUN (wakeup) user=0
[80648ms] INFO  BswM: Rule0 fired src=0 val=0x10 act=0 mask=0xFFFF
[80649ms] INFO  BswM: Rule3 fired src=0 val=0x10 act=2 mask=0x000
[80650ms] INFO  Com: IpduGroupStart grp=0 iPdu=2(TX) init=0
# 以降、E2EHealthStatus の PERIODIC 送信が再開する
```

（ログの正確なタイムスタンプ・`act=`/`mask=` の値は実機で確認してください。
`act=2`=`BSWM_ACTION_PDU_GROUP_START`、`act=3`=`BSWM_ACTION_PDU_GROUP_STOP`、
`mask=0x000` は PDU_GROUP 系アクションでは `TaskMask` を使わないため無意味な値です。）

<a id="e2e-fault-model"></a>
#### E2E が保護する故障モデル

| 故障モデル | 検出方法 | 対応 E2E フィールド |
|-----------|---------|------------------|
| データ破壊（ビット化け等） | 受信 CRC ≠ 計算 CRC | byte[0] CRC8 |
| フレーム脱落 | カウンタが 2 以上飛ぶ | byte[1] 下位 4bit Counter |
| フレーム重複 | カウンタが前回と同じ | byte[1] 下位 4bit Counter |
| 誤ルーティング（他 ECU のフレームが混入） | DataID が違うため CRC が一致しない | DataID（EngineInfo=0x0100 / AbsInfo=0x0110）を CRC 計算に含む |

<a id="e2e-check-rx"></a>
#### 受信側（Check）— EngineInfo / AbsInfo

E2E チェックの仕組み自体は両フレームで完全に共通（`E2E_P01Check` の同一実装を
設定テーブルだけ変えて使い回す）のため、以下では区別せず一つの仕組みとして説明し、
フレームレイアウトと設定値のみ個別に示します。

##### フレームレイアウト

**EngineInfo（CAN 0x100）:**
```
byte[0]   : CRC8 SAE J1850（多項式 0x1D、初期値 0x00、最終 XOR 0x00、SWS_E2E_00083 準拠）
            計算対象: DataID_low(0x00), DataID_high(0x01), byte[1], byte[2], byte[3], byte[4], byte[5]
            （CRC バイト自身を除く全バイト。CRC バイトより前の区間は 0 バイト）
byte[1]   : 上位 4bit=未使用、下位 4bit=Counter（0→1→…→14→0 のリングカウンタ、15 はスキップ）
byte[2-5] : シグナルデータ（EngineSpeed / CoolantTemp / EngineOnFlag）
```

**AbsInfo（CAN 0x110）:**
```
byte[0]   : CRC8 SAE J1850（多項式 0x1D、初期値 0x00、最終 XOR 0x00、SWS_E2E_00083 準拠）
            計算対象: DataID_low(0x10), DataID_high(0x01), byte[1], byte[2], byte[3], byte[4]
            （CRC バイト自身を除く全バイト。CRC バイトより前の区間は 0 バイト）
byte[1]   : 上位 4bit=未使用、下位 4bit=Counter（0→1→…→14→0 のリングカウンタ、15 はスキップ）
byte[2-4] : シグナルデータ（VehicleSpeed / BrakeActive / AbsActive）
```

CRC を先頭バイト・Counter をそれに続く 1 バイトの下位 4bit に配置するこのレイアウトは、
AUTOSAR 標準バリアント 1A（SWS_E2E_00227）にそのまま準拠している
（詳細は [`docs/E2E_Profile1_Notes.md`](docs/E2E_Profile1_Notes.md) 参照）。

##### カウンタデルタ判定と 8 状態フル state machine（`E2E_P01Check`）

受信カウンタと前回有効カウンタの差分 `delta` を軸に、AUTOSAR `E2E_P01CheckStatusType`
（SWS_E2E_00022）準拠の 8 状態で判定します。Profile 1 のカウンタは 0〜14 の 15 値を
循環する（15=0xF は予約値でスキップ、SWS_E2E_00075）ため、折り返しの補正は
`received >= lastValid ? received - lastValid : 15 + received - lastValid`
という mod-15 の式（E2ELibrary Figure 7-7）で計算します。
例: received=0, lastValid=14（カウンタが 0 に折り返した直後）→ `15 + 0 - 14 = 1`（正常）。
（`& 0x0F` によるビットマスク＝mod-16 の補正は Profile 2 側の式であり、Profile 1 に
適用すると折り返しのたびに delta を 1 大きく誤算出してしまうため使用しない）。

```
CRC 不一致                          → WRONGCRC（Counter 側は判定しない）
初回受信                            → INITIAL（カウンタ基準を確立）
delta == 0                          → REPEATED（フレーム重複）
delta > MaxDeltaCounter             → WRONGSEQUENCE（許容超過。再ロック開始、SyncCounter = SyncCounterInit）
0 < delta <= MaxDeltaCounter かつ
  SyncCounter > 0（再ロック中）      → SYNC（CRC/Counter は正常範囲内だが継続性未確定。SyncCounter--）
delta == 1 かつ SyncCounter == 0    → OK（正常）
1 < delta <= MaxDeltaCounter かつ
  SyncCounter == 0                  → OKSOMELOST（正常だが一部消失、許容範囲内）
```

**SyncCounter による再ロック機構**（`E2E_P01CheckStateType.SyncCounter`）:
一度 WRONGSEQUENCE（カウンタ飛びが許容超過）を検知すると、すぐに「正常」へ復帰するのではなく、
`SyncCounterInit` 回分（EngineInfo/AbsInfo とも 2 回）連続して正常範囲内のカウンタを受信するまで
状態を SYNC として報告し続けます。「異常を検知したら即座に信用を回復しない」という
安全側の設計です。SYNC 中の各フレーム自体は CRC・カウンタとも正常範囲内なので、
データそのものは使用可能と判断し Com バッファは更新します（後述）。

※ 公式仕様にはこのほか `NONEWDATA`（前回呼び出し以降、新規データなし）がありますが、
本実装は `Com_RxIndication()` からフレーム受信時にのみ `E2E_P01Check` を呼び出す設計のため、
「呼ばれたが新規データがない」状況が発生せず、この状態は実装していません
（詳細は [`docs/E2E_Profile1_Notes.md`](docs/E2E_Profile1_Notes.md) 参照）。

##### Com モジュールとの統合（E2E Transformer 方式）

Com は EngineInfo/AbsInfo のペイロード内容を一切検証しません。`Com_RxIndication()` は
受信の都度、CRC/Counter の妥当性にかかわらず**無条件に** RX バッファ・タイムアウトタイマを
更新した上で、`Com_IPduConfigType.RxIndicationCbk`（I-PDU ごとに `Com_PBCfg.c` で設定する
汎用フック、Com 本体は中身を関知しない）を呼び出すだけです。

実際の検証は `Rte` 層に置かれたグルー関数（`Rte_COMCbk_EngineInfo()` /
`Rte_COMCbk_AbsInfo()`、`src/Rte/Rte.c`）が担い、`Com_ReceiveSignalGroupArray()` で
I-PDU の生バイト列を取得した上で `E2EXf_InverseTransform()`（`src/Bsw/E2EXf/E2EXf.c`、
中身は `E2E_P01Check()` への薄いラッパー）へ渡します。検証に合格した場合のみ、Rte 内部の
ミラー変数（`Rte_EngineInfoMirror` / `Rte_AbsInfoMirror`）へ最新値を反映します。
検証に失敗した場合はミラーを更新せず、直前の正常値がシグナルとして残り続けます
（＝これが E2E 違反時のフェイルセーフの実体）。

```
Com_RxIndication() (RxIndicationCbk が設定された I-PDU。現状 IPduId=0/1 が対象):
  RX バッファ・タイムアウトタイマを無条件に更新（Com は E2E を関知しない）
  RxIndicationCbk() を呼び出す
    = Rte_COMCbk_EngineInfo() / Rte_COMCbk_AbsInfo() （Rte.c）
        Com_ReceiveSignalGroupArray() で生バイト列を取得
        E2EXf_InverseTransform() を呼び出す（CheckStatus 出力引数で生の8状態も受け取る）
          → E2E_P01Check() を実行
            OK / OKSOMELOST / SYNC / INITIAL
                      → Dem_ReportErrorStatus(DemEventId, PASSED)
                        E_OK を返す → Rte ミラーを更新
            REPEATED / WRONGCRC / WRONGSEQUENCE / ERROR
                      → DET_LOGW(TAG="E2EXf", "InverseTransform NG DemEvent=%u st=%u")
                        Dem_ReportErrorStatus(DemEventId, FAILED)
                        E_NOT_OK を返す → Rte ミラー非更新（前回値を維持）
        CheckStatus を Rte_MapE2EStatus() で Rte_IStatusType へ写像し
        Rte_EngineInfoStatus / Rte_AbsInfoStatus（静的変数）へ保存
          → 次回以降の Rte_Read_*() 呼び出しがこれを返す（詳細は次項）
```

OK/OKSOMELOST/SYNC/INITIAL はいずれも CRC が正しく検証されているため「データとしては信頼できる」
と判断し受理します。SYNC は再ロック中でシーケンスの継続性こそ未確定ですが、個々のフレームの
CRC・カウンタ自体は正常範囲内のため、ミラーは更新します。
E2E エラー（REPEATED/WRONGCRC/WRONGSEQUENCE/ERROR）時はミラーを更新しないため、
直前の正常値がシグナルとして残り続けます。一方、Com 側のタイムアウトタイマは
（Com が E2E を関知しないため）フレーム到達だけでリセットされ続けます。したがって
E2E 違反が続く間の古い値保持は「E2E フェイルセーフ」（Rte ミラー非更新）が担い、
物理的にフレームそのものが途絶えた場合は別軸の「Com 受信デッドライン監視」
（`Com_IsRxTimedOut()` → `E_NOT_OK`）がフェイルセーフを担う、という 2 段構えです。

> なぜフレーム受信の都度チェックするのか: E2E Counter によるシーケンス追跡は、
> 物理フレームが届くたびに 1 回検証しないと delta 計算の基準がずれる。ASW の Runnable が
> 読みに来るタイミング（本プロジェクトでは 3000ms 周期）までチェックを遅延させる設計には
> できない。そのため RxIndicationCbk はアプリ層の読み出し頻度とは独立に、
> 物理フレーム到着のたびに呼ばれる。

##### E2E 検証ステータスの Rte 経由での公開（`Rte_IStatusType`）

上記の通り、E2E 違反時は Rte ミラーを更新しないことで「直前の正常値を使い続ける」
というフェイルセーフを実現していますが、これだけでは **SWC（ASW）自身は E2E 違反が
起きたことを一切知る手段がありません**。`Rte_Read_SpeedSensor_EngineSpeed()` 等の
戻り値は、以前は `Com_IsRxTimedOut()` によるタイムアウト判定のみを反映する
`Std_ReturnType`（E_OK/E_NOT_OK の二値）でした。

実際の AUTOSAR では、データ変換（Transformer チェーン）を持つポートの
`Rte_Read`/`Rte_Receive` は `Std_ReturnType` ではなく `Rte_IStatusType` を返し、
`RTE_E_HARD_TRANSFORMER_ERROR`（チェーン中のいずれかが致命的エラー）・
`RTE_E_SOFT_TRANSFORMER_ERROR`（致命的ではないが軽微なエラー）・`RTE_E_COM_STOPPED`
等を区別できるようになっています（`[SWS_Rte_08576]`/`[SWS_Rte_08577]`/
`[SWS_Rte_01106]` 等。複数要因が「同一呼び出しで同時に新規発生した」場合の
優先順位は `[SWS_Rte_08594]` で `RTE_E_HARD_TRANSFORMER_ERROR` >
`RTE_E_COM_STOPPED` > `RTE_E_SOFT_TRANSFORMER_ERROR` の順と定義されています）。
本プロジェクトもこれを簡略化した `Rte_IStatusType`（`src/Rte/Rte_Type.h`）として
導入し、EngineInfo/AbsInfo 由来の 6 つの Read ポート
（`Rte_Read_SpeedSensor_EngineSpeed` 等）がこれを返すようにしています。

```
E2E_P01Check() の8状態          Rte_MapE2EStatus() による分類    Rte_IStatusType
OK / INITIAL / SYNC        ─────────────────────────────→  RTE_E_OK
OKSOMELOST                 ─────────────────────────────→  RTE_E_SOFT_TRANSFORMER_ERROR
REPEATED / WRONGCRC /
WRONGSEQUENCE / ERROR      ─────────────────────────────→  RTE_E_HARD_TRANSFORMER_ERROR

Rte_Read_SpeedSensor_EngineSpeed() 等（Rte.c）:
  *data は常に Rte_EngineInfoMirror の現在値を書き込む（戻り値に関わらず）
    ← 本実装の意図的な簡略化。実 AUTOSAR は HARD_TRANSFORMER_ERROR 時に
       出力引数を更新しないと定めるが（[SWS_Rte_08576] 等）、本プロジェクトは
       「E2E フェイルセーフ = 前回の正常値を使い続ける」という既存の設計方針
       （E2EXf セクション冒頭）を優先し、あえてこの点だけ逸脱している
  戻り値の合成（[SWS_Rte_08594] の優先順位をそのまま適用しない。下記注記参照）:
    Com_IsRxTimedOut(0) ? → RTE_E_COM_STOPPED
    それ以外              → Rte_EngineInfoStatus（OK / SOFT / HARD のいずれか）
```

> **なぜ `[SWS_Rte_08594]` の優先順位（HARD > COM_STOPPED）をそのまま
> 適用しないか**: `Rte_EngineInfoStatus` は「最後にフレームを受信した瞬間」の
> E2E チェック結果を保持するラッチであり、`Com_IsRxTimedOut()` は Com が
> 周期的に評価する「今まさに生きているか」という独立した軸です。実 AUTOSAR
> の優先順位規定は「同一呼び出しで複数要因が同時に新規発生した」場面を
> 想定していますが、本実装のようにラッチを先に見てしまうと、E2E ハード
> エラーを起こしたフレームを最後に通信が本当に途絶えた場合（配線断は
> E2E 異常と通信途絶を同時に招きやすい）、ラッチされた
> `RTE_E_HARD_TRANSFORMER_ERROR` が `Com_IsRxTimedOut()` の変化後も
> 永久に優先され続け、`DEM_EVENT_COMM_TIMEOUT` の FAULT 遷移が二度と
> 起きなくなってしまいます（コードレビューで発見・修正済み）。そのため
> 常に「現在も継続する物理層の状態」である `Com_IsRxTimedOut()` を先に
> 判定します。

**呼び出し元（SWC）はこの情報を無視してもよい**: 例えば `App_WarningIndicator_Run()`
は `(void)Rte_Read_AbsSensor_AbsActive(&absActive);` のように戻り値を捨てており、
これは実 AUTOSAR でも許される正当な使い方です（データ変換ポートであっても、
呼び出し側がステータスを見ずにベストエフォートの値だけを使う設計は珍しくありません）。
一方 `App_EngineManager_Run()` は `RTE_E_COM_STOPPED` のみを見て
`DEM_EVENT_COMM_TIMEOUT` の FAULT 遷移を判定し（E2E エラーは既に
`DEM_EVENT_E2E_ENGINEINFO`/`_ABSINFO` として別途 Dem に報告されるため、ここで
二重に COMM_TIMEOUT として扱わない）、`RTE_E_HARD_TRANSFORMER_ERROR` は状態遷移に
影響させず観測用の WARN ログのみ出力します。SWC が受け取った詳細ステータスを
実際にどう使うか（ログに留めるか、独自のフェールオペレーショナル判断に使うか）は
呼び出し元の設計次第であることを示す例です。

##### E2E モジュール設定（`src/Bsw/E2EXf/E2EXf_PBCfg.c` の E2E 設定テーブル）

E2E P01 の設定・状態実体は Com から独立し、`E2EXf_PBCfg.c` で保持しています
（`E2EXf_EngineInfoRxCfg` / `E2EXf_AbsInfoRxCfg` / `E2EXf_E2EHealthStatusTxCfg` として
`E2EXf_RxConfigType`/`E2EXf_TxConfigType` にまとめ、`Rte.c` のグルー関数から参照）。

**EngineInfo（`E2EXf_EngineInfoRxCfg`）:**

| 設定 | 値 | 意味 |
|------|----|------|
| DataID | 0x0100 | CRC 計算に含む ID（誤ルーティング検出） |
| DataLength | 6 | フレーム全体のバイト長 |
| MaxDeltaCounter | 1 | 許容する最大カウンタ飛び量 |
| CounterOffset | 1 | Counter を格納する byte インデックス |
| CRCOffset | 0 | CRC を格納する byte インデックス（AUTOSAR 標準バリアント 1A） |
| SyncCounterInit | 2 | WRONGSEQUENCE 検知後、OK へ戻るまでに必要な連続正常受信回数 |

**AbsInfo（`E2EXf_AbsInfoRxCfg`）:**

| 設定 | 値 | 意味 |
|------|----|------|
| DataID | 0x0110 | CRC 計算に含む ID（誤ルーティング検出） |
| DataLength | 5 | フレーム全体のバイト長 |
| MaxDeltaCounter | 1 | 許容する最大カウンタ飛び量 |
| CounterOffset | 1 | Counter を格納する byte インデックス |
| CRCOffset | 0 | CRC を格納する byte インデックス（AUTOSAR 標準バリアント 1A） |
| SyncCounterInit | 2 | WRONGSEQUENCE 検知後、OK へ戻るまでに必要な連続正常受信回数 |

##### ログ例

**正常受信時（初回は INITIAL、以降は OK）:**
```
（E2E 正常時はログなし — バッファが静かに更新される）
```

**CRC 不一致発生時（AbsInfo）:**
```
[7001ms] WARN  E2EXf: InverseTransform NG DemEvent=8 st=2  ← st=2: WRONGCRC（CRC 不一致）
[7002ms] DEBUG Dem: ev=8 debounce=1 (PREFAILED)  ← limit=1 のため次回確定
[7003ms] WARN  Dem: FAILED ev=8 dtc=0x000109     ← 即座に確定・EEPROM に保存
```

**CRC 不一致発生時（EngineInfo）:**
```
[8001ms] WARN  E2EXf: InverseTransform NG DemEvent=9 st=2  ← st=2: WRONGCRC（CRC 不一致）
[8002ms] DEBUG Dem: ev=9 debounce=1 (PREFAILED)  ← limit=1 のため次回確定
[8003ms] WARN  Dem: FAILED ev=9 dtc=0x00010A     ← 即座に確定・EEPROM に保存
```

**カウンタ飛び超過 → SYNC 再ロック → OK 復帰の一連の流れ（実機ログ、uds_tester で意図的にカウンタを飛ばして送信）:**

`E2E_P01.c`の`SyncCounter > 0`分岐に`DET_LOGW(TAG, "st=%u sync=%u", ...)`を追加することで、
ログレベルを変更せずに常時 SyncCounter の遷移を観測できるようにしている
（`E2E_P01STATUS_OK`は意図的に無ログのままなので、ログが出ないこと自体が「静かに OK へ復帰した」証拠になる）。

```
[30824ms] WARN  E2EXf: InverseTransform NG DemEvent=8 st=64  ← WRONGSEQUENCE（カウンタ飛び検知、SyncCounter=2 セット）
[30827ms] WARN  Dem: FAILED ev=8 dtc=0x000109      ← このフレームは不採用（ミラー非更新）
[33038ms] WARN  E2E_P01: st=3 sync=1       ← 1回目の正常カウンタ受信、再ロック中（SyncCounter 2→1）
[34205ms] WARN  E2E_P01: st=3 sync=0       ← 2回目の正常カウンタ受信、再ロック完了直前（SyncCounter 1→0）
                                            ← 3回目の正常カウンタ受信は無ログ = OK に復帰
```

**動作確認方法（意図的な E2E エラー）:**

uds_tester ツールの EngineInfo/AbsInfo データ入力欄で byte[0]（CRC バイト）を誤った値に
書き換えてから送信ボタンを押すと、Rte 層の E2E Transformer フックが CRC エラーを検出して
それぞれ DEM_EVENT_E2E_ENGINEINFO / DEM_EVENT_E2E_ABSINFO が報告されます。

**動作確認方法（WRONGSEQUENCE → SYNC 再ロック → OK 復帰）:**

uds_tester は送信するたびに Counter を自動で +1 するため、通常操作では delta は常に 1 に
なり WRONGSEQUENCE は発生しません。意図的にカウンタを飛ばすには、データ入力欄の byte[1]
下位 4bit（Counter）を手動で前回送信値+2 以上の値へ書き換えてから送信します
（CRC は送信直前に自動再計算されるため、Counter だけを書き換えれば十分です）。
これにより WRONGSEQUENCE → （以降 2 回連続正常送信で）SYNC × 2 回 → OK という
一連の遷移を実機ログで確認できます（EngineInfo/AbsInfo いずれも同じ手順）。

<a id="e2e-protect-tx"></a>
#### 送信側（Protect）— E2EHealthStatus

E2E 保護の対象は、実際にはエンジン状態フレーム（MeterStatus）ではなく、
`E2EMon`（後述の独立した CDD 相当モジュール）が発行するネットワーク健全性
テレメトリ `E2EHealthStatus`（CAN 0x220）です。監視ツールが「E2E エラー累積数」
自体の破損を検出できるようにする狙いで、MeterStatus ではなくこちらへ E2E 保護を
適用しています（MeterStatus は E2E 保護なしの単純な直接送信に単純化しています）。

##### フレームレイアウト

```
byte[0]   : CRC8 SAE J1850（多項式 0x1D、初期値 0x00、最終 XOR 0x00、SWS_E2E_00083 準拠）
            計算対象: DataID_low(0x00), DataID_high(0x20), byte[1], byte[2], byte[3]
            （CRC バイト自身を除く全バイト。CRC バイトより前の区間は 0 バイト）
byte[1]   : 上位 4bit=未使用、下位 4bit=Counter（0→1→…→14→0 のリングカウンタ、15 はスキップ）
byte[2]   : シグナルデータ（E2ECrcErrCount）
byte[3]   : シグナルデータ（E2ESeqErrCount）
```

AbsInfo（受信）と同じ「CRC→Counter→データ」の並びを踏襲しています
（AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠）。

##### エンコード処理（`E2E_P01Protect`）

`E2E_P01Check`（受信検証）と対になる、送信側のエンコード処理です。検証すべき前回値がないため、
状態は次に送信する Counter 値だけを保持します。

```
E2E_P01Protect() 呼び出しごと:
  1. Data[CounterOffset] = 現在の Counter（下位 4bit）を書き込む
  2. Counter = (Counter >= 14) ? 0 : Counter + 1   ← 次回送信用に進める（15 はスキップ、SWS_E2E_00075）
  3. DataID_low, DataID_high,
     Data[0..CRCOffset-1]（CRC より前、CRC が先頭なら 0 バイト）,
     Data[CRCOffset+1..DataLength-1]（CRC より後）
     から CRC8 を計算し Data[CRCOffset] へ書き込む
```

Counter は `E2E_P01ProtectInit()` で 0 に初期化されるため、起動後最初に送信される
E2EHealthStatus フレームは Counter=0 です。

##### Com モジュールとの統合（E2E Transformer 方式）

Com は E2EHealthStatus のペイロードにも一切関知しません。E2EHealthStatus は
`COM_TX_MODE_PERIODIC` のため、`Com_MainFunction()` が自分の周期タイマで
送信を決定した際に `Com_IPduConfigType.TxTransformCbk`（`Com_PBCfg.c` で
`Rte_COMTransform_E2EHealthStatus` を設定）を実 TX バッファへのポインタと
長さ付きで呼び出すだけです（DIRECT/MIXED I-PDU の変化時送信も同じ
`Com_MainFunction()` から呼ばれるため、「送信直前の最終変換」の仕組みを
そのまま再利用しています）。実際に Counter・
CRC8 を書き込むのは `Rte_COMTransform_E2EHealthStatus()`（`src/Rte/Rte.c`）で、
中身は `E2EXf_Transform()`（`E2E_P01Protect()` への薄いラッパー）を呼ぶだけです。
AbsInfo の Check とは逆に、失敗や再送は発生しません（送信側なので検証すべき
前提がないため）。E2EMon（データの生産者）はこの E2E 保護の存在を一切知りません。

```
Com_MainFunction()（PERIODIC モードの I-PDU。現状 IPduId=2 が対象）:
  TxTransformCbk(Com_TxBuffer[PduId], DLC) を呼び出す
    = Rte_COMTransform_E2EHealthStatus() （Rte.c）
        E2EXf_Transform() を呼び出す
          → E2E_P01Protect() を実行
            Counter を書き込み +1、CRC8 を計算して書き込む
  PduR_Transmit() で送信
```

##### E2E モジュール設定（`src/Bsw/E2EXf/E2EXf_PBCfg.c` の `E2EXf_E2EHealthStatusTxCfg`）

| 設定 | 値 | 意味 |
|------|----|------|
| DataID | 0x0220 | CRC 計算に含む ID（CAN ID と一致） |
| DataLength | 4 | フレーム全体のバイト長 |
| MaxDeltaCounter | 0 | Protect 側では未使用 |
| CounterOffset | 1 | Counter を格納する byte インデックス |
| CRCOffset | 0 | CRC を格納する byte インデックス（AUTOSAR 標準バリアント 1A） |

##### ログ例

```
[1019ms] INFO  Can_Hw: TX OK id=0x220 dlc=4 [XX 00 00 00]
                                              └┘  └┘ └┘ └┘ └── E2ESeqErrCount=0
                                              │   │  └──────── E2ECrcErrCount=0
                                              │   └─────────── Counter=0（初回送信）
                                              └─────────────── CRC8（自動計算）
[2019ms] INFO  Can_Hw: TX OK id=0x220 dlc=4 [YY 01 00 00]  ← Counter が 1 に進む
```

<a id="e2emon"></a>
#### E2EMon（ネットワーク健全性モニタ、独自 CDD 相当）

`E2EXf_InverseTransform()`（本セクション前半）が検出した E2E エラーは、
これまで Dem への DTC 報告にしか使われていませんでした。これとは別に、
EngineInfo/AbsInfo 受信の E2E 検証結果を集計し、CAN バス上へブロードキャストする
「ネットワーク健全性テレメトリ」を独立したモジュール `E2EMon`（`src/Bsw/E2EMon/`）
として実装しています。

**なぜ E2EXf や Rte に直接手を入れないのか**: 実際の AUTOSAR 開発では、RTE・
E2E Transformer（E2EXf）はいずれも ARXML 設定からコード生成ツールが生成する
成果物であり、生成後のファイルを手で書き換えても次回生成で上書きされます
（E2E の検証・保護アルゴリズム自体も、Vector 等が提供する認証済みライブラリで
あることが多く、ソース改変は現実的ではありません）。COM スタックに独自要件を
足したい場合、実務では標準モジュール自体を無改造のまま、**独自の CDD
（Complex Device Driver）を新規に書き、標準モジュールが持つ設定可能な通知フック
経由で配線する**のが一般的なパターンです。本プロジェクトにはコード生成ツールが
無いため Rte.c/E2EXf.c は「生成コードの手書き代用品」ですが、E2EMon はその代用品
すら改変せず、独立した CDD 相当のモジュールとして追加しています。

```
Rte_COMCbk_EngineInfo()/AbsInfo()（Rte.c、E2EXf_InverseTransform() 呼び出し直後）:
  E2EMon_NotifyCheckResult(checkStatus) を呼ぶ
    ← 実 AUTOSAR で言う「ARXML で設定した OnDataReceived 通知フックが RTE から
       生成され、独自 CDD の関数を呼ぶ」という接続方式を模したもの

E2EMon_NotifyCheckResult()（E2EMon.c）:
  status == WRONGCRC ?
    YES → E2EMon_CrcErrorCount++（0xFF で飽和）
  status == WRONGSEQUENCE または REPEATED ?
    YES → E2EMon_SequenceErrorCount++（0xFF で飽和）
  Com_SendSignal(COM_SIGNAL_E2E_CRC_ERR_COUNT, &E2EMon_CrcErrorCount)
  Com_SendSignal(COM_SIGNAL_E2E_SEQ_ERR_COUNT, &E2EMon_SequenceErrorCount)
    ← 値をセットするだけで、送信タイミングには一切関与しない
```

**送信スケジューリングは Com 自身の責務（PERIODIC 送信モード）**: 当初は
MeterStatus フレームに相乗りさせる案も検討しましたが、実務では「オペレーショナルな
ステータスフレームに診断テレメトリを混ぜない」「周期送信のスケジューリングは
CDD 自身ではなく Com モジュールの責務」という判断が一般的なため、専用の新規
I-PDU（`E2EHealthStatus`、CAN ID 0x220）として分離しました。Com には新たに
`ComTxModeMode` 相当の `TxModeMode` 設定（`COM_TX_MODE_DIRECT` / `_MIXED` / `_PERIODIC`）
を追加し、`E2EHealthStatus` は `COM_TX_MODE_PERIODIC` として `Com_MainFunction()`
が自分の周期タイマ（既定 6000ms、`COM_TX_PERIOD_E2EHEALTH_MS`）で自動送信します。
E2EMon は `Com_SendSignal()` を呼ぶだけで、送信タイミングには一切関与しません
（詳細は「CAN 通信スタック」セクションの Com モジュール説明を参照）。
テレメトリ自体の破損を監視ツールが検出できるよう、この `E2EHealthStatus` には
E2E P01 保護を付与しています（詳細は前項「送信側（Protect）— E2EHealthStatus」参照。
E2EMon 自身は E2E 保護の存在を一切知りません）。

**Dem の ExtendedData（故障確定回数）との違い**: Dem の ExtendedData
（UDS SID 0x19/06 で読み出せる、DTC ごとの確定 FAILED 回数）は NvM 永続化される
「デバウンス確定後」の累積回数です。一方この E2E エラーカウンタは、デバウンスを
介さない生の検証結果（1 フレームごとの WRONGCRC/WRONGSEQUENCE/REPEATED）を
そのまま数えており、UDS でポーリングせずとも CAN バス上の他 ECU・監視ツールが
`E2EHealthStatus` を受信するだけでリアルタイムに観測できる、という違いがあります。

##### E2EHealthStatus フレームレイアウト（CAN ID 0x220 / DLC=4 / PERIODIC / E2E P01 保護）

```
byte[0] : E2E CRC8（AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠）
byte[1] : E2E Counter（下位 4bit）
byte[2] : E2ECrcErrCount（EngineInfo/AbsInfo 受信の E2E CRC 不一致累積数、0-255 で飽和）
byte[3] : E2ESeqErrCount（EngineInfo/AbsInfo 受信の E2E シーケンス異常累積数、0-255 で飽和）
```

**動作確認方法**: uds_tester で EngineInfo/AbsInfo の byte[0]（CRC）を意図的に
誤った値にして送信すると、`E2EHealthStatus` の byte[2]（crcErr）が実機ログ・
uds_tester の受信モニター双方で 1 ずつ増えることが確認できます（uds_tester の
「E2EHealthStatus (0x220)」受信モニターは `crcErr=N seqErr=M` の形式で表示します）。
カウンタ飛びを起こすと byte[3]（seqErr）が増えます。6000ms 周期で自動送信される
ため、値が変化していなくても定期的にフレームが流れ続けることも確認できます
（1000ms だとシリアルログの出力量が多く流れてしまうため 6000ms を既定値としています）。

```
[1019ms] INFO  Com: TX iPdu=2 [XX 00 00 00]  # E2EHealthStatus、6000ms 周期で自動送信
[7019ms] INFO  Com: TX iPdu=2 [YY 01 00 00]  # Counter が 1 に進む

# EngineInfo の CRC を意図的に誤らせて送信した直後
[8501ms] WARN  E2EXf: InverseTransform NG DemEvent=9 st=2  ← st=2: WRONGCRC
[8502ms] INFO  E2EMon: (内部カウンタ更新、次回 PERIODIC 送信まではログなし)
[13019ms] INFO  Com: TX iPdu=2 [ZZ 02 01 00]  # crcErr が 0→1 に増加
```

<a id="secoc"></a>
### SecOC（Secure Onboard Communication、メッセージ認証）

E2E（上記）は CRC・カウンタによる「意図しない通信エラー」の検出が目的で、
アルゴリズム自体が公開されており秘密鍵を使わないため、悪意ある攻撃者が正しい
CRC/カウンタを計算して偽のフレームを送ること自体は防げません。**SecOC** は
これとは別の軸として、秘密鍵ベースの MAC（Message Authentication Code）と
フレッシュネス値（リプレイ攻撃対策）で「意図的な改ざん・なりすまし」を検出する
AUTOSAR モジュールです。ユーザーが実際に AUTOSAR CP R4.3.1 の SWS/SRS 仕様書
PDF（`docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf`）を入手したため、
これまでの Com 機能と同様、実 PDF から検証した要求番号を引用しながら実装
しています。

<a id="secoc-architecture"></a>
#### アーキテクチャ — E2E Transformer 方式とは異なる理由

E2E は「Com のコールバックフック（RxIndicationCbk/TxTransformCbk）経由で Rte 層が
呼ぶ」E2E Transformer 方式（AUTOSAR が定義する 3 つの E2E 統合方式の 1 つ）を
採用していますが、SecOC には対応する「Com フック経由」の統合方式が実 AUTOSAR に
存在しません。SecOC は常に、PduR のルーティング経路上に独立した宛先モジュールと
して構成されます（`Com_IPduConfigType.RxIndicationCbk` のような E2E 用の仕組みを
流用せず、`PduR_RxDestType`——CanTp/Com と同じ立ち位置——として実装しています）。

```
【RX】KeyFobEcu (uds_tester Python が模擬)
  → AES-128-CMAC で Secured PDU を生成（pycryptodome。Arduino 側の自前実装
    との相互検証は下記「検証」節参照）
  → CAN 0x120 送信

  Arduino (MeterEcu):
    Can_Hw → CanIf_RxIndication → PduR_ComRxIndication
      → SecOC_IfRxIndication()（PduR 宛先モジュール、DestPduId=0）
          Authentic Payload / Freshness Value / 切り詰め MAC を分離
          AES-128-CMAC を自前実装で再計算し MAC 一致を検証
          Freshness（8bit、単調増加）を検証（リプレイ検知）
          両方 OK → Com_RxIndication(ComRxIPduId=2, AuthenticPayload) を直接呼ぶ
          NG → ログのみ、Com へは一切転送しない
      → Com_ReceiveSignal(IMMOBILIZER_CMD) → Rte_COMCbk_SecureCommand()（ログのみ）

【TX】Arduino (MeterEcu、E2EHealthStatus):
  E2EMon → Com_SendSignal() → Com_MainFunction()（PERIODIC、6000ms周期）
    → TxTransformCbk（E2EXf、E2E CRC/Counter を書き込む）
    → PduR_Transmit(SrcPduId=3, 4byte)
        → TransmitOverrideFct=SecOC_IfTransmit()（Authentic I-PDU を内部
          バッファへコピーし即座に E_OK を返す。[SWS_SecOC_00058]）
    → 次回 SecOC_MainFunction()（100ms周期）:
        Freshness（自身の単調増加カウンタ）+ AES-128-CMAC を計算
        Secured I-PDU（8byte）を組み立て
        → PduR_SecOCTransmit(SrcPduId=3, 8byte) → CanIf_Transmit() → CAN 0x220 送信

  uds_tester (Python、受信モニター):
    受信した 8byte から MAC を pycryptodome で再計算し検証（下記「検証」節）
```

PduR の RX 振り分け機構（`PduR_RxDestType`）は元々、1 つの受信 PDU を複数の
上位層モジュールへ配信できる汎用的な関数ポインタテーブルだったため、RX 方向は
SecOC を新しい宛先として追加するだけで済み、`PduR.c` 自体の変更は不要でした。

TX 方向は当初 RX 専用スコープとして見送っていましたが、「E2EHealthStatus に
メッセージ認証を付与したい」という要求をきっかけに、実 AUTOSAR の
`SecOC_Transmit()` アーキテクチャ（`[7.4.1]` "Authentication during direct
transmission"）に忠実な形で追加しました。実装当初の RX 専用スコープでは
「TX 方向は PduR の TX 経路が `CanIf_Transmit()` 直呼びにハードコードされて
おり、汎用化には手が入る」という理由で意図的に見送っていましたが、今回は
その汎用化自体を行っています。`PduR_TxRoutingPathType` に
`TransmitOverrideFct`/`TransmitOverrideId` を追加し、NULL（既定）なら従来どおり
`CanIf_Transmit()` へ直接転送、非 NULL なら中間モジュール（SecOC）へ委譲する形に
一般化しました（既存の全 TX パスはこのフィールドを設定しないため無変更・無
リグレッションです）。中間モジュールは変換完了後、`PduR_Transmit()` とは別の
`PduR_SecOCTransmit()`（`TransmitOverrideFct` を再評価しない）を呼んで
`CanIf_Transmit()` まで到達させます。

Com/E2E は SecOC の存在を一切知りません（E2E Transformer 方式で Com が E2E の
存在を知らないのと同じ設計思想）。`Com_SendSignal()`/`TxTransformCbk` は
従来どおり 4byte の E2E 保護済みペイロードを扱うだけで、SecOC がその後ろに
Freshness/MAC を継ぎ足して 8byte の Secured I-PDU にすることを一切意識しません。

<a id="secoc-byte-layout"></a>
#### Secured I-PDU バイトレイアウト（SecOC Profile 1 準拠）

`docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf` の **SecOC Profile 1
(24Bit-CMAC-8Bit-FV)**（`[SWS_SecOC_00192]`）に忠実に、CMAC/AES-128・8bit
フレッシュネス・24bit 切り詰め MAC を採用しています。対象フレーム
「ImmobilizerCmd」（CAN ID 0x120、イモビライザー解除コマンドという実車でも
真に認証が必要な典型シナリオ）のレイアウト:

| byte | 内容 | サイズ |
|---|---|---|
| 0 | ImmobilizerCmd（0x00=LOCK, 0x01=UNLOCK） | 1 |
| 1 | Reserved（常に 0x00。将来の鍵 ID 等を想定） | 1 |
| 2 | Freshness Value（8bit、切り詰めなしの全ビットを送信） | 1 |
| 3-5 | 切り詰め MAC（AES-128-CMAC 128bit 出力の上位24bit） | 3 |

**Authenticator の対象データ**（`[7.1.1.2]` "DataToAuthenticator = Data
Identifier | secured part of the Authentic I-PDU | Complete Freshness
Value"）: `DataId(2byte, Big Endian, =0x0120) | AuthenticPayload(2byte) |
FreshnessValue(1byte)` の 5 バイトを AES-128-CMAC へ入力します（Big Endian は
`[SWS_SecOC_00011]`）。

TX 対象フレーム「E2EHealthStatus」（CAN ID 0x220）は、既に E2E P01 保護済みの
4byte ペイロード全体を Authentic I-PDU として扱い、SecOC で外側からさらに
保護します（内側=E2E で意図しない誤りを検出、外側=SecOC で意図的な改ざん・
なりすましを検出、という二重防御の実例）:

| byte | 内容 | サイズ |
|---|---|---|
| 0 | E2E CRC8（Authentic payload、内側の E2E 保護） | 1 |
| 1 | E2E Counter（Authentic payload、下位4bit） | 1 |
| 2 | E2ECrcErrCount（Authentic payload） | 1 |
| 3 | E2ESeqErrCount（Authentic payload） | 1 |
| 4 | Freshness Value（8bit、切り詰めなし） | 1 |
| 5-7 | 切り詰め MAC（AES-128-CMAC 128bit 出力の上位24bit） | 3 |

DLC が 4→8 バイトへ増える点は `CanIf_PBCfg.c` の当該 TxPdu の `.Dlc` のみを
変更しており、Com/E2E 側の DLC（4byte のまま）には一切手を入れていません
（Com は SecOC の存在を知らないため）。RX/TX で異なる鍵（用途・アセットが
異なるため）を割り当てています。

<a id="secoc-simplifications"></a>
#### 明示する簡略化

- **Freshness Value は 8bit 幅全体を送受信し、切り詰めを行いません**（Profile 1 の
  `SecOCFreshnessValueTxLength=8` と一致させ、実車で必要な「送信されない上位
  ビットの推定復元」機構を回避する簡略化）。
- **Csm(Crypto Service Manager)/CryIf(Crypto Interface)/Crypto(Crypto Driver)
  の3層に分離しています**（`src/Bsw/Csm/`・`src/Bsw/CryIf/`・`src/Bsw/Crypto/`）。
  SecOC は `Csm_MacGenerate()`/`Csm_MacVerify()` という抽象 API と `CsmJobId`
  （SecOC_PBCfg.c）しか知らず、実際にどの鍵・どのアルゴリズムを使うかは
  `Csm_PBCfg.c` の CsmJob 設定 → `Crypto_PBCfg.c` の鍵テーブルが決めます
  （AES-128/CMAC の実装自体は `Crypto_Aes128.c`/`Crypto_Cmac.c`。元は
  `SecOC_Aes128.c`/`SecOC_Cmac.c` として SecOC 内に直接持っていたものを移設）。
  また実 AUTOSAR の Csm/CryIf/Crypto はジョブベースの非同期処理
  （`Crypto_JobType` の START/UPDATE/FINISH モード、ジョブキュー）が前提ですが、
  本プロジェクトは全体が同期呼び出しのため常に `CRYPTO_OPERATIONMODE_SINGLECALL`
  のみを使う同期実装です。`Crypto_JobType` 自体も実 AUTOSAR の4段階入れ子構造体
  （`Crypto_JobPrimitiveInputOutputType` 等）ではなく、MAC 生成/検証の2用途に
  必要なフィールドのみを持つ簡略化フラット構造体にしています。
- **鍵バイト列の初期値は `Crypto_PBCfg.c` の固定配列**です（`Crypto_Init()` が
  起動時に RAM キースロットへコピーする。固定鍵をソースコードへ埋め込むことは
  本番運用では絶対に行ってはなりません）。**KeyM（Key Manager）による鍵更新は
  `src/Bsw/KeyM/` に実装済み**で、Dcm の WriteDataByIdentifier（DID
  0x0108 CryptoKeyUpdate）が模擬鍵マスターとして `KeyM_Start()`→
  `KeyM_Update()`→`KeyM_Finalize()` を1回の診断要求内で駆動し、
  `Csm_KeyElementSet()`/`Csm_KeySetValid()` 経由で RAM 上の鍵を書き換えます
  （更新直後は無効化され、Finalize されるまで MAC 生成/検証に使えません）。
  ただし **実 AUTOSAR の KeyM が持つ Certificate submodule（X.509 証明書
  チェーンの格納・検証）は完全に対応除外**（本プロジェクトに PKI の土台が
  一切ないため）で、Crypto key submodule も `KeyM_Start`/`KeyM_Update`/
  `KeyM_Finalize` の3 API のみに絞っています（`KeyM_Prepare`/`KeyM_Verify`、
  SHE M1M2M3 形式の鍵更新、`KEYM_DERIVE_KEY` は未対応）。鍵材料は NVM に
  永続化されないため、再起動すれば `Crypto_PBCfg.c` の初期値に戻ります
  （詳細は `src/Bsw/KeyM/KeyM.h` 冒頭コメント参照）。
- **リプレイ判定（RX）は単調増加チェックのみ**（`(uint8)(received -
  lastAccepted) < 128` という折り返し許容の「半区間」判定）で、実車の
  Freshness Manager（11 章、複数カウンタ・マスタースレーブ同期プロトコル）は
  実装していません。
- **TX の送信確認（TxConfirmation）経路は SecOC を経由しません**（実
  AUTOSAR は `[SWS_SecOC_00063]`/`[SWS_SecOC_00064]` で SecOC が確認結果を
  中継し、動的に確保した Secured I-PDU バッファを解放することを要求しますが、
  本実装は固定長静的バッファ（`SecOC_TxAuthenticBuffer[]`）のみを使い動的確保を
  行わないため、解放処理自体が不要です。したがって `CanIf_TxConfirmation` は
  従来どおり `PduR_CanIfTxConfirmation()` から直接 `Com_TxConfirmation()` へ
  届き、SecOC は一切関与しません）。

<a id="secoc-verification"></a>
#### 検証

外部ライブラリに依存しない AES-128+CMAC の自前実装（`Crypto_Aes128.c`/
`Crypto_Cmac.c`、Csm/CryIf/Crypto レイヤの最下層）が正しいことを、以下の
独立した手段で確認しています。

1. **FIPS-197 Appendix B の公式 AES-128 テストベクタ**（既知の鍵・平文に対する
   暗号文が一致するか）を `Crypto_Init()` 起動時セルフテストとして組み込み済み
   （実機ログで毎回 PASS/FAIL を確認できます）。
2. **RFC 4493 (The AES-CMAC Algorithm) の公式テストベクタ 4 件**（メッセージ長
   0/16/40/64 バイトの各ケース、パディングあり/なし・単一/複数ブロック連鎖の
   すべての分岐を網羅）について、本実装のアルゴリズム（K1/K2 サブ鍵導出・
   パディング・CBC-MAC 連鎖）を Python へ忠実に移植し、`pycryptodome`
   （実績のある独立したライブラリ）の CMAC 実装と全件一致することを確認済み
   （ホスト環境に C コンパイラが無く組込みコードを直接実行できなかったための
   代替検証手段。C コードとの対応は目視でも再確認済み）。
3. `tools/uds_tester` の `_apply_secoc()`（Python、pycryptodome で本物の
   AES-CMAC を計算）と、Arduino 側の `SecOC_IfRxIndication()` → `Csm_MacVerify()`
   → `Crypto_ProcessJob()`（同一の自前実装）が同じ鍵・同じメッセージに対して
   同じ MAC を計算することを、バイトレイアウト（DataId の Big Endian 連結順・
   切り詰め位置）も含めて突き合わせ済みです。
4. TX 方向は `SecOC_MainFunction()` が `Csm_MacGenerate()` 経由で呼ぶ
   `Crypto_Cmac_Calculate()`（RX と同一実装）が計算した MAC を、
   `tools/uds_tester` の `_verify_secoc()`（受信した Secured I-PDU から MAC を
   再計算する、`_apply_secoc()` の逆方向）で独立に再検証できます（下記
   「実機で検証可能」TX 項参照）。

**実機で検証可能**: `tools/uds_tester/config.json` に「ImmobilizerCmd
(0x120, KeyFobEcu)」ボタンを追加しました（UNLOCK/LOCK の 2 プリセット）。

- **正常系**: プリセットを送信すると、Arduino ログに
  `SecOC: RxInd: iPdu=0 verified OK (freshness=N)` に続けて
  `Rte: ImmobilizerCmd: UNLOCK (authenticated via SecOC)` が出力されます。
- **改ざん検知**: 送信前に Entry 欄の MAC バイト（末尾 3 バイト）を手入力で
  1 桁変更してから送信すると、`SecOC: RxInd W: ... MAC verification failed`
  が出力され、`Rte_COMCbk_SecureCommand()` は一切呼ばれません（E2E の
  WRONGCRC 検証と同じ「Entry を手入力で改ざんしてから送信する」方式。
  送信ボタンは常に Entry の内容をそのまま送るため、意図的な改ざんテストが
  行えます）。
- **リプレイ検知**: 一度送信したフレームのログ表示（またはコピーした Entry の
  内容）をそのまま Entry へ貼り戻し、フレッシュネス値を変えずに再送すると、
  MAC は依然として正しいにもかかわらず `SecOC: RxInd W: ... freshness check
  failed (replay or stale)` が出力され、拒否されることが確認できます。

**TX 方向の実機確認**: `tools/uds_tester` の「E2EHealthStatus (0x220)」受信
モニターが、`crcErr=N seqErr=M` に加えて `SecOC:OK`/`SecOC:NG` を表示します
（受信した 8byte から Python 側で MAC を独立に再計算し、Arduino が計算した
MAC と一致するかを毎回検証）。Arduino ログでも
`SecOC: MainFunction: iPdu=0 secured OK (freshness=N)` が 6000ms 周期（
`Com_MainFunction()` の PERIODIC 送信に追従して `SecOC_MainFunction()` が
検知した直後）で出力され続けることが確認できます。

<a id="secoc-scope-limitation"></a>
#### 意図的に応用範囲を限定した理由

本モジュールは Com/SecOC/PduR のアーキテクチャ学習が主目的のため、ドア施錠制御
等の実ハードウェア反応までは作り込んでいません（`Rte_COMCbk_SecureCommand()`
はログ出力のみ）。他の多くの Com 機能（`ComRxDataTimeoutAction` 等）が
「実利より仕様忠実性」であったのに対し、この機能は EngineInfo/AbsInfo の
E2E 検証と同じく、実際に受信経路を通り実機で検証可能です。

<a id="signal-gateway"></a>
### Signal Gateway（Com_GatewayRoute、SWC を介さないシグナル転送）

`docs/AUTOSAR_SWS_COM.pdf` 7.2.5/7.11 章が定義する **Signal Gateway** を実装しました。
RX シグナルの値を、SWC/Rte を一切介さずに Com 内部で直接 TX シグナルへ転送する
仕組みです。

```
The AUTOSAR COM module provides an integrated Signal Gateway for forwarding
signals and signal groups in a 1:n manner ... After the Signal Gateway
received signal or signal groups for routing, it acts immediately as a
sender for these signals ... The signal processing does not differ if the
integrated Signal Gateway forwards a signal ... or if a Software Component
sends it.
```

この最後の一文（"the signal processing does not differ..."）をそのまま実装に
反映し、`Com_GatewayRoute()` は RX バッファから生値をアンパックした後、
**SWC が直接呼ぶのと全く同じ `Com_SendSignal()`** を内部で呼び出します
（フィルタ・TMS・送信要否判定は `Com_SendSignal()` 既存のロジックがそのまま
適用されるため、ゲートウェイ専用の特別なパス分岐は不要です）。

<a id="gateway-example"></a>
#### 適用例 — ImmobilizerCmd（SecOC 検証済み）→ ImmobilizerStatus

SecOC 検証に成功した `ImmobilizerCmd`（RX、CAN 0x120、KeyFobEcu 想定）を、
新規フレーム `ImmobilizerStatus`（TX、CAN 0x230）へ直接転送します。

```
KeyFobEcu → SecOC（MAC・フレッシュネス検証） → Com_RxIndication(ComRxIPduId=2)
  → Rte_COMCbk_SecureCommand()（ログのみ、既存のデモ用グルー）
  → Com_GatewayRoute()（新規）
      RX バッファから ImmobilizerCmd の生値をアンパック
      → Com_SendSignal(COM_SIGNAL_IMMOBILIZER_STATUS, &value)
          （SWC が直接呼ぶ場合と全く同じ経路。COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD
            により値が変化したときだけ次回 Com_MainFunction() で送信）
  → CAN 0x230 (ImmobilizerStatus) 送信
```

**このシナリオを選んだ理由**: `ImmobilizerCmd` は SecOC が PduR レベルで
MAC・フレッシュネスを検証した**後**にしか `Com_RxIndication()` へ届きません
（検証失敗フレームは SecOC が握りつぶし、Com は一切見ません）。つまり
`Com_RxBuffer` に載っている時点で既に認証済みのデータであることが保証されて
おり、それを生のバッファから直接転送しても（＝`Com_ReceiveSignal()` の
ComDataInvalidAction 等のゲートを経由しなくても）安全です。これは実車の
セキュリティゲートウェイ ECU が担う典型的な役割そのもので、「暗号処理の重い
認証は 1 箇所（この場合は SecOC）に集約し、他の内部 ECU は認証済みの単純な
信号だけを受け取ればよい（SecOC/AES-CMAC を実装する必要がない）」という
構成を体現しています。

（対照的に `EngineInfo`/`AbsInfo` は E2E 保護されていますが、E2E 検証は
`RxIndicationCbk`（`Rte_COMCbk_EngineInfo` 等）側の責務であり、`Com_RxBuffer`
自体は E2E 検証の成否に関わらず常に最新の受信バイト列を保持します。この
違いにより、EngineSpeed 等を同じ方式でゲートウェイすると **E2E 未検証の
値を転送してしまう**リスクがあるため、今回は意図的に対象から外しています。）

<a id="gateway-rx-mapping"></a>
#### RX 側処理段階と実装の対応

`[SWS_Com_00872]` が定義する RX 側処理段階（1: デッドライン監視タイマ再始動、
2: I-PDU callout、3: update-bit 確認、4: エンディアン変換）のうち、本実装は
1〜2 を `Com_RxIndication()` の既存処理（`RxIndicationCbk` 呼び出しまで）が
担い、`Com_GatewayRoute()` はその直後（4 のエンディアン変換に相当する
アンパック）から始まります。3（update-bit 確認）は本実装の適用対象
（非 Signal Group シグナル同士）には存在しないため該当しません。

`Com_ReceiveSignal()` が経由する `ComRxDataTimeoutAction`/`ComDataInvalidAction`/
`ComFilterAlgorithm(NEW_IS_WITHIN)` はいずれも `[SWS_Com_00872]` の処理段階に
含まれておらず、ゲートウェイは経由しません。`[SWS_Com_00701]`
「デッドライン監視タイムアウト中でもゲートウェイはルーティングを行う」とも
整合します（本実装はフレーム受信直後の同期呼び出しのため、そもそも
タイムアウト状態になり得ません）。

<a id="gateway-simplifications"></a>
#### 明示する簡略化

- **非 Signal Group のシグナル同士（1:1）のみサポート**します。Signal Group の
  ゲートウェイ（`[SWS_Com_00361]`/`[SWS_Com_00383]`: グループを一貫した集合と
  して転送する要求）や update-bit 連動（`[SWS_Com_00702]`〜`[SWS_Com_00706]`）は、
  具体的な実機検証シナリオが無いため未実装です。
- **1:n のうち n=1 のみ設定**しています（`Com_GwMappingType` 自体は 1 つの
  ソースシグナルに対し複数のマッピングエントリを追加すれば 1:n に対応できる
  設計ですが、具体的な複数転送シナリオが無いため config は 1 件のみ）。
- **`ImmobilizerStatus` フレーム自体には E2E/SecOC いずれの保護も付与していません**
  （内部バスの「素の」ブロードキャストという位置づけ。認証はゲートウェイの
  入力側で既に完了しているため）。

<a id="gateway-verification"></a>
#### 動作確認方法

`uds_tester` で「ImmobilizerCmd」ボタンから UNLOCK/LOCK を送信すると、Arduino
ログに次のように出力されます。

```
[NNNNms] INFO  SecOC: RxInd: iPdu=0 verified OK (freshness=N)
[NNNNms] INFO  Com: RX iPdu=2 [01 00]
[NNNNms] WARN  Rte: ImmobilizerCmd: UNLOCK (authenticated via SecOC)
[NNNNms] INFO  Com: Gateway src=12 -> dst=13 value=1
[NNNNms] INFO  Com: TX iPdu=3 [01]
[NNNNms] INFO  PduR: TX src=4 canif=5
[NNNNms] INFO  CanIf: TX id=5 can=0x230
[NNNNms] INFO  Can_Hw: TX OK id=0x230 dlc=1 [01]
```

`uds_tester` の「ImmobilizerStatus (0x230, Signal Gateway)」受信モニターも
`(UNLOCK)`/`(LOCK)` を表示し、`ImmobilizerCmd` の送信直後に追従して更新される
ことが確認できます。

---
<a id="diag-stack"></a>
### 診断スタック（CanTp / Dcm / Dem / FiM / NvM）

UDS 診断（ISO 14229-1）を処理するスタックです。
CanTp が ISO 15765-2 のフレーム分割・組立を担い、Dcm が UDS サービスを処理します。
Dem は故障情報を DTC として管理し、NvM 経由で EEPROM に永続化します。
FiM は Dem が確定した DTC をもとにアプリ機能の実行許可を判定します。
診断フレームはアプリデータ（0x100 / 0x110 / 0x200）とは独立した CAN ID（0x7E0 / 0x7E8）で通信します。

| 層 | モジュール | 本プロジェクトでの役割 |
|---|---|---|
| BSW | CanTp | ISO 15765-2 のフレーム分割（FF/CF）と再組立。8 バイトを超える UDS 応答を実現 |
|  | Dcm | UDS 診断サービス処理（SID 0x10 / 0x11 / 0x14 / 0x19 / 0x22 / 0x27 / 0x28 / 0x2E / 0x2F / 0x31 / 0x3E）。S3 タイマでセッションを自動失効。SID×セッション許可テーブルで 0x14/0x27/0x28/0x2E/0x2F/0x31 を extendedSession 限定とし、SecurityAccess (0x27) でシード・キー認証保護。0x2E は要求が7バイト超のため CanTp の複数フレーム要求受信を実機検証。0x2F (IOControl) は Rte 層でランプ出力を ASW から奪って強制する診断専用オーバーライド。0x28 (CommunicationControl) は Com/Nm の送受信を個別に有効/無効化する |
|  | Dem | 診断イベントを DTC として管理。カウンタベースのデバウンスで確定し、NvM 経由で EEPROM に永続化。デバウンス確定 FAILED 時に FreezeFrame（RAM のみ）を記録し、ExtendedData（故障確定回数、NvM 永続化）を+1。クリーンな操作サイクルを1回経過すると PendingDTC を自動解除し、再故障せず複数回の操作サイクルを経ると経年回復（Aging）で CONFIRMED を自動解除 |
|  | FiM | Dem が確定（CONFIRMED）した DTC をもとにアプリ機能（FID）の実行許可を判定。100ms 周期で再評価し、結果を ASW へ `Rte_Call_FiM_GetFunctionPermission` で公開 |
|  | NvM | EEPROM の読み書きを抽象化。Dem は EEPROM アドレスを直接知らない。各ブロックに CRC8 を付加して破損を検出し、不一致時は ROM デフォルト値（NvM_RestoreBlockDefaults）へ自動復元。ブロックごとに冗長化（`NvMBlockManagementType=NVM_BLOCK_REDUNDANT` 相当、`Redundant` フラグ）を選択でき、片面が破損してももう片面から自己修復できる（DEM_EXTENDED で使用）。実際の物理バイト書き込みは MemIf 経由で Fee へ委譲し、NvM 自身はブロック・CRC・冗長化という「意味」のレイヤーのみを扱う（1バイトずつの非同期書き込み進行は Fee の責務） |

<a id="uds-diag-comm"></a>
#### UDS 診断通信（ISO 14229-1 / ISO 15765-2）

Dcm (Diagnostic Communication Manager) が UDS サービスを処理し、
CanTp (CAN Transport Protocol) が ISO 15765-2 のフレーム分割・組立を担います。

##### 診断フレームルーティング

```
外部テスター（Cangaroo 等）
  │  CAN 0x7E0  [UDS 要求 / FC]
  ↓
MCP2515 → Can → CanIf（CanId=0x7E0 → RxPduId=1）
                  ↓
                PduR（パス 1: CanTp 専用ルート）
                  ↓
                CanTp_RxIndication()
                  SF → UDS ペイロードを即時渡し
                  FF → FC 送信、CF 待ち
                  CF → バッファ組立、完成後に渡し
                  FC → TX 側の CF 送信を再開
                  ↓
                Dcm_ComIndication() → UDS サービス処理
                  ↓
                CanTp_Transmit()
                  ≤7B → SF 送信
                  ≥8B → FF 送信 → FC 待ち → CF 送信
                  ↓
                PduR_Transmit(SrcPduId=1) → CanIf → Can
  │  CAN 0x7E8  [UDS 応答 / FC]
  ↓
外部テスター
```

CAN 0x100（EngineInfo）・0x110（AbsInfo）・0x7E0（診断要求）は PduR でルートが分離されており、互いに干渉しません。

##### 対応 UDS サービス

テスト時に毎回フレーム構造を探し回らずに済むよう、SID×SubFunc 単位で送信フレームの
固定バイト・可変バイトをまとめます。byte0 は CanTp の SF PCI（UDS ペイロード長）です。
0x2E（後述）以外の要求ペイロードは 7 バイト以内のため SF で送信できます
（応答が長くなるケースは個別に後述します）。
**正応答 SID は ISO 14229-1 の共通規則により常に「要求 SID + 0x40」**
（例: 0x10→0x50、0x27→0x67）のため、表には記載しない。

| SID<br>サービス名 | Def | Ext | SubFunc | 要求フレーム（byte0=PCI） | 可変バイト・備考 |
|---|---|---|---|---|---|
| 0x10<br>DiagnosticSessionControl | ○ | ○ | 0x01<br>(Default) | `02 10 01 00 00 00 00 00` | — |
|  |  |  | 0x03<br>(Extended) | `02 10 03 00 00 00 00 00` | S3タイマ起動 |
| 0x11<br>ECUReset | ○ | ○ | 0x01<br>(hardReset) | `02 11 01 00 00 00 00 00` | — |
|  |  |  | 0x03<br>(softReset) | `02 11 03 00 00 00 00 00` | — |
| 0x14<br>ClearDiagnosticInformation | × | ○ | — | `04 14 FF FF FF 00 00 00` | byte2-4=groupOfDTC<br>・0xFFFFFF=全クリア<br>・DTCコード指定=1件クリア<br>**SecurityAccess Level1 必須**（未認証は NRC 0x33） |
| 0x19<br>ReadDTCInformation | ○ | ○ | 0x01<br>(件数取得) | `03 19 01 MM 00 00 00 00` | byte3=statusMask |
|  |  |  | 0x02<br>(DTC一覧取得) | `03 19 02 MM 00 00 00 00` | byte3=statusMask |
|  |  |  | 0x04<br>(FreezeFrame取得) | `06 19 04 HH MM LL RR 00` | byte3-5=DTCコード<br>byte6=recordNumber（固定0x01） |
|  |  |  | 0x06<br>(ExtendedData取得) | `06 19 06 HH MM LL RR 00` | byte3-5=DTCコード<br>byte6=recordNumber（固定0x01） |
| 0x22<br>ReadDataByIdentifier | ○ | ○ | — | `03 22 HH LL 00 00 00 00` | byte2-3=DID（0x0101/0x0102/0x0103/0x0104） |
| 0x27<br>SecurityAccess | × | ○ | 0x01<br>(requestSeed) | `02 27 01 00 00 00 00 00` | seed 2 バイト |
|  |  |  | 0x02<br>(sendKey) | `04 27 02 HH LL 00 00 00` | byte2-3=key（big-endian） |
| 0x2E<br>WriteDataByIdentifier | × | ○ | — | FF+CF（後述、SF 不可） | DID=0x0104 (TestPattern) 固定 8 バイト、DID=0x0108 (CryptoKeyUpdate) keyName(1)+key(16)=17バイトのみ対応<br>**SecurityAccess Level1 必須**（未認証は NRC 0x33） |
| 0x2F<br>InputOutputControlByIdentifier | × | ○ | 0x00-0x03<br>(controlOptionRecord) | `04 2F HH LL OO 00 00 00`<br>(shortTermAdjustment のみ `05 2F HH LL 03 SS 00 00`) | byte2-3=DID（0x0105/0x0106/0x0107）<br>byte4=controlOptionRecord<br>byte5=controlState（shortTermAdjustmentのみ、0/1）<br>**SecurityAccess 不要**（0x14/0x2E と異なり車両制御・NVM書換を伴わないため） |
| 0x28<br>CommunicationControl | × | ○ | 0x00-0x03<br>(controlType) | `03 28 CC TT 00 00 00 00` | byte2=controlType（0x00 enableRxAndTx/0x01 enableRxAndDisableTx/0x02 disableRxAndEnableTx/0x03 disableRxAndTx）<br>byte3=communicationType（0x01 通常通信/0x02 NM通信/0x03 両方）<br>拡張アドレス指定(0x04/0x05)・サブネット指定は非対応<br>**SecurityAccess 不要**（0x2F と同じ理由） |
| 0x31<br>RoutineControl | × | ○ | 0x01<br>(startRoutine) | `04 31 01 02 03 00 00 00` | RID=0x0203 (EngineHealthCheck) のみ対応<br>**SecurityAccess 不要**（センサ読み取りのみで車両制御・NVM書換を伴わないため） |
|  |  |  | 0x02<br>(stopRoutine) | `04 31 02 02 03 00 00 00` | 未開始 (IDLE) で呼ぶと NRC 0x24 |
|  |  |  | 0x03<br>(requestRoutineResults) | `04 31 03 02 03 00 00 00` | 実行中は結果未確定 (RUNNING)、完了後は PASS/FAIL を返す（後述） |
| 0x34<br>RequestDownload | × | ○ | — | FF+CF（後述、SF 不可、11バイト） | byte2=dataFormatIdentifier（0x00 固定）<br>byte3=addressAndLengthFormatIdentifier（例 0x44=アドレス4B+サイズ4B）<br>以降=memoryAddress+memorySize<br>**SecurityAccess Level1 必須**（未認証は NRC 0x33） |
| 0x36<br>TransferData | × | ○ | — | `0N 36 CC DD DD ...`（データ長により SF/FF+CF） | byte2=blockSequenceCounter（0x01開始）<br>byte3-=転送データ（実データは保持せずチェックサムのみ計算、後述） |
| 0x37<br>RequestTransferExit | × | ○ | — | `01 37 00 00 00 00 00 00` | 応答にチェックサム（後述） |
| 0x3E<br>TesterPresent | ○ | ○ | 0x00 | `02 3E 00 00 00 00 00 00` | S3タイマ維持 |

Def/Ext 列は `Dcm_SidSessionTable[]`（Dcm_Cbk.c）の設定そのもので、SID 単位
（SubFunc 単位ではない）の制約のため SID 行にのみ記載する。×の場合、該当セッションで
要求すると各ハンドラに到達する前に NRC 0x7F（serviceNotSupportedInActiveSession）で拒否される。
非対応サービスは NRC 0x11（serviceNotSupported）で応答します。
statusMask の代表値: `0x08`=confirmedDTC のみ / `0xFF`=全件。
0x19/04 の応答は 18 バイトと SF の 7 バイト制限を超えるため CanTp が FF+CF に分割します
（詳細は後述の「FreezeFrame」節）。0x19/02 も 2 件以上ヒットすると同様にマルチフレームになります。

##### DID 一覧（0x22 ReadDataByIdentifier）

| DID    | データ      | 型                                            | 単位 |
|--------|------------|-----------------------------------------------|------|
| 0x0101 | EngineSpeed | uint16, big-endian                           | rpm  |
| 0x0102 | CoolantTemp | uint8                                        | ℃   |
| 0x0103 | EngineState | uint8（0=OFF / 1=STARTING / 2=RUNNING / 3=FAULT） | —  |

##### DID 一覧（0x2F InputOutputControlByIdentifier）

0x22/0x2E とは別の DID 空間として扱う（`Dcm_ReadDid()` には含まれず、0x22 で読み出すことはできない）。

| DID    | データ    | 型            | 対応する物理出力 |
|--------|----------|---------------|-----------------|
| 0x0105 | RunLamp   | uint8 (0/1)  | RUNNING LED (D6) |
| 0x0106 | FaultLamp | uint8 (0/1)  | FAULT LED (D7)   |
| 0x0107 | AbsLamp   | uint8 (0/1)  | ABS LED (D8)     |

##### フレーム例（シングルフレーム）

**セッション切替（ExtendedDiagnosticSession）:**
```
送信 → 0x7E0: [02 10 03 00 00 00 00 00]
受信 ← 0x7E8: [06 50 03 00 19 01 F4 00]
```

**EngineSpeed 読み出し（DID 0x0101）:**
```
送信 → 0x7E0: [03 22 01 01 00 00 00 00]
受信 ← 0x7E8: [05 62 01 01 HH LL 00 00]  ← HH:LL が rpm 値（big-endian）
```

**ECUReset（hardReset）:**
```
送信 → 0x7E0: [02 11 01 00 00 00 00 00]
受信 ← 0x7E8: [02 51 01 00 00 00 00 00]
```

**RunLamp を強制点灯（shortTermAdjustment, DID 0x0105）:**
```
送信 → 0x7E0: [05 2F 01 05 03 01 00 00]
受信 ← 0x7E8: [05 6F 01 05 03 01 00 00]  ← byte5=controlStatusRecord（適用後の実際のレベル）
```

**RunLamp の診断制御を解除して ASW に戻す（returnControlToECU）:**
```
送信 → 0x7E0: [04 2F 01 05 00 00 00 00]
受信 ← 0x7E8: [04 6F 01 05 00 LL 00 00]  ← LL は解除直後にまだ出力されていた値
```

##### ランプ IOControl の実現方式（Rte でのオーバーライド調停）

`App_WarningIndicator_Run()`（500ms 周期）は、Dcm の存在を知らないまま従来どおり
`Rte_Call_LedRunning_SetLevel()` 等を毎サイクル呼び続けます。Dcm が診断制御中の間、
この呼び出しの引数を「無視」して固定値を出力し続けることで IOControl を実現しています。

```
Rte_Call_LedRunning_SetLevel(aswLevel)  ← ASW が 500ms ごとに呼ぶ（Dcm の存在を知らない）
  Rte_Lamp_ArbitrateAndWrite(RTE_LAMP_RUN, aswLevel):
    Rte_LampOverrideActive[RUN] == 0 (通常) ?
      YES → effectiveLevel = aswLevel               ← ASW の値がそのまま反映される
      NO  → effectiveLevel = Rte_LampOverrideValue[RUN]  ← Dcm が固定した値を優先
    IoHwAb_LedRunning_SetLevel(effectiveLevel)

Dcm_HandleIoControl(RunLamp, shortTermAdjustment, level=1):
  Rte_IoControl_Lamp_ShortTermAdjustment(RTE_LAMP_RUN, 1)
    Rte_LampOverrideActive[RUN] = 1
    Rte_LampOverrideValue[RUN]  = 1
    IoHwAb_LedRunning_SetLevel(1)   ← 応答を返す前に即座に物理出力へ反映

Dcm_HandleIoControl(RunLamp, returnControlToECU):
  Rte_IoControl_Lamp_ReturnControlToEcu(RTE_LAMP_RUN)
    Rte_LampOverrideActive[RUN] = 0
    ← 次回の App_WarningIndicator_Run() 呼び出し（最大500ms後）で ASW の計算値に復帰
```

**なぜ ASW ではなく Rte で調停するか**: ASW（SW-C）に「Dcm が制御中かどうか」を
判定させると、SW-C が BSW モジュール（Dcm）の存在を知ることになり、AUTOSAR の
層分離原則に反します。本プロジェクトでは Com の `ComFilterAlgorithm`（値が変化した
ときだけ送信するかどうかを Com 自身が決める）や Signal Group（複数シグナルの
コミットタイミングを Com 自身が決める）と同じ設計判断を、CAN 送信ではなく
物理出力の調停に適用しました。「ASW は要求するだけ、実際に反映するかどうかは
BSW/RTE が決める」という責務分離を、通信スタックだけでなく I/O 制御にも
一貫して適用したことになります。

**4つの controlOptionRecord の使い分け**:
- `returnControlToECU`（0x00）: オーバーライド解除のみ。物理出力への書き込みは行わず、
  次の ASW サイクルに委ねる（レイテンシは最大 500ms）。
- `resetToDefault`（0x01）: デフォルト値（消灯）に固定し、制御は診断側が保持し続ける
  （`returnControlToECU` を呼ぶまで ASW には戻らない）。ISO 14229-1 上は解釈に幅がある
  パラメータのため、本実装ではこの方針を採用したことをコメントに明記している。
- `freezeCurrentState`（0x02）: 新しい値を指定せず、「今まさに出力されている値」
  （`Rte_LampLastLevel`）をそのまま固定する。FAULT LED が点滅中に発行すると、
  その瞬間の点灯/消灯状態で止まる。
- `shortTermAdjustment`（0x03）: `controlState`（1 バイト、0/1）で明示的に値を指定する。

##### CommunicationControl（SID 0x28）

診断セッション中に通信の送受信そのものを無効化する UDS サービスです。実車では
リプログラミング（フラッシュ書き換え）の直前など、通常のバス通信が診断作業の
邪魔になる場面で使われます。

**controlType（サブ機能）と communicationType の組み合わせ**:

```
要求: [0x28, controlType, communicationType]
応答: [0x68, controlType]

controlType（Rx/Tx の有効/無効）:
  0x00 enableRxAndTx           rx=1 tx=1（既定状態への復帰）
  0x01 enableRxAndDisableTx    rx=1 tx=0
  0x02 disableRxAndEnableTx    rx=0 tx=1
  0x03 disableRxAndTx          rx=0 tx=0
  0x04/0x05（拡張アドレス指定）は非対応 → NRC 0x12 (subFunctionNotSupported)
  （本 ECU はゲートウェイではなく単一ネットワークのみのため、サブネット別の
   制御は行わない）

communicationType（対象。ビット0=通常通信、ビット1=NM通信）:
  0x01 通常通信（Com）のみ
  0x02 ネットワークマネジメント通信（Nm）のみ
  0x03 両方
  上記以外（サブネット指定を含む）→ NRC 0x31 (requestOutOfRange)
```

**対象と非対象**: `communicationType` の bit0（通常通信）は Com モジュール
（EngineInfo/AbsInfo 受信、MeterStatus/WarningStatus 送信）を、bit1（NM通信）は
Nm モジュール（CAN 0x400 送信）を制御します。診断通信そのもの（CanTp/Dcm）は
対象外です。これは仕様上も本質的な要件です — もし診断通信まで無効化してしまうと、
テスターが再度 CommunicationControl を送って復帰させる手段そのものを失ってしまいます。

**Rx 無効時の挙動**: `Com_RxIndication()` が受信フレームを無視します（バッファ・
タイムアウトタイマとも更新しません）。**Tx 無効時の挙動**: DIRECT/MIXED/PERIODIC
いずれの I-PDU も、実送信（`PduR_Transmit()`）は `Com_MainFunction()` 内で行われ、
`Com_TxEnabled==0` の間はここで抑制されます（詳細は次項）。TX バッファの値自体は
`Com_SendSignal()` が既に更新済みのため失われず、再開後に実際に値が変化した時、
または通常の周期フロアに新たに達した時に初めて送信されます。

**Rx 無効中の受信デッドライン監視**: `Com_MainFunction()`（受信デッドライン監視、
100ms 周期）は `Com_RxEnabled==0` の間、監視自体を評価しない
（SWS_Com_00684/SWS_Com_00685: `Com_IpduGroupStop` により I-PDU が止められた
間は受信処理だけでなくデッドライン監視自体も無効化する要求への対応）。
意図的に CommunicationControl で受信を止めているだけなのに、無効化を続けたことで
`Com_RxTimedOut` が誤って立ち「通信異常」と判定されることはない。

あわせて、再度有効化（`RxEnabled` が 0→1）した瞬間に全 RX I-PDU の
`Com_RxLastMs`（最終受信時刻）と `Com_RxTimedOut` をリセットする
（SWS_Com_00787: `Com_IpduGroupStart` 時にデッドライン監視タイマを
再始動する要求に対応）。これにより、無効化前の古いタイマのまま再有効化直後に
即座にタイムアウト判定される、ということは起きない。

**Tx 無効中の送信トリガー破棄**: DIRECT/MIXED I-PDU の変化検知は `Com_TxPending[]`
（「次回 `Com_MainFunction()` で送信すべき変化あり」フラグ）に記録されます。
`Com_MainFunction()` はこのフラグが立っている（または周期フロアに達した）I-PDU を
見つけるたび、`Com_TxEnabled` の値によらず無条件に `Com_TxPending[]` をクリアし
`Com_TxLastSentMs` を更新した**上で**、`Com_TxEnabled==0` なら実送信をスキップします。
つまり Tx 抑制中に変化があっても、そのフラグは Com_MainFunction() の次回巡回で
消費されて捨てられるだけで、後から蒸し返されることはありません
（SWS_Com_00777「停止中の I-PDU の送信要求はキャンセルしなければならない」、
SWS_Com_00334「停止中に発生した送信トリガーは保持されず、再開しても古い
トリガーで即座に送信されることはない」への対応）。再開後に実際に値が変化した時、
または通常の周期フロアに新たに達した時に初めて送信される（＝仕様どおり「即座には
送信されない」）。

**セキュリティ方針**: 0x2F と同様に SecurityAccess は要求しません（車両制御や
NVM 書き換えを伴わないため）。ただし通信を止めるという操作的な影響の大きさから、
0x2F/0x31 と同じく extendedSession 限定とします。

**セッション復帰時のリセット**: `Dcm_CommControlReset()` が defaultSession への
遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）で Rx/Tx を enableRxAndTx へ
自動的に戻します。`Dcm_SecurityLock()`・`Dcm_RoutineAbort()` と同じ「セッションが
変われば診断側の一時状態は破棄する」という方針です。これを怠ると、通信を無効化した
まま診断ツールが切断された場合、ECU が二度と正常に通信できなくなってしまいます。

**動作確認方法**: uds_tester の「CommunicationControl (0x28)」ボタンで
`disableRxAndTx / normal (03/01)` を送信すると、MeterStatus/WarningStatus の送信と
EngineInfo/AbsInfo の受信が止まることが CAN ログで確認できます。
`enableRxAndTx / normal (00/01)` で元に戻ります。

##### RoutineControl（SID 0x31、EngineHealthCheck）

0x2F（IOControl）が要求受信と同時に結果を確定するのに対し、0x31（RoutineControl）は
「開始（startRoutine）」と「結果取得（requestRoutineResults）」が別々のサービス呼び出しに
分かれる、UDS で頻出の非同期処理パターンを実装しています。本実装で対応する唯一の
RoutineID `0x0203`（`DCM_RID_ENGINE_HEALTH_CHECK`、実車の RID ではなく学習用の独自定義）は、
「EngineSpeed/CoolantTemp が正常範囲内かを判定する自己診断」を模しており、開始から
3 秒（`DCM_ROUTINE_DURATION_MS`）経過して初めて結果が確定します。

**状態機械（IDLE / RUNNING / COMPLETED）:**

```
startRoutine (31 01)
  IDLE/COMPLETED → RUNNING（開始時刻を記録）
  RUNNING 中の再要求 → NRC 0x22 conditionsNotCorrect（多重起動不可）

Dcm_MainFunction()（1000ms 周期）:
  RUNNING かつ経過時間 >= 3000ms ?
    YES → EngineSpeed <= 8000rpm かつ CoolantTemp <= 100℃ を判定
          RUNNING → COMPLETED（結果を確定）

requestRoutineResults (31 03)
  IDLE            → NRC 0x24 requestSequenceError（未開始）
  RUNNING         → routineStatusRecord=[0x00]（実行中、結果未確定）
  COMPLETED       → routineStatusRecord=[0x01, PASS(0x01)/FAIL(0x00)]
                    （結果は次の startRoutine まで何度でも取得可能）

stopRoutine (31 02)
  IDLE            → NRC 0x24（開始していないものは停止できない）
  RUNNING/COMPLETED → IDLE に戻す
```

defaultSession への遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）で
実行中・完了済みのルーチンは破棄され IDLE に戻ります（SecurityAccess の
再ロックと同じ「セッションが変われば診断側の一時状態は破棄する」という方針）。

**フレーム例:**
```
startRoutine:
送信 → 0x7E0: [04 31 01 02 03 00 00 00]
受信 ← 0x7E8: [04 71 01 02 03 00 00 00]

requestRoutineResults（実行中）:
送信 → 0x7E0: [04 31 03 02 03 00 00 00]
受信 ← 0x7E8: [05 71 03 02 03 00 00 00]   ← byte5=0x00 (RUNNING)

requestRoutineResults（完了後、PASS）:
送信 → 0x7E0: [04 31 03 02 03 00 00 00]
受信 ← 0x7E8: [06 71 03 02 03 01 01 00]   ← byte5=0x01 (COMPLETED), byte6=0x01 (PASS)
```

**動作確認方法**: uds_tester の「EngineHealthCheck (RID 0203)」ボタンで
startRoutine プリセットを送信後、3 秒待たずに requestRoutineResults を送ると
「実行中 (running)」、3 秒経過後に送ると「完了 (PASS/FAIL)」と表示されます。

##### RequestDownload/TransferData/RequestTransferExit（SID 0x34/0x36/0x37）

実車 ECU の代表的な機能である「UDS 経由でのソフトウェア再書き込み（フラッシュ
ブートローダ）」のシーケンスを模擬します。**実際にフラッシュへ書き込むわけでは
ありません**（Arduino 自身のファームウェアを書き換えることはしない、プロトコルの
状態遷移を学ぶためのシミュレーションです）。受信データそのものも保持せず、
(1) blockSequenceCounter の順序検証、(2) 受信バイト数の集計、(3) 全受信バイトの
簡易チェックサム（XOR）計算、の3点だけを行います。

**状態機械（IDLE / DOWNLOADING）:**

```
RequestDownload (34)
  SecurityAccess Level1 未アンロック → NRC 0x33 securityAccessDenied
  DOWNLOADING 中の再要求            → NRC 0x22 conditionsNotCorrect（多重開始不可）
  dataFormatIdentifier ≠ 0x00       → NRC 0x31（圧縮・暗号化は非対応）
  memorySize が 0 または上限超       → NRC 0x31
  受理 → 受信バイト数=0・チェックサム=0・期待カウンタ=0x01 にリセットし DOWNLOADING へ
         maxNumberOfBlockLength（既定16バイト）を応答

TransferData (36)  ← memorySize に達するまで繰り返す
  IDLE 中の要求                     → NRC 0x24 requestSequenceError（未開始）
  blockSequenceCounter が期待値と不一致 → NRC 0x73 wrongBlockSequenceCounter
  累計受信サイズが memorySize を超過 → NRC 0x71 transferDataSuspended
  受理 → 受信バイト数に加算、チェックサム更新（XOR）
         期待カウンタを+1（0xFF の次は 0x00、その次はまた 0x01）

RequestTransferExit (37)
  IDLE 中の要求                     → NRC 0x24（未開始）
  累計受信サイズ ≠ memorySize        → NRC 0x22（転送未完了）
  一致 → DOWNLOADING → IDLE に戻し、チェックサムを応答
```

defaultSession への遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）で
進行中の転送は破棄され IDLE に戻ります（RoutineControl と同じ「セッションが
変われば診断側の一時状態は破棄する」という方針）。SecurityAccess は
RequestDownload でのみ判定します。TransferData/RequestTransferExit は
`Dcm_TransferState` が RequestDownload 経由でしか DOWNLOADING にならないため、
個別の再チェックは不要です。

**blockSequenceCounter の巡回規則（ISO 14229-1）**: 0x01 から開始し、
0xFF に達したら次は 0x00、その次はまた 0x01 へ戻ります（0x00 が初期値になる
ことはなく、巡回の一部としてのみ出現します）。

**なぜチェックサムを返すのか**: 実務の書き込みシーケンスでも、
RequestTransferExit の応答（transferResponseParameterRecord、内容は
manufacturer-specific）でチェックサムや CRC を返し、テスタ側が転送全体の
整合性を確認できるようにすることがよくあります。本実装ではその一例として
単純な XOR チェックサムを採用しています。

**フレーム例**（UDS ペイロード表記。memorySize=20 バイトを 10 バイトずつ
2 ブロックに分けて転送。応答はいずれも SF のため先頭に実際の CAN PCI
バイトを付けて示す）:
```
RequestDownload（UDS ペイロード 11 バイト、SF 不可のため FF+CF）:
送信 → 0x7E0: [34 00 44 00 00 00 00 00 00 00 14]
                     └┘ └───────┬───────┘ └───┬───┘
                  format  memoryAddress=0  memorySize=0x14(20)
受信 ← 0x7E8: [04 74 20 00 10]  (SF、4バイト)
                     └┘ └──┬──┘
              lengthFmt  maxNumberOfBlockLength=16

TransferData（1ブロック目、UDS ペイロード 12 バイト、FF+CF、counter=0x01）:
送信 → 0x7E0: [36 01 11 22 33 44 55 66 77 88 99 AA]
受信 ← 0x7E8: [02 76 01]  (SF、2バイト)

TransferData（2ブロック目、UDS ペイロード 12 バイト、FF+CF、counter=0x02）:
送信 → 0x7E0: [36 02 BB CC DD EE FF 00 11 22 33 44]
受信 ← 0x7E8: [02 76 02]  (SF、2バイト)

RequestTransferExit（UDS ペイロード 1 バイト、SF）:
送信 → 0x7E0: [01 37]
受信 ← 0x7E8: [02 77 44]  (SF、2バイト。0x44=全20バイトの XOR チェックサム)
```

**動作確認方法**: uds_tester の「Software Download (0x34/0x36/0x37)」グループで
RequestDownload → TransferData（ブロック1）→ TransferData（ブロック2）→
RequestTransferExit の順に送信すると、上記のシーケンスが実機で確認できます。
「TransferData 異常系」ボタンの「Wrong Counter」プリセットを RequestDownload
直後に送ると counter 不一致で NRC 0x73 が、「宣言サイズ超過」プリセットを
ブロック1・2送信済みの後に送ると NRC 0x71 が返ることも確認できます。

##### S3 タイマ（セッションタイムアウト）

ISO 14229-1 では、defaultSession 以外（本実装では ExtendedDiagnosticSession）の間に
診断要求が一定時間（S3、既定 5000ms）途絶えると、テスターが離脱したとみなして
ECU が自動的に defaultSession へ復帰します。SID 0x3E（TesterPresent）は、
他に送るべき要求がないときにこの自動失効を防ぐためだけに存在するサービスです。

```
Dcm_ComIndication（要求受信時、SID を問わず毎回）:
  Dcm_LastActivityMs = millis()        ← S3 タイマをリセット

Dcm_MainFunction（1000ms 周期、Os Task 8）:
  session != Default かつ
  millis() - Dcm_LastActivityMs >= 5000ms (DCM_S3_TIMEOUT_MS) ?
    YES → session = Default
          INFO: "S3 timeout -> session=Default"
```

ExtendedDiagnosticSession に切り替えた後、5 秒以上どの診断要求も送らずに放置すると、
セッションが自動的に Default に戻ります。SID 0x22 等のセッション依存サービスは
本実装ではセッションを問わず応答するため動作に影響しませんが、シリアルログで
S3 タイマの遷移を確認できます。

##### SecurityAccess（SID 0x27、Level1）

ClearDiagnosticInformation（0x14、DTC 履歴の消去）は誤操作・悪用の影響が大きいため、
SecurityAccess の Level1（subFunc 0x01/0x02）でアンロックしないと NRC 0x33
（securityAccessDenied）で拒否されます。requestSeed → sendKey の 2 段階チャレンジ
レスポンス方式は ISO 14229-1 標準の認証フローです。

```
1. requestSeed (27 01)
     ECU が seed（millis() 由来、毎回変化）を発行し、
     「seed 発行済み・key 未受信」状態にする。
     既にアンロック済みなら ISO 14229-1 の作法通り allZeroSeed (0x0000) を返す
     （sendKey は不要、テスター側はこれを見てアンロック済みと判断する）。

2. sendKey (27 02 <keyH> <keyL>)
     テスターは seed から key を計算して送信する。
     本実装の変換式（学習用の単純な例）:
         key = seed XOR DCM_SECURITY_KEY_MASK (0xA55A)
     一致 → Level1 アンロック、0x67 正応答。
     不一致 → NRC 0x35 (invalidKey)。
     3 回連続失敗 → NRC 0x36 (exceededNumberOfAttempts) を返し、
       以後 10 秒間 (DCM_SECURITY_DELAY_MS) requestSeed 自体を
       NRC 0x37 (requiredTimeDelayNotExpired) で拒否する（ブルートフォース対策）。

3. defaultSession へ遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）すると
   Level1 は再ロックされる。ただし連続失敗回数・ロックアウト中フラグは
   セッションをまたいで保持する（セッション往復の繰り返しでロックアウトを
   回避できないようにするため）。
```

**注意:** `key = seed XOR 固定マスク` は仕組みを学ぶための最小限の例です。
量産 ECU は OEM 固有の非公開アルゴリズムや暗号学的アルゴリズムを使用するため、
本実装のロジックを実運用に転用しないでください。

##### SID × セッション許可テーブル

`SecurityAccess は extendedSession 限定`という制約は、当初
`Dcm_HandleSecurityAccess` の中に個別にハードコードされていました。
他のサービス（ClearDTC 等）にも同種のセッション制約が増えていくと、
「どの SID がどのセッションで使えるか」が各ハンドラに分散して見通しにくくなります。
そこで AUTOSAR の `DcmDspSessionRow` コンフィグに相当する一覧テーブルへ一般化し、
`Dcm_ComIndication()` が SID ディスパッチの**前**に全 SID 共通で判定するようにしました。

```c
/* Dcm_Cbk.c */
static const Dcm_SidSessionRowType Dcm_SidSessionTable[] =
{
    { DCM_SID_CLEAR_DTC,        DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_SECURITY_ACCESS,  DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_WRITE_DATA,       DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_IO_CONTROL,       DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_COMM_CONTROL,     DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_ROUTINE_CONTROL,  DCM_SESSION_MASK_EXTENDED },
};
```

テーブルに掲載のない SID はセッション制約なしとみなされ、defaultSession でも応答します。
0x14・0x27・0x28・0x2E・0x2F・0x31 のみ extendedSession 限定とし、defaultSession で要求すると各ハンドラに
到達する前に NRC 0x7F（serviceNotSupportedInActiveSession）で拒否されます
（各 SID の制約は前述の「対応 UDS サービス」表の Def/Ext 列を参照）。

このテーブルはセッション制約のみを一元管理し、SecurityAccess（追加の認証）が
必要かどうかは各ハンドラが個別に判定します（`Dcm_HandleClearDtc` / `Dcm_HandleWriteDataById`
先頭の `Dcm_SecurityLevel == 0U` チェック）。0x2F はテーブルには載るが
SecurityAccess チェックは行わないため、extendedSession でありさえすれば
認証なしで実行できます。これは「セッション制約」と「認証要求」が独立した
直交する保護軸であることを示す例でもあります。0x14/0x2E（車両データの恒久的な
消去・書き換え）は両方必須、0x2F（ダッシュボードランプの一時的な出力オーバーライド）
はセッションのみで十分、という判断です。

新しいサービスにセッション制約を追加する場合は、ハンドラ内に判定を書き足すのではなく
`Dcm_SidSessionTable[]` に行を追加するだけで済みます。

<a id="cantp"></a>
#### CanTp（ISO 15765-2 トランスポートプロトコル）

CanTp モジュールが ISO 15765-2 のフレーム処理を担い、
DCM は PCI バイトを意識せず生 UDS ペイロードのみを扱います。

##### ISO 15765-2 フレーム構造

| フレーム種別 | PCI (byte[0]) | 内容 |
|------------|--------------|------|
| SF (Single Frame) | `0x0N` N=ペイロード長 | UDS ペイロード ≤ 7 バイト |
| FF (First Frame)  | `0x1H 0xLL` HL=総長 | UDS ペイロード ≥ 8 バイト の先頭 6 バイト |
| CF (Consecutive Frame) | `0x2n` n=シーケンス番号 | 続きのデータ（最大 7 バイト/フレーム） |
| FC (Flow Control) | `0x3X` X=FS | CTS(0)/WAIT(1)/OVFLW(2)、BS、STmin |

##### RX 状態マシン（Arduino 受信側）

```
IDLE ──── SF 受信 ──────────────────→ Dcm_ComIndication → IDLE
     ──── FF 受信 → FC(CTS) 送信 ──→ WAIT_CF
WAIT_CF ─ CF 受信(未完) ────────────→ WAIT_CF
        ─ CF 受信(完成) ────────────→ Dcm_ComIndication → IDLE
        ─ N_Cr タイムアウト(1000ms) ─→ IDLE (中断)
        ─ 別の SF/FF 受信 ──────────→ 進行中の受信を中断し、新規受信として処理（後述）
```

##### WAIT_CF 中に別の SF/FF を受信した場合（SWS_CanTp_00124）

複数フレームの再組立中（`WAIT_CF`）に、別の新しい SF/FF を受信した場合は、
進行中の受信を**即座に中断し、受信したフレームを新規受信の開始として処理**します
（SWS_CanTp_00124: 同一コネクションで受信中に SF/FF を受信したら、進行中の受信を
中断して新規受信として処理しなければならない）。本プロジェクトは診断 RX N-SDU が
1 系統のみのため、常にこの「同一コネクション」に該当します。

古い再組立中のデータ（それまで受信していた CF の断片）は失われますが、これは
仕様上正しい挙動です。新しい SF/FF を無視して古いセッションの完了・タイムアウトを
待たせる設計は、テスターが再送したフレームを最大 N_Cr（1000ms）も無駄に待たせて
しまい、診断ツール側からは原因不明の遅延・タイムアウトに見えてしまいます。

```
[1000ms] INFO  CanTp: RX FF len=20                              ← 1 つ目の FF (WAIT_CF へ)
[1010ms] WARN  CanTp: RX FF aborts in-progress reception, starting new  ← 2 つ目の FF が届いた
[1010ms] INFO  CanTp: RX FF len=15                               ← 2 つ目を新規受信として即座に受理
```

##### TX 状態マシン（Arduino 送信側）

```
IDLE ──── ≤7 バイト → SF 送信 ──────────────────────────→ IDLE
     │                     └─ 送信失敗 ────────────────→ E_NOT_OK を返し IDLE のまま
     ──── ≥8 バイト → FF 送信 ──────────────────────────→ WAIT_FC
     │                     └─ 送信失敗 ────────────────→ E_NOT_OK を返し中断
WAIT_FC ─ FC(CTS) 受信 ─────────────→ SEND_CF
        ─ N_Bs タイムアウト(5 秒) ──→ IDLE (中断)
SEND_CF ─ CF 送信(MainFunction 毎) ─→ 完了 → IDLE
        ─ 送信失敗が続き N_As タイムアウト(1 秒) ─→ IDLE (中断)
```

**N_As タイムアウト（CF 送信、SWS_CanTp_00075 相当）**: 本実装は非同期の
TX 確認経路（`Can.c`/`CanIf.c`）が常に成功固定で真の送信失敗を表現できないため、
`CanTp_SendFrame()` の同期的な戻り値が失敗し続けた時間を計測し、
`CANTP_N_AS_TIMEOUT_MS`（既定 1000ms）を超えたらセッションを中断する。
これが無いと、バス障害等で CF 送信が失敗し続ける間 `CanTp_MainFunction()` が
無期限にリトライを続け、`CanTp_Transmit()` の busy 判定により新規送信が
永久に拒否され続けてしまう。SF/FF は送信失敗が同期的にその場で判明するため、
タイムアウトを待たず即座に中断・`E_NOT_OK` を返す。

##### フロー制御パラメータ（BS / STmin）

FC フレームの byte[1] が BS（Block Size）、byte[2] が STmin（Separation Time minimum）です。

| パラメータ | 本プロジェクト設定値 | 意味 |
|-----------|-------------------|------|
| **BS = 0** | `CANTP_BLOCK_SIZE 0U` | FF の後に FC を **1 回だけ** 送れば残り全 CF を連続送信してよい |
| BS = N (N≥1) | — | CF を N 枚送るごとに次の FC を待つ（本プロジェクトでは未使用） |
| STmin = 0 | `CANTP_ST_MIN 0U` | CF 間の最小待機時間なし |

**受信した STmin のデコード（`CanTp_DecodeStMin()`）**: 自 ECU が長い応答を CF に
分割送信する際、相手（テスター）から受け取る FC フレームの STmin バイトは
ISO 15765-2 の規約に従いデコードする。0x00-0x7F はそのまま ms。0xF1-0xF9 は
100-900µs 刻みだが、本実装は ms 分解能でしか CF 送信タイミングを制御できない
ため（`CanTp_MainFunction()` の駆動周期が 1ms）、追加待機なし（0ms）として扱う。
0x80-0xF0（予約値）は、誤った短い遅延で送信が詰まってしまわないよう安全側に
倒して 127ms（規格上の直接値レンジの最大値）にクランプする。

**BS=0 の動作イメージ:**

```
テスター                     Arduino
  │── FF ──────────────────→│   (FF 受信で FC 送信)
  │←─ FC (BS=0, STmin=0) ──│
  │                          │── CF1 ──→  ← FC 追加不要
  │                          │── CF2 ──→  ← FC 追加不要
  │                          │   ...
  │                          │── CFn ──→
```

CF1 と CF2 の間に追加 FC を送る必要はありません。
FC が 1 回届いた時点で「最後の CF まで送ってよい」という許可が出ているためです。

##### マルチフレーム応答例（2 DTC の場合）

2 件以上の DTC が一致すると応答が 8 バイトを超え、FF + CF に分割されます。

```
# 要求（SF）
送信 → 0x7E0: [03 19 02 FF 00 00 00 00]

# 応答 FF（総長 11 バイト = 3 ヘッダ + 2 DTC × 4 バイト）
受信 ← 0x7E8: [10 0B 59 02 2D 00 01 03]
               └──┘ └──────────────────┘
               FF    59=応答SID 02=subFunc 2D=availMask
               総長  00 01 03 = DTC1コード(ENGINE_SPEED_NO_FLAG)

# FC 送信（Cangaroo 等で手動送信 / 自動応答）
送信 → 0x7E0: [30 00 00 00 00 00 00 00]
               └┘ └┘ └┘
               FC  BS  STmin（すべて 0 = 即時全 CF 送信）

# 応答 CF（シーケンス番号 1）
受信 ← 0x7E8: [21 2C 00 01 04 2C 00 00]
               └┘ └┘ └──────┘ └┘
               CF SN=1        DTC2コード   DTC2ステータス
                  DTC1ステータス(ENGINE_SPEED_NO_FLAG=0x2C)
                              (STARTING_TIMEOUT=0x000104)
                                           (0x2C=FAILED_history)
```

**ステータス 0x2C（FAILED_history）:**
```
0x2C = 0b00101100
  bit5 (TFSLC) = 1  クリア後に一度は失敗した
  bit3 (CDTC)  = 1  確定済み（EEPROM 保存）
  bit2 (PDTC)  = 1  保留中
  bit0 (TF)    = 0  現在は失敗中でない（電源 OFF/ON 後にクリア）
```

##### Cangaroo で FC を手動送信する方法

Arduino が FF を送信すると WAIT_FC 状態になります。
5 秒以内に Cangaroo から FC を送信してください。

```
Plugins → RawSender で新しいフレームを作成:
  ID:   0x7E0
  Data: 30 00 00 00 00 00 00 00
        └┘ └┘ └┘
        FC CTS  BS=0  STmin=0ms
```

> **BS=0 のため、CF1・CF2 の間に追加 FC を送る必要はありません。**
> FC をこの 1 回送るだけで、Arduino は最後の CF まで連続して送信します。
> Cangaroo で CF1（`0x21`）が見えにくい場合は、CF1 と CF2 の間隔が約 25ms と短いためです
>（MCP2515 への SPI 通信 + TxConfirmation コールバックによる遅延）。

##### 0x2E WriteDataByIdentifier — 複数フレーム要求 (FF+CF) の実機検証

`CanTp_RxIndication()` の FF/CF 受信パス（複数フレーム**要求**の組立）は
実装こそされていましたが、対応する全 UDS サービスの要求が 7 バイト以内に
収まるため、これまで一度も実機を通っていませんでした。

DID 0x0104 (TestPattern, 固定 8 バイト) への書き込みは、要求ペイロードが
`SID(1) + DID(2) + data(8) = 11 バイト` となり SF の 7 バイト制限を超えるため、
このコードパスを実際に検証できる唯一の UDS サービスです。

```
要求: [0x2E, 0x01, 0x04, data0..data7]  (11 バイト、SF 不可)
  → FF: [0x10, 0x0B, 0x2E, 0x01, 0x04, data0, data1, data2]  (PCI=0x1, len=0x00B, 先頭6バイト)
  → ECU が FC(CTS, BS=0, STmin=0) を返す
  → CF: [0x21, data3, data4, data5, data6, data7, 00, 00]    (SN=1, 残り5バイト)
応答: [0x6E, 0x01, 0x04]  (SF, 3 バイト)
```

extendedSession かつ SecurityAccess Level1 アンロック済みでなければ NRC 0x33
（0x14 ClearDiagnosticInformation と同じ保護方針）。書き込んだ値は 0x22 で
DID 0x0104 を読み出すことで読み戻し確認できます（読み出し応答も
`0x62 + DID(2) + 8バイト = 11バイト` となり、こちらは既存の 0x19/04 等と同じ
応答方向の FF+CF で送られます）。

実際の車両データではなく、CanTp のトランスポート層を実機で確かめるための
学習用 DID です。

##### 0x2E WriteDataByIdentifier — CryptoKeyUpdate (DID 0x0108) による鍵更新

DID 0x0108 (CryptoKeyUpdate) は KeyM の鍵更新セッションを駆動する模擬鍵
マスターです。要求ペイロードは `keyName(1) + 新しい鍵16バイト = 17バイト`
（`SID(1)+DID(2)+17=20バイト`）で、TestPattern と同じく FF+CF の複数フレーム
要求になります。

```
要求: [0x2E, 0x01, 0x08, keyName, key0..key15]  (20 バイト、SF 不可)
応答: [0x6E, 0x01, 0x08]  (SF, 3 バイト)
```

`keyName` は `'1'`(0x31, ImmobilizerCmd 用) または `'2'`(0x32,
E2EHealthStatus 用) のみ有効。ECU 内部では `Dcm_HandleWriteDataById()` が
1回の呼び出し内で `KeyM_Start()`→`KeyM_Update()`→`KeyM_Finalize()` を実行し、
`Csm_KeyElementSet()`/`Csm_KeySetValid()` 経由で `Crypto.c` の RAM 鍵テーブル
を書き換えます。鍵は更新直後は無効化され、`KeyM_Finalize()`（＝この DID 書き込み
の完了）まで MAC 生成/検証には使われません。鍵材料は RAM のみで NVM に
永続化しないため、再起動すると `Crypto_PBCfg.c` の初期値に戻ります。

extendedSession かつ SecurityAccess Level1 アンロック済みでなければ NRC 0x33。
`keyName` 不一致・下位層の失敗時は NRC 0x31 (requestOutOfRange) を返します。

##### UDS ボタン送信ツール（tools/uds_tester）

セッション制御・SecurityAccess・複数フレーム応答の FC 送信など、手動操作する
項目が増えて Cangaroo での都度のフレーム手入力が煩雑になってきたため、
よく使う UDS コマンドをボタン 1 つで送信できる Python/Tkinter 製の補助ツールを
`tools/uds_tester/` に用意しています。

| 機能 | 説明 |
|------|------|
| ボタン送信 | `config.json` に定義した SF フレームをそのまま送信 |
| E2E P01 自動付加 | `config.json` の `"e2e"` フィールドを持つボタン（AbsInfo 等）は、Counter（0–15 のリングカウンタ）と CRC8 SAE J1850 を自動計算して付加。Counter は送信のたびにインクリメントされ、データ入力欄にもリアルタイム反映。入力欄の値を手動編集してから送信した場合はその値をそのまま送信（E2E バイトを上書きして送信 = 意図的な E2E エラーテストが可能） |
| 複数フレーム要求の送信 | `type: "multiframe"` のボタンは FF 送信 → ECU からの FC(CTS) 待ち → CF 送信、という ISO-TP 送信側を自前で実装（0x2E WriteDataByIdentifier 用） |
| 複数フレーム応答の自動 FC | 応答が FF で始まったら `30 00 00 00 00 00 00 00` を自動送信し、CF を再結合（上記の Cangaroo 手動 FC 送信が不要になる） |
| SecurityAccess Level1 自動実行 | requestSeed → `key = seed XOR 0xA55A`（`Dcm_ComputeSecurityKey()` と同一式）を計算 → sendKey を 1 クリックで実行 |
| 応答の簡易デコード | 0x22 の DID 値、0x19 の DTC 名・FreezeFrame、0x2F の controlOptionRecord 名・適用後レベル、0x31 の routineStatusRecord（実行中/PASS/FAIL）、0x7F の NRC 名を人間が読める形式で表示 |
| ランプ IOControl (0x2F) | RunLamp/FaultLamp/AbsLamp ごとに returnControlToECU / resetToDefault / freezeCurrentState / shortTermAdjustment(ON/OFF) をプリセットから送信 |
| RoutineControl (0x31) | EngineHealthCheck (RID 0203) の startRoutine / requestRoutineResults / stopRoutine をプリセットから送信 |
| Tester Present 自動送信 | チェックボックスで 2 秒毎に送信し S3 タイマ（60 秒）を維持 |
| Session/Security 状態の参考表示 | 送受信したフレームから推測した現在のセッション・ロック状態を表示（ECU 内部の正式な状態ではない点に注意） |

```
cd tools/uds_tester
pip install -r requirements.txt
python app.py
```

接続先は GUI 上の `interface` / `channel` / `bitrate` で指定します
（既定値は `config.json` の `can` セクション）。CANable / candleLight 互換
アダプタの場合は `interface=gs_usb`, `channel=0`。SLCAN 系の COM ポートアダプタ
の場合は `interface=slcan`, `channel=COM3` のように変更してください。

> **Cangaroo と同時に同じアダプタへ接続することはできません。** 干渉する場合は
> どちらか一方を切断してください。

ボタンの追加・変更はコードを触らず `config.json` の `buttons` 配列に項目を
追加するだけで行えます（本プロジェクトの `*_PBCfg.c` と同じ「コードと設定の分離」
の考え方です）。

###### CAPL 風スクリプト機能

ボタンの単発送信だけでは「セッション遷移→SecurityAccess→DID 読み出し」のような
複数手順の一連の操作や、応答内容による分岐を再現しにくいため、Vector CAPL に
近い書き味で一連の手順をスクリプトとして書ける機能を用意しています。

GUI の「スクリプト実行...」ボタンから `.py` ファイルを選択するとバックグラウンド
スレッドで実行されます（Connect 済みの `bus` をそのまま使用）。「停止」ボタンで
途中中断できます。スクリプトは Python 構文ですが、`tools/uds_tester/capl_api.py`
が公開する以下の関数だけを使えば CAPL に近い書き味で書けます。

| 関数 | 説明 |
|------|------|
| `send(payload)` | UDS 要求を送信（7 バイト以下は SF、超える場合は自動で FF+CF） |
| `send_can(can_id, data)` | 任意 CAN ID への生フレーム送信（応答待ちなし） |
| `wait_response(timeout=2.0)` | UDS 応答を待って返す（タイムアウト時はスクリプト中断） |
| `assert_positive(resp=None)` / `assert_negative(resp=None, nrc=None)` | 応答を検証し、不一致ならスクリプトを中断（`resp` 省略時は直前の `wait_response()` の結果を使う） |
| `security_unlock()` | SecurityAccess の seed→key 自動計算（ボタンの `security_access_auto` と同一処理） |
| `wait(seconds)` | 指定秒数待機（`@ctx.on_timer` を登録済みならその間もポーリングして発火させる） |
| `log(*args)` | GUI のログ欄に出力 |
| `@ctx.on_timer(interval_s)` | interval_s 秒毎に呼ばれる関数を登録するデコレータ（`wait()` の実行中のみ発火） |

サンプルは `tools/uds_tester/scripts/example_session_check.py` を参照してください。

<a id="nvm"></a>
#### NvM（Non-Volatile Memory Manager）

NvM は Dem が使う EEPROM ブロックを抽象化するモジュールです。
Dem は EEPROM アドレスを一切知らず、`NvM_BlockIdType`（NVM_BLOCK_ID_*）でのみ
ブロックを指定して `NvM_ReadBlock()` / `NvM_WriteBlock()` を呼び出します。

##### CRC によるデータ破損検出

EEPROM はビット化けや書き込み中の電源断で内容が壊れることがあります。
壊れたデータをそのまま信用すると、例えば Dem の経年回復カウンタ
(`NVM_BLOCK_ID_DEM_AGING`) が化けて閾値を超えた値になり、確定 DTC が誤って
早期に自動解除されてしまう、といった実害につながります。

これを検出するため、各ブロックはデータ本体直後の 1 バイトに
**AUTOSAR Crc8 (SAE J1850: 多項式 0x1D、初期値 0xFF、最終 XOR 0xFF)** を
付加して保存します。

```
NvM_Init()（起動時、各ブロックごとに）:
  MemIf_Read() 経由（実体は Fee）でデータ本体を RAM ミラーへロード
  storedCrc = MemIf_Read(BaseNumber + Length) から読み出し
  calcCrc   = RAM ミラーから再計算
  storedCrc == calcCrc ?
    YES → そのまま使用
    NO  → ERROR ログ + デフォルト値へ自動復元（後述）

NvM_WriteBlock() 呼び出し時:
  データを RAM ミラーへ即座に反映（同期）
  EEPROM への実書き込みは「保留」とマークするだけ（非同期、後述）

NvM_MainFunction()（周期呼び出し。ブロック/CRC/冗長化のオーケストレーションのみ）:
  保留中のブロックについて MemIf_Write() でジョブを開始 → MemIf_GetJobResult() で完了を待つ
  → データ本体 → CRC の順で 2 ジョブ完了させるとその面（プライマリ/ミラー）が完了

MemIf_MainFunction()（NvM_MainFunction とは独立に周期呼び出し。実体は Fee_MainFunction）:
  MemIf_Write() で開始されたジョブの物理バイト書き込みを 1 呼び出しにつき 1 バイトだけ進める
```

物理バイト単位の書き込みは NvM 自身ではなく MemIf（実体は Fee）の責務です。
詳細は後述の「非同期書き込みジョブキュー」を参照してください。

##### NvM_RestoreBlockDefaults — デフォルト値への復元

CRC 不一致を検出すると、ブロックごとに設定された **ROM デフォルト値**
（`NvM_PBCfg.c` の `NvM_BlockDescriptorType.RomBlockDataAddress`、未設定なら
全 0）を RAM ミラーへコピーし、CRC を付け直して EEPROM へ書き戻します。
この処理は `NvM_Init()` が破損検出時に内部的に呼ぶほか、
`NvM_RestoreBlockDefaults(BlockId)` として明示的にも呼び出せます
（AUTOSAR の `NvM_RestoreBlockDefaults()` 相当）。

| ブロック | デフォルト値 | 理由 |
|---|---|---|
| DEM_MAGIC | `0x00`（`DEM_NVM_MAGIC_BYTE`=0xDE とは異なる値） | MAGIC が破損から復元されても、Dem_Init() 自身の「マジック不一致 = 初回起動扱い」ロジックがそのまま働き、STATUS/AGING を含めて一貫した初期化になる |
| DEM_STATUS | 全イベント `DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR \| DEM_STATUS_NOT_COMPLETED_THIS_CYCLE` | Dem_Init() の初回起動時と同じ値。MAGIC は無事だが STATUS だけ破損したケースでも Dem が想定する初期状態と一致させる |
| DEM_AGING | 未設定（NULL）→ 全 0 で代替 | 経年回復カウンタの初回起動値はそもそも全イベント 0 であり、NvM の汎用フォールバックと完全に一致するため専用テーブル不要 |
| DEM_EXTENDED | 未設定（NULL）→ 全 0 で代替 | 故障確定回数 (ExtendedData) の初回起動値もそもそも全イベント 0 であり、同様に専用テーブル不要 |

DEM_STATUS のデフォルト値定義に `Dem_Cfg.h` の定数を使っているのは
`NvM_PBCfg.c` だけです。NvM 本体 (`NvM.c`) は Dem の存在を一切知りません
（PBCfg はモジュール間の配線を行うコンフィグ層であり、複数モジュールの
定義を参照してよい、という本プロジェクトの確立された方針）。

##### ログ例（EEPROM を直接書き換えて DEM_AGING ブロックを破損させた場合）

```
[19ms] ERROR NvM: block=2 CRC mismatch (stored=0xA3 calc=0x7F)
[19ms] WARN  NvM: block=2 defaults restored (zero-fill)
[19ms] INFO  NvM: Init ok blocks=4
```

##### 動作確認方法（CRC 不一致からの自動復元）

既存の UDS コマンドには EEPROM を直接破壊する手段がないため、`main.cpp` の
`setup()` 冒頭（`EcuM_Init()` 呼び出しより前）に動作確認用のコードを
一時的に追加します。本プロジェクトのビルド対象（Renesas RA）は `Fee_Hw.cpp`
と同じ `EEPROM.h`（Arduino 標準ライブラリ）の `EEPROM.write()`/`EEPROM.read()`
を使います。現在値との XOR で反転させているのは、たまたま元の値と同じ値を
書いてしまい「実は何も変化せず CRC 検査を素通りする」事故を避けるためです。

```c
#include <EEPROM.h>
// ...
EEPROM.write(0x0DU, EEPROM.read(0x0DU) ^ 0xFFU);  // DEM_AGING ブロックの先頭バイトを破壊
```

> **動作確認の前に**: 上記を追加して再アップロードすると、`NvM_Init()` が
> EEPROM を読み込む直前に DEM_AGING ブロックの先頭バイトを直接破壊します。
> 起動直後のシリアルログに `CRC mismatch` → `defaults restored` が出れば
> 動作確認完了です。**確認後は必ず追加したコードを削除して再アップロード**
> してください（破壊用コードを有効なまま運用すると毎回 DEM_AGING が
> リセットされてしまいます）。

##### 非同期書き込みジョブキュー（NvM ↔ MemIf ↔ Fee の責務分担）

**なぜ非同期化したか**: Renesas RA の EEPROM ライブラリ（内蔵フラッシュの
エミュレーション）は 1 バイトの書き込みでも消去・書き込みサイクルを伴うため、
9 バイト超のブロック（DEM_STATUS 等）をまとめて同期的に書くと数百 ms
協調スケジューラ全体が停止します。この停止を放置すると `Dem_ReportErrorStatus()`
が新規 DTC 確定のたびに WdgM の Deadline Supervision を巻き込んで HW
ウォッチドッグリセットを引き起こしうるため（経緯は
[DEVLOG](docs/DEVLOG.md#nvm-非同期書き込みジョブキューへの変更経緯) 参照）、
ブロッキングそのものを解消する非同期ジョブ方式を採用しています。

**NvM と MemIf/Fee の責務分担（重要）**: 当初はこの「1 回の呼び出しにつき
物理バイトを 1 個だけ書く」ジョブキューを NvM.c が自前で持っていましたが、
これは本来 AUTOSAR では Fee（さらにその下の MemIf 経由）が担うべき責務です。
NvM はブロック・CRC・冗長化という「意味」のレイヤーだけを扱い、実際の
バイト単位の物理書き込みの進行は Fee（MemIf 経由）に委譲するよう分離しました。

**設計**:

```
NvM_WriteBlock(id, data) / NvM_RestoreBlockDefaults(id):
  RAM ミラーを即座に更新（同期）
  該当ブロックを「保留」としてマークする（NvM_BlockPending[id] = 1）
  → 即座に E_OK を返す（書き込み完了を意味しない）

NvM_MainFunction()（10ms 周期、Os_PBCfg.c Task 12。ブロック単位のオーケストレーションのみ）:
  現在処理中のブロックがない ?
    YES → 保留中のブロックを FIFO キューの先頭から 1 つ取り出し処理開始
          （投入順。ブロック ID 昇順ではない — 下記参照）
    NO  → 処理を継続
  現在のフェーズ（データ本体 / CRC）のジョブが未開始 ?
    YES → MemIf_Write() でジョブを開始する（この呼び出し自体は即座に返る）
    NO  → MemIf_GetJobResult() で完了を待つだけ（MEMIF_JOB_PENDING ならこの tick は何もしない）
  データ本体ジョブが完了 → CRC ジョブを開始
  CRC ジョブも完了       → このブロックの保留フラグを下ろし、次のブロックへ

MemIf_MainFunction()（10ms 周期、Os_PBCfg.c Task 17。実体は Fee_MainFunction）:
  NvM_MainFunction() が MemIf_Write() で開始したジョブについて、
  1 回の呼び出しにつき未書き込みの 1 バイトだけを物理 EEPROM へ書く
```

Task 12（NvM）と Task 17（MemIf）はどちらも同じ 10ms 周期で、同一パス内を
インデックス昇順で実行されるため、NvM がジョブを開始した tick のうちに
MemIf 側の最初の 1 バイトも書かれます。最大 11 バイト（データ本体 10 バイト
+ CRC 1 バイト）のブロックでも、1 回の `MemIf_MainFunction()` 呼び出しで
ブロッキングするのは EEPROM 1 バイト分の書き込み時間のみです。DTC 確定から
永続化完了までは最大 11 サイクル（10ms 周期なので 110ms 程度、フェーズ切替
待ちの分だけ数 tick 余分にかかることがあります）かかりますが、Dem や
呼び出し元は結果を待たない fire-and-forget 設計（`(void)NvM_WriteBlock(...)`）
のため、この遅延は実用上問題になりません。

**ちぎれ書き対策**: 処理中のブロックに対して新たに `NvM_WriteBlock()` が
呼ばれた場合（例: 短時間に連続して DTC が確定した場合）、RAM ミラーは
最新値へ即座に上書きされる一方、`MemIf_Cancel()`（実体は `Fee_Cancel()`）で
進行中のジョブを中断し、書き込み位置はデータ本体フェーズの先頭へ巻き戻されます。
巻き戻さずに続行すると、EEPROM 上に「古いバイトと新しいバイトが混在した」
不整合な内容が残ってしまうためです。

**投入順 (FIFO) を維持する理由**: Dem は複数ブロックを連続して書く際、
書き込み順序そのものに電源断時の整合性を持たせている（例: 初回起動時の
STATUS→AGING→EXTENDED→MAGIC の順。有効性マーカーである MAGIC を最後に
書くことで、途中で電源が落ちてもマーカー不一致から「未初期化」と
再判定できる設計、[Dem.c](src/Bsw/Dem/Dem.c) 参照）。`NvM_MainFunction()`
がブロック ID 昇順で保留ブロックを拾ってしまうと、`NVM_BLOCK_ID_DEM_MAGIC=0`
が最小 ID のため、最後に投入したはずの MAGIC が真っ先に物理書き込みされ、
この整合性設計が壊れてしまう。そのため保留ブロックは ID 順ではなく、
投入順を記録した FIFO キューから取り出す。

**完了確認 API**: `NvM_GetErrorStatus(BlockId)` で `NVM_REQ_OK` /
`NVM_REQ_PENDING` / `NVM_REQ_NOT_OK` を取得できます（AUTOSAR
`NvM_GetErrorStatus()` 相当）。現状の呼び出し元はいずれも結果を確認しない
fire-and-forget ですが、API としては提供しています。

##### 冗長ブロック（Redundant Block）

CRC は「壊れたことを検出する」機能ですが、壊れてしまった後に救えるのは
ROM デフォルト値（多くの場合、全 0）だけです。`DEM_EXTENDED`（UDS SID 0x19/06
で読み出せる、車両生涯の故障確定回数）のように「デフォルト＝全 0 に戻る」
ことが実質的にデータ消失を意味するブロックにとって、CRC 不一致からの復元は
気休めにしかなりません。実 AUTOSAR の `NvMBlockManagementType=NVM_BLOCK_REDUNDANT`
（本実装では `NvM_BlockDescriptorType.Redundant`）は、同じデータを 2 か所の
EEPROM アドレス（プライマリ／ミラー）に保持することで、片方が破損しても
もう片方から復旧できるようにする機能です。

```
書き込み（NvM_MainFunction がオーケストレーションし、MemIf/Fee の非同期ジョブ経由で物理書き込み）:
  プライマリ面のデータ本体+CRC を完全に書き終える
    ↓（この時点で電源が落ちても、ミラー面はまだ触っていないので無傷）
  続けてミラー面のデータ本体+CRC を先頭から書く
    ↓（この時点で電源が落ちても、プライマリ面は既に新データで書き終わっている）
  両面完了 → ジョブ完了

読み込み（NvM_Init、起動時）:
  プライマリ面・ミラー面それぞれの CRC を検証
    両面とも正常   → プライマリの内容を採用
    片面のみ正常   → 正常な方の内容を採用し、破損した方をその内容で自己修復
    両面とも破損   → 通常のブロックと同様デフォルト値へ復元（両面に書く）
```

**なぜ「片方は必ず無傷」と言えるか**: プライマリとミラーを同時にではなく、
必ず順番に（プライマリを完全に書き終えてから初めてミラーに着手する形で）
書くためです。書き込みの途中で電源が落ちうるのはこの 2 つの完全な書き込みの
「どちらか一方」だけであり、もう一方は「まだ手つかず（＝直前の内容のまま
CRC 整合）」か「既に完了済み（＝新しい内容で CRC 整合）」のいずれかで、
中途半端な状態にはなりません。

**なぜ DEM_EXTENDED だけを冗長化したか**: DEM_STATUS（イベントステータス）や
DEM_AGING（経年回復カウンタ）は、たとえ CRC 不一致でデフォルト値へ戻っても、
その後の操作サイクル・エンジンサイクルの経過で内容が自然に再構築されていく
性質のデータです。一方 DEM_EXTENDED の故障確定回数は「車両の生涯を通じた
累積値」であり、一度失われた過去の回数は二度と再現できません。冗長化の
投資対効果（EEPROM 11 バイトの追加消費と書き込み時間 2 倍）が明確に
見合うのはこのブロックだけと判断しました。

**動作確認方法**: 前述の CRC 不一致確認と同様、`main.cpp` の `setup()`
冒頭（`EcuM_Init()` より前）に DEM_EXTENDED のプライマリ面（アドレス
0x18）を破壊するコードを一時的に追加して再アップロードします。

```c
#include <EEPROM.h>
// ...
EEPROM.write(0x18U, EEPROM.read(0x18U) ^ 0xFFU);  // DEM_EXTENDED プライマリ面の先頭バイトを破壊
```

`NvM: block=3 redundant: primary CRC mismatch, recovered from mirror`
のようなログとともに、故障確定回数が失われずにミラー面から復旧することを
確認できます。確認後は追加したコードを削除して再アップロードしてください
（前述の CRC 不一致確認と同じ理由）。

##### AUTOSAR 実装との主な違い (学習用簡略化)

- 冗長ブロック（ミラー/2重化）は `DEM_EXTENDED` のみで使用（ブロックごとに
  `NvM_BlockDescriptorType.Redundant` で選択可能。AUTOSAR の DATASET
  ブロック管理は未対応）
- Crc モジュールへの委譲なし（CRC8 計算を NvM.c 内に直接実装）
- ジョブは常に 1 個ずつ、投入順 (FIFO) に順次処理（優先度なし、複数ジョブの並行処理なし）
- `NvM_Init()` 自体（起動時の EEPROM 読み込み・CRC 不一致時の復元）は
  Os スケジューラ開始前のため同期処理のまま（他タスクを巻き込まないため無害）
- Fee は「論理ブロック番号 → 物理アドレス」を変換する FeeBlockConfiguration
  テーブルを持たない（呼び出し元が NvM 1 個のみで、かつ NvM_PBCfg.c が既に
  ブロックごとに一意な物理アドレスを静的に割り当てているため、追加の間接
  テーブルを設けても学習効果が薄いと判断。かわりに呼び出し元が物理アドレスを
  直接指定する）
- `MemIf_Init()`/`MemIf_MainFunction()` は実 AUTOSAR の SWS_MemIf には存在
  しない（[SWS_MemIf_00018]/[SWS_MemIf_00019] により、下位ドライバが 1 個
  しか構成されない場合 MemIf は単なるマクロ集合でよく、EcuM/Os が
  Fee_Init()/Fee_MainFunction() を直接呼ぶ設計になる）。本プロジェクトが
  あえてこの 2 関数を追加しているのは、Fee/MemIf 分割時点では AVR (Ea) と
  Renesas RA (Fee) の 2 プラットフォームに対応しており、プラットフォーム
  分岐を MemIf.c 1 箇所に閉じ込める設計だったため（現在は AVR/Ea 対応を
  削除し Renesas RA 専用だが、この構造はそのまま残している）
- `Fee_WriteImmediate()`（実仕様の `FeeImmediateData` ブロック属性相当）も
  同様に本プロジェクト独自の API

<a id="dem"></a>
#### DEM 診断イベント管理（AUTOSAR SWS_DEM）

Dem (Diagnostic Event Manager) モジュールがエンジン管理の故障を DTC として管理します。
DTC の永続化は NvM (Non-Volatile Memory Manager) 経由で行い、
Dem は EEPROM アドレスを直接知りません（NvM_WriteBlock / NvM_ReadBlock のみ使用）。
電源オフ後もクリア操作（SID 0x14）が行われない限り DTC が保持されます。

##### イベントと DTC コード

| EventId | イベント名 | 検出条件 | DTC コード |
|---------|-----------|---------|-----------|
| 0 | ENGINE_OVERHEAT | CoolantTemp ≥ 100 ℃（RUNNING 中） | 0x000101 |
| 1 | ENGINE_STALL | EngineSpeed < 100 rpm（RUNNING 中） | 0x000102 |
| 2 | ENGINE_SPEED_NO_FLAG | speed > 0 かつ flag = 0（OFF 中） | 0x000103 |
| 3 | STARTING_TIMEOUT | 起動から 5 秒超過（STARTING 中） | 0x000104 |
| 4 | COMM_TIMEOUT | EngineInfo 受信が 5 秒以上途絶（STARTING/RUNNING 中） | 0x000105 |
| 5 | BUTTON_STUCK | 警告確認ボタン（D9）が 5 秒以上押しっぱなし（IoHwAb 検出） | 0x000106 |
| 6 | ADC_VOLT_LOW | ADC センサ電圧（A0）が 1000mV 未満（IoHwAb が 10ms 周期で検出） | 0x000107 |
| 7 | CAN_BUSOFF | CAN Bus-Off が持続（L1 リトライ超過、L2 へ降格、CanSM が検出。回復試行自体は継続） | 0x000108 |
| 8 | E2E_ABSINFO | AbsInfo (CAN 0x110) の E2E エラー（CRC 不一致・カウンタ異常）（Rte 層の E2E Transformer が検出） | 0x000109 |
| 9 | E2E_ENGINEINFO | EngineInfo (CAN 0x100) の E2E エラー（CRC 不一致・カウンタ異常）（Rte 層の E2E Transformer が検出） | 0x00010A |

##### デバウンス (Counter-based Debouncing)

各イベントは `Dem_Cfg.h` の `DEM_DEBOUNCE_LIMIT_*` で**イベントごとに個別設定**する
デバウンスカウンタを持ちます（実車の `DemDebounceAlgorithmClass` — イベントごとに
別アルゴリズム/閾値を持てる — に相当）。FAILED 報告でカウンタ +1、PASSED 報告で -1
し（上下限でクランプ）、カウンタが **±limit に達した瞬間にのみ** DTC ステータス
（TF/PDTC/CDTC/TFSLC）を確定します。

```
FAILED 報告  → counter が負（確定 PASSED 側）なら 0 にリセットしてから ++  (上限 +limit でクランプ)
PASSED 報告  → counter が正（確定 FAILED 側）なら 0 にリセットしてから --  (下限 -limit でクランプ)

counter == +limit に達した瞬間のみ → 確定 FAILED（TF/PDTC/CDTC/TFSLC セット・FreezeFrame 記録・NvM 書込）
counter == -limit に達した瞬間のみ → 確定 PASSED（TF クリア。CDTC は SID 0x14 でのみクリア）
それ以外（中間値）                 → PRE-FAILED/PRE-PASSED。DTC ステータスは変更しない（DEBUG ログのみ）

カウンタが ±limit で飽和した後（既に確定済みの状態）に同じ方向の報告が続いても、
カウンタの値自体は変化しないため、このログは出力されない（毎サイクル報告するイベントでの
ログ多発を防ぐ）。
```

> **報告の方向が反転したら中立 (0) からやり直す理由**: 単純に counter++/-- だけだと、
> 既に確定 PASSED（counter=-limit）の状態から FAILED を 1 回報告しても counter は
> -limit+1 にしかならず、+limit に届くまで実質 2×limit 回分の反対方向の報告が必要に
> なってしまう。特に limit=1（BUTTON_STUCK / CAN_BUSOFF）ではこれが原因で確定に
> 到達できない不具合があったため、IoHwAb のボタンデバウンス（生レベルが確定値と
> 一致すればカウンタをリセットする）と同じ「割り込まれたら最初からやり直す」方式に
> 合わせている（経緯は [DEVLOG](docs/DEVLOG.md#dem-デバウンスカウンタの反転バグ) 参照）。

##### 閾値 (limit) の決め方

モニタ（報告元）が Dem に報告する前に、**既に十分な持続性チェックを行っているか**で
閾値を変えています。

| limit | 対象イベント | 理由 |
|---|---|---|
| 1（即確定） | BUTTON_STUCK, CAN_BUSOFF, E2E_ABSINFO, E2E_ENGINEINFO | IoHwAb の 5 秒固着判定／CanSM の 3 回リトライ後の断念は、それ自体が「十分粘った結果」。E2E は CRC 計算自体がエラー判定のため単発で確定。Dem 側で重ねてデバウンスすると二重チェックになり、確定が不必要に遅れる（または構造的に確定不可能になる） |
| 2（複数回要求） | ENGINE_OVERHEAT, ENGINE_STALL, ENGINE_SPEED_NO_FLAG, STARTING_TIMEOUT, COMM_TIMEOUT, ADC_VOLT_LOW | モニタは瞬時のしきい値超え（temp≥100 等）をそのまま報告するだけで、持続性チェックを行っていない。単発の誤検出で確定させないために Dem 側でデバウンスする |

> イベントごとの閾値にした経緯（当初は全イベント共通の単一閾値だった）は
> [DEVLOG](docs/DEVLOG.md#dem-デバウンス閾値を単一値からイベントごとに変更した経緯) を参照。

イベントごとの報告パターンによって、確定までにかかる実時間が異なります。

| イベント | 報告パターン | 確定までの目安 |
|---|---|---|
| ENGINE_OVERHEAT / STALL / SPEED_NO_FLAG / STARTING_TIMEOUT | 状態遷移の瞬間に単発報告（limit=2） | 同じ故障が**別々の機会に 2 回**発生する必要あり |
| COMM_TIMEOUT | 故障継続中は毎 Runnable サイクル（3000ms）報告（limit=2） | 2 サイクル分＝約 3000〜6000ms 追加 |
| ADC_VOLT_LOW | 故障継続中は毎 10ms サイクル報告（limit=2） | 数十 ms（実質的に即時） |
| BUTTON_STUCK / CAN_BUSOFF | 持続性チェック後に 1 回だけ報告（limit=1） | 即時確定 |

##### 複数 DTC を発生させる手順

各操作後は 3〜4 秒待ってシリアルモニタで状態遷移を確認してください（Runnable は 3 秒周期）。
フレーム表記は `<CAN ID>#<byte0>.<byte1>...`（Cangaroo 等の送信フォーマット）。

デバウンス（前述）により、ENGINE_OVERHEAT / ENGINE_STALL / ENGINE_SPEED_NO_FLAG / STARTING_TIMEOUT は
**同じ故障を別々の機会に 2 回**発生させないと DTC が確定（CDTC セット）しません。
1 回目は PRE-FAILED（カウンタ 1）に留まり、SID 0x19/02 にはまだ現れません。

| 順序 | 操作 | 状態 | デバウンス進行・登録 DTC |
|-----|------|------|------------------------|
| 1 | `100#01.F4.19.00` 送信（speed=500, flag=0） | OFF→FAULT | SPEED_NO_FLAG: cnt 0→1（PRE-FAILED） |
| 2 | `100#00.00.19.00` 送信（flag=0） | FAULT→OFF | — |
| 3 | `100#01.F4.19.00` 再送信（speed=500, flag=0） | OFF→FAULT | SPEED_NO_FLAG: cnt 1→2 → **確定** |
| 4 | `100#00.00.19.00` 送信（flag=0） | FAULT→OFF | — |
| 5 | `100#00.64.19.80` 送信（speed=100, flag=1）→ 6 秒待つ | OFF→STARTING→FAULT | STARTING_TIMEOUT: cnt 0→1（PRE-FAILED） |
| 6 | `100#00.00.19.00` 送信（flag=0） | FAULT→OFF | — |
| 7 | `100#00.64.19.80` 再送信（speed=100, flag=1）→ 6 秒待つ | OFF→STARTING→FAULT | STARTING_TIMEOUT: cnt 1→2 → **確定** |
| 8 | `100#00.00.19.00` 送信（flag=0） | FAULT→OFF | — |
| 9 | `100#03.E8.19.80` 送信（speed=1000, flag=1） | OFF→STARTING→RUNNING | — |
| 10 | EngineInfo の送信を止めて 8〜11 秒待つ（Runnable 周期との位相次第） | RUNNING→FAULT | COMM_TIMEOUT: 毎サイクル報告のため 2 サイクル目で自然に**確定** |
| 11 | `100#00.00.00.00` 送信（flag=0, speed=0）で復帰→ OFF へ | FAULT→OFF | — |
| 12 | `100#03.E8.19.80` 送信（speed=1000, flag=1） | OFF→STARTING→RUNNING | — |
| 13 | `100#03.E8.64.80` 送信（temp=100, flag=1） | RUNNING→FAULT | ENGINE_OVERHEAT: cnt 0→1（PRE-FAILED） |
| 14 | `100#00.00.19.00` 送信（flag=0） | FAULT→OFF | — |
| 15 | `100#03.E8.19.80` 送信（speed=1000, flag=1） | OFF→STARTING→RUNNING | — |
| 16 | `100#03.E8.64.80` 再送信（temp=100, flag=1） | RUNNING→FAULT | ENGINE_OVERHEAT: cnt 1→2 → **確定** |
| 17 | `100#00.00.19.00` 送信（flag=0） | FAULT→OFF | — |
| 18 | `100#03.E8.19.80` 送信（speed=1000, flag=1） | OFF→STARTING→RUNNING | — |
| 19 | `100#00.32.19.80` 送信（speed=50, flag=1） | RUNNING→FAULT | ENGINE_STALL: cnt 0→1（PRE-FAILED） |
| 20 | `100#00.00.19.00` 送信（flag=0） | FAULT→OFF | — |
| 21 | `100#03.E8.19.80` 送信（speed=1000, flag=1） | OFF→STARTING→RUNNING | — |
| 22 | `100#00.32.19.80` 再送信（speed=50, flag=1） | RUNNING→FAULT | ENGINE_STALL: cnt 1→2 → **確定** |

##### ABS LED 動作確認手順

RUNNING 状態で以下の AbsInfo フレームを 0x110 で送信して LED 動作を確認します。

AbsInfo は E2E P01 保護付きのため、**Counter（byte[3] 下位 4bit）と CRC8（byte[4]）を正しく付加**しないと
Com が E2E エラーと判定してフレームを破棄し、LED は反応しません。
uds_tester の「AbsInfo (0x110)」ボタンは Counter と CRC を自動計算して送信します。

| シグナル設定 | AbsActive | BrakeActive | D6 RUNNING | D7 FAULT | D8 ABS |
|-------------|-----------|-------------|:----------:|:--------:|:------:|
| `data: [0x27, 0x10, 0x00]` | 0 | 0 | 点灯 | 消灯 | 消灯 |
| `data: [0x27, 0x10, 0x40]` | 0 | 1 | 点灯 | 消灯 | 消灯（BrakeActive は LED に影響しない） |
| `data: [0x27, 0x10, 0xC0]` | 1 | 1 | 点灯 | 消灯 | **点灯** |
| `data: [0x27, 0x10, 0x80]` | 1 | 0 | 点灯 | 消灯 | **点灯** |

FAULT 状態で AbsActive=1 のフレームを送信すると、D7 が点滅しつつ D8 も同時に点灯します（3 LED は独立制御）。

##### DTC ステータスバイト（ISO 14229-1 Annex B）

SID 0x19 の応答に含まれるステータスバイトの各ビットの意味。

| ビット | マスク | 略称 | 意味 |
|-------|--------|------|------|
| bit0 | 0x01 | TF | testFailed — 今現在壊れている |
| bit2 | 0x04 | PDTC | pendingDTC — 今の電源サイクルで失敗した |
| bit3 | 0x08 | CDTC | confirmedDTC — 確定済み・EEPROM 保存済み |
| bit4 | 0x10 | TNCLC | testNotCompletedSinceLastClear — クリア後未テスト |
| bit5 | 0x20 | TFSLC | testFailedSinceLastClear — クリア後に失敗あり |

statusAvailabilityMask = **0x2D**（本実装がサポートするビットの OR）。

##### DTC ライフサイクル

| フェーズ | TF | PDTC | CDTC | TFSLC | TNCLC | ステータス値 |
|---------|:--:|:----:|:----:|:-----:|:-----:|:-----------:|
| 初回起動（EEPROM 未初期化） | 0 | 0 | 0 | 0 | **1** | `0x10` |
| PASSED 報告 2 回でデバウンス確定 | 0 | 0 | 0 | 0 | 0 | `0x00` |
| FAILED 報告 1 回目（PRE-FAILED, 未確定） | 0 | 0 | 0 | 0 | 0 | `0x00`（変化なし） |
| **FAILED 報告 2 回目でデバウンス確定** | **1** | **1** | **1** | **1** | 0 | **`0x2D`** |
| 電源再投入後（TF のみリセット。故障はこのサイクル中に起きたため PDTC はまだクリアされない） | **0** | 1 | **1** | 1 | 0 | **`0x2C`** |
| クリーンな操作サイクルを 1 回経過（**PDTC 自動クリア**） | 0 | **0** | 1 | 1 | 0 | `0x28` |
| さらにクリーンな操作サイクルを経過し経年回復完了（**CDTC 自動クリア**） | 0 | 0 | **0** | 1 | 0 | `0x20` |
| SID 0x14 実行後 | 0 | 0 | 0 | **0** | **1** | `0x10` |

> デバウンスカウンタ自体は RAM のみで保持するため、電源再投入時に中立 (0) へリセットされます。
> PRE-FAILED の途中で電源が切れた場合、その「あと1回」の進行はリセットされます。

- **CDTC（bit3）が永続化の本体**。電源再投入後も保持されるため、整備ツールで過去の故障を確認できる。
- TF（bit0）は電源再投入時にクリア。「今は動いているが過去に壊れた」を表現できる。
- **PDTC（bit2）は CDTC より早く消える**: SWS_Dem_00390 (Figure 7.19) 準拠で、
  「そのサイクル中に FAILED 確定なし・テスト済み」というクリーンな操作サイクルを
  **1 回**経過するだけで自動クリアされる（`Dem_EvaluatePendingClear()`、`Dem_Init()` から
  呼び出し）。CDTC のような複数サイクルのエージングカウンタは介さない。
- CDTC を消すには SID 0x14 による明示的なクリア、または経年回復（再故障せず複数回の操作サイクルを経過）のいずれか。

##### フレーム例（DTC 操作）

**DTC 件数を確認（confirmedDTC のみ = statusMask 0x08）:**
```
送信 → 0x7E0: [03 19 01 08 00 00 00 00]
受信 ← 0x7E8: [06 59 01 2D 01 00 NN 00]
                                   ↑ byte[5] が DTC 件数
```

**DTC 一覧を取得（全ステータス = statusMask 0xFF）:**

1 件の場合（SF 応答）:
```
送信 → 0x7E0: [03 19 02 FF 00 00 00 00]
受信 ← 0x7E8: [07 59 02 2D D1 D2 D3 SS]
                            └────────┘ └── byte[7]: DTC ステータス
                            byte[4-6]: DTC コード (例: 00 01 01 = EngineOverheat)
```

2 件以上の場合（マルチフレーム応答 → FC 要）:
```
送信 → 0x7E0: [03 19 02 FF 00 00 00 00]
受信 ← 0x7E8: [10 0B 59 02 2D D1 D2 D3]  FF（総長 0x0B=11 バイト）
送信 → 0x7E0: [30 00 00 00 00 00 00 00]  FC(CTS)
受信 ← 0x7E8: [21 SS D1 D2 D3 SS 00 00]  CF（残りの DTC）
```

**全 DTC クリア:**
```
送信 → 0x7E0: [04 14 FF FF FF 00 00 00]
受信 ← 0x7E8: [01 54 00 00 00 00 00 00]
```

**特定 DTC のみクリア（groupOfDTC に DTC コードを指定）:**

ENGINE_OVERHEAT（DTC 0x000101）だけをクリアする例:
```
送信 → 0x7E0: [04 14 00 01 01 00 00 00]
受信 ← 0x7E8: [01 54 00 00 00 00 00 00]
```
内部では `Dem_GetEventIdOfDTC(0x000101, &eventId)` で該当イベントを逆引きし、
`Dem_ClearDTC(eventId)` でそのイベントだけをステータス・デバウンスカウンタ・
FreezeFrame ともに未記録状態へ戻す（他の DTC には影響しない）。
一致する DTC が存在しない場合は NRC 0x31（requestOutOfRange）を返す。

##### PendingDTC の自動クリア

PDTC（pendingDTC, bit2）は SID 0x14 によるクリア以外に、**クリーンな操作サイクルを
1 回経過するだけで自動的に解除される**仕組みを持っています（SWS_Dem_00390,
Figure 7.19）。CDTC（confirmedDTC）が複数サイクルのエージングカウンタを介して
徐々に回復するのに対し、PDTC は「直前の操作サイクル中に一度も FAILED 確定が
無く、かつテスト済みだった」という条件さえ満たせば即座にクリアされる、より
軽量な保留フラグです。

判定は `Dem_Init()` が起動ごとに「直前の操作サイクルの最終状態」を見て行います
（`Dem_EvaluatePendingClear()`、経年回復の評価と同じタイミング・同じ理由で
TF/TFTOC/TNCTC を新サイクル用にリセットする直前の値を見る必要があります）。

```
Dem_Init()（起動時、TF/TFTOC/TNCTC を新サイクル用にリセットする直前）:
  PDTC=0                          → 対象外
  PDTC=1 かつ TFTOC=0 かつ TNCTC=0 → 「クリーンな操作サイクル」として即座に PDTC クリア
  PDTC=1 かつ (TFTOC=1 または TNCTC=1) → クリアしない（このサイクル中に FAILED 確定
                                          があった、またはテストされなかった）
```

**ログ例（ENGINE_OVERHEAT が FAILED 確定した翌サイクルが PASSED でクリーンだった場合）：**
```
[60ms] INFO  Dem: ev=0 pendingDTC cleared (clean operation cycle)
```

##### 経年回復（Aging）

CDTC（confirmedDTC）は SID 0x14 によるクリアだけでなく、**再故障せずに複数回の
操作サイクル（起動〜次回起動）を経過すると自動的に解除される**仕組みも持っています。
「一度故障した部品は永久にDTCが残り続ける」のではなく「故障が再発しなければ
時間とともに記録が薄れていく」という、実車の診断システムが持つ考え方です。

判定は `Dem_Init()` が起動ごとに「直前の操作サイクルの最終状態」を見て行います。

```
Dem_Init()（起動時、TF/TFTOC/TNCTC を新サイクル用にリセットする直前）:
  CDTC=0                        → エージング対象外、カウンタ=0
  CDTC=1 かつ TF=1（再故障）      → 連続性が途切れたためカウンタ=0
  CDTC=1 かつ TNCTC=1（未テスト）  → このサイクルは数えない（カウンタ維持）
  CDTC=1 かつ TF=0 かつ TNCTC=0   → 「クリーンな操作サイクル」としてカウンタ+1
    → Dem_AgingThresholdTable[EventId]（イベントごとの閾値）に達したら CDTC を自動クリア
```

カウンタは NvM (`NVM_BLOCK_ID_DEM_AGING`) で永続化するため、電源を切っても進行度が
失われません。実車では数十サイクル単位が一般的ですが、本プロジェクトでは電源の
再投入を数回行うだけで動作確認できるよう小さい値にしています。

回復のしやすさはデバウンス閾値と同様にイベントごとに個別設定します
（`Dem_Cfg.h` の `DEM_AGING_THRESHOLD_*`）。重大・誤回復のリスクが大きいイベント
ほど大きく（回復に時間がかかる）、一過性の可能性が高いイベントほど小さく
（早く回復する）設定しています。

| イベント | 閾値 | 理由 |
|---------|:---:|------|
| ENGINE_OVERHEAT / ENGINE_STALL / CAN_BUSOFF | 5 | 重大故障・通信路の重大故障。誤って早期回復しないよう慎重に |
| ENGINE_SPEED_NO_FLAG / COMM_TIMEOUT / BUTTON_STUCK / ADC_VOLT_LOW / E2E_ABSINFO / E2E_ENGINEINFO | 3 | 標準 |
| STARTING_TIMEOUT | 2 | 起動時の一過性要因の可能性が高い |

**ログ例（ENGINE_OVERHEAT は閾値 5。再故障せず 5 回起動した場合）：**
```
# 1～4 回目の再起動（FAILED 確定済み、直前サイクルはクリーン）
[60ms] INFO  Dem: ev=0 aging=1/5
[60ms] INFO  Dem: ev=0 aging=2/5
[60ms] INFO  Dem: ev=0 aging=3/5
[60ms] INFO  Dem: ev=0 aging=4/5

# 5 回目の再起動 → 経年回復完了、CDTC が自動クリア
[60ms] INFO  Dem: ev=0 healed (aging complete) dtc=0x000101

# もし途中の起動で再度 FAILED が確定していたら
[60ms] INFO  Dem: ev=0 aging reset (re-failed)
```

##### EEPROM レイアウト

Arduino UNO の内蔵 EEPROM 先頭 46 バイトを使用します（NvM の CRC バイトを含む。
詳細は後述の「NvM（Non-Volatile Memory Manager）」参照）。
アドレス割り当ては NvM_Cfg.h (`NVM_BLOCK_DEM_*_EEPROM_ADDR`) で一元管理しています。
Dem は NvM_BlockIdType (NVM_BLOCK_ID_DEM_MAGIC / _DEM_STATUS / _DEM_AGING / _DEM_EXTENDED)
でのみアクセスします。

| アドレス | NvM ブロック | 内容 |
|---------|-------------|------|
| 0x00 | NVM_BLOCK_ID_DEM_MAGIC (1 byte) | マジックバイト（0xDE = 有効データあり） |
| 0x01 | 〃 CRC (1 byte) | MAGIC ブロックの CRC8 |
| 0x02 | NVM_BLOCK_ID_DEM_STATUS (10 bytes) | EVENT_ENGINE_OVERHEAT ステータス |
| 0x03 | 〃 | EVENT_ENGINE_STALL ステータス |
| 0x04 | 〃 | EVENT_ENGINE_SPEED_NO_FLAG ステータス |
| 0x05 | 〃 | EVENT_STARTING_TIMEOUT ステータス |
| 0x06 | 〃 | EVENT_COMM_TIMEOUT ステータス |
| 0x07 | 〃 | EVENT_BUTTON_STUCK ステータス |
| 0x08 | 〃 | EVENT_ADC_VOLT_LOW ステータス |
| 0x09 | 〃 | EVENT_CAN_BUSOFF ステータス |
| 0x0A | 〃 | EVENT_E2E_ABSINFO ステータス |
| 0x0B | 〃 | EVENT_E2E_ENGINEINFO ステータス |
| 0x0C | 〃 CRC (1 byte) | STATUS ブロックの CRC8 |
| 0x0D | NVM_BLOCK_ID_DEM_AGING (10 bytes) | EVENT_ENGINE_OVERHEAT 経年回復カウンタ |
| 0x0E | 〃 | EVENT_ENGINE_STALL 経年回復カウンタ |
| 0x0F | 〃 | EVENT_ENGINE_SPEED_NO_FLAG 経年回復カウンタ |
| 0x10 | 〃 | EVENT_STARTING_TIMEOUT 経年回復カウンタ |
| 0x11 | 〃 | EVENT_COMM_TIMEOUT 経年回復カウンタ |
| 0x12 | 〃 | EVENT_BUTTON_STUCK 経年回復カウンタ |
| 0x13 | 〃 | EVENT_ADC_VOLT_LOW 経年回復カウンタ |
| 0x14 | 〃 | EVENT_CAN_BUSOFF 経年回復カウンタ |
| 0x15 | 〃 | EVENT_E2E_ABSINFO 経年回復カウンタ |
| 0x16 | 〃 | EVENT_E2E_ENGINEINFO 経年回復カウンタ |
| 0x17 | 〃 CRC (1 byte) | AGING ブロックの CRC8 |
| 0x18 | NVM_BLOCK_ID_DEM_EXTENDED (10 bytes) | EVENT_ENGINE_OVERHEAT 故障確定回数 |
| 0x19 | 〃 | EVENT_ENGINE_STALL 故障確定回数 |
| 0x1A | 〃 | EVENT_ENGINE_SPEED_NO_FLAG 故障確定回数 |
| 0x1B | 〃 | EVENT_STARTING_TIMEOUT 故障確定回数 |
| 0x1C | 〃 | EVENT_COMM_TIMEOUT 故障確定回数 |
| 0x1D | 〃 | EVENT_BUTTON_STUCK 故障確定回数 |
| 0x1E | 〃 | EVENT_ADC_VOLT_LOW 故障確定回数 |
| 0x1F | 〃 | EVENT_CAN_BUSOFF 故障確定回数 |
| 0x20 | 〃 | EVENT_E2E_ABSINFO 故障確定回数 |
| 0x21 | 〃 | EVENT_E2E_ENGINEINFO 故障確定回数 |
| 0x22 | 〃 CRC (1 byte) | EXTENDED ブロックの CRC8（プライマリ面） |
| 0x23 | NVM_BLOCK_ID_DEM_EXTENDED ミラー面 (10 bytes) | 故障確定回数（プライマリと同一内容の 2 面目、冗長ブロック） |
| 0x2D | 〃 CRC (1 byte) | EXTENDED ブロックの CRC8（ミラー面） |

DEM_EXTENDED のみ冗長ブロック（2 面化）にしています。理由・仕組みは後述の
「NvM（Non-Volatile Memory Manager）」の「冗長ブロック」参照。

<a id="freezeframe"></a>
##### FreezeFrame（故障時スナップショット）

DTC が FAILED に遷移した瞬間の車両状態（EngineSpeed / CoolantTemp / EngineState）を Dem が記録し、
UDS SID 0x19 subFunc 0x04（reportDTCSnapshotRecordByDTCNumber）で読み出せます。
本実装は **RAM のみに保持**し EEPROM へは永続化しません（電源 OFF で消去される学習用簡略化）。
イベントごとに保持するレコードは 1 件（recordNumber=0x01）のみです。

###### 記録の仕組み

```
App_EngineManager_Run（3000ms 周期、毎回呼ばれる）:
  speed/temp/flag を RTE から読み取った直後に
  Dem_SetFreezeFrameContext(speed, temp, s_state)
    → Dem_CurrentContext を更新するだけ（まだイベントには紐付かない）

Dem_ReportErrorStatus(EventId, FAILED) が呼ばれ、ステータスが変化した場合のみ:
  Dem_FreezeFrameTable[EventId] = Dem_CurrentContext   ← この瞬間のスナップショットを確定
  Dem_FreezeFrameValid[EventId] = 1

すでに FAILED 中の再報告（status == prev）はスナップショットを上書きしない。
→ 「最初に故障した瞬間」の値が保持される。
```

ボタン固着（BUTTON_STUCK）や ADC 電圧低下（ADC_VOLT_LOW）など、エンジン状態と直接関係しないイベントでも、
その時点の `Dem_CurrentContext`（直近の Runnable サイクルでの車両状態）がスナップショットされます。
これは実車 OBD-II の FreezeFrame が「DTC 固有のデータ」ではなく「DTC 検出時点の車両全体のスナップショット」を
記録する考え方と同じです。

###### フレーム例（SID 0x19/04）

ENGINE_OVERHEAT（DTC 0x000101）が温度 101℃・回転数 1000rpm・RUNNING 中に FAILED した場合:

```
# 要求: [19 04 DTC_H DTC_M DTC_L recordNumber]
送信 → 0x7E0: [06 19 04 00 01 01 01 00]

# 応答 18 バイト(0x12) → FF + CF×2 に分割（CanTp の CF は 7 データバイト固定、不足分は 0x00 パディング）
受信 ← 0x7E8: [10 12 59 04 00 01 01 2D]   FF（総長 0x12=18 バイト）
               └──┘ └──────────────────┘
               FF    59=応答SID 04=subFunc
               総長  00 01 01=DTC  2D=status

送信 → 0x7E0: [30 00 00 00 00 00 00 00]   FC(CTS)

受信 ← 0x7E8: [21 01 03 01 01 03 E8 01]   CF（SN=1）
               └┘ └┘ └┘ └───┘ └───┘ └┘
               CF recNo=1 numDID=3 DID1=0x0101 EngineSpeed=0x03E8(=1000) DID2_H=01(続く)

受信 ← 0x7E8: [22 02 65 01 03 02 00 00]   CF（SN=2、末尾 2 バイトは 0 パディング）
               └┘ └┘ └┘ └───┘ └┘
               CF DID2_L=02 CoolantTemp=0x65(=101) DID3=0x0103 EngineState=0x02(RUNNING)
```

未記録（一度も FAILED していない DTC）またはレコード番号不一致の場合は NRC 0x31
（requestOutOfRange）で応答します。

<a id="extendeddata"></a>
##### ExtendedData（故障確定回数）

FreezeFrame が「故障した瞬間の車両状態のスナップショット」（1 件のみ、上書きされる）
であるのに対し、ExtendedData は「これまでに何回確定 FAILED したか」を表す
**累積カウンタ**です。UDS SID 0x19 subFunc 0x06（reportExtendedDataRecordByDTCNumber）
で読み出せます。

| 観点 | FreezeFrame (subFunc 0x04) | ExtendedData (subFunc 0x06) |
|---|---|---|
| 内容 | 故障時点の車両状態（3 DID） | 確定 FAILED の累積回数（1 バイト） |
| 更新タイミング | 最初の確定 FAILED 時のみ（再報告では上書きしない） | 確定 FAILED の度に +1（0xFF で飽和） |
| 永続化 | RAM のみ（電源 OFF で消去） | NvM 経由で EEPROM に永続化 |
| SID 0x14 クリア時 | 未記録状態に戻る | 0 にリセット（経年回復カウンタと同じ扱い） |
| 応答サイズ | 18 バイト（CanTp が FF+CF に分割） | 8 バイト（SF で収まる） |

###### 記録の仕組み

```
Dem_ReportErrorStatus(EventId, FAILED) が呼ばれ、確定 FAILED に遷移した瞬間
（FreezeFrame 更新と同じ箇所）に:
  Dem_OccurrenceCounter[EventId]++   （0xFF で飽和、それ以上は増えない）
  NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter)

SID 0x14（全クリア／DTC 指定クリア）で対象イベントの Dem_OccurrenceCounter も 0 に戻る
（CDTC 自体や経年回復カウンタとは独立した値だが、クリア操作のタイミングは共通）。
```

###### フレーム例（SID 0x19/06）

ENGINE_OVERHEAT（DTC 0x000101）がこれまでに 3 回確定 FAILED した場合:

```
# 要求: [19 06 DTC_H DTC_M DTC_L recordNumber]
送信 → 0x7E0: [06 19 06 00 01 01 01 00]

# 応答 8 バイト → SF で収まる（FreezeFrame と異なり複数フレーム化不要）
受信 ← 0x7E8: [07 59 06 00 01 01 2D 01 03]
               └┘ └──────────────────┘ └┘ └┘
               SF 59=応答SID 06=subFunc   recNo=1
                  00 01 01=DTC 2D=status      occurrenceCounter=3
```

未記録（一度も FAILED していない DTC は occurrenceCounter=0 を返す。FreezeFrame と異なり
「未記録」という特別な NRC にはならない）またはレコード番号不一致の場合は NRC 0x31
（requestOutOfRange）で応答します。

<a id="fim"></a>
#### FiM（機能抑止マネージャ）

FiM (Function Inhibition Manager) は、Dem が確定（CONFIRMED）した DTC を根拠に、
関連するアプリ機能の実行を抑止するルールエンジンです。
「DTC を記録する」（Dem の責務）と「DTC を理由に機能を止める」（FiM の責務）を
分離するのが AUTOSAR の設計思想で、ASW は Dem の内部実装を一切知らずに
`Rte_Call_FiM_GetFunctionPermission()` だけで「この機能は今実行してよいか」を判定できます。

##### 機能 ID (FID) とイベントの対応

| FID | 機能 | 抑止条件 | 抑止時の挙動 |
|---|---|---|---|
| `FIM_FID_RUNNING_LED` | RUNNING LED (D6) の点灯 | `DEM_EVENT_CAN_BUSOFF` が CONFIRMED | D6 を強制消灯（EngineState は CAN 受信由来のため、Bus-Off 確定中は信頼できない） |
| `FIM_FID_BUTTON_ACK` | 警告確認ボタンによる FAULT 解除 | `DEM_EVENT_BUTTON_STUCK` が CONFIRMED | ボタン押下を無視（固着確定中の押下は物理的固着による偽信号の可能性がある） |

対応表は `FiM_PBCfg.c` の `FiM_Functions[]` で定義する（AUTOSAR の `FiMFunction` コンテナに相当）。
新しい FID を追加する場合は、ここに 1 行追加するだけで済む。

##### 判定の流れ

```
FiM_MainFunction（100 ms 周期、Os Task 9）:
  FiM_Functions[] を先頭から走査:
    status = Dem_GetStatusOfEvent(EventId)
    (status & InhibitStatusMask) != 0 ?
      YES → 該当 FID を「抑止」
      NO  → 該当 FID を「許可」
    （許可状態が変化した瞬間にのみログ出力）

ASW (App_WarningIndicator_Run / App_EngineManager_Run):
  Rte_Call_FiM_GetFunctionPermission(FID, &status)
  status == 0 (抑止) なら、当該機能の実行を見送る
```

FiM は Dem の状態だけを参照し、ASW は FiM の判定結果だけを参照します。
ASW が Dem を直接参照しないことで、「どの DTC が確定したら何を止めるか」という
ルールを FiM 側に閉じ込め、ASW のロジックを単純に保てます。

##### ログ例

```
# CAN Bus-Off が確定（3 回のリトライ断念）→ RUNNING LED が抑止される
[30313ms] WARN  Dem: FAILED ev=7 dtc=0x000108
[30400ms] WARN  FiM: FID0 inhibited (ev=7 status=0x2D)
[30900ms] INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]   # state=RUNNING でも D6 は消灯のまま

# UDS 0x14 で全 DTC クリア → 抑止解除
[31000ms] INFO  Dcm: 14 ClearAllDTC
[31100ms] INFO  FiM: FID0 permitted again
[31600ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:0]   # state=RUNNING なら D6 が再点灯

# 警告確認ボタンが 5 秒以上押されたまま固着確定 → FAULT 解除ボタンが無効化
[40000ms] WARN  IoHwAb: Button stuck dtc=0x000106
[40100ms] WARN  FiM: FID1 inhibited (ev=5 status=0x2D)
[40500ms] WARN  AppEng: FAULT->OFF btn=1 inhibited (FiM)   # 押下を受理しない
```

##### 呼び出し側（ASW）のフェールセーフ既定値

`FiM_GetFunctionPermission()` 自体は、FID が不正・FiM 未初期化などで判定できない
場合に `Status` を安全側（0 = 抑止）にしてから `E_NOT_OK` を返す契約になっています
（`FiM.h` 参照）。

ASW 側（`App_EngineManager_Run` / `App_WarningIndicator_Run`）の呼び出しも、
この契約に依存しきらず、呼び出し前のローカル変数の既定値そのものを
`0`（抑止）にし、戻り値が `E_NOT_OK` の場合も明示的に `0` へ上書きしています。

```c
uint8 ackPermitted = 0U;  /* 既定値は「抑止」(許可ではない) */
if (Rte_Call_FiM_GetFunctionPermission(FIM_FID_BUTTON_ACK, &ackPermitted) != E_OK)
{
    ackPermitted = 0U;
}
```

呼び出し先の実装が将来変わって `Status` を書き込まない失敗経路が増えても、
呼び出し側のローカルな既定値だけで安全側に倒れる（fail-safe）ようにする狙いです。
「許可判定が確認できないときは許可しない」という既定値の選び方は、
セキュリティ・機能安全の定石（fail-safe defaults）そのものです。

<a id="ecu-management"></a>
### ECU 管理層（EcuM / BswM / WdgM / ComM / CanSM / Nm）

ECU の起動・シャットダウンのライフサイクルと、タスク制御・ソフトウェア監視を担うモジュール群です。
EcuM が状態遷移を決定し、BswM がその状態に応じたタスクの有効・無効を制御し、WdgM がタスク内部の動作を監視します。

| 層 | モジュール | 本プロジェクトでの役割 |
|---|---|---|
| BSW | EcuM | ECU ライフサイクルを STARTUP → RUN → POST_RUN → SHUTDOWN の状態マシンで管理。`EcuM_RequestRUN` / `EcuM_ReleaseRUN` で RUN フェーズを調停。SHUTDOWN は CAN バスのウェイクアップにより常に RUN へ復帰可能（実機リセットが必要な終端状態は存在しない） |
|  | BswM | EcuM / ComM のモード変化をルールテーブルで受け取り `Os_SetTaskActive()` でタスクを有効・無効化するルールエンジン。POST_RUN 中はアプリタスクのみ停止し BSW タスクは継続。`BswMPduGroupSwitch`（[SWS_BswM_00273]）相当のアクションも持ち、RUN/POST_RUN で Com の「テレメトリ」I-PDU Group（E2EHealthStatus）を起動/停止する。`BswMLogicalExpression`（[SWS_BswM_00808]）の簡略版として AND/OR の複合条件ルールにも対応し、「EcuM==RUN AND ComM==FULL_COMMUNICATION」でテレメトリ開始、「ComM==SILENT_COMMUNICATION OR ComM==NO_COMMUNICATION」で停止する |
|  | WdgM | Supervised Entity の Alive Supervision（呼び出し回数を6000msごとに評価）・Logical Supervision（チェックポイント順序）・Deadline Supervision（チェックポイント間の経過時間）を独立したステータスで管理。判定は6000ms周期、実HWウォッチドッグへのリフレッシュは別途1000ms周期（WdgM_TriggerHwWatchdog）で行い、異常時はリフレッシュを止めて実際にMCUをリセットする。実際の HW 有効/無効/リフレッシュは WdgIf 経由で Wdg（下位ドライバ）に委譲し、WdgM 自身は判定ロジックのみを持つ |
|  | ComM | CAN バスの通信モード（NO_COM / SILENT_COM / FULL_COM）を管理し CanSM へ要求。`ComM_BusSMIndication` で EcuM の RUN 要求を操作。複数ユーザ（App_EngineManager=COMM_USER_0, Dcm=COMM_USER_1）の要求を最も通信レベルの高いモードへ集約する調停ロジックを持つ。両ユーザが NO_COM を要求したときのみ実際にボランタリスリープへ落ちる |
|  | CanSM | Bus-Off 検出直後（回復試行の前）に `ComM_BusSMIndication(SILENT_COMMUNICATION)` を呼び、ComM のチャネル状態が回復完了まで FULL_COM のまま古い情報として残ることを防ぐ（SWS_CanSM_00521。SILENT_COM は EcuM の RUN を維持するため回復中も RUN は落ちない）。受け付ける Bus-Off はコントローラが物理的に稼働中の状態（FULL_COM、および Nm の Bus-Sleep Mode 到達待ちで HW が稼働継続する NO_COM_PENDING_SLEEP）のみで、回復シーケンスは L1/L2 バックオフ（SWS_CanSM_00514/00515 準拠）で実施し、試行回数が `CANSM_BUSOFF_L1_TO_L2_COUNT` を超えるまでは短い周期（L1）でリトライし、超えたら Dem へ DTC を報告（limit=1 のため即座に確定）した上で長い周期（L2）へ切り替えて無期限にリトライを継続する（回復を諦めて停止する状態は存在しない）。再起動試行のたびに、Bus-Off 発生時点の状態（FULL_COM か NO_COM_PENDING_SLEEP か）へ復帰させる（`CanSM_BusOffFromPendingSleep`、後者の場合は誤って FULL_COM へ戻さない）。ComM の NO_COM 要求によるボランタリスリープでは即座にはスリープせず、Nm（CanNm 状態機械）が Bus-Sleep Mode へ到達した通知（`CanSM_NmBusSleepMode()`）を受けてから `Can_SetControllerMode(CAN_T_SLEEP)` で実 HW を実際にスリープさせる（協調スリープ、詳細は Nm セクション参照）。`CanSM_ControllerWakeup()` による復帰経路を持ち、復帰は即座に確定せず、ウェイクアップ検証（Wakeup Validation Protocol 相当）により有効な CAN フレーム受信を確認してから FULL_COM へ確定する |
|  | Nm | CanNm 状態機械（Network Mode の Repeat Message/Normal Operation/Ready Sleep の3内部状態、Prepare Bus-Sleep Mode、Bus-Sleep Mode）を実装。`ComM_BusSMIndication()` が呼ぶ `Nm_NetworkRequest()`/`Nm_NetworkRelease()` を契機に自律的に状態遷移し、NM-Timeout/Repeat Message/Wait-Bus-Sleep の3タイマで駆動する。Bus-Sleep Mode へ到達すると `CanSM_NmBusSleepMode()` を呼び、CanSM はこの通知を受けて初めて CAN コントローラを物理スリープさせる（協調スリープ。他ノードの NM フレーム受信が続く間は実際にはスリープしない）。PduR/Com を経由せず `CanIf_Transmit`/`CanIf_RxIndication` を直接やり取りする点が実車の CanNm と同じ。シグナル値を運ばないため E2E 保護は付与しない |

<a id="processing-flow-ecu"></a>
#### 処理の流れ（コールチェーン）

AUTOSAR では「上から下への要求 (Request)」と「下から上への通知 (Indication)」が分離されています。
Bus-Off 回復・ウェイクアップ検証は CanSM が中心となって EcuM/ComM/Nm/Can と連携するため、
特定の 1 モジュールに閉じた話ではなく、ここでモジュール横断のコールチェーンとしてまとめます。

```
【起動時】
EcuM_Init → ComM_RequestComMode(FULL_COM)   ← EcuM が ComM へ要求（上→下）
              └→ CanSM_RequestComMode(FULL_COM)
                   └→ ComM_BusSMIndication(FULL_COM)  ← CanSM が ComM へ通知（下→上）
                        └→ EcuM_RequestRUN(ECUM_USER_COMM)

【Bus-Off 検出時（回復試行の前、SWS_CanSM_00521）】
CanIf_ControllerBusOff → CanSM_ControllerBusOff
  受け付けるのは CANSM_STATE_FULL_COM と CANSM_STATE_NO_COM_PENDING_SLEEP
  （Nm の Bus-Sleep Mode 到達待ちでコントローラがまだ稼働中の状態）の 2 つのみ
  （2026-08 発見・修正: 以前は FULL_COM のみを受け付けており、
   NO_COM_PENDING_SLEEP 中の実 Bus-Off は黙って無視され、回復シーケンスが
   一切起動しないままコントローラが HW 的に Bus-Off し続ける不具合があった）
  └→ Can_SetControllerMode(CAN_T_STOP)
       └→ ComM_BusSMIndication(SILENT_COM)  ← CanSM が ComM へ通知（下→上）
            （SILENT_COM は EcuM_RequestRUN/ReleaseRUN いずれも呼ばない → RUN 維持）

【Bus-Off 回復試行時（L1/L2 バックオフで無期限に継続）】
CanSM_MainFunction（10ms タスク）
  └→ Can_SetControllerMode(CAN_T_START) で再起動を試行
       └→ 復帰先は Bus-Off 発生時点の状態で分岐する
          （CanSM_BusOffFromPendingSleep フラグ、CanSM.c 参照）
          ├─ 発生時 FULL_COM だった場合: CanSM state → FULL_COM
          │    └→ ComM_BusSMIndication(FULL_COM)  ← CanSM が ComM へ通知（下→上）
          │         └→ ComM_EcuMRunMode が既に FULL_COMMUNICATION のため
          │            EcuM_RequestRUN() は呼ばない（RUN は Bus-Off 中も維持
          │            されたまま）。Nm へは Nm_NetworkRequest() のみ送る
          └─ 発生時 NO_COM_PENDING_SLEEP だった場合: CanSM state →
               NO_COM_PENDING_SLEEP（FULL_COM へは戻さない。ComM は既に
               NO_COM を要求済みで、戻すと誰も再要求せず取り残されるため）
               └→ ComM_BusSMIndication(NO_COM)  ← CanSM が ComM へ通知（下→上）
                    └→ ComM_EcuMRunMode が既に NO_COMMUNICATION のため
                       EcuM_ReleaseRUN() は呼ばない（RUN は既にボランタリ
                       スリープ突入時点で解放済み）
  （L1 リトライ超過時は Dem へ FAILED を報告するのみで、RUN の状態には影響しない）

【ボランタリスリープ突入時（エンジン OFF 継続、復帰経路あり）】
App_EngineManager_Run（3000ms タスク、ENGINE_STATE_OFF が5周期継続）
  └→ Rte_Call_ComM_RequestComMode(NO_COM)   ← ASW が ComM へ要求（上→下）
       └→ ComM_RequestComMode(COMM_USER_0, NO_COM)
            └→ 集約結果が NO_COM（Dcm も extendedSession でない場合のみ）
                 └→ CanSM_RequestComMode(NO_COM) → Can_SetControllerMode(CAN_T_SLEEP)
                      └→ ComM_BusSMIndication(NO_COM)
                           └→ EcuM_ReleaseRUN(ECUM_USER_COMM)
                                └→ EcuM: RUN → POST_RUN → (5秒後) → SHUTDOWN

【ボランタリスリープからのウェイクアップ時 — 1st phase: 検知（CAN バス活動を検知）】
Can_Isr（INT ピン立ち下がりの真のハードウェア割り込み。SHUTDOWN 中も常に有効）
  └→ Can_WakeupIrqPending フラグをセットするのみ（SPI/Serial は行わない）
       └→ Can_MainFunction_Wakeup（1ms タスク、SHUTDOWN 中もこのタスクだけは動き続ける）
            がフラグをドレインし CanIf_ControllerWakeup(0)  ← Can が CanIf へ通知（下→上）
                 └→ CanSM_ControllerWakeup(0)
                      └→ Can_SetControllerMode(CAN_T_WAKEUP)   ← SLEEP→STOPPED (Listen-Only) のみ
                           └→ CanSM: NO_COM → WAKEUP_VALIDATING（ComM/EcuM へはまだ何も通知しない）

【2nd phase-a: 検証成功（検証タイマ内に有効な CAN フレームを受信）】
Can_MainFunction_Read（Listen-Only になったため通常の RX ドレインが有効。
                        SHUTDOWN 中もこのタスクだけは動き続ける）
  └→ CanIf_RxIndication()                     ← 受信フレームを検出
       └→ CanSM_RxIndication(0)                ← 全受信フレームで無条件に呼ばれる
            └→ CanSM: WAKEUP_VALIDATING → FULL_COM（Can_SetControllerMode(CAN_T_START)）
                 └→ ComM_BusSMIndication(FULL_COM)
                      └→ EcuM_RequestRUN(ECUM_USER_COMM)
                           └→ EcuM: SHUTDOWN → RUN（全タスク再有効化）
       └→ （同じフレームがそのまま PduR/Com/Dcm 等へも配信される）

【2nd phase-b: 検証失敗（検証タイマ超過、ノイズによる誤ウェイクアップ）】
CanSM_MainFunction（10ms タスク、SHUTDOWN 中も動き続ける）
  └→ CANSM_WAKEUP_VALIDATION_MS 超過を検出
       └→ Can_SetControllerMode(CAN_T_SLEEP)   ← STOPPED→SLEEP、ウェイクアップ割り込み再武装
            └→ CanSM: WAKEUP_VALIDATING → NO_COM（ComM/EcuM は一切関与せず、静かに再スリープ）
```

<a id="ecum"></a>
#### EcuM（ECU ステートマネージャ）

EcuM (ECU State Manager) は BSW スタック全体のライフサイクルを管理するモジュールです。
`main.cpp` は `EcuM_Init()` と `EcuM_MainFunction()` を呼ぶだけでよく、
個々の BSW モジュールを直接参照しません。

##### EcuM 状態マシン

```
          EcuM_Init() 完了
STARTUP ──────────────────→ RUN ── 全 RUN ユーザが解放 ──→ POST_RUN
                             ↑                                  │
                    EcuM_RequestRUN が来たら ←──────────────────┘
                    (POST_RUN 中の場合のみ)       ECUM_POST_RUN_TIMEOUT_MS (5秒) 経過
                                                               ↓
                                                           SHUTDOWN
                            (WdgM_TriggerHwWatchdog / Can_MainFunction_Read /
                             Can_MainFunction_Wakeup / CanSM_MainFunction /
                             NvM_MainFunction / MemIf_MainFunction /
                             Nm_MainFunction 以外は停止)
                             ↑                                  │
                    CAN バスのウェイクアップ ←──────────────────┘
                    (EcuM_RequestRUN 経由)
```

| 状態 | `Os_SchedulerStep()` | 遷移条件 |
|------|:-------------------:|---------|
| STARTUP | 停止 | `EcuM_Init()` 末尾で RUN へ自動遷移 |
| RUN | **実行** | 全 RUN ユーザが `EcuM_ReleaseRUN` → POST_RUN |
| POST_RUN | **実行**（後処理継続） | タイムアウト → SHUTDOWN / `EcuM_RequestRUN` → RUN |
| SHUTDOWN | **実行**（`WdgM_TriggerHwWatchdog` / `Can_MainFunction_Read` / `Can_MainFunction_Wakeup` / `CanSM_MainFunction` / `NvM_MainFunction` / `MemIf_MainFunction` / `Nm_MainFunction` のみ有効） | Arduino では電源断不可のためアイドル待機するが、`EcuM_RequestRUN` が来れば RUN へ復帰できる（CAN バスのウェイクアップ経由）。`Os_SchedulerStep()` 自体は呼ばれ続けるが、BswM Rule 2 がこの 7 タスク以外を無効化するため実質アイドル。HW ウォッチドッグ維持のため `WdgM_TriggerHwWatchdog`、CAN ウェイクアップ検出・検証中フレーム処理のため `Can_MainFunction_Read`/`Can_MainFunction_Wakeup`、ウェイクアップ検証タイムアウト監視のため `CanSM_MainFunction`、保留中の DTC 永続化のため `NvM_MainFunction`/`MemIf_MainFunction`（NvM がジョブを開始するだけの `NvM_MainFunction` だけを動かしても、物理バイト書き込みを進める `MemIf_MainFunction` を止めてしまうとジョブが永久に完了しない）、Nm 状態機械（Bus-Sleep Mode への到達判定・他ノードの NM フレーム受信によるスリープ延期の継続処理）のため `Nm_MainFunction` だけは動き続ける（CAN 受信自体は真のハードウェア割り込み `Can_Isr()` のため、この無効化に関わらず常に起動する） |

SHUTDOWN は CAN バスのウェイクアップにより常に RUN へ復帰できます。実機リセットが
必要な終端状態は存在しません。Bus-Off 回復は後述の通り L1/L2 バックオフで無期限に
継続するため、Bus-Off の検出・回復だけを理由に新たに RUN が解放されて SHUTDOWN へ
向かうことはなく、SHUTDOWN は ComM の NO_COM 要求による正常系（ボランタリ）スリープ
からのみ到達します（NO_COM_PENDING_SLEEP 中に実際に Bus-Off が発生した場合、回復時に
`ComM_BusSMIndication(NO_COM)` が呼ばれ直すことはありますが、RUN は既にボランタリ
スリープ突入時点で解放済みのため、これによって新たに `EcuM_ReleaseRUN()` が呼ばれる
ことはありません。詳細は CanSM.c の `CanSM_BusOffFromPendingSleep` 参照）。

##### Os のスケジューラティック（Gpt 駆動）

`Os_SchedulerStep()` の周期到来判定に使う時間源は、当初 Arduino コアの
`millis()` でしたが、2026-08 に Os 専用の Gpt チャネル（`GPT_CHANNEL_1`、
1000Hz=1ms 分解能、Notification なし）へ置き換えました。`Os_Init()` が
自らこのチャネルを `Gpt_StartTimer()` で起動し、以後は
`Gpt_GetTimeElapsed(GPT_CHANNEL_1)` を都度ポーリングします（本物の
AUTOSAR OS の OsCounter が HW タイマ割り込みで駆動される構成に近づける
ための変更。詳細は `src/Os/Os.c` 冒頭のコメント参照）。

`loop()` は従来どおり `EcuM_MainFunction()` を busy-spin で呼び続けます。
`Gpt_SetMode`/`Gpt_EnableWakeup` 系は本プロジェクトの EcuM が SLEEP モード
を持たないため未実装であり（Gpt モジュールの節参照）、CPU を寝かせる余地が
ないためです。つまりこの変更は「割り込みで CPU を起こす」設計ではなく、
「経過時間の計算に使う時計を millis() から Gpt の ISR 駆動ティックへ
差し替える」だけの、スコープを絞った変更です。

**millis() をフォールバック用に残した理由:** [DEVLOG](docs/DEVLOG.md#can-rx-割り込み化の実機検証で得られた教訓)
に記録のとおり、本プロジェクトは実機で割り込みが期待どおり発火しなかった
事象を CAN RX 割り込み化の際に一度経験しています。Os の時間源はスケジューラ
そのものであり、`WdgM_TriggerHwWatchdog` を含む全タスクの発火判定に使われる
ため、ここが完全に停止すると実 HW ウォッチドッグ（`WDGM_HW_WATCHDOG_TIMEOUT_MS`=
4000ms）でリセットされてしまいます。CAN フレーム 1 個の欠落よりも影響が
大きいため、単に「検知してログを残す」だけでは不十分です（ログを残しても
スケジューラ自体が止まったままでは結局リセットに至ってしまう）。

`Os_CrossCheckTickSource()` は Gpt とは別系統の HW タイマで駆動している
`millis()` との差分を `OS_TICK_CROSSCHECK_PERIOD_MS`（500ms、HW ウォッチドッグ
タイムアウトの 4000ms に対して 8 倍のマージン）ごとに突き合わせ、Gpt ティック
の進みが明らかに遅い（半分未満）場合は実際に時間源を `millis()` へ
フォールバックします（ラッチ式。一度切り替えたらその起動中は millis() を
使い続ける）。切り替える瞬間は全タスクの最終実行時刻を現在の `millis()` 値へ
リセットします（`Os_SetTaskActive()` が休止タスクを再開する際に行うのと
同じ考え方。リセットしないと基準時刻が「Gpt ティック（停止した値）」から
「millis()（現在の実時刻）」へ飛び、ほぼ全タスクが「周期を大幅に超過している」
と誤判定されて一斉に追いつき実行されてしまう。これは WdgM の Alive Supervision
が過去に繰り返し踏んだ「監視対象タスクに実行機会がほとんどないまま判定される」
誤検知と同種の事故になりうるため避けている）。

初版（2026-08 最初のコミット）ではクロスチェック周期を 5000ms、フォールバック
無しの「ログのみ」としていましたが、いずれも問題があるとレビューで指摘され
修正しました。5000ms は 4000ms の HW ウォッチドッグタイムアウトより長く、
最悪ケースでは診断ログさえリセット前に一度も出力されません。また「ログのみ」
ではスケジューラが止まったまま復旧しないため、結局リセットに至ることに
変わりありませんでした。

##### RUN ユーザ

RUN フェーズを継続するために「誰かが使っている」ことを宣言するしくみです。
ユーザが全員解放したときに POST_RUN へ遷移します。

| ユーザ | 定数 | `EcuM_RequestRUN` タイミング | `EcuM_ReleaseRUN` タイミング |
|-------|------|--------------------------|--------------------------|
| ComM | `ECUM_USER_COMM` | CAN バスが FULL_COM になったとき（起動時 / Bus-Off 回復試行時 / ボランタリスリープからのウェイクアップ時） | CAN バスが NO_COM になったとき（エンジン OFF 継続によるボランタリスリープ突入時。NO_COM_PENDING_SLEEP 中に Bus-Off が発生し回復した場合も `ComM_BusSMIndication(NO_COM)` は呼ばれ直すが、RUN は既に解放済みのため `EcuM_ReleaseRUN()` が再度呼ばれることはない） |

**重複要求・対応しない解放の検知（SWS_EcuM_04125/04127）:**
`EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` は、AUTOSAR の実 EcuM と同様に
「同一ユーザからの要求はネストできない」ことを検知します。各ユーザの RUN 要求は
`EcuM_RunUsers` のビットマスクで管理しており、既に立っているビットへ重ねて
`EcuM_RequestRUN()` を呼ぶと DET 相当のログ（`ECUM_E_MULTIPLE_RUN_REQUESTS`）を
出力して `E_NOT_OK` を返します。同様に、立っていないビットに対して
`EcuM_ReleaseRUN()` を呼ぶと `ECUM_E_MISMATCHED_RUN_RELEASE` 相当のログを
出力して `E_NOT_OK` を返します。呼び出し元は必ず `void` キャストで戻り値を
捨てているため、この検知は実行時の挙動には影響しません（開発時の診断用途）。

この検知の追加に伴い、`ComM_BusSMIndication()` 側も、実際に EcuM の RUN 要求状態が
変化した時のみ `EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` を呼ぶよう変更しました。
当初はチャネルモード（`ComM_ChannelMode`）そのものの変化で判定していましたが、
Bus-Off 検出時に一時的に挟まる `COMM_SILENT_COMMUNICATION`（EcuM の RUN 状態には
無関係）を経由すると、回復時の FULL_COM/NO_COM 通知が「SILENT_COM からの変化」として
見えてしまい、EcuM 側で `ERR=0x20`（多重要求）/`ERR=0x21`（不整合解放）を誤検知する
不具合があった（2026-08 発見・修正）。現在は `ComM_EcuMRunMode`（EcuM へ最後に伝えた
FULL/NO_COM の別。SILENT_COM では更新しない）という専用の内部状態で判定しており、
Bus-Off 回復中に SILENT_COM を何度挟んでも、EcuM への再通知は本当に FULL⇔NO_COM が
変化したときだけに限られます。

##### EcuM 設定（`EcuM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `ECUM_USER_COUNT` | 1 | RUN 要求できるユーザ数 |
| `ECUM_USER_COMM` | 0 | ComM のユーザ ID |
| `ECUM_POST_RUN_TIMEOUT_MS` | 5000 ms | POST_RUN タイムアウト |

<a id="bswm"></a>
#### BswM（BSW モードマネージャ）

BswM (BSW Mode Manager) は、EcuM や ComM からのモード変化通知を受け取り、
ルールテーブルに従って Os タスクの有効・無効を切り替えるルールエンジンです。

EcuM が「今どのフェーズか」を決めるのに対し、BswM は「そのフェーズで何をするか」を決めます。
この責任分離により、フェーズごとの振る舞いをコードを書かずにルールテーブルの変更だけで調整できます。

##### ルールテーブル（`BswM_PBCfg.c`）

| No | 条件（Operator） | アクション | 対象 |
|----|------------------|-----------|------|
| 0 | EcuM==RUN | ACTIVATE | 全タスク（`BSWM_TASK_MASK_ALL`） |
| 1 | EcuM==POST_RUN | DEACTIVATE | アプリタスクのみ（`BSWM_TASK_MASK_APP`） |
| 2 | EcuM==SHUTDOWN | DEACTIVATE | `BSWM_TASK_MASK_SHUTDOWN`（WdgM_TriggerHwWatchdog・Can_MainFunction_Read・Can_MainFunction_Wakeup・CanSM_MainFunction・NvM_MainFunction・MemIf_MainFunction・Nm_MainFunction を除く） |
| 3 | **EcuM==RUN `AND` ComM==FULL_COMMUNICATION** | PDU_GROUP_START | I-PDU Group「テレメトリ」(E2EHealthStatus) |
| 4 | EcuM==POST_RUN | PDU_GROUP_STOP | I-PDU Group「テレメトリ」 |
| 5 | **ComM==SILENT_COMMUNICATION `OR` ComM==NO_COMMUNICATION** | PDU_GROUP_STOP | I-PDU Group「テレメトリ」 |

Rule 3/5 が複合条件（`BswM_ConditionType` の配列を `BswM_LogicalOperatorType`
(`BSWM_OP_AND`/`BSWM_OP_OR`) で組み合わせる、[SWS_BswM_00808]
BswMLogicalExpression の簡略版）を使う唯一の例です。以前は単一条件ルール
しか組めない設計（AND/OR の LogicalExpression 相当が未実装）でしたが、
Nm（CanNm 状態機械）導入により ComM のチャネルモードが EcuM の RUN/POST_RUN
とは独立に変化しうるようになったため、「EcuM が RUN でも CAN チャネルが
実際には使えない（Bus-Off 中の SILENT_COMMUNICATION 等）」場合を正しく
扱うために複合条件対応を追加しました。Rule 3（AND、開始条件）と Rule 5
（OR、停止条件）が対になっており、CAN チャネルが FULL_COMMUNICATION から
離脱した瞬間（原因が Bus-Off でもボランタリスリープでも）に確実にテレメトリ
送信を止めます。

各ルールの Action は、条件の評価結果が **false→true へ遷移したときのみ**
実行されます（`BswM_RuleLastResult[]` で直近の結果をキャッシュし、
true が続く間の重複実行や true→false への遷移では実行しません）。複合条件
ルールは、どちらの条件（ソース）が最後に変化しても正しく発火するよう、
`BswM_ModeSrcCache[]` に両ソースの最新値を保持したうえで評価します
（詳細は `BswM.c` の `BswM_ExecuteRules()`/`BswM_EvaluateRule()` 参照）。

対応除外（実 AUTOSAR の BswMLogicalExpression と比べた簡略化）: NAND/NOT/XOR
演算子、3条件以上、LogicalExpression の入れ子（木構造）。本プロジェクトの
モードソースは EcuM/ComM の2つのみで、これらを使うルールの実績もないため
対応除外としています。

##### タスク ID とマスク（`BswM_Cfg.h`）

タスク数が 16 を超えるため、`TaskMask` は uint32（bits 0〜17）です。

| タスク ID | 定数 | 対応関数 | 周期 |
|---------|------|---------|------|
| 0 | `BSWM_OS_TASK_CAN_READ` | `Can_MainFunction_Read` | 1 ms |
| 1 | `BSWM_OS_TASK_CANTP_MAIN` | `CanTp_MainFunction` | 1 ms |
| 2 | `BSWM_OS_TASK_RTE_ENGINE` | `Rte_ScheduleRunnables` | 3000 ms |
| 3 | `BSWM_OS_TASK_RTE_WARNING` | `Rte_ScheduleWarningIndicator` | 500 ms |
| 4 | `BSWM_OS_TASK_CANSM_MAIN` | `CanSM_MainFunction` | 10 ms |
| 5 | `BSWM_OS_TASK_COM_MAIN` | `Com_MainFunction` | 100 ms |
| 6 | `BSWM_OS_TASK_IOHWAB_MAIN` | `IoHwAb_MainFunction` | 10 ms |
| 7 | `BSWM_OS_TASK_WDGM_MAIN` | `WdgM_MainFunction` | 6000 ms |
| 8 | `BSWM_OS_TASK_DCM_MAIN` | `Dcm_MainFunction` | 1000 ms |
| 9 | `BSWM_OS_TASK_FIM_MAIN` | `FiM_MainFunction` | 100 ms |
| 10 | `BSWM_OS_TASK_WDGM_TRIGGER` | `WdgM_TriggerHwWatchdog` | 1000 ms |
| 11 | `BSWM_OS_TASK_NM_MAIN` | `Nm_MainFunction` | 1000 ms |
| 12 | `BSWM_OS_TASK_NVM_MAIN` | `NvM_MainFunction` | 10 ms |
| 13 | `BSWM_OS_TASK_CAN_TX_CONF` | `Can_MainFunction_Write` | 1 ms |
| 14 | `BSWM_OS_TASK_CAN_BUSOFF` | `Can_MainFunction_BusOff` | 1 ms |
| 15 | `BSWM_OS_TASK_CAN_WAKEUP` | `Can_MainFunction_Wakeup` | 1 ms |
| 16 | `BSWM_OS_TASK_SECOC_MAIN` | `SecOC_MainFunction` | 100 ms |
| 17 | `BSWM_OS_TASK_MEMIF_MAIN` | `MemIf_MainFunction` | 10 ms |

`BSWM_TASK_MASK_APP = 0x00C`（bit2=Rte_Engine, bit3=Rte_Warning）がアプリタスクマスクです。
POST_RUN ではこの 2 タスクだけを停止し、BSW タスク（Can_MainFunction_Read/BusOff/Wakeup・CanTp・CanSM・Com・IoHwAb・WdgM・Dcm・FiM・WdgM_TriggerHwWatchdog・Nm・NvM・MemIf・SecOC・Can_MainFunction_Write）は継続させます。
Dcm を継続させることで、POST_RUN 中も S3 タイマ監視（セッションの自動失効）が動作し続けます。
Nm は POST_RUN 中も動き続けますが、POST_RUN へ遷移する経路（エンジン OFF 継続による
ボランタリスリープ突入。Bus-Off は L1/L2 バックオフで無期限に回復を試みるため
POST_RUN 遷移の原因にはならない）では ComM は既に NO_COM になっているため、
実際には送信を行いません。

`BSWM_TASK_MASK_SHUTDOWN = 0x163EE`（`BSWM_TASK_MASK_ALL` から bit10=WdgM_TriggerHwWatchdog・
bit0=Can_MainFunction_Read・bit15=Can_MainFunction_Wakeup・bit4=CanSM_MainFunction・
bit12=NvM_MainFunction・bit17=MemIf_MainFunction・bit11=Nm_MainFunction を除いたもの）が
SHUTDOWN 時の無効化対象マスクです。SecOC_MainFunction（bit16）はこの除外リストに
含まれないため（POST_RUN 中に Com_MainFunction が止まり SecOC の送信要求自体が
発生しなくなるのと同じ理由で、無効化しておくのが本来の設計意図）、
Can_MainFunction_Write（bit13）・Can_MainFunction_BusOff（bit14）と同様に
SHUTDOWN 中は停止します。BusOff ポーリングは `CanState==CAN_CS_STARTED` が条件のため
SHUTDOWN 中（SLEEP か Listen-Only）はどのみち無意味であり、TX 確認も SHUTDOWN 中は
新規送信が発生しないため停止して問題ありません（詳細は Can セクションの
「TX 確認の非同期化」参照）。
WdgM_TriggerHwWatchdog は、Renesas RA の IWDT が一度有効化すると無効化する手段がないため、
SHUTDOWN 後も動かし続けて `WdgM_SupervisionSuppressed` により無条件にリフレッシュを継続する
必要があります（詳細は WdgM セクションの「HW ウォッチドッグ連携」を参照）。
Can_MainFunction_Read・Can_MainFunction_Wakeup は、CAN バスのボランタリスリープからの
ウェイクアップ検出（Wakeup）、およびウェイクアップ検証中に届く診断フレームの受信処理（Read）
のために SHUTDOWN 後も動かし続けます（`Can_Isr()` は BswM の無効化に関わらず常に起動する
真のハードウェア割り込みだが、正しさをこの割り込みの成否だけに委ねない設計にしているため、
実際の SPI 読み出しと上位層への通知を担うこの 2 タスク自体を無効化するわけにはいかない。
詳細は Can セクションの「RX の割り込み化」を参照）。CanSM_MainFunction は、ウェイクアップ検証（後述）の検証タイムアウトを
監視するために SHUTDOWN 後も動かし続けます（詳細は後述の「CAN コントローラの実スリープ」参照）。
NvM_MainFunction は、SHUTDOWN 直前に Dem が新規 DTC を確定して書き込みジョブが保留中の
まま残る可能性があるため、SHUTDOWN 後も動かし続けて永続化を完了させます。
MemIf_MainFunction も同じ理由で動かし続ける必要があります。NvM_MainFunction は
MemIf_Write() でジョブを「開始」するだけで、実際の物理バイト書き込みを 1 バイトずつ
進めるのは MemIf_MainFunction（実体は Fee_MainFunction）だからです。NvM_MainFunction
だけを動かして MemIf_MainFunction を止めてしまうと、ジョブが開始されたまま永久に
`MEMIF_JOB_PENDING` を待ち続け、EEPROM への永続化が完了しません
（詳細は後述の「NvM（Non-Volatile Memory Manager）」の非同期書き込みジョブキュー参照）。
この 7 タスクの存在により、SHUTDOWN は HW ウォッチドッグを維持しつつ CAN バス活動
（ボランタリスリープからのウェイクアップ）で常に RUN へ復帰できる状態になっています。

##### POST_RUN でアプリタスクのみ停止する理由

POST_RUN 中も BSW タスクを動かし続けることで、以下のグレースフルシャットダウンが実現されます。

```
POST_RUN 中も動き続けるタスク:
  Can_MainFunction_Read / CanTp_Main → 受信中の診断フレームを最後まで処理
  CanSM_Main          → 回復シーケンスの完了まで管理
  Com_MainFunction    → デッドライン監視の最終確認
  IoHwAb_Main        → ボタンのデバウンス状態を正常終了
  WdgM_Main          → Alive Supervision のソフト評価は継続するが判定結果は無視される
                        （WdgM_SupervisionSuppressed が立っているため）
  WdgM_TriggerHwWatchdog → HW ウォッチドッグのリフレッシュは継続（判定結果を無視して
                        無条件にリフレッシュするため、実際のリセットは発生しない）
  Dcm_Main           → S3 タイマ監視を継続（拡張セッションも正しく失効する）
  Nm_MainFunction    → タスク自体は動き続けるが、ComM が既に NO_COM のため送信しない

POST_RUN 中に停止するタスク:
  Rte_ScheduleRunnables          → エンジン状態更新・DTC 登録を停止
  Rte_ScheduleWarningIndicator   → LED 制御を停止（消灯状態で固定）
```

##### 通知チェーン

```
ボランタリスリープ突入（エンジン OFF 継続）
  CanSM: CANSM_STATE_NO_COM へ遷移（この時点ではまだ物理スリープしない）
       → ComM_BusSMIndication(NO_COM)
            ├→ EcuM_ReleaseRUN(ECUM_USER_COMM)
            │     └→ EcuM: RUN → POST_RUN
            │               └→ BswM_EcuM_CurrentState(POST_RUN)
            │                     └→ Rule 1 発火: Os_SetTaskActive(Rte_Engine, OFF)
            │                                     Os_SetTaskActive(Rte_Warning, OFF)
            ├→ BswM_ComM_CurrentMode(0, NO_COM)
            │     └→ Rule 5 発火（OR 条件、ComM==NO_COM）:
            │           Com_IpduGroupStop(テレメトリ)  ← ComM==FULL_COM 前提の
            │           Rule 3 (AND条件) が既に false になっているため
            │           冪等（テレメトリが既に停止済みなら何もしない）
            └→ Nm_NetworkRelease()
                  → Nm: Normal Operation → Ready Sleep State（送信停止）
                  → NM-Timeout Timer 満了 → Prepare Bus-Sleep Mode
                  → Wait-Bus-Sleep Timer 満了（他ノードからの NM フレーム受信が
                    なければ）→ Bus-Sleep Mode へ到達
                        → CanSM_NmBusSleepMode()
                              → Can_SetControllerMode(CAN_T_SLEEP)  ← ここで初めて
                                MCP2515 を実際にスリープさせる（詳細は後述
                                「Nm（ネットワークマネジメント）」参照）

POST_RUN 5秒後
  EcuM: POST_RUN → SHUTDOWN
    └→ BswM_EcuM_CurrentState(SHUTDOWN)
          └→ Rule 2 発火: Os_SetTaskActive(WdgM_TriggerHwWatchdog / Can_MainFunction_Read /
                                          Can_MainFunction_Wakeup / CanSM_MainFunction /
                                          NvM_MainFunction / MemIf_MainFunction /
                                          Nm_MainFunction 以外, OFF)
                          （この 7 タスクだけは HW ウォッチドッグ維持 / CAN ウェイクアップ検出・
                            検証中フレーム処理 / ウェイクアップ検証タイムアウト監視 /
                            DTC永続化（ジョブ開始は NvM_MainFunction、物理バイト書き込みの
                            進行は MemIf_MainFunction） / Nm状態機械継続のため動き続ける。特に
                            Nm_MainFunction を止めてしまうと Nm が二度と Bus-Sleep Mode へ
                            到達できず、CAN コントローラが永久に物理スリープしなくなる不具合が
                            あったため SHUTDOWN 中も継続するよう変更した）
```

##### CAN コントローラの実スリープ（`Can_SetControllerMode(CAN_T_SLEEP)`）

`Can.c` には `CAN_T_SLEEP`/`CAN_T_WAKEUP` 遷移（MCP2515 を実際にスリープさせる
`Can_Hw_SetMode(CAN_HW_MODE_SLEEP)`）が以前から実装されていましたが、当初は
呼び出し元がなく死んだコードパスでした。現在は唯一の経路として、ComM の NO_COM
要求に端を発する `Nm`（CanNm 状態機械）の協調スリープから実際にスリープします。

`App_EngineManager_Run()` が `ENGINE_STATE_OFF` の継続を検知して
`ComM_RequestComMode(COMM_USER_0, NO_COM)` を要求し、ComM の集約結果が実際に
NO_COM になった場合（`Dcm` も extendedSession でないことが条件）、
`ComM_BusSMIndication()` が `Nm_NetworkRelease()` を呼びます。ここで CanSM は
まだ物理スリープしません。`Nm` が Ready Sleep → Prepare Bus-Sleep → Bus-Sleep
Mode と自律的に遷移し（他ノードからの NM フレーム受信があればその都度延期
される）、実際に Bus-Sleep Mode へ到達した時点で `CanSM_NmBusSleepMode()` を
呼んで初めて CanSM が実スリープを行います。MCP2515 の CAN バス活動による
ウェイクアップ割り込み（`mcp_can` の `setSleepWakeup()`）を事前に有効化して
からスリープするため、バス活動があれば自律的に起床できます。詳細は次項
「ボランタリスリープとウェイクアップ」および後述「Nm（ネットワークマネジメント）」
を参照してください。

> Bus-Off 回復（後述の「Bus-Off 回復シーケンス」参照）は L1/L2 バックオフで
> 無期限にリトライを継続する設計のため、CAN コントローラを実際にスリープさせる
> ことはありません（`Can_T_STOP`/`Can_T_START` の間を往復するのみ）。AUTOSAR
> 仕様（SWS_CanSM_00514/00515/00636）には「回復を諦めて二度と復帰しない」状態は
> 存在せず、この無期限リトライ設計に至った経緯は
> [DEVLOG](docs/DEVLOG.md#cansm-bus-off-回復断念設計の撤去) を参照してください。

##### ボランタリスリープとウェイクアップ

CAN コントローラを実際にスリープさせる唯一の経路（ボランタリスリープ）について、
スリープ判断からウェイクアップまでの一連の流れを詳しく説明します。

**スリープ判断（`App_EngineManager.c`）**

```
App_EngineManager_Run()（3000ms 周期）:
  ENGINE_STATE_OFF が続いている ?
    YES → s_offCycles++
           s_offCycles >= APP_ENGINE_SLEEP_OFF_CYCLES (既定 5、実質15秒) ?
             YES → Rte_Call_ComM_RequestComMode(NO_COM)
    NO  → s_offCycles = 0、Rte_Call_ComM_RequestComMode(FULL_COM)
```

`ComM_RequestComMode(COMM_USER_0, NO_COM)` は、Dcm（`COMM_USER_1`）が
extendedSession で FULL_COM を要求し続けている間は無効化されます
（ComM の複数ユーザ調停、前述の「ComM（通信マネージャ）」セクション参照）。
つまり「エンジンが止まっていて、かつ診断ツールも繋がっていない」ときだけ
実際にスリープします。

**ウェイクアップ検出とウェイクアップ検証（`Can_Isr()` / `Can_MainFunction_Read/Wakeup()` / `CanSM.c`）**

MCP2515 はスリープ中に CAN バス活動を検知すると、ソフトウェアの関与なしに
自律的に Listen-Only モードへ遷移し INT ピンをアサートします
（`setSleepWakeup(1)` で事前に有効化済み）。しかしこの WAKIF は電気的ノイズ等
でも誤って立ちうるため、INT アサートを検出しただけで即座に FULL_COM へ復帰
せず、AUTOSAR EcuM の **Wakeup Validation Protocol** に相当する 2 段階の
手順を踏みます。

```
Can_Isr()（INT ピン立ち下がりの真のハードウェア割り込み、SHUTDOWN 中も常に有効）:
  CAN_CS_SLEEP 中 ?
    YES → Can_WakeupIrqPending フラグをセットするのみ
    NO  → Can_RxIrqPending フラグをセットするのみ
  （SPI 通信・Serial ログ・CanIf 呼び出しはここでは一切行わない。理由は
   Can.c ファイル冒頭のコメントを参照）

Can_MainFunction_Wakeup()（1ms 周期、SHUTDOWN 中も動作）:
  CAN_CS_SLEEP 中 かつ Can_WakeupIrqPending ?
    YES → フラグをクリアし CanIf_ControllerWakeup(0) → CanSM_ControllerWakeup(0)
            → Can_SetControllerMode(CAN_T_WAKEUP)   ← SLEEP→STOPPED (Listen-Only) のみ
            → CanSM: NO_COM → WAKEUP_VALIDATING（ComM/EcuM へはまだ通知しない）

Can_MainFunction_Read()（1ms 周期、SHUTDOWN 中も動作）:
  CAN_CS_SLEEP 中でない かつ Can_RxIrqPending ?
    YES → フラグをクリアし、受信バッファが空になるまでドレイン
          （Listen-Only になった直後からは、この関数が受信フレームを処理する）
       → 受信フレームがあれば CanIf_RxIndication() → CanSM_RxIndication(0)
            CANSM_STATE_WAKEUP_VALIDATING 中 ?
              YES → 検証成功: Can_SetControllerMode(CAN_T_START) → CANSM_STATE_FULL_COM
                     → ComM_BusSMIndication(FULL_COM) → EcuM_RequestRUN
              NO  → 通常運用中は何もしない

CanSM_MainFunction()（10ms 周期、SHUTDOWN 中も動作）:
  CANSM_STATE_WAKEUP_VALIDATING 中 ?
    YES → CANSM_WAKEUP_VALIDATION_MS (既定 2000ms) 超過 ?
            YES → 検証失敗: Can_SetControllerMode(CAN_T_SLEEP)（再スリープ、割り込み再武装）
                   → CANSM_STATE_NO_COM（ComM/EcuM は一切関与しない）
```

`CanSM_RxIndication()` は AUTOSAR SWS_CanSM の `CanSMRxIndicationUsed` 設定に相当し、
CanIf が受信したフレーム**すべて**について（特定の PDU に一致するかどうかに
関わらず）呼び出されます。「何らかの構造的に正しい CAN フレームを実際に
受信できた」こと自体が、直前のウェイクアップが本物のバス活動だった証拠になる
という考え方です。

**旧設計からの改善: ウェイクアップ契機フレームの取りこぼしが解消された**

検証前の実装では、ウェイクアップ検出と同じ `Can_Isr()` 呼び出しの中で即座に
`CAN_T_START` まで遷移させていたため、ウェイクアップの引き金になった
フレーム自体は正しく受信される保証がありませんでした。今回、検証成功の
判定を「実際に受信できたフレーム」に基づかせる設計に変更したことで、
検証を成功させたフレームは `CanSM_RxIndication()` の直後に続く
`CanIf_RxIndication()` の通常の PDU 振り分け処理でそのまま PduR/Com/Dcm 等へ
配信されます。ウェイクアップの契機になったフレームが失われるという制約は
この設計変更で解消されました。

**想定されるログ例（スリープ突入 → ウェイクアップ検証成功）**

以下は設計上想定される一連のログです（実機での確認結果ではなく、コードから
導かれる期待値である点に注意してください）。

```
[エンジン OFF が15秒 (5周期) 継続]
INFO AppEng: OFF continued 5 cycles -> release COMM_USER_0 (voluntary sleep)
INFO ComM: User0 req=0 -> aggregated=0 (channel=2)
INFO CanSM: ->NO_COM (CAN controller SLEEP)
INFO ComM: ch0 ->mode=0
INFO EcuM: ->POST_RUN timeout=5000ms
INFO EcuM: ->SHUTDOWN

[CAN バスに何らかのフレームが送信される（INT アサート検出）]
INFO Can: Wakeup detected (INT asserted during SLEEP)
INFO CanIf: ControllerWakeup ch=0
INFO CanSM: Wakeup detected -> validating (Listen-Only, waiting for confirmed RX)

[検証タイマ内 (既定2000ms) に実際にフレームを受信 → 検証成功]
INFO CanSM: Wakeup validated (RX confirmed) -> FULL_COM
INFO ComM: ch0 ->mode=2
INFO EcuM: SHUTDOWN ->RUN (wakeup) user=0

[App_EngineManager_Run が再開（最大3000ms後）]
INFO AppEng: ComM FULL_COM resumed -> sleep countdown reset (grace cycle)
```

**想定されるログ例（ノイズによる誤ウェイクアップ、検証失敗）**

```
INFO Can: Wakeup detected (INT asserted during SLEEP)
INFO CanIf: ControllerWakeup ch=0
INFO CanSM: Wakeup detected -> validating (Listen-Only, waiting for confirmed RX)

[検証タイマ超過、有効なフレームを1つも受信できなかった]
WARN CanSM: Wakeup validation timeout (2000ms, no confirmed RX) -> back to SLEEP
（ComM/EcuM には何も通知されないため、SHUTDOWN 状態はそのまま維持される）
```

##### BswM 設定の変更方法

| 変更内容 | 編集ファイル |
|---------|------------|
| POST_RUN で停止するタスクの追加・変更 | `BswM_Cfg.h` の `BSWM_TASK_MASK_APP` |
| ルール追加（例: ComM モードに反応する） | `BswM_PBCfg.c` にルールを追記し `BSWM_RULE_COUNT` を更新 |
| タスク追加 | `BswM_Cfg.h` に ID 定数を追加し `Os_PBCfg.c` にも追記 |

<a id="comm"></a>
#### ComM（通信マネージャ）

ComM (Communication Manager) は、複数の「ユーザ」からの通信モード要求を集約し、
CAN バスの通信モード（NO_COM / SILENT_COM / FULL_COM）を決定するモジュールです。
実際の CAN コントローラ操作は CanSM (`CanSM_RequestComMode`) に委譲します。

##### 複数ユーザの調停（AUTOSAR SWS_ComM_00069）

当初は EcuM（`COMM_USER_0`）だけが起動時に一度 FULL_COM を要求し、以後誰も要求を
変えない「実質1ユーザ」の実装でした。Dcm の SID 0x2F (IOControl) 実装を機に、
Dcm を2人目のユーザ（`COMM_USER_1`）として追加し、`ComM_RequestComMode()` に
複数ユーザの要求を実際に集約するロジックを実装しました。

```
ComM_RequestComMode(User, ComMode):
  ComM_UserRequest[User] = ComMode            ← このユーザの要求を記録
  aggregated = max(ComM_UserRequest[0..N-1])  ← 全ユーザの要求のうち最も通信レベルの
                                                高いモードを採用
                                                (FULL_COM(2) > SILENT_COM(1) > NO_COM(0))
  aggregated == 現在のチャネルモード ?
    YES → 何もしない（要求は記録されたがチャネルへの反映は不要）
    NO  → CanSM_RequestComMode(0, aggregated) ← チャネルへ実際に反映
```

「誰か一人でも通信を必要としていればバスは落とさない」という考え方で、
1人が NO_COM を要求しても他のユーザが FULL_COM を要求していればチャネルは
FULL_COM のまま維持されます。

##### Dcm との連携（`COMM_USER_1`）

`Dcm_Cbk.c` は診断セッションの状態に応じて `COMM_USER_1` の要求を更新します
（`Dcm_UpdateComMRequest()`、セッション遷移が起こるすべての経路から呼ばれる）。

| タイミング | 要求するモード |
|-----------|---------------|
| extendedSession に入ったとき（SID 0x10/0x03） | `COMM_FULL_COMMUNICATION` |
| defaultSession へ戻ったとき（明示要求・S3タイムアウト・ECUReset のいずれも） | `COMM_NO_COMMUNICATION` |

「診断ツールが繋がっている間はバスを落とさない」という実車でもよくある要件を、
EcuM（`COMM_USER_0`）とは独立したユーザ要求として表現しています。

##### App_EngineManager との連携（`COMM_USER_0`）

当初は EcuM が起動時に要求した FULL_COM を一度も解放しない「実質固定」でしたが、
`App_EngineManager_Run()` が `ENGINE_STATE_OFF` の継続（既定 5 周期、実質15秒）を
検知すると `Rte_Call_ComM_RequestComMode(NO_COM)` 経由で `COMM_USER_0` の要求を
実際に解放するようになりました（ボランタリスリープ。詳細は「CAN 通信スタック」
セクションの「ボランタリスリープとウェイクアップ」を参照）。

これにより、複数ユーザ調停が実際に意味を持つ場面が生まれました。
「エンジンが止まっていて（`COMM_USER_0` が NO_COM 要求）、かつ診断ツールも
繋がっていない（`COMM_USER_1` も NO_COM 要求）」ときだけ集約結果が NO_COM になり、
どちらか一方でも通信を必要としていればチャネルは FULL_COM のまま維持されます。

```
[Extended Session 突入中にエンジン OFF が継続した場合]
INFO AppEng: OFF continued 5 cycles -> release COMM_USER_0 (voluntary sleep)
INFO ComM: User0 req=0 -> aggregated=2 (channel=2)   ← User1(Dcm)がFULL_COM(2)要求中のため変化なし

[Extended Session 終了後、なおエンジン OFF が継続していた場合]
INFO Dcm: S3 timeout -> session=Default
INFO ComM: User1 req=0 -> aggregated=0 (channel=2)   ← User0も既にNO_COM要求済みのため今度こそ集約結果が変化
INFO CanSM: ->NO_COM (CAN controller SLEEP)
```

##### ウェイクアップ時の User0 要求の再同期

CanSM がウェイクアップ検証成功時に `ComM_BusSMIndication(FULL_COM)` を呼んで
チャネル状態を更新するのは、どのユーザの要求でもない自動的な変化です。これを
放置すると `ComM_UserRequest[COMM_USER_0]` がスリープ突入時の古い値（`NO_COM`）
のまま残り、`App_EngineManager_Run()` がまだ 1 周期も再評価していない
（Task 2 が次に実行されるのは最大3000ms後）わずかな間に他ユーザ（Dcm）が
要求を変化させただけで、User0 の古い要求と誤って再集約され、ウェイクアップ
直後に意図せず即座に再スリープしてしまいます。

これを防ぐため `ComM_BusSMIndication()` は `FULL_COM`/`NO_COM` を通知するとき、
`ComM_UserRequest[COMM_USER_0]` もその値へ同期します。CanSM 側の自動的な状態変化を
「User0 の暫定的な要求」とみなすことで、App_EngineManager が次に実際のエンジン
状態に基づいて要求し直すまでの間、矛盾のない値を保持できます。Dcm
（`COMM_USER_1`）の要求はセッション状態に基づく独立した判断のため、これには
同期させません（実機で発見された経緯は
[DEVLOG](docs/DEVLOG.md#comm-ウェイクアップ直後の再集約による即座の再スリープ) 参照）。

##### ComM 設定（`ComM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `COMM_USER_COUNT` | 2 | 通信モードを要求できるユーザ数 |
| `COMM_USER_0` | 0 | EcuM/App_EngineManager（エンジン運転中は FULL_COM、OFF 継続時は NO_COM を要求） |
| `COMM_USER_1` | 1 | Dcm（extendedSession の間だけ FULL_COM を要求） |

<a id="wdgm"></a>
#### WdgM（ウォッチドッグマネージャ）

WdgM (Watchdog Manager) は「ソフトウェアが本当に動いているか」を監視するモジュールです。
EcuM や BswM がフェーズ管理・タスク制御を担うのに対し、WdgM はタスク内部の実行を監視します。

CAN バスが正常でも、タスクが無限ループやスタック破壊で停止することがあります。
WdgM は監視対象（Supervised Entity）に「生存報告」を埋め込み、報告が途絶えたとき（Alive Supervision）、
報告が想定外の順序で来たとき（Logical Supervision）、報告の間隔が異常に長い・短いとき
（Deadline Supervision）に異常と判断します。AUTOSAR が定める 3 つの監視アルゴリズムです。

異常時の最終アクションは **実ハードウェアウォッチドッグによる本当の MCU リセット**です
（後述）。ログ出力だけのシミュレーションではなく、実機上で実際に再起動が発生します。

##### Alive Supervision の仕組み

`App_EngineManager_Run()` は 1 回の実行で `WdgM_CheckpointReached` を 2 回呼ぶため
（後述の START/END チェックポイント）、AliveCount は 3000ms 周期の Run() 呼び出しごとに 2 ずつ増えます。

```
監視対象 Runnable が呼ぶ:
  App_EngineManager_Run()  (3000ms 周期)
    ├→ WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START)  ← 開始
    └→ WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END)    ← 終了（"私は正常に完了した"）

WdgM_MainFunction (6000ms 周期) が評価:
  AliveCount >= WDGM_ENGINE_EXPECTED_ALIVE_INDICATIONS (1) ?
    YES → LOCAL_STATUS_OK  → "SE0 OK alive=4"   ← 6000ms の間に Run() が 2 回 × 2 チェックポイント
    NO  → LOCAL_STATUS_FAILED → "SE0 FAILED alive=0 [HW WDT reset pending]"

評価後: AliveCount = 0 にリセットして次サイクル開始
  （HW ウォッチドッグへのリフレッシュはここでは行わない。別タスクの
   WdgM_TriggerHwWatchdog が 1000ms 周期で判定結果を見て行う。後述）
```

##### Logical Supervision の仕組み

チェックポイントが「来たかどうか」だけでなく「正しい順序で来たか」を検査します。
`WdgM_CheckpointReached()` が呼ばれた瞬間に、直前のチェックポイントから今回のチェックポイントへの
遷移が許可遷移テーブル（`WdgM_PBCfg.c` の `WdgM_EngineTransitions[]`）に含まれるかを即座に確認するため、
`WdgM_MainFunction` の周期を待たずに違反を検出できます。

Entity 0（App_EngineManager_Run）で許可される遷移グラフ:

```
(起動直後)
    │ WDGM_CP_INITIAL → WDGM_CP_ENGINE_START
    ▼
┌─────────┐  START→END   ┌───────┐
│  START  │ ────────────→│  END  │
└─────────┘               └───────┘
    ▲                          │
    └────── END→START ─────────┘
          (次サイクル)
```

上記以外の遷移（例: START の連続呼び出し、起動直後に END が来る等）は順序違反として
即座に `LOCAL_STATUS_FAILED` にし、WARN ログを出力します。

##### Deadline Supervision の仕組み

Alive（来たかどうか）・Logical（正しい順序か）に続く、AUTOSAR の 3 つ目の監視
アルゴリズムです。チェックポイント間の**実際の経過時間**が許容範囲内かを検査します。

`WdgM_CheckpointReached()` が呼ばれた瞬間に、直前のチェックポイントからの経過時間
(`millis()` の差分) を計算し、許容テーブル（`WdgM_PBCfg.c` の `WdgM_EngineDeadlines[]`）
に設定された `[MinMs, MaxMs]` と比較します。範囲外（遅すぎる、または速すぎる）なら
`Logical Supervision` と同様に `WdgM_MainFunction` の周期を待たず即座に検出します。

Entity 0（App_EngineManager_Run）で監視する 2 区間:

| 区間 | 許容範囲 | 検出する異常 |
|---|---|---|
| START→END | 0〜500ms | `Run()` 1 回分の処理が異常に遅い（無限ループ・ブロッキング処理） |
| END→START | 2500〜3500ms | 次サイクルの `Run()` 呼び出しがタスク周期 3000ms から大きくズレている |

START→END は通常 RTE 読み取り・状態遷移・CAN 送信トリガのみで数 ms 程度のはずなので、
下限 (MinMs) は実質的な意味を持たず 0 にしています。一方 END→START はタスク周期
3000ms を中心に ±500ms の許容幅を持たせており、協調スケジューラ（他タスクの実行で
多少のジッタが生じる）を前提にした現実的な範囲です。

> Alive Supervision との違い: Alive は「6000ms の間に 1 回以上呼ばれたか」という
> 粗い判定しかできませんが、Deadline は「正確に何 ms かかったか」を見るため、
> Alive では検出できない「動いてはいるが異常に遅い」状態を検出できます。

##### 複数 Supervised Entity（Entity 0: ENGINE / Entity 1: WARNING）

これまで WdgM は `App_EngineManager_Run()`（Entity 0）1 つしか監視しておらず、
「エンティティごとの独立したローカル判定 → 全エンティティを見たグローバル判定」
という WdgM 本来の構成が実機で一度も動いていませんでした。`WdgM.c` のコアロジック
（`WdgM_MainFunction` / `WdgM_TriggerHwWatchdog` / `WdgM_CheckpointReached`）は
最初から `WdgM_Cfg->EntityCount` を見て汎用的にループする作りだったため、
`App_WarningIndicator_Run()`（500ms 周期の警告灯タスク）を Entity 1 として
追加登録するだけで、この構成を確認できます。

| | Entity 0 (ENGINE) | Entity 1 (WARNING) |
|---|---|---|
| 監視対象 Runnable | `App_EngineManager_Run` | `App_WarningIndicator_Run` |
| 周期 | 3000ms | 500ms |
| Alive 判定サイクル | 6000ms（共通） | 6000ms（共通） |
| サイクル内の期待呼び出し回数 | 期待値 約2回中 最小1回 | 期待値 約12回中 最小6回 |
| Deadline (START→END) | 0〜500ms | 0〜200ms |
| Deadline (END→START) | 2500〜4500ms | 300〜1500ms |

2 つのエンティティは周期もチェックポイント ID も完全に独立しており
（`WdgM_LastCheckpoint[]` 等はエンティティごとの配列）、どちらか一方の
Alive/Logical/Deadline Supervision が FAILED になっても、もう一方の判定には
一切影響しません。一方で **グローバル判定**（実際に HW ウォッチドッグを
リフレッシュするかどうか）は両エンティティの結果を集約します。

```
WdgM_TriggerHwWatchdog()（1000ms 周期）:
  for each entity (ENGINE, WARNING):
    WdgM_GetLocalStatus(entity) != OK ?
      YES → allOk = false; break
  allOk == true ?
    YES → WdgIf_SetTriggerCondition()   ← ENGINE・WARNING 両方が OK の場合のみリフレッシュ
    NO  → 何もしない                     ← どちらか一方でも FAILED ならリフレッシュを止める
```

> **END→START の許容上限には他モジュール由来の遅延を見込んだ余裕がある**:
> NvM の EEPROM 書き込み（`Dem_ReportErrorStatus()` からの DTC 確定時）は
> ブロッキング処理のため、DTC 確定のたびに協調スケジューラが数百ms 単位で
> 止まりえます。これは WARNING タスク自身の異常ではなく他 BSW モジュール
> （Dem/NvM）由来の遅延のため、ENGINE・WARNING 双方の END→START 上限
> （下表参照）に実測値の約2倍の余裕を持たせています。
> 500ms 周期の WARNING は 3000ms 周期の ENGINE よりチェックポイント報告の
> 間隔が短いため、この種の一時的なブロッキングの影響を相対的に受けやすい点に
> 留意してください。
>
> `Os_SchedulerStep()` は各タスクの周期判定のたびに時間源（Os 専用の Gpt
> チャネル、`Os_GetTimeMs()` 経由の `Gpt_GetTimeElapsed()`。2026-08 に
> `millis()` から置き換え、詳細は下記「Os のスケジューラティック」参照）を
> 都度取得し直します（ループ先頭で 1 回だけ取得して使い回す実装だと、同一
> スキャン内で他タスクがブロッキングした際に後続タスクの `Os_LastRunMs[]`
> へ不正確な時刻が記録され、Deadline 判定を誤らせます）。また `Os_SetTaskActive()` は
> タスクを無効→有効へ切り替える瞬間に `Os_LastRunMs[]` を現在時刻へリセット
> します。これにより、長時間無効化されていたどのタスク（SHUTDOWN 中に
> 停止していた `WdgM_MainFunction` を含む）も、再開直後は必ずフルの周期を
> 待ってから初めて実行・評価されます。これらの設計に至った実機不具合の経緯は
> [DEVLOG](docs/DEVLOG.md#wdgm-deadline-supervision-上限緩和と-os_schedulerstep-のバグ) を参照。

##### HW ウォッチドッグ連携（実際の MCU リセット）

WdgM は実ハードウェアウォッチドッグと連携していますが、直接は触れません。
`WdgM → WdgIf（ディスパッチ層）→ Wdg（下位ドライバ）→ Wdg_Hw（Renesas RA
の WDT ライブラリをラップする HAL 層）` という 4 層構成を経由します
（NvM → MemIf → Fee → Fee_Hw と同じ構成。WdgIf は実 AUTOSAR 仕様上、
下位ドライバが Wdg 1 個のみの構成では単なるパススルーでよいとされる
（[SWS_WdgIf_00018]）が、MemIf と同じ理由でチェック自体は残している）。
シミュレーションではなく、実機上で実際にリセットが発生します。

判定（Alive/Logical/Deadline Supervision）とリフレッシュ（trigger）は
**意図的に別々の周期**で動きます。

```
EcuM_Init() 内、WdgM_Init() より前:
  Wdg_Init(&Wdg_Config)   ← コンフィグ（タイムアウト値）を記録するのみ。
                            HW にはまだ触れない（初期化処理自体が HW
                            ウォッチドッグのタイムアウトに巻き込まれないため）

WdgM_Init()（起動シーケンス末尾、Os_Init の直前）:
  WdgM_EnableHwWatchdog()
    → WdgIf_SetMode(WDGIF_DEVICE_0, WDGIF_FAST_MODE)
      → Wdg_SetMode(WDGIF_FAST_MODE)
        → Wdg_Hw_Enable(timeoutMs)   ← HW ウォッチドッグを 4000ms タイムアウトで有効化

WdgM_MainFunction()（6000ms 周期、判定のみ）:
  各エンティティの Alive/Logical/Deadline を評価し WdgM_AliveStatus 等に反映する。
  1 つでも FAILED な判定サイクルが続くたびに WdgM_ExpiredCycleCount を進め、
  WDGM_EXPIRED_SUPERVISION_CYCLE_TOL（既定 2）回を超えて初めて
  WdgM_GlobalStopped を立てる（詳細は次項）。ここでは HW ウォッチドッグに触れない。

WdgM_TriggerHwWatchdog()（1000ms 周期、リフレッシュのみ）:
  WdgM_GlobalStopped が立っていない ?
    YES → WdgIf_SetTriggerCondition(WDGIF_DEVICE_0, WDGM_HW_WATCHDOG_TIMEOUT_MS)
            → Wdg_SetTriggerCondition() → Wdg_Hw_Refresh()
            ← リフレッシュ。タイマが 0 から再カウント開始
    NO  → 何もしない              ← リフレッシュされず、カウントが進み続ける

リフレッシュされないまま 4000ms 経過 → HW が MCU を強制リセット
  → setup() から再起動（DET ログも最初から出力される）
```

**Wdg_SetMode(WDGIF_OFF_MODE) は常に失敗する**: `WdgM_DisableHwWatchdog()`
（POST_RUN 遷移時に呼ばれる）は内部で `WdgIf_SetMode(WDGIF_DEVICE_0,
WDGIF_OFF_MODE)` を呼ぶが、Renesas RA4M1 の IWDT は一度有効化すると
無効化する手段がないため、`Wdg_SetMode()` は常に `E_NOT_OK` を返す
（実 AUTOSAR の拡張プロダクションエラー `WDG_E_DISABLE_REJECTED` に相当する
状況。本プロジェクトはプロダクションエラーの仕組み自体を持たないため
`DET_LOGW` のみで通知する）。`WdgM_DisableHwWatchdog()` はこの戻り値を
無視し、`WdgM_SupervisionSuppressed` フラグを立てることで目的を達成する
（HW が物理的に無効化されたかどうかには依存しない設計。詳細は WdgM.c の
「HW ウォッチドッグ連携」コメント参照）。

**なぜ判定サイクルとリフレッシュ周期を分けているか:**
当初は AVR の `wdt_enable(WDTO_8S)`（8000ms）を前提に、`WdgM_MainFunction`
（6000ms 周期）が判定とリフレッシュを両方担っていました（タイムアウトが
監視サイクルより長ければそれで十分機能する）。しかし Arduino Uno R4 WiFi
（Renesas RA4M1）移行時、RA の IWDT（独立ウォッチドッグ）は最大タイムアウトが
約 5592ms しかなく、6000ms の判定サイクルに直接リフレッシュを同期させることが
仕様上不可能でした（判定が終わる前にタイムアウトしてしまう）。

これは実車の AUTOSAR WdgM が、Wdg への trigger 周期と `WdgMSupervisionCycle`
（判定周期）を別々に設定できる設計になっているのと同じ理由です。そこで本実装も
リフレッシュ専用の軽量タスク `WdgM_TriggerHwWatchdog`（1000ms 周期、判定結果を
参照するだけ）を新設し、判定は従来通り 6000ms 周期のまま、リフレッシュだけを
HW タイムアウト（4000ms）に対して十分短い周期で行うようにしました。
副次効果として、Logical/Deadline 違反発生からリフレッシュ停止までの遅延も
最大 6000ms → 最大 1000ms に縮まっています。

##### グローバルレベルの EXPIRED 許容サイクル

AUTOSAR 仕様（`[SWS_WdgM_00119]`〜`[SWS_WdgM_00121]`）は、Global Supervision
Status が `WDGM_GLOBAL_STATUS_OK`・`FAILED`・`EXPIRED` のいずれであっても
`WdgIf_SetTriggerCondition`（リフレッシュ相当）を同一に呼び続けることを要求して
おり、リフレッシュを 0（停止）にしてよいのは `[SWS_WdgM_00122]`
`WDGM_GLOBAL_STATUS_STOPPED` に到達したときだけです。STOPPED に到達するには
`WdgMExpiredSupervisionCycleTol`（グローバルレベルの EXPIRED 許容サイクル数）
分の判定サイクルを消費する必要があり（`[SWS_WdgM_00216]`/`[SWS_WdgM_00217]`
等）、単発の異常でいきなりリフレッシュを止めることは想定されていません。

これを表現するため、`WdgM_ExpiredCycleCount`（グローバルレベルの連続 FAILED
判定サイクル数）と `WdgM_GlobalStopped`（AUTOSAR の `WDGM_GLOBAL_STATUS_STOPPED`
相当）を持たせています。本実装は Local Supervision Status を OK/FAILED の
2 値に簡略化しており（仕様本来の FAILED/EXPIRED の区別や、per-SE の
`WdgMFailedAliveSupervisionRefCycleTol` は実装していません）、その代わりに
この 1 段のグローバル許容サイクル数（`WDGM_EXPIRED_SUPERVISION_CYCLE_TOL`、
既定 2）だけを持たせています（この機構を追加するに至った実機不具合の経緯は
[DEVLOG](docs/DEVLOG.md#wdgm-グローバル-expired-許容サイクルの追加) 参照）。

```
WdgM_MainFunction()（6000ms 周期）:
  いずれかのエンティティが FAILED ?
    YES → WdgM_ExpiredCycleCount < TOL ?
            YES → WdgM_ExpiredCycleCount++          （猶予中、リフレッシュ継続）
            NO  → WdgM_GlobalStopped = 1             （猶予を使い切った）
    NO  → WdgM_ExpiredCycleCount = 0, WdgM_GlobalStopped = 0   （全回復）

WdgM_TriggerHwWatchdog()（1000ms 周期）:
  WdgM_GlobalStopped ?
    YES → リフレッシュしない（HW タイムアウト後に実際にリセット）
    NO  → リフレッシュする（FAILED 判定中でも、猶予の範囲内なら継続）
```

`WdgM_GetLocalStatus()` 自体（各エンティティの真の Supervision 結果）は
この猶予とは無関係に、これまで通り即座に正確な値を返します。変わるのは
「その判定結果を受けて実際に HW ウォッチドッグのリフレッシュを止めるまでの
猶予」だけです。

##### グローバル猶予カウンタは resume でリセットしない

Logical/Deadline Supervision のステータスはそもそも `WdgM_Init` まで回復しない
ラッチ式の設計です。それを評価するグローバル猶予カウンタ
（`WdgM_ExpiredCycleCount`/`WdgM_GlobalStopped`）を RUN 復帰のたびに回復させて
しまうと、本プロジェクトのようにボランタリスリープが数十秒おきに発生する環境では、
恒久的な違反があっても `WdgM_GlobalStopped` に到達する前に必ず次のスリープが来て
猶予がリセットされ続け、フェイルセーフが実質的に機能しなくなります（この
非対称性が引き起こした実機不具合の経緯は
[DEVLOG](docs/DEVLOG.md#wdgm-グローバル猶予カウンタを-resume-でリセットしてはいけなかった) 参照）。

そのため `WdgM_ResumeSupervision()` はこの 2 つをリセットせず、真に全エンティティが
OK に戻ったとき（`WdgM_MainFunction()` 末尾の自然な回復判定）にのみクリアします。
これにより、恒久的な違反は何回スリープ/ウェイクアップを挟んでも判定サイクル換算で
着実に猶予を消費し続け、いずれ確実に `WdgM_GlobalStopped` に到達します。

あわせて、`WdgM_SupervisionSuppressed`（POST_RUN 中の想定内の Alive 不足を無視する
フラグ）が立っている間は、グローバル猶予カウンタの判定自体を凍結します（進めも
回復させもしない）。POST_RUN 中の Rte_Engine/Rte_Warning 停止による想定内の
Alive 不足が、POST_RUN の頻度や長さ次第でグローバル猶予を無関係に消費してしまう
ことを防ぐためです。

```
WdgM_ResumeSupervision()（RUN 復帰のたびに呼ばれる）:
  AliveCount/AliveStatus  ← リセットする（POST_RUN 中の想定内の不足のため）
  ExpiredCycleCount/GlobalStopped ← リセットしない（恒久的な違反を見逃さないため）

WdgM_MainFunction()（6000ms 周期の判定サイクル）:
  いずれかのエンティティが FAILED ?
    かつ WdgM_SupervisionSuppressed 中 → 猶予カウンタは凍結（進めない）
    かつ 抑制されていない            → 猶予カウンタを消費（上記の通常フロー）
    （全 OK）                        → 猶予カウンタをクリア
```

##### ブートローダ起因の無限リセットループ対策

MCU によっては、短いタイムアウトで WDT が有効なまま再起動すると、ブートローダの
待機中に再度タイムアウトしてスケッチに到達できない「無限リセットループ」に陥る
既知の問題があります（AVR で顕著）。これを防ぐため `main.cpp` の `setup()` の
最初で `Mcu_Hw_ReadAndClearResetReason()`（リセット原因取得、レジスタはクリア
される）→ `Mcu_Hw_DisableWatchdogAtBoot()`（WDT 無効化）を実行し、
`WdgM_Init()` が後から安全なタイムアウトで再度有効化します。
Renesas RA は WDT が `WDT.begin()` を呼ぶまで動作しないため、
`Mcu_Hw_DisableWatchdogAtBoot()` は RA では no-op です。

##### 意図的な POST_RUN 移行での無効化／RUN 復帰での再有効化

EcuM が RUN から POST_RUN へ遷移する際、`WdgM_DisableHwWatchdog()` を呼んで
HW ウォッチドッグを無効化します。POST_RUN では BswM Rule 1 によって Rte_Engine /
Rte_Warning タスク（WdgM の監視対象、Entity 0/1 双方）が意図的に停止するため、
両エンティティとも Alive Supervision は必ず FAILED になります。

無効化するタイミングを **SHUTDOWN ではなく POST_RUN 移行時**にしているのには理由が
あります。WdgM はタスクとしては POST_RUN 中も継続するため（CanTp/Com/IoHwAb と同じ
BSW タスク）、無効化しないと POST_RUN 中（最大 `ECUM_POST_RUN_TIMEOUT_MS`=5000ms）に
Alive Supervision が FAILED を検出し続け、リフレッシュが止まったままになります。
HW ウォッチドッグのタイムアウト（4000ms）は SHUTDOWN への遷移
（POST_RUN 開始から最大 5000ms 後）より短いため、無効化しなければほぼ確実に
「正常なシャットダウン処理中」のはずが予期しないリセットを起こしてしまいます。
POST_RUN への移行そのものを無効化のタイミングにすることで、この競合を避けています。

ボランタリスリープからのウェイクアップ等で POST_RUN/SHUTDOWN から RUN へ復帰した
場合は、`WdgM_EnableHwWatchdog()` で再度有効化し、Alive Supervision による監視を
再開します（Bus-Off 回復は L1/L2 バックオフで無期限に継続し RUN を解放しないため、
この経路で POST_RUN に入ることはありません）。

##### RUN 復帰時のリセット（`WdgM_ResumeSupervision()`）

POST_RUN 中は Rte_Engine / Rte_Warning タスクが意図的に停止するため、この間の
チェックポイント未到達・Alive 不足はいずれも「想定内」です。しかし SHUTDOWN や
POST_RUN からそのまま RUN へ復帰した直後にこれを正しく扱わないと、停止していた
だけの期間を実際の違反と誤検出してしまいます。そのため `EcuM_RequestRUN()` が
POST_RUN/SHUTDOWN→RUN へ遷移する際に `WdgM_ResumeSupervision()` を呼び、
以下をリセットします。

```
WdgM_ResumeSupervision():
  WdgM_LastCheckpoint[]       ← WDGM_CP_INITIAL にリセット
                                （再開後最初の遷移を「基準なし」として扱い、
                                 POST_RUN の停止時間を Deadline 違反と誤検出しない）
  WdgM_LastCheckpointTimeMs[] ← 現在時刻にリセット
  WdgM_AliveCount[]           ← 0 にリセット
  WdgM_AliveStatus[]          ← WDGM_LOCAL_STATUS_OK にリセット
                                （POST_RUN 中に付いた FAILED ラッチを RUN 復帰後まで
                                 持ち越さない）
  WdgM_SkipNextAliveJudgment  ← 1
                                （次回の WdgM_MainFunction 呼び出し 1 回分だけ
                                 Alive 判定自体をスキップする。POST_RUN が
                                 WDGM_SUPERVISION_CYCLE_MS より大幅に短いと、
                                 各エンティティがまだ一度もチェックインできて
                                 いないうちに判定サイクルが来てしまうため）
```

Logical/Deadline のラッチ済み FAILED 状態、およびグローバル猶予カウンタ
（前述）はリセットしません。前者は「停止前に本当に違反していた事実」を、
後者は「恒久的な違反を見逃さない」ことをそれぞれ優先するためです。

この一連のリセット処理は、Deadline Supervision 追加時・CommunicationControl
実機検証時・短時間 POST_RUN のシナリオそれぞれで実際に HW ウォッチドッグ
リセットを引き起こした 3 件の不具合を経て現在の形になりました。詳しい経緯は
[DEVLOG: POST_RUN→RUN 復帰時の Deadline Supervision 誤検出](docs/DEVLOG.md#wdgm-post_runrun-復帰時の-deadline-supervision-誤検出)、
[Alive Supervision 誤検出](docs/DEVLOG.md#wdgm-post_runrun-復帰時の-alive-supervision-誤検出)、
[短時間 POST_RUN での誤検出](docs/DEVLOG.md#wdgm-短時間-post_run-での-alive-supervision-誤検出)
を参照してください。

##### 本プロジェクトでの失敗アクション

| 環境 | 失敗時のアクション |
|---|---|
| 本プロジェクト（Arduino Uno R4 WiFi） | HW ウォッチドッグのリフレッシュが止まり、最大 4000ms 後に実際に MCU がリセットされる |
| 実機製品 | 同様に HW ウォッチドッグがリフレッシュを停止し、タイムアウト後にシステムリセット（本実装と同じ仕組み） |

Alive・Logical・Deadline の 3 つの Supervision は、それぞれ独立したステータス
（`WdgM_AliveStatus[]` / `WdgM_LogicalStatus[]` / `WdgM_DeadlineStatus[]`）で
管理されます。`WdgM_GetLocalStatus()` と HW ウォッチドッグの refresh 判定は、
いずれか一つでも FAILED なら FAILED として扱います。

- `WdgM_AliveStatus` は周期ごとに再評価され、Alive 条件を満たせば OK に戻ります。
- `WdgM_LogicalStatus` / `WdgM_DeadlineStatus` は `WdgM_Init()` までラッチされ、
  `WdgM_MainFunction` の周期処理では自動的に OK へ戻りません（違反が起きたという
  事実は、その後 Alive 条件を満たしても消えないため）。

Alive・Logical・Deadline を独立したステータス配列に分けているのは、単一の
統合ステータスにすると「Logical 違反の直後に Alive 条件さえ満たせば違反が
消えてしまう」問題が起こるためです（経緯は
[DEVLOG](docs/DEVLOG.md#wdgm-alive-と-logical-のステータス統合バグ) 参照）。
なお HW ウォッチドッグが有効な今は、いずれの FAILED も前述の通り通常
MainFunction の次サイクルを待たずにリセットに至るため、ステータスの遷移
そのものをログで観測できる場面は限られます。

##### シリアルログ確認例

**正常時（6000ms ごと）:**
```
[19ms]    INFO  WdgM: HW watchdog enabled (4000ms)   ← WdgM_Init 内（起動時）
[1019ms]  DEBUG WdgM: HW watchdog refreshed          ← WdgM_TriggerHwWatchdog（1000ms 周期）
[2019ms]  DEBUG WdgM: HW watchdog refreshed
[3019ms]  DEBUG WdgM: HW watchdog refreshed
[4019ms]  DEBUG WdgM: HW watchdog refreshed
[6017ms]  DEBUG WdgM: SE0 alive OK alive=4   ← 3000ms 周期で Run() が 2 回、各 2 チェックポイント（WdgM_MainFunction、6000ms 周期）
```

**POST_RUN 移行時（意図的な停止。HW ウォッチドッグは無効化されるため実際のリセットは発生しない）:**
```
[30312ms] INFO  EcuM: ->POST_RUN timeout=5000ms
[30313ms] INFO  WdgM: HW watchdog disabled
[36312ms] WARN  WdgM: SE0 alive FAILED alive=0 (exp>=1) [HW WDT reset pending]  ← ソフト的には FAILED と記録されるが
                                                                                ← 無効化済みのため実際にはリセットしない
```

**Alive 失敗検知（RUN 中に実際の異常が起きた場合 — 後述の動作確認方法）:**
```
[5019ms]  DEBUG WdgM: HW watchdog refreshed          ← 最後に成功したリフレッシュ
[6017ms]  WARN  WdgM: SE0 alive FAILED alive=0 (exp>=1) [HW WDT reset pending]
[6018ms]  ERROR WdgM: HW watchdog NOT refreshed - reset imminent
                        ↑ 6000ms は 1000ms の倍数のため、同じ Os_SchedulerStep 内で
                          WdgM_MainFunction(Task7) → WdgM_TriggerHwWatchdog(Task10)
                          の順に実行され、直後に検知される
（最後の成功リフレッシュ(5019ms)から HW ウォッチドッグのタイムアウト 4000ms 後に到達）
[9019ms]  INFO  NvM: Init ok blocks=2     ← MCU が実際にリセットされ、setup() から再起動
[9020ms]  INFO  Port: Init pins=4
...
```

**Logical 失敗検知（後述の動作確認方法で START 呼び出しを止めた場合）:**
```
[3010ms] WARN  WdgM: SE0 logical FAILED cp 1->1 (unexpected) [HW WDT reset pending]
                                  └┘  └┘
                                  前回END  今回END（START がスキップされ END→END になった）
[4019ms] ERROR WdgM: HW watchdog NOT refreshed - reset imminent
            ↑ 次の WdgM_TriggerHwWatchdog（1000ms 周期）が最短 1000ms 以内に FAILED を
              検知して停止する（WdgM_MainFunction の 6000ms 周期を待たない）
（最後の成功リフレッシュから HW ウォッチドッグのタイムアウト 4000ms 後に MCU が実際に
 リセットされる。もし MainFunction の次サイクルがその前に実行されれば
 [6017ms] WARN WdgM: SE0 logical still FAILED (latched since violation) [HW WDT reset pending]
 も見えることがあるが、リフレッシュ停止の判定自体は WdgM_TriggerHwWatchdog が行う）
```

**Deadline 失敗検知（後述の動作確認方法で START→END に人為的な遅延を入れた場合）:**
```
[3011ms] WARN  WdgM: SE0 deadline FAILED cp 0->1 elapsed=1003 (exp 0..500) [HW WDT reset pending]
                                                  └┘            └──────┘
                                                  実際の経過時間   許容範囲 (START_TO_END)
（以降は Logical 失敗検知と同様、次の WdgM_TriggerHwWatchdog（最短 1000ms 以内）で
 リフレッシュが止まり、そこから HW ウォッチドッグのタイムアウト 4000ms 後に
 MCU が実際にリセットされる）
```

> **動作確認の前に**: 以下のテストは実機を**実際にリセット**させます。
> シリアルモニタには `EcuM: ->RUN` 等の起動ログが再び表示され、リセットされたことが
> わかります（EEPROM の DTC は NvM 経由で保持されるため消えません）。元に戻すには
> コメントを外して再度アップロードしてください。

**動作確認方法（Alive Supervision）:** `App_EngineManager.c` の END 側 `WdgM_CheckpointReached` をコメントアウト。
```c
/* (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END); */
```
起動後 6000ms で `alive=0` の FAILED ログが出てリフレッシュが止まり、最後の成功
リフレッシュから HW ウォッチドッグのタイムアウト（最大 4000ms）後に実際に MCU が
リセットされる。

**動作確認方法（Logical Supervision）:** START 側だけをコメントアウトすると、END→END の
順序違反が次の Run() 実行時に即座に検出される（MainFunction の周期を待たない）。
```c
/* (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START); */
```

**動作確認方法（Deadline Supervision）:** START チェックポイントの直後に `delay(1000)`
を追加すると、START→END の許容上限 500ms を超え、Run() 終了時の END チェックポイントで
即座に検出される（MainFunction の周期を待たない）。
```c
(void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);
delay(1000);  /* 動作確認用: 500ms の許容上限を超えさせる */
```

##### WdgM 設定（`WdgM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `WDGM_SUPERVISED_ENTITY_COUNT` | 2 | 監視対象エンティティ数（ENGINE/WARNING） |
| `WDGM_ENTITY_ENGINE` / `WDGM_ENTITY_WARNING` | 0 / 1 | App_EngineManager_Run / App_WarningIndicator_Run のエンティティ ID |
| `WDGM_SUPERVISION_CYCLE_MS` | 6000 ms | Alive Supervision サイクル（WdgM_MainFunction 周期と一致、両エンティティ共通） |
| `WDGM_EXPECTED_ALIVE_INDICATIONS` (ENGINE) / `WDGM_WARNING_EXPECTED_ALIVE_INDICATIONS` | 1 / 6 | サイクル内の最小 CheckpointReached 呼び出し回数 |
| `WDGM_CP_ENGINE_START` / `_END` | 0 / 1 | ENGINE の Run() 開始直後・終了直前のチェックポイント ID |
| `WDGM_CP_WARNING_START` / `_END` | 0 / 1 | WARNING の Run() 開始直後・終了直前のチェックポイント ID |
| `WDGM_CP_INITIAL` | 0xFF | 起動直後（まだチェックポイント未報告）を示す特別な遷移元 ID |
| `WDGM_DEADLINE_START_TO_END_MIN_MS` / `_MAX_MS`（ENGINE） | 0 / 500 ms | START→END（Run() 1 回分）の許容経過時間 |
| `WDGM_DEADLINE_END_TO_START_MIN_MS` / `_MAX_MS`（ENGINE） | 2500 / 4500 ms | END→START（次サイクルまでの間隔）の許容経過時間 |
| `WDGM_WARNING_DEADLINE_START_TO_END_MIN_MS` / `_MAX_MS` | 0 / 200 ms | WARNING の START→END 許容経過時間 |
| `WDGM_WARNING_DEADLINE_END_TO_START_MIN_MS` / `_MAX_MS` | 300 / 1500 ms | WARNING の END→START 許容経過時間 |
| `WDGM_EXPIRED_SUPERVISION_CYCLE_TOL` | 2 | グローバルレベルの連続 FAILED 判定サイクル許容回数（超過で `WdgM_GlobalStopped`） |
| `WDGM_HW_TRIGGER_CYCLE_MS` | 1000 ms | HW ウォッチドッグへの実際のリフレッシュ周期（WdgM_TriggerHwWatchdog 周期と一致）。判定サイクル（`WDGM_SUPERVISION_CYCLE_MS`）とは意図的に分離 |
| `WDGM_HW_WATCHDOG_TIMEOUT_MS` | 4000 ms | 実 HW ウォッチドッグのタイムアウト。`Wdg_PBCfg.c` がこの値を直接引用して `Wdg_Config.DefaultTimeoutMs` を組み立て、`Wdg_Hw_Enable(timeoutMs)`（`Wdg_Hw.cpp`、`WDT.begin(timeoutMs)`）まで渡る。`WDGM_HW_TRIGGER_CYCLE_MS` より十分長く設定すること（RA4M1 の IWDT 最大タイムアウト ≒5592ms 未満という制約もある） |

許可遷移グラフは `WdgM_PBCfg.c` の `WdgM_EngineTransitions[]`、Deadline 許容範囲テーブルは
同ファイルの `WdgM_EngineDeadlines[]`（いずれもポストビルド設定）で管理します。

<a id="nm"></a>
#### Nm（ネットワークマネジメント）

Nm (Network Management) は、実車の各 ECU がバス上に周期的な生存確認フレーム
（NM フレーム）を送信し、クラスタ内の全 ECU が送信を止めたときにのみバス
スリープへ移行できる、という合意形成（協調スリープ）の仕組みです。
本プロジェクトの `Nm.c` は `docs/4.3.1/AUTOSAR_SWS_CANNetworkManagement.pdf`
の CanNm 状態機械をほぼそのまま実装しており、他ノード（uds_tester が模擬する
「仮想他ECU」）からの NM フレーム受信が自ノードのスリープ判断に反映される
ことを実機で確認できます。

##### 状態機械

```
Bus-Sleep Mode ─────────Nm_NetworkRequest()/RxIndication(Prepare Bus-Sleep中)──┐
     ↑ Wait-Bus-Sleep Timer満了                                                │
Prepare Bus-Sleep Mode                                                         │
     ↑ NM-Timeout Timer満了(Ready Sleepから)                                   ▼
Network Mode: Ready Sleep State ←─Nm_NetworkRelease()── Normal Operation State
     │  (送信停止)                                              ↑ (送信継続)
     └──────Repeat Message Time満了(要求あり=Normal Operationへ)─┘
                        ↑
              Repeat Message State（Network Mode への進入は必ずここを経由）
```

3つのタイマ（`Nm_Cfg.h`）で駆動します。

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `NM_TIMEOUT_MS` | 3000 ms | NM-Timeout Timer。送信成功確認/受信のたびに再起動される「他ノードを含め通信が生きているか」の監視タイマ。Ready Sleep State でこれが満了すると Prepare Bus-Sleep Mode へ遷移。Repeat Message/Normal Operation State での満了は本来 Bus-Off 等の異常時にのみ起こる想定（[SWS_CanNm_00193]/[SWS_CanNm_00194]。再送信は伴わず、タイマ再起動と DET 報告のみ） |
| `NM_REPEAT_MESSAGE_MS` | 1500 ms | Repeat Message State の滞在時間 |
| `NM_WAIT_BUS_SLEEP_MS` | 1500 ms | Prepare Bus-Sleep Mode の滞在時間 |
| `NM_CYCLE_MS` | 1000 ms | `Nm_MainFunction()` の呼び出し周期。実 CanNm の Message Cycle Timer（`CanNmMsgCycleTime`、[SWS_CanNm_00032]/[SWS_CanNm_00040]。NM-Timeout Timer とは独立に Repeat Message/Normal Operation State の周期送信を駆動する専用タイマ）をこの呼び出し周期自体で兼用する簡略化 |

Message Cycle Timer と NM-Timeout Timer は独立している点に注意してください。
健全な通信中は毎周期の送信成功が NM-Timeout Timer を先回りして再起動し続ける
ため、`NM_E_NETWORK_TIMEOUT` は通常発生しません（実装当初この分離を誤り、
NM-Timeout Timer 満了そのものを再送信のトリガとしてしまっていたため、健全時
でも約 `NM_TIMEOUT_MS`〜`NM_TIMEOUT_MS+NM_CYCLE_MS` ごとに誤って
`NM_E_NETWORK_TIMEOUT` が発生し続け、かつそのせいで Ready Sleep State 進入
時点でタイマが既に古くなっており Prepare Bus-Sleep Mode へ異常に早く遷移する、
という2つの不具合が実機ログから見つかり修正した経緯があります）。

**対応除外**（実 AUTOSAR CanNm が持つが本プロジェクトでは実装しない機能）:
Partial Networking（7.11章）、NM Coordinator Sync（7.9.7章）、User Data
（7.9.2章）、Remote Sleep Indication（`CanNm_CheckRemoteSleepIndication`、
7.9.1章）、Passive Mode（7.9.3章）。

##### MeterStatus との違い（なぜ Com を経由しないか）

`MeterStatus` は ASW → RTE → Com → PduR → CanIf → Can という
通常のシグナル送信経路を通ります。一方 `Nm` はシグナル値を運ばず、実車の `CanNm` も
Com スタックを経由せず直接 `CanIf_Transmit()`/`CanIf_RxIndication()` をやり取り
するため、本プロジェクトの `Nm.c` も同じ構造にしています。

```
MeterStatus: App_EngineManager → Com_SendSignal → Com_RequestTxOnChange（フラグのみ）
               … 次回 Com_MainFunction() → PduR_Transmit → CanIf_Transmit → Can_Write

Nm(TX):      Nm_MainFunction/Nm_NetworkRequest等 → CanIf_Transmit → Can_Write
Nm(RX):      Can_Isr → CanIf_RxIndication → Nm_RxIndication
             （いずれも PduR・Com を経由しない）
```

##### フレームレイアウト（CAN ID 0x400 / DLC=2）

```
byte[0] : Control Bit Vector（Bit0=Repeat Message Request のみ使用。他ビットは
          対応除外の機能に対応するため常に 0）
byte[1] : Source Node Identifier（本 ECU は 0x01）
```

シグナル値ではなく生存確認そのものが目的のため、E2E 保護は付与していません
（実車でも NM フレームは通常 E2E 保護の対象にしません）。

##### ComM との連携（エッジトリガ方式）

```
ComM_BusSMIndication() がチャネルモードを確定させるたびに:
  FULL_COM へ変化 → Nm_NetworkRequest()
  NO_COM   へ変化 → Nm_NetworkRelease()
```

以前は `Nm_MainFunction()` が毎周期 `ComM_GetCurrentComMode()` をポーリングして
送信可否だけを判断する簡易設計でしたが、現在は ComM からのエッジトリガ通知を
受けて Nm 自身が状態機械とタイマを自律的に管理します。これにより、通信解放後も
すぐには送信を止めず（Ready Sleep State）、さらに NM-Timeout Timer +
Wait-Bus-Sleep Timer の間は状態機械上の待機を続けるという、実車と同じ「猶予期間」
が生まれます。

##### CanSM との連携（協調スリープ）

Nm が実際に Bus-Sleep Mode へ到達すると `CanSM_NmBusSleepMode()` を呼びます。
CanSM はこの通知を受けて初めて `Can_SetControllerMode(CAN_T_SLEEP)` を実行し、
CAN コントローラを物理的にスリープさせます（以前は `ComM_RequestComMode(NO_COM)`
の時点で即座にスリープしていましたが、Nm 導入に伴い変更しました）。

途中で他ノード（仮想他ECU）から NM フレームを受信すると、Network Mode 中の
NM-Timeout Timer が再起動される（実質的にスリープが延期される）ため、
「他ノードがまだ通信中の間は実際にはスリープしない」という協調スリープの本質を
実機で確認できます。

##### ログ例（協調スリープにより物理スリープが延期される様子）

```
[30315ms] INFO  ComM: ch0 ->mode=0                          # ComM_BusSMIndication(NO_COM)
[30318ms] INFO  Nm: -> Network Mode: Ready Sleep State (tx stopped)
[33320ms] INFO  Nm: -> Prepare Bus-Sleep Mode                # NM-Timeout Timer(3000ms)満了
[33850ms] INFO  CanIf: RX can=0x400                          # 仮想他ECU(node=0x02)のNMフレーム受信
[33853ms] INFO  Nm: RxIndication: node=0x02 woke us from Prepare Bus-Sleep
[33854ms] INFO  Nm: -> Network Mode: Repeat Message State    # スリープ延期
[35360ms] INFO  Nm: -> Network Mode: Ready Sleep State (tx stopped)
[38362ms] INFO  Nm: -> Prepare Bus-Sleep Mode
[39865ms] INFO  Nm: -> Bus-Sleep Mode                        # 今度は他ノードのNMフレームが来なかった
[39866ms] INFO  CanSM: Nm reached Bus-Sleep Mode -> CAN controller SLEEP
```

##### 実機検証（uds_tester）

`tools/uds_tester` の「周辺ECU」グループに「NM 仮想他ECU (0x400, node=0x02)」
ボタンがあります。「定期」送信を有効にした状態でエンジンを OFF のまま放置すると、
本 ECU がスリープへ向かう途中で仮想他ECUの NM フレームを受信し続けるため、
ログ上でスリープが延期され続けることを確認できます（周期送信を止めれば、
その後の Wait-Bus-Sleep Timer 満了で通常どおり Bus-Sleep Mode に到達します）。
「NM (0x400)」受信モニターで自ノード・仮想他ECU双方の NM フレーム（Repeat
Message Request ビットの有無を含む）を観測できます。

---
<a id="io-stack"></a>
### IO スタック（IoHwAb / Dio / Port / Adc）

SW-C はピン番号を直接知りません。RTE の Client/Server ポートを通じて IoHwAb の論理 API を呼び出し、
IoHwAb が Dio / Adc チャネルへ変換します。ピン方向の初期設定は Port が担い、Dio は値の読み書きのみ、
Adc はアナログ入力の読み取りのみを行います。

| 層 | モジュール | 本プロジェクトでの役割 |
|---|---|---|
| BSW | IoHwAb | Dio チャネル番号を隠蔽し SW-C に論理的な LED / ボタン / ADC API を提供。10ms 周期でデバウンス（40ms 確定）・ボタン固着検出・ADC 電圧低下を Dem 報告 |
|  | Dio | `Dio_WriteChannel` / `Dio_ReadChannel` で GPIO 値を読み書きする MCAL |
|  | Port | `Port_Init` でピン方向（OUTPUT / INPUT_PULLUP）を設定する MCAL |
|  | Adc | `Adc_ReadChannel` で 10-bit アナログ生値（0–1023）を読み取る MCAL |

```
SW-C (App_EngineManager / App_WarningIndicator)
  │ Rte_Call_LedRunning_SetLevel / Rte_Call_Button_GetLevel / Rte_Call_Adc_GetValue_mV 等
  ↓
IoHwAb（論理 API：LED / ボタン / ADC）
  │ Dio_WriteChannel / Dio_ReadChannel          │ Adc_ReadChannel
  ↓                                              ↓
Dio（値の読み書き）                             Adc（生値読み取り）
  │ Dio_Hw_WriteChannel / Dio_Hw_ReadChannel      │ Adc_Hw_ReadChannel
  ↓                                              ↓
Dio_Hw（Arduino digitalWrite / digitalRead）    Adc_Hw（Arduino analogRead）

Port_Init（起動時 1 回のみ）
  └→ Port_Hw_SetPinDirection(D6/D7/D8, OUTPUT)
     Port_Hw_SetPinDirection(D9, INPUT_PULLUP)
     (A0 はアナログ専用ピンのため Port 設定不要)
```

<a id="iohwab-module"></a>
#### IoHwAb

`IoHwAb_MainFunction`（10ms 周期）が Dio / Adc の生値取得からデバウンス・固着検出・
電圧監視までを一手に担い、SW-C へは確定済みの値だけを静的変数経由で返します。

<a id="debounce"></a>
##### デバウンス（積分カウンタ方式）

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
##### ボタン固着検出

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
##### ADC センサ電圧監視

`IoHwAb_MainFunction` が 10ms 周期で `Adc_ReadChannel` を呼び出し、10-bit 生値を mV へ変換して
電圧低下を Dem へ報告します。`Dio_ReadChannel` と同様に、ADC アクセスも `IoHwAb_MainFunction` に
集約し、`IoHwAb_Adc_GetValue_mV` は変換済みの静的変数を返すだけにしています
（チャネル設定は下記「[Adc](#adc-module)」章の `Adc_Cfg.h` 参照）。

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
##### IoHwAb API 一覧（`IoHwAb.h`）

| 関数 | 呼び出し元（RTE 経由） | 動作 |
|------|----------------------|------|
| `IoHwAb_Init()` | EcuM_Init | 全 LED を消灯、カウンタをリセット |
| `IoHwAb_LedRunning_SetLevel(level)` | App_WarningIndicator | D6 を点灯 / 消灯 |
| `IoHwAb_LedFault_SetLevel(level)` | App_WarningIndicator | D7 を点灯 / 消灯 |
| `IoHwAb_Led_SetLevel(level)` | App_WarningIndicator | D8 (ABS LED) を点灯 / 消灯 |
| `IoHwAb_MainFunction()` | Os Task 6 (10ms) | デバウンスサンプリング・固着検出・ADC サンプリング |
| `IoHwAb_Button_GetLevel(&level)` | App_EngineManager | デバウンス済み押下状態を返す |
| `IoHwAb_Adc_GetValue_mV(&mv)` | App_EngineManager | 変換済み ADC 電圧値 [mV] を返す |

<a id="dio-module"></a>
#### Dio

<a id="channel-assignment"></a>
##### チャネル割り当て（`Dio_Cfg.h`）

| 定数 | Dio チャネル | Arduino ピン | 機能 | Port 方向 |
|------|------------|-------------|------|----------|
| `DIO_CHANNEL_LED_RUNNING` | 6 | D6 | RUNNING 灯（RUNNING 中点灯） | OUTPUT |
| `DIO_CHANNEL_LED_FAULT` | 7 | D7 | FAULT 灯（FAULT 中 500ms 点滅） | OUTPUT |
| `DIO_CHANNEL_LED_WARNING` | 8 | D8 | ABS 警告灯（AbsActive=1 で点灯） | OUTPUT |
| `DIO_CHANNEL_BUTTON` | 9 | D9 | 警告確認ボタン（FAULT→OFF 遷移） | INPUT_PULLUP |

ピン番号の変更は `Dio_Cfg.h` の定数を変えるだけで完了します（IoHwAb や SW-C の変更不要）。

<a id="port-module"></a>
#### Port

`Port_Init` が起動時に一度だけ、上記チャネル割り当て表の「Port 方向」列に従って
D6/D7/D8 を OUTPUT、D9 を INPUT_PULLUP に設定します（A0 はアナログ専用ピンのため
Port 設定不要）。以降 Port 自身が呼ばれることはありません。

<a id="adc-module"></a>
#### Adc

<a id="adc-channel-config"></a>
##### チャネル設定（`Adc_Cfg.h`）

| 定数 | 値 | 意味 |
|------|-----|------|
| `ADC_CHANNEL_SENSOR` | 0（A0） | アナログセンサ入力チャネル |
| `ADC_RESOLUTION_MAX` | 1023 | 10-bit ADC の最大生値 |
| `ADC_REF_VOLTAGE_MV` | 5000 | 基準電圧（5V） |

このチャネルを読み取り mV へスケーリングする処理（オーバーフロー対策込み）は
`IoHwAb_MainFunction` が行います。詳細は上記「[IoHwAb](#iohwab-module)」章の
ADC センサ電圧監視を参照してください。

<a id="application"></a>
### アプリケーション（App_EngineManager / App_WarningIndicator）

ASW（Application Software）層の SW-C（Software Component）2 つで構成されます。
各 SW-C は RTE ポート経由でシグナルを受け取り、IoHwAb ポート経由で LED / ボタンを操作します。
EcuM の POST_RUN 遷移時に Rte_Engine タスクと Rte_Warning タスクが停止し、SW-C も停止します。

| 層 | モジュール | 本プロジェクトでの役割 |
|---|---|---|
| ASW | App_EngineManager | エンジン状態遷移（OFF / STARTING / RUNNING / FAULT）・DTC 登録・CAN TX 要求。OFF 継続を検知して ComM へ通信不要（NO_COM）を要求するボランタリスリープ判断も担う |
|  | App_WarningIndicator | 3 LED 独立制御（D6=RUNNING / D7=FAULT 点滅 / D8=ABS） |

<a id="engine-state-machine"></a>
#### エンジン状態遷移

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

<a id="app-warning-indicator"></a>
#### App_WarningIndicator（警告灯 SW-C）

`Rte_ScheduleWarningIndicator` タスクが 500ms 周期で `App_WarningIndicator_Run` を起動します。
3 つの LED は互いに独立して制御され、状態の組み合わせを同時に表現できます。

| LED | ピン | 点灯条件 | 制御 API |
|-----|------|---------|---------|
| RUNNING 灯 | D6 | `EngineState == RUNNING` かつ `FIM_FID_RUNNING_LED` 許可中 | `IoHwAb_LedRunning_SetLevel` |
| FAULT 灯 | D7 | `EngineState == FAULT`（500ms 点滅） | `IoHwAb_LedFault_SetLevel`（毎 Runnable でトグル） |
| ABS 灯 | D8 | `AbsActive == 1` | `IoHwAb_Led_SetLevel` |

FAULT 中に AbsActive=1 のフレームを受信すると D7 が点滅しつつ D8 も同時に点灯します。
POST_RUN 遷移後は Rte_Warning タスクが停止し、LED は消灯状態で固定されます。

<a id="serial-log-example"></a>
## シリアルモニタ出力例

出力フォーマット: `[<起動からの経過ms>ms] LEVEL TAG: メッセージ`
LEVEL は ERROR / WARN  / INFO  / DEBUG の 5 文字固定幅で列が揃います。

```
# 起動シーケンス（2 回目以降）
[1ms] INFO  NvM: Init ok blocks=2
[2ms] INFO  Port: Init pins=4             # ピン方向設定（D6/D7/D8 を OUTPUT、D9 を INPUT_PULLUP に）
[3ms] INFO  Can: Init ok
[4ms] INFO  CanIf: Init ok TX=3 RX=3
[5ms] INFO  PduR: Init ok RX=3 TX=2
[6ms] INFO  Com: Init ok RX=2 TX=1 sig=7
[7ms] INFO  CanTp: Init ok
[8ms] INFO  Dcm: Init ok
[9ms] INFO  Dem: Init NvM restored ev=9   # 前回の DTC を EEPROM から復元
[10ms] INFO  CanSM: Init                  # NO_COM 状態で初期化
[11ms] INFO  ComM: Init ch=1
[12ms] INFO  CanSM: ->FULL_COM            # CanSM_RequestComMode 成功
[12ms] INFO  ComM: ch0 ->mode=2           # ComM_BusSMIndication(FULL_COM) → EcuM_RequestRUN
[13ms] INFO  Nm: Init ok node=0x01
[14ms] INFO  AppEng: Init->OFF
[15ms] INFO  IoHwAb: Init                 # LED 消灯（ピン方向は Port_Init 済み）
[16ms] INFO  WarnInd: Init
[17ms] INFO  BswM: Init ok rules=6        # ルールエンジン初期化
[18ms] INFO  WdgM: Init ok entities=2     # Alive Supervision 初期化（監視対象 2 エンティティ: ENGINE/WARNING）
[18ms] INFO  WdgM: HW watchdog enabled (4000ms)  # 実ウォッチドッグ有効化（初期化完了後）
[19ms] INFO  Os: Init ok tasks=16         # タスクスケジューラ起動（全モジュール初期化後）
[19ms] INFO  EcuM: ->RUN                  # 全モジュール初期化完了 → RUN フェーズへ

# 0x100 受信時（EngineOnFlag=1, Speed=500rpm）
[102ms] INFO  Can_Hw: RX OK id=0x100 dlc=4 [01 F4 00 80]
[103ms] INFO  CanIf: RX can=0x100 pdu=0
[104ms] INFO  PduR: RxInd src=0 mod=0 dst=0
[105ms] INFO  Com: RX iPdu=0 [01 F4 00 80]

# 0x110 受信時（VehicleSpeed=100km/h, BrakeActive=0, AbsActive=0, Counter=0, E2E OK）
[110ms] INFO  Can_Hw: RX OK id=0x110 dlc=5 [XX 00 27 10 00]
[111ms] INFO  CanIf: RX can=0x110 pdu=2
[112ms] INFO  PduR: RxInd src=2 mod=0 dst=1
[113ms] INFO  Com: RX iPdu=1 [XX 00 27 10 00]

# 3 秒周期の Runnable 起動 → OFF→STARTING
[3010ms] INFO  AppEng: OFF->STARTING

# 500ms 周期の警告灯 Runnable（初期は OFF）
[500ms]  INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]

# 次の 3 秒周期 → STARTING→RUNNING
[6010ms] INFO  AppEng: STARTING->RUNNING
[6011ms] INFO  Can: TX id=0x200 [02]              # byte[0]=EngineState(2=RUNNING)、E2E保護なし
[6012ms] INFO  Can_Hw: TX OK id=0x200 dlc=1 [02]
[6013ms] DEBUG AppEng: ADC=3260mV          # ADC 電圧（毎 Runnable サイクルで参考値ログ出力）
[6500ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:0]   # RUNNING → D6 点灯

# 6 秒周期の WdgM 評価（正常時）
[6017ms] DEBUG WdgM: SE0 OK alive=4              # Run() が 2 回 × START/END 2 チェックポイント
[7019ms] DEBUG WdgM: HW watchdog refreshed       # WdgM_TriggerHwWatchdog（1000ms 周期、別タスク）

# 0x110 受信時（AbsActive=1, Counter=1, E2E OK）→ ABS LED 点灯
[7000ms] INFO  Can_Hw: RX OK id=0x110 dlc=5 [XX 01 27 10 C0]
[7001ms] INFO  Com: RX iPdu=1 [XX 01 27 10 C0]
[7500ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:1]   # AbsActive=1 → D8 点灯（D6 も継続）

# EngineInfo 送信停止 → タイムアウト → FAULT
[5100ms] WARN  Com: RX timeout iPdu=0 (5000ms)   # Com_MainFunction が検出
[6010ms] WARN  AppEng: ->FAULT comm timeout       # 次の Runnable で E_NOT_OK を受け取り遷移
[6500ms] INFO  WarnInd: [RUN:0 FAULT:1 ABS:0]   # FAULT → D7 点滅（500ms ごとにトグル）

# FAULT 中にボタン押下 → デバウンス確定（40ms = 4 サンプル × 10ms）→ FAULT→OFF
[7010ms] INFO  IoHwAb: Button confirmed level=1  # 押下確定（IoHwAb_MainFunction が更新）
[9010ms] INFO  AppEng: FAULT->OFF btn=1          # App_EngineManager_Run が確定値を読み取り遷移
[9011ms] INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]   # 3 LED すべて消灯
# ボタン解放後も同様に 40ms 後に level=0 が確定する
[9100ms] INFO  IoHwAb: Button confirmed level=0  # 解放確定

# ボタンを 5 秒以上押しっぱなし → 固着検出 → DTC 0x000106 記録（limit=1 のため即座に確定）
[1000ms] INFO  IoHwAb: Button confirmed level=1  # 押下確定（40ms 後）
[6000ms] WARN  IoHwAb: Button stuck dtc=0x000106 # 5000ms 経過で固着判定
[6001ms] WARN  Dem: FAILED ev=5 dtc=0x000106     # EEPROM に保存
# ボタン解放 → PASSED 報告（TF ビットクリア・CDTC は残る）
[8000ms] INFO  IoHwAb: Button confirmed level=0
[8001ms] INFO  IoHwAb: Button stuck cleared
[8002ms] INFO  Dem: PASSED ev=5                  # testFailed クリア

# ADC センサ電圧が 1000mV 未満に低下 → 毎サイクル報告で 2 回目にデバウンス確定 → DTC 0x000107 記録
[12010ms] DEBUG AppEng: ADC=820mV                # 閾値未満（10ms 周期の IoHwAb_MainFunction が検出）
[12010ms] DEBUG Dem: ev=6 debounce=1 (PREFAILED)
[12020ms] WARN  Dem: FAILED ev=6 dtc=0x000107     # 2 回目（10ms 後）でデバウンス確定・EEPROM に保存
# 電圧が復帰すると毎サイクル PASSED 報告 → 2 回目でデバウンス確定（TF ビットクリア・CDTC は残る）
[15010ms] DEBUG AppEng: ADC=3300mV
[15020ms] INFO  Dem: PASSED ev=6

# 過熱検出（CoolantTemp=101℃）→ RUNNING→FAULT（1 回目は PRE-FAILED、まだ DTC 未確定）
[9010ms] WARN  AppEng: RUNNING->FAULT overheat=101
[9500ms] INFO  WarnInd: [RUN:0 FAULT:1 ABS:0]   # D7 点滅開始
[10000ms] INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]  # 500ms 後にトグル（D7 消灯）
[9011ms] DEBUG Dem: ev=0 debounce=1 (PREFAILED) # 単発報告のため確定には別機会でもう 1 回必要
# OFF→STARTING→RUNNING→再度過熱（2 回目）で初めてデバウンス確定
[18010ms] WARN  AppEng: RUNNING->FAULT overheat=101
[18011ms] WARN  Dem: FAILED ev=0 dtc=0x000101   # 2 回目でデバウンス確定・NvM 経由で EEPROM に保存
[18012ms] INFO  Dem: FreezeFrame ev=0 spd=1000 tmp=101 st=2  # 故障時のスナップショット（RAM のみ）

# UDS SID 0x19/04: 上記 ENGINE_OVERHEAT のFreezeFrame取得
[19500ms] INFO  CanTp: RX SF len=6
[19501ms] INFO  Dcm: req SID=0x19
[19502ms] INFO  Dcm: 19/04 dtc=0x000101 rec=1
[19503ms] INFO  CanTp: TX FF len=18        # 18 バイト → FF+CF×2 に分割
[19504ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [10 12 59 04 00 01 01 2D]

# Bus-Off 発生時（CAN バス切断 → TX エラー検出）
# TX 失敗のたびに EFLG 変化をログ出力（TXWAR→TXEP と TEC が積み上がる）
[15082ms] ERROR Can_Hw: TX FAIL id=0x200 eflg=0x05 (TXBO=0 TXEP=0 TXWAR=1)
[15083ms] DEBUG Can_Hw: EFLG=0x05 TXBO=0 TXEP=0 TXWAR=1 EWARN=1
# （3 秒ごとに TX 失敗が繰り返され TEC が積み上がる）
[15092ms] ERROR Can_Hw: TX FAIL id=0x200 eflg=0x15 (TXBO=0 TXEP=1 TXWAR=1)
[15093ms] DEBUG Can_Hw: EFLG=0x15 TXBO=0 TXEP=1 TXWAR=1 EWARN=1
# 5 回連続失敗でソフトウェア Bus-Off 検出（HW Bus-Off の補完）
[15094ms] WARN  Can: SW BusOff fallback: 5 consecutive TX failures
[15100ms] WARN  CanIf: ControllerBusOff ch=0
[15103ms] INFO  ComM: ch0 ->mode=1        # ComM_BusSMIndication(SILENT_COM)。RUN は維持（EcuM_RequestRUN/ReleaseRUN いずれも呼ばれない）
[15106ms] WARN  CanSM: BusOff detected! retry=0 (L1) recovery in 200ms
[15112ms] INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]  # EngineState が不定に

# 200ms 後に回復試行（アダプタ再接続済みなら正常復帰）
[15312ms] INFO  CanSM: BusOff: restart attempt 1 (L1, next in 200ms)
[15313ms] INFO  Dem: PASSED ev=7          # 回復成功を報告。limit=1 のため即座に確定（TF クリア）
[15314ms] INFO  ComM: ch0 ->mode=2        # ComM_BusSMIndication(FULL_COM) → EcuM_RequestRUN → RUN 維持
# → TX 成功 → 通常動作に復帰

# アダプタ未接続のまま L1 リトライ回数（既定 3）を超過 → 持続的な Bus-Off と判断し
# DTC を確定した上で L2（既定 5000ms）へ降格するが、回復試行そのものは無期限に継続する
# （AUTOSAR 仕様には「回復を諦めて停止する」状態が存在しないため、給電を切るまで
#  通信不能のままになるようなことはない）
[15909ms] INFO  ComM: ch0 ->mode=1        # ComM_BusSMIndication(SILENT_COM)。再度 Bus-Off のたびに呼ばれる
[15912ms] WARN  CanSM: BusOff detected! retry=3 (L2) recovery in 5000ms
[15913ms] ERROR CanSM: BusOff: L1(3) exceeded, degrade to L2 (5000ms)
[15914ms] WARN  Dem: FAILED ev=7 dtc=0x000108  # limit=1 のため即座に確定・EEPROM に保存
[15915ms] INFO  Dem: FreezeFrame ev=7 spd=... tmp=... st=...  # L2 降格時点のスナップショット
[15916ms] INFO  CanSM: BusOff: restart attempt 4 (L2, next in 5000ms)
[15917ms] INFO  Dem: PASSED ev=7          # 楽観的な再起動試行報告（CDTC/PDTC は保持される）
[15918ms] INFO  ComM: ch0 ->mode=2
# → 依然としてアダプタ未接続なら 5000ms 後に再び BusOff detected！が繰り返され、
#   実際にバスが復旧するまで L2 周期でのリトライが無期限に続く（EcuM は RUN のまま）

# UDS 診断要求 0x7E0（ReadDataByIdentifier DID=0x0101）
[9500ms] INFO  Can_Hw: RX OK id=0x7E0 dlc=8 [03 22 01 01 00 00 00 00]
[9501ms] INFO  CanIf: RX can=0x7E0 pdu=1
[9502ms] INFO  CanTp: RX SF len=3
[9503ms] INFO  Dcm: req SID=0x22
[9504ms] INFO  Dcm: 22 did=0x0101 len=2
[9505ms] INFO  CanTp: TX SF len=5
[9506ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [05 62 01 01 01 F4 00 00]

# UDS SID 0x19/02: DTC 一覧取得（2 件以上 → マルチフレーム応答）
[10000ms] INFO  CanTp: RX SF len=4
[10001ms] INFO  Dcm: req SID=0x19
[10002ms] INFO  Dcm: 19/02 mask=0xFF found=2
[10003ms] INFO  CanTp: TX FF len=11        # 11 バイト → FF+CF に分割
[10004ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [10 0B 59 02 2D 00 01 03]
# (Cangaroo から FC を受信後)
[10200ms] INFO  CanTp: RX FC fs=0          # ContinueToSend
[10201ms] INFO  CanTp: TX CF sn=1 pos=6
[10202ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [21 2C 00 01 04 2C 00 00]
[10203ms] INFO  CanTp: TX done

# UDS SID 0x14: defaultSession のまま ClearAllDTC を試みる → NRC 0x7F で拒否
# （Dcm_SidSessionTable[] による一元判定。ハンドラ内の SecurityAccess チェックにすら到達しない）
[10800ms] INFO  Dcm: req SID=0x14
[10801ms] ERROR Dcm: NRC sid=0x14 nrc=0x7F        # serviceNotSupportedInActiveSession

# extendedDiagnosticSession へ切替
[10850ms] INFO  Dcm: req SID=0x10
[10851ms] INFO  Dcm: 10 session=0x03

# UDS SID 0x14: extendedSession だが未アンロックで ClearAllDTC を試みる → NRC 0x33 で拒否
[10900ms] INFO  Dcm: req SID=0x14
[10901ms] ERROR Dcm: NRC sid=0x14 nrc=0x33        # securityAccessDenied

# UDS SID 0x27: requestSeed → sendKey でアンロック
[10950ms] INFO  Dcm: req SID=0x27
[10951ms] INFO  Dcm: 27/01 seed=0x3A12            # seed は millis() 由来、毎回変化する
[10952ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [04 67 01 3A 12 00 00 00]
# テスター側で key = seed XOR 0xA55A = 0x9F48 を計算して送信
[10970ms] INFO  Dcm: req SID=0x27
[10971ms] INFO  Dcm: 27/02 unlocked               # key 一致 → Level1 アンロック
[10972ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [02 67 02 00 00 00 00 00]

# UDS SID 0x14: 全 DTC クリア（アンロック済みのため成功）
[11000ms] INFO  Dcm: 14 ClearAllDTC
[11001ms] INFO  Dem: ClearAll ok
[11002ms] INFO  CanTp: TX SF len=1
[11003ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [01 54 00 00 00 00 00 00]

# UDS SID 0x14: ENGINE_OVERHEAT (DTC 0x000101) のみクリア
[12000ms] INFO  Dcm: 14 ClearDTC dtc=0x000101    # Dem_GetEventIdOfDTC で ev=0 を逆引き
[12001ms] INFO  Dem: Clear ev=0 ok               # 他の DTC のステータスは変化しない
[12002ms] INFO  Can_Hw: TX OK id=0x7E8 dlc=8 [01 54 00 00 00 00 00 00]

# UDS SID 0x27: key を 3 回連続で間違えるとロックアウト
[13000ms] INFO  Dcm: req SID=0x27
[13001ms] WARN  Dcm: 27/02 invalid key attempt=3
[13002ms] WARN  Dcm: 27 lockout 10000ms
[13003ms] ERROR Dcm: NRC sid=0x27 nrc=0x36        # exceededNumberOfAttempts
[13050ms] INFO  Dcm: req SID=0x27                 # ロックアウト中の requestSeed は拒否
[13051ms] ERROR Dcm: NRC sid=0x27 nrc=0x37        # requiredTimeDelayNotExpired

# S3 タイマ: ExtendedDiagnosticSession へ切替後、5 秒以上どの要求も来なかった場合
[20000ms] INFO  Dcm: 10 session=0x03             # ExtendedDiagnosticSession へ切替（S3 タイマ起動）
[25000ms] INFO  Dcm: S3 timeout -> session=Default  # 1000ms 周期の Dcm_MainFunction が検出
```

<a id="design-notes"></a>
## 設計上の注意点

<a id="c-cpp-boundary"></a>
### C / C++ 言語境界

| ファイル | 言語 | 理由 |
|---------|------|------|
| `Can_Hw.cpp` | C++ | MCP_CAN クラスのインスタンス化に placement new が必要 |
| `Det_Hw.cpp` | C++ | Arduino の `Serial` API を使用 |
| `Dio_Hw.cpp` | C++ | Arduino の `digitalWrite` API を使用 |
| `Port_Hw.cpp` | C++ | Arduino の `pinMode` API を使用 |
| `Wdg_Hw.cpp` | C++ | Renesas RA の WDT ライブラリ（`WDTimer` クラス、グローバルインスタンス `WDT`）を使用 |
| その他すべて | C | AUTOSAR CP の標準に準拠 |

C ファイルから C++ 関数を呼ぶすべてのヘッダに `extern "C"` ガードを設けています。

`Det_Hw.cpp` が唯一 `Serial.print()` を呼ぶファイルです。他の `.c` ファイルは `DET_LOG*` マクロのみを使います。

<a id="log-level"></a>
### ログレベルの抑制 (Det_Cfg.h)

`Det_Cfg.h` の `DET_LOG_LEVEL`（既定値 `LOG_I`）以下の重要度のログのみ出力されます
（`LogLevel` は数値が小さいほど重要度が高い: `LOG_E`=0 < `LOG_W` < `LOG_I` < `LOG_D`）。
既定では ERROR/WARN/INFO のみ出力し、DEBUG（例: IoHwAb の ADC 電圧低下デバウンス過程など、
毎サイクル出力されうる詳細ログ）を抑制します。全レベル出力したい場合は
`platformio.ini` の `build_flags` に `-D DET_LOG_LEVEL=LOG_D` を追加してください。

<a id="fixed-buffer-size"></a>
### 固定長バッファのサイズは設定定数から計算する

`Dcm_Cbk.c` の UDS 応答バッファ `Dcm_TxBuf` は、当初 `DEM_EVENT_COUNT`（その時点では 6）
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
### RX/TX で対称な入力検証

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
### 設定テーブルの一元管理

各モジュールの設定は対応する `*_PBCfg.c` ファイルで管理しています。

| 変更したい内容 | 編集ファイル |
|---|---|
| CAN ID・DLC（EngineInfo / AbsInfo など） | `CanIf_PBCfg.c` + `CanIf_Cfg.h` |
| シグナルのビット位置・エンディアン | `Com_PBCfg.c` + `Com_Cfg.h` |
| PDU ルーティングパス（RX/TX の対応関係） | `PduR_PBCfg.c` + `PduR_Cfg.h` |
| RTE ポート API（SW-C から見えるシグナル名） | `Rte.h` / `Rte.c` / `Rte_Type.h` |
| E2E チェック結果を Rte_IStatusType へ写像する分類の変更 | `Rte.c` の `Rte_MapE2EStatus()` |
| Com TX I-PDU の送信モード変更（DIRECT/MIXED/PERIODIC）・周期変更 | `Com_PBCfg.c` の該当 IPdu の `TxModeMode`/`TxPeriodMs`、周期定数は `Com_Cfg.h` |
| Com TMS（TxModeModeTrue への自動切り替え）変更・対象シグナル変更 | `Com_PBCfg.c` の該当 IPdu の `TxModeModeTrue`/`TxPeriodMsTrue`、対象シグナルの `TmsContributor`/`FilterX`/`Mask` |
| Com MDT（変化時送信の最小送信間隔）変更 | `Com_PBCfg.c` の該当 IPdu の `MinDelayMs`、周期定数は `Com_Cfg.h` |
| E2EMon（ネットワーク健全性テレメトリ）の集計対象・カウンタ追加 | `E2EMon.c` の `E2EMon_NotifyCheckResult()` |
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
| WdgM 時間監視（チェックポイント間の許容経過時間）の変更 | `WdgM_PBCfg.c` の `WdgM_EngineDeadlines[]` / 閾値は `WdgM_Cfg.h` の `WDGM_DEADLINE_*` |
| LED / ボタンのピン番号変更 | `Dio_Cfg.h`（`DIO_CHANNEL_LED_RUNNING` / `_LED_FAULT` / `_LED_WARNING` / `_BUTTON`） |
| ADC チャネル・分解能・基準電圧・電圧低下閾値 | `Adc_Cfg.h` / `IoHwAb.c`（`IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV`） |
| Dcm S3 タイマのタイムアウト時間変更 | `Dcm_Cfg.h` の `DCM_S3_TIMEOUT_MS` |
| Dcm SecurityAccess の鍵・試行回数・ロックアウト時間変更 | `Dcm_Cfg.h` の `DCM_SECURITY_KEY_MASK` / `DCM_SECURITY_MAX_ATTEMPTS` / `DCM_SECURITY_DELAY_MS` |
| SecurityAccess で保護するサービスの追加 | `Dcm_Cbk.c` の各ハンドラ先頭で `Dcm_SecurityLevel == 0U` をチェック（`Dcm_HandleClearDtc` 参照） |
| SID にセッション制約を追加・変更 | `Dcm_Cbk.c` の `Dcm_SidSessionTable[]` に行を追加（`DCM_SESSION_MASK_DEFAULT` / `_EXTENDED` / `_ALL`） |
| IOControl (0x2F) 対象ランプの追加 | `Dcm_Cfg.h` に `DCM_DID_*` を追加、`Dcm_LampIdOfDid()` に分岐を追加、`Rte.h` の `Rte_LampIdType` に列挙値を追加し `Rte_Call_*_SetLevel()` を `Rte_Lamp_ArbitrateAndWrite()` 経由にする |
| ComM ユーザの追加 | `ComM_Cfg.h` に `COMM_USER_*` を追加し `COMM_USER_COUNT` を更新。要求元モジュールから `ComM_RequestComMode(新ユーザID, モード)` を呼ぶだけで `ComM_RequestComMode()` の集約ロジックが自動的に対応する |
| FiM の抑止対象機能・イベントの追加・変更 | `FiM_Cfg.h` に `FIM_FID_*` を追加し、`FiM_PBCfg.c` の `FiM_Functions[]` に行を追加 |
| CanSM Bus-Off L1/L2 バックオフの変更 | `CanSM_Cfg.h` の `CANSM_BUSOFF_RECOVERY_L1_MS` / `_L2_MS` / `CANSM_BUSOFF_L1_TO_L2_COUNT` |
| ウェイクアップ検証タイムアウトの変更 | `CanSM_Cfg.h` の `CANSM_WAKEUP_VALIDATION_MS`（既定 2000ms） |
| ボランタリスリープに入るまでのエンジン OFF 継続時間の変更 | `App_EngineManager.c` の `APP_ENGINE_SLEEP_OFF_CYCLES`（Run 周期3000ms×既定5=15秒） |


<a id="unit-test"></a>
### 単体テスト（ホスト上でのロジック検証）

実 HW（UNO R4）を使わず、Bsw モジュールのロジックだけをホスト PC 上で
GoogleTest により検証する `[env:native]` 環境を用意している
（`platformio.ini` 参照）。対象は `build_src_filter` で個別指定した
モジュールのみで、現状は `src/Bsw/Gpt/Gpt.c` が対象（`test/test_gpt/`）。
Hal/Det/SchM_Hw 等の実 HW 依存部分は `test/test_gpt/fake_*.c` の
フェイク実装に差し替え、`Gpt_OnTick()`（本来 ISR から呼ばれる関数）を
テストから直接呼ぶことで、実割り込みなしに状態機械を駆動している。

```bash
# ホスト上でビルド・実行（GoogleTest、実 HW 不要）
pio test -e native
```

事前準備として、ホスト用の C++17 対応 MinGW-w64/GCC がインストールされ
`g++` に PATH が通っている必要がある（`uno_r4` 環境のビルドとは別の
ネイティブコンパイラ）。初回実行時に GoogleTest ライブラリと `native`
プラットフォームを自動ダウンロードする。

新しい Bsw モジュールのテストを追加する場合は `test/<module>/` を新設し、
対象モジュールに応じて `[env:native]` の `build_src_filter` と `-I` を
調整する（`test/test_gpt/` を雛形として流用できる）。

> **Windows 環境固有の注意（MinGW-w64 のランタイム不整合）**:
> 一部の MinGW-w64 配布物（msvcrt ランタイム版）では、GoogleTest の
> death test 機構経由で `libmingw32.a` 内の UCRT 専用シンボル
> (`__imp_quick_exit`/`__imp__Exit`) が要求され、
> `undefined reference to __imp_quick_exit` 等でリンクに失敗することがある。
> `test/test_gpt/win_quick_exit_stub.cpp` はこの環境向けの回避コード
> （該当シンボルを `std::exit()` へ委譲する自前スタブで満たす）。
> UCRT ランタイム版の MinGW-w64 を使っている場合は本来不要で、
> `__imp_quick_exit`/`__imp__Exit` の多重定義エラーが出たら削除すること。

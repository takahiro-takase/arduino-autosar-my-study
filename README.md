# arduino-autosar-my-study

Arduino + MCP2515 を用いて AUTOSAR CP の BSW CAN スタックを学習目的で実装したプロジェクトです。
ARXML や設定ツールは使用せず、コードで階層構造・型定義・設定テーブルを再現しています。

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

- **RX（CAN ID 0x100）**: エンジン ECU から回転数・水温・ON フラグを受信
- **RX（CAN ID 0x110）**: ABS ECU から車速・ブレーキ作動・ABS 作動フラグを受信
- **TX（CAN ID 0x200）**: エンジン状態（OFF / STARTING / RUNNING / FAULT）を定期送信
- **診断 RX（CAN ID 0x7E0）**: UDS 診断要求を受信（ISO 14229-1 / ISO 15765-2）
- **診断 TX（CAN ID 0x7E8）**: UDS 診断応答を送信（マルチフレーム対応）
- **RUNNING LED（D6）**: ENGINE_STATE_RUNNING のとき点灯
- **FAULT LED（D7）**: ENGINE_STATE_FAULT のとき 500ms 周期で点滅
- **ABS LED（D8）**: ABS 作動（AbsActive=1）のとき点灯
- **警告確認ボタン（D9）**: FAULT 状態でボタンを押すと FAULT→OFF に遷移（ドライバーが警告を確認したことを通知）

## アーキテクチャ

### 層構造

```
ASW ─── App_EngineManager / App_WarningIndicator
RTE ─── Rte（ポートベース S/R API）
OS  ─── Os（タイムトリガスケジューラ）
BSW ─── EcuM / BswM / WdgM / ComM / CanSM / Com / PduR / CanIf / Can
        CanTp / Dcm / Dem / NvM / IoHwAb / Dio / Port / Adc / SchM / Det
HAL ─── Can_Hw / Dio_Hw / Port_Hw / Adc_Hw（C++ のみ）
```

各層は上位層のヘッダのみに依存し、下位層の実装詳細を知りません。

### モジュール一覧

| 層 | モジュール | AUTOSAR 仕様 | 本プロジェクトでの役割 |
|---|---|---|---|
| ASW | App_EngineManager | — | エンジン状態遷移（OFF / STARTING / RUNNING / FAULT）・DTC 登録・CAN TX 要求 |
|  | App_WarningIndicator | — | 3 LED 独立制御（D6=RUNNING / D7=FAULT 点滅 / D8=ABS） |
| RTE | Rte | — | ポートベース S/R API。複数 SW-C が同一シグナルを独立ポートで受信 |
| OS | Os | SWS_Os | タイムトリガスケジューラ。タスクごとに周期を設定し `Os_SchedulerStep()` で到来タスクを順次実行 |
| BSW | EcuM | SWS_EcuStateManager | ECU ライフサイクルを STARTUP → RUN → POST_RUN → SHUTDOWN の状態マシンで管理。`EcuM_RequestRUN` / `EcuM_ReleaseRUN` で RUN フェーズを調停 |
|  | BswM | SWS_BswM | EcuM / ComM のモード変化をルールテーブルで受け取り `Os_SetTaskActive()` でタスクを有効・無効化するルールエンジン。POST_RUN 中はアプリタスクのみ停止し BSW タスクは継続 |
|  | WdgM | SWS_WdgM | Supervised Entity の Alive Supervision（呼び出し回数を6000msごとに評価）と Logical Supervision（チェックポイント順序を即時検証）を管理。異常時は AVR 実 HW ウォッチドッグのリフレッシュを止め、実際に MCU をリセットする |
|  | ComM | SWS_ComM | CAN バスの通信モード（NO_COM / FULL_COM）を管理し CanSM へ要求。`ComM_BusSMIndication` で EcuM の RUN 要求を操作 |
|  | CanSM | SWS_CanSM | Bus-Off 発生時の回復シーケンス（最大 3 回リトライ）を実施。回復断念・成功時に `ComM_BusSMIndication` を呼ぶ。回復断念は Dem にも DTC として報告（limit=1 のため即座に確定） |
|  | Com | SWS_Com | シグナルのビット単位パック／アンパックと受信デッドライン監視（タイムアウト検出） |
|  | PduR | SWS_PduR | 受信 PDU を Com へ、送信 PDU を CanIf へルーティング。通信スタックの配管役 |
|  | CanIf | SWS_CanIf | CAN ID ↔ 論理 PDU のマッピング。上位層は CAN ID を知らず PDU ID で通信 |
|  | Can | SWS_Can | MCP2515 の送受信・Bus-Off 検出を担う MCAL 最下層。HW を直接操作する唯一のモジュール |
|  | CanTp | SWS_CanTp | ISO 15765-2 のフレーム分割（FF/CF）と再組立。8 バイトを超える UDS 応答を実現 |
|  | Dcm | SWS_Dcm | UDS 診断サービス処理（SID 0x10 / 0x11 / 0x14 / 0x19 / 0x22 / 0x27 / 0x3E）。S3 タイマでセッションを自動失効。SID×セッション許可テーブルで 0x14/0x27 を extendedSession 限定とし、SecurityAccess (0x27) で ClearDTC をシード・キー認証保護 |
|  | Dem | SWS_Dem | 診断イベントを DTC として管理。カウンタベースのデバウンスで確定し、NvM 経由で EEPROM に永続化。デバウンス確定 FAILED 時に FreezeFrame（RAM のみ）を記録。再故障せず複数回の操作サイクルを経ると経年回復（Aging）で CONFIRMED を自動解除 |
|  | FiM | SWS_FiM | Dem が確定（CONFIRMED）した DTC をもとにアプリ機能（FID）の実行許可を判定。100ms 周期で再評価し、結果を ASW へ `Rte_Call_FiM_GetFunctionPermission` で公開 |
|  | NvM | SWS_NvM | EEPROM の読み書きを抽象化。Dem は EEPROM アドレスを直接知らない |
|  | IoHwAb | AUTOSAR 抽象化層 | Dio チャネル番号を隠蔽し SW-C に論理的な LED / ボタン / ADC API を提供。10ms 周期でデバウンス（40ms 確定）・ボタン固着検出・ADC 電圧低下を Dem 報告 |
|  | Dio | SWS_Dio | `Dio_WriteChannel` / `Dio_ReadChannel` で GPIO 値を読み書きする MCAL |
|  | Port | SWS_Port | `Port_Init` でピン方向（OUTPUT / INPUT_PULLUP）を設定する MCAL |
|  | Adc | SWS_Adc | `Adc_ReadChannel` で 10-bit アナログ生値（0–1023）を読み取る MCAL |
|  | SchM | SWS_SchM | 排他エリアマクロ（`SchM_Enter` / `SchM_Exit`）で共有リソースを保護 |
|  | Det | SWS_Det | `DET_LOG*` マクロ経由でタイムスタンプ付きログを Serial に出力するデバッグ用ブリッジ |
| HAL | Can_Hw | — | MCP2515 / mcp_can C++ ラッパー |
|  | Dio_Hw | — | Arduino `digitalWrite` / `digitalRead` ラッパー |
|  | Port_Hw | — | Arduino `pinMode` ラッパー |
|  | Adc_Hw | — | Arduino `analogRead` ラッパー |

> 各モジュールの詳細（フレーム構造・状態マシン・設定値）は後続セクションを参照してください。

## ディレクトリ構成

```
├── src/
│   ├── main.cpp                  # EcuM_Init / EcuM_MainFunction を呼ぶだけのエントリポイント
│   ├── Asw/
│   │   ├── App_EngineManager.h
│   │   ├── App_EngineManager.c      # エンジン状態遷移
│   │   ├── App_WarningIndicator.h
│   │   └── App_WarningIndicator.c   # 3 LED 独立制御（D6=RUNNING / D7=FAULT 点滅 / D8=ABS）
│   ├── Rte/
│   │   ├── Rte_Type.h            # アプリ型エイリアス（ARXML 自動生成相当）
│   │   ├── Rte.h
│   │   └── Rte.c                 # ポート API（周期管理は Os へ移管済み）
│   ├── Os/
│   │   ├── Os_Cfg.h              # タスク数定数
│   │   ├── Os.h / Os.c           # タイムトリガスケジューラ（Os_SchedulerStep）
│   │   ├── Os_PBCfg.h
│   │   └── Os_PBCfg.c            # タスクテーブル（周期・関数ポインタ）
│   └── Bsw/
│       ├── Can/                  # CAN ドライバ（AUTOSAR SWS_Can 準拠 API）
│       │   ├── Can_Hw.h          # 内部インタフェース（Can.c と Can_Hw.cpp の境界）
│       │   └── Can_Hw.cpp        # MCP2515 / mcp_can C++ ラッパー（旧 Mcp2515_Wrapper.cpp）
│       ├── CanIf/                # CAN インタフェース
│       │   ├── CanIf_Types.h     # 型定義（Can_PduType 等）
│       │   ├── CanIf_Cfg.h       # TX/RX PDU テーブル数定数
│       │   ├── CanIf_PBCfg.h     # ポストビルド設定宣言（CanIf_Config）
│       │   ├── CanIf_PBCfg.c     # TX/RX PDU ルーティングテーブル実体
│       │   ├── CanIf.h           # 公開インタフェース（CanIf_Transmit / CanIf_RxIndication）
│       │   └── CanIf.c           # CAN ID ↔ 論理 PDU マッピング・Can/PduR 間の仲介
│       ├── CanTp/                # CAN トランスポートプロトコル（ISO 15765-2）
│       │   ├── CanTp_Cfg.h       # ブロックサイズ・STmin・タイムアウト設定
│       │   ├── CanTp.h           # 公開インタフェース（CanTp_Transmit / CanTp_RxIndication）
│       │   └── CanTp.c           # SF/FF/CF/FC 状態機械・マルチフレーム組立分割
│       ├── Com/                  # COM（シグナル管理）
│       │   ├── Com_Types.h       # 型定義（Com_SignalIdType / Com_IPduIdType）
│       │   ├── Com_Cfg.h         # I-PDU・シグナル数定数・シグナル ID
│       │   ├── Com_PBCfg.h       # ポストビルド設定宣言（Com_Config）
│       │   ├── Com_PBCfg.c       # I-PDU/シグナルレイアウトテーブル実体
│       │   ├── Com.h             # 公開インタフェース（Com_SendSignal / Com_ReceiveSignal）
│       │   └── Com.c             # シグナルパック/アンパック・受信デッドライン監視
│       ├── PduR/                 # PDU ルーター
│       │   ├── PduR_Types.h      # 型定義
│       │   ├── PduR_Cfg.h        # RX/TX ルーティングパス数定数
│       │   ├── PduR_PBCfg.h      # ポストビルド設定宣言（PduR_Config）
│       │   ├── PduR_PBCfg.c      # ルーティングテーブル実体
│       │   ├── PduR_COM.h        # COM 向けコールバック型定義
│       │   ├── PduR_CanIf.h      # CanIf 向けコールバック型定義
│       │   ├── PduR.h            # 公開インタフェース
│       │   └── PduR.c            # PDU マルチキャスト配信・送信完了通知の転送
│       ├── Det/                  # Default Error Tracer（Serial ブリッジ）
│       │   ├── Det.h             # ログマクロ定義（DET_LOGI/W/E/D）
│       │   └── Det.cpp           # Arduino Serial 出力実装（Arduino API を呼ぶ唯一の場所）
│       ├── Dio/                  # デジタル I/O 値読み書き（MCAL・方向設定は Port が担う）
│       │   ├── Dio_Cfg.h         # チャネル ID 定義（D6=RUNNING / D7=FAULT / D8=ABS / D9=ボタン）
│       │   ├── Dio.h             # 公開インタフェース（Dio_WriteChannel / Dio_ReadChannel）
│       │   ├── Dio.c             # AUTOSAR Dio モジュール（純粋 C、Dio_Hw へ委譲）
│       │   ├── Dio_Hw.h          # 内部インタフェース（Dio.c と Dio_Hw.cpp の境界）
│       │   └── Dio_Hw.cpp        # Arduino digitalWrite / digitalRead ラッパー（C++ のみ）
│       ├── Port/                 # ピン方向設定（MCAL・Dio と責務を分離）
│       │   ├── Port_Cfg.h        # ピン番号定義（D6/D7/D8 OUTPUT / D9 INPUT_PULLUP）
│       │   ├── Port.h            # 公開インタフェース（Port_Init / Port_SetPinDirection）
│       │   ├── Port.c            # AUTOSAR Port モジュール（純粋 C、Port_Hw へ委譲）
│       │   ├── Port_Hw.h         # 内部インタフェース（Port.c と Port_Hw.cpp の境界）
│       │   └── Port_Hw.cpp       # Arduino pinMode ラッパー（C++ のみ）
│       ├── Adc/                  # ADC ドライバ（AUTOSAR SWS_Adc 準拠 API）
│       │   ├── Adc_Cfg.h         # チャネル定義・分解能・基準電圧
│       │   ├── Adc.h             # 公開インタフェース（Adc_ReadChannel）
│       │   ├── Adc.c             # AUTOSAR Adc モジュール（純粋 C、Adc_Hw へ委譲）
│       │   ├── Adc_Hw.h          # 内部インタフェース（Adc.c と Adc_Hw.cpp の境界）
│       │   └── Adc_Hw.cpp        # Arduino analogRead ラッパー（C++ のみ）
│       ├── IoHwAb/               # I/O ハードウェア抽象化（MCAL と SW-C の境界）
│       │   ├── IoHwAb.h          # 公開インタフェース（RTE が参照）
│       │   └── IoHwAb.c          # Dio/Adc へ委譲・デバウンス（40ms）・固着検出・ADC 電圧低下検出（Dem 報告）
│       ├── SchM/                 # スケジュールマネージャ（排他エリアマクロ）
│       │   └── SchM.h            # SchM_Enter/Exit マクロ定義（全モジュール共通）
│       ├── EcuM/                 # ECU ステートマネージャ（ライフサイクル管理）
│       │   ├── EcuM_Cfg.h        # RUN ユーザ定義・POST_RUN タイムアウト
│       │   ├── EcuM.h            # 公開インタフェース・EcuM_StateType 定義
│       │   └── EcuM.c            # 状態マシン・EcuM_RequestRUN / EcuM_ReleaseRUN
│       ├── BswM/                 # BSW モードマネージャ（ルール駆動タスク制御）
│       │   ├── BswM_Cfg.h        # タスク ID 定数・タスクマスク定義
│       │   ├── BswM_PBCfg.h      # ルール構造体型定義・BswM_Config 宣言
│       │   ├── BswM_PBCfg.c      # ルールテーブル実体（3 ルール）
│       │   ├── BswM.h            # 公開インタフェース（Init / EcuM通知 / ComM通知）
│       │   └── BswM.c            # ルールエンジン実装・Os_SetTaskActive 呼び出し
│       ├── WdgM/                 # ウォッチドッグマネージャ（Alive + Logical Supervision）
│       │   ├── WdgM_Cfg.h        # エンティティ ID・チェックポイント ID・監視サイクル・期待回数定義
│       │   ├── WdgM_PBCfg.h      # エンティティ/遷移設定構造体型定義・WdgM_Config 宣言
│       │   ├── WdgM_PBCfg.c      # エンティティテーブル・許可遷移テーブル実体（App_EngineManager_Run）
│       │   ├── WdgM.h            # 公開インタフェース（Init / CheckpointReached / MainFunction）
│       │   └── WdgM.c            # Alive/Logical 検証・AVR 実 HW ウォッチドッグ連携（avr/wdt.h）
│       ├── ComM/                 # 通信マネージャ（CAN バス通信モード管理）
│       │   ├── ComM_Cfg.h        # チャネル数・ユーザ数定数
│       │   ├── ComM.h            # 公開インタフェース（NO_COM/SILENT_COM/FULL_COM）
│       │   └── ComM.c            # 状態機械・CanSM_RequestComMode へ委譲
│       ├── CanSM/                # CAN ステートマネージャ（Bus-Off 回復シーケンス）
│       │   ├── CanSM_Cfg.h       # 回復待機時間・最大試行回数
│       │   ├── CanSM.h           # 公開インタフェース・CanSM_ControllerBusOff コールバック
│       │   └── CanSM.c           # 状態機械・Bus-Off 回復タイマ管理
│       ├── Dcm/                  # 診断通信マネージャ（UDS ISO 14229-1）
│       │   ├── Dcm_Cfg.h         # SID/NRC/DID 定数・セッション・S3 タイマ・SecurityAccess 設定
│       │   ├── Dcm_Cbk.h         # PduR から呼ばれる受信コールバック宣言（Dcm_ComIndication）
│       │   ├── Dcm.h             # 公開インタフェース（Dcm_Init / Dcm_MainFunction）
│       │   └── Dcm_Cbk.c         # UDS サービスディスパッチ実装・S3 タイマ監視・SecurityAccess 状態機械
│       ├── Dem/                  # 診断イベントマネージャ（DTC 管理）
│       │   ├── Dem_Cfg.h         # イベント ID・DTC コード・ステータスビットマスク・デバウンス閾値
│       │   ├── Dem.h             # 公開インタフェース（ReportErrorStatus / GetAllDTCs / FreezeFrame）
│       │   └── Dem.c             # DTC ライフサイクル・デバウンス・FreezeFrame 記録・NvM 永続化
│       ├── FiM/                  # 機能抑止マネージャ（DTC→機能抑止）
│       │   ├── FiM_Cfg.h         # 機能 ID (FID) 定義
│       │   ├── FiM_PBCfg.h       # FID×イベント設定構造体型定義・FiM_Config 宣言
│       │   ├── FiM_PBCfg.c       # FID×イベント対応テーブル実体
│       │   ├── FiM.h             # 公開インタフェース（FiM_Init / FiM_MainFunction / GetFunctionPermission）
│       │   └── FiM.c             # 許可状態の再評価・キャッシュ
│       └── NvM/                  # Non-Volatile Memory Manager（EEPROM 抽象化）
│           ├── NvM_Cfg.h         # ブロック ID・EEPROM アドレス・ブロックサイズ定義
│           ├── NvM_PBCfg.h       # ブロック設定構造体型定義・NvM_Config 宣言
│           ├── NvM_PBCfg.c       # ブロック設定テーブル実体（DEM_MAGIC / DEM_STATUS）
│           ├── NvM.h             # 公開インタフェース（NvM_ReadBlock / NvM_WriteBlock）
│           └── NvM.c             # avr/eeprom.h ラッパー（RAM ミラー・差分書き込み）
├── dbc/
│   └── engine_manager.dbc        # CAN シグナル定義（Cangaroo 等で使用）
└── platformio.ini
```

## CAN 通信スタック（Can / CanIf / PduR / Com）

CAN ドライバ（Can / Can_Hw）から CanIf・PduR を経由して COM モジュールへ至るデータパスを担うスタックです。
RX フレームは MCP2515 → Can_Hw → Can → CanIf → PduR → Com の順に上がり、TX フレームは逆順に下ります。

### CAN フレーム仕様

エンディアンはすべてビッグエンディアン（Motorola / CAN 標準）。
ビット 0 = byte[0] の MSB、ビット 7 = byte[0] の LSB。

#### RX フレーム（周辺 ECU → MeterEcu）

**EngineInfo（エンジン ECU / CAN ID 0x100 / DLC=4）**

| ビット位置 | サイズ | シグナル | 単位・値域 |
|-----------|--------|---------|----------|
| 0–15 | 16 bit | EngineSpeed | rpm（0–15000） |
| 16–23 | 8 bit | CoolantTemp | ℃（0–255） |
| 24 | 1 bit | EngineOnFlag | 0=OFF / 1=ON |

**RUNNING 状態に入る最小フレーム例（DLC=4）：**

```
byte[0] byte[1] byte[2] byte[3]
  01      F4      00      80
  └─────┘         └──┘   └──── EngineOnFlag=1（bit24 = byte[3] の MSB）
  Speed=500rpm    Temp=0℃
```

**AbsInfo（ABS ECU / CAN ID 0x110 / DLC=3）**

| ビット位置 | サイズ | シグナル | 単位・値域 |
|-----------|--------|---------|----------|
| 0–15 | 16 bit | VehicleSpeed | 0.01 km/h（raw 0x0064 = 1.00 km/h） |
| 16 | 1 bit | BrakeActive | 0=解除 / 1=作動 |
| 17 | 1 bit | AbsActive | 0=非作動 / 1=ABS 作動中 |

**ABS 作動フレーム例（VehicleSpeed=100km/h, BrakeActive=1, AbsActive=1）：**

```
byte[0] byte[1] byte[2]
  27      10      C0
  └─────┘         └──── BrakeActive=1（bit16）, AbsActive=1（bit17）
  Speed=10000 → 100.00 km/h （0x2710 × 0.01 = 100.00）
```

#### TX フレーム（Arduino → 外部）

| CAN ID | DLC | ビット位置 | サイズ | シグナル | 値 |
|--------|-----|-----------|--------|---------|-----|
| 0x200 | 1 | 0–7 | 8 bit | EngineState | 0=OFF / 1=STARTING / 2=RUNNING / 3=FAULT |

3 秒周期で現在の状態を送信します。

### 受信デッドライン監視（COM Deadline Monitoring）

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

#### タイムアウト設定値（`Com_Cfg.h`）

| I-PDU | 定数 | 既定値 | フォールバック動作 |
|-------|------|--------|-----------------|
| EngineInfo (0x100) | `COM_TIMEOUT_ENGINE_INFO_MS` | 5000 ms | STARTING/RUNNING → FAULT |
| AbsInfo (0x110) | `COM_TIMEOUT_ABS_INFO_MS` | 5000 ms | AbsActive が 0 に戻り ABS 警告消灯 |

#### タイムアウト確認手順

1. RUNNING 状態に遷移させてから EngineInfo の送信を止める
2. 5 秒後：`WARN Com: RX timeout iPdu=0 (5000ms)` が出力される
3. さらに最大 3 秒後（次の Runnable 起動時）：`WARN AppEng: ->FAULT comm timeout` が出力される
4. LED が点滅に変わる
5. UDS SID 0x19 で DTC 0x000105 (COMM_TIMEOUT) が取得できる
6. EngineInfo を再送すると Com_RxTimedOut がリセットされ、次の Runnable サイクルで復帰する

## 診断スタック（CanTp / Dcm / Dem / FiM / NvM）

UDS 診断（ISO 14229-1）を処理するスタックです。
CanTp が ISO 15765-2 のフレーム分割・組立を担い、Dcm が UDS サービスを処理します。
Dem は故障情報を DTC として管理し、NvM 経由で EEPROM に永続化します。
FiM は Dem が確定した DTC をもとにアプリ機能の実行許可を判定します。
診断フレームはアプリデータ（0x100 / 0x110 / 0x200）とは独立した CAN ID（0x7E0 / 0x7E8）で通信します。

### UDS 診断通信（ISO 14229-1 / ISO 15765-2）

Dcm (Diagnostic Communication Manager) が UDS サービスを処理し、
CanTp (CAN Transport Protocol) が ISO 15765-2 のフレーム分割・組立を担います。

#### 診断フレームルーティング

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

#### 対応 UDS サービス

テスト時に毎回フレーム構造を探し回らずに済むよう、SID×SubFunc 単位で送信フレームの
固定バイト・可変バイトをまとめます。byte0 は CanTp の SF PCI（UDS ペイロード長）です。
全サービスの要求ペイロードは 7 バイト以内のため、要求は必ず SF で送信できます
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
| 0x22<br>ReadDataByIdentifier | ○ | ○ | — | `03 22 HH LL 00 00 00 00` | byte2-3=DID（0x0101/0x0102/0x0103） |
| 0x27<br>SecurityAccess | × | ○ | 0x01<br>(requestSeed) | `02 27 01 00 00 00 00 00` | seed 2 バイト |
|  |  |  | 0x02<br>(sendKey) | `04 27 02 HH LL 00 00 00` | byte2-3=key（big-endian） |
| 0x3E<br>TesterPresent | ○ | ○ | 0x00 | `02 3E 00 00 00 00 00 00` | S3タイマ維持 |

Def/Ext 列は `Dcm_SidSessionTable[]`（Dcm_Cbk.c）の設定そのもので、SID 単位
（SubFunc 単位ではない）の制約のため SID 行にのみ記載する。×の場合、該当セッションで
要求すると各ハンドラに到達する前に NRC 0x7F（serviceNotSupportedInActiveSession）で拒否される。
非対応サービスは NRC 0x11（serviceNotSupported）で応答します。
statusMask の代表値: `0x08`=confirmedDTC のみ / `0xFF`=全件。
0x19/04 の応答は 18 バイトと SF の 7 バイト制限を超えるため CanTp が FF+CF に分割します
（詳細は後述の「FreezeFrame」節）。0x19/02 も 2 件以上ヒットすると同様にマルチフレームになります。

#### DID 一覧（0x22 ReadDataByIdentifier）

| DID    | データ      | 型                                            | 単位 |
|--------|------------|-----------------------------------------------|------|
| 0x0101 | EngineSpeed | uint16, big-endian                           | rpm  |
| 0x0102 | CoolantTemp | uint8                                        | ℃   |
| 0x0103 | EngineState | uint8（0=OFF / 1=STARTING / 2=RUNNING / 3=FAULT） | —  |

#### フレーム例（シングルフレーム）

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

#### S3 タイマ（セッションタイムアウト）

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

#### SecurityAccess（SID 0x27、Level1）

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

3. defaultSession へ遷移（明示要求または S3 タイムアウト）すると Level1 は再ロックされる。
   ただし連続失敗回数・ロックアウト中フラグはセッションをまたいで保持する
   （セッション往復の繰り返しでロックアウトを回避できないようにするため）。
```

**注意:** `key = seed XOR 固定マスク` は仕組みを学ぶための最小限の例です。
量産 ECU は OEM 固有の非公開アルゴリズムや暗号学的アルゴリズムを使用するため、
本実装のロジックを実運用に転用しないでください。

#### SID × セッション許可テーブル

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
    { DCM_SID_CLEAR_DTC,       DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_SECURITY_ACCESS, DCM_SESSION_MASK_EXTENDED },
};
```

テーブルに掲載のない SID はセッション制約なしとみなされ、defaultSession でも応答します。
0x14 と 0x27 のみ extendedSession 限定とし、defaultSession で要求すると各ハンドラに
到達する前に NRC 0x7F（serviceNotSupportedInActiveSession）で拒否されます
（各 SID の制約は前述の「対応 UDS サービス」表の Def/Ext 列を参照）。

新しいサービスにセッション制約を追加する場合は、ハンドラ内に判定を書き足すのではなく
`Dcm_SidSessionTable[]` に行を追加するだけで済みます。

### CanTp（ISO 15765-2 トランスポートプロトコル）

CanTp モジュールが ISO 15765-2 のフレーム処理を担い、
DCM は PCI バイトを意識せず生 UDS ペイロードのみを扱います。

#### ISO 15765-2 フレーム構造

| フレーム種別 | PCI (byte[0]) | 内容 |
|------------|--------------|------|
| SF (Single Frame) | `0x0N` N=ペイロード長 | UDS ペイロード ≤ 7 バイト |
| FF (First Frame)  | `0x1H 0xLL` HL=総長 | UDS ペイロード ≥ 8 バイト の先頭 6 バイト |
| CF (Consecutive Frame) | `0x2n` n=シーケンス番号 | 続きのデータ（最大 7 バイト/フレーム） |
| FC (Flow Control) | `0x3X` X=FS | CTS(0)/WAIT(1)/OVFLW(2)、BS、STmin |

#### RX 状態マシン（Arduino 受信側）

```
IDLE ──── SF 受信 ──────────────────→ Dcm_ComIndication → IDLE
     ──── FF 受信 → FC(CTS) 送信 ──→ WAIT_CF
WAIT_CF ─ CF 受信(未完) ────────────→ WAIT_CF
        ─ CF 受信(完成) ────────────→ Dcm_ComIndication → IDLE
        ─ N_Cr タイムアウト(5 秒) ──→ IDLE (中断)
```

#### TX 状態マシン（Arduino 送信側）

```
IDLE ──── ≤7 バイト → SF 送信 ──────────────────────────→ IDLE
     ──── ≥8 バイト → FF 送信 ──────────────────────────→ WAIT_FC
WAIT_FC ─ FC(CTS) 受信 ─────────────→ SEND_CF
        ─ N_Bs タイムアウト(5 秒) ──→ IDLE (中断)
SEND_CF ─ CF 送信(MainFunction 毎) ─→ 完了 → IDLE
```

#### フロー制御パラメータ（BS / STmin）

FC フレームの byte[1] が BS（Block Size）、byte[2] が STmin（Separation Time minimum）です。

| パラメータ | 本プロジェクト設定値 | 意味 |
|-----------|-------------------|------|
| **BS = 0** | `CANTP_BLOCK_SIZE 0U` | FF の後に FC を **1 回だけ** 送れば残り全 CF を連続送信してよい |
| BS = N (N≥1) | — | CF を N 枚送るごとに次の FC を待つ（本プロジェクトでは未使用） |
| STmin = 0 | `CANTP_ST_MIN 0U` | CF 間の最小待機時間なし |

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

#### マルチフレーム応答例（2 DTC の場合）

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

#### Cangaroo で FC を手動送信する方法

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

### DEM 診断イベント管理（AUTOSAR SWS_DEM）

Dem (Diagnostic Event Manager) モジュールがエンジン管理の故障を DTC として管理します。
DTC の永続化は NvM (Non-Volatile Memory Manager) 経由で行い、
Dem は EEPROM アドレスを直接知りません（NvM_WriteBlock / NvM_ReadBlock のみ使用）。
電源オフ後もクリア操作（SID 0x14）が行われない限り DTC が保持されます。

#### イベントと DTC コード

| EventId | イベント名 | 検出条件 | DTC コード |
|---------|-----------|---------|-----------|
| 0 | ENGINE_OVERHEAT | CoolantTemp ≥ 100 ℃（RUNNING 中） | 0x000101 |
| 1 | ENGINE_STALL | EngineSpeed < 100 rpm（RUNNING 中） | 0x000102 |
| 2 | ENGINE_SPEED_NO_FLAG | speed > 0 かつ flag = 0（OFF 中） | 0x000103 |
| 3 | STARTING_TIMEOUT | 起動から 5 秒超過（STARTING 中） | 0x000104 |
| 4 | COMM_TIMEOUT | EngineInfo 受信が 5 秒以上途絶（STARTING/RUNNING 中） | 0x000105 |
| 5 | BUTTON_STUCK | 警告確認ボタン（D9）が 5 秒以上押しっぱなし（IoHwAb 検出） | 0x000106 |
| 6 | ADC_VOLT_LOW | ADC センサ電圧（A0）が 1000mV 未満（IoHwAb が 10ms 周期で検出） | 0x000107 |
| 7 | CAN_BUSOFF | CAN Bus-Off からの回復断念（最大試行回数超過、CanSM が検出） | 0x000108 |

#### デバウンス (Counter-based Debouncing)

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
> なってしまう。これは特に limit=1（BUTTON_STUCK / CAN_BUSOFF）で問題になる。
> 例えば CAN_BUSOFF は起動直後に ComM から PASSED が 1 回報告されて counter=-1 まで
> 進むため、最初の Bus-Off 断念（FAILED 1 回）が counter を -1→0 にするだけで
> 確定 FAILED（+1）に届かず、WARN ログが出力されない、という不具合があった。
> IoHwAb のボタンデバウンス（生レベルが確定値と一致すればカウンタをリセットする）と
> 同じ「割り込まれたら最初からやり直す」方式に合わせて修正している。

#### 閾値 (limit) の決め方

モニタ（報告元）が Dem に報告する前に、**既に十分な持続性チェックを行っているか**で
閾値を変えています。

| limit | 対象イベント | 理由 |
|---|---|---|
| 1（即確定） | BUTTON_STUCK, CAN_BUSOFF | IoHwAb の 5 秒固着判定／CanSM の 3 回リトライ後の断念は、それ自体が「十分粘った結果」。Dem 側で重ねてデバウンスすると二重チェックになり、確定が不必要に遅れる（または構造的に確定不可能になる） |
| 2（複数回要求） | ENGINE_OVERHEAT, ENGINE_STALL, ENGINE_SPEED_NO_FLAG, STARTING_TIMEOUT, COMM_TIMEOUT, ADC_VOLT_LOW | モニタは瞬時のしきい値超え（temp≥100 等）をそのまま報告するだけで、持続性チェックを行っていない。単発の誤検出で確定させないために Dem 側でデバウンスする |

> **設計の経緯**: 当初は全イベント共通の単一閾値（`DEM_DEBOUNCE_LIMIT=2`）でした。
> しかし BUTTON_STUCK は「固着で+1、解放で-1」を繰り返すだけで確定（+2）に
> 決して到達できず、CAN_BUSOFF は「断念の瞬間に EcuM がシャットダウンへ進む」ため
> 2 回目の断念が起こり得ず確定不可能でした。どちらも「モニタ側が既に確定的な
> 判断をしてから1回だけ報告する」設計だったため、共通閾値2に合わせて呼び出し元を
> 不自然に作り替える必要がありました。イベントごとの閾値に変更したことで、
> 両モジュールとも報告ロジックを自然な「1 回だけ報告する」形に戻せました。

イベントごとの報告パターンによって、確定までにかかる実時間が異なります。

| イベント | 報告パターン | 確定までの目安 |
|---|---|---|
| ENGINE_OVERHEAT / STALL / SPEED_NO_FLAG / STARTING_TIMEOUT | 状態遷移の瞬間に単発報告（limit=2） | 同じ故障が**別々の機会に 2 回**発生する必要あり |
| COMM_TIMEOUT | 故障継続中は毎 Runnable サイクル（3000ms）報告（limit=2） | 2 サイクル分＝約 3000〜6000ms 追加 |
| ADC_VOLT_LOW | 故障継続中は毎 10ms サイクル報告（limit=2） | 数十 ms（実質的に即時） |
| BUTTON_STUCK / CAN_BUSOFF | 持続性チェック後に 1 回だけ報告（limit=1） | 即時確定 |

#### 複数 DTC を発生させる手順

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

#### ABS LED 動作確認手順

RUNNING 状態で以下の AbsInfo フレームを 0x110 で送信して LED 動作を確認します。

| 送信フレーム | AbsActive | BrakeActive | D6 RUNNING | D7 FAULT | D8 ABS |
|------------|-----------|-------------|:----------:|:--------:|:------:|
| `110#27.10.00` | 0 | 0 | 点灯 | 消灯 | 消灯 |
| `110#27.10.40` | 0 | 1 | 点灯 | 消灯 | 消灯（BrakeActive は LED に影響しない） |
| `110#27.10.C0` | 1 | 1 | 点灯 | 消灯 | **点灯** |
| `110#27.10.80` | 1 | 0 | 点灯 | 消灯 | **点灯** |

FAULT 状態で `110#27.10.C0`（AbsActive=1）を送信すると、D7 が点滅しつつ D8 も同時に点灯します（3 LED は独立制御）。

#### DTC ステータスバイト（ISO 14229-1 Annex B）

SID 0x19 の応答に含まれるステータスバイトの各ビットの意味。

| ビット | マスク | 略称 | 意味 |
|-------|--------|------|------|
| bit0 | 0x01 | TF | testFailed — 今現在壊れている |
| bit2 | 0x04 | PDTC | pendingDTC — 今の電源サイクルで失敗した |
| bit3 | 0x08 | CDTC | confirmedDTC — 確定済み・EEPROM 保存済み |
| bit4 | 0x10 | TNCLC | testNotCompletedSinceLastClear — クリア後未テスト |
| bit5 | 0x20 | TFSLC | testFailedSinceLastClear — クリア後に失敗あり |

statusAvailabilityMask = **0x2D**（本実装がサポートするビットの OR）。

#### DTC ライフサイクル

| フェーズ | TF | PDTC | CDTC | TFSLC | TNCLC | ステータス値 |
|---------|:--:|:----:|:----:|:-----:|:-----:|:-----------:|
| 初回起動（EEPROM 未初期化） | 0 | 0 | 0 | 0 | **1** | `0x10` |
| PASSED 報告 2 回でデバウンス確定 | 0 | 0 | 0 | 0 | 0 | `0x00` |
| FAILED 報告 1 回目（PRE-FAILED, 未確定） | 0 | 0 | 0 | 0 | 0 | `0x00`（変化なし） |
| **FAILED 報告 2 回目でデバウンス確定** | **1** | **1** | **1** | **1** | 0 | **`0x2D`** |
| 電源再投入後（TF のみリセット） | **0** | 1 | **1** | 1 | 0 | **`0x2C`** |
| クリーンな操作サイクルを 3 回経過（経年回復） | 0 | 1 | **0** | 1 | 0 | `0x14` |
| SID 0x14 実行後 | 0 | 0 | **0** | **0** | **1** | `0x10` |

> デバウンスカウンタ自体は RAM のみで保持するため、電源再投入時に中立 (0) へリセットされます。
> PRE-FAILED の途中で電源が切れた場合、その「あと1回」の進行はリセットされます。

- **CDTC（bit3）が永続化の本体**。電源再投入後も保持されるため、整備ツールで過去の故障を確認できる。
- TF（bit0）は電源再投入時にクリア。「今は動いているが過去に壊れた」を表現できる。
- CDTC を消すには SID 0x14 による明示的なクリア、または経年回復（再故障せず複数回の操作サイクルを経過）のいずれか。

#### フレーム例（DTC 操作）

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

#### 経年回復（Aging）

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
    → DEM_AGING_CYCLES_THRESHOLD (既定 3 回) に達したら CDTC を自動クリア
```

カウンタは NvM (`NVM_BLOCK_ID_DEM_AGING`) で永続化するため、電源を切っても進行度が
失われません。実車では数十サイクル単位が一般的ですが、本プロジェクトでは電源の
再投入を数回行うだけで動作確認できるよう小さい値にしています。

**ログ例（ENGINE_OVERHEAT が再故障せず 3 回起動した場合）：**
```
# 1 回目の再起動（FAILED 確定済み、直前サイクルはクリーン）
[60ms] INFO  Dem: ev=0 aging=1/3

# 2 回目の再起動
[60ms] INFO  Dem: ev=0 aging=2/3

# 3 回目の再起動 → 経年回復完了、CDTC が自動クリア
[60ms] INFO  Dem: ev=0 healed (aging complete) dtc=0x000101

# もし途中の起動で再度 FAILED が確定していたら
[60ms] INFO  Dem: ev=0 aging reset (re-failed)
```

#### EEPROM レイアウト

Arduino UNO の内蔵 EEPROM 先頭 17 バイトを使用します。
アドレス割り当ては NvM_Cfg.h (`NVM_BLOCK_DEM_*_EEPROM_ADDR`) で一元管理しています。
Dem は NvM_BlockIdType (NVM_BLOCK_ID_DEM_MAGIC / _DEM_STATUS / _DEM_AGING) でのみアクセスします。

| アドレス | NvM ブロック | 内容 |
|---------|-------------|------|
| 0x00 | NVM_BLOCK_ID_DEM_MAGIC (1 byte) | マジックバイト（0xDE = 有効データあり） |
| 0x01 | NVM_BLOCK_ID_DEM_STATUS (8 bytes) | EVENT_ENGINE_OVERHEAT ステータス |
| 0x02 | 〃 | EVENT_ENGINE_STALL ステータス |
| 0x03 | 〃 | EVENT_ENGINE_SPEED_NO_FLAG ステータス |
| 0x04 | 〃 | EVENT_STARTING_TIMEOUT ステータス |
| 0x05 | 〃 | EVENT_COMM_TIMEOUT ステータス |
| 0x06 | 〃 | EVENT_BUTTON_STUCK ステータス |
| 0x07 | 〃 | EVENT_ADC_VOLT_LOW ステータス |
| 0x08 | 〃 | EVENT_CAN_BUSOFF ステータス |
| 0x09 | NVM_BLOCK_ID_DEM_AGING (8 bytes) | EVENT_ENGINE_OVERHEAT 経年回復カウンタ |
| 0x0A | 〃 | EVENT_ENGINE_STALL 経年回復カウンタ |
| 0x0B | 〃 | EVENT_ENGINE_SPEED_NO_FLAG 経年回復カウンタ |
| 0x0C | 〃 | EVENT_STARTING_TIMEOUT 経年回復カウンタ |
| 0x0D | 〃 | EVENT_COMM_TIMEOUT 経年回復カウンタ |
| 0x0E | 〃 | EVENT_BUTTON_STUCK 経年回復カウンタ |
| 0x0F | 〃 | EVENT_ADC_VOLT_LOW 経年回復カウンタ |
| 0x10 | 〃 | EVENT_CAN_BUSOFF 経年回復カウンタ |

### FreezeFrame（故障時スナップショット）

DTC が FAILED に遷移した瞬間の車両状態（EngineSpeed / CoolantTemp / EngineState）を Dem が記録し、
UDS SID 0x19 subFunc 0x04（reportDTCSnapshotRecordByDTCNumber）で読み出せます。
本実装は **RAM のみに保持**し EEPROM へは永続化しません（電源 OFF で消去される学習用簡略化）。
イベントごとに保持するレコードは 1 件（recordNumber=0x01）のみです。

#### 記録の仕組み

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

#### フレーム例（SID 0x19/04）

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

### FiM（機能抑止マネージャ）

FiM (Function Inhibition Manager) は、Dem が確定（CONFIRMED）した DTC を根拠に、
関連するアプリ機能の実行を抑止するルールエンジンです。
「DTC を記録する」（Dem の責務）と「DTC を理由に機能を止める」（FiM の責務）を
分離するのが AUTOSAR の設計思想で、ASW は Dem の内部実装を一切知らずに
`Rte_Call_FiM_GetFunctionPermission()` だけで「この機能は今実行してよいか」を判定できます。

#### 機能 ID (FID) とイベントの対応

| FID | 機能 | 抑止条件 | 抑止時の挙動 |
|---|---|---|---|
| `FIM_FID_RUNNING_LED` | RUNNING LED (D6) の点灯 | `DEM_EVENT_CAN_BUSOFF` が CONFIRMED | D6 を強制消灯（EngineState は CAN 受信由来のため、Bus-Off 確定中は信頼できない） |
| `FIM_FID_BUTTON_ACK` | 警告確認ボタンによる FAULT 解除 | `DEM_EVENT_BUTTON_STUCK` が CONFIRMED | ボタン押下を無視（固着確定中の押下は物理的固着による偽信号の可能性がある） |

対応表は `FiM_PBCfg.c` の `FiM_Functions[]` で定義する（AUTOSAR の `FiMFunction` コンテナに相当）。
新しい FID を追加する場合は、ここに 1 行追加するだけで済む。

#### 判定の流れ

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

#### ログ例

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

## ECU 管理層（EcuM / BswM / WdgM / ComM / CanSM）

ECU の起動・シャットダウンのライフサイクルと、タスク制御・ソフトウェア監視を担うモジュール群です。
EcuM が状態遷移を決定し、BswM がその状態に応じたタスクの有効・無効を制御し、WdgM がタスク内部の動作を監視します。

### EcuM（ECU ステートマネージャ）

EcuM (ECU State Manager) は BSW スタック全体のライフサイクルを管理するモジュールです。
`main.cpp` は `EcuM_Init()` と `EcuM_MainFunction()` を呼ぶだけでよく、
個々の BSW モジュールを直接参照しません。

#### EcuM 状態マシン

```
          EcuM_Init() 完了
STARTUP ──────────────────→ RUN ── 全 RUN ユーザが解放 ──→ POST_RUN
                             ↑                                  │
                    EcuM_RequestRUN が来たら ←──────────────────┘
                    (POST_RUN 中の場合のみ)       ECUM_POST_RUN_TIMEOUT_MS (5秒) 経過
                                                               ↓
                                                           SHUTDOWN
                                                     (スケジューラ停止)
```

| 状態 | `Os_SchedulerStep()` | 遷移条件 |
|------|:-------------------:|---------|
| STARTUP | 停止 | `EcuM_Init()` 末尾で RUN へ自動遷移 |
| RUN | **実行** | 全 RUN ユーザが `EcuM_ReleaseRUN` → POST_RUN |
| POST_RUN | **実行**（後処理継続） | タイムアウト → SHUTDOWN / `EcuM_RequestRUN` → RUN |
| SHUTDOWN | 停止 | 終端状態（Arduino では電源断不可・アイドル待機） |

#### RUN ユーザ

RUN フェーズを継続するために「誰かが使っている」ことを宣言するしくみです。
ユーザが全員解放したときに POST_RUN へ遷移します。

| ユーザ | 定数 | `EcuM_RequestRUN` タイミング | `EcuM_ReleaseRUN` タイミング |
|-------|------|--------------------------|--------------------------|
| ComM | `ECUM_USER_COMM` | CAN バスが FULL_COM になったとき | Bus-Off 回復断念で NO_COM になったとき |

#### コールチェーン（上下双方向）

AUTOSAR では「上から下への要求 (Request)」と「下から上への通知 (Indication)」が分離されています。

```
【起動時】
EcuM_Init → ComM_RequestComMode(FULL_COM)   ← EcuM が ComM へ要求（上→下）
              └→ CanSM_RequestComMode(FULL_COM)
                   └→ ComM_BusSMIndication(FULL_COM)  ← CanSM が ComM へ通知（下→上）
                        └→ EcuM_RequestRUN(ECUM_USER_COMM)

【Bus-Off 回復断念時】
CanSM_MainFunction（10ms タスク）
  └→ ComM_BusSMIndication(NO_COM)           ← CanSM が ComM へ通知（下→上）
       └→ EcuM_ReleaseRUN(ECUM_USER_COMM)
            └→ EcuM: RUN → POST_RUN → (5秒後) → SHUTDOWN
```

#### EcuM 設定（`EcuM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `ECUM_USER_COUNT` | 1 | RUN 要求できるユーザ数 |
| `ECUM_USER_COMM` | 0 | ComM のユーザ ID |
| `ECUM_POST_RUN_TIMEOUT_MS` | 5000 ms | POST_RUN タイムアウト |

### BswM（BSW モードマネージャ）

BswM (BSW Mode Manager) は、EcuM や ComM からのモード変化通知を受け取り、
ルールテーブルに従って Os タスクの有効・無効を切り替えるルールエンジンです。

EcuM が「今どのフェーズか」を決めるのに対し、BswM は「そのフェーズで何をするか」を決めます。
この責任分離により、フェーズごとの振る舞いをコードを書かずにルールテーブルの変更だけで調整できます。

#### ルールテーブル（`BswM_PBCfg.c`）

| No | モード源 | モード値 | アクション | 対象タスクマスク |
|----|---------|---------|-----------|----------------|
| 0 | EcuM | RUN | ACTIVATE | 全タスク（0x1FF） |
| 1 | EcuM | POST_RUN | DEACTIVATE | アプリタスクのみ（0x0C） |
| 2 | EcuM | SHUTDOWN | DEACTIVATE | 全タスク（0x1FF） |

#### タスク ID とマスク（`BswM_Cfg.h`）

タスク数が 8 を超えるため、`TaskMask` は uint16（bits 0〜9）です。

| タスク ID | 定数 | 対応関数 | 周期 |
|---------|------|---------|------|
| 0 | `BSWM_OS_TASK_CAN_ISR` | `Can_Isr` | 1 ms |
| 1 | `BSWM_OS_TASK_CANTP_MAIN` | `CanTp_MainFunction` | 1 ms |
| 2 | `BSWM_OS_TASK_RTE_ENGINE` | `Rte_ScheduleRunnables` | 3000 ms |
| 3 | `BSWM_OS_TASK_RTE_WARNING` | `Rte_ScheduleWarningIndicator` | 500 ms |
| 4 | `BSWM_OS_TASK_CANSM_MAIN` | `CanSM_MainFunction` | 10 ms |
| 5 | `BSWM_OS_TASK_COM_MAIN` | `Com_MainFunction` | 100 ms |
| 6 | `BSWM_OS_TASK_IOHWAB_MAIN` | `IoHwAb_MainFunction` | 10 ms |
| 7 | `BSWM_OS_TASK_WDGM_MAIN` | `WdgM_MainFunction` | 6000 ms |
| 8 | `BSWM_OS_TASK_DCM_MAIN` | `Dcm_MainFunction` | 1000 ms |
| 9 | `BSWM_OS_TASK_FIM_MAIN` | `FiM_MainFunction` | 100 ms |

`BSWM_TASK_MASK_APP = 0x0C`（bit2=Rte_Engine, bit3=Rte_Warning）がアプリタスクマスクです。
POST_RUN ではこの 2 タスクだけを停止し、BSW タスク（CAN ISR・CanTp・CanSM・Com・IoHwAb・WdgM・Dcm・FiM）は継続させます。
Dcm を継続させることで、POST_RUN 中も S3 タイマ監視（セッションの自動失効）が動作し続けます。

#### POST_RUN でアプリタスクのみ停止する理由

POST_RUN 中も BSW タスクを動かし続けることで、以下のグレースフルシャットダウンが実現されます。

```
POST_RUN 中も動き続けるタスク:
  Can_Isr / CanTp_Main → 受信中の診断フレームを最後まで処理
  CanSM_Main          → 回復シーケンスの完了まで管理
  Com_MainFunction    → デッドライン監視の最終確認
  IoHwAb_Main        → ボタンのデバウンス状態を正常終了
  WdgM_Main          → Alive Supervision のソフト評価は継続（HW ウォッチドッグは
                        POST_RUN 移行時点で無効化済みのため実際のリセットは発生しない）
  Dcm_Main           → S3 タイマ監視を継続（拡張セッションも正しく失効する）

POST_RUN 中に停止するタスク:
  Rte_ScheduleRunnables          → エンジン状態更新・DTC 登録を停止
  Rte_ScheduleWarningIndicator   → LED 制御を停止（消灯状態で固定）
```

#### 通知チェーン

```
Bus-Off 回復断念
  CanSM → ComM_BusSMIndication(NO_COM)
            ├→ EcuM_ReleaseRUN(ECUM_USER_COMM)
            │     └→ EcuM: RUN → POST_RUN
            │               └→ BswM_EcuM_CurrentState(POST_RUN)
            │                     └→ Rule 1 発火: Os_SetTaskActive(Rte_Engine, OFF)
            │                                     Os_SetTaskActive(Rte_Warning, OFF)
            └→ BswM_ComM_CurrentMode(0, NO_COM)   ← ComM モード変化も通知（将来の拡張用）

POST_RUN 5秒後
  EcuM: POST_RUN → SHUTDOWN
    └→ BswM_EcuM_CurrentState(SHUTDOWN)
          └→ Rule 2 発火: Os_SetTaskActive(全タスク, OFF)
```

#### BswM 設定の変更方法

| 変更内容 | 編集ファイル |
|---------|------------|
| POST_RUN で停止するタスクの追加・変更 | `BswM_Cfg.h` の `BSWM_TASK_MASK_APP` |
| ルール追加（例: ComM モードに反応する） | `BswM_PBCfg.c` にルールを追記し `BSWM_RULE_COUNT` を更新 |
| タスク追加 | `BswM_Cfg.h` に ID 定数を追加し `Os_PBCfg.c` にも追記 |

### WdgM（ウォッチドッグマネージャ）

WdgM (Watchdog Manager) は「ソフトウェアが本当に動いているか」を監視するモジュールです。
EcuM や BswM がフェーズ管理・タスク制御を担うのに対し、WdgM はタスク内部の実行を監視します。

CAN バスが正常でも、タスクが無限ループやスタック破壊で停止することがあります。
WdgM は監視対象（Supervised Entity）に「生存報告」を埋め込み、報告が途絶えたとき（Alive Supervision）
や、報告が想定外の順序で来たとき（Logical Supervision）に異常と判断します。

異常時の最終アクションは **AVR 実ハードウェアウォッチドッグによる本当の MCU リセット**です
（後述）。ログ出力だけのシミュレーションではなく、Arduino UNO 上で実際に再起動が発生します。

#### Alive Supervision の仕組み

`App_EngineManager_Run()` は 1 回の実行で `WdgM_CheckpointReached` を 2 回呼ぶため
（後述の START/END チェックポイント）、AliveCount は 3000ms 周期の Run() 呼び出しごとに 2 ずつ増えます。

```
監視対象 Runnable が呼ぶ:
  App_EngineManager_Run()  (3000ms 周期)
    ├→ WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START)  ← 開始
    └→ WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END)    ← 終了（"私は正常に完了した"）

WdgM_MainFunction (6000ms 周期) が評価:
  AliveCount >= WDGM_EXPECTED_ALIVE_INDICATIONS (1) ?
    YES → LOCAL_STATUS_OK  → "SE0 OK alive=4"   ← 6000ms の間に Run() が 2 回 × 2 チェックポイント
    NO  → LOCAL_STATUS_FAILED → "SE0 FAILED alive=0 [HW WDT reset pending]"

評価後: AliveCount = 0 にリセットして次サイクル開始
  全エンティティ OK → wdt_reset() を呼ぶ（"HW watchdog refreshed"）
  1 つでも FAILED → 呼ばない（"HW watchdog NOT refreshed - reset imminent"）
```

#### Logical Supervision の仕組み

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

#### HW ウォッチドッグ連携（実際の MCU リセット）

WdgM は AVR 実ハードウェアウォッチドッグ（`<avr/wdt.h>`）と連携しています。
シミュレーションではなく、Arduino UNO 上で実際にリセットが発生します。

```
WdgM_Init()（起動シーケンス末尾、Os_Init の直前）:
  wdt_enable(WDTO_8S)   ← HW ウォッチドッグを 8000ms タイムアウトで有効化

WdgM_MainFunction()（6000ms 周期）:
  全エンティティが OK ?
    YES → wdt_reset()           ← リフレッシュ。タイマが 0 から再カウント開始
    NO  → 何もしない             ← リフレッシュされず、カウントが進み続ける

リフレッシュされないまま 8000ms 経過 → AVR ハードウェアが MCU を強制リセット
  → setup() から再起動（DET ログも最初から出力される）
```

タイムアウト（8000ms）は監視サイクル（6000ms）より長く設定し、健全な動作では
毎サイクルのリフレッシュがタイムアウトに必ず間に合うようにしています。
一度でも異常が発生すると、次の `WdgM_MainFunction` 呼び出し（最短でも 6000ms 後）を
待たずに約 2 秒後にはリセットが発生するため、ソフトウェアによる「次サイクルでの
recovered」処理が実際に走る前に MCU がリセットされるのが通常です。

#### ブートローダ起因の無限リセットループ対策

AVR/Arduino では、短いタイムアウトで WDT が有効なまま再起動すると、ブートローダの
待機中に再度タイムアウトしてスケッチに到達できない「無限リセットループ」に陥る
既知の問題があります。これを防ぐため `main.cpp` の `setup()` の最初で
`MCUSR = 0; wdt_disable();` を実行し、`WdgM_Init()` が後から安全なタイムアウトで
再度有効化します。

#### 意図的な POST_RUN 移行での無効化／RUN 復帰での再有効化

EcuM が RUN から POST_RUN へ遷移する際、`WdgM_DisableHwWatchdog()` を呼んで
HW ウォッチドッグを無効化します。POST_RUN では BswM Rule 1 によって Rte_Engine
タスク（WdgM の唯一の監視対象）が意図的に停止するため、Alive Supervision は
必ず FAILED になります。

無効化するタイミングを **SHUTDOWN ではなく POST_RUN 移行時**にしているのには理由が
あります。WdgM はタスクとしては POST_RUN 中も継続するため（CanTp/Com/IoHwAb と同じ
BSW タスク）、無効化しないと POST_RUN 中（最大 `ECUM_POST_RUN_TIMEOUT_MS`=5000ms）に
Alive Supervision が FAILED を検出し続け、リフレッシュが止まったままになります。
最後に成功したリフレッシュからの経過時間によっては、SHUTDOWN への遷移
（POST_RUN 開始から最大 5000ms 後）を待つ前に HW ウォッチドッグのタイムアウト
（8000ms）に達してしまう可能性があり、「正常なシャットダウン処理中」のはずが
予期しないリセットを起こしてしまいます。POST_RUN への移行そのものを無効化の
タイミングにすることで、この競合を避けています。

CanSM の Bus-Off 回復等で POST_RUN から RUN へ復帰した場合は、`WdgM_EnableHwWatchdog()`
で再度有効化し、Alive Supervision による監視を再開します。

#### 本プロジェクトでの失敗アクション

| 環境 | 失敗時のアクション |
|---|---|
| 本プロジェクト（Arduino UNO） | `wdt_reset()` のリフレッシュが止まり、最大 8000ms 後に実際に MCU がリセットされる |
| 実機製品 | 同様に HW ウォッチドッグがリフレッシュを停止し、タイムアウト後にシステムリセット（本実装と同じ仕組み） |

Alive と Logical はどちらも同じ `WdgM_LocalStatusType` に反映されます（学習用簡略化のため
アルゴリズムごとの個別ステータスは保持しません）。そのため Logical Supervision が FAILED と
判定した直後でも、次の `WdgM_MainFunction` サイクルで Alive 条件を満たせば「recovered」として
OK に復帰します。ただし HW ウォッチドッグが有効な今は、前述の通り通常その前にリセットが
発生するため、「recovered」ログが実際に観測できる場面は限られます。

#### シリアルログ確認例

**正常時（6000ms ごと）:**
```
[19ms]    INFO  WdgM: HW watchdog enabled (8000ms)   ← WdgM_Init 内（起動時）
[6017ms]  DEBUG WdgM: SE0 OK alive=4   ← 3000ms 周期で Run() が 2 回、各 2 チェックポイント
[6018ms]  DEBUG WdgM: HW watchdog refreshed
[12017ms] DEBUG WdgM: SE0 OK alive=4
[12018ms] DEBUG WdgM: HW watchdog refreshed
```

**POST_RUN 移行時（意図的な停止。HW ウォッチドッグは無効化されるため実際のリセットは発生しない）:**
```
[30312ms] INFO  EcuM: ->POST_RUN timeout=5000ms
[30313ms] INFO  WdgM: HW watchdog disabled
[36312ms] WARN  WdgM: SE0 FAILED alive=0 (exp>=1) [HW WDT reset pending]  ← ソフト的には FAILED と記録されるが
                                                                          ← 無効化済みのため実際にはリセットしない
```

**Alive 失敗検知（RUN 中に実際の異常が起きた場合 — 後述の動作確認方法）:**
```
[6017ms]  WARN  WdgM: SE0 FAILED alive=0 (exp>=1) [HW WDT reset pending]
[6018ms]  ERROR WdgM: HW watchdog NOT refreshed - reset imminent
（約 2000ms 後、リフレッシュなしで HW ウォッチドッグのタイムアウトに到達）
[8019ms]  INFO  NvM: Init ok blocks=2     ← MCU が実際にリセットされ、setup() から再起動
[8020ms]  INFO  Port: Init pins=4
...
```

**Logical 失敗検知（後述の動作確認方法で START 呼び出しを止めた場合）:**
```
[3010ms] WARN  WdgM: SE0 logical FAILED cp 1->1 (unexpected) [HW WDT reset pending]
                                  └┘  └┘
                                  前回END  今回END（START がスキップされ END→END になった）
（以降は Alive 失敗検知と同様、約 2000〜8000ms 後に MCU が実際にリセットされる）
```

> **動作確認の前に**: 以下のテストは Arduino UNO を**実際にリセット**させます。
> シリアルモニタには `EcuM: ->RUN` 等の起動ログが再び表示され、リセットされたことが
> わかります（EEPROM の DTC は NvM 経由で保持されるため消えません）。元に戻すには
> コメントを外して再度アップロードしてください。

**動作確認方法（Alive Supervision）:** `App_EngineManager.c` の END 側 `WdgM_CheckpointReached` をコメントアウト。
```c
/* (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END); */
```
起動後 6000ms で `alive=0` の FAILED ログが出てリフレッシュが止まり、約 2000ms 後（最後の
リフレッシュから合計 8000ms）に実際に MCU がリセットされる。

**動作確認方法（Logical Supervision）:** START 側だけをコメントアウトすると、END→END の
順序違反が次の Run() 実行時に即座に検出される（MainFunction の周期を待たない）。
```c
/* (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START); */
```

#### WdgM 設定（`WdgM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `WDGM_SUPERVISED_ENTITY_COUNT` | 1 | 監視対象エンティティ数 |
| `WDGM_ENTITY_ENGINE` | 0 | App_EngineManager_Run のエンティティ ID |
| `WDGM_SUPERVISION_CYCLE_MS` | 6000 ms | Alive Supervision サイクル（WdgM_MainFunction 周期と一致） |
| `WDGM_EXPECTED_ALIVE_INDICATIONS` | 1 | サイクル内の最小 CheckpointReached 呼び出し回数 |
| `WDGM_CP_ENGINE_START` | 0 | Run() 開始直後のチェックポイント ID |
| `WDGM_CP_ENGINE_END` | 1 | Run() 終了直前のチェックポイント ID |
| `WDGM_CP_INITIAL` | 0xFF | 起動直後（まだチェックポイント未報告）を示す特別な遷移元 ID |
| `WDGM_HW_WATCHDOG_TIMEOUT_MS` | 8000 ms | AVR 実 HW ウォッチドッグのタイムアウト目安（実際の列挙値 `WDTO_8S` は `WdgM.c` で指定）。`WDGM_SUPERVISION_CYCLE_MS` より長く設定すること |

許可遷移グラフ自体は `WdgM_PBCfg.c` の `WdgM_EngineTransitions[]`（ポストビルド設定）で管理します。

## IO スタック（IoHwAb / Dio / Port / Adc）

SW-C はピン番号を直接知りません。RTE の Client/Server ポートを通じて IoHwAb の論理 API を呼び出し、
IoHwAb が Dio / Adc チャネルへ変換します。ピン方向の初期設定は Port が担い、Dio は値の読み書きのみ、
Adc はアナログ入力の読み取りのみを行います。

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

### チャネル割り当て（`Dio_Cfg.h`）

| 定数 | Dio チャネル | Arduino ピン | 機能 | Port 方向 |
|------|------------|-------------|------|----------|
| `DIO_CHANNEL_LED_RUNNING` | 6 | D6 | RUNNING 灯（RUNNING 中点灯） | OUTPUT |
| `DIO_CHANNEL_LED_FAULT` | 7 | D7 | FAULT 灯（FAULT 中 500ms 点滅） | OUTPUT |
| `DIO_CHANNEL_LED_WARNING` | 8 | D8 | ABS 警告灯（AbsActive=1 で点灯） | OUTPUT |
| `DIO_CHANNEL_BUTTON` | 9 | D9 | 警告確認ボタン（FAULT→OFF 遷移） | INPUT_PULLUP |

ピン番号の変更は `Dio_Cfg.h` の定数を変えるだけで完了します（IoHwAb や SW-C の変更不要）。

### デバウンス（積分カウンタ方式）

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

### ボタン固着検出

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

### ADC センサ電圧監視

`IoHwAb_MainFunction` が 10ms 周期で `Adc_ReadChannel` を呼び出し、10-bit 生値を mV へ変換して
電圧低下を Dem へ報告します。`Dio_ReadChannel` と同様に、ADC アクセスも `IoHwAb_MainFunction` に
集約し、`IoHwAb_Adc_GetValue_mV` は変換済みの静的変数を返すだけにしています。

#### チャネル設定（`Adc_Cfg.h`）

| 定数 | 値 | 意味 |
|------|-----|------|
| `ADC_CHANNEL_SENSOR` | 0（A0） | アナログセンサ入力チャネル |
| `ADC_RESOLUTION_MAX` | 1023 | 10-bit ADC の最大生値 |
| `ADC_REF_VOLTAGE_MV` | 5000 | 基準電圧（5V） |

#### スケーリングと電圧低下検出

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

### IoHwAb API 一覧（`IoHwAb.h`）

| 関数 | 呼び出し元（RTE 経由） | 動作 |
|------|----------------------|------|
| `IoHwAb_Init()` | EcuM_Init | 全 LED を消灯、カウンタをリセット |
| `IoHwAb_LedRunning_SetLevel(level)` | App_WarningIndicator | D6 を点灯 / 消灯 |
| `IoHwAb_LedFault_SetLevel(level)` | App_WarningIndicator | D7 を点灯 / 消灯 |
| `IoHwAb_Led_SetLevel(level)` | App_WarningIndicator | D8 (ABS LED) を点灯 / 消灯 |
| `IoHwAb_MainFunction()` | Os Task 6 (10ms) | デバウンスサンプリング・固着検出・ADC サンプリング |
| `IoHwAb_Button_GetLevel(&level)` | App_EngineManager | デバウンス済み押下状態を返す |
| `IoHwAb_Adc_GetValue_mV(&mv)` | App_EngineManager | 変換済み ADC 電圧値 [mV] を返す |

## アプリケーション（App_EngineManager / App_WarningIndicator）

ASW（Application Software）層の SW-C（Software Component）2 つで構成されます。
各 SW-C は RTE ポート経由でシグナルを受け取り、IoHwAb ポート経由で LED / ボタンを操作します。
EcuM の POST_RUN 遷移時に Rte_Engine タスクと Rte_Warning タスクが停止し、SW-C も停止します。

### エンジン状態遷移

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

### App_WarningIndicator（警告灯 SW-C）

`Rte_ScheduleWarningIndicator` タスクが 500ms 周期で `App_WarningIndicator_Run` を起動します。
3 つの LED は互いに独立して制御され、状態の組み合わせを同時に表現できます。

| LED | ピン | 点灯条件 | 制御 API |
|-----|------|---------|---------|
| RUNNING 灯 | D6 | `EngineState == RUNNING` かつ `FIM_FID_RUNNING_LED` 許可中 | `IoHwAb_LedRunning_SetLevel` |
| FAULT 灯 | D7 | `EngineState == FAULT`（500ms 点滅） | `IoHwAb_LedFault_SetLevel`（毎 Runnable でトグル） |
| ABS 灯 | D8 | `AbsActive == 1` | `IoHwAb_Led_SetLevel` |

FAULT 中に AbsActive=1 のフレームを受信すると D7 が点滅しつつ D8 も同時に点灯します。
POST_RUN 遷移後は Rte_Warning タスクが停止し、LED は消灯状態で固定されます。

## ハードウェア・ビルド

### ハードウェア構成

| 機器 | 用途 |
|------|------|
| Arduino UNO R3 | マイコン本体 |
| MCP2515 + TJA1051 | CAN コントローラ + トランシーバ |
| LED + 抵抗（220〜470 Ω）× 3 | RUNNING 灯（D6）/ FAULT 灯（D7）/ ABS 灯（D8）各 1 本 |
| プッシュボタン | 警告確認ボタン（D9 と GND を接続・内部プルアップ使用） |
| USB-CAN アダプタ | PC との CAN バス接続（解析用） |
| Cangaroo 等 | CAN フレーム送受信ツール |

#### MCP2515 接続（Arduino UNO）

| MCP2515 ピン | Arduino ピン | 備考 |
|-------------|-------------|------|
| CS | D10 | SPI チップセレクト |
| INT | D2 | 受信割り込み（ポーリング）|
| SCK | D13 | SPI クロック |
| SI (MOSI) | D11 | SPI データ出力 |
| SO (MISO) | D12 | SPI データ入力 |

> **D13 は MCP2515 の SCK と共用されるため LED には使用できません。**
> LED は D6（RUNNING）・D7（FAULT）・D8（ABS）それぞれに 220〜470 Ω の抵抗を直列に挿入して接続してください。

> **警告確認ボタン（D9）** は D9 と GND の間にプッシュボタンを接続するだけです。
> Port が `INPUT_PULLUP` で初期化するため、外部プルアップ抵抗は不要です。
> ボタン押下時に D9 が GND と接続され `DIO_LOW` となり、`IoHwAb_Button_GetLevel()` 内で論理反転して「押下=1」に変換されます。

> CAN バスには両端に終端抵抗（120 Ω）が必要です。

### ビルド環境・設定

| 項目 | 値 |
|------|-----|
| プラットフォーム | PlatformIO + Atmel AVR |
| ボード | Arduino UNO |
| フレームワーク | Arduino |
| 外部ライブラリ | coryjfowler/mcp_can @ ^1.5.1 |
| CAN ボーレート | 500 kbps |
| クリスタル周波数 | 8 MHz |
| シリアルモニタ | 115200 bps |

### ビルドと書き込み

```bash
# ビルド
pio run

# 書き込み
pio run --target upload

# シリアルモニタ
pio device monitor
```

## シリアルモニタ出力例

出力フォーマット: `[<起動からの経過ms>ms] LEVEL TAG: メッセージ`
LEVEL は ERROR / WARN  / INFO  / DEBUG の 5 文字固定幅で列が揃います。

```
# 起動シーケンス（2 回目以降）
[1ms] INFO  NvM: Init ok blocks=2
[2ms] INFO  Port: Init pins=4             # ピン方向設定（D6/D7/D8 を OUTPUT、D9 を INPUT_PULLUP に）
[3ms] INFO  Can: Init ok
[4ms] INFO  CanIf: Init ok TX=2 RX=3
[5ms] INFO  PduR: Init ok RX=3 TX=2
[6ms] INFO  Com: Init ok RX=2 TX=1 sig=7
[7ms] INFO  CanTp: Init ok
[8ms] INFO  Dcm: Init ok
[9ms] INFO  Dem: Init NvM restored ev=8   # 前回の DTC を EEPROM から復元
[10ms] INFO  CanSM: Init                  # NO_COM 状態で初期化
[11ms] INFO  ComM: Init ch=1
[12ms] INFO  CanSM: ->FULL_COM            # CanSM_RequestComMode 成功
[12ms] INFO  ComM: ch0 ->mode=2           # ComM_BusSMIndication(FULL_COM) → EcuM_RequestRUN
[14ms] INFO  AppEng: Init->OFF
[15ms] INFO  IoHwAb: Init                 # LED 消灯（ピン方向は Port_Init 済み）
[16ms] INFO  WarnInd: Init
[17ms] INFO  BswM: Init ok rules=3        # ルールエンジン初期化
[18ms] INFO  WdgM: Init ok entities=1     # Alive Supervision 初期化（監視対象 1 エンティティ）
[18ms] INFO  WdgM: HW watchdog enabled (8000ms)  # AVR 実ウォッチドッグ有効化（初期化完了後）
[19ms] INFO  Os: Init ok tasks=9          # タスクスケジューラ起動（全モジュール初期化後）
[19ms] INFO  EcuM: ->RUN                  # 全モジュール初期化完了 → RUN フェーズへ

# 0x100 受信時（EngineOnFlag=1, Speed=500rpm）
[102ms] DEBUG Can_Hw: RX OK id=0x100 dlc=4 [01 F4 00 80]
[103ms] DEBUG CanIf: RX can=0x100 pdu=0
[104ms] DEBUG PduR: RxInd src=0 mod=0 dst=0
[105ms] DEBUG Com: RX iPdu=0 [01 F4 00 80]

# 0x110 受信時（VehicleSpeed=100km/h, BrakeActive=0, AbsActive=0）
[110ms] DEBUG Can_Hw: RX OK id=0x110 dlc=3 [27 10 00]
[111ms] DEBUG CanIf: RX can=0x110 pdu=2
[112ms] DEBUG PduR: RxInd src=2 mod=0 dst=1
[113ms] DEBUG Com: RX iPdu=1 [27 10 00]

# 3 秒周期の Runnable 起動 → OFF→STARTING
[3010ms] INFO  AppEng: OFF->STARTING

# 500ms 周期の警告灯 Runnable（初期は OFF）
[500ms]  INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]

# 次の 3 秒周期 → STARTING→RUNNING
[6010ms] INFO  AppEng: STARTING->RUNNING
[6011ms] DEBUG Can: TX id=0x200 [02]
[6012ms] DEBUG Can_Hw: TX OK id=0x200 dlc=1 [02]
[6013ms] DEBUG AppEng: ADC=3260mV          # ADC 電圧（毎 Runnable サイクルで参考値ログ出力）
[6500ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:0]   # RUNNING → D6 点灯

# 6 秒周期の WdgM 評価（正常時）
[6017ms] DEBUG WdgM: SE0 OK alive=4              # Run() が 2 回 × START/END 2 チェックポイント

# 0x110 受信時（AbsActive=1）→ ABS LED 点灯
[7000ms] DEBUG Can_Hw: RX OK id=0x110 dlc=3 [27 10 C0]
[7001ms] DEBUG Com: RX iPdu=1 [27 10 C0]
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
[19504ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [10 12 59 04 00 01 01 2D]

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
[15106ms] WARN  CanSM: BusOff detected! retry=0/3 recovery in 200ms
[15112ms] INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]  # EngineState が不定に

# 200ms 後に回復試行（アダプタ再接続済みなら正常復帰）
[15312ms] INFO  CanSM: BusOff: restart attempt 1/3
[15313ms] INFO  Dem: PASSED ev=7          # 回復成功を報告。limit=1 のため即座に確定（TF クリア）
[15314ms] INFO  ComM: ch0 ->mode=2        # ComM_BusSMIndication(FULL_COM) → EcuM_RequestRUN → RUN 維持
# → TX 成功 → 通常動作に復帰

# アダプタ未接続のままなら最大 3 回試行後に停止 → EcuM が POST_RUN → SHUTDOWN へ
[30312ms] ERROR CanSM: BusOff: max retries (3) exceeded, recovery stopped
[30313ms] WARN  Dem: FAILED ev=7 dtc=0x000108  # limit=1 のため断念 1 回で即座に確定・EEPROM に保存
[30314ms] INFO  Dem: FreezeFrame ev=7 spd=... tmp=... st=...  # 断念時点のスナップショット
[30315ms] INFO  ComM: ch0 ->mode=0        # ComM_BusSMIndication(NO_COM) → EcuM_ReleaseRUN
[30312ms] INFO  EcuM: ->POST_RUN timeout=5000ms
[30312ms] INFO  WdgM: HW watchdog disabled  # Rte_Engine 停止に伴う Alive FAILED が実リセットを起こさないように
[35312ms] INFO  EcuM: ->SHUTDOWN          # スケジューラ停止（タスク実行なし）

# UDS 診断要求 0x7E0（ReadDataByIdentifier DID=0x0101）
[9500ms] DEBUG Can_Hw: RX OK id=0x7E0 dlc=8 [03 22 01 01 00 00 00 00]
[9501ms] DEBUG CanIf: RX can=0x7E0 pdu=1
[9502ms] INFO  CanTp: RX SF len=3
[9503ms] INFO  Dcm: req SID=0x22
[9504ms] INFO  Dcm: 22 did=0x0101 len=2
[9505ms] INFO  CanTp: TX SF len=5
[9506ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [05 62 01 01 01 F4 00 00]

# UDS SID 0x19/02: DTC 一覧取得（2 件以上 → マルチフレーム応答）
[10000ms] INFO  CanTp: RX SF len=4
[10001ms] INFO  Dcm: req SID=0x19
[10002ms] INFO  Dcm: 19/02 mask=0xFF found=2
[10003ms] INFO  CanTp: TX FF len=11        # 11 バイト → FF+CF に分割
[10004ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [10 0B 59 02 2D 00 01 03]
# (Cangaroo から FC を受信後)
[10200ms] DEBUG CanTp: RX FC fs=0          # ContinueToSend
[10201ms] DEBUG CanTp: TX CF sn=1 pos=6
[10202ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [21 2C 00 01 04 2C 00 00]
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
[10952ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [04 67 01 3A 12 00 00 00]
# テスター側で key = seed XOR 0xA55A = 0x9F48 を計算して送信
[10970ms] INFO  Dcm: req SID=0x27
[10971ms] INFO  Dcm: 27/02 unlocked               # key 一致 → Level1 アンロック
[10972ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [02 67 02 00 00 00 00 00]

# UDS SID 0x14: 全 DTC クリア（アンロック済みのため成功）
[11000ms] INFO  Dcm: 14 ClearAllDTC
[11001ms] INFO  Dem: ClearAll ok
[11002ms] INFO  CanTp: TX SF len=1
[11003ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [01 54 00 00 00 00 00 00]

# UDS SID 0x14: ENGINE_OVERHEAT (DTC 0x000101) のみクリア
[12000ms] INFO  Dcm: 14 ClearDTC dtc=0x000101    # Dem_GetEventIdOfDTC で ev=0 を逆引き
[12001ms] INFO  Dem: Clear ev=0 ok               # 他の DTC のステータスは変化しない
[12002ms] DEBUG Can_Hw: TX OK id=0x7E8 dlc=8 [01 54 00 00 00 00 00 00]

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

## 設計上の注意点

### C / C++ 言語境界

| ファイル | 言語 | 理由 |
|---------|------|------|
| `Can_Hw.cpp` | C++ | MCP_CAN クラスのインスタンス化に placement new が必要 |
| `Det.cpp` | C++ | Arduino の `Serial` API を使用 |
| `Dio_Hw.cpp` | C++ | Arduino の `digitalWrite` API を使用 |
| `Port_Hw.cpp` | C++ | Arduino の `pinMode` API を使用 |
| その他すべて | C | AUTOSAR CP の標準に準拠 |

C ファイルから C++ 関数を呼ぶすべてのヘッダに `extern "C"` ガードを設けています。

### AVR メモリ最適化

Arduino UNO の SRAM は 2 KB しかないため、文字列リテラルを Flash に配置します。
`DET_LOG*` マクロは `PSTR()` で文字列を Flash 領域に置き、`vsnprintf_P` で展開します。

```c
// TAG・フォーマット文字列は Flash に配置される（SRAM を消費しない）
#define TAG "AppEng"
DET_LOGI(TAG, "STARTING->RUNNING");
DET_LOGW(TAG, "RUNNING->FAULT overheat=%u", (unsigned)temp);
```

`Det.cpp` が唯一 `Serial.print()` を呼ぶファイルです。他の `.c` ファイルは `DET_LOG*` マクロのみを使います。

### 設定テーブルの一元管理

各モジュールの設定は対応する `*_PBCfg.c` ファイルで管理しています。

| 変更したい内容 | 編集ファイル |
|---|---|
| CAN ID・DLC（EngineInfo / AbsInfo など） | `CanIf_PBCfg.c` + `CanIf_Cfg.h` |
| シグナルのビット位置・エンディアン | `Com_PBCfg.c` + `Com_Cfg.h` |
| PDU ルーティングパス（RX/TX の対応関係） | `PduR_PBCfg.c` + `PduR_Cfg.h` |
| RTE ポート API（SW-C から見えるシグナル名） | `Rte.h` / `Rte.c` / `Rte_Type.h` |
| EEPROM アドレス・ブロックサイズ | `NvM_PBCfg.c` / `NvM_Cfg.h` |
| Dem デバウンス閾値の変更（イベントごと） | `Dem_Cfg.h` の `DEM_DEBOUNCE_LIMIT_*` |
| Dem 経年回復（Aging）の閾値変更 | `Dem_Cfg.h` の `DEM_AGING_CYCLES_THRESHOLD` |
| **タスク周期・タスク追加/削除** | **`Os_PBCfg.c`** |
| EcuM POST_RUN タイムアウト・RUN ユーザ追加 | `EcuM_Cfg.h` |
| BswM ルール追加・タスクマスク変更 | `BswM_PBCfg.c` / `BswM_Cfg.h` |
| WdgM 監視サイクル・期待回数の変更 | `WdgM_Cfg.h` |
| WdgM 監視対象エンティティの追加 | `WdgM_PBCfg.c` に行を追加し `WDGM_SUPERVISED_ENTITY_COUNT` を更新 |
| WdgM 論理監視（許可されるチェックポイント順序）の変更 | `WdgM_PBCfg.c` の `WdgM_EngineTransitions[]` / チェックポイント ID は `WdgM_Cfg.h` |
| LED / ボタンのピン番号変更 | `Dio_Cfg.h`（`DIO_CHANNEL_LED_RUNNING` / `_LED_FAULT` / `_LED_WARNING` / `_BUTTON`） |
| ADC チャネル・分解能・基準電圧・電圧低下閾値 | `Adc_Cfg.h` / `IoHwAb.c`（`IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV`） |
| Dcm S3 タイマのタイムアウト時間変更 | `Dcm_Cfg.h` の `DCM_S3_TIMEOUT_MS` |
| Dcm SecurityAccess の鍵・試行回数・ロックアウト時間変更 | `Dcm_Cfg.h` の `DCM_SECURITY_KEY_MASK` / `DCM_SECURITY_MAX_ATTEMPTS` / `DCM_SECURITY_DELAY_MS` |
| SecurityAccess で保護するサービスの追加 | `Dcm_Cbk.c` の各ハンドラ先頭で `Dcm_SecurityLevel == 0U` をチェック（`Dcm_HandleClearDtc` 参照） |
| SID にセッション制約を追加・変更 | `Dcm_Cbk.c` の `Dcm_SidSessionTable[]` に行を追加（`DCM_SESSION_MASK_DEFAULT` / `_EXTENDED` / `_ALL`） |
| FiM の抑止対象機能・イベントの追加・変更 | `FiM_Cfg.h` に `FIM_FID_*` を追加し、`FiM_PBCfg.c` の `FiM_Functions[]` に行を追加 |
| CanSM Bus-Off リトライ回数の変更 | `CanSM_Cfg.h` の `CANSM_BUSOFF_MAX_RETRIES` |

## 前提条件

- ARXML や設定ツールは使用しません
- MCP2515 クリスタルは 8 MHz を想定しています（`Can_CrystalFreqType` で変更可）
- CAN バスには終端抵抗（120 Ω）を両端に取り付けてください

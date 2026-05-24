# arduino-mcp2515-autosar-my-study

Arduino + MCP2515 を用いて AUTOSAR CP の BSW CAN スタックを学習目的で実装したプロジェクトです。
ARXML や設定ツールは使用せず、コードで階層構造・型定義・設定テーブルを再現しています。

## 概要

AUTOSAR の ASW / RTE / BSW という 3 層アーキテクチャを Arduino UNO 上に構築し、エンジン管理を模した状態遷移アプリケーションを動作させます。

- **RX（CAN ID 0x100）**: 外部ツールから速度・水温・ON フラグを受信
- **TX（CAN ID 0x200）**: エンジン状態（OFF / STARTING / RUNNING / FAULT）を定期送信
- **診断 RX（CAN ID 0x7E0）**: UDS 診断要求を受信（ISO 14229-1 / ISO 15765-2）
- **診断 TX（CAN ID 0x7E8）**: UDS 診断応答を送信（マルチフレーム対応）

## アーキテクチャ

```
┌──────────────────────────────────────────────┐
│  ASW  App_EngineManager                      │  エンジン状態遷移
├──────────────────────────────────────────────┤
│  RTE  ポートベース API (S/R インタフェース)   │
├──────────────────────────────────────────────┤
│  OS   タイムトリガスケジューラ                │  タスク周期管理
├──────────────────────────────────────────────┤
│  BSW  COM    シグナルのビット単位パック/アンパック │
│       PduR   マルチキャスト PDU ルーティング     │
│       CanIf  CAN ID ↔ 論理 PDU マッピング      │
│       Can    MCP2515 ハードウェア制御           │
│       CanTp  ISO 15765-2 マルチフレーム処理     │
│       Dcm    UDS 診断通信 (ISO 14229-1)        │
│       Dem    DTC 管理（NvM 経由で永続化）       │
│       NvM    EEPROM 抽象化・RAM ミラー管理      │
│       Det    Serial 出力ブリッジ（デバッグ用）   │
├──────────────────────────────────────────────┤
│  HAL  Mcp2515_Wrapper  SPI 通信ラッパー        │  C++ のみ
└──────────────────────────────────────────────┘
```

各モジュールは上位層のヘッダのみに依存し、下位層の実装詳細を知りません。

## ディレクトリ構成

```
├── src/
│   ├── main.cpp                  # 全 BSW の設定テーブル・初期化・メインループ
│   ├── Asw/
│   │   ├── App_EngineManager.h
│   │   └── App_EngineManager.c   # エンジン状態遷移
│   ├── Os/
│   │   ├── Os_Cfg.h              # タスク数定数
│   │   ├── Os.h / Os.c           # タイムトリガスケジューラ（Os_SchedulerStep）
│   │   ├── Os_PBCfg.h
│   │   └── Os_PBCfg.c            # タスクテーブル（周期・関数ポインタ）
│   ├── Rte/
│   │   ├── Rte_Type.h            # アプリ型エイリアス（ARXML 自動生成相当）
│   │   ├── Rte.h
│   │   └── Rte.c                 # ポート API（周期管理は Os へ移管済み）
│   └── Bsw/
│       ├── Can/                  # CAN ドライバ（AUTOSAR SWS_Can 準拠 API）
│       ├── CanIf/                # CAN インタフェース
│       ├── CanTp/                # CAN トランスポートプロトコル（ISO 15765-2）
│       ├── Com/                  # COM（シグナル管理）
│       ├── PduR/                 # PDU ルーター
│       ├── Det/                  # Default Error Tracer（Serial ブリッジ）
│       ├── Dcm/                  # 診断通信マネージャ（UDS ISO 14229-1）
│       ├── Dem/                  # 診断イベントマネージャ（DTC 管理）
│       ├── NvM/                  # Non-Volatile Memory Manager（EEPROM 抽象化）
│       └── Mcp2515/              # MCP2515 C++ ラッパー
├── dbc/
│   └── engine_manager.dbc        # CAN シグナル定義（Cangaroo 等で使用）
└── platformio.ini
```

## CAN フレーム仕様

エンディアンはすべてビッグエンディアン（Motorola / CAN 標準）。
ビット 0 = byte[0] の MSB、ビット 7 = byte[0] の LSB。

### RX フレーム（外部 → Arduino）

| CAN ID | DLC | ビット位置 | サイズ | シグナル | 単位 |
|--------|-----|-----------|--------|---------|------|
| 0x100 | 4 | 0–15 | 16 bit | EngineSpeed | rpm |
| 0x100 | 4 | 16–23 | 8 bit | CoolantTemp | ℃ |
| 0x100 | 4 | 24 | 1 bit | EngineOnFlag | 0=OFF / 1=ON |

**RUNNING 状態に入る最小フレーム例（DLC=4）：**

```
byte[0] byte[1] byte[2] byte[3]
  01      F4      00      80
  └─────┘         └──┘   └──── EngineOnFlag=1（bit 24 = byte[3] の MSB）
  Speed=500rpm    Temp=0℃
```

### TX フレーム（Arduino → 外部）

| CAN ID | DLC | ビット位置 | サイズ | シグナル | 値 |
|--------|-----|-----------|--------|---------|-----|
| 0x200 | 1 | 0–7 | 8 bit | EngineState | 0=OFF / 1=STARTING / 2=RUNNING / 3=FAULT |

3 秒周期で現在の状態を送信します。

## UDS 診断通信（ISO 14229-1 / ISO 15765-2）

Dcm (Diagnostic Communication Manager) が UDS サービスを処理し、
CanTp (CAN Transport Protocol) が ISO 15765-2 のフレーム分割・組立を担います。
診断フレームはアプリデータとは独立した CAN ID で通信します。

### 診断フレームルーティング

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

CAN 0x100（COM データ）と 0x7E0（診断要求）は PduR でルートが分離されており、互いに干渉しません。

### 対応 UDS サービス

| SID  | サービス名 | SubFunction | 正応答 SID |
|------|-----------|-------------|-----------|
| 0x10 | DiagnosticSessionControl | 0x01=Default / 0x03=Extended | 0x50 |
| 0x11 | ECUReset | 0x01=hardReset / 0x03=softReset | 0x51 |
| 0x14 | ClearDiagnosticInformation | groupOfDTC=0xFFFFFF（全クリア） | 0x54 |
| 0x19 | ReadDTCInformation | 0x01=件数取得 / 0x02=DTC 一覧取得 | 0x59 |
| 0x22 | ReadDataByIdentifier | — | 0x62 |
| 0x3E | TesterPresent | 0x00 | 0x7E |

非対応サービスは NRC 0x11（serviceNotSupported）で応答します。

### DID 一覧（0x22 ReadDataByIdentifier）

| DID    | データ      | 型                                            | 単位 |
|--------|------------|-----------------------------------------------|------|
| 0x0101 | EngineSpeed | uint16, big-endian                           | rpm  |
| 0x0102 | CoolantTemp | uint8                                        | ℃   |
| 0x0103 | EngineState | uint8（0=OFF / 1=STARTING / 2=RUNNING / 3=FAULT） | —  |

### フレーム例（シングルフレーム）

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

## CanTp（ISO 15765-2 トランスポートプロトコル）

CanTp モジュールが ISO 15765-2 のフレーム処理を担い、
DCM は PCI バイトを意識せず生 UDS ペイロードのみを扱います。

### ISO 15765-2 フレーム構造

| フレーム種別 | PCI (byte[0]) | 内容 |
|------------|--------------|------|
| SF (Single Frame) | `0x0N` N=ペイロード長 | UDS ペイロード ≤ 7 バイト |
| FF (First Frame)  | `0x1H 0xLL` HL=総長 | UDS ペイロード ≥ 8 バイト の先頭 6 バイト |
| CF (Consecutive Frame) | `0x2n` n=シーケンス番号 | 続きのデータ（最大 7 バイト/フレーム） |
| FC (Flow Control) | `0x3X` X=FS | CTS(0)/WAIT(1)/OVFLW(2)、BS、STmin |

### RX 状態マシン（Arduino 受信側）

```
IDLE ──── SF 受信 ──────────────────→ Dcm_ComIndication → IDLE
     ──── FF 受信 → FC(CTS) 送信 ──→ WAIT_CF
WAIT_CF ─ CF 受信(未完) ────────────→ WAIT_CF
        ─ CF 受信(完成) ────────────→ Dcm_ComIndication → IDLE
        ─ N_Cr タイムアウト(5 秒) ──→ IDLE (中断)
```

### TX 状態マシン（Arduino 送信側）

```
IDLE ──── ≤7 バイト → SF 送信 ──────────────────────────→ IDLE
     ──── ≥8 バイト → FF 送信 ──────────────────────────→ WAIT_FC
WAIT_FC ─ FC(CTS) 受信 ─────────────→ SEND_CF
        ─ N_Bs タイムアウト(5 秒) ──→ IDLE (中断)
SEND_CF ─ CF 送信(MainFunction 毎) ─→ 完了 → IDLE
```

### マルチフレーム応答例（2 DTC の場合）

2 件以上の DTC が一致すると応答が 8 バイトを超え、FF + CF に分割されます。

```
# 要求（SF）
送信 → 0x7E0: [04 19 02 FF 00 00 00 00]

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

### Cangaroo で FC を手動送信する方法

Arduino が FF を送信すると WAIT_FC 状態になります。
5 秒以内に Cangaroo から FC を送信してください。

```
Plugins → RawSender で新しいフレームを作成:
  ID:   0x7E0
  Data: 30 00 00 00 00 00 00 00
        └┘ └┘ └┘
        FC CTS  BS=0  STmin=0ms
```

## DEM 診断イベント管理（AUTOSAR SWS_DEM）

Dem (Diagnostic Event Manager) モジュールがエンジン管理の故障を DTC として管理します。
DTC の永続化は NvM (Non-Volatile Memory Manager) 経由で行い、
Dem は EEPROM アドレスを直接知りません（NvM_WriteBlock / NvM_ReadBlock のみ使用）。
電源オフ後もクリア操作（SID 0x14）が行われない限り DTC が保持されます。

### イベントと DTC コード

| EventId | イベント名 | 検出条件 | DTC コード |
|---------|-----------|---------|-----------|
| 0 | ENGINE_OVERHEAT | CoolantTemp ≥ 100 ℃（RUNNING 中） | 0x000101 |
| 1 | ENGINE_STALL | EngineSpeed < 100 rpm（RUNNING 中） | 0x000102 |
| 2 | ENGINE_SPEED_NO_FLAG | speed > 0 かつ flag = 0（OFF 中） | 0x000103 |
| 3 | STARTING_TIMEOUT | 起動から 5 秒超過（STARTING 中） | 0x000104 |

### 複数 DTC を発生させる手順

各操作後は 3〜4 秒待ってシリアルモニタで状態遷移を確認してください（Runnable は 3 秒周期）。

| 順序 | 送信フレーム | 状態 | 登録 DTC |
|-----|------------|------|---------|
| 1 | `100#01.F4.19.00` speed=500, flag=0 | OFF→FAULT | ENGINE_SPEED_NO_FLAG |
| 2 | `100#00.00.19.00` flag=0 | FAULT→OFF | — |
| 3 | `100#00.64.19.80` speed=100, flag=1 | OFF→STARTING（6 秒待つ） | STARTING_TIMEOUT |
| 4 | `100#00.00.19.00` flag=0 | FAULT→OFF | — |
| 5 | `100#03.E8.19.80` speed=1000, flag=1 | OFF→STARTING→RUNNING | — |
| 6 | `100#03.E8.64.80` temp=100, flag=1 | RUNNING→FAULT | ENGINE_OVERHEAT |
| 7 | `100#00.00.19.00` flag=0 | FAULT→OFF | — |
| 8 | `100#03.E8.19.80` speed=1000, flag=1 | OFF→STARTING→RUNNING | — |
| 9 | `100#00.32.19.80` speed=50, flag=1 | RUNNING→FAULT | ENGINE_STALL |

### DTC ステータスバイト（ISO 14229-1 Annex B）

SID 0x19 の応答に含まれるステータスバイトの各ビットの意味。

| ビット | マスク | 略称 | 意味 |
|-------|--------|------|------|
| bit0 | 0x01 | TF | testFailed — 今現在壊れている |
| bit2 | 0x04 | PDTC | pendingDTC — 今の電源サイクルで失敗した |
| bit3 | 0x08 | CDTC | confirmedDTC — 確定済み・EEPROM 保存済み |
| bit4 | 0x10 | TNCLC | testNotCompletedSinceLastClear — クリア後未テスト |
| bit5 | 0x20 | TFSLC | testFailedSinceLastClear — クリア後に失敗あり |

statusAvailabilityMask = **0x2D**（本実装がサポートするビットの OR）。

### DTC ライフサイクル

| フェーズ | TF | PDTC | CDTC | TFSLC | TNCLC | ステータス値 |
|---------|:--:|:----:|:----:|:-----:|:-----:|:-----------:|
| 初回起動（EEPROM 未初期化） | 0 | 0 | 0 | 0 | **1** | `0x10` |
| PASSED 報告後 | 0 | 0 | 0 | 0 | 0 | `0x00` |
| **FAILED 報告後** | **1** | **1** | **1** | **1** | 0 | **`0x2D`** |
| 電源再投入後（TF のみリセット） | **0** | 1 | **1** | 1 | 0 | **`0x2C`** |
| SID 0x14 実行後 | 0 | 0 | **0** | **0** | **1** | `0x10` |

- **CDTC（bit3）が永続化の本体**。電源再投入後も保持されるため、整備ツールで過去の故障を確認できる。
- TF（bit0）は電源再投入時にクリア。「今は動いているが過去に壊れた」を表現できる。
- CDTC を消すには SID 0x14 による明示的なクリアのみ。

### フレーム例（DTC 操作）

**DTC 件数を確認（confirmedDTC のみ = statusMask 0x08）:**
```
送信 → 0x7E0: [04 19 01 08 00 00 00 00]
受信 ← 0x7E8: [06 59 01 2D 01 00 NN 00]
                                   ↑ byte[5] が DTC 件数
```

**DTC 一覧を取得（全ステータス = statusMask 0xFF）:**

1 件の場合（SF 応答）:
```
送信 → 0x7E0: [04 19 02 FF 00 00 00 00]
受信 ← 0x7E8: [07 59 02 2D D1 D2 D3 SS]
                            └────────┘ └── byte[7]: DTC ステータス
                            byte[4-6]: DTC コード (例: 00 01 01 = EngineOverheat)
```

2 件以上の場合（マルチフレーム応答 → FC 要）:
```
送信 → 0x7E0: [04 19 02 FF 00 00 00 00]
受信 ← 0x7E8: [10 0B 59 02 2D D1 D2 D3]  FF（総長 0x0B=11 バイト）
送信 → 0x7E0: [30 00 00 00 00 00 00 00]  FC(CTS)
受信 ← 0x7E8: [21 SS D1 D2 D3 SS 00 00]  CF（残りの DTC）
```

**全 DTC クリア:**
```
送信 → 0x7E0: [04 14 FF FF FF 00 00 00]
受信 ← 0x7E8: [01 54 00 00 00 00 00 00]
```

### EEPROM レイアウト

Arduino UNO の内蔵 EEPROM 先頭 5 バイトを使用します。
アドレス割り当ては NvM_Cfg.h (`NVM_BLOCK_DEM_*_EEPROM_ADDR`) で一元管理しています。
Dem は NvM_BlockIdType (NVM_BLOCK_ID_DEM_MAGIC / NVM_BLOCK_ID_DEM_STATUS) でのみアクセスします。

| アドレス | NvM ブロック | 内容 |
|---------|-------------|------|
| 0x00 | NVM_BLOCK_ID_DEM_MAGIC (1 byte) | マジックバイト（0xDE = 有効データあり） |
| 0x01 | NVM_BLOCK_ID_DEM_STATUS (4 bytes) | EVENT_ENGINE_OVERHEAT ステータス |
| 0x02 | 〃 | EVENT_ENGINE_STALL ステータス |
| 0x03 | 〃 | EVENT_ENGINE_SPEED_NO_FLAG ステータス |
| 0x04 | 〃 | EVENT_STARTING_TIMEOUT ステータス |

## エンジン状態遷移

```
          flag=1
  [OFF] ──────────> [STARTING]
    ^                  │  │  │
    │ flag=0           │  │  └── timeout(5s) ──> [FAULT]
    │                  │  └───── flag=0 ────────> [OFF]
    │        speed≥500 │
    │                  v
    │              [RUNNING]
    │                  │  │
    │    flag=0 ────── ┘  └── temp≥100℃ or speed<100rpm
    │                                   ↓
    └──────── flag=0 ────────────── [FAULT]
```

| 状態 | 遷移条件 | 遷移先 |
|------|---------|--------|
| OFF | EngineOnFlag = 1 | STARTING |
| STARTING | EngineSpeed ≥ 500 rpm | RUNNING |
| STARTING | 5 秒経過 | FAULT |
| STARTING | EngineOnFlag = 0 | OFF |
| RUNNING | CoolantTemp ≥ 100 ℃ | FAULT（過熱） |
| RUNNING | EngineSpeed < 100 rpm | FAULT（エンスト） |
| RUNNING | EngineOnFlag = 0 | OFF |
| FAULT | EngineOnFlag = 0 | OFF |

## ハードウェア構成

| 機器 | 用途 |
|------|------|
| Arduino UNO R3 | マイコン本体 |
| MCP2515 + TJA1051 | CAN コントローラ + トランシーバ |
| USB-CAN アダプタ | PC との CAN バス接続（解析用） |
| Cangaroo 等 | CAN フレーム送受信ツール |

### MCP2515 接続（Arduino UNO）

| MCP2515 ピン | Arduino ピン | 備考 |
|-------------|-------------|------|
| CS | D10 | SPI チップセレクト |
| INT | D2 | 受信割り込み（ポーリング）|
| SCK | D13 | SPI クロック |
| SI (MOSI) | D11 | SPI データ出力 |
| SO (MISO) | D12 | SPI データ入力 |

> CAN バスには両端に終端抵抗（120 Ω）が必要です。

## ビルド環境・設定

| 項目 | 値 |
|------|-----|
| プラットフォーム | PlatformIO + Atmel AVR |
| ボード | Arduino UNO |
| フレームワーク | Arduino |
| 外部ライブラリ | coryjfowler/mcp_can @ ^1.5.1 |
| CAN ボーレート | 500 kbps |
| クリスタル周波数 | 8 MHz |
| シリアルモニタ | 115200 bps |

## ビルドと書き込み

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
[3ms] INFO  Can: Init ok
[4ms] INFO  CanIf: Init ok TX=1 RX=2
[5ms] INFO  PduR: Init ok RX=2 TX=1
[6ms] INFO  Com: Init ok RX=1 TX=1 sig=4
[7ms] INFO  CanTp: Init ok
[8ms] INFO  Dcm: Init ok
[9ms] INFO  Dem: Init NvM restored ev=4   # 前回の DTC を EEPROM から復元
[10ms] INFO  AppEng: Init->OFF
[11ms] INFO  Os: Init ok tasks=3          # タスクスケジューラ起動（全モジュール初期化後）

# 0x100 受信時（EngineOnFlag=1, Speed=500rpm）
[102ms] DEBUG Mcp2515: RX OK id=0x100 dlc=4 [01 F4 00 80]
[103ms] DEBUG CanIf: RX can=0x100 pdu=0
[104ms] DEBUG Com: RX iPdu=0 [01 F4 00 80]

# 3 秒周期の Runnable 起動 → OFF→STARTING
[3010ms] INFO  AppEng: OFF->STARTING

# 次の 3 秒周期 → STARTING→RUNNING
[6010ms] INFO  AppEng: STARTING->RUNNING
[6011ms] DEBUG Can: TX id=0x200 [02]
[6012ms] DEBUG Mcp2515: TX OK id=0x200 dlc=1 [02]

# 過熱検出（CoolantTemp=101℃）→ RUNNING→FAULT
[9010ms] WARN  AppEng: RUNNING->FAULT overheat=101
[9011ms] WARN  Dem: FAILED ev=0 dtc=0x000101   # NvM 経由で EEPROM に保存

# UDS 診断要求 0x7E0（ReadDataByIdentifier DID=0x0101）
[9500ms] DEBUG Mcp2515: RX OK id=0x7E0 dlc=8 [03 22 01 01 00 00 00 00]
[9501ms] DEBUG CanIf: RX can=0x7E0 pdu=1
[9502ms] INFO  CanTp: RX SF len=3
[9503ms] INFO  Dcm: req SID=0x22
[9504ms] INFO  Dcm: 22 did=0x0101 len=2
[9505ms] INFO  CanTp: TX SF len=5
[9506ms] DEBUG Mcp2515: TX OK id=0x7E8 dlc=8 [05 62 01 01 01 F4 00 00]

# UDS SID 0x19/02: DTC 一覧取得（2 件以上 → マルチフレーム応答）
[10000ms] INFO  CanTp: RX SF len=4
[10001ms] INFO  Dcm: req SID=0x19
[10002ms] INFO  Dcm: 19/02 mask=0xFF found=2
[10003ms] INFO  CanTp: TX FF len=11        # 11 バイト → FF+CF に分割
[10004ms] DEBUG Mcp2515: TX OK id=0x7E8 dlc=8 [10 0B 59 02 2D 00 01 03]
# (Cangaroo から FC を受信後)
[10200ms] DEBUG CanTp: RX FC fs=0          # ContinueToSend
[10201ms] DEBUG CanTp: TX CF sn=1 pos=6
[10202ms] DEBUG Mcp2515: TX OK id=0x7E8 dlc=8 [21 2C 00 01 04 2C 00 00]
[10203ms] INFO  CanTp: TX done

# UDS SID 0x14: 全 DTC クリア
[11000ms] INFO  Dcm: 14 ClearAllDTC
[11001ms] INFO  Dem: ClearAll ok
[11002ms] INFO  CanTp: TX SF len=1
[11003ms] DEBUG Mcp2515: TX OK id=0x7E8 dlc=8 [01 54 00 00 00 00 00 00]
```

## 設計上の注意点

### C / C++ 言語境界

| ファイル | 言語 | 理由 |
|---------|------|------|
| `Mcp2515_Wrapper.cpp` | C++ | MCP_CAN クラスのインスタンス化に placement new が必要 |
| `Det.cpp` | C++ | Arduino の `Serial` API を使用 |
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
| CAN ID・DLC・フィルタ | `Can_PBCfg.c` / `CanIf_PBCfg.c` |
| シグナルのビット位置・エンディアン | `Com_PBCfg.c` |
| PDU ルーティングパス | `PduR_PBCfg.c` |
| EEPROM アドレス・ブロックサイズ | `NvM_PBCfg.c` / `NvM_Cfg.h` |
| **タスク周期・タスク追加/削除** | **`Os_PBCfg.c`** |

## 前提条件

- ARXML や設定ツールは使用しません
- MCP2515 クリスタルは 8 MHz を想定しています（`Can_CrystalFreqType` で変更可）
- CAN バスには終端抵抗（120 Ω）を両端に取り付けてください

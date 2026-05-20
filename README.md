# arduino-mcp2515-autosar-my-study

Arduino + MCP2515 を用いて AUTOSAR CP の BSW CAN スタックを学習目的で実装したプロジェクトです。
ARXML や設定ツールは使用せず、コードで階層構造・型定義・設定テーブルを再現しています。

## 概要

AUTOSAR の ASW / RTE / BSW という 3 層アーキテクチャを Arduino UNO 上に構築し、エンジン管理を模した状態遷移アプリケーションを動作させます。

- **RX（CAN ID 0x100）**: 外部ツールから速度・水温・ON フラグを受信
- **TX（CAN ID 0x200）**: エンジン状態（OFF / STARTING / RUNNING / FAULT）を定期送信
- **診断 RX（CAN ID 0x7E0）**: UDS 診断要求を受信（ISO 14229-1）
- **診断 TX（CAN ID 0x7E8）**: UDS 診断応答を送信

## アーキテクチャ

```
┌──────────────────────────────────────────────┐
│  ASW  App_EngineManager                      │  エンジン状態遷移
├──────────────────────────────────────────────┤
│  RTE  Rte_ScheduleRunnables (3 秒周期)       │  ポートベース API
├──────────────────────────────────────────────┤
│  BSW  COM    シグナルのビット単位パック/アンパック │
│       PduR   マルチキャスト PDU ルーティング     │
│       CanIf  CAN ID ↔ 論理 PDU マッピング      │
│       Can    MCP2515 ハードウェア制御           │
│       Dcm    UDS 診断通信 (ISO 14229-1)        │
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
│   ├── Rte/
│   │   ├── Rte_Type.h            # アプリ型エイリアス（ARXML 自動生成相当）
│   │   ├── Rte.h
│   │   └── Rte.c                 # ポート API・3 秒スケジューラー
│   └── Bsw/
│       ├── Can/                  # CAN ドライバ（AUTOSAR SWS_Can 準拠 API）
│       ├── CanIf/                # CAN インタフェース
│       ├── Com/                  # COM（シグナル管理）
│       ├── PduR/                 # PDU ルーター
│       ├── Det/                  # Default Error Tracer（Serial ブリッジ）
│       ├── Dcm/                  # 診断通信マネージャ（UDS ISO 14229-1）
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

## UDS 診断通信（ISO 14229-1）

Dcm (Diagnostic Communication Manager) モジュールが UDS over CAN を処理します。
診断フレームはアプリデータとは独立した CAN ID で通信します。

### 診断フレームルーティング

```
外部テスター（CANアナライザ等）
  │  CAN 0x7E0  [UDS 要求]
  ↓
MCP2515 → Can → CanIf（CanId=0x7E0 → RxPduId=1）
                  ↓
                PduR（パス 1: DCM 専用ルート）
                  ↓
                Dcm_ComIndication() → UDS サービス処理
                  ↓
                PduR_Transmit(SrcPduId=1) → CanIf → Can
  │  CAN 0x7E8  [UDS 応答]
  ↓
外部テスター
```

CAN 0x100（COM データ）と 0x7E0（診断要求）は PduR でルートが分離されており、互いに干渉しません。

### 対応 UDS サービス

| SID  | サービス名 | SubFunction | 正応答 SID |
|------|-----------|-------------|-----------|
| 0x10 | DiagnosticSessionControl | 0x01=Default / 0x03=Extended | 0x50 |
| 0x11 | ECUReset | 0x01=hardReset / 0x03=softReset | 0x51 |
| 0x22 | ReadDataByIdentifier | — | 0x62 |
| 0x3E | TesterPresent | 0x00 | 0x7E |

非対応サービスは NRC 0x11（serviceNotSupported）で応答します。

### DID 一覧（0x22 ReadDataByIdentifier）

| DID    | データ      | 型                                            | 単位 |
|--------|------------|-----------------------------------------------|------|
| 0x0101 | EngineSpeed | uint16, big-endian                           | rpm  |
| 0x0102 | CoolantTemp | uint8                                        | ℃   |
| 0x0103 | EngineState | uint8（0=OFF / 1=STARTING / 2=RUNNING / 3=FAULT） | —  |

### フレーム例

ISO 15765-2 Single Frame のみ対応。`byte[0]` が PCI バイト（上位ニブル=0x0, 下位ニブル=UDS ペイロード長）。

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

> **注意**: Multi-frame（FF/CF/FC）は未実装。Single Frame（最大 7 バイト UDS ペイロード）のみ対応。

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

```
[Can_Init] Initializing CAN...
[Can_Init] CAN Initialized successfully
[Com_Init] OK
  RxIPdus=1 TxIPdus=1 Signals=4
[Dcm_Init] session=Default
[EngineManager] Init->OFF

# 0x100 受信時（Mcp2515_Wrapper レベル）
[Mcp2515_Read] OK >>>> ID=0x100 DLC=4 DATA=[01 F4 00 80]
[CanIf_RxInd] CanId=0x100 -> RxPduId=0
[Com_RxInd] IPdu=0 raw=[01 F4 00 80]
[EngineManager] OFF->STARTING

# 次の 3 秒周期
[EngineManager] STARTING->RUNNING

# 0x200 送信時
[Mcp2515_Send] OK >>>> ID=0x200 DLC=1 DATA=[02]

# UDS 診断要求 0x7E0: [03 22 01 01] EngineSpeed 読み出し
[Mcp2515_Read] OK >>>> ID=0x7E0 DLC=8 DATA=[03 22 01 01 00 00 00 00]
[CanIf_RxInd] CanId=0x7E0 -> RxPduId=1
[Dcm] SID=0x22
[Dcm 0x22] DID=0x101 len=2
[Mcp2515_Send] OK >>>> ID=0x7E8 DLC=8 DATA=[05 62 01 01 01 F4 00 00]
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

```c
// Det API を通じて Flash 文字列を出力（SRAM を消費しない）
Det_LogP(PSTR("[EngineManager] STARTING->RUNNING"));
```

`Det.cpp` が唯一 `Serial.print()` を呼ぶファイルです。他の `.c` ファイルは Det API のみを使います。

### 設定テーブルの一元管理

ARXML 自動生成を模し、BSW 設定をすべて `main.cpp` に集約しています。CAN ID・DLC・シグナルのビット位置などを変更する際はこのファイルのみを編集します。

## 前提条件

- ARXML や設定ツールは使用しません
- MCP2515 クリスタルは 8 MHz を想定しています（`Can_CrystalFreqType` で変更可）
- CAN バスには終端抵抗（120 Ω）を両端に取り付けてください

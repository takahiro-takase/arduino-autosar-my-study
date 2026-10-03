# CAN フレーム仕様

> [README](../README.md) の補足から分離しました。

<a id="can-frame-spec"></a>
# CAN フレーム仕様

エンディアンはすべてビッグエンディアン（Motorola / CAN 標準）。
ビット 0 = byte[0] の MSB、ビット 7 = byte[0] の LSB。

**Tx/Rx フレーム一覧**

| Tx/Rx | フレーム | CAN ID | DLC | ビット位置 | サイズ | シグナル | 単位・値域 |
|-------|---------|--------|-----|-----------|--------|---------|----------|
| Tx | MeterStatus | 0x200 | 6 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–7 | 8 bit | EngineState | 0=OFF<br>1=STARTING<br>2=RUNNING<br>3=FAULT<br>（E2E 保護なし） |
|  |  |  |  | 8 | 1 bit | (update-bit) | EngineState 単体の update-bit（SWS_Com_00061/00062）。値変化時送信=1、周期フロア再送=0 |
|  |  |  |  | 16 | 1 bit | RunLamp (mirror) | WarningStatus.RunLampと同値のミラー（uds_tester仮想メータ表示用、本プロジェクト独自拡張） |
|  |  |  |  | 17 | 1 bit | FaultLamp (mirror) | WarningStatus.FaultLampと同値のミラー |
|  |  |  |  | 18 | 1 bit | AbsLamp (mirror) | WarningStatus.AbsLampと同値のミラー |
|  |  |  |  | 24–39 | 16 bit | EngineSpeed (mirror) | rpm（0–15000）。EngineInfoの検証済み値のミラー |
|  |  |  |  | 40–47 | 8 bit | CoolantTemp (mirror) | ℃（0–255）。EngineInfoの検証済み値のミラー |
| Tx | WarningStatus | 0x210 | 1 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0 | 1 bit | RunLamp | 0=消灯<br>1=点灯<br>（RUNNING LED D6 と同値） |
|  |  |  |  | 1 | 1 bit | FaultLamp | 0=消灯<br>1=点灯<br>（FAULT LED D7 と同値、点滅中は 500ms ごとに反転） |
|  |  |  |  | 2 | 1 bit | AbsLamp | 0=消灯<br>1=点灯<br>（ABS LED D8 と同値） |
|  |  |  |  | 3 | 1 bit | (update-bit) | Signal Group 全体の update-bit（SWS_Com_00801）。値変化時送信=1、MIXED 周期フロア再送=0 |
| Tx | E2EHealthStatus | 0x220 | 5 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–15 | 16 bit | E2E CRC | CRC16（多項式0x1021、E2E Profile05）<br>（Counterバイト+byte[3-4] → DataID=0x0220の順で計算。リトルエンディアンでbyte[0-1]に格納） |
|  |  |  |  | 16–23 | 8 bit | E2E Counter | 0–255 のリングカウンタ（送信のたびに +1、予約値なし） |
|  |  |  |  | 24–31 | 8 bit | E2ECrcErrCount | 0–255（飽和）<br>EngineInfo/AbsInfo受信のE2E CRC不一致累積数 |
|  |  |  |  | 32–39 | 8 bit | E2ESeqErrCount | 0–255（飽和）<br>EngineInfo/AbsInfo受信のE2Eシーケンス異常累積数 |
| Rx | EngineInfo | 0x100 | 7 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–15 | 16 bit | E2E CRC | CRC16（多項式0x1021、E2E Profile05）<br>（Counterバイト+byte[3-6] → DataID=0x0100の順で計算。リトルエンディアンでbyte[0-1]に格納） |
|  |  |  |  | 16–23 | 8 bit | E2E Counter | 0–255 のリングカウンタ（フレーム脱落・重複検出用、予約値なし） |
|  |  |  |  | 24–39 | 16 bit | EngineSpeed | rpm（0–15000） |
|  |  |  |  | 40–47 | 8 bit | CoolantTemp | ℃（0–255） |
|  |  |  |  | 48 | 1 bit | EngineOnFlag | 0=OFF / 1=ON |
| Rx | AbsInfo | 0x110 | 6 | ↓ | ↓ | ↓ | ↓ |
|  |  |  |  | 0–15 | 16 bit | E2E CRC | CRC16（多項式0x1021、E2E Profile05）<br>（Counterバイト+byte[3-5] → DataID=0x0110の順で計算。リトルエンディアンでbyte[0-1]に格納） |
|  |  |  |  | 16–23 | 8 bit | E2E Counter | 0–255 のリングカウンタ<br>（フレーム脱落・重複検出用、予約値なし） |
|  |  |  |  | 24–39 | 16 bit | VehicleSpeed | 0.01 km/h（raw 0x0064 = 1.00 km/h） |
|  |  |  |  | 40 | 1 bit | BrakeActive | 0=解除 / 1=作動 |
|  |  |  |  | 41 | 1 bit | AbsActive | 0=非作動 / 1=ABS 作動中 |

### TX フレーム（Arduino → 外部）

**MeterStatus（メータ ECU / CAN ID 0x200 / DLC=6 / E2E 保護なし / TxModeMode=MIXED）**

byte[2] の警告灯3bitと byte[3-4] の EngineSpeed・byte[5] の CoolantTemp は、
`uds_tester` の仮想メータ表示タブが1フレームだけで RPM・水温・警告灯をデコード
できるよう、`App_EngineManager`/`App_WarningIndicator` がそれぞれ
`EngineInfo`/`WarningStatus` と同じ値をミラー送信する本プロジェクト独自の拡張
です（詳細は [`docs/modules/Com_Notes.md`](modules/Com_Notes.md) 参照）。

`App_EngineManager_Run()`（3000ms 周期）は `Rte_Write_EngineStatus_EngineState()` で
値を書き込むだけで、送信自体は Com が判断します。`EngineState` が変化すると Com が
次回 `Com_MainFunctionTx()`（Os の 100ms タスク）で送信し、変化がなくても一定間隔
（周期フロア、後述）で再送し続けます。実際の CAN 送信（SPI 通信）は必ず
`Com_MainFunctionTx()` 側で行われるため、`App_EngineManager_Run()` 自身が SPI
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
次回 `Com_MainFunctionTx()` で送信されます。E2E 保護は付与していません
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

**E2EHealthStatus（メータ ECU / CAN ID 0x220 / DLC=5 / AUTOSAR E2E Profile 05 保護 / PERIODIC）**

`E2EMon`（CDD 相当モジュール）が EngineInfo/AbsInfo 受信側の E2E 検証エラー累積数を
集計し、Com の PERIODIC 送信モードにより 6000ms 周期で自動送信されるネットワーク
健全性テレメトリです。テレメトリ自体の破損を監視ツールが検出できるよう、CRC16 ベースの
E2E Profile05 で保護しています（データ＋8bit Counter＋16bit CRC。EngineInfo/AbsInfo の
受信保護と同じプロファイル）。詳細は「E2E 保護」セクションの
E2EMon サブセクションを参照してください。

### RX フレーム（外部 → Arduino）

**EngineInfo（エンジン ECU / CAN ID 0x100 / DLC=7 / AUTOSAR E2E Profile 05 保護）**

**RUNNING 状態に入るフレーム例（Speed=500rpm, Temp=0℃, EngineOnFlag=1, Counter=0）：**
```
byte[0] byte[1] byte[2] byte[3] byte[4] byte[5] byte[6]
  XX      XX      00      01      F4      00      80
  │       │       │       └─────┘         └──┘   └──── EngineOnFlag=1（bit48 = byte[6] の MSB）
  │       │       └─ Counter=0             Speed=500rpm    Temp=0℃
  └───────┴─────── CRC16（リトルエンディアン、XX XX は自動計算）
```
**AbsInfo（ABS ECU / CAN ID 0x110 / DLC=6 / AUTOSAR E2E Profile 05 保護）**

**ABS 作動フレーム例（VehicleSpeed=100km/h, BrakeActive=1, AbsActive=1, Counter=0）：**
```
byte[0] byte[1] byte[2] byte[3] byte[4] byte[5]
  XX      XX      00      27      10      C0
  │       │       │       └─────┘         └──── BrakeActive=1（bit40）, AbsActive=1（bit41）
  │       │       └─ Counter=0             Speed=10000 (0x2710) → 100.00 km/h
  └───────┴─────── CRC16（リトルエンディアン、XX XX は自動計算）
```

（SWS_E2E_00397/00405 に準拠し、CRC16 を先頭2バイト・Counter をそれに続く1バイト全体に
配置している）

> E2E Counter と CRC は uds_tester ツールが自動計算して付加します。
> Cangaroo から手動送信する場合は byte[0-1]=CRC16 の計算値（リトルエンディアン）、
> byte[2]=Counter 値を手動で付加してください。

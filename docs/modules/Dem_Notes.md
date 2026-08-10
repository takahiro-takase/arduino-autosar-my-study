# Dem（診断イベント管理、AUTOSAR SWS_DEM）

> [README](../../README.md) の「[診断スタック](../../README.md#diag-stack)」節から分離。

Dem (Diagnostic Event Manager) モジュールがエンジン管理の故障を DTC として管理します。
DTC の永続化は NvM (Non-Volatile Memory Manager) 経由で行い、
Dem は EEPROM アドレスを直接知りません（NvM_WriteBlock / NvM_ReadBlock のみ使用）。
電源オフ後もクリア操作（SID 0x14）が行われない限り DTC が保持されます。
イベント/DTC コード一覧、デバウンス、DTC ライフサイクル、経年回復（Aging）、
FreezeFrame、ExtendedData、EEPROM レイアウトの詳細を以下にまとめます。

## イベントと DTC コード

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

## デバウンス (Counter-based Debouncing)

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
> 合わせている（経緯は [DEVLOG](../DEVLOG.md#dem-デバウンスカウンタの反転バグ) 参照）。

## 閾値 (limit) の決め方

モニタ（報告元）が Dem に報告する前に、**既に十分な持続性チェックを行っているか**で
閾値を変えています。

| limit | 対象イベント | 理由 |
|---|---|---|
| 1（即確定） | BUTTON_STUCK, CAN_BUSOFF, E2E_ABSINFO, E2E_ENGINEINFO | IoHwAb の 5 秒固着判定／CanSM の 3 回リトライ後の断念は、それ自体が「十分粘った結果」。E2E は CRC 計算自体がエラー判定のため単発で確定。Dem 側で重ねてデバウンスすると二重チェックになり、確定が不必要に遅れる（または構造的に確定不可能になる） |
| 2（複数回要求） | ENGINE_OVERHEAT, ENGINE_STALL, ENGINE_SPEED_NO_FLAG, STARTING_TIMEOUT, COMM_TIMEOUT, ADC_VOLT_LOW | モニタは瞬時のしきい値超え（temp≥100 等）をそのまま報告するだけで、持続性チェックを行っていない。単発の誤検出で確定させないために Dem 側でデバウンスする |

> イベントごとの閾値にした経緯（当初は全イベント共通の単一閾値だった）は
> [DEVLOG](../DEVLOG.md#dem-デバウンス閾値を単一値からイベントごとに変更した経緯) を参照。

イベントごとの報告パターンによって、確定までにかかる実時間が異なります。

| イベント | 報告パターン | 確定までの目安 |
|---|---|---|
| ENGINE_OVERHEAT / STALL / SPEED_NO_FLAG / STARTING_TIMEOUT | 状態遷移の瞬間に単発報告（limit=2） | 同じ故障が**別々の機会に 2 回**発生する必要あり |
| COMM_TIMEOUT | 故障継続中は毎 Runnable サイクル（3000ms）報告（limit=2） | 2 サイクル分＝約 3000〜6000ms 追加 |
| ADC_VOLT_LOW | 故障継続中は毎 10ms サイクル報告（limit=2） | 数十 ms（実質的に即時） |
| BUTTON_STUCK / CAN_BUSOFF | 持続性チェック後に 1 回だけ報告（limit=1） | 即時確定 |

## 複数 DTC を発生させる手順

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

## ABS LED 動作確認手順

RUNNING 状態で以下の AbsInfo フレームを 0x110 で送信して LED 動作を確認します。

AbsInfo は E2E Profile05 保護付きのため、**Counter（byte[2]、フル値）と CRC16（byte[0-1]）を
正しく付加**しないと Com が E2E エラーと判定してフレームを破棄し、LED は反応しません。
uds_tester の「AbsInfo (0x110)」ボタンは Counter と CRC を自動計算して送信します。

| シグナル設定 | AbsActive | BrakeActive | D6 RUNNING | D7 FAULT | D8 ABS |
|-------------|-----------|-------------|:----------:|:--------:|:------:|
| `data: [0x27, 0x10, 0x00]` | 0 | 0 | 点灯 | 消灯 | 消灯 |
| `data: [0x27, 0x10, 0x40]` | 0 | 1 | 点灯 | 消灯 | 消灯（BrakeActive は LED に影響しない） |
| `data: [0x27, 0x10, 0xC0]` | 1 | 1 | 点灯 | 消灯 | **点灯** |
| `data: [0x27, 0x10, 0x80]` | 1 | 0 | 点灯 | 消灯 | **点灯** |

FAULT 状態で AbsActive=1 のフレームを送信すると、D7 が点滅しつつ D8 も同時に点灯します（3 LED は独立制御）。

## DTC ステータスバイト（ISO 14229-1 Annex B）

SID 0x19 の応答に含まれるステータスバイトの各ビットの意味。

| ビット | マスク | 略称 | 意味 |
|-------|--------|------|------|
| bit0 | 0x01 | TF | testFailed — 今現在壊れている |
| bit2 | 0x04 | PDTC | pendingDTC — 今の電源サイクルで失敗した |
| bit3 | 0x08 | CDTC | confirmedDTC — 確定済み・EEPROM 保存済み |
| bit4 | 0x10 | TNCLC | testNotCompletedSinceLastClear — クリア後未テスト |
| bit5 | 0x20 | TFSLC | testFailedSinceLastClear — クリア後に失敗あり |

statusAvailabilityMask = **0x2D**（本実装がサポートするビットの OR）。

## DTC ライフサイクル

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

## フレーム例（DTC 操作）

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

## PendingDTC の自動クリア

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

## 経年回復（Aging）

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

## EEPROM レイアウト

Arduino UNO の内蔵 EEPROM 先頭 46 バイトを使用します（NvM の CRC バイトを含む。
詳細は [`NvM_Notes.md`](./NvM_Notes.md) 参照）。
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

DEM_EXTENDED のみ冗長ブロック（2 面化）にしています。理由・仕組みは
[`NvM_Notes.md`](./NvM_Notes.md) の「冗長ブロック」参照。

<a id="freezeframe"></a>
## FreezeFrame（故障時スナップショット）

DTC が FAILED に遷移した瞬間の車両状態（EngineSpeed / CoolantTemp / EngineState）を Dem が記録し、
UDS SID 0x19 subFunc 0x04（reportDTCSnapshotRecordByDTCNumber）で読み出せます。
本実装は **RAM のみに保持**し EEPROM へは永続化しません（電源 OFF で消去される学習用簡略化）。
イベントごとに保持するレコードは 1 件（recordNumber=0x01）のみです。

### 記録の仕組み

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

### フレーム例（SID 0x19/04）

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
## ExtendedData（故障確定回数）

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

### 記録の仕組み

```
Dem_ReportErrorStatus(EventId, FAILED) が呼ばれ、確定 FAILED に遷移した瞬間
（FreezeFrame 更新と同じ箇所）に:
  Dem_OccurrenceCounter[EventId]++   （0xFF で飽和、それ以上は増えない）
  NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter)

SID 0x14（全クリア／DTC 指定クリア）で対象イベントの Dem_OccurrenceCounter も 0 に戻る
（CDTC 自体や経年回復カウンタとは独立した値だが、クリア操作のタイミングは共通）。
```

### フレーム例（SID 0x19/06）

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

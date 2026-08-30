# CanTp（ISO 15765-2 トランスポートプロトコル）

> [README](../../README.md) の「[診断スタック](../../README.md#diag-stack)」節から分離。
> `tools/can_tool`/CAPL風DSLの解説はモジュール非依存のツール文書のため
> README側にそのまま残しています。

CanTp モジュールが ISO 15765-2 のフレーム処理を担い、
DCM は PCI バイトを意識せず生 UDS ペイロードのみを扱います。

## ISO 15765-2 フレーム構造

| フレーム種別 | PCI (byte[0]) | 内容 |
|------------|--------------|------|
| SF (Single Frame) | `0x0N` N=ペイロード長 | UDS ペイロード ≤ 7 バイト |
| FF (First Frame)  | `0x1H 0xLL` HL=総長 | UDS ペイロード ≥ 8 バイト の先頭 6 バイト |
| CF (Consecutive Frame) | `0x2n` n=シーケンス番号 | 続きのデータ（最大 7 バイト/フレーム） |
| FC (Flow Control) | `0x3X` X=FS | CTS(0)/WAIT(1)/OVFLW(2)、BS、STmin |

## RX 状態マシン（Arduino 受信側）

```
IDLE ──── SF 受信 ──────────────────→ Dcm_ComIndication → IDLE
     ──── FF 受信 → FC(CTS) 送信 ──→ WAIT_CF
WAIT_CF ─ CF 受信(未完) ────────────→ WAIT_CF
        ─ CF 受信(完成) ────────────→ Dcm_ComIndication → IDLE
        ─ N_Cr タイムアウト(1000ms) ─→ IDLE (中断)
        ─ 別の SF/FF 受信 ──────────→ 進行中の受信を中断し、新規受信として処理（後述）
```

## WAIT_CF 中に別の SF/FF を受信した場合（SWS_CanTp_00124）

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

## TX 状態マシン（Arduino 送信側）

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

## フロー制御パラメータ（BS / STmin）

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

## TX バッファサイズと上位層(Dcm)のペイロード長の不整合（実機ログで発覚、2026-08）

`CanTp_Transmit()` は `PduInfoPtr->SduLength > CANTP_TX_BUFFER_SIZE` を
「invalid len」として即座に拒否する（`CanTp.c` 参照）。この定数は
`CanTp_Cfg.h` で固定値 32 バイトとして設定していたが、Dcm 側の
`Dcm_TxBuf`（`DCM_TX_BUF_SIZE = 3 + DEM_EVENT_COUNT*4`）は
`DEM_EVENT_COUNT` の変化に自動追従するよう既に修正済み（過去に
5→8 に増えた際、固定値 32 のバッファがオーバーフローしたバグの教訓から）
だった一方、CanTp 側の 32 バイトはこれと連動しておらず、独立した
固定値のまま取り残されていた。

UDS 0x19 subFunc 0x0A（reportSupportedDTC、statusMask による絞り込みを
行わず `DEM_EVENT_COUNT` 件を無条件に返す）を追加した際、
`DEM_EVENT_COUNT=10` での応答長は `3+10*4=43` バイトとなり、
32 バイトの `CANTP_TX_BUFFER_SIZE` を常に超えるため、実機で
subFunc 0x0A を送るたびに以下のように応答が一切送信されない状態に
なっていた（ユニットテストでは検出できなかった。`[env:native_dcm]` は
`CanTp_fake.c` で長さチェックを行わないため）:

```
受信 → 0x7E0: [02 19 0A 00 00 00 00 00]
[Dcm] Dcm_HandleReadDtcSupported: 19/0A supported=10
[CanTp] CanTp_Transmit: TX E: invalid len   ← 応答が送信されない
```

**対応**: `CANTP_TX_BUFFER_SIZE` を 48 バイトに引き上げ、`Dcm_Cbk.c` の
`DCM_TX_BUF_SIZE`（43バイト）を上回るようにした。48 は FF(6) + CF×6(7×6)
という ISO-TP のフレーム境界にちょうど一致する値で、最後の CF に
パディングの無駄が出ない。43 への最小限の対応（44 等）ではなく、
`DEM_EVENT_COUNT` が今後 11 に増えても（3+11*4=47 バイト）このバッファを
再度触らずに済む余裕を持たせた（`CanTp_Cfg.h` のコメントに、大きく
変更する場合はこの値も再確認する旨を明記）。

**教訓**: 新しい RX/TX パスを追加する際は、直接呼び出す層だけでなく、
その先の隣接レイヤ（本件では Dcm → CanTp）が独自に持つ同種の
サイズ/状態ガードで到達不能・拒否されないかも確認する必要がある
（`Dcm_TxBuf` 自身のオーバーフローは対策済みだったが、CanTp 側の
バッファは見落としていた）。

**実機再検証（修正後）**: `CANTP_TX_BUFFER_SIZE=44`（後に48へ再調整、下記参照）適用後、subFunc 0x0A
を再送信したところ FF（`len=43`）が受理され、CF sn=1〜6 まで送信完了
（`CanTp_SendNextCF: TX done`）。応答バイト列を手動デコードし、
`DEM_EVENT_COUNT=10` 件の DTC レコード（[SID,subFunc,availMask] +
4バイト×10）が正しい順序・内容で組み立てられていることを確認した。

なお同じログ中、直後に届いた同一リクエストの重複（`uds_tester` 側の
既知の現象）に対しては `CanTp_Transmit: TX E: busy`（FF+CF 送信中の
チャネルビジー判定、[SWS_CanTp_00123]）で正しく拒否されており、
これは新しい問題ではなく想定通りの排他動作。

## マルチフレーム応答例（2 DTC の場合）

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

## Cangaroo で FC を手動送信する方法

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

## 0x2E WriteDataByIdentifier — 複数フレーム要求 (FF+CF) の実機検証

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

## 0x2E WriteDataByIdentifier — CryptoKeyUpdate (DID 0x0108) による鍵更新

DID 0x0108 (CryptoKeyUpdate) は KeyM の鍵更新セッションを駆動する模擬鍵
マスターです。要求ペイロードは `keyName(1) + 新しい鍵16バイト = 17バイト`
（`SID(1)+DID(2)+17=20バイト`）で、TestPattern と同じく FF+CF の複数フレーム
要求になります。

```
要求: [0x2E, 0x01, 0x08, keyName, key0..key15]  (20 バイト、SF 不可)
応答: [0x6E, 0x01, 0x08]  (SF, 3 バイト)
```

`keyName` は `'1'`(0x31, ImmobilizerCmd 用) のみ有効（以前は `'2'`(0x32,
E2EHealthStatus 用) も使えたが、E2EHealthStatus の SecOC 撤去に伴い削除済み）。
ECU 内部では `Dcm_HandleWriteDataById()` が
1回の呼び出し内で `KeyM_Start()`→`KeyM_Update()`→`KeyM_Finalize()` を実行し、
`Csm_KeyElementSet()`/`Csm_KeySetValid()` 経由で `Crypto.c` の RAM 鍵テーブル
を書き換えます。鍵は更新直後は無効化され、`KeyM_Finalize()`（＝この DID 書き込み
の完了）まで MAC 生成/検証には使われません。鍵材料は RAM のみで NVM に
永続化しないため、再起動すると `Crypto_PBCfg.c` の初期値に戻ります。

extendedSession かつ SecurityAccess Level1 アンロック済みでなければ NRC 0x33。
`keyName` 不一致・下位層の失敗時は NRC 0x31 (requestOutOfRange) を返します。

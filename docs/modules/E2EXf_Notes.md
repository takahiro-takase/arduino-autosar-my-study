# E2EXf（E2E Transformer 統合層）

> [README](../../README.md) の「[E2E 保護](../../README.md#e2e-p01)」節から分離。
> E2E Profile01/05 のCRC/カウンタアルゴリズム自体の学習ノートは
> [`docs/E2E_Profile1_Notes.md`](./E2E_Profile1_Notes.md) /
> [`docs/E2E_Profile5_Notes.md`](./E2E_Profile5_Notes.md) を参照してください。
> このファイルは EngineInfo/AbsInfo(RX)・E2EHealthStatus(TX) への実際の適用と
> Rte/Com/Dem との統合について扱います。

Com と E2E の間を仲介する統合層。RX は `E2E_P05Check` でデータ破壊・フレーム
脱落・重複・誤ルーティングを検出して Dem へ報告し、TX は `E2E_P05Protect` で
Counter・CRC16 を付加する。Com 自身はこの層の存在を知らない。

## E2E が保護する故障モデル

| 故障モデル | 検出方法 | 対応 E2E フィールド |
|-----------|---------|------------------|
| データ破壊（ビット化け等） | 受信 CRC ≠ 計算 CRC | byte[0-1] CRC16 |
| フレーム脱落 | カウンタが 2 以上飛ぶ | byte[2] Counter（フル値） |
| フレーム重複 | カウンタが前回と同じ | byte[2] Counter（フル値） |
| 誤ルーティング（他 ECU のフレームが混入） | DataID が違うため CRC が一致しない | DataID（EngineInfo=0x0100 / AbsInfo=0x0110）を CRC 計算に含む |

## 受信側（Check）— EngineInfo / AbsInfo

E2E チェックの仕組み自体は両フレームで完全に共通（`E2E_P05Check` の同一実装を
設定テーブルだけ変えて使い回す）のため、以下では区別せず一つの仕組みとして説明し、
フレームレイアウトと設定値のみ個別に示します。2026-08 に Profile01(CRC8+4bitカウンタ)
から Profile05(CRC16+8bitカウンタ) へ移行しました（Profile01 用の設定・実装は
`E2E_P01.c`/`E2EXf_RxConfigType` として参考実装のまま残っています）。

### フレームレイアウト

**EngineInfo（CAN 0x100）:**
```
byte[0-1] : CRC16（多項式 0x1021、開始値 0xFFFF、SWS_E2E_00400/00406 準拠、リトルエンディアン）
            計算対象: byte[2](Counter), byte[3], byte[4], byte[5], byte[6],
                      DataID_low(0x00), DataID_high(0x01)
            （CRC バイト自身[byte0-1]を除き、Counter →データ→ DataID の順）
byte[2]   : Counter（8bit フル値、0→1→…→255→0 のリングカウンタ、予約値なし）
byte[3-6] : シグナルデータ（EngineSpeed / CoolantTemp / EngineOnFlag）
```

**AbsInfo（CAN 0x110）:**
```
byte[0-1] : CRC16（多項式 0x1021、開始値 0xFFFF、SWS_E2E_00400/00406 準拠、リトルエンディアン）
            計算対象: byte[2](Counter), byte[3], byte[4], byte[5],
                      DataID_low(0x10), DataID_high(0x01)
            （CRC バイト自身[byte0-1]を除き、Counter →データ→ DataID の順）
byte[2]   : Counter（8bit フル値、0→1→…→255→0 のリングカウンタ、予約値なし）
byte[3-5] : シグナルデータ（VehicleSpeed / BrakeActive / AbsActive）
```

CRC16 を先頭2バイト・Counter をそれに続く1バイト全体に配置するこのヘッダレイアウトは
SWS_E2E_00397/00405 にそのまま準拠している（詳細は
[`docs/E2E_Profile5_Notes.md`](./E2E_Profile5_Notes.md) 参照）。

### カウンタデルタ判定と 6 状態 state machine（`E2E_P05Check`）

受信カウンタと前回有効カウンタの差分 `delta` を軸に、AUTOSAR `E2E_P05CheckStatusType`
（8.2.4.4節、Figure 8-8）準拠の 6 状態で判定します。Profile05 のカウンタは 0〜255 の
フルレンジを予約値なしで循環するため、折り返しの補正は uint8 の自然なラップアラウンド
（`(uint8)(received - lastValid)`）がそのまま mod-256 補正になり、Profile01 のような
mod-15 の特別な式は不要です。

```
CRC 不一致                → ERROR（Counter 側は判定しない。NULL/長さ不正とも共用の値）
delta == 0                → REPEATED（フレーム重複）
delta > MaxDeltaCounter   → WRONGSEQUENCE（許容超過）
delta == 1                → OK（正常）
1 < delta <= MaxDeltaCounter → OKSOMELOST（正常だが一部消失、許容範囲内）
```

**Profile01 との構造的な違い: INITIAL/SYNC 状態・SyncCounter 再ロック機構が無い**
（詳細は [`docs/E2E_Profile5_Notes.md`](./E2E_Profile5_Notes.md) 参照）。
Profile01 は初回受信を `INITIAL` として特別扱いし、`WRONGSEQUENCE` 検知後は
`SyncCounterInit` 回分連続して正常受信するまで `SYNC` として再ロックする機構を
持っていましたが、公式の `E2E_P05CheckStateType`/`E2E_P05ConfigType` にはこれらに
相当するフィールドが無く、初回の `E2E_P05Check()` 呼び出しも他の呼び出しと全く同じ
delta 計算にそのまま乗ります（`ProtectState`/`CheckState` とも初期値は `Counter=0`
なので、起動直後の最初のフレーム（Counter=0）を初回 Check（State も Counter=0）
すると `delta=0` となり `REPEATED` 判定になります。「異常」ではなく Profile05の
仕様どおりの挙動です。ただし実運用では送信元 ECU が既に稼働中で Counter が0以外
から始まっていることが普通にあるため、この挙動をそのまま統合すると起動直後に
誤ったDTCが確定してしまう問題があった。E2EXf層で補った対処は
`E2EXf_RxConfigTypeP05.WaitForFirstData`（`E2EXf.h`/`E2EXf.c`）を参照）。

※ 公式仕様にはこのほか `NONEWDATA`（前回呼び出し以降、新規データなし）がありますが、
本実装は `Com_RxIndication()` からフレーム受信時にのみ `E2E_P05Check` を呼び出す設計のため、
「呼ばれたが新規データがない」状況が発生せず、この状態は実装していません。

### Com モジュールとの統合（E2E Transformer 方式）

Com は EngineInfo/AbsInfo のペイロード内容を一切検証しません。`Com_RxIndication()` は
受信の都度、CRC/Counter の妥当性にかかわらず**無条件に** RX バッファ・タイムアウトタイマを
更新した上で、`Com_IPduConfigType.RxIndicationCbk`（I-PDU ごとに `Com_PBCfg.c` で設定する
汎用フック、Com 本体は中身を関知しない）を呼び出すだけです。

実際の検証は `Rte` 層に置かれたグルー関数（`Rte_COMRxInd_EngineInfo()` /
`Rte_COMRxInd_AbsInfo()`、`src/Rte/Rte.c`）が担い、`Com_ReceiveSignalGroupArray()` で
I-PDU の生バイト列を取得した上で `E2EXf_InverseTransformP05()`（`src/Bsw/E2EXf/E2EXf.c`、
中身は `E2E_P05Check()` への薄いラッパー）へ渡します。検証に合格した場合のみ、Rte 内部の
ミラー変数（`Rte_EngineInfoMirror` / `Rte_AbsInfoMirror`）へ最新値を反映します。
検証に失敗した場合はミラーを更新せず、直前の正常値がシグナルとして残り続けます
（＝これが E2E 違反時のフェイルセーフの実体）。

```
Com_RxIndication() (RxIndicationCbk が設定された I-PDU。現状 IPduId=0/1 が対象):
  RX バッファ・タイムアウトタイマを無条件に更新（Com は E2E を関知しない）
  RxIndicationCbk() を呼び出す
    = Rte_COMRxInd_EngineInfo() / Rte_COMRxInd_AbsInfo() （Rte.c）
        Com_ReceiveSignalGroupArray() で生バイト列を取得
        E2EXf_InverseTransformP05() を呼び出す（CheckStatus 出力引数で生の6状態も受け取る）
          → E2E_P05Check() を実行
            OK / OKSOMELOST
                      → Dem_ReportErrorStatus(DemEventId, PASSED)
                        E_OK を返す → Rte ミラーを更新
            REPEATED / WRONGSEQUENCE / ERROR
                      → DET_LOGW(TAG="E2EXf", "InverseTransformP05 NG DemEvent=%u st=%u")
                        Dem_ReportErrorStatus(DemEventId, FAILED)
                        E_NOT_OK を返す → Rte ミラー非更新（前回値を維持）
        CheckStatus を Rte_MapE2EStatusP05() で Rte_IStatusType へ写像し
        Rte_EngineInfoStatus / Rte_AbsInfoStatus（静的変数）へ保存
          → 次回以降の Rte_Read_*() 呼び出しがこれを返す（詳細は次項）
```

OK/OKSOMELOST はいずれも CRC が正しく検証されているため「データとしては信頼できる」
と判断し受理します（Profile05 には Profile01 の SYNC/INITIAL に相当する状態が無いため、
判定は OK 単独で行います）。
E2E エラー（REPEATED/WRONGSEQUENCE/ERROR）時はミラーを更新しないため、
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

### E2E 検証ステータスの Rte 経由での公開（`Rte_IStatusType`）

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
E2E_P05Check() の6状態          Rte_MapE2EStatusP05() による分類  Rte_IStatusType
OK                         ─────────────────────────────→  RTE_E_OK
OKSOMELOST                 ─────────────────────────────→  RTE_E_SOFT_TRANSFORMER_ERROR
REPEATED / WRONGSEQUENCE /
ERROR                      ─────────────────────────────→  RTE_E_HARD_TRANSFORMER_ERROR

Rte_Read_SpeedSensor_EngineSpeed() 等（Rte.c）:
  *data は常に Rte_EngineInfoMirror の現在値を書き込む（戻り値に関わらず）
    ← 本実装の意図的な簡略化。実 AUTOSAR は HARD_TRANSFORMER_ERROR 時に
       出力引数を更新しないと定めるが（[SWS_Rte_08576] 等）、本プロジェクトは
       「E2E フェイルセーフ = 前回の正常値を使い続ける」という既存の設計方針
       （本ファイル冒頭）を優先し、あえてこの点だけ逸脱している
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

### E2E モジュール設定（`src/Bsw/E2EXf/E2EXf_PBCfg.c` の E2E 設定テーブル）

E2E の設定・状態実体は Com から独立し、`E2EXf_PBCfg.c` で保持しています
（`E2EXf_EngineInfoRxCfg` / `E2EXf_AbsInfoRxCfg`（Profile05、`E2EXf_RxConfigTypeP05`）/
`E2EXf_E2EHealthStatusTxCfgP05`（Profile05、`E2EXf_TxConfigTypeP05`）として
まとめ、`Rte.c` のグルー関数から参照）。

**EngineInfo（`E2EXf_EngineInfoRxCfg`）:**

| 設定 | 値 | 意味 |
|------|----|------|
| DataID | 0x0100 | CRC 計算に含む ID（誤ルーティング検出） |
| DataLength | 7 | フレーム全体のバイト長（CRC16 2B + Counter 1B + シグナル 4B） |
| MaxDeltaCounter | 1 | 許容する最大カウンタ飛び量 |
| Offset | 0 | E2E ヘッダ(CRC16+Counter)が始まる byte インデックス |

**AbsInfo（`E2EXf_AbsInfoRxCfg`）:**

| 設定 | 値 | 意味 |
|------|----|------|
| DataID | 0x0110 | CRC 計算に含む ID（誤ルーティング検出） |
| DataLength | 6 | フレーム全体のバイト長（CRC16 2B + Counter 1B + シグナル 3B） |
| MaxDeltaCounter | 1 | 許容する最大カウンタ飛び量 |
| Offset | 0 | E2E ヘッダ(CRC16+Counter)が始まる byte インデックス |

### ログ例

**正常受信時:**
```
（E2E 正常時はログなし — バッファが静かに更新される）
```

**CRC 不一致発生時（AbsInfo）:**
```
[7001ms] WARN  E2EXf: InverseTransformP05 NG DemEvent=8 st=7  ← st=7: ERROR（CRC 不一致）
[7002ms] DEBUG Dem: ev=8 debounce=1 (PREFAILED)  ← limit=1 のため次回確定
[7003ms] WARN  Dem: FAILED ev=8 dtc=0x000109     ← 即座に確定・EEPROM に保存
```

**CRC 不一致発生時（EngineInfo）:**
```
[8001ms] WARN  E2EXf: InverseTransformP05 NG DemEvent=9 st=7  ← st=7: ERROR（CRC 不一致）
[8002ms] DEBUG Dem: ev=9 debounce=1 (PREFAILED)  ← limit=1 のため次回確定
[8003ms] WARN  Dem: FAILED ev=9 dtc=0x00010A     ← 即座に確定・EEPROM に保存
```

**カウンタ飛び超過（WRONGSEQUENCE）検知の様子（実機ログ、uds_tester で意図的にカウンタを飛ばして送信）:**

Profile05 には Profile01 の SyncCounter 再ロック機構が無いため、カウンタ飛びを
検知した次のフレームが正常な delta（==1）でさえあれば、それだけで即座に OK へ戻ります
（SYNC 状態を経由した複数フレームの再ロック待ちは発生しません）。

```
[30824ms] WARN  E2EXf: InverseTransformP05 NG DemEvent=8 st=64  ← WRONGSEQUENCE（カウンタ飛び検知）
[30827ms] WARN  Dem: FAILED ev=8 dtc=0x000109      ← このフレームは不採用（ミラー非更新）
[31038ms]                                          ← 次のフレームが delta==1 なら無ログ = 即座に OK 復帰
```

**動作確認方法（意図的な E2E エラー）:**

uds_tester ツールの EngineInfo/AbsInfo データ入力欄で byte[0-1]（CRC16 バイト）を誤った値に
書き換えてから送信ボタンを押すと、Rte 層の E2E Transformer フックが CRC エラーを検出して
それぞれ DEM_EVENT_E2E_ENGINEINFO / DEM_EVENT_E2E_ABSINFO が報告されます。

**動作確認方法（WRONGSEQUENCE → OK 復帰）:**

uds_tester は送信するたびに Counter を自動で +1 するため、通常操作では delta は常に 1 に
なり WRONGSEQUENCE は発生しません。意図的にカウンタを飛ばすには、データ入力欄の byte[2]
（Counter、フル値）を手動で前回送信値+2 以上の値へ書き換えてから送信します
（CRC は送信直前に自動再計算されるため、Counter だけを書き換えれば十分です）。
これにより WRONGSEQUENCE → （次の1回の正常送信で即座に）OK 復帰という一連の遷移を
実機ログで確認できます（EngineInfo/AbsInfo いずれも同じ手順。Profile01 の SyncCounter
再ロックのような複数フレームの待ちは Profile05 には無いため、1 フレームで復帰します）。

## 送信側（Protect）— E2EHealthStatus

E2E 保護の対象は、実際にはエンジン状態フレーム（MeterStatus）ではなく、
`E2EMon`（[`E2EMon_Notes.md`](./E2EMon_Notes.md) 参照）が発行するネットワーク健全性
テレメトリ `E2EHealthStatus`（CAN 0x220）です。監視ツールが「E2E エラー累積数」
自体の破損を検出できるようにする狙いで、MeterStatus ではなくこちらへ E2E 保護を
適用しています（MeterStatus は E2E 保護なしの単純な直接送信に単純化しています）。

E2EHealthStatus は以前 E2E Profile01（CRC8）+ SecOC の二重保護でしたが、E2E 単体の
検出能力を高めるため **E2E Profile05（CRC16、`docs/AUTOSAR_SWS_E2ELibrary.pdf` 7.6節）**
に切り替え、SecOC は撤去しました（Profile05 はヘッダが CRC16(2byte)+Counter(1byte)=
3byte で、Profile01(2byte 相当)より1byte 増えるため、classic CAN の DLC=8 上限内に
SecOC のFreshness/MAC 分の余地が無くなったのが理由。詳細は
[`SecOC_Notes.md`](./SecOC_Notes.md) 参照）。

### フレームレイアウト

```
byte[0-1] : CRC16（多項式 0x1021、開始値 0xFFFF、SWS_E2E_00400/00406 準拠、リトルエンディアン）
            計算対象: byte[2](Counter), byte[3], byte[4],
                      DataID_low(0x20), DataID_high(0x02)
            （CRC バイト自身[byte0-1]を除き、Counter →データ→ DataID の順。
            Profile01 とは異なり DataID は「データの後」に投入する、SWS_E2E_00399/00406）
byte[2]   : Counter（8bit フル値、0→1→…→255→0 のリングカウンタ、予約値なし）
byte[3]   : シグナルデータ（E2ECrcErrCount）
byte[4]   : シグナルデータ（E2ESeqErrCount）
```

### エンコード処理（`E2E_P05Protect`）

`E2E_P05Check`（受信検証、EngineInfo/AbsInfo RXで実際に使用。前述参照）と
対になる、送信側のエンコード処理です。検証すべき前回値がないため、状態は次に送信する
Counter 値だけを保持します。

```
E2E_P05Protect() 呼び出しごと（SWS_E2E_00405/00406/00407/00409 準拠）:
  1. Data[Offset+2] = 現在の Counter（8bit フル値）を書き込む
  2. Data[Offset+2..DataLength-1]（Counter+データ）→ DataID_low → DataID_high
     の順で CRC16（開始値 0xFFFF）を計算し、Data[Offset..Offset+1] へリトルエンディアンで書き込む
  3. Counter++   ← 次回送信用に進める（uint8 の自然なラップアラウンド、0xFF の次は 0）
```

Counter は `E2E_P05ProtectInit()` で 0 に初期化されるため、起動後最初に送信される
E2EHealthStatus フレームは Counter=0 です。

### Com モジュールとの統合（E2E Transformer 方式）

Com は E2EHealthStatus のペイロードにも一切関知しません。E2EHealthStatus は
`COM_TX_MODE_PERIODIC` のため、`Com_MainFunctionTx()` が自分の周期タイマで
送信を決定した際に `Com_IPduConfigType.TxTransformCbk`（`Com_PBCfg.c` で
`Rte_COMTransform_E2EHealthStatus` を設定）を実 TX バッファへのポインタと
長さ付きで呼び出すだけです（DIRECT/MIXED I-PDU の変化時送信も同じ
`Com_MainFunctionTx()` から呼ばれるため、「送信直前の最終変換」の仕組みを
そのまま再利用しています）。実際に Counter・
CRC16 を書き込むのは `Rte_COMTransform_E2EHealthStatus()`（`src/Rte/Rte.c`）で、
中身は `E2EXf_TransformP05()`（`E2E_P05Protect()` への薄いラッパー。E2EXf.h には
Profile01 用の `E2EXf_Transform()` と並行して Profile05 専用の型・関数を追加している。
実 AUTOSAR の E2E Transformer が ARXML からプロファイルごとに専用コードを生成する
方式に倣ったもので、汎用的なプロファイル切り替え機構は導入していない）を呼ぶだけです。
AbsInfo の Check とは逆に、失敗や再送は発生しません（送信側なので検証すべき
前提がないため）。E2EMon（データの生産者）はこの E2E 保護の存在を一切知りません。

```
Com_MainFunctionTx()（PERIODIC モードの I-PDU。現状 IPduId=2 が対象）:
  TxTransformCbk(Com_TxBuffer[PduId], DLC) を呼び出す
    = Rte_COMTransform_E2EHealthStatus() （Rte.c）
        E2EXf_TransformP05() を呼び出す
          → E2E_P05Protect() を実行
            Counter を書き込み +1、CRC16 を計算して書き込む
  PduR_Transmit() で送信（SecOC 等の中間モジュールは挟まらず CanIf へ直結）
```

### E2E モジュール設定（`src/Bsw/E2EXf/E2EXf_PBCfg.c` の `E2EXf_E2EHealthStatusTxCfgP05`）

| 設定 | 値 | 意味 |
|------|----|------|
| DataID | 0x0220 | CRC 計算に含む ID（CAN ID と一致） |
| DataLength | 5 | フレーム全体のバイト長（CRC16 2byte 含む） |
| MaxDeltaCounter | 0 | Protect 側では未使用 |
| Offset | 0 | E2E ヘッダ（CRC16+Counter の3byte）が始まる byte インデックス |

### ログ例

```
[1019ms] INFO  Can_Hw: TX OK id=0x220 dlc=5 [XX XX 00 00 00]
                                              └───┘  └┘ └┘ └┘ └── E2ESeqErrCount=0
                                              │      │  └──────── E2ECrcErrCount=0
                                              │      └─────────── Counter=0（初回送信）
                                              └────────────────── CRC16（自動計算、リトルエンディアン2byte）
[2019ms] INFO  Can_Hw: TX OK id=0x220 dlc=5 [YY YY 01 00 00]  ← Counter が 1 に進む
```

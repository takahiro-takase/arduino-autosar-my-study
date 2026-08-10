# AUTOSAR E2E Profile 1 仕様まとめ（学習ノート）

- **出典**: `AUTOSAR_SWS_E2ELibrary.pdf`（Specification of SW-C End-to-End Communication
  Protection Library, AUTOSAR CP Release 4.3.1, Document ID 428）
  7.3 節「Specification of E2E Profile 1」（p.41-52）および
  8.2.1 節「E2E Profile 1 types」（p.144-149）を要約したもの。
- 本ファイルは仕様書の**逐語訳ではなく**、当プロジェクトの実装（`src/Bsw/E2E/E2E_P01.*`,
  `src/Bsw/E2EXf/E2EXf.*` の E2E Transformer 統合部分）を理解するための技術的まとめです。
  仕様書原文は著作権保護対象のため `docs/*.pdf`（gitignore 対象）としてローカルにのみ保持し、
  本ファイルには転載していません。正確な文言が必要な場合は原典を参照してください。

---

## 1. Profile 1 が提供する4つのメカニズム（表 7-1 / SWS_E2E_00218）

| メカニズム | 内容 |
|---|---|
| Counter | 4bit（明示送信）、送信毎に +1。Alive Counter と Sequence Counter を同じ 4bit で兼用 |
| タイムアウト監視 | E2E ライブラリ自体はブロッキング受信をしない。受信側が非ブロッキングで読み取った際、Counter の評価によりタイムアウトを検出する |
| Data ID | 16bit、CRC 計算に含める（送信はしない＝暗黙的送信が基本） |
| CRC | CRC-8-SAE J1850（多項式 0x1D）。開始値・XOR 値は **0x00**（CRC ライブラリ標準の 0xFF とは異なる） |

検出できる通信故障（表 7-1）:
- **Counter** → 反復・消失・挿入・順序異常・停止
- **Data ID + CRC** → なりすまし・誤アドレッシング・挿入
- **CRC** → 破壊・非対称情報

## 2. Counter の挙動（SWS_E2E_00075 / SWS_E2E_00076）

- **送信側**: 初回 0、以後 +1。**14 (0xE) の次は 0 に戻る（0xF はスキップ）**
  — 0xF はエラー値として予約されているため。
- **受信側**: 前回 Counter との差分から次の 6 パターンを判定
  1. 前回呼び出し以降、新データなし
  2. 受信開始後まだ受信なし
  3. データが反復（同一 Counter）
  4. +1（正常、消失なし）
  5. +1 超だが許容範囲内（一部消失）
  6. 許容超過（過剰消失）

### ✅ 対応済み: deltaCounter の折り返し補正が mod-16（Profile 2 式）になっていた不具合を修正

コードレビューで、`E2E_P01.c` の `delta` 計算が `(received - lastValid) & 0x0FU`
というビットマスク（＝mod-16 の折り返し補正）になっていたことが判明した。
Profile 1 のカウンタは 0〜14 の 15 値循環（上記の通り 0xF はスキップ）のため、
E2ELibrary Figure 7-7 が定義する正しい折り返し補正は mod-15
（`received >= lastValid ? received - lastValid : 15 + received - lastValid`）であり、
mod-16（`+16` 補正）は Profile 2 側の Figure 7-12 の式である。

具体的な影響: `LastValidCounter=14, ReceivedCounter=0`（正常な折り返し）のとき、
正しい delta は `15 + 0 - 14 = 1` だが、旧実装は `(0-14) & 0x0F = 2` を返していた。
AbsInfo/EngineInfo とも `MaxDeltaCounter=1` のため、データ欠落が実際には
起きていないにもかかわらず、15 フレームに 1 回（カウンタが 14→0 に折り返す
たびに）`WRONGSEQUENCE` と誤判定し、不要な `SyncCounter` 再ロックと Dem
イベント報告が発生していた。

上記の Check 側修正の実機検証中、送信側（`E2E_P01Protect`）にも同種の
不具合が残っていることが実ログから判明した。カウンタ増分が
`(Counter + 1) & 0x0FU` という mod-16 の式のままで、SWS_E2E_00075
「14 (0xE) の次は 0 に戻る（15=0xF はスキップ）」を満たしておらず、
実際に AbsInfo の受信ログで Counter が `0x0E → 0x0F`（14→15）と遷移する
（本来は 14→0）様子が観測された。Check 側の delta 計算は 0〜15 のどんな値
でも数式上エラーにはならないため気づきにくいが、予約値 15 を実際に送信
してしまうこと自体が仕様違反であり、他の準拠実装との相互接続では
問題になり得る。`E2E_P01Protect()`（`src/Bsw/E2E/E2E_P01.c`）と
`tools/uds_tester/app.py` の両方のカウンタ増分を
`(Counter >= 14) ? 0 : Counter + 1` に修正した。

## 3. Data ID の4つの包含モード（SWS_E2E_00163）

| モード (`E2E_P01DataIDMode`) | 挙動 |
|---|---|
| `E2E_P01_DATAID_BOTH` (0) | 上位・下位バイト両方を CRC に含める（variant 1A） |
| `E2E_P01_DATAID_ALT` (1) | Counter の偶奇で上位/下位バイトを交互に CRC へ含める（variant 1B） |
| `E2E_P01_DATAID_LOW` (2) | 下位バイトのみ（ID が実質 8bit の場合） |
| `E2E_P01_DATAID_NIBBLE` (3) | ID を 12bit に制限。上位バイトの下位ニブルだけ**明示送信**し、残りは暗黙（variant 1C） |

## 4. CRC 計算の順序（SWS_E2E_00082 / SWS_E2E_00083 / SWS_E2E_00190）

1. まず Data ID（1〜2 バイト、モードに応じて）
2. 続けて送信データ全体（CRC バイト自身を除く）
3. `Crc_CalculateCRC8()` を使用。開始値/XOR 値は最終的に 0x00 相当になるよう補正
   （CRC ライブラリ自体は R4.0 以降 0xFF を使うため、E2E ライブラリ側で追加の XOR 0xFF 補正を行う）

### ✅ 対応済み: E2E_P01.c の CRC8 開始値/最終XOR値が 0xFF になっていた不具合を修正

実機用ではなくコードレビューで、`E2E_P01.c`（`E2E_P01Check`/`E2E_P01Protect`）が
本ノート自身が正しく記述している「開始値・XOR 値は 0x00」（上記 3.）に反し、
`crc = 0xFFU` を開始値として使い `crc ^= 0xFFU` を最終 XOR として適用していたことが
判明した。これは CRC-8-SAE-J1850（init=0xFF/xorout=0xFF、check=0x4B）であり、
SWS_E2E_00083 が要求する CRC-8-SAE-J1850/ZERO 相当（init=0x00/xorout=0x00、
check=0x37）とは異なる、CRC カタログ上明確に別物のバリアントである。

本実装の `E2E_CalcCrc8()` は AUTOSAR 実 CRC ライブラリのような「呼び出しごとに
自動で 0xFF を XOR する」内部補正を持たない素の実装のため、上記 3. の「補正」
（複数呼び出しの相殺トリック）を再現する必要はなく、単純に `crc = 0x00U` を
渡すだけで SWS_E2E_00083 の要求と一致する。Protect/Check は互いに自己整合していた
ため単体では正常動作していたが、外部の本物の AUTOSAR E2E Profile 1 実装や公式
テストベクタとは異なる CRC 値になり相互接続性が失われる状態だった。
`E2E_P01.c` に加え、同じ誤ったパラメータを使っていた `tools/uds_tester/app.py` の
`_crc8_sae_j1850()` も合わせて修正した。

## 5. 標準バリアント 1A/1B/1C（SWS_E2E_00227 / SWS_E2E_00228 / SWS_E2E_00307）

3つの標準バリアントはいずれも**固定レイアウト**:

- **CRC は常にシグナルグループの 0 バイト目**（bit offset 0）
- **Counter は常に 1 バイト目の下位 4bit**（bit offset 8）
- 1C のみ、1 バイト目の上位 4bit に Data ID ニブルを追加

`E2E_P01ConfigType` の `CounterOffset` / `CRCOffset` は「ビットオフセット」と説明されているが、
実体は「バイト位置×8」＋「下位ニブルなら +0、上位ニブルなら +4」という2値セレクタである
（Figure 7-5 のフローチャートで `Offset % 8 == 0` の分岐として実装されている）。

### ✅ 対応済み: variant 1A 準拠のレイアウトへ修正

当初、当プロジェクトの `MeterStatus`（byte[0]=EngineState, byte[1]=Counter 下位ニブル,
byte[2]=CRC）は CRC がバイト 2 にあり、標準バリアント 1A/1B/1C が要求する
「CRC は常にバイト 0」を満たしていなかった（Profile 1 のアルゴリズム自体は仕様に忠実だが、
バイトレイアウトは標準バリアントのどれにも一致しない独自配置だった）。

この点を実務で気づいた発見をきっかけに、`MeterStatus` / `AbsInfo` とも
**byte[0]=CRC8、byte[1]=Counter（下位4bit）、byte[2]以降=シグナル**という
variant 1A 準拠のレイアウトへ修正した。あわせて、CRC 計算アルゴリズム自体も
「CRC バイトより前の区間」と「CRC バイトより後の区間」の2区間に分けて計算する
Figure 7-6 準拠の実装（`E2E_CalcCrc8OverDataExcludingCrcByte()`）に一般化し、
CRC がフレーム末尾に固定されていなくても正しく動作するようにした
（修正前は「CRC は常に末尾バイト」という前提を暗黙に置いていた実装だった）。

なお、当プロジェクトの `CounterOffset` / `CRCOffset` は単純なバイトインデックスとして
扱われており（仕様書の「バイト位置×8＋ニブル選択」という2値スキームではなく、整数の
バイト番号そのもの）、ニブル選択やビット単位の柔軟な配置は実装していない、
より単純化した形になっている。この点は variant 1A の固定レイアウト
（Counter は常に1バイト目の下位ニブル）だけを対象にする限り、実用上の差異はない。

## 6. E2E_P01Protect / E2E_P01Check（7.3.7〜7.3.9、Figure 7-5〜7-7）

**E2E_P01Protect()** (SWS_E2E_00195):
1. Counter を書き込む
2. （NIBBLE モードのみ）Data ID ニブルを書き込む
3. Data ID + Data で CRC を計算する
4. CRC を書き込む
5. Counter を次回用に +1（mod 15）

**E2E_P01Check()** (SWS_E2E_00196):
1. CRC を照合する
2. （NIBBLE モードのみ）Data ID ニブルを照合する
3. Counter の差分を評価する
4. Status を確定する

### `E2E_P01CheckStatusType`（8値、SWS_E2E_00022）

| 値 | 名前 | 意味 |
|---|---|---|
| 0x00 | `E2E_P01STATUS_OK` | 正常受信、CRC 正、Counter が前回 +1 |
| 0x01 | `E2E_P01STATUS_NONEWDATA` | 前回呼び出し以降、新規受信なし |
| 0x02 | `E2E_P01STATUS_WRONGCRC` | CRC 不一致（または NIBBLE モードで ID 不一致） |
| 0x03 | `E2E_P01STATUS_SYNC` | 異常検知後の再同期中（CRC は正しいが継続性未確定） |
| 0x04 | `E2E_P01STATUS_INITIAL` | 受信初期化後、最初の正常データ（Counter 未検証） |
| 0x08 | `E2E_P01STATUS_REPEATED` | CRC 正だが Counter が前回と同一（反復） |
| 0x20 | `E2E_P01STATUS_OKSOMELOST` | CRC 正、Counter が許容範囲内で飛んでいる（一部消失） |
| 0x40 | `E2E_P01STATUS_WRONGSEQUENCE` | CRC 正だが Counter の飛びが許容超過（過剰消失） |

## 7. 設定構造体の意味（8.2.1.1〜8.2.1.5）

- **`E2E_P01ConfigType`**: `CounterOffset` / `CRCOffset` / `DataID` / `DataIDMode` /
  `DataIDNibbleOffset` / `DataLength`（bit 単位、8 の倍数、240 以下） /
  `MaxDeltaCounterInit` / `MaxNoNewOrRepeatedData` / `SyncCounterInit`
- **`E2E_P01ProtectStateType`**: `Counter` のみ（送信側の状態は単純）
- **`E2E_P01CheckStateType`**: `LastValidCounter` / `MaxDeltaCounter` / `WaitForFirstData` /
  `NewDataAvailable` / `LostData` / `Status` / `SyncCounter` / `NoNewOrRepeatedDataCounter`
  — 受信側は「異常検知後の再同期（SyncCounter によるロックイン）」まで持つ、
  比較的作り込まれた状態機械になっている。

### ✅ 対応済み: 8 状態フル state machine + SyncCounter 再ロック機構の実装

当初、当プロジェクトの `E2E_P01StatusType` は OK / REPEATED / WRONG_SEQUENCE / ERROR / INIT の
5 状態に簡略化されており、特に **SyncCounter による再ロック機構**（異常検知後、即座に「正常」へ
復帰せず、数回連続で正常なカウンタを受信して初めて信頼を回復する仕組み）が欠けていた。

この点を実装し、公式仕様の `E2E_P01CheckStatusType`（SWS_E2E_00022）に定義された
8 値（値も公式のビットパターンと一致させた）のうち、`NONEWDATA` を除く 7 状態
（OK / WRONGCRC / SYNC / INITIAL / REPEATED / OKSOMELOST / WRONGSEQUENCE）を実装した。

- `WRONGSEQUENCE`（カウンタ飛びが許容超過）を検知すると `SyncCounter = SyncCounterInit`
  をセットし、以降 `SyncCounterInit` 回分の連続正常受信の間は `SYNC` を返し続ける
  （個々のフレームの CRC・カウンタ自体は正常範囲内でも、シーケンスの継続性が
  まだ確定していないことを示す）
- `1 < delta <= MaxDeltaCounter` の正常進行は `OKSOMELOST`、`delta == 1` は `OK` として区別
- 旧 `ERROR`（CRC 不一致）は公式名 `WRONGCRC` に、旧 `INIT` は公式名 `INITIAL` に改名
- `ERROR`（0x80）は NULL ポインタ・DLC 不足など入力パラメータ異常専用として本実装独自に残した
  （公式仕様では戻り値型が別になっている部分の簡略化）

`NONEWDATA`（前回呼び出し以降、新規データなし）のみ未実装。理由は、本実装が
`Com_RxIndication()` からフレーム受信時にのみ `E2E_P01Check` を呼び出す設計のため、
「Check は呼ばれたが新規データがない」という状況自体が発生しないため。同様に、公式仕様の
`MaxDeltaCounter` は呼び出しごとに動的に増加するフィールドだが、これも「呼ばれたが新規データが
ない」ケースへの対応が主目的のため、本実装では `Config` 側の固定値のまま据え置いている。

---

## 関連資料

- [`docs/REFERENCES.md`](./REFERENCES.md) — 本プロジェクトが参照する AUTOSAR 仕様書の入手先一覧
- `docs/AUTOSAR_SWS_E2ELibrary.pdf` — 本ノートの一次資料（ローカルのみ、gitignore 対象）
- `docs/AUTOSAR_SRS_E2E.pdf` — E2E の上位要求仕様（脅威モデル）。本ノートでは未要約

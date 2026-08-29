# NvM（Non-Volatile Memory Manager）

> [README](../../README.md) の「[診断スタック](../../README.md#diag-stack)」節から分離。

NvM は Dem が使う EEPROM ブロックを抽象化するモジュールです。
Dem は EEPROM アドレスを一切知らず、`NvM_BlockIdType`（NVM_BLOCK_ID_*）でのみ
ブロックを指定して `NvM_ReadBlock()` / `NvM_WriteBlock()` を呼び出します。
CRC による破損検出・デフォルト値復元・非同期書き込みジョブキュー
（NvM↔MemIf↔Fee 責務分担）・冗長ブロックの詳細を以下にまとめます。

## CRC によるデータ破損検出

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

## NvM_RestoreBlockDefaults — デフォルト値への復元

CRC 不一致を検出すると、ブロックごとに設定された **ROM デフォルト値**
（`NvM_PBCfg.c` の `NvM_BlockDescriptorType.RomBlockDataAddress`、未設定なら
全 0）を RAM ミラーへコピーし、CRC を付け直して EEPROM へ書き戻します。
この処理は `NvM_Init()` が破損検出時に内部的に呼ぶほか、
`NvM_RestoreBlockDefaults(BlockId, NvM_DestPtr)` として明示的にも呼び出せます
（SWS_NvM_00451 準拠の2引数形式。`NvM_DestPtr` は NULL 可で、非 NULL の場合
復元したデフォルト値がそこへも追加でコピーされます）。

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

## ログ例（EEPROM を直接書き換えて DEM_AGING ブロックを破損させた場合）

```
[19ms] ERROR NvM: block=2 CRC mismatch (stored=0xA3 calc=0x7F)
[19ms] WARN  NvM: block=2 defaults restored (zero-fill)
[19ms] INFO  NvM: Init ok blocks=4
```

## 動作確認方法（CRC 不一致からの自動復元）

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

## 非同期書き込みジョブキュー（NvM ↔ MemIf ↔ Fee の責務分担）

**なぜ非同期化したか**: Renesas RA の EEPROM ライブラリ（内蔵フラッシュの
エミュレーション）は 1 バイトの書き込みでも消去・書き込みサイクルを伴うため、
9 バイト超のブロック（DEM_STATUS 等）をまとめて同期的に書くと数百 ms
協調スケジューラ全体が停止します。この停止を放置すると `Dem_ReportErrorStatus()`
が新規 DTC 確定のたびに WdgM の Deadline Supervision を巻き込んで HW
ウォッチドッグリセットを引き起こしうるため（経緯は
後述の「[開発の経緯](#非同期書き込みジョブキューへの変更経緯)」、および
[`WdgM_Notes.md`](./WdgM_Notes.md#deadline-supervision-上限緩和と-os_schedulerstep-のバグ)
参照）、ブロッキングそのものを解消する非同期ジョブ方式を採用しています。

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
再判定できる設計、[Dem.c](../../src/Bsw/Dem/Dem.c) 参照）。`NvM_MainFunction()`
がブロック ID 昇順で保留ブロックを拾ってしまうと、`NVM_BLOCK_ID_DEM_MAGIC=0`
が最小 ID のため、最後に投入したはずの MAGIC が真っ先に物理書き込みされ、
この整合性設計が壊れてしまう。そのため保留ブロックは ID 順ではなく、
投入順を記録した FIFO キューから取り出す。

**完了確認 API**: `NvM_GetErrorStatus(BlockId, &result)`（SWS_NvM_00451
準拠の OUT パラメータ形式。戻り値 `Std_ReturnType` は成否のみを示し、
ジョブ結果本体は `result` に `NVM_REQ_OK`/`NVM_REQ_PENDING`/
`NVM_REQ_NOT_OK` として書き込まれる）で確認できます。現状の呼び出し元は
いずれも結果を確認しない fire-and-forget ですが、API としては提供しています。

## 冗長ブロック（Redundant Block）

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

## AUTOSAR 実装との主な違い (学習用簡略化)

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

## 開発の経緯（実機で見つかった不具合・設計変更）

> 現在の仕様を理解するだけなら読む必要はありません。実機検証で見つかった
> 不具合や、その結果としての設計変更の経緯を時系列でまとめています。

### 非同期書き込みジョブキューへの変更経緯

当初 `NvM_WriteBlock()` は EEPROM への書き込みも含めて完全に同期処理だった。
Renesas RA の EEPROM ライブラリ（内蔵フラッシュのエミュレーション）は
1 バイトの書き込みでも消去・書き込みサイクルを伴うため、9 バイト超のブロック
（DEM_STATUS 等）をまとめて同期的に書くと数百 ms 協調スケジューラ全体が
停止することが実機で判明した。この停止は `Dem_ReportErrorStatus()` が
新規 DTC 確定のたびに発生し、WdgM の Deadline Supervision を巻き込んで
実際に HW ウォッチドッグリセットを引き起こしていた（詳細は
[`WdgM_Notes.md`](./WdgM_Notes.md#deadline-supervision-上限緩和と-os_schedulerstep-のバグ)
参照）。

閾値を広げる対症療法ではなく、ブロッキングそのものを解消するため、実際の
AUTOSAR NvM と同じ非同期ジョブキュー方式（1 回の呼び出しで 1 バイトだけ書く、
10ms 周期の `NvM_MainFunction()`）へ変更した。同じ考え方は CAN の TX 確認
（`Can_MainFunction_Write`）の非同期化にも踏襲されている。

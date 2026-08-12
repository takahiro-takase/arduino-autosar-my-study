# AUTOSAR E2E Profile 5 仕様まとめ（学習ノート）

- **出典**: `AUTOSAR_SWS_E2ELibrary.pdf`（Specification of SW-C End-to-End Communication
  Protection Library, AUTOSAR CP Release 4.3.1, Document ID 428）
  7.6 節「Specification of E2E Profile 5」（p.76-78）および
  8.2.4 節「E2E Profile 5 types」（p.155-157）、8.3.4 節「E2E Profile 5 routines」
  （p.186 以降、`E2E_P05Protect()`/`E2E_P05Check()` の擬似コード）を要約したもの。
- 本ファイルは仕様書の**逐語訳ではなく**、当プロジェクトの実装（`src/Bsw/E2E/E2E_P05.*`,
  `src/Bsw/E2EXf/E2EXf.*` の Profile05 統合部分）を理解するための技術的まとめです。
  [`E2E_Profile1_Notes.md`](./E2E_Profile1_Notes.md) と対の位置づけで、Profile01 との
  差分を中心にまとめています。仕様書原文は著作権保護対象のため `docs/*.pdf`
  （gitignore 対象）としてローカルにのみ保持し、本ファイルには転載していません。

---

## なぜ Profile01 から Profile05 へ切り替えたか

`EngineHealthStatus`（CAN 0x220）は当初 E2E Profile01 + SecOC の二重保護
（内側=E2E で意図しない誤りを検出、外側=SecOC で意図的な改ざん・なりすましを
検出）でした。E2E の検出能力を高める（CRC8→CRC16）ために Profile05 へ切り替えた
ところ、Profile05 のヘッダは CRC16(2byte)+Counter(1byte)=3byte で Profile01 の
実質2byte より1byte 増え、SecOC の Freshness(1byte)+MAC(3byte) を足すと
classic CAN の DLC=8 上限（4+1+3=8byte だったところへ、E2E だけで5byte になり
5+1+3=9byte で超過）を超えてしまうため、SecOC 側を撤去し E2E Profile05 単体
保護に一本化した。詳細な経緯・代替案（SecOC を ImmobilizerStatus へ移す案）の
検討は README.md の「SecOC」セクション参照。

## 1. Profile 5 が提供する 3 つのメカニズム（表 7-4 / SWS_E2E_00394）

| メカニズム | 内容 |
|---|---|
| Counter | 8bit（明示送信）、送信毎に +1。**予約値なし**（0xFF も通常のカウンタ値として使う） |
| CRC | CRC16（多項式 0x1021、Autosar notation。`Crc_CalculateCRC16()` 相当） |
| Data ID | 16bit、CRC 計算にのみ使用（暗黙的送信、PDU 自体には含まれない） |

Profile01 との最大の違いは **Counter が 4bit(0-14循環、15予約) → 8bit(0-255循環、
予約値なし)** になり、mod-15 の折り返し補正が不要になった点（単純な `uint8` の
自然なラップアラウンドがそのまま mod-256 補正になる）。

## 2. ヘッダレイアウト（Figure 7-14、SWS_E2E_00397/00405）

```
byte[Offset+0..Offset+1] : CRC16（リトルエンディアン、最下位バイトが先）
byte[Offset+2]           : Counter（8bit フル値）
```

Profile01 と違い CRCOffset/CounterOffset が別々に設定可能な柔軟なレイアウトでは
なく、**CRC16+Counter の3byteがひとまとまりの固定ヘッダとして、PDU内の任意の
バイトオフセット (`Offset`) に配置される**（`Offset` は公式仕様ではビット単位・
8の倍数だが、`E2E_P01ConfigType.CounterOffset`/`CRCOffset` と同じ簡略化の考え方で
本実装ではバイト単位の `E2E_P05ConfigType.Offset` にしている）。標準的な配置は
`Offset=0`（ヘッダが PDU 先頭）。

## 3. Data ID の投入位置（SWS_E2E_00399/00406）— Profile01との重要な違い

> [SWS_E2E_00399] In the E2E Profile 5, the Data ID shall be implicitly transmitted,
> by adding the Data ID **after the user data** in the CRC calculation.

Profile01 は DataID を**データより先に**投入するが（`E2E_Profile1_Notes.md` 4章参照）、
Profile05 は**データの後に**投入する。順序自体（下位バイト→上位バイト）は
Profile01 と同じ。

## 4. CRC 計算の擬似コード（SWS_E2E_00406、一次資料で確認済み）

CRC ライブラリの開始値・XOR 値について、7.6.5 節本文は「CRC Library を参照」
としか書いておらず（本プロジェクトには CRC Library 仕様書 PDF が無いため直接
確認できない）、Profile01 のときのように仕様書本文だけからは開始値を断定
できなかった。しかし SWS_E2E_00406 の **擬似コード（Figure、E2E_P05Protect()/
E2E_P05Check() 共通）に `Crc_StartValue16: 0xFFFF` と明記**されていたため、
これを一次資料として採用した:

```
ComputedCRC = Crc_CalculateCRC16(&Data[0], Length=Offset,
                                  StartValue16=0xFFFF, IsFirstCall=TRUE)          [Offset > 0 の場合のみ]
ComputedCRC = Crc_CalculateCRC16(&Data[Offset+2], Length=DataLength-Offset-2,
                                  StartValue16=0xFFFF または上記の続き, IsFirstCall=...)
ComputedCRC = Crc_CalculateCRC16(DataID & 0xFF,        Length=1, StartValue16=ComputedCRC, IsFirstCall=FALSE)
ComputedCRC = Crc_CalculateCRC16((DataID>>8) & 0xFF,   Length=1, StartValue16=ComputedCRC, IsFirstCall=FALSE)
```

（`Offset > 0` の分岐は Profile01 の「CRC より前の区間」に相当するが、本
プロジェクトの標準バリアントは `Offset=0` 固定のためこの区間は常に0byte。
`E2E_P05.c` の `E2E_CalcCrc16Body()` は Offset=0 前提で単純化して実装している）

CRC の書き込みは `SWS_E2E_00407` により Data[Offset]/Data[Offset+1] へ
**リトルエンディアン**（下位バイトが先）で行う。

`E2E_P01.c` が「実 AUTOSAR の `Crc_CalculateCRC8()` は内部で 0xFF 相殺トリックを
使うが、本実装は素の実装なので単純に開始値をそのまま渡せばよい」と判断したのと
同じ考え方で、`E2E_CalcCrc16()`（`E2E_P05.c`）も自動補正の無い素の実装のため、
そのまま `crc=0xFFFFU` を渡すだけで SWS_E2E_00406 と一致する。

## 5. Counter の挙動（SWS_E2E_00397/00409、SWS_E2E_00416）

- **送信側**: 初回 0、以後 `Counter++`（uint8、0xFF の次は自然に 0 へラップ。
  Profile01 のような「特定の値をスキップする」処理は不要）。
- **受信側**（SWS_E2E_00416 の "Do Checks" 擬似コード）:
  ```
  deltaCounter = ReceivedCounter - State->Counter   (uint8、0xFF ラップアラウンド込み)
  deltaCounter > MaxDeltaCounter ?  → WRONGSEQUENCE
  deltaCounter == 0 ?               → REPEATED
  deltaCounter == 1 ?               → OK
  それ以外 (1 < delta <= Max)       → OKSOMELOST
  ```
  CRC 不一致時は `State->Counter` を更新しない（次に CRC が正しいフレームが
  来た時点で、その前の正常な状態を基準に判定できるようにするため。
  `E2E_P01Check()` と同じ方針）。

### Profile01 との構造的な違い: INITIAL/SYNC 状態が無い

Profile01 の `E2E_P01CheckStateType` は `WaitForFirstData`（初回受信の特別扱い）・
`SyncCounter`（`WRONGSEQUENCE` 検知後の再ロック機構）を持つが、公式の
`E2E_P05ConfigType`/`E2E_P05CheckStateType` にはこれらに相当するフィールドが
存在しない（`SyncCounterInit` フィールド自体が無い）。そのため:

- 初回の `E2E_P05Check()` 呼び出しも、他の呼び出しと全く同じ delta 計算に
  そのまま乗る。`ProtectState`/`CheckState` とも初期値は `Counter=0` なので、
  初回フレーム（Counter=0）を初回 Check（State->Counter も 0）すると
  `delta=0` となり **`REPEATED` と判定される**（`OK` にはならない）。
  これは Profile05 の仕様上正しい挙動であり、バグではない
  （`test/test_native/Bsw_E2E_test.cpp` の
  `FirstCheckAfterInitIsRepeatedBecauseBothStartAtCounterZero` で確認済み）。
- `WRONGSEQUENCE` 検知後も、Profile01 のような「SyncCounterInit 回分は
  `SYNC` を返し続ける再ロック期間」は無く、次のフレームで `delta` が
  1 に戻れば即座に `OK` に復帰する。

## 6. ステータス値（`E2E_P05CheckStatusType`、8.2.4.4節、Figure 8-8）

| 値 | 名前 | 意味 |
|---|---|---|
| 0x00 | `E2E_P05STATUS_OK` | 正常受信、CRC 正、Counter が前回 +1 |
| 0x01 | `E2E_P05STATUS_NONEWDATA` | 新規データなし（本実装の呼び出し方式では未使用） |
| 0x07 | `E2E_P05STATUS_ERROR` | CRC 不一致、または NULL ポインタ・DLC 不一致等の入力異常 |
| 0x08 | `E2E_P05STATUS_REPEATED` | CRC 正だが Counter が前回と同一（反復） |
| 0x20 | `E2E_P05STATUS_OKSOMELOST` | CRC 正、Counter が許容範囲内で飛んでいる（一部消失） |
| 0x40 | `E2E_P05STATUS_WRONGSEQUENCE` | CRC 正だが Counter の飛びが許容超過（過剰消失） |

Profile01 (`E2E_P01StatusType`) とはビットパターンが異なる点に注意
（特に `ERROR` は Profile01 が独自拡張の `0x80`、Profile05 は公式値の `0x07`）。

## 7. 設定構造体の意味（8.2.4.1〜8.2.4.3）

- **`E2E_P05ConfigType`**: `DataID`(uint16) / `DataLength`(uint16、公式はビット単位・
  8の倍数・3\*8以上4096\*8以下だが、本実装は Profile01 と同じ簡略化でバイト単位) /
  `MaxDeltaCounter`(uint8) / `Offset`(uint16、公式はビット単位だが本実装はバイト単位)
- **`E2E_P05ProtectStateType`**: `Counter` のみ（Profile01 と同じ、送信側の状態は単純）
- **`E2E_P05CheckStateType`**: `Counter` / `Status` のみ。Profile01 の
  `LastValidCounter`/`WaitForFirstData`/`SyncCounter` 等に比べてシンプル
  （上記5章の「INITIAL/SYNC が無い」構造的な違いに対応）

## 8. 本実装での設計判断メモ

- **Check側は当初(EngineHealthStatus TX単体の時期)は本プロジェクト内に呼び出し元が
  無かった**。対称性のため Protect と同じ構成で先に実装し、正しさは
  `test/test_native/Bsw_E2E_test.cpp`（GoogleTest、`env:native`）の
  ホストテストのみで検証していた。その後 2026-08 に EngineInfo(RX,0x100)/
  AbsInfo(RX,0x110) を Profile01 から Profile05 へ移行し、`E2EXf_InverseTransformP05()`
  （`src/Bsw/E2EXf/E2EXf.c`）経由で `E2E_P05Check()` の実際の呼び出し元になった
  （`src/Rte/Rte.c` の `Rte_COMCbk_EngineInfo()`/`Rte_COMCbk_AbsInfo()`）。
  次に検証結果を確認する場合は、送信側（他 ECU 役の uds_tester/CAPL スクリプト）が
  `E2E_P05Protect` と同じ CRC16/カウンタ計算で組み立てたフレームを、実機の
  `E2E_P05Check` が正しく判定するかを実機ログで確認すること。
- **起動直後の初回受信を E2EXf 層で救済（コードレビューで発見・修正済み）**:
  `E2E_P05Check()` は本章 5 節の通り Profile01 の `WaitForFirstData`/`INITIAL`
  相当の初回受信の特別扱いを持たない（仕様通り、意図的）。EngineInfo/AbsInfo
  の送信元 ECU は本 ECU より先に起動して Counter が 0 以外から始まっている
  ことが普通にあり得るため、そのまま繋ぐと起動直後の最初の（CRC は正しい）
  フレームが `REPEATED`/`WRONGSEQUENCE` と誤判定され、
  `DEM_DEBOUNCE_LIMIT_E2E_ENGINEINFO`/`_ABSINFO`（`Dem_Cfg.h`、いずれも 1 = 即確定）
  と相まって毎回の電源投入直後に誤った DTC が確定してしまう。`E2E_P05.c`
  自体は仕様に忠実なまま変更せず（`test/test_native/Bsw_E2E_test.cpp` の
  `FirstCheckAfterInitIsRepeatedBecauseBothStartAtCounterZero` が反する変更を
  検出する）、統合層である `E2EXf_RxConfigTypeP05.WaitForFirstData` フラグと
  `E2EXf_InverseTransformP05()` 側の格上げ処理で対処した。CRC が正しい最初の
  1 フレームに限り判定結果を `OK` に格上げして Dem/E2EMon/Rte ステータスの
  いずれにも誤検出が伝播しないようにし、2 フレーム目以降は
  `E2E_P05Check()` が内部で `State->Counter` を受信値へ同期済みのため通常の
  delta 判定にそのまま戻る。
- **E2EXf層の統合方式**: `E2EXf_TxConfigType`（Profile01専用）は削除せず、
  `E2EXf_TxConfigTypeP05`/`E2EXf_TransformP05()` を並行して追加する形にした。
  実 AUTOSAR の E2E Transformer は ARXML 設定から RTE 生成コードが
  「トランスフォーマーインスタンスごとに専用コード」を生成する方式であり、
  プロファイルをまたいだ汎用的な切り替え機構を持たないため、この方式に倣った
  （`E2EXf_Initialized` フラグ・`EEXF_API_ID_TRANSFORM` は Profile01/05 で共用）。
- **CRC16計算の一次資料**: `Crc_StartValue16=0xFFFF`・DataID投入順（下位→上位、
  データの後）はいずれも SWS_E2E_00406 の擬似コードで確認済み（推測ではない）。

---

## 関連資料

- [`docs/E2E_Profile1_Notes.md`](./E2E_Profile1_Notes.md) — Profile01 の学習ノート（対の関係）
- [`docs/REFERENCES.md`](../autosar/REFERENCES.md) — 本プロジェクトが参照する AUTOSAR 仕様書の入手先一覧
- `docs/AUTOSAR_SWS_E2ELibrary.pdf` — 本ノートの一次資料（ローカルのみ、gitignore 対象）
- `test/test_native/Bsw_E2E_test.cpp` — CRC16・Protect/Check のホストテスト

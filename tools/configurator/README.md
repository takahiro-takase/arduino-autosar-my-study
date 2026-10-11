# コンフィグレータ（試作）

信号表（CAN ビットアサイン表）と、モジュールごとの設定 json から、各モジュールの設定ソース
（`*_PBCfg.c`、`*_Cfg.h`）の該当部分を生成する Python ツールです。
現在の対象は **CanIf**、**Com**、**PduR**、**E2EXf**（E2E Transformer）、**SecOC**、**Csm / Crypto / KeyM**（暗号処理の連鎖）、**BswM**（モード遷移のルール）の 8 モジュールです。Com スタックの他のモジュールへ広げていく計画です。

最終的な構想は `arxml ⇔ json ⇔ Cfg.c` です。このうち json → Cfg.c の部分と、
信号表の変更に設定を連動させる部分を、この範囲で試しています。

## 全体の流れ

```
config/data/can_signals.json（ビットアサイン表の正本。can_tool の GUI で編集）
        │  sync_cfg.py: 追加されたフレームの設定のひな形を、各モジュールの設定 json へ追加
        ▼
config/data/CanIf.json, Com.json, PduR.json, E2EXf.json, SecOC.json, CryptoStack.json, BswM.json
        │  gen_cfg.py: スキーマ・整合性を検査して、Cfg ソースの生成領域を更新（json → C の一方向）
        ▼
src/Bsw/CanIf/CanIf_PBCfg.c, CanIf_Cfg.h / src/Bsw/Com/Com_PBCfg.c, Com_Cfg.h / src/Bsw/PduR/PduR_PBCfg.c, PduR_Cfg.h
/ src/Bsw/E2EXf/E2EXf_PBCfg.c, E2EXf_PBCfg.h / src/Bsw/SecOC/SecOC_PBCfg.c, SecOC_Cfg.h
/ src/Bsw/Csm, Crypto, KeyM の *_PBCfg.c, *_Cfg.h（鍵・ジョブ・鍵名のテーブルと ID マクロ）
/ src/Bsw/BswM/BswM_PBCfg.c, BswM_Cfg.h（ルールテーブルとルール数）
```

- **信号表の値は設定 json にコピーしません。** CAN ID、DLC、信号のビット位置・ビット長、update-bit の位置、
  送信周期・受信タイムアウトは、設定 json の `frame`（フレーム名）と `field`（フィールド名）から、
  生成時に信号表を引いて埋めます。値の正本は信号表の 1 か所だけです。
- 信号表に無いフレーム（UDS 診断など）だけ、設定 json に `canId` と `dlc` を直接書きます。
- 設定 json に書くのは、信号表から決まらない値です（PDU ID の割り当て、経路の宛先モジュール、HTH/HRH、
  Com の送信モード・フィルタ・コールバック名など）。
- **PduR は Com の I-PDU を名前で参照します**（`ipdu` / `confIpdu`）。PduR の宛先 ID の数値は、生成時に Com の設定から引きます。
  Com の `pduRId` も、PduR の経路から引きます（SecOC 経由の I-PDU は、SecOC の設定から引きます）。
- **E2E / SecOC のレイアウトも信号表から引きます。** E2EXf の DataLength・Offset・DataID（既定は CAN ID）、
  SecOC の Freshness / MAC の位置と長さ・Authentic Payload 長・Secured I-PDU 長・DataId（既定は CAN ID）は、
  `e2e_crc` / `e2e_counter` / `secoc_freshness` / `secoc_mac` のフィールドから求めます。
  SecOC 経由の Com の I-PDU の `dlc` と `pduRId` も、SecOC の設定から引きます。

## 使い方

信号表にフレームを追加したとき:

```
python tools/configurator/sync_cfg.py             # 不足している設定のひな形を、CanIf.json / Com.json / PduR.json へ追加
python tools/configurator/gen_cfg.py              # Cfg ソースを生成
```

設定を直接変えるとき: `config/data/*.json` を編集して `gen_cfg.py` を実行します。

```
python tools/configurator/gen_cfg.py              # 生成して書き換える
python tools/configurator/gen_cfg.py --check      # 書き換えず、生成結果と食い違えば終了コード 1
python tools/configurator/gen_cfg.py PduR         # モジュールを指定
python tools/configurator/sync_cfg.py --dry-run   # 追加内容を表示するだけ
```

C ソースの生成領域は手で編集しません（次の生成で上書きされます）。

信号表の編集画面（can_tool の信号定義エディタ）にも、「検査」「同期の確認」「同期」「生成」のボタンがあります（[can_tool の README](../can_tool/README.md) 参照）。
設定 json は、can_tool の左側のナビゲーションの `Com`、`PduR` などの項目から、スキーマに従って GUI で編集できます（同じく [can_tool の README](../can_tool/README.md) 参照）。

### 同期ツールの動き
- 追加するのは、信号表にあって設定に無いフレームの「ひな形」です。CanIf の PDU、Com の I-PDU とシグナル
  （E2E / SecOC 用と update-bit を除く各フィールド）、PduR の経路を、既定値で作ります（経路は CanIf → PduR → Com が既定）。
- 削除や名前変更は自動では扱いません。信号表に無いフレームを参照している設定は、警告として報告します。
- E2E / SecOC 用のフィールドを持つフレームは、E2EXf / SecOC の設定を自動では作りません（Dem イベント、Csm のジョブ、
  `E2EXf.c` や Rte の呼び出し関数が必要なため）。手で `E2EXf.json` / `SecOC.json` に足すまで、`gen_cfg.py` は整合性エラーで止まります。
- 人が決める値（Com の送信モード、タイムアウト、I-PDU グループ、コールバックなど）は、既定値を入れた上で「要確認」として表示します。
  追加後に `Com.json` を直してから、`gen_cfg.py` を実行します。

## 仕組み

C ソースが「テンプレート」を兼ねます。生成する範囲を、目印のコメントで挟んであります。

```c
/* @@GEN-BEGIN pdur-routing  生成元: config/data/PduR.json  （自動生成: 手編集禁止） */
...  ここが json から生成される ...
/* @@GEN-END pdur-routing */
```

- 目印の外（ファイル冒頭の説明、インクルード、帯見出しなど）には触れません。
- 領域の ID と生成関数の対応は、`modules/` の各モジュール（`SPEC`）と、`gen_cfg.py` の `MODULES` に書いてあります。
- 一方向（json → C）です。C → json の逆変換は行いません。json が正本です。

## ファイル構成

| パス | 内容 |
|------|------|
| `gen_cfg.py` | 目印コメントの差し込み、スキーマ検証、生成の実行 |
| `sync_cfg.py` | 信号表に追加されたフレームの設定を、設定 json へ追加 |
| `checks.py` | 信号表と各モジュール設定の整合性検査 |
| `modules/canif.py`, `com.py`, `pdur.py`, `e2exf.py`, `secoc.py`, `cryptostack.py`, `bswm.py` | モジュールごとの生成関数（`SPEC` に生成先と領域 ID を持つ） |
| `modules/common.py` | コメント・構造体フィールドの整形 |
| `../../config/schema/*.schema.json` | 設定 json のスキーマ（型・範囲・列挙） |
| `../../config/data/*.json` | モジュールごとの設定データ（リポジトリ直下の `config/`。コードと分けて置く） |

## 検証

生成時に、次を検査します。

1. スキーマ検証（必須項目、型、PduId の範囲、モジュール名の列挙など）
2. モジュール内の整合性（`name` と `srcPduId` の重複、`frame` と `canId` の指定の排他など）
3. **モジュールをまたぐ整合性**（`checks.py`）
   - 設定が参照するフレームが、信号表に実在し、向き（TX/RX）が合っている
   - 信号表のフレームが、CanIf に漏れなく設定されている
   - CAN ID が、TX 同士・RX 同士で重複していない
   - PduR の TX 経路の `srcPduId` と、CanIf の `upperLayerTxPduId` が一致している（RX も同様）
   - PduR の経路と CanIf の PDU が、同じフレームを指している
   - 信号表のフレームが、Com に漏れなく設定されている（CanNm が直接扱うフレームは `excludedFrames` で除外）
   - 信号表のフィールドが、Com のシグナルとして漏れなく設定されている（意図して使わないものは I-PDU の `unusedFields`）
   - Com のシグナルのフィールドが信号表に実在し、ビット範囲が DLC 内に収まり、互いに重ならない
   - update-bit の位置が、信号表の `(update-bit)` フィールドと一致している
   - PduR が参照する Com の I-PDU が実在し、Com の I-PDU に PduR の経路（または SecOC の経路）がある
   - E2E 用フィールド（`e2e_crc` + `e2e_counter`）を持つフレームに E2EXf の設定がある。Profile05 のレイアウト（CRC16 の直後に Counter8）である、DataID が重複しない、Com の I-PDU に E2E の呼び出し元（RX は `rxIndicationCbk`、TX は `txTransformCbk`）がある
   - SecOC 用フィールドを持つフレームに SecOC の設定がある。Com の I-PDU・PduR の経路・CanIf の RX PDU が、同じフレームでつながっている
   - SecOC の `csmJobId` が、Csm のジョブとして存在し、MACVERIFY である。ジョブが使う鍵が存在する（未使用の鍵・ジョブは警告）
   - BswM のルールが指す I-PDU グループが Com に存在する。I-PDU が属するグループに、**起動する BswM のルールがある**（無いとグループは既定で停止状態のまま動かない。停止ルールが無い場合、属する I-PDU が無いグループは警告）
4. 使うモジュールのヘッダが、C ソースにインクルードされているか（警告）

導入時は、生成の前後でコンパイル結果（オブジェクトファイル）が完全に一致することを確認しています。
生成によって変わるのはコメントの体裁（桁揃え、DaVinci 対応コメントの付与）だけで、構造体の値は変わりません。
追加のフレームを加えた場合の生成結果も、コンパイルが通ることを確認しています。

## json の項目と ECUC パラメータの対応

| json | C の構造体 | DaVinci（ECUC） |
|------|------------|-----------------|
| CanIf `txPdus[].upperLayerTxPduId` | `UpperLayerTxPduId` | `CanIfTxPduId` |
| CanIf `txPdus[].frame` → CAN ID | `CanId` | `CanIfTxPduCanId` |
| CanIf `txPdus[].frame` → DLC | `Dlc` | `CanIfTxPduDlc` |
| CanIf `txPdus[].hth` | `Hth` | `CanIfTxPduHthIdRef` |
| CanIf `txPdus[].txConfirm` | `TxConfirmFct` | `CanIfTxPduUserTxConfirmationName` |
| CanIf `rxPdus[].upperLayerRxPduId` | `UpperLayerRxPduId` | `CanIfRxPduUpperLayerPduId` |
| CanIf `rxPdus[].hrh` | `Hrh` | `CanIfRxPduHrhIdRef` |
| CanIf `rxPdus[].rxIndication` | `RxIndicationFct` | `CanIfRxPduUserRxIndicationName` |
| PduR `rxPaths[].srcPduId` | `SrcPduId` | `PduRSrcPdu/PduRSrcPduHandleId` |
| PduR `rxPaths[].dests[].module` | `Module` | `PduRDestPdu/PduRDestModule` |
| PduR `rxPaths[].dests[].ipdu`（Com 宛て）→ Com の RX I-PDU の添字 | `DestPduId` | `PduRDestPdu/PduRDestPduHandleId` |
| PduR `txPaths[].srcPduId` | `SrcPduId` | `PduRSrcPdu/PduRSrcPduHandleId` |
| PduR `txPaths[].canIfTxPduId` | `CanIfTxPduId` | `PduRDestPdu/PduRDestPduHandleId` |
| PduR `txPaths[].confModule` | `ConfFct` | `PduRTxConfirmation` |
| Com `rxIpdus[]` / `txIpdus[]` の並び順 | `IPduId` | `ComIPduHandleId` |
| Com `rxIpdus[].frame` → DLC（または `dlc`） | `DLC` | `ComIPduLength` |
| Com `*Ipdus[].txModeMode` ほか | `TxModeMode` ほか | `ComTxModeMode` ほか |
| Com `signals[]` の並び順 / `macro` | `SignalId` | `ComHandleId` |
| Com `signals[].ipdu` | `IPduId` | `ComIPduRef` |
| Com `signals[].field` → ビット位置・ビット長 | `BitPosition` / `BitSize` | `ComBitPosition` / `ComBitSize` |
| E2EXf `instances[]`（`frame` → DataLength / Offset / DataID） | `E2E_P05ConfigType` | `E2EXf` の E2EProfile 設定（`E2EXf_*_E2EXf`） |
| E2EXf `smConfig` | `E2E_SMConfigType` | E2E ステートマシンの設定（`ECUC_E2E_*`） |
| SecOC `rxPdus[]`（`frame` → レイアウト） | `SecOC_RxPduConfigType` | `SecOCRxSecuredPduLayer` ほか |
| CryptoStack `keys[]` / `jobs[]` / `keys[].keyM` | `Crypto_KeyTable` / `Csm_JobConfigType` / `KeyM_CryptoKeyConfigType` | `CryptoKeyType` / `CsmJob` / `KeyMCryptoKey` |
| BswM `rules[]` | `BswM_RuleType` | `BswModeRequestPort` / `BswMRule` / `BswMAction` |
| Com `ipduGroups[]` の並び順 | `COM_IPDU_GROUP_*` | `ComIPduGroup`（`ComIPduGroupHandleId`） |
| `name` | （なし） | `PduRRoutingPath` / `CanIfTxPduCfg` / `CanIfRxPduCfg` / `ComIPdu` / `ComSignal` の ShortName |

## 方式の検討（なぜ「目印差し込み」にしたか）

生成方式を比較したときの記録です（2026-10）。

| # | 方式 | 概要 | 長所 | 短所 |
|---|------|------|------|------|
| 1 | **目印差し込み（採用）** | 実ソースが兼テンプレート。目印の間だけ生成 | テンプレートが別にない。生成前でもビルドできる。手書きの説明や帯見出しを残せる | 領域内の手編集は消える。目印の外に設定の重複が残りうる |
| 2 | 別ファイルのテンプレート（Jinja2 など） | `.c.j2` から全体を生成し、`.c` は純粋な生成物 | ソース全体を一括管理できる。条件分岐や繰り返しが書きやすい | テンプレートと実ファイルの二重管理。依存が増える。テンプレートは C としてビルドできない |
| 3 | Python が全文を直接出力 | 生成コードを文字列組み立てで全部書く | 依存なし。ロジックと出力が近い | C の体裁が Python 側に埋もれ、見た目を変えにくい |
| 4 | ファイル全体を生成物にする（DaVinci/tresos と同じ流儀） | `*_PBCfg.c` を丸ごと生成。手書き部分は別ファイルに分離 | 実ツールと同じ形で学習価値が高い。目印も混在もない。ARXML の導入時に自然 | 既存の説明コメントの移し先が必要。生成物をコミットするか決める必要がある |
| 5 | データだけを生成（`.inc` や X マクロ） | json → 初期化子の中身だけの `.inc`。`.c` は手書きで `#include` | 手書きと生成の境界がファイル単位で明確 | `.inc` が読みにくい。型の宣言と配列サイズの二重管理が残る |
| 6 | C → json の逆解析（C を正本にする） | C をパースして json に取り込む | 今のコードをそのまま使える | コメントやマクロの解析が難しい。正本が曖昧になる |

### 判断
- 試作の段階では方式 1 が最も軽く、既存の注釈やコメントを残したまま、コンパイル結果が一致することを確認できた。
- 方式 4 へは、目印の範囲を「ファイル全体」に広げるだけで移れる。生成ロジックは変わらない。
  ARXML を扱う段階（ファイルを全面的に機械生成する形）で移行を判断する。
- 方式 2 は二重管理が増え、方式 3 は体裁の変更が難しくなるため、現時点では採らない。
  方式 6 は逆解析の難しさに見合う利点がない。

### 方式 1 を保つ間の注意
1. ファイル冒頭の概要コメントは、json の内容を説明する文面に限る。数値や ID は書かない（手で合わせる必要が出る）。
2. モジュールを足すときは、生成領域の ID と生成関数を `gen_cfg.py` の `MODULES` に登録する。
3. 全モジュールへ展開して手書き部分が薄くなってきたら、方式 4 へ切り替えるかを判断する。

## Com の構成

Com（`Com_PBCfg.c`、`Com_Cfg.h`）の設定は、信号表に由来する値と、Com としての振る舞いを分けて持ちます。

### 信号表に置くものと、Com の設定 json に置くもの

| 信号表（フレームの事実） | Com の設定 json（Com としての振る舞い） |
|--------------------------|------------------------------------------|
| フレーム名、CAN ID、DLC、方向 | I-PDU ID、信号 ID（RX / TX で別の番号空間。並び順が ID） |
| 送信周期、受信タイムアウト（`constants` の `fromFrame` で参照） | 送信モード（DIRECT / PERIODIC / MIXED）、最小送信間隔、再送回数・間隔、送信デッドライン |
| フィールド名、ビット位置・サイズ、型、単位・スケール・範囲 | フィルタ、TransferProperty、初期値、タイムアウト時の動作、無効値の扱い |
| update-bit フィールド（位置） | どのフィールドを Com の信号にするか（`field` で名前参照）、update-bit への寄与の指定 |
| E2E / SecOC 用のフィールド（種別のみ） | コールバック名（通知、タイムアウト、callout、変換関数）、I-PDU グループ、ゲートウェイの対応 |

- 信号表のフィールドのうち、`e2e_*` / `secoc_*` の種別と、名前が `(` で始まるもの（update-bit）は、Com のシグナルにならない（自動で除外）。
- 信号表の値と異なる DLC を使う場合だけ、I-PDU に `dlc` を書く（例: SecOC 保護のフレームで、Com が見るのは認証対象のペイロードだけ）。
- `comments` に、各フィールドの注釈を書ける（無ければ、標準の注釈または注釈なし）。

### I-PDU グループと BswM
I-PDU グループは `Com.json` の `ipduGroups`（並び順が ID）に書く。I-PDU は `ipduGroupId` にマクロ名で所属を指定し（省略すると `COM_IPDU_GROUP_NONE`）、
BswM のルール（`BswM.json`）は `ipduGroup` に同じマクロ名を書いて、グループを起動・停止する。
BswM のルールは `perValue` で、値ごとに 1 本ずつ展開できる（Dcm の通信モード 12 通りなど）。
ルールの番号は並び順（添字）で、コメント中の `Rule N` は手で書いた文字なので、ルールを挿入・削除したときは直す。

### 既定値（json で省略したときの値）
`Com.json` には、**既定値と違う値だけ**を書く。省略した項目は、次の値になる（`modules/com.py` の `DEFAULT_*` と `ENUM_ZERO`）。

| 項目 | 省略したときの値 | 備考 |
|------|------------------|------|
| 数値の項目（`timeoutMs`、`isSignalGroup`、`mask`、`filterX` など） | 0 | C の初期化子の省略と同じ |
| コールバック（`*Cbk`） | なし（NULL） | 同上 |
| 列挙の項目（`filterAlgorithm`、`transferProperty`、`rxDataTimeoutAction`、`dataInvalidAction`） | 値が 0 の列挙値（`COM_FILTER_ALWAYS` など） | C の初期化子の省略と同じ |
| `ipduGroupId` | `COM_IPDU_GROUP_NONE` | **C の省略（0）とは異なる。** 0 は `COM_IPDU_GROUP_TELEMETRY` |
| `updateBitPosition` | 信号表の `(update-bit)` の位置。無ければ 255（なし） | **C の省略（0）とは異なる** |
| `txModeMode`（TX の I-PDU のみ） | `COM_TX_MODE_DIRECT` | **C の省略（0）とは異なる。** 0 は `COM_TX_MODE_MIXED` |

- 生成される C では、C の省略（0）と値が異なる上の 3 項目は、省略しても必ず明示して出力する。
- `comments` に注釈があるフィールドは、値を省略していても既定値で出力する（注釈と一緒に残す）。
- `txModeModeTrue` は、TMS を使う I-PDU だけが持つ項目なので、既定値は置かない（省略すると 0 = `COM_TX_MODE_MIXED`）。

### 移行で見つかった食い違い
- `EngineInfo` / `AbsInfo` の受信タイムアウトは、信号表では 3000 ms だったが、Com の実際の値（`COM_TIMEOUT_*_MS`）は 5000 ms だった。
  信号表の注記に「`COM_TIMEOUT_*_MS` の実測値」とあったため、信号表を実際の動作に合わせて 5000 ms に直した。

### E2E / SecOC の設定 json
E2E と SecOC の設定は、Com から分けて別の設定 json（`E2EXf.json`、`SecOC.json`）にしてある。

- **E2EXf.json**: どのフレームをどのインスタンスで保護するか（`frame`、`direction`、`profile`）、MaxDeltaCounter、Dem イベント、ステートマシンのしきい値。
  対応するプロファイルは Profile05 のみ。インスタンスごとの C の変数名は `stem` から作る（`E2EXf_<stem>CfgP05` など）。
- **SecOC.json**: どのフレームを保護するか、検証成功後に渡す Com の RX I-PDU（`comIpdu`）、Csm のジョブ、検証結果の通知関数。
  TX の Secured I-PDU は未対応（現状 TX 方向で SecOC を使う PDU は無い）。
- Com の変換関数・callout の名前（`txTransformCbk`、`rxIndicationCbk` など）は、Com の設定 json に書いてある。E2EXf の設定とつながっているかは検査する。
- 鍵・ジョブ・鍵名は `CryptoStack.json` にある（Crypto の鍵表、Csm のジョブ表、KeyM の鍵名表を 1 つの json から生成）。鍵は学習用の固定鍵で、16 進 32 桁で書く。
  ジョブの ID と鍵の ID は、並び順（添字）が値になる。CryIf の設定は対象外。

## 今後の課題

- Com の設定 json の整理。既定値を導入して、通常と違う値だけを書く形にする。ID マクロの説明など、json の中の長い説明文の置き場所も見直す。
- E2E の他のプロファイル（P01、P02 など）への対応。SecOC の TX Secured I-PDU への対応。
- 他のモジュール（Can、FiM、WdgM、NvM など）。信号表との関係は薄いため、必要になったら足す。
- Com のシグナルを使う側（Rte など）の連動。信号を追加したとき、Rte 側の読み書き関数は手で足す必要がある。
- 他のモジュール（CanSM、ComM、BswM など）への展開。
- 診断（DCM / UDS）の設定の対象化。入力となる表（DID 一覧など）の整理が先に必要。
- PDU ID のシンボリック名（`#define` による名前付け）の生成。現状の ID は直値。名前を付ける場合は、値の正本がポストビルドの表（`*_PBCfg.c`）にあるため、
  名前の定義も `*_PBCfg.h` に置いて値と同じ設定の単位にそろえるのを第一候補とする。ただし置き場所を定める仕様の記述は、手元の SWS には見つかっていない（未確認）。
- ARXML の入出力。json の項目名を ECUC のパラメータ名に寄せてあるので、変換層を足す形にできる。

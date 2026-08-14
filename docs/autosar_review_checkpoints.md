# AUTOSAR モジュールレビュー SHA チェックポイント台帳

> [README](../README.md) 関連。BSW モジュールを AUTOSAR 仕様書（`docs/autosar/`配下の PDF）と
> 突き合わせてレビューした際の記録。モジュール数が多く同種のチェックを繰り返す見込みのため、
> 「前回レビュー時から対象ファイルが変わっていなければ再レビュー不要」と判定できるように、
> レビュー観点ごとに対象ファイルの git SHA-1（短縮形）を記録する。

## 仕組み

1. モジュールをレビューする際、**そのレビューで実際に見た対象ファイル**を明確にする
   （例: Init() シグネチャの仕様適合性チェックなら `<Module>.h` のみ、内部ロジックまで見るなら
   `.c` も含める）。
2. `git log -1 --format=%h -- <対象ファイル>` で対象ファイルの直近コミット SHA を取得し、
   この台帳に観点・SHA・日付・結果とともに記録する。
3. 次回同じ観点でのレビューを依頼されたら、まず現在の SHA を再取得して台帳の値と比較する。
   - **一致** → 前回レビュー時から変更なし。再レビューせず、その旨を回答する。
   - **不一致** → 対象ファイルが変わっている。改めて仕様書と突き合わせ、この台帳の SHA・
     結果を更新する。

**注意**: SHA はモジュールフォルダ全体ではなく、レビュー観点に応じた対象ファイル単位で
記録すること。2026-08-13 にほぼ全モジュールの `.c` へトレースログ追加コミット
（`914c041 add: trace log`）が入ったため、フォルダ全体の SHA で判定すると常に
「変更あり」になってしまい効率化の意味がなくなる。Init() シグネチャのような公開 API の
仕様適合性は `.h` ファイルの SHA だけで十分判定できる（`.c` 内部実装の変更では
公開 API の仕様適合性は変わらないため）。

**`.h SHA` と `.c SHA` の役割分担**（2026-08-13 追加）: シグネチャのみを見た表（下記）は
`.h SHA` が変わっていなければそのレビュー結果（シグネチャの仕様適合性）は今も有効。
一方 `.c SHA` は「実装側が最後に触られたのはいつか」を並記するための補助情報で、
`.h SHA` が一致していても `.c SHA` が前回と変わっていれば、シグネチャ以外の観点
（挙動・エラー処理等）を深く見ていない以上「実装は変更されているので、今後より深い
レビュー（KeyM/WdgIf の表のような）をする際はスキップしないこと」を示す。
`.c SHA` の不一致だけでシグネチャ表の再レビューをトリガーする必要はない。

## 台帳: Init() シグネチャ仕様適合性

**観点**: AUTOSAR 4.3.1 SWS 文書が `<Module>_Init()` に対して `const <Module>_ConfigType*`
引数を要求しているか、`void`（引数なし）でよいか。**対象ファイル**: 各モジュールの `<Module>.h`。
**確認日**: 2026-08-13。

### 仕様通り（ConfigPtr 注入、実装も注入）

| モジュール | .h SHA | .c SHA | 仕様根拠 |
|---|---|---|---|
| Can | 2dad126 | 914c041 | SWS_Can_00223 |
| CanIf | 694993d | 914c041 | SWS_CANIF_00001 |
| Com | d628731 | 914c041 | Com_Init 本文の SWS_Com 記述 |
| PduR | 694993d | 914c041 | SWS_PduR 記述 |
| Gpt | 7aa4ccd | 914c041 | Gpt SWS 記述 |
| Mcu | 72d091f | 914c041 | Mcu SWS 記述 |
| NvM | a471e3b | 914c041 | NvM SWS 記述 |
| Wdg | 9bc33bb | 914c041 | Wdg SWS 記述 |
| WdgM | 9bc33bb | 914c041 | WdgM SWS 記述 |
| BswM | a7cc1b3 | 914c041 | BswM SWS 記述 |
| FiM | 694993d | 914c041 | SWS_Fim_00077 |
| SecOC | ed3f7d2 | 914c041 | SecOC SWS 記述 |

### 仕様通り（void、実装も void）

| モジュール | .h SHA | .c SHA | 仕様根拠 |
|---|---|---|---|
| Csm | 3435dc8 | 914c041 | SWS_Csm_00646 |
| Crypto | 3435dc8 | 914c041 | SWS_Crypto_91000 |
| CryIf | 3435dc8 | 914c041 | SWS_CryIf_91000 |
| EcuM | 694993d | 914c041 | SWS_EcuM_02811 |

### 対応済み（2026-08-13、opaque ConfigType + 常に NULL 方式で修正）

CanSM/CanTp/ComM/Dcm/Dem/Fee/Port/IoHwAb の8モジュールについて、`KeyM_ConfigType`と
同じ設計（不透明型・呼び出し元は常に NULL を渡す）で `_Init()` に `const
<Module>_ConfigType*` 引数を追加し、シグネチャを仕様準拠にした。ユーザー方針:
「IF（シグネチャ）はあまり乖離させたくないが、関数の中身は簡略化してよい」に基づき、
内部実装は `(void)ConfigPtr;` で無視するのみ（post-build データの実体は作らない）。
呼び出し元（`EcuM.c`、Fee のみ `MemIf.c`）も `NULL` 引数で更新済み。
`test/test_chain/` の `CanSM_Init()` 呼び出し3箇所（Bsw_RxChain_test.cpp/
Bsw_SleepChain_test.cpp/Bsw_WakeupChain_test.cpp）も追随済み。
`pio test -e native`(62件)/`pio test -e native_chain`(19件)/`pio run -e uno_r4`
いずれも成功を確認済み。コミット `444b7a0`（"modified: init function parameter"）で
確定（`docs: modified README.md` 等とは別コミット）。

| モジュール | .h SHA | .c SHA | 仕様根拠 |
|---|---|---|---|
| CanSM | 444b7a0 | 444b7a0 | SWS_CanSM_00023: `CanSM_Init(const CanSM_ConfigType* ConfigPtr)` |
| CanTp | 444b7a0 | 444b7a0 | SWS_CanTp_00208: `CanTp_Init(const CanTp_ConfigType* CfgPtr)` |
| ComM | 444b7a0 | 444b7a0 | SWS_ComM_00146: `ComM_Init(const ComM_ConfigType* ConfigPtr)` |
| Dcm | 444b7a0 | 444b7a0 | SWS_Dcm_00037: `Dcm_Init(const Dcm_ConfigType* ConfigPtr)`（実装は `Dcm_Cbk.c`） |
| Dem | 444b7a0 | 444b7a0 | SWS_Dem_00181: `Dem_Init(const Dem_ConfigType* ConfigPtr)` |
| Fee | 444b7a0 | 444b7a0 | SWS_Fee_00085: `Fee_Init(const Fee_ConfigType* ConfigPtr)` |
| Port | 444b7a0 | 444b7a0 | SWS_Port_00140: `Port_Init(const Port_ConfigType* ConfigPtr)`（既存の `Port_Cfg.h` 静的テーブル参照はそのまま維持、注入ポインタは未使用） |
| IoHwAb | 444b7a0 | 444b7a0 | SWS_IoHwAb_00119: `IoHwAb_Init<Id>(const IoHwAb..._ConfigType* ConfigPtr)` |

### 対応済み（2026-08-13追加、Adc: ヘッダコメントの「対象外」判定を誤って鵜呑みにしていた見落とし修正）

上記8モジュールの修正後、ユーザーから「意図的簡略化コメントがあった対象外モジュールは
本当に仕様通りか」と再確認を求められ、`Adc.h`/`Adc_Cfg.h` の「Adc_Init を持たない」という
既存コメントを検証せず信用していたことが判明。実際に `AUTOSAR_SWS_ADCDriver.pdf` を
`pdftotext` で確認したところ [SWS_Adc_00365] は `void Adc_Init(const Adc_ConfigType*
ConfigPtr)` を必須 API として規定しており、これは今回の8モジュールと同種の仕様乖離
（関数自体が存在しない、より大きな乖離）だった。同じ opaque ConfigType + 常に NULL 方式で
`Adc_Init` を追加し、`EcuM_Init()` に `Port_Init(NULL)` の直後（同じ MCAL 下位層）で
`Adc_Init(NULL);` を追加。Dio/MemIf は同様に `pdftotext` で `Dio_Init`/`MemIf_Init` を
grep したが SWS 文書中に0件で、こちらは確かに仕様上 Init を持たないことを確認済み
（下表参照）。

一方で `Dio`/`MemIf`/`SchM` は本ラウンドで改めて grep 検証済みのため、以降「対象外」
ヘッダコメントを再度疑う必要はない。教訓: 既存コードのコメント（「〜を持たない」等の
断定）も、それ自体が過去のレビューの結論を記録したものでない限り検証対象にすること。

| モジュール | .h SHA | .c SHA | 仕様根拠 |
|---|---|---|---|
| Adc | 444b7a0 | 444b7a0 | SWS_Adc_00365: `Adc_Init(const Adc_ConfigType* ConfigPtr)`。本実装はハードウェア初期化状態を持たないためシグネチャ適合のみ（実処理なし） |

### 未対応（本プロジェクト独自モジュールのため対象外）

| モジュール | 備考 |
|---|---|
| Nm | 本プロジェクト独自の簡略 Nm。仕様上対応する CanNm は SWS_CanNm_00208 で ConfigPtr 必須だが、Nm 自体が実 AUTOSAR の正式なモジュールではないため今回のスコープ外 |

### 対象外（仕様上 Init を持たない、または意図的簡略化として既にヘッダに明記済み）

| モジュール | 備考 |
|---|---|
| Dio | ヘッダコメントで「Dio_Init は存在しない」と明記済み。`AUTOSAR_SWS_DIODriver.pdf` を`pdftotext`後`grep -i Dio_Init`で0件ヒットを確認済み（2026-08-13） |
| MemIf | 仕様上 Init 自体が定義されていない（switch 専用モジュール）。`AUTOSAR_SWS_MemoryAbstractionInterface.pdf`を`pdftotext`後`grep -i MemIf_Init`で0件ヒットを確認済み（2026-08-13） |
| SchM | 実 AUTOSAR でも RTE 生成コード相当の排他エリアマクロ集であり、独立した SWS 文書・Init API を持たない（`SchM.h`は`SchM_Enter_*`/`SchM_Exit_*`マクロのみ） |

## 台帳: KeyM / WdgIf 深いレビュー（Init だけでなく主要 API 全体）

**観点**: Init だけでなく、主要 API（KeyM: Start/Update/Finalize、WdgIf: SetMode/SetTriggerCondition）
の挙動・引用している `\AUTOSARReq` タグの正確性を SWS 文書と突き合わせ。
**対象ファイル**: `<Module>.h` + `<Module>.c`。**確認日**: 2026-08-13。

| モジュール | SHA | 結果 |
|---|---|---|
| KeyM | 914c041 | 仕様通り。引用要求（SWS_KeyM_00158/00086/00103/00106/00013 等）の内容を実際の SWS_KeyManager.pdf（4.4.0）で確認、記載・実装とも正確。スコープ縮小（証明書 submodule 除外、Start/Update/Finalize のみ実装等）はヘッダに明記済み |
| WdgIf | 914c041 | 仕様通り。ServiceID(0x01/0x02/0x03)・引用要求(SWS_WdgIf_00042/00044/00046/00018)とも SWS_WatchdogInterface.pdf と一致。`WdgIf_Init`が無いことも仕様通り（そもそも定義されていない） |

## 台帳: CanSM / ComM 状態遷移レビュー

**観点**: CanSM/ComM の状態遷移の挙動（Bus-Off 回復シーケンス、複数ユーザ調停、
DET_E_UNINIT ガード等）と、引用している `\AUTOSARReq`/コメント中の SWS 番号の
正確性を SWS_CANStateManager.pdf / SWS_COMManager.pdf と突き合わせ。
**対象ファイル**: `<Module>.h` + `<Module>.c`。**確認日**: 2026-08-14。

| モジュール | SHA | 結果 |
|---|---|---|
| CanSM | 444b7a0 | 挙動は仕様通り。Bus-Off L1/L2 バックオフのタイミング・閾値ロジックは SWS_CanSM_00514/00515 と完全一致、DET_E_UNINIT ガード（SWS_CanSM_00184/00188/00190）も正確。SWS_CanSM_00636 の引用はやや不正確（00636 は `CanSMEnableBusOffDelay=TRUE` 時のみの要求だが、本実装は FALSE 相当。直後に「FALSE 相当」と注記済みのため実害は軽微、未修正） |
| ComM | 未反映（コミット前） | **修正済み**: `ComM_RequestComMode()` の複数ユーザ調停ロジックが `SWS_ComM_00069`（実際は「FULL_COM 突入時に Bus State Manager へ転送する」ことのみを規定）を誤って引用していた。正しくは調停の "highest wins" 戦略を規定する `SWS_ComM_00686`、および「キューイングせず最新要求で上書き」を規定する `SWS_ComM_00500`。`ComM.c`/`ComM.h` のコメント・`\AUTOSARReq` タグを修正済み（コミット後に SHA 更新要）。他の引用（00612/00858/00242）は正確 |

（`ComM_RequestComMode()` の引数バリデーションが `COMM_SILENT_COMMUNICATION`
（SWS_ComM_00151/00868 によりユーザーが直接要求できないはずの値）を弾いていない
という防御的ギャップも発見。実際の呼び出し元は全て NO_COM/FULL_COM のみを渡して
おり現状は到達しないため、未対応のまま記録のみ。）

## 経緯（Why）

2026-08-13、「プロジェクト全体をレビューしてほしい」→「BSW スタック全体のアーキテクチャ・
一貫性」という観点で Init() の設定注入パターンを横断チェックした。最初に「Csm/Crypto が
仕様から外れている」と指摘したが、これは記憶だけを頼りにした誤った指摘だった。実際に
`docs/autosar/4.3.1/` の SWS PDF を読んで検証したところ、Csm/Crypto はむしろ仕様通り
（`void`が正しい）で、逆に CanSM/CanTp/ComM/Dcm/Dem/Fee/Port/IoHwAb が仕様と乖離している
ことが分かった。教訓: 仕様適合性の主張は必ず実際の SWS PDF を読んで検証すること。

2026-08 の一括監査（PR #46-54、`docs/modules/`各所や実装コメントに記録）はモジュール構成の
網羅性と主要 API の仕様適合性が中心で、「全関数・全 requirement 番号を検証したわけではない」
ため、この Init() 引数の観点はその監査でも見落とされていた。

上記8モジュールの修正完了後、ユーザーから「意図的簡略化コメントがあった対象外モジュール
（Adc/Dio/MemIf/SchM）は本当に仕様通りか」と再確認を求められた。この時点でも
`Adc.h`/`Adc_Cfg.h` の「Adc_Init を持たない」という既存コメントを検証せずそのまま信用して
「対象外」に分類していたが、実際に `AUTOSAR_SWS_ADCDriver.pdf` を確認すると
[SWS_Adc_00365] が `Adc_Init(const Adc_ConfigType* ConfigPtr)` を必須 API として規定して
おり、見落としだった（Dio/MemIf は grep 0件で確認済み、こちらは正しかった）。
教訓: 既存コードの「〜を持たない」という断定コメントも、それ自体が過去のレビューの結論を
記録したものでない限り、実装者の思い込みの可能性があるため検証対象にすること。

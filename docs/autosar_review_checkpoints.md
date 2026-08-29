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

**固定 `.h SHA`/`.c SHA` 2 列を前提にしない**（2026-08-14 追加）: モジュールによっては
`<Module>_Cfg.h` 等、`.h`/`.c` 以外のファイルにも `\AUTOSARReq` 引用や仕様適合上の
判断（DET エラーコード、タイミング定数等）が書かれている（例: CanTp は
`CanTp_Cfg.h` に SWS_CanTp_00031/00321 の引用がある）。2 列表は「そのモジュールが
本当に `.h`+`.c` の 2 ファイルで完結する」場合のみ使い、それ以外は「対象ファイル」列に
実際にレビューした全ファイルと SHA を列挙する（Step 1 の原則「そのレビューで実際に見た
対象ファイルを明確にする」の徹底）。

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
| Nm | 2026-08-30 対応済み。`Nm_Init`/`NetworkRequest`/`NetworkRelease`/`RepeatMessageRequest`/`GetState` の5関数に SWS_CanNm_00208/00213/00214/00221/00223 準拠の ConfigPtr/NetworkHandleType 引数を追加（`NM_MAIN_NETWORK_HANDLE`、`NM_E_INVALID_CHANNEL` [SWS_CanNm_00192] による範囲チェック含む）。「Nm自体が実AUTOSARの正式なモジュールではないため対象外」という旧判断を、IFシグネチャは仕様準拠という方針のもとで上書きした |

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

## 台帳: CanTp 仕様引用レビュー

**観点**: ISO 15765-2 フレーム分割/組立・フロー制御・N_As/N_Bs/N_Cr タイムアウト監視の
挙動と、引用している `\AUTOSARReq`/コメント中の SWS 番号の正確性を
`AUTOSAR_SWS_CANTransportLayer.pdf` と突き合わせ。**確認日**: 2026-08-14。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| CanTp.h | 444b7a0 | **修正済み**: `CanTp_Transmit` の説明文が引用する `SWS_CanTp_00218` は仕様書中に存在しない番号（grep 0件）。実際に SF/FF 分割を規定するのは `SWS_CanTp_00231`（データが1フレームに収まる場合は SF）と `SWS_CanTp_00232`（収まらない場合はマルチフレームセッション開始）の2件。引用を修正済み（コミット後に SHA 更新要） |
| CanTp.c | 444b7a0 | **修正済み**: (1) `CanTp_TxConfirmation()` の `\AUTOSARReq` が `SWS_CanTp_00236`（実際は「割り込みコンテキストから呼び出し可能」という副次要求）を引用していたが、本プロジェクトの慣例（各関数は自身の「Service name」定義項目を引用する。Init→00208, Transmit→00212, RxIndication→00214 等）に合わせ `SWS_CanTp_00215`（CanTp_TxConfirmation 自身の Service 定義）に修正。(2) `CanTp_MainFunction()` の `\AUTOSARReq` も同じ `SWS_CanTp_00236` を引用しており、TxConfirmation 用の記述をそのまま流用したコピペ誤りだった（MainFunction とは無関係）。`SWS_CanTp_00213`（CanTp_MainFunction 自身の Service 定義）に修正。(3) SF 送信失敗時のコメントが「SWS_CanTp_00075 相当」（実際は N_As タイムアウト＝確認応答が届かない場合の規定であり、同期的な送信拒否とは状況が異なる）とやや不正確な類推引用をしていたため、関数自体に既に付いている `SWS_CanTp_00212` の定義（要求非受理時は E_NOT_OK）を指す形に修正。挙動面（busy 判定・N_Cr/N_Bs タイムアウト・FF 中断規則等）は仕様通りで問題なし（コミット後に SHA 更新要） |
| CanTp_Cfg.h | 444b7a0 | 問題なし。`SWS_CanTp_00031`（UNINIT ガード対象 API）、`CANTP_MODULE_ID=35`（BSWModuleList 参照）、タイミング定数のコメントはいずれも仕様と一致 |

## 台帳: Dem 仕様引用・デバウンス挙動レビュー

**観点**: DTC ライフサイクル (デバウンス確定・PendingDTC/ConfirmedDTC 自動クリア・
Aging・ExtendedData) の挙動と、引用している `\AUTOSARReq`/コメント中の SWS 番号の
正確性を `AUTOSAR_SWS_DiagnosticEventManager.pdf` と突き合わせ。**確認日**: 2026-08-14。

**前提**: `Dem_Cfg.h` 冒頭に「本モジュールは実際の SWS_Dem の個々の関数シグネチャとは
大きく異なる学習用簡略実装のため、SWS 側の Service ID との照合は行わない」と明記済み
（実際 `Dem_Init` の実仕様 Service ID は `0x02` だが本実装は `0x01`。これは既知・
意図的な逸脱であり今回のスコープ外）。今回は Service ID ではなく `\AUTOSARReq` 引用と
挙動を確認した。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| Dem.h | 444b7a0 | **修正済み**: `Dem_EventStatusType` の doc コメントが「PREPASSED/PREFAILED は Dem 内部が導出する値で、モニタからの入力としては受け付けない」ことを、あたかも SWS_Dem_00926 自体がそう規定しているかのように書いていたが誤り。実際は逆で、SWS_Dem_00418/00419/01212 により PREPASSED/PREFAILED はモニタが報告してよい正当な入力（段階的にカウンタを増減させる debounce 進行）であり、むしろ本来の FAILED/PASSED（SWS_Dem_00420/00421）は「モニタ側で既に確定した単発の結果」を意味しカウンタを閾値へ直接ジャンプさせる。本実装は FAILED/PASSED の名前のまま、仕様上は PREFAILED/PREPASSED に割り当てられた段階的カウント方式を採用しており、これ自体は学習用の妥当な簡略化だが、コメントの説明が不正確だったため修正（AUTOSAR がこれを規定しているのではなく本プロジェクト独自の簡略化である旨を明記）（コミット後に SHA 更新要） |
| Dem.c | 444b7a0 | **修正済み**: `Dem_ReportErrorStatus()` の doc コメントに同様の誤り（上記と同一修正）。加えて、CONFIRMED(bit3) を即時確定FAILEDで直ちに立てる挙動について、仕様の Figure 7.20 (SWS_Dem_00391) が本来持つ複数操作サイクルにまたがる `DemEventFailureCycleCounterThreshold`（PENDING→CONFIRMED 昇格の多サイクル成熟過程）を実装していない旨を、ヘッダの「AUTOSAR実装との主な違い」一覧に追記（`DemEventFailureCycleCounterThreshold=1` 相当として仕様上有効な設定であり不整合ではないため、挙動修正はせず注記のみ）。PendingDTC 自動クリア(SWS_Dem_00390 Figure 7.19)・testNotCompletedSinceLastClear の両方向クリア(SWS_Dem_00392 Figure 7.21)・Aging の「passed かつテスト済みサイクルのみ進行」(SWS_Dem_00489 の趣旨) はいずれも仕様と正確に一致、問題なし（コミット後に SHA 更新要） |
| Dem_Cfg.h | 444b7a0 | 問題なし。DEM_E_UNINIT 対象外 API 一覧 (SWS_Dem_00124) は Dem_SetEventStatus/ResetEventStatus/SetEventAvailable/ResetEventDebounceStatus/GetVersionInfo という仕様の列挙と正確に一致。DTC ステータスビットマスク定義(ISO 14229-1 Annex B)・DEM_MODULE_ID=54 (BSWModuleList 参照) も正確 |

## 台帳: Dcm 仕様引用・サービス挙動レビュー

**観点**: セッション制御・SecurityAccess ブルートフォース対策・SID×セッション許可表・
RequestDownload/TransferData/RequestTransferExit 状態機械・ECUReset の挙動と、
引用している `\AUTOSARReq`/コメント中の SWS 番号の正確性を
`AUTOSAR_SWS_DiagnosticCommunicationManager.pdf` と突き合わせ。**確認日**: 2026-08-14。

**前提**: `Dcm_Cbk.c` は本プロジェクト最大のファイル（2224 行）だが `\AUTOSARReq`/`SWS_Dcm_`
引用は元々 5 件のみと密度が低かった。`Dcm_Cfg.h` 冒頭に明記済みの通り、実際の SWS_Dcm が
規定するストリーミング型 PduR-Dcm インタフェース（Dcm_TpRxIndication 等）ではなく、CanTp が
組み立て済みペイロードを一括で渡す `Dcm_ComIndication` という本プロジェクト独自の簡略設計の
ため、個々のハンドラの Service ID 等は SWS 側と対応しない（既知・スコープ外）。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| Dcm.h | 444b7a0 | **修正済み**: `Dcm_Init()` の doc コメントが引用する `SWS_Dcm_00769` は実際には `DataServices_DIDRange_{Range}`（DID レンジ用の SW-C ポートインタフェース定義）であり、Init のセッションリセットとは無関係。正しくは `SWS_Dcm_00034`「During Dcm initialization, the session state is set to the value 0x01 ("DefaultSession")」（コミット後に SHA 更新要） |
| Dcm_Cbk.c | 444b7a0 | **修正済み**: 同一の `SWS_Dcm_00769` → `SWS_Dcm_00034` 誤引用（`\AUTOSARReq` タグ）。挙動面は以下いずれも仕様と整合し問題なし: SecurityAccess の「既にアンロック済みなら allZeroSeed (seed=0x0000) を返す」(SWS_Dcm_00323 の趣旨と一致、現状未引用だが正確)、ECUReset の「正応答送信 → セッションリセット」の順序(SWS_Dcm_00373/00594/00834 が要求する「モード遷移要求→応答送信→(TxConfirmation後に)実処理」の趣旨と、本実装が実際のリセットを行わずログのみで代替する簡略化の範囲で整合)、`Dcm_ComIndication` のディスパッチ順序（セッション許可判定を全 SID 共通で先に行い、その後 switch で SID 未対応=NRC 0x11 serviceNotSupported をデフォルト分岐とする構成）も仕様の一般的な流れと矛盾しない。SecurityAccess のブルートフォース対策（3 回失敗でロックアウト、待機時間中は NRC 0x37）自体は SWS_Dcm が規定せず ISO 14229-1 のアプリケーション実装判断に委ねられる領域のため、本実装独自設計として妥当（コミット後に SHA 更新要） |
| Dcm_Cbk.h | 444b7a0 | 該当引用なし。`Dcm_ComIndication` は AUTOSAR 標準の単一関数に対応しない本プロジェクト独自エントリポイントであることが Dcm_Cfg.h に明記済み |
| Dcm_Cfg.h | 444b7a0 | 問題なし。`SWS_Dcm_00143`（S3Server 既定値 5s。本実装は TesterPresent ログ抑制のため意図的に 60s へ変更、既に明記済み）は仕様と正確に一致 |

**未検証（ISO 14229-1 本体が `docs/autosar/` に無いため対象外）**: NRC 判定順序
（serviceNotSupported → subFunctionNotSupported → incorrectMessageLength →
conditionsNotCorrect/securityAccessDenied → requestOutOfRange 等の一般的な優先順位）や
blockSequenceCounter のラップアラウンド仕様は ISO 14229-1 本体の規定であり、
AUTOSAR SWS_Dcm はこれを深く再規定していないため、手元の SWS PDF だけでは確証を
持って検証できなかった。目視では各ハンドラの判定順序に明らかな矛盾は見られない。

## 台帳: NvM 仕様引用・ジョブキュー/冗長ブロック挙動レビュー

**観点**: RAM ミラー設計・非同期書き込みジョブキュー（FIFO・キャンセル巻き戻し）・
冗長ブロックの読み込み検証/自己修復アルゴリズムの挙動と、引用している
`\AUTOSARReq`/コメント中の SWS 番号の正確性を `AUTOSAR_SWS_NVRAMManager.pdf` と
突き合わせ。**確認日**: 2026-08-14。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| NvM.h | 914c041 | **修正済み**: 2 箇所が同じ誤った番号 `SWS_NvM_00426`（実際は `NvM_ReadAll` が `NvMDrvModeSwitch` 設定時にメモリデバイスを fast-mode へ切り替える、という無関係な要求）を引用していた。(1) `NvM_RequestResultType` の型コメント → 正しくは `SWS_NvM_00470`（`NvM_RequestResultType` の ImplementationDataType 定義）。(2) `NvM_GetErrorStatus()` のコメント → 正しくは `SWS_NvM_00451`（`NvM_GetErrorStatus` の Service 定義。Service ID 0x04 は一致確認済み）。`SWS_NvM_00208`（WriteBlock はジョブキューへ積むだけで即座に返る）・`SWS_NvM_00452`（GetVersionInfo 定義）は正確（コミット後に SHA 更新要） |
| NvM.c | 914c041 | 問題なし。冗長ブロックの読み込み検証（プライマリ・ミラー両面を毎回読み CRC 検証し、片面のみ破損なら健全な方で自己修復）は、仕様の最小要件 `SWS_NvM_00199`（ブロック1の読み込みが失敗した場合のみブロック2を読む、という遅延読み込み）より踏み込んだ実装だが、矛盾ではなく仕様が要求する最終結果（健全な方のデータを採用し破損を検出する）を包含する強化。ジョブキャンセル時の巻き戻し・FIFO投入順維持・2フェーズ(データ本体→CRC)ジョブ進行はいずれも仕様の趣旨と整合 |
| NvM_Cfg.h | 914c041 | **修正済み**: `NVM_BLOCK_ID_DEM_STATUS`/`_AGING`/`_EXTENDED` の doxygen コメントが「(9 bytes)」のままだったが、実際の `NVM_BLOCK_DEM_*_LENGTH` は `DEM_EVENT_COUNT`(=10) に追従して既に 10U（Dem のイベント数が過去に 6→8→10 と変遷した際の古いコメントの取り残し、`Dcm_Cbk.c` の `DCM_TX_BUF_SIZE` で見つかった過去の類似バグと同種）。「(10 bytes)」に修正。DET エラーコード・アドレス計算・「先頭 46 バイト」の記述は実際のレイアウトと正確に一致（コミット後に SHA 更新要） |
| NvM_PBCfg.h/.c | 914c041 | 該当引用なし |

## 台帳: BswM 仕様引用・ルールエンジン挙動レビュー

**観点**: ルール評価（AND/OR、単一条件との等価性、false→true 遷移のみ Action 実行）・
タスクマスク切り替え・PduGroupSwitch アクションの挙動と、引用している
`\AUTOSARReq`/コメント中の SWS・ECUC 番号の正確性を `AUTOSAR_SWS_BSWModeManager.pdf`
と突き合わせ。**確認日**: 2026-08-14。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| BswM.h | 914c041 | 問題なし。`SWS_BswM_00003`（GetVersionInfo 定義、Service ID 0x01 一致確認済み）は正確 |
| BswM.c | 914c041 | **修正済み**: `SWS_BswM_00808` として引用していたのは実際には `ECUC_BswM_00820`（BswMArgumentRef、コンフィグ項目 ID）だった。`\AUTOSARReq` 系ではなく `ECUC_` 系番号を `SWS_` として誤記していたパターン。また「ConditionCount=1 なら Operator によらず単一条件と等価」の根拠として引用していた `SWS_BswM_00814` は仕様書に存在しない番号（grep 0件）だったが、調べたところ実際には `ECUC_BswM_00814`（BswMLogicalOperator の説明文「If the logical operator is set to something other than BSWM_NOT, and the expression only consists of a single condition, then this parameter will have no effect」）が本文まで含めて正確に一致する内容だったため、`SWS_` → `ECUC_` へ修正して正しい引用として復元した。`SWS_BswM_00245`/`SWS_BswM_00247`（AND/OR 評価定義）・`SWS_BswM_00273`（PduGroupSwitch、Com_IpduGroupStart/Stop 呼び出し。仕様注記が「厳密な順序が必要なら 1 ルール=1 グループの個別アクションに分ける」と明記しており、本実装の 1 ルール=1 グループの簡略化そのものを公式に裏付けている）は正確（コミット後に SHA 更新要） |
| BswM_Cfg.h | 914c041 | **修正済み**: 同種の `SWS_BswM_00808` → `ECUC_BswM_00820`（BswMArgumentRef、Multiplicity=1..* の記述箇所）（コミット後に SHA 更新要） |
| BswM_PBCfg.h | 914c041 | **修正済み**: 3 箇所で同様の `SWS_`/`ECUC_` 誤記。`SWS_BswM_00807`→`ECUC_BswM_00807`（BswMModeCondition コンテナ）、`SWS_BswM_00814`→`ECUC_BswM_00814`（BswMLogicalOperator、2 箇所）、`SWS_BswM_00808`→`ECUC_BswM_00808`（BswMLogicalExpression コンテナ）（コミット後に SHA 更新要） |
| BswM_PBCfg.c | 914c041 | 問題なし。Rule3 が AND 複合条件になった経緯（Nm 導入後 ComM チャネルモードが EcuM の RUN/POST_RUN と独立に変化しうるため）、Rule5 の OR 条件との対称設計、いずれもコメント記載どおりで矛盾なし |

**教訓**: BswM は AUTOSAR の `SWS_<Module>_NNNNN`（挙動仕様）と `ECUC_<Module>_NNNNN`
（コンフィグパラメータ定義）という 2 系統の独立した番号体系を持つモジュールで、
本プロジェクトは元々コンフィグ項目を指すつもりで `ECUC_` 番号の数字部分だけを
`SWS_` プレフィックスで引用していた（数字自体は全て実在し内容も正確だった）。
他モジュールでは主に `SWS_` 系のみを扱っていたため見落とされていた誤記パターン。

## 台帳: WdgM 仕様引用・グローバル猶予サイクル/フェイルセーフ挙動レビュー

**観点**: Alive/Logical/Deadline Supervision の判定・グローバル EXPIRED 許容サイクル
（WdgMExpiredSupervisionCycleTol 相当）・POST_RUN↔RUN 復帰時のリセット挙動と、
引用している `\AUTOSARReq`/コメント中の SWS・ECUC 番号の正確性を
`AUTOSAR_SWS_WatchdogManager.pdf` と突き合わせ。**確認日**: 2026-08-14。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| WdgM.h | 914c041 | **修正済み**: `WdgM_LocalStatusType` の `WDGM_LOCAL_STATUS_DEACTIVATED` が `0x0FU` だったが、`SWS_WdgM_00359`（`WdgM_LocalStatusType` の Implementation Data Type 定義）では `WDGM_LOCAL_STATUS_DEACTIVATED = 4` と規定されている。本実装は WdgM_GetLocalStatus() が返す内部限定の値でワイヤフォーマット等の外部依存はないが、仕様の数値と異なっていたため `0x04U` へ修正（コミット後に SHA 更新要）。**2026-08-30 追記**: 当時 `WdgM_GetLocalStatus()` は `WdgM_LocalStatusType` を直接返す簡略シグネチャだったため「戻り値としてのみ使う」としていたが、その後 `Std_ReturnType WdgM_GetLocalStatus(SEID, WdgM_LocalStatusType* Status)`（SWS_WdgM_00169 準拠の OUT パラメータ形式）へ修正済み。DEACTIVATED は現在 `*Status` に書き込まれる値であり、戻り値（`Std_ReturnType`）ではない |
| WdgM.c | 914c041 | 問題なし。`SWS_WdgM_00119`〜`00122`（Global Supervision Status が OK/FAILED/EXPIRED のいずれでも WdgIf_SetTriggerCondition によるリフレッシュを継続し、STOPPED でのみ停止する）の引用は本文と完全一致。グローバル猶予カウンタ (`WdgM_ExpiredCycleCount`/`WdgM_GlobalStopped`) の実装、および `WdgM_ResumeSupervision()` がこのカウンタを意図的にリセットしない設計（ラッチ経由のフェイルセーフを壊さないため）は仕様の趣旨と整合 |
| WdgM_Cfg.h | 914c041 | **修正済み**: `WDGM_E_PARAM_SEID` のコメントが未解決のプレースホルダ `[SWS_WdgM_00xxx]` のままだった。`WdgM_CheckpointReached`/`WdgM_GetLocalStatus` それぞれの SEId 範囲チェック要求である `SWS_WdgM_00278`/`SWS_WdgM_00172` に修正。`WDGM_E_NO_INIT [SWS_WdgM_00389等]`・`ECUC_WdgM_00329`（WdgMExpiredSupervisionCycleTol）は正確（コミット後に SHA 更新要） |
| WdgM_PBCfg.h/.c | 914c041 | 該当する SWS/ECUC 番号引用なし（コンテナ名の言及のみ、数値照合の対象外） |

## 台帳: Com 仕様引用・TMS/デッドライン監視/ゲートウェイ挙動レビュー

**観点**: シグナル/シグナルグループ送受信・update-bit・TMS（Transmission Mode
Selector）・送受信デッドライン監視・I-PDU Group start/stop・Signal Gateway の
挙動と、引用している 88 件の一意な `\AUTOSARReq`/コメント中 `SWS_Com_NNNNN` 番号の
正確性を `AUTOSAR_SWS_COM.pdf` と突き合わせ。**確認日**: 2026-08-14。

**手法**: Com は本プロジェクト最大級の引用密度（Com.c だけで 116 件、6 ファイル
合計 218 件、一意番号 88 件）のため、まず 88 件全てが仕様書に実在するか機械的に
照合し（結果: 全件実在。CanTp/Dcm/NvM/BswM で見られた「存在しない番号」パターンは
今回は無かった）、そのうえで各番号の要求文を抽出し、引用元コードの文脈と突き合わせた。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| Com.c | c047082 | **1 件のドキュメント修正**: `Com_RecalcTms()` が「TmsContributor を持つシグナルが1つも無いI-PDUはTMS=false」としていたが、これは `SWS_Com_00679`（寄与するシグナルは存在するがどれも偽なら false）の適用範囲外で、仕様上その場合は `SWS_Com_00677` により TMS=true が正しい。ただし実際に修正を試みたところ、本プロジェクトの `Com_PBCfg.c` は TmsContributor を持たない I-PDU（E2EHealthStatus=PERIODIC、ImmobilizerStatus=DIRECT 等）について `TxModeModeTrue`/`TxPeriodMsTrue` を設定していない（0=MIXED/0ms のまま）ため、仕様どおり TMS=true にすると `Com_EffectiveTxModeMode()` が未設定の True 側フィールドを返し、実際に意図した送信モードが壊れる（E2EHealthStatus が PERIODIC から MIXED+周期0ms に変わる等）ことが判明。コードは変更せず、既知の意図的な相違点として doc コメントに記録した（TMS の True/False 切替を実際に使う WarningStatus は TmsContributor を持つため影響を受けない）。それ以外の 87 件（RX 部分受信 `SWS_Com_00574/00575/00870`、update-bit `SWS_Com_00055/00061/00062/00067/00324/00801/00802`、デッドライン監視 `SWS_Com_00333/00470/00500/00716/00738/00787/00875/00876/00879`、I-PDU Group start/stop `SWS_Com_91001/91002/00114/00222/00223/00444/00479/00491/00684/00685/00777/00787/00800/00840`、Signal Gateway `SWS_Com_00357/00360/00361/00377/00383/00701/00702/00706`、フィルタ `SWS_Com_00273/00303/00602/00603/00695`、TMC/TMS `SWS_Com_00032/00245/00676/00678/00679/00799`、TX トリガ `SWS_Com_00734/00742/00743` 等）はいずれも正確 |
| Com.h | c047082 | 問題なし。`Com_Init`(00432)・`Com_DeInit`(00129/00130/00804)・`Com_GetStatus`(00194)・`Com_GetVersionInfo`(00426)・`Com_RxIndication`(00123)・`Com_ReceiveSignal`(00198)・`Com_SendSignal`(00197)・`Com_SendSignalGroup`(00200)・`Com_ReceiveSignalGroup`(00201/00051/00638/00461)・`Com_TxConfirmation`(00124)・`Com_IpduGroupStart/Stop`(91001/91002 他) いずれも関数と要求の対応が正確 |
| Com_Cfg.h | c047082 | 問題なし。DET エラーコード（00442 ModuleId=50、00803 COM_E_PARAM=0x01、00804 COM_E_UNINIT=0x02、00805 COM_E_PARAM_POINTER=0x03、00837 COM_E_INIT_FAILED=0x04）はいずれも仕様の値と一致 |
| Com_Types.h | c047082 | 問題なし。update-bit・TMS・DataInvalidAction（NOTIFY/REPLACE、00680/00681/00717）・RxDataTimeoutAction（NONE/REPLACE/SUBSTITUTE、00470/00500/00875/00876）の各コメントはいずれも仕様の要求文とほぼ逐語的に一致 |
| Com_PBCfg.c | c047082 | 問題なし。個々のシグナル設定コメント（update-bit 位置、DataInvalidAction、RxDataTimeoutAction 等）の引用も正確 |

**総括**: 6 ファイルで唯一の実質的な発見が「意図的だが未文書化だった仕様との相違点」
であり、CanTp/Dem/Dcm/NvM/BswM で見られたような誤引用・存在しない番号は無かった。
最大規模のモジュールが最も引用精度が高かった（過去の `/code-review`/`/simplify` の
繰り返し適用や、このセッション内の RxTimeoutChain 単体テスト追加等で既に相応の
検証を受けてきたためと考えられる）。

## 台帳: SecOC / Csm / CryIf / Crypto 仕様引用レビュー

**観点**: SecOC の Secured I-PDU 検証（MAC 照合・フレッシュネス単調増加判定）・
Csm/CryIf/Crypto 3 層構成（ジョブディスパッチ・鍵書き換え/有効化のパススルー連鎖）の
挙動と、引用している `\AUTOSARReq`/コメント中の SWS 番号の正確性を
`AUTOSAR_SWS_SecureOnboardCommunication.pdf`/`AUTOSAR_SWS_CryptoServiceManager.pdf`/
`AUTOSAR_SWS_CryptoInterface.pdf`/`AUTOSAR_SWS_CryptoDriver.pdf`（および相互参照先の
`AUTOSAR_SWS_KeyManager.pdf`）と突き合わせ。**確認日**: 2026-08-15。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| SecOC.h | 914c041 | **修正済み**: `SecOC_IfTransmit()` の説明が引用する範囲 `[SWS_SecOC_00057]〜[SWS_SecOC_00059]` のうち `SWS_SecOC_00059` は仕様書中に独立した定義が存在せず（他要求からの参照でのみ登場: 「(see SWS_SecOC_00057, SWS_SecOC_00058, SWS_SecOC_00059)」）、しかもその参照文脈は本実装が使わない別の送信フロー（7.4.2 "Authentication during triggered transmission"、FrIf 等のトリガ式送信）に属する。本実装が実装するのは 7.4.1 "Authentication during direct transmission"（ad-hoc、CanIf 等）で、これに該当する `SWS_SecOC_00057`/`SWS_SecOC_00058` のみに絞った（コミット後に SHA 更新要） |
| SecOC.c / SecOC_Types.h / SecOC_Cfg.h / SecOC_PBCfg.c | 914c041 | 問題なし。DataToAuthenticator の Big Endian 連結(00011)・切り詰め MAC(00192, Profile 1)・ad-hoc 送信の 3 段階(00058/00060〜00062)・Development Error 表(00101)はいずれも正確 |
| Csm.h / Csm.c / Csm_Cfg.h | 914c041 | 問題なし。全 13 件の引用（Init 未初期化例外なし=91008、DET コード 91009/91011/91012/91010、MacGenerate/MacVerify/KeyElementSet/KeySetValid の各 Service 定義=00982/01050/00957/00958、CryIf への実質パススルー=01002/01003、他モジュール参照 SWS_KeyM_00016）すべて仕様と正確に一致 |
| CryIf.h / CryIf.c / CryIf_Cfg.h | 914c041 | 問題なし。全 11 件の引用（GetVersionInfo の UNINIT 例外なし=00016、ProcessJob/KeyElementSet/KeySetValid の Service 定義=91003/91004/91005、Crypto への実質パススルー=00044/00055/00058、DET コード 00027/00028/00029/00053）すべて正確 |
| Crypto.h / Crypto.c / Crypto_Cfg.h | 914c041 | **修正済み**: `CRYPTO_E_KEY_NOT_VALID=9` の根拠として `SWS_Crypto_00194`（Runtime Error Types 表）を引用していたが、この表には `CRYPTO_E_RE_*` 系4項目（SMALL_BUFFER/KEY_NOT_AVAILABLE/KEY_READ_FAIL/ENTROPY_EXHAUSTED）しかなく `CRYPTO_E_KEY_NOT_VALID` は含まれない。実際の値9は `SWS_Crypto_00043`「Std_ReturnType の拡張値レンジ」の一項目であり、DET の Development/Runtime Error のいずれでもない（サービス関数の戻り値拡張）。カテゴリを取り違えていたため、`Crypto_Cfg.h`/`Crypto.c` のコメントを修正（本実装は同期処理のみでこの拡張戻り値自体は使わず、DET_LOGW のログ識別にのみ値を流用している点は変更なし）。他の引用（Init 失敗=00045、GetVersionInfo NULL チェック=00047、ProcessJob/KeyElementSet/KeySetValid の Service 定義=91003/91004/91014、DET コード 00057/00058/00079、KeySizeMismatch 相当=00146、他モジュール参照 SWS_KeyM_00046/00008）はすべて正確（コミット後に SHA 更新要） |
| Crypto_Aes128.c/.h・Crypto_Cmac.c/.h・各 PBCfg | 914c041 | `\AUTOSARReq` 引用なし（アルゴリズム実装自体は SWS の対象外、学習用自前実装として既に明記済み） |

**総括**: SecOC/Csm/CryIf/Crypto の 4 層で、SecOC に 1 件（存在しない/別文脈の要求番号を
範囲引用に含めていた）、Crypto に 1 件（実在する番号だが参照先テーブルの種別を
取り違えていた）の計 2 件を修正。Csm・CryIf は全件正確だった。

## 台帳: CanIf / PduR 仕様引用レビュー

**観点**: CAN ドライバ⇄上位層（CanTp/Com/SecOC/Dcm）を仲介する配送層の
ディスパッチ・ルーティング（TX/RX PDU テーブル検索、HOH/CanId 照合、
マルチキャスト配信、TransmitOverrideFct 経由の SecOC 迂回）の挙動と、
引用している `\AUTOSARReq`/コメント中の SWS 番号の正確性を
`AUTOSAR_SWS_CANInterface.pdf`/`AUTOSAR_SWS_PDURouter.pdf` と突き合わせ。
**確認日**: 2026-08-15。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| CanIf.c | 914c041 | **修正済み**: `CanIf_DeInit()` の `\AUTOSARReq` が `SWS_CANIF_00002`（仕様書に存在しない番号）を引用していた。正しくは `SWS_CANIF_91002`（`CanIf_DeInit` の Service 定義、Service ID 0x02 一致確認済み）（コミット後に SHA 更新要） |
| CanIf_Types.h | 914c041 | **修正済み**: `CanIf_RxIndicationFctType`（上位層 RX 通知コールバック型）のコメントが `SWS_CANIF_00056`（実際は Data Length Check 時に配信先モジュールを識別する、という無関係な挙動要求）を引用していた。正しくは `SWS_CANIF_00012`（`<User_RxIndication>` 自体の Service 定義、シグネチャも一致確認済み）。`CanIf_TxConfirmationFctType` の `SWS_CANIF_00011`（`<User_TxConfirmation>` 定義）は正確（コミット後に SHA 更新要） |
| CanIf_Cfg.h | 914c041 | 問題なし。DET エラーコード全 6 値（CANID=10/HOH=12/LPDU=13/CONTROLLERID=15/POINTER=20/INVALID_TXPDUID=50）は 7.27.1 表と完全一致 |
| CanIf.h / CanIf_PBCfg.h・c | 914c041 | 該当引用なし |
| PduR.c / PduR.h | 914c041 | 問題なし。Init(00334)・RxIndication テンプレート(00362, `<User:Lo>RxIndication`)・TxConfirmation テンプレート(00365, `<User:Lo>TxConfirmation`)・Transmit テンプレート(00406, `<User:Up>Transmit`)・GetVersionInfo(00338)・UNINIT 例外(00119)はいずれも関数と要求の対応が正確。`PduR_SecOCTransmit()` が引用する他モジュール要求 `SWS_SecOC_00062` も適切（SecOC 側レビューで確認済みの内容と一致） |
| PduR_Types.h | 914c041 | **修正済み**: `PduR_PBConfigType` 型のコメントが `SWS_PduR_00328`（実際は「ONLINE 状態でのみルーティングする」という無関係な挙動要求）を引用していた。正しくは `SWS_PduR_00241`（`PduR_PBConfigType` 自体の定義：「post-build-time 設定データを持つ外部データ構造、PduR_PBcfg.c に実装される」）（コミット後に SHA 更新要） |
| PduR_Cfg.h | 914c041 | **修正済み**（引用ではなくアーキテクチャ記述の陳腐化）: TX パス 3 のコメントが「COM → SecOC → CanIf（E2EHealthStatus）」という、E2E Profile05 単体保護への切り替えで撤去済みの旧構成のまま残っていた。同じモジュール内の `PduR_PBCfg.c` 側は既に正しく更新済みだったため、`PduR_Cfg.h` 側もそれに合わせて修正（「COM → CanIf」に、SecOC 撤去の経緯を注記）。DET エラーコード（00100 表、00119 UNINIT、00221 PDU_ID_INVALID）は正確（コミット後に SHA 更新要） |
| PduR_CanIf.h / PduR_Com.h / PduR_SecOC.h / PduR_PBCfg.c | 914c041 | 問題なし（00362/00365 のエイリアスマクロ参照のみ） |

**総括**: CanIf に 2 件（存在しない番号、無関係な番号）、PduR に 2 件（無関係な番号、
モジュール内ファイル間の記述陳腐化）を修正。これで CanTp/Dcm/SecOC 等、既にレビュー
済みの上位モジュール群が依拠する配送層（CanIf/PduR）も一通り検証できた。

## 台帳: E2EXf 仕様引用レビュー

**観点**: Com と E2E_P01/E2E_P05（ライブラリ本体、別途 docs/E2E_Profile1_Notes.md・
docs/E2E_Profile5_Notes.md でレビュー済み）を繋ぐ統合層としての E2EXf の挙動
（モジュール自身の初期化状態管理、Check/Protect への委譲、Dem へのイベント報告、
Profile05 の WaitForFirstData 初回受信特別扱い）と、引用している
`\AUTOSARReq`/コメント中の SWS 番号の正確性を `AUTOSAR_SWS_E2ETransformer.pdf`
と突き合わせ。**確認日**: 2026-08-15。

**注記**: `SWS_E2EXf_00133/00151` のようにスラッシュで複数番号をまとめて引用する
省略記法が本モジュールに存在し、単純な正規表現（`SWS_E2EXf_[0-9]+`）では2番目の
番号（00151）を取りこぼす。捕捉用の正規表現を修正して再走査し、全 8 件を確認した。

| 対象ファイル | SHA | 結果 |
|---|---|---|
| E2EXf.h | 914c041 | 問題なし。Init(00130)・DeInit(00146/00148)・Transform/InverseTransform 系の UNINIT ガード(00133/00151)・GetVersionInfo(00036、UNINIT 例外・00149 NULL チェック)、DET エラーコード表(00137: UNINIT=0x01/PARAM_POINTER=0x04)いずれも正確 |
| E2EXf.c | 914c041 | 問題なし。上記と同一の引用を実装が忠実に反映。Profile01 の OK/OKSOMELOST/SYNC/INITIAL 4状態受理・Profile05 の OK/OKSOMELOST 2状態受理・WaitForFirstData の初回特別扱いはいずれも E2EXf 独自の統合ロジックとして明確に文書化済みで、仕様との矛盾はない |
| E2EXf_PBCfg.h / .c | 914c041 | 問題なし（00130 の参照のみ） |

**総括**: E2EXf は誤引用ゼロ。CanTp/Dcm/NvM/BswM/WdgM/Com/SecOC/Crypto/CanIf/PduR で
少なくとも1件ずつ見つかった誤引用パターンが、Csm/CryIf に続きここでも見られなかった
（両者とも「呼び出し元を一切知らない薄い層」という設計の共通点があり、外部依存が
少なく引用ミスが混入しにくかった可能性がある）。

## 台帳: Can 仕様引用レビュー

**観点**: MCP2515 を操作する最下層ドライバの挙動（状態遷移検証、TX 確認の
Can_MainFunction_Write への遅延、RX 割り込み＋ポーリング二重化、Bus-Off/Wakeup
ポーリング）と、引用している `\AUTOSARReq`/コメント中の SWS 番号の正確性を
`AUTOSAR_SWS_CANDriver.pdf` と突き合わせ。**確認日**: 2026-08-15。

**注記**: 本モジュールにも `SWS_Can_00195/00409-00412` のようなスラッシュ・
ダッシュ区切りの複数番号引用があり、単純な正規表現では2番目以降を取りこぼす
（`E2EXf` レビューで発見した問題と同じ）。捕捉用の正規表現を修正して再走査した。
また `SWS_Can_00104` の Development Errors 表（7.11 章）は pdftotext -layout の
多段組みで項目名と値[hex]が視覚的にずれて抽出される（BswM/Crypto で遭遇したのと
同種の問題）。素朴に隣接テキストとして読むと `CAN_E_PARAM_DLC=0x02` 等に見えるが、
項目名の並び（PARAM_POINTER/PARAM_HANDLE/PARAM_DLC/PARAM_CONTROLLER/UNINIT/
TRANSITION/DATALOST/PARAM_BAUDRATE/ICOM_CONFIG_INVALID の9件）と値の並び
（0x01〜0x09）をそれぞれ独立した並びとして再整列すると、`CAN_E_PARAM_POINTER
=0x01`,`CAN_E_PARAM_DLC=0x03`,`CAN_E_PARAM_CONTROLLER=0x04`,`CAN_E_UNINIT=0x05`,
`CAN_E_TRANSITION=0x06` となり、本実装の定義済み値と完全に一致することを確認した
（「気づいたが実際には正しかった」ケース。修正はしていない）。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| Can.c | 914c041 | **修正済み**: (1) `Can_SetControllerMode()` が本文コメントと `\AUTOSARReq` の両方で引用していた `SWS_Can_00195` は仕様書に存在しない番号（大文字小文字を無視しても不在）。正しくは `SWS_Can_00200`（「不正な遷移要求で CAN_E_TRANSITION を報告する」という一般要求。`Can_Cfg.h` の `CAN_E_TRANSITION` 定義で既に正しく引用されていた番号と同一、かつ隣接する `SWS_Can_00409〜00412`（各遷移個別の要求）が参照する先でもある）に修正。(2) `Can_MainFunction_Wakeup()`/`Can_Isr()` が引用する `SWS_Can_00271` は本来「Can モジュールが `EcuM_CheckWakeup()` を直接呼ぶ」ことを要求するが、本実装は EcuM のウェイクアップソース管理を持たず、一貫して CanIf 経由で `CanSM_ControllerWakeup()`（AUTOSAR EcuM Wakeup Validation Protocol 相当を CanSM 側で模した検証シーケンス）へ委譲する設計を採る。00271 の「ISR または Can_MainFunction_Wakeup のいずれかの文脈で通知する」というタイミング要件自体は満たしているが、通知先関数名までは仕様どおりではないため、その旨を doc コメントに明記した（コードの挙動自体は変更せず、既存の CanIf/CanSM 経由設計をそのまま維持）（コミット後に SHA 更新要） |
| Can.h / Can_PBCfg.h・c | 914c041 | 該当引用なし |
| Can_Cfg.h | 914c041 | 問題なし。DET エラーコード5件（上記の再整列で確認）・ApiId 全8件・`CAN_FRAME_MAX_DLC=8`(00218)・`Can_ConfigType`(00413) いずれも正確 |
| Can_GeneralTypes.h | 914c041 | 問題なし。`Can_HwType`(仕様書内では大文字違いの`SWS_CAN_00496`、内容は完全一致)・`Can_StateTransitionType`(00417、具体的な列挙値は仕様が規定していないため実装依存で問題なし)・`Can_PduType`/`Can_ReturnType` いずれも整合 |

**総括**: Can に 2 件（存在しない番号、通知先関数名の仕様との既知の相違）を修正。
いずれも「実機で見つかった問題を踏まえた意図的な設計判断」の周辺にあった引用で、
挙動自体は変更せず記録を正確にする方向で対応した。

## 台帳: Gpt 仕様引用レビュー

**観点**: チャネル状態機械（initialized/running/stopped/expired）・ISR コンテキスト
（Gpt_OnTick）とメインループ側 API 間の排他制御・CONTINUOUS/ONESHOT モードの
挙動と、引用している `\AUTOSARReq`/コメント中の SWS 番号の正確性を
`AUTOSAR_SWS_GPTDriver.pdf` と突き合わせ。**確認日**: 2026-08-15。

**注記**: `SWS_Gpt_00404` は初回の機械照合で「存在しない」と誤検出したが、原因は
7.4.1 Development Errors 表（ID 列）が pdftotext で `SWS_Gpt` と `_00404` の
2行に分割されて抽出されたためで、実際には存在し内容も正確だった（誤検出と判明
したため修正なし）。同じ表の Value[hex] 列も `SWS_Gpt_00218` 等の並びと視覚的に
ずれて抽出され、素朴に読むと `GPT_E_INIT_FAILED=0x15` に見えたが、-layout なしの
逐次抽出で ID・エラーコード名・値をそれぞれ独立した並びとして再整列すると
`GPT_E_UNINIT=0x0A / ALREADY_INITIALIZED=0x0D / INIT_FAILED=0x0E / PARAM_CHANNEL=0x14
/ PARAM_VALUE=0x15 / PARAM_POINTER=0x16` となり、本実装の定義済み値と完全に一致
（Runtime Errors 表の `GPT_E_BUSY=0x0B`/`GPT_E_MODE=0x0C` も同様に一致確認）。
Can レビューに続き、この手の多段組み DET 表は必ず複数の抽出方法で読み直してから
判定すること。

| 対象ファイル | SHA | 結果 |
|---|---|---|
| Gpt.h | 914c041 | 問題なし。Init(00107)・DeInit(00234, BUSY)・GetTimeElapsed(00010/00361)・GetTimeRemaining(00083)・StartTimer(00274/00084 BUSY)・StopTimer(00013/00344 no-op)・EnableNotification(00014/00377)・DisableNotification(00015/00379)・Wakeup機能未実装の根拠(00201)・チャネル状態機械(00295)・ISR通知(00275) いずれも関数と要求の対応が正確 |
| Gpt.c | 914c041 | 問題なし。CONTINUOUS モードでの周回リセット(00361)・StartTimer の Value 範囲チェック(00218)・GetTimeRemaining の EXPIRED=0(00305)・StopTimer no-op(00344)・GPT_E_INIT_FAILED が Gpt_Init 専用である根拠(00404、実際は存在する番号)いずれも正確 |
| Gpt_Cfg.h | 914c041 | 問題なし。DET エラーコード全8件（Development 6件 + Runtime 2件、上記の表再整列で確認）・ApiId 全9件・型定義(00358 Gpt_ChannelType/00359 Gpt_ValueType) すべて正確 |
| Gpt_PBCfg.h / .c | 914c041 | 該当引用なし |

**総括**: Gpt は誤引用ゼロ。Csm・CryIf・E2EXf に続き4件目の「誤引用が見つからな
かった」モジュールとなった。

## 台帳: Mcu 仕様引用レビュー

**観点**: リセット原因の読み取り・キャッシュ（`Mcu_Hw_ReadAndClearResetReason()`
が1起動につき1回しか呼べない制約）と、Mcu_Init() が Serial.begin() より前に
呼ばれるため DET ログ関数を一切使えない制約下での実装の挙動、および引用している
`\AUTOSARReq`/コメント中の SWS 番号の正確性を `AUTOSAR_SWS_MCUDriver.pdf` と
突き合わせ。**確認日**: 2026-08-15。

**注記**: 2026-08 の一括監査（PR #46-54）で Mcu モジュール自体は新規実装されたが、
台帳に記録済みの通りその監査は「モジュール構成の網羅性」中心で「全 requirement
番号の検証」はスコープ外だった。今回が本モジュール初の深いレビュー。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| Mcu.h | 914c041 | 問題なし。Init(00153/00026)・GetResetReason(00158/00005/00133/00125)・GetResetRawValue(00159/00006/00135/00125)・GetVersionInfo(00162、00125 の UNINIT 例外) いずれも関数と要求の対応が正確。`Mcu_ResetType`(00252)・`Mcu_RawResetType`(00253) も正確 |
| Mcu.c | 914c041 | 問題なし。`Mcu_Hw_ReadAndClearResetReason()` の「読み取りと同時にクリアするため1起動1回」という制約に対応するキャッシュ設計、`Mcu_Init()` が NULL ConfigPtr チェックより前に必ずレジスタクリアを実行する順序（2026-08 のレビューで発見・修正済みの過去バグの教訓を踏まえた設計）、`MCU_RESET_UNDEFINED` を未初期化時(00133)と BrownOut/External 単独検出時の両方に使う簡略化、いずれも仕様と整合し矛盾なし |
| Mcu_Cfg.h | 914c041 | **修正済み**: `MCU_E_PARAM_CONFIG` のコメントが「Mcu_Init() の NULL ConfigPtr チェックに使う」と、あたかも実際に `Det_ReportError()` されているかのように書かれていたが、`Mcu_Init()` は Serial.begin() より前に呼ばれるため DET ログ関数を一切呼ばない設計（意図的、Mcu.c 冒頭コメントに詳細あり）であり、この DET コードは定義のみで実際には一度も報告されない（NULL ConfigPtr は黙って早期 return するのみ）。「使う」という言い切りを「定義のみで実際には報告されない」旨に訂正。DET エラーコード3件の値（0x0A/0x0F/0x10、Development Errors 表の逐次抽出で確認）・ApiId 全4件は正確（コミット後に SHA 更新要） |
| Mcu_PBCfg.h / .c | 914c041 | 該当引用なし |

**総括**: Mcu は仕様引用そのものに誤りはなく、唯一の指摘は「定義されているが実際
には使われていない DET コードの説明が、あたかも使われているかのように書かれて
いた」というドキュメントの正確性の問題（Csm/CryIf/E2EXf/Gpt に続き、これも
「誤引用ゼロ」に近いモジュール）。

## 台帳: MemIf 仕様引用レビュー

**観点**: NvM と Fee（唯一の下位ドライバ）の間のルーティング層としての挙動
（DeviceIndex 検証の要否、AUTOSAR 非標準の Init/MainFunction/WriteImmediate
拡張）と、引用している `\AUTOSARReq`/コメント中の SWS 番号の正確性を
`AUTOSAR_SWS_MemoryAbstractionInterface.pdf` と突き合わせ。**確認日**: 2026-08-15。

**前提**: `MemIf.c` 冒頭のコメントに記録済みの通り、`SWS_MemIf_00018/00019/00022`
の引用は 2026-08 のスペック監査で「実際の文言と逆の主張になっていた」誤りが
既に発見・修正されていた（単一デバイス構成では DeviceIndex 検証は仕様上省略して
よいにもかかわらず、あたかも必須であるかのように誤って引用していた）。今回は
その修正が正しく反映されているか、および残り全件を独立して再検証した。

| 対象ファイル | SHA | 結果 |
|---|---|---|
| MemIf.h | 444b7a0 | 問題なし。単一デバイス構成でのマクロ実装許容(00018/00019)・複数デバイス時のみ DeviceIndex 検証必須(00022)の引用はいずれも正確（過去の誤り修正が正しく反映されている）。MemIf_Init/MemIf_MainFunction が AUTOSAR 非標準の拡張である旨も正確 |
| MemIf.c | 444b7a0 | 問題なし。`MemIf_GetJobResult()` の `\AUTOSARReq` 相当コメントが引用する `SWS_MemIf_00043`（Return value: development error 検出時は 00022 に従い MEMIF_JOB_FAILED を返す）は Service 定義の Return value 記述と逐語的に一致 |
| MemIf_Cfg.h | 444b7a0 | 問題なし。DET エラーコード2件（`MEMIF_E_PARAM_DEVICE=0x01`/`MEMIF_E_PARAM_POINTER=0x02`、7.1.1 表の逐次抽出で確認）・ApiId 6件（Read=0x02/Write=0x03/Cancel=0x04/GetStatus=0x05/GetJobResult=0x06/GetVersionInfo=0x08、8.3 章の各 Service ID[hex] と一致）いずれも正確 |
| MemIf_Types.h | 444b7a0 | 問題なし。`MemIf_StatusType`(00064)/`MemIf_JobResultType`(00065) とも仕様は列挙順のみ規定し具体的な数値を強制しないため、実装側の連番割当は問題なし |

**総括**: MemIf は誤引用ゼロ（かつ過去に発見済みの誤りが正しく修正済みであることも
確認）。Csm/CryIf/E2EXf/Gpt/Mcu に続き6件目の「誤引用が見つからなかった」モジュール。

## 台帳: Nm 仕様引用レビュー

**観点**: 独自簡略実装（単一チャネル、Partial Networking/NM Coordinator Sync/User Data/
Remote Sleep Indication/Passive Mode 対応除外）である Nm の CanNm 状態機械実装が、
`AUTOSAR_SWS_CANNetworkManagement.pdf` の該当要求と正しく対応しているか、43件の
固有 `SWS_CanNm_NNNNN` 引用（全件が仕様書に実在することは確認済み）を一件ずつ内容
突き合わせで検証。**確認日**: 2026-08-15。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| Nm.h | 75f8007 | **修正済み**: (1) `Nm_NetworkRequest()` の `\AUTOSARReq {SWS_CanNm_00208}` は実際には `CanNm_Init` の Service description block（8.3.1 章、ApiId 0x00）であり、`Nm_NetworkRequest`（ApiId 0x02）の正しい引用は `SWS_CanNm_00213`（8.3.4 章、Service ID[hex] 0x02 で一致確認）。(2) `Nm_RxIndication()` の Bus-Sleep Mode 中受信の説明が引用する `SWS_CanNm_00126`（Bus-Sleep Mode に**進入した時**の Nm_BusSleepMode 通知要求）は、ここで説明している「Bus-Sleep Mode 中に NM PDU を**受信した**」という別事象とは無関係。正しくは `SWS_CanNm_00127`（Bus-Sleep Mode 中の受信 → 上位層通知）に訂正 |
| Nm.c | 914c041 | **修正済み**: (1) 上記と同一の `00126`→`00127` 誤りが `Nm_RxIndication()` 本体にも重複していたため同様に修正（Nm.h とは異なり `00127` は元々併記されていたため、`00126` のみ除去）。(2) `Nm_RepeatMessageRequest()` は `SWS_CanNm_00137` のガードにより到達可能な状態が Normal Operation/Ready Sleep の2状態のみだが、コメントは Normal Operation 分の番号（`00121`=ビットセット、`00120`=状態遷移）のみを引用し、かつ状態遷移側は `00119`（Repeat Message Request **Bit を受信**した場合の要求。関数呼び出しではなく他ノードからの受信イベント）という無関係な番号を誤って併記していた。Ready Sleep 分の `00113`（ビットセット）/`00112`（状態遷移）を追加し、`00119` を `00112` に訂正 |
| Nm_Cfg.h | 75f8007 | **修正済み**: `NM_DLC` のバイト配置コメントが「byte[0]=CBV/byte[1]=NID は `SWS_CanNm_00074`/`00075` の**デフォルト**配置」と主張していたが、仕様のデフォルトは逆順（`CanNmPduNidPosition` 既定 Byte 0 = NID、`CanNmPduCbvPosition` 既定 Byte 1 = CBV）であり、本プロジェクトの配置はデフォルトの反転である。「デフォルト配置」という誤った言い切りを、仕様上の配置可能値の言及＋非デフォルトを選択している旨の説明に訂正（コミット後に SHA 更新要） |

**検証**: DET エラーコード4件（`NM_E_UNINIT=0x01`/`NM_E_NET_START_IND=0x04`/
`NM_E_NETWORK_TIMEOUT=0x11`/`NM_E_PARAM_POINTER=0x12`、7.14.1 Development Errors 表と
一致）・ApiId 全11件（Init=0x00/NetworkRequest=0x02/NetworkRelease=0x03/
RepeatMessageRequest=0x08/GetState=0x0B/SetTxEnabled=0x0C(CanNm_DisableCommunication
代用と明記済み)/MainFunction=0x13/TxConfirmation=0x40/RxIndication=0x42/DeInit=0x10/
GetVersionInfo=0xF1）いずれも各 Service ID[hex] 記載と一致。

**総括**: Nm は43件中4件（内訳: 別APIの番号を誤引用1件、無関係な事象の番号を誤引用2件、
「デフォルト」の事実誤認1件）に実引用ミスがあり、修正後 `native`(62/62)・`native_chain`
(23/23) とも全パスを確認。

## 台帳: E2E (E2E_P01/E2E_P05) 仕様引用レビュー

**観点**: CRC/カウンタ計算アルゴリズム本体（E2EXf 層ではなく、実際に CRC8/CRC16 を
計算する `E2E_P01.c`/`E2E_P05.c`）が引用する18件の固有 `SWS_E2E_NNNNN` を
`AUTOSAR_SWS_E2ELibrary.pdf` の該当擬似コード・表と突き合わせ。既存の
`docs/E2E_Profile1_Notes.md`/`E2E_Profile5_Notes.md` とは別観点（コード側の引用の
正確性）での検証。**確認日**: 2026-08-15。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| E2E_P01.h | 18cdce9 | 問題なし。`E2E_P01StatusType` の8値（`SWS_E2E_00022`）は ERROR(0x80、本実装独自拡張と明記済み) を除く7値とも仕様の Range 表の値と完全一致。`SyncCounterInit`(00196相当)・標準バリアント1Aのレイアウト(00227)の引用も正確 |
| E2E_P01.c | 914c041 | 問題なし。CRC8 SAE-J1850 の多項式0x1D/開始値・XOR値0x00(00083)、Crc_CalculateCRC8のR4.0以降0xFF相殺トリックの説明(00070/00190)、カウンタ0〜14循環でmod-15補正が必要な理由(00075)、CRC計算範囲がDataID→Data[0..CRCOffset-1]→Data[CRCOffset+1..DataLength-1]である点(00082)、いずれも擬似コード・本文と一致 |
| E2E_P05.h | c022655 | 問題なし。CRC16多項式0x1021・開始値0xFFFF(00400/00406)、DataIDをデータの後に投入する配置(00399)、8bitカウンタ0-255循環で予約値なし、INITIAL/SYNC相当の状態が無くP01と異なり初回呼び出しも通常のdelta計算に乗る(00411-00416)、いずれも正確 |
| E2E_P05.c | 914c041 | **修正済み**: `E2E_CalcCrc16Body()` のドキュメントコメントが `SWS_E2E_00406` の擬似コード全体を実装しているかのように書かれていたが、実際には擬似コードが規定する `Config->Offset > 0` の分岐（ヘッダより前の `Data[0..Offset-1]` も CRC 計算に含める経路）を実装しておらず、常に `Offset==0` 側の経路のみを実装している。本プロジェクトの3用途（EngineHealthStatus/EngineInfo/AbsInfo、`E2EXf_PBCfg.c` で確認）はいずれも `Offset=0U` 固定のため実害はないが、この実装ギャップが未文書化だったため、関数コメントに `\note` として明記（挙動は変更していない） |

**検証**: `E2E_P01StatusType`(8値)・`E2E_P05StatusType`(6値)の数値がいずれも
`AUTOSAR_SWS_E2ELibrary.pdf` 8.2.1.2/8.2.4.4 章の Range 表と一致することを確認済み。

**総括**: E2E は18件中17件が正確、1件（P05 の Offset>0 分岐の未文書化ギャップ、現状は
到達不能のため実害なし）を文書化。修正後 `native`(62/62)・`native_chain`(23/23) とも
全パスを確認。

## 台帳: EcuM / Wdg 仕様引用レビュー

**観点**: 両モジュールとも固有引用が少数（EcuM 3件、Wdg 7件。前回の候補提示時の
grep カウントはショートハンド引用の見落としで実際より少なく出ていた）だったため、
ファイル全体を読んで全引用を `AUTOSAR_SWS_ECUStateManager.pdf`/
`AUTOSAR_SWS_WatchdogDriver.pdf` と突き合わせ。**確認日**: 2026-08-15。

| 対象ファイル | SHA | 結果 |
|---|---|---|
| EcuM.h | 694993d | 問題なし。`SWS_EcuM_04125`（RUN 要求はネストできない旨）・`SWS_EcuM_04127`（対応する要求のない解放は DET 相当を報告する旨、`EcuM_ReleaseRUN` の Service description block の Error Codes 節として正しく該当）とも正確。`EcuM_GetState()` は実 AUTOSAR EcuM に存在しない本プロジェクト独自の関数で、`\AUTOSARReq` タグ自体を付けておらず誤引用には該当しない |
| EcuM.c | 444b7a0 | 問題なし。上記2件の引用が実装側にも同一内容で反映されている |
| EcuM_Cfg.h | 694993d | 問題なし。`SWS_EcuM_04032`（Development Error コードの値は実装時に自由に割り当ててよい旨）の引用は正確。ApiId 全4件（Init=0x01/RequestRUN=0x03/ReleaseRUN=0x04/GetVersionInfo=0x00、Service ID[hex] 各記載と一致）も正確 |
| Wdg.h | 9bc33bb | 問題なし。`SWS_Wdg_00001`（`Wdg_Init` は本来デフォルトモード・タイムアウトを即座に有効化する要求）を、本プロジェクトが HW 有効化を `Wdg_SetMode` まで遅延させる意図的な仕様逸脱として正直に注記している（Can の wakeup コールバックと同種の「文書化した上での逸脱」）。`SWS_Wdg_00146`（timeout が最大値超過時 `WDG_E_PARAM_TIMEOUT` を報告）も逐語一致 |
| Wdg.c | 914c041 | 問題なし。該当する SWS 番号引用なし（ファイル冒頭の一般的な準拠表明のみ） |
| Wdg_Cfg.h | 9bc33bb | 問題なし。`SWS_Wdg_00010`（7.2.1 Development Errors 表）に基づく DET コード4件（`WDG_E_DRIVER_STATE=0x10`/`WDG_E_PARAM_MODE=0x11`/`WDG_E_PARAM_TIMEOUT=0x13`/`WDG_E_PARAM_POINTER=0x14`）は表と完全一致（未使用の`WDG_E_PARAM_CONFIG=0x12`/`WDG_E_INIT_FAILED=0x15`も含め正確）。ApiId 引用 `[SWS_Wdg_00106]/[00107]/[00155]/[00109]`（"SWS_Wdg_" 接頭辞を省略した括弧区切りのショートハンド表記）はいずれも該当関数（Init/SetMode/SetTriggerCondition/GetVersionInfo）の Service description block を指しており、Service ID[hex]（0x00/0x01/0x03/0x04）も ApiId 定義と一致 |
| Wdg_PBCfg.h / .c | 9bc33bb | 該当する SWS 番号引用なし |

**総括**: EcuM(3件)・Wdg(7件)とも誤引用ゼロ。Csm/CryIf/E2EXf/Gpt/Mcu/MemIf に続き、
「誤引用が見つからなかった」モジュールが2件追加。両モジュールとも `native`(62/62)・
`native_chain`(23/23) の再実行で回帰なしを確認（コード変更なし）。

## 台帳: IoHwAb 仕様引用レビュー

**経緯**: 初回レビュー時点では `AUTOSAR_SWS_IOHardwareAbstraction.pdf` が
`docs/autosar/4.3.1/` に存在せず検証不能だった。ユーザーが AUTOSAR 公式サイト
（`https://www.autosar.org/fileadmin/standards/R4.3.1/CP/AUTOSAR_SWS_IOHardwareAbstraction.pdf`、
文書内表記は Release 4.3.0 だが 4.3.1 として re-publish されたもの）から入手・配置し、
再検証を実施した。**確認日**: 2026-08-15。

| 対象ファイル | SHA（変更前） | 結果 |
|---|---|---|
| IoHwAb.h | 444b7a0 | **修正済み**: `IoHwAb_Init()` の `\ServiceID {0xC0}` は誤り。仕様の `[SWS_IoHwAb_00119]`（`IoHwAb_Init<Init_Id>` の Service description block）に明記された `Service ID[hex]: 0x01` に訂正。`\AUTOSARReq {SWS_IoHwAb_00119}` 自体（ConfigPtr を要求する根拠）は Syntax 節と逐語一致し正確だった |
| IoHwAb.c | 444b7a0 | **修正済み**: (1) `IoHwAb_Init()` 実装側の `\ServiceID` も同様に `0xC0`→`0x01` に訂正。(2) `SWS_IoHwAb_91001` という番号は仕様書に**存在しない**（7.6.1 Development Errors 章の「開発エラー分類は実装者に一任する("Up to the implementer")」という記述には、そもそも SWS 番号自体が付与されていない）。番号引用を削除し、章番号（7.6.1）を根拠として明記する記述に訂正 |

**検証**: `AUTOSAR_TR_BSWModuleList.pdf` で `ModuleId=254` が IoHwAb に正しく割り当てられて
いることを確認済み（この引用は元々正確だった）。`Led_SetLevel`/`Button_GetLevel`/
`Adc_GetValue_mV`/`MainFunction` 等のプロジェクト固有の個別 I/O 関数は、仕様上
「no prefix (AUTOSAR interface)」区分（信号ごとに ECU 側で自由に名付けるポート）に
相当し、標準化された Service ID が存在しないため `0xC1`〜`0xC6` の独自割り当ては
検証対象外として妥当。`IoHwAb_GetVersionInfo`（仕様上は `SWS_IoHwAb_00120`、
Service ID 0x10）を本プロジェクトが実装しない点は、過去のレビュー（2026-07-26、
GetVersionInfo/DeInit 未実装ギャップ監査。Det/IoHwAb/Adc_DeInit は意図的スキップ）で
既知・許容済みのため今回は再指摘しない。

**総括**: IoHwAb は2件中2件に誤りがあった（存在しない Service ID の自己流割り当て1件、
存在しない SWS 番号の創作1件）。今回レビューした中で最も誤引用密度が高いモジュール。
修正後 `native`(62/62)・`native_chain`(23/23) とも全パスを確認。

## 台帳: Adc / Det / Dio / Port 仕様引用レビュー

**観点**: 未レビューの最後の4モジュール。いずれも固有引用1件のみだったため、
ファイル全体を読んで `AUTOSAR_SWS_ADCDriver.pdf`/`AUTOSAR_SWS_DefaultErrorTracer.pdf`/
`AUTOSAR_SWS_DIODriver.pdf`/`AUTOSAR_SWS_PortDriver.pdf` と突き合わせ。**確認日**: 2026-08-15。

| 対象ファイル | SHA | 結果 |
|---|---|---|
| Adc.h | 444b7a0 | 問題なし。`SWS_Adc_00365`（`Adc_Init` の Service description block、Service ID[hex] 0x00）は逐語一致。`Adc_ReadChannel()` が実 `SWS_Adc`（`Adc_ReadGroup` 等のグループ・バッファベース API）に存在しない本プロジェクト独自関数である旨も、`Adc_ReadChannel`/`Adc_ReadGroup` を仕様書全文検索して確認（前者 0 件・後者多数件） |
| Adc_Cfg.h | 444b7a0 | 問題なし。`ADC_E_PARAM_POINTER=0x14` は一見「7.6.1 表に 0x0E と 0x14 の2つの ADC_E_PARAM_POINTER 行がある」ように読めたが、これは `pdftotext` の多段組テーブル誤読（0x0E 行は変更履歴に記載のある別コード `ADC_E_PARAM_CONFIG` の値であり、テーブル抽出時に名前列がずれて重複表示されていた）。`SWS_Adc_00457`（`Adc_SetupResultBuffer` の NULL チェック要求文）の単一カラムの平文記述で `ADC_E_PARAM_POINTER` の文脈を確認し、0x14 で正しいと確定（Can の DET テーブル誤読ニアミスと同種の事例。修正はしていない） |
| Det.h | 914c041 | 問題なし。`SWS_Det_00009`（`Det_ReportError` の Service description block）の Return value 記述「`Std_ReturnType` は値を返さないが、hook 等との互換性のためだけに戻り値型を持つ」は、本プロジェクトの「戻り値は互換性のためだけに存在し実際には使われない」という説明と完全一致 |
| Det.c | 444b7a0 | 問題なし。上記と同一引用が実装側にも正確に反映されている |
| Dio.h | 694993d | 問題なし。「実 SWS_Dio にも Dio_Init は存在しない」という断定コメントを `Dio_Init` で全文検索し 0 件を確認（既存の正しい記述） |
| Dio_Cfg.h | 694993d | **修正済み**: (1) `SWS_Dio_2078` という番号は仕様書に存在しない（実在する番号はすべて `SWS_Dio_00001`〜`00195` の5桁形式）。正しい引用は `SWS_Dio_00188`（DET エラーコード値表）/`SWS_Dio_00189`（`Dio_GetVersionInfo` の NULL チェック要求）に訂正。(2) `DIO_E_PARAM_POINTER` の値そのものが `0x14` になっていたが、`SWS_Dio_00188` の該当行は `DIO_E_PARAM_POINTER Value: [hex]: 0x20` であり、`0x14` は実際には別のエラーコード `DIO_E_PARAM_INVALID_PORT_ID` の値だった。`0x20U` に訂正（唯一の使用箇所である `Dio_GetVersionInfo()` の NULL チェックのみ、コミット後に SHA 更新要） |
| Dio.c | 914c041 | 問題なし。`Dio_GetVersionInfo`/`Dio_ReadChannel`/`Dio_WriteChannel` の Service ID（0x12/0x00/0x01）はいずれも仕様と一致 |
| Port.h | 444b7a0 | 問題なし。`SWS_Port_00140`（`Port_Init` の Service description block、Service ID[hex] 0x00）は逐語一致 |
| Port_Cfg.h | 694993d | 問題なし。`PORT_E_PARAM_POINTER=0x10`（7.2.1 Development Errors 表と一致）・`Port_GetVersionInfo` の Service ID 0x03 とも正確 |

**総括**: 4モジュール中3モジュール（Adc/Det/Port）は誤引用ゼロ、Dio のみ2件の実バグ
（存在しない SWS 番号の創作＋DET エラーコード値そのものの誤り）があった。特に Dio の
値バグは今回のドキュメント訂正主体のレビューの中で唯一「実際に報告される DET エラー値が
間違っていた」という機能面のバグ。修正後 `native`(62/62)・`native_chain`(23/23) とも
全パスを確認。これで2026-08-15セッションで候補提示していた未レビューモジュール
（Nm/E2E/EcuM/Wdg/IoHwAb/Adc/Det/Dio/Port）がすべて完了。

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

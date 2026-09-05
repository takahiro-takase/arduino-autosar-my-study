# Det

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

`DET_LOG*` マクロ経由でタイムスタンプ付きログを Serial に出力するデバッグ用ブリッジ。
加えて標準準拠の `Det_ReportError()`（[SWS_Det_00009]）も実装している。以下は
全 BSW モジュールへの展開経緯。

## DET 準拠（Det_ReportError による標準化エラー通知）

本プロジェクトの各 BSW モジュールの NULL チェック・範囲チェック・未初期化チェックは、
当初すべて `DET_LOGE(TAG, "自由文字列")`（`Det.h`、Serial 出力用の自作ロガー）のみで
報告していました。しかし AUTOSAR の各モジュール SWS は、開発エラー検出時に標準化された
`Det_ReportError()` 呼び出しを個別に要求しています。最初に対応した Com モジュールの
要求（`docs/AUTOSAR_SWS_COM.pdf` `[SWS_Com_00442]`、7.13 章 Error Notification）を
例に示すと次の通りです。

```
When a development error is detected, the function Det_ReportError of the
Default Error Tracer shall be called with:
     50 as the AUTOSAR COM's ModuleId
     0 as InstanceId
     the service ID of the AUTOSAR COM module's API in which the error was
         detected as ApiId
     the error ID as defined in Chapter 7.12.1 as ErrorId
```

自作ロガーの`DET_LOGE`はこの標準化された `Det_ReportError(ModuleId, InstanceId,
ApiId, ErrorId)` 呼び出し（`docs/AUTOSAR_SWS_DefaultErrorTracer.pdf`
`[SWS_Det_00009]`）とは全くの別物で、上位の診断ツール・DET フックから
機械可読な形で捕捉できる手段が存在していませんでした。この非適合は個々の
機能追加時には指摘してこなかった、プロジェクト全体に及ぶ体系的なギャップ
でした。

## 対応方針

情報を失わないよう、既存の `DET_LOGE(...)`（呼び出し元固有の自由文字列、
人間が読むための詳細情報）は**そのまま残し**、標準化された
`Det_ReportError(...)` を**並行して**呼ぶようにしました。

```c
if (config == NULL)
{
    DET_LOGE(TAG, "Init E: config NULL");                              // 既存
    Det_ReportError(COM_MODULE_ID, 0U, COM_API_ID_INIT, COM_E_PARAM_POINTER); // 追加
    return;
}
```

`Det_ReportError()` 自体は `Det.h`/`Det.c` に新規実装し、`DET_LOGE` とは
独立したチャネルとして `[<ms>ms] DET M=<ModuleId> I=<InstanceId> API=0x<ApiId>
ERR=0x<ErrorId>` という 1 行を出力します（本実装は実 AUTOSAR のコールアウト
フック登録・実行停止等の高度な機能は持たない学習用の簡略実装）。

エラーコードは該当 SWS PDF の章から検証した値をそのまま `<Module>_Cfg.h` に
定数化しています。例えば Com の場合は `docs/AUTOSAR_SWS_COM.pdf` 7.12.1 章から
検証した値を `Com_Cfg.h` に次のように定数化しています。

| エラーコード | 値 | 意味（要求番号） |
|---|---|---|
| `COM_E_PARAM` | 0x01 | API に不正なパラメータで呼ばれた（`[SWS_Com_00803]`） |
| `COM_E_UNINIT` | 0x02 | `Com_Init` 前 / `Com_Deinit` 後に呼ばれた（`[SWS_Com_00804]`） |
| `COM_E_PARAM_POINTER` | 0x03 | NULL ポインタチェック（`[SWS_Com_00805]`） |
| `COM_E_INIT_FAILED` | 0x04 | 不正なコンフィグセット選択（`[SWS_Com_00837]`） |

ApiId は各関数の Doxygen `\ServiceID` タグ（以前から記録されていたが実際の
エラー通知には反映されていなかった値）をそのまま定数化して使っています。
`Com_ConfigPtr == NULL || 他の条件` のように複数の異常系を 1 つの `if` で
まとめていた箇所（`Com_RxIndication`/`Com_ReceiveSignal`/`Com_SendSignal`/
`Com_TxConfirmation`）は、正しいエラーコードを区別して報告するために条件を
分割しました（`Com_TxConfirmation` の `result != E_OK` のような「異常では
ない早期 return」までエラー報告してしまわないよう、原因ごとに分けています）。

## 対応範囲：全 BSW モジュールへの展開

当初は Com モジュールのみの対応でしたが、その後全 BSW モジュール
（Can, CanIf, PduR, CanTp, Dcm, Dem, FiM, NvM, EcuM, BswM, WdgM, WdgIf, Wdg,
ComM, CanSM, Nm, IoHwAb, Dio, Port, Adc, SecOC, E2E, E2EXf, Fee, MemIf の
25 モジュール）へ同じ方針で展開しました。DET_LOGE で既に報告されていた
箇所（NULL/範囲/未登録チェック）に加え、ログを一切出していなかった暗黙の
NULL/未初期化チェックにも同じ基準で追加しています。

**ModuleId の出典**: Com 以外のほとんどのモジュールは SWS 本文に
ModuleId の固定値が明記されていません（Com の `[SWS_Com_00442]` は
例外的な明記）。そのため `docs/AUTOSAR_TR_BSWModuleList.pdf`
（Release 4.3.1、「List of Basic Software Modules」表）から各モジュールの
ModuleId を検証・転記しています。この値は Com=50 という既知の値と
独立に一致したため、出典として信頼できることを確認済みです。各モジュールの
ModuleId は README の「[モジュール一覧](../../README.md#module-list)」表を
参照してください。うち EcuM（10）はエラーコード値自体が SWS 標準で未固定
（`[SWS_EcuM_04032]`）のため本実装で独自に割り当てたもの、IoHwAb（254）は
エラー分類自体が実装者定義（`SWS_IoHwAb_91001`）であることに注意してください。

**対象外・チェック追加なしと判断したモジュール**:

- **E2E（`src/Bsw/E2E/E2E_P01.c`）**: `docs/AUTOSAR_SWS_E2ELibrary.pdf`
  `[SWS_E2E_00216]` が「E2E Library は DET/DEM/RTE を一切呼び出しては
  ならない」と明記しており、エラーは戻り値（`E2E_P01STATUS_*`）のみで
  呼び出し元（E2EXf）に伝達する設計です。Det_ReportError の追加は仕様
  違反になるため行っていません。
- **Dio / Port**: `Dio_ChannelType`/`Port_PinType` は本プロジェクトでは
  Arduino の生ピン番号をそのまま渡す簡略設計で、設定済みチャネル一覧との
  照合テーブルを持ちません。両モジュールともポインタ引数もないため、
  AUTOSAR が定義する `DIO_E_PARAM_INVALID_CHANNEL_ID` 等のエラーは
  本実装の設計上そもそも検出対象が存在せず、追加していません
  （ServiceID タグの誤りのみ修正）。

**非標準の独自拡張関数（DET 対象外）**: `CanSM_RxIndication` /
`WdgM_EnableHwWatchdog` /
`WdgM_DisableHwWatchdog` / `WdgM_ResumeSupervision` / `Dcm_ComIndication` /
`Adc_ReadChannel` はいずれも実際の AUTOSAR SWS には存在しない、本プロジェクト
独自の簡略化・拡張関数のため、DET のエラーコード・ApiId は個々の実 SWS 関数
とは対応付けていません（ServiceID は既存の非標準値をそのまま踏襲。ただし
`CanSM_RxIndication` は2026-09-05に0x07→0x15へ変更、CanSM_Cfg.hのコメント参照）。
`CanSM_ControllerWakeup`は2026-09-05に実仕様の`CanSM_ControllerModeIndication`
（[SWS_CanSM_00396]、ControllerMode引数を追加）へ改名・是正したため、この
一覧からは除外した。同じく`CanIf_ControllerWakeup`（実仕様に存在しない独自
API だった）は2026-09-05に廃止し、実仕様の`EcuM_CheckWakeup`
（[SWS_Can_00271]/[SWS_EcuM_02929]、ServiceID 0x42）へ役割を引き継がせたため
この一覧から除外した。

**副次的に発見・修正した ServiceID タグの誤り**: 対応作業中、各モジュールの
実 SWS「Service ID[hex]」記載と実測照合したところ、20 件以上の Doxygen
`\ServiceID` タグの誤りを発見し、あわせて修正しました（例:
`PduR_Init` 0x00→0xF0、`BswM_EcuM_CurrentState`/`BswM_ComM_CurrentMode` が
互いに入れ替わっていた、`NvM_*` 系はほぼ全関数が誤り、等）。これらは
Det_ReportError の ApiId 引数に直接使う値のため、誤ったまま報告していると
診断ツール側で API を取り違える実害がありました。

## 検証方法について

`Det_ReportError()` が呼ばれるのは NULL ポインタ・範囲外 ID・未初期化状態と
いった、正常な CAN 通信では到達しない開発時の異常系のみです。したがって
UDS/CAN フレーム経由で外部から誘発することはできず、`uds_tester` 等での
実機確認の対象にはなりません（各モジュールについて `pio run -e uno_r4` の
ビルド成功と、エラーコード・ModuleId の割り当てが対応する SWS PDF の原文と
一致していることの確認に留めています）。

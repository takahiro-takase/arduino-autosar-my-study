# FiM（機能抑止マネージャ）

> [README](../../README.md) の「[診断スタック](../../README.md#diag-stack)」節から分離。

FiM (Function Inhibition Manager) は、Dem が確定（CONFIRMED）した DTC を根拠に、
関連するアプリ機能の実行を抑止するルールエンジンです。
「DTC を記録する」（Dem の責務）と「DTC を理由に機能を止める」（FiM の責務）を
分離するのが AUTOSAR の設計思想で、ASW は Dem の内部実装を一切知らずに
`Rte_Call_FiM_GetFunctionPermission()` だけで「この機能は今実行してよいか」を判定できます。
FID↔イベント対応表・判定フロー・フェールセーフ既定値の詳細を以下にまとめます。

## 機能 ID (FID) とイベントの対応

| FID | 機能 | 抑止条件 | 抑止時の挙動 |
|---|---|---|---|
| `FIM_FID_RUNNING_LED` | RUNNING LED (D6) の点灯 | `DemConf_DemEventParameter_CAN_BUSOFF` が CONFIRMED | D6 を強制消灯（EngineState は CAN 受信由来のため、Bus-Off 確定中は信頼できない） |
| `FIM_FID_BUTTON_ACK` | 警告確認ボタンによる FAULT 解除 | `DemConf_DemEventParameter_BUTTON_STUCK` が CONFIRMED | ボタン押下を無視（固着確定中の押下は物理的固着による偽信号の可能性がある） |

対応表は `FiM_PBCfg.c` の `FiM_Functions[]`（抑止の条件の配列。`config/data/FiM.json` から生成）で定義する
（AUTOSAR の `FiMInhibitionConfiguration` コンテナに相当）。1 行が 1 つの条件（FID・Dem イベント・抑止するステータスのビット）で、
新しい FID を追加する場合は、`FiM.json` の `functions[]` に 1 件足すだけで済む。

### 1 つの FID に複数の条件（2026-10 追加）

`FiM.json` の機能ごとの `inhibitions[]` に、条件を複数書くと、同じ FID の行が複数になる。**どれか 1 つの条件が成り立てば、
その FID を抑止し**、1 つも成り立たなければ許可する（OR、[SWS_Fim_00064]・仕様 7.2.2.4）。以前は、1 つの FID に、1 つのイベントの
行しか持てず、同じ FID の行を並べても、最後の行の結果だけが、FID の状態になっていた。`FIM_FUNCTION_COUNT` は FID の数、
`FIM_INHIBITION_COUNT` は条件（行）の数で、`FiM_ConfigType` の `FunctionCount`・`InhibitionCount` に対応する。
簡略化: 実仕様の `FiMSummaryEvent`（複数のイベントをまとめた名前）は持たず、同じ FID に、イベントごとの行を並べる。

### 抑止の条件の表し方（仕様との違い）

実仕様の `FiMInhInhibitionMask` は、Dem のイベントステータスに対する4種類の列挙（`FIM_LAST_FAILED`: TF=1、`FIM_NOT_TESTED`:
TNCTOC=1、`FIM_TESTED`: TNCTOC=0、`FIM_TESTED_AND_FAILED`: TF=1 かつ TNCTOC=0）です。本プロジェクトは、Dem のステータスバイトに対する
ビットマスク（`InhibitStatusMask`）で表し、**いずれか 1 ビットでも立っていれば**成り立つ、とします。TF・TNCTOC のビットに加えて、
DTC の確定（`DEM_STATUS_CONFIRMED`）も使えるので、実際の設定は、確定した DTC で機能を抑止する形にしています（単発の異常で
抑止しないため）。ビットが「立っていないこと」を条件にする `FIM_TESTED` と、2 ビットが同時に成り立つ `FIM_TESTED_AND_FAILED` は、
表せません（現在の設定では不要）。

## 判定の流れ

```
FiM_MainFunction（100 ms 周期、Os Task 10）:
  全 FID を「許可」にしてから、FiM_Functions[]（抑止の条件）を先頭から走査:
    status = Dem_GetEventUdsStatus(EventId)
    (status & InhibitStatusMask) != 0 ?
      YES → 該当 FID を「抑止」（同じ FID の他の行が許可でも、抑止のまま）
      NO  → その行では、何もしない
    （許可状態が変化した瞬間にのみログ出力）

ASW (App_WarningIndicator_Run / App_EngineManager_Run):
  Rte_Call_FiM_GetFunctionPermission(FID, &permission)
  permission == FALSE (抑止) なら、当該機能の実行を見送る
```

FiM は Dem の状態だけを参照し、ASW は FiM の判定結果だけを参照します。
ASW が Dem を直接参照しないことで、「どの DTC が確定したら何を止めるか」という
ルールを FiM 側に閉じ込め、ASW のロジックを単純に保てます。

## 利用可否の強制設定 (FiM_SetFunctionAvailable)

`FiM_SetFunctionAvailable(FID, Availability)`（[SWS_Fim_00106]）は、上記の
Dem ベースの判定とは独立に、FID の利用可否を外部から強制設定する API です。
`Availability=FALSE`（利用不可）に設定した FID は、Dem のイベントステータスが
どうであれ `FiM_GetFunctionPermission()` が常に「抑止」を返します
（[SWS_Fim_00105]）。実装は判定フローそのものを変えず、読み出し側で
両条件の論理積を取るだけです:

```
FiM_GetFunctionPermission(FID):
  Available[FID] == FALSE ?
    YES → 常に「抑止」（Permitted[FID] の値に関わらず）
    NO  → Permitted[FID]（上記 Dem ベースの判定結果）をそのまま返す
```

実仕様は `FiMAvailabilitySupport` が configured=True の場合のみ有効な
任意サービスだが、本プロジェクトはそのようなビルド時コンフィグ切替を
持たないため常に有効とする（学習用簡略化）。2026-09 時点では
`Rte_Call_FiM_SetFunctionAvailable()` のような ASW 向けラッパー・呼び出し元は
まだ存在せず、API 自体は未配線（本サーベイ系統の他の多くの新規 API と同様）。

## ログ例

```
# CAN Bus-Off が確定（3 回のリトライ断念）→ RUNNING LED が抑止される
[30313ms] WARN  Dem: FAILED ev=7 dtc=0x000108
[30400ms] WARN  FiM: FID0 inhibited (ev=7)
[30900ms] INFO  WarnInd: [RUN:0 FAULT:0 ABS:0]   # state=RUNNING でも D6 は消灯のまま

# UDS 0x14 で全 DTC クリア → 抑止解除
[31000ms] INFO  Dcm: 14 ClearDTC dtc=0xFFFFFF
[31100ms] INFO  FiM: FID0 permitted again
[31600ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:0]   # state=RUNNING なら D6 が再点灯

# 警告確認ボタンが 5 秒以上押されたまま固着確定 → FAULT 解除ボタンが無効化
[40000ms] WARN  IoHwAb: Button stuck dtc=0x000106
[40100ms] WARN  FiM: FID1 inhibited (ev=5)
[40500ms] WARN  AppEng: FAULT->OFF btn=1 inhibited (FiM)   # 押下を受理しない
```

## 呼び出し側（ASW）のフェールセーフ既定値

`FiM_GetFunctionPermission()` 自体は、FID が不正・FiM 未初期化などで判定できない
場合に `Permission` を安全側（FALSE = 抑止）にしてから `E_NOT_OK` を返す契約に
なっています（`FiM.h` 参照）。

ASW 側（`App_EngineManager_Run` / `App_WarningIndicator_Run`）の呼び出しも、
この契約に依存しきらず、呼び出し前のローカル変数の既定値そのものを
`FALSE`（抑止）にし、戻り値が `E_NOT_OK` の場合も明示的に `FALSE` へ上書き
しています。

```c
boolean ackPermitted = FALSE;  /* 既定値は「抑止」(許可ではない) */
if (Rte_Call_FiM_GetFunctionPermission(FIM_FID_BUTTON_ACK, &ackPermitted) != E_OK)
{
    ackPermitted = FALSE;
}
```

呼び出し先の実装が将来変わって `Permission` を書き込まない失敗経路が増えても、
呼び出し側のローカルな既定値だけで安全側に倒れる（fail-safe）ようにする狙いです。
「許可判定が確認できないときは許可しない」という既定値の選び方は、
セキュリティ・機能安全の定石（fail-safe defaults）そのものです。

# BswM（BSW モードマネージャ）

> [README](../../README.md) の「[ECU 管理層](../../README.md#ecu-management)」節から分離。
> なお「CAN コントローラの実スリープ」「ボランタリスリープとウェイクアップ」の2節は
> 実質的に Can/CanSM/Nm の解説であるため、README側にそのまま残しています。

BswM (BSW Mode Manager) は、EcuM や ComM からのモード変化通知を受け取り、
ルールテーブルに従って Os タスクの有効・無効を切り替えるルールエンジンです。
ルールテーブル・タスク ID/マスク・POST_RUN/SHUTDOWN でのタスク継続理由・
通知チェーン・設定変更方法の詳細を以下にまとめます。

EcuM が「今どのフェーズか」を決めるのに対し、BswM は「そのフェーズで何をするか」を決めます。
この責任分離により、フェーズごとの振る舞いをコードを書かずにルールテーブルの変更だけで調整できます。

## ルールテーブル（`BswM_PBCfg.c`）

| No | 条件（Operator） | アクション | 対象 |
|----|------------------|-----------|------|
| 0 | EcuM==RUN | ACTIVATE | 全タスク（`BSWM_TASK_MASK_ALL`） |
| 1 | EcuM==POST_RUN | DEACTIVATE | アプリタスクのみ（`BSWM_TASK_MASK_APP`） |
| 2 | EcuM==SHUTDOWN | DEACTIVATE | `BSWM_TASK_MASK_SHUTDOWN`（WdgM_TriggerHwWatchdog・Can_MainFunction_Read・Can_MainFunction_Wakeup・CanSM_MainFunction・NvM_MainFunction・MemIf_MainFunction・Nm_MainFunction を除く） |
| 3 | **EcuM==RUN `AND` ComM==FULL_COMMUNICATION** | PDU_GROUP_START | I-PDU Group「テレメトリ」(E2EHealthStatus) |
| 4 | EcuM==POST_RUN | PDU_GROUP_STOP | I-PDU Group「テレメトリ」 |
| 5 | **ComM==SILENT_COMMUNICATION `OR` ComM==NO_COMMUNICATION** | PDU_GROUP_STOP | I-PDU Group「テレメトリ」 |

Rule 3/5 が複合条件（`BswM_ConditionType` の配列を `BswM_LogicalOperatorType`
(`BSWM_OP_AND`/`BSWM_OP_OR`) で組み合わせる、[SWS_BswM_00808]
BswMLogicalExpression の簡略版）を使う唯一の例です。以前は単一条件ルール
しか組めない設計（AND/OR の LogicalExpression 相当が未実装）でしたが、
Nm（CanNm 状態機械）導入により ComM のチャネルモードが EcuM の RUN/POST_RUN
とは独立に変化しうるようになったため、「EcuM が RUN でも CAN チャネルが
実際には使えない（Bus-Off 中の SILENT_COMMUNICATION 等）」場合を正しく
扱うために複合条件対応を追加しました。Rule 3（AND、開始条件）と Rule 5
（OR、停止条件）が対になっており、CAN チャネルが FULL_COMMUNICATION から
離脱した瞬間（原因が Bus-Off でもボランタリスリープでも）に確実にテレメトリ
送信を止めます。

各ルールの Action は、条件の評価結果が **false→true へ遷移したときのみ**
実行されます（`BswM_RuleLastResult[]` で直近の結果をキャッシュし、
true が続く間の重複実行や true→false への遷移では実行しません）。複合条件
ルールは、どちらの条件（ソース）が最後に変化しても正しく発火するよう、
`BswM_ModeSrcCache[]` に両ソースの最新値を保持したうえで評価します
（詳細は `BswM.c` の `BswM_ExecuteRules()`/`BswM_EvaluateRule()` 参照）。

対応除外（実 AUTOSAR の BswMLogicalExpression と比べた簡略化）: NAND/NOT/XOR
演算子、3条件以上、LogicalExpression の入れ子（木構造）。本プロジェクトの
モードソースは EcuM/ComM の2つのみで、これらを使うルールの実績もないため
対応除外としています。

## タスク ID とマスク（`BswM_Cfg.h`）

タスク数が 16 を超えるため、`TaskMask` は uint32（bits 0〜18）です。

| タスク ID | 定数 | 対応関数 | 周期 |
|---------|------|---------|------|
| 0 | `BSWM_OS_TASK_CAN_READ` | `Can_MainFunction_Read` | 1 ms |
| 1 | `BSWM_OS_TASK_CANTP_MAIN` | `CanTp_MainFunction` | 1 ms |
| 2 | `BSWM_OS_TASK_RTE_ENGINE` | `Rte_ScheduleRunnables` | 3000 ms |
| 3 | `BSWM_OS_TASK_RTE_WARNING` | `Rte_ScheduleWarningIndicator` | 500 ms |
| 4 | `BSWM_OS_TASK_CANSM_MAIN` | `CanSM_MainFunction` | 10 ms |
| 5 | `BSWM_OS_TASK_COM_MAIN` | `Com_MainFunctionRx` | 100 ms |
| 6 | `BSWM_OS_TASK_COM_MAIN_TX` | `Com_MainFunctionTx` | 100 ms |
| 7 | `BSWM_OS_TASK_IOHWAB_MAIN` | `IoHwAb_MainFunction` | 10 ms |
| 8 | `BSWM_OS_TASK_WDGM_MAIN` | `WdgM_MainFunction` | 6000 ms |
| 9 | `BSWM_OS_TASK_DCM_MAIN` | `Dcm_MainFunction` | 1000 ms |
| 10 | `BSWM_OS_TASK_FIM_MAIN` | `FiM_MainFunction` | 100 ms |
| 11 | `BSWM_OS_TASK_WDGM_TRIGGER` | `WdgM_TriggerHwWatchdog` | 1000 ms |
| 12 | `BSWM_OS_TASK_NM_MAIN` | `Nm_MainFunction` | 1000 ms |
| 13 | `BSWM_OS_TASK_NVM_MAIN` | `NvM_MainFunction` | 10 ms |
| 14 | `BSWM_OS_TASK_CAN_TX_CONF` | `Can_MainFunction_Write` | 1 ms |
| 15 | `BSWM_OS_TASK_CAN_BUSOFF` | `Can_MainFunction_BusOff` | 1 ms |
| 16 | `BSWM_OS_TASK_CAN_WAKEUP` | `Can_MainFunction_Wakeup` | 1 ms |
| 17 | `BSWM_OS_TASK_SECOC_MAIN` | `SecOC_MainFunctionTx` | 100 ms |
| 18 | `BSWM_OS_TASK_MEMIF_MAIN` | `MemIf_MainFunction` | 10 ms |
| 19 | (マスク対象外) | `App_GptDemo_Run` | 2000 ms |
| 20 | (マスク対象外) | `ComM_MainFunction` | 100 ms |

Task 6（`Com_MainFunctionTx`）は 2026-08、単体だった `Com_MainFunction` を
実仕様準拠の `Com_MainFunctionRx`/`Com_MainFunctionTx` へ分割した際に追加。
Task 17（`SecOC_MainFunctionTx`）が同一 `Os_SchedulerStep()` パス内で Com の
ディスパッチ結果を同じティックで拾えるよう、末尾ではなく Task 5 の直後へ
挿入し、Task 7 以降を 1 つずつ後ろへずらした（`/code-review` で「末尾に
追加すると 1 ティック分の遅延が生じる」と指摘され是正。詳細は
`Os_PBCfg.c`/`BswM_Cfg.h` 参照）。

`BSWM_TASK_MASK_APP = 0x00C`（bit2=Rte_Engine, bit3=Rte_Warning）がアプリタスクマスクです。
POST_RUN ではこの 2 タスクだけを停止し、BSW タスク（Can_MainFunction_Read/BusOff/Wakeup・CanTp・CanSM・Com・IoHwAb・WdgM・Dcm・FiM・WdgM_TriggerHwWatchdog・Nm・NvM・MemIf・SecOC・Can_MainFunction_Write）は継続させます。
Dcm を継続させることで、POST_RUN 中も S3 タイマ監視（セッションの自動失効）が動作し続けます。
Nm は POST_RUN 中も動き続けますが、POST_RUN へ遷移する経路（エンジン OFF 継続による
ボランタリスリープ突入。Bus-Off は L1/L2 バックオフで無期限に回復を試みるため
POST_RUN 遷移の原因にはならない）では ComM は既に NO_COM になっているため、
実際には送信を行いません。

`BSWM_TASK_MASK_SHUTDOWN = 0x163EE`（`BSWM_TASK_MASK_ALL` から bit10=WdgM_TriggerHwWatchdog・
bit0=Can_MainFunction_Read・bit15=Can_MainFunction_Wakeup・bit4=CanSM_MainFunction・
bit12=NvM_MainFunction・bit17=MemIf_MainFunction・bit11=Nm_MainFunction を除いたもの）が
SHUTDOWN 時の無効化対象マスクです。SecOC_MainFunctionTx（bit16）はこの除外リストに
含まれないため（POST_RUN 中に Com_MainFunctionTx が止まり SecOC の送信要求自体が
発生しなくなるのと同じ理由で、無効化しておくのが本来の設計意図）、
Can_MainFunction_Write（bit13）・Can_MainFunction_BusOff（bit14）と同様に
SHUTDOWN 中は停止します。BusOff ポーリングは `CanState==CAN_CS_STARTED` が条件のため
SHUTDOWN 中（SLEEP か Listen-Only）はどのみち無意味であり、TX 確認も SHUTDOWN 中は
新規送信が発生しないため停止して問題ありません（詳細は [`Can_Notes.md`](./Can_Notes.md) の
「TX 確認の非同期化」参照）。
WdgM_TriggerHwWatchdog は、Renesas RA の IWDT が一度有効化すると無効化する手段がないため、
SHUTDOWN 後も動かし続けて `WdgM_SupervisionSuppressed` により無条件にリフレッシュを継続する
必要があります（詳細は [`WdgM_Notes.md`](./WdgM_Notes.md) の「HW ウォッチドッグ連携」を参照）。
Can_MainFunction_Read・Can_MainFunction_Wakeup は、CAN バスのボランタリスリープからの
ウェイクアップ検出（Wakeup）、およびウェイクアップ検証中に届く診断フレームの受信処理（Read）
のために SHUTDOWN 後も動かし続けます（`Can_Isr()` は BswM の無効化に関わらず常に起動する
真のハードウェア割り込みだが、正しさをこの割り込みの成否だけに委ねない設計にしているため、
実際の SPI 読み出しと上位層への通知を担うこの 2 タスク自体を無効化するわけにはいかない。
詳細は [`Can_Notes.md`](./Can_Notes.md) の「RX の割り込み化」を参照）。CanSM_MainFunction は、ウェイクアップ検証（README の「CAN コントローラの実スリープ」参照）の検証タイムアウトを
監視するために SHUTDOWN 後も動かし続けます。
NvM_MainFunction は、SHUTDOWN 直前に Dem が新規 DTC を確定して書き込みジョブが保留中の
まま残る可能性があるため、SHUTDOWN 後も動かし続けて永続化を完了させます。
MemIf_MainFunction も同じ理由で動かし続ける必要があります。NvM_MainFunction は
MemIf_Write() でジョブを「開始」するだけで、実際の物理バイト書き込みを 1 バイトずつ
進めるのは MemIf_MainFunction（実体は Fee_MainFunction）だからです。NvM_MainFunction
だけを動かして MemIf_MainFunction を止めてしまうと、ジョブが開始されたまま永久に
`MEMIF_JOB_PENDING` を待ち続け、EEPROM への永続化が完了しません
（詳細は [`NvM_Notes.md`](./NvM_Notes.md) の非同期書き込みジョブキュー参照）。
この 7 タスクの存在により、SHUTDOWN は HW ウォッチドッグを維持しつつ CAN バス活動
（ボランタリスリープからのウェイクアップ）で常に RUN へ復帰できる状態になっています。

## POST_RUN でアプリタスクのみ停止する理由

POST_RUN 中も BSW タスクを動かし続けることで、以下のグレースフルシャットダウンが実現されます。

```
POST_RUN 中も動き続けるタスク:
  Can_MainFunction_Read / CanTp_Main → 受信中の診断フレームを最後まで処理
  CanSM_Main          → 回復シーケンスの完了まで管理
  Com_MainFunctionRx/Tx → 受信デッドライン監視の最終確認・送信スケジューリング
  IoHwAb_Main        → ボタンのデバウンス状態を正常終了
  WdgM_Main          → Alive Supervision のソフト評価は継続するが判定結果は無視される
                        （WdgM_SupervisionSuppressed が立っているため）
  WdgM_TriggerHwWatchdog → HW ウォッチドッグのリフレッシュは継続（判定結果を無視して
                        無条件にリフレッシュするため、実際のリセットは発生しない）
  Dcm_Main           → S3 タイマ監視を継続（拡張セッションも正しく失効する）
  Nm_MainFunction    → タスク自体は動き続けるが、ComM が既に NO_COM のため送信しない

POST_RUN 中に停止するタスク:
  Rte_ScheduleRunnables          → エンジン状態更新・DTC 登録を停止
  Rte_ScheduleWarningIndicator   → LED 制御を停止（消灯状態で固定）
```

## 通知チェーン

```
ボランタリスリープ突入（エンジン OFF 継続）
  CanSM: CANSM_STATE_NO_COM へ遷移（この時点ではまだ物理スリープしない）
       → ComM_BusSM_ModeIndication(NO_COM)
            ├→ EcuM_ReleaseRUN(ECUM_USER_COMM)
            │     └→ EcuM: RUN → POST_RUN
            │               └→ BswM_EcuM_CurrentState(POST_RUN)
            │                     └→ Rule 1 発火: Os_SetTaskActive(Rte_Engine, OFF)
            │                                     Os_SetTaskActive(Rte_Warning, OFF)
            ├→ BswM_ComM_CurrentMode(0, NO_COM)
            │     └→ Rule 5 発火（OR 条件、ComM==NO_COM）:
            │           Com_IpduGroupStop(テレメトリ)  ← ComM==FULL_COM 前提の
            │           Rule 3 (AND条件) が既に false になっているため
            │           冪等（テレメトリが既に停止済みなら何もしない）
            └→ Nm_NetworkRelease()
                  → Nm: Normal Operation → Ready Sleep State（送信停止）
                  → NM-Timeout Timer 満了 → Prepare Bus-Sleep Mode
                  → Wait-Bus-Sleep Timer 満了（他ノードからの NM フレーム受信が
                    なければ）→ Bus-Sleep Mode へ到達
                        → CanSM_NmBusSleepMode()
                              → Can_SetControllerMode(CAN_T_SLEEP)  ← ここで初めて
                                MCP2515 を実際にスリープさせる（詳細は
                                [`Nm_Notes.md`](./Nm_Notes.md) 参照）

POST_RUN 5秒後
  EcuM: POST_RUN → SHUTDOWN
    └→ BswM_EcuM_CurrentState(SHUTDOWN)
          └→ Rule 2 発火: Os_SetTaskActive(WdgM_TriggerHwWatchdog / Can_MainFunction_Read /
                                          Can_MainFunction_Wakeup / CanSM_MainFunction /
                                          NvM_MainFunction / MemIf_MainFunction /
                                          Nm_MainFunction 以外, OFF)
                          （この 7 タスクだけは HW ウォッチドッグ維持 / CAN ウェイクアップ検出・
                            検証中フレーム処理 / ウェイクアップ検証タイムアウト監視 /
                            DTC永続化（ジョブ開始は NvM_MainFunction、物理バイト書き込みの
                            進行は MemIf_MainFunction） / Nm状態機械継続のため動き続ける。特に
                            Nm_MainFunction を止めてしまうと Nm が二度と Bus-Sleep Mode へ
                            到達できず、CAN コントローラが永久に物理スリープしなくなる不具合が
                            あったため SHUTDOWN 中も継続するよう変更した）
```

CAN コントローラの実スリープ処理・ボランタリスリープ〜ウェイクアップ検証の詳細シーケンスは
README の「CAN コントローラの実スリープ」「ボランタリスリープとウェイクアップ」節を参照
してください（Can/CanSM/Nm 横断のため README 側に残しています）。

## BswM 設定の変更方法

| 変更内容 | 編集ファイル |
|---------|------------|
| POST_RUN で停止するタスクの追加・変更 | `BswM_Cfg.h` の `BSWM_TASK_MASK_APP` |
| ルール追加（例: ComM モードに反応する） | `BswM_PBCfg.c` にルールを追記し `BSWM_RULE_COUNT` を更新 |
| タスク追加 | `BswM_Cfg.h` に ID 定数を追加し `Os_PBCfg.c` にも追記 |

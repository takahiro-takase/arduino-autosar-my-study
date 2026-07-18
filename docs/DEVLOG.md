# 開発ログ（実機で見つかった不具合・設計変更の経緯）

このファイルは [README.md](../README.md) から分離した「過去の経緯」の記録です。
README は現在の仕様・実装を理解するためのドキュメントとして保守し、
「なぜ今の設計になったか」「実機でどんな不具合を踏んだか」という時系列の経緯は
すべてこちらにまとめています。README の各セクションから該当箇所へのリンクを
張っているので、興味がある場合だけ読んでください。

もくじ:
- [Can: RX 割り込み化の実機検証で得られた教訓](#can-rx-割り込み化の実機検証で得られた教訓)
- [Com: CommunicationControl 実装時の仕様不整合](#com-communicationcontrol-実装時の仕様不整合)
- [Dem: デバウンスカウンタの反転バグ](#dem-デバウンスカウンタの反転バグ)
- [Dem: デバウンス閾値を単一値からイベントごとに変更した経緯](#dem-デバウンス閾値を単一値からイベントごとに変更した経緯)
- [ComM: ウェイクアップ直後の再集約による即座の再スリープ](#comm-ウェイクアップ直後の再集約による即座の再スリープ)
- [CanSM: Bus-Off 回復断念設計の撤去](#cansm-bus-off-回復断念設計の撤去)
- [NvM: 非同期書き込みジョブキューへの変更経緯](#nvm-非同期書き込みジョブキューへの変更経緯)
- [WdgM: Deadline Supervision 上限緩和と Os_SchedulerStep のバグ](#wdgm-deadline-supervision-上限緩和と-os_schedulerstep-のバグ)
- [WdgM: グローバル EXPIRED 許容サイクルの追加](#wdgm-グローバル-expired-許容サイクルの追加)
- [WdgM: グローバル猶予カウンタを resume でリセットしてはいけなかった](#wdgm-グローバル猶予カウンタを-resume-でリセットしてはいけなかった)
- [WdgM: POST_RUN→RUN 復帰時の Deadline Supervision 誤検出](#wdgm-post_runrun-復帰時の-deadline-supervision-誤検出)
- [WdgM: POST_RUN→RUN 復帰時の Alive Supervision 誤検出](#wdgm-post_runrun-復帰時の-alive-supervision-誤検出)
- [WdgM: 短時間 POST_RUN での Alive Supervision 誤検出](#wdgm-短時間-post_run-での-alive-supervision-誤検出)
- [WdgM: Alive と Logical のステータス統合バグ](#wdgm-alive-と-logical-のステータス統合バグ)

---

## Can: RX 割り込み化の実機検証で得られた教訓

`Can_Isr()` を真のハードウェア割り込みに変更した際の実機検証初回テストで、
`Can_Isr()` が一度も起動されていないように見える現象が発生した（診断用に
一時的に追加したカウンタが長時間 0 のまま）。後日の再テストでは同じカウンタが
正常にインクリメントしており、割り込み自体は機能することを確認した。初回テストで
0 のままだった直接の原因は特定できていない（その時点でバス上に実際のフレームが
流れていなかった可能性が高い）。

この経緯を踏まえ、`Can_MainFunction_Read()`/`Can_MainFunction_Wakeup()` は
「割り込みが本当に発火するか」に正しさを依存させない設計とした:

- `Can_MainFunction_Read()` は `Can_RxIrqPending` の有無に関わらず、毎回
  無条件に `Can_Hw_CheckReceive()`（SPI 経由のステータスレジスタ読み出し。
  INT ピンの実際の状態には依存しない）でドレインする。
- `Can_MainFunction_Wakeup()` は `Can_WakeupIrqPending` に加えて
  `digitalRead(intPin)` の直接ポーリングも併用する（旧実装と同じフォールバック）。

`Can_Isr()`・ペンディングフラグ・`SchM_Enter/Exit_Can_IRQFLAG_EXCLUSIVE_AREA()`
の構造はそのまま残り、割り込みが発火すればより低遅延に反応できる「ボーナス経路」
として機能するが、たとえ割り込みが何らかの理由で発火しなくてもポーリング側だけで
正しく動作することが実機で確認できている。単一の検出経路（割り込みのみ）に
正しさを委ねず、独立したポーリングでも動作を保証する設計にしたこと自体が、
実機検証を通じて得られた教訓である。

（README 該当箇所: [CAN 通信スタック > RX の割り込み化](../README.md#rx-の割り込み化can_isr--can_mainfunction_readbusoffwakeup)）

---

## Com: CommunicationControl 実装時の仕様不整合

UDS CommunicationControl (SID 0x28) の実装時、2 つの仕様不整合が見つかった。

**Rx 無効中の受信デッドライン監視**: 当初、`Com_MainFunction()`（受信デッドライン
監視、100ms 周期）は `Com_RxEnabled` を一切参照していなかった。SWS_Com_00684/
SWS_Com_00685（`Com_IpduGroupStop` により I-PDU が止められた間は受信処理だけでなく
デッドライン監視自体も無効化することを要求）に反しており、意図的に
CommunicationControl で受信を止めているだけなのに、`TimeoutMs` を超えて
無効化し続けると `Com_RxTimedOut` が誤って立ち、上位層（RTE/ASW）へ
「通信異常」として伝わってしまっていた。`Com_MainFunction()` は
`Com_RxEnabled==0` の間は監視自体を評価しないよう修正した。

あわせて、再度有効化（`RxEnabled` が 0→1）した瞬間に全 RX I-PDU の
`Com_RxLastMs`（最終受信時刻）と `Com_RxTimedOut` をリセットするようにした
（SWS_Com_00787: `Com_IpduGroupStart` 時にデッドライン監視タイマを
再始動する要求に対応）。これをしないと、`TimeoutMs`（EngineInfo/AbsInfo
とも 5000ms）以上の時間受信を無効化していた場合、再有効化した直後の
`Com_MainFunction()` 呼び出しで「無効化前の古い `Com_RxLastMs`」のまま
即座にタイムアウト判定されてしまう（実際にはまだ新しいフレームを
1 つも受信できていない段階で）。

**Tx 無効中の送信トリガー保持**: 当初、`Com_TriggerIPDUSend()` は Tx 抑制中も
`Com_TxUpdatePending`・周期フロアのカウンタをあえて保持し、「再度有効化された
瞬間に無効化中の更新が送信される」設計にしていた（コメント上は意図的な設計と
していたが、仕様とは逆方向の判断だった）。しかし SWS_Com_00777「停止中の
I-PDU の送信要求はキャンセルしなければならない」、および SWS_Com_00334 の
説明文「停止中に発生した送信トリガーは保持されず、再開しても古いトリガーで
即座に送信されることはない」に反していた。`Com_TriggerIPDUSend()` は
Tx 抑制中を検出した時点で `Com_TxUpdatePending`/`Com_TxCyclesSinceSent` を
破棄するよう修正した。

（README 該当箇所: [診断スタック > CommunicationControl（SID 0x28）](../README.md#communicationcontrolsid-0x28)）

---

## Dem: デバウンスカウンタの反転バグ

単純に counter++/-- だけだと、既に確定 PASSED（counter=-limit）の状態から
FAILED を 1 回報告しても counter は -limit+1 にしかならず、+limit に届くまで
実質 2×limit 回分の反対方向の報告が必要になってしまう。これは特に limit=1
（BUTTON_STUCK / CAN_BUSOFF）で問題になる。

実際に、CAN_BUSOFF は起動直後に ComM から PASSED が 1 回報告されて counter=-1
まで進むため、最初の Bus-Off 断念（FAILED 1 回）が counter を -1→0 にする
だけで確定 FAILED（+1）に届かず、WARN ログが出力されない、という不具合が
あった。

修正として、報告の方向が反転したら中立 (0) からやり直す方式にした
（FAILED 報告時、counter が負なら 0 にリセットしてから ++。PASSED 報告時は逆）。
IoHwAb のボタンデバウンス（生レベルが確定値と一致すればカウンタをリセットする）
と同じ「割り込まれたら最初からやり直す」方式に合わせている。

（README 該当箇所: [DEM 診断イベント管理 > デバウンス](../README.md#デバウンス-counter-based-debouncing)）

---

## Dem: デバウンス閾値を単一値からイベントごとに変更した経緯

当初は全イベント共通の単一閾値（`DEM_DEBOUNCE_LIMIT=2`）だった。しかし
BUTTON_STUCK は「固着で+1、解放で-1」を繰り返すだけで確定（+2）に決して
到達できず、CAN_BUSOFF は「断念の瞬間に EcuM がシャットダウンへ進む」ため
2 回目の断念が起こり得ず確定不可能だった。どちらも「モニタ側が既に確定的な
判断をしてから1回だけ報告する」設計だったため、共通閾値2に合わせて呼び出し元を
不自然に作り替える必要があった。

イベントごとの閾値（`DEM_DEBOUNCE_LIMIT_*`）に変更したことで、両モジュールとも
報告ロジックを自然な「1 回だけ報告する」形に戻せた。

（README 該当箇所: [DEM 診断イベント管理 > 閾値 (limit) の決め方](../README.md#閾値-limit-の決め方)）

---

## ComM: ウェイクアップ直後の再集約による即座の再スリープ

ウェイクアップ検証の実機テスト中に、次のような不具合が見つかった。

```
[ウェイクアップ検証成功 → FULL_COM 確定 → EcuM: SHUTDOWN -> RUN]
INFO CanSM: Wakeup validated (RX confirmed) -> FULL_COM
INFO ComM: ch0 ->mode=2
INFO EcuM: SHUTDOWN ->RUN (wakeup) user=0

[同じ受信フレームの中身が defaultSession への SessionControl だった場合]
INFO Dcm: req SID=0x10
INFO ComM: User1 req=0 -> aggregated=0 (channel=2)   ← 直後に再スリープ！
INFO CanSM: ->NO_COM (CAN controller SLEEP)
```

`extendedSession` への切替では再現せず、`defaultSession` への切替でのみ
再現していた。原因は `ComM_BusSMIndication()` にあった。CanSM がウェイクアップ
検証成功時に `ComM_BusSMIndication(FULL_COM)` を呼んでチャネル状態を更新しても、
**どのユーザの要求でもない自動的な変化**であるため `ComM_UserRequest[COMM_USER_0]`
は更新されず、スリープ突入時の古い値（`NO_COM`）のまま残っていた。

`App_EngineManager_Run()` がまだ 1 周期も再評価していない（Task 2 が次に実行
されるのは最大3000ms後）そのわずかな間に、受信したフレームの内容が
defaultSession への SessionControl だったため、Dcm が
`ComM_RequestComMode(COMM_USER_1, NO_COM)` を呼んだ。この時点で再集約すると
`max(User0=NO_COM（古い値）, User1=NO_COM) = NO_COM` となり、実際には
ウェイクアップ検証を通過してチャネルが FULL_COM になったばかりにも関わらず、
即座に再スリープしてしまっていた。`extendedSession` の場合は `User1=FULL_COM`
になるため、User0 の古い値が埋もれて表面化しなかっただけである。

**修正**: `ComM_BusSMIndication()` が `FULL_COM`/`NO_COM` を通知するとき、
`ComM_UserRequest[COMM_USER_0]` もその値へ同期するようにした。CanSM 側の
自動的な状態変化を「User0 の暫定的な要求」とみなすことで、App_EngineManager が
次に実際のエンジン状態に基づいて要求し直すまでの間、矛盾のない値を保持できる。
Dcm（`COMM_USER_1`）の要求はセッション状態に基づく独立した判断のため、これには
同期させない。

（README 該当箇所: [ECU 管理層 > ComM > App_EngineManager との連携](../README.md#app_enginemanager-との連携comm_user_0)）

---

## CanSM: Bus-Off 回復断念設計の撤去

以前は Bus-Off の回復リトライを一定回数（既定 3 回）で断念し、専用の恒久
スリープ（AUTOSAR 標準外の `Can_EnterFinalSleep()`）へ落とす設計だった。
外部レビューで AUTOSAR 仕様（SWS_CanSM_00514/00515/00636）を確認したところ、
「回復を諦めて二度と復帰しない」状態はそもそも仕様に存在しないことが判明した。
仕様が定めるのは L1（短い間隔）→ L2（長い間隔）の二段階バックオフのみで、
無期限にリトライを継続する設計である。

これを受けて `Can_EnterFinalSleep()` とその呼び出し経路（`Can.c`/`Can.h`/
`Can_Hw.h`/`Can_Hw.cpp` の `CAN_HW_MODE_SLEEP_FINAL` を含む）を全て撤去し、
`CANSM_BUSOFF_RECOVERY_L1_MS`/`_L2_MS`/`CANSM_BUSOFF_L1_TO_L2_COUNT` による
L1/L2 バックオフに置き換えた。あわせて Bus-Off 検出直後（回復試行の前）に
`ComM_BusSMIndication(SILENT_COMMUNICATION)` を呼ぶようにし（SWS_CanSM_00521）、
ComM のチャネル状態が回復完了まで FULL_COM のまま古い情報として残ることを
防いだ。

この設計変更は EcuM 側にも影響した。EcuM に「同一ユーザからの重複 RUN 要求」
検知（SWS_EcuM_04125/04127）を追加したところ、CanSM の Bus-Off L1/L2
バックオフがリトライ成功のたびに `ComM_BusSMIndication(FULL_COM)` を呼ぶ
（RUN を解放していないため）ことと衝突し、重複要求ログが頻発する状態に
なった。これを避けるため `ComM_BusSMIndication()` はチャネルモードが実際に
変化した時のみ `EcuM_RequestRUN()`/`EcuM_ReleaseRUN()` を呼ぶよう修正した。

（README 該当箇所: [CAN コントローラの実スリープ](../README.md#can-コントローラの実スリープcan_setcontrollermodecan_t_sleep)）

---

## NvM: 非同期書き込みジョブキューへの変更経緯

当初 `NvM_WriteBlock()` は EEPROM への書き込みも含めて完全に同期処理だった。
Renesas RA の EEPROM ライブラリ（内蔵フラッシュのエミュレーション）は
1 バイトの書き込みでも消去・書き込みサイクルを伴うため、9 バイト超のブロック
（DEM_STATUS 等）をまとめて同期的に書くと数百 ms 協調スケジューラ全体が
停止することが実機で判明した。この停止は `Dem_ReportErrorStatus()` が
新規 DTC 確定のたびに発生し、WdgM の Deadline Supervision を巻き込んで
実際に HW ウォッチドッグリセットを引き起こしていた（詳細は
[WdgM: Deadline Supervision 上限緩和と Os_SchedulerStep のバグ](#wdgm-deadline-supervision-上限緩和と-os_schedulerstep-のバグ)
を参照）。

閾値を広げる対症療法ではなく、ブロッキングそのものを解消するため、実際の
AUTOSAR NvM と同じ非同期ジョブキュー方式（1 回の呼び出しで 1 バイトだけ書く、
10ms 周期の `NvM_MainFunction()`）へ変更した。同じ考え方は CAN の TX 確認
（`Can_MainFunction_Write`）の非同期化にも踏襲されている。

（README 該当箇所: [NvM > 非同期書き込みジョブキュー](../README.md#非同期書き込みジョブキュー)）

---

## WdgM: Deadline Supervision 上限緩和と Os_SchedulerStep のバグ

Deadline Supervision 導入直後、以下の 3 段階の不具合が連続して見つかった。

**1. END→START 上限超過（NvM の同期書き込みが原因）**

当初 WARNING エンティティの END→START 上限は 700ms（500ms 周期に ±200ms/±40%）
だったが、障害注入を伴わない通常動作でも Deadline Supervision が FAILED になり、
実際に HW ウォッチドッグリセットが発生した（`elapsed=741ms`）。原因は
`Dem_ReportErrorStatus()` が新規 DTC 確定時に `NvM_WriteBlock()` 経由で
EEPROM へ同期書き込みすることで、DTC 確定のたびに協調スケジューラが
数百ms 単位で止まっていたためである（詳細は
[NvM: 非同期書き込みジョブキューへの変更経緯](#nvm-非同期書き込みジョブキューへの変更経緯)
参照）。この遅延は WARNING タスク自身の異常ではなく他 BSW モジュール
（Dem/NvM）由来であるため、ENGINE と WARNING 双方の END→START 上限に
実測値の約2倍の余裕を持たせた（ENGINE: 3500→4500ms、WARNING: 700→1500ms）。
500ms 周期の WARNING は 3000ms 周期の ENGINE よりチェックポイント報告の間隔が
短いため、この種の「他モジュール起因の一時的なブロッキング」の影響を
相対的に受けやすいという教訓が得られた。

**2. Os_SchedulerStep() 自体のバグ（本質的な原因）**

上記の上限緩和後も、今度は `elapsed=224ms`（下限 300ms を下回る、短すぎる
間隔）という逆方向の Deadline 違反が発生した。原因は `Os_SchedulerStep()`
（`src/Os/Os.c`）が `now = millis()` をループの**先頭で 1 回だけ**取得し、
全タスクの周期判定・`Os_LastRunMs[]` 更新に使い回していたことである。
ENGINE タスクが同じスキャン内で NvM のブロッキング書き込みにより数百ms
専有すると、その後にチェックされる WARNING タスクの `Os_LastRunMs[]` には
「実際に判定・実行した時刻」ではなく「スキャン開始時点の古い時刻」が
記録されてしまう。次回のスキャンでは、既に進んだ実時刻との差分が本来より
大きく計算され、周期到来判定が早まって短い間隔で再実行されてしまう。
Deadline の閾値をいくら調整してもこの問題は解消できず、`now` をタスクごとに
ループ内で毎回取得し直すよう `Os_SchedulerStep()` 自体を修正した。

**3. SHUTDOWN から RUN 復帰直後の Alive Supervision 誤検出**

上記の修正後、今度はボランタリスリープ → SHUTDOWN → CAN ウェイクアップに
よる RUN 復帰の直後に `WARN WdgM: SE1 alive FAILED alive=2 (exp>=6)` が
発生し、実際にリセットした。原因は `WdgM_MainFunction`（Task 7、6000ms 周期）
自身が SHUTDOWN 中は `BSWM_TASK_MASK_SHUTDOWN` により無効化されており、
その `Os_LastRunMs[]` が無効化前の古い時刻のまま残っていたことである。
SHUTDOWN が 6000ms 以上続いた後に RUN へ復帰すると、`Os_SchedulerStep()` は
「経過時間が周期を大幅に超えている」と判定し、`WdgM_MainFunction` を
再開直後にほぼ即座に実行してしまう。この時点では WARNING タスクがまだ
1〜2 回しか実行されておらず（`AliveCount` が閾値 6 に届かない）、本来なら
復帰後まるまる 6000ms 監視して初めて判定すべきところを、実行機会が
ほとんどないまま Alive Supervision が FAILED と誤判定していた。

修正として、`Os_SetTaskActive()` がタスクを無効→有効へ切り替える瞬間に
`Os_LastRunMs[]` を現在時刻へリセットするようにした。これにより、長時間
無効化されていたどのタスクも、再開直後は必ずフルの周期を待ってから初めて
実行・評価されるようになる。

（README 該当箇所: [WdgM > Deadline Supervision の仕組み](../README.md#deadline-supervision-の仕組み)）

---

## WdgM: グローバル EXPIRED 許容サイクルの追加

当初、`WdgM_TriggerHwWatchdog()` は「1 つでもエンティティが FAILED ならその場で
リフレッシュを止める」設計だった。しかし `docs/AUTOSAR_SWS_WatchdogManager.pdf`
を確認すると、`[SWS_WdgM_00119]`〜`[SWS_WdgM_00121]` は Global Supervision
Status が `WDGM_GLOBAL_STATUS_OK`・`FAILED`・`EXPIRED` のいずれであっても
`WdgIf_SetTriggerCondition`（リフレッシュ相当）を同一に呼び続けることを
要求しており、リフレッシュを 0（停止）にしてよいのは `[SWS_WdgM_00122]`
`WDGM_GLOBAL_STATUS_STOPPED` に到達したときだけだった。STOPPED に到達するには
`WdgMExpiredSupervisionCycleTol`（グローバルレベルの EXPIRED 許容サイクル数）
分の判定サイクルを消費する必要があり（`[SWS_WdgM_00216]`/`[SWS_WdgM_00217]`
等）、仕様は単発の異常でいきなりリフレッシュを止めることを想定していない。

実際、上記の「Deadline Supervision 上限緩和」で記録した「NvM の EEPROM
ブロッキング書き込みで数百ms（最大 elapsed=741ms）のスケジューラ停止が起き、
Deadline 違反を誤検出した」不具合は、まさに仕様の猶予機構が本来吸収すべき
「一時的なスケジューラ遅延」を、タイミングマージンの拡張という対症療法だけで
凌いでいた状態だった。

修正として、`WdgM_ExpiredCycleCount`（グローバルレベルの連続 FAILED 判定
サイクル数）と `WdgM_GlobalStopped`（AUTOSAR の `WDGM_GLOBAL_STATUS_STOPPED`
相当）を追加した。本実装は Local Supervision Status を OK/FAILED の 2 値に
簡略化しており（仕様本来の FAILED/EXPIRED の区別や、per-SE の
`WdgMFailedAliveSupervisionRefCycleTol` は実装していない）、その代わりに
この 1 段のグローバル許容サイクル数（`WDGM_EXPIRED_SUPERVISION_CYCLE_TOL`、
既定 2）だけを持たせている。

（README 該当箇所: [WdgM > HW ウォッチドッグ連携](../README.md#hw-ウォッチドッグ連携実際の-mcu-リセット)）

---

## WdgM: グローバル猶予カウンタを resume でリセットしてはいけなかった

上記の猶予機構を追加した直後は、`WdgM_ResumeSupervision()` が
`WdgM_ExpiredCycleCount`/`WdgM_GlobalStopped` も `WdgM_AliveCount` と同様に
リセットしていた。しかし実機で「`App_WarningIndicator_Run()` の
`WDGM_CP_WARNING_START` チェックポイント報告をコメントアウトし、恒久的な
Logical Supervision 違反を意図的に注入する」というテストを行ったところ、
**500 秒以上経過してもリセットが一切発生しない**という重大な不具合が発覚した。

原因は、本プロジェクトのボランタリスリープが数十秒おきに POST_RUN →
SHUTDOWN → ウェイクアップ → RUN 復帰のサイクルを繰り返す一方、
`WDGM_EXPIRED_SUPERVISION_CYCLE_TOL`（既定 2）を使い切って
`WdgM_GlobalStopped` に到達するには判定サイクル（6000ms）3 回分＝
18000ms の連続した猶予消費が必要だったことである。RUN 復帰のたびに
猶予カウンタが 0 にリセットされてしまうため、3 回目の判定サイクルへ
到達する前に必ず次のスリープサイクルが来て猶予がリセットされ続け、
**恒久的な Logical Supervision 違反（本物のプログラムフローバグ）が
存在するにもかかわらず、フェイルセーフ機構が永久に発動しない**という、
本末転倒な状態になっていた。

Logical/Deadline Supervision のステータス自体はそもそも `WdgM_Init` まで
回復しないラッチ式の設計である。それを評価するグローバル猶予カウンタだけを
resume のたびに回復させてしまうのは非対称で誤りだった。修正として
`WdgM_ResumeSupervision()` からこの 2 つのリセットを削除し、真に全
エンティティが OK に戻ったとき（`WdgM_MainFunction()` 末尾の自然な回復
判定）にのみクリアされるようにした。これにより、恒久的な違反は何回
スリープ/ウェイクアップを挟んでも判定サイクル換算で着実に猶予を消費し続け、
いずれ確実に `WdgM_GlobalStopped` に到達するようになる。

あわせて、`WdgM_SupervisionSuppressed`（POST_RUN 中の想定内の Alive 不足を
無視するフラグ）が立っている間は、グローバル猶予カウンタの判定自体を凍結する
（進めも回復させもしない）ようにした。POST_RUN 中の Rte_Engine/Rte_Warning
停止による想定内の Alive 不足が、POST_RUN の頻度や長さ次第でグローバル猶予を
無関係に消費してしまうことを防ぐためである。

（README 該当箇所: [WdgM > HW ウォッチドッグ連携](../README.md#hw-ウォッチドッグ連携実際の-mcu-リセット)）

---

## WdgM: POST_RUN→RUN 復帰時の Deadline Supervision 誤検出

Deadline Supervision を追加した直後の見直しで、POST_RUN→RUN 復帰時に
誤って FAILED と判定してしまう不具合が見つかった。

POST_RUN 中は Rte_Engine / Rte_Warning タスクが意図的に停止するため
`WdgM_CheckpointReached()` が呼ばれず、「直前のチェックポイントの発生時刻」
(`WdgM_LastCheckpointTimeMs[]`) は停止前の古い値のまま残る。これをリセット
せずに RUN へ復帰すると、再開後最初のチェックポイントで、END→START の
経過時間として **POST_RUN の停止時間（最大 `ECUM_POST_RUN_TIMEOUT_MS`=5000ms）**
を計算してしまい、ENGINE・WARNING いずれの許容上限（それぞれ 3500ms /
700ms、当時の値）も確実に超過し、誤って Deadline Supervision が FAILED と
判定 → ラッチされてしまう。

これを防ぐため、`EcuM_RequestRUN()` が POST_RUN→RUN へ遷移する際に
`WdgM_ResumeSupervision()` を呼び、全エンティティのチェックポイント基準
（`WdgM_LastCheckpoint[]`）を `WDGM_CP_INITIAL` にリセットするようにした。
これにより再開後最初の遷移は起動直後と同じ「基準なしの遷移」として扱われ、
Deadline 比較の対象から外れる（既存の `WDGM_CP_INITIAL` 除外ルールをそのまま
再利用しているだけで、`WdgM_CheckpointReached()` 自体への変更は不要だった）。
既にラッチされている Logical/Deadline の FAILED 状態はリセットしない
（停止前に本当に違反していた事実は消さないため）。

（README 該当箇所: [WdgM > 意図的な POST_RUN 移行での無効化／RUN 復帰での再有効化](../README.md#意図的な-post_run-移行での無効化run-復帰での再有効化)）

---

## WdgM: POST_RUN→RUN 復帰時の Alive Supervision 誤検出

UDS CommunicationControl（SID 0x28）の実機検証中、S3 タイムアウト →
ボランタリスリープ → POST_RUN → SHUTDOWN → CAN ウェイクアップによる RUN 復帰、
という一連の流れの直後に実際に HW ウォッチドッグリセットが発生した。

POST_RUN 中は Rte_Engine / Rte_Warning タスクが意図的に停止するため、
`WdgM_MainFunction()` の判定サイクルで両エンティティの Alive Supervision が
必ず FAILED とラッチされる（POST_RUN 中はこれ自体は無害。
`WdgM_SupervisionSuppressed` により `WdgM_TriggerHwWatchdog()` がリフレッシュ
拒否を無視するため）。しかし `WdgM_ResumeSupervision()` はチェックポイント
基準（`WdgM_LastCheckpoint[]` / `WdgM_LastCheckpointTimeMs[]`、上記 Deadline
誤検出対策で導入）のみリセットし、`WdgM_AliveCount[]` / `WdgM_AliveStatus[]`
はリセットしていなかった。そのため POST_RUN 中に付いた FAILED ラッチが
SHUTDOWN を越えて RUN 復帰後まで残ってしまう。

RUN 復帰直後は `WdgM_EnableHwWatchdog()` により `WdgM_SupervisionSuppressed`
が解除されるため、アプリタスクは実際には正常に再開しているにもかかわらず、
古い FAILED ラッチのせいで `WdgM_TriggerHwWatchdog()` がリフレッシュを拒否し
続ける。次に `WdgM_MainFunction()` が判定し直してラッチが自然に解消される
のは最大 6000ms 後だが、HW ウォッチドッグのタイムアウトは 4000ms しかない
ため、判定が間に合わず実際に MCU がリセットされてしまった。

修正として、`WdgM_ResumeSupervision()` でチェックポイント基準に加えて
`WdgM_AliveCount[]`（0 に）・`WdgM_AliveStatus[]`（OK に）も全エンティティ分
リセットするようにした。Logical/Deadline のラッチはリセットしない
（POST_RUN 中に新規発生することがなく、Alive とは異なり純粋な RUN 中の異常
のみを検出する項目のため、消してしまうと本当の違反を見逃すことになる）。

実機で POST_RUN→SHUTDOWN→wakeup サイクルを再現し、修正後は
`HW watchdog NOT refreshed` が発生せず正常稼働が継続することを確認済み。

（README 該当箇所: [WdgM > 意図的な POST_RUN 移行での無効化／RUN 復帰での再有効化](../README.md#意図的な-post_run-移行での無効化run-復帰での再有効化)）

---

## WdgM: 短時間 POST_RUN での Alive Supervision 誤検出

上記の修正後も、ボランタリスリープに入った直後（POST_RUN 遷移から 1 秒未満）に
バス活動を検知して即座にウェイクアップし RUN へ復帰する、という短い
POST_RUN のシナリオで、再度実際に HW ウォッチドッグリセットが発生した。

原因は `WdgM_MainFunction()`（Task 7、Alive/Logical/Deadline を判定する
タスク自身）が POST_RUN 中も継続動作するタスクであるため、その呼び出し
タイミング（Os の内部スケジュール `Os_LastRunMs[7]`）が
`WdgM_ResumeSupervision()` の `WdgM_AliveCount[]` リセットと同期していない
ことである。POST_RUN が `WDGM_SUPERVISION_CYCLE_MS`（6000ms）よりも大幅に
短いと、リセット直後にたまたま `WdgM_MainFunction()` の次回呼び出し
タイミングが来てしまうことがある。この場合、Rte_Engine（3000ms 周期）等の
エンティティが `WdgM_ResumeSupervision()` 以降まだ一度もチェックポイントへ
到達できていないうちに判定が行われ、`alive=0 (exp>=1)` として誤って
FAILED と判定 → `WdgM_TriggerHwWatchdog()` がリフレッシュを拒否し続け、
実際に MCU がリセットされてしまった。

上記の修正（`WdgM_AliveCount[]`/`WdgM_AliveStatus[]` のリセット）だけでは、
「リセットするタイミング」と「判定するタイミング」がそもそも同期していない
という、より根本的な問題は解消できていなかったことになる。

修正として、`WdgM_ResumeSupervision()` が `WdgM_SkipNextAliveJudgment` フラグを
立て、`WdgM_MainFunction()` は次回呼び出し 1 回分だけ Alive Supervision の
判定そのものをスキップするようにした。`WdgM_AliveCount[]` には触れない
（リセットしない）ため、スキップした分の蓄積は次回の判定へそのまま持ち越される。
Os 自身のスケジューリングにより次回呼び出しは必ず一定時間後になるため、
その頃には各エンティティが十分チェックインを済ませており、正しく判定できる。

（README 該当箇所: [WdgM > 意図的な POST_RUN 移行での無効化／RUN 復帰での再有効化](../README.md#意図的な-post_run-移行での無効化run-復帰での再有効化)）

---

## WdgM: Alive と Logical のステータス統合バグ

以前は Alive と Logical を 1 つの `WdgM_LocalStatusType` に統合していたため、
Logical Supervision が FAILED と判定した直後でも、次の `WdgM_MainFunction`
サイクルで Alive 条件を満たすだけで検出した違反が消えてしまう問題があった
（本プロジェクトでも実際に確認・修正済み）。

修正として、Alive・Logical・Deadline それぞれ独立したステータス配列
（`WdgM_AliveStatus[]` / `WdgM_LogicalStatus[]` / `WdgM_DeadlineStatus[]`）
で管理するようにした。Deadline Supervision の追加にあたり、この独立
ステータス配列のパターンをそのまま 3 つ目にも適用している。

（README 該当箇所: [WdgM > 本プロジェクトでの失敗アクション](../README.md#本プロジェクトでの失敗アクション)）

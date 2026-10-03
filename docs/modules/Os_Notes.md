# Os

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

タイムトリガスケジューラ。タスクごとに周期を設定し `Os_SchedulerStep()` で到来タスクを
順次実行する。時間源は Os 専用の Gpt チャネル（`GPT_CHANNEL_1`）で、詳細は
[`EcuM_Notes.md`](./EcuM_Notes.md) の「Os のスケジューラティック」を参照。

Gpt が止まった場合に備え、`Os_CrossCheckTickSource()` が 500ms 周期で Gpt と `millis()` を
突き合わせ、異常を検知すると `millis()` へフォールバックしてスケジューラを止めません
（周期は HW ウォッチドッグの 4000ms より短く保つ必要がある）。他モジュールが現在時刻を得る
ときは `GetCounterValue(SYSTEM_COUNTER, ...)` を使い、フォールバックの前後でも値が連続します。
タスク一覧と周期は `Os_PBCfg.c` を参照してください。

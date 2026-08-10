# Os

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

タイムトリガスケジューラ。タスクごとに周期を設定し `Os_SchedulerStep()` で到来タスクを
順次実行する。時間源は Os 専用の Gpt チャネル（`GPT_CHANNEL_1`）で、詳細は
[`EcuM_Notes.md`](./EcuM_Notes.md) の「Os のスケジューラティック」を参照。

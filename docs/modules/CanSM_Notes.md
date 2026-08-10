# CanSM

> [README](../../README.md) の「[ECU 管理層](../../README.md#ecu-management)」節から分離。

Bus-Off 検出直後（回復試行の前）に `ComM_BusSMIndication(SILENT_COMMUNICATION)` を
呼び、ComM のチャネル状態が回復完了まで FULL_COM のまま古い情報として残ることを
防ぐ（SWS_CanSM_00521。SILENT_COM は EcuM の RUN を維持するため回復中も RUN は
落ちない）。受け付ける Bus-Off はコントローラが物理的に稼働中の状態（FULL_COM、
および Nm の Bus-Sleep Mode 到達待ちで HW が稼働継続する NO_COM_PENDING_SLEEP）
のみで、回復シーケンスは L1/L2 バックオフ（SWS_CanSM_00514/00515 準拠）で実施し、
試行回数が `CANSM_BUSOFF_L1_TO_L2_COUNT` を超えるまでは短い周期（L1）でリトライし、
超えたら Dem へ DTC を報告（limit=1 のため即座に確定）した上で長い周期（L2）へ
切り替えて無期限にリトライを継続する（回復を諦めて停止する状態は存在しない）。
再起動試行のたびに、Bus-Off 発生時点の状態（FULL_COM か NO_COM_PENDING_SLEEP か）
へ復帰させる（`CanSM_BusOffFromPendingSleep`、後者の場合は誤って FULL_COM へ
戻さない）。ComM の NO_COM 要求によるボランタリスリープでは即座にはスリープせず、
Nm（CanNm 状態機械）が Bus-Sleep Mode へ到達した通知（`CanSM_NmBusSleepMode()`）を
受けてから `Can_SetControllerMode(CAN_T_SLEEP)` で実 HW を実際にスリープさせる
（協調スリープ、詳細は [`Nm_Notes.md`](./Nm_Notes.md) 参照）。`CanSM_ControllerWakeup()`
による復帰経路を持ち、復帰は即座に確定せず、ウェイクアップ検証（Wakeup Validation
Protocol 相当）により有効な CAN フレーム受信を確認してから FULL_COM へ確定する。

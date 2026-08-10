# CanIf

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

CAN ID ↔ 論理 PDU のマッピングを担う。上位層は CAN ID を知らず PDU ID で通信する。
設定 DLC 未満の受信 L-PDU は上位層へ渡さず棄却する（SWS_CANIF_00026 のデータ長
チェック）。

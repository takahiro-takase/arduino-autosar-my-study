# PduR

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

受信 PDU を Com/CanTp/SecOC へ（1つの RxPduId から複数宛先への配信にも対応）、
送信 PDU を CanIf へルーティングする、通信スタックの配管役。TX 経路は既定では
`CanIf_Transmit()` へ直接転送するが、`PduR_TxRoutingPathType.TransmitOverrideFct`
が設定されている場合は中間モジュール（SecOC）へ委譲できるよう汎用化されている
（既存の全 TX パスはこのフィールドを使わないため無変更）。

# Rte

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

ポートベース S/R API。複数 SW-C が同一シグナルを独立ポートで受信する。E2E
Transformer を持つ Read ポートは `Std_ReturnType` ではなく `Rte_IStatusType` を
返し、E2E チェック結果（OK/ハードエラー/ソフトエラー）と Com タイムアウトを
区別して SWC へ伝える。

本プロジェクトには RTE ジェネレータが無いため、`src/Rte/Rte.c` は ARXML から生成される
コードを手書きで代用したものです（実 AUTOSAR では生成物なので手で編集しない）。

## 構成

| 区分 | 関数 | 役割 |
|------|------|------|
| ライフサイクル | `Rte_Start()` / `Rte_Stop()` / `Rte_Init_EngineManager()` / `Rte_Init_WarningIndicator()` | ミラー変数・ランプ状態の初期化と、SW-C の Init Runnable 起動（`EcuM_Init()` から呼ぶ） |
| Read（E2E 付き） | `Rte_Read_SpeedSensor_EngineSpeed()` 等 6 つ | `Rte_IStatusType` を返す（[`E2EXf_Notes.md`](./E2EXf_Notes.md) 参照） |
| Write / Read（その他） | `Rte_Write_MeterStatus_*()` / `Rte_Write_WarningStatus_*()` / `Rte_SendSignalGroup_WarningStatus()` / `Rte_Write_EngineStatus_EngineState()` 等 | Com の送信シグナル・Signal Group への書き込み、SW-C 間の状態共有 |
| Call（Client/Server） | `Rte_Call_Led_SetLevel()` / `Rte_Call_Button_GetLevel()` / `Rte_Call_Adc_GetValue_mV()` / `Rte_Call_FiM_GetFunctionPermission()` / `Rte_Call_ComM_*()` | IoHwAb・FiM・ComM など BSW のサービス呼び出し |
| IoControl | `Rte_IoControl_Lamp_*()` | UDS 0x2F（IOControl）によるランプの強制・解除。ASW のランプ出力と調停する |
| スケジューラ | `Rte_ScheduleRunnables()` / `Rte_ScheduleWarningIndicator()` | 各 SW-C の周期 Runnable 呼び出し |
| Com コールバック | `Rte_COM*()`（下表） | Com から呼ばれるグルー |

## Com からのコールバック（`Com_PBCfg.c` で配線）

| 関数 | Com 側の設定 | 内容 |
|------|-------------|------|
| `Rte_COMRxInd_EngineInfo()` / `Rte_COMRxInd_AbsInfo()` | `RxIndicationCbk` | E2E 検証→ミラー更新→E2EMon 通知（[`E2EXf_Notes.md`](./E2EXf_Notes.md)） |
| `Rte_COMRxInd_SecureCommand()` | `RxIndicationCbk` | SecOC 検証を通った ImmobilizerCmd を受けるログ出力デモ |
| `Rte_COMRxIpduCallout_SecureCommand()` / `Rte_COMTxIpduCallout_ImmobilizerStatus()` | `RxIpduCalloutCbk` / `TxIpduCalloutCbk` | I-PDU Callout（[`Com_Notes.md`](./Com_Notes.md) 参照） |
| `Rte_COMCbk_EngineOnFlag()` / `Rte_COMCbk_AbsInfo()` | `RxAckCbk` | 受信確認通知 |
| `Rte_COMCbkRxTOut_EngineOnFlag()` / `Rte_COMCbkRxTOut_AbsInfo()` | `RxTOutCbk` | 受信デッドライン超過通知 |
| `Rte_COMCbkTAck_EngineState()` / `Rte_COMCbkTAck_WarningStatus()` | `TxAckCbk` | 送信確認通知 |
| `Rte_COMCbkTxTOut_EngineState()` / `Rte_COMCbkTxTOut_WarningStatus()` | `TxTOutCbk` | 送信確認タイムアウト通知 |
| `Rte_COMInvalidNotify_CoolantTemp()` | `InvalidNotificationCbk` | 無効値受信通知 |
| `Rte_COMFilterReject_EngineSpeed()` | `FilterRejectCbk` | フィルタで棄却されたことの通知 |
| `Rte_COMTransform_E2EHealthStatus()` | `TxTransformCbk` | 送信前に E2E Profile05 を付加（E2EHealthStatus） |
| `Rte_SecOCVerificationStatus_ImmobilizerCmd()` | SecOC の `VerificationStatusCallout` | SecOC の検証結果通知（[`SecOC_Notes.md`](./SecOC_Notes.md)） |

これらは Rte.h には公開せず（`Rte_Cbk.h` に宣言）、Com 設定テーブルからのみ参照されます。
ミラー変数へのアクセスは `SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()` で保護しています
（Com の受信割り込み後処理と Runnable の読み出しが競合するため）。

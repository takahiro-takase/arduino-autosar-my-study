# AUTOSAR 参考資料

本プロジェクトで参照している AUTOSAR 仕様書の入手先一覧です。
PDF ファイルは著作権により再配布禁止のため git 管理外（`.gitignore`）としています。
以下の URL から無償でダウンロードできます（要アカウント登録）。

入手先: https://www.autosar.org/standards/classic-platform

---

## アーキテクチャ

| 文書名 | ファイル名 | 説明 |
|--------|-----------|------|
| Layered Software Architecture | `AUTOSAR_EXP_LayeredSoftwareArchitecture.pdf` | ASW / RTE / BSW / MCAL の層構造と各モジュールの位置づけ。IoHwAb の概念もここで説明される |

---

## ECU 管理

| 文書名 | ファイル名 | 説明 |
|--------|-----------|------|
| SWS ECU State Manager | `AUTOSAR_SWS_ECUStateManager.pdf` | EcuM の状態マシン・RUN ユーザ・POST_RUN タイムアウト |
| SWS BSW Mode Manager | `AUTOSAR_SWS_BSWModeManager.pdf` | BswM のルールエンジン・モード源・アクション |
| SWS Watchdog Manager | `AUTOSAR_SWS_WatchdogManager.pdf` | WdgM の Alive Supervision・Supervised Entity |

---

## 通信スタック

| 文書名 | ファイル名 | 説明 |
|--------|-----------|------|
| SWS CAN Driver | `AUTOSAR_SWS_CANDriver.pdf` | Can モジュール API（Can_Init / Can_Write / Bus-Off 検出） |
| SWS CAN Interface | `AUTOSAR_SWS_CANInterface.pdf` | CanIf の PDU マッピング・コールバック |
| SWS PDU Router | `AUTOSAR_SWS_PDURouter.pdf` | PduR のルーティングテーブル |
| SWS COM | `AUTOSAR_SWS_COM.pdf` | シグナルパック/アンパック・受信デッドライン監視 |
| SWS CAN Transport Layer | `AUTOSAR_SWS_CANTransportLayer.pdf` | ISO 15765-2 フレーム分割・組立（CanTp） |
| SWS CAN State Manager | `AUTOSAR_SWS_CANStateManager.pdf` | CanSM の Bus-Off 回復シーケンス |
| SWS COM Manager | `AUTOSAR_SWS_COMManager.pdf` | ComM の通信モード管理（NO_COM / FULL_COM） |
| SWS E2E Library | `AUTOSAR_SWS_E2ELibrary.pdf` | E2E Profile 1 の CRC/カウンタ/状態機械。要約は [`E2E_Profile1_Notes.md`](./E2E_Profile1_Notes.md) 参照 |
| SRS E2E | `AUTOSAR_SRS_E2E.pdf` | E2E の上位要求仕様（脅威モデル・なぜ E2E が必要か） |

---

## 診断スタック

| 文書名 | ファイル名 | 説明 |
|--------|-----------|------|
| SWS Diagnostic Communication Manager | `AUTOSAR_SWS_DiagnosticCommunicationManager.pdf` | Dcm の UDS サービス処理（SID 0x10 / 0x14 / 0x19 / 0x22 等） |
| SWS Diagnostic Event Manager | `AUTOSAR_SWS_DiagnosticEventManager.pdf` | Dem の DTC 管理・ステータスバイト（ISO 14229-1 Annex B） |
| SWS NV RAM Manager | `AUTOSAR_SWS_NVRAMManager.pdf` | NvM の EEPROM 抽象化・ブロック管理 |

---

## IO スタック

| 文書名 | ファイル名 | 説明 |
|--------|-----------|------|
| SWS I/O Hardware Abstraction | `AUTOSAR_SWS_IOHardwareAbstraction.pdf` | IoHwAb の位置づけ（API はプロジェクト定義のため仕様は薄い） |
| SWS DIO Driver | `AUTOSAR_SWS_DIODriver.pdf` | Dio_WriteChannel / Dio_ReadChannel の型・戻り値規定 |
| SWS PORT Driver | `AUTOSAR_SWS_PORTDriver.pdf` | Port_Init のピン方向設定 API |
| SWS ADC Driver | `AUTOSAR_SWS_ADCDriver.pdf` | Adc_StartGroupConversion / Adc_ReadGroup（来週 ADC 追加時に参照） |

---

## 基盤

| 文書名 | ファイル名 | 説明 |
|--------|-----------|------|
| SWS Schedule Manager | `AUTOSAR_SWS_ScheduleManager.pdf` | SchM の排他エリア（ExclusiveArea）設計 |
| General Specification of Basic Software Modules | `AUTOSAR_SWS_BSWGeneral.pdf` | BSW 全モジュール共通の命名規則・型定義・ServiceID 規約 |
| SWS Standard Types | `AUTOSAR_SWS_StandardTypes.pdf` | `Std_ReturnType` / `E_OK` / `E_NOT_OK` 等の基本型定義 |

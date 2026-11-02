# AUTOSAR 参考資料

本プロジェクトで参照している AUTOSAR 仕様書の入手先一覧です。
PDF ファイルは著作権により再配布禁止のため git 管理外（`.gitignore`）としています。
以下の URL から無償でダウンロードできます（要アカウント登録）。

入手先: https://www.autosar.org/standards/classic-platform

## 格納フォルダと版の読み方

PDF は、**参照した版ごとのフォルダ**（`docs/autosar/<版>/`）に置きます。各フォルダは `.gitignore` の対象です（新しい版のフォルダを作ったら、`.gitignore` にも `docs/autosar/<版>/*.*` を足す）。

| フォルダ | 内容 |
|---|---|
| `4.3.1/` | 本プロジェクトの基準（R4.3.1）。ComStack の PDF は、すべて表紙が「Part of Standard Release 4.3.1」で、変更履歴の先頭が 4.3.1（2017-12-08）であることを確認済み（2026-11-02） |
| `4.4.0/` | R4.3.1 に存在しない文書（KeyM など） |
| `R19-11/` | 4.3.1 の文書が古い内容だったときの代わり（下記） |
| `R25-11/` | `AUTOSAR_CP_SWS_IOHardwareAbstraction.pdf`（IoHwAb の新しい版。4.3.1 の IoHwAb は `4.3.1/AUTOSAR_SWS_IOHardwareAbstraction.pdf`） |

**版の読み方（重要）**
- 表紙の版番号ではなく、**変更履歴（表紙の次のページ）を先頭から全部読んで**判断します。
- 新しい版の PDF を使うときは、**4.3.1 までの内容だけ**を見ます。変更履歴で、4.3.1 より後（4.4.0 / R18-10 / R19-03 / R19-11 など）の機能追加を特定し、その分は除外します。
- **CAN ドライバ**: `4.3.1/AUTOSAR_SWS_CANDriver.pdf` は、公式の R4.3.1 フォルダから取れる版だが、表紙が「V4.3.0 / R4.1 Rev 3」で、変更履歴が 2014-03（4.1.3）で止まっており、4.2.2 以降に追加された API（`Can_GetControllerErrorState`、`Can_DeInit`、`Can_GetControllerMode` など）が載っていない。`R19-11/AUTOSAR_SWS_CANDriver.pdf` の変更履歴は、4.3.1（2017-12）を含み、その後は 4.4.0（文書状態の変更のみ）と R19-11（軽微な修正のみ）なので、**4.3.1 の内容として読める**。

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
| SWS CAN Driver | `AUTOSAR_SWS_CANDriver.pdf` | Can モジュール API（Can_Init / Can_Write / Bus-Off 検出）。**4.3.1 の内容は `R19-11/` の版で読む**（下の「格納フォルダと版の読み方」参照） |
| SWS CAN Interface | `AUTOSAR_SWS_CANInterface.pdf` | CanIf の PDU マッピング・コールバック |
| SWS PDU Router | `AUTOSAR_SWS_PDURouter.pdf` | PduR のルーティングテーブル |
| SWS COM | `AUTOSAR_SWS_COM.pdf` | シグナルパック/アンパック・受信デッドライン監視 |
| SWS CAN Transport Layer | `AUTOSAR_SWS_CANTransportLayer.pdf` | ISO 15765-2 フレーム分割・組立（CanTp） |
| SWS CAN State Manager | `AUTOSAR_SWS_CANStateManager.pdf` | CanSM の Bus-Off 回復シーケンス |
| SWS COM Manager | `AUTOSAR_SWS_COMManager.pdf` | ComM の通信モード管理（NO_COM / FULL_COM） |
| SWS E2E Library | `AUTOSAR_SWS_E2ELibrary.pdf` | E2E Profile 1/5 の CRC/カウンタ/状態機械。要約は [`E2E_Profile1_Notes.md`](../modules/E2E_Profile1_Notes.md) / [`E2E_Profile5_Notes.md`](../modules/E2E_Profile5_Notes.md) 参照 |
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

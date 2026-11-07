# AUTOSAR CP R4.3.1 CAN State Manager 仕様書（和訳）

原本: `docs/autosar/4.3.1/AUTOSAR_SWS_CANStateManager.pdf`（Document ID 253）

## 本書について

- 学習用の参考訳です。**正本は原本（英語）**で、解釈に迷ったら必ず原本を参照してください。
- 条項番号（`[SWS_CanSM_xxxxx]`）、API 名、定数名、型名、状態名は原文のまま残しています。
- 訳の範囲は第 1〜5 章、第 7 章（機能仕様）、第 8 章（API 仕様）です。目次、要件トレーサビリティ表（第 6 章）、シーケンス図（第 9 章）、コンフィギュレーション仕様（第 10 章）は含みません。
- 原本の図（ステートマシン図など）は文字として取り出せないため訳していません。図に書かれた内容が条項本文に書かれている場合は、その条項を訳しています。
- 条項末尾の `(SRS_xxx)` は、その条項が満たす上位要件の ID です。

---

## 1 はじめに、および機能の概要

本仕様書は、AUTOSAR Basic Software モジュール「CAN State Manager」の機能、API、およびコンフィギュレーションを記述する。

AUTOSAR の BSW スタックは、通信バスごとに、そのバス専用のステートマネージャを規定している。本モジュールは、対応するバスの制御フローを実装する。CAN State Manager（CanSM）は、図 1-1 に示すように Communication Service Layer の構成要素であり、Communication Hardware Abstraction Layer および System Service Layer と相互作用する。

（図 1-1: CanSM から見た階層型ソフトウェアアーキテクチャ。原本の図を参照。）

## 2 略語

| 略語 | 意味 |
|---|---|
| API | Application Program Interface |
| BSW | Basic Software |
| CAN | Controller Area Network |
| CanIf | CAN Interface |
| CanSM | CAN State Manager |
| ComM | Communication Manager |
| DEM | Diagnostic Event Manager |
| DET | Default Error Tracer |
| EcuM | ECU State Manager |
| PDU | Protocol Data Unit |
| RX | 受信 |
| TX | 送信 |
| SchM | BSW Scheduler |
| SWC | Software Component |
| BswM | Basic Software Mode Manager |

## 3 関連文書

### 3.1 入力文書

原本の一覧を参照（BSW モジュール一覧、階層型ソフトウェアアーキテクチャ、BSW 一般要件、ECU コンフィギュレーション、標準型、通信スタック型、CAN 要件、モード管理要件、CAN Transceiver Driver、Communication Manager、ECU State Manager、Diagnostic Event Manager、CAN Interface、BSW Scheduler、Default Error Tracer、Basic Software Mode Manager、CAN Network Management、Diagnostic Communication Manager、BSW 一般仕様の各仕様書）。

### 3.2 関連する規格

なし。

### 3.3 関連する仕様

AUTOSAR は BSW モジュール一般に関する一般仕様書 [21]（SWS BSW General）を提供しており、これは CAN State Manager にも有効である。したがって、SWS BSW General は CAN State Manager にとって追加の必須仕様として扱わなければならない。

## 4 制約と前提

### 4.1 制限事項

CanSM モジュールは CAN 通信専用である。その役割は、CanIf モジュールと協調して、下位の 1 つまたは複数の CAN コントローラと CAN Transceiver Driver を制御することである。CAN 以外のプロトコル（LIN や FlexRay など）はサポートしない。

### 4.2 適用できる車両ドメイン

CAN State Manager モジュールは、CAN プロトコルを使用する場合、あらゆるドメインのアプリケーションで使用できる。

## 5 他モジュールへの依存関係

以下の各節は、CanSM モジュールが他のモジュールに要求するコンフィギュレーション情報とサービスを簡潔に説明する。

（図 5-1: CanSM モジュールのモジュール依存関係。原本の図を参照。）

### 5.1 ECU State Manager（EcuM）

EcuM モジュールは CanSM モジュールを初期化し、CAN のウェイクアップ検証について CanSM モジュールと相互作用する（詳細は EcuM の仕様書 [11] を参照）。

### 5.2 BSW Scheduler（SchM）

BSW Scheduler モジュールは、CanSM モジュールのメイン関数を呼び出す。これは CanSM モジュールの周期処理に必要である（詳細は [14] を参照）。

### 5.3 Communication Manager（ComM）

ComM モジュールは、CanSM モジュールの API を使って、固有のネットワークハンドルで識別される CAN ネットワークの通信モードを要求する（詳細は [10] を参照）。

CanSM モジュールは、自分の CAN ネットワークの現在の通信モードを ComM モジュールへ通知する。

### 5.4 CAN Interface（CanIf）

CanSM モジュールは、CanIf モジュールの API を使って、各 CAN ネットワークに割り当てられた CAN コントローラと CAN トランシーバの動作モードを制御する（詳細は [13] を参照）。

CanIf モジュールは、周辺機器のイベントを CanSM モジュールへ通知する。

### 5.5 Diagnostic Event Manager（DEM）

CanSM モジュールは、バス固有のプロダクションエラーを DEM モジュールへ報告する（詳細は [12] を参照）。

### 5.6 Basic Software Mode Manager（BswM）

CanSM は、バス固有のモード変化を BswM モジュールへ通知する必要がある（詳細は [18] を参照）。

### 5.7 CAN Network Management（CanNm）

CanSM モジュールは、部分ネットワークの利用可否を CanNm モジュールへ通知する必要があり、また Partial Networking の場合は、CanNm から通知されるタイムアウト例外を処理しなければならない（詳細は [19] を参照）。

### 5.8 Default Error Tracer（DET）

CanSM モジュールは、開発エラーとランタイムエラーを DET モジュールへ報告する。開発エラーは、コンフィギュレーションで開発エラー処理が有効な場合にのみ報告される（詳細は [15] を参照）。

### 5.9 ファイル構成

#### 5.9.1 コードファイル構成

詳細は SWS_BSWGeneral の 5.1.6 節「Code file structure」を参照。

#### 5.9.2 ヘッダファイル構成

**[SWS_CanSM_00008]** ヘッダファイル CanSM.h は、CanSM モジュール固有の型と、API の CanSM_GetVersionInfo、CanSM_MainFunction、CanSM_Init をエクスポートしなければならない。(SRS_BSW_00447)

**[SWS_CanSM_00238]** ヘッダファイル CanSM.h は、ヘッダファイル ComStack_Types.h をインクルードしなければならない。(SRS_Can_01142)

備考: ヘッダファイル ComStack_Types.h は、ヘッダファイル Std_Types.h をインクルードする。

**[SWS_CanSM_00174]** ヘッダファイル CanSM.h は、ヘッダファイル ComM.h をインクルードしなければならない。(SRS_Can_01142)

理由: CanSM の一部の API が、ComM モジュールの型定義を使うため。

**[SWS_CanSM_00009]** ヘッダファイル CanSM_ComM.h は、ComM モジュール向けのインタフェースと、それに対応する型をエクスポートしなければならない。(SRS_Can_01142)

**[SWS_CanSM_00010]** ヘッダファイル CanSM_Cfg.h は、C ソースファイル CanSM_Lcfg.c と CanSM_PBcfg.c のパラメータへの参照（上記 5.9.1 節参照）を含み、"const" パラメータとしてではなく define として宣言されるプリコンパイルパラメータを含まなければならない。(SRS_BSW_00344, SRS_BSW_00404, SRS_BSW_00345, SRS_BSW_00381, SRS_BSW_00412)

**[SWS_CanSM_00015]** CanSM モジュール（CanSM.c）は、ヘッダファイル Det.h をインクルードしなければならない。(SRS_BSW_00171)

理由: Det.h で宣言された関数は、開発エラーとランタイムエラーの報告に使われるため。

**[SWS_CanSM_00017]** CanSM モジュール（CanSM.c）は、ヘッダファイル CanIf.h をインクルードしなければならない。(SRS_Can_01145)

理由: 周辺機器の制御に CanIf モジュールの API が必要なため。

**[SWS_CanSM_00191]** CanSM モジュール（CanSM.c）は、ヘッダファイル ComM_BusSM.h をインクルードしなければならない。(SRS_Can_01142)

理由: ファイル ComM_BusSM.h は、バスステートマネージャ専用の ComM モジュールの API を提供するため。

**[SWS_CanSM_00347]** ヘッダファイル CanSM_BswM.h は、BswM モジュール向けのインタフェースと、それに対応する型をエクスポートしなければならない。(SRS_Can_01142)

**[SWS_CanSM_00348]** CanSM モジュール（CanSM.c）は、ヘッダファイル CanSM_BswM.h をインクルードしなければならない。(SRS_Can_01142)

**[SWS_CanSM_00548]** Partial Networking が有効な場合（ECUC_CanSM_00344 参照）、CanSM モジュール（CanSM.c）は、ヘッダファイル CanNm_Cbk.h をインクルードしなければならない。(SRS_Can_01142)

**[SWS_CanSM_00549]** ヘッダファイル CanSM_TxTimeoutException.h は、CanNm モジュールへのオプションのインタフェースとして、コールバック関数 CanSM_TxTimeoutException を提供しなければならない。(SRS_Can_01142)

#### 5.9.3 バージョンチェック

詳細は SWS_BSWGeneral の 5.1.8 節「Version Check」を参照。

---

## 7 機能仕様

本章は、AUTOSAR BSW アーキテクチャにおける CanSM モジュールの各機能を規定する。

ECU は複数の通信ネットワークを持つことができる。各ネットワークは、固有のネットワークハンドルで識別されなければならない。ComM モジュールは、各ネットワークに通信モードを要求する。どのハンドルがどの種類のネットワークに割り当てられているかは、ComM がそのコンフィギュレーションから知っている。CAN の場合、ComM は CanSM モジュールを使う。

CanSM モジュールは、CAN ネットワークの制御フローの抽象化を担う。

CanSM モジュールは、ComM モジュールからのモード要求に応じて、設定された CAN ネットワークの通信モードを変更する。

そのために CanSM モジュールは CanIf モジュールの API を使う。CanIf モジュールは、設定された CAN コントローラと CAN トランシーバの制御フローの抽象化を担う（CanIf モジュールのデータフローの抽象化は CanSM モジュールには関係しない）。CAN コントローラのモードと CAN トランシーバのモードの変更は、すべて CanIf モジュールから CanSM モジュールへ通知される。CanSM モジュールは、この通知と、設定された CAN ネットワークごとに実装しなければならない CAN ネットワークのステートマシンの状態に応じて、ComM と BswM へ通知する（詳細は 7.2 節を参照）。

### 7.1 一般要件

**[SWS_CanSM_00266]** CanSM モジュールは、設定された CAN ネットワークごとに、現在のネットワークモードを内部に保存しなければならない（ECUC_CanSM_00126 参照）。(SRS_Can_01142)

**[SWS_CanSM_00284]** CanSM モジュールが内部に保存するネットワークモードは、COMM_NO_COMMUNICATION、COMM_SILENT_COMMUNICATION、COMM_FULL_COMMUNICATION のいずれかの値をとれる。(SRS_Can_01142)

**[SWS_CanSM_00428]** CanSM のステートマシン CANSM_BSM（図 7-1 参照）のすべてのエフェクトは、CanSM のメイン関数のコンテキストで実行されなければならない（SWS_CanSM_00065 参照）。(SRS_Can_01142, SRS_Can_01145)

**[SWS_CanSM_00278]** CanSM のステートマシン CANSM_BSM（図 7-1 参照）が CANSM_BSM_S_NOT_INITIALIZED 状態にある場合、ComM モジュールからのネットワークモード要求を拒否しなければならない（SWS_CanSM_00062 参照）。(SRS_Can_01142)

**[SWS_CanSM_00385]** CanSM モジュールのステートマシンが T_REPEAT_MAX でトリガされた場合（SWS_CanSM_00463、SWS_CanSM_00480、SWS_CanSM_00495、SWS_CanSM_00523、SWS_CanSM_00536 参照）、CanSM モジュールは、EventId := CANSM_E_MODE_REQUEST_TIMEOUT を指定して Dem_SetEventStatus を呼ばなければならない（7.3 節参照）。(SRS_Can_01142)

**[SWS_CanSM_00422]** CanIf モジュールが、設定された CAN トランシーバの PN 利用可能を、コールバック関数 CanSM_ConfirmPnAvailability（SWS_CanSM_00419 参照）で CanSM モジュールへ通知した場合、CanSM モジュールは、PN の利用可能を CanNm モジュールへ確認するため、関連する CAN ネットワークをチャネルとして API CanNm_ConfirmPnAvailability（8.5.1 節参照）を呼ばなければならない。(SRS_Can_01142)

**[SWS_CanSM_00560]** CAN ネットワークに CanSMTransceiverId（ECUC_CanSM_00137 参照）が設定されていない場合、CanSM モジュールは、その CAN ネットワークについて規定されたすべての CanIf_SetTrcvMode 呼び出し（例: SWS_CanSM_00446）を省略し、すでに期待される CanSM_TransceiverModeIndication を受け取ったかのように（例: SWS_CanSM_00448）、各状態遷移を進めなければならない。(SRS_Can_01145)

**[SWS_CanSM_00635]** CanSM モジュールは、設定された CAN ネットワークごとに（ECUC_CanSM_00126 参照）、API 要求 CanSM_RequestComMode（SWS_CANSM_00062、SWS_CANSM_00182 参照）が E_OK を返して受け付けた最新の通信モード要求を保存し、それを、関連する CAN ネットワークのステートマシンのトリガとして使わなければならない（図 7-1、SWS_CanSM_00427、SWS_CanSM_00429、SWS_CanSM_00499、SWS_CanSM_00542、SWS_CanSM_00543、SWS_CANSM_00425、SWS_CANSM_00426、SWS_CANSM_00554 参照）。(SRS_Can_01142)

**[SWS_CanSM_00638]** CanSM モジュールは、CAN コントローラのモード変更が成功するたび（SWS_CANSM_00396 参照）、またはバスオフに起因して CAN_CS_STOPPED へ変更されるたび（SWS_CANSM_00064 参照）に、変更後のモードを CAN コントローラごとに内部へ保存しなければならない。(SRS_Can_01145)

### 7.2 CAN ネットワークごとのステートマシン

次の図は、CanSM モジュールの振る舞いのステートマシンを規定する。これは、設定された CAN ネットワークごとに実装しなければならない（ECUC_CanSM_00126 参照）。

図 7-1: CANSM_BSM、1 つの CAN ネットワークのステートマシン図（原本の図を参照）。

図に現れる状態、トリガ（`T_`）、ガード条件（`G_`）、エフェクト（`E_`）の関係は、次の各項で条項として規定される。

#### 7.2.1 トリガ: PowerOn

**[SWS_CanSM_00424]** PowerOn の後、CanSM のステートマシン（図 7-1 参照）は、状態 CANSM_BSM_NOT_INITIALIZED になければならない。

#### 7.2.2 トリガ: CanSM_Init

**[SWS_CanSM_00423]** CanSM モジュールが関数 CanSM_Init（8.3.1 節参照）で要求された場合、これは、設定されたすべての CAN ネットワークについて（ECUC_CanSM_00126 参照）、CanSM のステートマシン（図 7-1 参照）をトリガ CanSM_Init でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.3 トリガ: CanSM_DeInit

**[SWS_CanSM_00658]** CanSM モジュールが関数 CanSM_DeInit で要求された場合、これは、設定されたすべての CAN ネットワークについて（ECUC_CanSM_00126 参照）、CanSM のステートマシン（図 7-1 参照）をトリガ CanSM_DeInit でトリガしなければならない。(SRS_Can_01164)

注: CanSM_DeInit 関数の呼び出し側は、すべての CAN ネットワークが状態 CANSM_NO_COMMUNICATION にあることを保証しなければならない。

#### 7.2.4 トリガ: T_START_WAKEUP_SOURCE

**[SWS_CanSM_00607]** API 要求 CanSM_StartWakeUpSource（SWS_CanSM_00609 参照）が E_OK を返す場合（SWS_CanSM_00616 参照）、これはステートマシン（図 7-1 参照）を T_START_WAKEUP_SOURCE でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.5 トリガ: T_STOP_WAKEUP_SOURCE

**[SWS_CanSM_00608]** API 要求 CanSM_StopWakeUpSource（SWS_CanSM_00610 参照）が E_OK を返す場合（SWS_CanSM_00622 参照）、これはステートマシン（図 7-1 参照）を T_STOP_WAKEUP_SOURCE でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.6 トリガ: T_FULL_COM_MODE_REQUEST

**[SWS_CanSM_00425]** パラメータ ComM_Mode が COMM_FULL_COMMUNICATION に等しい API 要求 CanSM_RequestComMode（SWS_CanSM_00635 参照）は、関数パラメータ network がコンフィギュレーションパラメータ CANSM_NETWORK_HANDLE（ECUC_CanSM_00161 参照）に一致する場合、ステートマシンを T_FULL_COM_MODE_REQUEST でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.7 トリガ: T_NO_COM_MODE_REQUEST

**[SWS_CanSM_00426]** パラメータ ComM_Mode が COMM_NO_COMMUNICATION に等しい API 要求 CanSM_RequestComMode（SWS_CanSM_00635 参照）は、関数パラメータ network がコンフィギュレーションパラメータ CANSM_NETWORK_HANDLE（ECUC_CanSM_00161 参照）に一致する場合、ステートマシンを T_NO_COM_MODE_REQUEST でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.8 トリガ: T_BUS_OFF

**[SWS_CanSM_00606]** コールバック関数 CanSM_ControllerBusOff（SWS_CanSM_00064 参照）は、その CAN ネットワークに設定された CAN コントローラのいずれかが、コールバック関数 CanSM_ControllerBusOff の関数パラメータ ControllerId に一致する場合、その CAN ネットワークのステートマシン CANSM_BSM（図 7-1 参照）を T_BUS_OFF でトリガしなければならない。(SRS_Can_01144, SRS_Can_01146)

#### 7.2.9 トリガ: T_REPEAT_MAX

**[SWS_CanSM_00523]** ステートマシン CANSM_BSM（図 7-1 参照）が、そのサブステートマシンのいずれかで、CAN ネットワークの CAN コントローラを開始する CanIf API の呼び出し（例: SWS_CanSM_00509 参照）を、設定された回数（ECUC_CanSM_00335 参照）より多く繰り返しても、戻り値 E_OK が得られず、かつ期待されるモード通知（例: SWS_CanSM_00511 参照）も得られなかった場合、これはステートマシン CANSM_BSM を T_REPEAT_MAX でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.10 ガード条件: G_FULL_COM_MODE_REQUESTED

**[SWS_CanSM_00427]** CanSM_BSM ステートマシン（図 7-1 参照）のガード条件 G_FULL_COM_MODE_REQUESTED は、そのステートマシンのネットワークハンドルに対して CanSM_RequestComMode（SWS_CanSM_00635 参照）で受け付けた最新の通信モード要求が、パラメータ ComM_Mode が COMM_FULL_COMMUNICATION に等しいものであったかどうかを評価しなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.11 ガード条件: G_SILENT_COM_MODE_REQUESTED

**[SWS_CanSM_00429]** CanSM_BSM ステートマシン（図 7-1 参照）のガード条件 G_SILENT_COM_MODE_REQUESTED は、そのステートマシンのネットワークハンドルに対して CanSM_RequestComMode（SWS_CanSM_00635 参照）で受け付けた最新の通信モード要求が、パラメータ ComM_Mode が COMM_SILENT_COMMUNICATION に等しいものであったかどうかを評価しなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.12 エフェクト: E_PRE_NOCOM

**[SWS_CanSM_00431]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_PRE_NOCOM は、対応する CAN ネットワークについて、パラメータ Network := CanSMComMNetworkHandleRef、CurrentState := CANSM_BSWM_NO_COMMUNICATION を指定して API BswM_CanSM_CurrentState を呼ばなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.13 エフェクト: E_NOCOM

**[SWS_CanSM_00430]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_NOCOM は、対象の CAN ネットワークの、内部に保存されたネットワークモード（SWS_CanSM_00266 参照）を COMM_NO_COMMUNICATION へ変更しなければならない。(SRS_Can_01142, SRS_Can_01145)

**[SWS_CanSM_00651]** そのネットワークに対する通信モード要求がすでに存在し（SWS_CanSM_00635 参照）、保存された通信モード要求が COMM_NO_COMMUNICATION である場合、CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_NOCOM は、パラメータ Channel := CanSMComMNetworkHandleRef（ECUC_CanSM_00161 参照）、ComMode := COMM_NO_COMMUNICATION を指定して API ComM_BusSM_ModeIndication を呼ばなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.14 エフェクト: E_FULL_COM

**[SWS_CanSM_00539]** ECU passive が FALSE の場合（SWS_CanSM_00646 参照）、CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_FULL_COM は、**最初に**、CAN ネットワークに設定された各 CAN コントローラについて、パラメータ ControllerId := CanSMControllerId（ECUC_CanSM_00141 参照）、PduModeRequest := CANIF_ONLINE を指定して API CanIf_SetPduMode を呼ばなければならない。(SRS_Can_01158)

**[SWS_CanSM_00647]** ECU passive が TRUE の場合（SWS_CanSM_00646 参照）、CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_FULL_COM は、**最初に**、CAN ネットワークに設定された各 CAN コントローラについて、パラメータ ControllerId := CanSMControllerId（ECUC_CanSM_00141 参照）、PduModeRequest := CANIF_TX_OFFLINE_ACTIVE を指定して API CanIf_SetPduMode を呼ばなければならない。(SRS_Can_01158)

**[SWS_CanSM_00435]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_FULL_COM の中で SWS_CANSM_00539 と SWS_CanSM_00647 を考慮した後、CanSM モジュールは、対応する CAN ネットワークについて、パラメータ Channel := CanSMComMNetworkHandleRef（ECUC_CanSM_00161 参照）、ComMode := COMM_FULL_COMMUNICATION を指定して API ComM_BusSM_ModeIndication を呼ばなければならない。

**[SWS_CanSM_00540]** エフェクト E_FULL_COM の中で SWS_CANSM_00435 を考慮した後、CanSM モジュールは、対応する CAN ネットワークについて、パラメータ Network := CanSMComMNetworkHandleRef、CurrentState := CANSM_BSWM_FULL_COMMUNICATION を指定して API BswM_CanSM_CurrentState を呼ばなければならない。(SRS_Can_01142, SRS_Can_01145)

> 学習メモ: 条項どおりの順序は「①CanIf_SetPduMode → ②ComM_BusSM_ModeIndication(FULL) → ③BswM_CanSM_CurrentState(FULL)」である。

#### 7.2.15 エフェクト: E_FULL_TO_SILENT_COM

**[SWS_CanSM_00434]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_FULL_TO_SILENT_COM は、**最初に**、対応する CAN ネットワークについて、パラメータ Network := CanSMComMNetworkHandleRef、CurrentState := CANSM_BSWM_SILENT_COMMUNICATION を指定して API BswM_CanSM_CurrentState を呼ばなければならない。(SRS_Can_01142, SRS_Can_01145)

**[SWS_CanSM_00541]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_FULL_TO_SILENT_COM は、**2 番目に**、CAN ネットワークに設定された各 CAN コントローラについて、パラメータ ControllerId := CanSMControllerId（ECUC_CanSM_00141 参照）、PduModeRequest := CANIF_TX_OFFLINE を指定して API CanIf_SetPduMode を呼ばなければならない。(SRS_Can_01142, SRS_Can_01145)

**[SWS_CanSM_00538]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_FULL_TO_SILENT_COM は、**4 番目に**、対応する CAN ネットワークについて、パラメータ Channel := CanSMComMNetworkHandleRef（ECUC_CanSM_00161 参照）、ComMode := COMM_SILENT_COMMUNICATION を指定して API ComM_BusSM_ModeIndication を呼ばなければならない。(SRS_Can_01142, SRS_Can_01145)

> 学習メモ: 3 番目の処理は本節の抜粋には現れない（原本の別条項または図を参照）。

#### 7.2.16 エフェクト: E_BR_END_FULL_COM

**[SWS_CanSM_00432]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_BR_END_FULL_COM は、E_FULL_COM（7.2.14 節参照）と同じでなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.17 エフェクト: E_BR_END_SILENT_COM

**[SWS_CanSM_00433]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_BR_END_SILENT_COM は、E_FULL_TO_SILENT_COM（7.2.15 節参照）と同じでなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.18 エフェクト: E_SILENT_TO_FULL_COM

**[SWS_CanSM_00550]** CanSM_BSM ステートマシン（図 7-1 参照）のエフェクト E_SILENT_TO_FULL_COM は、E_FULL_COM（7.2.14 節参照）と同じでなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.19 サブステートマシン CANSM_BSM_WUVALIDATION

図 7-2: CANSM_BSM_WUVALIDATION、CANSM_BSM のサブステートマシン（原本の図を参照）。状態は S_TRCV_NORMAL（+ do / DO_SET_TRCV_MODE_NORMAL）→ S_TRCV_NORMAL_WAIT → S_CC_STOPPED（+ do / DO_SET_CC_MODE_STOPPED）→ S_CC_STOPPED_WAIT → S_CC_STARTED（+ do / DO_SET_CC_MODE_STARTED）→ S_CC_STARTED_WAIT → WAIT_WUVALIDATION_LEAVE の順に進み、各 WAIT 状態でタイムアウトすると前の状態へ戻って再要求する。

##### 7.2.19.1 状態の do 動作: S_TRCV_NORMAL

**[SWS_CanSM_00623]** CAN ネットワークに CAN トランシーバが設定されている場合（ECUC_CanSM_00137 参照）、サブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）が状態 S_TRCV_NORMAL にある間、CanSM モジュールは do 動作 DO_SET_TRCV_MODE_NORMAL を実行しなければならない。すなわち、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に対して、TransceiverMode が CANTRCV_TRCVMODE_NORMAL に等しい API 要求 CanIf_SetTrcvMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.2 ガード条件 G_TRCV_NORMAL_E_OK

**[SWS_CanSM_00624]** サブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）のガード条件 G_TRCV_NORMAL_E_OK は、SWS_CanSM_00483 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.3 トリガ: T_TRCV_NORMAL_INDICATED

**[SWS_CanSM_00625]** CanSM モジュールが、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）について、前述の要求（SWS_CanSM_00623 参照）の後に CANTRCV_TRCVMODE_NORMAL のモード通知（SWS_CanSM_00399 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）を T_TRCV_NORMAL_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.4 トリガ: T_TRCV_NORMAL_TIMEOUT

**[SWS_CanSM_00626]** 期待されるトランシーバのノーマル通知（SWS_CanSM_00625 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）を T_TRCV_NORMAL_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.5 状態の do 動作: S_CC_STOPPED

**[SWS_CanSM_00627]** サブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）が状態 S_CC_STOPPED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STOPPED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STOPPED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.6 ガード条件: G_CC_STOPPED_OK

**[SWS_CanSM_00628]** サブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）のガード条件 G_CC_STOPPED_OK は、SWS_CanSM_00627 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.7 トリガ: T_CC_STOPPED_INDICATED

**[SWS_CanSM_00629]** CanSM モジュールが、CAN ネットワークの CAN コントローラを停止する要求（SWS_CanSM_00627 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）を T_CC_STOPPED_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.8 トリガ: T_CC_STOPPED_TIMEOUT

**[SWS_CanSM_00630]** 期待されるすべてのコントローラ停止のモード通知（SWS_CanSM_00629 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）を T_CC_STOPPED_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.9 状態の do 動作: S_CC_STARTED

**[SWS_CanSM_00631]** サブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）が状態 S_CC_STARTED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STARTED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STARTED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.10 ガード条件: G_CC_STARTED_E_OK

**[SWS_CanSM_00632]** サブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）のガード条件 G_CC_STARTED_OK は、SWS_CanSM_00631 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.11 トリガ: T_CC_STARTED_INDICATED

**[SWS_CanSM_00633]** CanSM モジュールが、CAN ネットワークの CAN コントローラを開始する要求（SWS_CanSM_00631 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）を T_CC_STARTED_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.19.12 トリガ: T_CC_STARTED_TIMEOUT

**[SWS_CanSM_00634]** 期待されるすべてのコントローラ開始のモード通知（SWS_CanSM_00633 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_WUVALIDATION（図 7-2 参照）を T_CC_STARTED_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.20 サブステートマシン: CANSM_BSM_S_PRE_NOCOM

図 7-3: CANSM_BSM_S_PRE_NOCOM、CANSM_BSM のサブステートマシン（原本の図を参照）。入口で `CANSM_BSM_G_PN_NOT_SUPPORTED` なら `CANSM_BSM_DeinitPnNotSupported` へ、`CANSM_BSM_G_PN_SUPPORTED` なら `CANSM_BSM_DeinitPnSupported` へ進む。

##### 7.2.20.1 ガード条件: CANSM_BSM_G_PN_NOT_SUPPORTED

**[SWS_CanSM_00436]** サブステートマシン CANSM_BSM_S_PRE_NO_COM（図 7-3 参照）のガード条件 CANSM_BSM_G_PN_NOT_SUPPORTED は、参照 CanSMTransceiverId（ECUC_CanSM_00137 参照）経由で得られるコンフィギュレーションパラメータ CanTrcvPnEnabled（[9]、ECUC_CanTrcv_00172 参照）が FALSE であるか、または CanSMTransceiverId がまったく設定されていないかを評価しなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.20.2 ガード条件: CANSM_BSM_G_PN_SUPPORTED

**[SWS_CanSM_00437]** サブステートマシン CANSM_BSM_S_PRE_NO_COM（図 7-3 参照）のガード条件 CANSM_BSM_G_PN_SUPPORTED は、CanSMTransceiverId（ECUC_CanSM_00137 参照）が設定されており、かつ参照 CanSMTransceiverId（ECUC_CanSM_00137 参照）経由で得られるコンフィギュレーションパラメータ CanTrcvPnEnabled（[9]、ECUC_CanTrcv_00172 参照）が TRUE かを評価しなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.20.3 サブステートマシン: CANSM_BSM_DeInitPnSupported

図 7-4: CANSM_BSM_DeinitPnSupported、CANSM_BSM_S_PRE_NOCOM のサブステートマシン（原本の図を参照）。状態は次の順に進む（各 WAIT 状態でタイムアウトすると前の状態へ戻る）。

`S_PN_CLEAR_WUF` → `S_PN_CC_STOPPED` → `S_TRCV_NORMAL` → `S_TRCV_STANDBY` → `S_CC_SLEEP` → `S_CHECK_WFLAG_IN_CC_SLEEP`（または `S_CHECK_WFLAG_IN_NOT_CC_SLEEP`）→ 出口

###### 7.2.20.3.1 状態の do 動作: S_PN_CLEAR_WUF

**[SWS_CanSM_00438]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が状態 S_PN_CLEAR_WUF にある間、CanSM モジュールは do 動作 DO_CLEAR_TRCV_WUF を実行する。すなわち、API 要求 CanIf_ClrTrcvWufFlag（8.5.1 節参照）を繰り返し、設定されたトランシーバ（ECUC_CanSM_00137 参照）を API 関数のパラメータとして使う。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.2 ガード条件: G_PN_CLEAR_WUF_E_OK

**[SWS_CanSM_00439]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）のガード条件 G_PN_CLEAR_WUF_E_OK は、SWS_CanSM_00438 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.3 トリガ: T_CLEAR_WUF_INDICATED

**[SWS_CanSM_00440]** コールバック関数 CanSM_ClearTrcvWufFlagIndication（SWS_CanSM_00413 参照）は、CanSM_ClearTrcvWufFlagIndication の関数パラメータ Transceiver が、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に一致する場合、その CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_CLEAR_WUF_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.4 トリガ: T_CLEAR_WUF_TIMEOUT

**[SWS_CanSM_00443]** コールバック関数 CanSM_ClearTrcvWufFlagIndication（SWS_CanSM_00440 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_CLEAR_WUF_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.5 状態の do 動作: S_PN_CC_STOPPED

**[SWS_CanSM_00441]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が状態 S_PN_CC_STOPPED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STOPPED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STOPPED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.6 ガード条件: G_CC_STOPPED_E_OK

**[SWS_CanSM_00442]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）のガード条件 G_CC_STOPPED_E_OK は、SWS_CanSM_00441 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.7 トリガ: T_CC_STOPPED_INDICATED

**[SWS_CanSM_00444]** CanSM モジュールが、CAN ネットワークの CAN コントローラを停止する要求（SWS_CanSM_00442 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_CC_STOPPED_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.8 トリガ: T_CC_STOPPED_TIMEOUT

**[SWS_CanSM_00445]** 期待されるすべてのコントローラ停止のモード通知（SWS_CanSM_00444 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_CC_STOPPED_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.9 状態の do 動作: S_TRCV_NORMAL

**[SWS_CanSM_00446]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が状態 S_TRCV_NORMAL にある間、CanSM モジュールは do 動作 DO_SET_TRCV_MODE_NORMAL を実行しなければならない。すなわち、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に対して、TransceiverMode が CANTRCV_TRCVMODE_NORMAL に等しい API 要求 CanIf_SetTrcvMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.10 ガード条件: G_TRCV_NORMAL_E_OK

**[SWS_CanSM_00447]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）のガード条件 G_TRCV_NORMAL_E_OK は、SWS_CanSM_00446 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.11 トリガ: T_TRCV_NORMAL_INDICATED

**[SWS_CanSM_00448]** CanSM モジュールが、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）について、前述の要求（SWS_CanSM_00446 参照）の後に CANTRCV_TRCVMODE_NORMAL のモード通知（SWS_CanSM_00399 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_TRCV_NORMAL_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.12 トリガ: T_TRCV_NORMAL_TIMEOUT

**[SWS_CanSM_00449]** 期待されるトランシーバのノーマル通知（SWS_CanSM_00448 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_TRCV_NORMAL_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.13 状態の do 動作: S_TRCV_STANDBY

**[SWS_CanSM_00450]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が状態 S_TRCV_STANDBY にある間、CanSM モジュールは do 動作 DO_SET_TRCV_STANDBY を実行しなければならない。すなわち、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に対して、TransceiverMode が CANTRCV_TRCVMODE_STANDBY に等しい API 要求 CanIf_SetTrcvMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.14 ガード条件: G_TRCV_STANDBY_E_OK

**[SWS_CanSM_00451]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）のガード条件 G_TRCV_STANDBY_E_OK は、SWS_CanSM_00450 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.15 トリガ: T_TRCV_STANDBY_INDICATED

**[SWS_CanSM_00452]** CanSM モジュールが、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）について、前述の要求（SWS_CanSM_00450 参照）の後に CANTRCV_TRCVMODE_STANDBY のモード通知（SWS_CanSM_00399 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_TRCV_STANDBY_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.16 トリガ: T_TRCV_STANDBY_TIMEOUT

**[SWS_CanSM_00454]** 期待されるトランシーバのスタンバイ通知（SWS_CanSM_00452 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_TRCV_STANDBY_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.17 状態の do 動作: S_CC_SLEEP

**[SWS_CanSM_00453]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が状態 S_CC_SLEEP にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_SLEEP を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_SLEEP に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.18 ガード条件: G_CC_SLEEP_E_OK

**[SWS_CanSM_00455]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）のガード条件 G_CC_SLEEP_E_OK は、SWS_CanSM_00453 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.19 トリガ: T_CC_SLEEP_INDICATED

**[SWS_CanSM_00456]** CanSM モジュールが、CAN ネットワークの CAN コントローラをスリープモードにする要求（SWS_CanSM_00453 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_CC_SLEEP_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.20 トリガ: CANSM_BSM_T_CC_SLEEP_TIMEOUT

**[SWS_CanSM_00457]** 期待されるすべてのコントローラのスリープモード通知（SWS_CanSM_00456 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を CANSM_BSM_T_CC_SLEEP_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.21 状態の do 動作: S_CHECK_WFLAG_IN_CC_SLEEP

**[SWS_CanSM_00458]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が状態 S_CHECK_WFLAG_IN_CC_SLEEP にある間、CanSM モジュールは do 動作 DO_CHECK_WFLAG を実行する。すなわち、API 要求 CanIf_CheckTrcvWakeFlag（8.5.1 節参照）を繰り返し、関連するネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）を Transceiver パラメータとして使う。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.22 ガード条件: G_CHECK_WFLAG_E_OK

**[SWS_CanSM_00459]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）のガード条件 G_CHECK_WFLAG_E_OK は、SWS_CanSM_00458 または SWS_CanSM_00462 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.23 トリガ: T_CHECK_WFLAG_INDICATED

**[SWS_CanSM_00460]** コールバック関数 CanSM_CheckTransceiverWakeFlagIndication（SWS_CanSM_00416 参照）は、CanSM_CheckTransceiverWakeFlagIndication の関数パラメータ Transceiver が、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に一致する場合、その CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_CHECK_WFLAG_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.24 トリガ: T_CHECK_WFLAG_TIMEOUT

**[SWS_CanSM_00461]** コールバック関数 CanSM_CheckTransceiverWakeFlagIndication（SWS_CanSM_00460 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）を T_CHECK_WFLAG_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.25 状態の do 動作: S_CHECK_WFLAG_IN_NOT_CC_SLEEP

**[SWS_CanSM_00462]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が状態 S_CHECK_WFLAG_IN_NOT_CC_SLEEP にある間、CanSM モジュールは do 動作 DO_CHECK_WFLAG を実行する。すなわち、API 要求 CanIf_CheckTrcvWakeFlag（8.5.1 節参照）を繰り返し、関連するネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）を Transceiver パラメータとして使う。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.3.26 トリガ: T_REPEAT_MAX

**[SWS_CanSM_00463]** サブステートマシン CANSM_BSM_DeinitPnSupported（図 7-4 参照）が、CanIf の API 呼び出し（SWS_CanSM_00438、SWS_CanSM_00441、SWS_CanSM_00446、SWS_CanSM_00450、SWS_CanSM_00453、SWS_CanSM_00458、SWS_CanSM_00462 参照）のいずれかを、設定された回数（ECUC_CanSM_00335 参照）より多く繰り返しても、戻り値 E_OK が得られず、かつ期待されるモード通知コールバック（SWS_CanSM_00444、SWS_CanSM_00448、SWS_CanSM_00452、SWS_CanSM_00456、SWS_CanSM_00460 参照）も得られなかった場合、これはサブステートマシン CANSM_BSM_DeinitPnSupported を T_REPEAT_MAX でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

##### 7.2.20.4 サブステートマシン: CANSM_BSM_DeInitPnNotSupported

図 7-5: CANSM_BSM_DeinitPnNotSupported、CANSM_BSM_S_PRE_NOCOM のサブステートマシン（原本の図を参照）。状態は次の順に進む（各 WAIT 状態でタイムアウトすると前の状態へ戻る。T_REPEAT_MAX で出口）。

`S_CC_STOPPED` → `S_CC_SLEEP` → `S_TRCV_NORMAL` → `S_TRCV_STANDBY` → 出口

###### 7.2.20.4.1 状態の do 動作: S_CC_STOPPED

**[SWS_CanSM_00464]** サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）が状態 S_CC_STOPPED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STOPPED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STOPPED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.2 ガード条件: CANSM_BSM_G_CC_STOPPED_OK

**[SWS_CanSM_00465]** サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）のガード条件 CANSM_BSM_G_CC_STOPPED_OK は、SWS_CanSM_00464 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.3 トリガ: T_CC_STOPPED_INDICATED

**[SWS_CanSM_00466]** CanSM モジュールが、CAN ネットワークの CAN コントローラを停止する要求（SWS_CanSM_00464 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_CC_STOPPED_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.4 トリガ: T_CC_STOPPED_TIMEOUT

**[SWS_CanSM_00467]** 期待されるすべてのコントローラ停止のモード通知（SWS_CanSM_00466 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_CC_STOPPED_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.5 状態の do 動作: S_CC_SLEEP

**[SWS_CanSM_00468]** サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）が状態 S_CC_SLEEP にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_SLEEP を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_SLEEP に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.6 ガード条件: G_CC_SLEEP_E_OK

**[SWS_CanSM_00469]** サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）のガード条件 G_CC_SLEEP_E_OK は、SWS_CanSM_00468 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.7 トリガ: T_CC_SLEEP_INDICATED

**[SWS_CanSM_00470]** CanSM モジュールが、CAN ネットワークの CAN コントローラをスリープモードにする要求（SWS_CanSM_00468 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_CC_SLEEP_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.8 トリガ: T_CC_SLEEP_TIMEOUT

**[SWS_CanSM_00471]** 期待されるすべてのコントローラのスリープモード通知（SWS_CanSM_00470 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_CC_SLEEP_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.9 状態の do 動作: S_TRCV_NORMAL

**[SWS_CanSM_00472]** CAN ネットワークに CAN トランシーバが設定されている場合（ECUC_CanSM_00137 参照）、サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）が状態 S_TRCV_NORMAL にある間、CanSM モジュールは do 動作 DO_SET_TRCV_MODE_NORMAL を実行しなければならない。すなわち、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に対して、TransceiverMode が CANTRCV_TRCVMODE_NORMAL に等しい API 要求 CanIf_SetTrcvMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.10 ガード条件: G_TRCV_NORMAL_E_OK

**[SWS_CanSM_00473]** サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）のガード条件 G_TRCV_NORMAL_E_OK は、SWS_CanSM_00472 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.11 トリガ: T_TRCV_NORMAL_INDICATED

**[SWS_CanSM_00474]** CanSM モジュールが、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）について、前述の要求（SWS_CanSM_00472 参照）の後に CANTRCV_TRCVMODE_NORMAL のモード通知（SWS_CanSM_00399 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_TRCV_NORMAL_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

**[SWS_CanSM_00556]** CAN ネットワークに CAN トランシーバが設定されていない場合、これは、状態 S_TRCV_NORMAL にあるその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_TRCV_NORMAL_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.12 トリガ: T_TRCV_NORMAL_TIMEOUT

**[SWS_CanSM_00475]** 期待されるトランシーバのノーマル通知（SWS_CanSM_00474 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_TRCV_NORMAL_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.13 状態の do 動作: S_TRCV_STANDBY

**[SWS_CanSM_00476]** CAN ネットワークに CAN トランシーバが設定されている場合（ECUC_CanSM_00137 参照）、サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）が状態 S_TRCV_STANDBY にある間、CanSM モジュールは do 動作 DO_SET_TRCV_MODE_STANDBY を実行しなければならない。すなわち、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に対して、TransceiverMode が CANTRCV_TRCVMODE_STANDBY に等しい API 要求 CanIf_SetTrcvMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.14 ガード条件: G_TRCV_STANDBY_E_OK

**[SWS_CanSM_00477]** サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）のガード条件 G_TRCV_STANDBY_E_OK は、SWS_CanSM_00476 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.15 トリガ: T_TRCV_STANDBY_INDICATED

**[SWS_CanSM_00478]** CanSM モジュールが、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）について、前述の要求（SWS_CanSM_00476 参照）の後に CANTRCV_TRCVMODE_STANDBY のモード通知（SWS_CanSM_00399 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_TRCV_STANDBY_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

**[SWS_CanSM_00557]** CAN ネットワークに CAN トランシーバが設定されていない場合（ECUC_CanSM_00137 参照）、これは、状態 S_TRCV_STANDBY にあるその CAN ネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を T_TRCV_STANDBY_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.16 トリガ: CANSM_BSM_T_TRCV_STANDBY_TIMEOUT

**[SWS_CanSM_00479]** 期待されるトランシーバのスタンバイ通知（SWS_CanSM_00478 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）を CANSM_BSM_T_TRCV_STANDBY_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

###### 7.2.20.4.17 トリガ: T_REPEAT_MAX

**[SWS_CanSM_00480]** サブステートマシン CANSM_BSM_DeinitPnNotSupported（図 7-5 参照）が、CanIf の API 呼び出し（SWS_CanSM_00464、SWS_CanSM_00468、SWS_CanSM_00472、SWS_CanSM_00476 参照）のいずれかを、設定された回数（ECUC_CanSM_00335 参照）より多く繰り返しても、戻り値 E_OK が得られず、かつ期待されるモード通知コールバック（SWS_CanSM_00466、SWS_CanSM_00470、SWS_CanSM_00474、SWS_CanSM_00478 参照）も得られなかった場合、これはサブステートマシン CANSM_BSM_DeinitPnNotSupported を T_REPEAT_MAX でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145)

#### 7.2.21 サブステートマシン: CANSM_BSM_S_SILENTCOM_BOR

図 7-6: CANSM_BSM_S_SILENTCOM_BOR、CANSM_BSM のサブステートマシン（原本の図を参照）。入口で E_BUS_OFF を実行して `S_RESTART_CC`（do / DO_SET_CC_MODE_STARTED）へ進み、`S_RESTART_CC_WAIT` で通知を待つ。通知を受けると E_TX_OFF を実行して出口へ進む。タイムアウトすると `S_RESTART_CC` へ戻る。

##### 7.2.21.1 エフェクト: E_BUS_OFF

**[SWS_CanSM_00605]** サブステートマシン CANSM_BSM_S_SILENTCOM_BOR（図 7-6 参照）のエフェクト E_BUS_OFF は、パラメータ EventId := CANSM_E_BUS_OFF（ECUC_CanSM_00070 参照）、EventStatus := DEM_EVENT_STATUS_PRE_FAILED を指定して Dem_SetEventStatus（8.5.1 節参照）を呼び出さなければならない。(SRS_BSW_00422)

> 注: 原本の本文は「CANSM_BSM_S_FULLCOM CANSM_BSM_S_SILENTCOM_BOR」と 2 つの名前が連続して書かれている（原本の誤記と思われる）。

##### 7.2.21.2 状態の動作: S_RESTART_CC

**[SWS_CanSM_00604]** サブステートマシン CANSM_BSM_S_SILENTCOM_BOR（図 7-6 参照）が状態 S_RESTART_CC にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STARTED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STARTED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01142, SRS_Can_01145, SRS_Can_01144, SRS_Can_01146)

##### 7.2.21.3 G_RESTART_CC_E_OK

**[SWS_CanSM_00603]** サブステートマシン CANSM_BSM_S_SILENTCOM_BOR（図 7-6 参照）のガード条件 G_RESTART_CC_OK は、SWS_CanSM_00604 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01142, SRS_Can_01145, SRS_Can_01144, SRS_Can_01146)

##### 7.2.21.4 トリガ: T_RESTART_CC_INDICATED

**[SWS_CanSM_00600]** CanSM モジュールが、CAN ネットワークの CAN コントローラを開始する要求（SWS_CanSM_00604 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステート CANSM_BSM_S_SILENTCOM_BOR（図 7-6 参照）を T_RESTART_CC_INDICATED でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145, SRS_Can_01144, SRS_Can_01146)

##### 7.2.21.5 T_RESTART_CC_TIMEOUT

**[SWS_CanSM_00602]** 期待されるすべてのコントローラ開始のモード通知（SWS_CanSM_00600 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_SILENTCOM_BOR（図 7-6 参照）を T_RESTART_CC_TIMEOUT でトリガしなければならない。(SRS_Can_01142, SRS_Can_01145, SRS_Can_01144, SRS_Can_01146)

##### 7.2.21.6 エフェクト: E_TX_OFF

エフェクト E_TX_OFF は何もしてはならない（CAN コントローラの再起動後の既定の PDU モードはすでに TX OFF であるため。CanIf 仕様書参照）。

#### 7.2.22 サブステートマシン: CANSM_BSM_S_PRE_FULLCOM

図 7-7: CANSM_BSM_S_PRE_FULLCOM、CANSM_BSM のサブステートマシン（原本の図を参照）。状態は `S_TRCV_NORMAL` → `S_CC_STOPPED` → `S_CC_STARTED` の順に進み（各 WAIT 状態でタイムアウトすると前の状態へ戻る）、完了すると FULLCOM へ向かう出口へ進む。T_REPEAT_MAX の場合は REPEAT_MAX の出口へ進む。

##### 7.2.22.1 状態の do 動作: S_TRCV_NORMAL

**[SWS_CanSM_00483]** CAN ネットワークに CAN トランシーバが設定されている場合（ECUC_CanSM_00137 参照）、サブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）が状態 S_TRCV_NORMAL にある間、CanSM モジュールは do 動作 DO_SET_TRCV_MODE_NORMAL を実行しなければならない。すなわち、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）に対して、TransceiverMode が CANTRCV_TRCVMODE_NORMAL に等しい API 要求 CanIf_SetTrcvMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.2 ガード条件: G_TRCV_NORMAL_E_OK

**[SWS_CanSM_00484]** サブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）のガード条件 G_TRCV_NORMAL_E_OK は、SWS_CanSM_00483 の API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.3 トリガ: T_TRCV_NORMAL_INDICATED

**[SWS_CanSM_00485]** CanSM モジュールが、その CAN ネットワークに設定された CAN トランシーバ（ECUC_CanSM_00137 参照）について、前述の要求（SWS_CanSM_00483 参照）の後に CANTRCV_TRCVMODE_NORMAL のモード通知（SWS_CanSM_00399 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）を T_TRCV_NORMAL_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00558]** CAN ネットワークに CAN トランシーバが設定されていない場合（ECUC_CanSM_00137 参照）、これは、状態 S_TRCV_NORMAL にあるその CAN ネットワークのサブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）を T_TRCV_NORMAL_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.4 トリガ: T_TRCV_NORMAL_TIMEOUT

**[SWS_CanSM_00486]** 期待されるトランシーバのノーマル通知（SWS_CanSM_00485 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）を T_TRCV_NORMAL_TIMEOUT でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.5 状態の do 動作: S_CC_STOPPED

**[SWS_CanSM_00487]** サブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）が状態 S_CC_STOPPED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STOPPED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STOPPED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.6 ガード条件: G_CC_STOPPED_OK

**[SWS_CanSM_00488]** サブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）のガード条件 G_CC_STOPPED_OK は、SWS_CanSM_00487 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.7 トリガ: T_CC_STOPPED_INDICATED

**[SWS_CanSM_00489]** CanSM モジュールが、CAN ネットワークの CAN コントローラを停止する要求（SWS_CanSM_00487 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）を T_CC_STOPPED_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.8 トリガ: T_CC_STOPPED_TIMEOUT

**[SWS_CanSM_00490]** 期待されるすべてのコントローラ停止のモード通知（SWS_CanSM_00489 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）を T_CC_STOPPED_TIMEOUT でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.9 状態の do 動作: S_CC_STARTED

**[SWS_CanSM_00491]** サブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）が状態 S_CC_STARTED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STARTED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STARTED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.10 ガード条件: G_CC_STARTED_OK

**[SWS_CanSM_00492]** サブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）のガード条件 G_CC_STARTED_OK は、SWS_CanSM_00491 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.11 トリガ: T_CC_STARTED_INDICATED

**[SWS_CanSM_00493]** CanSM モジュールが、CAN ネットワークの CAN コントローラを開始する要求（SWS_CanSM_00491 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）を T_CC_STARTED_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.12 トリガ: T_CC_STARTED_TIMEOUT

**[SWS_CanSM_00494]** 期待されるすべてのコントローラ開始のモード通知（SWS_CanSM_00493 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）を T_CC_STARTED_TIMEOUT でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.22.13 トリガ: T_REPEAT_MAX

**[SWS_CanSM_00495]** サブステートマシン CANSM_BSM_S_PRE_FULLCOM（図 7-7 参照）が、CanIf の API 呼び出し（SWS_CanSM_00483、SWS_CanSM_00487、SWS_CanSM_00491 参照）のいずれかを、設定された回数（ECUC_CanSM_00335 参照）より多く繰り返しても、戻り値 E_OK が得られず、かつ期待されるモード通知コールバック（SWS_CanSM_00485、SWS_CanSM_00489、SWS_CanSM_00493 参照）も得られなかった場合、これはサブステートマシン CANSM_BSM_S_PRE_FULLCOM を T_REPEAT_MAX でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

#### 7.2.23 サブステートマシン CANSM_BSM_S_FULLCOM

図 7-8: CANSM_BSM_S_FULLCOM、CANSM_BSM のサブステートマシン（原本の図を参照）。主な状態は `S_NO_BUS_OFF`、`S_BUS_OFF_CHECK`、`S_RESTART_CC`、`S_RESTART_CC_WAIT`、`S_TX_OFF`、`S_TX_TIMEOUT_EXCEPTION`。

##### 7.2.23.1 ガード条件: G_BUS_OFF_PASSIVE

**[SWS_CanSM_00496]** CANSM_BOR_TX_CONFIRMATION_POLLING が無効であり（ECUC_CanSM_00339 参照）、かつエフェクト E_TX_ON からの経過時間が、コンフィギュレーションパラメータ CANSM_BOR_TIME_TX_ENSURED（ECUC_CanSM_00130 参照）以上である場合、サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のガード条件 G_BUS_OFF_PASSIVE は通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00497]** CANSM_BOR_TX_CONFIRMATION_POLLING が有効であり（ECUC_CanSM_00339 参照）、かつ API CanIf_GetTxConfirmationState（8.5.1 節参照）が、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）について CANIF_TX_RX_NOTIFICATION を返す場合、サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のガード条件 G_BUS_OFF_PASSIVE は通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.2 エフェクト: E_BUS_OFF_PASSIVE

**[SWS_CanSM_00498]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_BUS_OFF_PASSIVE は、パラメータ EventId := CANSM_E_BUS_OFF（ECUC_CanSM_00070 参照）、EventStatus := DEM_EVENT_STATUS_PASSED を指定して Dem_SetEventStatus（8.5.1 節参照）を呼び出さなければならない。(SRS_BSW_00422)

##### 7.2.23.3 トリガ: T_SILENT_COM_MODE_REQUEST

**[SWS_CanSM_00499]** パラメータ ComM_Mode が COMM_SILENT_COMMUNICATION に等しい API 要求 CanSM_RequestComMode（SWS_CanSM_00635 参照）は、関数パラメータ network とコンフィギュレーションパラメータ CANSM_NETWORK_HANDLE（ECUC_CanSM_00161 参照）に対応するサブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）を T_SILENT_COM_MODE_REQUEST でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

根拠: CanNm のネットワークモードから CanNm の Prepare Bus-Sleep モードへ遷移する場合の通常のユースケースである。

**[SWS_CanSM_00554]** パラメータ ComM_Mode が COMM_NO_COMMUNICATION に等しい API 要求 CanSM_RequestComMode（SWS_CanSM_00635 参照）は、関数パラメータ network とコンフィギュレーションパラメータ CANSM_NETWORK_HANDLE（ECUC_CanSM_00161 参照）に対応するサブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）を T_SILENT_COM_MODE_REQUEST でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

備考: ComM のコンフィギュレーションにより、ComM モジュールは、まず COMM_SILENT_COMMUNICATION を要求してから COMM_NO_COMMUNICATION を要求するか、または COMM_NO_COMMUNICATION を直接要求する（ComMNmVariant=LIGHT）。

##### 7.2.23.4 トリガ: T_CHANGE_BR_REQUEST

**[SWS_CanSM_00507]** CanSM_SetBaudrate 要求（SWS_CANSM_00503 参照）を拒否する条件がない場合、これはステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）と、それぞれの親ステートマシン CANSM_BSM（図 7-1 参照）を T_CHANGE_BR_REQUEST でトリガする（可能なら CanIf_SetBaudrate による直接のボーレート変更、または、それを行うために必要な非同期処理の開始を引き起こす）。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.5 エフェクト: E_CHANGE_BR_BSWM_MODE

**[SWS_CanSM_00528]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_CHANGE_BR_BSWM_MODE は、対応する CAN ネットワークについて、パラメータ Network := CanSMComMNetworkHandleRef、CurrentState := CANSM_BSWM_CHANGE_BAUDRATE を指定して API BswM_CanSM_CurrentState を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.6 トリガ: T_BUS_OFF

**[SWS_CanSM_00500]** コールバック関数 CanSM_ControllerBusOff（SWS_CanSM_00064 参照）は、その CAN ネットワークに設定された CAN コントローラのいずれかが、コールバック関数 CanSM_ControllerBusOff の関数パラメータ ControllerId に一致する場合、その CAN ネットワークのサブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）を T_BUS_OFF でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00653]** 1 つの CAN ネットワークに複数の CAN コントローラが属し、そのうちの 1 つのコントローラについて CanSM_ControllerBusOff でバスオフが通知された場合、CanSM は、エフェクト E_BUS_OFF の中で、その CAN ネットワークの他の CAN コントローラも停止しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.7 エフェクト: E_BUS_OFF

**[SWS_CanSM_00508]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_BUS_OFF は、**最初に**、対応する CAN ネットワークについて、パラメータ Network := CanSMComMNetworkHandleRef、CurrentState := CANSM_BSWM_BUS_OFF を指定して API BswM_CanSM_CurrentState を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00521]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_BUS_OFF は、**2 番目に**、対応する CAN ネットワークについて、パラメータ Channel := CanSMComMNetworkHandleRef（ECUC_CanSM_00161 参照）、ComMode := COMM_SILENT_COMMUNICATION を指定して API ComM_BusSM_ModeIndication を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00522]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_BUS_OFF は、パラメータ EventId := CANSM_E_BUS_OFF（ECUC_CanSM_00070 参照）、EventStatus := DEM_EVENT_STATUS_PRE_FAILED を指定して Dem_SetEventStatus（8.5.1 節参照）を呼び出さなければならない。(SRS_BSW_00422)

> 学習メモ: バスオフ時の通知順は「①BswM_CanSM_CurrentState(BUS_OFF) → ②ComM_BusSM_ModeIndication(SILENT) → Dem(PRE_FAILED)」である。バスオフ中は ComM のモードが SILENT_COMMUNICATION になるため、BswM が ComM のモードだけを条件にしていると、バスオフからの復帰を見分けられない。

##### 7.2.23.8 状態の do 動作: S_RESTART_CC

**[SWS_CanSM_00509]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）が状態 S_RESTART_CC にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STARTED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STARTED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.9 ガード条件: G_RESTART_CC_OK

**[SWS_CanSM_00510]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のガード条件 G_RESTART_CC_OK は、SWS_CanSM_00509 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.10 トリガ: T_RESTART_CC_INDICATED

**[SWS_CanSM_00511]** CanSM モジュールが、CAN ネットワークの CAN コントローラを開始する要求（SWS_CanSM_00509 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステート CANSM_BSM_S_FULLCOM（図 7-8 参照）を T_RESTART_CC_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.11 トリガ: T_RESTART_CC_TIMEOUT

**[SWS_CanSM_00512]** 期待されるすべてのコントローラ開始のモード通知（SWS_CanSM_00511 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）を T_RESTART_CC_TIMEOUT でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.12 エフェクト: E_TX_OFF

エフェクト E_TX_OFF は何もしてはならない。

##### 7.2.23.13 ガード条件: G_TX_ON

**[SWS_CanSM_00514]** CanSMEnableBusOffDelay が FALSE の場合、サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のガード条件 G_TX_ON は、直近の T_BUS_OFF から CanSMBorTimeL1（ECUC_CanSM_00128 参照）の時間が経過した後に通過しなければならない。ただし、ガード条件 G_BUS_OFF_PASSIVE を通過しないままの E_BUS_OFF によるバスオフ回復のリトライ回数が CanSMBorCounterL1ToL2（ECUC_CanSM_00131 参照）より少ない場合に限る。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00515]** CanSMEnableBusOffDelay が FALSE の場合、サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のガード条件 G_TX_ON は、直近の T_BUS_OFF から CanSMBorTimeL2（ECUC_CanSM_00129 参照）の時間が経過した後に通過しなければならない。ただし、ガード条件 G_BUS_OFF_PASSIVE を通過しないままの E_BUS_OFF によるバスオフ回復のリトライ回数が CanSMBorCounterL1ToL2（ECUC_CanSM_00131 参照）以上の場合に限る。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00636]** CanSMEnableBusOffDelay が TRUE の場合、SWS_CANSM_00514 と SWS_CANSM_00515 のガード条件は、それぞれに規定された時間に、バスオフ事象の後に設定されたコールバック関数 `<User_GetBusOffDelay>` で要求されるランダムな追加遅延値を加えた時間が経過した後に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.14 エフェクト: E_TX_ON

**[SWS_CanSM_00516]** ECU passive が FALSE の場合（SWS_CanSM_00646 参照）、サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_TX_ON は、**最初に**、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）について、パラメータ ControllerId := CanSMControllerId（ECUC_CanSM_00141 参照）、PduModeRequest := CANIF_ONLINE を指定して API 関数 CanIf_SetPduMode（8.5.1 節参照）を呼ばなければならない。(SRS_Can_01158)

**[SWS_CanSM_00648]** ECU passive が TRUE の場合（SWS_CanSM_00646 参照）、サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_TX_ON は、**最初に**、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）について、パラメータ ControllerId := CanSMControllerId（ECUC_CanSM_00141 参照）、PduModeRequest := CANIF_TX_OFFLINE_ACTIVE を指定して API 関数 CanIf_SetPduMode（8.5.1 節参照）を呼ばなければならない。(SRS_Can_01158)

**[SWS_CanSM_00517]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_TX_ON は、**2 番目に**、対応する CAN ネットワークについて、パラメータ Network := CanSMComMNetworkHandleRef、CurrentState := CANSM_BSWM_FULL_COMMUNICATION を指定して API BswM_CanSM_CurrentState を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00518]** サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）のエフェクト E_TX_ON は、**3 番目に**、パラメータ Channel := CanSMComMNetworkHandleRef（ECUC_CanSM_00161 参照）、ComMode := COMM_FULL_COMMUNICATION を指定して API ComM_BusSM_ModeIndication を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

> 学習メモ: バスオフ回復後の E_TX_ON の順序は「①CanIf_SetPduMode → ②BswM_CanSM_CurrentState(FULL_COM) → ③ComM_BusSM_ModeIndication(FULL)」である。この回復経路では、BswM が先に FULL_COM を知り、ComM のモードが FULL に戻るのはその後になる（初回の E_FULL_COM（7.2.14）とは ComM と BswM の順序が逆）。

##### 7.2.23.15 トリガ: T_TX_TIMEOUT_EXCEPTION

**[SWS_CanSM_00584]** コールバック関数 CanSM_TxTimeoutException（SWS_CANSM_00410 参照）は、サブステートマシン CANSM_BSM_S_FULLCOM（図 7-8 参照）を T_TX_TIMEOUT_EXCEPTION でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.23.16 注記

状態 S_NO_BUS_OFF では、CanSM モジュールに必要な状態動作はない。

##### 7.2.23.17 サブステートマシン: CANSM_BSM_S_TX_TIMEOUT_EXCEPTION

図 7-9: CANSM_BSM_S_TX_TIMEOUT_EXCEPTION、CANSM_BSM_S_FULLCOM のサブステートマシン（原本の図を参照）。状態は `S_CC_STOPPED` → `S_CC_STARTED` の順に進み（各 WAIT 状態でタイムアウトすると前の状態へ戻る）、出口 TxTimeout へ進む。

###### 7.2.23.17.1 トリガ: T_CC_STOPPED_TIMEOUT

**[SWS_CanSM_00576]** 期待されるすべてのコントローラ停止のモード通知（SWS_CanSM_00579 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）を T_CC_STOPPED_TIMEOUT でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.2 ガード条件: G_CC_STOPPED_E_OK

**[SWS_CanSM_00577]** サブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）のガード条件 G_CC_STOPPED_E_OK は、SWS_CanSM_00578 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.3 状態の動作: DO_SET_CC_MODE_STOPPED()

**[SWS_CanSM_00578]** サブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）が状態 S_CC_STOPPED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STOPPED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STOPPED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.4 トリガ: T_CC_STOPPED_INDICATED

**[SWS_CanSM_00579]** CanSM モジュールが、CAN ネットワークの CAN コントローラを停止する要求（SWS_CanSM_00524 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）を T_CC_STOPPED_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.5 トリガ: T_CC_STARTED_INDICATED

**[SWS_CanSM_00580]** CanSM モジュールが、CAN ネットワークの CAN コントローラを開始する要求（SWS_CanSM_00582 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）を T_CC_STARTED_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.6 ガード条件: G_CC_STARTED_E_OK

**[SWS_CanSM_00581]** サブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）のガード条件 G_CC_STARTED_E_OK は、SWS_CanSM_00582 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.7 状態の動作: DO_SET_CC_MODE_STARTED

**[SWS_CanSM_00582]** サブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）が状態 S_CC_STARTED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STARTED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STARTED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.8 トリガ: T_REPEAT_MAX

**[SWS_CANSM_00575]** サブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）が、CAN ネットワークの CAN コントローラを再起動する CanIf API を、設定された回数（ECUC_CanSM_00335 参照）より多く繰り返しても、期待されるモード通知が得られなかった場合、これはサブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION を T_REPEAT_MAX でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

###### 7.2.23.17.9 出口: TxTimeout

**[SWS_CanSM_00655]** サブステートマシン CANSM_BSM_S_TX_TIMEOUT_EXCEPTION（図 7-9 参照）が T_CC_STARTED_INDICATED でトリガされた場合、T_REPEAT_MAX が発生していない限り、API CanIf_SetPduMode() を CANIF_ONLINE で呼ばなければならない。

#### 7.2.24 サブステートマシン: CANSM_BSM_S_CHANGE_BAUDRATE

図 7-10: CANSM_BSM_S_CHANGE_BAUDRATE、CANSM_BSM のサブステートマシン（原本の図を参照）。入口で `DO_SET_BAUDRATE_DIRECT` を実行し、成功（G_SET_BAUDRATE_DIRECT_OK）なら `S_CC_STOPPED` を経由せず、失敗（G_SET_BAUDRATE_DIRECT_NOT_OK）なら `S_CC_STOPPED` → エフェクト E_CHANGE_BAUDRATE → `S_CC_STARTED` の順に進む。最後に、直近の要求が NO_COM なら出口 NO_COM（G_NO_COM_MODE_REQUESTED）、そうでなければ出口 FULL_OR_SILENT_COM（G_NO_COM_MODE_NOT_REQUESTED）へ進む。T_REPEAT_MAX の場合は出口 NO_COM へ進む。

##### 7.2.24.1 入口での状態動作: DO_SET_BAUDRATE_DIRECT

**[SWS_CanSM_00639]** 状態動作 DO_SET_BAUDRATE_DIRECT（図 7-10 参照）は、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）について、それぞれの ControllerId パラメータで API 要求 CanIf_SetBaudrate（8.5.2 節参照）を呼ばなければならない。BaudRateConfigID パラメータには、CanSM_SetBaudrate() の呼び出しで渡された関数パラメータ BaudRateConfigID を使わなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.2 ガード条件: G_SET_BAUDRATE_DIRECT_OK

**[SWS_CanSM_00641]** すべての CanIf_SetBaudrate 要求（SWS_CanSM_00639 参照）が E_OK を返した場合、ガード条件 G_SET_BAUDRATE_DIRECT_OK を通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.3 ガード条件: G_SET_BAUDRATE_DIRECT_NOT_OK

**[SWS_CanSM_00642]** CanIf_SetBaudrate 要求（SWS_CanSM_00639 参照）のいずれかが E_NOT_OK を返した場合、状態 CANSM_BSM_CHANGE_BR_SYNC（図 7-10 参照）のガード条件 G_SET_BAUDRATE_NOT_OK を通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.4 状態の do 動作: S_CC_STOPPED

**[SWS_CanSM_00524]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）が状態 S_CC_STOPPED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STOPPED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STOPPED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.5 ガード条件: G_CC_STOPPED_OK

**[SWS_CanSM_00525]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）のガード条件 G_CC_STOPPED_OK は、SWS_CanSM_00524 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.6 トリガ: T_CC_STOPPED_INDICATED

**[SWS_CanSM_00526]** CanSM モジュールが、CAN ネットワークの CAN コントローラを停止する要求（SWS_CanSM_00524 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）を T_CC_STOPPED_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.7 トリガ: T_CC_STOPPED_TIMEOUT

**[SWS_CanSM_00527]** 期待されるすべてのコントローラ停止のモード通知（SWS_CanSM_00526 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）を T_CC_STOPPED_TIMEOUT でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.8 エフェクト: E_CHANGE_BAUDRATE

**[SWS_CanSM_00529]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）のエフェクト E_CHANGE_BAUDRATE は、**最初に**、対応する CAN ネットワークについて、パラメータ Channel := CanSMComMNetworkHandleRef（ECUC_CanSM_00161 参照）、ComMode := COMM_NO_COMMUNICATION を指定して API ComM_BusSM_ModeIndication を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00531]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）のエフェクト E_CHANGE_BAUDRATE は、**2 番目に**、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）について、それぞれの ControllerId パラメータで API 要求 CanIf_SetBaudrate（8.5.2 節参照）を呼ばなければならない。BaudRateConfigID パラメータには、CanSM_SetBaudrate() の呼び出しで記憶した BaudRateConfigID を使わなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.9 状態の do 動作: S_CC_STARTED

**[SWS_CanSM_00532]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）が状態 S_CC_STARTED にある間、CanSM モジュールは do 動作 DO_SET_CC_MODE_STARTED を実行しなければならない。すなわち、現在の CAN コントローラのモード（SWS_CanSM_00638 参照）が異なる場合、その CAN ネットワークに設定されたすべての CAN コントローラ（ECUC_CanSM_00141 参照）に対して、ControllerMode が CAN_CS_STARTED に等しい API 要求 CanIf_SetControllerMode（8.5.1 節参照）を繰り返さなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.10 ガード条件: G_CC_STARTED_OK

**[SWS_CanSM_00533]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）のガード条件 G_CC_STARTED_OK は、SWS_CanSM_00532 のすべての API 呼び出しが E_OK を返した場合に通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.11 トリガ: T_CC_STARTED_INDICATED

**[SWS_CanSM_00534]** CanSM モジュールが、CAN ネットワークの CAN コントローラを開始する要求（SWS_CanSM_00532 参照）の後に、その CAN ネットワークに設定された CAN コントローラ（ECUC_CanSM_00141 参照）のすべてのモード通知（SWS_CanSM_00396 参照）を受け取った場合、これはその CAN ネットワークのサブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）を T_CC_STARTED_INDICATED でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.12 トリガ: T_CC_STARTED_TIMEOUT

**[SWS_CanSM_00535]** 期待されるすべてのコントローラ開始のモード通知（SWS_CanSM_00534 参照）に対して CANSM_MODEREQ_REPEAT_TIME（ECUC_CanSM_00336 参照）がタイムアウトした後、この条件は、そのネットワークのサブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）を T_CC_STARTED_TIMEOUT でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.13 トリガ: T_REPEAT_MAX

**[SWS_CanSM_00536]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）が、対応する CAN ネットワークの CAN コントローラについて、参照された CanIf API（SWS_CanSM_00524、SWS_CanSM_00532 参照）を、設定された回数（ECUC_CanSM_00335 参照）より多く繰り返しても、戻り値 E_OK が得られず、かつ期待されるモード通知（SWS_CanSM_00526、SWS_CanSM_00534 参照）も得られなかった場合、これはサブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE を T_REPEAT_MAX でトリガしなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.14 ガード条件: G_NO_COM_MODE_REQUESTED

**[SWS_CanSM_00542]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）は、そのステートマシンのネットワークハンドルに対して CanSM_RequestComMode（SWS_CanSM_00635 参照）で受け付けた最新の通信モード要求が、パラメータ ComM_Mode が COMM_NO_COMMUNICATION に等しいものであった場合、ガード条件 G_NO_COM_MODE_REQUESTED を通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

##### 7.2.24.15 ガード条件: G_NO_COM_MODE_NOT_REQUESTED

**[SWS_CanSM_00543]** サブステートマシン CANSM_BSM_S_CHANGE_BAUDRATE（図 7-10 参照）は、そのステートマシンのネットワークハンドルに対して CanSM_RequestComMode（SWS_CanSM_00635 参照）で受け付けた最新の通信モード要求が、パラメータ ComM_Mode が COMM_SILENT_COMMUNICATION または COMM_FULL_COMMUNICATION に等しいものであった場合、ガード条件 G_NO_COM_MODE_NOT_REQUESTED を通過しなければならない。(SRS_Can_01145, SRS_Can_01142)

### 7.3 エラー分類

「General Specification of Basic Software Modules」の 7.x 節「Error Handling」が、Basic Software のエラー処理を詳しく説明している。何より、BSW モジュールで発生しうる 5 種類のエラーからなる分類体系を構成している。この基礎に基づき、以下の節で、それぞれの小節ごとに個別のエラーを規定する。

#### 7.3.1 開発エラー

本章は、このソフトウェアモジュールの中で検出できるすべての開発エラーを列挙する。各エラーに値を定義しなければならない。

**[SWS_CanSM_00654]**

| エラーの種類 | 関連 | エラーコード | 値 [hex] |
|---|---|---|---|
| モジュールの初期化前に API サービスを使用した | 開発 | CANSM_E_UNINIT | 0x01 |
| 不正なポインタで API サービスを呼び出した | 開発 | CANSM_E_PARAM_POINTER | 0x02 |
| 不正なパラメータで API サービスを呼び出した | 開発 | CANSM_E_INVALID_NETWORK_HANDLE | 0x03 |
| 不正なパラメータで API サービスを呼び出した | 開発 | CANSM_E_PARAM_CONTROLLER | 0x04 |
| 不正なパラメータで API サービスを呼び出した | 開発 | CANSM_E_PARAM_TRANSCEIVER | 0x05 |
| すべての CAN ネットワークが状態 CANSM_NO_COMMUNICATION にないときに DeInit API サービスを呼び出した | 開発 | CANSM_E_NOT_IN_NO_COM | 0x0B |

（SRS_BSW_00337）

> 注: 原本の表は PDF からの抽出で列が崩れていた。上の表は、エラーの種類の並びとコードの並びが原本の行順に対応するものとして再構成した。値の対応は原本の表で確認すること。

#### 7.3.2 ランタイムエラー

##### 7.3.2.1 CANSM_E_MODE_REQUEST_TIMEOUT

**[SWS_CanSM_00664]**

- エラー名: CANSM_E_MODE_REQUEST_TIMEOUT
- 短い説明: ネットワークのモード要求が、コンフィギュレーションで許される回数を超えて失敗した。
- 詳しい説明: CAN State Manager が、CanSMModeRequestRepetitionMax 回のリトライ後も、CAN ネットワークのモードを変更できなかった。次の CanIf サービスと、それに対応する通知を監視する。
  - CanIf_SetControllerMode() → CanSM_ControllerModeIndication()
  - CanIf_SetTrcvMode() → CanSM_TransceiverModeIndication()
  - CanIf_CheckTrcvWakeFlag() → CanSM_CheckTransceiverWakeFlagIndication()
  - CanIf_ClrTrcvWufFlag() → CanSM_ClearTrcvWufFlagIndication()
- 推奨 DTC: DEM が割り当てる
- 検出基準
  - 失敗: CAN State Manager が上記の CanIf サービスのいずれかを実行し、対応する通知を CanSMModeRequestRepetitionMax 回受け取らなかった場合、拡張プロダクションエラー CANSM_E_MODE_REQUEST_TIMEOUT を、イベント状態 DEM_EVENT_STATUS_PREFAILED で DEM へ報告しなければならない。
  - 成功: CAN State Manager が上記の通知のいずれかを受け取った場合、拡張プロダクションエラー CANSM_E_MODE_REQUEST_TIMEOUT を、イベント状態 DEM_EVENT_STATUS_PREPASSED で DEM へ報告しなければならない。
- 副次パラメータ: なし
- 所要時間: CanSMModeRequestRepetitionMax と CanSMMainFunctionTimePeriod に依存する。
- 監視頻度: 連続
- MIL 点灯: DEM が割り当てる

（SRS_BSW_00466）

#### 7.3.3 一過性の故障

一過性の故障はない。

#### 7.3.4 プロダクションエラー

（なし）

#### 7.3.5 拡張プロダクションエラー

##### 7.3.5.1 CANSM_E_BUS_OFF

**[SWS_CanSM_00666]**

- エラー名: CANSM_E_BUS_OFF（ECUC_CanSM_00070 参照）
- 短い説明: バスオフの検出
- 詳しい説明: CAN ネットワークのバスオフ回復ステートマシンが、回復に成功しないまま一定回数連続してバスオフを検出した。
- 推奨 DTC: DEM が割り当てる
- 検出基準
  - 失敗: CanSM_ControllerBusOff が呼ばれたとき（T_BUS_OFF/E_BUS_OFF）に PRE_FAILED。デバウンスは OEM が DEM で定義する。
  - 成功: CAN フレームの送信に成功した後（G_BUS_OFF_PASSIVE/E_BUS_OFF_PASSIVE）
- 副次パラメータ: なし
- 所要時間: PRE_FAILED は即時（エラー割り込みのコンテキスト）、FAILED は DEM のデバウンス設定に依存する。
- 監視頻度: 連続
- MIL 点灯: DEM が割り当てる

### 7.4 Pretended Networking 機能

#### 7.4.1 有効化

**[SWS_CanSM_00588]** Pretended Networking を有効にするため、CanSM モジュールは CanIf_SetIcomConfiguration を呼んで ICOM コンフィギュレーションを要求しなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00589]** CanSM は、BswM_CanSM_CurrentIcomConfiguration を呼んで、有効化の状態を BswM へ知らせなければならない。(SRS_Can_01145, SRS_Can_01142)

#### 7.4.2 無効化

**[SWS_CanSM_00590]** CanSM は、Pretended Networking を無効にして ICOM コンフィギュレーションを 0 に戻すため、提供されている API CanIf_SetIcomConfiguration を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00591]** CanSM は、BswM_CanSM_CurrentIcomConfiguration を呼んで、無効化の状態を BswM へ知らせなければならない。(SRS_Can_01145, SRS_Can_01142)

### 7.5 ECU のオンライン active / passive モード

**[SWS_CanSM_00646]** CanSM ステートマネージャは、要求された ECU passive モードの状態を保存しなければならない（第 8 章の SWS_CanSM_00644 参照）。(SRS_Can_01158)

**[SWS_CanSM_00649]** CanSM_SetEcuPassive が CanSM_Passive=true で呼ばれた場合（第 8 章の SWS_CanSM_00644 参照）、CanSM は、設定された CAN コントローラの PDU モードのうち、その時点で CANIF_ONLINE のものすべてを、パラメータ ControllerId := CanSMControllerId（ECUC_CanSM_00141 参照）、PduModeRequest := CANIF_TX_OFFLINE_ACTIVE を指定して API CanIf_SetPduMode（8.5.1 節参照）を呼ぶことで、CANIF_TX_OFFLINE_ACTIVE へ変更しなければならない。(SRS_Can_01158)

**[SWS_CanSM_00650]** CanSM_SetEcuPassive が CanSM_Passive=false で呼ばれた場合（第 8 章の SWS_CanSM_00644 参照）、CanSM は、設定された CAN コントローラの PDU モードのうち、その時点で CANIF_TX_OFFLINE_ACTIVE のものすべてを、パラメータ ControllerId := CanSMControllerId（ECUC_CanSM_00141 参照）、PduModeRequest := CANIF_ONLINE を指定して API CanIf_SetPduMode（8.5.1 節参照）を呼ぶことで、CANIF_ONLINE へ変更しなければならない。(SRS_Can_01158)

**[SWS_CanSM_00656]** CanSM が実際の PduMode に関する情報を必要とする場合、CanSM は、CanIf の現在の PDU モードを取得するため、API CanIf_GetPduMode を呼ばなければならない。(SRS_Can_01158)

### 7.6 エラー検出

詳細は SWS_BSWGeneral の 7.3 節「Error Detection」を参照。

### 7.7 エラー通知

詳細は SWS_BSWGeneral の 7.4 節「Error notification」を参照。

### 7.8 AUTOSAR のデバッグとトレースのためのインタフェース

詳細は SWS_BSWGeneral の 7.1.17 節「Debugging support」を参照。

### 7.9 非機能の設計ルール

CanSM は、SRS General [3] のソフトウェアモジュール設計要件を満たさなければならない。

---

## 8 API 仕様

### 8.1 インポートする型

本章では、次のファイルからインクルードされるすべての型を列挙する。

**[SWS_CanSM_00243]**

| モジュール | インポートする型 |
|---|---|
| CanIf | CanIf_NotifStatusType, CanIf_PduModeType |
| Can_GeneralTypes | CanTrcv_TrcvModeType, Can_ControllerStateType |
| ComM | ComM_ModeType |
| ComStack_Types | IcomConfigIdType, IcomSwitch_ErrorType, NetworkHandleType |
| Dem | Dem_EventIdType, Dem_EventStatusType |
| Std_Types | Std_ReturnType, Std_VersionInfoType |

### 8.2 型定義

次の表は、CanSM モジュールの型定義である。

#### 8.2.1 CanSM_StateType

**[SWS_CanSM_00596]**

- 名前: CanSM_StateType
- 種類: 列挙型
- 値: CANSM_INITED、CANSM_UNINITED
- 説明: CanSM モジュールの内部状態の値を定義する。

(SRS_BSW_00405, SRS_BSW_00101, SRS_BSW_00406, SRS_BSW_00358, SRS_BSW_00414, SRS_BSW_00404)

#### 8.2.2 CanSM_ConfigType

**[SWS_CanSM_00597]**

- 名前: CanSM_ConfigType
- 種類: 構造体
- 説明: この型は、CanSM のポストビルドパラメータのデータ構造を定義する。初期化時に、CanSM は、初期化に必要なコンフィギュレーションデータへアクセスするため、この型の構造体へのポインタを受け取る。

(SRS_BSW_00400, SRS_BSW_00438)

#### 8.2.3 CanSM_BswMCurrentStateType

**[SWS_CanSM_00598]**

- 名前: CanSM_BswMCurrentStateType
- 種類: 列挙型
- 値: CANSM_BSWM_NO_COMMUNICATION、CANSM_BSWM_SILENT_COMMUNICATION、CANSM_BSWM_FULL_COMMUNICATION、CANSM_BSWM_BUS_OFF、CANSM_BSWM_CHANGE_BAUDRATE
- 説明: BswM モジュールへ通知される、CAN 固有の通信モード / 状態。

(SRS_ModeMgm_09251)

### 8.3 関数定義

次の各節は、CanSM モジュールが提供する API 関数を規定する。

#### 8.3.1 CanSM_Init

**[SWS_CanSM_00023]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_Init |
| 構文 | `void CanSM_Init(const CanSM_ConfigType* ConfigPtr)` |
| サービス ID | 0x00 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | ConfigPtr: CanSM のポストビルドパラメータ用の初期化構造体へのポインタ |
| 戻り値 | なし |
| 説明 | このサービスは CanSM モジュールを初期化する。 |

(SRS_BSW_00405, SRS_BSW_00101, SRS_BSW_00406, SRS_BSW_00358, SRS_BSW_00414, SRS_BSW_00404, SRS_BSW_00400, SRS_BSW_00438)

#### 8.3.2 CanSM_DeInit

**[SWS_CanSM_91001]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_DeInit |
| 構文 | `void CanSM_DeInit(void)` |
| サービス ID | 0x14 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 戻り値 | なし |
| 説明 | このサービスは CanSM モジュールを初期化解除する。 |

(SRS_Can_01164, SRS_BSW_00336)

注: 初期化解除関数の一般的な振る舞いと制約は、[SWS_BSW_00152]、[SWS_BSW_00072]、[SWS_BSW_00232]、[SWS_BSW_00233] で規定される。

注意: CanSM_DeInit 関数の呼び出し側は、すべての CAN ネットワークが状態 CANSM_NO_COMMUNICATION にあることを保証しなければならない。

**[SWS_CanSM_00660]** CanSM モジュールの開発エラー検出が有効な場合: すべての CAN ネットワークが状態 CANSM_NO_COMMUNICATION にないとき、関数 CanSM_DeInit はエラー CANSM_E_NOT_IN_NO_COM を発生させなければならない。(SRS_BSW_00369)

#### 8.3.3 CanSM_RequestComMode

**[SWS_CanSM_00062]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_RequestComMode |
| 構文 | `Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType ComM_Mode)` |
| サービス ID | 0x02 |
| 同期 / 非同期 | 非同期 |
| リエントラント | 可（異なるネットワークハンドルに対してのみ） |
| 入力パラメータ | network: 要求の宛先となる通信ネットワークのハンドル<br>ComM_Mode: 要求する通信モード |
| 戻り値 | E_OK: サービスを受け付けた<br>E_NOT_OK: サービスを拒否した |
| 説明 | このサービスは、CAN ネットワークの通信モードを要求されたものへ変更する。 |

(SRS_Can_01145, SRS_Can_01142)

備考: 通信モードの詳細な説明は [10] を参照。

**[SWS_CanSM_00369]** 要求の NetworkHandle パラメータが CanSM モジュールのコンフィギュレーションに含まれるハンドル（ECUC_CanSM_00161 参照）である場合、関数 CanSM_RequestComMode はその要求を受け付けなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00370]** 要求の NetworkHandle パラメータが CanSM モジュールのコンフィギュレーションに含まれるハンドル（ECUC_CanSM_00161 参照）でない場合、関数 CanSM_RequestComMode はその要求を拒否しなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00555]** CanSM_Init 要求（SWS_CanSM_00423、SWS_CanSM_00430 参照）の後、要求された CAN ネットワークの初期遷移がまだ完了していない場合、CanSM モジュールは API 要求 CanSM_RequestComMode を拒否しなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00183]** 要求のネットワークハンドルを受け付けない場合、関数 CanSM_RequestComMode は、ErrorId パラメータ CANSM_E_INVALID_NETWORK_HANDLE で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00182]** 関数 CanSM_RequestComMode が要求を受け付けた場合、その要求は CanSM のステートマシンで処理されなければならない（SWS_CanSM_00635 参照）。(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00184]** CanSM モジュールが初期化されていない状態で関数 CanSM_RequestComMode が呼ばれた場合、この関数は、ErrorId パラメータ CANSM_E_UNINIT で関数 Det_ReportError を呼ばなければならない。(SRS_BSW_00406)

#### 8.3.4 CanSM_GetCurrentComMode

**[SWS_CanSM_00063]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_GetCurrentComMode |
| 構文 | `Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType* ComM_ModePtr)` |
| サービス ID | 0x03 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | network: 現在の通信モードを出力するネットワークのハンドル |
| 出力パラメータ | ComM_ModePtr: 現在の通信モードを出力する先のポインタ |
| 戻り値 | E_OK: サービスを受け付けた<br>E_NOT_OK: サービスを拒否した |
| 説明 | このサービスは、CAN ネットワークの現在の通信モードを出力する。 |

(SRS_ModeMgm_09084)

**[SWS_CanSM_00282]** CanSM モジュールは、提供された API CanSM_Init（SWS_CANSM_00023 参照）が呼ばれるまで、API 要求 CanSM_GetCurrentComMode に対して E_NOT_OK を返さなければならない。(SRS_Can_01142)

**[SWS_CanSM_00371]** 要求の NetworkHandle パラメータが CanSM モジュールのコンフィギュレーションに含まれるハンドル（ECUC_CanSM_00161 参照）である場合、関数 CanSM_GetCurrentComMode はその要求を受け付けなければならない。(SRS_Can_01142)

**[SWS_CanSM_00372]** 要求の NetworkHandle パラメータが CanSM モジュールのコンフィギュレーションに含まれるハンドル（ECUC_CanSM_00161 参照）でない場合、関数 CanSM_GetCurrentComMode はその要求を拒否しなければならない。(SRS_Can_01142)

**[SWS_CanSM_00187]** 要求のネットワークハンドルを受け付けない場合、関数 CanSM_GetCurrentComMode は、ErrorId パラメータ CANSM_E_INVALID_NETWORK_HANDLE で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01142)

**[SWS_CanSM_00186]** 要求を受け付けた場合、関数 CanSM_GetCurrentComMode は、そのネットワークハンドルの現在の通信モード（SWS_CanSM_00266 参照）を、指定された ComM_ModeType 型のポインタへ出力しなければならない。(SRS_Can_01142)

**[SWS_CanSM_00188]** CanSM モジュールが初期化されていない状態（SWS_CANSM_00282 参照）で関数 CanSM_GetCurrentComMode が呼ばれた場合、この関数は、ErrorId パラメータ CANSM_E_UNINIT で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01142)

**[SWS_CanSM_00360]** この関数のユーザが ComM_ModePtr として NULL ポインタを渡した場合、関数 CanSM_GetCurrentComMode は、開発エラー CANSM_E_PARAM_POINTER を DET へ報告しなければならない。(SRS_Can_01142)

#### 8.3.5 CanSM_StartWakeupSource

**[SWS_CanSM_00609]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_StartWakeupSource |
| 構文 | `Std_ReturnType CanSM_StartWakeupSource(NetworkHandleType network)` |
| サービス ID | 0x11 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | network: 対象の CAN ネットワーク |
| 戻り値 | E_OK: 要求を受け付けた<br>E_NOT_OK: 要求を拒否した |
| 説明 | この関数は、ウェイクアップソースを開始するとき、EcuM から呼ばれなければならない。 |

(SRS_Can_01145)

**[SWS_CanSM_00611]** CanSM モジュールが CanSM_Init（SWS_CANSM_00023 参照）でまだ初期化されていない場合、API 関数 CanSM_StartWakeupSource は E_NOT_OK を返さなければならない。(SRS_Can_01145)

**[SWS_CanSM_00617]** CanSM モジュールが CanSM_Init（SWS_CANSM_00023 参照）でまだ初期化されていない場合、関数 CanSM_StartWakeupSource は、ErrorId パラメータ CANSM_E_UNINIT で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

**[SWS_CanSM_00612]** CanSM モジュールが初期化済みで、要求の network パラメータが CanSM モジュールのコンフィギュレーションに含まれるハンドル（ECUC_CanSM_00161 参照）でない場合、関数 CanSM_StartWakeupSource は E_NOT_OK を返さなければならない。(SRS_Can_01145)

**[SWS_CanSM_00613]** CanSM モジュールが初期化済みで、要求されたハンドルが CanSM のコンフィギュレーション（ECUC_CanSM_00161 参照）に照らして無効な場合、関数 CanSM_StartWakeupSource は、ErrorId パラメータ CANSM_E_INVALID_NETWORK_HANDLE で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

**[SWS_CanSM_00616]** CanSM モジュールが初期化済みで、要求されたハンドルが CanSM のコンフィギュレーション（ECUC_CanSM_00161 参照）に照らして有効な場合、関数 CanSM_StartWakeupSource は E_OK を返さなければならず、これは、関連するネットワークのステートマシンへのトリガ（SWS_CanSM_00607 参照）として扱われなければならない。(SRS_Can_01145)

#### 8.3.6 CanSM_StopWakeupSource

**[SWS_CanSM_00610]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_StopWakeupSource |
| 構文 | `Std_ReturnType CanSM_StopWakeupSource(NetworkHandleType network)` |
| サービス ID | 0x12 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | network: 対象の CAN ネットワーク |
| 戻り値 | E_OK: 要求を受け付けた<br>E_NOT_OK: 要求を拒否した |
| 説明 | この関数は、ウェイクアップソースを停止するとき、EcuM から呼ばれなければならない。 |

(SRS_Can_01145)

**[SWS_CanSM_00618]** CanSM モジュールが CanSM_Init（SWS_CANSM_00023 参照）でまだ初期化されていない場合、API 関数 CanSM_StopWakeupSource は E_NOT_OK を返さなければならない。(SRS_Can_01145)

**[SWS_CanSM_00619]** CanSM モジュールが CanSM_Init（SWS_CANSM_00023 参照）でまだ初期化されていない場合、関数 CanSM_StopWakeupSource は、ErrorId パラメータ CANSM_E_UNINIT で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

**[SWS_CanSM_00620]** CanSM モジュールが初期化済みで、要求の network パラメータが CanSM モジュールのコンフィギュレーションに含まれるハンドル（ECUC_CanSM_00161 参照）でない場合、関数 CanSM_StopWakeupSource は E_NOT_OK を返さなければならない。(SRS_Can_01145)

**[SWS_CanSM_00621]** CanSM モジュールが初期化済みで、要求されたハンドルが CanSM のコンフィギュレーション（ECUC_CanSM_00161 参照）に照らして無効な場合、関数 CanSM_StopWakeupSource は、ErrorId パラメータ CANSM_E_INVALID_NETWORK_HANDLE で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

**[SWS_CanSM_00622]** CanSM モジュールが初期化済みで、要求されたハンドルが CanSM のコンフィギュレーション（ECUC_CanSM_00161 参照）に照らして有効な場合、関数 CanSM_StopWakeupSource は E_OK を返さなければならず、これは、関連するネットワークのステートマシンへのトリガ（SWS_CanSM_00608 参照）として扱われなければならない。(SRS_Can_01145)

#### 8.3.7 オプション

##### 8.3.7.1 CanSM_GetVersionInfo

**[SWS_CanSM_00024]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_GetVersionInfo |
| 構文 | `void CanSM_GetVersionInfo(Std_VersionInfoType* VersionInfo)` |
| サービス ID | 0x01 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 出力パラメータ | VersionInfo: このモジュールのバージョン情報を格納する先のポインタ |
| 戻り値 | なし |
| 説明 | このサービスは、このモジュールのバージョン情報（モジュール ID、ベンダ ID、BSW00407 に関連するベンダ固有のバージョン番号）を出力する。 |

(SRS_BSW_00407, SRS_BSW_00003)

**[SWS_CanSM_00374]** この関数のユーザが VersionInfo として NULL ポインタを渡した場合、関数 CanSM_GetVersionInfo は、開発エラー CANSM_E_PARAM_POINTER を DET へ報告しなければならない。(SRS_BSW_00407, SRS_BSW_00003)

##### 8.3.7.2 CanSM_SetBaudrate

**[SWS_CanSM_00561]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_SetBaudrate |
| 構文 | `Std_ReturnType CanSM_SetBaudrate(NetworkHandleType Network, uint16 BaudRateConfigID)` |
| サービス ID | 0x0d |
| 同期 / 非同期 | 同期 |
| リエントラント | 異なるネットワークに対しては可。同一ネットワークに対しては不可。 |
| 入力パラメータ | Network: ボーレート変更の対象となる CAN ネットワークのハンドル<br>BaudRateConfigID: ボーレートコンフィギュレーションを ID で参照する（CanControllerBaudRateConfigID 参照） |
| 戻り値 | E_OK: サービス要求を受け付け、（新しい）ボーレートの設定を開始した<br>E_NOT_OK: サービス要求を受け付けなかった |
| 説明 | このサービスは、ある CAN ネットワークに設定された CAN コントローラのボーレートを変更する非同期処理を開始する。必要なボーレートの変更内容によっては、コントローラのリセットが必要になることがある。 |

(SRS_Can_01142)

**[SWS_CanSM_00569]** コンフィギュレーションパラメータ CANSM_SET_BAUDRATE_API（ECUC_CanSM_00343 参照）が値 TRUE に設定されている場合、CanSM モジュールは API 関数 CanSM_SetBaudrate を提供しなければならない。(SRS_Can_01142)

**[SWS_CanSM_00570]** コンフィギュレーションパラメータ CANSM_SET_BAUDRATE_API（ECUC_CanSM_00343 参照）が値 FALSE に設定されている場合、CanSM モジュールは API 関数 CanSM_SetBaudrate を提供してはならない。(SRS_Can_01142)

**[SWS_CanSM_00502]** NetworkHandle パラメータが CanSM モジュールに設定されたネットワークハンドル（ECUC_CanSM_00161 参照）に一致しない場合、CanSM モジュールは CanSM_SetBaudrate の API 要求を拒否しなければならない。(SRS_Can_01142)

**[SWS_CanSM_00504]** 要求のネットワークハンドルを受け付けない場合、関数 CanSM_SetBaudrate は、ErrorId パラメータ CANSM_E_INVALID_NETWORK_HANDLE（7.3 節参照）で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01142)

**[SWS_CanSM_00505]** 要求された CAN ネットワークが通信モード COMM_FULL_COMMUNICATION にない場合、関数 CanSM_SetBaudrate はその要求を拒否しなければならない。(SRS_Can_01142)

**[SWS_CanSM_00530]** CanSM モジュールが初期化されていない場合、CanSM モジュールは CanSM_SetBaudrate の API 要求を拒否しなければならない。(SRS_Can_01142)

**[SWS_CanSM_00506]** 関数 CanSM_SetBaudrate が呼ばれたとき CanSM モジュールが初期化されていない場合、この関数は、ErrorId パラメータ CANSM_E_UNINIT（7.3 節参照）で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01142)

**[SWS_CanSM_00503]** SWS_CANSM_00502、SWS_CANSM_00505、SWS_CANSM_00530 により CanSM_SetBaudrate 要求を拒否する条件がない場合、CanSM モジュールは E_OK を返し、SWS_CANSM_00507 で規定されるとおり、要求されたボーレート変更の処理を実行しなければならない。(SRS_Can_01142)

##### 8.3.7.3 CanSM_SetIcomConfiguration

**[SWS_CanSM_00586]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_SetIcomConfiguration |
| 構文 | `Std_ReturnType CanSM_SetIcomConfiguration(NetworkHandleType Network, IcomConfigIdType ConfigurationId)` |
| サービス ID | 0x0f |
| 同期 / 非同期 | 非同期 |
| リエントラント | 異なるネットワークハンドルに対してのみ可 |
| 入力パラメータ | Network: 要求の宛先となる通信ネットワークのハンドル<br>ConfigurationId: 要求するコンフィギュレーション |
| 戻り値 | E_OK: 要求を受け付けた<br>E_NOT_OK: 要求を拒否した |
| 説明 | このサービスは、CAN ネットワークの Icom コンフィギュレーションを要求されたものへ変更する。 |

(SRS_Can_01142)

**[SWS_CanSM_00599]** コンフィギュレーションパラメータ CANSM_ICOM_SUPPORT（ECUC_CanSM_00345 参照）が値 TRUE に設定されている場合、CanSM モジュールは API 関数 CanSM_SetIcomConfiguration を提供しなければならない。(SRS_Can_01142)

**[SWS_CanSM_00593]** 要求されたネットワークが CanSM モジュールに設定されている場合、API CanSM_SetIcomConfiguration は、Pretended Networking を有効または無効にする（7.4 節参照）ため、指定されたチャネルの ICOM コンフィギュレーションを要求し、要求した CanIf API の戻り値に応じて E_OK または E_NOT_OK を返さなければならない。(SRS_Can_01142)

**[SWS_CanSM_00594]** 要求されたネットワークが CanSM モジュールに設定されていない場合、API CanSM_SetIcomConfiguration は E_NOT_OK を返し、DET エラー CANSM_E_INVALID_NETWORK_HANDLE を通知しなければならない。(SRS_Can_01142)

##### 8.3.7.4 CanSM_SetEcuPassive

**[SWS_CanSM_00644]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_SetEcuPassive |
| 構文 | `Std_ReturnType CanSM_SetEcuPassive(boolean CanSM_Passive)` |
| サービス ID | 0x13 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | CanSM_Passive: TRUE = すべての CanSM チャネルを passive（受信専用）にする。FALSE = すべての CanSM チャネルを passive でない状態に戻す。 |
| 戻り値 | E_OK: 要求を受け付けた<br>E_NOT_OK: 要求を受け付けなかった |
| 説明 | この関数は、ECU のすべての CanSM チャネルを受信専用モードにするために使える。このモードは、元に戻されるか、ECU がリセットされるまで維持される。 |

(SRS_Can_01158)

**[SWS_CanSM_00645]** コンフィギュレーションパラメータ CanSMTxOfflineActiveSupport（ECUC_CanSM_00349 参照）が値 TRUE に設定されている場合、CanSM モジュールは API 関数 CanSM_SetEcuPassive を提供しなければならない。(SRS_Can_01158)

#### 8.3.8 コールバック通知

これは、他のモジュールに提供される関数の一覧である。コールバック関数のプロトタイプは、ファイル CanSM_Cbk.h で提供されなければならない。

#### 8.3.9 CanSM_ControllerBusOff

**[SWS_CanSM_00064]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_ControllerBusOff |
| 構文 | `void CanSM_ControllerBusOff(uint8 ControllerId)` |
| サービス ID | 0x04 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可（異なる CanController に対してのみ） |
| 入力パラメータ | ControllerId: バスオフ事象を検出した CAN コントローラ |
| 戻り値 | なし |
| 説明 | このコールバック関数は、ある CAN コントローラでのバスオフ事象を CanSM へ通知する。この事象は、影響を受ける CAN ネットワークについて、規定されたバスオフ回復処理で扱われなければならない。 |

(SRS_BSW_00359, SRS_BSW_00333)

**[SWS_CanSM_00189]** 関数 CanSM_ControllerBusOff が、CanSM モジュールのコンフィギュレーションで CanSMControllerId として設定されていないコントローラを受け取った場合、ErrorId パラメータ CANSM_E_PARAM_CONTROLLER で関数 Det_ReportError を呼ばなければならない。(SRS_BSW_00359, SRS_BSW_00333)

**[SWS_CanSM_00190]** CanSM モジュールが初期化されていない状態で関数 CanSM_ControllerBusOff が呼ばれた場合、関数 CanSM_ControllerBusOff は、ErrorId パラメータ CANSM_E_UNINIT で関数 Det_ReportError を呼ばなければならない。(SRS_BSW_00359, SRS_BSW_00333)

**[SWS_CanSM_00235]** CanSM モジュールが初期化済みで、入力パラメータ Controller が、パラメータ CanSMControllerId で設定された CAN コントローラの 1 つである場合、このバスオフ事象は、CAN ネットワークのステートマシンで処理されなければならない（SWS_CanSM_00500 参照）。(SRS_BSW_00359, SRS_BSW_00333)

補足:

1. 呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のどちらかである。
2. 複数の CAN コントローラを使う場合は、リエントラントである必要がある。

#### 8.3.10 CanSM_ControllerModeIndication

**[SWS_CanSM_00396]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_ControllerModeIndication |
| 構文 | `void CanSM_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)` |
| サービス ID | 0x07 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可（異なる CAN コントローラに対してのみ） |
| 入力パラメータ | ControllerId: モードが変化した CAN コントローラ<br>ControllerMode: 通知される CAN コントローラのモード |
| 戻り値 | なし |
| 説明 | このコールバックは、CAN コントローラのモード変化を CanSM モジュールへ通知する。 |

(SRS_Can_01145)

**[SWS_CanSM_00397]** 関数 CanSM_ControllerModeIndication が、CanSM モジュールのコンフィギュレーションで CanSMControllerId として設定されていない ControllerId を受け取った場合、ErrorId パラメータ CANSM_E_PARAM_CONTROLLER で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

**[SWS_CanSM_00398]** CanSM モジュールが初期化されていない状態で関数 CanSM_ControllerModeIndication が呼ばれた場合、関数 CanSM_ControllerModeIndication は、ErrorId パラメータ CANSM_E_UNINIT で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

#### 8.3.11 CanSM_TransceiverModeIndication

**[SWS_CanSM_00399]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_TransceiverModeIndication |
| 構文 | `void CanSM_TransceiverModeIndication(uint8 TransceiverId, CanTrcv_TrcvModeType TransceiverMode)` |
| サービス ID | 0x09 |
| 同期 / 非同期 | 同期 |
| リエントラント | 異なる CAN トランシーバに対して可 |
| 入力パラメータ | TransceiverId: モードが変化した CAN トランシーバ<br>TransceiverMode: 通知される CAN トランシーバのモード |
| 戻り値 | なし |
| 説明 | このコールバックは、CAN トランシーバのモード変化を CanSM モジュールへ通知する。 |

(SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00400]** 関数 CanSM_TransceiverModeIndication が、CanSM モジュールのコンフィギュレーションで CanSMTransceiverId として設定されていない TransceiverId を受け取った場合、ErrorId パラメータ CANSM_E_PARAM_TRANSCEIVER で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

**[SWS_CanSM_00401]** CanSM モジュールが初期化されていない状態で関数 CanSM_TransceiverModeIndication が呼ばれた場合、関数 CanSM_TransceiverModeIndication は、ErrorId パラメータ CANSM_E_UNINIT で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

#### 8.3.12 CanSM_TxTimeoutException

**[SWS_CanSM_00410]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_TxTimeoutException |
| 構文 | `void CanSM_TxTimeoutException(NetworkHandleType Channel)` |
| サービス ID | 0x0b |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: 対象の CAN ネットワーク |
| 戻り値 | なし |
| 説明 | この関数は、対象の部分 CAN ネットワークで CanNm が送信タイムアウト例外を検出したことを CanSM モジュールへ通知する。この例外は、CanSM モジュールのそれぞれのネットワークステートマシンの中で回復されなければならない。 |

(SRS_Can_01142, SRS_Can_01145)

**[SWS_CanSM_00411]** CanSM がまだ初期化されていない場合、関数 CanSM_TxTimeoutException は CANSM_E_UNINIT を DET へ報告しなければならない。(SRS_Can_01145)

**[SWS_CanSM_00412]** 関数 CanSM_TxTimeoutException が、CanSM のコンフィギュレーションで CanSMNetworkHandle として設定されていない Channel で参照された場合、CANSM_E_INVALID_NETWORK_HANDLE を DET へ報告しなければならない。(SRS_Can_01145)

備考: 異なるチャネルに対しては、リエントラントである必要がある。

#### 8.3.13 CanSM_ClearTrcvWufFlagIndication

**[SWS_CanSM_00413]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_ClearTrcvWufFlagIndication |
| 構文 | `void CanSM_ClearTrcvWufFlagIndication(uint8 Transceiver)` |
| サービス ID | 0x08 |
| 同期 / 非同期 | 同期 |
| リエントラント | 異なる CAN トランシーバに対して可 |
| 入力パラメータ | Transceiver: 要求されたトランシーバ |
| 戻り値 | なし |
| 説明 | このコールバック関数は、通知された CAN トランシーバについて、CanIf_ClearTrcvWufFlag API の処理の終了を示す。 |

(SRS_Can_01145)

**[SWS_CanSM_00414]** CanSM がまだ初期化されていない場合、関数 CanSM_ClearTrcvWufFlagIndication は CANSM_E_UNINIT を DET へ報告しなければならない。(SRS_Can_01145)

**[SWS_CanSM_00415]** 関数 CanSM_ClearTrcvWufFlagIndication が、CanSM モジュールのコンフィギュレーションで設定されていない（ECUC_CanSM_00137 参照）TransceiverId を受け取った場合、ErrorId パラメータ CANSM_E_PARAM_TRANSCEIVER で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

#### 8.3.14 CanSM_CheckTransceiverWakeFlagIndication

**[SWS_CanSM_00416]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_CheckTransceiverWakeFlagIndication |
| 構文 | `void CanSM_CheckTransceiverWakeFlagIndication(uint8 Transceiver)` |
| サービス ID | 0x0a |
| 同期 / 非同期 | 同期 |
| リエントラント | 異なる CAN トランシーバに対して可 |
| 入力パラメータ | Transceiver: 要求されたトランシーバ |
| 戻り値 | なし |
| 説明 | このコールバック関数は、通知された CAN トランシーバについて、CanIf_CheckTrcvWakeFlag API の処理の終了を示す。 |

(SRS_Can_01145)

**[SWS_CanSM_00417]** CanSM モジュールがまだ初期化されていない場合、関数 CanSM_CheckTransceiverWakeFlagIndication は CANSM_E_UNINIT を DET へ報告しなければならない。(SRS_Can_01145)

**[SWS_CanSM_00418]** 関数 CanSM_CheckTransceiverWakeFlagIndication が、CanSM モジュールのコンフィギュレーションで設定されていない（ECUC_CanSM_00137 参照）TransceiverId を受け取った場合、ErrorId パラメータ CANSM_E_PARAM_TRANSCEIVER で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

#### 8.3.15 CanSM_ConfirmPnAvailability

**[SWS_CanSM_00419]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_ConfirmPnAvailability |
| 構文 | `void CanSM_ConfirmPnAvailability(uint8 TransceiverId)` |
| サービス ID | 0x06 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | TransceiverId: PN の利用可能性を確認した CAN トランシーバ |
| 戻り値 | なし |
| 説明 | このコールバック関数は、トランシーバが PN 通信モードで動作していることを示す。 |

(SRS_Can_01145)

**[SWS_CanSM_00546]** 関数 CanSM_ConfirmPnAvailability は、設定されたトランシーバ（ECUC_CanSM_00137 参照）を入力パラメータとして呼ばれた場合、CanNm モジュールへ通知しなければならない（SWS_CanSM_00422 参照）。(SRS_Can_01145)

**[SWS_CanSM_00420]** CanSM モジュールがまだ初期化されていない場合、関数 CanSM_ConfirmPnAvailability は CANSM_E_UNINIT を DET へ報告しなければならない。(SRS_Can_01145)

**[SWS_CanSM_00421]** 関数 CanSM_ConfirmPnAvailability が、CanSM モジュールのコンフィギュレーションで設定されていない（ECUC_CanSM_00137 参照）TransceiverId を受け取った場合、ErrorId パラメータ CANSM_E_PARAM_TRANSCEIVER で関数 Det_ReportError を呼ばなければならない。(SRS_Can_01145)

#### 8.3.16 CanSM_CurrentIcomConfiguration

**[SWS_CanSM_00587]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_CurrentIcomConfiguration |
| 構文 | `void CanSM_CurrentIcomConfiguration(uint8 ControllerId, IcomConfigIdType ConfigurationId, IcomSwitch_ErrorType Error)` |
| サービス ID | 0x10 |
| 同期 / 非同期 | 同期 |
| リエントラント | 異なるネットワークハンドルに対してのみ可 |
| 入力パラメータ | ControllerId: コンフィギュレーションが変化した CAN コントローラの ID<br>ConfigurationId: 変更後のコンフィギュレーション ID<br>Error: ICOM_SWITCH_E_OK = エラーなし。ICOM_SWITCH_E_FAILED = 要求されたコンフィギュレーションへの切り替えに失敗した（重大なエラー）。 |
| 戻り値 | なし |
| 説明 | このサービスは、CAN ネットワークの Icom コンフィギュレーションの変更を知らせる。 |

(SRS_Can_01142)

**[SWS_CanSM_00595]** パラメータ CANSM_ICOM_SUPPORT（ECUC_CanSM_00345 参照）が値 TRUE に設定されている場合、コールバック関数 BSWM_CanSM_CurrentIcomConfiguration は、通知された ControllerId をコンフィギュレーションに含む CAN ネットワークについて、Pretended Networking の有効化または無効化の状態（7.4 節参照）を BswM へ通知しなければならない。そのために、ConfigurationId と Error パラメータを BswM へ引き渡さなければならない。(SRS_Can_01142)

### 8.4 スケジュールされる関数

詳細は SWS_BSWGeneral の 8.5 節「Scheduled functions」を参照。

#### 8.4.1 CanSM_MainFunction

**[SWS_CanSM_00065]**

| 項目 | 内容 |
|---|---|
| サービス名 | CanSM_MainFunction |
| 構文 | `void CanSM_MainFunction(void)` |
| サービス ID | 0x05 |
| 説明 | CanSM のスケジュールされる関数 |

(SRS_BSW_00424, SRS_BSW_00425, SRS_Can_01145, SRS_Can_01142)

**[SWS_CanSM_00167]** CanSM モジュールのメイン関数は、CanSM モジュールが設定された CAN ネットワークごとに実装しなければならない CanSM のステートマシンのエフェクト（7.2 節参照）を実行しなければならない。(SRS_BSW_00424, SRS_BSW_00425, SRS_Can_01145, SRS_Can_01142)

### 8.5 期待されるインタフェース

本章は、他のモジュールに要求されるすべてのインタフェースを列挙する。

#### 8.5.1 必須インタフェース

本章は、このモジュールのコア機能を満たすために必要なすべてのインタフェースを定義する。

| API 関数 | 説明 |
|---|---|
| BswM_CanSM_CurrentIcomConfiguration | Icom コンフィギュレーションの切り替えを BswM に知らせる関数。 |
| BswM_CanSM_CurrentState | CanSM が現在の状態を示すために呼ぶ関数。 |
| CanIf_CheckTrcvWakeFlag | 指定した CAN トランシーバのウェイクフラグの確認を CanIf モジュールに要求する。 |
| CanIf_ClearTrcvWufFlag | 指定した CAN トランシーバの WUF フラグのクリアを CanIf モジュールに要求する。 |
| CanIf_GetTxConfirmationState | このサービスは、直近の CAN コントローラ開始以降、CAN コントローラ全体で送信確認が 1 つでも行われたかどうかを報告する。 |
| CanIf_SetControllerMode | このサービスは、CAN コントローラのモードを変更するため、対応する CAN ドライバのサービスを呼ぶ。 |
| CanIf_SetPduMode | このサービスは、あらかじめ定義された論理 PDU チャネルの L-PDU に、要求されたモードを設定する。 |
| CanIf_SetTrcvMode | このサービスは、対応する CAN トランシーバドライバのサービスを呼ぶことで、トランシーバ TransceiverId の動作モードを変更する。 |
| CanNm_ConfirmPnAvailability | 指定した NM チャネルで PN フィルタ機能を有効にする。利用可否: この API は CanNmGlobalPnSupport が TRUE の場合にのみ利用できる。 |
| ComM_BusSM_ModeIndication | 対応するバスステートマネージャによる、実際のバスモードの通知。ComM は、通知された状態を RTE と BswM を使ってユーザへ伝えなければならない。 |
| Dem_SetEventStatus | SW-C または BSW モジュールが、モニタの状態情報を Dem へ報告するために呼ぶ。Dem_SetEventStatus を呼ぶ BSW モジュールは、戻り値を安全に無視できる。 |
| Det_ReportRuntimeError | ランタイムエラーを報告するサービス。コールアウトが設定されている場合は、そのコールアウトが呼ばれなければならない。 |

> 注: 原本の表は PDF からの抽出で列が崩れていた。API 名と説明の対応は、説明の内容から対応付けて再構成した。原本の表で確認すること。

##### 8.5.1.1 備考: CanIf_SetPduMode の使い方

CanIf モジュールは、より多くの要求可能な PDU モードを提供しているが、CanSM モジュールは、API CanIf_SetPduMode の呼び出しに、パラメータ CANIF_ONLINE、CANIF_TX_OFFLINE_ACTIVE、CANIF_TX_OFFLINE だけを使う。

CANIF_OFFLINE モードは CanIf によって自動的に想定されるため、CanSM が設定する必要はない。

#### 8.5.2 オプションのインタフェース

本章は、このモジュールのオプションの機能を満たすために必要なすべてのインタフェースを定義する。

| API 関数 | 説明 |
|---|---|
| CanIf_SetBaudrate | このサービスは、CAN コントローラのボーレートコンフィギュレーションを設定する。必要なボーレートの変更内容によっては、コントローラのリセットが必要になることがある。 |
| CanIf_SetIcomConfiguration | このサービスは、CAN コントローラの Icom コンフィギュレーションを要求されたものへ変更する。 |
| Det_ReportError | 開発エラーを報告するサービス。 |

#### 8.5.3 設定可能なインタフェース

本章は、対象の関数を設定できるインタフェースをすべて列挙する。対象の関数は通常コールバック関数である。この種のインタフェースの名前は設定可能なため、固定されていない。

##### 8.5.3.1 `<User_GetBusOffDelay>`

**[SWS_CanSM_00637]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_GetBusOffDelay>` |
| 構文 | `void <User_GetBusOffDelay>(NetworkHandleType network, uint8* delayCyclesPtr)` |
| 同期 / 非同期 | 同期 |
| リエントラント | 異なるネットワークに対して可 |
| 入力パラメータ | network: バスオフが発生した CAN ネットワーク |
| 出力パラメータ | delayCyclesPtr: バスオフ発生後、L1/L2 に加えて追加で待つ CanSM の基本周期の数 |
| 戻り値 | なし |
| 説明 | このコールアウト関数は、バスオフ発生後、L1/L2 に加えて追加で待つ CanSM の基本周期の数を返す。 |

(SRS_Can_01144, SRS_Can_01146)

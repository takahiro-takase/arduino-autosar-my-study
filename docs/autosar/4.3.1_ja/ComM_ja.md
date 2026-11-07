# AUTOSAR CP R4.3.1 Communication Manager 仕様書（和訳）

原本: `docs/autosar/4.3.1/AUTOSAR_SWS_COMManager.pdf`

## 本書について

- 学習用の参考訳です。**正本は原本（英語）**で、解釈に迷ったら必ず原本を参照してください。
- 条項番号（`[SWS_ComM_xxxxx]`）、API 名、定数名、型名、状態名は原文のまま残しています。
- 訳の範囲は第 1〜5 章、第 7 章（機能仕様）、第 8 章（API 仕様）です。目次、要件トレーサビリティ表（第 6 章）、シーケンス図（第 9 章）、コンフィギュレーション仕様（第 10 章）は含みません。
- 原本の図（ステートマシン図など）は文字として取り出せないため訳していません。図に書かれた内容が条項本文に書かれている場合は、その条項を訳しています。
- PDF からの抽出で表の列が崩れた箇所は、内容から対応を再構成し、その旨を注記しています。
- [CanSM_ja.md](CanSM_ja.md) と合わせて読むと、CanSM と ComM の連動（`CanSM_RequestComMode`、`ComM_BusSM_ModeIndication`）が追えます。

---

## 1 はじめに、および機能の概要

Communication Manager モジュール（COM Manager、ComM）は、Basic Software（BSW）の構成要素である。これはリソースマネージャであり、下位の通信サービスの制御をカプセル化する。ComM モジュールは、通信に関する Basic Software モジュールを制御するものであり、ソフトウェアコンポーネントや runnable entity を制御するものではない。ComM モジュールは、通信の要求元（第 2 章の用語「User」の定義を参照）からのバス通信アクセス要求を集め、それらを調停する。

ComM モジュールの目的は次のとおりである。

- ユーザにとってのバス通信スタックの利用を簡単にする。これには、ネットワークマネージメントの扱いの簡略化が含まれる。
- 1 つの ECU 上の、複数の独立したソフトウェアコンポーネントについて、バス通信スタックの利用可能性（信号の送受信を許可すること）を調停する。
  - コメント: ユーザはハードウェアについて（たとえばどのチャネルで通信するかを）知る必要がない。ユーザは単に「通信モード」を要求し、ComM モジュールが、対応するチャネルの通信機能を ON/OFF する。
- ECU が（能動的に）通信バスを起こすのを防ぐため、信号の送信を無効にする API を提供する。
  - コメント: CAN ではすべてのメッセージがバスを起こす。FlexRay では、ウェイクアップパターンと呼ばれるものでしかバスを起こせない。
- ECU のチャネルごとにチャネルのステートマシンを実装して、複数の通信バスチャネルを制御する。
  - コメント: ComM モジュールは、対応するバスステートマネージャモジュールに通信モードを要求する。実際のバスの状態は、対応するバスステートマネージャモジュールが制御する。
- バスを起こし続けている ECU を、強制的に「No Communication」モードにする可能性を提供する（詳細は 7.3.1.2 節を参照）。
- 要求された通信モードに必要なすべてのリソースを割り当てることで、リソース管理を簡単にする。
  - コメント: たとえば、ユーザが「Full Communication」モードを要求したときに通信が許可されているかを確認し、通信中に ECU がシャットダウンするのを防ぐ。

## 2 略語と用語の定義

### 略語

| 略語 | 意味 |
|---|---|
| BSW | Basic Software |
| BswM | Basic Software Mode Manager |
| ComM | Communication Manager |
| DCM | Diagnostic Communication Manager |
| Det | Default Error Tracer |
| EcuM | ECU State Manager モジュール |
| I-PDU | Information Protocol Data Unit |
| NM | Network Management |
| PDU | Protocol Data Unit |
| SW-C | Software Component |
| VMM | Vehicle Message Matrix |

### 用語

| 用語 | 説明 |
|---|---|
| DCM_ActiveDiagnostic indication | DCM モジュールが、アクティブな診断セッションを示すもの。DCM は診断の目的で "full communication" = COMM_FULL_COMMUNICATION を必要とする。 |
| Active wake-up | たとえばセンサなど、ホストとなる ECU が引き起こすウェイクアップ。 |
| Application signal scheduling | VMM に従ったアプリケーション信号の送信。CAN のアプリケーション信号のスケジューリングは通信モジュールが、LIN のアプリケーション I-PDU（信号を含む PDU）のスケジューリングは LIN インタフェースが、FlexRay のアプリケーション PDU のスケジューリングは FlexRay インタフェースモジュールが行う。 |
| Bus sleep | 通信バス上で動作が必要ない状態（たとえば CAN バススリープ）。 |
| Bus communication messages | 通信バス上で送信されるすべてのメッセージ。診断メッセージでもアプリケーションメッセージでもよい。 |
| COM Inhibition status | full communication、silent communication、またはウェイクアップが許可されているかどうかを定義する。 |
| Communication Channel | 送信者（または送信機）から受信者へ情報を伝えるための媒体。 |
| Communication Mode | どの種類の通信が許可されるかを決めるモード。<br>"full communication" = COMM_FULL_COMMUNICATION<br>"no communication" = COMM_NO_COMMUNICATION<br>"silent communication" = COMM_SILENT_COMMUNICATION<br>注: COMM_SILENT_COMMUNICATION は、ユーザは要求できない。シャットダウン時にネットワークを同期させるための内部モードである。 |
| Diagnostic PDU scheduling | 診断 PDU の送信。CAN の診断 PDU のスケジューリングは診断モジュールが、LIN の診断 PDU のスケジューリングは診断モジュールと LIN インタフェースが、FlexRay の診断 PDU のスケジューリングは診断モジュールと FlexRay インタフェースモジュールが行う。 |
| ECU shut down | ECU State Manager の仕様書 [6] を参照。 |
| Fan-out | 同じメッセージ / 通知が、複数の宛先 / 受信者へ送られること。 |
| Independent software component | 他のソフトウェアアプリケーションとのインタフェースを最小限にして、まとまった機能を実行する、別々に開発されたソフトウェアコンポーネント。たとえば、基本ソフトウェアコンポーネントでも、アプリケーションソフトウェアコンポーネントでもよい。 |
| Passive wake-up | 別の ECU によるウェイクアップが（たとえばバスやウェイクアップラインで）、注目している ECU へ伝わったもの。 |
| System User | デフォルトの要求を行い、ユーザの要求を上書きするための管理機能（ComM の内部コンテキストで生成される特別な「ユーザ」）。 |
| User | ECU State Manager モジュールと Communication Manager モジュールに対する要求元の概念。ユーザは、BswM、runnable entity、SW-C、または、ECU State Manager モジュールと Communication Manager モジュールに対して 1 つの単位として動作する SW-C のグループでもよい。 |
| User Request | ユーザは、ComM に対して異なる通信モードを要求できる。 |

> 注: 原本の略語表と用語表は、PDF からの抽出で列が崩れていた。略語と意味、用語と説明の対応は、内容から再構成した。

## 3 関連文書

### 3.1 入力文書

原本の一覧を参照（BSW モジュール一覧、階層型ソフトウェアアーキテクチャ、BSW 一般要件、モード管理要件、ECU コンフィギュレーション、ECU State Manager、NVRAM Manager、RTE、汎用ネットワークマネージメントインタフェース、Communication、Diagnostic Communication Manager、LIN Interface、FlexRay Interface、Default Error Tracer、CAN Transceiver Driver、CAN Interface、FlexRay Transceiver Driver、PDU Router、IPDU Multiplexer、システムサービスのモード管理、C 実装ルール、LIN State Manager、CAN State Manager、FlexRay State Manager、BSW モジュール記述テンプレート、用語集、Ethernet State Manager、Basic Software Mode Manager、ECU State Manager Fixed、BSW 一般仕様の各文書）。

### 3.2 関連する規格

該当なし。

### 3.3 関連する仕様

AUTOSAR は BSW モジュール一般に関する一般仕様書 [31]（SWS BSW General）を提供しており、これは COM Manager にも有効である。したがって、SWS BSW General は COM Manager にとって追加の必須仕様として扱わなければならない。

## 4 制約と前提

### 4.1 制限事項

参照されている成果物 AUTOSAR_SWS_ECUStateManagerFixed は、リリース 4.3.1 でステータス "obsolete"（廃止）に設定されている。

### 4.2 適用できる車両ドメイン

制限なし。

## 5 他モジュールへの依存関係

Communication Manager モジュールと他のモジュールとの依存関係を示す文脈図を、図 1 に示す。

（図 1: Communication Manager モジュールの文脈図。原本の図を参照。）

Communication Manager モジュールは、ユーザから要求された通信機能を、バスステートマネージャモジュールへ要求する。

### 5.1 ファイル構成

#### 5.1.1 ヘッダファイル構成

**[SWS_ComM_00506]** ComM モジュールは、ComM のコンフィギュレーションに応じて、ComM にインタフェースを提供するモジュールのヘッダファイルをインクルードしなければならない（図 1 参照）。

| モジュール | ヘッダファイル |
|---|---|
| ComM Schedule Manager | SchM_ComM.h |
| RTE が生成するヘッダファイル | Rte_ComM.h |
| Default Error Tracer | Det.h |
| Diagnostic Communication Manager | Dcm_Cbk.h |
| NVRAM Manager | NvM.h |
| ECU State Manager | EcuM.h |
| Network Management Interface | Nm.h |
| LIN State Manager | LinSM.h |
| CAN State Manager | CanSM_ComM.h |
| FlexRay State Manager | FrSM.h |
| Ethernet State Manager | EthSM.h |
| Basic Software Mode Manager | BswM_ComM.h |
| Communication | Com.h |

**[SWS_ComM_00956]** モジュールヘッダファイル ComM.h は、BSW モジュールとソフトウェアコンポーネントで共通に使われる型を取り込むため、Rte_ComM_Type.h をインクルードしなければならない。このファイルは、Rte_ComM_Type.h にまだ定義されていない型だけを含まなければならない。

**[SWS_ComM_00463]** ComM モジュールは、ComM_Lcfg.c と ComM_PBcfg.c に加えて、少なくとも次のファイルを提供しなければならない。

| 種類 | ファイル |
|---|---|
| ComM ヘッダファイル | ComM.h |
| ComM コールバック宣言 | ComM_Nm.h、ComM_EcuMBswM.h |
| ComM コンフィギュレーションファイル | ComM_Cfg.h |
| ComM ソースファイル | ComM.c |

(SRS_BSW_00346, SRS_BSW_00381, SRS_BSW_00412, SRS_BSW_00415)

SWS_ComM_00463 の根拠: ソースコードとコンフィギュレーションが厳密に分離される。ユーザ定義のコンフィギュレーションが、元のソースコードの変更を伴うことはない。

### 5.2 AUTOSAR Runtime Environment（RTE）

すべてのユーザは、通信モードを要求できる。RTE は、ユーザの要求を ComM モジュールへ伝え、ComM からの通信モードの通知をユーザへ伝える（詳細は [8] を参照）。

### 5.3 ECU State Manager（EcuM）

EcuM には、EcuM-Fixed と EcuM-Flex という 2 種類のバリアントを使える。2 つのバリアントの違いの詳細は、EcuM-Flex [6] と EcuM-Fixed [30] を参照。

EcuM-Fixed は ComM の初期化を担う。どちらの EcuM も、ウェイクアップイベントを検証し、ウェイクアップが検証されたら ComM へ通知する責任を持つ。

EcuM-Fixed を使う場合、EcuM-Fixed は、通信の開始が許可されるかどうかを ComM に知らせる。その後、EcuM-Fixed は、ECU をシャットダウンできるかどうか、すなわち通信が進行中かどうかを ComM に確認しなければならない。

EcuM-Flex を使う場合、上記の機能（通信の許可と ECU のシャットダウン）は、EcuM-Flex が BswM と一緒に扱う。

### 5.4 Basic Software Mode Manager（BswM）

BswM は、Application Mode Management と Vehicle Mode Management を適用できるようにするため、Mode Arbitration と Mode Control の 2 つの機能を実現する。

BswM は、BswM のアクションリストの中で設定されていれば、ユーザの要求を ComM モジュールへ伝える。これにより、BswM 経由で ComM のモードを要求できる。

BswM は、アクションリストの中で Com_IpduGroupControl の呼び出しが設定されていれば、AUTOSAR 通信モジュール（COM）の PDU グループを制御する。

**[SWS_ComM_00976]** ComM は、すべてのチャネルのメイン状態の変化と、すべての PNC 状態の変化を、BswM へ通知する。

EcuM-Flex を使う場合、BswM は、通信が許可されるかどうかを ComM へ知らせる。

### 5.5 NVRAM Manager

ComM モジュールは、不揮発データを保存および読み出すため、NVRAM Manager を使う。NVRAM データの初期値の詳細は第 10 章を参照。

コメント: NVRAM Manager は、ECU の電源投入またはリセットの後に初期化されなければならない。ComM が初期化されるとき、ComM は、NVRAM が使える状態であり、不揮発のコンフィギュレーションデータを読み戻せると仮定するので、NVRAM Manager は ComM より前に初期化されなければならない。ComM が初期化解除されるとき、ComM は不揮発データを NVRAM へ書き込む。

### 5.6 Diagnostic Communication Manager（DCM）

DCM は、診断 PDU のスケジューリングを行う。DCM は、診断を実行する場合、「DCM_ActiveDiagnostic」通知を介して通信モード COMM_FULL_COMMUNICATION を要求することで、ユーザとして動作する。DCM は、送受信を開始 / 停止する API を提供しないが、通信機能が ComM モジュールの通信モードに従うことを保証する。

### 5.7 LIN State Manager

LIN State Manager は、ComM モジュールの通信モードに対応する、LIN バスの実際の状態を制御する。ComM モジュールは LIN State Manager へ通信モードを要求し、LIN State Manager は通信モードをバスの状態へ対応付ける。

### 5.8 CAN State Manager

CAN State Manager は、ComM モジュールの通信モードに対応する、CAN バスの実際の状態を制御する。ComM モジュールは CAN State Manager へ通信モードを要求し、CAN State Manager は通信モードをバスの状態へ対応付ける。

### 5.9 FlexRay State Manager

FlexRay State Manager は、ComM モジュールの通信モードに対応する、FlexRay バスの実際の状態を制御する。ComM モジュールは FlexRay State Manager へ通信モードを要求し、FlexRay State Manager は通信モードをバスの状態へ対応付ける。

### 5.10 Ethernet State Manager

Ethernet State Manager は、ComM モジュールの通信モードに対応する、Ethernet バスの実際の状態を制御する。ComM モジュールは Ethernet State Manager へ通信モードを要求し、Ethernet State Manager は通信モードをバスの状態へ対応付ける。

### 5.11 Network Management（NM）

ComM モジュールは、ネットワーク全体で通信機能の制御を同期する（同期した起動とシャットダウン）ために NM を使う。

### 5.12 Default Error Tracer（DET）

DET は、開発エラー、ランタイムエラー、一過性のエラーを報告するサービスを提供する（7.8 節参照）。

### 5.13 Communication（COM）

**[SWS_ComM_00975]** AUTOSAR 通信モジュール（COM）は、COM 信号を使って PNC に関する状態情報を配布するために使われなければならない。

---

## 7 機能仕様

Communication Manager（ComM）モジュールは、ユーザにとってのリソース管理を簡単にする。ここでユーザとは、runnable entity、SW-C、BswM（たとえば BswM 経由の SW-C の要求）、または DCM（診断の目的で必要な通信）である。

**[SWS_ComM_00867]** ComM は 3 種類の通信モードを提供しなければならない。最も高い通信モードは COMM_FULL_COMMUNICATION でなければならない。最も低い通信モードは COMM_NO_COMMUNICATION でなければならない。(SRS_ModeMgm_09083)

**[SWS_ComM_00151]** ユーザが要求できる通信モードは、COMM_NO_COMMUNICATION と COMM_FULL_COMMUNICATION だけでなければならない（ComM_RequestComMode()、SWS_ComM_00110 参照）。

SWS_ComM_00151 の根拠: 通信モード COMM_SILENT_COMMUNICATION とサブモード / サブ状態は、AUTOSAR NM との同期のためにだけ必要である。

**[SWS_ComM_00868]** 通信モード COMM_SILENT_COMMUNICATION は、ネットワークの同期にだけ使われなければならない。(SRS_ModeMgm_09083)

注: COMM_SILENT_COMMUNICATION モードを要求できる可能性は、リリース 2.0 以降、取り除かれている。

コメント: ComM モジュールは、特定のユーザが要求した通信モードを問い合わせられる（ComM_GetRequestedComMode()、SWS_ComM_00079 参照）。

コメント: ComM モジュールは、チャネルの実際の通信モードを問い合わせられる（ComM_GetCurrentComMode()、SWS_ComM_00083 参照）。

**[SWS_ComM_00845]** COMM_FULL_COMMUNICATION モードでは、ComM モジュールは、対象の物理チャネルでの送信と受信を許可しなければならない。(SRS_ModeMgm_09083)

**[SWS_ComM_00846]** COMM_NO_COMMUNICATION モードでは、ComM モジュールは、対象の物理チャネルでの送信と受信を防がなければならない。(SRS_ModeMgm_09083)

**[SWS_ComM_00686]** 複数の独立したユーザ要求のうち少なくとも 1 つが、より高い通信モードを要求している場合（SWS_ComM_00867 と SWS_ComM_00868 参照）、ComM モジュールは、そのより高い通信モードを目標の通信モードとして設定しなければならない。(SRS_ModeMgm_09078)

SWS_ComM_00686 の根拠: ComM は、複数の独立したユーザ要求を「最も高いものが勝つ」戦略で調停する。通信モード COMM_FULL_COMMUNICATION が COMM_NO_COMMUNICATION に優先する。

**[SWS_ComM_00500]** ComM モジュールは、ユーザ要求をキューに入れてはならない。同じユーザの最新の要求は、たとえ古い要求が完了していなくても、古い要求を上書きしなければならない。

**[SWS_ComM_00866]** コンフィギュレーションパラメータ ComMNmVariant=FULL|LIGHT|NONE（ECUC_ComM_00568）の場合、DCM_ActiveDiagnostic 通知は、指定された通信チャネルに対する COMM_FULL_COMMUNICATION 要求として扱われなければならない（ComM_DCM_ActiveDiagnostic(channel)、SWS_ComM_00873 参照）。

SWS_ComM_00866 の根拠: 診断の目的で複数のチャネルが必要な場合、DCM はチャネルごとに DCM_ActiveDiagnostic を通知する必要がある。

**[SWS_ComM_00092]** 通信チャネルごとに、（SWS_ComM_00686 に従って評価された）通信モードの目標状態が 1 つなければならない。この目標モードは、対応するバスステートマネージャモジュールが制御する実際のモードと、一時的に異なることがある。

コメント: 対応するバスステートマネージャモジュールによるモードの切り替えには時間がかかり、モードの抑止が有効なこともある。

**[SWS_ComM_00084]** ComM モジュールは、ComM_GetCurrentComMode() の呼び出し（SWS_ComM_00083 参照）を、ユーザが設定されているチャネルのバスステートマネージャモジュールへ伝えなければならない（SWS_ComM_00176 と SWS_ComM_00798 も参照）。

SWS_ComM_00084 の根拠: ComM モジュールは実際のバスの状態を制御しないので、状態の要求は、対応するバスステートマネージャモジュールへ伝える必要がある。

コメント: この機能は「通常の SW-C」は使わない。通常の SW-C はチャネルについての知識を持たないためである。この機能は、システムのトポロジを知っている（知る必要がある）特権的な SW-C、たとえばシステム診断機能のために必要である。

**[SWS_ComM_00884]** ComM モジュールは、サポートするすべてのチャネルについて、チャネルの通信が許可されているかどうかを、個別の CommunicationAllowed ブール型フラグに保存しなければならない。ComM の初期化後の既定値は、通信が許可されていない状態、すなわち CommunicationAllowed=FALSE でなければならない。

**[SWS_ComM_00885]** SWS_ComM_00884 の通信の許可 / 不許可の状態変化は、ComM_CommunicationAllowed(<channel>, TRUE|FALSE)（SWS_ComM_00871）の通知で ComM に与えられなければならない。

### 7.1 Partial Network Cluster の管理

#### 7.1.1 概要

ComM は、PNC の通信モードを表すため、Partial Network Cluster（PNC）ごとにステートマシンを実装する。

各 PNC は自分の状態を持つ。状態の定義は、対応付けを簡単にするため、ComM の状態に関連している。

ComM のユーザは、PNC を要求および解放するために使われる。

システムチャネルのノード上のすべての PNC の状態は、ネットワークマネージメントのユーザデータを介して交換される。

各 PNC は、CAN と FlexRay の NM ユーザデータの中のビットベクタ内の、専用のビット位置を使う。PNC がノード上のローカルな ComM ユーザに要求された場合、ノードは NM ユーザデータの対応するビットを 1 に設定する。PNC がもう要求されなくなったら、ノードは NM ユーザデータの対応するビットを 0 に設定する。BusNm は、PNC の NM ユーザデータを集めて集約し、その状態を COM のビットベクタとして COM 信号を介して ComM へ提供する。

各 PNC は、NM を持つすべてのシステムチャネルで、NM ユーザデータの同じビット位置を使う。ComM は、PNC の状態情報を交換するために、EIRA と ERA という 2 種類のビットベクタを使う。「EIRA」と「ERA」の定義は、AUTOSAR SWS CAN NM と AUTOSAR SWS FlexRay NM にある。

ComM は、ノード上の PNC に必要なシステム通信バスチャネルを要求および解放する。

ノードでの Partial Network Cluster の管理の有効化と無効化は、ポストビルドでコンフィギュレーションできなければならない。

Partial Networking は、バスの種類 CAN と FlexRay でサポートされなければならない。誤ったタイムアウトを避けるため、FlexRay ノードでは、PNC の I-PDU グループの有効化と無効化が必要である。COM での I-PDU グループの開始と停止は、BswM が扱う。単独の FlexRay ECU を無効にすることはできない。

#### 7.1.2 Partial Network Cluster 管理の機能

**[SWS_ComM_00910]** PNC の機能は、パラメータ ComMPncSupport が TRUE に設定されている場合にのみ存在しなければならない（ECUC_ComM_00839 参照）。

**[SWS_ComM_00911]** PNC 機能の有効化と無効化は、パラメータ ComMPncEnabled（ECUC_ComM_00878 参照）を使って、ポストビルドでコンフィギュレーションできなければならない。

コメント: ComM モジュールは、PNC ステートマシンのすべての状態変化を、BswM_ComM_CurrentPncMode() を呼ぶことで BswM へ通知する（SWS_ComM_00908 参照）。

**[SWS_ComM_00982]** PNC の状態情報を交換するには、ビットベクタを使わなければならない（すなわち、最大 56 個の PNC 状態ビットを含む信号を 1 つだけ使う）。

コメント: ComM は、PNC のビットベクタが uint8_n 型の配列として設定されていることを前提とする。コンフィギュレーションパラメータ ComMPncComSignalRef を参照。

**[SWS_ComM_00825]** 1 つの ComMPncId に対応するビットがある byteIndex と bitIndex は、次のように決めなければならない。
- byteIndex = (ComMPncId div 8) - <PNC Vector Offset>
- bitIndex = (ComMPncId mod 8)

ヒント: <PNC Vector Offset>（と、必要なら <PNC Vector Length>）の値は、<Bus> Network Management モジュールのコンフィギュレーションから得られる。

コメント: SWS_ComM_00825 は、byteIndex と bitIndex の計算だけを定義しており、どう実装するかは定義していない。

**[SWS_ComM_00984]** ComM は、ComMPncComSignalKind が EIRA または ERA であるビットベクタ（信号）を、Com_ReceiveSignal() を使って受け取る。

**[SWS_ComM_00986]** ComM は、モジュール通信内の信号の変化を知らせるため、API ComM_COMCbk_<sn>() を提供しなければならない。

**[SWS_ComM_00916]** ComM モジュールは、ComMPncComSignalDirection が "TX" としてその PNC に割り当てられた、PNC の状態を表すビットを含むビットベクタとして、1 つ以上の COM 信号を使い、1 つ以上の通信バスを介して、PNC の状態（PNC ステートマシンの結果）を配布できなければならない（詳細は図 2: PNC ステートマシンを参照）。

#### 7.1.3 ComM PNC ステートマシン

**[SWS_ComM_00953]** コンフィギュレーションパラメータ ComMPncEnabled が TRUE に設定されて PNC 機能が有効な場合（ECUC_ComM_00878 参照）、PNC の変化に関連するすべての動作は、チャネルに関連する動作（チャネルに関連する動作は 7.3 章を参照）より前に実行されなければならない。

**[SWS_ComM_00909]** Partial Network ごとに、PNC ステートマシンを 1 つだけ実装しなければならない（すなわち、ComMChannel の数にかかわらず、PNC ごとに 1 つの PNC ステートマシン）。

**[SWS_ComM_00920]** ComM モジュールは、最大 56 個の PNC ステートマシンをサポートしなければならない。

**[SWS_ComM_00924]** PNC ステートマシンは、COMM_PNC_FULL_COMMUNICATION と COMM_PNC_NO_COMMUNICATION の 2 つのメイン状態で構成されなければならない。

**[SWS_ComM_00907]** PNC のメイン状態 COMM_PNC_FULL_COMMUNICATION は、サブ状態 COMM_PNC_PREPARE_SLEEP、COMM_PNC_READY_SLEEP、COMM_PNC_REQUESTED で構成されなければならない。

**[SWS_ComM_00908]** （ComM_PncModeType に列挙された）すべての状態変化は、PowerOff から来るメイン状態 COMM_PNC_NO_COMMUNICATION への進入を除き、API 呼び出し BswM_ComM_CurrentPncMode() で、進入した PNC の状態とともに通知されなければならない。

**[SWS_ComM_00978]** ComM_RequestComMode() の呼び出しによってトリガされる、ComM の PNC ステートマシンの状態遷移は、ComM_MainFunction_<Channel_Id> の中でのみ実行されなければならない。

コメント: PN の有効化のたびに PN ベクタが n 回送信されるため、デバウンスしないとバス負荷が増える。

**[SWS_ComM_00944]** 信号の種類が "EIRA" である Rx ビットベクタの中で、PNC に対応するビットの少なくとも 1 つが '1' に等しい場合、ComM 内の EIRA のうち、この PNC に対応するビットを '1' に設定しなければならない。

**[SWS_ComM_00945]** コンフィギュレーションパラメータ ComMPncGatewayEnabled（ECUC_ComM_00840 参照）が true で、ComMChannel のパラメータ ComMPncGatewayType が COMM_GATEWAY_TYPE_ACTIVE に設定されており、かつ、信号の種類が "ERA" である Rx ビットベクタの中で PNC に対応するビットの少なくとも 1 つが '1' に等しい場合、ComM 内の ERA のうち、この PNC に対応するビットを '1' に設定しなければならない。

**[SWS_ComM_00971]** トリガ ComM_COMCbk は、ERA または EIRA と呼ばれる PNC の状態情報を含む受信信号についての、AUTOSAR 通信モジュールによる通知を表す。

**[SWS_ComM_00972]** トリガ "ComMUser" は、API ComM_RequestComMode() の呼び出しによる、ComMUser の通信要求についての通知を表す。

**[SWS_ComM_00987]** 1 つ以上の PNC にマップされたチャネルの ComM_MainFunction_<Channel_Id> の中では、要求された状態を次の順序で扱わなければならない。

1. そのチャネルの 1 つ以上の PNC にマップされた ComM ユーザの、ComM ユーザ要求
2. そのチャネルにマップされた ComM ユーザの、ComM ユーザ要求
3. ERA（コンフィギュレーションスイッチ ComMPncGatewayEnabled が TRUE に設定されている場合）
4. EIRA

コメント: 要求は、それが影響するチャネルのメイン関数の中で扱われる。

**[SWS_ComM_00919]** コンフィギュレーションコンテナ ComMPncComSignal（ECUC_ComM_00881 参照）を使って、PNC を表すビットを含む COM 信号を、1 つの PNC に 2 つ以上割り当てられなければならない。

根拠: これにより、コンフィギュレータは、たとえば 1 つの EIRA と n 個の ERA を 1 つの PNC に割り当てられる。

コメント: EIRA の異なる ID は、物理的にサポートされるチャネルの FlexRay、Can1、Can2 … へコンフィギュレーションできる。

**[SWS_ComM_00827]** 「通信の許可（Communication allowed）」とモードの抑止に関しては、PNC ステートマシンに由来する要求は、対応するチャネルに対するユーザ要求と同様に扱われなければならない。

（図 2: PNC ステートマシン。原本の図を参照。図には次の遷移条件が書かれている。）
- `ComM_RequestComMode()` または `ComM_COMCbk()` で [ComMUser = FullCom、または（ERAn の PNC ビット = 1 かつ ComMPncGatewayEnabled = TRUE）] のとき、`BswM_ComM_CurrentPncMode()` と `Com_SendSignal()` を呼び、「チャネルの Full Communication 要求」を行う。
- `ComM_RequestComMode()` または `ComM_COMCbk()` で [ComMUser = NoCom かつ（ERAn の PNC ビット = 1 かつ ComMPncGatewayEnabled = TRUE）でない] のとき、`BswM_ComM_CurrentPncMode()` と `Com_SendSignal()` を呼び、「チャネルの Full Communication 要求を解放」する。
- `ComM_COMCbk()` で [EIRA の PNC ビット = 1] のとき、`BswM_ComM_CurrentPncMode()` を呼ぶ。
- `ComM_EcuM_PNCWakeUpIndication()` で `BswM_ComM_CurrentPncMode()` を呼び、`ComMPncPrepareSleepTimer` を開始する。タイマが [expired（満了）] したら `BswM_ComM_CurrentPncMode()` を呼ぶ。

##### 7.1.3.1 PNC のメイン状態 COMM_PNC_NO_COMMUNICATION での振る舞い

**[SWS_ComM_00926]** PNC のメイン状態 COMM_PNC_NO_COMMUNICATION は、電源オフからの既定の PNC 状態でなければならない。

PNC が ECU 内部からも外部からも要求されない限り、メイン状態 COMM_PNC_NO_COMMUNICATION が目標状態である。

**[SWS_ComM_00931]** PNC 状態 COMM_PNC_NO_COMMUNICATION で API ComM_EcuM_WakeUpIndication() が呼ばれ、かつコンフィギュレーションスイッチ ComMSynchronousWakeUp が TRUE に設定されている場合（ECUC_ComM_00695 参照）、PNC のメイン状態 COMM_PNC_NO_COMMUNICATION を離れ、PNC のサブ状態 COMM_PNC_PREPARE_SLEEP に入らなければならない。

**[SWS_ComM_00990]** PNC 状態 COMM_PNC_NO_COMMUNICATION で API ComM_EcuM_WakeUpIndication() が呼ばれ、かつコンフィギュレーションスイッチ ComMSynchronousWakeUp が FALSE に設定されている場合、PNC のメイン状態 COMM_PNC_NO_COMMUNICATION が現在の状態のままでなければならない。

コメント: 非同期ウェイクアップの場合、PNC 状態は、PNC の要求を受け取るまで（EIRA の PNC ビットが '1' に設定されるまで）COMM_PNC_NO_COMMUNICATION にとどまらなければならない。

**[SWS_ComM_00964]** PNC 状態 PNC_NO_COMMUNICATION で API ComM_EcuM_PNCWakeUpIndication(<PNC>) が呼ばれた場合、PNC のメイン状態 PNC_NO_COMMUNICATION を離れ、PNC のサブ状態 PNC_PREPARE_SLEEP に入らなければならない。

**[SWS_ComM_00932]** PNC のメイン状態 COMM_PNC_NO_COMMUNICATION で、この PNC に割り当てられた ComMUser の少なくとも 1 つが「Full Communication」を要求した場合、この状態を離れ、メイン状態 COMM_PNC_FULL_COMMUNICATION のサブ状態 COMM_PNC_REQUESTED に入らなければならない。

**[SWS_ComM_00933]** メイン状態 COMM_PNC_NO_COMMUNICATION で、この PNC を表す EIRA のビットの少なくとも 1 つが '1' に変化した場合、メイン状態 COMM_PNC_NO_COMMUNICATION を離れ、COMM_PNC_READY_SLEEP に入らなければならない。

**[SWS_ComM_00934]** メイン状態 COMM_PNC_NO_COMMUNICATION で、この PNC を表す ERAn のビットの少なくとも 1 つが '1' に変化した場合、パラメータ ComMPncGatewayEnabled（ECUC_ComM_00840）が TRUE に等しいなら、メイン状態 COMM_PNC_NO_COMMUNICATION を離れ、サブ状態 COMM_PNC_REQUESTED に入らなければならない。

##### 7.1.3.2 PowerOff からの PNC メイン状態 COMM_PNC_NO_COMMUNICATION への進入時

**[SWS_ComM_00927]** 電源を入れた後、PowerOff からメイン状態 COMM_PNC_NO_COMMUNICATION に入らなければならない。

##### 7.1.3.3 PNC のメイン状態 COMM_PNC_FULL_COMMUNICATION での振る舞い

**[SWS_ComM_00929]** この PNC に割り当てられたすべての ComMChannel は、Full Communication 状態になければならない。

##### 7.1.3.4 PNC のサブ状態 COMM_PNC_REQUESTED への進入時

**[SWS_ComM_00930]** PNC のサブ状態 COMM_PNC_REQUESTED に入るとき、ComMPncGatewayEnabled = FALSE であれば、ComMPncComSignalDirection が "TX" としてこの PNC に割り当てられた Com 信号について、この PNC を表すビットに値 '1' を指定して API Com_SendSignal() を呼ばなければならない。

**[SWS_ComM_00992]** PNC のサブ状態 COMM_PNC_REQUESTED に入るとき、ComMPncGatewayEnabled = TRUE であれば、この PNC に関連し ComMPncGatewayType == COMM_GATEWAY_TYPE_ACTIVE であるすべての ComM チャネルについて、ComMPncComSignalDirection が "TX" としてこの PNC に割り当てられた Com 信号の、この PNC を表すビットに値 '1' を指定して、API Com_SendSignal() を呼ばなければならない。

**[SWS_ComM_00993]** サブ状態 COMM_PNC_REQUESTED に他の状態から入るたびに、ComM は、この PNC に設定されたすべての ComM チャネルについて、たとえそのチャネルがすでに要求済みであっても、COMM_FULL_COMMUNICATION を要求しなければならない。

##### 7.1.3.5 PNC のサブ状態 COMM_PNC_REQUESTED での振る舞い

**[SWS_ComM_00164]** ComMPncGatewayEnabled == TRUE であり、COMM_PNC_REQUESTED への進入時または COMM_PNC_REQUESTED の中で、次のいずれかの場合、この PNC に関連し ComMPncGatewayType == COMM_GATEWAY_TYPE_PASSIVE であるすべての ComM チャネルについて、ComMPncComSignalDirection == "TX" としてこの PNC に割り当てられた Com 信号の、この PNC を表すビットに値 "1" を指定して、API Com_SendSignal() を呼ばなければならない。
- この PNC に割り当てられた ComM ユーザの少なくとも 1 つが "Full Com" を要求している場合。
- 信号の属性が ComMPncComSignalDirection == "RX" かつ ComMPncComSignalKind == "ERA" で、チャネルの属性が ComMPncGatewayType == "COMM_GATEWAY_TYPE_ACTIVE" であるチャネルから Com_ReceiveSignal() で受け取った ComMPncComSignal の少なくとも 1 つが "1" に設定されている場合。

(SRS_ModeMgm_09243)

**[SWS_ComM_00959]** ComMPncGatewayEnabled == TRUE であり、COMM_PNC_REQUESTED の中で、次のすべてが成り立つ場合、この PNC に関連し ComMPncGatewayType == COMM_GATEWAY_TYPE_PASSIVE であるすべての ComM チャネルについて、ComMPncComSignalDirection == "TX" としてこの PNC に割り当てられた Com 信号の、この PNC を表すビットに値 "0" を指定して、API Com_SendSignal() を呼ばなければならない。
- この PNC に割り当てられたすべての ComM ユーザが "No Com" を要求している。
- 信号の属性が ComMPncComSignalDirection == "RX" かつ ComMPncComSignalKind == "ERA" で、チャネルの属性が ComMPncGatewayType == "COMM_GATEWAY_TYPE_ACTIVE" であるチャネルから Com_ReceiveSignal() で受け取ったすべての ComMPncComSignal が "0" に設定されている。

(SRS_ModeMgm_09243)

この PNC に割り当てられた ComMUser の少なくとも 1 つが「Full Communication」を要求している限り、COMM_PNC_REQUESTED が現在の PNC 状態である。

**[SWS_ComM_00966]** ComM0PncVectorAvoidance が TRUE に設定されており、参照される ComMChannel のすべての信号が（SWS_ComM_00959 により）'0' に設定されている場合、ComM モジュールはこの ComMChannel を解放しなければならない。少なくとも 1 つの信号が再び '1' に戻ったらすぐに、ComM モジュールはこの ComMChannel を再び要求しなければならない。

PNC がリモートから要求されている間（すなわち、この PNC に割り当てられた ERA 信号内のビットの少なくとも 1 つが '1' に等しい間）、かつコンフィギュレーションスイッチ ComMPncGatewayEnabled が TRUE に設定されている間（ECUC_ComM_00840 参照）、COMM_PNC_REQUESTED が現在の PNC 状態である。

**[SWS_ComM_00938]** この PNC に割り当てられたすべての ComMUser が「No Communication」を要求した場合、コンフィギュレーションスイッチ ComMPncGatewayEnabled が FALSE に設定されているなら、サブ状態 COMM_PNC_REQUESTED を離れ、サブ状態 COMM_PNC_READY_SLEEP に入らなければならない。

**[SWS_ComM_00991]** この PNC に割り当てられたすべての ComMUser が「No Communication」を要求し、かつすべての ERAn の PNC ビットが 0 に等しい場合、コンフィギュレーションスイッチ ComMPncGatewayEnabled が TRUE に設定されているなら、サブ状態 COMM_PNC_REQUESTED を離れ、サブ状態 COMM_PNC_READY_SLEEP に入らなければならない。

##### 7.1.3.6 PNC のサブ状態 COMM_PNC_READY_SLEEP への進入時

**[SWS_ComM_00960]** COMM_PNC_REQUESTED から PNC のサブ状態 COMM_PNC_READY_SLEEP に入るとき、ComMPncComSignalDirection が "TX" としてこの PNC に割り当てられたすべての Com 信号について、この PNC を表すビットに値 '0' を指定して API Com_SendSignal() を呼ばなければならない。

**[SWS_ComM_00961]** COMM_PNC_REQUESTED から PNC のサブ状態 COMM_PNC_READY_SLEEP に入るとき、ComM は、この PNC に設定されたすべての ComM チャネルについて、COMM_FULL_COMMUNICATION の要求を解放しなければならない。

##### 7.1.3.7 PNC のサブ状態 COMM_PNC_READY_SLEEP での振る舞い

PNC が要求されており（すなわち EIRA 内の PNC ビットの少なくとも 1 つが '1' に等しい）、かつこの PNC に割り当てられた ComMUser が「Full Communication」を要求していない間、COMM_PNC_READY_SLEEP が現在の状態である。

**[SWS_ComM_00940]** PNC が解放された場合（すなわち EIRA 内のすべての PNC ビットが '0' に等しい）、サブ状態 COMM_PNC_READY_SLEEP を離れ、サブ状態 COMM_PNC_PREPARE_SLEEP に入らなければならない。

**[SWS_ComM_00165]** PNC のサブ状態 COMM_PNC_READY_SLEEP で、この PNC に割り当てられた ComMUser の少なくとも 1 つが「Full Communication」を要求した場合、または ComMPncGatewayEnabled（ECUC_ComM_00887）が TRUE に等しく、かつ ERAn 内のこの PNC を表すビットの 1 つが '1' に変化した場合、この状態を離れ、サブ状態 COMM_PNC_REQUESTED に入らなければならない。(SRS_ModeMgm_09246, SRS_ModeMgm_09247, SRS_ModeMgm_09248)

##### 7.1.3.8 PNC のサブ状態 COMM_PNC_PREPARE_SLEEP への進入時

**[SWS_ComM_00952]** サブ状態 COMM_PNC_PREPARE_SLEEP に入る場合、タイマ ComMPncPrepareSleepTimer（ECUC_ComM_00841 参照）を、設定された初期値で開始しなければならない。

##### 7.1.3.9 PNC のサブ状態 COMM_PNC_PREPARE_SLEEP での振る舞い

タイマ ComMPncPrepareSleepTimer（ECUC_ComM_00841 参照）が動作している間、かつ ComMUser、EIRA、ERAn に変化が起きない間、COMM_PNC_PREPARE_SLEEP が現在の状態である。

**[SWS_ComM_00947]** タイマ ComMPncPrepareSleepTimer（ECUC_ComM_00841 参照）が満了したとき、PNC のサブ状態 COMM_PNC_PREPARE_SLEEP を離れ、PNC のメイン状態 COMM_PNC_NO_COMMUNICATION に入らなければならない。

**[SWS_ComM_00948]** COMM_PNC_PREPARE_SLEEP で、この PNC に割り当てられた ComMUser の少なくとも 1 つが「Full Communication」を要求した場合、COMM_PNC_PREPARE_SLEEP 状態を離れなければならない。タイマ ComMPncPrepareSleepTimer を停止し、サブ状態 COMM_PNC_REQUESTED 状態に入らなければならない。

**[SWS_ComM_00950]** COMM_PNC_PREPARE_SLEEP で、EIRA 内の PNC ビットの少なくとも 1 つが '1' に変化した場合、サブ状態 COMM_PNC_PREPARE_SLEEP を離れなければならない。タイマ ComMPncPrepareSleepTimer を停止し、サブ状態 COMM_PNC_READY_SLEEP に入らなければならない。

**[SWS_ComM_00951]** サブ状態 COMM_PNC_PREPARE_SLEEP で、ERAn 内の PNC ビットの少なくとも 1 つが '1' に変化し、かつパラメータ ComMPncGatewayEnabled が TRUE に等しい場合、サブ状態 COMM_PNC_PREPARE_SLEEP を離れなければならない。タイマ ComMPncPrepareSleepTimer を停止し、サブ状態 COMM_PNC_REQUESTED に入らなければならない。

#### 7.1.4 PNC ゲートウェイ

**[SWS_ComM_00981]** コンフィギュレーションパラメータ ComMPncGatewayEnabled（ECUC_ComM_00840 参照）が TRUE の場合、既定のゲートウェイの種類は active（COMM_GATEWAY_TYPE_ACTIVE）でなければならない。

SWS_ComM_00981 へのコメント: 両方の信号の種類（すなわち ComMPncComSignalKind = EIRA と ComMPncComSignalKind = ERA）が設定されていると仮定できる。

##### 7.1.4.1 Active PNC ゲートウェイ

注: コンフィギュレーションパラメータ ComMPncGatewayEnabled（ECUC_ComM_00840 参照）が TRUE で、ComMChannel（ECUC_ComM_00842 参照）のパラメータ ComMPncGatewayType が COMM_GATEWAY_TYPE_ACTIVE に設定されていても、active PNC ゲートウェイは、図 2: PNC ステートマシンに示されたとおりに動作する。

コメント: システムチャネル上の active PNC ゲートウェイは、そのシステムチャネル上で PNC を解放する最後のノードでなければならない。

コメント: すべての ERAn で PNC のビットがゼロに等しい場合、PNC ゲートウェイ以外のノードは PNC を要求していない。

##### 7.1.4.2 Passive PNC ゲートウェイ

コメント: passive に調停されるチャネルは、2 つ以上の PNC ゲートウェイに接続されている場合にのみ存在する。ComM の PNC ゲートウェイ機能が有効な場合（ComMPncGatewayEnabled = true）、このゲートウェイにマップされた ComM チャネルは、active または passive の種類（COMM_GATEWAY_TYPE_ACTIVE または COMM_GATEWAY_TYPE_PASSIVE）に設定できる。ComM チャネルが 2 つの異なる PNC ゲートウェイにマップされている場合、一方のゲートウェイだけがこのチャネルを active に調停し、もう一方は passive に調停する。すなわち、PNC ゲートウェイは常に、種類が active の ComM チャネルに少なくとも 1 つマップされ、種類が passive の ComM チャネルに 1 つまたはいくつかマップされることがある。

コメント: PNC ゲートウェイは、ローカルの ComM ユーザが PNC を要求した場合、または、passive PNC ゲートウェイの active に調停されるシステムチャネルに由来する ERA 内の PNC ビットの少なくとも 1 つが 0 に等しくない場合に、PNC を要求する。

SWS_ComM_00959 へのコメント: PNC ゲートウェイは、passive に調停されるチャネルに送る ERA の Tx ビットベクタ内の PNC のビット値を、active に調停されるチャネルの ERA 内のビット値と同じ方法で計算するが、SWS_ComM_00959 の規則に従って PNC のビットを '0' に設定する。

#### 7.1.5 ComM ユーザと PNC の関係

図 3: ユーザから Partial Network とチャネルへのマッピングのユースケース（原本の図を参照）。図には次の 7 つのユースケースが示される。

1. Partial Network を伴うコンフィギュレーション
2. 冗長なコンフィギュレーション
3. Partial Network を伴うコンフィギュレーション（PN 1 と PN 2、チャネル 1 と 2）
4. Partial Network を伴うコンフィギュレーション
5. Partial Network を伴わないコンフィギュレーション
6. Partial Network と EthIfSwitch-Portgroup を伴うコンフィギュレーション（EthIfSwitch-Portgroup の制御は BswM 経由）
7. Partial Network と EthIfSwitch-Portgroup を伴わないコンフィギュレーション（EthIfSwitch-Portgroup の制御はチャネルの要求経由）

**[SWS_ComM_00912]** パラメータ ComMUserPerPnc（ECUC_ComM_00876 参照）を使って、設定可能な数の ComMUser を 1 つ以上の PNC にマップできなければならない。

**[SWS_ComM_00994]** Partial Networking のための BusNm フィルタのコンフィギュレーションからの制約は、ComM ユーザの PNC への割り当てに適用されてはならない。

コメント: BusNM フィルタのコンフィギュレーションは、ComM の PNC のコンフィギュレーションから独立していなければならない。

根拠: これにより、PNC のメンバでなくても PNC を起こせる。たとえば、ノードが PNC のウェイクアップをトリガするだけで、そのノードは PNC によって起こされたままにはならず、他のノードが PNC を起こしたままにする場合である。

**[SWS_ComM_00995]** パラメータ ComMUserPerChannel を使って、設定可能な数の ComMUser を 1 つ以上の ComM チャネルにマップできなければならない。

コメント: 後方互換性のため、ComM ユーザからシステムチャネルへの既存のマッピングも、引き続き可能でなければならない（すなわち、コンフィギュレーションコンテナは変更しないままとする）。

**[SWS_ComM_00913]** パラメータ ComMChannelPerPnc（ECUC_ComM_00880 参照）を使って、設定可能な数の PNC を設定可能な数の ComM チャネルにマップできなければならない。

**[SWS_ComM_00996]** ComMUser を、PNC に、かつ、すでにその PNC によって参照されている ComM チャネルにもマップすることはできてはならない（図 3 のユースケース 2 参照）。

根拠: チャネルはすでに PNC によって暗黙に参照されているので、冗長なコンフィギュレーションを避ける。

**[SWS_ComM_CONSTR_00001]** PNC によって参照される ComM チャネルは、その PNC が少なくとも 1 つの EthIfSwitchPortGroup を参照している場合、どの ComMUser からも参照されてはならない（図 3 のユースケース 6 参照）。コンフィギュレーションツールは、そのようなコンフィギュレーションを無効（エラー）として拒否しなければならない。この制約は、Ethernet スイッチを制御するホスト ECU でのみ有効である。それ以外のすべてのユースケースでは、ComMChannel は PNC と ComMUser の両方から参照できる。

根拠: PNC を使い、SwitchPortGroup が導出される場合（EcuInstance.ethSwitchPortGroupDerivation==TRUE）、SwitchPortGroup は、チャネルの要求ではなく、EthIf_SwitchPortGroupRequestMode API で切り替えられる。

#### 7.1.6 Partial Networking のコンフィギュレーションのヒント

通常、PNCSignal はすべて、その PNC にリンクされた ComMChannel で送受信される。しかし、これは必須ではなく、次のようなコンフィギュレーションも許される。

- PNCSignal を、この PNC に**リンクされていない** ComMChannel で送信できる。
- PNCSignal を、この PNC に**リンクされていない** ComMChannel を介して受信できる。

### 7.2 ComM チャネルステートマシン

**[SWS_ComM_00979]** オプションの PNC 機能が有効な場合（ECUC_ComM_00883 参照）、チャネルに関連する動作を実行する前に、すべての PNC の動作を行わなければならない。

**[SWS_ComM_00980]** パラメータ ComMPncNmRequest が TRUE に等しく（ECUC_ComM_00886 参照）、PNC ステートマシンの COMM_PNC_REQUESTED への変化（SWS_COMM_00993 参照）によって「FULL Communication」が要求される場合、現在の状態がすでに「Full communication」であっても、API Nm_NetworkRequest() を呼ばなければならない。

根拠: これは、NM が NM メッセージを n 回（n は設定可能）すぐに送信し、PNC トランシーバのウェイクアップと同期を確実にするためのトリガである。

**[SWS_ComM_00051]** ComM は、図 4 に示すチャネルステートマシンを、表 1 に列挙された要件とともに、通信チャネルごとに独立して 1 つ実装しなければならない。(SRS_ModeMgm_09080)

SWS_ComM_00051 の根拠: チャネルごとに必要な通信機能は異なることがあるため、制御は独立していなければならない。

SWS_ComM_00051 のユースケース: CAN と LIN のチャネルを持つ ECU で、たとえばセンサ値を要求するために LIN だけがフル通信を必要とし、CAN は非アクティブのままである場合。

図 4: ComM チャネルステートマシン（原本の図を参照）。図には次の遷移が書かれている。

- `COMM_NO_COMM_NO_PENDING_REQUEST` → `COMM_NO_COM_REQUEST_PENDING`: `ComM_RequestComMode(<user>, COMM_FULL_COMMUNICATION)`
- `COMM_NO_COM_REQUEST_PENDING` → `COMM_FULL_COMMUNICATION`（`COMM_FULL_COM_NETWORK_REQUESTED`）: [EcuM がそのチャネルについて RUN に入った]、[CommunicationAllowed=TRUE]、[通信の制限が無効]
- `COMM_NO_COM_REQUEST_PENDING` → `COMM_NO_COM_NO_PENDING_REQUEST`: `ComM_RequestComMode(<user>, COMM_NO_COMMUNICATION)`
- `COMM_FULL_COM_NETWORK_REQUESTED` → `COMM_FULL_COM_READY_SLEEP`: `ComM_RequestComMode(<user>, COMM_NO_COMMUNICATION)`
- `COMM_FULL_COM_READY_SLEEP` → `COMM_FULL_COM_NETWORK_REQUESTED`: `ComM_RequestComMode(<user>, COMM_FULL_COMMUNICATION)` または `ComM_DCM_ActiveDiagnostic(ChX)`
- `COMM_FULL_COMMUNICATION` → `COMM_SILENT_COMMUNICATION`: `ComM_Nm_PrepareBusSleepMode(ChX)`
- `COMM_SILENT_COMMUNICATION` → `COMM_NO_COMMUNICATION`: `ComM_Nm_BusSleepMode(ChX)`
- `COMM_SILENT_COMMUNICATION` → `COMM_FULL_COMMUNICATION`: `ComM_Nm_NetworkMode(ChX)`
- `COMM_FULL_COM_READY_SLEEP` → `COMM_NO_COMMUNICATION`: タイマ ComMLightTimeout の満了（[ComMNmVariant = LIGHT]、[COMM_BUS_TYPE_INTERNAL]）、または `ComM_Nm_BusSleepMode(ChX)`

凡例: ChX = チャネル ID、<user> = ChX に設定されたユーザ、PNC = PNC ID。

> 注: 図の遷移は、PDF から抽出された文字（矢印の向きは含まれない）と、表 1 の要件番号から**推定して再構成した**ものである。矢印の向きと、`ComM_Nm_BusSleepMode(ChX)` がどの遷移に付くかは、必ず原本の図 4 と、次の 7.2.1 節以降の各条項で確認すること。

**表 1: チャネルステートマシン、およびその遷移の詳細な説明へのリンク**

| 状態 | 節 / 要件 |
|---|---|
| COMM_NO_COMMUNICATION | 7.2.1 |
| COMM_SILENT_COMMUNICATION | SWS_ComM_00313、SWS_ComM_00073。サブ状態 COMM_NO_COM_NO_PENDING_REQUEST では SWS_ComM_00296 |
| COMM_FULL_COMMUNICATION | 7.2.3。状態への進入: SWS_ComM_00069。状態の中: SWS_ComM_00637、SWS_ComM_00826 |
| サブ状態 COMM_FULL_COM_NETWORK_REQUESTED | 7.2.3.1。サブ状態の中: SWS_ComM_00869、SWS_ComM_00870、SWS_ComM_00665、SWS_ComM_00888、SWS_ComM_00889、SWS_ComM_00890 |
| サブ状態 COMM_FULL_COM_READY_SLEEP | 7.2.3.2。サブ状態への進入: SWS_ComM_00133。サブ状態の中: SWS_ComM_00610、SWS_ComM_00671、SWS_ComM_00882、SWS_ComM_00883 |

| 遷移（元 → 先） | 要件 |
|---|---|
| COMM_NO_COMMUNICATION → COMM_FULL_COMMUNICATION | SWS_ComM_00893、SWS_ComM_00894、SWS_ComM_00694、SWS_ComM_00875、SWS_ComM_00876、SWS_ComM_01014、SWS_ComM_01015 |
| COMM_FULL_COM_NETWORK_REQUESTED → COMM_FULL_COM_READY_SLEEP | SWS_ComM_00665 |
| COMM_FULL_COM_READY_SLEEP → COMM_FULL_COM_NETWORK_REQUESTED | SWS_ComM_00882、SWS_ComM_00883 |
| COMM_FULL_COMMUNICATION → COMM_SILENT_COMMUNICATION | SWS_ComM_00826 |
| COMM_FULL_COM_READY_SLEEP → COMM_NO_COMMUNICATION | SWS_ComM_00610、SWS_ComM_00671 |
| COMM_FULL_COMMUNICATION → COMM_NO_COMMUNICATION | SWS_ComM_00637 |
| COMM_SILENT_COMMUNICATION → COMM_FULL_COMMUNICATION | SWS_ComM_00877、SWS_ComM_00878 |
| COMM_SILENT_COMMUNICATION → COMM_FULL_COM_READY_SLEEP | SWS_ComM_00296 |
| COMM_SILENT_COMMUNICATION → COMM_NO_COMMUNICATION | SWS_ComM_00295 |

> 注: 原本の表 1 は PDF からの抽出で列が崩れていた。状態と要件の対応は、行の並びから再構成した。原本の表で確認すること。

**[SWS_ComM_00879]** ComM チャネルステートマシンは、通信モードに対応する 3 つのメイン状態、COMM_NO_COMMUNICATION、COMM_SILENT_COMMUNICATION、COMM_FULL_COMMUNICATION で構成されなければならない。

**[SWS_ComM_00880]** COMM_FULL_COMMUNICATION 状態は、2 つのサブ状態 COMM_FULL_COM_NETWORK_REQUESTED と COMM_FULL_COM_READY_SLEEP を持たなければならない。

**[SWS_ComM_00881]** COMM_NO_COMMUNICATION 状態は、2 つのサブ状態 COMM_NO_COM_REQUEST_PENDING と COMM_NO_COM_NO_PENDING_REQUEST を持たなければならない。

SWS_ComM_00879 と SWS_ComM_00880 の根拠: COMM_FULL_COM_READY_SLEEP と COMM_SILENT_COMMUNICATION は、バス上の通信のシャットダウンを同期させるために必要である。1 つの ECU だけが通信を切ると、その ECU がアプリケーション信号の送信を止めるため、他の ECU がエラーを記録してしまう。

コメント: メイン状態は、ユーザの関心の対象である、チャネルごとの通信機能の抽象化された状態を示す。サブ状態は、外部のパートナーおよび管理プロトコル（たとえば NM）との同期した遷移をサポートする動作を行う中間状態を表す。

**[SWS_ComM_00485]** 各 ComM チャネルステートマシンの既定の状態は、COMM_NO_COMMUNICATION でなければならない。

**[SWS_ComM_00896]** 各 ComM チャネルステートマシンは、SWS_ComM_00884 に従った、対応する通信状態フラグ CommunicationAllowed を、サブ状態 COMM_NO_COM_REQUEST_PENDING でのみ評価しなければならない。

SWS_ComM_00896 の根拠: ComM_CommunicationAllowed(<channel>, FALSE)（SWS_ComM_00871）の通知は、チャネルがサブ状態 COMM_NO_COM_REQUEST_PENDING にない場合、目に見える効果を持たない。すなわち、ComM チャネルステートマシンは、たとえば COMM_FULL_COMMUNICATION のような別の状態にある場合、すぐに状態 COMM_NO_COMMUNICATION へ変化することはない。

**[SWS_ComM_00472]** メイン状態の変化（SWS_ComM_00879 参照）は、対応する通知（8.6.1.5 節と 8.6.1.6 節参照）でユーザへ知らせなければならない。例外: 初期化後の既定の状態（SWS_ComM_00313 参照）。

コメント: 対応するチャネルステートマシンに複数のユーザが関連している場合、ComM モジュールは、すべてのユーザへファンアウトを行う必要がある。

**[SWS_ComM_00191]** ComM チャネルステートマシンの内部機能は、ユーザから見えてはならない。ユーザは、ComM チャネルステートマシンの内部の仕組みや規則（たとえば「最も高いものが勝つ」戦略）についての情報を必要とせず、また得てはならない。

対応するモードで要求される通信機能の概要を、表 2 に示す。

**表 2: 対応するモードで許可される通信機能**

| 通信モード | メッセージの送信 | メッセージの受信 | NM | ウェイクアップ / 再開の能力 |
|---|---|---|---|---|
| COMM_FULL_COMMUNICATION | On | On | バス通信 | 該当なし |
| COMM_SILENT_COMMUNICATION | Off | On | バス通信 | ユーザ / 診断の要求 |
| COMM_NO_COMMUNICATION | Off | Off | バス通信 | ユーザ / 診断の要求 |

7.1.1〜7.1.3 節についての注: 各 ComM チャネルステートマシンは、接続されたバスステートマネージャを持つ 1 つのチャネル / ネットワークを扱う責任を持つ（「対応する」= ComM チャネルステートマシンが責任を持つチャネル / ネットワーク）。

7.1.1〜7.1.3 節についての注: ComM モジュールは、1 つまたは複数の ComM チャネルステートマシンを含む。ComM チャネルステートマシンは、接続されたバスステートマネージャと直接通信し、その他のインタフェースは ComM モジュールが扱う。

#### 7.2.1 状態 COMM_NO_COMMUNICATION での振る舞い

**[SWS_ComM_00898]** 状態 COMM_NO_COMMUNICATION に入るとき、ComM チャネルステートマシンは、サブ状態 COMM_NO_COM_NO_PENDING_REQUEST へ進まなければならない。

**[SWS_ComM_00313]** 初期化後に既定で状態 COMM_NO_COMMUNICATION に入るとき、ComM モジュールは、そのモード変化を RTE や BswM を介してユーザへ知らせてはならない。

SWS_ComM_00313 の根拠: この時点では RTE がまだ初期化されていない。

**[SWS_ComM_00073]** 状態 COMM_NO_COMMUNICATION に入るとき、ComM チャネルステートマシンは、送信と受信の機能を切らなければならない。これは、ComM チャネルステートマシンが、対応する通信モードをバスステートマネージャモジュールへ要求することで行わなければならない（XXSM_RequestComMode(network:=<チャネルステートマシンのネットワーク>, mode:= COMM_NO_COMMUNICATION、SWS_ComM_00829 参照）。

SWS_ComM_00073 の根拠: COMM_NO_COMMUNICATION モードは、対応するチャネルでの、バス通信 PDU の送受信を禁止する。

**[SWS_ComM_00288]** 状態 COMM_NO_COMMUNICATION に入り、かつコンフィギュレーションパラメータ ComMNmVariant=FULL（ECUC_ComM_00568 参照）の場合、ComM モジュールは、ネットワークマネージメントモジュールにネットワークの解放を要求しなければならない（Nm_NetworkRelease()）。

注: Nm_NetworkRelease は、ComM が Nm を要求している場合に必要である。

注: Nm_NetworkRelease は、ComM が（Nm_NetworkRequest または Nm_PassiveStartup で）そのチャネルについてすでに Nm を要求し、まだ解放していない場合に必要である。

SWS_ComM_00073、SWS_COMM_00288、SWS_ComM_00875、SWS_ComM_00876 の根拠: 部分ネットワークを避けるため、FlexRay のシャットダウンは中断できない。

コメント: 状態 COMM_NO_COMMUNICATION では、ComM チャネルステートマシンは、設定されたチャネルのバス通信を、バスステートマネージャモジュールへ要求しなくてもよい。

上記コメントのユースケース: ECU が、バス通信に参加せずに、ローカルで制御機能を実行している。

コメント: 通信モードは 1 つのチャネルについてローカルなものであるため、ECU は他のチャネルでは引き続き通信してよい。

##### 7.2.1.1 サブ状態 COMM_NO_COM_NO_PENDING_REQUEST

**[SWS_ComM_00875]** サブ状態 COMM_NO_COM_NO_PENDING_REQUEST で、ユーザが COMM_FULL_COMMUNICATION を要求し、通信の制限が無効な場合（7.3.1 節参照）、ComM チャネルステートマシンは、すぐにサブ状態 COMM_NO_COM_REQUEST_PENDING へ切り替えなければならない。

**[SWS_ComM_00876]** サブ状態 COMM_NO_COM_NO_PENDING_REQUEST で、コンフィギュレーションパラメータ ComMNmVariant=FULL|LIGHT|NONE（ECUC_ComM_00568）であり、かつ DCM が ComM_DCM_ActiveDiagnostic（SWS_ComM_00873）を通知した場合、ComM チャネルステートマシンは、すぐにサブ状態 COMM_NO_COM_REQUEST_PENDING へ切り替えなければならない。

SWS_ComM_00876 の根拠: 通信の制限が発生しうる場合（7.3.1 節参照）でも、アクティブな診断セッションの間は、一時的に無効にしなければならない（SWS_ComM_00182 参照）。

**[SWS_ComM_00893]** サブ状態 COMM_NO_COM_NO_PENDING_REQUEST で ComM_EcuM_WakeUpIndication が呼ばれ、かつコンフィギュレーションパラメータ ComMSynchronousWakeUp=FALSE（ECUC_ComM_00695）の場合、ComM モジュールは、要求された ComM チャネルステートマシン（すなわちチャネル）を、サブ状態 COMM_NO_COM_REQUEST_PENDING へ切り替えなければならない。(SRS_ModeMgm_09087)

**[SWS_ComM_00894]** サブ状態 COMM_NO_COM_NO_PENDING_REQUEST で、NM モジュールが再開 ComM_Nm_RestartIndication()（SWS_ComM_00792）を通知した場合、ComM チャネルステートマシンは、すぐにサブ状態 COMM_NO_COM_REQUEST_PENDING へ切り替えなければならない。(SRS_ModeMgm_09087)

SWS_ComM_00893 と SWS_ComM_00894 の根拠: バスのウェイクアップの後、できるだけ早く通信が始まることを保証しなければならない。

コメント: ComM チャネルステートマシンは、COMM_FULL_COMMUNICATION 状態に入った後、すぐにサブ状態 COMM_FULL_COM_NETWORK_REQUESTED へ切り替わる。どのユーザも COMM_FULL_COMMUNICATION モードを要求しない場合、AUTOSAR NM、または ComMTMinFullComModeDuration（ECUC_ComM_00557）の ComM モジュールのタイマが、ユーザの要求が起こりうる前のシステムの初期化 / 起動時間を乗り越えるため、COMM_NO_COMMUNICATION と COMM_FULL_COMMUNICATION の間の行き来を防ぐ。

**[SWS_ComM_00694]** サブ状態 COMM_NO_COM_NO_PENDING_REQUEST で ComM_EcuM_WakeUpIndication が呼ばれ、かつコンフィギュレーションパラメータ ComMSynchronousWakeUp=TRUE（ECUC_ComM_00695）の場合、ComM モジュールは、すべての ComM チャネルステートマシン（すなわちチャネル）を、サブ状態 COMM_NO_COM_REQUEST_PENDING へ切り替えなければならない。(SRS_ModeMgm_09248)

**[SWS_ComM_01014]** サブ状態 COMM_NO_COM_NO_PENDING_REQUEST で ComM_EcuM_PNCWakeUpIndication(<PNC>) が呼ばれ、かつコンフィギュレーションパラメータ ComMSynchronousWakeUp=FALSE（ECUC_ComM_00695）、ComMPncSupport=TRUE（ECUC_ComM_00839 参照）の場合、ComM モジュールは、その PNC が参照する ComM チャネルステートマシン（すなわちチャネル）を、サブ状態 COMM_NO_COM_REQUEST_PENDING へ切り替えなければならない。(SRS_ModeMgm_09248)

**[SWS_ComM_01015]** サブ状態 COMM_NO_COM_NO_PENDING_REQUEST で ComM_EcuM_PNCWakeUpIndication(<PNC>) が呼ばれ、かつコンフィギュレーションパラメータ ComMSynchronousWakeUp=TRUE（ECUC_ComM_00695）、ComMPncSupport=TRUE（ECUC_ComM_00839 参照）の場合、ComM モジュールは、すべての ComM チャネルステートマシン（すなわちチャネル）を、サブ状態 COMM_NO_COM_REQUEST_PENDING へ切り替えなければならない。(SRS_ModeMgm_09248)

##### 7.2.1.2 サブ状態 COMM_NO_COM_REQUEST_PENDING

**[SWS_ComM_00895]** サブ状態 COMM_NO_COM_REQUEST_PENDING では、ComM チャネルステートマシンは、SWS_ComM_00884 と SWS_ComM_00885 に従って保存・設定された、対応する CommunicationAllowed フラグを評価しなければならない。CommunicationAllowed=TRUE と評価された場合、ComM チャネルステートマシンは、すぐに状態 COMM_FULL_COMMUNICATION へ切り替えなければならない。

**[SWS_ComM_00897]** サブ状態 COMM_NO_COM_REQUEST_PENDING で、COMM_FULL_COMMUNICATION に対する有効な保留中の要求がもうなくなった場合、ComM チャネルステートマシンは、既定のサブ状態 COMM_NO_COM_NO_PENDING_REQUEST へ戻らなければならない。

SWS_ComM_00897 の根拠: 何らかの理由で通信が許可されなかった場合に、既定のサブ状態へ戻れるようにする。たとえば、ユーザの要求 ComM_RequestComMode(<user>, COMM_FULL_COMMUNICATION)（SWS_ComM_00871）、または DCM の通知 ComM_DCM_ActiveDiagnostic(<channel>)（SWS_ComM_00873）によって COMM_NO_COM_REQUEST_PENDING への遷移がトリガされたが、その後 ComM_RequestComMode(<user>, COMM_NO_COMMUNICATION)（SWS_ComM_00871）または DCM の ComM_DCM_InactiveDiagnostic(<channel>)（SWS_ComM_00874）で取り消された場合。

コメント: EcuM-Fixed は、スリープ / シャットダウンの前に、ComM_GetState()（SWS_ComM_00872）で ComM チャネルステートマシンのサブ状態を読み取って評価しなければならない。

#### 7.2.2 状態 COMM_SILENT_COMMUNICATION での振る舞い

**[SWS_ComM_00071]** 状態 COMM_SILENT_COMMUNICATION に入るとき、ComM チャネルステートマシンは、送信の機能を切らなければならない（受信の機能は入れたままにする）。これは、ComM チャネルステートマシンが、対応する通信モードをバスステートマネージャモジュールへ要求することで行わなければならない（XXSM_RequestComMode(network:=<チャネルステートマシンのネットワーク>, mode:= COMM_SILENT_COMMUNICATION)、SWS_ComM_00829）。

SWS_ComM_00071 の根拠: COMM_SILENT_COMMUNICATION モードは、バス通信 PDU の受信を許可し、バス通信 PDU の送信を禁止する。

コメント: 受信の機能が入っていても、何も受信されないことがありうる（たとえばバスオフ中）。

ユースケース: NM モジュールを使ったシャットダウンの調整（prepare bus sleep 状態）。

**[SWS_ComM_00877]** 状態 COMM_SILENT_COMMUNICATION で、ユーザが COMM_FULL_COMMUNICATION を要求し、通信の制限が無効な場合（7.3.1 節参照）、ComM チャネルステートマシンは、状態 COMM_FULL_COMMUNICATION へ切り替えなければならない。

**[SWS_ComM_00878]** 状態 COMM_SILENT_COMMUNICATION で、コンフィギュレーションパラメータ ComMNmVariant=FULL|LIGHT|NONE（ECUC_ComM_00568）であり、かつ DCM が ComM_DCM_ActiveDiagnostic（SWS_ComM_00873）を通知した場合、ComM チャネルステートマシンは、状態 COMM_FULL_COMMUNICATION へ切り替えなければならない。

SWS_ComM_00878 の根拠: 通信の制限が発生しうる場合（7.3.1 節参照）でも、アクティブな診断セッションの間は、一時的に無効にしなければならない（SWS_ComM_00182 参照）。

**[SWS_ComM_00295]** 状態 COMM_SILENT_COMMUNICATION で、ネットワークマネージャモジュールが ComM_Nm_BusSleepMode()（SWS_ComM_00392）を通知した場合、ComM チャネルステートマシンは、状態 COMM_NO_COMMUNICATION へ切り替えなければならない。

**[SWS_ComM_00296]** 状態 COMM_SILENT_COMMUNICATION で、ネットワークマネージャモジュールが ComM_Nm_NetworkMode()（SWS_ComM_00390 参照）を通知した場合、ComM チャネルステートマシンは、状態 COMM_FULL_COMMUNICATION、サブ状態 COMM_FULL_COM_READY_SLEEP へ切り替えなければならない。

#### 7.2.3 状態 COMM_FULL_COMMUNICATION での振る舞い

**[SWS_ComM_00899]** 状態 COMM_FULL_COMMUNICATION に入るとき、遷移で特定のサブ状態が指定されていない場合、ComM チャネルステートマシンは、サブ状態 COMM_FULL_COM_NETWORK_REQUESTED へ進まなければならない。

SWS_ComM_00899 の根拠: COMM_SILENT_COMMUNICATION から切り替えるとき、遷移で指定されていれば、ComM チャネルステートマシンは、サブ状態 COMM_FULL_COM_READY_SLEEP へ直接切り替えられる（SWS_ComM_00296 参照）。

**[SWS_ComM_00069]** 状態 COMM_FULL_COMMUNICATION に入るとき、ComM チャネルステートマシンは、送信と受信の機能を入れなければならない。これは、ComM チャネルステートマシンが、対応する通信モードをバスステートマネージャモジュールへ要求することで行わなければならない（XXSM_RequestComMode(network:=<チャネルステートマシンのネットワーク>, mode:= COMM_FULL_COMMUNICATION)、SWS_ComM_00829）。

SWS_ComM_00069 の根拠: COMM_FULL_COMMUNICATION モードは、対応するチャネルでの、バス通信 PDU の送受信を許可する。

**[SWS_ComM_00637]** 状態 COMM_FULL_COMMUNICATION で、ネットワークマネージャモジュールが ComM_Nm_BusSleepMode()（SWS_ComM_00392）を通知した場合、ComM チャネルステートマシンは、状態 COMM_NO_COMMUNICATION へ切り替えなければならない。

SWS_ComM_00637 の根拠: ユーザが、バスを起こしたままにすることを「遅すぎる」タイミングで要求することがある（クラスタがすでにシャットダウンに合意しているため、NM はバスを起こしたままにする投票を送れない）。

**[SWS_ComM_00826]** COMM_FULL_COMMUNICATION で、コンフィギュレーションパラメータ ComMNmVariant=FULL|PASSIVE（ECUC_ComM_00568）であり、かつネットワークマネージャモジュールが ComM_Nm_PrepareBusSleepMode()（SWS_ComM_00391）を通知した場合、ComM ステートマシンは、状態 COMM_SILENT_COMMUNICATION へ切り替えなければならない。

SWS_ComM_00826 の根拠: ComM_Nm_PrepareBusSleepMode() は、能動的な要求が Nm_NetworkRelease() で解放される前には受け取れず、PASSIVE のチャネルは能動的なウェイクアップで起こされることがないため、この遷移は常に有効であると仮定しても安全である。

##### 7.2.3.1 サブ状態 COMM_FULL_COM_NETWORK_REQUESTED

**[SWS_ComM_00886]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED に入り、かつコンフィギュレーションパラメータ ComMNmVariant=LIGHT|NONE（ECUC_ComM_00568）の場合、ComMTMinFullComModeDuration（ECUC_ComM_00557）のタイマを開始しなければならない。

**[SWS_ComM_00665]** COMM_NO_COM_REQUEST_PENDING からサブ状態 COMM_FULL_COM_NETWORK_REQUESTED に入り、かつ EcuM モジュールが ComM_EcuM_WakeUpIndication(<channel>)（SWS_ComM_00275）または ComM_EcuM_PNCWakeUpIndication(<PNC>)（SWS_ComM_91001）でウェイクアップを通知した場合、ComM モジュールは、ネットワークマネージメントへ Nm_PassiveStartup(<channel>) を要求しなければならない。

**[SWS_ComM_00902]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED に入り、かつ Nm モジュールが再開 ComM_Nm_RestartIndication(<channel>)（SWS_ComM_00792）を通知した場合、ComM モジュールは、ネットワークマネージメントへ Nm_PassiveStartup(<channel>) を要求しなければならない。

**[SWS_ComM_00903]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED に入り、かつ Nm モジュールがネットワーク開始 ComM_Nm_NetworkStartIndication(<channel>)（SWS_ComM_00383）を通知した場合、ComM モジュールは、ネットワークマネージメントへ Nm_PassiveStartup(<channel>) を要求しなければならない。

SWS_ComM_00903 へのコメント: これは COMM_FULL_COMMUNICATION への「通常の」遷移ではない。ComM は ComM_Nm_NetworkStartIndication() を「競合状態」のエラーとして扱う（7.6.1 節参照）。

**[SWS_ComM_00869]** 他の状態またはサブ状態からサブ状態 COMM_FULL_COM_NETWORK_REQUESTED に入るとき、コンフィギュレーションパラメータ ComMNmVariant=FULL（ECUC_ComM_00568）で、ユーザが ComM_RequestComMode(<user>, COMM_FULL_COMMUNICATION)（SWS_ComM_00110）を要求している場合、ComM モジュールは、対応する NM チャネルについて、ネットワークマネージメントへ Nm_NetworkRequest(<channel>) を要求しなければならない。(SRS_ModeMgm_00049)

注: そのほか、SWS_ComM_00980 により Nm_NetworkRequest が呼ばれることがある。

**[SWS_ComM_00870]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED に入るとき、コンフィギュレーションパラメータ ComMNmVariant=FULL（ECUC_ComM_00568）で、DCM が ComM_DCM_ActiveDiagnostic(<channel>)（SWS_ComM_00873）を通知している場合、ComM モジュールは、対応する NM チャネルについて、ネットワークマネージメントへ Nm_NetworkRequest(<channel>) を要求しなければならない。(SRS_ModeMgm_00049)

**[SWS_ComM_00889]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED で、コンフィギュレーションパラメータ ComMNmVariant=LIGHT|NONE（ECUC_ComM_00568）であり、ComMTMinFullComModeDuration（ECUC_ComM_00557）のタイマが満了し、ユーザ要求 ComM_RequestComMode(<user>, COMM_FULL_COMMUNICATION) がなく、かつ DCM が ComM_DCM_ActiveDiagnostic(<channel>)（SWS_ComM_00873）を通知していない場合、ComM チャネルステートマシンは、サブ状態 COMM_FULL_COM_READY_SLEEP へ切り替えなければならない。

SWS_ComM_00889 の根拠: ComMTMinFullComModeDuration のタイマが満了していない間は、行き来を防ぐため、サブ状態を維持しなければならない。

**[SWS_ComM_00888]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED で、コンフィギュレーションパラメータ ComMNmVariant=FULL（ECUC_ComM_00568）であり、ユーザ要求 ComM_RequestComMode(<user>, COMM_FULL_COMMUNICATION) がなく、かつ DCM が ComM_DCM_ActiveDiagnostic(<channel>)（SWS_ComM_00873）を通知していない場合、ComM チャネルステートマシンは、サブ状態 COMM_FULL_COM_READY_SLEEP へ切り替えなければならない。

SWS_ComM_00888 の根拠: AUTOSAR NM を使う場合、タイマは必要ない。AUTOSAR NM もこの機能を保証するので、機能の重複を避けられる。

**[SWS_ComM_00915]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED で、コンフィギュレーションパラメータ ComMNmVariant=PASSIVE（ECUC_ComM_00568）の場合、ComM チャネルステートマシンは、サブ状態 COMM_FULL_COM_READY_SLEEP へ切り替えなければならない。

**[SWS_ComM_00890]** サブ状態 COMM_FULL_COM_NETWORK_REQUESTED で、DCM が ComM_DCM_ActiveDiagnostic(<channel>)（SWS_ComM_00873）を通知しておらず、かつ通信の制限が要求されている場合（7.3.1 節参照）、ComM チャネルステートマシンは、すぐにサブ状態 COMM_FULL_COM_READY_SLEEP へ切り替え、ComMTMinFullComModeDuration のタイマを取り消さなければならない。

##### 7.2.3.2 サブ状態 COMM_FULL_COM_READY_SLEEP

**[SWS_ComM_00133]** サブ状態 COMM_FULL_COM_READY_SLEEP に入り、かつコンフィギュレーションパラメータ ComMNmVariant=FULL（ECUC_ComM_00568）の場合、ComM モジュールは、対応する NM チャネルについて、ネットワークマネージメントへ Nm_NetworkRelease() を要求しなければならない。

**[SWS_ComM_00891]** サブ状態 COMM_FULL_COM_READY_SLEEP に入り、かつコンフィギュレーションパラメータ ComMNmVariant=LIGHT（ECUC_ComM_00568）の場合、ComMNmLightTimeout（ECUC_ComM_00606）のタイマを開始しなければならない。

**[SWS_ComM_00610]** サブ状態 COMM_FULL_COM_READY_SLEEP で、コンフィギュレーションパラメータ ComMNmVariant=LIGHT（ECUC_ComM_00568）であり、ComMNmLightTimeout（ECUC_ComM_00606）のタイマが満了した場合、ComM チャネルステートマシンは、状態 COMM_NO_COMMUNICATION へ切り替えなければならない。

**[SWS_ComM_00671]** サブ状態 COMM_FULL_COM_READY_SLEEP で、コンフィギュレーションパラメータ ComMBusType=COMM_BUS_TYPE_INTERNAL（ECUC_ComM_00567）の場合、ComM チャネルステートマシンは、すぐに状態 COMM_NO_COMMUNICATION へ切り替えなければならない。

**[SWS_ComM_00882]** サブ状態 COMM_FULL_COM_READY_SLEEP で、ユーザが COMM_FULL_COMMUNICATION を要求し、通信の制限が無効な場合（7.3.1 節参照）、ComM チャネルステートマシンは、すぐにサブ状態 COMM_FULL_COM_NETWORK_REQUESTED へ切り替えなければならない。

**[SWS_ComM_00883]** サブ状態 COMM_FULL_COM_READY_SLEEP で、コンフィギュレーションパラメータ ComMNmVariant=FULL|LIGHT|NONE（ECUC_ComM_00568）であり、かつ DCM が ComM_DCM_ActiveDiagnostic（SWS_ComM_00873）を通知した場合、ComM チャネルステートマシンは、サブ状態 COMM_FULL_COM_NETWORK_REQUESTED へ切り替えなければならない。

SWS_ComM_00883 の根拠: 通信の制限が発生しうる場合（7.3.1 節参照）でも、アクティブな診断セッションの間は、一時的に無効にしなければならない（SWS_ComM_00182 参照）。

**[SWS_ComM_00892]** サブ状態 COMM_FULL_COM_READY_SLEEP で、コンフィギュレーションパラメータ ComMNmVariant=LIGHT（ECUC_ComM_00568）であり、SWS_ComM_00882 または SWS_ComM_00883 の要件に従った COMM_FULL_COMMUNICATION の要求によって、サブ状態 COMM_FULL_COM_NETWORK_REQUESTED へ切り替わる場合、ComMNmLightTimeout（ECUC_ComM_00606）のタイマを取り消さなければならない。

> 学習メモ: 本プロジェクトの経路に関係する箇所
> - 通信の開始は、`COMM_NO_COM_REQUEST_PENDING` で CommunicationAllowed=TRUE を評価して FULL へ進む（SWS_ComM_00895）。
> - 通信の停止は、NM の `ComM_Nm_PrepareBusSleepMode` で SILENT へ（SWS_ComM_00826）、`ComM_Nm_BusSleepMode` で NO_COMMUNICATION へ（SWS_ComM_00295）進む。
> - FULL でも `ComM_Nm_BusSleepMode` が来れば直接 NO_COMMUNICATION へ進む（SWS_ComM_00637）。

### 7.3 拡張機能

**[SWS_ComM_00470]** 本章で説明する拡張機能は、機能ごとに、実行時に個別にコンフィギュレーションできなければならない（たとえば、ウェイクアップの抑止を有効にし、NO_COMMUNICATION への制限を無効にする）。

SWS_ComM_00470 の根拠: 実行時に、変化する条件に対応するため、抑止 / 制限の方針の変更が必要である。

ユースケース: 診断でウェイクアップの抑止を変更する。

コメント: パラメータ ComMEcuGroupClassification でコンフィギュレーションできる（ECUC_ComM_00563 参照）。

#### 7.3.1 通信の抑止

注: 1. モード抑止の目的は、通信機能を制限することである。詳細は 7.3.1.1 節と 7.3.1.2 節を参照。

2. 次のパラメータは通信の抑止に関係し、下で説明する API と関係がある。

- **ComMNoCom**: モード抑止の「要求ビット」（NoCom への制限）。ComMEcuGroupClassification がこの機能を有効にしている場合にのみ、ComM_LimitChannelToNoComMode() と ComM_LimitECUToNoComMode() で制御できる（ECUC_ComM_00563、SWS_ComM_00163、SWS_ComM_00124 参照）。
- **ComMNoWakeup**: モード抑止の「要求ビット」（ウェイクアップの抑止）。ComMEcuGroupClassification がこの機能を有効にしている場合にのみ、ComM_PreventWakeUp() で制御できる（ECUC_ComM_00563、SWS_ComM_00156 参照）。
- **ComMEcuGroupClassification**: モード抑止の振る舞いの「マスクビット」。ComMNoCom と ComMNoWakeup の値にかかわらず、ComM_SetECUGroupClassification() で制御できる。

**[SWS_ComM_00301]** ComM モジュールは、対応するモード抑止を要求および解放するインタフェースを提供しなければならない。

コメント: ComM モジュールは、誰がモード抑止を要求するかを気にしないが、それは「通常の」SW-C ではない。特権的な SW-C、または OEM 固有の BSW である。

**[SWS_ComM_00488]** チャネル（チャネルステートマシン）ごとに独立して、モード抑止を有効および無効にできなければならない。この機能は、ComM モジュール自身が使ってはならない。

**[SWS_ComM_00839]** ComM モジュールは、ユーザ要求の状態を保存しなければならない。

コメント: SWS_ComM_00839 は、モード制限が有効な間の望ましい振る舞いを説明している。

**[SWS_ComM_00840]** モード抑止が有効な間にユーザが要求を解放した場合、ComM モジュールは、ユーザ要求の更新された状態を保存しなければならない。

SWS_ComM_00840 の根拠: 抑止が無効になったとき、ユーザの要求が認められなければならない。

コメント: 異なるユーザからの有効なユーザ要求の数。SWS_ComM_00840 は、モード制限が有効な間の望ましい振る舞いを説明している。

**[SWS_ComM_00182]** 通信の抑止は、アクティブな診断セッションの間は、一時的に無効にならなければならない。

SWS_ComM_00182 の根拠: アクティブな診断セッションの間、ECU はスリープしてはならない。

コメント: DCM は、アクティブな診断セッションの開始を ComM_DCM_ActiveDiagnostic(<channel>)（SWS_ComM_00873）で、診断セッションの終了を ComM_DCM_InactiveDiagnostic(<channel>)（SWS_ComM_00874）で知らせる。

##### 7.3.1.1 バスウェイクアップの抑止

情報: ComM モジュールの文脈でのバスウェイクアップの抑止とは、ComM モジュールが、通信を開始することで他の ECU を起こしてしまうことに備えて、予防措置を取るべきだという意味である。

根拠: 通信によって他の ECU を起こすことは避けるべきである。ECU がバスを起こすのは、エラー（たとえばセンサの故障）が原因であると想定されるためである。

ユースケース: アクティブなウェイクアップラインの信号経路でエラーが検出され、この信頼できないウェイクアップ源が、もはやシステム全体を起こせないようにすべき場合。エラー時の反応を制御する SW-C は、このイベントの結果として通常は SW-C から通信要求を受け取る、関連する通信チャネルのウェイクアップ抑止状態を設定できる。これにより、信頼できないウェイクアップイベントに基づく、システム全体での通信の転送が妨げられる。または、アプリケーション固有のシステム制御の場合、たとえばトランスポートモードのような条件で、通信によるシステム全体のウェイクアップの転送を切るべき SW-C がある。

**[SWS_ComM_00302]** バスウェイクアップの抑止は、ユーザ要求を無視することで行わなければならない。(SRS_ModeMgm_09089)

コメント: ユーザ要求を無視するとは、要求を受け付けるが、モード抑止のために実行しないという意味である。「最も高いものが勝つ」戦略は、モード抑止が切られたらすぐに適用される（SWS_ComM_00839 と SWS_ComM_00840 参照）。

**[SWS_ComM_00218]** ユーザからの通信要求（COMM_FULL_COMMUNICATION）は、対応するチャネルについて ComM の抑止状態が ComMNoWakeup=TRUE（ECUC_ComM_00569）に等しく、かつチャネルの現在の状態が COMM_NO_COMMUNICATION または COMM_SILENT_COMMUNICATION の場合に、抑止されなければならない。

SWS_ComM_00218 の根拠: 抑止状態が設定されていても、通信チャネルがすでにアクティブな場合は、抑止が有効になるべきではない。

**[SWS_ComM_00219]** 現在の通信状態が COMM_FULL_COMMUNICATION の場合、抑止は有効になってはならない。

SWS_ComM_00219 の根拠: 現在の通信状態が COMM_FULL_COMMUNICATION の場合、バスはすでに起きている。

**[SWS_ComM_00066]** ComM モジュールは、「passive wake-up」の機能を決して抑止してはならない。

SWS_ComM_00066 の根拠: EcuM モジュールから通知されるバスのウェイクアップには、常に反応できなければならない。

コメント: COMM_NO_COMMUNICATION モードでは受信は切られているが、ウェイクアップの機能は入っている。

**[SWS_ComM_00157]** ComMNoWakeup の状態は、不揮発に保存されなければならない。

SWS_ComM_00157 の根拠: 通信が有効になる前（「Full Communication」モードに入る前）の起動中に、この情報が利用できなければならない。変更や問い合わせは、通信が有効になった起動後にのみ可能である（通常、抑止が有効かどうかを決める「マスタ」は同じ ECU 上にない）。

**[SWS_ComM_00625]** ユーザが要求を解放した場合にも、ユーザ要求の状態を更新しなければならない。

##### 7.3.1.2 COMM_NO_COMMUNICATION モードへの制限

**[SWS_ComM_00303]** ComM モジュールは、COMM_FULL_COMMUNICATION モードに対するユーザ要求があっても、シャットダウンを開始するため COMM_FULL_COM_READY_SLEEP 状態へ切り替え、新しい COMM_FULL_COMMUNICATION モードの要求を無視することで、COMM_NO_COMMUNICATION モードへの制限を行わなければならない。(SRS_ModeMgm_09071)

SWS_ComM_00303 の根拠: バスを起こし続けているソフトウェアコンポーネントを停止させるには、COMM_NO_COMMUNICATION モードへの強制が必要である。

**[SWS_ComM_00355]** ComM は、設定されている場合（ComMResetAfterForcingNoComm=TRUE）、「No Communication」モードに入った後に BswM_ComM_InitiateReset() を呼んで、ECU のリセットを強制しなければならない。

根拠: 不具合のあるユーザは、再初期化なしには「Full Communication」の要求を解放しないと想定される。「Full Communication」の要求を有効なままにすると、ネットワークのシャットダウンと起動が交互に繰り返される。

ユースケース: 不具合のある ECU がバスを起こし続けると想定される。その結果、「ネットワークマスタ」が、すべての ECU をスリープさせるよう強制すると決める。

**[SWS_ComM_00841]** ComM モジュールは、現在の状態が COMM_FULL_COM_NETWORK_REQUESTED の場合にのみ、COMM_NO_COMMUNICATION モードへの制限を行わなければならない。

注: SWS_COMM_00841 は、ステートマシンの遷移だけを指す。つまり、COMM_NO_COMMUNICATION への制限による抑止状態の更新のような他の動作は、現在の状態にかかわらず、常に行わなければならない。

**[SWS_ComM_00842]** ComM モジュールは、COMM_FULL_COM_NETWORK_REQUESTED 以外の状態での、COMM_NO_COMMUNICATION への制限の要求を無視しなければならない。

注: SWS_COMM_00841 と SWS_COMM_00842 は、ローカルの ComM ユーザが、特定の ComM チャネルに FULL_COM を（能動的に）要求している場合の振る舞いを説明している。つまり、COMM_NO_COMMUNICATION への制限は、チャネルが能動的に要求された場合にのみ行われなければならない。ComM チャネルが、passive なウェイクアップによってリモートから起きたままにされている場合は、NoCom への制限を行ってはならない。

**[SWS_ComM_00215]** チャネル X の COMM_NO_COMMUNICATION 状態に入ることを保証するため、対応するチャネルについて ComM の抑止 ComMNoCom=TRUE（ECUC_ComM_00571 参照）の場合、通信チャネル X に対するすべての能動的なユーザ要求を無視しなければならない。

**[SWS_ComM_00582]** ComM モジュールは、対応するユーザに属するすべてのチャネルが COMM_NO_COMMUNICATION モードに入った後、ユーザ要求をクリアしなければならない。

SWS_ComM_00582 の根拠: バスを起こし続けていると想定される、保存された（不具合のある）ユーザ要求は、クリアしなければならない。

説明: ComM モジュールは、初期化中に、ComM の抑止状態の既定値を ComMNoCom（ECUC_ComM_00571 参照）から再読み込みしなければならない。

コメント: 各チャネルの現在の ComMNoCom の状態は、永続的に保存してはならない。SWS_ComM_00582 は、モード制限を実行した後の望ましい振る舞いを説明している。

### 7.4 バス通信の管理

**[SWS_ComM_00402]** ComM モジュールは、通信機能を制御するため、バスステートマネージャモジュールの対応するインタフェースを使わなければならない。

**[SWS_ComM_00664]** コンフィギュレーションパラメータ ComMBusType=COMM_BUS_TYPE_INTERNAL（ECUC_ComM_00567）の場合、ComM モジュールは、通信機能を制御する呼び出しを省略しなければならない。(SRS_ModeMgm_09168)

SWS_ComM_00664 の根拠: 内部通信には、対応するバスインタフェースがない。

### 7.5 ネットワークマネージメントの依存関係

**[SWS_ComM_00599]** ComM モジュールは、表 3 に示すシャットダウン同期のバリアント（ComMNmVariant で設定。ECUC_ComM_00568 参照）LIGHT、PASSIVE、FULL をサポートしなければならない。

コメント: 同期したシャットダウンをネットワークのすべてのノード間で保証できるのは、バリアント FULL と PASSIVE だけである。NmIf は、調整されたすべてのネットワークがスリープに入る準備ができる前に、調整されたネットワークの同期したシャットダウンを開始できないため、そのような調整されたバスでネットワーク通信を解放する ComM から NmIf への要求は、考慮されるが、常に直接実行されるとは限らない。NmIf は引き続き E_OK で応答するが、調整されたすべてのネットワークがスリープに入る準備ができるまで、ネットワークは解放されない。

**表 3: Communication Manager モジュールがサポートするネットワークマネージメントのバリアント**

| NM バリアント | バスを起こし続ける能力 | シャットダウンの同期 |
|---|---|---|
| NONE | （なし） | ComM によるシャットダウンの同期はない。ECU の電源を切ることでシャットダウンする。 |
| LIGHT | （なし） | ComM が、タイムアウト（ComMNmLightTimeout、ECUC_ComM_00606 で設定）を使ってシャットダウンを同期する。 |
| PASSIVE | ECU はバスを起こし続けることが許されない。 | ComM が、AUTOSAR NM を使ってシャットダウンを同期する。 |
| FULL | ECU はバスを起こし続けることが許される。 | ComM が、AUTOSAR NM を使ってシャットダウンを同期する。 |

コメント: LIGHT バリアントでは同期したシャットダウンはできない。そのため、後からシャットダウンするノードのメッセージによって、ECU が再起動を続ける（「行き来する」）ことがありうる。

**[SWS_ComM_00602]** コンフィギュレーションパラメータ ComMNmVariant=LIGHT|NONE の場合（ECUC_ComM_00568 参照）、ComM モジュールは NM サービスの呼び出しを省略しなければならない。

SWS_ComM_00602 の根拠: NM が利用できない場合、NM サービスも利用できない。

**[SWS_ComM_00667]** コンフィギュレーションパラメータ ComMNmVariant=PASSIVE の場合（ECUC_ComM_00568 参照）、ComM モジュールは、NM の Nm_NetworkRequest() の呼び出しを省略しなければならない。

SWS_ComM_00667 の根拠: サービス Nm_NetworkRequest() は利用できない。

### 7.6 バスエラーの管理

#### 7.6.1 ネットワーク開始通知（Network Start Indication）

**[SWS_ComM_00583]** NM が ComM_Nm_NetworkStartIndication(<channel X>) を通知し、かつ CommunicationAllowed フラグが TRUE に設定されている場合、ComM モジュールは、チャネル X を COMM_FULL_COMMUNICATION へ切り替えなければならない。

SWS_ComM_00583 のユースケース: あるノードが「Prepare Bus Sleep」状態で NM メッセージを送るが、「競合状態」のため、他のノードはすでに「Bus Sleep」状態になっている場合。

### 7.7 テスト支援要件

#### 7.7.1 抑止された Full Communication 要求のカウンタ

**[SWS_ComM_00138]** ComM モジュールは、拒否されたすべての COMM_FULL_COMMUNICATION モード要求のために、抑止カウンタを 1 つ提供しなければならない。このカウンタは、システムが通信モードを抑止しているために満たせないユーザ要求を数える。(SRS_ModeMgm_09155)

SWS_ComM_00138 の根拠: このカウンタは、動機のない通信バスのウェイクアップに関連する、潜在的なソフトウェアの問題を検出するために使われる。

**[SWS_ComM_00140]** 拒否されたすべての COMM_FULL_COMMUNICATION モード要求の抑止カウンタ（SWS_ComM_00138）は、不揮発メモリに保存されなければならない。

**[SWS_ComM_00141]** 拒否されたすべての COMM_FULL_COMMUNICATION モード要求の抑止カウンタ（SWS_ComM_00138）の範囲は、0〜65535 でなければならない。

**[SWS_ComM_00142]** 拒否されたすべての COMM_FULL_COMMUNICATION モード要求の抑止カウンタ（SWS_ComM_00138）は、最大のカウンタ値に達したら、増加を止めなければならない。

**[SWS_ComM_00143]** 拒否されたすべての COMM_FULL_COMMUNICATION モード要求の抑止カウンタ（SWS_ComM_00138）の値は、ComM モジュールの API 呼び出しで読み出し、リセットできなければならない。

SWS_ComM_00143 のユースケース: カウンタの現在の状態を、診断サービスで読み出し、リセットできなければならない。

### 7.8 エラー分類

#### 7.8.1 開発エラー

**[SWS_ComM_00234]** ComM モジュールは、エラーを報告するため、表 4 のエラーコードを使わなければならない。

**表 4: エラー分類**（SRS_BSW_00323、SRS_BSW_00327、SRS_BSW_00337、SRS_BSW_00385、SRS_BSW_00386）

| エラーの種類 | 関連 | エラーコード | 値 |
|---|---|---|---|
| モジュールの初期化前に API サービスを使用した | 開発 | COMM_E_UNINIT | 0x1 |
| 不正なパラメータで API サービスを使用した | 開発 | COMM_E_WRONG_PARAMETERS | 0x2 |
| ヌルポインタで API サービスを使用した | 開発 | COMM_E_PARAM_POINTER | 0x3 |
| 初期化に失敗した | 開発 | COMM_E_INIT_FAILED | 0x4 |

**[SWS_ComM_00612]** ComM が初期化されていない場合、ComM_Init()（SWS_ComM_00146 参照）、ComM_GetVersionInfo()（SWS_COMM_00370 参照）、ComM_GetStatus()（SWS_COMM_00242 参照）以外のすべての ComM モジュールと API サービスは、次のようにしなければならない。

- 通常の動作を実行しない。
- 標準の戻り値型を持つ場合は E_NOT_OK を返す。

(SRS_BSW_00406)

**[SWS_ComM_00858]** ComMDevErrorDetect（ECUC_ComM_00555 参照）で開発エラーの検出が有効な場合、関数は、サービス ComM_Init が事前に呼ばれたかを確認しなければならない。確認に失敗した場合、関数は開発エラー COMM_E_UNINIT を発生させ、そうでなければ（DET が無効な場合は）E_NOT_OK を返さなければならない。(SRS_BSW_00406)

#### 7.8.2 ランタイムエラー

ランタイムエラーはない。

#### 7.8.3 一過性の故障

一過性の故障はない。

### 7.9 非機能要件

**[SWS_ComM_00459]** ソースコードまたはオブジェクトコードとして提供される ComM モジュールを、AUTOSAR スタックへ統合できなければならない。

根拠:
- IP の保護と、保証されたテストカバレッジを可能にする: オブジェクトコード
- システム生成時に（インテグレータによる）高い効率とコンフィギュレーションの柔軟性を可能にする: ソースコード

(SRS_BSW_00342)

### 7.10 Communication Manager モジュールのサービス

本節は、Communication Manager モジュールサービス（ComM）の AUTOSAR インタフェースを定義する。

#### 7.10.1 アーキテクチャ

Communication Manager モジュールサービスの全体のアーキテクチャを、図 5 に示す。

図 5: Communication Manager モジュールの ARPackage（原本の図を参照）。SW-C 側は ComM_RequestMode、ComM_CurrentMode を require し、Vehicle Mode Manager（BswM）側は ComM_ChannelWakeup、ComM_ChannelLimitation、ComM_ECUModeLimitation を require する。ComM 側が、それぞれに対応する provide ポート（UR000、UM000、CW000、CL000、modeLimitation）を持つ。

#### 7.10.2 ユースケース

##### 7.10.2.1 SW-C が ComM モジュールをまったく気にしない場合

Communication Manager モジュールを気にしない SW-C は、Communication Manager モジュールの ARPackage で定義されたインタフェースを、まったく require しない。

##### 7.10.2.2 SW-C が通信システムの状態だけを気にする場合

このユースケースでは、SW-C が、自分がどのような通信機能を持つか（通信モード「none」、「silent」、「full」で表される。ComM_ModeType 参照）を知りたい。SW-C は、インタフェース ComM_GetCurrentComMode を require するポートを定義することで、それを知る。利用できる通信機能に応じて、SW-C の特定の runnable を実行するかどうかを指定できる。Communication Manager モジュールは、この SW-C に、現在の通信モードについてのこの情報を提供するポートを持つように、正しくコンフィギュレーションされていなければならない（たとえば、この SW-C が論理通信に使う物理チャネルなど）。

図 6: SW-C が Communication Manager モジュールへ状態変化を要求する（原本の図を参照）。

##### 7.10.2.3 SW-C が自分の通信状態に明示的に影響を与えたい場合

図 7: SW-C が Communication Manager モジュールの状態変化を要求し、現在の通信状態を読み出す（原本の図を参照）。

このユースケースでは、SW-C が、自分が必要とする物理チャネルの通信状態に、明示的に影響を与えたい。SW-C は、特定のポートでこれを示す。SW-C は、このポートを通じて、Communication Manager モジュールのモード「No Communication」または「Full Communication」を要求できる。Communication Manager モジュールは、これらの呼び出しを使って、対応するバスステートマネージャモジュールへ、対応する通信モードを要求する。

**[SWS_ComM_00848]** Communication Manager モジュールは、`ComM_RequestComMode` を呼び出して通信モードを要求できるようにする、AUTOSAR ポートを提供しなければならない（SWS_ComM_00110 参照）。

RTE の「direct API」を使う SW-C では、たとえば次のようにできる。

```c
MySW-C_Runnable_Init(self)
{
        // SW-C がデータを送受信したい
        e = Rte_Call_comRequest_RequestComMode(COMM_FULL_COMMUNICATION);

        if (e == RTE_E_OK)
        {
                 // Com Manager モジュールを full communication モードへ
                 // 移すことを、正常に要求できた
        }
        else
        {
                 // Com Manager モジュールとのやり取りでエラーが発生した
                 if (e == E_MODE_LIMITATION)
                 {
                         // 現在の ComMMode の制限が、そのモードへ移ることを
                         // 禁止している。許される最大の ComMMode を尋ねてみる
                         Rte_Call_comRequest_GetMaxComMode(&max);
                         if (max==COMM_NO_COMMUNICATION)
                         {
                                  ...
                         };
                 }
                 else
                 {
                         // もっと深刻なエラーが発生した ...
                 };
        };
        ...
};

MySW-C_Runnable_Loop(self)
{
        if (status == ready_to_sleep)
        {
                 // 送信する必要はない。通信のシャットダウンの準備ができた
                 Rte_Call_comRequest_RequestComMode(COMM_NO_COMMUNICATION);
                 ...
        };
};
```

コメント: これらの API は、SW-C が、自分が必要とするチャネルについての知識を持つことを要求しない点に注意。

##### 7.10.2.4 SW-C が物理チャネルと直接やり取りし、ECU モードの制限を有効にしたい場合

SW-C は、BswM にモードを要求しなければならない。BswM が、ComM との直接の通信を扱う。

図 8: BswM と ComM モジュールの間のやり取り（原本の図を参照）。

#### 7.10.3 ポートとポートインタフェースの仕様

本節は、RTE を介して Communication Manager モジュールの機能を動作させるために必要なポートインタフェースを規定する。

##### 7.10.3.1 インタフェースで使われる型

8.7.4 節「Implementation Data Types」を参照。

##### 7.10.3.2 ユーザ要求のためのポートとポートインタフェース

###### 7.10.3.2.1 一般的な考え方

ECU のローカルな Communication Manager モジュールを、特定の状態へ明示的に向けたい SW-C は、クライアント・サーバインタフェース ComM_UserRequest を require する。このインタフェースを通じて、SW-C は、そのコンポーネントに関連するすべての通信チャネルの望ましい状態を、「No Communication」または「Full Communication」に設定できる。SW-C のコードを、ユーザを Communication Manager モジュールに対して識別するために使われるハンドルの値から独立に保つため、これらのハンドルは SW-C から Communication Manager モジュールへ渡されない。そうではなく、Communication Manager モジュール側の Provide ポートの「ポートで定義された引数値」としてモデル化される。その結果、これらのハンドルは、クライアント・サーバインタフェース ComM_UserRequest の操作の引数としては現れない。さらにこの方法の結果として、Communication Manager モジュールは、ユーザごとに別々のポートを持つ。

###### 7.10.3.2.2 データ型

このインタフェースに必要なデータ型はない。

###### 7.10.3.2.3 ポートインタフェース ComM_UserRequest

8.7.2.4 節「ComM_UserRequest」を参照。

##### 7.10.3.3 Communication Manager モジュールの現在のモードのためのポートとポートインタフェース

###### 7.10.3.3.1 一般的な考え方

**[SWS_ComM_00847]** Communication Manager モジュールは、ModeSwitchInterface インタフェース `ComM_CurrentMode` を提供する AUTOSAR ポートを持たなければならない。

**[SWS_ComM_00733]** Communication Manager モジュールは、SW-C が接続される設定済みのユーザごとに、ModeSwitchInterface インタフェース `ComM_CurrentMode` を提供する別々のポートを持たなければならない。

自分の Communication Manager モジュールの現在のモードの通知を受けたい SW-C は、ModeSwitchInterface インタフェース ComM_CurrentMode を require する。

###### 7.10.3.3.2 ポートインタフェース ComM_CurrentMode

8.7.3.1 節「ComM_CurrentMode」を参照。

##### 7.10.3.4 現在 FULL_COMM を要求している ComM ユーザのためのポートとポートインタフェース

###### 7.10.3.4.1 一般的な考え方

**[SWS_ComM_00734]** Communication Manager モジュールは、設定済みの ComM チャネルごとに、センダ・レシーバインタフェース `ComM_CurrentChannelRequest` を提供する、オプションの（ECUC_ComM_00787 参照）別々のポートを持たなければならない。

SWS_ComM_00734 の根拠: 現在 FULL_COM を要求しているユーザの通知を受けたい SW-C は、センダ・レシーバインタフェース `ComM_CurrentChannelRequest` を require する。

**[SWS_ComM_00736]** あるチャネルについて、現在 FULL_COMM を要求している ComM ユーザの集合が変化するたびに、Communication Manager モジュールは、データ要素 fullComRequestors を更新しなければならない。変化は、Communication Manager モジュールが ComM ユーザの通信要求を受け付けた場合にのみ、データ要素を更新しなければならない。

SWS_ComM_00736 の根拠: アクティブな ModeLimitation のために拒否された要求は、データ要素の更新にはつながらない。

###### 7.10.3.4.2 データ型

8.7.4.4 節「ComM_UserHandleArrayType」を参照。

###### 7.10.3.4.3 ポートインタフェース ComM_CurrentChannelRequest

8.7.1.1 節「ComM_CurrentChannelRequest」を参照。

##### 7.10.3.5 ECU モード制限のためのポートとポートインタフェース

###### 7.10.3.5.1 一般的な考え方

**[SWS_ComM_00740]** Communication Manager モジュールは、クライアント・サーバインタフェース ComM_ECUModeLimitation を提供する AUTOSAR ポートを持つようにコンフィギュレーションできる。

「Mode Manager」の役割を担う SW-C は、このインタフェースを使って、ECU 全体の振る舞いを変えられる。

###### 7.10.3.5.2 ポートインタフェース ComM_ECUModeLimitation

8.7.2.3 節「ComM_ECUModeLimitation」を参照。

##### 7.10.3.6 チャネルのウェイクアップのためのポートとポートインタフェース

###### 7.10.3.6.1 一般的な考え方

**[SWS_ComM_00747]** Communication Manager モジュールは、クライアント・サーバインタフェース ComM_ChannelWakeup を提供する AUTOSAR ポートを持つようにコンフィギュレーションできる。

「Mode Manager」の役割を担う SW-C は、このインタフェースを使って、通信を開始することで他の ECU を起こしてしまうことに備えた予防措置を取るよう、Communication Manager モジュールをコンフィギュレーションできる。SW-C のコードを、Communication Manager モジュールに対して特定のハンドルを識別するために使われるハンドルの値から独立に保つため、これらのハンドルは SW-C から Communication Manager モジュールへ渡されない。そうではなく、Communication Manager モジュール側の Provide ポートの「ポートで定義された引数値」としてモデル化される。その結果、これらのハンドルは、クライアント・サーバインタフェース ComM_ChannelWakeup の操作の引数としては現れない。さらにこの方法の結果として、Communication Manager モジュールは、チャネルごとに別々のポートを持つ。

###### 7.10.3.6.2 ポートインタフェース ComM_ChannelWakeup

8.7.2.2 節「ComM_ChannelWakeup」を参照。

##### 7.10.3.7 チャネル制限インタフェースのためのポートとポートインタフェース

###### 7.10.3.7.1 一般的な考え方

**[SWS_ComM_00752]** Communication Manager モジュールは、クライアント・サーバインタフェース ComM_ChannelLimitation を提供する AUTOSAR ポートを持つようにコンフィギュレーションできる。

「Mode Manager」の役割を担う SW-C は、このインタフェースを使って、指定したチャネルの通信モードを抑止するよう、Communication Manager モジュールをコンフィギュレーションできる。SW-C のコードを、Communication Manager モジュールに対して特定のハンドルを識別するために使われるハンドルの値から独立に保つため、これらのハンドルは SW-C から Communication Manager モジュールへ渡されない。そうではなく、Communication Manager モジュール側の Provide ポートの「ポートで定義された引数値」としてモデル化される。その結果、これらのハンドルは、クライアント・サーバインタフェース ComM_ChannelLimitation の操作の引数としては現れない。さらにこの方法の結果として、Communication Manager モジュールは、チャネルごとに別々のポートを持つ。

###### 7.10.3.7.2 ポートインタフェース ComM_ChannelLimitation

8.7.2.1 節「ComM_ChannelLimitation」を参照。

##### 7.10.3.8 Communication Manager モジュールのサービスの定義

本節は、Communication Manager モジュールサービスの定義についての指針を示す。RTE の両側にポートがある。この Communication Manager モジュールサービスの記述は、RTE の下側のポートを定義する。このサービスを使う各 SW-C は、RTE を生成できるように、COM Manager モジュールのポートへ接続される「サービスポート」を、自身の SW-C 記述の中に含めなければならない。

コメント: これらの定義は、ECU のコンフィギュレーション中にしか完成できない点に注意（Communication Manager モジュールサービスが提供するポートの数を決める、Communication Manager モジュールの特定のコンフィギュレーションパラメータに依存するため）。また、SW-C の実装は、これらの定義に依存しない点にも注意。

**[SWS_ComM_00744]**

```
/* これは、サービスとしての Communication Manager モジュールの定義である。
   これは Communication Manager モジュールの「外から見た姿」である */
Service ComM
{
        // ComMModeLimitationEnabled（ECUC_ComM_00560 参照）の場合に存在するポート
        ProvidePort ComM_ECUModeLimitation modeLimitation;

        // ComMModeLimitationEnabled（ECUC_ComM_00560 参照）の場合、
        // チャネルごとに存在するポート。NC 個のチャネルがある。
        ProvidePort ComM_ChannelLimitation CL000;
        ...
        ProvidePort ComM_ChannelLimitation CL<NC-1>;

        // COMM_WAKEUP_INHIBITION_ENABLED（ECUC_ComM_00559 参照）の場合、
        // チャネルごとに存在するポート
        ProvidePort ComM_ChannelWakeup CW000;
        ...
        ProvidePort ComM_ChannelWakeup CW<NC-1>;

        // ユーザごとに、Communication Manager モジュールは 2 つのポートを提供する。
        // コンフィギュレーションを容易にするため、このユーザのインデックスは、
        // Communication Manager モジュールのコンフィギュレーションに使われる
        // 配列 COMM_USER_LIST のインデックスに対応しなければならない。
        // ユーザの数は、この配列のサイズに対応しなければならない。
        ProvidePort ComM_UserRequest UR000; // （7.10.3.2.2 参照）
        ProvidePort ComM_CurrentMode UM000;
        ProvidePort ComM_UserRequest UR001; // （7.10.3.2.2 参照）
        ProvidePort ComM_CurrentMode UM001;
        ...
        ProvidePort ComM_UserRequest UR<COMM_USER_LIST.size-1>;
        ProvidePort ComM_CurrentMode UM<COMM_USER_LIST.size-1>;

        // 設定されている場合、チャネルごとに存在するポート
        // （ECUC_ComM_00787 参照）。NC 個のチャネルがある。
        ProvidePort ComM_CurrentChannelRequest CR000;
        ...
        ProvidePort ComM_CurrentChannelRequest CR<NC-1>;
};
```

#### 7.10.4 Runnable とエントリポイント

##### 7.10.4.1 内部の振る舞い

これは Communication Manager モジュールの内側の記述である。この詳細な記述は、ローカルの RTE のコンフィギュレーションにのみ必要である。

**[SWS_ComM_00745]**

```
InternalBehavior of the Communication Manager Module
{
        // Communication Manager モジュールの Runnable Entity
        RunnableEntity LimitECUToNoComMode     symbol "ComM_LimitECUToNoComMode"      /* SWS_ComM_00124 */  canbeInvokedConcurrently = FALSE
        RunnableEntity ReadInhibitCounter      symbol "ComM_ReadInhibitCounter"       /* SWS_ComM_00224 */  canbeInvokedConcurrently = FALSE
        RunnableEntity ResetInhibitCounter     symbol "ComM_ResetInhibitCounter"      /* SWS_ComM_00108 */  canbeInvokedConcurrently = FALSE
        RunnableEntity SetECUGroupClassification symbol "ComM_SetECUGroupClassification" /* SWS_ComM_00552 */ canbeInvokedConcurrently = FALSE
        RunnableEntity LimitChannelToNoComMode symbol "ComM_LimitChannelToNoComMode"  /* SWS_ComM_00163 */  canbeInvokedConcurrently = FALSE
        RunnableEntity GetInhibitionStatus     symbol "ComM_GetInhibitionStatus"      /* SWS_ComM_00619 */  canbeInvokedConcurrently = FALSE
        RunnableEntity PreventWakeup           symbol "ComM_PreventWakeup"            canbeInvokedConcurrently = FALSE
        RunnableEntity RequestComMode          symbol "ComM_RequestComMode"           /* SWS_ComM_00110 */  canbeInvokedConcurrently = TRUE
        RunnableEntity GetMaxComMode           symbol "ComM_GetMaxComMode"            /* SWS_ComM_00085 */  canbeInvokedConcurrently = TRUE
        RunnableEntity GetRequestedComMode     symbol "ComM_GetRequestedComMode"      canbeInvokedConcurrently = TRUE
        RunnableEntity GetCurrentComMode       symbol "ComM_GetCurrentComMode"        /* SWS_ComM_00083 */  canbeInvokedConcurrently = TRUE

        // 次は ComMModeLimitationEnabled（ECUC_ComM_00560 参照）の場合に適用される
        modeLimitation.LimitECUToNoComMode       -> LimitECUToNoComMode
        modeLimitation.ReadInhibitCounter        -> ReadInhibitCounter
        modeLimitation.ResetInhibitCounter       -> ResetInhibitCounter
        modeLimitation.SetECUGroupClassification -> SetECUGroupClassification

        // チャネルごとの振る舞いは、ComMModeLimitationEnabled
        // （ECUC_ComM_00560 参照）の場合にのみ存在する。NC 個のチャネルがある。
        // コンフィギュレーションを容易にするため、チャネルの名前は、
        // Communication Manager モジュールのコンフィギュレーションに使われる
        // "Channel" コンテナ内のチャネルのインデックスに対応する。
        CL000.LimitChannelToNoComMode -> LimitChannelToNoComMode
        CL000.GetInhibitionStatus     -> GetInhibitionStatus
        PortArgument {port=CL000, value.type=NetworkHandleType, value.value=Channel[0].COMM_CHANNEL_ID}
        ...（CLnnn も同様）

        // チャネルごとの振る舞いは、COMM_WAKEUP_INHIBITION_ENABLED
        // （ECUC_ComM_00559 参照）の場合にのみ存在する。
        CW000.preventWakeUp -> PreventWakeUp
        PortArgument {port=CW000, value.type=NetworkHandleType, value.value=Channel[0].COMM_CHANNEL_ID}
        ...（CWnnn も同様）

        // ユーザごとの振る舞い
        // ポート引数の値は、コンフィギュレーション COMM_USER_LIST の値と
        // 整合していなければならない点に注意。
        // UserHandleType の正確なデータ型は、RTE のコンフィギュレーションの
        // 前に定義しなければならないが、このサービスを使う SW-C から見える
        // API には影響しない点に注意。
        UR000.RequestComMode      -> RequestComMode
        UR000.GetMaxComMode       -> GetMaxComMode
        UR000.GetRequestedComMode -> GetRequestedComMode
        UR000.GetCurrentComMode   -> GetCurrentComMode
        PortArgument {port=UR000, value.type=ComM_UserhandleType, value.value=COMM_USER_LIST[0]}
        ...（URnnn も同様）
};
```

コメント: 'modeLimitation.LimitECUToNoComMode -> LimitECUToNoComMode' は、OperationPrototype を、実行されるべき Runnable Entity に結びつける OperationInvokedEvent を定義するものである。

##### 7.10.4.2 Communication Manager モジュールがインクルードするヘッダファイル

RTE は、Communication Manager モジュールを、通常の SW-C と同様に扱う。RTE は、Communication Manager モジュールの内部の振る舞いの記述に基づき、たとえば、Communication Manager モジュールが利用できる API（Rte_Ports_CurrentMode_P のようなもの）の定義を含むヘッダファイルを生成できる。これは、Communication Manager モジュールの実装が、この生成されたヘッダファイルをインクルードしなければならないことを意味する。

## 8 API 仕様

### 8.1 インポートする型

#### 8.1.1 標準型

本章では、次のファイルからインクルードされるすべての型を列挙する。

**[SWS_ComM_00820]**

| モジュール | インポートする型 |
|---|---|
| ComStack_Types | NetworkHandleType, PNCHandleType |
| EcuM | EcuM_StateType |
| NvM | NvM_BlockIdType, NvM_RequestResultType |
| Std_Types | Std_ReturnType, Std_VersionInfoType |

(SRS_BSW_00348, SRS_BSW_00357)

ComM の API は、Std_ReturnType の次の拡張を使う。

**[SWS_ComM_00649]**

- 値の範囲: COMM_E_MODE_LIMITATION = 2。関数呼び出しは成功したが、モードを（抑止などのために）認められなかった。（原本の説明は途中で切れている）

(SRS_BSW_00331, SRS_BSW_00369, SRS_BSW_00377, SRS_BSW_00441)

### 8.2 型定義

**[SWS_ComM_00863]** この仕様書で定義された関数には、次のデータ型を使わなければならない。(SRS_BSW_00441)

#### 8.2.1 ComM_InitStatusType

**[SWS_ComM_00668]**

- 名前: ComM_InitStatusType
- 種類: 列挙型
- 値:
  - COMM_UNINIT = 0x00: COM Manager は初期化されていない、または使用できない。
  - COMM_INIT = 0x01: COM Manager は初期化され、使用できる。
- 説明: ComM の初期化状態。

#### 8.2.2 ComM_PncModeType

**[SWS_ComM_00673]**

- 名前: ComM_PncModeType
- 種類: 列挙型
- 値:
  - COMM_PNC_REQUESTED = 0x00: PNC がローカルの ComM ユーザに要求されている。
  - COMM_PNC_READY_SLEEP = 0x01: PNC がリモートの ComM ユーザに要求されている。
  - COMM_PNC_PREPARE_SLEEP = 0x02: PNC はアクティブで、デッドライン監視はない。
  - COMM_PNC_NO_COMMUNICATION = 0x03: PNC は通信しない。
- 説明: PNC の現在のモード。

#### 8.2.3 ComM_StateType

**[SWS_ComM_00674]**

- 名前: ComM_StateType
- 種類: uint8
- 値:
  - COMM_NO_COM_NO_PENDING_REQUEST = 0
  - COMM_NO_COM_REQUEST_PENDING = 1
  - COMM_FULL_COM_NETWORK_REQUESTED = 2
  - COMM_FULL_COM_READY_SLEEP = 3
  - COMM_SILENT_COM = 4
- 説明: ComM ステートマシンの状態とサブ状態。ComM の状態と通信モードの対応は次のとおり。
  - COMM_NO_COM*: 通信モード = 'No Communication'
  - COMM_FULL_COM*: 通信モード = 'Full Communication'
  - COMM_SILENT_COM: 通信モード = 'Silent Communication'

#### 8.2.4 ComM_ConfigType

**[SWS_ComM_00162]**

- 名前: ComM_ConfigType
- 種類: 構造体
- 値の範囲: 実装固有。初期化データ構造の内容は実装に固有である。
- 説明: この型は、実装固有のポストビルドコンフィギュレーションの構造を含む。

### 8.3 関数定義

これは、上位層のモジュールに提供される関数の一覧である。

注: 本章のすべての関数は、次のものを除き、事前の初期化（ComM_Init）を必要とする。
- ComM_Init
- ComM_GetVersionInfo

#### 8.3.1 ComM_Init

**[SWS_ComM_00146]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_Init |
| 構文 | `void ComM_Init(const ComM_ConfigType* ConfigPtr)` |
| サービス ID | 0x01 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | ConfigPtr: ポストビルドコンフィギュレーションデータへのポインタ |
| 戻り値 | なし |
| 説明 | AUTOSAR Communication Manager を初期化し、内部のステートマシンを再起動する。 |

(SRS_BSW_00101, SRS_BSW_00358, SRS_BSW_00414)

**[SWS_ComM_00793]** ComM_Init() の注意事項: ComM モジュールのパラメータへ「直接」アクセスできるようにするため、NVRAM Manager モジュールが初期化されていなければならない。

**[SWS_ComM_00864]** ComM_Init() の中で、ComM は、SWS_ComM_00103 で規定された不揮発パラメータを NVRAM から読み出さなければならない。パラメータが利用できない場合、ComM は、ComM のコンフィギュレーションの既定値を使わなければならない。

#### 8.3.2 ComM_DeInit

**[SWS_ComM_00147]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_DeInit |
| 構文 | `void ComM_DeInit(void)` |
| サービス ID | 0x02 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 戻り値 | なし |
| 説明 | この API は、AUTOSAR Communication Manager を初期化解除する。 |

(SRS_BSW_00336)

**[SWS_ComM_00794]** ComM_DeInit() での初期化解除は、ComM モジュールが制御するすべてのチャネルが COMM_NO_COMMUNICATION モードにある場合にのみ行わなければならない。

SWS_ComM_00794 の根拠: ComM_DeInit() API はエラーメッセージを返せないため、ComM_DeInit() が呼ばれる前に、すべてのチャネルが COMM_NO_COMMUNICATION モードかつサブ状態 COMM_NO_COM_NO_PENDING_REQUEST にあることを保証しなければならない。たとえば、状態を ComM_GetState(Channel, ...) で確認すべきであり、ComM_CommunicationAllowed(Channel, TRUE) は、ComM_DeInit() が呼ばれる前に呼べない。

**[SWS_ComM_00865]** ComM_DeInit の中で、ComM は、SWS_ComM_00103 で規定された不揮発パラメータを NVRAM へ保存しなければならない。

#### 8.3.3 ComM_GetState

**[SWS_ComM_00872]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_GetState |
| 構文 | `Std_ReturnType ComM_GetState(NetworkHandleType Channel, ComM_StateType* State)` |
| サービス ID | 0x34 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | Channel: ComM ステートマシンの状態を要求する対象のネットワークチャネル |
| 出力パラメータ | State: ComM チャネルステートマシンの状態。COMM_NO_COM_NO_PENDING_REQUEST、COMM_NO_COM_REQUEST_PENDING、COMM_FULL_COM_NETWORK_REQUESTED、COMM_FULL_COM_READY_SLEEP、COMM_SILENT_COM のいずれか |
| 戻り値 | E_OK: ComM ステートマシンの現在の状態を正常に返した<br>E_NOT_OK: ComM ステートマシンの現在の状態を返せなかった |
| 説明 | ComM チャネルステートマシンの、サブ状態を含む現在の状態を返す。<br>この関数の使用は、EcuM-Fixed を使う場合にのみ有効である。<br>RUN を離れるには、状態 / サブ状態が COMM_NO_COM_NO_PENDING_REQUEST（通信がなく、通信を開始する保留中の要求もない）でなければならない。<br>POST RUN から RUN へ戻るには、状態 / サブ状態が COMM_NO_COM_REQUEST_PENDING（通信はないが、通信を開始する保留中の要求がある）でなければならない。<br>EcuM-Flex と BswM を使う場合、BswM は代わりに、ComM から受け取ったモード通知（BswM_ComM_RequestedMode(..)）を使う。 |

#### 8.3.4 ComM_GetStatus

**[SWS_ComM_00242]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_GetStatus |
| 構文 | `Std_ReturnType ComM_GetStatus(ComM_InitStatusType* Status)` |
| サービス ID | 0x03 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 出力パラメータ | Status: COMM_UNINIT = ComM は初期化されていない、または使用できない（起動後、または ComM_DeInit() が呼ばれた後の既定値）。COMM_INIT = ComM は初期化され、使用できる。 |
| 戻り値 | E_OK: 初期化状態を正常に返した |
| 説明 | AUTOSAR Communication Manager の初期化状態を返す。ComM_DeInit() の呼び出しの後、ComM は状態 COMM_UNINIT でなければならず、内部のステートマシンを既定値へ再起動するには、ComM_Init の新たな呼び出しが必要である。 |

(SRS_BSW_00406)

#### 8.3.5 ComM_GetInhibitionStatus

**[SWS_ComM_00619]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_GetInhibitionStatus |
| 構文 | `Std_ReturnType ComM_GetInhibitionStatus(NetworkHandleType Channel, ComM_InhibitionStatusType* Status)` |
| サービス ID | 0x04 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | Channel: NetworkHandleType を参照 |
| 出力パラメータ | Status: ComM_InhibitionStatusType を参照 |
| 戻り値 | E_OK: 抑止状態を正常に返した<br>E_NOT_OK: 抑止状態を返せなかった |
| 説明 | ComM チャネルの抑止状態を返す。 |

#### 8.3.6 ComM_RequestComMode

**[SWS_ComM_00110]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_RequestComMode |
| 構文 | `Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode)` |
| サービス ID | 0x05 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | User: モードを要求するユーザのハンドル<br>ComMode: COMM_FULL_COMMUNICATION または COMM_NO_COMMUNICATION |
| 戻り値 | E_OK: 新しいモードへ正常に変更した<br>E_NOT_OK: 新しいモードへの変更に失敗した<br>COMM_E_MODE_LIMITATION: モード抑止のため、モードを認められない |
| 説明 | ユーザによる通信モードの要求。<br>注: 内部的に、モード COMM_SILENT_COMMUNICATION は、ユーザによる有効な要求ではない（シャットダウン時の同期に使われるモードである）。有効なモードは COMM_NO_COMMUNICATION と COMM_FULL_COMMUNICATION である。通信要求は、ComM の通信抑止によって解放されることもある。 |

(SRS_ModeMgm_09081)

**[SWS_ComM_00795]** ComM_RequestComMode のコンフィギュレーション: ユーザとチャネルの関係。ユーザは、1 つ以上のチャネルに静的にマップされる。

#### 8.3.7 ComM_GetMaxComMode

**[SWS_ComM_00085]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_GetMaxComMode |
| 構文 | `Std_ReturnType ComM_GetMaxComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)` |
| サービス ID | 0x06 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | User: モードを要求するユーザのハンドル |
| 出力パラメータ | ComMode: ComM_ModeType を参照 |
| 戻り値 | E_OK: 許可される最大の通信モードを正常に返した<br>E_NOT_OK: 許可される最大の通信モードを返せなかった |
| 説明 | 対応するユーザについて、許可される最大の通信モードを問い合わせる関数。 |

ユースケース: この関数は、最大の可能なモードを問い合わせる手段を提供する（たとえば、ユーザが、「Full Communication」モードにできるか、制限 / 抑止が有効かを確認したい）。これは診断 / デバッグのために必要である。

**[SWS_ComM_00374]** 1 つのユーザ要求に複数のチャネルがリンクされ、チャネルごとの許可される最大のモードが異なる場合、関数 ComM_GetMaxComMode は、最も低いモードを返さなければならない（SWS_ComM_00867 と SWS_ComM_00868 参照）。

**[SWS_ComM_00796]** ComM_GetMaxComMode のコンフィギュレーション: ユーザとチャネルの関係。ユーザは、1 つ以上のチャネルに静的にマップされる。

#### 8.3.8 ComM_GetRequestedComMode

**[SWS_ComM_00079]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_GetRequestedComMode |
| 構文 | `Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)` |
| サービス ID | 0x07 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | User: モードを要求するユーザのハンドル |
| 出力パラメータ | ComMode: 要求されたモードの名前 |
| 戻り値 | E_OK: 要求された通信モードを正常に返した |
| 説明 | 対応するユーザについて、現在要求されている通信モードを問い合わせる関数。 |

(SRS_ModeMgm_09149)

SWS_ComM_00079 の根拠: ユーザが要求した「通信モード」は、ユーザによる状態情報の冗長な保存を避けるため、Communication Manager モジュール自身の中に揮発性で保存されなければならない。

コメント: Communication Manager モジュールがこのサービスを持たない場合、すべてのユーザが自分で状態を保存しなければならない → 同じデータの冗長で、場合によっては不整合な保存になる。

注: ユーザは、1 つ以上のチャネルに静的にマップされる。ユーザとチャネルの関係は、コンフィギュレーションに反映される（ECUC_ComM_00658 参照）。

#### 8.3.9 ComM_GetCurrentComMode

**[SWS_ComM_00083]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_GetCurrentComMode |
| 構文 | `Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)` |
| サービス ID | 0x08 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | User: モードを要求するユーザのハンドル |
| 出力パラメータ | ComMode: ComM_ModeType を参照 |
| 戻り値 | E_OK: バスステートマネージャから通信モードを正常に返した<br>E_NOT_OK: バスステートマネージャからの通信モードを返せなかった |
| 説明 | 現在の通信モードを問い合わせる関数。ComM は、ネットワークの現在の通信モードを得るため、バスステートマネージャの対応するインタフェースを使わなければならない（バスステートマネージャ API の呼び出し: XXXSM_GetCurrentComMode(...)）。 |

(SRS_ModeMgm_09084)

**[SWS_ComM_00176]** 1 つのユーザ要求に複数のチャネルがリンクされ、チャネルのモードが異なる場合、関数 ComM_GetCurrentComMode は、最も低いモードを返さなければならない（SWS_ComM_00867 と SWS_ComM_00868 参照）。(SRS_ModeMgm_09172)

**[SWS_ComM_00798]** ComM_GetCurrentComMode のコンフィギュレーション: ユーザとチャネルの関係。ユーザは、1 つ以上のチャネルに静的にマップされる。

#### 8.3.10 ComM_PreventWakeUp

**[SWS_ComM_00156]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_PreventWakeUp |
| 構文 | `Std_ReturnType ComM_PreventWakeUp(NetworkHandleType Channel, boolean Status)` |
| サービス ID | 0x09 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | Channel: NetworkHandleType を参照<br>Status: FALSE = ウェイクアップの抑止を切る。TRUE = ウェイクアップの抑止を入れる。 |
| 戻り値 | E_OK: チャネルのウェイクアップ状態を正常に変更した<br>E_NOT_OK: チャネルのウェイクアップ状態の変更に失敗した（たとえば ComMEcuGroupClassification がこの機能を無効にしている。ECUC_ComM_00563 参照） |
| 説明 | 対応するチャネルの抑止状態 COMM_NO_WAKEUP を変更する。 |

(SRS_ModeMgm_09157)

**[SWS_ComM_00799]** ComM_PreventWakeUp のコンフィギュレーション: COMM_WAKEUP_INHIBITION_ENABLED でコンフィギュレーションできる（ECUC_ComM_00559 参照）。

#### 8.3.11 ComM_LimitChannelToNoComMode

**[SWS_ComM_00163]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_LimitChannelToNoComMode |
| 構文 | `Std_ReturnType ComM_LimitChannelToNoComMode(NetworkHandleType Channel, boolean Status)` |
| サービス ID | 0x0b |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | Channel: NetworkHandleType を参照<br>Status: FALSE = チャネルの COMM_NO_COMMUNICATION への制限を無効にする。TRUE = 有効にする。 |
| 戻り値 | E_OK: チャネルの抑止状態を正常に変更した<br>E_NOT_OK: チャネルの抑止状態の変更に失敗した（たとえば ComMEcuGroupClassification がこの機能を無効にしている。ECUC_ComM_00563 参照） |
| 説明 | COMM_NO_COMMUNICATION からより高い通信モードへ変えるための、チャネルの抑止状態を変更する。（ComM_LimitECUToNoComMode も参照。機能は同じで、対象がすべてのチャネルである。） |

(SRS_ModeMgm_09157)

**[SWS_ComM_00800]** ComM_LimitChannelToNoComMode のコンフィギュレーション: ComMModeLimitationEnabled（ECUC_ComM_00560 参照）と COMM_RESET_AFTER_FORCING_NO_COMM（ECUC_ComM_00558 参照）でコンフィギュレーションできる。

#### 8.3.12 ComM_LimitECUToNoComMode

**[SWS_ComM_00124]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_LimitECUToNoComMode |
| 構文 | `Std_ReturnType ComM_LimitECUToNoComMode(boolean Status)` |
| サービス ID | 0x0c |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | Status: FALSE = ECU の COMM_NO_COMMUNICATION への制限を無効にする。TRUE = 有効にする。 |
| 戻り値 | E_OK: ECU の抑止状態を正常に変更した<br>E_NOT_OK: ECU の抑止状態の変更に失敗した（たとえば ComMEcuGroupClassification がこの機能を無効にしている。ECUC_ComM_00563 参照） |
| 説明 | COMM_NO_COMMUNICATION からより高い通信モードへ変えるための、ECU（= すべてのチャネル）の抑止状態を変更する。（ComM_LimitChannelToNoComMode も参照。機能は同じで、対象が特定のチャネルである。） |

(SRS_ModeMgm_09157)

**[SWS_ComM_00801]** ComM_LimitECUToNoComMode のコンフィギュレーション: ComMModeLimitationEnabled（ECUC_ComM_00560 参照）と COMM_RESET_AFTER_FORCING_NO_COMM（ECUC_ComM_00558 参照）でコンフィギュレーションできる。

#### 8.3.13 ComM_ReadInhibitCounter

**[SWS_ComM_00224]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_ReadInhibitCounter |
| 構文 | `Std_ReturnType ComM_ReadInhibitCounter(uint16* CounterValue)` |
| サービス ID | 0x0d |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 出力パラメータ | CounterValue: 拒否された COMM_FULL_COMMUNICATION ユーザ要求の数 |
| 戻り値 | E_OK: 抑止カウンタを正常に返した<br>E_NOT_OK: 抑止カウンタを返せなかった |
| 説明 | この関数は、拒否された COMM_FULL_COMMUNICATION ユーザ要求の数を返す。 |

(SRS_ModeMgm_09156)

**[SWS_ComM_00802]** ComM_ReadInhibitCounter のコンフィギュレーション: ComMModeLimitationEnabled（ECUC_ComM_00560 参照）でコンフィギュレーションできる。この関数は、ComMModeLimitationEnabled（ECUC_ComM_00560 参照）が有効で、ComMGlobalNvMBlockDescriptor が設定されている場合にのみ利用できる。

#### 8.3.14 ComM_ResetInhibitCounter

**[SWS_ComM_00108]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_ResetInhibitCounter |
| 構文 | `Std_ReturnType ComM_ResetInhibitCounter(void)` |
| サービス ID | 0x0e |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 戻り値 | E_OK: 抑止された COMM_FULL_COMMUNICATION カウンタを正常にリセットした<br>E_NOT_OK: 抑止された COMM_FULL_COMMUNICATION カウンタのリセットに失敗した |
| 説明 | この関数は、抑止された COMM_FULL_COMMUNICATION 要求のカウンタをリセットする。 |

(SRS_ModeMgm_09156)

**[SWS_ComM_00803]** ComM_ResetInhibitCounter のコンフィギュレーション: ComMModeLimitationEnabled（ECUC_ComM_00560 参照）でコンフィギュレーションできる。この関数は、ComMModeLimitationEnabled（ECUC_ComM_00560 参照）が有効で、ComMGlobalNvMBlockDescriptor が設定されている場合にのみ利用できる。

#### 8.3.15 ComM_SetECUGroupClassification

**[SWS_ComM_00552]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_SetECUGroupClassification |
| 構文 | `Std_ReturnType ComM_SetECUGroupClassification(ComM_InhibitionStatusType Status)` |
| サービス ID | 0x0f |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | Status: ComM_InhibitionStatusType を参照 |
| 戻り値 | E_OK: ECU グループ分類の状態を正常に変更した<br>E_NOT_OK: ECU グループ分類の状態の変更に失敗した |
| 説明 | ECU グループ分類の状態を変更する（10.2.2 節参照）。 |

#### 8.3.16 ComM_GetVersionInfo

**[SWS_ComM_00370]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_GetVersionInfo |
| 構文 | `void ComM_GetVersionInfo(Std_VersionInfoType* Versioninfo)` |
| サービス ID | 0x10 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 出力パラメータ | Versioninfo: Std_VersionInfoType を参照 |
| 戻り値 | なし |
| 説明 | この関数は、公開された情報を返す（詳細は表 10.3 を参照）。 |

(SRS_BSW_00407)

### 8.4 コールバック通知

これは、他のモジュールに提供される関数の一覧である。コールバック関数のプロトタイプは、ヘッダファイル ComM_Nm.h、ComM_EcuMBswM.h、ComM_Dcm.h、ComM_BusSM.h、ComM_Com.h で提供されなければならない。

**[SWS_ComM_00620]** 提供されるすべての通知関数は、プリコンパイル時に実装されなければならない。

注: 本章のすべての関数は、ComM モジュールが正しく初期化されていることを必要とする。

#### 8.4.1 AUTOSAR ネットワークマネージメントインタフェース

##### 8.4.1.1 ComM_Nm_NetworkStartIndication

**[SWS_ComM_00383]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_Nm_NetworkStartIndication |
| 構文 | `void ComM_Nm_NetworkStartIndication(NetworkHandleType Channel)` |
| サービス ID | 0x15 |
| 同期 / 非同期 | 非同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: NetworkHandleType を参照 |
| 戻り値 | なし |
| 説明 | バススリープモードで NM メッセージを受信したことの通知。これは、ネットワーク内の一部のノードが、すでにネットワークモードに入っていることを示す。 |

##### 8.4.1.2 ComM_Nm_NetworkMode

**[SWS_ComM_00390]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_Nm_NetworkMode |
| 構文 | `void ComM_Nm_NetworkMode(NetworkHandleType Channel)` |
| サービス ID | 0x18 |
| 同期 / 非同期 | 非同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: チャネル |
| 戻り値 | なし |
| 説明 | ネットワークマネージメントがネットワークモードに入ったことの通知。 |

##### 8.4.1.3 ComM_Nm_PrepareBusSleepMode

**[SWS_ComM_00391]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_Nm_PrepareBusSleepMode |
| 構文 | `void ComM_Nm_PrepareBusSleepMode(NetworkHandleType Channel)` |
| サービス ID | 0x19 |
| 同期 / 非同期 | 非同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: チャネル |
| 戻り値 | なし |
| 説明 | ネットワークマネージメントが Prepare Bus-Sleep モードに入ったことの通知。リエントラント: 可（ただし同じ NM チャネルに対しては不可）。 |

##### 8.4.1.4 ComM_Nm_BusSleepMode

**[SWS_ComM_00392]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_Nm_BusSleepMode |
| 構文 | `void ComM_Nm_BusSleepMode(NetworkHandleType Channel)` |
| サービス ID | 0x1a |
| 同期 / 非同期 | 非同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: チャネル |
| 戻り値 | なし |
| 説明 | ネットワークマネージメントが Bus-Sleep モードに入ったことの通知。このコールバック関数は、ハードウェアとトランシーバを bus-sleep モードへ遷移させるべきである。 |

##### 8.4.1.5 ComM_Nm_RestartIndication

**[SWS_ComM_00792]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_Nm_RestartIndication |
| 構文 | `void ComM_Nm_RestartIndication(NetworkHandleType Channel)` |
| サービス ID | 0x1b |
| 同期 / 非同期 | 非同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: チャネル |
| 戻り値 | なし |
| 説明 | NmIf が、調整されたバスのシャットダウンを開始し、かつ、調整されたすべてのバスがバススリープ状態を通知しておらず、かつ、調整されたバスの少なくとも 1 つで NM が再開された場合、NM インタフェースは、すでにバススリープ状態を通知したチャネルの nmNetworkHandle を指定して、コールバック関数 ComM_Nm_RestartIndication を呼ばなければならない。 |

#### 8.4.2 AUTOSAR Diagnostic Communication Manager インタフェース

##### 8.4.2.1 ComM_DCM_ActiveDiagnostic

**[SWS_ComM_00873]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_DCM_ActiveDiagnostic |
| 構文 | `void ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel)` |
| サービス ID | 0x1f |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: 診断通信に必要なチャネル |
| 戻り値 | なし |
| 説明 | DCM によるアクティブな診断の通知。 |

##### 8.4.2.2 ComM_DCM_InactiveDiagnostic

**[SWS_ComM_00874]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_DCM_InactiveDiagnostic |
| 構文 | `void ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel)` |
| サービス ID | 0x20 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: 診断通信にもう必要ないチャネル |
| 戻り値 | なし |
| 説明 | DCM による非アクティブな診断の通知。 |

#### 8.4.3 AUTOSAR ECU State Manager インタフェース

##### 8.4.3.1 ComM_EcuM_WakeUpIndication

**[SWS_ComM_00275]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_EcuM_WakeUpIndication |
| 構文 | `void ComM_EcuM_WakeUpIndication(NetworkHandleType Channel)` |
| サービス ID | 0x2a |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: チャネル |
| 戻り値 | なし |
| 説明 | 対応するチャネルでのウェイクアップの通知。 |

##### 8.4.3.2 ComM_EcuM_PNCWakeUpIndication

**[SWS_ComM_91001]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_EcuM_PNCWakeUpIndication |
| 構文 | `void ComM_EcuM_PNCWakeUpIndication(PNCHandleType PNCid)` |
| サービス ID | 0x37 |
| 同期 / 非同期 | 同期 |
| リエントラント | 可 |
| 入力パラメータ | PNCid: Partial Network Cluster の識別子 |
| 戻り値 | なし |
| 説明 | 対応する Partial Network Cluster でのウェイクアップの通知。 |

#### 8.4.4 AUTOSAR ECU State Manager と Basic Software Mode Manager のインタフェース

##### 8.4.4.1 ComM_CommunicationAllowed

**[SWS_ComM_00871]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_CommunicationAllowed |
| 構文 | `void ComM_CommunicationAllowed(NetworkHandleType Channel, boolean Allowed)` |
| サービス ID | 0x35 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 入力パラメータ | Channel: チャネル<br>Allowed: TRUE = 通信が許可される。FALSE = 通信が許可されない。 |
| 戻り値 | なし |
| 説明 | EcuM または BswM が、通信が許可されるとき、ComM に知らせなければならない。EcuM-Fixed を使う場合は EcuM-Fixed、EcuM-Flex を使う場合は BswM が知らせる。 |

> 注: 原本のこの表は PDF からの抽出で行がずれていた。項目の対応は、内容から再構成した。

#### 8.4.5 Bus State Manager インタフェース

##### 8.4.5.1 ComM_BusSM_ModeIndication

**[SWS_ComM_00675]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_BusSM_ModeIndication |
| 構文 | `void ComM_BusSM_ModeIndication(NetworkHandleType Channel, ComM_ModeType ComMode)` |
| サービス ID | 0x33 |
| 同期 / 非同期 | 非同期 |
| リエントラント | 可 |
| 入力パラメータ | Channel: NetworkHandleType を参照<br>ComMode: ComM_ModeType を参照 |
| 戻り値 | なし |
| 説明 | 対応するバスステートマネージャによる、実際のバスモードの通知。ComM は、通知された状態を RTE と BswM を使ってユーザへ伝えなければならない。 |

#### 8.4.6 COM インタフェース

**[SWS_ComM_00819]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_COMCbk_<sn> |
| 構文 | `void ComM_COMCbk_<sn>(void)` |
| サービス ID | 0x36 |
| 同期 / 非同期 | 同期 |
| リエントラント | 不可 |
| 戻り値 | なし |
| 説明 | このコールバックは、COM 内で EIRA または ERA が更新されたときに呼ばれる。この呼び出しは、ERA と EIRA の変化を ComM に知らせるだけである。実際の処理は、ComM_MainFunction_<Channel_Id> の次の呼び出しで、対応する PN ステートマシンを変化させることで行われる。 |

### 8.5 スケジュールされる関数

これらの関数は、Basic Software Scheduler から直接呼ばれる。次の関数は、戻り値もパラメータも持たないものでなければならない。すべての関数は、リエントラントであってはならない。

#### 8.5.1 ComM_MainFunction

**[SWS_ComM_00429]**

| 項目 | 内容 |
|---|---|
| サービス名 | ComM_MainFunction_<Channel_Id> |
| 構文 | `void ComM_MainFunction_<Channel_Id>(void)` |
| サービス ID | 0x60 |
| 説明 | この関数は、たとえば RTE からの呼び出しなどによって直接起動されない、AUTOSAR ComM の処理を実行しなければならない。ComM のチャネルごとに、専用のメイン関数が 1 つなければならない。<br>前提条件: ComM が初期化されていなければならない。 |

(SRS_BSW_00373)

**[SWS_ComM_00818]** ComM_MainFunction_<Channel_Id> のコンフィギュレーション: 10.2.2 節を参照。

### 8.6 期待されるインタフェース

本章は、他のモジュールに要求されるすべてのインタフェースを示す。要求されるインタフェースの概要は、図 1 に示されている。

#### 8.6.1 必須インタフェース

本章は、このモジュールのコア機能を満たすために必要なすべてのインタフェースを定義する。

**[SWS_ComM_00828]**

| API 関数 | モジュール | 説明 |
|---|---|---|
| Nm_PassiveStartUp | Nm | この関数は、<BusNm>_PassiveStartUp 関数を呼ぶ（たとえば、チャネルが CAN として設定されていれば CanNm_PassiveStartUp 関数が呼ばれる）。 |
| Nm_NetworkRequest | Nm | この関数は、<BusNm>_NetworkRequest を呼ぶ（たとえば、チャネルが CAN として設定されていれば CanNm_NetworkRequest 関数が呼ばれる）。 |
| Nm_NetworkRelease | Nm | この関数は、<BusNm>_NetworkRelease 関数（たとえば CanNm_NetworkRelease 関数）を呼ぶ。 |
| Dcm_ComM_NoComModeEntered | Dcm | この呼び出しは、COMM_NO_COMMUNICATION への ComM のモード変化を Dcm モジュールに知らせる。 |
| Dcm_ComM_SilentComModeEntered | Dcm | この呼び出しは、COMM_SILENT_COMMUNICATION への ComM のモード変化を Dcm モジュールに知らせる。 |
| Dcm_ComM_FullComModeEntered | Dcm | この呼び出しは、COMM_FULL_COMMUNICATION への ComM のモード変化を Dcm モジュールに知らせる。 |
| Rte_Ports_UserMode_P()[n].Switch_currentMode(RTE_MODE_ComMMode_COMM_NO_COMMUNICATION) | Rte | COMM_NO_COMMUNICATION モードを RTE へ通知する。 |
| Rte_Ports_UserMode_P()[n].Switch_currentMode(RTE_MODE_ComMMode_COMM_SILENT_COMMUNICATION) | Rte | COMM_SILENT_COMMUNICATION を RTE へ通知する。 |
| Rte_Ports_UserMode_P()[n].Switch_currentMode(RTE_MODE_ComMMode_COMM_FULL_COMMUNICATION) | Rte | COMM_FULL_COMMUNICATION モードを RTE へ通知する。 |
| BswM_ComM_CurrentMode | BswM | 通信モードを BswM へ通知する。 |
| NvM_ReadBlock | NvM | NVRAM マネージャの、ブロック読み出しの API。 |
| NvM_WriteBlock | NvM | NVRAM マネージャの、ブロック書き込みの API。 |
| NvM_GetErrorStatus | NvM | NVRAM マネージャの、エラー状態取得の API。 |
| <BusSM>_GetCurrentComMode | <BusSM> | 実際の通信モードを問い合わせる関数。 |
| <BusSM>_RequestComMode | <BusSM> | （原本では説明が切れている。バスステートマネージャへ通信モードを要求する関数。） |

> 注: 原本の表は PDF からの抽出で列が崩れていた。API 名と説明の対応は、内容から再構成した。最後の行の説明は、原本の表で確認すること。

##### 8.6.1.1 AUTOSAR NVRAM Manager モジュール

**[SWS_ComM_00103]** ComM モジュールは、不揮発のコンフィギュレーションデータである ComMNoWakeup（ECUC_ComM_00569 参照）、ComMEcuGroupClassification（ECUC_ComM_00563 参照）、抑止状態（SWS_ComM_00157 参照）、抑止カウンタ（SWS_ComM_00140 参照）を保存および読み出すため、NVRAM Manager モジュールの対応する標準化されたサービスを使わなければならない（SWS_ComM_00828 参照）。

コメント: コンフィギュレーションデータをいつ読み出して保存するかは、SWS_ComM_00864 と SWS_ComM_00865 を参照。詳細は AUTOSAR NVRAM Manager モジュールの仕様書 [7] を参照。

##### 8.6.1.2 AUTOSAR Bus State Manager

**[SWS_ComM_00962]** ステートマネージャ API の接頭辞（"<BusSm>"）は、パラメータ ComMBusType が COMM_BUS_TYPE_CAN、COMM_BUS_TYPE_LIN、COMM_BUS_TYPE_FR、COMM_BUS_TYPE_ETH である場合に、それぞれ CanSM、LinSM、FrSM、EthSM でなければならない。

**[SWS_ComM_00957]** ComMBusType = "COMM_BUS_TYPE_CDD" の場合、API の接頭辞（"<BusSm>"）は、パラメータ "ComMCDDBusPrefix" で設定されなければならない。

**[SWS_ComM_00963]** Communication Manager モジュールは、必要な場合、現在の通信モードを問い合わせるため、ステートマネージャの <BusSm>_GetCurrentComMode() を使わなければならない。

**[SWS_ComM_00958]** Communication Manager モジュールは、特定の通信モードを要求するため、ステートマネージャの <BusSm>_RequestComMode() を使わなければならない。

特定の通信モードを要求する必要があるのがいつかは、チャネルステートマシンの各インスタンスの現在の状態に依存する（上記参照）。バスステートマネージャモジュールの機能の詳細は、それぞれの仕様書 [23]、[24]、[25]、[28] を参照。

コメント: これらの API は、異なるチャネルとコントローラの番号が使われる限り、リエントラントに呼べる。

> 学習メモ: CAN の場合の `<BusSm>_RequestComMode()` は `CanSM_RequestComMode()` である。その中身は [CanSM_ja.md](CanSM_ja.md) の 8.3.3 節と 7.2.6〜7.2.7 節を参照。

##### 8.6.1.3 AUTOSAR ネットワークマネージメントインタフェース

**[SWS_ComM_00261]** ComM モジュールは、ネットワークマネージメントのバスの起動とシャットダウンを同期するため、対応する関数を使わなければならない（SWS_ComM_00828 参照）。詳細は AUTOSAR NM インタフェースの仕様書 [9] を参照。

##### 8.6.1.4 AUTOSAR Diagnostic Communication Manager モジュール

**[SWS_ComM_00266]** ComM モジュールは、DCM モジュールの通信機能を制御するため、DCM が提供する対応する関数を使わなければならない（SWS_ComM_00828 参照）。

コメント: DCM は、送信と受信を開始 / 停止する関数を提供しない。DCM は、通知された Communication Manager モジュールの状態に従って通信を制御することを保証する。

詳細は AUTOSAR DCM の仕様書 [11] を参照。

##### 8.6.1.5 SW-C のために RTE が ComM に提供する AUTOSAR RTE インタフェース

**[SWS_ComM_00091]** ComM モジュールは、ユーザへモードを通知するため、RTE が提供する対応する関数を使わなければならない。ユーザごとに 1 つの通知がなければならない。複数のユーザに関連するモード通知のファンアウトは、Communication Manager モジュールが行わなければならない。(SRS_ModeMgm_09085)

**[SWS_ComM_00663]** 1 つのユーザ要求に複数のチャネルがリンクされ、チャネルのモードが異なる場合、ComM モジュールは、最も低いモードをユーザへ通知しなければならない。

**[SWS_ComM_00662]** ユーザの並びは、ユーザ 0 からユーザ N までとし、モードポートの名前は UM000、UM001、…、UM<N> でなければならない。

SWS_ComM_00662 の根拠: ポートベースの API を使って、特定のユーザへ直接アクセスすることも可能でなければならない。

コメント: ポートの配列の中では、ポートはアルファベット順に名付けられる。

**[SWS_ComM_00778]** ComM モジュールは、SW-C が接続されている個々のユーザそれぞれへ、モードの変化を明示的に通知しなければならない。ComM モジュールは、これを、ポート "UMnnn" を通じて RTE の正しい API を呼ぶことで行わなければならない。

コメント: SW-C が接続されている設定済みのユーザごとに、このようなポートが 1 つある。SW-C から使われないユーザ（たとえば ECUC_ComM_00840 によって作られるユーザ）には、モードポートは作られない。

実装のヒント: ComM モジュールの実装は、ユーザへモードの変化を知らせるため、通常の RTE の仕組みのどれを使ってもよい。Communication Manager モジュールの特有のコンフィギュレーションの柔軟性を考えると、RTE の「Indirect API」を使うのが最も適切と思われる。これは次のように動作する（詳細は RTE の仕様書を参照）。

Communication Manager モジュールの実装は、実行時に "UMnnn" ポートの配列を得るため、"Rte_Ports" API を使える。

```c
/* インタフェース ComM_CurrentMode を提供するすべてのポートの配列を返す。
   特定の命名規則を選んでいるため、この配列の要素 n は、ポート UM<nnn> を
   参照する。たとえば userModePorts[1] は、ポート UM001 のハンドルになる */
userModePorts = Rte_Ports_ComM_CurrentMode_P();
```

このような userModePorts の数は、呼び出し Rte_NPorts_ComM_CurrentMode_P で得られる。この値は、COMM_USER_LIST 配列のサイズに対応する。

ユーザ n が新しいモードにあることを知らせるには、Communication Manager モジュールは、userModePorts[n].Switch_currentMode(newMode) を呼ぶべきである。

詳細は AUTOSAR RTE の仕様書 [8] と、AUTOSAR Services Mode Management の仕様書 [21] を参照。

##### 8.6.1.6 Basic Software Mode Manager（BswM）

**[SWS_ComM_00861]** ComM モジュールは、Communication Manager モジュールのチャネルの状態を報告するため、BswM が提供する対応する関数を使わなければならない（SWS_ComM_00828 参照）。

詳細は AUTOSAR Basic Software Mode Manager モジュール [29] を参照。

> 学習メモ: これが、BswM が ComM のモードを条件にルールを評価する経路（`BswM_ComM_CurrentMode`）の根拠である。

#### 8.6.2 オプションのインタフェース

本章は、このモジュールのオプションの機能を満たすために必要なすべてのインタフェースを定義する。

**[SWS_ComM_00829]**

| API 関数 | モジュール | 説明 |
|---|---|---|
| Det_ReportError | Det | 開発エラーを報告するサービス |
| BswM_ComM_CurrentPNCMode | BswM | ComM が PNC の現在のモードを示すために呼ぶ関数 |

##### 8.6.2.1 AUTOSAR DET

Communication Manager モジュールは、開発エラーを報告するため、Default Error Tracer モジュールの Det_ReportError を使わなければならない。

#### 8.6.3 設定可能なインタフェース

なし。

#### 8.6.4 AUTOSAR COM

**[SWS_Com_00197]**

| 項目 | 内容 |
|---|---|
| サービス名 | Com_SendSignal |
| 構文 | `uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)` |
| サービス ID | 0x0a |
| 同期 / 非同期 | 非同期 |
| リエントラント | 同じ信号に対しては不可。異なる信号に対しては可。 |
| 入力パラメータ | SignalId: 送信する信号の ID<br>SignalDataPtr: 送信する信号データへの参照 |
| 戻り値 | E_OK: サービスを受け付けた<br>COM_SERVICE_NOT_AVAILABLE: 対応する I-PDU グループが停止していた（または開発エラーのためサービスが失敗した）<br>COM_BUSY: 大きなデータ型の扱いのために TP バッファがロックされている場合 |
| 説明 | サービス Com_SendSignal は、SignalId で識別される信号オブジェクトを、SignalDataPtr パラメータが参照する信号で更新する。 |

(SRS_Com_02037)

**[SWS_Com_00198]**

| 項目 | 内容 |
|---|---|
| サービス名 | Com_ReceiveSignal |
| 構文 | `uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)` |
| サービス ID | 0x0b |
| 同期 / 非同期 | 同期 |
| リエントラント | 同じ信号に対しては不可。異なる信号に対しては可。 |
| 入力パラメータ | SignalId: 受信する信号の ID |
| 出力パラメータ | SignalDataPtr: 受信した信号データを格納する場所への参照 |
| 戻り値 | E_OK: サービスを受け付けた<br>COM_SERVICE_NOT_AVAILABLE: 対応する I-PDU グループが停止していた（または開発エラーのためサービスが失敗した）<br>COM_BUSY: 大きなデータ型の扱いのために TP バッファがロックされている場合 |
| 説明 | Com_ReceiveSignal は、SignalId で識別される信号のデータを、SignalDataPtr で指定された場所へコピーする。 |

(SRS_Com_02037)

### 8.7 サービスインタフェース

#### 8.7.1 センダ・レシーバインタフェース

##### 8.7.1.1 ComM_CurrentChannelRequest

**[SWS_ComM_00904]**

- 名前: ComM_CurrentChannelRequest_{channel_name}
- コメント: このチャネルについて、FULL_COM 要求を現在保持している ComMUserIdentifier の配列。属性 fullComRequestors.handleArray のサイズは NUM_COMM_USER_PER_CHANNEL である。
- IsService: true
- バリエーション: {ecuc(ComM/ComMConfigSet/ComMChannel/ComMFullCommRequestNotificationEnabled)} == true、channel_name = {ecuc(ComM/ComMConfigSet/ComMChannel.SHORT-NAME)}
- データ要素: fullComRequestors（型: ComM_UserHandleArrayType_{channel_name}）

#### 8.7.2 クライアント・サーバインタフェース

##### 8.7.2.1 ComM_ChannelLimitation

**[SWS_ComM_00743]**

- 名前: ComM_ChannelLimitation
- コメント: 「Mode Manager」の役割を担う SW-C は、このインタフェースを使って、指定したチャネルの通信モードを抑止するよう、Communication Manager モジュールをコンフィギュレーションできる。
- IsService: true
- バリエーション: {ecuc(ComM/ComMGeneral.ComMModeLimitationEnabled)} == true
- 起こりうるエラー: 0 = E_OK、1 = E_NOT_OK
- 操作:
  - **GetInhibitionStatus**: チャネルの抑止状態を返す。パラメータ Status（型 ComM_InhibitionStatusType、方向 OUT）。エラー: E_OK = 操作成功、E_NOT_OK。
  - **LimitChannelToNoComMode**: COMM_NO_COMMUNICATION からより高い通信モードへ変えるための、チャネルの抑止状態を変更する（ComM_LimitECUToNoComMode も参照。機能は同じで、対象がすべてのチャネルである）。パラメータ Status（型 boolean、方向 IN。FALSE = チャネルの COMM_NO_COMMUNICATION への制限を無効、TRUE = 有効）。エラー: E_OK = 操作成功、E_NOT_OK。

##### 8.7.2.2 ComM_ChannelWakeup

**[SWS_ComM_00742]**

- 名前: ComM_ChannelWakeup
- コメント: 「Mode Manager」の役割を担う SW-C は、このインタフェースを使って、通信を開始することで他の ECU を起こしてしまうことに備えた予防措置を取るよう、Communication Manager モジュールをコンフィギュレーションできる。
- IsService: true
- バリエーション: {ecuc(ComM/ComMGeneral.ComMWakeupInhibitionEnabled)} == true
- 起こりうるエラー: 0 = E_OK、1 = E_NOT_OK
- 操作:
  - **GetInhibitionStatus**: チャネルの抑止状態を返す。パラメータ Status（型 ComM_InhibitionStatusType、方向 OUT）。エラー: E_OK = 操作成功、E_NOT_OK。
  - **PreventWakeUp**: 対応するチャネルの抑止状態 COMM_NO_WAKEUP を変更する。パラメータ Status（型 boolean、方向 IN）。エラー: E_OK = 操作成功、E_NOT_OK。

##### 8.7.2.3 ComM_ECUModeLimitation

**[SWS_ComM_00741]**

- 名前: ComM_ECUModeLimitation
- コメント: 「Mode Manager」の役割を担う SW-C は、このインタフェースを使って、ECU 全体の振る舞いを変えられる。
- IsService: true
- バリエーション: {ecuc(ComM/ComMGeneral.ComMModeLimitationEnabled)} == true
- 起こりうるエラー: 0 = E_OK、1 = E_NOT_OK
- 操作:
  - **LimitECUToNoComMode**: COMM_NO_COMMUNICATION からより高い通信モードへ変えるための、ECU（= すべてのチャネル）の抑止状態を変更する（ComM_LimitChannelToNoComMode も参照。機能は同じで、対象が特定のチャネルである）。パラメータ Status（方向 IN。FALSE = ECU の COMM_NO_COMMUNICATION への制限を無効）。エラー: E_OK = 操作成功、E_NOT_OK。
  - **ReadInhibitCounter**: 「抑止された full communication 要求カウンタ」の値を返す。バリエーション: {ecuc(ComM/ComMGeneral.ComMGlobalNvMBlockDescriptor)} != NULL。パラメータ CounterValue（型 uint16、方向 OUT）。エラー: E_OK = 操作成功、E_NOT_OK。
  - **ResetInhibitCounter**: 「抑止された full communication 要求カウンタ」をリセットする。バリエーション: {ecuc(ComM/ComMGeneral.ComMGlobalNvMBlockDescriptor)} != NULL。エラー: E_OK = 操作成功、E_NOT_OK。
  - **SetECUGroupClassification**: ECU グループ分類の状態を変更する。パラメータ Status（型 ComM_InhibitionStatusType、方向 IN）。エラー: E_OK = 操作成功、E_NOT_OK。

##### 8.7.2.4 ComM_UserRequest

**[SWS_ComM_01000]**

- 名前: ComM_UserRequest
- コメント: ECU のローカルな Communication Manager モジュールを、特定の状態へ明示的に向けたい SW-C は、クライアント・サーバインタフェース ComM_UserRequest を require する。このインタフェースを通じて、SW-C は、そのコンポーネントに関連するすべての通信チャネルの望ましい状態を、「No Communication」または「Full Communication」に設定できる。
- IsService: true
- 起こりうるエラー: 0 = E_OK、1 = E_NOT_OK、2 = E_MODE_LIMITATION
- 操作:
  - **GetCurrentComMode**: SW-C の、現在の Communication Manager モジュールのモードを返す。パラメータ ComMode（型 ComM_ModeType）。エラー: E_OK = 操作成功、E_NOT_OK。
  - **GetMaxComMode**: SW-C の、現在の Communication Manager モジュールのモードを返す（原本の説明）。パラメータ ComMode（型 ComM_ModeType）。エラー: E_OK = 操作成功、E_NOT_OK。
  - **GetRequestedComMode**: SW-C が最後に要求した Communication Manager モジュールのモードを返す。パラメータ ComMode（型 ComM_ModeType）。エラー: E_OK = 操作成功、E_NOT_OK。
  - **RequestComMode**: SW-C が、自分が必要とするすべての通信チャネルが、指定した Communication Manager モジュールのモードになることを要求する。パラメータ ComMode（型 ComM_ModeType、方向 IN）。エラー: E_OK = 操作成功、E_NOT_OK、E_MODE_LIMITATION = ComMMode の抑止のため、ComMMode を認められない。

#### 8.7.3 モードスイッチインタフェース

##### 8.7.3.1 ComM_CurrentMode

**[SWS_ComM_01001]**

- 名前: ComM_CurrentMode
- コメント: 自分の現在の Communication Manager モジュールのモードの通知を受けたい SW-C は、ModeSwitchInterface ComM_CurrentMode を require する。
- IsService: true
- モードグループ: currentMode（ComMMode）

#### 8.7.4 実装データ型

##### 8.7.4.1 ComM_InhibitionStatusType

**[SWS_ComM_00669]**

- 名前: ComM_InhibitionStatusType
- 種類: ビットフィールド
- 派生元: uint8
- 要素:
  - WakeupInhibitionActive、マスク 0x01: ビット 0（LSB）: ウェイクアップの抑止が有効。
  - LimitedToNoCom、マスク 0x02: ビット 1: NoCom への制限（原本の説明は途中で切れている）。
- 説明: ComM の抑止状態。モードの抑止が有効かどうかを定義する。たとえば status=00000011 → ウェイクアップの抑止と、COMM_NO_COMMUNICATION モードへの制限が有効。

##### 8.7.4.2 ComM_ModeType

**[SWS_ComM_00672]**

- 名前: ComM_ModeType
- 種類: 型
- 派生元: uint8
- 説明: Communication Manager の現在のモード（ステートマシンのメイン状態）。
- 値の範囲:
  - COMM_NO_COMMUNICATION = 0
  - COMM_SILENT_COMMUNICATION = 1
  - COMM_FULL_COMMUNICATION = 2

> 注: 原本の各値の説明は、PDF からの抽出で途切れていた（「…Communication" mode. Configured channel…」）。

##### 8.7.4.3 ComM_UserHandleType

**[SWS_ComM_00670]**

- 名前: ComM_UserHandleType
- 種類: 型
- 派生元: uint8
- 説明: ユーザを識別するハンドル。ユーザごとに、システム生成時に固有の値を定義しなければならない。
- バリエーション: ユーザの最大数は 255。有効なユーザ ID の範囲は 0〜254。ユーザ ID 255 は予約されており、シンボル表現 COMM_NOT_USED_USER_ID を持つ。

##### 8.7.4.4 ComM_UserHandleArrayType

**[SWS_ComM_00906]**

- 名前: ComM_UserHandleArrayType_{channel_name}
- 種類: 構造体
- 要素:
  - numberOfRequesters（uint8）
  - handleArray（ComM_UserHandleSubArrayType_{channel_name}）
- 説明: numberOfRequesters は、"handleArray" メンバ内の有効なユーザハンドルのエントリの数を含む。チャネルを要求したままにしているユーザがいなければ、これはゼロである。{LOWER-LIMIT=0, UPPER-LIMIT=MAX_CHANNEL_REQUESTER}
- バリエーション: channel_name = {ecuc(ComM/ComMConfigSet/ComMChannel.SHORT-NAME)}

##### 8.7.4.5 ComM_UserHandleSubArrayType

**[SWS_ComM_01005]**

- 名前: ComM_UserHandleSubArrayType_{channel_name}
- 種類: 配列
- 要素の型: ComM_UserHandleType
- サイズ: COUNT{ecuc(ComM/ComMConfigSet/ComMChannel/ComMUserPerChannel)} 個
- 説明: この要素は、チャネルを要求したままにしているユーザ（がいれば）のユーザハンドルを、最初のエントリから順に含む。配列のサイズ MAX_CHANNEL_REQUESTERS は、チャネルを要求するユーザの数の最大値である。
- バリエーション: channel_name = {ecuc(ComM/ComMConfigSet/ComMChannel.SHORT-NAME)}

#### 8.7.5 ポート

##### 8.7.5.1 ComM_CL

**[SWS_ComM_01006]**

- 名前: CL_{channel_name}
- 種類: ProvidedPort、インタフェース ComM_ChannelLimitation
- ポートで定義された引数値: 型 NetworkHandleType、値 {ecuc(ComM/ComMConfigSet/ComMChannel/ComMChannelId.value)}
- バリエーション: {ecuc(ComM/ComMGeneral.ComMModeLimitationEnabled)} == true

##### 8.7.5.2 ComM_CR

**[SWS_ComM_01007]**

- 名前: CR_{channel_name}
- 種類: ProvidedPort、インタフェース ComM_CurrentChannelRequest_{channel_name}
- バリエーション: {ecuc(ComM/ComMConfigSet/ComMChannel/ComMFullCommRequestNotificationEnabled)} == true

##### 8.7.5.3 ComM_CW

**[SWS_ComM_01008]**

- 名前: CW_{channel_name}
- 種類: ProvidedPort、インタフェース ComM_ChannelWakeup
- ポートで定義された引数値: 型 NetworkHandleType、値 {ecuc(ComM/ComMConfigSet/ComMChannel/ComMChannelId.value)}
- バリエーション: {ecuc(ComM/ComMGeneral.ComMWakeupInhibitionEnabled)} == true

##### 8.7.5.4 ComM_modeLimitation

**[SWS_ComM_01009]**

- 名前: modeLimitation
- 種類: ProvidedPort、インタフェース ComM_ECUModeLimitation
- バリエーション: {ecuc(ComM/ComMGeneral.ComMModeLimitationEnabled)} == true

##### 8.7.5.5 ComM_UM

**[SWS_ComM_01010]**

- 名前: UM_{user_name}
- 種類: ProvidedPort、インタフェース ComM_CurrentMode
- バリエーション: user_name = {ecuc(ComM/ComMConfigSet/ComMUser.SHORT-NAME)}

##### 8.7.5.6 ComM_UR

**[SWS_ComM_01011]**

- 名前: UR_{user_name}
- 種類: ProvidedPort、インタフェース ComM_UserRequest
- ポートで定義された引数値: 型 ComM_UserHandleType、値 ecuc(ComM/ComMConfigSet/ComMUser/ComMUserIdentifier.value)}
- バリエーション: user_name = {ecuc(ComM/ComMConfigSet/ComMUser.SHORT-NAME)}

#### 8.7.6 モード宣言グループ

##### 8.7.6.1 ComMMode

**[SWS_ComM_01012]**

- 名前: ComMMode
- 種類: ModeDeclarationGroup
- カテゴリ: ALPHABETIC_ORDER
- 初期モード: COMM_NO_COMMUNICATION
- モード: COMM_FULL_COMMUNICATION、COMM_NO_COMMUNICATION、COMM_SILENT_COMMUNICATION

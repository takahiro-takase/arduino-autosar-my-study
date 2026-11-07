# CAN Interface（CanIf）仕様書 日本語訳

- 原典: AUTOSAR CP Release 4.3.1 `AUTOSAR_SWS_CANInterface.pdf`（Document ID 012）
- 範囲: 第7章（機能仕様）と第8章（API 仕様）。第1〜6章（導入、関連文書、制約、依存関係、トレーサビリティ）、第9章（シーケンス図）、第10章以降（コンフィギュレーション）は対象外。
- 凡例: `[SWS_CANIF_xxxxx]` は原典の条項 ID。`（SRS_…）` は原典が条項に付けている上位要求の ID。`ECUC_CanIf_xxxxx` はコンフィギュレーションパラメータの ID。`[1]`、`[3]` などは、原典の参考文献の番号（[1] は CAN Driver、[2] は CAN Transceiver Driver、[3] は CAN State Manager の仕様書）。
- 用語: CanDrv は CAN Driver、CanTrcv は CAN Transceiver Driver、CanSm は CAN State Manager、HOH は Hardware Object Handle（HTH は送信、HRH は受信）、L-PDU／L-SDU は CAN の PDU／SDU、`<User_…>` は上位層のモジュールがコンフィギュレーションで決めるコールバック名。
- 図は訳せないため、位置に注記だけを残している。図の内容は原典を参照すること。
- 「学習メモ」は原文の訳ではなく、このプロジェクトの実装と照らすための訳者の解釈。

---

## 7 機能仕様

### 7.1 一般的な機能

CanIf のサービスは、次の主なグループに分けられる。

- 初期化
- 送信要求のサービス
- 送信確認のサービス
- 受信通知のサービス
- コントローラのモード制御のサービス
- PDU のモード制御のサービス

CanIf の使用の可能性:

1. 割り込みモード
   CanDrv は、CAN コントローラが起こす割り込みを処理する。イベント駆動の CanIf は、イベントが起きたときに通知される。この場合、関連する CanIf のサービスは、CanDrv の対応する ISR の中で呼ばれる。
2. ポーリングモード
   CanDrv は SchM により起動され、以降の処理を行う（ポーリングモード）。この場合、`Can_MainFunction_<Write/Read/BusOff/Wakeup/Transceiver>()` を、定義された時間間隔の中で周期的に呼ぶ必要がある。CanIf は、CAN コントローラのいずれかで起きたイベント（受信、送信、バスオフ、送信キャンセル、タイムアウト）について、割り込み駆動の動作と同様に、CanDrv から通知される。CanDrv は、CAN コントローラで起きたイベントに属する、対応する情報（例: L-PDU の受信）の更新に責任を持つ。
3. 混在モード: 割り込み駆動とポーリング駆動の CanDrv
   機能は、使われる CAN コントローラに応じて、割り込み駆動とポーリング駆動の動作モードに分けられる。例: ポーリング駆動の FullCAN の受信と、割り込み駆動の BasicCAN の受信、ポーリング駆動の送信と割り込み駆動の受信、など。

この仕様は、3 つの動作モードすべてに有効な、一意のインタフェースを記述する。要約すると、CanIf は、イベントが割り込み、タスクレベル、または混在のいずれで処理されても、同じように動作する。唯一の違いは、呼び出しのコンテキストと、おそらく通知の割り込みの方法（プリエンプティブか協調的か）である。すべてのサービスは、コンフィギュレーションに従って実行される。

以降の項で、CanIf の機能を説明する。

### 7.2 ハードウェアオブジェクトハンドル

送信用（HTH）と受信用（HRH）のハードウェアオブジェクトハンドル（HOH）は、CanId、DLC、データなどの CAN に関するパラメータを含む、CAN メールボックスの構造への、抽象的な参照を表す。CAN ハードウェアバッファの抽象化に基づき、各ハードウェアオブジェクトは、CAN ハードウェアバッファのレイアウトから独立して、CanIf で参照される。HOH は、CanDrv のインタフェースサービスの呼び出しのパラメータとして使われ、CanDrv のコンフィギュレーションにより提供され、CanDrv により CAN メールボックスの通信バッファの識別子として使われる。

CanIf は、ハードウェアオブジェクトハンドルのユーザとして動作するだけで、ハードウェア固有の情報に基づいてそれを解釈しない。したがって、CanIf はハードウェアから独立したままである。

**[SWS_CANIF_00023]** CanIf は、ハードウェア固有の通信バッファへの直接のアクセスを避け、CanDrv のインタフェースサービスを介してだけアクセスしなければならない。（SRS_Can_01001）

[SWS_CANIF_00023] の根拠: CanDrv のインタフェースは、具体的な CAN ハードウェアバッファの特性から抽象化された HOH のパラメータで呼ばれるため、CanIf はハードウェアから独立したままである。

各 CAN コントローラは、CAN メールボックスに、複数の CAN 送信ハードウェアオブジェクトを提供できる。これらは、ハードウェアオブジェクトの 1 つのプール全体（多重化されたハードウェアオブジェクト）へ論理的に結び付けられ、したがって 1 つの HTH で扱える。

**[SWS_CANIF_00662]** CanIf は、CanDrv へのアクセスを可能にするために、2 種類の HOH を使わなければならない。

- ハードウェア送信ハンドル（HTH）、および
- ハードウェア受信ハンドル（HRH）

**[SWS_CANIF_00291]** HRH の定義: HRH は、CAN コントローラのメールボックスの、論理的なハードウェア受信オブジェクトを参照するハンドルでなければならない。

**[SWS_CANIF_00665]** HRH は、CanIf が、参照される受信ユニットの BasicCAN または FullCAN の受信方式を使い、受信した L-SDU を対象の上位層のモジュールへ通知できるようにしなければならない。

**[SWS_CANIF_00663]** HRH が、BasicCAN の受信にコンフィギュレーションされた受信ユニットを参照する場合、CanIf でソフトウェアフィルタリングが有効にされなければならない。

**[SWS_CANIF_00664]** 複数の HRH が使われる場合、各 HRH は、少なくとも、Rx L-SDU（`CanRxPduId`）の単一の、または固定のグループに属さなければならない。

HRH は、次のものを受信するようにコンフィギュレーションできる。

- 1 つの CanId（FullCAN）
- 単一の CanId のグループ（BasicCAN）
- CanId の範囲／領域（BasicCAN）、または
- すべての CanId

> （図 7.1「PDU Id と HW オブジェクトハンドルの対応付け」: 図。訳なし。）

**[SWS_CANIF_00292]** HTH の定義: HTH は、CAN コントローラのメールボックスの、論理的なハードウェア送信オブジェクトを参照するハンドルでなければならない。

**[SWS_CANIF_00666]** HTH は、CanIf が、参照される送信ユニットの BasicCAN または FullCAN の送信方式を使い、送信した L-SDU を対象の上位層のモジュールへ確認できるようにしなければならない。

**[SWS_CANIF_00466]** CanIf の各 Tx L-PDU は、コンフィギュレーションの時点で、1 つの `CanIfBufferCfg` コンフィギュレーションコンテナへ静的に割り当てられなければならない（`CanIfTxPduBufferRef` を参照）。

[SWS_CANIF_00466] の根拠: CanIf の Tx L-PDU は、HTH を参照せず、`CanIfBufferCfg` を参照し、それが HTH を参照する。

**[SWS_CANIF_00667]** 複数の HTH が使われる場合、各 HTH は、Tx L-PDU（`CanTxPduId`）の単一の、または固定のグループに属さなければならない。

**[SWS_CANIF_00115]** CanIf は、1 つの CanDrv のすべての HRH と HTH を、ゼロから始まる、共通の単一の番号領域として使えなければならない。

専用の HRH と HTH は、CanDrv のコンフィギュレーションセットから導かれる。番号領域とハードウェアオブジェクトの中での HTH/HRH の定義は、CanDrv に任される。

### 7.3 静的な L-PDU

CanIf は、上位層へ、CAN の L-SDU に関するデータへの一般的なアクセスを提供する。次の表の属性は、コンフィギュレーションパラメータとして表され、第 10 章で規定される。

> **注（再構成）**: 原典の表は、2 つの列の内容がテキストの抽出で混ざっていた。「CAN Interface 固有の属性」と「CAN プロトコル制御情報（PCI）」の項目を、それぞれ列挙し直した。

| CAN Interface 固有の属性 | CAN プロトコル制御情報（PCI） |
|---|---|
| ソフトウェアフィルタリングの方法 `CanIfPrivateSoftwareFilterType` | CAN 識別子（CanId）: `CanIfTxPduCanId`、PDU ごとの CanId の範囲（`CanIfRxPduCanIdRange` を参照）、`CanIfRxPduCanId`、`CanIfRxPduCanIdMask` |
| L-PDU の方向（Tx、Rx）`CanIfTxPduId`、`CanIfRxPduId` | CAN 識別子の種類（StandardCAN、ExtendedCAN）: CanDrv から `CanIfHthIdSymRef`、`CanIfHrhIdSymRef` で参照される |
| CAN コントローラの HTH/HRH | データ長とデータ長コード（DLC）: `CanIfRxPduDataLength` |
| 対応する上位層の対象 ID: `CanIfTxPduUserTxConfirmationUL`、`CanIfRxPduUserRxIndicationUL` | PDU データへの参照（[1, CAN Driver の仕様] を参照） |
| 送信 L-PDU の種類（STATIC、DYNAMIC）`CanIfTxPduType` | |
| Tx/Rx L-PDU の種類（FullCAN、BasicCAN）`CanIfHthIdSymRef`、`CanIfHrhIdSymRef` | |

**[SWS_CANIF_00046]** CanIf は、各 L-PDU を 1 つの CAN コントローラにだけ割り当てなければならない。したがって、1 つの L-PDU を複数の CAN コントローラへ割り当てることは禁止される。

[SWS_CANIF_00046] の根拠: この関係は、送信確認と受信通知のイベントで、L-SDU を正しく振り分けるために使われる。この方法で、CanIf は、L-PDU から CAN コントローラを識別できる。

CanIf は、1 つの CAN コントローラに属するすべての L-PDU の、送信と受信の両方について、有効化と無効化をサポートする（7.19.2 節、`CanIf_SetPduMode()`、[SWS_CANIF_00008] を参照）。L-PDU のモード制御は、7.19 節を参照。

各 L-PDU は、受信、送信確認、データアクセスの間の振り分けを正しく行うために、上位層のモジュールに関連付けられる。各上位層のモジュールは、L-PDU を使って、異なる CAN コントローラに同時にサービスを提供できる。

AUTOSAR の通信スタック全体で定義された PDU のアーキテクチャ（[7, Layered Software Architecture] を参照）に従い、L-PDU の使用は 2 つの異なる方法に分けられる。

- 送信要求と、送信／受信のポーリング API では、上位層のモジュールは、CanIf が定義した L-SDU ID（`CanTxPduId`/`CanRxPduId`）をパラメータとして使う。
- CanIf が上位層のモジュールで呼ぶ、すべてのコールバック API では、CanIf は、各上位層のモジュールが定義した対象の PduId をパラメータとして渡す。

原則は、呼び出し側が、呼ばれる側の定義した対象の L-PDU/L-SDU Id を使わなければならないことである。

電源投入時の初期化が行われず、上位層が CanIf に送信要求を行った場合、L-SDU は下位層へ送信されず、DET が呼ばれる。したがって、初期化されていないデータが、ネットワークで送信されることはない。L-PDU/L-SDU の送信関数の動作は、8.3.6 項で詳しく規定される。

### 7.4 動的な L-PDU

CanIf は、`CanIfRxPduCanIdMask` を使って、受信メッセージをフィルタリングする能力をサポートしなければならない。フィルタリングは、受信した CanId を、保存された `CanIfRxPduCanId` と、両方の ID にマスク `CanIfRxPduCanIdMask` を適用した後で比較して行う。これは、マスクなしの通常の CanId のフィルタリングの後に行われるべきである。これにより、マスクで定義された範囲、または CanId に基づく範囲に含まれる、一部の CanId を、別に扱える。

さらに、CanId が L-SDU のメタデータの中にある、動的な Tx と Rx の L-SDU がサポートされなければならない。

動的な L-SDU の送信では、`CanIfTxPduCanIdMask` が定義されている場合、メタデータで渡された CanId の可変の部分を、このマスクを使って CanId とマージしなければならない。`CanIfTxPduCanIdMask` も `CanIfTxPduCanId` もコンフィギュレーションされていない場合、メタデータが、そのまま CanId として使われなければならない。

動的な L-SDU の受信では、受信した CanId は、L-SDU のメタデータに置かれなければならない。メタデータの内容は、パラメータ `CanIfRxPduCanIdMask` から独立している。

**[SWS_CANIF_00844]** CanIf は、CanId、または CanId の関連する部分が、L-SDU のメタデータに置かれる、動的な L-PDU をサポートしなければならない。

#### 7.4.1 動的な送信 L-PDU

動的な送信 L-PDU の定義: 実行時に CanId の再設定を許す（`CanIfTxPduType`）、または ID もしくはその一部が L-SDU のメタデータとして渡される L-PDU。

他のすべての L-PDU の要素の使い方は、通常の静的な送信 L-PDU と同じである。

- 送信確認の通知 `CanIfTxPduUserTxConfirmationUL` は、L-PDU に属するため、再設定できない。
- データ長とデータバッファへのポインタは、どちらも、`CanIf_Transmit()` の呼び出し時に、上位層のモジュールにより決められる。

関数 `CanIf_SetDynamicTxId()`（[SWS_CANIF_00189] を参照）は、`CanIfTxPduType` を持つ動的な L-PDU の CanId を再設定する。

**[SWS_CANIF_00188]** CanIf は、どの種類の CanId が使われ、したがって動的な送信 L-PDU をどのように送信するかを決めるために、CanId の上位 2 ビットを処理しなければならない（[1, CAN Driver の仕様]、`Can_IdType` の定義 [SWS_Can_00416] を参照）。

**[SWS_CANIF_00673]** CanIf は、関数 `CanIf_SetDynamicTxId()` の実行中、CanId のデータの一貫性を保証しなければならない。このサービスは、同じ L-PDU に影響する `CanIf_Transmit()` のプリエンプティブな呼び出しに、中断されうる。[SWS_CANIF_00064] を参照。

**[SWS_CANIF_00855]** `CanIfTxPduCanIdMask` と `CanIfTxPduCanId` が省略されている場合、CanId は、直接メタデータから取られる。

**[SWS_CANIF_00856]** `CanIfTxPduCanIdMask` は、メタデータのコンフィギュレーションに、この L-SDU の `CAN_ID_32` が含まれない場合、無視されなければならない。

**[SWS_CANIF_00854]** MetaDataItem の `CAN_ID_32`、`CanIfTxPduCanIdMask`、`CanIfTxPduCanId` が利用できる場合、`CanIfTxPduCanIdMask` は、`CanIfTxPduCanId` の中で、実際の CanId に現れるべきビットを定義し、他のビットはメタデータから取られる。

注: 結果の ID は、次のように計算できる: `(CanIfTxPduCanId & CanIfTxPduCanIdMask) | (<動的な ID の部分> & CanIfTxPduCanIdMask)`

**[SWS_CANIF_00857]** `CanIf_Init()`（[SWS_CANIF_00085] を参照）は、`CanIfTxPduType` を持つ動的な送信 L-PDU の CanId を、`CanIfTxPduCanId` でコンフィギュレーションされた値に初期化する。

#### 7.4.2 動的な受信 L-PDU

動的な受信 L-PDU の定義: CanId の集合に対応し、実際に受信した CanId が、PDU データの一部として上位層へ渡される L-PDU。

**[SWS_CANIF_00847]** コンフィギュレーションは、動的な受信 L-PDU が、ID の範囲またはマスクを使い、MetaDataItem の `CAN_ID_32` が L-SDU にコンフィギュレーションされていることを、保証しなければならない。さらに、これらの L-SDU では、ソフトウェアフィルタリングが有効でなければならない。

**[SWS_CANIF_00848]** 動的な L-SDU の受信時に、CanIf は、CanId を、型 `CAN_ID_32` の MetaDataItem に置かなければならない。

### 7.5 物理チャネルの見え方

物理チャネルは、1 つの CAN コントローラと 1 つの CAN トランシーバに結び付けられ、1 つまたは複数の物理チャネルが 1 つのネットワークに接続されうる。

CanIf は、ECU のすべてのサポートされる CAN チャネルの、CAN コントローラや CAN トランシーバなどのすべての CAN デバイスを制御するサービスを提供する。これらの API は、1 つのネットワークに接続されたすべての物理チャネルの、ウェイクアップとスリープの要求を行うために使われる ComM（[3] を参照）へ、ネットワークの見え方を提供するために、CanSm により使われる。

CanIf は、CanDrv と CanTrcv が提供する状態情報を、物理チャネルごとに別々に、CanSm の状態情報として渡す（`<User_ControllerBusOff>()`、[SWS_CANIF_00014] を参照）。

**[SWS_CANIF_00653]** CanIf は、異なる CanDrv のインスタンスの、異なるコントローラから抽象化された、ControllerId を提供しなければならない。CanIf の中の ControllerId の範囲は、'0' から始まらなければならない。これは、`CANIF_CTRL_ID`（ECUC_CanIf_00647 を参照）でコンフィギュレーションできなければならない。

例:

| CanIf | CanDrv A | CanDrv B |
|---|---|---|
| ControllerId 0 | Controller 0 | Controller 0 |
| ControllerId 1 | Controller 1 | |
| ControllerId 2 | | |

**[SWS_CANIF_00655]** CanIf は、異なる CanTrcv のインスタンスの、異なるトランシーバから抽象化された、TransceiverId を提供しなければならない。CanIf の中の TransceiverId の範囲は、'0' から始まらなければならない。これは、`CANIF_TRCV_ID`（ECUC_CanIf_00654 を参照）でコンフィギュレーションできなければならない。

例:

| CanIf | CanDrv A | CanDrv B |
|---|---|---|
| TransceiverId 0 | Transceiver 0 | Transceiver 0 |
| TransceiverId 1 | Transceiver 1 | |
| TransceiverId 2 | | |

通知の処理の間、CanIf は、ドライバモジュールの元の CAN コントローラまたは CAN トランシーバのパラメータを、CanSm へのパラメータへ対応付ける。この対応付けは、参照される CAN コントローラまたは CAN トランシーバのパラメータが、CanIf の抽象化されたパラメータ ControllerId または TransceiverId でコンフィギュレーションされているために行われる。

> （図 7.2「物理チャネルの見え方の定義の例 A」、図 7.3「例 B」: 図。訳なし。）

CanIf は、複数の物理的な CAN チャネルをサポートする。これらは、ネットワークの制御のために、CanSm により区別されなければならない。CanIf の API は、基盤の複数の物理的な CAN チャネルについて、要求と読み取りの制御を提供する。

さらに、CanIf は、1 つまたは複数の CAN コントローラが接続される、専用の種類の CAN の物理層（すなわち、ローススピード CAN またはハイスピード CAN）を区別しない。

### 7.6 CAN ハードウェアユニット

CAN ハードウェアユニットは、同じ種類の 1 つまたは複数の CAN コントローラモジュールを組み合わせたもので、オンチップでも、外付けの単独のデバイスでもよい。各 CAN ハードウェアユニットは、対応する CanDrv により扱われる。

異なる種類の CAN コントローラが使われる場合、CanIf へ統一された API を持つ、異なる種類の CanDrv も適用しなければならない。CanIf は、CAN コントローラの数と種類、およびそれらのハードウェアオブジェクトについての情報を、コンフィギュレーションの時点で集める。これにより、上位層のモジュールが HOH を使って（7.2 節「ハードウェアオブジェクトハンドル」と 7.24 節「複数の CAN ドライバのサポート」を参照）、CAN コントローラへ、透過的で、ハードウェアに依存しないアクセスができる。

図 7.4 は、2 つの物理チャネルに接続された、同じ種類の 2 つの CAN コントローラから成る、CAN ハードウェアユニットを示す。

> （図 7.4「典型的な CAN ハードウェアユニット」: 図。訳なし。）

### 7.7 BasicCAN と FullCAN の受信

CanIf は、ソフトウェアのアクセプタンスフィルタリングの有効化について、BasicCAN と FullCAN の扱いを区別する。

FullCAN の動作の CAN メールボックス（ハードウェアオブジェクト）は、単一の CanId の送信または受信だけを可能にする。これに対し、1 つのハードウェアオブジェクトの BasicCAN の動作は、ある範囲の CanId の送信または受信を可能にする。

BasicCAN の受信にコンフィギュレーションされたハードウェア受信オブジェクトは、そのハードウェアのアクセプタンスフィルタを通過する、ある範囲の CanId を受信できる。この範囲は、この HRH で受信される、あらかじめ定義された Rx L-PDU の一覧を超えることがある。したがって、CanIf は、その後、あらかじめ定義された Rx L-PDU の一覧だけを、対応する上位層のモジュールへ渡すために、ソフトウェアフィルタリングを実行しなければならない。詳細は、7.20 節「ソフトウェア受信フィルタ」を参照。

**[SWS_CANIF_00467]** CanIf は、コンフィギュレーションコンテナ `CanIfHthCfg`（ECUC_CanIf_00258 を参照）と `CanIfHrhCfg`（ECUC_CanIf_00259 を参照）から導かれるすべての HOH について、HTH と HRH の順序を、コンフィギュレーションし、保存しなければならない。

**[SWS_CANIF_00468]** CanIf は、コンフィギュレーションパラメータ `CANIF_HTH_ID_SYMREF`（ECUC_CanIf_00627 を参照）と `CANIF_HRH_ID_SYMREF`（ECUC_CanIf_00634 を参照）から導かれる各 HOH について、ハードウェアのアクセプタンスフィルタを参照しなければならない。

BasicCAN と FullCAN の動作の主な違いは、ソフトウェアのアクセプタンスフィルタリングの仕組み（7.20 節「ソフトウェア受信フィルタ」を参照）の必要性にある。

**[SWS_CANIF_00469]** CanIf は、パラメータ `CANIF_HRH_SOFTWARE_FILTER`（ECUC_CanIf_00632 を参照）でコンフィギュレーションされた、BasicCAN の種類の各 HRH について、ソフトウェアのアクセプタンスフィルタをコンフィギュレーションし、保存する可能性を与えなければならない。

**[SWS_CANIF_00211]** CanIf は、コールバック関数 `CanIf_RxIndication()` で渡された HRH について、[SWS_CANIF_00469] のソフトウェアのアクセプタンスフィルタを実行しなければならない。

BasicCAN と FullCAN のオブジェクトは、1 つのコンフィギュレーションの中で共存できる。基盤の CAN コントローラが提供するなら、複数の BasicCAN と FullCAN の受信オブジェクトを使える。

**[SWS_CANIF_00877]** CanIf が L-PDU を受信した場合（`CanIf_RxIndication()` を参照）、`CanIfRxPduCfg` でコンフィギュレーションされた正しい受信 L-SDU を選ぶために、次の比較を行わなければならない。

- `CanIfRxPduCanId` を、渡された `Mailbox->CanId`（`Can_IdType`）と、上位 2 ビットを除いて比較する。
- `CanIfRxPduCanIdType` を、渡された `Mailbox->CanId`（`Can_IdType`）の上位 2 ビットと比較する。

（SRS_Can_01140、SRS_Can_01141、SRS_Can_01162）

基本的に、CanIf は、パラメータ `CANIF_TXPDU_CANIDTYPE`（ECUC_CanIf_00590 を参照）と `CANIF_RXPDU_CANIDTYPE`（ECUC_CanIf_00596 を参照）により、1 つの物理 CAN チャネルで、標準の CAN ID または拡張の CAN ID のどちらかの受信をサポートする。

**[SWS_CANIF_00281]** CanIf は、同じ物理チャネルで、StandardCAN ID と ExtendedCAN ID を受け付け、扱わなければならない（＝混在モードの動作）。（SRS_Can_01140）

混在モードの動作では、標準の CAN ID と拡張の CAN ID を、同じ CAN ネットワークで同時に混ぜて使える。混在モードの動作は、BasicCAN/FullCAN のハードウェアオブジェクトが、コンフィギュレーションパラメータ `CANIF_TXPDU_CANIDTYPE`（ECUC_CanIf_00590 を参照）と `CANIF_RXPDU_CANIDTYPE`（ECUC_CanIf_00596 を参照）を使って、StandardCAN または ExtendedCAN の動作のそれぞれに、別々にコンフィギュレーションされている場合に実現できる。混在モードの動作の場合、ソフトウェアのアクセプタンスフィルタのアルゴリズム（7.20 節「ソフトウェア受信フィルタ」を参照）は、両方の種類の CanId を扱えなければならない。

[SWS_CANIF_00281] は、オプションの機能である。この機能は、実装のさまざまな変種で実現でき、コンフィギュレーションのオプションは無い。

### 7.8 初期化

EcuM は、CanIf 全体の初期化のために、CanIf の関数 `CanIf_Init()` を呼ぶ（[SWS_CANIF_00001] を参照）。すべてのグローバル変数とデータ構造は、フラグとバッファを含めて、初期化の処理の間に初期化される。EcuM は、CanDrv と CanTrcv の初期化を、それぞれの対応する初期化サービスの呼び出しで、別々に実行する（[1] と [2, CAN Transceiver Driver の仕様] を参照）。

CanIf は、初期化の処理が完了した後、CAN コントローラが、電源投入リセット後のように、STOPPED モードのままであることを期待する。このモードでは、CanIf と CanDrv は、CAN L-PDU の送信も受信もできない（[SWS_CANIF_00001] を参照）。

実行時に、CAN モジュール全体の再初期化が必要な場合、EcuM は、CanSm（[3] を参照）を呼び出して、CAN Interface モジュールの API サービス `CanIf_SetControllerMode()` の呼び出しにより、CAN コントローラの必要な状態遷移を開始させなければならない。CanIf は、CanSm からの呼び出しを、それぞれの CanDrv の呼び出しへ対応付ける（8.6.3 項を参照）。

### 7.9 送信要求

CanIf の送信要求関数 `CanIf_Transmit()`（[SWS_CANIF_00005]）は、上位層が CAN ネットワークで L-PDU を送信するための、共通のインタフェースである。上位の通信層のモジュールは、CanDrv への直接のアクセスなしに、CanIf のサービスだけを介して送信を開始する。開始された送信要求は、CanDrv が L-PDU のデータを CAN ハードウェアの送信オブジェクトへ書き込めた場合に、正常に完了する。

上位層のモジュールは、API サービス `CanIf_Transmit()` を使って、送信要求を開始する（8.3.6 項「CanIf_Transmit」を参照）。

CanIf は、サービス `CanIf_Transmit()` の呼び出し時に、L-PDU の送信のために、次の処理を行う。

- CanIf の初期化の状態を検査する。
- CanDrv を識別する（複数の CanDrv が使われる場合のみ）。
- CAN ハードウェアの送信オブジェクトへアクセスするための HTH を決める。
- CanDrv の `Can_Write()` を呼ぶ。

送信は、送信要求のサービス `CanIf_Transmit()` が `E_OK` を返した場合に、正常に完了する。

**[SWS_CANIF_00382]** L-PDU の送信が、`CANIF_OFFLINE` に等しい PDU チャネルモード（7.19.2 節「PDU チャネルモード」を参照）を介して要求された場合、CanIf は、ランタイムエラーコード `CANIF_E_STOPPED` を DET の `Det_ReportRuntimeError()` サービスへ報告し、`CanIf_Transmit()` は `E_NOT_OK` を返さなければならない。（SRS_Can_01126）

`Can_Write()` の呼び出しが `CAN_BUSY` で戻る場合は、詳細について、7.12 節「送信確認」を参照。

### 7.10 送信のデータの流れ

送信要求サービス `CanIf_Transmit()` は、L-PDU に基づく。L-SDU 固有のデータへのアクセスは、次のパラメータで構成される。

- 送信 L-PDU ＝ L-SDU ID
- L-SDU に関するデータを含むデータ構造への参照: L-SDU へのポインタ、メタデータへのポインタ、L-SDU の長さ。

L-SDU のデータ構造への参照は、いくつかの CanIf の API サービス（例: `CanIf_Transmit()` やコールバックサービス `<User_RxIndication>()`）のパラメータとして使われる。L-PDU がトリガ送信用にコンフィギュレーションされている場合、L-SDU のポインタは NULL ポインタである。

> （図 7.5「送信のデータの流れ」: フローチャート。訳なし。
> 　読み取れる内容: 上位層が `CanIf_Transmit()` を呼ぶと、`Can_Write()` が呼ばれる。CAN ハードウェアが空いていれば、データを CAN ハードウェアへコピーし、CAN コントローラで送信要求を設定する。空いていない場合、トリガ送信の PDU でなければ、データを CanIf の送信バッファへコピーし（バッファが空いている場合）、トリガ送信の PDU であれば、送信要求をキューに入れる。いずれの場合も、`CanIf_Transmit()` は `E_OK` を返す。）

CanIf は、送信の目的でコンフィギュレーションされた、利用できるハードウェアオブジェクトについての情報を保存する。関数 `CanIf_Transmit()` は、`CanTxPduId` を対応する HTH へ対応付け、関数 `Can_Write()` を呼ぶ（[SWS_CANIF_00318] を参照）。

### 7.11 送信バッファリング

#### 7.11.1 一般的な動作

CanIf の範囲では、送信の処理は、`CanIf_Transmit()` の呼び出しで始まり、上位層のモジュールのコールバックサービス `<User_TxConfirmation>()` の呼び出しで終わる。送信の処理の間、CanIf、CanDrv、CAN メールボックスは、全体で、送信する L-PDU を、1 つの場所に 1 回だけ保存しなければならない。送信の方式に応じて、これらは次のとおりである。

- CAN ハードウェアの送信オブジェクト、または
- 送信バッファリングが有効な場合、CanIf の中の送信 L-PDU バッファ

トリガ送信では、CanIf は、与えられた L-PDU の送信要求だけを保存すればよく、そのデータは保存しなくてよい。データは、HTH が（再び）空いたときに、トリガ送信関数を使って、ちょうど必要なときに取得される。送信を要求された 1 つの Tx L-PDU は、2 回保存されてはならない。この動作は、CAN ネットワークでの周期的な通信の通常の方法に対応する。

送信バッファリングが有効な場合、CanIf は、送信要求のときに CanDrv に拒否された Tx L-PDU を、CanIf の送信 L-PDU バッファ（`CanIfBufferCfg`）に保存する。

基本的に、Tx L-PDU をバッファリングするための CanIf の全体のバッファは、1 つまたは複数の `CanIfBufferCfg` から成る（`CanIfBufferCfg` を参照）。各 `CanIfBufferCfg` は、1 つまたは複数の専用の `CanIfBufferHthRef`（`CanIfBufferHthRef` を参照）に割り当てられ、1 つまたは複数の Tx L-PDU をバッファリングするようにコンフィギュレーションできる。ただし、上述のとおり、`CanIfBufferCfg` の全体の中で、Tx L-PDU ごとに 1 つのインスタンスだけがバッファリングされうる。

L-PDU の送信中の CanIf の動作は、対応する Tx L-PDU について、コンフィギュレーションで送信バッファリングが有効か否かで異なる。送信バッファリングが無効で、CanDrv への送信要求が失敗した場合（CAN コントローラのメールボックスが使用中、BasicCAN）、L-PDU は CAN コントローラのメールボックスへコピーされず、`CanIf_Transmit()` は値 `E_NOT_OK` を返す。送信バッファリングが有効で、CanDrv への送信要求が失敗した場合、`CanIfTxBuffer` のコンフィギュレーションに応じて、L-PDU を `CanIfTxBuffer` に保存できる。この場合、API `CanIf_Transmit()` は、送信が行えなかったにもかかわらず、値 `E_OK` を返す。この場合、CanIf が、`CanIf_TxConfirmation()` コールバックを介して、L-PDU の未処理の送信を担当し、上位層は、送信要求を再試行する必要は無い。

利用できる送信の CanIf Tx L-PDU バッファの数は、この ECU の CAN ネットワーク記述ファイルで定義された、使われる送信 L-PDU の数とは、完全に独立にコンフィギュレーションできる。

[SWS_CANIF_00835] に従い、Tx L-PDU は、コンフィギュレーションコンテナ `CanIfBufferCfg` を介して HTH を参照する（`CanIfBufferCfg` を参照）。これは、送信バッファリングが必要ない場合にも有効である。この場合、`CanIfBufferCfg` のバッファサイズ（`CanIfBufferSize` を参照）は 0 に設定しなければならない。その場合、コンフィギュレーションコンテナ `CanIfBufferCfg` は、HTH を参照するためだけに使われる。

#### 7.11.2 バッファの特性

`CanIfTxPduBufferRef`、`CanIfBufferCfg`、`CanIfBufferHthRef`、`CanIfBufferSize` が、`CanIfBufferCfg` の可能なコンフィギュレーションを記述する。

##### 7.11.2.1 送信 L-PDU バッファへの L-PDU の保存

CanIf は、`Can_Write()` の呼び出しの間に CanDrv が `CAN_BUSY` を返した場合に限り、新しい送信 L-PDU またはその送信要求を、送信 L-PDU バッファに保存しようとする（[SWS_CANIF_00381] を参照）。

**[SWS_CANIF_00063]** CanIf は、パラメータ `CanIfPublicTxBuffering`（`CanIfPublicTxBuffering` を参照）が有効な場合、BasicCAN の送信のための CAN L-PDU の、CanIf でのバッファリングをサポートしなければならない。（SRS_Can_01020）

**[SWS_CANIF_00849]** 動的な送信 L-PDU では、CanId も `CanIfTxBuffer` に保存しなければならない。

**[SWS_CANIF_00381]** 送信バッファリングが有効（[SWS_CANIF_00063] を参照）で、直接の送信にコンフィギュレーションされた PDU の `Can_Write()` の呼び出しが `CAN_BUSY` で戻る場合、CanIf は、`Can_Write()` で送信を要求された CanIf Tx L-PDU を、`CanIfTxBuffer` にバッファリングできるかを検査しなければならない。（SRS_Can_01126）

`Can_Write()` の呼び出しが `CAN_BUSY` で戻るとき、CanDrv は、送信要求（Tx 要求）の時点で利用できる空きのハードウェアオブジェクトが無いために、要求された L-PDU の送信を拒否した（[1] を参照）。

**[SWS_CANIF_00895]** 拒否されたデータ長が、コンフィギュレーションされたサイズを超える場合、CanIf は、次のようにしなければならない。

- コンフィギュレーションされた量のデータをバッファリングし、残りを破棄する。
- ランタイムエラーコード `CANIF_E_DATA_LENGTH_MISMATCH` を、DET の `Det_ReportRuntimeError()` サービスへ報告する。

**[SWS_CANIF_00881]** 送信バッファリングが有効（[SWS_CANIF_00063] を参照）で、トリガ送信にコンフィギュレーションされた PDU の `Can_Write()` の呼び出しが `CAN_BUSY` で戻る場合、CanIf は、`Can_Write()` で送信を要求された送信要求を、`CanIfTxBuffer` にバッファリングできるかを検査しなければならない。（SRS_Can_01126）

**[SWS_CANIF_00835]** CanIf が、CanIf Tx L-PDU または送信要求をバッファリングできるかを検査する場合（[SWS_CANIF_00381]、[SWS_CANIF_00881] を参照）、CanIf Tx L-PDU が、バッファサイズ（`CanIfBufferSize` を参照）がゼロより大きくコンフィギュレーションされた `CanIfBufferCfg`（`CanIfBufferCfg` を参照）に割り当てられている（`CanIfTxPduBufferRef` を参照）場合に限り、可能でなければならない。

どの `CanIfTxBuffer` のバッファサイズも、送信バッファリングが有効な場合に限り、ゼロより大きくコンフィギュレーションできる。さらに、1 つの `CanIfTxBuffer` のバッファサイズは、その `CanIfTxBuffer` が FullCAN の HTH に割り当てられていない場合に限り、ゼロより大きくコンフィギュレーションできる（`CanIfBufferSize` を参照）。

**[SWS_CANIF_00836]** 割り当てられた `CanIfTxBuffer` のバッファサイズがゼロより大きい（[SWS_CANIF_00835] を参照）ために、CanIf Tx L-PDU または送信要求をバッファリングできる場合、CanIf は、CanIf Tx L-PDU または送信要求が、その `CanIfTxBuffer` にまだバッファリングされていなければ、割り当てられた `CanIfTxBuffer` の空いているバッファ要素に、CanIf Tx L-PDU または送信要求をバッファリングしなければならない。

**[SWS_CANIF_00068]** 割り当てられた `CanIfTxBuffer` のバッファサイズがゼロより大きい（[SWS_CANIF_00835] を参照）ために、CanIf Tx L-PDU または送信要求をバッファリングできる場合、CanIf は、`Can_Write()` が `CAN_BUSY` を返したときに、CanIf Tx L-PDU がすでにその `CanIfTxBuffer` にバッファリングされていれば、割り当てられた `CanIfTxBuffer` の、直接送信された CanIf Tx L-PDU を上書きしなければならない。（SRS_Can_01011）

注: すでに保存されている送信要求（[SWS_CANIF_00068] を参照）には、何もする必要が無い。データは、CanDrv が（`CanIf_TriggerTransmit()` を使って）直接取得するためである。したがって、最新のデータが自動的に送信される。

複数の異なる L-PDU の送信要求の順序を保つ必要がある場合、上位層のモジュールの送信要求は、前の送信確認の通知に結び付けられなければならない。すなわち、後続の L-PDU は、前の L-PDU の送信確認が CanIf から通知された場合に限り、上位層のモジュールにより送信を要求される。

注: さらに、送信要求の順序は、コンフィギュレーションされたハードウェア送信オブジェクトの数によって、異なりうる。

**[SWS_CANIF_00837]** バッファサイズがゼロより大きく、すべてのバッファ要素が使用中で、新しい L-PDU（同じ L-PDU の他のインスタンスが、バッファにすでに保存されていない）で `CanIf_Transmit()` が呼ばれた場合、新しい L-PDU またはその送信要求は保存されてはならず、`CanIf_Transmit()` は `E_NOT_OK` を返さなければならない。

##### 7.11.2.2 送信 L-PDU バッファのクリア

**[SWS_CANIF_00386]** CanIf は、送信確認（[SWS_CANIF_00007] を参照）の間に、新たに空いたハードウェア送信オブジェクト（[SWS_CANIF_00466] を参照）に割り当てられた `CanIfTxBuffer` に、保留中の CanIf Tx L-PDU または送信要求が保存されているかを評価しなければならない。

**[SWS_CANIF_00668]** [SWS_CANIF_00386] のとおり、`CanIfTxBuffer` に保留中の CanIf Tx L-PDU または送信要求がある場合、CanIf は、その保留中の CanIf Tx L-PDU または送信要求（新しいハードウェア送信オブジェクトに割り当てられたもの）のうち、最も優先度の高いもの（[SWS_CANIF_00070] を参照）について、`Can_Write()` を呼ばなければならない。

**[SWS_CANIF_00070]** CanIf は、送信 L-PDU バッファに保存された L-PDU または送信要求を、HTH ごとに、優先度の順（[12] を参照）に送信しなければならない。CanIf は、L-PDU と送信要求を区別してはならない。

**[SWS_CANIF_00183]** CanIf が、`CanIfTxBuffer` に保存された、優先度付けされた L-PDU と送信要求について、関数 `Can_Write()` を呼び、`Can_Write()` の戻り値が `E_OK` の場合、CanIf は、この L-PDU または送信要求を、送信確認が戻る前に、直ちに送信 L-PDU バッファから取り除かなければならない。

[SWS_CANIF_00183] で規定された動作は、送信 L-PDU バッファに保存された新しい送信 L-PDU の選択を簡単にする。

##### 7.11.2.3 送信 L-PDU バッファの初期化

**[SWS_CANIF_00387]** 関数 `CanIf_Init()` が呼ばれたとき、CanIf は、CanIf に割り当てられたすべての送信 L-PDU バッファを初期化しなければならない。

要求 [SWS_CANIF_00387] は、CAN コントローラの再起動後に、古いデータが送信されることを防ぐために必要である。

#### 7.11.3 送信 L-PDU バッファのデータの完全性

**[SWS_CANIF_00033]** CanIf は、送信 L-PDU と送信要求のための送信 L-PDU バッファへの、並行するアクセスから保護しなければならない。（SRS_Can_01114）

これは、BSW スケジューラの中で定義された排他領域を使って実現できる。これらの排他領域は、例えば、排他領域に入っている間、すべての割り込みが無効になるようにコンフィギュレーションできる。BSW スケジューラモジュールの対応するサービスは、`SchM_Enter_CanIf()` と `SchM_Exit_CanIf()` である。

[SWS_CANIF_00033] の根拠: 送信 L-PDU バッファへのプリエンプティブなアクセスは、常に避けられるわけではない。新しい L-PDU の保存や、送信された L-PDU の取り除きなどの、そのような送信 L-PDU バッファへのアクセスは、プリエンプティブに起こりうる。

### 7.12 送信確認

前の送信要求が正常に完了した場合、CanDrv は、`CanIf_TxConfirmation()`（[SWS_CANIF_00007]）の呼び出しで、それを CanIf へ通知する。

**[SWS_CANIF_00383]** コールバック通知 `CanIf_TxConfirmation()` が呼ばれたとき、CanIf は、正常に送信された L-PDU に結び付けられた上位の通信層（[SWS_CANIF_00414] を参照）を識別し、CanIf の送信確認サービス `<User_TxConfirmation>(E_OK)`（7.12 節「送信確認」を参照）の呼び出しで、実行された送信をそれに通知しなければならない。

コールバックサービス `<User_TxConfirmation>()` は、通知される上位層のモジュールにより実装される。

上位の通信層のモジュールは、送信確認を、異なる L-PDU または L-PDU のグループについて、単一または複数のコールバックサービスで処理できるように、設計またはコンフィギュレーションできる。それらのサービスはすべて、対応する L-PDU の送信要求の送信確認時に、CanIf により呼ばれる。送信 L-PDU により、対象の上位層のモジュールに関連付けられた、異なる確認サービスを振り分けられる。この割り当ては、コンフィギュレーションの間に静的に行われる。

1 つの送信 L-PDU は、1 つの送信確認コールバックサービスにだけ割り当てられる。8.6.3.2 項「`<User_TxConfirmation>`」を参照。

**[SWS_CANIF_00740]** `CanIfPublicTxConfirmPollingSupport` が有効な場合、CanIf は、そのコントローラのコントローラモードが `CAN_CS_STARTED` の状態であれば、受信した `TxConfirmation` についての情報を、CAN コントローラごとにバッファリングしなければならない。

### 7.13 受信のデータの流れ

AUTOSAR の基本ソフトウェアのアーキテクチャに従い、受信したデータは、上位の通信スタック（すなわち、AUTOSAR COM、CanNm、CanTp、DCM）で評価され、処理される。これは、上位層のモジュールが、CanDrv（Rx）のバッファを扱うこと（すなわち変更すること）も、CanIf（Tx）のバッファへアクセスすることも、できないことを意味する。

CanIf は、`CANIF_PUBLIC_READRXPDU_DATA_API`（ECUC_CanIf_00607 を参照）が `TRUE` に設定されている場合に限り、受信経路で内部のバッファリングを提供する（7.15 節を参照）。Tx のバッファリングは 7.11 節で、動的な L-PDU は 7.4 節で扱う。

L-PDU を新たに受信した場合、CanDrv は、CanIf の `CanIf_RxIndication()`（[SWS_CANIF_00006] を参照）を呼ぶ。L-PDU 固有のデータへのアクセスは、次のパラメータで構成される。

- ハードウェア受信ハンドル（HRH）
- 受信した CAN 識別子（CanId）
- 受信したデータ長
- 受信した L-PDU への参照

受信した L-PDU は、ハードウェアに依存し（ニブルとバイトの順序、アクセスの種類）、通信システムの最下位の層である CanDrv に割り当てられる。HRH は、CanDrv と、その L-PDU を使う上位層のモジュールの間の、リンクとして働く。HRH は、新しい CAN L-PDU を受信した、1 つの CAN ハードウェア受信オブジェクトを識別する。

CanDrv による、受信した L-PDU の通知の後（`CanIf_RxIndication()` が呼ばれる）、CanIf は、7.14「受信通知」に述べたとおりに進めなければならない。CanIf は、CanDrv が一時的なバッファリングを使うか、直接のハードウェアアクセスを使うかを、認識できない。CanIf は、`CanIf_RxIndication()` の呼び出しで、正規化された L-PDU のデータを期待する。

CAN ハードウェアの受信オブジェクトは、一時的なバッファ、または上位層のモジュールのバッファへのコピーの処理が終わるまで、ロックされる。ハードウェアオブジェクトは、データの喪失を避けるため、CanIf の `CanIf_RxIndication()` が戻った直後に解放される。

CanDrv、CanIf、受信した L-PDU に属する上位層のモジュールは、同じ一時的な中間バッファへアクセスする。これは、CAN コントローラの CAN ハードウェア受信オブジェクトの中、または CanDrv の一時的なバッファのいずれかに置ける。

> （図 7.6「受信のデータの流れ」: フローチャート。訳なし。
> 　読み取れる内容: 受信割り込みの後、データの正規化が必要なら CanDrv の一時的なバッファへ。`CanIf_RxIndication()` を呼ぶ。BasicCAN で受信した Rx L-PDU なら、ソフトウェアフィルタリングを行い、通過した L-PDU だけが続く。データ長の検査が有効なら検査し、失敗した場合は、エラーコード `CANIF_E_INVALID_DATA_LENGTH` で `Det_ReportRuntimeError()` を呼ぶ。通過した場合は、`<User_RxIndication>()` を上位層へ呼び、L-PDU バッファへデータをコピーする。`<User_RxIndication>()` が戻り、`CanIf_RxIndication()` が戻る。）

### 7.14 受信通知

`CanIf_RxIndication()`（[SWS_CANIF_00006] を参照）の呼び出しは、そのパラメータで、新たに受信した CAN L-PDU を参照する。関数 `CanIf_RxIndication()` が呼ばれた場合、CanIf は、CAN L-PDU を、受け付けるかを評価し、上位の通信層による後のアクセスのために、L-SDU を準備する。CanIf は、コンフィギュレーションされていて、この CAN L-PDU が正常に検出され、以降の処理のために受け付けられた場合、`<User_RxIndication>()`（8.6.3.3 項「`<User_RxIndication>`」、[SWS_CANIF_00012] を参照）を使って、この非同期のイベントを、上位層のモジュールへ通知する。この動作の詳細な要求は、以下のとおりである。

**[SWS_CANIF_00389]** 関数 `CanIf_RxIndication()` が呼ばれた場合、CanIf は、コンフィギュレーションされていれば（ECUC_CanIf_00628 の多重度が 0.. に等しい場合）、7.20 で規定されたとおり、受信した L-PDU のソフトウェアフィルタリングを処理しなければならない。ソフトウェアフィルタリングが、受信した L-PDU を拒否した場合、CanIf は、この `CanIf_RxIndication()` の呼び出しについて、受信通知を終えなければならない。

**[SWS_CANIF_00390]** CanIf が、ソフトウェアフィルタリング（[SWS_CANIF_00389] を参照）の間に、`CanIf_RxIndication()` を介して受信した L-PDU を受け付けた場合、CanIf は、コンフィギュレーションされていれば（ECUC_CanIf_00617 を参照）、その後で、データ長の検査を処理しなければならない。

詳細は、7.21 節「データ長の検査」を参照。

**[SWS_CANIF_00297]** CanIf が、データ長の検査（[SWS_CANIF_00390] を参照）の間に、`CanIf_RxIndication()` を介して受信した L-PDU を受け付けた場合、CanIf は、その L-PDU に静的な受信バッファがコンフィギュレーションされていれば（[SWS_CANIF_00198]、ECUC_CanIf_00600 を参照）、コンフィギュレーションされたデータ長（ECUC_CanIf_00599 を参照）に従ったバイト数を、静的な受信バッファへコピーしなければならない。

**[SWS_CANIF_00851]** 受信した L-SDU にメタデータがコンフィギュレーションされている場合、CanIf は、PDU のペイロードを静的な受信バッファへ、CAN ID を型 `CAN_ID_32` の MetaDataItem へ、コピーしなければならない。

**[SWS_CANIF_00056]** CanIf が、データ長の検査（[SWS_CANIF_00390]、[SWS_CANIF_00026] を参照）の間に、`CanIf_RxIndication()` を介して受信した L-PDU を受け付けた場合、CanIf は、受信した L-SDU について、その受信通知サービスで呼ばれるようにコンフィギュレーションされた（[SWS_CANIF_00012] のコンフィギュレーションの説明、ECUC_CanIf_00529、ECUC_CanIf_00530 を参照）、対象の上位層のモジュールがあるかを識別しなければならない。

**[SWS_CANIF_00135]** その受信通知サービスで呼ばれるようにコンフィギュレーションされた、対象の上位層のモジュールがある場合（[SWS_CANIF_00056] を参照）、CanIf は、このコンフィギュレーションされた受信通知のコールバックサービス（ECUC_CanIf_00530 を参照）を呼び、上位層の通知のコールバック関数に必要なパラメータ（[SWS_CANIF_00012] を参照）を、`CanIf_RxIndication()` のパラメータに基づいて渡さなければならない。（SRS_BSW_00325）

注: 1 つの受信 L-PDU は、1 つの受信通知コールバックサービスにだけ割り当てられる（`CANIF_USERRXINDICATION_NAME`、ECUC_CanIf_00530 の多重度を参照）。

概要: CanIf は、`CanIf_RxIndication()` の呼び出し時に、次の手順を行う。

- ソフトウェアフィルタリング（BasicCAN のみ）、コンフィギュレーションされていれば
- データ長の検査、コンフィギュレーションされていれば
- 受信した L-SDU のバッファリング、コンフィギュレーションされていれば
- 上位層の受信通知コールバックサービスの呼び出し、コンフィギュレーションされていれば

### 7.15 受信データの読み取り

受信データの読み取り API `CanIf_ReadRxPduData()`（[SWS_CANIF_00194] を参照）は、上位層のモジュールが、CAN ネットワークから最近受信した CAN L-SDU を読むための、共通のインタフェースである。上位層のモジュールは、CanDrv への直接のアクセスなしに、CanIf のサービスだけを介して、受信要求を開始する。開始された受信要求は、CanIf が、受信した L-SDU を、上位層のモジュールの I-PDU バッファへ書き込んだ場合に、正常に完了する。

関数 `CanIf_ReadRxPduData()` は、受信のイベント（`RxIndication`）に依存せずに、データを読み出せるようにする。これがコンフィギュレーションの時点で有効にされている場合（`CANIF_PUBLIC_READRXPDU_DATA_API`、ECUC_CanIf_00607 を参照）、同じ L-SDU の受信通知サービスは、必ずしもコンフィギュレーションしなくてよい（ECUC_CanIf_00529 を参照）。必要であれば、受信通知も有効にできる。

この方法で、（CanIf の上位層のモジュールで）L-SDU を受信する仕組みの種類を、対応する受信 L-SDU が属する上位層のモジュールの必要に応じて、パラメータ `CANIF_RXPDU_USERRXINDICATION_UL`（ECUC_CanIf_00529 を参照）とパラメータ `CANIF_RXPDU_READ_DATA`（ECUC_CanIf_00600 を参照）で、コンフィギュレーションの時点で選べる。詳細は、9.10 節「受信データの読み取り」を参照。

**[SWS_CANIF_00198]** コンフィギュレーションパラメータ `CANIF_PUBLIC_READRXPDU_DATA_API`（ECUC_CanIf_00607）が `TRUE` に設定されている場合、CanIf は、`CANIF_RXPDU_READDATA`（ECUC_CanIf_00600）が有効な、受信した各 L-SDU を、受信 L-SDU バッファへ保存しなければならない。これは、コンフィギュレーションパラメータ `CANIF_RXPDU_READDATA`（ECUC_CanIf_00600）が `TRUE` に設定されている場合、CanIf が、この受信 L-SDU のために、受信 L-SDU バッファを確保しなければならないことを意味する。

**[SWS_CANIF_00199]** `CanIf_RxIndication()` の呼び出しと、ソフトウェアフィルタリングとデータ長の検査の通過の後、CanIf は、受信した L-SDU を、この受信 L-SDU バッファへ保存しなければならない。最近受信した L-SDU を含む、割り当てられた受信 L-SDU バッファが、`CanIf_ReadRxPduData()` の呼び出しの間にある場合、CanIf は、その受信 L-SDU バッファへの、プリエンプティブな受信 L-SDU バッファのアクセスのイベント（[SWS_CANIF_00064] を参照）を避けなければならない。

### 7.16 Tx/Rx の通知ステータスの読み取り

通知のコールバック関数に加えて、CanIf は、任意の送信 L-SDU の送信確認のステータスを読む API サービス `CanIf_ReadTxNotifStatus()`（[SWS_CANIF_00202] を参照）と、任意の受信 L-SDU の受信通知のステータスを読む API サービス `CanIf_ReadRxNotifStatus()` を提供する。

CanIf の API サービス `CanIf_ReadTxNotifStatus()`（[SWS_CANIF_00202] を参照）と `CanIf_ReadRxNotifStatus()`（[SWS_CANIF_00230] を参照）は、プリコンパイル時のコンフィギュレーションで、コンフィギュレーションパラメータ `CANIF_PUBLIC_READTXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00609）、`CANIF_PUBLIC_READRXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00608）、`CANIF_TXPDU_READ_NOTIFYSTATUS`（ECUC_CanIf_00589）、`CANIF_RXPDU_READ_NOTIFYSTATUS`（ECUC_CanIf_00595）を使って、全体で、または L-SDU ごとに、有効／無効にできる。

**[SWS_CANIF_00472]** コンフィギュレーションパラメータ `CANIF_PUBLIC_READTXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00609）が `TRUE` に設定されている場合、CanIf は、送信 L-SDU ごとに、現在の通知ステータスを保存しなければならない。

**[SWS_CANIF_00473]** コンフィギュレーションパラメータ `CANIF_PUBLIC_READRXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00608）が `TRUE` に設定されている場合、CanIf は、受信 L-SDU ごとに、現在の通知ステータスを保存しなければならない。

[SWS_CANIF_00391] と [SWS_CANIF_00393]、それぞれ [SWS_CANIF_00392] と [SWS_CANIF_00394] の根拠: この「読んで消費する（read-and-consume）」動作は、このサービスの最後の呼び出しの後に、少なくとも 1 回の送信または受信の成功のイベントが起きたことを保証する。

### 7.17 データの完全性

**[SWS_CANIF_00064]** 共有コードは再入可能でなければならない。CanIf は、CanIf のイベント処理の間に変更されうる共有リソースへアクセスする、プリエンプティブなイベントを、互いに対して保護しなければならない。（SRS_BSW_00312）

根拠: 上位層のモジュールのバッファと CanIf の内部バッファのデータを更新する試みは、割り込みサービスルーチンのコンテキストや他のプリエンプティブなイベントで行われた、起こりうる変更を考慮して行う必要がある。プリエンプティブなイベントは、おそらく、プリエンプティブなタスク、（ゲートウェイなどで複数の物理チャネルが使われる場合の）複数の CAN の割り込み、またはネットワークで L-PDU の送受信を必要とする、他の周辺機器やネットワークシステムの割り込みから起こる。

**[SWS_CANIF_00058]** CanIf の環境が、関数 `CanIf_Transmit()`、`CanIf_TxConfirmation()`、`CanIf_ReadRxPduData()` のいずれかの呼び出しにより開始された、CanIf が管理するメモリ領域からデータを読む場合、CanIf は、提供される値が、最も新しく取得された値であることを保証しなければならない。

ヒント: 関数 `CanIf_Transmit()`、`CanIf_TxConfirmation()`、`CanIf_ReadRxPduData()` は、CanIf が送信バッファまたは受信バッファを使うようにコンフィギュレーションされている場合に限り、CanIf が管理するメモリ領域のデータにアクセスする。

共有の送信・受信の L-PDU/L-SDU バッファの扱いは、CanIf の実装にとって重要な課題である。したがって、CanIf は、データの完全性を保証し、送信／受信の L-PDU/L-SDU バッファなどの共有リソースへのアクセスに、適切な仕組みを使わなければならない。プリエンプティブなイベント、すなわち、他の CAN コントローラからの送信と受信のイベントは、同じ L-PDU/L-SDU バッファへ書き込むことで、データの完全性を損ないうる。

CanIf は、例えば、CanDrv のサービスを使って、重要な区間の入口と出口で、CAN コントローラごとに別々に、CAN の割り込みとその通知を有効（`Can_EnableControllerInterrupts()`）・無効（`Can_DisableControllerInterrupts()`）にできる。複数の CAN コントローラの共通のリソースがある場合、CAN の割り込み全体をロックしなければならない。これらの区間は、重大な性能の低下を防ぐため、長くかかってはならない。したがって、データのコピー、静的変数、カウンタ、セマフォの変更は、これらの重要な区間の内側で行われるべきである。データの完全性、割り込み可能性、再入可能性を保証するために、適切な仕組みを使うかどうかは、実装に任される。

送信要求 API `CanIf_Transmit()` は、異なるプリエンプティブなイベントによる、異なる L-PDU/L-SDU の複数の送信要求の呼び出しを可能にするため、再入可能に動作できなければならない。CanDrv の送信要求 API `Can_Write()` も、再入可能に動作する。

### 7.18 CAN コントローラのモード

#### 7.18.1 一般的な機能

CanIf は、基盤の CanDrv が表す、サポートされるすべての CAN コントローラの、通信モードを制御するサービスを提供する。これは、すべての CAN コントローラが、現在のコントローラモードを要求し、読むための、対応する提供される API サービスにより、制御されることを意味する。

CAN コントローラの状態は、上位層の要求により、サービス `CanIf_SetControllerMode()` の呼び出しで変更されうる。要求は、CanIf により、CanDrv の API を介して、対象の CAN コントローラへ渡される。

1 つの CAN ネットワークに接続されたすべての CAN コントローラの一貫した管理は、CanSm のタスクである。この方法で、CanSm は、1 つの CAN ネットワークのすべての CAN コントローラを、順番にスリープモードへ設定するか、起こす責任を持つ。

CanIf は、関数 `CanIf_SetControllerMode()` または `CanIf_ControllerBusOff()` を呼ぶことによる、すべての状態遷移の要求を受け付ける。CanIf は、要求された CAN コントローラのモード遷移が有効かどうかを決めない。CanIf は、現在のモードを取得し、要求されたモード遷移を実行することで、CanDrv とやり取りするだけである。

このネットワークに関する状態機械は、CanSm に実装されている。[3] を参照。CanIf は、要求されたモードを保存し、要求された遷移を実行するだけである。

ヒント: 内部での使用のために CanDrv への頻繁な要求を避ける最適化として、`CanIf_ControllerModeIndication()` と `Can_GetControllerMode()` が示した最後の状態を、コントローラごとに保存してもよい。

ヒント: CAN コントローラのモードの変更を要求できるのは、CanSm だけではないことに注意する必要がある。

#### 7.18.2 CAN コントローラの動作モード

CanSm により要求された動作モードに従い、CanIf は、要求を CanDrv へ転送する。

**[SWS_CANIF_00677]** `ControllerId` で参照されるコントローラモードが `CAN_CS_STOPPED` の状態で、`CanIf_Transmit()` の呼び出しの `PduIdType` パラメータが、その CAN コントローラに割り当てられている場合、`CanIf_Transmit()` の呼び出しは、`Can_Write()` の呼び出し（[SWS_CANIF_00317] を参照）を引き起こさず、`E_NOT_OK` を返す。

**[SWS_CANIF_00485]** `ControllerId` で参照されるコントローラモードが `CAN_CS_STOPPED` の状態に入った場合、CanIf は、対応する CAN コントローラに割り当てられた CanIf の送信バッファをクリアしなければならない。

**[SWS_CANIF_00739]** `ControllerId` で参照されるコントローラモードが `CAN_CS_STOPPED` の状態に入った場合、CanIf は、その CAN コントローラに割り当てられた、未処理の `TxConfirmation` のそれぞれについて、`<User_TxConfirmation>(id, E_NOT_OK)` を呼んで、対応する上位層のモジュールへ、送信の失敗を知らせなければならない。`CanIfPublicTxConfirmPollingSupport` が有効な場合、CanIf は、`TxConfirmation` についての情報もクリアしなければならない（[SWS_CANIF_00740] を参照）。

注: これにより、`CanIf_Transmit()` で送信される各 PDU について、肯定または否定の `<User_TxConfirmation>()` のどちらかが、呼ばれることが保証される。

**[SWS_CANIF_00724]** コールバック `CanIf_ControllerBusOff(ControllerId)` が呼ばれたとき、CanIf は、CanSm の `CanSM_ControllerBusOff(ControllerId)`（8.6.3.9 項を参照）、または CDD（[SWS_CANIF_00559]、[SWS_CANIF_00560] を参照）を呼ばなければならない。

**[SWS_CANIF_00711]** コールバック `CanIf_ControllerModeIndication(ControllerId, ControllerMode)` が呼ばれたとき、CanIf は、CanSm の `CanSM_ControllerModeIndication(ControllerId, ControllerMode)`（8.6.3.9 項「`<User_ControllerModeIndication>`」を参照）、または CDD（[SWS_CANIF_00691]、[SWS_CANIF_00692] を参照）を呼ばなければならない。

**[SWS_CANIF_00712]** コールバック `CanIf_TrcvModeIndication(Transceiver, TransceiverMode)` が呼ばれたとき、CanIf は、CanSm の `CanSM_TransceiverModeIndication(TransceiverId, TransceiverMode)`（8.6.3.9 項「`<User_ControllerModeIndication>`」を参照）、または CDD（[SWS_CANIF_00697]、[SWS_CANIF_00698] を参照）を呼ばなければならない。

#### 7.18.3 コントローラモードの遷移

CAN コントローラへの状態変更要求の API は、コールバックサービスによる非同期の通知を伴う、非同期の動作をする。

要求されたモードへの実際の遷移は、CAN コントローラのハードウェアでの遷移要求の設定（例: スリープ遷移 `CAN_CS_SLEEP` の要求）に基づいて、非同期に起こる。例えば、`CAN_CS_SLEEP` モードへの変更が成功した後、CanDrv は、関数 `CanIf_ControllerModeIndication()` を呼び、それを受けて CanIf は、関数 `<User_ControllerModeIndication>()` を呼ぶ。CAN の遷移が非常に速い場合、`CanIf_ControllerModeIndication()` は、`CanIf_SetControllerMode()` の間に呼ばれうる。これは実装固有である。

CAN コントローラのモード遷移の失敗や、遷移が起こらないことは、上位層のモジュールが追跡しなければならない。モード遷移 `CAN_CS_STARTED` と `CAN_CS_STOPPED` も、同様に扱われる。

CanIf の上位層のモジュールは、`CanIf_GetControllerMode()` で、現在のコントローラモードをポーリングできる。

すべての種類の CAN コントローラが、スリープとウェイクアップのモードをサポートするわけではない。その場合、これらのモードは、CanDrv が、そのインタフェースを介して、ハードウェアに依存しない動作モードを提供することで隠され、それを CanIf が管理しなければならない。

注: `CAN_CS_STOPPED` から `CAN_CS_SLEEP` への遷移の間に、CAN コントローラが、ECU 統合コードへウェイクアップ割り込みを通知することがありうる。

CanIf は、内部で開始された CAN コントローラのウェイクアップ要求（内部要求）と、ネットワークのウェイクアップ要求（外部要求）を区別する。内部要求は、CanIf の関数 `CanIf_SetControllerMode(ControllerId, CAN_CS_STARTED)` の呼び出しで開始され、内部の非同期の要求である。外部要求は、CanDrv または CanTrcv から ECU 統合コードへ通知される、CAN コントローラのイベントである。詳細は、文書 [13] の「CAN ウェイクアップのシーケンス」の章の、それぞれの UML 図を参照。

#### 7.18.4 ウェイクアップ

ECU は、使われるウェイクアップの方法（CAN コントローラまたは CAN トランシーバを直接使う）に関係なく、CAN コントローラと CAN トランシーバが、ある種の「ウェイクアップを待ち受ける」モードに設定されている場合に限り、CAN ネットワークによるウェイクアップをサポートする。これは通常、通常の通信が無効になる、スリープモードである。このモードだけが、CAN コントローラが停止していることを保証する。したがって、ウェイクアップ割り込みを有効にできる。

##### 7.18.4.1 ウェイクアップの検出

ウェイクアップのサポートが有効な場合（[SWS_CANIF_00180] を参照）、CanIf は、検出された CAN のウェイクアップを、サービス `CanIf_CheckWakeup()` により、統合コードから通知される（[13] の CAN ウェイクアップのシーケンスを参照）。

CAN バスの「ウェイクアップ」のイベントの場合、関数 `CanIf_CheckWakeup(WakeupSource)` は、`EcuM_CheckWakeup(WakeupSource)` の実行中に呼ばれうる（EcuM のウェイクアップのシーケンス図を参照）。CanIf は、その後、CanDrv の `EcuMWakeupSource` へのコンフィギュレーションされた入力の参照により、どの CanDrv を検査すべきかを調べる。CanIf は、この情報を、参照 `CanIfCtrlCanCtrlRef`（ECUC_CanIf_00636 を参照）から得る。

呼ばれる通信サービスは、コンフィギュレーションの間に定義されたサービスに属する（ECUC_CanIf_00250 を参照）。この方法で、EcuM も CanSm も、CAN コントローラの状態を変え、バスオフからの回復やウェイクアップの手順に関するシステムの動作を制御できる。

**[SWS_CANIF_00395]** `CanIf_CheckWakeup(EcuM_WakeupSourceType WakeupSource)` が呼ばれたとき、CanIf は、どの正確な CAN ハードウェアデバイスがバスのウェイクアップを引き起こしたかを、`CanTrcv_CheckWakeup()` または `Can_CheckWakeup()` を介して、CanDrv / CanTrcv に問い合わせなければならない。

注: どのコントローラとトランシーバに問い合わせるかは、実装固有である。CanIf は、正確な CAN ハードウェアデバイスを突き止めればよい。

**[SWS_CANIF_00720]** `Can_CheckWakeup()` または `CanTrcv_CheckWakeup()` の関数呼び出しの少なくとも 1 つが、CanIf へ `E_OK` を返す場合、`CanIf_CheckWakeup()` は `E_OK` を返さなければならない。

**[SWS_CANIF_00678]** `Can_CheckWakeup()` または `CanTrcv_CheckWakeup()` のすべての呼び出しが、CanIf へ `E_NOT_OK` を返す場合、`CanIf_CheckWakeup()` は `E_NOT_OK` を返さなければならない。

##### 7.18.4.2 ウェイクアップの検証

注: CAN コントローラ／CAN トランシーバが、バスのウェイクアップのイベントを検出したとき、これは、ECU State Manager へ直接通知される。このようなウェイクアップのイベントを検証する必要がある場合、EcuM（または CDD）は、対応する CAN コントローラ（`CanIf_SetControllerMode()`）と CAN トランシーバ（`CanIf_SetTrcvMode()`）を ON にする（詳細は [13] の第 9 章を参照）。

注意: CanIf は、PDU チャネルモードが `CANIF_ONLINE` または `CANIF_TX_OFFLINE` に設定された後に、受信したメッセージについて、上位層のモジュールへ通知する。したがって、ウェイクアップの検証が必要な場合、PDU チャネルモードを `CANIF_ONLINE` または `CANIF_TX_OFFLINE` に設定してはならない。

注: [SWS_CAN_00411] と CAN コントローラの状態図（[1] を参照）に従い、モード `CAN_CS_SLEEP` から `CAN_CS_STARTED` への直接の遷移は許されない。

**[SWS_CANIF_00226]** CanIf は、次の場合に限り、ウェイクアップサービス `CanIf_CheckValidation()` を提供しなければならない。

- 基盤の CAN コントローラがウェイクアップのサポートを提供し、ウェイクアップが、パラメータ `CANIF_CTRL_WAKEUP_SUPPORT`（ECUC_CanIf_00637 を参照）と CanDrv のコンフィギュレーションにより有効にされている、
- かつ／または、基盤の CAN トランシーバがウェイクアップのサポートを提供し、ウェイクアップが、パラメータ `CANIF_TRCV_WAKEUP_SUPPORT`（ECUC_CanIf_00606 を参照）と CanTrcv のコンフィギュレーションにより有効にされている、
- かつ、コンフィギュレーションパラメータ `CANIF_PUBLIC_WAKEUP_CHECK_VALIDATION_SUPPORT`（ECUC_CanIf_00611 を参照）が有効である。

**[SWS_CANIF_00286]** `CanIfPublicWakeupCheckValidSupport` が `TRUE` に等しい場合、CanIf は、CAN のウェイクアップの検証のための検出を有効にする。そのため、CanIf は、`CAN_CS_STARTED` に設定された CAN コントローラの、`CanIf_RxIndication()` の最初の有効な呼び出しのイベントを保存する。`CanIf_RxIndication()` の最初の呼び出しは、次の場合に有効である。

- `CanIfPublicWakeupCheckValidByNM` が `TRUE` の場合は、受信した NM メッセージの場合だけ
- `CanIfPublicWakeupCheckValidByNM` が `FALSE` の場合は、コンフィギュレーションされた Rx PDU に対応する、すべての受信したメッセージ

（SRS_Can_01151）

**[SWS_CANIF_00179]** `<User_ValidateWakeupEvent>(sources)` は、最初に呼ばれた `CanIf_RxIndication()` のイベントが、対応する CAN コントローラで CanIf に保存されている場合、`CanIf_CheckValidation(WakeupSource)` の間に呼ばれなければならない。ここで、`sources` は `WakeupSource` に設定される。（SRS_Can_01136）

注: CanIf にウェイクアップのイベントが保存されていない場合、`CanIf_CheckValidation()` は、`<User_ValidateWakeupEvent>()` を呼ぶべきではない。

注: 関数 `<User_ValidateWakeupEvent>()` のパラメータの型は、次のとおりである。

- `sources`: `EcuM_WakeupSourceType`（[13] を参照）

**[SWS_CANIF_00756]** コントローラモードが `CAN_CS_SLEEP` に設定されたとき、以前のウェイクアップ（`CanIf_RxIndication` の最初の呼び出し）の保存されたイベントは、クリアされなければならない（[SWS_CANIF_00179] を参照）。

### 7.19 PDU チャネルのモード制御

#### 7.19.1 PDU チャネルグループ

各 L-PDU は、1 つの CAN コントローラと 1 つの CAN ネットワークに接続された、1 つの専用の物理 CAN チャネルに割り当てられる。この方法で、1 つの物理チャネルに属するすべての L-PDU を、論理的に単一の L-PDU チャネルグループの扱いの見え方で、制御できる。これらの論理グループは、1 つの基盤の CAN ネットワークに接続された、1 つの ECU のすべての L-PDU を表す。

下の図 7.7 は、L-PDU チャネルグループの 1 つの使い方と、上位層および（または）ネットワークとの関係を示す。L-PDU は、1 つのチャネルグループにだけ割り当てられる。

PduR やネットワークマネジメントなどの典型的なユーザは、PDU の動作モードの制御に責任を持つ。

> （図 7.7「チャネル PDU グループ」: 図。訳なし。）

#### 7.19.2 PDU チャネルモード

CanIf は、次の処理を防ぐために、サービス `CanIf_SetPduMode()` と `CanIf_GetPduMode()` を提供する。

- 1 つの論理チャネルに属する、すべての送信 L-PDU
- 1 つの論理チャネルに属する、すべての送信 L-PDU と受信 L-PDU

PDU チャネルモードの変更は、対応するコントローラモードが `CAN_CS_STARTED` に等しい場合にだけ許される（[SWS_CANIF_00874] を参照）。

`CANIF_ONLINE` と `CANIF_OFFLINE` は通信全体に影響するのに対し、PDU チャネルモード `CANIF_TX_OFFLINE` と `CANIF_TX_OFFLINE_ACTIVE` は、送信の経路を別々に有効／無効にする。

CanIf は、サービス `CanIf_GetPduMode()` を介して、現在の PDU チャネルモードについての情報を提供する。

> （図 7.8「PDU チャネルモードの制御」: 状態図。訳なし。）

図 7.8 は、取りうる PDU チャネルモードの図を示す。各 L-PDU チャネルは、`CANIF_OFFLINE`（通信なし）、`CANIF_TX_OFFLINE`（受動モード ＝ 送信せずに受信待ち）、`CANIF_TX_OFFLINE_ACTIVE`（受信待ちなしの、送信の模擬。[SWS_CANIF_00072] を参照）、`CANIF_ONLINE`（完全な通信）のいずれかの状態にありうる。デフォルトの状態は `CANIF_OFFLINE` モードである。

##### 7.19.2.1 CANIF_OFFLINE

**[SWS_CANIF_00864]** 初期化の間に、CanIf は、すべてのチャネルを `CANIF_OFFLINE` へ切り替えなければならない。

**[SWS_CANIF_00865]** `CanIf_SetControllerMode(ControllerId, CAN_CS_SLEEP)` が呼ばれた場合、CanIf は、対応するチャネルの PDU チャネルモードを `CANIF_OFFLINE` に設定しなければならない。

**[SWS_CANIF_00073]** `CANIF_OFFLINE` モードへ切り替わる物理チャネルについて、CanIf は、次のようにしなければならない。

- 関連する L-PDU の送信要求 `CanIf_Transmit()` の、CanDrv への転送を防ぐ（呼び出した上位層のモジュールへ `E_NOT_OK` を返す）。
- 対応する CanIf の送信バッファをクリアする。
- 上位層のモジュールの受信通知のコールバックサービスの呼び出しを防ぐ。
- 上位層のモジュールの送信確認のコールバックサービスの呼び出しを防ぐ。

**[SWS_CANIF_00866]** `CanIf_SetControllerMode(ControllerId, CAN_CS_STOPPED)` または `CanIf_ControllerBusOff(ControllerId)` が呼ばれた場合、CanIf は、対応するチャネルの PDU チャネルモードを `CANIF_TX_OFFLINE` に設定しなければならない。

**[SWS_CANIF_00489]** `CANIF_TX_OFFLINE` モードへ切り替わる物理チャネルについて、CanIf は、次のようにしなければならない。

- 関連する L-PDU の送信要求 `CanIf_Transmit()` の、CanDrv への転送を防ぐ（呼び出した上位層のモジュールへ `E_NOT_OK` を返す）。
- 対応する CanIf の送信バッファをクリアする。
- 上位層のモジュールの送信確認のコールバックサービスの呼び出しを防ぐ。
- 上位層のモジュールの受信通知のコールバックサービスの呼び出しを有効にする。

バスオフの通知は、`CANIF_OFFLINE` と `CANIF_TX_OFFLINE` の場合、暗黙のうちに抑制される。これは、L-PDU を送信できず、したがって、新たに送信を要求された L-PDU により、CAN コントローラがバスオフモードに入れないためである。

**[SWS_CANIF_00118]** CAN 送信ハードウェアオブジェクトで、すでに送信を待っている送信 L-PDU が、`CANIF_TX_OFFLINE` または `CANIF_OFFLINE` モードへの変更の後、直ちに送信され、その後にバスオフのイベントが起きた場合、CanIf は、バスオフの通知 `<User_ControllerBusOff>(ControllerId)` の実行を禁止してはならない。

ウェイクアップの通知は、PDU チャネルモードの変更に関して影響を受けない。

##### 7.19.2.2 CANIF_ONLINE

**[SWS_CANIF_00075]** `CANIF_ONLINE` モードへ切り替わる物理チャネルについて、CanIf は、次のようにしなければならない。

- 関連する L-PDU の送信要求 `CanIf_Transmit()` の、CanDrv への転送を有効にする。
- 上位層のモジュールの受信通知のコールバックサービスの呼び出しを有効にする。
- 上位層のモジュールの送信確認のコールバックサービスの呼び出しを有効にする。

##### 7.19.2.3 CANIF_OFFLINE_ACTIVE

`CanIfTxOfflineActiveSupport = TRUE` の場合、CanIf は、`CANIF_TX_OFFLINE_ACTIVE` モードにより、送信成功の模擬を提供する。このモードは、`CanIf_SetPduMode(ControllerId, CANIF_TX_OFFLINE_ACTIVE)` の呼び出しで有効になり、送信の経路にだけ影響する。

**[SWS_CANIF_00072]** `CANIF_TX_OFFLINE_ACTIVE` モードのチャネルに割り当てられたすべての L-PDU について、CanIf は、`CanIf_Transmit()` の呼び出しの間に、L-PDU のバッファリングや CanDrv への転送の代わりに、上位層のモジュールの送信確認のコールバックサービスを、直ちに呼ばなければならない。

注: `CANIF_TX_OFFLINE_ACTIVE` モードの間は、上位層が、送信確認の実行を扱わなければならない。送信確認の扱いは、送信要求の終わりに直ちに実行される（[SWS_CANIF_00072] を参照）。

根拠: この機能は、特別な動作モード（例: 診断の受動モード）を実現するために、通知の仕組みに影響を与えずに、バスのトラフィックを避けるのに役立つ。このモードは、通常、診断の用途に使われる。

### 7.20 ソフトウェア受信フィルタ

BasicCAN のハードウェアオブジェクトで、ハードウェアのアクセプタンスフィルタを通過し、したがって正常に受信されうる L-PDU のすべてが、受信 L-PDU として定義され、対応する ECU で必要とされるわけではない。CanIf は、オプションで、これらの L-PDU を除外し、以降のソフトウェア処理を禁止する。

特定のソフトウェアフィルタのアルゴリズムは、ソフトウェアフィルタの実行時間を最適化するために提供される。ソフトウェアフィルタの仕組みのアプローチは、現在処理中の HRH と CanId から、対応する L-PDU を見つけ出すことである。L-PDU が見つかった後、CanIf は、受信を受け付け、上位層が L-SDU の情報へ直接アクセスできるようにする。

#### 7.20.1 ソフトウェアフィルタリングの概念

コンフィギュレーションツールは、ハードウェアのアクセプタンスフィルタの設定についての情報を扱う。最も重要な設定は、L-PDU のハードウェアオブジェクトの数と、その範囲である。出口の範囲が、どの受信 L-PDU が各ハードウェア受信オブジェクトに属するかを定義する。次の定義が可能である。

- 単一の受信 L-PDU（FullCAN の受信）
- 受信 L-PDU の一覧、または
- 受信 L-PDU の 1 つまたは複数の範囲を、ハードウェア受信オブジェクトに結び付けられる（BasicCAN の受信）

範囲の受信を定義するには、CanId または ID の範囲全体が、定義された範囲の内側にある Rx L-PDU を、少なくとも 1 つ定義する必要がある。

**[SWS_CANIF_00645]** ソフトウェア受信フィルタを通過させる CanId の範囲は、その上限（`CANIF_HRHRANGE_UPPER_CANID`、ECUC_CanIf_00630 を参照）と下限（`CANIF_HRHRANGE_LOWER_CANID`、ECUC_CanIf_00629 を参照）の CanId、または、ベース ID（`CANIF_HRHRANGE_BASEID` を参照）とベース ID の関連するビットを定義するマスク（`CANIF_HRHRANGE_MASK` を参照）のいずれかで、定義されなければならない。

注: ソフトウェア受信フィルタリングはオプションである（ECUC_CanIf_00628 の 0.. の多重度を参照）。

**[SWS_CANIF_00646]** ソフトウェア受信フィルタを通過させる、コンフィギュレーションできる各 CanId の範囲（[SWS_CANIF_00645] を参照）は、`CANIF_HRHRANGE_CANIDTYPE`（ECUC_CanIf_00644 を参照）を介して、標準の CAN ID または拡張の CAN ID のいずれかについて、コンフィギュレーションできなければならない。

受信 L-PDU は、通信マトリクスから静的に生成された定数の構造体として提供される。それらは、対応するハードウェアのアクセプタンスフィルタに従って並べられ、ハードウェア受信オブジェクト（HRH）ごとに、受信 CanId の単一の一覧が 1 つあるようになっている。複数の BasicCAN オブジェクトが使われる場合、対応する一覧は、HRH から導ける。その後のフィルタリングは、複数の CanId の 1 つの一覧を探索し、新しく受信した CanId と比較することである。一致した場合、受信 L-PDU は、見つかった CanId から導かれる。

**[SWS_CANIF_00030]** HRH で受信した L-PDU の CanId が、受信するようにコンフィギュレーションされている場合、CanIf は、この L-PDU を受け付けなければならず、ソフトウェアフィルタリングのアルゴリズムは、見つかった CanId から、対応する受信 L-PDU を導かなければならない。（SRS_Can_01018）

> （図 7.9「ソフトウェアフィルタリングの例」: 図。訳なし。）

**[SWS_CANIF_00852]** 範囲が別の範囲に（部分的に）含まれる場合、または単一の CanId が範囲に含まれる場合、ソフトウェアフィルタは、次の前提に基づいて L-PDU を選ばなければならない。

- 単一の CanId は、常に範囲より関連性が高い。
- 小さい範囲は、大きい範囲より関連性が高い。

#### 7.20.2 ソフトウェアフィルタのアルゴリズム

適切なソフトウェア探索アルゴリズムの選択は、CanIf の実装に任される。CAN コントローラが提供する、BasicCAN の受信の動作の幅広い可能性に応じて、ほとんどの使用例に最適な解決策を提供するために、線形探索、テーブル探索、ハッシュ探索の変種など、複数の探索アルゴリズムを提供することが推奨される。

### 7.21 データ長の検査

受信したデータ長の値は、受信した L-PDU のコンフィギュレーションされたデータ長の値と比較される。コンフィギュレーションされたデータ長の値は、この L-PDU の中の、使用されるバイトのサイズから導かれなければならない。コンフィギュレーションされたデータ長の値は、CAN の通信マトリクスで定義され、この CAN L-PDU の送信側が使うデータ長の値であるとは限らない。

**[SWS_CANIF_00026]** CanIf は、データ長の値が、コンフィギュレーションされたデータ長の値（ECUC_CanIf_00599 を参照）以上である、受信したすべての L-PDU（[SWS_CANIF_00390] を参照）を、受け付けなければならない。（SRS_Can_01005）

ヒント: データ長の検査は、CanIf のコンフィギュレーション（`CanIfPrivateDataLengthCheck` を参照）で、使われるすべての CanDrv について、全体で有効・無効にできる。

**[SWS_CANIF_00168]** データ長の検査が、受信した L-PDU を拒否した場合（[SWS_CANIF_00026] を参照）、CanIf は、ランタイムエラーコード `CANIF_E_INVALID_DATA_LENGTH` を DET モジュールの `Det_ReportRuntimeError()` サービスへ報告しなければならない。

**[SWS_CANIF_00829]** データ長の検査を通過した場合、CanIf は、受信した（[SWS_CANIF_00006] を参照）長さの値を、対象の上位層のモジュールへ渡さなければならない（[SWS_CANIF_00135] を参照）。

**[SWS_CANIF_00830]** データ長の検査がコンフィギュレーションされていない場合（ECUC_CanIf_00617 を参照）、CanIf は、受信した（[SWS_CANIF_00006] を参照）長さの値を、対象の上位層のモジュールへ渡さなければならない（[SWS_CANIF_00135] を参照）。

### 7.22 上位層への L-SDU の振り分け

根拠: 送信側では、L-SDU の振り分けは、対象の上位層のモジュールの、対応する Tx 確認のコールバックサービスを見つけなければならない。受信側では、各 L-SDU が、宛先として 1 つの上位層のモジュールに属する。この関係は、コンフィギュレーションの時点で静的に割り当てられる。CanIf の中の L-SDU の振り分けの役割は、受信した L-SDU の受け取り先を見つけ、見つけた上位層へ通知を振り分けることである。これらの送信確認と受信通知の通知サービスは、通知される上位層のモジュールで定義された、異なる名前で、複数存在しうる。それらの通知サービスは、サービスを提供する必要のある層に応じて、静的にコンフィギュレーションされる。

### 7.23 ポーリングモード

ポーリングモードは、ハードウェア割り込みを使わずに、CAN ハードウェアで起きた、送信、受信、エラーのイベントの処理を提供する。したがって、CanIf と CanDrv は、対応するハードウェアのイベントを検出し、実行するための通知サービスを提供する。ポーリングモードでは、これらの CanIf の通知サービスの動作は変わらない。この方法で、上位層のモジュールは、ハードウェアのイベントを検出する方式から抽象化される。異なる CanDrv が使われる場合、呼び出しの頻度は、コンフィギュレーションの設定とシステムの統合の間に調和させなければならない。

これらの通知サービスは、CAN ハードウェアオブジェクトで、前回の実行以降に起きた新しいイベントを検出できる。CanDrv が検出したイベントを転送する CanIf の通知サービスは、割り込み動作の場合と同じである（8.4 節「コールバック通知」を参照）。

ユーザは、CanIf が、割り込みで起動された通知サービスを割り込みレベルで実行できるとともに、呼び出された通知サービスをタスクレベルで実行できなければならないことを、考慮しなければならない。CAN コントローラのメールボックスへのアクセスがブロックされている場合、後続の送信バッファリングが行われる（7.11 節「送信バッファリング」を参照）。

ポーリングモードと割り込みモードは、基盤の CAN コントローラごとにコンフィギュレーションできる。

### 7.24 複数の CAN ドライバのサポート

CanIf は、上位層へ共通のインタフェースを提供するために、複数の CanDrv を扱う特定の対応付けを必要とする。したがって、CanIf は、すべての動作を、上から下へ、対応する CanDrv と基盤の CAN コントローラの API へ振り分けなければならない。下から上への経路では、CanIf は、複数の CanDrv を区別するために、適切なコールバック通知を提供しなければならない。

各 CanDrv は、一定数の基盤の CAN コントローラと、固定数の HTH/HRH をサポートする。各 CanDrv は、独自の番号領域を持ち、CAN コントローラと HTH では、常にゼロから始まる。CanIf は、API で渡された L-SDU から、対応する CanDrv を導かなければならない。パラメータは、それに応じて変換されなければならない。すなわち、L-SDU ⇒ HTH/HRH、CanId、データ長である。

複数の CanDrv のサポートは、コンフィギュレーションパラメータ `CanIfPublicMultipleDrvSupport` で、有効・無効にできる。

#### 7.24.1 複数の CAN ドライバを使った送信要求

各送信 L-PDU により、CanIf は、対応する CAN コントローラ、したがって、影響を受けるハードウェアユニットにサービスを提供する CanDrv を、暗黙のうちに導ける。これらの依存関係を解決できるのは、CAN コントローラハンドルの構造のためである。これは、CanDrv ハンドルと、ハードウェアユニットの対応する CAN コントローラを組み合わせたものである。

コンフィギュレーションの時点で、CAN コントローラハンドルが、各 CAN コントローラへ対応付けられる。下の図 7.10 のシーケンス図は、異なる CanDrv へ向けられた 2 つの送信要求を示す。CanIf は、正しい API サービスを呼ぶために、対応する CanDrv を選ぶだけでよい。

注: 図 7.10 と次の表は、例としてだけ使われる。最終的に、基盤の CanDrv の正しい API へアクセスすることは、実装に任される。

> （図 7.10「複数の CAN ドライバでの送信要求（簡略化）」: シーケンス図。訳なし。読み取れる内容: CAN コントローラ A を使う場合、`CanIf_Transmit()` → CanDrv A の `Can_Write()` → CAN ハードウェア A へ L-PDU をコピー。CAN コントローラ B を使う場合は、CanDrv B の `Can_Write()` → CAN ハードウェア B へコピー。）

| 呼ばれる操作 | 説明 |
|---|---|
| `CanIf_Transmit(PduId_1, PduInfoPtr_1)` | 上位層が送信要求を開始する。PduId は、要求された CAN コントローラを追跡し、次にハードウェアユニットにサービスを提供するために使われる。 |
| `Can_Write(Hth, PduInfoPtr)`、ハードウェアへの要求 | ハードウェアユニットの番号は、関数へのポインタの配列の添字として使われるため、振り分けに関係する。最初に、PDU チャネルグループの番号が `PduId_1` から取り出される。各 PDU チャネルグループは、CAN チャネル、したがって、ハードウェアユニット番号と CAN コントローラ番号も参照する。ハードウェアユニット番号は、CanDrv のインスタンスを指し、したがって、使われるハードウェアユニットのためにコンフィギュレーションされたすべての API サービスを参照する。これらのサービスの 1 つが、要求された送信サービスである。対応する CAN ドライバ（例: 「A」ハードウェアユニットの CAN コントローラ #0 にサービスを提供するもの）への送信の要求。すべての L-PDU のデータは、例えば、ハードウェアユニット「A」の CAN コントローラ #0 のハードウェアに設定され、送信要求が有効になる。 |
| `CanIf_Transmit(PduId_2, PduInfoPtr_2)` | 上位層が送信要求を開始する。PduId は、別の CAN コントローラ、次に別のハードウェアユニットへつながる。 |
| `Can_Write(Hth, PduInfoPtr_2)`、ハードウェアへの要求 | 上記と同様。対応する CAN ドライバ（例: 「B」ハードウェアユニットの CAN コントローラ #1 にサービスを提供するもの）への送信の要求。すべての L-PDU のデータは、例えば、ハードウェアユニット「B」の CAN コントローラ #1 のハードウェアに設定され、送信要求が有効になる。 |

#### 7.24.2 複数の CAN ドライバを使った通知の仕組み

1 つの ECU で複数の CanDrv が使われる場合でも、CanDrv が CanIf で呼ぶ通知のコールバックサービスは、それぞれ 1 つだけ存在する。これは、CanIf が、渡されたパラメータを使って、呼び出した CanDrv を識別しなければならないことを意味する。CanIf は、メールボックス（`Can_HwType`）構造体の中の `ControllerId` から、呼び出した CanDrv を識別する。

> （図 7.11「複数の CanDrv での受信割り込み（簡略化）」: シーケンス図。訳なし。）

| 呼ばれる操作 | 説明 |
|---|---|
| 受信割り込み | CAN コントローラ 1 が受信の成功を通知し、受信割り込みを起こす。CanDrv A の ISR が呼ばれる。 |
| `CanIf_RxIndication(Mailbox_1, PduInfoPtr_1)` | 受信が、`CanIf_RxIndication()` の呼び出しで、CanIf へ通知される。ポインタ `Mailbox_1` は、`PduInfoPtr_1` で指定された、受信した L-PDU を含む HRH と、対応する CAN コントローラを識別する。 |
| 検証の検査（ソフトウェアフィルタリング、データ長の検査） | ソフトウェアフィルタリングは、受信した L-PDU が、ローカルの ECU で処理されるかを検査する。処理されない場合、受信した L-SDU は上位層へ通知されず、以降の処理は抑制される。L-PDU が見つかった場合、受信した L-PDU のデータ長は、受信した L-PDU について、静的にコンフィギュレーションされた期待値と比較される。 |
| `<User_RxIndication>(CanRxPduId_1, CanPduInfoPtr_1)` | 上位層の対応する受信通知サービスが呼ばれる。これは、対象の上位層へ、受信の成功を知らせる。パラメータ `CanRxPduId_1` は、受信した L-SDU の ID を指定する。2 つ目のパラメータは、L-SDU を含むバッファへのアクセスを提供する、`PduInfoType` への参照である。 |
| 受信割り込み | CAN コントローラ 2 が受信の成功を通知し、受信割り込みを起こす。CanDrv B の ISR が呼ばれる。 |
| `CanIf_RxIndication(Mailbox_2, PduInfoPtr_2)` | 上記と同様（`Mailbox_2` と `PduInfoPtr_2` について）。 |
| 検証の検査 | 上記と同様。 |
| `<User_RxIndication>(CanRxPduId_2, CanPduInfoPtr_2)` | 上記と同様（`CanRxPduId_2` と `CanPduInfoPtr_2` について）。 |

### 7.25 パーシャルネットワーキング

**[SWS_CANIF_00747]** パーシャルネットワーキング（PN）が有効な場合（`CANIF_PUBLIC_PN_SUPPORT`、ECUC_CanIf_00772 を参照）、CanIf は、PDU チャネルモードに重ねて働く `PnTxFilter` を、CAN コントローラごとにサポートしなければならない。

**[SWS_CANIF_00748]** [SWS_CANIF_00747] の `PnTxFilter` は、CAN コントローラごとに 0 個より多くの Tx L-PDU が `CanIfTxPduPnFilterPdu` としてコンフィギュレーションされている場合に限り（`CANIF_TXPDU_PNFILTERPDU`、ECUC_CanIf_00773 を参照）、効果を持ち、そのモード（有効／無効）を遷移させなければならない。

**[SWS_CANIF_00863]** `PnTxFilter` は、初期化の間に有効にされなければならない（[SWS_CANIF_00747] と [SWS_CANIF_00748] を参照）。

**[SWS_CANIF_00749]** `CanIf_SetControllerMode(ControllerId, CAN_CS_SLEEP)` が呼ばれた場合、対応する CAN コントローラの `PnTxFilter` は有効にされなければならない（[SWS_CANIF_00748] と [SWS_CANIF_00747] を参照）。

**[SWS_CANIF_00750]** CAN コントローラの `PnTxFilter` が有効な場合、CanIf は、要求された Tx L-PDU が、その CAN コントローラのコンフィギュレーションされた `CanIfTxPduPnFilterPdu` の 1 つである場合を除き、その CAN コントローラへのすべての Tx 要求をブロックしなければならない（`CanIf_Transmit()` が呼ばれたとき、`E_NOT_OK` を返す）。これらの `CanIfTxPduPnFilterPdu` は、常に対応する CAN ドライバへ渡されなければならない。

**[SWS_CANIF_00751]** `CanIf_TxConfirmation()` が呼ばれた場合、対応する `PnTxFilter` は無効にされなければならない（[SWS_CANIF_00747] と [SWS_CANIF_00748] を参照）。

**[SWS_CANIF_00896]** `CanIf_RxIndication()` が呼ばれ、`PnTxFilter` が有効な場合、対応する `PnTxFilter` は無効にされなければならない（[SWS_CANIF_00747] と [SWS_CANIF_00748] を参照）。

**[SWS_CANIF_00752]** CAN コントローラの `PnTxFilter` が無効な場合、CanIf は、`CanIf_SetPduMode()`（[SWS_CANIF_00008] を参照）を介して要求されたとおりに動作しなければならない。

**[SWS_CANIF_00878]** `CanIf_SetPduMode(ControllerId, CANIF_TX_OFFLINE)` が呼ばれ、パーシャルネットワーキングが有効な場合（`CANIF_PUBLIC_PN_SUPPORT`、ECUC_CanIf_00772 を参照）、対応する CAN コントローラの `PnTxFilter` は有効にされなければならない（[SWS_CANIF_00748] と [SWS_CANIF_00747] を参照）。

### 7.26 CAN FD のサポート

性能上の理由から、一部の CAN コントローラは、CAN FD と呼ばれる、可変データレートの機能を使える（[12, ISO 11898-1:2015] を参照）。ペイロードのより高いボーレートのほか、CAN FD は、最大 64 バイトの送信を可能にする、拡張ペイロードもサポートする。これらの機能が利用できるかは、CAN コントローラの CAN FD の一般的なサポートと、CAN コントローラが CAN FD モードにあるか（有効な `CanControllerFdBaudrateConfig`）に依存する。

L-SDU を CAN FD で送るか、従来の CAN 2.0 のフレームで送るかは、コンフィギュレーションされた `CanIfTxPduCanIdType` に依存する。CanIf は、これを、`Can_Write()` を呼ぶときに渡す `PduInfo->id`（`Can_IdType`）の上位から 2 番目のビットを使って、CanDrv に示す。

注: CanDrv が CAN FD モードでない場合（`CanControllerFdBaudrateConfig` が無い）、L-PDU は、`SduLength <= 8` バイトである限り、従来の CAN 2.0 のフレームで送られる。

注: 従来の CAN 2.0 のフレームと CAN FD のフレームのアービトレーションフェーズは、同じ CanId が使われる場合、異ならない。したがって、CAN FD のフレームを使う場合でも、各 CanId を 2 回以上使ってはならない。

CanDrv が、どの種類のフレームを受信したかも、`CanIf_RxIndication()` で渡された `Can_IdType`（`Mailbox->CanId`）の上位から 2 番目のビットを使って示される。この情報に基づいて、CanIf は、[SWS_CANIF_00877] に述べたとおり、コンフィギュレーションされた L-SDU（`CanIfRxPduCfg`）へ、どのように対応付けるかを決める。

注: 上位層が、メッセージを従来の CAN 2.0 のフレームと CAN FD のフレームのどちらで受信したかを気にしない場合、両方の種類に 1 つの `CanIfRxPduCfg` だけを使うことができる（`CanIfRxPduCanIdType` を参照）。これにより、局所的な最適化ができるかもしれない。ただし、システムの観点からは、各フレームの形式をコンフィギュレーションしなければならない。そうでなければ、送信側は、どの種類のフレームを送信すべきかを知らない。

### 7.27 エラーの分類

この章では、このソフトウェアモジュールの中で検出できるすべてのエラーを列挙し、分類する。各エラーは、関連性（開発／プロダクション）と、関連するエラーコードに従って分類される。開発エラーには、値が定義されている。

#### 7.27.1 開発エラー

次の表は、利用できるエラーコードを示す。CanIf は、コンフィギュレーションされていれば、これらを DET へ検出しなければならない。

> **注（再構成）**: 原典の表は、テキストの抽出で、エラーの種類、エラーコード、値の対応が崩れていた。エラーコードと値の並びから、種類との対応を復元した。

| エラーの種類 | 関連性 | 関連するエラーコード | 値 |
|---|---|---|---|
| API サービスが無効なパラメータで呼ばれた | 開発 | `CANIF_E_PARAM_CANID` | 10 |
| 同上 | 開発 | `CANIF_E_PARAM_HOH` | 12 |
| 同上 | 開発 | `CANIF_E_PARAM_LPDU` | 13 |
| 同上 | 開発 | `CANIF_E_PARAM_CONTROLLERID` | 15 |
| 同上 | 開発 | `CANIF_E_PARAM_WAKEUPSOURCE` | 16 |
| 同上 | 開発 | `CANIF_E_PARAM_TRCV` | 17 |
| 同上 | 開発 | `CANIF_E_PARAM_TRCVMODE` | 18 |
| 同上 | 開発 | `CANIF_E_PARAM_TRCVWAKEUPMODE` | 19 |
| 同上 | 開発 | `CANIF_E_PARAM_CTRLMODE` | 21 |
| 同上 | 開発 | `CANIF_E_PARAM_PDU_MODE` | 22 |
| API サービスが無効なポインタで呼ばれた | 開発 | `CANIF_E_PARAM_POINTER` | 20 |
| API サービスが、モジュールの初期化なしで使われた | 開発 | `CANIF_E_UNINIT` | 30 |
| 送信 PDU ID が無効 | 開発 | `CANIF_E_INVALID_TXPDUID` | 50 |
| 受信 PDU ID が無効 | 開発 | `CANIF_E_INVALID_RXPDUID` | 60 |
| CAN Interface の初期化に失敗した | 開発 | `CANIF_E_INIT_FAILED` | 80 |

#### 7.27.2 ランタイムエラー

> **注（再構成）**: 原典の表は、テキストの抽出で、2 行目（`CANIF_E_DATA_LENGTH_MISMATCH`）の「エラーの種類」の記述が、途中で切れていた。

| エラーの種類 | 関連性 | 関連するエラーコード | 値 |
|---|---|---|---|
| データ長の検査の失敗 | ランタイム | `CANIF_E_INVALID_DATA_LENGTH` | 61 |
| データ長（の不一致。原典の記述が途中で切れている） | ランタイム | `CANIF_E_DATA_LENGTH_MISMATCH` | 62 |
| オフラインの PDU チャネルで送信が要求された | ランタイム | `CANIF_E_STOPPED` | 70 |
| メッセージの長さが最大の長さを超えていた | ランタイム | `CANIF_E_TXPDU_LENGTH_EXCEEDED` | 90 |

#### 7.27.3 一時的な故障

一時的な故障は無い。

#### 7.27.4 プロダクションエラー

プロダクションエラーは無い。

#### 7.27.5 拡張プロダクションエラー

拡張プロダクションエラーは無い。

### 7.28 エラーの検出

**[SWS_CANIF_00661]** `CanIf_Init()` と `CanIf_GetVersionInfo()` 以外のすべての CanIf の API サービスは、CanIf が、先行する `CanIf_Init()` の呼び出しで初期化されていない限り、通常の動作を実行してはならず、`E_NOT_OK` を返さなければならない。

**[SWS_CANIF_00901]** スイッチ `CANIF_PUBLIC_DEV_ERROR_DETECT` が有効な場合、`CanIf_Init()` と `CanIf_GetVersionInfo()` 以外のすべての CanIf の API サービスは、CanIf が、先行する `CanIf_Init()` の呼び出しで初期化されていない限り、（`CANIF_E_UNINIT` を使って）DET へ報告しなければならない。

### 7.29 エラーの通知

**[SWS_CANIF_00223]** 定義されたすべてのプロダクションエラーについて、エラーまたは診断に関係するイベント（例: 状態の変化、L-PDU が無いイベント）が起きたときに、イベントを報告することだけが求められる。いかなるステータスも、報告する必要は無い。

**[SWS_CANIF_00119]** 特定の実装および（または）特定のハードウェアの特性のために検出される追加のエラーは、CanIf 固有の実装仕様に追加されなければならない。そのために、上に挙げた分類と列挙は、増分した列挙値で拡張できる。

## 8 API 仕様

### 8.1 インポートする型

この章では、次のファイルから取り込まれるすべての型を示す。

**[SWS_CANIF_00142]**

> **注（再構成）**: 原典の表 8.1 は、テキストの抽出で、モジュール名と型名の対応が崩れていた。インポートされる型の名前だけが確定できたため、列挙する（モジュールごとの内訳は、原典の表 8.1（75 ページ）で確認すること）。

- モジュール: `Can_GeneralTypes`、`ComStack_Types`、`EcuM`、`Std_Types`
- インポートする型: `CanTrcv_TrcvModeType`、`CanTrcv_TrcvWakeupModeType`、`CanTrcv_TrcvWakeupReasonType`、`Can_ControllerStateType`、`Can_ErrorStateType`、`Can_HwHandleType`、`Can_HwType`、`Can_IdType`、`Can_PduType`、`IcomConfigIdType`、`IcomSwitch_ErrorType`、`PduIdType`、`PduInfoType`、`EcuM_WakeupSourceType`、`Std_ReturnType`、`Std_VersionInfoType`

（SRS_BSW_00348、SRS_BSW_00353、SRS_BSW_00361）

### 8.2 型の定義

#### 8.2.1 CanIf_ConfigType

**[SWS_CANIF_00144]**

| 項目 | 内容 |
|---|---|
| 名前 | `CanIf_ConfigType` |
| 型 | 構造体 |
| 範囲 | 実装固有（初期化データ構造の内容は、CAN Interface 固有である） |
| 説明 | この型は、基盤のすべての CAN ドライバについての、CAN Interface のポストビルドのパラメータのためのデータ構造を定義する。初期化のとき、CanIf は、初期化に必要な、そのコンフィギュレーションデータへアクセスするために、この型の構造体へのポインタを得る。 |

**[SWS_CANIF_00523]** 特定の `CanIf_ConfigType` の初期化データ構造は、CanIf の公開パラメータの定義と、各 L-PDU/L-SDU の定義を含まなければならない。

注: CanIf の公開パラメータの定義と、各 L-PDU/L-SDU の定義は、第 10 章で規定される。

#### 8.2.2 CanIf_PduModeType

**[SWS_CANIF_00137]**

| 項目 | 内容 |
|---|---|
| 名前 | `CanIf_PduModeType` |
| 型 | 列挙型 |
| 範囲 | `CANIF_OFFLINE`（0x00）: 対応するチャネルの送信経路と受信経路が無効 ⇒ 通信なしのモード。<br>`CANIF_TX_OFFLINE`（0x01）: 対応するチャネルの送信経路が無効。受信経路は有効。<br>`CANIF_TX_OFFLINE_ACTIVE`（0x02）: 対応するチャネルの送信経路が、オフラインのアクティブモード（[SWS_CANIF_00072] を参照）。受信経路は無効。このモードは、`CanIfTxOfflineActiveSupport = TRUE` を必要とする。<br>`CANIF_ONLINE`（0x03）: 対応するチャネルの送信経路と受信経路が有効 ⇒ 完全な動作モード。 |
| 説明 | チャネルの PduMode は、その送信または受信の動作を定義する。チャネルの通信の方向（送信および（または）受信）は、上位層により、別々に、または一緒に制御できる。 |

> **注（再構成）**: 原典の表 8.3 は、テキストの抽出で、名前、値、説明の対応が崩れていた。値の並び（0x00〜0x03）と説明の順序から復元した。

#### 8.2.3 CanIf_NotifStatusType

**[SWS_CANIF_00201]**

| 項目 | 内容 |
|---|---|
| 名前 | `CanIf_NotifStatusType` |
| 型 | 列挙型 |
| 範囲 | `CANIF_TX_RX_NOTIFICATION`: 要求された Rx/Tx の CAN L-PDU が、正常に送信または受信された。<br>`CANIF_NO_NOTIFICATION`（0x00）: 要求された L-PDU について、送信または受信のイベントが起きていない。 |
| 説明 | CAN L-PDU の通知ステータスの戻り値。 |

> **注（再構成）**: 原典の表 8.4 は、テキストの抽出で、名前と値と説明の対応が崩れていた。値（0x00）は `CANIF_NO_NOTIFICATION` に対応すると読み取れる。

### 8.3 関数の定義

#### 8.3.1 CanIf_Init

**[SWS_CANIF_00001]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_Init` |
| 構文 | `void CanIf_Init(const CanIf_ConfigType* ConfigPtr)` |
| サービス ID [hex] | 0x01 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `ConfigPtr`: コンフィギュレーションパラメータセットへのポインタ。例えば、ポストビルドのパラメータに使われる。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、以降の処理のために、CAN Interface の内部と外部のインタフェースを初期化する。 |

（SRS_BSW_00405、SRS_BSW_00101、SRS_BSW_00358、SRS_BSW_00414、SRS_Can_01021、SRS_Can_01022）

注: 基盤のすべての CAN コントローラとトランシーバは、まだ動作しない状態のままである。

注: サービス `CanIf_Init()` は、EcuM だけから呼ばれる。

**[SWS_CANIF_00085]** サービス `CanIf_Init()` は、CanIf のグローバル変数とデータ構造を、フラグとバッファを含めて初期化しなければならない。

#### 8.3.2 CanIf_DeInit

**[SWS_CANIF_91002]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_DeInit` |
| 構文 | `void CanIf_DeInit(void)` |
| サービス ID [hex] | 0x02 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | CanIf モジュールを終了処理（De-initialize）する。 |

（SRS_Can_01168、SRS_BSW_00336）

注: 終了処理の関数の一般的な動作と制約は、[SWS_BSW_00152]、[SWS_BSW_00072]、[SWS_BSW_00232]、[SWS_BSW_00233] で規定されている。

注意: `CanIf_DeInit()` 関数の呼び出し元は、進行中の送信・受信や、保留中の送信確認が無いことを、確実にしなければならない。

#### 8.3.3 CanIf_SetControllerMode

**[SWS_CANIF_00003]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_SetControllerMode` |
| 構文 | `Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode)` |
| サービス ID [hex] | 0x03 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能（同じコントローラでは不可） |
| 入力パラメータ | `ControllerId`: モード遷移を要求される CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。<br>`ControllerMode`: 要求されるモード遷移 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: コントローラのモード要求が受理された。<br>`E_NOT_OK`: コントローラのモード要求が受理されなかった。 |
| 説明 | このサービスは、CAN コントローラのモードを変更するために、対応する CAN ドライバのサービスを呼ぶ。 |

> **注（再構成）**: 原典の表 8.7 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

（SRS_Can_01027）

注: サービス `CanIf_SetControllerMode()` は、パラメータ `ControllerId` で割り当てられた CAN コントローラの、要求された CAN コントローラモード `ControllerMode` への遷移を開始する。

**[SWS_CANIF_00308]** サービス `CanIf_SetControllerMode()` は、要求された CAN コントローラについて、`Can_SetControllerMode(Controller, Transition)` を呼ばなければならない。

**[SWS_CANIF_00311]** `CanIf_SetControllerMode()` のパラメータ `ControllerId` が無効な値の場合、CanIf は、`CanIf_SetControllerMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00774]** `CanIf_SetControllerMode()` のパラメータ `ControllerMode` が無効な値（`CAN_CS_STARTED`、`CAN_CS_SLEEP`、`CAN_CS_STOPPED` でない）の場合、CanIf は、`CanIf_SetControllerMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CTRLMODE` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: CAN コントローラの ID は、CanIf のコンフィギュレーションの記述の中で公開される。

#### 8.3.4 CanIf_GetControllerMode

**[SWS_CANIF_00229]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_GetControllerMode` |
| 構文 | `Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType* ControllerModePtr)` |
| サービス ID [hex] | 0x04 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `ControllerId`: 現在の動作モードを要求される CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `ControllerModePtr`: CAN コントローラの現在のモードを格納するメモリの場所へのポインタ。 |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: コントローラのモード要求が受理された。<br>`E_NOT_OK`: コントローラのモード要求が受理されなかった。 |
| 説明 | このサービスは、CAN コントローラの現在の状態を得るために、対応する CAN ドライバのサービスを呼ぶ。 |

> **注（再構成）**: 原典の表 8.8 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

（SRS_Can_01028）

**[SWS_CANIF_00313]** `CanIf_GetControllerMode()` のパラメータ `ControllerId` が無効な場合、CanIf は、`CanIf_GetControllerMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00656]** `CanIf_GetControllerMode()` のパラメータ `ControllerModePtr` が無効な値の場合、CanIf は、`CanIf_GetControllerMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_POINTER` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: CAN コントローラモジュールの ID は、CanIf のコンフィギュレーションの記述の中で公開される。

#### 8.3.5 CanIf_GetControllerErrorState

**[SWS_CANIF_91001]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_GetControllerErrorState` |
| 構文 | `Std_ReturnType CanIf_GetControllerErrorState(uint8 ControllerId, Can_ErrorStateType* ErrorStatePtr)` |
| サービス ID [hex] | 0x4b |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ ControllerId では再入不可 |
| 入力パラメータ | `ControllerId`: エラー状態を要求される CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `ErrorStatePtr`: CAN コントローラのエラー状態を格納するメモリの場所へのポインタ。 |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラー状態の要求が受理された。<br>`E_NOT_OK`: エラー状態の要求が受理されなかった。 |
| 説明 | このサービスは、CAN コントローラのエラー状態を得るために、対応する CAN ドライバのサービスを呼ぶ。 |

（SRS_Can_01169）

**[SWS_CANIF_00898]** `CanIf_GetControllerErrorState()` のパラメータ `ControllerId` が無効な値の場合、CanIf は、`CanIf_GetControllerErrorState()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00899]** `CanIf_GetControllerErrorState()` のパラメータ `ErrorStatePtr` が NULL ポインタの場合、CanIf は、`CanIf_GetControllerErrorState()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_POINTER` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

#### 8.3.6 CanIf_Transmit

**[SWS_CANIF_00005]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_Transmit` |
| 構文 | `Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x49 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信する PDU の識別子<br>`PduInfoPtr`: PDU のデータの長さとポインタ、およびメタデータへのポインタ。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 送信要求が受理された。<br>`E_NOT_OK`: 送信要求が受理されなかった。 |
| 説明 | PDU の送信を要求する。 |

（SRS_Can_01008）

注: 対応する CAN コントローラと HTH は、`TxPduId` で解決されなければならない。

**[SWS_CANIF_00317]** サービス `CanIf_Transmit()` は、`ControllerId` で参照されるコントローラモードが `CAN_CS_STARTED` と異なり、少なくとも送信経路のチャネルモードがオンラインまたはオフラインのアクティブでない場合、送信要求を受け付けてはならない。

**[SWS_CANIF_00318]** `CanIf_Transmit()` は、与えられた `TxPduId` に対応するハードウェア送信ハンドルと、次のように設定された `Can_PduType` 構造体で、`Can_Write()` を呼ばなければならない。

- `swPduHandle` は、対応する `CanIf_TxConfirmation()` の呼び出しで使われる `CanTxPduId` に設定される。
- `length` は、`PduInfoPtr->SduLength` として渡された値に設定される（[SWS_CANIF_00894] に従って、減らされる場合がある）。
- `id` は、`TxPduId` に関連付けられた CAN ID に設定される。
- `sdu` は、`PduInfoPtr->SduDataPtr` として渡されたポインタに設定される。

注: `PduInfoPtr` は、L-SDU のユーザメモリ、CAN 識別子、L-SDU のハンドル、データ長へのポインタである（[1, CAN Driver の仕様] を参照）。

**[SWS_CANIF_00243]** CanIf は、事前に定義された CanId を `Can_Write()` の呼び出しで CanDrv へ渡す前に、CanId（`PduInfoPtr->id`）の上位 2 ビット（「識別子拡張フラグ」（[12, ISO11898 (CAN)] を参照）と「CAN FD フラグ」）を設定しなければならない（[1, CAN Driver の仕様]、`Can_IdType` の定義 [SWS_Can_00416] を参照）。各 CAN L-PDU の CanId の形式の種類は、`CanIfTxPduCanIdType` でコンフィギュレーションできる。`CanIfTxPduCanIdType` を参照。（SRS_Can_01141）

**[SWS_CANIF_00882]** `CanIf_Transmit()` は、PDU がトリガ送信にコンフィギュレーションされている場合（`CanIfTxPduTriggerTransmit = TRUE`）、`PduInfoPtr->SduDataPtr` として NULL ポインタを受け付けなければならない。

**[SWS_CANIF_00162]** `Can_Write()` の呼び出しが `E_OK` を返す場合、送信要求サービス `CanIf_Transmit()` は `E_OK` を返さなければならない。

注: `Can_Write()` の呼び出しが `E_NOT_OK` を返す場合、送信要求サービス `CanIf_Transmit()` は `E_NOT_OK` を返さなければならない。送信要求サービス `CanIf_Transmit()` が `E_NOT_OK` を返す場合、上位層のモジュールが、送信要求を繰り返す責任を持つ。

**[SWS_CANIF_00319]** `CanIf_Transmit()` のパラメータ `TxPduId` が無効な値の場合、CanIf は、`CanIf_Transmit()` が呼ばれたときに、開発エラーコード `CANIF_E_INVALID_TXPDUID` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00320]** `CanIf_Transmit()` のパラメータ `PduInfoPtr` が無効な値の場合、CanIf は、`CanIf_Transmit()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_POINTER` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00893]** `CanIf_Transmit()` が、`PduInfoPtr->SduLength` が `TxPduId` で参照される PDU の最大長を超えて呼ばれたとき:

- `Can_IdType` が従来の CAN フレームを示す場合、`SduLength > 8`
- `Can_IdType` が CAN FD フレームを示す場合、`SduLength > 64`

CanIf は、ランタイムエラーコード `CANIF_E_DATA_LENGTH_MISMATCH` を、DET の `Det_ReportRuntimeError()` サービスへ報告しなければならない。

注: 静的にコンフィギュレーションされた送信のほかに、動的な送信もある。したがって、有効なデータ長は、常に `PduInfoPtr->SduLength` で渡される。さらに、フレームの種類も、`CanIf_SetDynamicTxId()` により変わりうる。[SWS_CANIF_00893] は、一致しない送信要求を、DET を介して検出できることを保証する。

**[SWS_CANIF_00894]** `CanIf_Transmit()` が、`PduInfoPtr->SduLength` が `TxPduId` で参照される PDU の最大長を超えて呼ばれ、`CanIfTxPduTruncation` が有効な場合、CanIf は、できるだけ多くのデータを送信し、残りを破棄しなければならない。

**[SWS_CANIF_00900]** `CanIf_Transmit()` が、`PduInfoPtr->SduLength` が `TxPduId` で参照される PDU の最大長を超えて呼ばれ、`CanIfTxPduTruncation` が無効な場合、CanIf は、ランタイムエラー `CANIF_E_TXPDU_LENGTH_EXCEEDED` を報告し、それ以上の処理をせずに `E_NOT_OK` を返さなければならない。

注: `CanIf_Transmit()` の呼び出しの間、`PduInfoPtr` のバッファは CanIf により管理され、このバッファに、別の呼び出しのコンテキストから、読み書きのアクセスをしてはならない。この呼び出しが戻った後、所有権は上位層に移る。

#### 8.3.7 CanIf_CancelTransmit

**[SWS_CANIF_00520]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_CancelTransmit` |
| 構文 | `Std_ReturnType CanIf_CancelTransmit(PduIdType TxPduId)` |
| サービス ID [hex] | 0x4a |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: キャンセルする PDU の識別。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: キャンセルが、宛先のモジュールにより正常に実行された。<br>`E_NOT_OK`: キャンセルが、宛先のモジュールにより拒否された。 |
| 説明 | 下位層の通信モジュールで進行中の PDU の送信の、キャンセルを要求する。 |

注: サービス `CanIf_CancelTransmit()` には機能が無く、バスに依存しない動作を実現するために、AUTOSAR の PduR から呼ばれる。

**[SWS_CANIF_00521]** サービス `CanIf_CancelTransmit()` は、コンフィギュレーションパラメータ `CANIF_PUBLIC_CANCEL_TRANSMIT_SUPPORT`（ECUC_CanIf_00614 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。`PduRComCancelTransmitSupport` が ON にコンフィギュレーションされている場合、ON にコンフィギュレーションされなければならない。

**[SWS_CANIF_00652]** `CanIf_CancelTransmit()` のパラメータ `TxPduId` が無効な値の場合、CanIf は、`CanIf_CancelTransmit()` が呼ばれたときに、開発エラーコード `CANIF_E_INVALID_TXPDUID` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

#### 8.3.8 CanIf_ReadRxPduData

**[SWS_CANIF_00194]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ReadRxPduData` |
| 構文 | `Std_ReturnType CanIf_ReadRxPduData(PduIdType CanIfRxSduId, PduInfoType* CanIfRxInfoPtr)` |
| サービス ID [hex] | 0x06 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `CanIfRxSduId`: 対応する CAN L-SDU ID と、暗黙のうちに CAN ドライバのインスタンス、および対応する CAN コントローラデバイスを指定する、受信 L-SDU のハンドル。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `CanIfRxInfoPtr`: 受信した PDU の長さ（`SduLength`）、PDU を含むバッファへのポインタ（`SduDataPtr`）、およびこの PDU に関するメタデータを含む。 |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: L-SDU のデータの要求が受理された。<br>`E_NOT_OK`: 有効なデータが受信されていない。 |
| 説明 | このサービスは、要求された `CanIfRxSduId` のデータ長と受信したデータを、呼び出した上位層へ提供する。 |

> **注（再構成）**: 原典の表 8.12 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

（SRS_Can_01125、SRS_Can_01129）

**[SWS_CANIF_00324]** 関数 `CanIf_ReadRxPduData()` は、`ControllerId` で参照される対応するコントローラモードが `CAN_CS_STARTED` と異なり、チャネルモードが受信経路のオンラインである場合、要求を受け付けず、`E_NOT_OK` を返さなければならない。

**[SWS_CANIF_00325]** `CanIf_ReadRxPduData()` のパラメータ `CanIfRxSduId` が、無効な値（例: `CANIF_READRXPDU_DATA`（ECUC_CanIf_00600）で CanIf に保存されるようにコンフィギュレーションされていない）の場合、CanIf は、`CanIf_ReadRxPduData()` が呼ばれたときに、開発エラーコード `CANIF_E_INVALID_RXPDUID` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00326]** `CanIf_ReadRxPduData()` のパラメータ `CanIfRxInfoPtr` が無効な値の場合、CanIf は、`CanIf_ReadRxPduData()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_POINTER` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00329]** `CanIf_ReadRxPduData()` は、複数の CAN-ID を受信する（範囲の受信）ように定義された `CanIfRxSduId` には、使ってはならない。

注: `CanIf_ReadRxPduData()` の呼び出しの間、`CanIfRxInfoPtr` のバッファは CanIf により管理され、このバッファに、別の呼び出しのコンテキストから、読み書きのアクセスをしてはならない。この呼び出しが戻った後、所有権は上位層に移る。

**[SWS_CANIF_00330]** `CanIf_ReadRxPduData()` のコンフィギュレーション: この API は、プリコンパイル時のコンフィギュレーションで、コンフィギュレーションパラメータ `CANIF_PUBLIC_READRXPDU_DATA_API`（ECUC_CanIf_00607 を参照）により、有効・無効にできる。

#### 8.3.9 CanIf_ReadTxNotifStatus

**[SWS_CANIF_00202]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ReadTxNotifStatus` |
| 構文 | `CanIf_NotifStatusType CanIf_ReadTxNotifStatus(PduIdType CanIfTxSduId)` |
| サービス ID [hex] | 0x07 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `CanIfTxSduId`: 送信される L-SDU のハンドル。このハンドルは、対応する CAN L-SDU ID と、暗黙のうちに CAN ドライバのインスタンス、および対応する CAN コントローラデバイスを指定する。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `CanIf_NotifStatusType`: 対応する CAN Tx L-PDU の現在の確認ステータス。 |
| 説明 | このサービスは、`CanIfTxSduId` で要求された、特定の静的または動的な CAN Tx L-PDU の、確認ステータス（確認が起きたか否か）を返す。 |

（SRS_Can_01130）

注: この関数は、対応する要求された L-SDU への、送信確認のイベントを、上位層へ通知する。

**[SWS_CANIF_00393]** コンフィギュレーションパラメータ `CANIF_PUBLIC_READTXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00609）と、送信された L-SDU の `CANIF_TXPDU_READ_NOTIFYSTATUS`（ECUC_CanIf_00589）が `TRUE` に設定されていて、`CanIf_ReadTxNotifStatus()` が呼ばれた場合、CanIf は、送信された L-SDU の通知ステータスをリセットしなければならない。

**[SWS_CANIF_00331]** `CanIf_ReadTxNotifStatus()` のパラメータ `CanIfTxSduId` が範囲外の場合、またはこの CAN Tx L-SDU にステータス情報がコンフィギュレーションされていない場合、CanIf は、`CanIf_ReadTxNotifStatus()` が呼ばれたときに、開発エラーコード `CANIF_E_INVALID_TXPDUID` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00335]** `CanIf_ReadTxNotifyStatus()` のコンフィギュレーション: この API は、プリコンパイル時のコンフィギュレーションで、パラメータ `CANIF_PUBLIC_READTXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00609 を参照）により、全体で有効・無効にできる。

#### 8.3.10 CanIf_ReadRxNotifStatus

**[SWS_CANIF_00230]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ReadRxNotifStatus` |
| 構文 | `CanIf_NotifStatusType CanIf_ReadRxNotifStatus(PduIdType CanIfRxSduId)` |
| サービス ID [hex] | 0x08 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `CanIfRxSduId`: 対応する CAN L-SDU ID と、暗黙のうちに CAN ドライバのインスタンス、および対応する CAN コントローラデバイスを指定する、受信 L-SDU のハンドル。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `CanIf_NotifStatusType`: 対応する CAN Rx L-PDU の現在の通知ステータス。 |
| 説明 | このサービスは、`CanIfRxSduId` で要求された、特定の CAN Rx L-PDU の、通知ステータス（通知が起きたか否か）を返す。 |

（SRS_Can_01130、SRS_Can_01131）

注: この関数は、対応する要求された L-SDU への、受信通知のイベントを、上位層へ通知する。

**[SWS_CANIF_00394]** コンフィギュレーションパラメータ `CANIF_PUBLIC_READRXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00608）と `CANIF_RXPDU_READ_NOTIFYSTATUS`（ECUC_CanIf_00595）が `TRUE` に設定されていて、`CanIf_ReadRxNotifStatus()` が呼ばれた場合、CanIf は、受信した L-SDU の通知ステータスをリセットしなければならない。

**[SWS_CANIF_00336]** `CanIf_ReadRxNotifStatus()` のパラメータ `CanIfRxSduId` が範囲外の場合、`CANIF_READRXPDU_DATA_API` が無効なのに `CanRxPduId` のステータスが要求された場合、またはこの CAN Rx L-SDU にステータス情報がコンフィギュレーションされていない場合、CanIf は、`CanIf_ReadRxNotifStatus()` が呼ばれたときに、開発エラーコード `CANIF_E_INVALID_RXPDUID` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: 関数 `CanIf_ReadRxNotifStatus()` は、複数の CAN-ID を受信する（範囲の受信）ように定義された `CanIfRxSduId` には、使ってはならない。

**[SWS_CANIF_00340]** `CanIf_ReadRxNotifStatus()` のコンフィギュレーション: この API は、プリコンパイル時のコンフィギュレーションで、パラメータ `CANIF_PUBLIC_READRXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00608 を参照）により、全体で有効・無効にできる。

#### 8.3.11 CanIf_SetPduMode

**[SWS_CANIF_00008]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_SetPduMode` |
| 構文 | `Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, CanIf_PduModeType PduModeRequest)` |
| サービス ID [hex] | 0x09 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `ControllerId`: 物理的な CAN コントローラに割り当てられた、対応する CanIf の ControllerId に接続された、自 ECU のすべての PDU が対象になる。<br>`PduModeRequest`: 要求される PDU モードの変更 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: モード遷移の要求が受理された。<br>`E_NOT_OK`: モード遷移の要求が受理されなかった。 |
| 説明 | このサービスは、あらかじめ定義された論理的な PDU チャネルの L-PDU に、要求されたモードを設定する。 |

> **注（再構成）**: 原典の表 8.15 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

注: あらかじめ定義された論理的な PDU チャネルを示すチャネルパラメータは、関数 `CanIf_SetPduMode()` のパラメータ `ControllerId` から導ける。

**[SWS_CANIF_00341]** `CanIf_SetPduMode()` が無効な `ControllerId` で呼ばれた場合、CanIf は、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00860]** `CanIf_SetPduMode()` が無効な `PduModeRequest` で呼ばれた場合、CanIf は、開発エラーコード `CANIF_E_PARAM_PDU_MODE` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00874]** サービス `CanIf_SetPduMode()` は、`ControllerId` で参照されるコントローラモードが `CAN_CS_STARTED` の状態でない場合、いかなる要求も受け付けてはならず、`E_NOT_OK` を返さなければならない。

#### 8.3.12 CanIf_GetPduMode

**[SWS_CANIF_00009]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_GetPduMode` |
| 構文 | `Std_ReturnType CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType* PduModePtr)` |
| サービス ID [hex] | 0x0a |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能（同じチャネルでは不可） |
| 入力パラメータ | `ControllerId`: 物理的な CAN コントローラに割り当てられた、対応する CanIf の ControllerId に接続された、自 ECU のすべての PDU が対象になる。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `PduModePtr`: 論理的な PDU チャネルの現在のモードを格納するメモリの場所へのポインタ。 |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: PDU モードの要求が受理された。<br>`E_NOT_OK`: PDU モードの要求が受理されなかった。 |
| 説明 | このサービスは、要求された PDU チャネルの現在のモードを報告する。 |

**[SWS_CANIF_00346]** `CanIf_GetPduMode()` が無効な `ControllerId` で呼ばれた場合、CanIf は、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00657]** `CanIf_GetPduMode()` が無効な `PduModePtr` で呼ばれた場合、CanIf は、開発エラーコード `CANIF_E_PARAM_POINTER` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

#### 8.3.13 CanIf_GetVersionInfo

**[SWS_CANIF_00158]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_GetVersionInfo` |
| 構文 | `void CanIf_GetVersionInfo(Std_VersionInfoType* VersionInfo)` |
| サービス ID [hex] | 0x0b |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | `VersionInfo`: このモジュールのバージョン情報を格納する場所へのポインタ。 |
| 戻り値 | なし |
| 説明 | このサービスは、呼ばれた CAN Interface モジュールのバージョン情報を返す。 |

（SRS_BSW_00407、SRS_BSW_00411）

#### 8.3.14 CanIf_SetDynamicTxId

**[SWS_CANIF_00189]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_SetDynamicTxId` |
| 構文 | `void CanIf_SetDynamicTxId(PduIdType CanIfTxSduId, Can_IdType CanId)` |
| サービス ID [hex] | 0x0c |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `CanIfTxSduId`: 送信される L-SDU のハンドル。このハンドルは、対応する CAN L-SDU ID と、暗黙のうちに CAN ドライバのインスタンス、および対応する CAN コントローラデバイスを指定する。<br>`CanId`: FD または従来の CAN フレームとして送信される CAN L-SDU の、標準／拡張の CAN ID。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、要求された CAN L-PDU の、対応する CAN 識別子を再設定する。 |

> **注（再構成）**: 原典の表 8.18 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

**[SWS_CANIF_00352]** `CanIf_SetDynamicTxId()` のパラメータ `CanIfTxSduId` が無効な値の場合、CanIf は、`CanIf_SetDynamicTxId()` が呼ばれたときに、開発エラーコード `CANIF_E_INVALID_TXPDUID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00353]** `CanIf_SetDynamicTxId()` のパラメータ `CanId` が無効な値の場合、CanIf は、`CanIf_SetDynamicTxId()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CANID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00355]** `CanIf_SetDynamicTxId()` を呼ぶ前に CanIf が初期化されていなかった場合、関数 `CanIf_SetDynamicTxId()` は、Tx CanId の再設定を実行してはならない。

**[SWS_CANIF_00356]** `CanIf_SetDynamicTxId()` は、同じ L-SDU ID が扱われている場合、`CanIf_Transmit()` に中断されてはならない。

**[SWS_CANIF_00357]** `CanIf_SetDynamicTxId()` のコンフィギュレーション: この関数は、コンフィギュレーションパラメータ `CANIF_PUBLIC_SETDYNAMICTXID_API`（ECUC_CanIf_00610 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

#### 8.3.15 CanIf_SetTrcvMode

**[SWS_CANIF_00287]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_SetTrcvMode` |
| 構文 | `Std_ReturnType CanIf_SetTrcvMode(uint8 TransceiverId, CanTrcv_TrcvModeType TransceiverMode)` |
| サービス ID [hex] | 0x0d |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `TransceiverId`: モード遷移を要求される CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。<br>`TransceiverMode`: 要求されるモード遷移 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: トランシーバのモード要求が受理された。<br>`E_NOT_OK`: トランシーバのモード要求が受理されなかった。 |
| 説明 | このサービスは、対応する CAN Transceiver Driver のサービスを呼んで、トランシーバ `TransceiverId` の動作モードを変更する。 |

> **注（再構成）**: 原典の表 8.19 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

注: 詳細は、[2, CAN Transceiver Driver の仕様] を参照。

**[SWS_CANIF_00358]** 関数 `CanIf_SetTrcvMode()` は、対応する要求された CAN Transceiver Driver モジュールの、関数 `CanTrcv_SetOpMode(Transceiver, OpMode)` を呼ばなければならない。

注: サービス `CanTrcv_SetOpMode()` のパラメータの型は、次のとおりである。

- `OpMode`: `CanTrcv_TrcvModeType`（希望する動作モード）
- `Transceiver`: `uint8`（関数呼び出しを適用するトランシーバ）

（[2, CAN Transceiver Driver の仕様] を参照）

**[SWS_CANIF_00538]** `CanIf_SetTrcvMode()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_SetTrcvMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: トランシーバのモードは、トランシーバの以前のモードが `CANTRCV_TRCVMODE_NORMAL` だった場合に限り、`CANTRCV_TRCVMODE_STANDBY` に変更できる（[2] を参照）。ただし、これは CanIf では検査されない。

注: トランシーバのモードは、トランシーバの以前のモードが `CANTRCV_TRCVMODE_STANDBY` だった場合に限り、`CANTRCV_TRCVMODE_SLEEP` に変更できる（[2] を参照）。ただし、これは CanIf では検査されない。

**[SWS_CANIF_00648]** `CanIf_SetTrcvMode()` のパラメータ `TransceiverMode` が無効な値（`CANTRCV_TRCVMODE_STANDBY`、`CANTRCV_TRCVMODE_SLEEP`、`CANTRCV_TRCVMODE_NORMAL` でない）の場合、CanIf は、`CanIf_SetTrcvMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCVMODE` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: 関数 `CanIf_SetTrcvMode()` は、トランシーバのハードウェアがそれらのモードをサポートするかに関係なく、すべての CAN トランシーバに、`TransceiverMode` のすべての値で適用できるべきである。これは、割り当てられた物理 CAN チャネルの、CanIf からの見え方を簡単にするためである。

**[SWS_CANIF_00362]** `CanIf_SetTrcvMode()` のコンフィギュレーション: 各ネットワークの、サポートされるトランシーバの種類の数は、コンフィギュレーションの段階で設定される（`CanInterfaceTransceiverConfiguration` ECUC_CanIf_00587 と `CanInterfaceTransceiverDriverConfiguration` ECUC_CanIf_00273 を参照）。トランシーバが使われない場合、この関数は省略してもよい。したがって、LT または PB クラスでトランシーバがコンフィギュレーションされていない場合、API は `E_NOT_OK` で戻らなければならない。

#### 8.3.16 CanIf_GetTrcvMode

**[SWS_CANIF_00288]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_GetTrcvMode` |
| 構文 | `Std_ReturnType CanIf_GetTrcvMode(uint8 TransceiverId, CanTrcv_TrcvModeType* TransceiverModePtr)` |
| サービス ID [hex] | 0x0e |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `TransceiverId`: 現在の動作モードを要求される CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `TransceiverModePtr`: トランシーバが接続された、要求されたネットワークの、要求されたモード。 |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: トランシーバのモード要求が受理された。<br>`E_NOT_OK`: トランシーバのモード要求が受理されなかった。 |
| 説明 | この関数は、`CanTrcv_GetOpMode` を呼び、パラメータ `TransceiverModePtr` を、CanTrcv が提供した値 `OpMode` で更新する。 |

注: 詳細は、[2, CAN Transceiver Driver の仕様] を参照。

**[SWS_CANIF_00363]** 関数 `CanIf_GetTrcvMode()` は、対応する要求された CAN Transceiver Driver モジュールの、関数 `CanTrcv_GetOpMode(Transceiver, OpMode)` を呼ばなければならない。

注: 関数 `CanTrcv_GetOpMode` のパラメータの型は、次のとおりである。

- `OpMode`: `CanTrcv_TrcvModeType`（希望する動作モード）
- `Transceiver`: `uint8`（API の呼び出しを適用するトランシーバ）

（[2, CAN Transceiver Driver の仕様] を参照）

**[SWS_CANIF_00364]** `CanIf_GetTrcvMode()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_GetTrcvMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00650]** `CanIf_GetTrcvMode()` のパラメータ `TransceiverModePtr` が無効な値の場合、CanIf は、`CanIf_GetTrcvMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_POINTER` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00367]** `CanIf_GetTrcvMode()` のコンフィギュレーション: 各ネットワークの、サポートされるトランシーバの種類の数は、コンフィギュレーションの段階で設定される（`CanInterfaceTransceiverConfiguration` ECUC_CanIf_00587 と `CanInterfaceTransceiverDriverConfiguration` ECUC_CanIf_00273 を参照）。トランシーバが使われない場合、この関数は省略してもよい。したがって、LT または PB クラスでトランシーバがコンフィギュレーションされていない場合、API は `E_NOT_OK` で戻らなければならない。

#### 8.3.17 CanIf_GetTrcvWakeupReason

**[SWS_CANIF_00289]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_GetTrcvWakeupReason` |
| 構文 | `Std_ReturnType CanIf_GetTrcvWakeupReason(uint8 TransceiverId, CanTrcv_TrcvWakeupReasonType* TrcvWuReasonPtr)` |
| サービス ID [hex] | 0x0f |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `TransceiverId`: ウェイクアップの理由を要求される CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `TrcvWuReasonPtr`: 要求されたトランシーバのウェイクアップの理由が返される場所へ提供されたポインタ。 |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: トランシーバのウェイクアップの理由の要求が受理された。<br>`E_NOT_OK`: トランシーバのウェイクアップの理由の要求が受理されなかった。 |
| 説明 | このサービスは、対応する CAN Transceiver Driver のサービスを呼んで、トランシーバ `TransceiverId` のウェイクアップの理由を返す。 |

注: 取りうるウェイクアップの理由を検出し、区別する能力は、CAN トランシーバのハードウェアに強く依存する。詳細は、[2, CAN Transceiver Driver の仕様] を参照。

**[SWS_CANIF_00368]** 関数 `CanIf_GetTrcvWakeupReason()` は、対応する要求された CanTrcv の `CanTrcv_GetBusWuReason(Transceiver, Reason)` を呼ばなければならない。

注: 関数 `CanTrcv_GetBusWuReason()` のパラメータの型は、次のとおりである。

- `Reason`: `CanTrcv_TrcvWakeupReasonType`
- `Transceiver`: `uint8`（API の呼び出しを適用するトランシーバ）

（[2, CAN Transceiver Driver の仕様] を参照）

**[SWS_CANIF_00537]** `CanIf_GetTrcvWakeupReason()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_GetTrcvWakeupReason()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00649]** `CanIf_GetTrcvWakeupReason()` のパラメータ `TrcvWuReasonPtr` が無効な値の場合、CanIf は、`CanIf_GetTrcvWakeupReason()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_POINTER` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: 利用できるネットワークが複数ある場合、各ネットワークが、異なるウェイクアップの理由を報告しうることに注意。例えば、ECU が CAN を使う場合、CAN によるウェイクアップが起き、入ってくるデータが、別の CAN ネットワークの内部ウェイクアップを引き起こすことがありうる。サービス `CanIf_GetTrcvWakeupReason()` は、「ネットワークごと」の見え方を持ち、より重要な理由や順序を、内部で判定しない。例えば、一方のトランシーバが電源供給を制御し、他方は電源が入っているか、いないかだけの場合も、同じことが言える。その場合、一方は `CANIF_TRCV_WU_POWER_ON` を返せ、他方は例えば `CANIF_TRCV_WU_RESET` を示すことがある。ウェイクアップの情報をどう扱うかは、呼び出し元のモジュールが決めることである。

**[SWS_CANIF_00371]** `CanIf_GetTrcvWakeupReason()` のコンフィギュレーション: 各ネットワークの、サポートされるトランシーバの種類の数は、コンフィギュレーションの段階で設定される（`CanInterfaceTransceiverConfiguration` ECUC_CanIf_00587 と `CanInterfaceTransceiverDriverConfiguration` ECUC_CanIf_00273 を参照）。トランシーバが使われない場合、この関数は省略してもよい。したがって、LT または PB クラスでトランシーバがコンフィギュレーションされていない場合、API は `E_NOT_OK` で戻らなければならない。

#### 8.3.18 CanIf_SetTrcvWakeupMode

**[SWS_CANIF_00290]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_SetTrcvWakeupMode` |
| 構文 | `Std_ReturnType CanIf_SetTrcvWakeupMode(uint8 TransceiverId, CanTrcv_TrcvWakeupModeType TrcvWakeupMode)` |
| サービス ID [hex] | 0x10 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `TransceiverId`: ウェイクアップ通知のモード遷移を要求される CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。<br>`TrcvWakeupMode`: 要求されるトランシーバのウェイクアップ通知のモード |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: ウェイクアップ通知の状態が、要求されたモードに変更された場合に返される。<br>`E_NOT_OK`: ウェイクアップ通知の状態の変更が失敗した場合、またはパラメータが許される範囲外の場合に返される。以前の状態は変更されていない。 |
| 説明 | この関数は、`CanTrcv_SetTrcvWakeupMode` を呼ばなければならない。 |

> **注（再構成）**: 原典の表 8.22 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

注: 詳細は、[2, CAN Transceiver Driver の仕様] を参照。

**[SWS_CANIF_00372]** 関数 `CanIf_SetTrcvWakeupMode()` は、対応する要求された CanTrcv の `CanTrcv_SetWakeupMode(Transceiver, TrcvWakeupMode)` を呼ばなければならない。

情報: 関数 `CanTrcv_SetWakeupMode()` のパラメータの型は、次のとおりである。

- `TrcvWakeupMode`: `CanTrcv_TrcvWakeupModeType`（[2, CAN Transceiver Driver の仕様] を参照）
- `Transceiver`: `uint8`（API の呼び出しを適用するトランシーバ）

（[2, CAN Transceiver Driver の仕様] を参照）

注: 次の 3 つの段落は、すでに CanTrcv の仕様（[2] を参照）に記述されている。これらは、パラメータ `TrcvWakeupMode` で要求された、それぞれのトランシーバのウェイクアップモードでの、CanTrcv の動作を説明する。

- `CANIF_TRCV_WU_ENABLE`: CanTrcv が、対象の CAN ネットワークについて、保存されたウェイクアップイベントを保留している場合、通知は、関数 `CanTrcv_SetTrcvWakeupMode()` の中、またはその直後（実装に依存する）に実行される。
- `CANIF_TRCV_WU_DISABLE`: 対象の CAN ネットワークのウェイクアップイベントの通知は、CanTrcv を通して渡されない。トランシーバデバイスと基盤の通信ドライバは、検出されたウェイクアップイベントをバッファリングし、ウェイクアップの通知が再び有効になったときに、そのイベントを発生させなければならない。
- `CANIF_TRCV_WU_CLEAR`: ウェイクアップイベントの通知が無効の場合（モード `CANIF_TRCV_WU_DISABLE` の説明を参照）、検出されたウェイクアップイベントはバッファリングされる。パラメータ `CANIF_TRCV_WU_CLEAR` で `CanIf_SetTrcvWakeupMode()` を呼ぶと、これらのバッファリングされたイベントがクリアされる。ウェイクアップイベントのクリアは、ウェイクアップの通知が無効な場合に、CanTrcv の上位層の制御下にある、保存されたすべてのウェイクアップイベントをクリアするために使わなければならない。

**[SWS_CANIF_00535]** `CanIf_SetTrcvWakeupMode()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_SetTrcvWakeupMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00536]** `CanIf_SetTrcvWakeupMode()` のパラメータ `TrcvWakeupMode` が無効な値の場合、CanIf は、`CanIf_SetTrcvWakeupMode()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCVWAKEUPMODE` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00373]** `CanIf_SetTrcvWakeupMode()` のコンフィギュレーション: 各ネットワークの、サポートされるトランシーバの種類の数は、コンフィギュレーションの段階で設定される（`CanInterfaceTransceiverConfiguration` ECUC_CanIf_00587 と `CanInterfaceTransceiverDriverConfiguration` ECUC_CanIf_00273 を参照）。トランシーバが使われない場合、この関数は省略してもよい。したがって、LT または PB クラスでトランシーバがコンフィギュレーションされていない場合、API は `E_NOT_OK` で戻らなければならない。

#### 8.3.19 CanIf_CheckWakeup

**[SWS_CANIF_00219]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_CheckWakeup` |
| 構文 | `Std_ReturnType CanIf_CheckWakeup(EcuM_WakeupSourceType WakeupSource)` |
| サービス ID [hex] | 0x11 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `WakeupSource`: ウェイクアップのイベントを起こしたソースデバイス: CAN コントローラまたは CAN トランシーバ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: ウェイクアップの検査の要求が受理された場合に返される。<br>`E_NOT_OK`: ウェイクアップの検査の要求が受理されなかった場合に返される。 |
| 説明 | このサービスは、基盤の CAN ドライバまたは CAN トランシーバドライバが、すでにウェイクアップのイベントを通知しているかを検査する。 |

> **注（再構成）**: 原典の表 8.23 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

注: 統合コードがこの関数を呼ぶ。

**[SWS_CANIF_00398]** `CanIf_CheckWakeup()` のパラメータ `WakeupSource` が無効な値の場合、CanIf は、`CanIf_CheckWakeup()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_WAKEUPSOURCE` を DET の `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: `CanIf_CheckWakeup()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00180]** CanIf は、次の場合に限り、ウェイクアップサービス `CanIf_CheckWakeup()` を提供しなければならない。

- 基盤の CAN コントローラがウェイクアップのサポートを提供し、ウェイクアップが、パラメータ `CanIfCtrlWakeupSupport` と CanDrv のコンフィギュレーションにより有効にされている、
- かつ／または、基盤の CAN トランシーバがウェイクアップのサポートを提供し、ウェイクアップが、パラメータ `CanIfTrcvWakeupSupport` と CanTrcv のコンフィギュレーションにより有効にされている、
- かつ、コンフィギュレーションパラメータ `CanIfWakeupSupport` が有効である。

**[SWS_CANIF_00892]** `CanIf_CheckWakeup()` のコンフィギュレーション: ウェイクアップを使わない場合、この API は、`CanIfWakeupSupport` を無効にして省略できる。

#### 8.3.20 CanIf_CheckValidation

**[SWS_CANIF_00178]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_CheckValidation` |
| 構文 | `Std_ReturnType CanIf_CheckValidation(EcuM_WakeupSourceType WakeupSource)` |
| サービス ID [hex] | 0x12 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `WakeupSource`: ウェイクアップのイベントを起こし、検証する必要のあるソースデバイス: CAN コントローラまたは CAN トランシーバ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 検証の検査の要求が受理された場合に返される。<br>`E_NOT_OK`: 検証の検査の要求が受理されなかった場合に返される。 |
| 説明 | このサービスは、以前のウェイクアップのイベントを検証するために実行される。 |

注: 統合コードがこの関数を呼ぶ。

**[SWS_CANIF_00404]** `CanIf_CheckValidation()` のパラメータ `WakeupSource` が無効な値の場合、CanIf は、`CanIf_CheckValidation()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_WAKEUPSOURCE` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: `CanIf_CheckValidation()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

注意: 対応する CAN コントローラと CAN トランシーバは、`CanTrcv_SetOpMode(Transceiver, CANTRCV_TRCVMODE_NORMAL)` と `Can_SetControllerMode(Controller, CAN_CS_STARTED)` で ON にされていなければならず、対応するモードの通知が呼ばれていなければならない。

**[SWS_CANIF_00408]** `CanIf_CheckValidation()` のコンフィギュレーション: 検証が不要な場合、この API は、`CANIF_PUBLIC_WAKEUP_CHECK_VALIDATION_SUPPORT`（ECUC_CanIf_00611 を参照）を無効にして省略できる。

#### 8.3.21 CanIf_GetTxConfirmationState

**[SWS_CANIF_00734]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_GetTxConfirmationState` |
| 構文 | `CanIf_NotifStatusType CanIf_GetTxConfirmationState(uint8 ControllerId)` |
| サービス ID [hex] | 0x19 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能（同じコントローラでは不可） |
| 入力パラメータ | `ControllerId`: CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `CanIf_NotifStatusType`: CAN コントローラのすべての TX PDU について合わせた、TX 確認のステータス |
| 説明 | このサービスは、最後の CAN コントローラの開始以降に、CAN コントローラ全体で、いずれかの TX 確認が行われたかを報告する。 |

**[SWS_CANIF_00736]** `CanIf_GetTxConfirmationState()` のパラメータ `ControllerId` が無効な値の場合、CanIf は、`CanIf_GetTxConfirmationState()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

注: `CanIf_GetTxConfirmationState()` の呼び出しのコンテキストは、タスクレベル（ポーリングモード）である。

**[SWS_CANIF_00738]** `CanIf_GetTxConfirmationState()` のコンフィギュレーション: CanSm のバスオフ回復が、Tx 確認のステータスを必要としない場合（[SWS_CANIF_00740] を参照）、この API は、`CanIfPublicTxConfirmPollingSupport` を無効にして省略できる。

#### 8.3.22 CanIf_ClearTrcvWufFlag

**[SWS_CANIF_00760]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ClearTrcvWufFlag` |
| 構文 | `Std_ReturnType CanIf_ClearTrcvWufFlag(uint8 TransceiverId)` |
| サービス ID [hex] | 0x1e |
| 同期／非同期 | 非同期 |
| 再入可能性 | 異なる CAN トランシーバでは再入可能 |
| 入力パラメータ | `TransceiverId`: 指定された CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 要求が受理された<br>`E_NOT_OK`: 要求が受理されなかった |
| 説明 | CanIf モジュールに、指定された CAN トランシーバの WUF フラグをクリアするよう要求する。 |

**[SWS_CANIF_00766]** `CanIf_ClearTrcvWufFlag()` の中で、関数 `CanTrcv_ClearTrcvWufFlag()` が呼ばれなければならない。

**[SWS_CANIF_00769]** `CanIf_ClearTrcvWufFlag()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_ClearTrcvWufFlag()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

**[SWS_CANIF_00771]** `CanIf_ClearTrcvWufFlag()` のコンフィギュレーション: CanIf がこの関数をサポートするかどうかは、コンフィギュレーションパラメータ `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

#### 8.3.23 CanIf_CheckTrcvWakeFlag

**[SWS_CANIF_00761]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_CheckTrcvWakeFlag` |
| 構文 | `Std_ReturnType CanIf_CheckTrcvWakeFlag(uint8 TransceiverId)` |
| サービス ID [hex] | 0x1f |
| 同期／非同期 | 非同期 |
| 再入可能性 | 異なる CAN トランシーバでは再入可能 |
| 入力パラメータ | `TransceiverId`: 指定された CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 要求が受理された<br>`E_NOT_OK`: 要求が受理されなかった |
| 説明 | CanIf モジュールに、指定された CAN トランシーバのウェイクフラグを検査するよう要求する。 |

**[SWS_CANIF_00765]** `CanIf_CheckTrcvWakeFlag()` の中で、関数 `CanTrcv_CheckWakeFlag()` が呼ばれなければならない。

**[SWS_CANIF_00770]** `CanIf_CheckTrcvWakeFlag()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_CheckTrcvWakeFlag()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

**[SWS_CANIF_00813]** `CanIf_CheckTrcvWakeFlag()` のコンフィギュレーション: CanIf がこの関数をサポートするかどうかは、コンフィギュレーションパラメータ `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

#### 8.3.24 CanIf_SetBaudrate

**[SWS_CANIF_00867]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_SetBaudrate` |
| 構文 | `Std_ReturnType CanIf_SetBaudrate(uint8 ControllerId, uint16 BaudRateConfigID)` |
| サービス ID [hex] | 0x27 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる ControllerId では再入可能。同じ ControllerId では再入不可。 |
| 入力パラメータ | `ControllerId`: ボーレートを設定する CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。<br>`BaudRateConfigID`: ID でボーレートのコンフィギュレーションを参照する（`CanControllerBaudRateConfigID` を参照）。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: サービスの要求が受理され、（新しい）ボーレートの設定が開始された<br>`E_NOT_OK`: サービスの要求が受理されなかった |
| 説明 | このサービスは、CAN コントローラのボーレートのコンフィギュレーションを設定する。必要なボーレートの変更に応じて、コントローラをリセットしなければならない場合がある。 |

**[SWS_CANIF_00868]** サービス `CanIf_SetBaudrate()` は、要求された CAN コントローラについて、`Can_SetBaudrate(Controller, BaudRateConfigID)` を呼ばなければならない。

**[SWS_CANIF_00869]** `CanIf_SetBaudrate()` が無効な `ControllerId` で呼ばれた場合、CanIf は、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: `CanIf_SetBaudrate()` のパラメータ `BaudRateConfigID` は、CanIf では検査されない。これは、責任を持つ CanDrv が行わなければならない。

注: `CanIf_SetBaudrate()` の呼び出しのコンテキストは、タスクレベル（ポーリングモード）である。

**[SWS_CANIF_00871]** CanIf がボーレートの変更をサポートし、したがって `CanIf_SetBaudrate()` をサポートする場合、`CANIF_SET_BAUDRATE_API`（ECUC_CanIf_00838 を参照）でコンフィギュレーションできなければならない。

#### 8.3.25 CanIf_SetIcomConfiguration

**[SWS_CANIF_00861]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_SetIcomConfiguration` |
| 構文 | `Std_ReturnType CanIf_SetIcomConfiguration(uint8 ControllerId, IcomConfigIdType ConfigurationId)` |
| サービス ID [hex] | 0x25 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 異なるコントローラ Id でだけ再入可能 |
| 入力パラメータ | `ControllerId`: CAN コントローラに割り当てられた、抽象化された CanIf のコントローラ Id。<br>`ConfigurationId`: 要求されるコンフィギュレーション |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 要求が受理された<br>`E_NOT_OK`: 要求が拒否された |
| 説明 | このサービスは、CAN コントローラの Icom コンフィギュレーションを、要求されたものへ変更する。 |

> **注（再構成）**: 原典の表 8.29 は、テキストの抽出で行がずれていたため、項目の対応を復元した。

注: インタフェース `CanIf_SetIcomConfiguration()` は、Pretended Networking を有効にし、要求された ICOM のコンフィギュレーションを CAN ドライバを介してロードするために、CanSm から呼ばれる。

**[SWS_CANIF_00838]** サービス `CanIf_SetIcomConfiguration()` は、要求された ICOM のコンフィギュレーションを設定するために、要求された CanDrv の `Can_SetIcomConfiguration(Controller, ConfigurationId)` を呼ばなければならない。

**[SWS_CANIF_00872]** `CanIf_SetIcomConfiguration()` が無効な `ControllerId` で呼ばれた場合、CanIf は、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00875]** `CanIf_SetIcomConfiguration()` は、コンフィギュレーションパラメータ `CANIF_PUBLIC_ICOM_SUPPORT`（ECUC_CanIf_00839 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

### 8.4 コールバック通知

他のモジュールへ提供される関数の一覧である。

#### 8.4.1 CanIf_TriggerTransmit

**[SWS_CANIF_00883]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_TriggerTransmit` |
| 構文 | `Std_ReturnType CanIf_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x41 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信が要求される SDU の ID。 |
| 入出力パラメータ | `PduInfoPtr`: SDU のデータのコピー先のバッファへのポインタ（`SduDataPtr`）と、`SduLength` に利用できるバッファサイズを含む。戻るとき、サービスは、コピーした SDU のデータの長さを `SduLength` に示す。 |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: SDU がコピーされ、`SduLength` がコピーされたバイト数を示す。<br>`E_NOT_OK`: SDU のデータはコピーされていない。`PduInfoPtr` は、NULL ポインタや無効なデータを指している可能性があるため、使ってはならない。 |
| 説明 | この API の中で、上位層のモジュール（呼び出されるモジュール）は、利用できるデータが、`PduInfoPtr->SduLength` で報告されたバッファサイズに収まるかを検査しなければならない。収まる場合は、データを `PduInfoPtr->SduDataPtr` が与えるバッファへコピーし、実際にコピーしたデータの長さを `PduInfoPtr->SduLength` に更新しなければならない。収まらない場合は、`PduInfoPtr` を変更せずに `E_NOT_OK` を返す。 |

> **注（再構成）**: 原典の表 8.30〜8.39 は、すべて、テキストの抽出で行がずれていたため、項目の対応を復元した。

**[SWS_CANIF_00884]** CanIf は、トリガ送信のサポートが有効な場合（`CanIfTriggerTransmitSupport = TRUE`）に限り、API 関数 `CanIf_TriggerTransmit()` を提供しなければならない。

**[SWS_CANIF_00885]** 関数 `CanIf_TriggerTransmit()` は、対応する `<User_TriggerTransmit>()` 関数を、変換した `TxPduId` と、PduInfo 構造体へのポインタ（`PduInfoPtr`）を渡して呼ばなければならない。戻るとき、`CanIf_TriggerTransmit()` は、その `<User_TriggerTransmit>()` の戻り値を返さなければならない。

#### 8.4.2 CanIf_TxConfirmation

**[SWS_CANIF_00007]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_TxConfirmation` |
| 構文 | `void CanIf_TxConfirmation(PduIdType CanTxPduId)` |
| サービス ID [hex] | 0x13 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `CanTxPduId`: 正常に送信された CAN L-PDU の L-PDU ハンドル。この ID は、対応する CAN L-PDU ID と、暗黙のうちに CAN ドライバのインスタンス、および対応する CAN コントローラデバイスを指定する。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、CAN TxPDU の、以前に正常に処理された送信を確認する。 |

（SRS_Can_01009）

注: サービス `CanIf_TxConfirmation()` は、CanIf に実装され、CAN L-PDU が CAN ネットワークで送信された後に、CanDrv により呼ばれる。

注: CanDrv は、[14, ECU Configuration の仕様] に述べられた HandleId の概念をサポートしないため、サービス `CanIf_TxConfirmation()` の中で、CanDrv は、`Can_Write(Hth, *PduInfo)` から保持された `PduInfo->swPduHandle` を、`CanTxPduId` として使う。

**[SWS_CANIF_00391]** コンフィギュレーションパラメータ `CANIF_PUBLIC_READTXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00609）と、送信された L-PDU の `CANIF_TXPDU_READ_NOTIFYSTATUS`（ECUC_CanIf_00589）が `TRUE` に設定されていて、`CanIf_TxConfirmation()` が呼ばれた場合、CanIf は、送信された L-PDU の通知ステータスを設定しなければならない。

**[SWS_CANIF_00410]** `CanIf_TxConfirmation()` のパラメータ `CanTxPduId` が無効な値の場合、CanIf は、`CanIf_TxConfirmation()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_LPDU` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00412]** `CanIf_TxConfirmation()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_TxConfirmation()` が呼ばれたときに、サービス `<User_TxConfirmation>()` を呼んではならず、Tx 確認のステータスを設定してはならない。

注: `CanIf_TxConfirmation()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00414]** `CanIf_TxConfirmation()` のコンフィギュレーション: 各 Tx L-PDU（ECUC_CanIf_00248 を参照）は、`CanIf_TxConfirmation()` の中で呼ばれる、上位層のモジュールの対応する送信確認サービス（[SWS_CANIF_00011] を参照）でコンフィギュレーションしなければならない。

#### 8.4.3 CanIf_RxIndication

**[SWS_CANIF_00006]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_RxIndication` |
| 構文 | `void CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x14 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `Mailbox`: HRH と、対応する CAN コントローラを識別する。<br>`PduInfoPtr`: 受信した L-PDU へのポインタ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、すべてのフィルタと検証の検査を通過した後に、受信した CAN Rx L-PDU の受信の成功を、CanIf へ通知する。 |

注: サービス `CanIf_RxIndication()` は、CanIf に実装され、CAN L-PDU を受信した後に、CanDrv により呼ばれる。

**[SWS_CANIF_00415]** サービス `CanIf_RxIndication()` の中で、CanIf は、この通知を、コンフィギュレーションされた上位層の対象のサービスへ経路付けする。

**[SWS_CANIF_00392]** コンフィギュレーションパラメータ `CANIF_PUBLIC_READRXPDU_NOTIFY_STATUS_API`（ECUC_CanIf_00608）と、受信した L-PDU の `CANIF_RXPDU_READ_NOTIFYSTATUS`（ECUC_CanIf_00595）が `TRUE` に設定されていて、`CanIf_RxIndication()` が呼ばれた場合、CanIf は、受信した L-PDU の通知ステータスを設定しなければならない。

**[SWS_CANIF_00416]** `CanIf_RxIndication()` のパラメータ `Mailbox->Hoh` が無効な値の場合、CanIf は、`CanIf_RxIndication()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_HOH` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00417]** `CanIf_RxIndication()` のパラメータ `Mailbox->CanId` が無効な値の場合、CanIf は、`CanIf_RxIndication()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CANID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

注: `CanIf_RxIndication()` が、無効な `PduInfoPtr->SduLength` で呼ばれた場合、ランタイムエラー `CANIF_E_INVALID_DATA_LENGTH` が報告される（[SWS_CANIF_00168] を参照）。

**[SWS_CANIF_00419]** `CanIf_RxIndication()` のパラメータ `PduInfoPtr` または `Mailbox` が無効な値の場合、CanIf は、`CanIf_RxIndication()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_POINTER` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00421]** `CanIf_RxIndication()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_RxIndication()` が呼ばれたときに、受信通知の処理を実行してはならない。

注: `CanIf_RxIndication()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00423]** `CanIf_RxIndication()` のコンフィギュレーション: 各 Rx L-PDU（ECUC_CanIf_00249 を参照）は、`CanIf_RxIndication()` の中で呼ばれる、上位層のモジュールの対応する受信通知サービス（[SWS_CANIF_00012] を参照）でコンフィギュレーションしなければならない。

#### 8.4.4 CanIf_ControllerBusOff

**[SWS_CANIF_00218]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ControllerBusOff` |
| 構文 | `void CanIf_ControllerBusOff(uint8 ControllerId)` |
| サービス ID [hex] | 0x16 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `ControllerId`: バスオフが起きた CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、抽象化された CanIf の ControllerId を持つ、対応する CAN コントローラの、コントローラのバスオフのイベントを通知する。 |

注: コールバックサービス `CanIf_ControllerBusOff()` は、CanDrv により呼ばれ、CanIf に実装される。これは、CanDrv のモード変更の通知の場合に呼ばれる。

**[SWS_CANIF_00429]** `CanIf_ControllerBusOff()` のパラメータ `ControllerId` が無効な値の場合、CanIf は、`CanIf_ControllerBusOff()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00431]** `CanIf_ControllerBusOff()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_ControllerBusOff()` が呼ばれたときに、バスオフの通知を実行してはならない。

注: `CanIf_ControllerBusOff()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00433]** `CanIf_ControllerBusOff()` のコンフィギュレーション: CAN コントローラの ID は、CanIf のコンフィギュレーションの記述の中で公開される（ECUC_CanIf_00546 を参照）。

注: このサービスは、常に利用できなければならないため、対応するコンフィギュレーションパラメータは存在しない。

#### 8.4.5 CanIf_ConfirmPnAvailability

**[SWS_CANIF_00815]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ConfirmPnAvailability` |
| 構文 | `void CanIf_ConfirmPnAvailability(uint8 TransceiverId)` |
| サービス ID [hex] | 0x1a |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `TransceiverId`: PN の利用可能性が検査された CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、抽象化された CanIf の TransceiverId を持つ、対応する CAN トランシーバについて、トランシーバが PN の通信モードで動作していることを通知する。 |

**[SWS_CANIF_00753]** `CanIf_ConfirmPnAvailability()` が呼ばれた場合、CanIf は `<User_ConfirmPnAvailability>()` を呼ぶ。

注: CanIf は、渡されたパラメータ `TransceiverId` を、上位層のモジュールへ渡す。

**[SWS_CANIF_00816]** `CanIf_ConfirmPnAvailability()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_ConfirmPnAvailability()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

**[SWS_CANIF_00817]** `CanIf_ConfirmPnAvailability()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_ConfirmPnAvailability()` が呼ばれたときに、通知を実行してはならない。

注: `CanIf_ConfirmPnAvailability()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00754]** `CanIf_ConfirmPnAvailability()` のコンフィギュレーション: この関数は、コンフィギュレーションパラメータ `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

#### 8.4.6 CanIf_ClearTrcvWufFlagIndication

**[SWS_CANIF_00762]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ClearTrcvWufFlagIndication` |
| 構文 | `void CanIf_ClearTrcvWufFlagIndication(uint8 TransceiverId)` |
| サービス ID [hex] | 0x20 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `TransceiverId`: この関数が呼ばれた CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、抽象化された CanIf の TransceiverId を持つ、対応する CAN トランシーバについて、トランシーバが WufFlag をクリアしたことを通知する。 |

**[SWS_CANIF_00757]** `CanIf_ClearTrcvWufFlagIndication()` が呼ばれた場合、CanIf は `<User_ClearTrcvWufFlagIndication>()` を呼ぶ。

注: CanIf は、渡されたパラメータ `TransceiverId` を、上位層のモジュールへ渡す。

**[SWS_CANIF_00805]** `CanIf_ClearTrcvWufFlagIndication()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_ClearTrcvWufFlagIndication()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

**[SWS_CANIF_00806]** `CanIf_ClearTrcvWufFlagIndication()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_ClearTrcvWufFlagIndication()` が呼ばれたときに、通知を実行してはならない。

注: `CanIf_ClearTrcvWufFlagIndication()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00808]** `CanIf_ClearTrcvWufFlagIndication()` のコンフィギュレーション: この関数は、コンフィギュレーションパラメータ `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

#### 8.4.7 CanIf_CheckTrcvWakeFlagIndication

**[SWS_CANIF_00763]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_CheckTrcvWakeFlagIndication` |
| 構文 | `void CanIf_CheckTrcvWakeFlagIndication(uint8 TransceiverId)` |
| サービス ID [hex] | 0x21 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `TransceiverId`: この関数が呼ばれた CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、抽象化された CanIf の TransceiverId を持つ、対応する CAN トランシーバにより、トランシーバのウェイクアップフラグの検査が終わったことを通知する。この通知は、トランシーバの非同期の通信に対応するために使われる。 |

**[SWS_CANIF_00759]** `CanIf_CheckTrcvWakeFlagIndication()` が呼ばれた場合、CanIf は `<User_CheckTrcvWakeFlagIndication>()` を呼ぶ。

注: CanIf は、渡されたパラメータ `TransceiverId` を、上位層のモジュールへ渡す。

**[SWS_CANIF_00809]** `CanIf_CheckTrcvWakeFlagIndication()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_CheckTrcvWakeFlagIndication()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

**[SWS_CANIF_00810]** `CanIf_CheckTrcvWakeFlagIndication()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_CheckTrcvWakeFlagIndication()` が呼ばれたときに、通知を実行してはならない。

注: `CanIf_CheckTrcvWakeFlagIndication()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00812]** `CanIf_CheckTrcvWakeFlagIndication()` のコンフィギュレーション: この関数は、コンフィギュレーションパラメータ `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

#### 8.4.8 CanIf_ControllerModeIndication

**[SWS_CANIF_00699]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_ControllerModeIndication` |
| 構文 | `void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)` |
| サービス ID [hex] | 0x17 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `ControllerId`: 状態が遷移した CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。<br>`ControllerMode`: CAN コントローラが遷移したモード |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、抽象化された CanIf の ControllerId を持つ、対応する CAN コントローラの、コントローラの状態遷移を通知する。 |

注: コールバックサービス `CanIf_ControllerModeIndication()` は、CanDrv により呼ばれ、CanIf に実装される。これは、CanDrv の状態遷移の通知の場合に呼ばれる。

**[SWS_CANIF_00700]** `CanIf_ControllerModeIndication()` のパラメータ `ControllerId` が無効な値の場合、CanIf は、`CanIf_ControllerModeIndication()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

**[SWS_CANIF_00702]** `CanIf_ControllerModeIndication()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_ControllerModeIndication()` が呼ばれたときに、状態遷移の通知を実行してはならない。

注: `CanIf_ControllerModeIndication()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

#### 8.4.9 CanIf_TrcvModeIndication

**[SWS_CANIF_00764]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_TrcvModeIndication` |
| 構文 | `void CanIf_TrcvModeIndication(uint8 TransceiverId, CanTrcv_TrcvModeType TransceiverMode)` |
| サービス ID [hex] | 0x22 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `TransceiverId`: 状態が遷移した CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。<br>`TransceiverMode`: CAN トランシーバが遷移したモード |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、抽象化された CanIf の TransceiverId を持つ、対応する CAN トランシーバの、トランシーバの状態遷移を通知する。 |

注: コールバックサービス `CanIf_TrcvModeIndication()` は、CanDrv により呼ばれ、CanIf に実装される。これは、CanDrv の状態遷移の通知の場合に呼ばれる。

**[SWS_CANIF_00706]** `CanIf_TrcvModeIndication()` のパラメータ `TransceiverId` が無効な値の場合、CanIf は、`CanIf_TrcvModeIndication()` が呼ばれたときに、開発エラーコード `CANIF_E_PARAM_TRCV` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。

**[SWS_CANIF_00708]** `CanIf_TrcvModeIndication()` を呼ぶ前に CanIf が初期化されていなかった場合、CanIf は、`CanIf_TrcvModeIndication()` が呼ばれたときに、状態遷移の通知を実行してはならない。

注: `CanIf_TrcvModeIndication()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00710]** `CanIf_TrcvModeIndication()` のコンフィギュレーション: CAN トランシーバの ID は、パラメータ `CANIF_TRCV_ID`（ECUC_CanIf_00654 を参照）を介して、CanIf のコンフィギュレーションの記述の中で公開される。

**[SWS_CANIF_00730]** `CanIf_TrcvModeIndication()` のコンフィギュレーション: トランシーバがサポートされていない場合（`CanIfTrcvDrvCfg` がコンフィギュレーションされていない。ECUC_CanIf_00273 を参照）、`CanIf_TrcvModeIndication()` は、CanIf により提供されてはならない。

#### 8.4.10 CanIf_CurrentIcomConfiguration

**[SWS_CANIF_00862]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanIf_CurrentIcomConfiguration` |
| 構文 | `void CanIf_CurrentIcomConfiguration(uint8 ControllerId, IcomConfigIdType ConfigurationId, IcomSwitch_ErrorType Error)` |
| サービス ID [hex] | 0x26 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なるコントローラ Id でだけ再入可能 |
| 入力パラメータ | `ControllerId`: コンフィギュレーション Id を通知する CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。<br>`ConfigurationId`: 有効なコンフィギュレーション Id。<br>`Error`: `ICOM_SWITCH_E_OK`: エラーなし。`ICOM_SWITCH_E_FAILED`: 要求されたコンフィギュレーションへの切り替えが失敗した。重大なエラー。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、抽象化された CanIf の ControllerId を使って、CAN コントローラの Icom コンフィギュレーションの変更を通知する。 |

注: インタフェース `CanIf_CurrentIcomConfiguration()` は、CanDrv が、指定されたチャネルの Pretended Networking の有効化または無効化の状況を、CanIf へ通知するために使う。

**[SWS_CANIF_00839]** `CanIf_CurrentIcomConfiguration()` が呼ばれた場合、CanIf は、ICOM の現在の状況を CanSM へ通知するために、`CanSM_CurrentIcomConfiguration(ControllerId, ConfigurationId, Error)` を呼ばなければならない。

**[SWS_CANIF_00873]** `CanIf_CurrentIcomConfiguration()` が無効な `ControllerId` で呼ばれた場合、CanIf は、開発エラーコード `CANIF_E_PARAM_CONTROLLERID` を DET モジュールの `Det_ReportError` サービスへ報告しなければならない。（SRS_BSW_00323）

**[SWS_CANIF_00876]** `CanIf_CurrentIcomConfiguration()` は、コンフィギュレーションパラメータ `CANIF_PUBLIC_ICOM_SUPPORT`（ECUC_CanIf_00839 を参照）で、プリコンパイル時に ON/OFF をコンフィギュレーションできなければならない。

### 8.5 スケジュールされる関数

注: CanIf には、スケジュールされる関数は無く、必要としない。

### 8.6 期待するインタフェース

この章では、他のモジュールに要求されるすべてのインタフェースを列挙する。

#### 8.6.1 必須のインタフェース

注: この項は、モジュールの中核の機能を満たすために必要な、すべてのインタフェースを定義する。

**[SWS_CANIF_00040]**

| API 関数 | 説明 |
|---|---|
| `Can_SetControllerMode` | この関数は、CAN コントローラの状態機械の、ソフトウェアで起動される状態遷移を行う。 |
| `Can_Write` | この関数は、送信のために CAN メッセージを CanDrv へ渡すために、CanIf から呼ばれる。 |
| `Det_ReportRuntimeError` | ランタイムエラーを報告するサービス。コールアウトがコンフィギュレーションされていれば、このコールアウトが呼ばれなければならない。 |
| `SchM_Enter_CanIf_<ExclusiveArea>` | モジュールのローカルな排他領域に入るために、`SchM_Enter` 関数を呼ぶ。 |
| `SchM_Exit_CanIf_<ExclusiveArea>` | 排他領域から出るために、`SchM_Exit` 関数を呼ぶ。 |

表 8.40: CanIf の必須のインタフェース

#### 8.6.2 オプションのインタフェース

この項は、モジュールのオプションの機能を満たすために必要な、すべてのインタフェースを定義する。

**[SWS_CANIF_00294]**

> **注（再構成）**: 原典の表 8.41 は、テキストの抽出で、API 関数の名前の列と、説明の列が、別々に並んでいた。API 関数の並びと、説明の並びの順序から、対応を復元した。同じ説明（受信通知、送信確認）が、`CanNm`、`CanTp`、`J1939Nm`、`J1939Tp`、`PduR`、`Xcp` について繰り返されている。

| API 関数 | 説明 |
|---|---|
| `Can_CheckWakeup` | この関数は、指定されたコントローラについて、ウェイクアップが起きたかを検査する。 |
| `Can_SetBaudrate` | このサービスは、CAN コントローラのボーレートのコンフィギュレーションを設定する。必要なボーレートの変更に応じて、コントローラをリセットしなければならない場合がある。 |
| `Can_SetIcomConfiguration` | このサービスは、CAN コントローラの Icom コンフィギュレーションを、要求されたものへ変更する。 |
| `CanNm_RxIndication` | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |
| `CanNm_TxConfirmation` | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |
| `CanSM_CheckTransceiverWakeFlagIndication` | このコールバック関数は、通知された CAN トランシーバについて、`CanIf_CheckTrcvWakeFlag` API の処理の終了を示す。 |
| `CanSM_ClearTrcvWufFlagIndication` | このコールバック関数は、通知された CAN トランシーバについて、`CanIf_ClearTrcvWufFlag` API の処理の終了を示す。 |
| `CanSM_ConfirmPnAvailability` | このコールバック関数は、トランシーバが PN の通信モードで動作していることを示す。 |
| `CanSM_ControllerBusOff` | このコールバック関数は、ある CAN コントローラでのバスオフのイベントを、影響を受ける CAN ネットワークについて指定されたバスオフ回復の扱いで考慮する必要があるものとして、CanSM へ通知する。 |
| `CanSM_ControllerModeIndication` | このコールバックは、CAN コントローラのモードの変更を、CanSM モジュールへ通知する。 |
| `CanSM_CurrentIcomConfiguration` | このサービスは、CAN ネットワークの Icom コンフィギュレーションの変更を通知する。 |
| `CanSM_TransceiverModeIndication` | このコールバックは、CAN トランシーバのモードの変更を、CanSM モジュールへ通知する。 |
| `CanTp_RxIndication` | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |
| `CanTp_TxConfirmation` | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |
| `CanTrcv_CheckWakeFlag` | トランシーバのハードウェアから、ウェイクアップフラグのステータスを検査するよう要求する。 |
| `CanTrcv_CheckWakeup` | ウェイクアップ割り込みが検出された場合に、基盤の CANIF により呼ばれるサービス。 |
| `CanTrcv_GetBusWuReason` | トランシーバのウェイクアップの理由を取得し、パラメータ `Reason` で返す。 |
| `CanTrcv_GetOpMode` | トランシーバのモードを取得し、`OpMode` で返す。 |
| `CanTrcv_SetOpMode` | トランシーバのモードを、値 `OpMode` に設定する。 |
| `CanTrcv_SetWakeupMode` | `TrcvWakeupMode` に従って、トランシーバのウェイクアップイベントを、有効、無効、またはクリアする。 |
| `Det_ReportError` | 開発エラーを報告するサービス。 |
| `EcuM_ValidateWakeupEvent` | ウェイクアップの後、ECU State Manager は、ウェイクアップイベントの検証を待つために、WAKEUP VALIDATION の状態／シーケンスの間、処理を止める。この API サービスは、`sources` パラメータで示されたウェイクアップイベントが検証されたことを、ECU Manager モジュールへ示すために使われる。 |
| `J1939Nm_RxIndication` | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |
| `J1939Nm_TxConfirmation` | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |
| `J1939Tp_RxIndication` | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |
| `J1939Tp_TxConfirmation` | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |
| `PduR_CanIfRxIndication` | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |
| `PduR_CanIfTxConfirmation` | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |
| `Xcp_CanIfRxIndication` | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |
| `Xcp_CanIfTxConfirmation` | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |

表 8.41: CanIf のオプションのインタフェース

#### 8.6.3 コンフィギュレーションできるインタフェース

この項では、呼ばれる上位層の対象の関数を、コンフィギュレーションで設定しなければならない、すべてのインタフェースを列挙する。これらのコールバックサービスは、AUTOSAR の BSW のアーキテクチャに従って CanIf を使う、上位の通信モジュールで規定され、実装される。特定のコールバック通知は、対応する SWS の文書で規定される（第 3 章「関連文書」を参照）。

インタフェース名が必須と規定されていない限り、API 名がコンフィギュレーションされていなければ、コールバックは行われない。この項は、コールバックの通知の内容、CanIf の中の呼び出しのコンテキスト、および呼び出しのイベントによる正確な時刻だけを説明する。

`<User_NotificationName>` — この条件は、上位層に実装され、CanIf から呼ばれる、そのようなインタフェースサービスに適用される。この条件は、対応する上位層のモジュールの、コールバックサービスの機能グループのシンボリックな名前を表す。各上位層のモジュールは、同じ機能（すなわち、送信確認）について、コールバックサービスを、定義しなくても、1 つでも、複数でも定義できる。振り分けは、L-SDU ID により保証される。

上位層のモジュールが、次の関数のサービス ID を提供する。

##### 8.6.3.1 `<User_TriggerTransmit>`

**[SWS_CANIF_00886]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_TriggerTransmit>` |
| 構文 | `Std_ReturnType <User_TriggerTransmit>(PduIdType TxPduId, PduInfoType* PduInfoPtr)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信が要求される SDU の ID。 |
| 入出力パラメータ | `PduInfoPtr`: SDU のデータのコピー先のバッファへのポインタ（`SduDataPtr`）と、`SduLength` に利用できるバッファサイズを含む。戻るとき、サービスは、コピーした SDU のデータの長さを `SduLength` に示す。 |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: SDU がコピーされ、`SduLength` がコピーされたバイト数を示す。<br>`E_NOT_OK`: SDU のデータはコピーされていない。`PduInfoPtr` は、NULL ポインタや無効なデータを指している可能性があるため、使ってはならない。 |
| 説明 | この API の中で、上位層のモジュール（呼び出されるモジュール）は、利用できるデータが、`PduInfoPtr->SduLength` で報告されたバッファサイズに収まるかを検査しなければならない。収まる場合は、データを `PduInfoPtr->SduDataPtr` が与えるバッファへコピーし、実際にコピーしたデータの長さを `PduInfoPtr->SduLength` に更新しなければならない。収まらない場合は、`PduInfoPtr` を変更せずに `E_NOT_OK` を返す。 |

> **注（再構成）**: 原典の表 8.42〜8.51 は、すべて、テキストの抽出で行がずれていたため、項目の対応を復元した。

注: このコールバックサービスは、CanIf により呼ばれ、対応する上位層のモジュールに実装される。これは、CanDrv のトリガ送信の要求の場合に呼ばれる。

注: `<User_TriggerTransmit>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00888]** `<User_TriggerTransmit>()` のコンフィギュレーション: TriggerTransmit のコールバックサービスを提供する上位層のモジュールは、`CanIfTxPduUserTxConfirmationUL`（`CanIfTxPduUserTxConfirmationUL` を参照）でコンフィギュレーションしなければならない。上位層のモジュールがコンフィギュレーションされていない場合、TriggerTransmit のコールバックサービスは実行されず、したがって、その PDU ではトリガ送信の機能はサポートされない。

**[SWS_CANIF_00889]** `<User_TriggerTransmit>()` のコンフィギュレーション: CanIf から呼ばれる API `<User_TriggerTransmit>()` の名前は、パラメータ `CanIfTxPduUserTriggerTransmitName`（`CanIfTxPduUserTriggerTransmitName` を参照）で、CanIf にコンフィギュレーションしなければならない。

注: `CanIfTxPduTriggerTransmit` が指定されていないか `FALSE` の場合、トリガ送信のために上位層のモジュールをコンフィギュレーションする必要は無い。したがって、`<User_TriggerTransmit>()` は呼ばれず、`CanIfTxPduUserTxConfirmationUL` と `CanIfTxPduUserTriggerTransmitName` もコンフィギュレーションする必要は無い。

**[SWS_CANIF_00890]** `<User_TriggerTransmit>()` のコンフィギュレーション: `CanIfTxPduUserTxConfirmationUL` が `PDUR` に設定されている場合、`CanIfTxPduUserTriggerTransmitName` は `PduR_CanIfTriggerTransmit` でなければならない。

**[SWS_CANIF_00891]** `<User_TriggerTransmit>()` のコンフィギュレーション: `CanIfTxPduUserTxConfirmationUL` が `CDD` に設定されている場合、API `<User_TriggerTransmit>()` の名前は、パラメータ `CanIfTxPduUserTriggerTransmitName` でコンフィギュレーションしなければならない。

##### 8.6.3.2 `<User_TxConfirmation>`

**[SWS_CANIF_00011]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_TxConfirmation>` |
| 構文 | `void <User_TxConfirmation>(PduIdType TxPduId, Std_ReturnType result)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信された PDU の ID。<br>`result`: `E_OK`: PDU が送信された。`E_NOT_OK`: PDU の送信に失敗した。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |

注: このコールバックサービスは、CanIf により呼ばれ、対応する上位層のモジュールに実装される。これは、CanDrv の送信確認の場合に呼ばれる。

注: この種類の確認のコールバックサービスは、主に PduR、CanNm、CanTp のために設計されているが、それに限らない。

注: パラメータ `TxPduId` は、`<User>` のコンフィギュレーションから導かれる。

注: `<User_TxConfirmation>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00438]** `<User_TxConfirmation>()` のコンフィギュレーション: このコールバックサービスを提供する上位層のモジュールは、`CanIfTxPduUserTxConfirmationUL`（ECUC_CanIf_00527 を参照）でコンフィギュレーションしなければならない。`<User_TxConfirmation>()` を使った送信確認のために、上位層のモジュールがコンフィギュレーションされていない場合、送信確認は実行されない。

**[SWS_CANIF_00542]** `<User_TxConfirmation>()` のコンフィギュレーション: CanIf から呼ばれる API `<User_TxConfirmation>()` の名前は、パラメータ `CanIfTxPduUserTxConfirmationName`（ECUC_CanIf_00528 を参照）で、CanIf にコンフィギュレーションしなければならない。

注: 送信確認が必要ない場合、または送信確認のために上位層のモジュールがコンフィギュレーションされておらず、したがって `<User_TxConfirmation>()` を呼ばない場合、`CanIfTxPduUserTxConfirmationUL` と `CanIfTxPduUserTxConfirmationName` をコンフィギュレーションする必要は無い。

次の規則は、`CanIfTxPduUserTxConfirmationUL` の設定値に対する、`CanIfTxPduUserTxConfirmationName` の必須の値を定める。

| 条項 | `CanIfTxPduUserTxConfirmationUL` | `CanIfTxPduUserTxConfirmationName` |
|---|---|---|
| **[SWS_CANIF_00439]** | `PDUR` | `PduR_CanIfTxConfirmation` でなければならない。 |
| **[SWS_CANIF_00543]** | `CAN_NM` | `CanNm_TxConfirmation` でなければならない。 |
| **[SWS_CANIF_00858]** | `J1939NM` | `J1939Nm_TxConfirmation` でなければならない。 |
| **[SWS_CANIF_00544]** | `J1939TP` | `J1939Tp_TxConfirmation` でなければならない。 |
| **[SWS_CANIF_00550]** | `CAN_TP` | `CanTp_TxConfirmation` でなければならない。 |
| **[SWS_CANIF_00556]** | `XCP` | `Xcp_CanIfTxConfirmation` でなければならない。 |
| **[SWS_CANIF_00551]** | `CDD` | API `<User_TxConfirmation>()` の名前を、パラメータ `CanIfTxPduUserTxConfirmationName` でコンフィギュレーションしなければならない。 |
| **[SWS_CANIF_00879]** | `CAN_TSYN` | `CanTSyn_CanIfTxConfirmation` でなければならない。 |

ヒント（別のモジュールへの依存）: 少なくとも 1 つの CanIf Tx L-SDU が `CanNm_TxConfirmation()` でコンフィギュレーションされている場合、すなわち `CanIfTxPduUserTxConfirmationUL` が `CAN_NM` に等しい場合、CanNm のコンフィギュレーションパラメータ `CANNM_IMMEDIATE_TXCONF_ENABLED` は `FALSE` に設定しなければならない（CanNm に関する詳細は、[4, CAN Network Management の仕様]、[SWS_CANNM_00284] を参照）。

##### 8.6.3.3 `<User_RxIndication>`

**[SWS_CANIF_00012]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_RxIndication>` |
| 構文 | `void <User_RxIndication>(PduIdType RxPduId, const PduInfoType* PduInfoPtr)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `RxPduId`: 受信した PDU の ID。<br>`PduInfoPtr`: 受信した PDU の長さ（`SduLength`）、PDU を含むバッファへのポインタ（`SduDataPtr`）、およびこの PDU に関するメタデータを含む。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |

（SRS_Can_01003）

注: このサービスは、すべてのフィルタと検証の検査を通過した後に、L-SDU の受信の成功を、上位層のモジュールへ通知する。

注: このコールバックサービスは、CanIf により呼ばれ、コンフィギュレーションされた上位層のモジュール（例: PduR、CanNm、CanTp など）に、そのようにコンフィギュレーションされていれば（ECUC_CanIf_00529 を参照）、実装される。

注: `<User_RxIndication>()` が戻るまで、CanIf は `<PduInfoPtr>` にアクセスしない。`<PduInfoPtr>` は、通知が戻るまでの間だけ有効で、上位層が使える。CanIf は、この `<PduInfoPtr>` について、コンフィギュレーションされたバイト数が有効であることを保証する。

注: `<User_RxIndication>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

**[SWS_CANIF_00441]** `<User_RxIndication>()` のコンフィギュレーション: このコールバックサービスを提供する上位層のモジュールは、`CANIF_RXPDU_USERRXINDICATION_UL`（ECUC_CanIf_00529 を参照）でコンフィギュレーションしなければならない。

**[SWS_CANIF_00552]** `<User_RxIndication>()` のコンフィギュレーション: CanIf から呼ばれる API `<User_RxIndication>()` の名前は、パラメータ `CANIF_RXPDU_USERRXINDICATION_NAME`（ECUC_CanIf_00530 を参照）で、CanIf にコンフィギュレーションしなければならない。

注: 受信通知が必要ない場合、または受信通知のために上位層のモジュールがコンフィギュレーションされておらず、したがって `<User_RxIndication>()` を呼ばない場合、`CANIF_RXPDU_USERRXINDICATION_UL` と `CANIF_RXPDU_USERRXINDICATION_NAME` をコンフィギュレーションする必要は無い。

次の規則は、`CANIF_RXPDU_USERRXINDICATION_UL` の設定値に対する、`CANIF_RXPDU_USERRXINDICATION_NAME` の必須の値を定める。

| 条項 | `CANIF_RXPDU_USERRXINDICATION_UL` | `CANIF_RXPDU_USERRXINDICATION_NAME` |
|---|---|---|
| **[SWS_CANIF_00442]** | `PDUR` | `PduR_CanIfRxIndication` でなければならない。 |
| **[SWS_CANIF_00445]** | `CAN_NM` | `CanNm_RxIndication` でなければならない。 |
| **[SWS_CANIF_00859]** | `J1939NM` | `J1939Nm_RxIndication` でなければならない。 |
| **[SWS_CANIF_00448]** | `CAN_TP` | `CanTp_RxIndication` でなければならない。 |
| **[SWS_CANIF_00554]** | `J1939TP` | `J1939Tp_RxIndication` でなければならない。 |
| **[SWS_CANIF_00555]** | `XCP` | `Xcp_CanIfRxIndication` でなければならない。 |
| **[SWS_CANIF_00557]** | `CDD` | API の名前を、パラメータ `CANIF_RXPDU_USERRXINDICATION_NAME` でコンフィギュレーションしなければならない。 |
| **[SWS_CANIF_00880]** | `CAN_TSYN` | `CanTSyn_CanIfRxIndication` でなければならない。 |

[SWS_CANIF_00445] の補足: API パラメータ `CanNmRxPduId` で CanNm へ渡される値は、CanNm モジュールの中の CanNm チャネルのハンドルを参照する（CanNm に関する詳細は、[4, CAN Network Management の仕様] を参照）。

##### 8.6.3.4 `<User_ValidateWakeupEvent>`

**[SWS_CANIF_00532]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_ValidateWakeupEvent>` |
| 構文 | `void <User_ValidateWakeupEvent>(EcuM_WakeupSourceType sources)` |
| 同期／非同期 | （提供する上位層のモジュールの中で定義される） |
| 再入可能性 | （提供する上位層のモジュールの中で定義される） |
| 入力パラメータ | `sources`: 検証された CAN のウェイクアップイベント。各 CAN コントローラまたは CAN トランシーバが、別々のウェイクアップソースになりうる。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、CAN ドライバまたは CAN Transceiver Driver モジュールへの以前の要求の後に、ウェイクアップソース（CAN コントローラまたはトランシーバ）から起こされたウェイクアップのイベントが、有効かを示す。 |

注: このコールバックサービスは、主に ECU State Manager モジュールに実装され、使われる（[13, ECU State Manager の仕様] を参照）。

注: CanIf がこのコールバックサービスを呼ぶ。これは、コンフィギュレーションされた上位層のモジュールにより実装される。これは、対応する CAN コントローラで、ウェイクアップのイベントの後の、最初の CAN L-PDU の受信のイベントが起きた場合の、`CanIf_CheckValidation()` の呼び出しの間にだけ、呼ばれる。

注: `<User_ValidateWakeupEvent>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

注: コールバックサービス `<User_ValidateWakeupEvent>()` は、一般に、複数の CAN コントローラの使用では再入可能だが、同じ CAN コントローラでは再入不可である。

**[SWS_CANIF_00659]** `<User_ValidateWakeupEvent>()` のコンフィギュレーション: 検証が不要な場合、この API は、`CANIF_PUBLIC_WAKEUP_CHECK_VALIDATION_SUPPORT`（ECUC_CanIf_00611 を参照）を無効にして省略できる。

**[SWS_CANIF_00456]** `<User_ValidateWakeupEvent>()` のコンフィギュレーション: このコールバックサービスを提供する上位層のモジュールは、`CanIfDispatchUserValidateWakeupEventUL`（ECUC_CanIf_00549 を参照）でコンフィギュレーションしなければならない。ただし、

- `<User_ValidateWakeupEvent>()` を使ったウェイクアップの通知のために、上位層のモジュールがコンフィギュレーションされていない場合、ウェイクアップの通知をコンフィギュレーションする必要は無い。`CanIfDispatchUserValidateWakeupEventUL` をコンフィギュレーションする必要は無い。
- ウェイクアップがサポートされていない場合（`CANIF_CTRL_WAKEUP_SUPPORT` と `CANIF_TRCV_WAKEUP_SUPPORT` が `FALSE` に等しい。ECUC_CanIf_00637、ECUC_CanIf_00606 を参照）、`CanIfDispatchUserValidateWakeupEventUL` はコンフィギュレーションできない。

**[SWS_CANIF_00563]** `<User_ValidateWakeupEvent>()` のコンフィギュレーション: `CanIfDispatchUserValidateWakeupEventUL` が `ECUM` に設定されている場合、`CanIfDispatchUserValidateWakeupEventName` は `EcuM_ValidateWakeupEvent` でなければならない。

**[SWS_CANIF_00564]** `<User_ValidateWakeupEvent>()` のコンフィギュレーション: `CanIfDispatchUserValidateWakeupEventUL` が `CDD` に設定されている場合、API の名前は、パラメータ `CanIfDispatchUserValidateWakeupEventName` でコンフィギュレーションしなければならない。

##### 8.6.3.5 `<User_ControllerBusOff>`

**[SWS_CANIF_00014]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_ControllerBusOff>` |
| 構文 | `void <User_ControllerBusOff>(uint8 ControllerId)` |
| 同期／非同期 | （提供する上位層のモジュールの中で定義される） |
| 再入可能性 | （提供する上位層のモジュールの中で定義される） |
| 入力パラメータ | `ControllerId`: バスオフが起きた CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、バスオフのイベントを、対応する上位層のモジュール（主に CAN State Manager モジュール）へ通知する。 |

（SRS_Can_01029）

注: このコールバックサービスは、主に CanSm に実装され、使われる（[3, CAN State Manager の仕様] を参照）。

注: このコールバックサービスは、CanIf により呼ばれ、コンフィギュレーションされた上位層のモジュールにより実装される。これは、CanDrv の `CanIf_ControllerBusOff()` を介したバスオフの通知の場合に呼ばれる。サービス `CanIf_ControllerBusOff()` で渡されたパラメータ `ControllerId` は、上位層のモジュールへ渡される。

注: `<User_ControllerBusOff>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

注: コールバックサービス `<User_ControllerBusOff>()` は、一般に、複数の CAN コントローラの使用では再入可能だが、同じ CAN コントローラでは再入不可である。

注: バスオフ回復の間の、再初期化／再開が実行される前に、CAN コントローラで複数のバスオフのイベントがあった場合でも、`<User_ControllerBusOff>()` は 1 回だけ実行される。

`<User_ControllerBusOff>()` のコンフィギュレーション:

**[SWS_CANIF_00450]** `<User_ControllerBusOff>()` のコンフィギュレーション: このコールバックサービスを提供する上位層のモジュールは、`CanIfDispatchUserCtrlBusOffUL`（ECUC_CanIf_00547 を参照）でコンフィギュレーションしなければならない。

**[SWS_CANIF_00558]** `<User_ControllerBusOff>()` のコンフィギュレーション: CanIf から呼ばれる API `<User_ControllerBusOff>()` の名前は、パラメータ `CanIfDispatchUserCtrlBusOffName`（ECUC_CanIf_00525 を参照）で、CanIf にコンフィギュレーションしなければならない。

**[SWS_CANIF_00524]** `<User_ControllerBusOff>()` のコンフィギュレーション: `CanIf_ControllerBusOff()` の呼び出しが起きた場合に、CanIf が呼べる、少なくとも 1 つの上位層のモジュール、したがって `<User_ControllerBusOff>()` の API を、必ずコンフィギュレーションしなければならない。

**[SWS_CANIF_00559]** `<User_ControllerBusOff>()` のコンフィギュレーション: `CanIfDispatchUserCtrlBusOffUL` が `CAN_SM` に設定されている場合、`CanIfDispatchUserCtrlBusOffName` は `CanSM_ControllerBusOff` でなければならない。

**[SWS_CANIF_00560]** `<User_ControllerBusOff>()` のコンフィギュレーション: `CanIfDispatchUserCtrlBusOffUL` が `CDD` に設定されている場合、API の名前は、パラメータ `CanIfDispatchUserCtrlBusOffName` でコンフィギュレーションしなければならない。

##### 8.6.3.6 `<User_ConfirmPnAvailability>`

**[SWS_CANIF_00821]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_ConfirmPnAvailability>` |
| 構文 | `void <User_ConfirmPnAvailability>(uint8 TransceiverId)` |
| 同期／非同期 | （提供する上位層のモジュールの中で定義される） |
| 再入可能性 | （提供する上位層のモジュールの中で定義される） |
| 入力パラメータ | `TransceiverId`: PN の利用可能性が検査された CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、CAN トランシーバが PN の通信モードで動作していることを通知する。 |

注: このコールバックサービスは、主に CanSm に実装され、使われる（[3, CAN State Manager の仕様] を参照）。

注: `<User_ConfirmPnAvailability>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

注: コールバックサービス `<User_ConfirmPnAvailability>()` は、一般に、複数の CAN コントローラの使用では再入可能だが、同じ CAN コントローラでは再入不可である。

**[SWS_CANIF_00823]** `<User_ConfirmPnAvailability>()` のコンフィギュレーション: `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）が True に等しい場合、呼ばれる上位層のモジュール（[SWS_CANIF_00753] を参照）は、`CanIfDispatchUserConfirmPnAvailabilityUL`（ECUC_CanIf_00820 を参照）でコンフィギュレーションできなければならない。

**[SWS_CANIF_00824]** `<User_ConfirmPnAvailability>()` のコンフィギュレーション: `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）が True に等しい場合、`<User_ConfirmPnAvailability>()` の名前は、`CanIfDispatchUserConfirmPnAvailabilityName`（ECUC_CanIf_00819 を参照）でコンフィギュレーションできなければならない。

**[SWS_CANIF_00825]** `<User_ConfirmPnAvailability>()` のコンフィギュレーション: CanIf がこのサービスをサポートするかどうかは、`CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）でコンフィギュレーションできなければならない（False: サポートしない、True: サポートする）。

**[SWS_CANIF_00826]** `<User_ConfirmPnAvailability>()` のコンフィギュレーション: `CanIfDispatchUserConfirmPnAvailabilityUL` が `CAN_SM` に設定されている場合、`CanIfDispatchUserConfirmPnAvailabilityName` は `CanSM_ConfirmPnAvailability` でなければならない。

**[SWS_CANIF_00827]** `<User_ConfirmPnAvailability>()` のコンフィギュレーション: `CanIfDispatchUserConfirmPnAvailabilityUL` が `CDD` に設定されている場合、サービスの名前は、パラメータ `CanIfDispatchUserConfirmPnAvailabilityName` でコンフィギュレーションできなければならない。

##### 8.6.3.7 `<User_ClearTrcvWufFlagIndication>`

**[SWS_CANIF_00788]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_ClearTrcvWufFlagIndication>` |
| 構文 | `void <User_ClearTrcvWufFlagIndication>(uint8 TransceiverId)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `TransceiverId`: この関数が呼ばれた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、CAN トランシーバが WufFlag をクリアしたことを通知する。この関数は、`CanIf_ClearTrcvWufFlagIndication` の中で呼ばれる。 |

注: このコールバックサービスは、主に CanSm に実装され、使われる（[3, CAN State Manager の仕様] を参照）。

注: `<User_ClearTrcvWufFlagIndication>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

注: コールバックサービス `<User_ClearTrcvWufFlagIndication>()` は、一般に、複数の CAN コントローラの使用では再入可能だが、同じ CAN コントローラでは再入不可である。

**[SWS_CANIF_00794]** `<User_ClearTrcvWufFlagIndication>()` のコンフィギュレーション: `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）が True に等しい場合、呼ばれる上位層のモジュール（[SWS_CANIF_00757] を参照）は、`CanIfDispatchUserClearTrcvWufFlagIndicationUL`（ECUC_CanIf_00790 を参照）でコンフィギュレーションできなければならない。

**[SWS_CANIF_00795]** `<User_ClearTrcvWufFlagIndication>()` のコンフィギュレーション: `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）が True に等しい場合、`<User_ClearTrcvWufFlagIndication>()` の名前は、`CanIfDispatchUserClearTrcvWufFlagIndicationName`（ECUC_CanIf_00789 を参照）でコンフィギュレーションできなければならない。

**[SWS_CANIF_00796]** `<User_ClearTrcvWufFlagIndication>()` のコンフィギュレーション: CanIf がこのサービスをサポートするかどうかは、`CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）でコンフィギュレーションできなければならない（False: サポートしない、True: サポートする）。

**[SWS_CANIF_00797]** `<User_ClearTrcvWufFlagIndication>()` のコンフィギュレーション: `CanIfDispatchUserClearTrcvWufFlagIndicationUL` が `CAN_SM` に設定されている場合、`CanIfDispatchUserClearTrcvWufFlagIndicationName` は `CanSM_ClearTrcvWufFlagIndication` でなければならない。

**[SWS_CANIF_00798]** `<User_ClearTrcvWufFlagIndication>()` のコンフィギュレーション: `CanIfDispatchUserClearTrcvWufFlagIndicationUL` が `CDD` に設定されている場合、サービスの名前は、パラメータ `CanIfDispatchUserClearTrcvWufFlagIndicationName` でコンフィギュレーションできなければならない。

##### 8.6.3.8 `<User_CheckTrcvWakeFlagIndication>`

**[SWS_CANIF_00814]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_CheckTrcvWakeFlagIndication>` |
| 構文 | `void <User_CheckTrcvWakeFlagIndication>(uint8 TransceiverId)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `TransceiverId`: この関数が呼ばれた、抽象化された CanIf の TransceiverId。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、CAN トランシーバのウェイクアップフラグがセットされていることを通知する。この関数は、`CanIf_CheckTrcvWakeFlagIndication` の中で呼ばれる。 |

注: このコールバックサービスは、主に CanSm に実装され、使われる（[3, CAN State Manager の仕様] を参照）。

注: `<User_CheckTrcvWakeFlagIndication>()` の呼び出しのコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。

注: コールバックサービス `<User_CheckTrcvWakeFlagIndication>()` は、一般に、複数の CAN コントローラの使用では再入可能だが、同じ CAN コントローラでは再入不可である。

**[SWS_CANIF_00800]** `<User_CheckTrcvWakeFlagIndication>()` のコンフィギュレーション: `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）が True に等しい場合、呼ばれる上位層のモジュール（[SWS_CANIF_00759] を参照）は、`CanIfDispatchUserCheckTrcvWakeFlagIndicationUL`（ECUC_CanIf_00792 を参照）でコンフィギュレーションできなければならない。

**[SWS_CANIF_00801]** `<User_CheckTrcvWakeFlagIndication>()` のコンフィギュレーション: `CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）が True に等しい場合、`<User_CheckTrcvWakeFlagIndication>()` の名前は、`CanIfDispatchUserCheckTrcvWakeFlagIndicationName`（ECUC_CanIf_00791 を参照）でコンフィギュレーションできなければならない。

**[SWS_CANIF_00802]** `<User_CheckTrcvWakeFlagIndication>()` のコンフィギュレーション: CanIf がこのサービスをサポートするかどうかは、`CANIF_PUBLIC_PN_SUPPORT`（ECUC_CanIf_00772 を参照）でコンフィギュレーションできなければならない（False: サポートしない、True: サポートする）。

**[SWS_CANIF_00803]** `<User_CheckTrcvWakeFlagIndication>()` のコンフィギュレーション: `CanIfDispatchUserCheckTrcvWakeFlagIndicationUL` が `CAN_SM` に設定されている場合、`CanIfDispatchUserCheckTrcvWakeFlagIndicationName` は `CanSM_CheckTrcvWakeFlagIndication` でなければならない。

**[SWS_CANIF_00804]** `<User_CheckTrcvWakeFlagIndication>()` のコンフィギュレーション: `CanIfDispatchUserCheckTrcvWakeFlagIndicationUL` が `CDD` に設定されている場合、サービスの名前は、パラメータ `CanIfDispatchUserCheckTrcvWakeFlagIndicationName` でコンフィギュレーションできなければならない。

##### 8.6.3.9 `<User_ControllerModeIndication>`

**[SWS_CANIF_00687]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_ControllerModeIndication>` |
| 構文 | `void <User_ControllerModeIndication>(uint8 ControllerId, Can_ControllerStateType ControllerMode)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `ControllerId`: コントローラの状態遷移が起きた CAN コントローラに割り当てられた、抽象化された CanIf の ControllerId。<br>`ControllerMode`: 通知された CAN コントローラのモード |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、CAN コントローラの状態遷移を、対応する上位層のモジュール（主に CAN State Manager モジュール）へ通知する。 |

注: 上位層のモジュールが、サービス ID を提供する。

注: このコールバックサービスは、主に CanSm に実装され、使われる（[3, CAN State Manager の仕様] を参照）。

注: CanIf がこのコールバックサービスを呼ぶ。これは、コンフィギュレーションされた上位層のモジュールにより実装される。これは、CanDrv の `CanIf_ControllerModeIndication()` を介した状態遷移の通知の場合に呼ばれる。サービス `CanIf_ControllerModeIndication()` で渡されたパラメータ `ControllerId` は、上位層のモジュールへ渡される。サービス `CanIf_ControllerModeIndication()` で渡されたパラメータ `ControllerMode` は、`<User_ControllerModeIndication>()` の適切なパラメータ `ControllerMode` へ対応付けられる。

注: 異なる上位層のユーザには、異なるサービス名を使わなければならない。

注: `<User_ControllerModeIndication>()` の呼び出しのコンテキストは、タスクレベル（ポーリングモード）である。

注: コールバックサービス `<User_ControllerModeIndication>()` は、一般に、複数の CAN コントローラの使用では再入可能だが、同じ CAN コントローラでは再入不可である。

**[SWS_CANIF_00689]** `<User_ControllerModeIndication>()` のコンフィギュレーション: このコールバックサービスを提供する上位層のモジュールは、`CANIF_USERCONTROLLERMODEINDICATION_UL`（ECUC_CanIf_00684 を参照）でコンフィギュレーションしなければならない。

**[SWS_CANIF_00690]** `<User_ControllerModeIndication>()` のコンフィギュレーション: CanIf から呼ばれる `<User_ControllerModeIndication>()` の名前は、パラメータ `CanIfDispatchUserCtrlModeIndicationName`（ECUC_CanIf_00683 を参照）で、CanIf にコンフィギュレーションしなければならない。これは、状態遷移の通知が `CanIfDispatchUserCtrlModeIndicationUL` を介してコンフィギュレーションされている場合にだけ必要である。

**[SWS_CANIF_00691]** `<User_ControllerModeIndication>()` のコンフィギュレーション: `CanIfDispatchUserCtrlModeIndicationUL` が `CAN_SM` に設定されている場合、`CanIfDispatchUserCtrlModeIndicationName` は `CanSM_ControllerModeIndication` でなければならない。

**[SWS_CANIF_00692]** `<User_ControllerModeIndication>()` のコンフィギュレーション: `CanIfDispatchUserCtrlModeIndicationUL` が `CDD` に設定されている場合、関数の名前は、パラメータ `CanIfDispatchUserCtrlModeIndicationName` でコンフィギュレーションしなければならない。

##### 8.6.3.10 `<User_TrcvModeIndication>`

**[SWS_CANIF_00693]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<User_TrcvModeIndication>` |
| 構文 | `void <User_TrcvModeIndication>(uint8 TransceiverId, CanTrcv_TrcvModeType TransceiverMode)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `TransceiverId`: トランシーバの状態遷移が起きた CAN トランシーバに割り当てられた、抽象化された CanIf の TransceiverId。<br>`TransceiverMode`: 通知された CAN トランシーバのモード |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、CAN トランシーバの状態遷移を、対応する上位層のモジュール（主に CAN State Manager モジュール）へ通知する。 |

注: 上位層のモジュールが、サービス ID を提供する。

注: このコールバックサービスは、主に CanSm に実装され、使われる（[3, CAN State Manager の仕様] を参照）。

注: CanIf がこのコールバックサービスを呼ぶ。これは、コンフィギュレーションされた上位層のモジュールにより実装される。これは、CanTrcv の `CanIf_TrcvModeIndication()` を介した状態遷移の通知の場合に呼ばれる。サービス `CanIf_TrcvModeIndication()` で渡されたパラメータ `Transceiver` は、（コンフィギュレーションされたとおりに）適切なパラメータ `TransceiverId` へ対応付けられ、上位層のモジュールへ渡される。サービス `CanIf_TrcvModeIndication()` で渡されたパラメータ `TransceiverMode` は、`<User_TrcvModeIndication>()` の適切なパラメータ `TransceiverMode` へ対応付けられる。

注: 異なる上位層のユーザには、異なるサービス名を使わなければならない。

**[SWS_CANIF_00694]** `<User_TrcvModeIndication>()` の注意点:

- CanTrcv は、電源投入後に初期化されていなければならない。
- 呼び出しのコンテキストは、タスクレベル（ポーリングモード）である。
- このコールバックサービスは、一般に、複数の CAN トランシーバの使用では再入可能だが、同じ CAN トランシーバでは再入不可である。

**[SWS_CANIF_00695]** `<User_TrcvModeIndication>()` のコンフィギュレーション: このコールバックサービスを提供する上位層のモジュールは、`CanIfDispatchUserTrcvModeIndicationUL`（ECUC_CanIf_00686 を参照）でコンフィギュレーションしなければならない。ただし、

- `<User_TrcvModeIndication>()` を使ったトランシーバのモードの通知のために、上位層のモジュールがコンフィギュレーションされていない場合、トランシーバのモードの通知をコンフィギュレーションする必要は無い。`CanIfDispatchUserTrcvModeIndicationUL` をコンフィギュレーションする必要は無い。
- トランシーバがサポートされていない場合（`CanInterfaceTransceiverDriverConfiguration` がコンフィギュレーションされていない。ECUC_CanIf_00273 を参照）、`CanIfDispatchUserTrcvModeIndicationUL` はコンフィギュレーションできない。

`<User_TrcvModeIndication>()` を使った状態遷移の通知のために、上位層のモジュールがコンフィギュレーションされていない場合、状態遷移の通知をコンフィギュレーションする必要は無い。

**[SWS_CANIF_00696]** `<User_TrcvModeIndication>()` のコンフィギュレーション: CanIf から呼ばれる `<User_TrcvModeIndication>()` の名前は、パラメータ `CanIfDispatchUserTrcvModeIndicationName`（ECUC_CanIf_00685 を参照）で、CanIf にコンフィギュレーションしなければならない。これは、状態遷移の通知が `CanIfDispatchUserTrcvModeIndicationUL` を介してコンフィギュレーションされている場合にだけ必要である。

**[SWS_CANIF_00697]** `<User_TrcvModeIndication>()` のコンフィギュレーション: `CanIfDispatchUserTrcvModeIndicationUL` が `CAN_SM` に設定されている場合、`CanIfDispatchUserTrcvModeIndicationName` は `CanSM_TransceiverModeIndication` でなければならない。

**[SWS_CANIF_00698]** `<User_TrcvModeIndication>()` のコンフィギュレーション: `CanIfDispatchUserTrcvModeIndicationUL` が `CDD` に設定されている場合、API の名前は、パラメータ `CanIfDispatchUserTrcvModeIndicationName` でコンフィギュレーションしなければならない。

> **学習メモ（原文でなく訳者の解釈）**: このプロジェクトの CanIf との対応を見るときの手がかり。
> - PDU チャネルモード（7.19.2 節）は、`CANIF_OFFLINE`（初期化時と `CAN_CS_SLEEP` への設定時）、`CANIF_TX_OFFLINE`（`CAN_CS_STOPPED` への設定時とバスオフ時。受信は有効）、`CANIF_ONLINE`、`CANIF_TX_OFFLINE_ACTIVE` の 4 つ。`CanIf_SetPduMode` は、コントローラが `CAN_CS_STARTED` の状態でなければ受け付けない（[SWS_CANIF_00874]）。
> - 送信バッファリングは、`Can_Write` が `CAN_BUSY` を返した場合にだけ働く（[SWS_CANIF_00381]）。送信バッファリングが無効で `CAN_BUSY` のときは、`CanIf_Transmit` は `E_NOT_OK` を返す（7.11.1 節）。
> - `CanIf_RxIndication` の処理の順序は、ソフトウェアフィルタ（BasicCAN のみ）→ データ長の検査 → 受信 L-SDU のバッファリング → 上位層の通知（7.14 節）。データ長の検査は、受信長がコンフィギュレーションされた長さ以上なら通過する（[SWS_CANIF_00026]）。

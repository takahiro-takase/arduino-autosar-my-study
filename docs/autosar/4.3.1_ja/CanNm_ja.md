# CAN Network Management（CanNm）仕様書 日本語訳

- 原典: AUTOSAR CP Release 4.3.1 `AUTOSAR_SWS_CANNetworkManagement.pdf`（Document ID 013）
- 範囲: 第7章（機能仕様）と第8章（API 仕様）。第1〜6章（導入、関連文書、制約、依存関係、トレーサビリティ）、第9章（シーケンス図）、第10章以降（コンフィギュレーション）は対象外。
- 凡例: `[SWS_CanNm_xxxxx]` は原典の条項 ID。`（SRS_…）` は原典が条項に付けている上位要求の ID。`[7]` などは、原典の参考文献の番号（[7] は ComM）。
- 用語: NM は Network Management、PDU は NM PDU（Network Management PDU）、CBV は Control Bit Vector、PN はパーシャルネットワーキング。
- 図は訳せないため、位置に注記だけを残している。図の内容は原典を参照すること。
- 「学習メモ」は原文の訳ではなく、このプロジェクトの実装と照らすための訳者の解釈。

---

## 7 機能仕様

### 7.1 調整のアルゴリズム

AUTOSAR の CanNm は、分散型の直接ネットワーク管理の戦略に基づく。これは、すべてのネットワークノードが、通信システムの中で受信または送信される Network Management PDU だけに依存して、自己完結的に動作することを意味する。

AUTOSAR の CanNm のアルゴリズムは、周期的な Network Management PDU に基づき、これは、ブロードキャスト送信で、クラスタのすべてのノードにより受信される。Network Management PDU の受信は、送信側のノードが、ネットワークマネジメントのクラスタを起きたままにしたいことを示す。あるノードがバススリープモードへ移行する準備ができた場合、そのノードは Network Management PDU の送信を止めるが、他のノードからの Network Management PDU を受信している間は、バススリープモードへの遷移を延期する。最終的に、Network Management PDU がもう受信されないために、専用のタイマが満了すると、すべてのノードが、バススリープモードへの遷移を開始する。

ネットワークマネジメントのクラスタのいずれかのノードが、バスの通信を必要とする場合、Network Management PDU を送信することで、ネットワークマネジメントのクラスタをバススリープモードから起こせる。ウェイクアップの手順自体の詳細は、AUTOSAR SWS ComM [7] を参照。

AUTOSAR の CanNm のアルゴリズムの主な概念は、次の 2 つの重要な要求で定義できる。

**[SWS_CanNm_00087]** CanNm のクラスタのすべてのネットワークノードは、バスの通信を必要とする間は、周期的な Network Management PDU を送信しなければならない。そうでなければ、Network Management PDU を送信してはならない。

**[SWS_CanNm_00088]** CanNm のクラスタでバスの通信が解放され、設定可能な時間（コンフィギュレーションパラメータ `CanNmTimeoutTime` + `CanNmWaitBusSleepTime` で決まる）の間、バスに Network Management PDU が無い場合、バススリープモードへの遷移を行わなければならない。（SRS_Nm_00054、SRS_Nm_00149）

AUTOSAR の CanNm のアルゴリズムの全体の状態機械は、次のように定義できる。

**[SWS_CanNm_00089]** AUTOSAR の CanNm の状態機械は、ネットワークマネジメントのクラスタの 1 つのノードの視点から見た、AUTOSAR の CanNm のアルゴリズムに必要な、状態、遷移、起動条件を含まなければならない。

注: 状態遷移は、遅くとも次のメイン関数の中で行わなければならない。

注: ネットワークマネジメントのクラスタの 1 つのノードの視点から見た、AUTOSAR の CanNm の状態機械の UML のステートチャートは、API 仕様の第 8 章に詳しく示されている。

### 7.2 動作モード

この章では、AUTOSAR の CanNm のアルゴリズムの動作モードを、詳しく説明する。

**[SWS_CanNm_00092]** AUTOSAR の CanNm は、モジュールのインタフェースで見える、3 つの動作モードを含まなければならない。

- ネットワークモード（Network Mode）
- バススリープ準備モード（Prepare Bus-Sleep Mode）
- バススリープモード（Bus-Sleep Mode）

**[SWS_CanNm_00093]** AUTOSAR の CanNm の動作モードの変更は、コールバック関数により上位層へ通知されなければならない。

**[SWS_CanNm_00091]** `CanNm_GetState` が呼ばれたとき、CanNm は、現在の NM の状態とモードを返さなければならない。（SRS_Nm_00050）

#### 7.2.1 ネットワークモード

**[SWS_CanNm_00094]** ネットワークモードは、3 つの内部状態から成らなければならない。

- リピートメッセージ状態（Repeat Message State）
- 通常動作状態（Normal Operation State）
- スリープ準備完了状態（Ready Sleep State）

**[SWS_CanNm_00314]** ネットワークモードにバススリープから入るとき、デフォルトでは、CanNm モジュールは、リピートメッセージ状態に入らなければならない。

**[SWS_CanNm_00315]** ネットワークモードにバススリープ準備モードから入るとき、デフォルトでは、CanNm モジュールは、リピートメッセージ状態に入らなければならない。

**[SWS_CanNm_00096]** ネットワークモードに入るとき、CanNm モジュールは、NM タイムアウトタイマを開始しなければならない。

**[SWS_CanNm_00097]** ネットワークモードに入るとき、CanNm は、コールバック関数 `Nm_NetworkMode` を呼んで、新しい現在の動作モードを上位層へ通知しなければならない。（SRS_Nm_00051）

**[SWS_CanNm_00098]** ネットワークモードで Network Management PDU の受信に成功したとき（`CanNm_RxIndication` の呼び出し）、PDU の送信能力が有効であれば、CanNm モジュールは、NM タイムアウトタイマを再開しなければならない。

**[SWS_CanNm_00099]** ネットワークモードで Network Management PDU の送信に成功したとき（`CanNm_TxConfirmation` が `E_OK` で呼ばれる）、CanNm モジュールは、NM タイムアウトタイマを再開しなければならない。（SRS_Nm_00151）

注: `CanNmImmediateTxConfEnabled` が有効な場合、各 Network Management PDU の送信要求は、Network Management PDU の送信の成功になるものと仮定される。

**[SWS_CanNm_00206]** CAN NM モジュールは、NM タイムアウトタイマが開始または再開されるたびに、それをリセットしなければならない。

**[SWS_CanNm_00147]** ネットワークモードで `CanNm_PassiveStartUp` が呼ばれた場合、CanNm モジュールは、このサービスを実行してはならず、`E_NOT_OK` を返さなければならない。

##### 7.2.1.1 リピートメッセージ状態

パッシブモードでないノード（7.9.3 章を参照）では、リピートメッセージ状態は、バススリープまたはバススリープ準備からネットワークモードへの遷移が、ネットワークの他のノードから見えるようになることを保証する。さらに、どのノードも、最小限の時間は動作したままであることを保証する。これは、存在するノードの検出に使える。

**[SWS_CanNm_00100]** リピートメッセージ状態に入るとき、CanNm モジュールは、パッシブモードが有効、および（または）通信が無効でない限り、Network Management PDU の送信を（再）開始しなければならない。（SRS_Nm_02510）

**[SWS_CanNm_00101]** リピートメッセージ状態で NM タイムアウトタイマが満了したとき、CanNm モジュールは、NM タイムアウトタイマを（再）開始しなければならない。

**[SWS_CanNm_00102]** ネットワークマネジメントの状態機械は、`CanNmRepeatMessageTime`（コンフィギュレーションパラメータ）で決まる、設定可能な時間の間、リピートメッセージ状態にとどまらなければならない。その時間の後、CanNm モジュールは、リピートメッセージ状態を離れなければならない。

**[SWS_CanNm_00103]** リピートメッセージ状態を離れるとき、ネットワークが要求されている場合（[SWS_CanNm_00104] を参照）、CanNm モジュールは、通常動作状態に入らなければならない。（SRS_Nm_00053）

**[SWS_CanNm_00106]** リピートメッセージ状態を離れるとき、ネットワークが解放されている場合（[SWS_CanNm_00105] を参照）、CanNm モジュールは、スリープ準備完了状態に入らなければならない。（SRS_Nm_00053）

**[SWS_CanNm_00107]** `CanNmNodeDetectionEnabled` が `TRUE` に設定されている場合、CanNm は、リピートメッセージ状態を離れるとき、リピートメッセージビットをクリアしなければならない。

**[SWS_CanNm_00137]** サービス `CanNm_RepeatMessageRequest` が、リピートメッセージ状態、バススリープ準備モード、またはバススリープモードで呼ばれた場合、CanNm モジュールは、サービスを実行してはならず、`E_NOT_OK` を返さなければならない。

##### 7.2.1.2 通常動作状態

通常動作状態は、ネットワークが要求されている限り、どのノードも、ネットワークマネジメントのクラスタを起きたままにできることを保証する。

**[SWS_CanNm_00116]** 通常動作状態に、スリープ準備完了状態から入るとき、CanNm モジュールは、Network Management PDU の送信を開始しなければならない。

注: パッシブモードが有効な場合、または Network Management PDU の送信能力が無効にされている場合、NM PDU は送信されないため、必要な処理は無い。

**[SWS_CanNm_00117]** 通常動作状態で NM タイムアウトタイマが満了したとき、CanNm モジュールは、NM タイムアウトタイマを（再）開始しなければならない。

**[SWS_CanNm_00118]** ネットワークが解放され、現在の状態が通常動作状態の場合、CanNm モジュールは、スリープ準備完了状態に入らなければならない（[SWS_CanNm_00105] を参照）。（SRS_Nm_00053）

**[SWS_CanNm_00119]** `CanNmNodeDetectionEnabled` が `TRUE` に設定され、通常動作状態でリピートメッセージ要求ビットを受信した場合、CanNm モジュールは、リピートメッセージ状態に入らなければならない。（SRS_Nm_00153）

**[SWS_CanNm_00120]** `CanNmNodeDetectionEnabled` が `TRUE` に設定され、通常動作状態で関数 `CanNm_RepeatMessageRequest` が呼ばれた場合、CanNm モジュールは、リピートメッセージ状態に入らなければならない。（SRS_Nm_00153）

**[SWS_CanNm_00121]** `CanNmNodeDetectionEnabled` が `TRUE` に設定され、通常動作状態で関数 `CanNm_RepeatMessageRequest` が呼ばれた場合、CanNm モジュールは、リピートメッセージビットを設定しなければならない。（SRS_Nm_00153）

##### 7.2.1.3 スリープ準備完了状態

スリープ準備完了状態は、ネットワークマネジメントのクラスタのどのノードも、他のノードがネットワークマネジメントのクラスタを起きたままにしている限り、バススリープ準備モードへの遷移を待つことを保証する。

**[SWS_CanNm_00108]** スリープ準備完了状態に、リピートメッセージ状態または通常動作状態から入るとき、CanNm モジュールは、Network Management PDU の送信を止めなければならない。

注: パッシブモードが有効な場合、NM PDU は送信されないため、必要な処理は無い。

**[SWS_CanNm_00109]** スリープ準備完了状態で NM タイムアウトタイマが満了したとき、CanNm モジュールは、バススリープ準備モードに入らなければならない。（SRS_Nm_00053）

**[SWS_CanNm_00110]** ネットワークが要求され、現在の状態がスリープ準備完了状態の場合、CanNm モジュールは、通常動作状態に入らなければならない（[SWS_CanNm_00104] を参照）。（SRS_Nm_00053）

**[SWS_CanNm_00111]** `CanNmNodeDetectionEnabled` が `TRUE` に設定され、スリープ準備完了状態でリピートメッセージ要求ビットを受信した場合、CanNm モジュールは、リピートメッセージ状態に入らなければならない。（SRS_Nm_00153）

**[SWS_CanNm_00112]** `CanNmNodeDetectionEnabled` が `TRUE` に設定され、スリープ準備完了状態で関数 `CanNm_RepeatMessageRequest` が呼ばれた場合、CanNm モジュールは、リピートメッセージ状態に入らなければならない。（SRS_Nm_00153）

**[SWS_CanNm_00113]** `CanNmNodeDetectionEnabled` が `TRUE` に設定され、スリープ準備完了状態で関数 `CanNm_RepeatMessageRequest` が呼ばれた場合、CanNm モジュールは、リピートメッセージビットを設定しなければならない。（SRS_Nm_00153）

#### 7.2.2 バススリープ準備モード

バススリープ準備モードの目的は、バススリープモードに入る前に、すべてのノードが、ネットワークの動作を止める時間を持つことを保証することである。バススリープ準備モードでは、バスの動作が落ち着けられ（すなわち、キューに入ったメッセージが、すべての Tx バッファを空にするために送信される）、最終的に、バススリープ準備モードでは、バスの動作が無くなる。

**[SWS_CanNm_00114]** バススリープ準備モードに入るとき、CanNm モジュールは、`Nm_PrepareBusSleepMode` を呼んで、上位層へ通知しなければならない。（SRS_Nm_00051）

**[SWS_CanNm_00115]** CanNm モジュールは、`CanNmWaitBusSleepTime`（コンフィギュレーションパラメータ）で決まる、設定可能な時間の間、バススリープ準備モードにとどまらなければならない。その時間の後、バススリープ準備モードを離れ、バススリープモードに入らなければならない。（SRS_Nm_00051）

**[SWS_CanNm_00124]** バススリープ準備モードで Network Management PDU の受信に成功したとき、CanNm モジュールは、ネットワークモードに入らなければならない。デフォルトでは、CanNm モジュールは、リピートメッセージ状態に入らなければならない（[SWS_CanNm_00315] を参照）。（SRS_Nm_00151）

**[SWS_CanNm_00123]** バススリープ準備モードでネットワークが要求されたとき、CanNm モジュールは、ネットワークモードに入らなければならない。デフォルトでは、CanNm モジュールは、リピートメッセージ状態に入らなければならない（[SWS_CanNm_00315] を参照）。

**[SWS_CanNm_00122]** バススリープ準備モードでネットワークが要求され（[SWS_CanNm_00104] を参照）、CanNm モジュールがネットワークモードに入り、`CanNmImmediateRestartEnabled`（コンフィギュレーションパラメータ）が `TRUE` に設定されている場合、CanNm モジュールは、Network Management PDU を送信しなければならない。

根拠: クラスタの他のノードは、まだバススリープ準備モードにある。上述の例外的な状況では、バススリープモードへの遷移を避け、バスの通信を、できるだけ速く復元しなければならない。CanNm での Network Management PDU の送信オフセットのために、リピートメッセージ状態での最初の Network Management PDU の送信が、大きく遅れることがある。ネットワークの再開の遅れを避けるために、Network Management PDU の送信を、直ちに要求できる。

注: `CanNmImmediateRestartEnabled` が `TRUE` に設定され、ウェイクアップラインが使われる場合、すべてのネットワークノードが、バススリープ準備モードでネットワークの要求を受けると、Network Management PDU のバーストが起きる。

#### 7.2.3 バススリープモード

バススリープモードの目的は、メッセージを交換しない場合に、ノードの消費電力を減らすことである。通信コントローラは、スリープモードに切り替えられ、対応するウェイクアップの仕組みが有効になり、最終的に、バススリープモードで、消費電力が適切なレベルに下げられる。

`CanNmTimeoutTime` + `CanNmWaitBusSleepTime`（どちらもコンフィギュレーションパラメータ）で決まる、設定可能な時間が、ネットワークマネジメントのクラスタのすべてのノードで同一にコンフィギュレーションされている場合、AUTOSAR の NM のアルゴリズムで調整される、ネットワークマネジメントのクラスタのすべてのノードは、ほぼ同じ時刻に、バススリープモードへの遷移を行う。

注: パラメータ `CanNmTimeoutTime` と `CanNmWaitBusSleepTime` は、ネットワークマネジメントのクラスタのすべてのネットワークノードで、同じ値であるべきである。特定の実装に応じて、バススリープモードへの遷移は、正確に、またはおおよそ同じ時刻に行われる。この遷移の時間のジッタは、次の要因に依存する。

- 内部クロックの精度（発振器のドリフト）
- NM タスクの周期時間（タスクがグローバル時間に同期されていない場合）
- Tx キューでの Network Management PDU の待ち時間（送信確認が、送信要求の直後に行われる場合）

最良の場合、`CanNmTimeoutTime` + `CanNmWaitBusSleepTime`（どちらもコンフィギュレーションパラメータ）の値で決まる、設定可能な時間について、発振器のドリフトだけを考慮すればよい。

**[SWS_CanNm_00126]** バススリープモードに入るとき、初期化時のデフォルトを除き、CanNm モジュールは、コールバック関数 `Nm_BusSleepMode` を呼んで、上位層へ通知しなければならない。（SRS_Nm_00051）

**[SWS_CanNm_00127]** CanNm モジュールが、バススリープモードで Network Management PDU の受信に成功したとき（`CanNm_RxIndication` の呼び出し）、CanNm モジュールは、コールバック関数 `Nm_NetworkStartIndication` を呼んで、上位層へ通知しなければならない。（SRS_Nm_00151）

根拠: ネットワークマネジメントとモード管理の間の、競合状態と状態の不整合を避けるため、CanNm は、バススリープモードからネットワークモードへの遷移を、自動的には行わない。CanNm は、ウェイクアップの判断をしなければならない上位層へ、通知するだけである。バススリープモードでの Network Management PDU の受信は、ECU のシャットダウン／起動の処理の現在の状態に応じて、扱わなければならない。

**[SWS_CanNm_00336]** CanNm モジュールが、バススリープモードで Network Management PDU の受信に成功したとき（`CanNm_RxIndication` の呼び出し）、CanNm モジュールは、エラー `CANNM_E_NET_START_IND` を DET へ報告しなければならない。

**[SWS_CanNm_00128]** `CanNm_PassiveStartUp` が、バススリープモードまたはバススリープ準備モードで呼ばれた場合、CanNm モジュールは、ネットワークモードに入らなければならない。デフォルトでは、CanNm モジュールは、リピートメッセージ状態に入らなければならない（[SWS_CanNm_00314] と [SWS_CanNm_00315] を参照）。（SRS_Nm_02536）

注: バススリープ準備モードとバススリープモードでは、バスの通信が明示的に要求されない限り、ネットワークは解放されていると仮定される。

**[SWS_CanNm_00129]** バススリープモードでネットワークが要求されたとき、CanNm モジュールは、ネットワークモードに入らなければならない。デフォルトでは、CanNm モジュールは、リピートメッセージ状態に入らなければならない（[SWS_CanNm_00314] と [SWS_CanNm_00104] を参照）。（SRS_Nm_00046）

### 7.3 ネットワークの状態

ネットワークの状態（すなわち、「要求（requested）」と「解放（released）」）は、AUTOSAR の CanNm の状態機械に並行して存在する、2 つの追加の状態である。ネットワークの状態は、ソフトウェアコンポーネントが、バスで通信する必要があるか（ネットワークの状態は「要求」）、ソフトウェアコンポーネントが、バスで通信する必要が無いか（バスのネットワークの状態は「解放」）を示す。ネットワークが解放されていても、他の ECU がまだネットワークを要求しているために、ECU が通信を続けることがあることに注意。

**[SWS_CanNm_00104]** 関数呼び出し `CanNm_NetworkRequest` は、ネットワークを要求しなければならない。すなわち、CanNm モジュールは、ネットワークの状態を「要求」に変更しなければならない。（SRS_Nm_00045、SRS_Nm_00047、SRS_Nm_02513）

**[SWS_CanNm_00105]** 関数呼び出し `CanNm_NetworkRelease` は、ネットワークを解放しなければならない。すなわち、CanNm モジュールは、ネットワークの状態を「解放」に変更しなければならない。（SRS_Nm_00045、SRS_Nm_00047、SRS_Nm_02513）

### 7.4 初期化

**[SWS_CanNm_00141]** CanNm モジュールの初期化（`CanNm_Init`）が成功した場合、CanNm モジュールは、ネットワークマネジメントの状態を、バススリープモードに設定しなければならない。

注: CanNm モジュールは、CanIf が初期化された後、他のネットワークマネジメントのサービスが呼ばれる前に、初期化されるべきである。

**[SWS_CanNm_00143]** 初期化されたとき、デフォルトでは、CanNm モジュールは、ネットワークの状態を「解放」に設定しなければならない。

**[SWS_CanNm_00144]** 初期化されたとき、デフォルトでは、CanNm モジュールは、バススリープモードに入らなければならない。

**[SWS_CanNm_00060]** 関数 `CanNm_Init` は、渡されたコンフィギュレーションのポインタのパラメータにより、有効なコンフィギュレーションセットを選択しなければならない（8.3.1 を参照）。

**[SWS_CanNm_00061]** `CanNmGlobalPnSupport` が `TRUE` に設定され、CanNm が初期化された場合（`CanNm_Init` の呼び出し）、CanNm は、NM メッセージの Tx タイムアウトタイマを停止しなければならない。

**[SWS_CanNm_00023]** 初期化の間に、CanNm モジュールは、バスの負荷の低減を無効にしなければならない。

**[SWS_CanNm_00033]** 初期化の後、CanNm モジュールは、メッセージの周期タイマを停止することで、Network Management PDU の送信を止めなければならない。

注: `CanNmPassiveModeEnabled` が `TRUE` に設定されている場合、そのようなノードは Network Management PDU を送信しないため、CanNm のメッセージの周期は必要ない。

**[SWS_CanNm_00002]** CanNm が初期化されていない場合、（`CanNm_Init` 以外の）CanNm の関数の呼び出しは、拒否されなければならず、API に戻り値があれば `E_NOT_OK` が返されなければならない。開発エラーの検出が有効な場合（`CanNmDevErrorDetect` が `TRUE` に設定されている）、`CANNM_E_UNINIT` を DET へ報告しなければならない。

**[SWS_CanNm_00025]** 初期化の間に、CanNm モジュールは、ユーザデータの各バイトを `0xFF` に設定しなければならない。

**[SWS_CanNm_00085]** 初期化の間に、CanNm モジュールは、Control Bit Vector を `0x00` に設定しなければならない。

### 7.5 実行

#### 7.5.1 プロセッサのアーキテクチャ

**[SWS_CanNm_00146]** AUTOSAR の CanNm のアルゴリズムは、プロセッサに依存してはならない。これは、プロセッサ固有のハードウェアのサポートに依存してはならず、したがって、AUTOSAR の範囲にあるどのプロセッサのアーキテクチャでも実現できるべきであることを意味する。

#### 7.5.2 タイミングパラメータ

**[SWS_CanNm_00246]** コンフィギュレーションパラメータ `CanNmTimeoutTime` は、AUTOSAR の CanNm のタイミングパラメータ「NM タイムアウト時間（NM-Timeout Time）」を決めなければならない。

**[SWS_CanNm_00247]** コンフィギュレーションパラメータ `CanNmRepeatMessageTime` は、AUTOSAR の CanNm のタイミングパラメータ「リピートメッセージ時間（Repeat Message Time）」を決めなければならない。

**[SWS_CanNm_00248]** コンフィギュレーションパラメータ `CanNmWaitBusSleepTime` は、AUTOSAR の CanNm のタイミングパラメータ「バススリープ待ち時間（Wait Bus-Sleep Time）」を決めなければならない。

**[SWS_CanNm_00249]** コンフィギュレーションパラメータ `CanNmRemoteSleepIndTime` は、AUTOSAR の CanNm のタイミングパラメータ「リモートスリープ通知時間（Remote Sleep Indication Time）」を決めなければならない。

### 7.6 Network Management PDU の構造

次の図は、8 バイトの例について、Network Management PDU の形式を示す。ここでは、送信元のノード識別子が最初のバイトに、Control Bit Vector が 2 バイト目にある。

> （図 7-1「Network Management PDU の形式の例」: 図。訳なし。読み取れる内容: バイト 0 が送信元ノード識別子（Source Node Identifier）、バイト 1 が Control Bit Vector、バイト 2〜7 がユーザデータ 0〜5。）

**[SWS_CanNm_00074]** 送信元ノード識別子の位置は、`CanNmPduNidPosition` により、バイト 0、バイト 1、またはオフにコンフィギュレーションできなければならない（デフォルト: バイト 0）。（SRS_Nm_02505）

注: `CanNmPduNidPosition` をオフに設定することは、NM PDU の中で、送信元ノード識別子が領域を占めないことを意味する。したがって、ユーザデータに 1 バイト多く使える。

**[SWS_CanNm_00075]** Control Bit Vector の位置は、`CanNmPduCbvPosition` により、バイト 0、バイト 1、またはオフにコンフィギュレーションできなければならない（デフォルト: バイト 1）。

注: `CanNmPduCbvPosition` をオフに設定することは、NM PDU の中で、Control Bit Vector が領域を占めないことを意味する。したがって、ユーザデータに 1 バイト多く使える。（原典の注は「送信元ノード識別子」と書かれているが、内容は Control Bit Vector についてのものである。）

注: Network Management PDU の長さは、「グローバル」な ECUC モジュールのパラメータ `PduLength` で定義される（[EcuC003_Conf]、Ecu Configuration の仕様を参照）。有効なシステムバイトの数と長さの差が、ユーザデータのバイト数である。

次の図は、Control Bit Vector の形式を説明する。

> （図 7-2「Control Bit Vector」: 図。訳なし。読み取れる内容: ビット 7 は予約、ビット 6 はパーシャルネットワーク情報ビット（PNI）、ビット 5 は予約、ビット 4 はアクティブウェイクアップビット、ビット 3 は NM コーディネータスリープ準備ビット、ビット 2 と 1 は予約（R3.2 の NM コーディネータ ID の上位／下位ビット）、ビット 0 はリピートメッセージ要求ビット。）

**[SWS_CanNm_00045]** Control Bit Vector は、次から成らなければならない。

- ビット 0: リピートメッセージ要求（Repeat Message Request）
  - 0: リピートメッセージ状態は要求されていない
  - 1: リピートメッセージ状態が要求されている
- ビット 3: NM コーディネータスリープビット（NM Coordinator Sleep Bit）
  - 0: 同期したシャットダウンの開始は、メインのコーディネータにより要求されていない
  - 1: 同期したシャットダウンの開始が、メインのコーディネータにより要求されている
- ビット 4: アクティブウェイクアップビット（Active Wakeup Bit）
  - 0: ノードがネットワークを起こしていない（パッシブなウェイクアップ）
  - 1: ノードがネットワークを起こした（アクティブなウェイクアップ）
- ビット 6: パーシャルネットワーク情報ビット（PNI）
  - 0: NM PDU に、パーシャルネットワークの要求の情報が含まれない
  - 1: NM PDU に、パーシャルネットワークの要求の情報が含まれる
- ビット 1、2、5、7 は、将来の拡張のために予約されている
  - 0: 無効／将来の使用のために予約

注: Control Bit Vector は、初期化の間に `0x00` で初期化される（[SWS_CanNm_00085] も参照）。

**[SWS_CanNm_00013]** CanNm モジュールは、`CanNmPduNidPosition` がオフに設定されていない限り、送信元ノード識別子を、コンフィギュレーションパラメータ `CanNmNodeId` で設定しなければならない。（SRS_Nm_02503）

**[SWS_CanNm_00401]** CanNm が、`CanNm_NetworkRequest` の呼び出し（すなわち、アクティブなウェイクアップ）により、バススリープモードまたはバススリープ準備モードからネットワークモードへの状態変更を行い、`CanNmActiveWakeupBitEnabled` が `TRUE` の場合、CanNm は、CBV のアクティブウェイクアップビットを設定しなければならない。

**[SWS_CanNm_00402]** CanNm モジュールがネットワークモードを離れ、`CanNmActiveWakeupBitEnabled` が `TRUE` の場合、CanNm モジュールは、CBV のアクティブウェイクアップビットをクリアしなければならない。

### 7.7 通信のスケジューリング

#### 7.7.1 送信

注: この章で説明する送信の仕組みは、Network Management PDU の送信能力が有効な場合にだけ関係する。

**[SWS_CanNm_00072]** Network Management PDU の送信能力は、`CanNmPassiveModeEnabled`（10.2 章を参照）により、コンフィギュレーションできなければならない。（SRS_Nm_02510）

注: この章で説明する送信の仕組みは、`CanNmPassiveModeEnabled` が `FALSE` の場合にだけ関係する。

**[SWS_CanNm_00237]** CanNm モジュールは、周期的な送信モードを提供しなければならない。この送信モードでは、CanNm モジュールは、Network Management PDU を周期的に送らなければならない。

**[SWS_CanNm_00238]** CanNm モジュールは、オプションで、バス負荷の低減付きの周期的な送信モードを提供しなければならない。この送信モードでは、CanNm モジュールは、特定のアルゴリズムにより、Network Management PDU を送信しなければならない。

周期的な送信モードは、「リピートメッセージ状態」と「通常動作状態」で使われる。バス負荷の低減付きの周期的な送信モードは、「通常動作状態」でだけ利用できる。

注: 周期的な送信モードは、バス負荷の低減の仕組みが無効な場合、「リピートメッセージ状態」と「通常動作状態」で使われる。バス負荷の低減付きの周期的な送信モードは、バス負荷の低減の仕組みが有効な場合に限り、「通常動作状態」でだけ使われる。

**[SWS_CanNm_00071]** 即時の送信確認の仕組みは、`CanNmImmediateTxConfEnabled`（10.2 を参照）により、コンフィギュレーションできなければならない。（SRS_Nm_02510）

注: 即時の送信確認の仕組みは、CanIf からの実際の確認を使いたくないシステムで使われる。

根拠: バスへのアクセスが、オフラインのシステム設計ツールにより完全に規定されている場合、CanIf による実際の送信確認は、冗長とみなせる。最大のアービトレーション時間が分かっていると仮定されるため、送信要求の時点で、直ちに確認を発生させることは許容できる。

**[SWS_CanNm_00005]** リピートメッセージ状態が `CanNm_NetworkRequest` を介して入られなかった場合、または `CanNmImmediateNmTransmissions` がゼロの場合、NM PDU の送信は、リピートメッセージ状態に入った後、`CanNmMsgCycleOffset` だけ遅らされなければならない。

**[SWS_CanNm_00334]** `CanNm_NetworkRequest()`（アクティブなウェイクアップ）により、バススリープモードまたはバススリープ準備モードからリピートメッセージ状態に入るとき、`CanNmImmediateNmTransmissions` がゼロより大きい場合、NM PDU は、周期時間として `CanNmImmediateNmCycleTime` を使って送信されなければならない。最初の NM PDU の送信は、できるだけ早く起動されなければならない。送信の後、メッセージの周期タイマは、`CanNmImmediateNmCycleTime` で再ロードされなければならない。この場合、`CanNmMsgCycleOffset` は適用されてはならない。

**[SWS_CanNm_00006]** 通常動作状態にスリープ準備完了状態から入る場合、NM PDU の送信は、直ちに開始されなければならない。

**[SWS_CanNm_00454]** `CanNmPnHandleMultipleNetworkRequests` が `TRUE` に設定されている場合、`CanNm_NetworkRequest` は、ネットワークモードからリピートメッセージ状態への状態遷移を起動しなければならない。PDU の送信能力が有効な場合、NM PDU は、周期時間として `CanNmImmediateNmCycleTime` を使って送信されなければならない。最初の NM PDU の送信は、できるだけ早く起動されなければならない。送信の後、メッセージの周期タイマは、`CanNmImmediateNmCycleTime` で再ロードされなければならない。この場合、`CanNmMsgCycleOffset` は適用されてはならない。

注: この場合、ECUC_CanNm_00056 により、`CanNmImmediateNmTransmissions` はゼロより大きくなければならない。

**[SWS_CanNm_00335]** NM PDU を `CanNmImmediateNmCycleTime` で送信する場合（[SWS_CanNm_00334] と [SWS_CanNm_00454] を参照）、CanNm は、このタイミングでの `CanNmImmediateNmTransmissions` 回の（最初の即時の送信を含む）送信が、正常に要求されることを、確実にしなければならない。CanIf への送信要求が失敗した場合（`E_NOT_OK` が返される）、CanNm は、次のメイン関数で、送信要求を再試行しなければならない。その後、CanNm は、`CanNmMsgCycleTime` を使って、NM PDU の送信を続けなければならない。

注: `CanNmImmediateNmCycleTime` を使って NM PDU を送信している間は、他の NM PDU を送信してはならない（すなわち、`CanNmMsgCycleTime` の送信周期は止められる）。

**[SWS_CanNm_00032]** Network Management PDU の送信が開始され、CanNm のメッセージの周期タイマが満了した場合、CanNm モジュールは、CanIf の関数 `CanIf_Transmit` を呼んで、Network Management PDU を送信しなければならない。

注: `CanIf_Transmit` の関数呼び出しが失敗した場合、7.12 章に述べる送信エラーの扱いが、CanNm モジュールに知らせる。

**[SWS_CanNm_00040]** CanNm のメッセージの周期タイマが満了した場合、CanNm モジュールは、`CanNmMsgCycleTime` で再開しなければならない。

**[SWS_CanNm_00051]** Network Management PDU の送信が止められた場合、CanNm モジュールは、メッセージの周期タイマをキャンセルしなければならない。

#### 7.7.2 受信

NM PDU の受信が成功した場合、CanIf モジュールは、コールバック関数 `CanNm_RxIndication` を呼ぶ。

**[SWS_CanNm_00035]** コールバック関数 `CanNm_RxIndication` の呼び出しで、CanNm モジュールは、関数のパラメータで参照された Network Management PDU のデータを、内部のバッファへコピーしなければならない。

### 7.8 バス負荷の低減の仕組み

Network Management PDU の送信周期は、通常、タイミングパラメータ `CanNmMsgCycleTime` で決まる。このパラメータは、ネットワークマネジメントのクラスタに属するすべての NM ノードで、等しくなければならない。何もしなければ、これは、ネットワークマネジメントのクラスタのメンバーの数に依存する、バス負荷につながる。ノード固有のタイミングパラメータ `CanNmMsgCycleOffset` で、バーストが防がれる場合でも、ネットワークマネジメントのクラスタの大きさに依存せずに、バス負荷を減らす仕組みが必要である。

それを実現するために、次の 2 つの点を考慮しなければならない。

1. Network Management PDU が受信された場合、CanNm のメッセージの周期タイマは、ノード固有のタイミングパラメータ `CanNmMsgReducedTime` で再ロードされる。ノード固有の時間 `CanNmMsgReducedTime` は、`CanNmMsgCycleTime` の 1/2 より大きく、`CanNmMsgCycleTime` より小さくあるべきである。
2. Network Management PDU が送信された場合、CanNm のメッセージの周期タイマは、ネットワークマネジメントのクラスタ固有のタイミングパラメータ `CanNmMsgCycleTime` で再ロードされる。

これは、次の動作につながる。

`CanNmMsgReducedTime` の時間が最も小さい 2 つのノードだけが、ネットワークで交互に Network Management PDU を送信する。いずれかのノードが送信を止めた場合、次に小さい `CanNmMsgReducedTime` の時間を持つノードが、Network Management PDU の送信を始める。バスの通信を必要とするノードがネットワークに 1 つだけの場合、`CanNmMsgCycleTime` ごとに 1 つの Network Management PDU が送信される。

このアルゴリズムは、バス負荷が、`CanNmMsgCycleTime` ごとに最大 2 つの Network Management PDU に制限されることを保証する。

例は、11 章にある。

**[SWS_CanNm_00052]** バス負荷の低減の仕組みは、`CanNmBusLoadReductionEnabled` パラメータ（10.2 を参照）により、静的にコンフィギュレーションできなければならない。

**[SWS_CanNm_00156]** リピートメッセージ状態に、バススリープモード、バススリープ準備モード、通常動作状態、またはスリープ準備完了状態から入るとき、CanNm モジュールは、バス負荷の低減を無効にしなければならない。

**[SWS_CanNm_00157]** 通常動作状態に、リピートメッセージ状態またはスリープ準備完了状態から入り、`CanNmBusLoadReductionEnabled` が `TRUE` の場合、CanNm モジュールは、バス負荷の低減を有効にしなければならない。

**[SWS_CanNm_00069]** バス負荷の低減の仕組みが、全体で有効（`CanNmBusLoadReductionEnabled` が `TRUE`）で、特定のネットワークで有効にされ、PDU の送信能力が有効で、このネットワークについて関数 `CanNm_RxIndication` が呼ばれた場合、CanNm モジュールは、CanNm のメッセージの周期タイマを、ノード固有の時間 `CanNmMsgReducedTime` で再開しなければならない。

### 7.9 追加の機能

#### 7.9.1 リモートスリープ通知の検出

「リモートスリープ通知（Remote Sleep Indication）」は、通常動作状態のノードが、クラスタの他のすべてのノードが、スリープ準備完了状態にあることを見つける状況を表す。通常動作状態のノードは、それでもバスを起きたままにする。

**[SWS_CanNm_00149]** リモートスリープ通知の検出は、`CanNmRemoteSleepIndEnabled` スイッチ（コンフィギュレーションパラメータ）を使って、静的にコンフィギュレーションできなければならない。

**[SWS_CanNm_00150]** CanNm モジュールが、通常動作状態で、`CanNmRemoteSleepIndTime`（コンフィギュレーションパラメータ）で決まる、設定可能な時間の間、Network Management PDU を受信しなかった場合、CanNm モジュールは、コールバック関数 `Nm_RemoteSleepIndication` を呼ばなければならない。（SRS_Nm_00052）

`Nm_RemoteSleepIndication` の呼び出しで、CanNm は、クラスタのすべてのノードがスリープの準備ができていること（いわゆる「リモートスリープ通知」）を、Nm モジュールへ通知する。

**[SWS_CanNm_00151]** リモートスリープ通知が以前に検出されていて、通常動作状態またはスリープ準備完了状態で、Network Management PDU が再び受信された場合、CanNm モジュールは、コールバック関数 `Nm_RemoteSleepCancellation` を呼ばなければならない。（SRS_Nm_02509）

**[SWS_CanNm_00152]** リモートスリープ通知が以前に検出されていて、リピートメッセージ状態に、通常動作状態またはスリープ準備完了状態から入る場合、CanNm モジュールは、コールバック関数 `Nm_RemoteSleepCancellation` を呼ばなければならない。（SRS_Nm_02509）

`Nm_RemoteSleepCancellation` の呼び出しで、CanNm は、クラスタの一部のノードが、もうスリープの準備ができていないこと（いわゆる「リモートスリープの取り消し」）を、Nm モジュールへ通知する。

**[SWS_CanNm_00154]** サービス `CanNm_CheckRemoteSleepIndication` が呼ばれ、状態がバススリープモード、バススリープ準備モード、またはリピートメッセージ状態の場合、CanNm モジュールは、サービスを実行してはならず、`E_NOT_OK` を返さなければならない。

#### 7.9.2 ユーザデータ

**[SWS_CanNm_00158]** NM のユーザデータのサポートは、`CanNmUserDataEnabled` スイッチ（コンフィギュレーションパラメータ）を使って、静的にコンフィギュレーションできなければならない。

**[SWS_CanNm_00159]** `CanNm_SetUserData` が呼ばれたとき、CanNm モジュールは、次にバスで送信される Network Management PDU のための、Network Management のユーザデータを設定しなければならない。（SRS_Nm_02503）

**[SWS_CanNm_00160]** `CanNm_GetUserData` が呼ばれたとき、CanNm モジュールは、最も最近受信した Network Management PDU の、Network Management のユーザデータを返さなければならない。（SRS_Nm_02504）

注: ユーザデータがコンフィギュレーションされている場合、リピートメッセージ状態では確実に送信される。通常動作状態では、ユーザデータが送信されるかは、バス負荷の低減のコンフィギュレーションに依存する。スリープ準備完了状態では、ユーザデータは送信されない。

##### 7.9.2.1 COM のユーザデータ

ユーザデータを設定・取得するために CanNm の API を使う代わりに、CanNm は、ユーザデータを取得するために COM を使ってもよい。

**[SWS_CanNm_00327]** `CanNmComUserDataSupport` が有効な場合、API `CanNm_SetUserData` は利用できてはならない。

**[SWS_CanNm_00328]** `CanNmComUserDataSupport` が有効で、NM-PDU が CanIf でトリガ送信にコンフィギュレーションされていない場合（`CanIfTxPduTriggerTransmit` が `FALSE` に設定）、CanNm は、対応する NM PDU の送信を要求するたびにその前に、`PduR_CanNmTriggerTransmit` を呼んで、参照された NM I-PDU から NM ユーザデータを集め、そのユーザデータを他の NM のバイトと組み合わせなければならない。（SRS_Nm_02503）

注: トリガ送信の場合、送信要求の時点ではデータは不要で、長さだけが必要である。データは、`CanNm_TriggerTransmit` の中で集められる（8.4.4 章 `CanNm_TriggerTransmit` を参照）。

**[SWS_CanNm_00450]** `CanNmComUserDataSupport` が有効で、`PduR_CanNmTriggerTransmit` が `E_NOT_OK` を返した場合、NM は、`NmUserData` として、最後に送信した値を使わなければならない。

注: 古い NM データの送信は、`NmUserData` の送信に使われる COM の IPdu を止めないことで、避けられる。

**[SWS_CanNm_00329]** `CanNmComUserDataSupport` が有効で、`CanNm_TxConfirmation` が呼ばれた場合、CanNm は、`PduR_CanNmTxConfirmation` を呼んで、送信確認の結果を PduR へ転送しなければならない。

**[SWS_CanNm_00332]** `CanNmComUserDataSupport` が有効で、利用できるユーザデータのバイト数が、参照された I-PDU の長さと一致しない場合、生成の時点でエラーを報告しなければならない。

#### 7.9.3 パッシブモード

パッシブモードでは、ノードは Network Management PDU を受信するだけで、Network Management PDU を送信しない。

**[SWS_CanNm_00161]** パッシブモードは、`CanNmPassiveModeEnabled` スイッチ（コンフィギュレーションパラメータ）を使って、静的にコンフィギュレーションできなければならない。（SRS_Nm_02511）

注: パッシブモードは、1 つの ECU の中のすべての NM ネットワークについて、有効にするか、無効にするかのどちらかでなければならない。

#### 7.9.4 Network Management PDU の Rx 通知

**[SWS_CanNm_00037]** コールバック関数 `CanNm_RxIndication` の呼び出しで、CanNm モジュールは、`CanNmPduRxIndicationEnabled`（コンフィギュレーションパラメータ）が `TRUE` に設定されている場合に限り、Nm のコールバック関数 `Nm_PduRxIndication` を呼ばなければならない。

#### 7.9.5 状態変更の通知

**[SWS_CanNm_00166]** AUTOSAR の CanNm の状態のすべての変更は、コールバック `Nm_StateChangeNotification` が有効な場合（コンフィギュレーションパラメータ `CanNmStateChangeIndEnabled` が `TRUE`）、`Nm_StateChangeNotification` を呼んで、上位層へ通知されなければならない。（SRS_Nm_00051）

#### 7.9.6 通信の制御

注: 通信の制御は、コンフィギュレーションパラメータ `CanNmComControlEnabled` により、静的にコンフィギュレーションできる。

**[SWS_CanNm_00170]** サービス `CanNm_DisableCommunication` が呼ばれた場合、CanNm モジュールは、Network Management PDU の送信能力を無効にしなければならない。（SRS_Nm_02512）

注: この動作は、リピートメッセージ状態でも適用されなければならない。通信の制御の機能は、リピートメッセージ状態の継続時間に影響しない。

**[SWS_CanNm_00173]** Network Management PDU の送信能力が無効にされたとき、CanNm モジュールは、Network Management PDU の送信を止めるために、CanNm のメッセージの周期タイマを止めなければならない。（SRS_Nm_02512）

**[SWS_CanNm_00174]** Network Management PDU の送信能力が無効にされたとき、CanNm モジュールは、NM タイムアウトタイマを止めなければならない。

**[SWS_CanNm_00175]** Network Management PDU の送信能力が無効にされたとき、CanNm モジュールは、リモートスリープ通知の検出を止めなければならない。

**[SWS_CanNm_00178]** Network Management PDU の送信能力が有効にされたとき、NM PDU の送信は、遅くとも次の NM のメイン関数の中で開始されなければならない。（SRS_Nm_02512）

**[SWS_CanNm_00179]** Network Management PDU の送信能力が有効にされたとき、CanNm モジュールは、NM タイムアウトタイマを再開しなければならない。

**[SWS_CanNm_00180]** `CanNmRemoteSleepIndEnabled` が `TRUE` で、Network Management PDU の送信能力が有効にされたとき、CanNm モジュールは、リモートスリープ通知の検出を再開しなければならない。

**[SWS_CanNm_00181]** サービス `CanNm_RequestBusSynchronization` は、Network Management PDU の送信能力が無効の場合、`E_NOT_OK` を返さなければならない。

#### 7.9.7 コーディネータの同期のサポート

複数のコーディネータが同じバスに接続されている場合、CBV の特別なビットである NmCoordinatorSleepReady ビットが、メインのコーディネータがシャットダウンのシーケンスの開始を要求していることを示すために使われる。アルゴリズムの主な機能は、Nm モジュールに記述されている。

**[SWS_CanNm_00341]** `CanNmCoordinatorSyncSupport` が `TRUE` に設定され、CanNm がネットワークモードに入った、または以前に `Nm_CoordReadyToSleepCancellation` を呼んでいた場合、CanNm は、NmCoordinatorSleepReady ビット（CBV を参照）が 1 に設定された NM PDU の最初の受信で、`Nm_CoordReadyToSleepIndication` を呼んで、Nm へ通知しなければならない。

**[SWS_CanNm_00348]** `CanNmCoordinatorSyncSupport` が `TRUE` に設定され、CanNm が `Nm_CoordReadyToSleepIndication` を呼んでいて、まだネットワークモードにある場合、CanNm は、NmCoordinatorSleepReady ビット（CBV を参照）が 0 に設定された NM PDU の最初の受信で、`Nm_CoordReadyToSleepCancellation` を呼んで、Nm へ通知しなければならない。

**[SWS_CanNm_00342]** `CanNmCoordinatorSyncSupport` が `TRUE` に設定され、API `CanNm_SetSleepReadyBit` が呼ばれた場合、CanNm は、「NM コーディネータスリープ準備ビット」を渡された値に設定し、単一の Network Management PDU を起動しなければならない。

### 7.10 カーウェイクアップ

**[SWS_CanNm_00405]** NM-PDU の中のカーウェイクアップビットの位置は、コンフィギュレーションパラメータ `CanNmCarWakeUpBytePosition` と `CanNmCarWakeUpBitPosition` で定義される。

#### 7.10.1 Rx の経路

**[SWS_CanNm_00406]** 受信した NM-PDU のいずれかの中のカーウェイクアップビットが 1 で、`CanNmCarWakeUpRxEnabled` が `TRUE`、`CanNmCarWakeUpFilterEnabled` が `FALSE` の場合、CanNm は、`Nm_CarWakeUpIndication` を呼び、標準の Rx 通知の処理を行わなければならない。

**[SWS_CanNm_00407]** `CanNm_GetPduData` が `Nm_CarWakeUpIndication` のコンテキストで呼ばれ、`CannNmNodeDetectionEnabled`、`CanNmUserDataEnabled`、`CanNmNodeIdEnabled` のいずれかが `TRUE` に設定されている場合、CanNm は、`Nm_CarWakeUpIndication` の呼び出しを引き起こした PDU の PDU データを返さなければならない。

注: これは、ECU が、カーウェイクアップの要求の送信側についての詳細を識別できるようにするために必要である。

**[SWS_CanNm_00408]** `CanNmCarWakeUpFilterEnabled` が `TRUE` で、受信した NM-PDU のいずれかの中のカーウェイクアップビットが 1 で、`CanNmCarWakeUpRxEnabled` が `TRUE` で、受信した NM-PDU の中のノード ID が `CanNmCarWakeUpFilterNodeId` に等しい場合、CanNm モジュールは、`Nm_CarWakeUpIndication` を呼び、標準の Rx 通知の処理を行わなければならない。

注: カーウェイクアップのフィルタは、誤ったウェイクアップを避けるために、中央のゲートウェイのカーウェイクアップだけを考慮する、サブゲートウェイを実現するために必要である。

#### 7.10.2 Tx の経路

カーウェイクアップビットの送信は、CanNm モジュールが提供する NM ユーザデータの仕組みを使って、アプリケーションが扱わなければならない。

### 7.11 パーシャルネットワーキング

#### 7.11.1 NM PDU の Rx の扱い

**[SWS_CanNm_00409]** `CanNmPnEnabled` が `FALSE` の場合、CanNm は、NM PDU を以降の Rx 通知の処理から落としてはならず、パーシャルネットワーキングの拡張は無効にされなければならない。（SRS_Nm_02517）

**[SWS_CanNm_00410]** `CanNmPnEnabled` が `TRUE` で、受信した NM-PDU の PNI ビットが 0 で、`CanNmAllNmMessagesKeepAwake` が `TRUE` の場合、CanNm モジュールは、パーシャルネットワーキングの拡張を省略して、NM PDU を以降の Rx 通知の処理から落としてはならない。（SRS_Nm_02517、SRS_Nm_02520）

注: これは、ゲートウェイがどの種類の NM-PDU でも起きたままでいられるようにするために必要である。

**[SWS_CanNm_00411]** `CanNmPnEnabled` が `TRUE` で、受信した NM-PDU の PNI ビットが 0 で、`CanNmAllNmMessagesKeepAwake` が `FALSE` の場合、CanNm モジュールは、受信した NM-PDU を無視しなければならない。（SRS_Nm_02517、SRS_Nm_02518、SRS_Nm_02520）

**[SWS_CanNm_00412]** `CanNmPnEnabled` が `TRUE` で、受信した NM-PDU の PNI ビットが 1 の場合、CanNm モジュールは、NM-PDU のパーシャルネットワーキング情報を、7.11.3 章「NM PDU フィルタのアルゴリズム」で説明されるとおりに処理しなければならない。（SRS_Nm_02517、SRS_Nm_02518、SRS_Nm_02520）

#### 7.11.2 NM PDU の Tx の扱い

**[SWS_CanNm_00413]** `CanNmPnEnabled` が `TRUE` の場合、CanNm モジュールは、送信する PNI ビットの値を 1 に設定しなければならない。（SRS_Nm_02517、SRS_Nm_02518、SRS_Nm_02521）

注: パーシャルネットワーキングが使われる場合、CBV の使用は必須である。

**[SWS_CanNm_00414]** `CanNmPnEnabled` が `FALSE` の場合、CanNm モジュールは、送信する PNI ビットの値を、常に 0 に設定しなければならない。（SRS_Nm_02517、SRS_Nm_02518）

#### 7.11.3 NM PDU フィルタのアルゴリズム

NM-PDU フィルタのアルゴリズムの意図は、ECU に関係のない、受信したすべての NM-PDU を落とすことである。受信する ECU に関係のある NM-PDU がネットワークに無い場合、NM タイムアウトタイマはもう再開されず、CanNm モジュールは、バスの通信が動作している間に、バススリープ準備モードへ移る。

ECU に関係のある NM-PDU と関係のない PDU を区別するために、CanNm は、要求する ECU が提供する PN の要求を含む、NM ユーザデータを評価する。PN の要求情報の各ビットは、1 つの PN を表す。

ECU（CanNm）が、特定のパーシャルネットワークの 1 つに含まれるかどうかは、静的にコンフィギュレーションされる。ECU が、要求されたパーシャルネットワークに含まれない場合、NM-PDU は無視される。

**[SWS_CanNm_00403]** 初期化の間に、CanNm は、`CanNmPnEnabled` が `TRUE` のすべてのネットワークで、NM-PDU フィルタのアルゴリズムを無効にしなければならない。（SRS_Nm_02517、SRS_Nm_02527）

**[SWS_CanNm_00404]** CanSm が `CanNm_ConfirmPnAvailability` を呼んだ場合、NM-PDU フィルタのアルゴリズムは、示されたチャネルで有効にされなければならない。（SRS_Nm_02517、SRS_Nm_02527）

根拠: これは、誤動作している PN トランシーバが、残りのネットワークと同期してシャットダウンできるようにするために必要である。

注: NM-PDU フィルタのアルゴリズムが有効でない場合（例: PN トランシーバの誤動作のため）、CanNm は、NM-PDU を受信したときに、NM タイムアウトタイマを再開する。したがって、通常のシャットダウンの動作が行われる。

**[SWS_CanNm_00415]** NM-PDU フィルタのアルゴリズムは、受信した NM-PDU のバイトのうち、バイト 0 から数えた `CanNmPnInfoOffset`（バイト単位）と `CanNmPnInfoLength`（バイト単位）で定義されるものを評価しなければならない。この範囲は、PN 情報の範囲（PN Info Range）と呼ばれる。（SRS_Nm_02517、SRS_Nm_02527）

**[SWS_CanNm_00416]** PN 情報の範囲の各ビットは、1 つのパーシャルネットワークを表す。ビットが 1 に設定されている場合、そのパーシャルネットワークが要求されている。ビットが 0 に設定されている場合、この PN の要求は無い。（SRS_Nm_02517）

**[SWS_CanNm_00417]** フィルタのアルゴリズムは、関係のある PN が要求されているかを検出するために、受信した PN 情報を、PN フィルタマスクと比較（ビットごとの AND）しなければならない。PN フィルタマスクの各ビットは、次の意味を持たなければならない。

- 0: PN の要求は、ECU に関係が無い。このビットが、受信した NM-PDU で設定されていても、ECU の通信スタックは起きたままにされない。
- 1: PN の要求は、ECU に関係がある。このビットが、受信した NM-PDU で設定されている場合、ECU の通信スタックは起きたままにされる。

（SRS_Nm_02517、SRS_Nm_02527）

**[SWS_CanNm_00419]** 受信した NM-PDU で、少なくとも 1 つの関係のある PN が要求されている場合、PDU は、以降の Rx 通知の処理から落とされてはならない。（SRS_Nm_02517、SRS_Nm_02527）

**[SWS_CanNm_00420]** 受信した NM-PDU で、関係のある PN が要求されておらず、`CanNmAllNmMessagesKeepAwake` が `FALSE` の場合、PDU は、以降の処理から落とされなければならない。（SRS_Nm_02517、SRS_Nm_02527）

**[SWS_CanNm_00421]** 受信した NM-PDU で、関係のある PN が要求されておらず、`CanNmAllNmMessagesKeepAwake` が `TRUE` の場合、PDU は、以降の Rx 通知の処理から落とされてはならない。（SRS_Nm_02517、SRS_Nm_02527）

注: これは、ゲートウェイがどの種類の NM-PDU でも起きたままでいられるようにするために必要である。

例: `CanNmPnInfoOffset` = 4、`CanNmPnInfoLength` = 2 の場合、NM PDU のバイト 4 とバイト 5 だけが PN 情報を含む。

> **注（再構成）**: 原典の図 7-3 と図 7-4 は、テキストの抽出で崩れていたため、内容を表にした。

図 7-3「PN 情報を含む NM PDU の例」:

| バイト 0 | バイト 1 | バイト 2〜3 | バイト 4〜5 | バイト 6〜7 |
|---|---|---|---|---|
| CBV: 0x40 | NID: 0x00 | ユーザデータ: 0xFF 0xFF | PN 情報: 0x12 0x8E | ユーザデータ: 0xFF 0xFF |

この例では、2 つの `CanNmPnFilterMaskByte` が定義される。例えば、

- `CanNmPnFilterMaskByteIndex` = 0 で `CanNmPnFilterMaskByteValue` = 0x01
- `CanNmPnFilterMaskByteIndex` = 1 で `CanNmPnFilterMaskByteValue` = 0x97

フィルタのアルゴリズムの動作と結果は、次のようになる（図 7-4）。

| フィルタマスクの値（バイト） | 受信した PN 情報との比較 | 結果 |
|---|---|---|
| 0x01（バイト 0） | 0x12（NM PDU のバイト 4） | 0x00（関係のある PN 情報なし） |
| 0x97（バイト 1） | 0x8E（NM PDU のバイト 5） | 0x86（関係のある PN 情報あり） |

1 つのバイトに関係のある情報が含まれるため、この NM PDU は、以降の Rx 通知の処理から落とされない。

#### 7.11.4 内部と外部から要求されたパーシャルネットワークの集約

注: この機能は、パーシャルネットワークの動作のために、I-PDU グループを切り替える必要のある、すべての ECU で使われる（例: 誤ったタイムアウトを防ぐ）。I-PDU グループは、対応する PN が内部または外部から要求された場合に、オンに切り替えられなければならない。I-PDU グループは、対応する PN の内部と外部のすべての要求が解放されるまで、オフに切り替えてはならない。I-PDU グループを切り替える論理は、ComM に実装される。CanNm は、PN が要求されているかどうかの情報だけを提供する。COM モジュールは、データを上位層へ渡すために使われる。直接接続されたすべての ECU で、I-PDU グループを同期して切り替えるために、CanNm は、要求の変化の情報を、すべての ECU で（ほぼ）同じ時刻に、上位層へ提供しなければならない。そのため、リセットタイマは、受信した、および送信した NM PDU のたびに再開される（下記を参照）。内部／外部から要求された PN の集約された状態は、External Internal Request Array（EIRA）と呼ばれる。

**[SWS_CanNm_00424]** `CanNmPnEiraCalcEnabled` が `TRUE` の場合、CanNm は、関係のあるすべてのチャネル（`CanNmPnEnabled` が `TRUE` のすべての CanNm チャネル）にわたって合わせた、外部と内部から要求された PN を保存する可能性を提供しなければならない。初期化時に、すべての PN の値は 0（要求なし）に設定されなければならない。（SRS_Nm_02517、SRS_Nm_02522）

**[SWS_CanNm_00426]** 次のすべてが成り立つ場合、CanNm は、これらの PN の要求情報（値 1）を保存しなければならない。（SRS_Nm_02517、SRS_Nm_02522）

- `CanNmPnEiraCalcEnabled` が `TRUE`
- かつ、NM-PDU が受信された
- かつ、このメッセージの中で PN が要求されている（ビットが 1）
- かつ、要求された PN が、コンフィギュレーションされた PN フィルタマスクの中で 1 に設定されている

**[SWS_CanNm_00427]** 次のすべてが成り立つ場合、CanNm は、これらの PN の要求情報（値 1）を保存しなければならない。（SRS_Nm_02517、SRS_Nm_02522）

- `CanNmPnEiraCalcEnabled` が `TRUE`
- かつ、NM-PDU が、CanNm により送信を要求されている
- かつ、このメッセージの中で PN が要求されている（ビットが 1）
- かつ、要求された PN が、コンフィギュレーションされた PN フィルタマスクの中で 1 に設定されている

**[SWS_CanNm_00428]** `CanNmPnEiraCalcEnabled` が `TRUE` の場合、CanNm は、各 PN について、この PN が、関係のあるチャネルの少なくとも 1 つで、まだ外部または内部から要求されているかを監視する可能性を提供しなければならない。（SRS_Nm_02517、SRS_Nm_02522）

注: これは、複数の接続された物理チャネルの 1 つの PN を扱うために、タイマが 1 つだけ必要であることを意味する。例: 6 つの物理チャネルと 8 つのパーシャルネットワークを持つゲートウェイの要求を扱うために、EIRA のリセットタイマが 8 つだけ必要である。これは、PN の PDU グループの切り替えが、ECU について全体で行われ、物理チャネルに依存しないために可能である。

**[SWS_CanNm_00429]** `CanNmPnEiraCalcEnabled` が `TRUE` で、PN がメッセージの受信または送信により要求された場合（[SWS_CanNm_00426] と [SWS_CanNm_00427] を参照）、この PN の監視は、`CanNmPnResetTime` に関して再開されなければならない。（SRS_Nm_02517、SRS_Nm_02522）

注: `CanNmPnResetTime` は、`CanNmMsgCycleTime` より大きい値にコンフィギュレーションしなければならない。`CanNmPnResetTime` が `CanNmMsgCycleTime` より小さい値にコンフィギュレーションされ、1 つの ECU だけが PN を要求している場合、要求している ECU が次の NM PDU を送れる前に要求の状態がリセットされるため、EIRA の要求の状態が、トグルする。

注: `CanNmPnResetTime` は、NM がすでにバススリープ準備へ移った後に、タイマが経過しうることを避けるため、`CanNmTimeoutTime` より小さい値にコンフィギュレーションしなければならない。

**[SWS_CanNm_00431]** `CanNmPnEiraCalcEnabled` が `TRUE` で、PN が `CanNmPnResetTime` の間に再び要求されなかった場合、この PN の対応する保存された値は、要求なし（値 0）に設定されなければならない。（SRS_Nm_02517、SRS_Nm_02522）

**[SWS_CanNm_00432]** `CanNmPnEiraCalcEnabled` が `TRUE` で、PN の保存された値が、要求ありに、または要求なしに戻るように設定された場合（[SWS_CanNm_00426]、[SWS_CanNm_00427]、[SWS_CanNm_00429] を参照）、CanNm は、コンフィギュレーションされた EIRA の PDU について `PduR_CanNmRxIndication()` を呼んで、上位層へ通知しなければならない（すなわち、変更された EIRA の情報が、COM へ渡される）。（SRS_Nm_02517、SRS_Nm_02524）

#### 7.11.5 外部から要求されたパーシャルネットワークの集約

注: この機能は、外部の PN の要求だけを集めるために、ゲートウェイで使われる。外部の PN の要求は、要求しているバスへ折り返され、中央のゲートウェイの他の（必要な）物理チャネルへ提供される。サブゲートウェイの場合、中央のゲートウェイとサブゲートウェイの間の、静的なウェイクアップを避けるために、要求ビットを、要求している物理チャネルへ折り返してはならない。この論理は、ComM に実装されなければならない。CanNm モジュールは、PN が外部から要求されているかどうかの情報を提供する。COM モジュールは、上位層へのデータの送信に使われる。外部から要求された PN の集約された状態は、「External Request Array」（ERA）と呼ばれる。

**[SWS_CanNm_00435]** `CanNmPnEraCalcEnabled` が `TRUE` の場合、CanNm は、関係のある各チャネルで、外部から要求された PN を保存する可能性を提供しなければならない。初期化時に、すべての PN の値は 0（要求なし）に設定されなければならない。（SRS_Nm_02517、SRS_Nm_02523）

**[SWS_CanNm_00437]** 次のすべてが成り立つ場合、CanNm は、これらの PN の要求情報（値 1）を保存しなければならない。（SRS_Nm_02517、SRS_Nm_02523）

- `CanNmPnEraCalcEnabled` が `TRUE`
- かつ、NM-PDU が受信された
- かつ、このメッセージの中で PN が要求されている（ビットが 1）
- かつ、要求された PN が、コンフィギュレーションされた PN フィルタマスクの中で 1 に設定されている

**[SWS_CanNm_00438]** `CanNmPnEraCalcEnabled` が `TRUE` の場合、CanNm は、関係のある各チャネルで、各 PN について、この PN がまだ外部から要求されているかを監視する可能性を提供しなければならない。（SRS_Nm_02517、SRS_Nm_02523）

注: これは、複数の物理チャネルの 1 つの PN を扱うために、別々のタイマが必要であることを意味する。例: 6 つの物理チャネルと 8 つのパーシャルネットワークを持つゲートウェイの要求を扱うために、48 個の ERA のリセットタイマが必要である。EIRA のタイマのようにリセットタイマをまとめることはできない。これは、サブゲートウェイが、外部の要求を、要求しているバスへ折り返してはならないためである。したがって、要求ビットの発生元の物理チャネルを検出する必要がある。

**[SWS_CanNm_00439]** `CanNmPnEraCalcEnabled` が `TRUE` で、PN がメッセージの受信により要求された場合（[SWS_CanNm_00437] を参照）、この PN の監視は、`CanNmPnResetTime` に関して再開されなければならない。（SRS_Nm_02517、SRS_Nm_02523）

注: `CanNmPnResetTime` は、`CanNmMsgCycleTime` より大きい値にコンフィギュレーションしなければならない。`CanNmPnResetTime` が `CanNmMsgCycleTime` より小さい値にコンフィギュレーションされ、1 つの ECU だけが PN を要求している場合、要求している ECU が次の NM-PDU を送れる前に要求の状態がリセットされるため、ERA の要求の状態が、トグルする。

注: `CanNmPnResetTime` は、NM がすでにバススリープ準備へ移った後に、タイマが経過しうることを避けるため、`CanNmTimeoutTime` より小さい値にコンフィギュレーションしなければならない。

**[SWS_CanNm_00442]** `CanNmPnEraCalcEnabled` が `TRUE` で、PN が `CanNmPnResetTime` の間に再び要求されなかった場合、この PN の対応する保存された値は、要求なし（値 0）に設定されなければならない。（SRS_Nm_02517、SRS_Nm_02523）

**[SWS_CanNm_00443]** `CanNmPnEraCalcEnabled` が `TRUE` で、PN の保存された値が、要求ありに、または要求なしに戻るように変わった場合（[SWS_CanNm_00437] と [SWS_CanNm_00442] を参照）、CanNm は、コンフィギュレーションされた ERA の PDU について `PduR_CanNmRxIndication()` を呼んで、上位層へ通知しなければならない（すなわち、変更された ERA の情報が、COM へ渡される）。（SRS_Nm_02517、SRS_Nm_02524）

**[SWS_CanNm_00445]** `CanNmPnEiraCalcEnabled` が `TRUE` で、`CanNmPnEraCalcEnabled` が `TRUE` の場合、PN の状態情報は、EIRA と ERA の情報の両方について、別々に保存されなければならない（[SWS_CanNm_00435] と [SWS_CanNm_00424] を比較）。（SRS_Nm_02517）

#### 7.11.6 `CanNm_NetworkRequest` による NM PDU の自発的な送信

**[SWS_CanNm_00444]** `CanNm_NetworkRequest` が呼ばれ、`CanNmPnHandleMultipleNetworkRequests` が `TRUE` で、CanNm がスリープ準備完了状態、通常動作状態、またはリピートメッセージ状態にある場合、CanNm は、リピートメッセージ状態へ変更する、または再開しなければならない。（SRS_Nm_02517、SRS_Nm_02528）

注: `CanNmPnHandleMultipleNetworkRequests` が `TRUE` に設定されている場合、CanNm の機能「即時の送信（Immediate Transmission）」は必須である。

注: PN 制御モジュール（例: ComM）は、PN の要求ビットが変わったときに、`CanNm_NetworkRequest` を呼ぶ責任を持つ。

### 7.12 送信エラーの扱い

コンフィギュレーションに応じて、CanNm は、Network Management PDU が正常に送信されたかどうかの確認関数を評価する。CanNm は、これらの確認を監視し、送信確認が結果 `E_NOT_OK` で受信された場合、または特定の時間内に受信されなかった場合に、上位層へ警告する。タイムアウトの監視は、ネットワークのすべての ECU が、パーシャルネットワークのトランシーバを使う場合に、最初のメッセージが確認されることを保証するために、パーシャルネットワーキングで必要である。そうでない場合、CanSM へ通知され、CanSM は CAN ドライバを再開する（SWS CanSM も参照）。

**[SWS_CanNm_00073]** `CanNmPassiveModeEnabled` が `TRUE` に設定されている場合（[SWS_CanNm_00072] を参照）、または `CanNmImmediateTxConfEnabled` が `TRUE` に設定されている場合、CanNm は、送信エラーの扱いを行ってはならず、要求 [SWS_CanNm_00061]、[SWS_CanNm_00064]、[SWS_CanNm_00065]、[SWS_CanNm_00066]、[SWS_CanNm_00446] を省略しなければならない。

根拠: 送信エラーの扱いは、ノードが Network Management PDU を送信でき、CanIf からの実際の確認が評価される場合にだけ、意味がある。

**[SWS_CanNm_00064]** `CanNmGlobalPnSupport` が `TRUE` に設定され、`CanNmMsgTimeoutTime` が定義されていて、CanNm が NM PDU の送信を要求した場合（`CanIf_Transmit` の呼び出し）、CanNm は、NM メッセージの Tx タイムアウトタイマを、`CanNmMsgTimeoutTime` で開始しなければならない。（SRS_Nm_00137）

**[SWS_CanNm_00065]** `CanNmGlobalPnSupport` が `TRUE` に設定され、`CanNmMsgTimeoutTime` が定義されていて、`CanNm_TxConfirmation` が呼ばれた場合、CanNm は、NM メッセージの Tx タイムアウトタイマを止めなければならない。（SRS_Nm_00137）

**[SWS_CanNm_00066]** `CanNm_TxConfirmation` が結果 `E_NOT_OK` で呼ばれた場合、または `CanNmGlobalPnSupport` が `TRUE` に設定され、NM メッセージの Tx タイムアウトタイマが満了した場合、CanNm は、関数 `Nm_TxTimeoutException` を呼ばなければならない。（SRS_Nm_00137）

**[SWS_CanNm_00446]** `CanNmGlobalPnSupport` が `TRUE` に設定され、NM メッセージの Tx タイムアウトタイマが満了した場合、CanNm は、関数 `CanSM_TxTimeoutException` を呼ばなければならない。（SRS_Nm_00137、SRS_Nm_02529）

### 7.13 CanNm API に対する機能要求

**[SWS_CanNm_00014]** `CanNmNodeDetectionEnabled` と `CanNmRepeatMsgIndEnabled` が `TRUE` に設定され、リピートメッセージ要求ビットが受信された場合、CanNm モジュールは、コールバック関数 `Nm_RepeatMessageIndication` を呼ばなければならない。（SRS_Nm_00153）

**[SWS_CanNm_00086]** `CanNmUserDataEnabled` が有効なのに、ユーザデータのバイトが利用できない場合、CanNm モジュールは、コンフィギュレーションまたはコンパイルの時点で、エラーを発生させなければならない。

### 7.14 エラーの分類

#### 7.14.1 開発エラー

次の開発エラーが、CanNm により検出できなければならない。

**[SWS_CanNm_00316]** 開発エラーの種類

> **注（再構成）**: 原典の表は、テキストの抽出で、行の対応が崩れていたため、復元した。

| エラーの種類 | 関連するエラーコード | エラーの値 |
|---|---|---|
| モジュールの初期化なしで API サービスが使われた | `CANNM_E_UNINIT` | 0x01 |
| API サービスが誤ったチャネルハンドルで呼ばれた | `CANNM_E_INVALID_CHANNEL` | 0x02 |
| API サービスが誤った PDU-ID で呼ばれた | `CANNM_E_INVALID_PDUID` | 0x03 |
| CanNm の初期化が失敗した（例: 選択されたコンフィギュレーションセットが存在しない） | `CANNM_E_INIT_FAILED` | 0x05 |
| 引数として NULL ポインタが渡された | `CANNM_E_PARAM_POINTER` | 0x12 |
| すべての CAN ネットワークがバススリープモードでないときに、DeInit API サービスが呼ばれた | `CANNM_E_NOT_IN_BUS_SLEEP` | 0x13 |

#### 7.14.2 ランタイムエラー

**[SWS_CanNm_00317]** ランタイムエラーの種類

| エラーの種類 | 関連するエラーコード | エラーの値 |
|---|---|---|
| バススリープモードでの NM PDU の受信 | `CANNM_E_NET_START_IND` | 0x04 |
| NM タイムアウトタイマが、スリープ準備完了状態の外で、異常に満了した。次の場合に起こりうる: (1) バスオフの状態のため、(2) NM タイムアウトタイマが満了する直前に、ある ECU がバスの通信またはノード検出を要求し、Network Management PDU を間に合うように送信できなかった。この競合状態は、イベント駆動のシステムに当てはまる | `CANNM_E_NETWORK_TIMEOUT` | 0x11 |

#### 7.14.3 一時的な故障

現在、CanNm は、一時的な故障を報告しない。

#### 7.14.4 プロダクションエラー

現在、CanNm は、プロダクションエラーを報告しない。

#### 7.14.5 拡張プロダクションエラー

現在、CanNm は、拡張プロダクションエラーを報告しない。

### 7.15 エラーの検出

詳細は、SWS_BSWGeneral [9] の 7.3 章「エラーの検出」を参照。

### 7.16 エラーの通知

**[SWS_CanNm_00189]** CanNm モジュールは、API 関数で開発エラーを返してはならない。開発エラーの場合、該当する API 関数の実行は中止され、該当する場合は `E_NOT_OK` が返されなければならない。

**[SWS_CanNm_00190]** CanNm モジュールは、API 関数でプロダクションエラーを返してはならない。プロダクションエラーの場合、該当する API 関数の実行は中止され、該当する場合は `E_NOT_OK` が返されなければならない。

注: 現在、CAN NM にプロダクションエラーは規定されていない。

**[SWS_CanNm_00191]** CanNm の初期化がされていないために実行されなかった各 CanNm の関数は、該当する場合、呼び出した関数へ `E_NOT_OK` を返さなければならない。開発エラーの検出が有効な場合（`CanNmDevErrorDetect` が `TRUE` に設定されている）、対応するエラー `CANNM_E_UNINIT` が DET へ報告されなければならない。

**[SWS_CanNm_00192]** 無効なネットワークハンドルで CanNm のサービスが呼ばれた場合、呼ばれた関数は実行されてはならず、該当する場合、呼び出した関数へ `E_NOT_OK` を返さなければならない。開発エラーの検出が有効な場合（`CanNmDevErrorDetect` が `TRUE` に設定されている）、対応するエラー `CANNM_E_INVALID_CHANNEL` が DET へ報告されなければならない。

注: ネットワークハンドルは、許されるコンフィギュレーションされた値と異なる場合に、無効である。

注: NULL ポインタの検査は、BSW General [9] の中で規定されている。

**[SWS_CanNm_00193]** リピートメッセージ状態で NM タイムアウトタイマが満了したとき、CanNm モジュールは、`CANNM_E_NETWORK_TIMEOUT` を DET へ報告しなければならない。（SRS_Nm_00137）

**[SWS_CanNm_00194]** 通常動作状態で NM タイムアウトタイマが満了したとき、CanNm モジュールは、`CANNM_E_NETWORK_TIMEOUT` を DET へ報告しなければならない。（SRS_Nm_00137）

**[SWS_CanNm_00195]** 無効な PDU ID で CanNm のサービスが呼ばれた場合、呼ばれた関数は実行されてはならず、該当する場合、呼び出した関数へ `E_NOT_OK` を返さなければならない。開発エラーの検出が有効な場合（`CanNmDevErrorDetect` が `TRUE` に設定されている）、対応するエラー `CANNM_E_INVALID_PDUID` が DET へ報告されなければならない。

### 7.17 メイン関数のスケジューリング

詳細は、SWS_BSWGeneral [9] の 8.5 章「スケジュールされる関数」を参照。

### 7.18 アプリケーションの注意

#### 7.18.1 ウェイクアップの通知

ウェイクアップの通知は、ECU State Manager の仕様に詳しく定義されている。

#### 7.18.2 結合されたネットワークの調整

**[SWS_CanNm_00185]** オンデマンドのバスの同期のサポートは、`CanNmBusSynchronizationEnabled` スイッチ（コンフィギュレーションパラメータ）を使って、静的にコンフィギュレーションできなければならない。

注: CanNm のシャットダウンはいつでも行えるため、API `Nm_SynchronizationPoint` の呼び出しはサポートされない。

#### 7.18.3 デバッグの概念

詳細は、SWS_BSWGeneral [9] の 7.1.17 章「デバッグのサポート」を参照。

### 7.19 CanNm のタイミング要求のまとめ

この項は、CanNm のタイミング要求のまとめを示す。この章はまとめにすぎず、要求を置き換えたり、要求として働いたりしないことに注意。さらに、この項は、実装の特定の方法を要求しない。

> **注（再構成）**: 原典の表は、テキストの抽出で、行と列の対応が崩れていた。項目と要求の ID の並びから復元したが、行との対応は確定できなかった（特に [SWS_CanNm_00061] と [SWS_CanNm_00173]、[SWS_CanNm_00175]、[SWS_CanNm_00180] の位置）。原典の表（50 ページ）で確認すること。

| タイミングの種類 | 要求 |
|---|---|
| NM タイムアウトに関するもの | [SWS_CanNm_00061]、[SWS_CanNm_00096]、[SWS_CanNm_00098]、[SWS_CanNm_00099]、[SWS_CanNm_00101]、[SWS_CanNm_00109]、[SWS_CanNm_00117]、[SWS_CanNm_00174]、[SWS_CanNm_00179]、[SWS_CanNm_00193]、[SWS_CanNm_00194]、[SWS_CanNm_00206] |
| Tx 確認のタイムアウトに関するもの | [SWS_CanNm_00064]、[SWS_CanNm_00065]、[SWS_CanNm_00066] |
| NmPdu の送信に関するもの | [SWS_CanNm_00005]、[SWS_CanNm_00032]、[SWS_CanNm_00040] |
| リモートスリープ通知に関するもの | [SWS_CanNm_00051]、[SWS_CanNm_00061]、[SWS_CanNm_00069]、[SWS_CanNm_00173]、[SWS_CanNm_00178]、[SWS_CanNm_00175]、[SWS_CanNm_00180] |

### 7.20 UML のステートチャート図

次の図は、API 仕様に関する UML の状態図を示す。モードの変更に関する遷移は緑、エラー処理に関する遷移は赤、オプションのノード検出に関する遷移は青で示される。さらに、バス負荷の低減の機能が有効であると仮定している。

> （図 7-5「CanNm のアルゴリズム」: 状態機械図。訳なし。原典を参照。
> 　読み取れる内容: 電源投入後、`CanNm_Init()` でバススリープモード（初期化では `Nm_BusSleepMode` を呼ばない）。バススリープモードで `CanNm_RxIndication()` を受けると `Nm_NetworkStartIndication()`（モードは変わらない）、`CanNm_PassiveStartUp()` または `CanNm_NetworkRequest()` でネットワークモードのリピートメッセージ状態へ（NM タイムアウトタイマとリピートメッセージタイマを開始し、`Nm_NetworkMode()`）。リピートメッセージ状態はリピートメッセージタイマの満了で、ネットワークが要求されていれば通常動作状態へ、解放されていればスリープ準備完了状態へ。通常動作状態は `CanNm_NetworkRelease()` でスリープ準備完了状態へ、スリープ準備完了状態は `CanNm_NetworkRequest()` で通常動作状態へ。スリープ準備完了状態で NM タイムアウトタイマが満了すると、バススリープ準備モードへ（バススリープ待ちタイマを開始し、`Nm_PrepareBusSleepMode()`）。バススリープ準備モードは、バススリープ待ちタイマの満了で `Nm_BusSleepMode()` を呼んでバススリープモードへ、`CanNm_RxIndication()`、`CanNm_NetworkRequest()`、`CanNm_PassiveStartUp()` でネットワークモードへ戻る。ノード検出が有効なら、リピートメッセージビットの受信または `CanNm_RepeatMessageRequest()` で、通常動作状態とスリープ準備完了状態からリピートメッセージ状態へ。Tx タイムアウトタイマの満了で `Nm_TxTimeoutException()`。リピートメッセージ状態への進入時はバス負荷の低減を止め、通常動作状態への進入時にバス負荷の低減を開始する。）

## 8 API 仕様

**[SWS_CanNm_00244]** CanNm モジュールは、無効なパラメータで呼ばれたサービスの実行を拒否しなければならず、DET へ通知しなければならない。（SRS_BSW_00323）

AUTOSAR の CanNm の API は、CAN 固有で、必要なときにいつでも呼べるサービスから成る。`CanNm_Init` 以外の各サービスは、1 つの NM チャネルだけを参照する。

### 8.1 インポートする型

この章では、次のモジュールから取り込まれるすべての型を示す。

**[SWS_CanNm_00245]**

| モジュール | インポートする型 |
|---|---|
| ComStack_Types | `NetworkHandleType`、`PduIdType`、`PduInfoType` |
| Nm | `Nm_ModeType`、`Nm_StateType` |
| Std_Types | `Std_ReturnType`、`Std_VersionInfoType` |

これらの型の詳細は、対応する仕様 [4]、[6]、[8] を参照。

### 8.2 型の定義

#### 8.2.1 CanNm_ConfigType

**[SWS_CanNm_00447]**

| 項目 | 内容 |
|---|---|
| 名前 | `CanNm_ConfigType` |
| 型 | 構造体 |
| 範囲 | 実装固有 |
| 説明 | この型は、少なくとも、10 章に従ってポストビルドで変更できるすべてのパラメータを含まなければならない。 |

### 8.3 関数の定義

#### 8.3.1 CanNm_Init

**[SWS_CanNm_00208]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_Init` |
| 構文 | `void CanNm_Init(const CanNm_ConfigType* cannmConfigPtr)` |
| サービス ID [hex] | 0x00 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `cannmConfigPtr`: 選択されたコンフィギュレーション構造体へのポインタ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | CanNm モジュールを初期化する。 |

**[SWS_CanNm_00253]** `CanNm_Init` の注意点: 関数 `CanNm_Init` は、CanIf の初期化の後に呼ばなければならない。

#### 8.3.2 CanNm_DeInit

**[SWS_CanNm_91002]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_DeInit` |
| 構文 | `void CanNm_DeInit(void)` |
| サービス ID [hex] | 0x10 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | CanNm モジュールを終了処理（De-initialize）する。 |

（SRS_BSW_00336）

注: 終了処理の関数の一般的な動作と制約は、[SWS_BSW_00152]、[SWS_BSW_00072]、[SWS_BSW_00232]、[SWS_BSW_00233] で規定されている。

注意: `CanNm_DeInit` 関数の呼び出し元は、すべての CAN ネットワークがバススリープモードであることを、確実にしなければならない。

**[SWS_CanNm_00352]** CanNm モジュールの開発エラーの検出が有効な場合: すべての CAN ネットワークがバススリープモードでないとき、関数 `CanNm_DeInit` は、エラー `CANNM_E_NOT_IN_BUS_SLEEP` を発生させなければならない。（SRS_BSW_00369）

#### 8.3.3 CanNm_PassiveStartUp

**[SWS_CanNm_00211]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_PassiveStartUp` |
| 構文 | `Std_ReturnType CanNm_PassiveStartUp(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x01 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ネットワークマネジメントのパッシブな起動が失敗した |
| 説明 | AUTOSAR CAN NM のパッシブな起動。これは、バススリープモードまたはバススリープ準備モードから、ネットワークモードのリピートメッセージ状態への遷移を起動する。注意点: CanNm が正しく初期化されている。 |

**[SWS_CanNm_00254]** `CanNm_PassiveStartUp` の注意点: CanNm モジュールが正しく初期化されている。

#### 8.3.4 CanNm_NetworkRequest

**[SWS_CanNm_00213]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_NetworkRequest` |
| 構文 | `Std_ReturnType CanNm_NetworkRequest(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x02 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ネットワークの要求が失敗した |
| 説明 | ECU がバスで通信する必要があるため、ネットワークを要求する。 |

**[SWS_CanNm_00255]** 関数 `CanNm_NetworkRequest` は、ネットワークの状態を「要求」に変更しなければならない。

**[SWS_CanNm_00256]** `CanNm_NetworkRequest` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00257]** `CanNm_NetworkRequest` のコンフィギュレーション: オプション（`CanNmPassiveModeEnabled` が定義されていない場合にだけ利用できる）。

#### 8.3.5 CanNm_NetworkRelease

**[SWS_CanNm_00214]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_NetworkRelease` |
| 構文 | `Std_ReturnType CanNm_NetworkRelease(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x03 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ネットワークの解放が失敗した |
| 説明 | ECU がバスで通信する必要が無いため、ネットワークを解放する。 |

**[SWS_CanNm_00259]** `CanNm_NetworkRelease` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00260]** `CanNm_NetworkRelease` のコンフィギュレーション: オプション（`CanNmPassiveModeEnabled` が定義されていない場合にだけ利用できる）。

#### 8.3.6 CanNm_DisableCommunication

**[SWS_CanNm_00215]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_DisableCommunication` |
| 構文 | `Std_ReturnType CanNm_DisableCommunication(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x0c |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM PDU の送信能力の無効化が失敗した |
| 説明 | ISO14229 の Communication Control（28hex）サービスにより、NM PDU の送信能力を無効にする。 |

**[SWS_CanNm_00261]** `CanNm_DisableCommunication` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00262]** `CanNm_DisableCommunication` のコンフィギュレーション: オプション（`CanNmComControlEnabled` が `TRUE` に設定されている場合にだけ利用できる）。

**[SWS_CanNm_00172]** サービス `CanNm_DisableCommunication` は、現在のモードがネットワークモードでない場合、`E_NOT_OK` を返さなければならない。

**[SWS_CanNm_00298]** モジュールがパッシブモード（`CanNmPassiveModeEnabled`）で動作する場合、サービス `CanNm_DisableCommunication` は、効果を持たず、直接 `E_NOT_OK` を返さなければならない。

#### 8.3.7 CanNm_EnableCommunication

**[SWS_CanNm_00216]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_EnableCommunication` |
| 構文 | `Std_ReturnType CanNm_EnableCommunication(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x0d |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM PDU の送信能力の有効化が失敗した |
| 説明 | ISO14229 の Communication Control（28hex）サービスにより、NM PDU の送信能力を有効にする。 |

**[SWS_CanNm_00176]** サービス `CanNm_EnableCommunication` は、Network Management PDU の送信能力が無効の場合、Network Management PDU の送信能力を有効にしなければならない。（SRS_Nm_02512）

**[SWS_CanNm_00177]** サービス `CanNm_EnableCommunication` は、Network Management PDU の送信能力が有効の場合、`E_NOT_OK` を返さなければならない。

**[SWS_CanNm_00295]** サービス `CanNm_EnableCommunication` は、現在のモードがネットワークモードでない場合、`E_NOT_OK` を返さなければならない。

**[SWS_CanNm_00263]** `CanNm_EnableCommunication` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00264]** `CanNm_EnableCommunication` のコンフィギュレーション: オプション（`CanNmComControlEnabled` が `TRUE` に設定されている場合にだけ利用できる）。

**[SWS_CanNm_00297]** モジュールがパッシブモード（`CanNmPassiveModeEnabled`）で動作する場合、サービス `CanNm_EnableCommunication` は、効果を持たず、直接 `E_NOT_OK` を返さなければならない。

#### 8.3.8 CanNm_SetUserData

**[SWS_CanNm_00217]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_SetUserData` |
| 構文 | `Std_ReturnType CanNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8* nmUserDataPtr)` |
| サービス ID [hex] | 0x04 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別<br>`nmUserDataPtr`: 次に送信される NM PDU のユーザデータのコピー元のポインタ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ユーザデータの設定が失敗した |
| 説明 | 次にバスで送信される NM PDU のユーザデータを設定する。 |

**[SWS_CanNm_00265]** `CanNm_SetUserData` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00266]** `CanNm_SetUserData` のコンフィギュレーション: オプション（`CanNmUserDataEnabled` が `TRUE` に設定され、`CanNmPassiveModeEnabled` が定義されていない場合にだけ利用できる）。

#### 8.3.9 CanNm_GetUserData

**[SWS_CanNm_00218]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_GetUserData` |
| 構文 | `Std_ReturnType CanNm_GetUserData(NetworkHandleType nmChannelHandle, uint8* nmUserDataPtr)` |
| サービス ID [hex] | 0x05 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmUserDataPtr`: 最も最近受信した NM PDU のユーザデータのコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ユーザデータの取得が失敗した |
| 説明 | 最も最近受信した NM PDU から、ユーザデータを取得する。 |

**[SWS_CanNm_00267]** `CanNm_GetUserData` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00268]** `CanNm_GetUserData` のコンフィギュレーション: オプション（`CanNmUserDataEnabled` が `TRUE` に設定されている場合にだけ利用できる）。

#### 8.3.10 CanNm_Transmit

**[SWS_CanNm_00331]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_Transmit` |
| 構文 | `Std_ReturnType CanNm_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x49 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信する PDU の識別子<br>`PduInfoPtr`: PDU のデータの長さとポインタ、およびメタデータへのポインタ。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 送信要求が受理された。<br>`E_NOT_OK`: 送信要求が受理されなかった。 |
| 説明 | PDU の送信を要求する。 |

**[SWS_CanNm_00330]** `CanNmComUserDataSupport` または `CanNmGlobalPnSupport` が有効な場合、CanNm の実装は、API `CanNm_Transmit` を提供しなければならない。

**[SWS_CanNm_00333]** `CanNmComUserDataSupport` が有効で、CanNm がリピートメッセージ状態または通常動作状態にあり、`CanNm_Transmit()` が呼ばれた場合、CanNm は、現在のユーザデータで、NM PDU の追加の送信を要求しなければならない。（SRS_Nm_02527）

#### 8.3.11 CanNm_GetNodeIdentifier

**[SWS_CanNm_00219]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_GetNodeIdentifier` |
| 構文 | `Std_ReturnType CanNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8* nmNodeIdPtr)` |
| サービス ID [hex] | 0x06 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmNodeIdPtr`: 最も最近受信した NM PDU のノード識別子のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: 最も最近受信した NM PDU のノード識別子の取得が失敗した、またはこのネットワークハンドルについてコンフィギュレーションされていない。 |
| 説明 | 最も最近受信した NM PDU から、ノード識別子を取得する。 |

**[SWS_CanNm_00132]** サービス呼び出し `CanNm_GetNodeIdentifier` は、`CanNmNodeIdEnabled` が `TRUE` に設定されている場合、最も最近受信した Network Management PDU のノード識別子を提供しなければならない。（SRS_Nm_02506）

**[SWS_CanNm_00269]** `CanNm_GetNodeIdentifier` の注意点: CanNm モジュールが正しく初期化されている。

#### 8.3.12 CanNm_GetLocalNodeIdentifier

**[SWS_CanNm_00220]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_GetLocalNodeIdentifier` |
| 構文 | `Std_ReturnType CanNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle, uint8* nmNodeIdPtr)` |
| サービス ID [hex] | 0x07 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmNodeIdPtr`: ローカルノードのノード識別子のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ローカルノードのノード識別子の取得が失敗した、またはこのネットワークハンドルについてコンフィギュレーションされていない。 |
| 説明 | ローカルノードにコンフィギュレーションされたノード識別子を取得する。 |

**[SWS_CanNm_00133]** サービス呼び出し `CanNm_GetLocalNodeIdentifier` は、`CanNmNodeIdEnabled` が `TRUE` に設定されている場合、ローカルのホストノードにコンフィギュレーションされたノード識別子を提供しなければならない。

**[SWS_CanNm_00271]** `CanNm_GetLocalNodeIdentifier` の注意点: CanNm モジュールが正しく初期化されている。

#### 8.3.13 CanNm_RepeatMessageRequest

**[SWS_CanNm_00221]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_RepeatMessageRequest` |
| 構文 | `Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x08 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: リピートメッセージ要求ビットの設定が失敗した、またはこのネットワークハンドルについてコンフィギュレーションされていない |
| 説明 | 次にバスで送信される NM PDU に、リピートメッセージ要求ビットを設定する。 |

**[SWS_CanNm_00273]** `CanNm_RepeatMessageRequest` の注意点: CanNm モジュールが正しく初期化されている。

#### 8.3.14 CanNm_GetPduData

**[SWS_CanNm_00222]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_GetPduData` |
| 構文 | `Std_ReturnType CanNm_GetPduData(NetworkHandleType nmChannelHandle, uint8* nmPduDataPtr)` |
| サービス ID [hex] | 0x0a |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmPduDataPtr`: NM PDU のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM PDU のデータの取得が失敗した、またはこのネットワークハンドルについてコンフィギュレーションされていない。 |
| 説明 | 最も最近受信した NM PDU から、PDU データの全体を取得する。 |

**[SWS_CanNm_00275]** `CanNm_GetPduData` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00138]** サービス呼び出し `CanNm_GetPduData` は、`CanNmNodeDetectionEnabled`、`CannNmUserDataEnabled`、`CanNmNodeIdEnabled` のいずれかが `TRUE` に設定されている場合、最も最近受信した Network Management PDU の PDU データの全体（ノード ID、Control Bit Vector、ユーザデータ）を提供しなければならない。

#### 8.3.15 CanNm_GetState

**[SWS_CanNm_00223]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_GetState` |
| 構文 | `Std_ReturnType CanNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr)` |
| サービス ID [hex] | 0x0b |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmStatePtr`: ネットワークマネジメントの状態のコピー先のポインタ<br>`nmModePtr`: ネットワークマネジメントのモードのコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM の状態の取得が失敗した |
| 説明 | ネットワークマネジメントの状態とモードを返す。 |

**[SWS_CanNm_00277]** `CanNm_GetState` の注意点: CanNm モジュールが正しく初期化されている。

#### 8.3.16 CanNm_GetVersionInfo

**[SWS_CanNm_00224]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_GetVersionInfo` |
| 構文 | `void CanNm_GetVersionInfo(Std_VersionInfoType* versioninfo)` |
| サービス ID [hex] | 0xf1 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | `versioninfo`: このモジュールのバージョン情報を格納する場所へのポインタ |
| 戻り値 | なし |
| 説明 | このサービスは、このモジュールのバージョン情報を返す。 |

#### 8.3.17 CanNm_RequestBusSynchronization

**[SWS_CanNm_00226]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_RequestBusSynchronization` |
| 構文 | `Std_ReturnType CanNm_RequestBusSynchronization(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0xc0 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: バスの同期の要求が失敗した |
| 説明 | バスの同期を要求する。 |

（SRS_Nm_02516）

**[SWS_CanNm_00279]** `CanNm_RequestBusSynchronization` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00280]** `CanNm_RequestBusSynchronization` のコンフィギュレーション: オプション（`CanNmBusSynchronizationEnabled` が `TRUE` に設定されている場合にだけ利用でき）、`CanNmPassiveModeEnabled` が定義されていない。（SRS_Nm_02516）

**[SWS_CanNm_00130]** サービス呼び出し `CanNm_RequestBusSynchronization` は、`CanNmPassiveModeEnabled`（コンフィギュレーションパラメータ）が定義されていない場合、単一の Network Management PDU の送信を起動しなければならない。（SRS_Nm_02516）

根拠: このサービスは、通常、NM ゲートウェイの拡張をサポートするために使われる。

**[SWS_CanNm_00187]** `CanNm_RequestBusSynchronization` が、バススリープモードとバススリープ準備モードで呼ばれた場合、CanNm モジュールは、サービスを実行してはならず、`E_NOT_OK` を返さなければならない。

#### 8.3.18 CanNm_CheckRemoteSleepIndication

**[SWS_CanNm_00227]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_CheckRemoteSleepIndication` |
| 構文 | `Std_ReturnType CanNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle, boolean* nmRemoteSleepIndPtr)` |
| サービス ID [hex] | 0xd0 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmRemoteSleepIndPtr`: リモートスリープ通知の検査結果のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: リモートスリープ通知のビットの検査が失敗した |
| 説明 | リモートスリープ通知が起きているかを検査する。 |

**[SWS_CanNm_00153]** サービス呼び出し `CanNm_CheckRemoteSleepIndication` は、リモートスリープ通知の現在の状態（すなわち、すでに検出されたか否か）についての情報を提供しなければならない。（SRS_Nm_00052、SRS_Nm_02509）

**[SWS_CanNm_00281]** `CanNm_CheckRemoteSleepIndication` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00282]** `CanNm_CheckRemoteSleepIndication` のコンフィギュレーション: オプション（`CanNmRemoteSleepIndEnabled` が `TRUE` に設定されている場合にだけ利用できる）。

#### 8.3.19 CanNm_SetSleepReadyBit

**[SWS_CanNm_00338]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_SetSleepReadyBit` |
| 構文 | `Std_ReturnType CanNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit)` |
| サービス ID [hex] | 0x17 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別<br>`nmSleepReadyBit`: CBV の ReadySleep ビットに書き込まれる値 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: リモートスリープ通知ビットの書き込みが失敗した |
| 説明 | Control Bit Vector の NM コーディネータスリープ準備ビットを設定する。 |

**[SWS_CanNm_00339]** `CanNm_SetSleepReadyBit` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00340]** `CanNm_SetSleepReadyBit` のコンフィギュレーション: オプション（`CanNmCoordinatorSyncSupport` が `TRUE` に設定されている場合にだけ利用できる）。

### 8.4 コールバック通知

#### 8.4.1 CanNm_TxConfirmation

**[SWS_CanNm_00228]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_TxConfirmation` |
| 構文 | `void CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)` |
| サービス ID [hex] | 0x40 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では不可 |
| 入力パラメータ | `TxPduId`: 送信された PDU の ID<br>`result`: `E_OK`: PDU が送信された<br>`E_NOT_OK`: PDU の送信が失敗した |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースモジュールが、PDU の送信、または送信の失敗を確認する。 |

**[SWS_CanNm_00283]** `CanNm_TxConfirmation` の注意点:
- 呼び出しコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。このコールバックサービスは、複数の CAN コントローラの使用に対して再入可能である。
- CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00284]** `CanNm_TxConfirmation` のコンフィギュレーション: オプション（`CanNmPassiveModeEnabled` と `CanNmImmediateTxConfEnabled` がどちらも `FALSE` に設定されている場合にだけ利用できる）。（SRS_Nm_02510）

#### 8.4.2 CanNm_RxIndication

**[SWS_CanNm_00231]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_RxIndication` |
| 構文 | `void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x42 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では不可 |
| 入力パラメータ | `RxPduId`: 受信した PDU の ID<br>`PduInfoPtr`: 受信した PDU の長さ（SduLength）、PDU を格納したバッファへのポインタ（SduDataPtr）、およびこの PDU に関するメタデータを含む |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースモジュールからの、PDU の受信の通知。 |

注記: コールバック関数 `CanNm_RxIndication` は CAN インタフェースから呼び出され、CanNm モジュールが実装する。CAN ドライバの受信通知イベントが起きた場合に呼び出される。

**[SWS_CanNm_00285]** `CanNm_RxIndication` の注意点:
- このサービスが戻るまで、CAN インタフェースは canSduPtr にアクセスしない。canSduPtr は、通知が戻るまでの間だけ有効であり、上位層で使用できる。CAN インタフェースは、この canNmRxPduId に対して設定されたバイト数が有効であることを保証する。呼び出しコンテキストは、割り込みレベル（割り込みモード）またはタスクレベル（ポーリングモード）のいずれかである。このコールバックサービスは、複数の CAN コントローラの使用に対して再入可能である。
- CanNm モジュールが正しく初期化されている。

#### 8.4.3 CanNm_ConfirmPnAvailability

**[SWS_CanNm_00344]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_ConfirmPnAvailability` |
| 構文 | `void CanNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x16 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能（同じ NM チャネルでは不可） |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 指定した NM チャネルで PN フィルタ機能を有効にする。利用可否: この API は `CanNmGlobalPnSupport` が `TRUE` の場合にだけ利用できる。 |

**[SWS_CanNm_00345]** `CanNm_ConfirmPnAvailability` の注意点: CanNm モジュールが正しく初期化されている。

**[SWS_CanNm_00346]** `CanNm_ConfirmPnAvailability` のコンフィギュレーション: オプション（`CanNmGlobalPnSupport` が `TRUE` に設定されている場合にだけ利用できる）。

#### 8.4.4 CanNm_TriggerTransmit

**[SWS_CanNm_91001]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_TriggerTransmit` |
| 構文 | `Std_ReturnType CanNm_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x41 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では不可 |
| 入力パラメータ | `TxPduId`: 送信を要求されている SDU の ID |
| 入出力パラメータ | `PduInfoPtr`: SDU データのコピー先バッファへのポインタ（SduDataPtr）と、利用可能なバッファサイズ（SduLength）を含む。戻る際、サービスはコピーした SDU データの長さを SduLength に示す |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: SDU がコピーされ、SduLength がコピーしたバイト数を示す<br>`E_NOT_OK`: SDU データはコピーされていない。PduInfoPtr は NULL ポインタや無効なデータを指している可能性があるため、使用してはならない |
| 説明 | この API の中で、上位層モジュール（呼ばれる側のモジュール）は、利用可能なデータが PduInfoPtr->SduLength で通知されたバッファサイズに収まるかを検査しなければならない。収まる場合は、PduInfoPtr->SduDataPtr で与えられたバッファへデータをコピーし、実際にコピーしたデータの長さを PduInfoPtr->SduLength に更新しなければならない。収まらない場合は、PduInfoPtr を変更せずに `E_NOT_OK` を返す。 |

**[SWS_CanNm_00350]** `CanNmComUserDataSupport` が有効な場合、CanNm は、`PduR_CanNmTriggerTransmit` を呼び出して、参照されている NM I-PDU から NM ユーザデータを集め、`CanNm_TriggerTransmit` の呼び出しの中で、ほかの NM のバイトとユーザデータを結合しなければならない。（SRS_Nm_02503）

**[SWS_CanNm_00351]** 関数 `CanNm_TriggerTransmit` は、TxPduId で要求された該当する NM PDU の NM PDU データをコピーしなければならない。（SRS_Nm_02503）

注記: 関数 `CanNm_TriggerTransmit` は、CanIf から割り込みコンテキストで呼び出される可能性がある。

### 8.5 スケジュール関数

#### 8.5.1 CanNm_MainFunction

**[SWS_CanNm_00234]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanNm_MainFunction` |
| 構文 | `void CanNm_MainFunction(void)` |
| サービス ID [hex] | 0x13 |
| 説明 | この文書に記述されたアルゴリズムを処理する、CanNm のメイン関数。 |

要件 SWS_BSW_00037 が規定するとおり、`CanNm_MainFunction` は、モジュールが初期化されていない場合、何も処理せずに戻る。

### 8.6 期待されるインタフェース

この章では、他のモジュールから必要とされるインタフェースをすべて挙げる。

#### 8.6.1 必須インタフェース

この章では、このモジュールの中核機能を実現するために必要なインタフェースをすべて定義する。

**[SWS_CanNm_00324]**

| API 関数 | 説明 |
|---|---|
| `Det_ReportRuntimeError` | ランタイムエラーを報告するサービス。コールアウトが設定されている場合は、そのコールアウトを呼び出す。 |
| `Nm_BusSleepMode` | ネットワークマネジメントがバススリープモードに入ったことの通知。 |
| `Nm_NetworkMode` | ネットワークマネジメントがネットワークモードに入ったことの通知。 |
| `Nm_NetworkStartIndication` | バススリープモードで NM メッセージを受信したことの通知。ネットワーク内の一部のノードがすでにネットワークモードに入っていることを示す。 |
| `Nm_PrepareBusSleepMode` | ネットワークマネジメントがバススリープ準備モードに入ったことの通知。 |

> 再構成の注記: PDF の抽出で、API 名と説明の対応が崩れていた。説明文の内容と順序から上記の対応に組み直した。

#### 8.6.2 オプションインタフェース

この章では、このモジュールのオプション機能を実現するために必要なインタフェースを定義する。

**[SWS_CanNm_00325]**

| API 関数 | 説明 |
|---|---|
| `CanIf_Transmit` | PDU の送信を要求する。 |
| `CanSM_TxTimeoutException` | CanNm が、影響を受ける部分 CAN ネットワークで送信タイムアウト例外を検出したことを CanSM モジュールへ通知する。CanSM モジュールの該当するネットワーク状態機械の中で回復される。 |
| `Det_ReportError` | 開発エラーを報告するサービス。 |
| `Nm_CarWakeUpIndication` | <Bus>Nm が CWU 要求の受信を通知するために呼び出す。 |
| `Nm_CoordReadyToSleepCancellation` | Control Bit Vector の NM コーディネータスリープ準備ビットが 0 に戻されたときに、通知を取り消す。 |
| `Nm_CoordReadyToSleepIndication` | Control Bit Vector の NM コーディネータスリープ準備ビットが設定されたときに、通知を設定する。 |
| `Nm_PduRxIndication` | NM メッセージを受信したことの通知。 |
| `Nm_RemoteSleepCancellation` | ネットワークマネジメントが、ネットワーク上のほかのノードがすべてはバススリープモードに入る準備ができているわけではなくなったことを検出したことの通知。 |
| `Nm_RemoteSleepIndication` | ネットワークマネジメントが、ネットワーク上のほかのノードがすべてバススリープモードに入る準備ができたことを検出したことの通知。 |
| `Nm_RepeatMessageIndication` | Repeat Message Request ビットが設定された NM メッセージを受信したことを通知するサービス。 |
| `Nm_StateChangeNotification` | 下位層の <BusNm> の状態が変わったことの通知。 |
| `Nm_TxTimeoutException` | NM メッセージの送信の試行が失敗したことを通知するサービス。 |
| `PduR_CanNmRxIndication` | 下位層の通信インタフェースモジュールからの、PDU の受信の通知。 |
| `PduR_CanNmTriggerTransmit` | この API の中で、上位層モジュール（呼ばれる側のモジュール）は、利用可能なデータが PduInfoPtr->SduLength で通知されたバッファサイズに収まるかを検査する。収まる場合は、PduInfoPtr->SduDataPtr のバッファへデータをコピーし、実際にコピーしたデータの長さを PduInfoPtr->SduLength に更新する。収まらない場合は、PduInfoPtr を変更せずに `E_NOT_OK` を返す。 |
| `PduR_CanNmTxConfirmation` | 下位層の通信インタフェースモジュールが、PDU の送信、または送信の失敗を確認する。 |

#### 8.6.3 設定可能なインタフェース

CanNm は、設定可能なインタフェースを提供しない。

#### 8.6.4 ジョブ終了通知

CanNm は、ジョブ終了通知を提供しない。

### 8.7 サービスインタフェース

CanNm は、サービスインタフェースを提供しない。

---

## 学習メモ（訳者による解釈。仕様書の記述ではない）

- CanNm の状態は、Bus-Sleep、Prepare Bus-Sleep、Network（Repeat Message／Normal Operation／Ready Sleep）の 3 系統に整理できる。各状態の遷移は、タイマー（NM タイムアウト、リピートメッセージ、WaitBusSleep）と、NM PDU の受信・送信、上位からの NetworkRequest／NetworkRelease で駆動される。
- NM PDU の先頭側のバイトに Control Bit Vector と Source Node Identifier を置く位置は、`CanNmPduCbvPosition` / `CanNmPduNidPosition` で設定する。
- `CanNm_TxConfirmation` は `CanNmImmediateTxConfEnabled` が `TRUE` のとき不要になる。その場合は CanNm が送信要求の直後に自分で確認済みとして扱う。
- 本プロジェクトの実機ログで見る `CANNM_E_NET_START_IND`（ランタイムエラー 0x04）は、バススリープ中に NM メッセージを受信して起床したときの仕様どおりの通知であり、異常ではない（[SWS_CanNm_00336]）。

# Network Management Interface（Nm）仕様書 日本語訳

- 原典: AUTOSAR CP Release 4.3.1 `AUTOSAR_SWS_NetworkManagementInterface.pdf`（Document ID 228）
- 範囲: 第7章（機能仕様）と第8章（API 仕様）。第1〜6章（導入、関連文書、制約、依存関係、トレーサビリティ）、第9章（シーケンス図）、第10章以降（コンフィギュレーション）は対象外。
- 凡例: `[SWS_Nm_xxxxx]` は原典の条項 ID。`（SRS_…）` は原典が条項に付けている上位要求の ID。
- 用語: NM は Network Management、`<BusNm>` は CanNm・FrNm・LinNm・UdpNm・J1939Nm などバス固有の NM モジュール、CBV は Control Bit Vector、PN はパーシャルネットワーキング、コーディネータは NM Coordinator。
- 図は訳せないため、位置に注記だけを残している。図の内容は原典を参照すること。
- 「学習メモ」は原文の訳ではなく、このプロジェクトの実装と照らすための訳者の解釈。

---

## 7 機能仕様

NM インタフェースの機能は 2 つの部分からなる。
- ECU 上でバス固有の NM モジュールとともに AUTOSAR NM を動かすために必要な、基本機能。
- ゲートウェイ ECU が 1 つ以上のバスを同期してシャットダウンするために使う、NM コーディネータ機能。

### 7.1 基本機能

汎用ネットワークマネジメントインタフェースモジュール（Nm）は、バス固有のネットワークマネジメントモジュール（CanNm、J1939Nm、FrNm、LinNm、UdpNm など）と、通信マネージャモジュール（ComM）との間の、バスに依存しないアダプテーション層として動作する。

注記: Nm は、この文書で規定したもの以外のインタフェース関数を提供しない。Nm は ComM に対して、下位のバスの種類についての具体的な知識を含まず、かつ必要なネットワークマネジメント機能を実現するのに十分なインタフェースを提供する。Nm が扱うアルゴリズムはバスに依存しない。

注記: ほかのサービス層モジュールも、ネットワークマネジメント機能へ Nm 経由でのみアクセスすることが求められ、バス固有の NM 機能への迂回路があってはならない。

**[SWS_Nm_00006]** Nm は、ComM からの汎用の関数呼び出しを、バス固有の NM 層の関数へ変換しなければならない。（SRS_Nm_00154、SRS_Nm_02513）

**[SWS_Nm_00012]** Nm は、バス固有の NM 層から呼び出されるコールバック関数を、ComM への汎用のコールバックへ変換しなければならない。（SRS_Nm_00154、SRS_Nm_02513）

**[SWS_Nm_00091]** Nm の基本機能は、全体または一部をマクロを使って実装してもよい。（SRS_BSW_00330）

### 7.2 NM コーディネータ機能

NM コーディネータ機能は、Nm の機能のひとつで、調整アルゴリズムを使って、ECU が接続されているすべてのバス、または 1 つ以上の独立したバスの部分集合の上で、NM のシャットダウンを調整する。

コンフィギュレーションにより、調整アルゴリズムは、シャットダウンの同期のレベルを変えるように設定できる。

NM コーディネータ機能を能動的に実行する NM を使う ECU は、一般に NM コーディネータと呼ばれる。ただし、この仕様書では、要求の中でこの語を使う場合、NM コーディネータ機能と同義とする。

注記: バスの種類によって、Network、Channel、Cluster の用語の使い方が異なる点に注意すること。

**[SWS_Nm_00292]** NM コーディネータ機能が設定されている場合、コンフィギュレーションパラメータ `NmCycletimeMainFunction` には、Nm のメイン関数（[SWS_Nm_00118] を参照）の連続する 2 回の呼び出しの周期を設定しなければならない。（SRS_BSW_00478）

注記: NM コーディネータは、これを使って内部タイマのタイムアウト状態を計算してよい。

#### 7.2.1 NM コーディネータ機能の適用範囲

**[SWS_Nm_00001]** 調整アルゴリズムは、複数の調整対象バスが 1 つの NM コーディネータに接続されているトポロジを扱えなければならない。（SRS_Nm_02514）

**[SWS_Nm_00256]** NM コーディネータは、同じ NM クラスタに接続された 2 つ以上の NM コーディネータをサポートしなければならない。（SRS_Nm_02535）

**[SWS_Nm_00051]** NM コーディネータは、公式の AUTOSAR バス固有 NM が動くバスに加え、7.4.2 節で規定する必要な機能・コールバック・インタフェースを実装した、そのほかの汎用バス NM のバスも調整できなければならない。（SRS_Nm_00044、SRS_Nm_02515）

注記: J1939Nm はシャットダウン処理をサポートしないため、J1939Nm に対するコーディネータのサポートは不要である。

**[SWS_Nm_00055]** NM インタフェースのコンフィギュレーションは、NM コーディネータ機能のサポートがあるかないかを定義するパラメータ `NmCoordinatorSupportEnabled` を提供しなければならない。（SRS_Nm_00150）

**[SWS_Nm_00167]** 独立に調整される複数の NM 調整クラスタを設定できなければならない。（SRS_Nm_00045）

**[SWS_Nm_00168]** 各バスは、0 個または 1 個の NM 調整クラスタに属さなければならない。（SRS_Nm_00045、SRS_Nm_02511、SRS_Nm_02514）

根拠: コンフィギュレーションパラメータ `NmCoordClusterIndex` は、バスがどの調整クラスタに属するかを指定するために使う。このパラメータがチャネルに対して未定義の場合、対応するバスはどの NM 調整クラスタにも属さない。

**[SWS_Nm_00169]** シャットダウンの調整は、調整クラスタのうち、現在起きているネットワークでのみ行わなければならない。すでに「バススリープモード」にあるネットワークは、引き続き監視されるが、調整はされない。（SRS_Nm_02516）

根拠: NM コーディネータは、調整クラスタのすべてのバスが起きていることを必要とせず、調整クラスタの部分集合、すなわちパーシャルネットワークで動作して、調整されたシャットダウンを行う。常にシャットダウンの開始条件を監視し、条件が満たされた時点で、調整クラスタの中で現在起きているすべてのバスの調整されたシャットダウンを行う。

注記: 調整対象のバスで同期したウェイクアップを提供することは、Nm の範囲外である。あるチャネルでウェイクアップが起きた場合に、必要なチャネルまたはすべてのチャネルを起こすことは、アプリケーション（車両モードマネジメント）の役割である。

#### 7.2.2 調整対象バスを起こし続ける

**[SWS_Nm_00002]** NM コーディネータを実装するノードが、調整クラスタの少なくとも 1 つのバスでスリープの準備ができていない（すなわち、ネットワークを能動的に要求している）間は、NM コーディネータは、その調整クラスタの現在アクティブなすべてのバスでネットワークが要求されているようにしなければならない。（SRS_Nm_02514）

**[SWS_Nm_00003]** 調整クラスタの少なくとも 1 つのバスがスリープの準備ができていない（すなわち、NM コーディネータ以外のノードがそのバスを要求している）間は、ローカル ECU 自身がその調整クラスタのすべてのバスでスリープの準備ができていても、NM コーディネータは、その調整クラスタの現在アクティブなすべてのバスでネットワークが要求されているようにしなければならない。（SRS_Nm_02514）

根拠: バス固有の NM は、バスがスリープの準備ができているかどうかを、コールバック `Nm_RemoteSleepIndication` と `Nm_RemoteSleepCancellation` を呼び出して Nm へ知らせる。ローカル ECU は、ネットワーク上でスリープの準備ができているかどうかを、API 関数 `Nm_NetworkRelease` と `Nm_NetworkRequest` を使って知らせる。

根拠: Nm は、バス固有の NM の関数 `<BusNm>_NetworkRequest` を呼び出して、バス上のネットワークを要求する。

AUTOSAR のバス固有 NM はすべて、ネットワークを要求している間は 1 つの AUTOSAR ノードがバスを起こし続けられる、という原則で作られている。そのため、NM コーディネータは、バス固有の NM に対してネットワークを要求することで、調整クラスタのすべてのバスを起こし続ける。

上の 2 つの要求 [SWS_Nm_00002] と [SWS_Nm_00003] は、次のようにまとめられる。（NM コーディネータを実装するノードを含め）少なくとも 1 つのノードが調整クラスタのいずれかのバスを起こし続けている間は、NM コーディネータは、その調整クラスタのすべてのバスを起こし続ける。

**[SWS_Nm_00228]** 調整クラスタのバスで `NmChannelSleepMaster` が `TRUE` に設定されている場合、NM コーディネータは、そのバスを常にスリープの準備ができているものとみなさなければならず、そのネットワークのシャットダウンを始める前に、そのバスからの `Nm_RemoteSleepIndication` の呼び出しを待ってはならない。（SRS_Nm_02511）

根拠: このプロパティは、バスのスリープをローカルノードだけで絶対的に決められ、そのバスのほかのノードがその決定に反対できない、バス固有 NM のすべてに設定すべきである。そのようなネットワークの例は LIN で、ローカルの AUTOSAR ECU が常に LIN のバスマスタであり、ネットワークをいつスリープさせるかを単独で決められる。

#### 7.2.3 調整対象バスのシャットダウン

達成できる同期のレベルは、コンフィギュレーションに依存する。7.2.5 節を参照。図 7.1 は、調整アルゴリズムの概要を示す。7.2.1 節で述べたとおり、調整アルゴリズムと調整されたシャットダウンは、NM 調整クラスタごとに独立に適用する。

> 図 7.1「調整されたシャットダウンを含む、調整アルゴリズムの概要」（訳注: 図は省略。PDF の抽出が崩れて読み取れないため、図の構成を文字で補う。）
> 流れ: 開始 →［すべてのネットワークと自ノードがスリープの準備ができているか？］No の間は待つ → Yes なら［有効な同期通知を待つ（オプションのステップ）］→ 調整されたシャットダウン:［すべてのシャットダウン遅延タイマを開始］→［次のタイマの満了を待つ］→［すべてのタイマが満了したか？］No ならタイマの満了待ちへ戻る → 満了したタイマのネットワークについて `<BusNm>_RequestBusSynchronization`（図中表記は BusNm_BusSynchronization）に続けて `<BusNm>_NetworkRelease` を呼ぶ → ［すべてのネットワークがスリープするのを待ち、`Nm_BusSleepMode` を受ける］→ 終了。

注記: 調整アルゴリズムが行う動作を実行する場所に、制限はない。Nm のメイン関数（`NmMainFunction`）で行っても、モジュールの通知・コールバックで行ってもよい。

**[SWS_Nm_00171]** 調整クラスタのすべてのネットワークが、スリープの準備ができている、またはすでに「バススリープモード」にある場合、NM コーディネータは、起きているすべてのネットワークで調整されたシャットダウンを開始しなければならない。NM コーディネータは、調整されたシャットダウンを開始できるかどうかを継続的に評価しなければならない。（SRS_Nm_00047、SRS_Nm_02516）

根拠: シャットダウン条件の評価は、メイン関数以外の API 呼び出しの中でも行える。その場合、評価を分割して、その API 呼び出しの影響を受ける特定の条件だけを検査できる。このため、メインの処理周期ごと、API 呼び出しごとに、すべての条件を再評価する必要はない。

**[SWS_Nm_00172]** 調整クラスタのバスのいずれかでコンフィギュレーションパラメータ `NmSynchronizingNetwork` が `TRUE` の場合、調整されたシャットダウンは、この調整クラスタの同期ネットワークとして設定されたネットワークが `Nm_SynchronizationPoint` を呼び出すまで、遅延されなければならない。（SRS_Nm_00044）

根拠: NM 調整クラスタのネットワークのうち 1 つ以上が（FlexRay のように）周期的な場合、アルゴリズムを含まれる周期的ネットワークのひとつと同期させると、より高いレベルの同期したシャットダウンが達成できる。そう設定された場合、調整対象のすべてのネットワークのシャットダウンタイマは、同期ネットワークが `Nm_SynchronizationPoint` を呼び出すまで開始されない。

根拠: NM 調整クラスタごとに同期点を示すように設定すべきネットワークは 1 つだけであるが、これにより NM コーディネータ機能は、各調整クラスタの同期ネットワークとして設定されたネットワークから発生したもの以外の、すべての同期通知を除外できる。

**[SWS_Nm_00173]** 調整されたシャットダウンを開始するすべての条件が満たされていない場合、または調整されたシャットダウンがすでに開始されている（ただし中止されていない）場合、`Nm_SynchronizationPoint` の呼び出しは無視しなければならない。（SRS_Nm_02514）

根拠: 場合によっては、同期しないネットワークがスリープするのに、より長い時間がかかる。その場合、調整されたシャットダウンは 1 回の同期通知を基に開始されるが、同期ネットワークはすぐには解放されないので、（複数回の）同期通知を呼び出し続ける。それらは、安全に無視できる。

**[SWS_Nm_00174]** 調整クラスタの現在起きているすべてのバスで `NmSynchronizingNetwork` が `FALSE` の場合、タイマは、`Nm_SynchronizationPoint()` の呼び出しを待たずに、すべてのシャットダウン条件が満たされた後に開始しなければならない。（[SWS_Nm_00172] も参照）。（SRS_Nm_02516）

**[SWS_Nm_00175]** 調整されたシャットダウンが開始されたとき、調整クラスタの現在起きている各チャネルに対して、シャットダウン遅延タイマを起動しなければならない。各タイマには、`NmGlobalCoordinatorTime` から、そのチャネル固有のシャットダウン時間 TSHUTDOWN_CHANNEL を引いて計算した、そのチャネルのシャットダウン遅延タイマ値を設定しなければならない。（SRS_Nm_00149、SRS_Nm_02516）

**[SWS_Nm_00284]** `NmGlobalCoordinatorTime` がゼロの場合、すべてのチャネルのシャットダウン遅延タイマも、ゼロでなければならない。（SRS_Nm_00149、SRS_Nm_02516）

注記: TSHUTDOWN_CHANNEL は、7.2.5 節で述べた方法、または次の式で計算できる。
- CanNm: Ready Sleep Time + Prepare BusSleep Time
- FrNm: Ready Sleep Time。例: (FrNmReadySleepCnt+1) * FrNmRepetitionCycle * "FlexRay の 1 サイクルの長さ"
- GenericNm: `NmGenericBusNmShutdownTime`

**[SWS_Nm_00176]** あるネットワークのシャットダウンタイマが満了したとき、Nm は、`<BusNm>_RequestBusSynchronization` に続けて `<BusNm>_NetworkRelease` を呼び出して、そのネットワークを解放しなければならない。（SRS_Nm_02516）

**[SWS_Nm_00177]** Nm は、解放されたが、まだ「バススリープモード」を報告していないすべてのネットワークを追跡しなければならない。シャットダウンが中止された場合、これらのネットワークは、引き続きアクティブなネットワークとみなさなければならない。（7.3.3 節を参照）。（SRS_Nm_02516）

定義: すべてのネットワークが解放され、すべてのネットワークが「バススリープモード」にあるとき、調整されたシャットダウンは完了する。

#### 7.2.4 入れ子になったサブバスの調整

入れ子になったサブバスの調整をサポートするため、NM コーディネータは、調整の階層を構築するように設定される必要がある。最上位の NM コーディネータは、調整クラスタごとに、能動的に調整されるチャネル（`NmActiveCoordinator == TRUE`）だけを持つ。この NM コーディネータは、ほかのすべてのコーディネータに対して調整されたシャットダウンを開始しなければならない。入れ子になった NM コーディネータは、受動的に設定されたチャネル（`NmActiveCoordinator == FALSE`）からシャットダウン通知の情報を受け取り、その情報を、能動的に調整されるチャネル（`NmActiveCoordinator == TRUE`）を通じて、後続の NM コーディネータへ提供する。図 7.2 が、これを例で説明する。

> 図 7.2「入れ子のゲートウェイのユースケース」（訳注: 図は省略。）

図 7.2 の例のトポロジは、次の調整方針を持つ。GW 1 は、Network 1 と Network 2 へのチャネルを、能動的に調整するチャネルとして設定する。GW 2 は、Network 2 への接続を受動的に調整されるチャネルとして設定し、Network 3 には能動的に調整するチャネルを設定する。GW 3 は、Network 3 では受動的に調整されるチャネルとして設定し、Network 4 への接続には能動的に調整するチャネルを設定する必要がある。

**[SWS_Nm_00280]** 入れ子になったサブバスを調整する機能は、パラメータ `NmCoordinatorSyncSupport` が `TRUE` に設定されている場合に利用できなければならない。（SRS_Nm_02535）

注記: この章のすべての要求は、「NM 調整クラスタごと」に有効である（[SWS_Nm_00167] を参照）。

パラメータ `NmActiveCoordinator` は、NM コーディネータがそのチャネルで能動的に振る舞うか（能動的に調整されるチャネル）[`NmActiveCoordinator = TRUE`]、受動的に振る舞うか（受動的に調整されるチャネル）[`NmActiveCoordinator = FALSE`] を示す。

**[SWS_Nm_00257]** 受動的に調整されるチャネルでは、NM コーディネータは、ノードにネットワークマネジメントの要求が保留されている場合、またはその NM コーディネータが能動的に調整している接続先のネットワークがスリープの準備ができていない場合にだけ、Nm メッセージを送信しなければならない。（SRS_Nm_02535）

根拠: これは、同じチャネルの 2 つの NM コーディネータが、スリープの準備ができているときに NM メッセージを送信して、バスを起こし続けてしまうことを防ぐ。この仕組みがなければ、ほかに少なくとも 1 つのアクティブなノードがあるかどうかを検出できなくなる。

**[SWS_Nm_00259]** NM コーディネータは、次の条件がすべて成り立つ場合、能動的に調整されるチャネルで、`<BusNm>_SetSleepReadyBit` を介して NM メッセージの NMcoordinatorSleepReady ビットを 1 に設定しなければならない。
- NM 調整クラスタのすべてのノードがスリープの準備ができている（RemoteSleepIndication）。
- `NmSynchronizingNetwork` が有効な場合、対応するチャネルで `Nm_SynchronizationPoint()` の呼び出しを受け取っている。
- この NM 調整クラスタのすべてのチャネルが `NmActiveCoordinator == TRUE` として設定されている。

（SRS_Nm_02535）

注記: CBV 内のコーディネータビットの位置は、該当する `<BusNm>` の仕様を参照すること。

注記: これは最上位のコーディネータ（受動的に調整されるチャネルを持たないもの）に適用される。

根拠: 受動的に調整されるチャネルを持つノードは、すでに能動的なコーディネータの sleep ready ビットで同期されているため、同期点を必要としない。

注記: 受動的に調整されるチャネルを持つノードは、[SWS_Nm_00261] の要求に従ってビットを設定する。

**[SWS_Nm_00261]** 受動的に調整されるチャネルで `Nm_CoordReadyToSleepIndication` を受け取った場合、NmCoordinator は、能動的に調整されるすべてのチャネルで、`<BusNm>_SetSleepReadyBit` の API 呼び出しにより、NMCoordinatorSleepReady ビットを SET（1）に設定しなければならない。（SRS_Nm_02535）

**[SWS_Nm_00271]** 受動的に調整されるチャネルで `Nm_CoordReadyToSleepCancellation` を受け取った場合、NmCoordinator は、能動的に調整されるすべてのチャネルで、`<BusNm>_SetSleepReadyBit` の API 呼び出しにより、NMCoordinatorSleepReady ビットを UNSET（0）に設定しなければならない。（SRS_Nm_02535）

注記: 受動的に調整されるチャネルでは、NM コーディネータが（`<BusNm>` の関数呼び出しにより）Sleep Ready ビットを設定することは決してなく、受け取った Sleep Ready ビットの状態の変化を、能動的に調整されるチャネルへ転送する。

注記: 能動的に調整されるチャネルでは、NM コーディネータで `Nm_CoordReadyToSleepIndication` と `Nm_CoordReadyToSleepCancellation` が呼び出されることは想定されない。

**[SWS_Nm_00262]** NM コーディネータは、Sleep Ready ビットを SET の状態で要求した後に、調整されたシャットダウンを開始しなければならない。（SRS_Nm_02535）

**[SWS_Nm_00281]** `NmGlobalCoordinatorTime` は、少なくとも、調整されるすべてのネットワークをシャットダウンするために必要な最大の時間に設定しなければならない。（SRS_Nm_00149）

注記: これには、入れ子になったすべての接続が含まれる（例は図 7.3 を参照）。

> 図 7.3「`Nm_GlobalCoordinatorTime` を使ったシャットダウン」（訳注: 図は省略。）

**[SWS_Nm_00267]** 調整されたシャットダウンが何らかの理由で中止された場合、NM コーディネータは、能動的に調整されるすべてのチャネルで、`<BusNm>_SetSleepReadyBit` の API 呼び出しにより、NMCoordinatorSleepReady ビットを UNSET（0）に設定しなければならない。（SRS_Nm_02535、SRS_Nm_02537）

注記: 中止されたシャットダウンの詳細は、7.3.3 節にある。

#### 7.2.5 シャットダウンタイマの計算

調整アルゴリズムは、達成できる同期のレベルがスイッチとタイマのコンフィギュレーションに依存するため、かなり柔軟である。どのイベントまたは時点に同期させるのが目標かによって、設定のしかたは変わる。この章には、3 つの異なる同期のレベルを達成する方法の指針を含める。この指針に従うか、独自の設定を選んで別の同期の順序を達成するかは、コンフィギュレーションを行う者に任される。したがって、この節には要求を含めず、推奨だけを含める。

絶対的な同期は決して達成できないことに注意すること。同期の正確さを決めるジッタの要因には、Nm の処理周期、タイマの正確さ、非決定的なバスのバス負荷がある。

正しく設定すれば、以下のユースケースは、これらの状況で達成できる最良の同期を与える。

NM コーディネータの以前の版では、調整アルゴリズムが調整されたシャットダウンの開始を「何ラウンドか」遅らせる機能を含んでいた。この遅延は削除されたが、すべてのシャットダウンタイマ（コンフィギュレーションパラメータ `NmGlobalCoordinatorTime`）を長くすることで、同様の挙動を得られる。FlexRay のような周期的なネットワークを使う場合に、この延長した遅延時間を、それらのネットワークの同期通知の周期に合わせて量子化したいときは、特に注意が必要である。

#### 7.2.6 同期のユースケース 1 — 同期コマンド

このユースケースは、異なるネットワークが解放される時点を同期させることに注目する。これにより、すべてのネットワークの合計のシャットダウンは最も速くなるが、ネットワークが同時に「バススリープモード」に入らないという欠点がある。

根拠: このユースケースの例は、いずれかの CAN ノードがネットワークのひとつを要求している間は複数の CAN ネットワークを起こし続け、すべてのノードがスリープの準備ができたときには、異なるネットワークが同時に「バススリープモード」に入るかどうかは問題にならない場合である。

このユースケースはネットワークの周期的な振る舞いを考慮しないため、同期パラメータ `NmSynchronizingNetwork` はすべてのネットワークで `FALSE` に設定し、`Nm_SynchronizationPoint` コールバックを呼び出すように設定するバス固有 NM があってはならない。最も速いシャットダウンを達成するには、シャットダウンタイマのパラメータ `NmGlobalCoordinatorTime` を 0.0 に設定する必要がある。

#### 7.2.7 同期のユースケース 2 — 同期した開始

このユースケースはユースケース 1 の拡張で、一部のネットワークでは、ネットワークを解放する要求が特定の時点でしか処理されないことを考慮する。このユースケースは、ユースケース 1 と同様に同時のシャットダウンを命令するが、同期ネットワークに適した時点まで待つ。

根拠: このユースケースの例は、1 つの FlexRay ネットワークと複数の CAN ネットワークがあり、すべてのネットワークがアクティブな時間を最大にしつつ、ネットワークをできるだけ速くスリープさせる場合である。

このユースケースは、選んだネットワークの周期的な振る舞いを考慮するため、ネットワークのうち 1 つで同期パラメータ `NmSynchronizingNetwork` を `TRUE` に設定し、ほかのネットワークではこのパラメータを `FALSE` に設定する。同期ネットワークのバス固有 NM も、シャットダウンを開始すべき適切な時点で `Nm_SynchronizationPoint` コールバックを呼び出すように設定する。最も速いシャットダウンを達成するには、シャットダウンタイマのパラメータ `NmGlobalCoordinatorTime` を 0.0 に設定する必要がある。

#### 7.2.8 同期のユースケース 3 — 同期したネットワークスリープ

このユースケースは、異なるネットワークが「バススリープモード」に入る時点を同期させることに注目する。同期ネットワークからの通知を待ち、その後、すべてのネットワークの解放をタイミング値に基づいて遅らせることで、「ネットワークモード」（または「バススリープ準備モード」）から「バススリープモード」への遷移が、可能な限り同期するようにする。

根拠: このユースケースの例は、1 つの FlexRay ネットワークと複数の CAN ネットワークが、同時に通信を止める場合である。

このユースケースは、選んだネットワークの周期的な振る舞いを考慮するため、ネットワークのうち、できれば周期的なものを、同期パラメータ `NmSynchronizingNetwork` が `TRUE` のネットワークとし、ほかのネットワークではこのパラメータを `FALSE` に設定する。同期ネットワークのバス固有 NM も、シャットダウンを開始すべき適切な時点で `Nm_SynchronizationPoint` コールバックを呼び出すように設定する。

各ネットワークのシャットダウンタイマ TSHUTDOWN_CHANNEL を計算するには、各ネットワークのタイミング上の振る舞いに関する具体的な知識を得る必要がある。

すべてのネットワークで TSHUTDOWN_CHANNEL を計算する必要がある。これは、ネットワークが「バススリープモード」に入るまでにかかる最小の時間である。周期的でないネットワーク（CAN など）では、ネットワークが解放された時点から「バススリープモード」に入るまでの時間を測る。周期的なネットワーク（FlexRay など）では、ネットワークが解放される直前に行われた同期通知からの全範囲も含める。汎用のバス NM では、時間はコンフィギュレーションパラメータ `NmGenericBusNmShutdownTime` で与えられる。

同期ネットワークでは、TSYNCHRONIZATION_INDICATION を決める必要がある。これは、そのバス固有 NM が `Nm_SynchronizationPoint` を呼び出す、連続する任意の 2 回の呼び出しの間の時間である。

`NmGlobalCoordinatorTime` は、調整アルゴリズムに必要な合計の時間とする。これには、入れ子になったサブバスのシャットダウン時間も含まれる。まず、`NmGlobalCoordinatorTime` を、同期ネットワークの TSHUTDOWN_CHANNEL と同じ値に設定する。ほかのネットワークの TSHUTDOWN_CHANNEL が `NmGlobalCoordinatorTime` より大きい場合は、`NmGlobalCoordinatorTime` が、どの TSHUTDOWN_CHANNEL とも等しいかそれより大きくなるまで、`NmGlobalCoordinatorTime` を TSYNCHRONIZATION_INDICATION だけ繰り返し延長する。

各ネットワークのシャットダウン遅延タイマは、`NmGlobalCoordinatorTime` からそのネットワークの TSHUTDOWN_CHANNEL を引いた値として計算する。

周期的なネットワークでは、ネットワークの解放が 2 回の同期通知の間、`Nm_SynchronizationIndication` が（呼び出されたとしたら）呼び出された少し後に起きるようにするため、このパラメータをさらに少し増やす必要がある。タイマを延長する量は、バス固有 NM の実装とコンフィギュレーションに依存するが、TSYNCHRONIZATION_INDICATION よりずっと小さくすべきである。

##### 7.2.8.1 例

最初のケース（図 7.4）では、同期ネットワークが最大の TSHUTDOWN_CHANNEL を持ち、これが `NmGlobalCoordinatorTime` に等しくなる。同期ネットワークでは、シャットダウン遅延タイマは `NmGlobalCoordinatorTime - TSHUTDOWN_CHANNEL` でゼロになるが、Nm が 2 つの同期点の間でネットワークの解放を待つようにするため、少しの時間を足す。周期的でないネットワークでは、シャットダウン遅延タイマは単に `NmGlobalCoordinatorTime - TSHUTDOWN_CHANNEL` である。

> 図 7.4「タイミングの例 1」（訳注: 図は省略。）

2 番目のケース（図 7.5）では、周期的でないネットワークのシャットダウンに非常に長い時間がかかり、そのため最大の TSHUTDOWN_CHANNEL を持つ。`NmGlobalCoordinatorTime` は、同期ネットワークの（わずかに短い）TSHUTDOWN_CHANNEL に、TSYNCHRONIZATION_INDICATION を 1 回加えて得ている。

同期ネットワークでは、シャットダウンタイマは `NmGlobalCoordinatorTime - TSHUTDOWN_CHANNEL` に、Nm が 2 つの同期点の間でネットワークの解放を待つようにするための少しの時間を加えたものになる。周期的でないネットワークでは、シャットダウンタイマは単に `NmGlobalCoordinatorTime - TSHUTDOWN_CHANNEL` である。

> 図 7.5「タイミングの例 2」（訳注: 図は省略。）

### 7.3 調整されたシャットダウンのウェイクアップと中止

Nm は、ノードまたはネットワークの通常のウェイクアップには責任を持たない。これは通信マネージャ（ComM）が行う。

#### 7.3.1 外部からのネットワークのウェイクアップ

基本機能と NM コーディネータ機能のどちらでも、Nm は、ネットワークからのウェイクアップ通知（バス固有 NM がコールバック `Nm_NetworkStartIndication` を呼び出すことで通知される）を、`ComM_Nm_NetworkStartIndication()` を呼び出して ComM へ転送する。その後、ComM が `Nm_PassiveStartUp` を呼び出し、これを Nm が、バス固有 NM の対応するインタフェースへ転送する。

バススリープ中のチャネルのウェイクアップイベント（トランシーバとコントローラの状態に関係する）の処理は、EcuM と ComM が扱う。ここでは Nm のやり取りは発生しない。Nm は、ウェイクアップの検証と、それぞれの通信の必要性に応じて、上で述べたとおり ComM からネットワークの要求を受け取る。

**[SWS_Nm_00245]** ComM が、調整されたネットワーク群のクラスタに属するネットワークに対して `Nm_PassiveStartUp()` を呼び出した場合、Nm コーディネータ機能は、この呼び出しを、ComM が `Nm_NetworkRequest()` を呼び出したかのように扱わなければならない。Nm は、`<BusNm>_NetworkRequest` の呼び出しを下位層へ転送しなければならない。したがって、そのネットワークは、NM コーディネータにより要求されたものとして数えなければならない。（SRS_Nm_02536）

注記: 言い換えると、調整されたネットワーク群のクラスタに属するネットワークに対する `Nm_PassiveStartUp` の呼び出しは、`Nm_NetworkRequest` の呼び出しへ「変換」される、または同様に扱われる。

#### 7.3.2 調整されたウェイクアップ

コンフィギュレーションによっては、ComM は、1 つのネットワークからの通知を基に、複数のネットワークを起動できる。1 つのネットワークがネットワーク開始を通知した場合に、NM 調整クラスタのすべてのネットワークを自動的に起動するように ComM を設定することが推奨されるが、常に必要というわけではない。ネットワークのウェイクアップは Nm の範囲外なので、これは NM 調整機能を使うかどうかとは無関係である。

#### 7.3.3 調整されたシャットダウンの中止

NM 調整機能が有効で、NM 調整クラスタで調整されたシャットダウンが開始されている場合、調整アルゴリズムのコンフィギュレーションによっては、含まれる各バスが実際に解放されるまでに時間がかかることがある。調整されるバスのいずれかのノードが状態を変え、すべてのネットワークが解放される前にネットワークの要求を始めた場合、調整アルゴリズムで競合状態が起こりうる。これは、次の 4 通りで起こりうる。

1. まだ解放されておらず、「ネットワークモード」にあるネットワークのノードが、ふたたびネットワークを要求し始める。これはバス固有 NM により検出され、バス固有 NM は `Nm_RemoteSleepCancellation` を呼び出して Nm へ通知する。
2. すでに解放され、「バススリープ準備モード」を通知したが、まだ「バススリープモード」を通知していないネットワークのノードが、ふたたびネットワークを要求し始める。これはバス固有 NM により検出され、バス固有 NM は自動的に状態を「ネットワークモード」へ変え、`Nm_NetworkMode` を呼び出して Nm へ通知する。
3. ComM が、NM 調整クラスタのいずれかのネットワークを要求する。
4. このネットワークを能動的に調整しているコーディネータが、Ready-Sleep ビットをクリアした Nm メッセージを送る。これはバス固有 NM により検出され（受動的に調整されるチャネルでのみ）、`Nm_CoordReadyToSleepCancellation` を呼び出して NM へ転送される。

一般的な方法は、シャットダウンを中止し、ふたたびネットワークを要求し始めることである。ただし、すでに「バススリープモード」に入ったネットワークは、自動的に起こしてはならない。これは ComM が明示的に要求しなければならない。

**[SWS_Nm_00181]** 調整されたシャットダウンは、その NM 調整クラスタのいずれかのネットワークが次のいずれかを行った場合に、中止しなければならない。
- `Nm_RemoteSleepCancellation` を通知する、または
- `Nm_NetworkMode` を通知する、または
- `Nm_CoordReadyToSleepCancellation` を通知する、
- または、ComM が `Nm_NetworkRequest` または `Nm_PassiveStartUp` でいずれかのネットワークを要求する。

（SRS_Nm_02537）

注記: `Nm_NetworkStartIndication` は、これが上位層で扱われるため、調整されたシャットダウンを中止するきっかけにはならない。

**[SWS_Nm_00182]** 調整されたシャットダウンが中止された場合、NM コーディネータは、すでに「バススリープ」を通知したすべてのネットワークについて、`ComM_Nm_RestartIndication` を呼び出さなければならない。（SRS_Nm_02537）

根拠: Nm は自分でネットワークを起こす判断ができないので、（外部の）ウェイクアップの場合と同じく、ComM が決めなければならない。

**[SWS_Nm_00183]** 調整されたシャットダウンが中止された場合、NM コーディネータは、「バススリープ」を通知していないネットワークについて、`<BusNm>` へネットワークを要求しなければならない。（SRS_Nm_02537）

**[SWS_Nm_00185]** 調整アルゴリズムが中止された場合、調整されたシャットダウンの開始を制御するすべての条件を、あらためて評価しなければならない。（SRS_Nm_02537）

根拠: 調整されたシャットダウンが中止されたとき、ほとんどの場合、その NM 調整クラスタには、ネットワークのスリープが可能だと通知しなくなったネットワークがある。そのため、NM コーディネータは、現在眠っていないすべてのネットワークを起こし続けなければならない。どの条件も変わっていない場合もあり、その場合は調整されたシャットダウンが再開始されるだけになる。

**[SWS_Nm_00235]** 調整されたシャットダウンが中止され、Nm が `<BusNm>_NetworkRequest` に対して `E_NOT_OK` を受け取った場合、そのネットワークは、調整されたシャットダウンを開始する条件をふたたび評価するときに、起きているものとみなしてはならない。（SRS_Nm_02537）

根拠: 中止された調整されたシャットダウンの間に再要求が必要な `<BusNm>` は、いずれも以前に、ComM と Nm の両方により解放されている。ネットワークが本当に解放されたことを（Nm を通じて）ComM へ知らせるのは `<BusNm>` の責任であり、そのため、`Nm_NetworkRequest` へのエラー応答が ComM に直接届かなくても、ComM はネットワークの状態を知っている。

**[SWS_Nm_00236]** 調整されたシャットダウンが開始され、Nm が `<BusNm>_NetworkRelease` に対して `E_NOT_OK` を受け取った場合、シャットダウンは直ちに中止しなければならない。「バススリープモード」に入っていないすべてのネットワークについて、Nm は、そのネットワークを要求しなければならない。これには、`<BusNm>_NetworkRelease` でエラーを示したネットワークも含まれる。これが済み次第、調整されたシャットダウンを開始する条件を、ふたたび評価できる。これは、現在の調整されたシャットダウンに能動的に参加していなかったネットワークにも適用される。（SRS_Nm_02537）

根拠: ネットワークが解放できない場合、Nm の NM コーディネータと `<BusNm>` の間で状態を同期させるために、直ちに再要求しなければならない。`<BusNm>` の問題が続く限り、調整されたシャットダウンは、いずれふたたび開始される。問題を DEM やデフォルトエラートレーサへ直接報告するのは `<BusNm>` の責任であり、NM コーディネータは、成功するまでネットワークの解放を試みるだけにする。

### 7.4 バス固有ネットワークマネジメントモジュールの前提条件

この章は、基本機能と NM 調整機能で使われる API 呼び出しの概要と、両方の機能でバス固有 NM に期待される振る舞いの情報を示す。インタフェースの具体的な要求と、API を有効・無効にするコンフィギュレーションパラメータについては、第 8 章を参照。

#### 7.4.1 基本機能の前提条件

基本機能では、Nm は、ComM とバス固有 NM の間の転送層としてのみ働く。上位層からのすべての API 呼び出しは、下位層の対応する API 呼び出しへ転送しなければならない。下位層から呼び出される Nm のすべてのコールバックは、上位層の対応するコールバックへ転送しなければならない。基本機能は、ComM に次の API 呼び出しを提供する。

- `Nm_NetworkRequest` — [SWS_Nm_00032]
- `Nm_NetworkRelease` — [SWS_Nm_00046]
- `Nm_PassiveStartUp` — [SWS_Nm_00031]

注記: これは、バス固有 NM が、対応する関数 `<BusNm>_NetworkRequest`、`<BusNm>_NetworkRelease`、`<BusNm>_PassiveStartUp` を提供することを意味する。

基本機能は、ComM へ次の API コールバックを転送する。

- `Nm_NetworkStartIndication` — [SWS_Nm_00154]
- `Nm_NetworkMode` — [SWS_Nm_00156]
- `Nm_BusSleepMode` — [SWS_Nm_00162]
- `Nm_PrepareBusSleepMode` — [SWS_Nm_00159]

注記: これは、ComM が、対応するコールバック関数 `ComM_Nm_NetworkStartIndication`、`ComM_Nm_NetworkMode`、`ComM_Nm_BusSleepMode`、`ComM_Nm_PrepareBusSleepMode` を提供することを意味する。

Nm は、ComM が使わない、いくつかの API 呼び出しを上位層に提供する。これらは NM スタックの OEM 固有の拡張のために提供されるもので、AUTOSAR のどのモジュールからも必要とされない。これらは、バス固有 NM が提供する対応する API 呼び出しへ転送しなければならない。基本機能は、上位層の OEM 拡張に、次の API 呼び出しを提供する。

- `Nm_DisableCommunication` — [SWS_Nm_00033]
- `Nm_EnableCommunication` — [SWS_Nm_00034]
- `Nm_SetUserData` — [SWS_Nm_00035]
- `Nm_GetUserData` — [SWS_Nm_00036]
- `Nm_GetPduData` — [SWS_Nm_00037]
- `Nm_RepeatMessageRequest` — [SWS_Nm_00038]
- `Nm_GetNodeIdentifier` — [SWS_Nm_00039]
- `Nm_GetLocalNodeIdentifier` — [SWS_Nm_00040]
- `Nm_CheckRemoteSleepIndication` — [SWS_Nm_00042]
- `Nm_GetState` — [SWS_Nm_00043]

注記: これは、バス固有 NM が、対応する関数を任意で提供することを意味する。

#### 7.4.2 NM コーディネータ機能の前提条件

調整アルゴリズムは、バス固有 NM の次のインタフェースを使う。

- `<BusNm>_NetworkRequest` — [SWS_Nm_00119]
- `<BusNm>_NetworkRelease` — [SWS_Nm_00119]
- `<BusNm>_RequestBusSynchronization` — [SWS_Nm_00119]
- `<BusNm>_CheckRemoteSleepIndication` — [SWS_Nm_00119]

注記: NM コーディネータ機能の調整されたクラスタに含まれるように設定されたすべての NM ネットワークは、対応するバス NM が、NM メッセージを能動的に送信できるように設定されていなければならない（例: `CANNM_PASSIVE_MODE_ENABLED = false`）。この設定上の制約の結果として、Nm モジュールのコーディネータ機能が使うすべての BusNm は、API `<BusNm>_NetworkRequest` を提供しなければならない。

注記: ネットワークが調整されたネットワーク群のクラスタに含まれ、対応する BusNm がパッシブに設定されているコンフィギュレーションは、無効である。

注記: `<BusNm>_RequestBusSynchronization` は、非同期ネットワークがネットワークの解放前に同期できるように、Nm が `<BusNm>_NetworkRelease` の直前に呼び出す。ネットワークによっては、この呼び出しに意味がない。それでも、バス固有 NM は、NM コーディネータ機能の汎用性をサポートするため、このインタフェースを提供しなければならないが、空の実装を選んでもよい。

根拠: `<BusNm>_CheckRemoteSleepIndication` は、調整アルゴリズムの中で明示的には言及されない。使い方は実装に依存する。

調整アルゴリズムは、バス固有 NM が Nm の次のコールバックを呼び出せることを必要とする。

- `Nm_NetworkStartIndication` — [SWS_Nm_00154]
- `Nm_NetworkMode` — [SWS_Nm_00156]
- `Nm_BusSleepMode` — [SWS_Nm_00162]
- `Nm_PrepareBusSleepMode` — [SWS_Nm_00159]
- `Nm_RemoteSleepIndication` — [SWS_Nm_00192]
- `Nm_RemoteSleepCancellation` — [SWS_Nm_00193]
- `Nm_SynchronizationPoint` — [SWS_Nm_00194]

注記: `Nm_NetworkStartIndication`、`Nm_NetworkMode`、`Nm_BusSleepMode`、`Nm_PrepareBusSleepMode` は、調整アルゴリズムが、異なるネットワークの状態を追跡し、中止されたシャットダウンを扱う（7.3.3 節を参照）ために使われる。

注記: `Nm_RemoteSleepIndication` と `Nm_RemoteSleepCancellation` は、調整アルゴリズムが、調整されたシャットダウンを開始するすべての条件が満たされた時点を判断するために使われる。この通知は、バス固有 NM が、ネットワーク上のほかのすべてのノード（自分自身を除く）が「バススリープモード」へ入る準備ができたことを検出したときに呼び出される。実装によっては、API 呼び出し `<BusNm>_CheckRemoteSleepIndication` も使う。

注記: 調整クラスタに含まれるバス固有 NM は、バスを監視して、ネットワーク上のほかのすべてのノードがスリープの準備ができた時点を識別しなければならない。それが起きたとき、バス固有 NM は、Nm のコールバック `Nm_RemoteSleepIndication` を呼び出さなければならない（[SWS_Nm_00192] を参照）。

注記: 調整クラスタに含まれるバス固有 NM が、ネットワーク上のほかのすべてのノードがスリープの準備ができたことを Nm へ通知した後（[SWS_Nm_00192] を参照）も、いずれかのノードがふたたびネットワークを要求し始め、バスがもうスリープの準備ができていない状態になったかを識別するため、バスの監視を続けなければならない。それが起きたとき、バス固有 NM は、Nm のコールバック `Nm_RemoteSleepCancellation` を呼び出さなければならない（[SWS_Nm_00193] を参照）。

注記: Remote Sleep Indication と Cancellation の機能は、それぞれのバス固有 NM でさらに規定される。

根拠: `Nm_SynchronizationPoint` は、バス固有 NM が、調整されたシャットダウンを開始する適切な時点を調整アルゴリズムへ知らせるために呼び出さなければならない。周期的なネットワークでは、これは通常サイクルの境界である。周期的でないネットワークでは、これをほかの手段で定義する必要がある。NM 調整クラスタごとに、同期通知を使うか使わないかを設定でき（[SWS_Nm_00172] を参照）、使う場合、調整アルゴリズムは通知を選別して、同期ネットワークとして設定されたネットワークからの通知にだけ従う。

注記: `<bus>Nm` の実装に関する注意: 周期的なネットワークは、ほかのノードがネットワークを要求していないとき、`Nm_SynchronizationPoint` を繰り返し呼び出す。この呼び出しは通常、NM の投票に変化が起きうる、バス固有 NM のプロトコルの境界で行われる。

これらの `Nm_SynchronizationPoint` のうち 2 つの間に行われた `<BusNm>_ReleaseNetwork` の呼び出しは、次の `Nm_SynchronizationPoint` が呼び出されたはずの時点と同じ時点で処理されると想定される。

根拠: 同期通知は、`Nm_RemoteSleepIndication` が通知されたときに始まり、ネットワークが解放される（`<BusNm>_NetworkRelease`）か、`Nm_RemoteSleepCancellation` が呼び出されるまで続けなければならない。

注記: FlexRay のチャネル A と B を調整するユースケースで、NM クラスタの中にほかのネットワークがない場合、すなわち NM コーディネータが NM チャネルを 1 つしか含まない場合、この `NmChannelConfig` の `NmActiveCoordinator` を `TRUE` に、`NmChannelSleepMaster` を `FALSE` に設定して、チャネルが自分自身を調整できるようにする必要がある。なお、`NmSynchronizingNetwork` の値は、このネットワークがほかのネットワークと同じ調整クラスタにある場合にだけ意味を持つ。

#### 7.4.3 バス固有ネットワークのグローバルパラメータの設定

Nm のコンフィギュレーションには、バス固有 NM にあるオプション機能のサポートを制御するパラメータが含まれる。NM コーディネータ機能が使わない機能については、Nm は単なる通過のインタフェース層であるため、Nm のコンフィギュレーションでそれらを有効にしても、多くの場合は、制御用の API 関数と、バス固有層からのコールバック通知の通過が有効になるだけである。

NM に定義されたパラメータの多くは、すべてのバス固有 NM モジュールのグローバルな設定の元としてのみ使われる。バス固有 NM の対応するパラメータは、これらのパラメータから導出される。

### 7.5 追加機能

#### 7.5.1 Nm_CarWakeUpIndication

**[SWS_Nm_00252]** `<bus>Nm` が `Nm_CarWakeUpIndication` を呼び出し、`NmCarWakeUpCallout` が定義されている場合、NM インタフェースは、`NmCarWakeUpCallout` で定義されたコールアウト関数を、`nmNetworkHandle` をパラメータとして呼び出さなければならない。（SRS_Nm_02503）

**[SWS_Nm_00285]** `<bus>Nm` が `Nm_CarWakeUpIndication` を呼び出し、`NmCarWakeUpCallout` が定義されていない場合、NM インタフェースは、関数 `BswM_Nm_CarWakeUpIndication` を、`nmNetworkHandle` をパラメータとして呼び出さなければならない。（SRS_Nm_02503）

注記: `NmCarWakeUpCallout` から呼び出されるアプリケーションは、カーウェイクアップ（CWU）要求を管理し、自分の Nm メッセージの CWU ビットを設定することで、その要求をほかの Nm チャネルへ配信する責任を持つ。このアプリケーションは、一定の時間内に要求が繰り返されない場合、その CWU 要求を破棄する。

注記: コールアウトは、SWS_BSW_00039 と SWS_BSW_00135 で規定されたとおりに宣言される。

#### 7.5.2 Nm_StateChangeNotification

**[SWS_Nm_00249]** `NmStateReportEnabled` が `TRUE` に設定されている場合、`Nm_StateChangeNotification` は、`Com_SendSignal(uint8, Com_SignalIdType, const void*)` を、`NmStateReportSignalRef` を `Com_SignalIdType` として呼び出さなければならない。`NmStateReportSignalRef` は、Network Management State（NMS）と呼ばれる 6 ビットの信号を指す。NMS は Com で設定する必要がある。NMS は、表 7.1 に従った値に設定しなければならない。（SRS_Nm_00051）

| ビット | 値 | 名前 | 説明 |
|---|---|---|---|
| 0 | 1 | `NM_RM_BSM` | NM が RepeatMessage 状態（BusSleepMode からの遷移） |
| 1 | 2 | `NM_RM_PBSM` | NM が RepeatMessage 状態（PrepareBusSleepMode からの遷移） |
| 2 | 4 | `NM_NO_RM` | NM が NormalOperation 状態（RepeatMessage からの遷移） |
| 3 | 8 | `NM_NO_RS` | NM が NormalOperation 状態（ReadySleep からの遷移） |
| 4 | 16 | `NM_RM_RS` | NM が RepeatMessage 状態（ReadySleep からの遷移） |
| 5 | 32 | `NM_RM_NO` | NM が RepeatMessage 状態（NormalOperation からの遷移） |

表 7.1: ネットワークマネジメントの状態

> 再構成の注記: 表 7.1 は PDF の抽出で説明の行が崩れていたため、各行の名前と説明を、名前の意味に合わせて組み直した。

### 7.6 エラーの分類

#### 7.6.1 開発エラー

**[SWS_Nm_00232]** Nm は、コンフィギュレーションに応じて、表 7.2 に従う次のエラーと例外を検出できなければならない。（SRS_BSW_00327、SRS_BSW_00337、SRS_BSW_00385、SRS_BSW_00386）

| エラーの種類 | 関連性 | 関連するエラーコード | 値 [hex] |
|---|---|---|---|
| Nm インタフェースの初期化なしに API サービスが使われた | 開発 | `NM_E_UNINIT` | 0x00 |
| API サービスが、NULL ポインタ以外の誤ったパラメータで呼ばれた | 開発 | `NM_E_INVALID_CHANNEL` | 0x01 |
| API サービスが NULL ポインタで呼ばれた | 開発 | `NM_E_PARAM_POINTER` | 0x02 |

表 7.2: サポートする開発エラー

> 再構成の注記: 表 7.2 は PDF の抽出で行が混ざっていたため、各エラーの説明・コード・値を、意味に合わせて組み直した。

#### 7.6.2 ランタイムエラー

このモジュールは、ランタイムエラーを規定しない。

#### 7.6.3 一時的な故障

このモジュールは、一時的な故障を規定しない。

#### 7.6.4 製品エラー

このモジュールは、製品エラーを規定しない。

#### 7.6.5 拡張された製品エラー

このモジュールは、拡張された製品エラーを規定しない。

### 7.7 エラーの検出

詳細は、[2, SWS_BSWGeneral] の 7.3 章「エラーの検出」を参照。

### 7.8 エラーの通知

**[SWS_Nm_00233]** プリプロセッサスイッチ `NmDevErrorDetect` が `TRUE` に設定されている場合、`NetworkHandleType` パラメータを含むすべての関数呼び出しは、ネットワークのパラメータが設定済みのネットワークハンドルでなければ、エラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_BSW_00323、SRS_BSW_00369、SRS_BSW_00386）

注記: NULL ポインタの扱いは [2, SWS_BSW General] の中で規定されている。SWS_BSW_00212 を参照。

## 8 API 仕様

### 8.1 インポートする型

この章には、次のファイルから取り込むすべての型を挙げる。

**[SWS_Nm_00117]**

| モジュール | インポートする型 |
|---|---|
| Com | `Com_SignalIdType` |
| ComStack_Types | `NetworkHandleType` |
| Std_Types | `Std_ReturnType`、`Std_VersionInfoType` |

表 8.1: Nm_ImportedTypes（SRS_BSW_00301）

### 8.2 型定義

次の NM スタックの型が規定され、`NmStack_types.h` の中で定義しなければならない。

#### 8.2.1 Nm_ModeType

**[SWS_Nm_00274]**

| 項目 | 内容 |
|---|---|
| 名前 | `Nm_ModeType` |
| 型 | 列挙型 |
| 範囲 | `NM_MODE_BUS_SLEEP`: バススリープモード<br>`NM_MODE_PREPARE_BUS_SLEEP`: バススリープ準備モード<br>`NM_MODE_SYNCHRONIZE`: 同期モード<br>`NM_MODE_NETWORK`: ネットワークモード |
| 説明 | ネットワークマネジメントの動作モード。 |

表 8.2: Nm_ModeType（SRS_Nm_00044）

#### 8.2.2 Nm_StateType

**[SWS_Nm_00275]**

| 項目 | 内容 |
|---|---|
| 名前 | `Nm_StateType` |
| 型 | 列挙型 |
| 範囲 | `NM_STATE_UNINIT` = 0x00: 未初期化状態<br>`NM_STATE_BUS_SLEEP` = 0x01: バススリープ状態<br>`NM_STATE_PREPARE_BUS_SLEEP` = 0x02: バススリープ準備状態<br>`NM_STATE_READY_SLEEP` = 0x03: Ready Sleep 状態<br>`NM_STATE_NORMAL_OPERATION` = 0x04: Normal Operation 状態<br>`NM_STATE_REPEAT_MESSAGE` = 0x05: Repeat Message 状態<br>`NM_STATE_SYNCHRONIZE` = 0x06: Synchronize 状態<br>`NM_STATE_OFFLINE` = 0x07: Offline 状態 |
| 説明 | ネットワークマネジメントの状態機械の状態。 |

表 8.3: Nm_StateType（SRS_Nm_00050）

#### 8.2.3 Nm_BusNmType

**[SWS_Nm_00276]**

| 項目 | 内容 |
|---|---|
| 名前 | `Nm_BusNmType` |
| 型 | 列挙型 |
| 範囲 | `NM_BUSNM_CANNM`: CAN NM の型<br>`NM_BUSNM_FRNM`: FR NM の型<br>`NM_BUSNM_LINNM`: LIN NM の型<br>`NM_BUSNM_UDPNM`: UDP NM の型<br>`NM_BUSNM_GENERICNM`: 汎用 NM の型<br>`NM_BUSNM_UNDEF`: NM の型が未定義。FFh として定義しなければならない<br>`NM_BUSNM_J1939NM`: SAE J1939 NM の型（アドレスクレーミング） |
| 説明 | BusNm の型。 |

表 8.4: Nm_BusNmType（SRS_Nm_00044、SRS_Nm_00154、SRS_Nm_02515）

> 再構成の注記: PDF の抽出で、列挙子と説明の対応が崩れていた。名前の意味（CAN、FR、LIN、UDP、汎用、未定義、J1939）に合わせて組み直した。

#### 8.2.4 Nm_ConfigType

**[SWS_Nm_00282]**

| 項目 | 内容 |
|---|---|
| 名前 | `Nm_ConfigType` |
| 型 | 構造体 |
| 範囲 | 実装固有 |
| 説明 | Nm モジュールのコンフィギュレーションデータの構造体。 |

表 8.5: Nm_ConfigType（SRS_BSW_00414）

### 8.3 関数定義

#### 8.3.1 NM インタフェースが提供する標準サービス

##### 8.3.1.1 Nm_Init

**[SWS_Nm_00030]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_Init` |
| 構文 | `void Nm_Init(const Nm_ConfigType* ConfigPtr)` |
| サービス ID [hex] | 0x00 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `ConfigPtr`: 選択されたコンフィギュレーションセットへのポインタ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | NM インタフェースを初期化する。 |

表 8.6: Nm_Init（SRS_BSW_00101、SRS_BSW_00344、SRS_BSW_00358、SRS_BSW_00405、SRS_BSW_00414）

> 再構成の注記: PDF の抽出で、表の項目と値が行ごとに離れていたため、項目名の順序に合わせて値を組み直した。以降の API 表も同様。

**[SWS_Nm_00127]** `Nm_Init` の注意点: このサービス関数は、該当するバスインタフェースの初期化の後に呼び出さなければならない。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00283]** コンフィギュレーションポインタ `ConfigPtr` は、常に `NULL_PTR` の値でなければならない。（SRS_BSW_00414）

注記: コンフィギュレーションポインタ `ConfigPtr` は現在使われないため、`NULL_PTR` の値に設定しなければならない。

##### 8.3.1.2 Nm_PassiveStartUp

**[SWS_Nm_00031]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_PassiveStartUp` |
| 構文 | `Std_ReturnType Nm_PassiveStartUp(NetworkHandleType NetworkHandle)` |
| サービス ID [hex] | 0x01 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ネットワークマネジメントのパッシブ開始が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | この関数は `<BusNm>_PassiveStartUp` 関数を呼び出す（例: チャネルが CAN として設定されている場合は `CanNm_PassiveStartUp` 関数が呼ばれる）。 |

表 8.7: Nm_PassiveStartUp（SRS_Nm_00046、SRS_Nm_00051、SRS_Nm_00151、SRS_Nm_02513、SRS_Nm_02536）

**[SWS_Nm_00128]** `Nm_PassiveStartUp` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

##### 8.3.1.3 Nm_NetworkRequest

**[SWS_Nm_00032]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_NetworkRequest` |
| 構文 | `Std_ReturnType Nm_NetworkRequest(NetworkHandleType NetworkHandle)` |
| サービス ID [hex] | 0x02 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: バス通信の要求が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | この関数は `<BusNm>_NetworkRequest` を呼び出す（例: チャネルが CAN として設定されている場合は `CanNm_NetworkRequest` 関数が呼ばれる）。 |

表 8.8: Nm_NetworkRequest（SRS_Nm_00046、SRS_Nm_00047、SRS_Nm_00051、SRS_Nm_02513）

**[SWS_Nm_00129]** `Nm_NetworkRequest` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00130]** `Nm_NetworkRequest` が、`NmPassiveModeEnabled` が `TRUE` に設定されているネットワークハンドルで呼び出された場合、何も実行せず、`E_NOT_OK` を返さなければならない。`NmDevErrorDetect` が `TRUE` に設定されている場合は、この場合にエラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_Nm_00150）

##### 8.3.1.4 Nm_NetworkRelease

**[SWS_Nm_00046]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_NetworkRelease` |
| 構文 | `Std_ReturnType Nm_NetworkRelease(NetworkHandleType NetworkHandle)` |
| サービス ID [hex] | 0x03 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: バス通信の解放が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | この関数は、バス固有の関数 `<BusNm>_NetworkRelease` を呼び出す（例: チャネルが CAN として設定されている場合は `CanNm_NetworkRelease` 関数が呼ばれる）。 |

表 8.9: Nm_NetworkRelease（SRS_Nm_00048、SRS_Nm_00051）

**[SWS_Nm_00131]** `Nm_NetworkRelease` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00132]** `Nm_NetworkRelease` が、`NmPassiveModeEnabled` が `TRUE` に設定されているネットワークハンドルで呼び出された場合、何も実行せず、`E_NOT_OK` を返さなければならない。`NmDevErrorDetect` が `TRUE` に設定されている場合は、この場合にエラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_Nm_00150）

#### 8.3.2 NM インタフェースが提供する通信制御サービス

次のサービスは、診断通信マネージャ（DCM）が NM メッセージの送信を制御できるようにするために、NM インタフェースが提供する。

注記: 調整アルゴリズムを正しく動かすには、ECU がシャットダウンされる前に、NM PDU の送信機能が有効になっていることを保証する必要がある。NM PDU の送信機能が無効な間に `<BusNm>_NetworkRelease` が呼び出された場合、ECU は、NM PDU の送信機能が再び有効になった後でシャットダウンする。そのため、競合状態（例: 診断セッションを抜けた直後に通信を有効にする場合）や、通信制御の誤った使い方の場合にも、ECU はシャットダウンしうる。

##### 8.3.2.1 Nm_DisableCommunication

**[SWS_Nm_00033]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_DisableCommunication` |
| 構文 | `Std_ReturnType Nm_DisableCommunication(NetworkHandleType NetworkHandle)` |
| サービス ID [hex] | 0x04 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM PDU の送信機能の無効化が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | NM PDU の送信機能を無効にする。そのために `<BusNm>_DisableCommunication` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_DisableCommunication` 関数が呼ばれる）。 |

表 8.10: Nm_DisableCommunication（SRS_Nm_02513、SRS_Nm_02512）

**[SWS_Nm_00133]** `Nm_DisableCommunication` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00134]** `Nm_DisableCommunication` のコンフィギュレーション: この関数は、`NmComControlEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

**[SWS_Nm_00286]** `Nm_DisableCommunication` が、`NmPassiveModeEnabled` が `TRUE` に設定されているネットワークハンドルで呼び出された場合、何も実行せず、`E_NOT_OK` を返さなければならない。`NmDevErrorDetect` が `TRUE` に設定されている場合は、この場合にエラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_Nm_00150）

##### 8.3.2.2 Nm_EnableCommunication

**[SWS_Nm_00034]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_EnableCommunication` |
| 構文 | `Std_ReturnType Nm_EnableCommunication(NetworkHandleType NetworkHandle)` |
| サービス ID [hex] | 0x05 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM PDU の送信機能の有効化が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | NM PDU の送信機能を有効にする。そのために `<BusNm>_EnableCommunication` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_EnableCommunication` 関数が呼ばれる）。 |

表 8.11: Nm_EnableCommunication（SRS_Nm_00047、SRS_Nm_02512）

**[SWS_Nm_00135]** `Nm_EnableCommunication` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00136]** `Nm_EnableCommunication` のコンフィギュレーション: この関数は、`NmComControlEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

**[SWS_Nm_00287]** `Nm_EnableCommunication` が、`NmPassiveModeEnabled` が `TRUE` に設定されているネットワークハンドルで呼び出された場合、何も実行せず、`E_NOT_OK` を返さなければならない。`NmDevErrorDetect` が `TRUE` に設定されている場合は、この場合にエラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_Nm_00150）

#### 8.3.3 NM インタフェースが提供する追加サービス

次のサービスは、NM スタックの OEM 固有の拡張のために NM インタフェースが提供するもので、AUTOSAR のどのモジュールからも必要とされない。

##### 8.3.3.1 Nm_SetUserData

**[SWS_Nm_00035]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_SetUserData` |
| 構文 | `Std_ReturnType Nm_SetUserData(NetworkHandleType NetworkHandle, const uint8* nmUserDataPtr)` |
| サービス ID [hex] | 0x06 |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別<br>`nmUserDataPtr`: 次に送信される NM メッセージのユーザデータ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ユーザデータの設定が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | バスで次に送信される NM メッセージのユーザデータを設定する。そのために `<BusNm>_SetUserData` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_SetUserData` 関数が呼ばれる）。 |

表 8.12: Nm_SetUserData（SRS_Nm_02503）

**[SWS_Nm_00137]** `Nm_SetUserData` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00138]** `Nm_SetUserData` のコンフィギュレーション: この関数は、`NmUserDataEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

**[SWS_Nm_00288]** `Nm_SetUserData` が、`NmPassiveModeEnabled` が `TRUE` に設定されているネットワークハンドルで呼び出された場合、何も実行せず、`E_NOT_OK` を返さなければならない。`NmDevErrorDetect` が `TRUE` に設定されている場合は、この場合にエラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_Nm_00150）

**[SWS_Nm_00241]** `Nm_SetUserData` のコンフィギュレーション: `NmComUserDataSupport` が `TRUE` の場合、API `Nm_SetUserData` は利用できてはならない。（SRS_Nm_00150）

##### 8.3.3.2 Nm_GetUserData

**[SWS_Nm_00036]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_GetUserData` |
| 構文 | `Std_ReturnType Nm_GetUserData(NetworkHandleType NetworkHandle, uint8* nmUserDataPtr)` |
| サービス ID [hex] | 0x07 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmUserDataPtr`: 最後に正常に受信した NM メッセージのユーザデータのコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ユーザデータの取得が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | 最後に正常に受信した NM メッセージのユーザデータを取得する。そのために `<BusNm>_GetUserData` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_GetUserData` 関数が呼ばれる）。 |

表 8.13: Nm_GetUserData（SRS_Nm_02504）

**[SWS_Nm_00139]** `Nm_GetUserData` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00140]** `Nm_GetUserData` のコンフィギュレーション: この関数は、`NmUserDataEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

##### 8.3.3.3 Nm_GetPduData

**[SWS_Nm_00037]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_GetPduData` |
| 構文 | `Std_ReturnType Nm_GetPduData(NetworkHandleType NetworkHandle, uint8* nmPduData)` |
| サービス ID [hex] | 0x08 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmPduData`: NM PDU のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM PDU データの取得が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | 最も最近受信した NM メッセージから、PDU データ全体を取得する。そのために `<BusNm>_GetPduData` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_GetPduData` 関数が呼ばれる）。 |

表 8.14: Nm_GetPduData（SRS_Nm_02506）

**[SWS_Nm_00141]** `Nm_GetPduData` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

##### 8.3.3.4 Nm_RepeatMessageRequest

**[SWS_Nm_00038]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_RepeatMessageRequest` |
| 構文 | `Std_ReturnType Nm_RepeatMessageRequest(NetworkHandleType NetworkHandle)` |
| サービス ID [hex] | 0x09 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: Repeat Message Request ビットの設定が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | バスで次に送信される NM メッセージの Repeat Message Request ビットを設定する。そのために `<BusNm>_RepeatMessageRequest` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_RepeatMessageRequest` 関数が呼ばれる）。これにより、バス上のすべてのノードが、識別できるように NM メッセージを送信するようになる。 |

表 8.15: Nm_RepeatMessageRequest（SRS_Nm_00153）

**[SWS_Nm_00143]** `Nm_RepeatMessageRequest` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00289]** `Nm_RepeatMessageRequest` が、`NmPassiveModeEnabled` が `TRUE` に設定されているネットワークハンドルで呼び出された場合、何も実行せず、`E_NOT_OK` を返さなければならない。`NmDevErrorDetect` が `TRUE` に設定されている場合は、この場合にエラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_Nm_00150）

##### 8.3.3.5 Nm_GetNodeIdentifier

**[SWS_Nm_00039]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_GetNodeIdentifier` |
| 構文 | `Std_ReturnType Nm_GetNodeIdentifier(NetworkHandleType NetworkHandle, uint8* nmNodeIdPtr)` |
| サービス ID [hex] | 0x0a |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmNodeIdPtr`: 最後に正常に受信した NM メッセージのノード識別子のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: 最後に受信した NM メッセージからのノード識別子の取得が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | 最後に正常に受信した NM メッセージから、ノード識別子を取得する。関数 `<BusNm>_GetNodeIdentifier` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_GetNodeIdentifier` 関数が呼ばれる）。 |

表 8.16: Nm_GetNodeIdentifier（SRS_Nm_02505）

**[SWS_Nm_00145]** `Nm_GetNodeIdentifier` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

##### 8.3.3.6 Nm_GetLocalNodeIdentifier

**[SWS_Nm_00040]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_GetLocalNodeIdentifier` |
| 構文 | `Std_ReturnType Nm_GetLocalNodeIdentifier(NetworkHandleType NetworkHandle, uint8* nmNodeIdPtr)` |
| サービス ID [hex] | 0x0b |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `NetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmNodeIdPtr`: ローカルノードのノード識別子のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: ローカルノードのノード識別子の取得が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | ローカルノードに設定されたノード識別子を取得する。そのために `<BusNm>_GetLocalNodeIdentifier` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_GetLocalNodeIdentifier` 関数が呼ばれる）。 |

表 8.17: Nm_GetLocalNodeIdentifier（SRS_Nm_02508）

**[SWS_Nm_00147]** `Nm_GetLoclaNodeIdentifier`（原文の綴りのまま。`Nm_GetLocalNodeIdentifier` のこと）の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

##### 8.3.3.7 Nm_CheckRemoteSleepIndication

**[SWS_Nm_00042]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_CheckRemoteSleepIndication` |
| 構文 | `Std_ReturnType Nm_CheckRemoteSleepIndication(NetworkHandleType nmNetworkHandle, boolean* nmRemoteSleepIndPtr)` |
| サービス ID [hex] | 0x0d |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ NetworkHandle では再入不可、それ以外では再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmRemoteSleepIndPtr`: リモートスリープ通知の検査結果のコピー先のポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: リモートスリープ通知のビットの検査が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | リモートスリープ通知が起きているかを検査する。そのために `<BusNm>_CheckRemoteSleepIndication` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_CheckRemoteSleepIndication` 関数が呼ばれる）。 |

表 8.18: Nm_CheckRemoteSleepIndication（SRS_Nm_02513）

**[SWS_Nm_00149]** `Nm_CheckRemoteSleepIndication` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

**[SWS_Nm_00290]** `Nm_CheckRemoteSleepIndication` が、`NmPassiveModeEnabled` が `TRUE` に設定されているネットワークハンドルで呼び出された場合、何も実行せず、`E_NOT_OK` を返さなければならない。`NmDevErrorDetect` が `TRUE` に設定されている場合は、この場合にエラー `NM_E_INVALID_CHANNEL` を発生させなければならない。（SRS_Nm_00150）

**[SWS_Nm_00150]** `Nm_CheckRemoteSleepIndication` のコンフィギュレーション: この関数は、`NmRemoteSleepIndEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

##### 8.3.3.8 Nm_GetState

**[SWS_Nm_00043]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_GetState` |
| 構文 | `Std_ReturnType Nm_GetState(NetworkHandleType nmNetworkHandle, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr)` |
| サービス ID [hex] | 0x0e |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmStatePtr`: ネットワークマネジメントの状態のコピー先のポインタ<br>`nmModePtr`: ネットワークマネジメントのモードのコピー先の位置へのポインタ |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: エラーなし<br>`E_NOT_OK`: NM の状態の取得が失敗した<br>　NetworkHandle が存在しない（開発時のみ）<br>　モジュールが未初期化（開発時のみ） |
| 説明 | ネットワークマネジメントの状態を返す。関数 `<BusNm>_GetState` を呼び出さなければならない（例: チャネルが CAN として設定されている場合は `CanNm_GetState` 関数が呼ばれる）。 |

表 8.19: Nm_GetState（SRS_Nm_00050）

**[SWS_Nm_00151]** `Nm_GetState` の注意点: `<BusNm>` と Nm 自身が、正しく初期化されている。（SRS_BSW_00101、SRS_BSW_00416）

##### 8.3.3.9 Nm_GetVersionInfo

**[SWS_Nm_00044]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_GetVersionInfo` |
| 構文 | `void Nm_GetVersionInfo(Std_VersionInfoType* nmVerInfoPtr)` |
| サービス ID [hex] | 0x0f |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | `nmVerInfoPtr`: このモジュールのバージョン情報の格納先を指すポインタ |
| 戻り値 | なし |
| 説明 | このサービスは、このモジュールのバージョン情報を返す。 |

表 8.20: Nm_GetVersionInfo（SRS_BSW_00003、SRS_BSW_00407、SRS_BSW_00482）

### 8.4 コールバック通知

コールバック通知は、下位層のバス固有のネットワークマネジメントモジュールから呼び出される。Nm の基本機能（7.1 節）では、コールバックは上位層の ComM へ転送しなければならない。Nm の NM コーディネータ機能（7.2 節）では、コールバックは、NM コーディネータを制御するために使われる通知を提供する。

**[SWS_Nm_00028]** Nm のすべてのコールバックは、タスクまたは割り込みのどちらのコンテキストでも動作しうると想定しなければならない。（SRS_BSW_00333）

#### 8.4.1 標準のコールバック通知

##### 8.4.1.1 Nm_NetworkStartIndication

**[SWS_Nm_00154]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_NetworkStartIndication` |
| 構文 | `void Nm_NetworkStartIndication(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x11 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | バススリープモードで NM メッセージを受信したことの通知。ネットワーク内の一部のノードがすでにネットワークモードに入っていることを示す。 |

表 8.21: Nm_NetworkStartIndication（SRS_BSW_00359、SRS_Nm_02513）

**[SWS_Nm_00155]** コールバック関数 `Nm_NetworkStartIndication` による通知は、`ComM_Nm_NetworkStartIndication` を呼び出して ComM へ転送しなければならない。（SRS_Nm_02513）

##### 8.4.1.2 Nm_NetworkMode

**[SWS_Nm_00156]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_NetworkMode` |
| 構文 | `void Nm_NetworkMode(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x12 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | ネットワークマネジメントがネットワークモードに入ったことの通知。 |

表 8.22: Nm_NetworkMode（SRS_BSW_00359、SRS_Nm_00051）

**[SWS_Nm_00158]** コールバック関数 `Nm_NetworkMode` による通知は、`ComM_Nm_NetworkMode` を呼び出して ComM へ転送しなければならない。（SRS_Nm_00051）

##### 8.4.1.3 Nm_BusSleepMode

**[SWS_Nm_00162]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_BusSleepMode` |
| 構文 | `void Nm_BusSleepMode(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x14 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | ネットワークマネジメントがバススリープモードに入ったことの通知。 |

表 8.23: Nm_BusSleepMode（SRS_BSW_00359、SRS_Nm_00051）

**[SWS_Nm_00163]** コールバック関数 `Nm_BusSleepMode` による通知は、`ComM_Nm_BusSleepMode` を呼び出して ComM へ転送しなければならない。（SRS_Nm_00051）

##### 8.4.1.4 Nm_PrepareBusSleepMode

**[SWS_Nm_00159]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_PrepareBusSleepMode` |
| 構文 | `void Nm_PrepareBusSleepMode(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x13 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | ネットワークマネジメントがバススリープ準備モードに入ったことの通知。 |

表 8.24: Nm_PrepareBusSleepMode（SRS_BSW_00359、SRS_Nm_00051）

**[SWS_Nm_00161]** コールバック関数 `Nm_PrepareBusSleepMode` による通知は、`ComM_Nm_PrepareBusSleepMode` を呼び出して ComM へ転送しなければならない。（SRS_Nm_00051）

##### 8.4.1.5 Nm_RemoteSleepIndication

**[SWS_Nm_00192]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_RemoteSleepIndication` |
| 構文 | `void Nm_RemoteSleepIndication(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x17 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | ネットワークマネジメントが、ネットワーク上のほかのノードがすべてバススリープモードに入る準備ができたことを検出したことの通知。 |

表 8.25: Nm_RemoteSleepIndication（SRS_BSW_00359、SRS_Nm_00052）

**[SWS_Nm_00277]** `Nm_RemoteSleepIndication` のコンフィギュレーション: この関数は、`NmRemoteSleepIndEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

ネットワーク上のほかのノードがすべてバススリープモードに入る準備ができたという通知は、NM コーディネータの内部の目的にだけ必要である。

注記: NM コーディネータ機能が無効な場合、`Nm_RemoteSleepIndication` は空の関数でよい。

##### 8.4.1.6 Nm_RemoteSleepCancellation

**[SWS_Nm_00193]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_RemoteSleepCancellation` |
| 構文 | `void Nm_RemoteSleepCancellation(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x18 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | ネットワークマネジメントが、ネットワーク上のほかのノードがすべてはバススリープモードに入る準備ができているわけではなくなったことを検出したことの通知。 |

表 8.26: Nm_RemoteSleepCancellation（SRS_BSW_00359、SRS_Nm_02509）

**[SWS_Nm_00278]** `Nm_RemoteSleepCancellation` のコンフィギュレーション: この関数は、`NmRemoteSleepIndEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

ネットワーク上のほかのノードがすべてはバススリープモードに入る準備ができているわけではなくなったという通知は、NM コーディネータの内部の目的にだけ必要である。

注記: NM コーディネータ機能が無効な場合、`Nm_RemoteSleepCancellation` は空の関数でよい。

##### 8.4.1.7 Nm_SynchronizationPoint

**[SWS_Nm_00194]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_SynchronizationPoint` |
| 構文 | `void Nm_SynchronizationPoint(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x19 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | NM コーディネータ機能への通知。調整されたシャットダウンを開始するのに適した時点であることを示す。 |

表 8.27: Nm_SynchronizationPoint（SRS_BSW_00359、SRS_Nm_02516）

調整されたシャットダウンを開始するのに適した時点であるという通知は、NM コーディネータの内部の目的にだけ必要である。

##### 8.4.1.8 Nm_CoordReadyToSleepIndication

**[SWS_Nm_00254]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_CoordReadyToSleepIndication` |
| 構文 | `void Nm_CoordReadyToSleepIndication(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x1e |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | Control Bit Vector の NM コーディネータ Sleep Ready ビットが設定されたときに、通知を設定する。 |

表 8.28: Nm_CoordReadyToSleepIndication（SRS_BSW_00359、SRS_Nm_02535）

**[SWS_Nm_00255]** `Nm_CoordReadyToSleepIndication` のコンフィギュレーション: オプション。`NmCoordinatorSyncSupport` が `TRUE` に設定されている場合、Nm は API `Nm_CoordReadyToSleepIndication` を提供しなければならない。（SRS_Nm_00150）

##### 8.4.1.9 Nm_CoordReadyToSleepCancellation

**[SWS_Nm_00272]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_CoordReadyToSleepCancellation` |
| 構文 | `void Nm_CoordReadyToSleepCancellation(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x1f |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | Control Bit Vector の NM コーディネータ Sleep Ready ビットが 0 に戻されたときに、通知を取り消す。 |

表 8.29: Nm_CoordReadyToSleepCancellation（SRS_BSW_00359、SRS_Nm_02535）

**[SWS_Nm_00273]** `Nm_CoordReadyToSleepCancellation` のコンフィギュレーション: オプション。`NmCoordinatorSyncSupport` が `TRUE` に設定されている場合、Nm は API `Nm_CoordReadyToSleepCancellation` を提供しなければならない。（SRS_Nm_00150）

#### 8.4.2 追加のコールバック通知

次のコールバック通知は、バス固有の NM コンポーネントの OEM 固有の拡張のために NM インタフェースが提供するもので、AUTOSAR のどのモジュールからも必要とされない。基本機能と NM コーディネータ機能の文脈では、特別な用途はない。

##### 8.4.2.1 Nm_PduRxIndication

**[SWS_Nm_00112]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_PduRxIndication` |
| 構文 | `void Nm_PduRxIndication(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x15 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | NM メッセージを受信したことの通知。 |

表 8.30: Nm_PduRxIndication（SRS_BSW_00359）

NM メッセージを受信したという通知は、NM コーディネータの OEM 固有の拡張にだけ必要である。

**[SWS_Nm_00164]** `Nm_PduRxIndication` のコンフィギュレーション: この関数は、`NmPduRxIndicationEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

##### 8.4.2.2 Nm_StateChangeNotification

**[SWS_Nm_00114]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_StateChangeNotification` |
| 構文 | `void Nm_StateChangeNotification(NetworkHandleType nmNetworkHandle, Nm_StateType nmPreviousState, Nm_StateType nmCurrentState)` |
| サービス ID [hex] | 0x16 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別<br>`nmPreviousState`: NM チャネルの直前の状態<br>`nmCurrentState`: NM チャネルの現在の（新しい）状態 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の `<BusNm>` の状態が変わったことの通知。 |

表 8.31: Nm_StateChangeNotification（SRS_BSW_00359、SRS_Nm_00050）

バス固有 NM の状態が変わったという通知は、NM コーディネータの OEM 固有の拡張にだけ必要である。

**[SWS_Nm_00165]** `Nm_StateChangeNotification` のコンフィギュレーション: この関数は、`NmStateChangeIndEnabled` が `TRUE` に設定されている場合にだけ利用できる。（SRS_Nm_00150）

##### 8.4.2.3 Nm_RepeatMessageIndication

**[SWS_Nm_00230]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_RepeatMessageIndication` |
| 構文 | `void Nm_RepeatMessageIndication(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x1a |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | Repeat Message Request ビットが設定された NM メッセージを受信したことを通知するサービス。 |

表 8.32: Nm_RepeatMessageIndication（SRS_BSW_00359、SRS_Nm_00153）

Repeat Message ビットが設定された NM メッセージを受信したという通知は、NM コーディネータの OEM 固有の拡張にだけ必要である。

##### 8.4.2.4 Nm_TxTimeoutException

**[SWS_Nm_00234]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_TxTimeoutException` |
| 構文 | `void Nm_TxTimeoutException(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x1b |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別（原文の説明は抽出で欠落） |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | NM メッセージの送信の試行が失敗したことを通知するサービス。 |

表 8.33: Nm_TxTimeoutException（SRS_BSW_00359）

NM メッセージの送信の試行が失敗したという通知は、Nm の OEM 固有の拡張にだけ必要である。


##### 8.4.2.5 Nm_CarWakeUpIndication

**[SWS_Nm_00250]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_CarWakeUpIndication` |
| 構文 | `void Nm_CarWakeUpIndication(NetworkHandleType nmChannelHandle)` |
| サービス ID [hex] | 0x1d |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `nmChannelHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | この関数は、CWU 要求の受信を通知するために `<Bus>Nm` から呼び出される。 |

表 8.34: Nm_CarWakeUpIndication（SRS_BSW_00359、SRS_Nm_02503）

**[SWS_Nm_00251]** `Nm_CarWakeUpIndication` のコンフィギュレーション: オプション。`NmCarWakeUpRxEnabled` が `TRUE` の場合、Nm は API `Nm_CarWakeUpIndication` を提供しなければならない。（SRS_Nm_00150）

### 8.5 スケジュール関数

基本機能（7.1 章）には、上位層または下位層からの呼び出しの範囲外で呼び出す必要のあるロジックが含まれないため、メイン関数は、NM コーディネータ機能（7.2 章）を実装するためにだけ必要である。

**[SWS_Nm_00020]** スケジュールされるメイン関数には、NM コーディネータ機能に関するロジックだけを含めなければならない。（SRS_BSW_00373）

**[SWS_Nm_00121]** メイン関数が、Nm が初期化される前に呼び出された場合、メイン関数は、エラーを出さずに直ちに戻らなければならない。（SRS_BSW_00450）

根拠: NM コーディネータ機能が使われない、または無効な場合、メイン関数の呼び出しがエラーになってはならず、何も実行されるべきでない。

#### 8.5.1 Nm_MainFunction

**[SWS_Nm_00118]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Nm_MainFunction` |
| 構文 | `void Nm_MainFunction(void)` |
| サービス ID [hex] | 0x10 |
| 説明 | この関数は、固定の周期的なスケジューリングを必要とする、NM インタフェースの処理を実装する。 |

表 8.35: Nm_MainFunction（SRS_BSW_00424、SRS_BSW_00425）

**[SWS_Nm_00279]** `NmCoordinatorSupportEnabled` が `TRUE` に設定されている場合、`Nm_MainFunction` API は利用できなければならない。（SRS_Nm_00150）

### 8.6 期待されるインタフェース

この章には、ほかのモジュールから必要とされるインタフェースをすべて挙げる。

#### 8.6.1 必須インタフェース

この章には、ほかのモジュールから必要とされるインタフェースをすべて挙げる。

**[SWS_Nm_00119]**

| API 関数 | 説明 |
|---|---|
| `ComM_Nm_BusSleepMode` | ネットワークマネジメントがバススリープモードに入ったことの通知。このコールバック関数は、ハードウェアとトランシーバのバススリープモードへの遷移を行うべきである。 |
| `ComM_Nm_NetworkMode` | ネットワークマネジメントがネットワークモードに入ったことの通知。 |
| `ComM_Nm_NetworkStartIndication` | バススリープモードで NM メッセージを受信したことの通知。ネットワーク内の一部のノードがすでにネットワークモードに入っていることを示す。 |
| `ComM_Nm_PrepareBusSleepMode` | ネットワークマネジメントがバススリープ準備モードに入ったことの通知。 |
| `ComM_Nm_RestartIndication` | 再入可能性: 再入可能（ただし同じ NM チャネルでは不可）。NmIf が調整されたバスのシャットダウンを開始していて、かつ、調整されたバスのすべてがバススリープ状態を通知しているわけではなく、かつ、調整されたバスの少なくとも 1 つで NM が再起動された場合、NM インタフェースは、すでにバススリープ状態を通知したチャネルの `nmNetworkHandle` を指定して、コールバック関数 `ComM_Nm_RestartIndication` を呼び出さなければならない。 |

表 8.36: Nm の必須インタフェース（SRS_Nm_02515、SRS_Nm_02536）

> 再構成の注記: PDF の抽出で、API 名と説明の対応が崩れていた（`ComM_Nm_PrepareBusSleepMode` の名前も 2 行に分かれていた）。説明文の内容に合わせて組み直した。

#### 8.6.2 オプションインタフェース

この章は、このモジュールのオプション機能を実現するために必要なインタフェースを定義する。

**[SWS_Nm_00166]**

| API 関数 | 説明 |
|---|---|
| `BswM_Nm_CarWakeUpIndication` | カーウェイクアップを通知するために Nm が呼び出す関数。 |
| `Com_SendSignal` | サービス `Com_SendSignal` は、SignalId で識別される信号オブジェクトを、パラメータ SignalDataPtr が参照する信号で更新する。 |
| `Det_ReportError` | 開発エラーを報告するサービス。 |

表 8.37: Nm のオプションインタフェース（SRS_Nm_00150、SRS_Nm_02515）

#### 8.6.3 設定可能なインタフェース

この章には、呼び出し先の関数を設定できるインタフェースをすべて挙げる。呼び出し先の関数は通常、コールバック関数である。この種のインタフェースの名前は設定可能なため、固定されていない。

##### 8.6.3.1 NmCarWakeUpCallout

**[SWS_Nm_00291]**

| 項目 | 内容 |
|---|---|
| サービス名 | `<NmCarWakeUpCallout>` |
| 構文 | `void <NmCarWakeUpCallout>(NetworkHandleType nmNetworkHandle)` |
| サービス ID [hex] | 0x20 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `nmNetworkHandle`: NM チャネルの識別 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | `Nm_CarWakeUpIndication()` から呼び出されるコールアウト関数。 |

表 8.38: NmCarWakeUpCallout（SRS_Nm_02504）

### 8.7 バージョンチェック

詳細は、[2, SWS_BSWGeneral] の 5.1.8 章「バージョンチェック」を参照。

---

## 学習メモ（訳者による解釈。仕様書の記述ではない）

- Nm は、ComM とバス固有 NM（CanNm など）の間の「薄い転送層」である。基本機能では、`Nm_NetworkRequest`／`Nm_NetworkRelease`／`Nm_PassiveStartUp` を `<BusNm>_…` へ、`Nm_NetworkStartIndication`／`Nm_NetworkMode`／`Nm_PrepareBusSleepMode`／`Nm_BusSleepMode` を `ComM_Nm_…` へ、それぞれ転送するだけである。
- NM コーディネータ機能（複数バスのシャットダウンの同期）は、`NmCoordinatorSupportEnabled` が `TRUE` のときだけ必要になる。1 つのバスだけを扱う構成では不要で、その場合 `Nm_MainFunction`、`Nm_RemoteSleepIndication`、`Nm_RemoteSleepCancellation` などは空の関数でよい。
- 調整されたシャットダウンが中止されたとき、すでにバススリープに入ったネットワークを Nm は自分では起こさず、`ComM_Nm_RestartIndication` で ComM に判断を委ねる（[SWS_Nm_00182]）。ネットワークの起動は ComM の責任、という分担を示している。
- `NmPassiveModeEnabled` が `TRUE` のチャネルに対する `Nm_NetworkRequest` などは、何も実行せず `E_NOT_OK` を返す。`NmDevErrorDetect` が有効なら `NM_E_INVALID_CHANNEL` も出す（[SWS_Nm_00130] ほか）。

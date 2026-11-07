# PDU Router（PduR）仕様書 日本語訳

- 原典: AUTOSAR CP Release 4.3.1 `AUTOSAR_SWS_PDURouter.pdf`（Document ID 035）
- 範囲: 第7章（機能仕様）と第8章（API 仕様）。第1〜6章（導入、関連文書、制約、依存関係、トレーサビリティ）、第9章（シーケンス図）、第10章以降（コンフィギュレーション）は対象外。
- 凡例: `[SWS_PduR_xxxxx]` は原典の条項 ID（大文字の `[SWS_PDUR_xxxxx]` も原典のまま）。`（SRS_…）` は原典が条項に付けている上位要求の ID。`<Up>`、`<Lo>`、`<LoTp>`、`<User:Up>` などは、PduR が相手のモジュール名に置き換える、コンフィギュレーションできる API 名の記号（`<Up>` は上位層、`<Lo>` は下位層のインタフェース、`<LoTp>` は下位層のトランスポートプロトコル。`<SrcLo>`、`<DstLo>` は、ゲートウェイの送信元と宛先の下位層）。
- 図は訳せないため、位置に注記だけを残している。図の内容は原典を参照すること。
- 「学習メモ」は原文の訳ではなく、このプロジェクトの実装と照らすための訳者の解釈。

---

## 7 機能仕様

PDU Router モジュールは、インタフェースモジュールとトランスポートプロトコルモジュール（下位層のモジュール）の上、COM と DCM（上位層のモジュール）の下に置かれる、I-PDU の転送ユニットである（図 1 を参照）。

PDU Router モジュールの隣に、多重化された I-PDU のサポートを提供する、I-PDU Multiplexer（IpduM）モジュール [7] がある。IpduM は、多重化された I-PDU を送信するために PDU Router モジュールを呼ぶとき、または、多重化された I-PDU の受信、送信確認、またはトリガ送信によるデータの提供のために PDU Router モジュールから呼ばれるときは、上位層のモジュールとみなされなければならない。IpduM が、送信確認または受信通知を上位層（例: COM）へ転送するために PDU Router モジュールを呼ぶとき、または、多重化された I-PDU に属する I-PDU を更新するために PDU Router モジュールから呼ばれるときは、下位層のモジュールとみなされなければならない。

ECU から見ると、PDU Router モジュールは、3 つの異なる種類の操作を行える。

- **ローカルのモジュールへの PDU の受信**: 下位層のモジュールから I-PDU を受信し、1 つ以上の上位層のモジュールへ転送する。
- **ローカルのモジュールからの PDU の送信**: 上位層のモジュールの要求で、I-PDU を 1 つ以上の下位層のモジュールへ送信する。
- **PDU ゲートウェイ**:
  1. インタフェースモジュールから I-PDU を受信し、同じ、または他の通信インタフェースモジュールを介して、直ちにその I-PDU を送信する。
  2. トランスポートプロトコルモジュールから I-PDU を受信し、同じ、または他のトランスポートプロトコルモジュールを介して、その I-PDU を送信する。

PDU の受信と PDU ゲートウェイの組み合わせは許される。例: COM モジュールが I-PDU を受信すると同時に、それが別の下位層のモジュールへゲートウェイされる。

### 7.1 I-PDU の扱い

**[SWS_PduR_00160]** PDU Router モジュールは、I-PDU を、変更せずに、一貫した形で、送信元のモジュールから宛先のモジュールへ転送しなければならない。（SRS_PduR_06049）

I-PDU は、I-PDU ID、または（もしくは）シンボリック名（すなわち、I-PDU のコンテナの `SymbolicNameValue`）で識別される [6]。デバッグとポストビルドのためには、PDU Router モジュールがコンパイルされた後に I-PDU を識別する必要があるため、I-PDU ID が必要である。PDU Router モジュールがプリコンパイル（すなわち、ソースコード）の場合は、シンボリック名を使ってよい。ECU Configuration の仕様 [6] を参照。

I-PDU を扱い、I-PDU のための API を提供する各 BSW モジュールは、I-PDU ID の一覧を含まなければならない [6]。これは、呼ばれる各モジュールが、I-PDU を識別するための参照表を持つことを意味する。

例: COM モジュールが `PduR_ComTransmit` を呼ぶ（ここで PDU Router モジュールが I-PDU ID を列挙する）。PDU Router モジュールが `CanIf_Transmit` を呼ぶ（ここで CanIf モジュールのコンフィギュレーションが I-PDU ID を列挙する）。CanIf が `PduR_CanIfTxConfirmation` を呼ぶ（ここで PDU Router モジュールのコンフィギュレーションが I-PDU ID を列挙する）。PDU Router モジュールが `Com_TxConfirmation` を呼ぶ（ここで COM モジュールのコンフィギュレーションが I-PDU ID を列挙する）。この例は、次の図 3 に示されている（パラメータとして I-PDU ID だけを示す）。

> （図 3「I-PDU ID の例」: 図。訳なし。読み取れる内容: COM が `PduR_Transmit(11)` を呼び、PduR が `CanIf_Transmit(22)` を呼ぶ。送信確認は、CanIf が `PduR_CanIfTxConfirmation(42)` を呼び、PduR が `Com_TxConfirmation(32)` を呼ぶ。同じ I-PDU でも、モジュールごとに別の ID が使われる。）

**[SWS_PduR_00161]** PDU Router モジュールは、経路を、送信元のモジュールの I-PDU ID（PDU Router のコンフィギュレーションにある）と、宛先の I-PDU ID（呼ばれる宛先のモジュールのコンフィギュレーションにある）の組み合わせで、一意に識別しなければならない。（SRS_PduR_06003）

**[SWS_PduR_00766]** PDU Router モジュールは、送信の経路と、確認／通知の経路の両方について、I-PDU ID を宛先のモジュールへ変換しなければならない。（SRS_PduR_06003）

例: COM モジュールが、I-PDU を CanIf と LinIf へ送信する。`PduR_ComTransmit` が呼ばれる。PDU Router モジュールは、送信元の I-PDU ID（PDU Router モジュールのコンフィギュレーション）を、LinIf 用の I-PDU ID 1 つ（LinIf モジュールのコンフィギュレーション）と、CanIf 用の I-PDU ID 1 つ（CanIf モジュールのコンフィギュレーション）へ変換する。COM モジュールから受け取った `PduInfoType` の値は、変更せずに CanIf と LinIf のモジュールへコピーされる。

例: LinIf が、I-PDU ID とともに、送信の成否に応じて、結果 `E_OK`（送信成功）または `E_NOT_OK`（送信失敗）で `PduR_LinIfTxConfirmation` を呼ぶ。その後、PDU Router モジュールは、この I-PDU ID を変換し、変換した I-PDU ID と、受け取った結果で `Com_TxConfirmation` を使って、呼び出しを COM へ転送する。

**[SWS_PduR_00162]** PDU Router モジュールは、コンフィギュレーションで与えられた経路に従ってだけ、I-PDU を経路付けしなければならない。（SRS_PduR_06003）

**[SWS_PDUR_00828]** PduR のジェネレータ（検証）は、異なる `MetaDataType` の I-PDU が、経路で接続されているコンフィギュレーションを、拒否しなければならない。

#### 7.1.1 上位のモジュールへの I-PDU の受信

PDU Router モジュールの受信の操作は、常に、下位層のモジュール（通信インタフェースモジュールまたはトランスポートプロトコルモジュール）からの通知（`PduR_<Lo>RxIndication` または `PduR_<LoTp>RxIndication`）で終わる。通知関数は、下位層により、通信ドライバをポーリングした後の周期関数のコンテキスト、または割り込みのコンテキストで実行される。

##### 7.1.1.1 通信インタフェース

送信元の通信インタフェースモジュールは、`PduR_<Lo>RxIndication` を呼んで、受信した I-PDU を通知する。I-PDU は、経路でコンフィギュレーションされたとおり、複数のローカルの宛先モジュールを持ちうる。

**[SWS_PduR_00164]** PDU Router モジュールは、通信インタフェースモジュールから受信し、1 つ以上の上位層のモジュールへ経路付けされる I-PDU について、1:n の経路付けを提供しなければならない。

例: I-PDU を CanIf で受信し、COM へ転送する。

注: I-PDU は、1 つ以上の上位のモジュールが受信すると同時に、1 つ以上の通信インタフェースへゲートウェイされることがある。7.1.3 を参照。（SRS_PduR_06012、SRS_PduR_06030）

**[SWS_PduR_00621]** `PduR_<Lo>RxIndication` が呼ばれたとき、PDU Router モジュールは、宛先の上位のモジュールごとに `<Up>_RxIndication` を呼ばなければならない。（SRS_PduR_06012）

**[SWS_PduR_00744]** I-PDU がローカルのモジュールで受信される場合、PDU Router は、I-PDU の長さを検査せず、通知を上位層のモジュールへ転送するだけでなければならない。

PDU Router モジュールは、この I-PDU をバッファリングしないため、コンフィギュレーションされた長さより長い／短い I-PDU を拒否する必要は無い。（SRS_PduR_06012）

##### 7.1.1.2 トランスポートプロトコル

トランスポートプロトコルモジュールの場合、PDU Router モジュールは、最初のフレーム（FF）またはシングルフレーム（SF）の受信時に、まず受信開始の通知を受ける。この呼び出しは、`<Up>_StartOfReception` を呼ぶことで、関連する上位層のモジュールへ転送される。各セグメント（N-PDU）のペイロードは、その後の `<Lo>_CopyRxData` の呼び出しの中で、宛先の上位層のモジュールへコピーされる。最後の N-PDU の受信後、トランスポートプロトコルモジュールは、I-PDU 全体が受信されたことを PDU Router モジュールに通知し、PDU Router モジュールは、`<Up>_TpRxIndication` を呼ぶことで、この通知を関連する上位層のモジュールへ転送する。

トランスポートプロトコルモジュールによる I-PDU の受信では、経路でコンフィギュレーションされる上位層のモジュールは、1 つだけでよい。

**[SWS_PduR_00673]** PDU Router モジュールは、送信元のトランスポートプロトコルモジュールから受信し、1 つの宛先の上位のモジュールへ経路付けされる I-PDU について、1:1 の経路付けを提供しなければならない。

例: 機能アドレッシングの要求（SF）を、CanTp モジュールから受信し、DCM モジュールへ経路付けする。（SRS_PduR_06026）

**[SWS_PduR_00549]** 送信元のトランスポートプロトコルモジュールが、上位層の宛先だけを持つ PDU の受信の開始を、`PduR_<LoTp>StartOfReception` で通知したとき、PDU Router モジュールは、`<Up>_StartOfReception` を呼んで、要求を宛先の上位層のモジュールへ転送しなければならない。（SRS_PduR_06026）

**[SWS_PduR_00623]** PDU Router は、`<Up>_StartOfReception` の戻り値を、送信元のトランスポートプロトコルへ転送しなければならない。（SRS_PduR_06104）

**[SWS_PduR_00428]** 送信元のトランスポートプロトコルモジュールが、`PduR_<LoTp>CopyRxData` を使って、受信したデータのコピーを PDU Router モジュールへ要求したとき、PDU Router モジュールは、`<Up>_CopyRxData` を呼んで、要求を宛先の上位層のモジュールへ転送しなければならない。（SRS_PduR_06026）

**[SWS_PduR_00429]** 送信元のトランスポートプロトコルモジュールが、`PduR_<LoTp>RxIndication` を呼んで、I-PDU 全体の受信を通知したとき、PDU Router モジュールは、`<Up>_TpRxIndication` を呼んで、通知を宛先の上位層のモジュールへ転送しなければならない。（SRS_PduR_06026）

**[SWS_PduR_00207]** 送信元のトランスポートプロトコルモジュールが、`PduR_<LoTp>RxIndication` を使ってエラーを報告した場合、PDU Router モジュールは、通知を上位層のモジュールへ転送する以外の、エラー処理を行ってはならない。（SRS_PduR_06104）

###### 7.1.1.2.1 長さが不明な I-PDU の扱い

PduR は、TP API を使って、長さが不明な I-PDU（すなわち、ストリーミング型のデータ）を扱える。不明な長さの定義は、`TpSduLength=0` で示される。

**[SWS_PDUR_00821]** ローカルの受信の状況で、`PduR_<SrcLoTp>StartOfReception` が `TpSduLength=0` で呼ばれた場合、PduR は、`TpSduLength=0` で `<Up>_StartOfReception` を呼ばなければならない。

#### 7.1.2 上位のモジュールからの I-PDU の送信

宛先の下位層のモジュールの送信の操作は、常に非同期である。これは、送信サービスの要求が、I-PDU が PDU Router モジュールにより宛先の下位層のモジュールへ渡された後、直ちに戻ることを意味する。I-PDU の送信の成功または失敗の後に、PDU Router モジュールが、宛先の下位層のモジュールから `PduR_<Lo>TxConfirmation`（通信インタフェース）または `PduR_<LoTp>TxConfirmation`（トランスポートプロトコル）で通知を受けた場合、PDU Router モジュールは、この通知を、`<Up>_TxConfirmation`（通信インタフェース）または `<Up>_TpTxConfirmation`（トランスポートプロトコル）で、上位層のモジュールへ転送する。

PDU Router モジュールの送信の操作は、送信元の上位層のモジュールからの PDU の送信要求で起動され、要求を宛先の下位層のモジュールへ転送する。

**[SWS_PduR_00629]** 送信元の上位層のモジュールからの PDU の送信の場合、I-PDU は、PDU Router モジュールにバッファリングされてはならない。（SRS_PduR_06012、SRS_PduR_06026）

##### 7.1.2.1 マルチキャスト

マルチキャストの機能は、4.1.1 節で述べたこの機能を使う場合の問題があるため、独立した節に分けられている。

PDU Router モジュールが直接扱う、さらなる要求:

**[SWS_PduR_00218]** 与えられた I-PDU ID が PDU のグループ（マルチキャストの送信要求）を表し、転送された送信要求の少なくとも 1 つが正常に戻る場合、関数 `PduR_<Up>Transmit` は `E_OK` を返さなければならない。（SRS_PduR_06125）

`E_OK` を返す通信インタフェースは、直接、またはトリガ送信を介して、そのデータを送信することに注意。

他のトランスポートプロトコルモジュールは `E_NOT_OK` を返すことがあり、その場合、これらのモジュールは `PduR_<LoTp>CopyTxData` を呼ばない。送信元の上位層のモジュールは、少なくとも 1 つの送信が成功したことを知らされているため、少なくとも 1 つのトランスポートプロトコルモジュールが、`PduR_<LoTp>CopyTxData` を呼ぶ。

**[SWS_PduR_00633]** 送信要求に、複数の下位層の宛先のモジュール（1:n、n>1）がある場合、これらのモジュールはすべて、通信インタフェースモジュールか、トランスポートプロトコルモジュールのどちらかでなければならない。両者の混在は不可。（SRS_PduR_06029、SRS_PduR_06030）

例: 上記の要求は、基本的に、COM モジュールが送信を要求し、PDU Router モジュールが、同じ I-PDU を使って、CanTp モジュールと CanIf モジュールへ送信を経路付けすることができない、ということである。

**[SWS_PduR_00589]** マルチキャスト（1:n、n>1）の通信インタフェースの送信の場合、PDU Router は、送信確認をサポートする通信インタフェースモジュールからの、最後の送信確認を受信したときに、上位層のモジュールの送信確認 API を呼ばなければならない。（SRS_PduR_06125、SRS_PduR_06119）

注: 上記の要求は、すべての宛先が送信確認を提供するわけではない場合でも、動作する。

実装上の注意: 送信元のモジュールが送信を要求し、PduR がマルチキャスト（1:n、n>1）を行う場合、要求とマルチキャストのすべての I-PDU は、異なる I-PDU ID を持つ。したがって、PduR は、送信を正しく確認できるように、送信要求の I-PDU ID を覚えておく必要がある。

##### 7.1.2.2 通信インタフェース

通信インタフェースで I-PDU を送信する方法は、3 つある。

1. **直接のデータ提供**: 上位のモジュールが `PduR_<Up>Transmit` 関数を呼び、PDU Router モジュールが呼び出しを `<Lo>_Transmit` へ転送し、データは、その呼び出しの中で、下位の通信インタフェースモジュールによりコピーされる。
2. **トリガ送信によるデータ提供**: 下位の通信インタフェースモジュールが、`PduR_<Lo>TriggerTransmit` を使って I-PDU の送信を要求し、PDU Router モジュールが呼び出しを `PduR_<Up>TriggerTransmit` へ転送し、データは上位のモジュールによりコピーされる。
3. 上位のモジュールが `PduR_<Up>Transmit` 関数を呼び、PDU Router モジュールが呼び出しを `PduR_<Lo>Transmit` へ転送し、データは下位のモジュール（通信インタフェースモジュール）によりコピーされない。データは、後で、下位層が `PduR_<Lo>TriggerTransmit` を使って要求する。

I-PDU の送信の確認は、直接のデータ提供とトリガ送信によるデータ提供で、同じである。

**[SWS_PduR_00627]** 通信インタフェースモジュールが `PduR_<Lo>TxConfirmation` を呼んだとき、PDU Router は、上位のモジュールの `<Up>_TxConfirmation` を呼び、送信の結果を、下位層から上位層のインタフェースへ転送しなければならない。（SRS_PduR_06012）

**[SWS_PduR_00745]** I-PDU が上位層のモジュールにより送信される場合、PDU Router モジュールは、I-PDU の長さを検査してはならない。（SRS_PduR_06012）

**[SWS_PduR_00625]** 送信元の上位層のモジュールが `PduR_<Up>Transmit` を呼んだとき、PDU Router は、宛先の通信インタフェースモジュールごとに `<Lo>_Transmit` を呼ばなければならない。（SRS_PduR_06026）

**[SWS_PduR_00626]** シングルキャスト（1:1）の場合、`<Lo>_Transmit` の呼び出しの戻り値は、送信元の上位層のモジュールへ転送されなければならない。（SRS_PduR_06012、SRS_PduR_06104）

###### 7.1.2.2.1 トリガ送信によるデータ提供

上位層のモジュールは、更新ビットをリセットするかどうかを知らされなければならず、I-PDU カウンタを適切に扱わなければならない。

**[SWS_PduR_00430]** PDU Router モジュールは、通信インタフェースモジュールによる `PduR_<Lo>TriggerTransmit` の要求を、`<Up>_TriggerTransmit` を呼んで、上位のモジュールへ転送しなければならない。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PduR_00661]** PDU Router モジュールは、`<Up>_TriggerTransmit` の戻り値を、下位層のモジュールへコピーしなければならない。（SRS_PduR_06104）

###### 7.1.2.2.2 エラー処理

通信インタフェースモジュールを使ったシングルキャストやマルチキャストで起きたエラーには、特定のエラー処理は行われない。戻り値のエラーは、送信元の上位層のモジュールへ転送される。

##### 7.1.2.3 トランスポートプロトコル

トランスポートプロトコルを使った I-PDU の送信には、シングルキャストとマルチキャストの 2 種類がある。シングルキャスト（1:1）の送信は、送信元の上位層のモジュール 1 つと、宛先の下位層のトランスポートプロトコルモジュール 1 つから成る。マルチキャスト（1:n、n>1）の送信は、宛先の下位層のトランスポートプロトコルモジュールが複数ある。PDU Router モジュールは、送信要求が、1 つの N-PDU（SF）か、複数の N-PDU（FF、CF など）かを検査しない。

I-PDU の送信の開始は、送信元の上位層のモジュールによる `PduR_<Up>Transmit` の要求で行われる。PduR は、経路に従って、`<Lo>_Transmit` を使って、要求を 1 つ以上の宛先の下位層のトランスポートプロトコルモジュールへ転送する。`<Lo>_Transmit` には、データが含まれる場合も、含まれない場合もあることに注意。

宛先のモジュールは、`PduR_<LoTp>CopyTxData` を呼んで、データを要求する。（トランスポートプロトコルがサポートする場合の）データの再送は、`RetryInfoType` パラメータで行われる。送信を完了するために、宛先のモジュールは `PduR_<LoTp>TxConfirmation` を呼び、これは送信元の上位層のモジュールへ転送される。

マルチキャストのマルチフレームの送信は、7.1.2.3.1 章で説明される。

**[SWS_PduR_00634]** 上位層のモジュールが `PduR_<Up>Transmit` を呼んだとき、PDU Router モジュールは、宛先のトランスポートプロトコルモジュールごとに `<LoTp>_Transmit` を呼ばなければならない。（SRS_PduR_06026）

**[SWS_PduR_00299]** 宛先のトランスポートプロトコルモジュールが `PduR_<LoTp>CopyTxData` を呼んだとき、PDU Router モジュールは、送信元の上位層のモジュールの `<Up>_CopyTxData` を呼ばなければならない。（SRS_PduR_06026）

**[SWS_PduR_00676]** `<Up>_CopyTxData` の戻り値は、呼び出した宛先の下位層のトランスポートプロトコルモジュールへ転送されなければならない。（SRS_PduR_06104）

**[SWS_PduR_00301]** シングルキャストの場合、PDU Router モジュールは、下位層のトランスポートプロトコルモジュールからの確認 `PduR_<LoTp>TxConfirmation` を、`<Up>_TpTxConfirmation` を使って、上位層のモジュールへ転送しなければならない。（SRS_PduR_06026）

**[SWS_PduR_00432]** シングルキャストの場合で、`<Lo>_Transmit` を呼んだ後、PDU Router モジュールは、送信元の上位層のモジュールからの `PduR_<Up>Transmit` の呼び出しへ、同じ戻り値を返さなければならない。（SRS_PduR_06104）

###### 7.1.2.3.1 マルチキャストの送信

この項には、トランスポートプロトコルモジュールを使った I-PDU のマルチキャストの送信に特有の要求が含まれる。

1:n（n>1）の経路付けは PDU Router モジュールで行われるため、PDU Router モジュールは、送信元の上位層のモジュールから、同じデータを複数回要求しなければならない。また、マルチキャストの確認は、特別に扱わなければならない。

上位層は、同じデータを複数回コピーしなければならないため、PDU Router は、同じデータを複数回問い合わせるために、`RetryInfoPtr` [4] を使う。`RetryInfoPtr` は、`TpDataState` という状態の型を含む。

**[SWS_PduR_00631]** 送信の各位置について、最初の宛先の下位層のモジュールの `PduR_<LoTP>CopyTxData` の要求は、`TpDataState` を `TP_CONFPENDING` に設定して転送されなければならない。（SRS_PduR_06029）

**[SWS_PduR_00632]** 後続の `PduR_<LoTp>CopyTxData` の要求はすべて、同じデータをコピーできるように、`TP_DATARETRY` で転送されなければならない。（SRS_PduR_06029）

**[SWS_PDUR_00812]** すべてのトランスポートプロトコルがデータを受け取った後、PDU Router モジュールは、上位層のモジュールへデータを確認してよい。（SRS_PduR_06029）

**[SWS_PduR_00765]** マルチキャストの送信の場合、PDU Router モジュールは、下位層のトランスポートプロトコルモジュールからの最後の `PduR_<LoTp>TxConfirmation` を受信した後に、`<Up>_TpTxConfirmation` を使って、上位層のモジュールを呼ばなければならない。`result` パラメータは、少なくとも 1 つの `PduR_<LoTp>TxConfirmation` が `E_OK` を報告した場合、`E_OK` でなければならない。（SRS_PduR_06029、SRS_PduR_06125）

###### 7.1.2.3.2 エラー処理

PDU Router モジュールは、起きたエラーについて、特定の処置を取らず、エラーは、送信元の上位層のモジュールへ転送される。適切なエラー処理は、上位層のモジュールの責任である。

###### 7.1.2.3.3 長さが不明な I-PDU の扱い

PduR は、TP API を使って、長さが不明な I-PDU（すなわち、ストリーミング型のデータ）を扱える。不明な長さの定義は、`TpSduLength=0` で示される。

**[SWS_PDUR_00822]** ローカルの送信の状況で、`PduR_<Up>Transmit` が `PduInfoType.SduLength=0` で呼ばれ、I-PDU が TP モジュールへ経路付けされる場合、PduR は、すべての宛先の TP モジュールへ、`PduInfoType.SduLength=0` で `<LoTp>_Transmit` を呼ばなければならない。

#### 7.1.3 I-PDU ゲートウェイ

PDU Router モジュールは、I-PDU を、1 つの送信元のバスから、1 つ以上の宛先のバスへゲートウェイすることをサポートする。ローカルのモジュールからの、またはローカルのモジュールへの送受信との違いは、PDU Router モジュールが、同時に受信側であり送信側でなければならず、場合によっては、I-PDU のバッファリングも提供することである。

ゲートウェイの要求は、ゲートウェイが不要な場合に、PDU Router モジュールを効率的に実装できるよう、意図的に分けられている。PDU Router モジュールが I-PDU のゲートウェイを許す場合、これらの要求は、前の要求を置き換えるのではなく、追加のものとみなされる。

次の一覧は、I-PDU ゲートウェイの機能の概要を示す。

- I-PDU は、送信元の通信インタフェースモジュールから、1 つ（1:1）、または複数の宛先の通信インタフェースモジュールへ、ゲートウェイされうる（1:n の I-PDU ゲートウェイ）。
  - 各宛先について、PDU Router モジュールは、I-PDU の各宛先を、設定可能な深さでバッファリングしてよい（すなわち、複数の I-PDU がある場合は FIFO）。
  - I-PDU は、上位層のモジュールが受信すると同時に、n 個の宛先の通信インタフェースへゲートウェイされうる。
- TP で運ばれる I-PDU は、次の範囲で、1 つ以上の宛先の TP モジュールへゲートウェイされうる。
  - シングルフレームもマルチフレームも、複数の宛先の TP モジュール、またはローカルのモジュール（例: DCM）へゲートウェイされうる。
  - 複数の N-PDU で運ばれる I-PDU は、1 つの宛先へ「オンザフライ」でゲートウェイされうる。すなわち、宛先の TP モジュールで送信を始める前に、I-PDU 全体を受信している必要はない。
  - 複数の N-PDU で運ばれる I-PDU は、別の TP モジュールへゲートウェイされるか、ローカルのモジュールで受信されうるが、両方はできない。
  - TP モジュールで運ばれる I-PDU は、FIFO でバッファリングされうる。これは、SF と、マルチフレームの I-PDU の両方に適用される。
- I-PDU は、通信インタフェースモジュールの間、または TP モジュールの間でのみ、ゲートウェイできる。両者の混在は不可。例えば、I-PDU を CanIf から受信して LinTp へゲートウェイすることはできない。

これは、PDU Router モジュールが、1 つの下位層のモジュール（送信元のネットワーク）から受信した I-PDU を、与えられた I-PDU ID で識別される下位層のモジュール（宛先のネットワーク）へ転送しなければならないことを意味する。この節では、設定可能な API について、「Src」と「Dst」が使われることに注意。これは、どの呼び出しが、送信元のモジュールと宛先のモジュールのどちらに属するかを明確にするためだけである。

**[SWS_PduR_00638]** I-PDU は、通信インタフェースの間、または TP の間でのみ、ゲートウェイしてよい。両者の混在は不可。

例: FrIf から受信した I-PDU は、CanTp へゲートウェイしてはならない。（SRS_PduR_06012、SRS_PduR_06026）

**[SWS_PDUR_00825]** I-PDU を n:1 の形でゲートウェイできなければならない。

**[SWS_PDUR_00826]** n:1 のゲートウェイを使う場合、PduR は、受信した I-PDU の順序が、宛先で保たれることを保証しなければならない。

**[SWS_PDUR_00827]** I-PDU を n:1 の形で転送（フォワード）できなければならない。同時に有効にできる送信元は 1 つだけで、それぞれ `PduR_EnableRouting` と `PduR_DisableRouting` を使う。起動時にこの条件を保証するために、`PduRIsEnabledAtInit` を使わなければならない。

注: n:1 の形での、転送とゲートウェイの組み合わせは、サポートされない。

**[SWS_PDUR_00829]** ゲートウェイの状況で、メタデータが含まれ、バッファリングが必要な場合、PduR は、I-PDU に加えて、メタデータもバッファリングしなければならない。

##### 7.1.3.1 通信インタフェースモジュール

I-PDU は、1 つの通信インタフェースモジュールで受信され、ローカルのモジュールを含む n 個の通信インタフェースモジュールへゲートウェイされるようにコンフィギュレーションできる。すなわち、1:n のゲートウェイである。ゲートウェイでは、宛先の通信インタフェースモジュールごとに FIFO をコンフィギュレーションすることもできる（ただし、ローカルのモジュールには不可）。

**[SWS_PduR_00436]** PDU Router モジュールは、送信元の通信インタフェースモジュールと、1 つ以上の宛先の通信インタフェースモジュールの間での、I-PDU の経路付けをサポートしなければならない（1:n のゲートウェイ）。（SRS_PduR_06012、SRS_PduR_06030）

**[SWS_PduR_00437]** PDU Router モジュールは、通信インタフェースモジュールの間での、（PDU Router によるレート生成なしの）即時の送信による I-PDU の経路付けをサポートしなければならない。（SRS_PduR_06012）

異なる周期やレートの通信インタフェースモジュールの間での I-PDU の経路付け（レート変換）は、サポートされない。これは、COM モジュールの信号ゲートウェイを使って行える。この場合、I-PDU は COM モジュールへ経路付けされなければならない。

I-PDU のゲートウェイには、宛先のインタフェースモジュールに応じて、2 つの種類がある。使われる種類は、コンフィギュレーションで制御される。

- **[SWS_PduR_00303]** 直接のデータ提供: 宛先の I-PDU の `PduRDestPduDataProvision` が `PDUR_DIRECT` にコンフィギュレーションされる。`<DstLo>_Transmit` が呼ばれたとき、`<DstLo>` モジュールがデータをコピーし、PDU Router は、送信した I-PDU を、それ以上バッファリングしない。（SRS_PduR_06032）
- **[SWS_PduR_00306]** トリガ送信によるデータ提供: 宛先の I-PDU の `PduRDestPduDataProvision` が `PDUR_TRIGGERTRANSMIT` にコンフィギュレーションされる。`<DstLo>_Transmit` が呼ばれたとき、`<DstLo>` モジュールはデータをコピーせず、PDU Router モジュールは、I-PDU をバッファリングし、`<DstLo>` モジュールからの `PduR_<DstLo>TriggerTransmit` の呼び出しを待たなければならない。（SRS_PduR_06032）

**[SWS_PDUR_00809]** last-is-best のバッファリングの場合（`PduRQueueDepth` を参照）、PduR は、トリガ送信によるデータ提供の場合に、最新の I-PDU をバッファリングしなければならない。（SRS_PduR_06032）

トリガ送信によるデータ提供のために保存しなければならない理由は、宛先の通信インタフェースが、スケジュールに従って I-PDU を送信することがあるためである。この場合、通信インタフェースは、先行する `<DstLo>_Transmit` の呼び出しなしに、`PduR_<DstLo>TriggerTransmit` を呼ぶ。

**[SWS_PduR_00662]** FIFO バッファを使う場合（`PduRQueueDepth` を参照）、宛先の通信インタフェースモジュールが `PduR_<DstLo>TriggerTransmit` を使って I-PDU のバッファを要求し、FIFO が空の場合、戻り値 `E_NOT_OK` が使われなければならない。（SRS_PduR_06032、SRS_PduR_06105）

I-PDU のゲートウェイでは、`PduR_<Lo>TxConfirmation` は重要ではないことに注意（直接のデータ提供の PDU の FIFO の場合を除く）。

**[SWS_PduR_00640]** 宛先の通信インタフェースモジュールが、`PduR_<DstLo>TxConfirmation` を使って、I-PDU の（成功または失敗した）送信を確認し、宛先が FIFO バッファ付きの直接のデータ提供の PDU でない場合、PDU Router モジュールは、何もしてはならない。（SRS_PduR_06012、SRS_PduR_06032、SRS_PduR_06105）

ゲートウェイされる I-PDU の長さが異なりうる。したがって:

**[SWS_PduR_00783]** I-PDU が PDU Router でバッファリングされずにゲートウェイされる場合、PDU Router は、長さを検査せずに転送し、宛先のモジュールの送信関数を呼ぶだけでなければならない。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PduR_00746]** I-PDU が PDU Router モジュールにバッファリングされる場合: PDU Router モジュールは、I-PDU のデータを、次の値のうち最小のものまでコピーしなければならない。

- 受信したデータ長（受信した I-PDU の `PduLength`）
- バッファのコンフィギュレーションされた最大の `PduLength`（`PduRPduMaxLength`）。この場合、受信した I-PDU の残りは捨てられなければならない。

（SRS_PduR_06012、SRS_PduR_06032）

注: [SWS_PduR_00746] は、`PduR_<Lo>TriggerTransmit` が呼ばれるときの、バッファのオーバーランを避ける可能性をユーザに与える。ユーザは、`PduRMaxPduLength` を、宛先の I-PDU の長さより大きくならないようにコンフィギュレーションする必要がある。

**[SWS_PduR_00784]** I-PDU が、PduR のバッファから宛先のモジュールへ（直接のデータ提供で）送信されるとき、PduR は、バッファへコピーされたバイト数を `SduLength` として渡さなければならない。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PDUR_00819]** I-PDU が、PduR のバッファから宛先のモジュールへ（トリガ送信によるデータ提供で）コピーされるとき、PduR は、`SduLength` として渡された下位層のバッファサイズを検査しなければならない。バッファが、保存された PDU のデータに対して小さすぎる場合、PduR は `E_NOT_OK` を返し、`TriggerTransmit` の呼び出しを、それ以上処理してはならない。

注: [SWS_PDUR_00819] で定義された `TriggerTransmit` の呼び出しを処理しないことは、PDU を PduR のバッファから取り除いてはならないことを意味する。

###### 7.1.3.1.1 エラー処理

PDU Router モジュールは、インタフェースモジュールが、ゲートウェイの操作に属する送信要求を拒否した場合、I-PDU のインスタンスについて、いかなるエラー処理も行ってはならない。

**[SWS_PduR_00256]** PDU Router モジュールは、宛先の通信インタフェースモジュールが `<DstLo>_Transmit` を呼んだ後に `E_NOT_OK` を返した場合、送信を再試行してはならない。（SRS_PduR_06012、SRS_PduR_06105）

ここでは、宛先が何らかの理由で `E_NOT_OK` を返しており、このエラーも報告する。PDU Router モジュールは、I-PDU を破棄する以外に、何もできない。

##### 7.1.3.2 トランスポートプロトコル

送信元のトランスポートプロトコルから、1 つ以上の宛先のトランスポートプロトコルモジュールへの I-PDU のゲートウェイは、完全な I-PDU として直接ゲートウェイされる（I-PDU を構成する N-PDU の完全な集合を、送信する前に受信する）か、設定されたバイト数（しきい値）を送信前に受信する、断片化した I-PDU（オンザフライの経路付け）としてゲートウェイされる。

一般に、PDU Router は、ペイロードだけをゲートウェイし、SF、FF、CF、PCI などのトランスポートプロトコルの詳細を意識しない。ただし、PduR は、グローバルな PDU の `MetaDataType` でコンフィギュレーションされた、メタデータを持つ I-PDU のゲートウェイもサポートしなければならない。この種の I-PDU は、インタフェースの経路付けや転送では、特別な扱いを必要としないが、TP の経路付けでは、追加の情報を別に転送する必要がある。次の要求は、直接のゲートウェイとオンザフライのゲートウェイの両方に関係する。

**[SWS_PduR_00794]** `PduR_<SrcLoTp>StartOfReception` で提供された I-PDU のメタデータは、保存され、I-PDU とともに `<DstLoTp>_Transmit` へ提供されなければならない。（SRS_PduR_06026、SRS_PduR_06124）

トランスポートプロトコルモジュールでは、I-PDU は、複数の N-PDU（FF と CF）、または 1 つの N-PDU（SF）で運ばれうる。使用例の 1 つは、複数の N-PDU で運ばれる I-PDU がマルチキャストではなく（すなわち、物理アドレッシング）、1 つの N-PDU ではマルチキャスト（すなわち、機能アドレッシング）でありうることである。別の使用例は、マルチフレームのメッセージを、ローカルの受信側と、複数のゲートウェイの宛先へマルチキャストすることである。

例えば: CAN で受信した SF を、2 つの LIN バスへ送信する。受信した SF は最大 6 バイトのデータを運べるが、LIN の SF は最大 5 バイトのデータである。したがって、2 つの LIN バスへゲートウェイされる場合、CAN の SF は 5 バイトのデータに制限される。

トランスポートプロトコルモジュールで運ばれる I-PDU は、通信インタフェースを通して（すなわち、N-PDU を直接ゲートウェイして）、フレームごとに直接ゲートウェイされることもあることに注意。これは、PDU Router モジュールに特別な扱いを必要とせず、通信インタフェースモジュールを通したゲートウェイ（7.1.3.1 を参照）で扱える。ただし、これには、送信元と宛先のバスで、N-PDU の詰め方がまったく同じであること（例: CAN から CAN へ）が必要である。

###### 7.1.3.2.1 バッファの割り当て

PduR は、トランスポートプロトコルの PDU のゲートウェイに、2 種類の異なるバッファを使う。`PduRRoutingPaths` でコンフィギュレーションされる、大きな TP バッファのプールと、`PduRDestTxBufferRef` でコンフィギュレーションされる、専用のバッファである。専用のバッファは、シングルフレームの経路付けに使われ、関係するバスシステムの、起こりうる最大のシングルフレームを含めるのに十分な大きさでなければならない。大きな `PduRTpBuffers` は、マルチフレームの経路付けに使われ、オンザフライのゲートウェイの場合は TP データの 1 ブロックを、直接のゲートウェイの場合は PDU 全体を、含めるのに十分な大きさでなければならない。シングルフレーム用の専用のバッファを持つ主な理由は、機能診断の要求、特に OBD の要求が、非常に優先度が高く、バッファの割り当て方針により遅らせてはならないためである。

**[SWS_PduR_00797]** ゲートウェイされる TP の PDU の受信が `PduR_<SrcLoTp>StartOfReception` で通知され、パラメータ `TpSduLength` で報告された SDU の全体のサイズが、`PduRDestTxBufferRef` が参照する専用のバッファの、コンフィギュレーションされた `PduRPduMaxLength` より大きくない場合、PduR は、専用のバッファを使わなければならない。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PduR_00798]** ゲートウェイされる TP の PDU の受信が `PduR_<SrcLoTp>StartOfReception` で通知され、パラメータ `TpSduLength` で報告された SDU の全体のサイズが、専用のバッファのコンフィギュレーションされた `PduRPduMaxLength` より大きい場合、PduR は、`PduRTxBuffer` から動的にバッファを割り当てなければならない。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PduR_00799]** ゲートウェイされる TP の PDU の受信のための `PduR_<SrcLoTp>StartOfReception` の呼び出しの間に、バッファを割り当てられなかった場合、PduR は、この I-PDU のそれ以上の処理を直ちに止め、`BUFREQ_E_OVFL` を返さなければならない。（SRS_PduR_06026、SRS_PduR_06124、SRS_PduR_06105）

**[SWS_PDUR_00818]** TP のゲートウェイのシナリオでは、`PduR_<LoTp>CopyTxData` の `availableDataPtr` は、PduR の（ゲートウェイの）TP バッファで利用できる、残りのバイト数を示す。

###### 7.1.3.2.2 直接のゲートウェイ

**[SWS_PduR_00551]** 結果が `E_OK` の場合、`PduR_<SrcLoTp>TpRxIndication` の中で、各宛先のトランスポートプロトコルモジュールについて、`<DstLoTp>_Transmit` が呼ばれなければならない。（SRS_PduR_06026、SRS_PduR_06029、SRS_PduR_06124）

**[SWS_PduR_00697]** シングルフレームの送信で、`PduR_<DstLoTp>CopyTxData` が `TpDataState` の `TP_CONFPENDING` または `TP_DATACONF` で呼ばれたとき、または `RetryInfoType` ポインタが NULL のとき、PDU Router は、`SduLength` バイトのデータをコピーしなければならない。十分なデータが利用できない場合、PDU Router は、データをコピーせずに `BUFREQ_E_BUSY` を返さなければならない。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PduR_00705]** シングルフレームの送信で、`PduR_<DstLoTp>CopyTxData` が `TpDataState` の `TP_DATARETRY` で呼ばれたとき、PDU Router は、現在の位置を `TxTpDataCnt` バイトだけ戻し、`SduLength` バイトのデータをコピーしなければならない。PDU Router が、要求どおりに位置を戻せない場合、現在の位置を変更せず、データもコピーせずに、`BUFREQ_E_NOT_OK` を返さなければならない。現在の位置をリセットした後、コピーに十分なデータが利用できない場合、PDU Router は、データをコピーせずに `BUFREQ_E_BUSY` を返さなければならない。（SRS_PduR_06026、SRS_PduR_06124、SRS_PduR_06105）

**[SWS_PDUR_00813]** マルチフレームの送信で、`PduR_<DstLoTp>CopyTxData` が `TpDataState` の `TP_CONFPENDING` または `TP_DATACONF` で呼ばれたとき、または `RetryInfoType` ポインタが NULL のとき、PDU Router は、`SduLength` バイトのデータをコピーしなければならない。最後の宛先のモジュールからの呼び出しだけが、バッファの位置を進めなければならない。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PDUR_00814]** 十分なデータが利用できない場合、または、前のフレームについて、他のすべての宛先のトランスポートプロトコルモジュールが `PduR_<DstLoTp>CopyTxData` を呼んでいない場合、PDU Router は、データをコピーせずに `BUFREQ_E_BUSY` を返さなければならない。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PDUR_00815]** マルチフレームの送信で、`PduR_<DstLoTp>CopyTxData` が `TpDataState` の `TP_DATARETRY` で呼ばれたとき、PDU Router は、データをコピーせずに `BUFREQ_E_NOT_OK` を返さなければならない。（SRS_PduR_06026、SRS_PduR_06124、SRS_PduR_06105）

###### 7.1.3.2.3 上位層への転送

I-PDU が、1 つ以上の宛先のトランスポートプロトコルモジュールへ（直接のトランスポートプロトコルのゲートウェイで）ゲートウェイされる場合、この I-PDU は、ローカルの上位層のモジュールにも受信されうる。

実装は、上位層への受信が成功しなかったときに、（DET を介して）報告することを選んでよい。この場合、下位層へのゲートウェイは、中止されるべきではない。

直接のゲートウェイの、上位層への受信の部分は、以下で規定される。

**[SWS_PduR_00789]** ゲートウェイの場合、PduR が下位層から正常な `RxIndication` を受信したとき、モジュールは、コンフィギュレーションされた上位層の宛先について、受信セッションを開始しなければならない。`<UpTp>_StartOfReception`、`<UpTp>_CopyRxData`、`<UpTp>_RxIndication` が、この順に呼ばれる。（SRS_PduR_06026、SRS_PduR_06029）

**[SWS_PduR_00790]** ローカルの宛先がコンフィギュレーションされた、マルチキャストの TP のゲートウェイで、`<UpTp>_StartOfReception` がエラーを返したとき、PduR は、上位層とのそれ以上のやり取りなしに、上位層の受信を止めなければならない。（SRS_PduR_06026、SRS_PduR_06029、SRS_PduR_06105）

**[SWS_PduR_00791]** `<UpTp>_StartOfReception` が `BUFREQ_OK` を返したが、利用できるバッファが、メッセージ全体を受信するには小さすぎる場合、PduR は、`result = E_NOT_OK` で `<UpTp>_RxIndication` を呼ばなければならない。（SRS_PduR_06026、SRS_PduR_06029、SRS_PduR_06105）

**[SWS_PduR_00792]** `<UpTp>_CopyRxData` がエラーを返したとき、PduR は、`result = E_NOT_OK` で `<UpTp>_RxIndication` を呼ばなければならない。（SRS_PduR_06026、SRS_PduR_06029、SRS_PduR_06105）

###### 7.1.3.2.4 オンザフライのゲートウェイ

オンザフライのゲートウェイでは、PDU Router モジュールは、特定のしきい値に達したときに、宛先のトランスポートプロトコルモジュールへの送信を開始する。

**[SWS_PduR_00708]** オンザフライのゲートウェイを使う場合、宛先のトランスポートプロトコルモジュールは 1 つだけ許される。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PduR_00317]** PDU Router モジュールは、特定の宛先について Tx のしきい値に達したらすぐに、`<DstLoTp>_Transmit` を呼んで、宛先のバスでの TP の送信を開始しなければならない。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PDUR_00811]** TP の送信が `PduR_<SrcLo>StartOfReception` を介して開始された場合、PDU Router モジュールは、`PduRTpThreshold = 0` であれば、直接 `<DstLoTp>_Transmit` を呼ばなければならない。（SRS_PduR_06124）

オンザフライのゲートウェイを使う場合、PDU Router は、すでに送信したデータの再送をサポートしてはならない。

**[SWS_PDUR_00808]** PDU Router モジュールは、`PduR_<SrcLoTp>RxIndication` の結果値が `E_OK` の場合、TP のしきい値に達していなくても、`<DstLoTp>_Transmit` を呼んで、宛先のバスでの TP の送信を開始しなければならない。（SRS_PduR_06012、SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PduR_00707]** `PduR_<DstLoTp>CopyTxData` が `TpDataState` の `TP_DATACONF` で呼ばれた場合、または `RetryInfoType` ポインタが NULL の場合、PDU Router は、`SduLength` バイトのデータをコピーしなければならない。（SRS_PduR_06026、SRS_PduR_06124）

###### 7.1.3.2.5 共通の要求

次の要求は、直接のゲートウェイと、オンザフライの経路付けのゲートウェイの両方に適用される。

**[SWS_PduR_00696]** `PduR_<DstLoTp>CopyTxData` が呼ばれ、状態が `TP_DATACONF` の場合、PDU Router は、すでにコピーしたデータを解放してよい。（SRS_PduR_06026、SRS_PduR_06124）

###### 7.1.3.2.6 すべての宛先のトランスポートプロトコルモジュールが I-PDU の送信を確認する場合

**[SWS_PduR_00637]** PDU Router モジュールが `PduR_<DstLoTp>TxConfirmation` を受信したとき、PDU Router は、この宛先の I-PDU バッファを解放しなければならない。（SRS_PduR_06026、SRS_PduR_06124）

**[SWS_PduR_00740]** トランスポートプロトコルモジュールが、長さゼロ（`PduInfoType.SduLength = 0`）で `PduR_<LoTp>CopyTxData` または `PduR_<LoTp>CopyRxData` を呼んだ場合、PDU Router モジュールは、それぞれ、現在利用できるバッファのサイズ、または現在利用できるデータのサイズを返さなければならない。（SRS_PduR_06026、SRS_PduR_06124）

###### 7.1.3.2.7 エラー処理

トランスポートプロトコルモジュールを使ってゲートウェイされる I-PDU のエラー処理は単純で、PDU Router モジュールは何もせず、トランスポートプロトコルモジュールが、通信エラーを適切に扱うことに任せる。

**[SWS_PduR_00687]** オンザフライの経路付けが使われ、`PduR_<SrcLoTp>CopyRxData` が呼ばれて、渡されたデータをバッファに格納できない場合、`BUFREQ_E_NOT_OK` が返され、I-PDU のゲートウェイの実行が止められなければならない。（SRS_PduR_06026、SRS_PduR_06124、SRS_PduR_06105）

**[SWS_PduR_00689]** `PduR_<SrcLoTp>RxIndication` の結果値が `E_OK` でない場合、PDU Router は、I-PDU のそれ以上の処理を直ちに止めなければならない。（SRS_PduR_06026、SRS_PduR_06124、SRS_PduR_06105）

注: PDU Router は、`PduR_<SrcLoTp>StartOfRecpetion` が失敗した後に、`PduR_<SrcLoTp>RxIndication` を期待してはならない。PDU Router は、`<LoTp>_Transmit` が失敗した後に、`PduR_<DstLoTp>TxConfirmation` を期待してはならない。

**[SWS_PduR_00803]** TP の間のゲートウェイの場合、1 つの宛先が失敗したとき（`Transmit` が `E_NOT_OK` を返す、または `TpTxConfirmation` がエラーで呼ばれる）、他の宛先は続けなければならない。

**[SWS_PduR_00804]** TP の間のゲートウェイの場合、すべての宛先が失敗したとき、受信側は、`CopyRxData` または `StartOfReception` の現在の呼び出しに `BUFREQ_E_NOT_OK` を返すことで、止められなければならない。

###### 7.1.3.2.8 長さが不明な I-PDU の扱い

PduR は、TP API を使って、長さが不明な I-PDU（すなわち、ストリーミング型のデータ）を扱える。不明な長さの定義は、`TpSduLength=0` で示される。

**[SWS_PDUR_00823]** ゲートウェイの状況で、`PduR_<SrcLoTp>StartOfReception` が `TpSduLength=0` で呼ばれた場合、オンザフライのゲートウェイだけがサポートされる。

##### 7.1.3.3 FIFO

1 つの送信元から、複数の宛先の下位層のモジュールへ、FIFO のキューの動作を使って、I-PDU をゲートウェイすることができる。FIFO のキューは、通信インタフェースとトランスポートプロトコルに（複数の N-PDU から成るメッセージでも）使える。

**[SWS_PduR_00785]** `PduRQueueDepth` が 1 より大きい値にコンフィギュレーションされている場合、Tx の PDU バッファは、先入れ先出し（FIFO）の動作を持たなければならない。（SRS_PduR_06012、SRS_PduR_06032）

以降の章では、「FIFO」または「FIFO のキュー」という用語を、PduR の Tx I-PDU バッファの同義語として使う。

FIFO には状態があり、これらの状態は、異なるコンテキストから呼ばれるさまざまな PduR の API を呼ぶと、変わりうる。例えば、`PduR_<SrcLo>RxIndication` の呼び出しが、`PduR_<DstLo>TxConfirmation` の呼び出しに割り込まれることがある。したがって、これらの並行する呼び出しを保護する必要がある。

直接のデータ提供の場合に FIFO を使うときは、宛先の I-PDU を、`PduR_<DstLo/DstLoTp>TxConfirmation` を呼ぶようにコンフィギュレーションしなければならない。`PduRTransmissionConfirmation` を参照。

**[SWS_PduR_00307]** 宛先の I-PDU ごとに FIFO をコンフィギュレーションできなければならない。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PduR_00793]** FIFO で直接のデータ提供が使われる場合: `PduR_<SrcLo/SrcLoTp>RxIndication` が呼ばれ、同じ PDU の最後の送信が `PduR_<DstLo/DstLoTp>TxConfirmation` でまだ確認されていないとき、PduR は、新しいデータを FIFO へ入れなければならない。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PduR_00665]** FIFO で直接のデータ提供が使われる場合: `PduR_<SrcLo/SrcLoTp>RxIndication` が呼ばれ、FIFO のキューが空で、同じ PDU に未処理の確認が無いとき、`<DstLo/DstLoTp>_Transmit` が直接呼ばれなければならない。FIFO は空のままである。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PduR_00786]** トリガ送信によるデータ提供で、`PduR_<SrcLo/SrcLo>RxIndication` が呼ばれ、FIFO のキューが空のとき、受信した I-PDU は FIFO へコピーされ、`<DstLo/DstLoTp>_Transmit` が呼ばれなければならない。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PduR_00787]** `PduR_<SrcLo/SrcLoTp>RxIndication` が呼ばれ、FIFO のキューが空でないとき、受信した I-PDU は、最新のエントリとしてコピーされなければならない。（SRS_PduR_06012、SRS_PduR_06032）

**[SWS_PduR_00667]** 直接のデータ提供で、`PduR_<DstLo/DstLoTp>TxConfirmation` が呼ばれ、FIFO のキューが空でないとき、`<DstLo/DstLoTp>_Transmit` が、FIFO の最も古い I-PDU で呼ばれなければならない。送信した I-PDU は、その後で取り除かれなければならない。（SRS_PduR_06012、SRS_PduR_06032）

###### 7.1.3.3.1 通信インタフェース

**[SWS_PduR_00666]** `PduR_<DstLo>TriggerTransmit` が呼ばれ、[SWS_PduR_00819] に従って `E_OK` を返すとき、最も古い FIFO のエントリがコピーされ、その後、取り除かれなければならない。その後、FIFO のキューが空でない場合、`<DstLo>_Transmit` が、FIFO の最も古い I-PDU で呼ばれなければならない。（SRS_PduR_06012、SRS_PduR_06032）

注: 宛先のモジュールが FrIf の場合、新しい送信は、カウンタが減らされる前に呼ばれるため、PDU の `FrIfCounterLimit` を 1 より大きくコンフィギュレーションする必要がある。LinIf にはそのような制約は無いが、FIFO のキューによる、散発的なフレームへの経路付けは、サポートされない。

###### 7.1.3.3.2 トランスポートプロトコル

**[SWS_PDUR_00830]** `PduR_<SrcLoTp>StartOfReception` で FIFO が有効な場合、PduR は、`PduRTxBuffer` から（1:n の場合は各宛先について）十分なバッファを確保しなければならない。

**[SWS_PDUR_00831]** PduR は、`PduRTpThreshold` に達したとき、または I-PDU 全体を受信したとき（`PduR_<DstLoTp>RxIndication` が呼ばれたとき）のいずれかで、宛先のトランスポートプロトコルでの送信を開始しなければならない。

**[SWS_PDUR_00832]** 同じ経路に対する別の `PduR_<SrcLoTp>StartOfReception` が呼ばれた場合、PduR は、I-PDU を FIFO に保存しなければならない。

**[SWS_PDUR_00833]** 宛先のトランスポートプロトコルモジュールから `PduR_<DstLoTp>TxConfirmation` を受信したとき、FIFO が空でなければ、PduR は、次の I-PDU の送信を開始しなければならない。

**[SWS_PDUR_00834]** ゲートウェイで、オンザフライの経路付けを使うことが許されなければならない。FIFO のキューが空の場合、PduR は、`PduRTpThreshold` に達するたびに、`<DstLoTp>_Transmit` を呼べる。

**[SWS_PDUR_00835]** FIFO のキューにすでに少なくとも 1 つのエントリがある場合、受信したメッセージは FIFO へ保存され、この FIFO のキューのエントリが送信の順番になったとき（すなわち、このメッセージが FIFO の先頭になったとき）に、`<DstLoTp>_Transmit` が呼ばれなければならない。

注: FIFO を使ったオンザフライのゲートウェイの効果は、TP のメッセージをゲートウェイする、より速い方法になることである。もちろん、FIFO が空でない場合、受信したメッセージは保存されなければならず、宛先の TP へ転送してはならない。

###### 7.1.3.3.3 エラー処理

**[SWS_PduR_00788]** FIFO バッファの I-PDU で呼ばれた `<DstLo>_Transmit()` が `E_NOT_OK` を返した場合、I-PDU は FIFO から取り除かれ、次の FIFO のエントリが、あれば送信されなければならない。（SRS_PduR_06012、SRS_PduR_06105）

**[SWS_PDUR_00807]** FIFO を使う経路で、`<DstLo>_Transmit()` が `E_NOT_OK` を返したとき、PDU Router は、`PDUR_E_PDU_INSTANCES_LOST` を DET モジュールへ報告しなければならない。（SRS_PduR_06012、SRS_PduR_06105）

**[SWS_PduR_00806]** 1 つの宛先が失敗したとき（`Transmit` が `E_NOT_OK` を返す）、他の宛先は続けなければならない。

**[SWS_PduR_00255]** FIFO が満杯で、新しい `PduR_<SrcLo>RxIndication` が呼ばれた場合、FIFO は破棄（フラッシュ）されなければならない。（SRS_PduR_06012、SRS_PduR_06032）

注: これは、`PduRQueueDepth` が 1 にコンフィギュレーションされ、`PduRDestPduDataProvision` が `PDUR_TRIGGERTRANSMIT` にコンフィギュレーションされている場合、新しい I-PDU が、常に次の `PduR_<Lo>TriggerTransmit` の呼び出しの中でコピーされることを意味する。これは「last is best」の動作である。

**[SWS_PduR_00670]** FIFO がフラッシュされた場合、PDU Router は、`PDUR_E_PDU_INSTANCES_LOST` を DET モジュールへ報告しなければならない。（SRS_PduR_06012、SRS_PduR_06032、SRS_PduR_06106）

**[SWS_PduR_00669]** FIFO がフラッシュされた場合、`PduR_<SrcLo>RxIndication` が渡した新しい I-PDU は、FIFO が空であるかのように扱われなければならない。（SRS_PduR_06012、SRS_PduR_06032）

FIFO のフラッシュを引き起こした新しい I-PDU は、破棄されず、処理される。

### 7.2 送信のキャンセル

上位層のモジュールは、（通信インタフェースモジュールまたはトランスポートプロトコルモジュールで運ばれる）I-PDU のキャンセルを要求してよい。PDU Router モジュールは、要求を、1 つの宛先のモジュール（シングルキャスト）、または複数の宛先のモジュール（マルチキャスト）へ転送する。

`PduR_<Up>CancelTransmit` は、通信インタフェースの I-PDU をキャンセルするため、および転送（フォワード）の場合のトランスポートプロトコルの I-PDU をキャンセルするために使われる。

送信のキャンセルはオプションで、モジュールごとにコンフィギュレーションで有効にされる。コンフィギュレーションパラメータ `PduRCancelTransmit` を参照。

転送の場合、上位層のモジュールが I-PDU のキャンセルを要求し、PDU Router は、経路に従って、要求を 1 つ以上の宛先のモジュールへ転送する。

ゲートウェイの場合、送信のキャンセルは、リソースの扱いを最適化するために、PduR のゲートウェイの部分で使える（例: 宛先がもう利用できない場合）。

**[SWS_PduR_00710]** 要求された I-PDU の経路が無効な場合、`PduR_<Up>CancelTransmit` は、それ以上の処理をせずに、直接 `E_NOT_OK` を返さなければならない。（SRS_PduR_06122、SRS_PduR_06120）

次の要求は、`PduR_<Up>CancelTransmit` が呼ばれたときの、PDU Router モジュールの動作を説明する。

**[SWS_PduR_00721]** 通信インタフェースモジュール、`PduR_<Up>CancelTransmit`、宛先が 1 つの場合。PDU Router モジュールは、I-PDU の宛先のモジュールについて `<Lo>_CancelTransmit` を呼ばなければならない。（SRS_PduR_06122）

**[SWS_PduR_00723]** 通信インタフェースモジュール、`PduR_<Up>CancelTransmit`、宛先が複数の場合。PDU Router モジュールは、I-PDU の各宛先のモジュールについて `<Lo>_CancelTransmit` を呼ばなければならない。（SRS_PduR_06122、SRS_PduR_06030）

**[SWS_PduR_00722]** トランスポートプロトコルモジュール、`PduR_<Up>CancelTransmit`、宛先が 1 つの場合。PDU Router モジュールは、I-PDU の宛先のモジュールについて `<LoTp>_CancelTransmit` を呼ばなければならない。（SRS_PduR_06122）

**[SWS_PduR_00724]** トランスポートプロトコルモジュール、`PduR_<Up>CancelTransmit`、宛先が複数の場合。PDU Router モジュールは、I-PDU の各宛先のモジュールについて `<LoTp>_CancelTransmit` を呼ばなければならない。（SRS_PduR_06122、SRS_PduR_06029）

次の要求は、`<Lo>_CancelTransmit` または `<LoTp>_CancelTransmit` の戻り値を受け取ったときの、PDU Router モジュールの動作を説明する。

**[SWS_PduR_00700]** 宛先が 1 つの場合、PDU Router モジュールは、呼び出した上位層のモジュールへ、同じ戻り値を返さなければならない。（SRS_PduR_06122、SRS_PduR_06104）

**[SWS_PduR_00701]** 宛先が複数の場合、すべての宛先のモジュールが `E_OK` を返したときに、呼び出した上位層へ `E_OK` が返されなければならない。そうでなければ、`E_NOT_OK` が返されなければならない。（SRS_PduR_06029、SRS_PduR_06030、SRS_PduR_06125、SRS_PduR_06122、SRS_PduR_06104）

### 7.3 受信のキャンセル

上位層のモジュールは、トランスポートプロトコルモジュールで運ばれる I-PDU のキャンセルを要求してよい。PDU Router モジュールは、`PduR_<Up>CancelReceive` を通して要求を受け取る。キャンセルの要求の確認は、`<LoTp>_CancelReceive` の戻り値を通して行われ、これは、`PduR_<Up>CancelReceive` の戻り値として、上位層のモジュールへ転送される。

**[SWS_PduR_00726]** 要求された I-PDU の経路が無効な場合、`PduR_<Up>CancelReceive` は、それ以上の処理をせずに、直接 `E_NOT_OK` を返さなければならない。（SRS_PduR_06122、SRS_PduR_06120）

受信側での I-PDU ID の流れは、下位のモジュールから上位のモジュールへである。ここでの流れは、上位のモジュールから下位のモジュールへである。これは、ID が、すでに（部分的に）受信した I-PDU に属し、その受信がキャンセルされるためである。

**[SWS_PduR_00736]** 呼び出しで渡された I-PDU ID は、Rx の I-PDU ID であり、したがって、PDU Router モジュールは、この I-PDU を正しく識別できなければならない。（SRS_PduR_06122）

**[SWS_PduR_00727]** `PduR_<Up>CancelReceive` が呼ばれたとき、PDU Router モジュールは、I-PDU の宛先のトランスポートプロトコルモジュールについて、`<LoTp>_CancelReceive` を呼ばなければならない。（SRS_PduR_06026、SRS_PduR_06122）

**[SWS_PduR_00732]** `<LoTp>_CancelReceive` の戻り値は、上位層のモジュールへ転送されなければならない。（SRS_PduR_06122、SRS_PduR_06105）

### 7.4 ゼロコストの動作

ゼロコストの動作は、送信元と宛先のモジュールがそれぞれ 1 つで、ソースコードの形で提供されている場合に行える最適化である（モジュールの 1 つはソースコードでなければならない。そうでなければ、PDU Router は、関数呼び出しのためのグルーコードを作成しなければならない）。例えば、COM モジュールと 1 つの CAN バスを持つ ECU では、`PduR_ComTransmit` が、PduR モジュールの中に何の論理も無く、直接 `CanIf_Transmit` を呼んでよい。PDU Router は、マクロの層になる。

この最適化は、経路がコンフィギュレーションクラス Pre-Compile の場合にだけ可能である。

**[SWS_PduR_00287]** `PduRZeroCostOperation` が真に設定され、すべての経路がコンフィギュレーションクラス Pre-Compile の場合、PDU Router の直接上または直接下のモジュールは、PduR モジュールの関数を使わずに、互いに直接呼び出してよい。（SRS_PduR_06020）

**[SWS_PduR_00619]** `PduRZeroCostOperation` が真に設定され、少なくとも 1 つの経路がコンフィギュレーションクラス Pre-Compile でない場合、PDU Router モジュールのコンフィギュレーションジェネレータは、エラーを報告しなければならない。（SRS_PduR_06020）

### 7.5 状態管理

PDU Router モジュールの状態機械は、図 4 に示されている。

**[SWS_PduR_00644]** 状態機械のインスタンスは、PDUR モジュールに 1 つだけ存在しなければならない。（SRS_BSW_00406）

**[SWS_PduR_00324]** PDU Router モジュールは、`PduR_StateType` で定義された 2 つの状態 `PDUR_UNINIT` と `PDUR_ONLINE` から成らなければならない。（SRS_BSW_00406）

**[SWS_PduR_00325]** PDU Router モジュールは、電源投入後（すなわち、`PduR_Init` 関数を呼ぶ前）、状態 `PDUR_UNINIT` でなければならない。（SRS_BSW_00406）

**[SWS_PduR_00326]** PDU Router が、関数 `PduR_Init` を介して正常に初期化されたとき、PDU Router モジュールは、状態 `PDUR_ONLINE` へ変わらなければならない。（SRS_BSW_00406）

> （図 4「PDU Router の状態」: 状態機械図。訳なし。読み取れる内容: 電源投入（Powerup）の直後は `PDUR_UNINIT`。`PduR_Init(ConfigPtr)` がエラー無しなら `PDUR_ONLINE` へ。）

**[SWS_PduR_00328]** PDU Router モジュールは、オンラインの状態（`PDUR_ONLINE`）のときにだけ、PDU Router の経路表に従って、PDU の経路付けを行わなければならない。（SRS_BSW_00406、SRS_PduR_06001）

**[SWS_PduR_00330]** PDU Router モジュールは、初期化されていない状態（`PDUR_UNINIT`）のときは、経路付けを行ってはならない。（SRS_BSW_00406、SRS_PduR_06001）

**[SWS_PduR_00645]** PDU Router モジュールは、`PduR_Init` 関数の中で、すべてのバッファを解放しなければならない。（SRS_BSW_00406）

**[SWS_PduR_00308]** 関数 `PduR_Init` は、PDU の送信バッファへ、コンフィギュレーションされたすべてのデフォルト値を初期化しなければならない。（SRS_BSW_00406、SRS_PduR_06001）

### 7.6 経路グループ

経路グループとは、実行時に無効・有効にできる I-PDU のグループである。グループは、宛先の I-PDU を含み、経路自体は含まない。これは、特定のバスについて、I-PDU を有効・無効にすることが望ましいためである。また、経路は、I-PDU を複数のバスへマルチキャストできる。

経路グループの有効化と無効化は、通常、BswM モジュールにより使われる。

**[SWS_PduR_00717]** API で使われる I-PDU ID が、すべての宛先のモジュールで無効な場合、（可能であれば）それ以上の処理をせずに `E_NOT_OK` が返されなければならない。（SRS_PduR_06120、SRS_PduR_06029、SRS_PduR_06030）

**[SWS_PduR_00715]** I-PDU の経路グループの有効化は、即時でなければならない。（SRS_PduR_06120）

例: 続いて `PduR_<Up>Transmit` を呼ぶと、この I-PDU は直ちに処理されなければならない。

**[SWS_PDUR_00805]** 経路グループが（`PduR_DisableRouting` の呼び出しで）無効にされた場合、PduR は、この経路グループについて、次の関数で直接戻らなければならない。

- `PduR_<User:Up>Transmit`
- `PduR_<User:Lo>RxIndication`
- `PduR_<User:Lo>TriggerTransmit`
- `PduR_<User:LoTp>StartOfReception`
- `PduR_<User:LoTp>CopyRxData`
- `PduR_<User:LoTp>CopyTxData`

関数の戻り値が `Std_ReturnType` の場合、`E_NOT_OK` を返さなければならない。関数の戻り値が `BufReq_ReturnType` の場合、`BUFREQ_E_NOT_OK` を返さなければならない。

注: これは、`PduR_<User:LoTp>RxIndication` に影響しない。

**[SWS_PDUR_00810]** 単一のバッファ（`PduRQueueDepth == 1`）に関連付けられた経路が停止されるとき、`PduR_DisableRouting` が `initialize` を真にして呼ばれた場合は、対応するバッファをデフォルト値に設定し、そうでなければ現在の値を保持しなければならない。（SRS_PduR_06120、SRS_PduR_06032、SRS_PduR_06124）

**[SWS_PduR_00663]** FIFO（`PduRQueueDepth > 1`）に関連付けられた経路が停止されるとき、対応する FIFO はフラッシュされなければならず、DET の報告が有効な場合、PduR は `PDUR_E_PDU_INSTANCES_LOST` を DET へ報告しなければならない。（SRS_PduR_06120、SRS_PduR_06032、SRS_PduR_06124）

例: ゲートウェイの操作が行われ、PDU Router モジュールが I-PDU をバッファリングして、宛先の通信モジュールがトリガ送信を呼ぶのを待っている場合、バッファはクリアされ、バッファが利用できないことが、宛先の通信インタフェースへ返される。

### 7.7 コンプレックスドライバとのやり取り

AUTOSAR の Com モジュールと Dcm モジュールのほかに、コンプレックスドライバ（CDD）も、PduR の上位層または下位層のモジュールになりうる。

CDD が PduR の上位層か下位層のモジュールかは、`PduRBswModules` のコンフィギュレーションの、コンフィギュレーションパラメータ `PduRUpperModule` または `PduRLowerModule` でコンフィギュレーションできる。

CDD は、`PduRBswModules` のコンフィギュレーションの、コンフィギュレーションパラメータ `PduRCommunicationInterface` と `PduRTransportProtocol` に応じて、通信インタフェースの API とトランスポートプロトコルの API のどちらも必要としうる。CDD とのやり取りのために PduR が提供する API 関数は、コンフィギュレーションパラメータ `apiServicePrefix` で指定された、CDD のサービスのプレフィックスを含む。[SWS_PduR_00504] を参照。

PduR は、上位層の CDD ごとに、一意の送信関数 `PduR_<Cdd>Transmit` を提供する。PduR のコールアウト関数が、上位層の CDD が送信または受信する Pdu について、下位層のモジュールから呼ばれたとき、PduR は、CDD の対応する対象の関数を呼ぶ。

通信インタフェースの API を必要とする下位層の CDD には、PduR は、一意の通信インタフェースの API 関数の集合 `PduR_<Cdd>RxIndication` と、（コンフィギュレーションされていれば）`PduR_<Cdd>TxConfirmation`、`PduR_<Cdd>TriggerTransmit` を提供する。8.3.3 節を参照。

トランスポートプロトコルの API を必要とする下位層の CDD には、PduR は、一意のトランスポートプロトコルの API 関数の集合 `PduR_<Cdd>CopyRxData`、`PduR_<Cdd>CopyTxData`、`PduR_<Cdd>RxIndication`、`PduR_<Cdd>StartOfReception`、`PduR_<Cdd>TxConfirmation` を提供する。8.3.4 節を参照。

PduR の API 関数が、下位層の CDD が送信または受信する Pdu について、上位層のモジュールから呼ばれたとき、PduR は、CDD の対応する対象の関数を呼ぶ。

Pdu が CDD により送信または受信されるかを判断するために、PduR は、EcuC モジュールの Pdu の一覧への参照の出所を調べる必要がある。

- 経路の送信元の Pdu が、上位層の CDD からも参照される Pdu の一覧の中の Pdu を参照している場合、その Pdu は CDD により送信される。
- 経路の宛先の Pdu が、上位層の CDD からも参照される Pdu の一覧の中の Pdu を参照している場合、その Pdu は CDD により受信される。
- 経路の送信元の Pdu が、下位層の CDD からも参照される Pdu の一覧の中の Pdu を参照している場合、その Pdu は CDD から受信される。
- 経路の宛先の Pdu が、下位層の CDD からも参照される Pdu の一覧の中の Pdu を参照している場合、その Pdu は CDD を介して送信される。

**[SWS_PduR_00504]** PduR は、GenericComServices の API（第 12 章を参照）の `<Lo>`、`<Up>`、`<LoTp>` のタグを置き換えるために、CDD のベンダ固有のモジュール定義（`EcucModuleDef` の要素）の `apiServicePrefix` 属性を使わなければならない。CDD のベンダ固有のモジュール定義は、コンフィギュレーションパラメータ `PduRBswModuleRef` を介して間接的にアクセスできる。これは、CDD の具体的なコンフィギュレーションのトップレベルの要素（すなわち、`EcucModuleConfigurationValues` の要素）を参照し、それが CDD のベンダ固有のモジュール定義（`EcucModuleDef` の要素）を参照する。（SRS_BSW_00310）

### 7.8 エラーの分類

詳細は、SWS_BSWGeneral の 7.2 章「エラーの分類」を参照。PduR は、プロダクションエラーを報告しないことに注意。

#### 7.8.1 開発エラー

**[SWS_PduR_00100]** 次の開発エラーと例外が、ビルドのバージョン（開発／製品モード）に応じて、PDU Router モジュールにより検出できなければならない。

> **注（再構成）**: 原典の表は、テキストの抽出で、エラーの種類、エラーコード、値の対応が崩れていた。AUTOSAR 4.3.1 のエラーコードの並び（値の昇順）から対応を復元した。

| エラーの種類 | 関連するエラーコード | 値 [hex] |
|---|---|---|
| 無効なコンフィギュレーションポインタ | `PDUR_E_INIT_FAILED` | 0x00 |
| モジュールの初期化なしで API サービスが使われた、または `PDUR_UNINIT` 以外の状態で `PduR_Init` が呼ばれた | `PDUR_E_UNINIT` | 0x01 |
| 無効な PDU 識別子 | `PDUR_E_PDU_ID_INVALID` | 0x02 |
| `PduR_EnableRouting` または `PduR_DisableRouting` 関数に渡された経路表が無効な場合 | `PDUR_E_ROUTING_PATH_GROUP_ID_INVALID` | 0x08 |
| 引数として NULL ポインタが渡された | `PDUR_E_PARAM_POINTER` | 0x09 |

（SRS_BSW_00337、SRS_BSW_00323、SRS_BSW_00452）

#### 7.8.1 ランタイムエラー

**[SWS_PDUR_00816]** ランタイムエラーの種類

| エラーの種類 | 関連するエラーコード | 値 [hex] |
|---|---|---|
| TP モジュールが、有効な PDU 識別子の送信要求を拒否した | `PDUR_E_TP_TX_REQ_REJECTED` | 0x03 |
| PDU インスタンスの喪失（ゲートウェイの操作でのバッファのオーバーラン） | `PDUR_E_PDU_INSTANCES_LOST` | 0x0a |

> **注（再構成）**: 原典の 7.8.1（ランタイムエラー）の節番号は、原典のまま（7.8.1 が 2 回現れる）。

#### 7.8.2 一時的な故障

一時的な故障は無い。

#### 7.8.3 プロダクションエラー

プロダクションエラーは無い。

### 7.9 エラーの検出

**[SWS_PduR_00119]** PDU Router モジュールが初期化されていない場合（状態 `PDUR_UNINIT`）、`PduRDevErrorDetect` が有効なとき、`PduR_Init` と `PduR_GetVersionInfo` 以外のすべての関数は、呼ばれたときに、DET を介してエラー `PDUR_E_UNINIT` を報告しなければならない。（SRS_BSW_00406）

**[SWS_PDUR_00824]** PduR が、開発、ランタイム、または一時的なエラーを検出したとき、Default Error Tracer モジュールを呼ぶときのインスタンス ID として、呼び出し元のモジュールのモジュール ID を使わなければならない。

注: 標準化されたモジュール ID は、Basic Software Module の一覧の文書 [9] にある。パラメータ `PduRBswModuleRef` が、使われるモジュールを識別する。この情報により、`BswModuleDescription.moduleId` からモジュール ID を取得できる。

### 7.10 API パラメータの検査

**[SWS_PduR_00221]** 開発エラーの検出が有効で、PDU 識別子が、指定された範囲に無く、その PDU 識別子が、PDU Router モジュールにより使われるようにコンフィギュレーションされている場合、PDU Router モジュールは、`PduRDevErrorDetect` が有効なとき、エラー `PDUR_E_PDU_ID_INVALID` を DET モジュールへ報告しなければならない。（SRS_PduR_06103、SRS_BSW_00323）

### 7.11 デバッグ

詳細は、SWS_BSWGeneral の 7.1.17 章「デバッグのサポート」を参照。

## 8 API 仕様

この項では、PDU Router モジュールの API を規定する。

### 8.1 インポートする型

この章では、次のファイルから取り込まれるすべての型を示す。

**[SWS_PduR_00333]**

| モジュール | インポートする型 |
|---|---|
| ComStack_Types | `BufReq_ReturnType`、`PduIdType`、`PduInfoType`、`PduLengthType`、`RetryInfoType`、`TPParameterType` |
| Std_Types | `Std_ReturnType`、`Std_VersionInfoType` |

（SRS_BSW_00301）

### 8.2 型の定義

次の PDU Router の型が規定され、`PduR_Types.h` で定義されなければならない。

#### 8.2.1 PduR_PBConfigType

ポストビルド時のコンフィギュレーションは、2 つの機能を満たす。

- ポストビルドで選択可能: ECU に複数のコンフィギュレーションがあり、PDU Router モジュールの初期化時に 1 つが選択される。
- ポストビルドでロード可能: ECU に 1 つのコンフィギュレーションがある。このコンフィギュレーションは、コンパイル時の後に、書き換えられてもよい。

基本的に、選択可能とロード可能を混ぜることに制限は無い。通常、ポストビルドでロード可能なものは、他のモジュール／アプリケーションに影響せずに書き換えられる、専用のフラッシュセクタに置かれる。

**[SWS_PduR_00743]**

| 項目 | 内容 |
|---|---|
| 名前 | `PduR_PBConfigType` |
| 型 | 構造体 |
| 範囲 | 実装固有 |
| 説明 | PDU Router のポストビルド時のコンフィギュレーションデータを含むデータ構造。 |

（SRS_BSW_00400、SRS_BSW_00438、SRS_BSW_00404、SRS_BSW_00305）

**[SWS_PduR_00241]** 型 `PduR_PBConfigType` は、PDU Router モジュールのポストビルド時のコンフィギュレーションデータを含む外部のデータ構造であり、`PduR_PBcfg.c` に実装されなければならない（5.1.1 章と 10.2 章を参照）。（SRS_BSW_00438、SRS_BSW_00404）

#### 8.2.2 PduR_PBConfigIdType

この型は、`PduR_GetConfigurationId` API が返す。

**[SWS_PduR_00771]**

| 項目 | 内容 |
|---|---|
| 名前 | `PduR_PBConfigIdType` |
| 型 | `uint16` |
| 説明 | I-PDU の経路付けに現在使われている、ポストビルドのコンフィギュレーションの識別。ECU には、複数のコンフィギュレーション（ポストビルドで選択可能）が含まれ、それぞれが一意の Id を持つことがある。 |

（SRS_BSW_00405、SRS_BSW_00305、SRS_PduR_06097）

#### 8.2.3 PduR_RoutingPathGroupIdType

経路グループ ID は、経路の宛先の特定のグループを識別するために使われる。その理由は、1:n の経路の宛先は、通常、複数のバスに属し、バスごとに経路付けを有効／無効にできなければならないためである。したがって、経路グループは、1:n の経路を、1:1 の経路に分ける。

**[SWS_PduR_00654]**

| 項目 | 内容 |
|---|---|
| 名前 | `PduR_RoutingPathGroupIdType` |
| 型 | `uint16` |
| 説明 | 経路表の識別 |

（SRS_BSW_00305、SRS_PduR_06120）

#### 8.2.4 PduR_StateType

**[SWS_PduR_00742]**

| 項目 | 内容 |
|---|---|
| 名前 | `PduR_StateType` |
| 型 | 列挙型 |
| 範囲 | `PDUR_UNINIT`: PDU Router が初期化されていない<br>`PDUR_ONLINE`: PDU Router が正常に初期化された |
| 説明 | PDU Router の状態 |

（SRS_BSW_00305、SRS_BSW_00335、SRS_BSW_00406）

### 8.3 関数の定義

#### 8.3.1 PDU Router が提供する一般の関数

##### 8.3.1.1 PduR_Init

**[SWS_PduR_00334]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_Init` |
| 構文 | `void PduR_Init(const PduR_PBConfigType* ConfigPtr)` |
| サービス ID [hex] | 0xf0 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `ConfigPtr`: ポストビルドのコンフィギュレーションへのポインタ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | PDU Router を初期化する |

（SRS_BSW_00101、SRS_BSW_00358、SRS_BSW_00414、SRS_BSW_00310）

統合上の注意: PDU Router モジュールを初期化されていない状態で呼ぶ問題を避けるため、PDU Router モジュールは、インタフェースするモジュールより前に初期化されることが重要である。

**[SWS_PduR_00709]** 初期化の後、すべての I-PDU の経路グループは、コンフィギュレーションパラメータ「起動時に有効（enable at start）」に従って有効にされなければならない。（SRS_PduR_06120）

注: NULL ポインタの検査は、文書 SWS_BSWGeneral の中で規定されている。

##### 8.3.1.2 PduR_GetVersionInfo

**[SWS_PduR_00338]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_GetVersionInfo` |
| 構文 | `void PduR_GetVersionInfo(Std_VersionInfoType* versionInfo)` |
| サービス ID [hex] | 0xf1 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | `versionInfo`: このモジュールのバージョン情報を格納する場所へのポインタ。 |
| 戻り値 | なし |
| 説明 | このモジュールのバージョン情報を返す。 |

（SRS_BSW_00407、SRS_BSW_00411、SRS_BSW_00310）

##### 8.3.1.3 PduR_GetConfigurationId

**[SWS_PduR_00341]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_GetConfigurationId` |
| 構文 | `PduR_PBConfigIdType PduR_GetConfigurationId(void)` |
| サービス ID [hex] | 0xf2 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `PduR_PBConfigIdType`: ポストビルド時のコンフィギュレーションの識別子 |
| 説明 | PDU Router のポストビルド時のコンフィギュレーションの一意の識別子を返す |

> **注（再構成）**: 原典の表は、テキストの抽出で行がずれていたため、項目の対応を復元した。

（SRS_PduR_06097、SRS_BSW_00310）

**[SWS_PduR_00280]** 関数 `PduR_GetConfigurationId` は、PDU Router モジュールのポストビルド時のコンフィギュレーションの一意の識別子を返さなければならない。（SRS_PduR_06097）

##### 8.3.1.4 PduR_EnableRouting

**[SWS_PduR_00615]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_EnableRouting` |
| 構文 | `void PduR_EnableRouting(PduR_RoutingPathGroupIdType id)` |
| サービス ID [hex] | 0xf3 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 経路グループの識別。経路グループは、PDU Router のコンフィギュレーションで定義される。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 経路グループを有効にする。 |

（SRS_PduR_06120、SRS_BSW_00310）

**[SWS_PduR_00647]** 経路グループの `id` が存在しない場合、PDU Router モジュールは、何もせずに戻らなければならない。（SRS_PduR_06120、SRS_BSW_00323）

**[SWS_PduR_00648]** 経路グループの `id` が存在せず、`PduRDevErrorDetect` が有効な場合、PDU Router モジュールは、`PDUR_E_ROUTING_PATH_GROUP_ID_INVALID` を報告しなければならない。（SRS_PduR_06120、SRS_BSW_00323）

##### 8.3.1.5 PduR_DisableRouting

**[SWS_PduR_00617]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_DisableRouting` |
| 構文 | `void PduR_DisableRouting(PduR_RoutingPathGroupIdType id, boolean initialize)` |
| サービス ID [hex] | 0xf4 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 経路グループの識別。経路グループは、PDU Router のコンフィギュレーションで定義される。<br>`initialize`: `true`: 単一のバッファをデフォルト値に初期化する。`false`: 単一のバッファの現在の値を保持する。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 経路グループを無効にする。 |

> **注（再構成）**: 原典の表は、テキストの抽出で行がずれていたため、項目の対応を復元した。

（SRS_PduR_06120、SRS_BSW_00310）

**[SWS_PduR_00716]** 経路グループの `id` が存在しない場合、PDU Router モジュールは、何もせずに戻らなければならない。（SRS_PduR_06120、SRS_BSW_00323）

**[SWS_PduR_00649]** 経路表の `id` が存在せず、`PduRDevErrorDetect` が有効な場合、PDU Router モジュールは、`PDUR_E_ROUTING_PATH_GROUP_ID_INVALID` を報告しなければならない。（SRS_PduR_06120、SRS_BSW_00323）

#### 8.3.2 上位層のモジュールとのやり取りのための、設定可能なインタフェースの定義

API の説明が汎用的なアプローチになったため、上位層の API 関数のサービス ID も汎用である。複数の上位層を区別するために、PduR は、Det の呼び出しで、上位層のモジュールのモジュール ID を、インスタンス ID の引数として使う。

##### 8.3.2.1 PduR_<User:Up>Transmit

**[SWS_PduR_00406]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:Up>Transmit` |
| 構文 | `Std_ReturnType PduR_<User:Up>Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x49 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信する PDU の識別子<br>`PduInfoPtr`: PDU のデータの長さとポインタ、およびメタデータへのポインタ。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 送信要求が受理された。<br>`E_NOT_OK`: 送信要求が受理されなかった。 |
| 説明 | PDU の送信を要求する。 |

（SRS_PduR_06012、SRS_PduR_06026、SRS_PduR_06114、SRS_PduR_06115、SRS_PduR_06116、SRS_BSW_00310）

##### 8.3.2.2 PduR_<User:Up>CancelTransmit

**[SWS_PduR_00769]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:Up>CancelTransmit` |
| 構文 | `Std_ReturnType PduR_<User:Up>CancelTransmit(PduIdType TxPduId)` |
| サービス ID [hex] | 0x4a |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: キャンセルする PDU の識別。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: キャンセルが、宛先のモジュールにより正常に実行された。<br>`E_NOT_OK`: キャンセルが、宛先のモジュールにより拒否された。 |
| 説明 | 下位層の通信モジュールで進行中の PDU の送信の、キャンセルを要求する。 |

（SRS_PduR_06122、SRS_PduR_06114、SRS_PduR_06115、SRS_PduR_06116、SRS_BSW_00310）

##### 8.3.2.3 PduR_<User:Up>ChangeParameter（廃止）

**[SWS_PduR_00482]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:Up>ChangeParameter`（廃止） |
| 構文 | `Std_ReturnType PduR_<User:Up>ChangeParameter(PduIdType id, TPParameterType parameter, uint16 value)` |
| サービス ID [hex] | 0x4b |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `id`: パラメータの変更の対象となる PDU の識別。<br>`parameter`: 変更するパラメータの ID。<br>`value`: パラメータの新しい値。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: パラメータが正常に変更された。<br>`E_NOT_OK`: パラメータの変更が拒否された。 |
| 説明 | 特定のトランスポートプロトコルのパラメータ（例: ブロックサイズ）の変更を要求する。タグ: `atp.Status=obsolete`（廃止） |

（SRS_PduR_06026、SRS_PduR_06114、SRS_PduR_06115、SRS_PduR_06116、SRS_BSW_00310）

##### 8.3.2.4 PduR_<User:Up>CancelReceive

**[SWS_PduR_00767]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:Up>CancelReceive` |
| 構文 | `Std_ReturnType PduR_<User:Up>CancelReceive(PduIdType RxPduId)` |
| サービス ID [hex] | 0x4c |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `RxPduId`: キャンセルする PDU の識別。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: キャンセルが、宛先のモジュールにより正常に実行された。<br>`E_NOT_OK`: キャンセルが、宛先のモジュールにより拒否された。 |
| 説明 | 下位層のトランスポートプロトコルのモジュールで進行中の PDU の受信の、キャンセルを要求する。 |

（SRS_PduR_06026、SRS_PduR_06114、SRS_PduR_06115、SRS_PduR_06116、SRS_BSW_00310）

#### 8.3.3 下位層の通信インタフェースモジュールとのやり取りのための、設定可能なインタフェースの定義

API の説明が汎用的なアプローチになったため、下位層の API 関数のサービス ID も汎用である。複数の下位層を区別するために、PduR は、Det の呼び出しで、下位層のモジュールのモジュール ID を、インスタンス ID の引数として使う。

##### 8.3.3.1 PduR_<User:Lo>RxIndication

**[SWS_PduR_00362]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:Lo>RxIndication` |
| 構文 | `void PduR_<User:Lo>RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x42 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `RxPduId`: 受信した PDU の ID。<br>`PduInfoPtr`: 受信した PDU の長さ（`SduLength`）、PDU を含むバッファへのポインタ（`SduDataPtr`）、およびこの PDU に関するメタデータを含む。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |

（SRS_PduR_06012、SRS_PduR_06116、SRS_PduR_06117、SRS_PduR_06123、SRS_BSW_00310）

##### 8.3.3.2 PduR_<User:Lo>TxConfirmation

**[SWS_PduR_00365]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:Lo>TxConfirmation` |
| 構文 | `void PduR_<User:Lo>TxConfirmation(PduIdType TxPduId, Std_ReturnType result)` |
| サービス ID [hex] | 0x40 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信された PDU の ID。<br>`result`: `E_OK`: PDU が送信された。`E_NOT_OK`: PDU の送信に失敗した。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |

（SRS_PduR_06012、SRS_PduR_06116、SRS_PduR_06117、SRS_PduR_06123、SRS_BSW_00310）

##### 8.3.3.3 PduR_<User:Lo>TriggerTransmit

**[SWS_PduR_00369]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:Lo>TriggerTransmit` |
| 構文 | `Std_ReturnType PduR_<User:Lo>TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x41 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信が要求される SDU の ID。 |
| 入出力パラメータ | `PduInfoPtr`: SDU のデータのコピー先のバッファへのポインタ（`SduDataPtr`）と、`SduLength` に利用できるバッファサイズを含む。戻るとき、サービスは、コピーした SDU のデータの長さを `SduLength` に示す。 |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: SDU がコピーされ、`SduLength` がコピーされたバイト数を示す。<br>`E_NOT_OK`: SDU のデータはコピーされていない。`PduInfoPtr` は、NULL ポインタや無効なデータを指している可能性があるため、使ってはならない。 |
| 説明 | この API の中で、上位層のモジュール（呼び出されるモジュール）は、利用できるデータが、`PduInfoPtr->SduLength` で報告されたバッファサイズに収まるかを検査しなければならない。収まる場合は、データを `PduInfoPtr->SduDataPtr` が与えるバッファへコピーし、実際にコピーしたデータの長さを `PduInfoPtr->SduLength` に更新しなければならない。収まらない場合は、`PduInfoPtr` を変更せずに `E_NOT_OK` を返す。 |

（SRS_PduR_06012、SRS_PduR_06032、SRS_PduR_06116、SRS_PduR_06117、SRS_PduR_06123、SRS_BSW_00310）

#### 8.3.4 下位層のトランスポートプロトコルモジュールとのやり取りのための、設定可能なインタフェースの定義

API の説明が汎用的なアプローチになったため、下位層のトランスポートプロトコルの API 関数のサービス ID も汎用である。複数の下位層を区別するために、PduR は、Det の呼び出しで、下位層のモジュールのモジュール ID を、インスタンス ID の引数として使う。

##### 8.3.4.1 PduR_<User:LoTp>CopyRxData

**[SWS_PduR_00512]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:LoTp>CopyRxData` |
| 構文 | `BufReq_ReturnType PduR_<User:LoTp>CopyRxData(PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr)` |
| サービス ID [hex] | 0x44 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 受信した I-PDU の識別。<br>`info`: コピー元のバッファ（`SduDataPtr`）と、コピーするバイト数（`SduLength`）を与える。`SduLength` が 0 のとき、上位層のモジュールで現在利用できるバッファの量を問い合わせるために使える。この場合、`SduDataPtr` は `NULL_PTR` でもよい。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `bufferSizePtr`: データがコピーされた後に利用できる受信バッファ。 |
| 戻り値 | `BufReq_ReturnType`<br>`BUFREQ_OK`: データが正常にコピーされた。<br>`BUFREQ_E_NOT_OK`: エラーが起きたため、データはコピーされなかった。 |
| 説明 | この関数は、I-PDU のセグメント（N-PDU）の受信データを、上位層へ渡すために呼ばれる。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。残りのバッファのサイズは、`bufferSizePtr` が示す位置に書かれる。 |

（SRS_PduR_06026、SRS_PduR_06121、SRS_BSW_00310）

##### 8.3.4.2 PduR_<User:LoTp>RxIndication

**[SWS_PduR_00375]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:LoTp>RxIndication` |
| 構文 | `void PduR_<User:LoTp>RxIndication(PduIdType id, Std_ReturnType result)` |
| サービス ID [hex] | 0x45 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 受信した I-PDU の識別。<br>`result`: 受信の結果。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | TP API を介して I-PDU が受信された後に呼ばれる。結果が、送信が成功したかを示す。 |

（SRS_PduR_06026、SRS_PduR_06121、SRS_BSW_00310）

##### 8.3.4.3 PduR_<User:LoTp>StartOfReception

**[SWS_PduR_00507]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:LoTp>StartOfReception` |
| 構文 | `BufReq_ReturnType PduR_<User:LoTp>StartOfReception(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength, PduLengthType* bufferSizePtr)` |
| サービス ID [hex] | 0x46 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: I-PDU の識別。<br>`info`: トランスポートプロトコルの I-PDU の受信の、最初のフレームまたはシングルフレームの、ペイロードデータ（プロトコル情報を除く）とペイロード長、およびこの PDU に関するメタデータを含む、`PduInfoType` 構造体へのポインタ。最初／シングルフレームのデータもメタデータも利用できない場合、このパラメータは `NULL_PTR` に設定される。<br>`TpSduLength`: 受信する N-SDU の全長。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `bufferSizePtr`: 受信側のモジュールで利用できる受信バッファ。このパラメータは、トランスポートプロトコルのモジュールで、ブロックサイズ（BS）を計算するために使われる。 |
| 戻り値 | `BufReq_ReturnType`<br>`BUFREQ_OK`: 接続が受理された。`bufferSizePtr` が利用できる受信バッファを示し、受信が続けられる。要求されたサイズのバッファが利用できない場合、`bufferSizePtr` で受信バッファサイズ 0 が示されなければならない。<br>`BUFREQ_E_NOT_OK`: 接続が拒否された。受信は中止される。`bufferSizePtr` は変更されない。<br>`BUFREQ_E_OVFL`: 必要な長さのバッファを提供できない。受信は中止される。`bufferSizePtr` は変更されない。 |
| 説明 | この関数は、N-SDU の受信の開始時に呼ばれる。N-SDU は、複数の N-PDU に分割される場合（FF と、それに続く 1 つ以上の CF）と、1 つの N-PDU から成る場合（SF）がある。`TpSduLength` が 0 で呼ばれたとき、このサービスは、現在利用できる最大のバッファサイズを提供しなければならない。 |

（SRS_PduR_06026、SRS_PduR_06121、SRS_BSW_00310）

##### 8.3.4.4 PduR_<User:LoTp>CopyTxData

**[SWS_PduR_00518]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:LoTp>CopyTxData` |
| 構文 | `BufReq_ReturnType PduR_<User:LoTp>CopyTxData(PduIdType id, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr)` |
| サービス ID [hex] | 0x43 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 送信された I-PDU の識別。<br>`info`: コピー先のバッファ（`SduDataPtr`）と、コピーするバイト数（`SduLength`）を与える。十分な送信データが利用できない場合、上位層のモジュールはデータをコピーせず、`BUFREQ_E_BUSY` が返される。下位層のモジュールは、呼び出しを再試行してもよい。`SduLength` が 0 のとき、`retry` パラメータの状態の変化を示す、または上位層のモジュールで現在利用できるデータの量を問い合わせるために使える。この場合、`SduDataPtr` は `NULL_PTR` でもよい。<br>`retry`: このパラメータは、送信されたデータを確認する、または送信の問題の後にデータを再送するために使われる。`retry` パラメータが `NULL_PTR` の場合、送信データは、コピーされた後、直ちにバッファから取り除いてよいことを示す。そうでない場合、`retry` パラメータは、有効な `RetryInfoType` の要素を指さなければならない。`TpDataState` が `TP_CONFPENDING` を示す場合、以前にコピーされたデータは、エラーからの回復のために利用できるよう、TP バッファに残らなければならない。`TP_DATACONF` は、この呼び出しの前にコピーされたすべてのデータが確認され、TP バッファから取り除いてよいことを示す。この API の呼び出しでコピーされたデータは除かれ、後で確認される。`TP_DATARETRY` は、この API の呼び出しが、エラーから回復するために、以前にコピーしたデータをコピーすることを示す。この場合、`TxTpDataCnt` は、現在のデータのコピー位置からのオフセット（バイト単位）を指定する。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `availableDataPtr`: 上位層のモジュールの Tx バッファで利用できる、残りのバイト数を示す。`availableDataPtr` は、動的なペイロード長をサポートする TP モジュール（例: FrIsoTp）が、後続の CF のサイズを決めるために使える。 |
| 戻り値 | `BufReq_ReturnType`<br>`BUFREQ_OK`: データが、要求されたとおり完全に送信バッファへコピーされた。<br>`BUFREQ_E_BUSY`: 必要な量の Tx データが利用できないため、要求を満たせなかった。下位層のモジュールは、この呼び出しを後で再試行してもよい。データはコピーされていない。<br>`BUFREQ_E_NOT_OK`: データはコピーされていない。要求が失敗した。 |
| 説明 | この関数は、I-PDU のセグメント（N-PDU）の送信データを取得するために呼ばれる。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。ただし `retry->TpDataState` が `TP_DATARETRY` の場合を除く。この場合、関数は、`retry->TxTpDataCnt` が示す現在の位置からのオフセットで始まるデータのコピーをやり直す。残りのデータのサイズは、`availableDataPtr` が示す位置に書かれる。 |

（SRS_PduR_06026、SRS_PduR_06121、SRS_BSW_00310）

##### 8.3.4.5 PduR_<User:LoTp>TxConfirmation

**[SWS_PduR_00381]**

| 項目 | 内容 |
|---|---|
| サービス名 | `PduR_<User:LoTp>TxConfirmation` |
| 構文 | `void PduR_<User:LoTp>TxConfirmation(PduIdType id, Std_ReturnType result)` |
| サービス ID [hex] | 0x48 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 送信された I-PDU の識別。<br>`result`: I-PDU の送信の結果。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | この関数は、I-PDU がそのネットワークで送信された後に呼ばれる。結果が、送信が成功したかを示す。 |

（SRS_PduR_06026、SRS_PduR_06121、SRS_BSW_00310）

### 8.4 スケジュールされる関数

PDU Router の操作は、すべて隣接する通信モジュールにより起動されるため、PDU Router は、スケジュールされる関数を必要としない。

### 8.5 期待するインタフェース

この章では、他のモジュールに要求されるすべてのインタフェースを列挙する。

PDU Router モジュールは、異なる上位・下位のモジュールとインタフェースできる、汎用のモジュールとしてモデル化されている。この汎用のアプローチをモデル化するために取られた方法は、GenericComServices という仮想のモジュールを持つことである。この仮想のモジュールは、PDU Router が上位層または下位層のモジュールで呼ぶ API の集合を含む。これらの API は、`<Lo>`、`<Up>`、`<LoTp>` のタグを含み、これがインタフェースするモジュールに置き換えられるという意味で、汎用である。タグは、`PduRBswModuleRef` 参照パラメータを使った、`PduRBSWModules` コンテナのコンフィギュレーションで設定される。

#### 8.5.1 必須のインタフェース

Pdu Router は、必須のインタフェースを必要としない。必要な API 関数は、コンフィギュレーションに依存する。

**[SWS_PduR_91001]**

| API 関数 | 説明 |
|---|---|
| `Det_ReportRuntimeError` | ランタイムエラーを報告するサービス。コールアウトがコンフィギュレーションされていれば、このコールアウトが呼ばれなければならない。 |

（SRS_BSW_00384）

#### 8.5.2 オプションのインタフェース

この章では、モジュールのオプションの機能を満たすために必要な、すべてのインタフェースを定義する。

**[SWS_PduR_00424]**

> **注（再構成）**: 原典の表は、テキストの抽出で、API 関数の名前と説明の列が分かれて並んでいた。API 関数の並びと説明の並びの順序から対応を復元した。

| API 関数 | 説明 |
|---|---|
| （廃止）`<Provider:LoTp>_ChangeParameter` | 特定のトランスポートプロトコルのパラメータ（例: ブロックサイズ）の変更を要求する。 |
| `<Provider:Lo>_CancelTransmit` | 下位層の通信モジュールで進行中の PDU の送信の、キャンセルを要求する。 |
| `<Provider:Lo>_Transmit` | PDU の送信を要求する。 |
| `<Provider:LoTp>_CancelReceive` | 下位層のトランスポートプロトコルのモジュールで進行中の PDU の受信の、キャンセルを要求する。 |
| `<Provider:LoTp>_CancelTransmit` | 下位層の通信モジュールで進行中の PDU の送信の、キャンセルを要求する。 |
| `<Provider:LoTp>_Transmit` | PDU の送信を要求する。 |
| `<Provider:Up>_RxIndication` | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |
| `<Provider:Up>_TriggerTransmit` | この API の中で、上位層のモジュール（呼び出されるモジュール）は、利用できるデータが、`PduInfoPtr->SduLength` で報告されたバッファサイズに収まるかを検査しなければならない。収まる場合は、データを `PduInfoPtr->SduDataPtr` が与えるバッファへコピーし、実際にコピーしたデータの長さを `PduInfoPtr->SduLength` に更新しなければならない。収まらない場合は、`PduInfoPtr` を変更せずに `E_NOT_OK` を返す。 |
| `<Provider:Up>_TxConfirmation` | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |
| `<Provider:UpTp>_CopyRxData` | I-PDU のセグメント（N-PDU）の受信データを、上位層へ渡すために呼ばれる。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。残りのバッファのサイズは、`bufferSizePtr` が示す位置に書かれる。 |
| `<Provider:UpTp>_CopyTxData` | I-PDU のセグメント（N-PDU）の送信データを取得するために呼ばれる。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。ただし `retry->TpDataState` が `TP_DATARETRY` の場合を除く。この場合、関数は、`retry->TxTpDataCnt` が示す現在の位置からのオフセットで始まるデータのコピーをやり直す。残りのデータのサイズは、`availableDataPtr` が示す位置に書かれる。 |
| `<Provider:UpTp>_StartOfReception` | N-SDU の受信の開始時に呼ばれる。N-SDU は、複数の N-PDU に分割される場合（FF と、それに続く 1 つ以上の CF）と、1 つの N-PDU から成る場合（SF）がある。`TpSduLength` が 0 で呼ばれたとき、このサービスは、現在利用できる最大のバッファサイズを提供しなければならない。 |
| `<Provider:UpTp>_TpRxIndication` | TP API を介して I-PDU が受信された後に呼ばれる。結果が、送信が成功したかを示す。 |
| `<Provider:UpTp>_TpTxConfirmation` | I-PDU がそのネットワークで送信された後に呼ばれる。結果が、送信が成功したかを示す。 |
| `Det_ReportError` | 開発エラーを報告するサービス。 |

（SRS_BSW_00384）

> **学習メモ（原文でなく訳者の解釈）**: このプロジェクトの PduR との対応を見るときの手がかり。
> - 上位層の API（`PduR_<Up>Transmit` など）と下位層の API（`PduR_<Lo>RxIndication` など）は、`<Up>`・`<Lo>`・`<LoTp>` の記号を、コンフィギュレーションしたモジュール名に置き換えて作る、仕様上は汎用の API である。
> - 経路グループを無効にしたとき、`Transmit` 系は `E_NOT_OK`、`CopyRxData` などの `BufReq_ReturnType` の関数は `BUFREQ_E_NOT_OK` を返す（[SWS_PDUR_00805]）。ただし `PduR_<User:LoTp>RxIndication` には影響しない。
> - 単一のバッファ（`PduRQueueDepth == 1`）の経路を止めるときは、`PduR_DisableRouting` の `initialize` が真ならデフォルト値、偽なら現在の値を保持する（[SWS_PDUR_00810]）。

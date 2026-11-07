# CAN Transport Layer（CanTp）仕様書 日本語訳

- 原典: AUTOSAR CP Release 4.3.1 `AUTOSAR_SWS_CANTransportLayer.pdf`（Document ID 014）
- 範囲: 第7章（機能仕様）と第8章（API 仕様）。第1〜5章、第6章（要求のトレーサビリティ）、第9章（シーケンス図）、第10章以降（コンフィギュレーション）は対象外。
- 凡例: `[SWS_CanTp_xxxxx]` は原典の条項 ID。`（SRS_…）` は原典が条項に付けている上位要求の ID。
- 図は訳せないため、位置に注記だけを残している。図の内容は原典を参照すること。
- 「学習メモ」は原文の訳ではなく、このプロジェクトの実装と照らすための訳者の解釈。

---

## 7 機能仕様

この章では、CAN Transport Layer の機能を説明する。上位層・下位層へ提供するサービスと、CAN Transport Layer の内部の動作を説明する。

CanTp モジュールは、メッセージの分割、フロー制御を伴う送信、およびメッセージの再構築のサービスを提供する。主な目的は、1 つの CAN フレームに収まる、あるいは収まらないメッセージを、送受信することである。1 つの CAN フレームに収まらないメッセージは、複数の部分に分割され、各部分が 1 つの CAN フレームで送信される。

この文書を読む際には、このモジュールが ISO 15765-2（OEM 拡張診断 [14]）の推奨に従い、ISO 15765-4（排出ガス関連システムの要求 [16]）を満たせるものであることを、念頭に置く必要がある。

**[SWS_CanTp_00033]** ISO 15765-2 の推奨が、この SWS で明示的に除外されていなければ、CanTp モジュールはその推奨に従わなければならない。（SRS_Can_01065）

SF、FF、CF、FC の各フレーム、ネットワーク層のタイミングパラメータ、および CAN Transport Layer のその他の機能の詳細は、ISO 15765-2 の仕様 [14] を参照すること。

ISO 15765-4 は、ISO 15765-2 の特殊な場合である。そのため、CAN Transport Layer は、ISO 15765-4 のすべての使用例（特定のタイミング、パディングの設定、アドレッシングモードなど）に合わせられるよう、コンフィギュレーションできる。詳細は第 10 章「コンフィギュレーション仕様」を参照すること。

### 7.1 上位層へ提供するサービス

CanTp モジュールのサービスインタフェースは、次の主な分類に分けられる。

- 初期化とシャットダウン
- 通信サービス

以降の項で、各分類の機能を説明する。

#### 7.1.1 初期化とシャットダウン

**[SWS_CanTp_00027]** CanTp モジュールは、`CANTP_OFF` と `CANTP_ON` の 2 つの内部状態を持たなければならない。

**[SWS_CanTp_00253]** `CANTP_OFF` と `CANTP_ON` の状態は、デバッグのために参照できなければならない（デバッグの概念の実装は 7.7 節に詳述）。

**[SWS_CanTp_00168]** CanTp モジュールは、電源投入後は `CANTP_OFF` の状態でなければならない。

**[SWS_CanTp_00169]** `CANTP_OFF` の状態では、CanTp は、ポストビルドのコンフィギュレーションの更新を許可しなければならない。

**[SWS_CanTp_00170]** CanTp が `CanTp_Init()` で正常に初期化されたとき、CanTp モジュールは内部状態 `CANTP_ON` へ変わらなければならない。（SRS_Can_01075）

**[SWS_CanTp_00238]** CanTp モジュールは、`CANTP_ON` の状態のときだけ、分割と再構築の処理を行わなければならない。

**[SWS_CanTp_00030]** 関数 `CanTp_Init` は、モジュールのすべてのグローバル変数を初期化し、すべてのトランスポートプロトコルの接続を、分割送信も分割受信も進行していない `CANTP_ON` のサブ状態（Rx スレッドは `CANTP_RX_WAIT`、Tx スレッドは `CANTP_TX_WAIT` の状態）に設定しなければならない。（SRS_Can_01075）

**[SWS_CanTp_00031]** CanTp モジュールの開発エラー検出が有効なとき、PDU Router または CAN Interface が、`CanTp_Init` が呼ばれる前に（`CanTp_GetVersionInfo` を除く）いずれかの関数を使おうとした場合、CanTp モジュールはエラー（`CANTP_E_UNINIT`）を報告しなければならない。（SRS_Can_01076）

**[SWS_CanTp_00111]** CanTp モジュールがグローバル状態 `CANTP_ON` のときに呼ばれた場合、関数 `CanTp_Init` は、モジュールを Idle の状態（状態は `CANTP_ON` のままで、送信も受信も進行していない）へ戻さなければならない。

**[SWS_CanTp_00273]** CanTp モジュールがグローバル状態 `CANTP_ON` のときに `CanTp_Init` が呼ばれた場合、CanTp モジュールは、現在のすべての接続を失わなければならない。

**[SWS_CanTp_00010]** 関数 `CanTp_Shutdown` は、CanTp モジュールを適切に停止しなければならない。（SRS_BSW_00336）

次の図は、上記の要求をまとめたものである。

> （図 4「CAN Transport Layer のライフサイクル」: 状態機械図。訳なし。原典を参照。
> 　読み取れる内容: 電源投入（PowerUp）後は `CANTP_OFF`。`CanTp_Init()` がエラー無しなら `CANTP_ON` へ、エラーありなら `CANTP_OFF` のまま。`CANTP_ON` の中に、接続チャネルごとのサブ状態 `CANTP_RX_WAIT` ⇄ `CANTP_RX_PROCESSING`（N-PDU の受信／これ以上の N-PDU が無い）と、`CANTP_TX_WAIT` ⇄ `CANTP_TX_PROCESSING`（N-SDU の送信／送信すべき N-SDU が無い）がある。`CanTp_Shutdown()` で `CANTP_OFF` へ戻り、電源断（PowerDown）はどの状態からも起こる。）

#### 7.1.2 送信要求

送信操作 `CanTp_Transmit()` により、上位層は、CAN トランスポートプロトコルの機能（分割、拡張アドレッシング形式など）を使ったデータ転送を依頼できる。

**[SWS_CanTp_00176]** 関数 `CanTp_Transmit()` は、非同期でなければならない。

**[SWS_CanTp_00177]** 送信要求が受理された後、N-SDU の転送の処理が（成功でも失敗でも）完全に終わったとき、CanTp モジュールは上位層へ通知しなければならない。

#### 7.1.3 送信のキャンセル

送信のキャンセル機能により、上位層は、進行中の送信を取り消せる。

使用例: 優先度の高い別の診断プロトコルを受信したために、診断の送信を取り消す。

**[SWS_CanTp_00242]** この機能は、静的なコンフィギュレーション（パラメータ `CanTpTc`）で有効・無効にできなければならない。

**[SWS_CanTp_00274]** 送信のキャンセルは、`CanTp_CancelTransmit()` の呼び出しで起動される。

**[SWS_CanTp_00243]** サービス `CanTp_CancelTransmit()` の呼び出しの後、この接続での転送は中止されなければならない。

注: 送信のキャンセルの後、`PduR_CanTpTxConfirmation()` は、値 `E_NOT_OK` で呼ばれなければならない（[SWS_CanTp_00255] も参照）。

転送が進行中の場合、受信側でタイムアウトエラーが発生することに注意。

### 7.2 下位層へ提供するサービス

AUTOSAR の通信スタックの仕様に従い、CAN Transport Layer は、CAN Interface へ次の 2 つのコールバック関数を提供する: `CanTp_TxConfirmation()` と `CanTp_RxIndication()`。

#### 7.2.1 送信確認

CanIf モジュールは、CanTp が要求した CAN フレームの送信が、成功したか否かを CAN Transport Layer へ通知するために、送信確認の関数を呼ぶ。対応する送信を識別するため、L-PDU の識別子が呼び出しに添えられる。

**[SWS_CanTp_00075]** 送信確認が、最大時間（N_As に等しい）の間に受信されなかったとき、CanTp モジュールは、対応するセッションを中止しなければならない。N-PDU は、`TxConfirmation` が（成功でも失敗でも）受信されるまで、他の並行するセッションから使えないままである。

**[SWS_CanTp_00076]** 確認の呼び出しのために、CanTp モジュールは関数 `CanTp_TxConfirmation()` を提供しなければならない。

**[SWS_CanTp_00355]** `CanTp_TxConfirmation()` が結果 `E_NOT_OK` で呼ばれたとき、CanTp は、対応するセッションを中止しなければならない。

#### 7.2.2 受信通知

CanIf モジュールは、新しい CAN の N-PDU フレーム（すなわち、トランスポートプロトコルのフレーム）が受信されたことを CanTp モジュールへ通知するために、受信通知の関数を呼ぶ。

受信通知は、CanIf のコンフィギュレーションに応じて、ISR のコンテキストで行われうる。

**[SWS_CanTp_00078]** 受信通知のために、CanTp モジュールは `CanTp_RxIndication()` を提供しなければならない。

### 7.3 内部の動作

CAN Transport Layer の内部の動作は、このモジュールの主な目的である、メッセージを 1 つの CAN フレームまたは複数の CAN フレームで転送するための、基本的な仕組みを提供する。

CAN Transport Layer の動作全体は、イベント駆動である。これにより、CanTp は、PDU Router（および CAN Interface）から直接来る N-SDU（および L-SDU）の転送を処理できる。

#### 7.3.1 N-SDU の受信

**[SWS_CanTp_00079]** SF または FF の N-PDU を受信したとき、CanTp モジュールは、関数 `PduR_CanTpStartOfReception` を使って、この受信を上位層（PDU Router）へ通知しなければならない。

注: 上位層は、N-SDU の受信のためのバッファを確保し、ロックする。

**[SWS_CanTp_00329]** CanTp は、`PduR_CanTpStartOfReception()` のパラメータ `TpSduInfoPtr` を使って、FF/SF の内容を PduR へ渡さなければならない。

**[SWS_CanTp_00350]** 受信したデータリンク層のデータ長（RX_DL）は、最初に受信した CAN フレーム／PDU のペイロード長（CAN_DL）から、次のように導かなければならない。

- CAN_DL の値が 8 バイト以下のとき、RX_DL の値は 8 とする。
- CAN_DL の値が 8 バイトより大きいとき、RX_DL の値は CAN_DL の値に等しい。

（SRS_Can_01065）

注: ISO のフレームの概要の表。

> （図 5「N_PCI バイトのまとめ」: 表の画像。訳なし。原典を参照。）

**[SWS_CanTp_00330]** SF または FF の N-PDU が、メタデータ付き（汎用の接続を示す）で `CanTp_RxIndication` により通知されたとき、CanTp モジュールは、PDU のメタデータに含まれるアドレッシング情報を保存し、この情報を、上位層への接続の開始、FC の N-PDU の送信、CF の N-PDU の識別に使わなければならない。メタデータのアドレッシング情報は、アドレッシング形式により異なる。

- Normal、Extended、Mixed 11 bit: なし
- Normal fixed、Mixed 29 bit: N_SA、N_TA

**[SWS_CanTp_00331]** 汎用の接続（メタデータ付きの N-SDU）に対して `PduR_CanTpStartOfReception()` を呼ぶとき、CanTp モジュールは、取り出したアドレッシング情報を、N-SDU のメタデータで渡さなければならない。メタデータのアドレッシング情報は、アドレッシング形式により異なる。

- Normal: なし
- Extended: N_TA
- Mixed 11 bit: N_AE
- Normal fixed: N_SA、N_TA
- Mixed 29 bit: N_SA、N_TA、N_AE

**[SWS_CanTp_00332]** 汎用の接続（メタデータ付きの N-PDU）の FC に対して `CanIf_Transmit()` を呼ぶとき、CanTp モジュールは、保存したアドレッシング情報を、N-PDU のメタデータで渡さなければならない。メタデータのアドレッシング情報は、アドレッシング形式により異なる。

- Normal、Extended、Mixed 11 bit: なし
- Normal fixed、Mixed 29 bit: N_SA（保存した N_TA）、N_TA（保存した N_SA）

**[SWS_CanTp_00333]** 汎用の接続（メタデータ付きの N-PDU）の CF に対して `CanTp_RxIndication` が呼ばれたとき、CanTp モジュールは、N-PDU のメタデータに含まれるアドレッシング情報を、FF から保存した値と照合しなければならない。

**[SWS_CanTp_00166]** FF、またはブロックの最後の CF を受信したとき、CanTp モジュールは、`PduR_CanTpStartOfReception` または `PduR_CanTpCopyRxData` を呼ぶ前に、タイムアウト N_Br を開始しなければならない。

**[SWS_CanTp_00080]** 利用できる Rx バッファのサイズは、サービス `PduR_CanTpStartOfReception()` の出力ポインタのパラメータで、CanTp へ報告される。利用できる Rx バッファは、期待する N-SDU のデータ長より小さくてもよい。

注: 上位層が、エラー（例: ゲートウェイの場合、宛先のネットワークへのトランスポートセッションが切れたことを示す場合がある）やリソースの制限（例: N-SDU の長さが上位層の最大バッファサイズを超える）のために、バッファを用意できない場合、関数 `PduR_CanTpStartOfReception()` は `BUFREQ_E_NOT_OK` または `BUFREQ_E_OVFL` を返す。

**[SWS_CanTp_00081]** First Frame または Single Frame の受信後、関数 `PduR_CanTpStartOfReception()` が CanTp モジュールへ `BUFREQ_E_NOT_OK` を返した場合、CanTp モジュールは、この N-SDU の受信を中止しなければならない。この場合、Flow Control は送られず、`PduR_CanTpRxIndication()` も呼ばれない。

**[SWS_CanTp_00318]** First Frame の受信後、関数 `PduR_CanTpStartOfReception()` が CanTp モジュールへ `BUFREQ_E_OVFL` を返した場合、CanTp モジュールは、オーバーフローのステータスの Flow Control N-PDU（FC(OVFLW)）を送り、N-SDU の受信を中止しなければならない。

**[SWS_CanTp_00353]** Single Frame の受信後、関数 `PduR_CanTpStartOfReception()` が CanTp モジュールへ `BUFREQ_E_OVFL` を返した場合、CanTp モジュールは、N-SDU の受信を中止しなければならない。

**[SWS_CanTp_00339]** First Frame または Single Frame の受信後、関数 `PduR_CanTpStartOfReception()` が、すでに受信したデータに必要なサイズより小さい、利用できるバッファサイズとともに `BUFREQ_OK` を返した場合、CanTp モジュールは、N-SDU の受信を中止し、結果 `E_NOT_OK` で `PduR_CanTpRxIndication()` を呼ばなければならない。

**[SWS_CanTp_00082]** First Frame の受信後、関数 `PduR_CanTpStartOfReception()` が、次のブロックに必要なサイズより小さい、利用できるバッファサイズとともに `BUFREQ_OK` を返した場合、CanTp モジュールは、タイマ N_Br を開始しなければならない。

**[SWS_CanTp_00325]** ブロックの最後の Consecutive Frame の受信後に呼んだ関数 `PduR_CanTpCopyRxData()` が `BUFREQ_OK` を返したが、残りのバッファが次のブロックの受信に足りない場合、CanTp モジュールは、タイマ N_Br を開始しなければならない。

**[SWS_CanTp_00222]** タイマ N_Br が動作している間、CanTp モジュールは、MainFunction の処理のたびに、データ長 0（ゼロ）、データバッファ `NULL_PTR` で、サービス `PduR_CanTpCopyRxData()` を呼ばなければならない。

注: ISO 15765-2 の仕様は、次の性能要求を定めている: (N_Br + N_Ar) < 0.9 × N_Bs のタイムアウト。

**[SWS_CanTp_00341]** N_Br タイマが満了し、利用できるバッファサイズがまだ十分でない場合、CanTp モジュールは、N-SDU の受信を中断するために、新しい FC(WAIT) を送り、N_Br タイマを再び設定しなければならない。

**[SWS_CanTp_00223]** CanTp モジュールは、連続する FC(WAIT) の N-PDU を、最大で WFTmax 回まで送らなければならない。この回数に達したとき、CanTp モジュールは、この N-SDU の受信を中止し（受信側が FC の N-PDU を送らなかったことになるため、送信側で N_Bs タイマが満了し、送信が中止される）、結果 `E_NOT_OK` の受信通知を行わなければならない。

**[SWS_CanTp_00311]** N_Ar のタイムアウトが発生した場合（送った FC フレームのいずれについても、CAN ドライバからの確認が無い場合）、CanTp モジュールは、受信を中止し、結果 `E_NOT_OK` で通知関数 `PduR_CanTpRxIndication()` を呼んで、この失敗を上位層へ通知しなければならない。

**[SWS_CanTp_00224]** Rx バッファが次のブロックに足りるとき（First Frame の直後、ブロックの最後の Consecutive Frame の直後、または [SWS_CanTp_00222] に従って `PduR_CanTpCopyRxData()` を繰り返し呼んだ後）、CanTp モジュールは、ClearToSend のステータスの Flow Control N-PDU（FC(CTS)）を送り、その後、Consecutive Frame の N-PDU の受信を待たなければならない。

**[SWS_CanTp_00269]** Consecutive Frame を 1 つ受信するたびに、CanTp モジュールは、データバッファとデータ長を含む PduInfo ポインタで、関数 `PduR_CanTpCopyRxData()` を呼ばなければならない。

- CAN 2.0 フレームでは、6 または 7 バイト（最後の CF ではそれ以下）
- CAN FD フレームでは、DLC-1 または DLC-2 バイト（図 5 と [SWS_CanTp_00351] を参照）

出力ポインタのパラメータは、データがコピーされた後に、利用できる Rx バッファのサイズを CanTp へ知らせる。

**[SWS_CanTp_00312]** CanTp モジュールは、Consecutive Frame を受信通知するたびに（ブロックの最後のものを除く）、および、送信側で CF の送信を始めさせる FC（FS=CTS）の送信を確認するたびに、タイムアウト N_Cr を開始しなければならない。

**[SWS_CanTp_00313]** N_Cr のタイムアウトが発生した場合、CanTp モジュールは、受信を中止し、結果 `E_NOT_OK` で通知関数 `PduR_CanTpRxIndication()` を呼んで、この失敗を上位層へ通知しなければならない。

**[SWS_CanTp_00271]** ブロックの中の Consecutive Frame の受信後、`PduR_CanTpCopyRxData()` が `BUFREQ_E_NOT_OK` を返した場合、CanTp は、N-SDU の受信を中止し、結果 `E_NOT_OK` で `PduR_CanTpRxIndication()` を呼んで、PduR モジュールへ通知しなければならない。

**[SWS_CanTp_00314]** CanTp は、分割受信の間に受信した各 SN の正しさを検査しなければならない。誤った SN を受信した場合、CanTp モジュールは、受信を中止し、結果 `E_NOT_OK` で通知関数 `PduR_CanTpRxIndication()` を呼んで、この失敗を上位層へ通知しなければならない。

**[SWS_CanTp_00084]** トランスポートの受信セッションが（成功でも失敗でも）完了したとき、CanTp モジュールは、上位層の通知サービス `PduR_CanTpRxIndication()` を呼ばなければならない。

**[SWS_CanTp_00277]** FF の N-PDU の受信に関して、Flow Control N-PDU の内容は、`PduR_CanTpStartOfReception()` サービスの結果に依存する。

**[SWS_CanTp_00064]** さらに、FF の N-PDU を受信したときは、`PduR_CanTpStartOfReception()` サービスの結果を得てから初めて、Flow Control を送らなければならないことに注意。

**[SWS_CanTp_00278]** FC の N-PDU は、BS（Block Size）個の Consecutive Frame から成る各ブロックの後にだけ送られることに注意。

**[SWS_CanTp_00067]** CanTp は、分割受信の間に送る各 FC で、BS と STmin のパラメータに同じ値を使わなければならない。異なる N-SDU の受信では、これらのパラメータに異なる値を使ってよい。

**[SWS_CanTp_00342]** FC を送るときに `CanIf_Transmit()` が `E_NOT_OK` を返した場合、CanTp は、現在の受信の接続を終了しなければならない。

> **学習メモ（原文でなく訳者の解釈）**: このプロジェクトの CanTp 実装との対応を見るときの手がかり。
> - 受信側の「バッファが足りない間の待機」は、[SWS_CanTp_00082]（N_Br の開始）→ [SWS_CanTp_00222]（MainFunction ごとに長さ 0 の `PduR_CanTpCopyRxData`）→ [SWS_CanTp_00341]（満了で新しい FC(WAIT)）→ [SWS_CanTp_00223]（WFTmax 回まで）の 4 条項で 1 組。
> - 送信側が FC(WAIT) を受けたときの扱い（WFTmax の数え上げ）は、7.3.2 節側の条項にある。

#### 7.3.2 N-SDU の送信

上位層は、`CanTp_Transmit()` を呼んで N-SDU の送信を依頼する。`CanTp_Transmit()` のパラメータは、CAN NSduId と、送信する Tx N-SDU の全長を表す。

**[SWS_CanTp_00225]** メタデータを使わない特定の接続では、関数 `CanTp_Transmit` は、Single Frame または First Frame の PCI を準備するために、全体の `SduLength` の情報だけを使い、利用できる N-SDU のデータバッファを使ってはならない。

**[SWS_CanTp_00334]** メタデータ付きの N-SDU に対して `CanTp_Transmit` が呼ばれたとき、CanTp モジュールは、N-SDU のメタデータに含まれるアドレッシング情報を保存し、この情報を、SF、FF、CF の N-PDU の送信と、FC の N-PDU の識別に使わなければならない。メタデータのアドレッシング情報は、アドレッシング形式により異なる。

- Normal: なし
- Extended: N_TA
- Mixed 11 bit: N_AE
- Normal fixed: N_SA、N_TA
- Mixed 29 bit: N_SA、N_TA、N_AE

**[SWS_CanTp_00335]** 汎用の接続（メタデータ付きの N-PDU）の SF、FF、CF に対して `CanIf_Transmit()` を呼ぶとき、CanTp モジュールは、保存したアドレッシング情報を、N-PDU のメタデータで渡さなければならない。メタデータのアドレッシング情報は、アドレッシング形式により異なる。

- Normal、Extended、Mixed 11 bit: なし
- Normal fixed、Mixed 29 bit: N_SA、N_TA

**[SWS_CanTp_00336]** 汎用の接続（メタデータ付きの N-PDU）の FC に対して `CanTp_RxIndication` が呼ばれたとき、CanTp モジュールは、メタデータに含まれるアドレッシング情報を、保存した値と照合しなければならない。

**[SWS_CanTp_00167]** 上位層からの送信要求の後、CanTp モジュールは、`PduR_CanTpCopyTxData` を呼ぶ前に、タイムアウト N_Cs を開始しなければならない。タイマが経過するまでにデータが得られなかった場合、CanTp モジュールは、通信を中止しなければならない。

**[SWS_CanTp_00086]** CanTp モジュールは、送る各セグメント（SF、FF、CF）に対して、サービス `PduR_CanTpCopyTxData` を呼ばなければならない。上位層は、送信データを `PduInfoType` 構造体へコピーする。（SRS_Can_01079）

**[SWS_CanTp_00272]** API `PduR_CanTpCopyTxData()` は、リカバリの仕組みに使うパラメータ `retry` を持つ。ISO 15765-2 はそのような仕組みをサポートしないため、CAN Transport Layer はいかなる種類のリカバリも実装しない。したがって、このパラメータは常に NULL ポインタに設定される。

上位層が、エラー（例: ゲートウェイの場合、送信元のネットワークからのトランスポートセッションが終了したことを示す場合がある）のために、Tx データを用意できない場合、関数 `PduR_CanTpCopyTxData()` は `BUFREQ_E_NOT_OK` を返す。

**[SWS_CanTp_00087]** `PduR_CanTpCopyTxData()` が `BUFREQ_E_NOT_OK` を返した場合、CanTp モジュールは、送信要求を中止し、結果 `E_NOT_OK` でコールバック関数 `PduR_CanTpTxConfirmation()` を呼んで、この失敗を上位層へ通知しなければならない。

注: 上位層に一時的に利用できる Tx バッファが無い場合、関数 `PduR_CanTpCopyTxData()` は `BUFREQ_E_BUSY` を返す。

**[SWS_CanTp_00184]** 関数 `PduR_CanTpCopyTxData()` が `BUFREQ_E_BUSY` を返した場合、CanTp モジュールは、後で（実装依存）、データのコピーを再試行しなければならない。

注: N_Cs タイマが満了するまでにデータが得られない場合（ISO 15765-2 の仕様は、次の性能要求を定めている: (N_Cs + N_As) < 0.9 × N_Cr のタイムアウト）、CanTp モジュールは、この送信セッションを中止しなければならない。

**[SWS_CanTp_00280]** N_Cs のタイムアウトまでにデータが得られない場合、CanTp モジュールは、結果 `E_NOT_OK` でコールバック関数 `PduR_CanTpTxConfirmation` を呼んで、この失敗を上位層へ通知しなければならない。

**[SWS_CanTp_00089]** Tx データが利用できるようになったとき、CanTp モジュールは、N-SDU の送信を再開しなければならない。

**[SWS_CanTp_00310]** N_As のタイムアウトが発生した場合（CAN ドライバからの確認が無い場合）、CanTp モジュールは、結果 `E_NOT_OK` でコールバック関数 `PduR_CanTpTxConfirmation()` を呼んで、上位層へ通知しなければならない。

**[SWS_CanTp_00309]** FS が OVFLW に設定された FC フレームを受信した場合、CanTp モジュールは、送信要求を中止し、結果 `E_NOT_OK` でコールバック関数 `PduR_CanTpTxConfirmation()` を呼んで、上位層へ通知しなければならない。

**[SWS_CanTp_00317]** 無効な FS の FC フレームを受信した場合、CanTp モジュールは、このメッセージの送信を中止し、結果 `E_NOT_OK` でコールバック関数 `PduR_CanTpTxConfirmation()` を呼んで、上位層へ通知しなければならない。

**[SWS_CanTp_00315]** CanTp モジュールは、FF の送信の確認、ブロックの最後の CF の送信の確認、および、FS=WT の FC を通知されるたびに（すなわち、次の FC を受信するまでの時間）、N_Bs の時間のタイムアウト監視を開始しなければならない。

**[SWS_CanTp_00316]** N_Bs のタイムアウトが発生した場合、CanTp モジュールは、このメッセージの送信を中止し、結果 `E_NOT_OK` でコールバック関数 `PduR_CanTpTxConfirmation()` を呼んで、上位層へ通知しなければならない。

**[SWS_CanTp_00090]** トランスポートの送信セッションが正常に完了したとき、CanTp モジュールは、上位層の通知サービス `PduR_CanTpTxConfirmation()` を、結果 `E_OK` で呼ばなければならない。

**[SWS_CanTp_00343]** SF、FF、CF を送るときに `CanIf_Transmit()` が `E_NOT_OK` を返した場合、CanTp は、現在の送信の接続を終了しなければならない。

> **学習メモ（原文でなく訳者の解釈）**: この節（送信側）の条項に、FC(WAIT) の回数を数えて WFTmax で打ち切る規定は無い。送信側が FC(WAIT) を受けたときの規定は [SWS_CanTp_00315]（N_Bs を再開）だけで、待ち続ける上限は N_Bs のタイムアウト（[SWS_CanTp_00316]）で与えられる。WFTmax は、受信側が FC(WAIT) を送る回数の上限（[SWS_CanTp_00223]、7.3.1 節）である。

#### 7.3.3 バッファの方針

CanTp はバッファリングの能力を持たないため、送信する N-SDU のペイロードは内部へコピーされず、受信した N-PDU も内部で再構築されない。

CAN Transport Layer は、上位層（例: PduR、DCM、COM）のメモリ領域を直接扱う。これらのメモリ領域へアクセスするために、CAN Transport Layer は、関数 `PduR_CanTpCopyTxData()` または `PduR_CanTpCopyRxData()` を使う。

したがって、データの一貫性を保証するために、上位層は、通知が来るまでこのメモリ領域をロックすべきである。

- 送信バッファがロックされているとき、上位層は、そのバッファ領域の中へデータを書いてはならない。
- 受信バッファがロックされているとき、CAN Transport Layer は、そのバッファのデータの一貫性を保証しない。上位層は、そのバッファ領域のデータを読んでも書いてもならない。

> （図 6「Tx と Rx のバッファのロック」: 状態機械図。訳なし。原典を参照。
> 　読み取れる内容: 送信バッファは `CanTp_Transmit()` が `E_OK` を返したときにロックされ、`PduR_CanTpTxConfirmation` の呼び出しでアンロックされる。受信バッファは `PduR_CanTpStartOfReception` が `BUFREQ_OK` を返したときにロックされ、`PduR_CanTpRxIndication` の呼び出しでアンロックされる。）

上位層のモジュールは、`PduR_CanTpStartOfReception()` の呼び出しに状態 `BUFREQ_OK` を返したとき、または `CanTp_Transmit()` が `E_OK` を返したときに、バッファをロックしたものとみなされ、確認または通知（`PduR_CanTpTxConfirmation()` または `PduR_CanTpRxIndication()` の呼び出し）が起きるまで、バッファをロックしたままにしなければならない。

次の図は、CAN 2.0 フレームを使って 50 バイトのフレームを送る処理をまとめた例である。

> （図 7「送信処理の例」: シーケンス図。訳なし。）

1. PduR が、50 バイトのデータの送信を依頼する。
2. CanTp が、PduR へペイロードデータを要求し、First Frame を送る。
3. CanTp が、残りのペイロードデータを、Consecutive Frame の列として送る。各 CF で、6 または 7 バイトのペイロードデータが、上位層によりコピーされる。
4. CanTp が、ペイロードデータの送信を確認する。

次の図は、49 バイトの N-SDU を受信する例である。上位層は、利用できる Rx バッファとして 25 バイトを報告する。

> （図 8「受信処理の例」: シーケンス図と表。訳なし。表の内容: 各フレームの受信後の「SDU の残りのバイト数」と「利用できるバッファ」。FF（6 バイト）で残り 43・バッファ 19、CF（7 バイト）で残り 36・バッファ 12、次の CF で残り 29・バッファ 5、ここでバッファ待ち。以降も同様に、バッファ待ちを挟みながら、最後の CF（1 バイト）で残り 0。）

1. CanIf が、`CanTp_RxIndication()` で新しい受信を通知する。CanTp は、この通知を PduR へ転送する。
2. PduR が、利用できるバッファサイズ 25 バイトを返し、CanTp が、送信元へ FlowControl の CTS を送る。
3. CanTp が、受信した各フレームのデータを PduR へ渡し、残りのバッファサイズを監視する。2 つ目の Consecutive Frame の後、残りのバッファサイズは、次のブロック（Consecutive Frame 2 つ分）に足りない。
4. CanTp が、データ長 0、データ `NULL_PTR` で `PduR_CanTpCopyRxData()` を呼んで、PduR に残りのバッファサイズを尋ね、送信元へ FlowControl の Wait を送る。これは、次のブロックに十分なバッファが得られるまで続けられる。
5. バッファサイズがついに次のブロックに十分になると、CanTp は、送信元へ FlowControl の CTS を送り、次の Consecutive Frame のブロックの受信を続ける。
6. ブロックの最後の Consecutive Frame をコピーした後、残りのバッファが次のブロックには少なすぎるため、CanTp は、再び Wait フレームを送り、残りのバッファサイズを監視する。
7. 最後のブロックのためのバッファが得られると、CanTp は、受信を続ける。
8. CanTp が、`PduR_CanTpRxIndication()` の呼び出しで、受信の終了を PduR へ知らせる。

#### 7.3.4 プロトコルパラメータの設定サービス

**[SWS_CanTp_00091]** CanTp モジュールは、アプリケーションによる、トランスポートプロトコルの内部パラメータ（STmin と BS）の動的な設定のための、（ISO 15765-2 の仕様が提案する）オプションのプリミティブをサポートしなければならない。

BS の値は、あくまで最大値である。バッファ長の都合で、CAN Transport Layer は、設定された最大値の範囲内で、BS の値を調整できる。

#### 7.3.5 Tx と Rx のデータの流れ

次の図は、分割しないメッセージの送信と、分割するメッセージの送信の例を示す。

> （図 9「単一パートのメッセージの例」、図 10「複数パートのメッセージの例」: 画像。訳なし。）

フロー制御は、送信側を、受信側の能力に合わせるために使われる。このトランスポートプロトコルの主な用途は、ピアツーピアの通信（すなわち、1 対 1 の通信、物理アドレッシング [14]）である。

**[SWS_CanTp_00092]** CanTp モジュールは、SF の N-PDU（および SF の N-SDU のみ）に対する機能として、1 対 n の通信（すなわち、機能アドレッシング [14]）を提供しなければならない。

コンフィギュレーションツールは、機能アドレッシングの属性が設定されているのが SF の N-PDU だけであるか、検査しなければならない。

**[SWS_CanTp_00093]** 通信タイプが機能アドレッシングであるハンドルで、（受信側でも送信側でも）複数に分割されたセッションが起きた場合、CanTp モジュールは、その要求を拒否し、ランタイムエラーコード `CANTP_E_INVALID_TATYPE` を Default Error Tracer へ報告しなければならない。

#### 7.3.6 CAN NSduId と CAN LSduId の関係

この項では、トランスポートプロトコルのデータユニットの送受信を可能にするための、CAN NSduId と CAN LSduId の間の接続関係を説明する。

**[SWS_CanTp_00035]** CAN NSduId は、SF、FF、FC、CF のフレームの送信に使われる、1 つの CAN LSduId にだけ結び付けられなければならない。（SRS_Can_01068、SRS_Can_01069、SRS_Can_01071、SRS_Can_01078）

**[SWS_CanTp_00281]** ただし、メッセージが、拡張アドレッシングまたはミックスアドレッシングの形式を使うように設定されている場合、CanTp モジュールは、送信する各セグメント（SF、FF、CF）の最初のバイトを、N_TA（拡張アドレッシングの場合）または N_AE（ミックスアドレッシングの場合）の値で埋めなければならない。したがって、CAN NSduId は、N_TA または N_AE の値にも関連付けられうる。

**[SWS_CanTp_00282]** FC のプロトコルデータユニットは、後続の CF の N-PDU の送信を許可または遅延させることで、受信側が送信側のデータの流れを制御できるようにする。

**[SWS_CanTp_00283]** 拡張アドレッシングの形式では、FC の最初のデータバイトにも、N_TA の値、または N_TA と N_TAtype の値の一意の組み合わせが含まれる。ミックスアドレッシングの形式では、FC の最初のデータバイトに N_AE の値が含まれる。

**[SWS_CanTp_00094]** したがって、FC フレームの CAN LSduId を、その N_TA の値（例: N_AI）または N_AE の値と組み合わせたものは、1 つの CAN NSduId だけを識別しなければならない。

**[SWS_CanTp_00284]** 受信方向では、各（SF、FF、CF）トランスポートプロトコルデータユニットの最初のデータバイトの値が、関連する N-SDU を決めるために使われる。

**[SWS_CanTp_00095]** したがって、拡張アドレッシングの N-PDU の受信では、CanTp モジュールは、関連する N-SDU を確定するために、N_TA の値を取り出さなければならない。ミックスアドレッシングモードでも、N_AE の値に関して同じ処理を適用しなければならない。

次の図は、これらの議論をまとめたものである。

> （図 11「NSduId と LSduId の間に考えられる結び付き」: データモデル図。訳なし。
> 　読み取れる内容: N-SDU は NSduId を持ち、L-SDU は LSduId を持つ。制約として、SF、FF、CF は同じ LSduId を使い、FC は異なる LSduId を使う。N_AI は N_TA と N_AE（0..1）から成る。）

#### 7.3.7 並行する接続

CAN Transport Layer は、複数の接続を同時に管理できる（例: UDS と OBD の要求を同時に受信できる）。

**[SWS_CanTp_00096]** CanTp モジュールは、複数の接続を同時にサポートしなければならない。（SRS_Can_01066）

**[SWS_CanTp_00120]** CanTp モジュールで、並行する接続をコンフィギュレーションできなければならない。（SRS_Can_01066）

**[SWS_CanTp_00285]** 接続チャネルは、CAN TP の内部での使用だけを目的とするため、外部からアクセスできない。

**[SWS_CanTp_00286]** 必要な情報（チャネル番号、タイミングパラメータなど）はすべて、CAN Transport Layer モジュールの内部でコンフィギュレーションされる。

**[SWS_CanTp_00121]** 各 N-SDU は、1 つの接続チャネルに静的に結び付けられる。この接続チャネルは、N-SDU の送信または受信のための、内部の経路を表す。接続チャネルは、1 つ以上の N-SDU に結び付けられる。（SRS_Can_01066）

**[SWS_CanTp_00122]** 各接続チャネルは、他の接続チャネルから独立している。これは、接続チャネルが、内部バッファ、タイマ、状態機械などの、自分自身のリソースを使うことを意味する。（SRS_Can_01066）

**[SWS_CanTp_00190]** CanTp モジュールは、N-SDU を、正しくコンフィギュレーションされた接続チャネルを通して経路付けしなければならない。

**[SWS_CanTp_00287]** CanTp モジュールは、同じ識別子の N-SDU の受信または送信を、並行して受け付けてはならない。そうしないと、受信したフレームを正しい接続へ割り当てられないためである。（メタデータを使わない）特定の接続だけが使われる場合、各 N-SDU が 1 つの接続チャネルにだけ結び付けられるため、この要求はコンフィギュレーションによって守られる。

ユーザが、特定の接続チャネルを 1 つの N-SDU 専用にしたい場合は、コンフィギュレーションの過程で、この接続チャネルを 1 つの N-SDU にだけ割り当てるべきである。

**[SWS_CanTp_00288]** 接続チャネルが複数の N-SDU に割り当てられている場合、リソースは異なる N-SDU の間で共有され、空いている接続チャネルが無いときは、CAN Transport Layer は、送信を拒否するか、受信を中止する。

**[SWS_CanTp_00289]** 接続チャネルの数は、直接コンフィギュレーションできない。コンフィギュレーションの過程で、N-SDU とチャネルの経路表を解析して、コンフィギュレーションツールが決める。

**[SWS_CanTp_00123]** コンフィギュレーションされた送信の接続チャネルが使用中（状態 `CANTP_TX_PROCESSING`）のとき、CanTp モジュールは、このチャネルに結び付けられた新しい送信要求を拒否しなければならない。送信を拒否するために、上位層が関数 `CanTp_Transmit()` で送信を依頼したとき、CanTp は `E_NOT_OK` を返す。（SRS_Can_01066）

**[SWS_CanTp_00124]** メタデータを持たない SF または FF の N-PDU を受信し、対応する接続チャネルが、すでに同じ接続を受信している（状態 `CANTP_RX_PROCESSING`、同じ N_AI）とき、CanTp モジュールは、進行中の受信を中止し、受信したフレームを新しい受信の開始として処理しなければならない。メタデータを持たない SF または FF の N-PDU を、動作中の接続チャネルで、別の接続（異なる N_AI）について受信した場合、その SF または FF は無視されなければならない。（SRS_Can_01066）

**[SWS_CanTp_00337]** メタデータ付き（汎用の接続を示す）の SF または FF の N-PDU を受信し、対応する接続チャネルが現在受信中のとき、その SF または FF は無視されなければならない。

**[SWS_CanTp_00248]** Tx の N-PDU が、異なるチャネルの 2 つ以上の異なる接続で使われる場合、この N-PDU へのアクセスは、`TxConfirmation` を使って直列化されなければならない。Rx の N-PDU を 2 つ以上の異なる接続チャネルで使えるのは、この N-PDU に関して拡張アドレッシングまたはミックスアドレッシングを使う場合、またはメタデータを持つ（したがって汎用の接続に属する）場合だけである。

注: CAN FD のフレームと CAN のフレームは、フレーム形式（CAN FD か CAN 2.0 か）に応じて、CanIf により異なる PDU へ割り当てられる。したがって、CAN FD と従来の CAN の通信を区別できる。

#### 7.3.8 N-PDU のパディング

フレームのデータ長に関する、上位層のすべての要求との完全な互換性を保証するため（例: OBD は、データ長を常に 8 バイトに設定するよう要求するが、UDS は要求しない）、パディングを有効にするかは、N-SDU ごとに、コンパイル時にコンフィギュレーションできる。Rx N-SDU には `CanTpRxPaddingActivation`、Tx N-SDU には `CanTpTxPaddingActivation` を使う。

**[SWS_CanTp_00116]** パディングありのモードでも、パディングなしのモードでも、CanTp モジュールは、使用されたデータバイトだけを上位層へ渡さなければならない。（SRS_Can_01073）

**[SWS_CanTp_00059]** パディングバイトに使う値は、コンフィギュレーションパラメータ `CANTP_PADDING_BYTE`（ECUC_CanTp_00298 を参照）でコンフィギュレーションできる。（SRS_Can_01086）

**[SWS_CanTp_00344]** ペイロードが 8 以下のフレーム（CAN 2.0 のフレームまたは小さい CAN FD のフレーム）が Rx N-SDU に使われ、`CanTpRxPaddingActivation` が `CANTP_ON` に等しい場合、CanTp は、その N-SDU に属する SF の Rx N-PDU、または最後の CF の Rx N-PDU を、長さが 8 バイト（すなわち `PduInfoPtr.SduLength = 8`）のものだけ受け付けなければならない。（SRS_Can_01073）

**[SWS_CanTp_00345]** ペイロードが 8 以下のフレーム（CAN 2.0 のフレームまたは小さい CAN FD のフレーム）が Rx N-SDU に使われ、`CanTpRxPaddingActivation` が `CANTP_ON` に等しい場合に、CanTp が、`CanTp_RxIndication()` の呼び出しで、その N-SDU に属する、8 バイトより短い（すなわち `PduInfoPtr.SduLength < 8`）SF の Rx N-PDU を受け取ったとき、CanTp は、その受信を拒否しなければならない。ランタイムエラーコード `CANTP_E_PADDING` を Default Error Tracer へ報告しなければならない。（SRS_Can_01073）

**[SWS_CanTp_00346]** ペイロードが 8 以下のフレーム（CAN 2.0 のフレームまたは小さい CAN FD のフレーム）が Rx N-SDU に使われ、`CanTpRxPaddingActivation` が `CANTP_ON` に等しい場合に、CanTp が、`CanTp_RxIndication()` の呼び出しで、その N-SDU に属する、8 バイトでない（すなわち `PduInfoPtr.SduLength != 8`）最後の CF の Rx N-PDU を受け取ったとき、CanTp は、結果 `E_NOT_OK` で `PduR_CanTpRxIndication()` を呼んで、進行中の受信を中止しなければならない。ランタイムエラーコード `CANTP_E_PADDING` を Default Error Tracer へ報告しなければならない。（SRS_Can_01073）

**[SWS_CanTp_00347]** Rx N-SDU に対して `CanTpRxPaddingActivation` が `CANTP_ON` に等しい場合、CanTp モジュールは、FC の N-PDU を、長さ 8 バイトで送信しなければならない。N-PDU の未使用のバイトは、`CANTP_PADDING_BYTE`（ECUC_CanTp_00298 を参照）で更新されなければならない。

**[SWS_CanTp_00348]** ペイロードが 8 以下のフレーム（CAN 2.0 のフレームまたは小さい CAN FD のフレーム）が Tx N-SDU に使われ、`CanTpTxPaddingActivation` が `CANTP_ON` に等しい場合、CanTp は、その Tx N-SDU に属する、SF の Tx N-PDU または最後の CF の Tx N-PDU を、`CanIf_Transmit()` の呼び出しで、長さ 8 バイト（すなわち `PduInfoPtr.SduLength = 8`）で送信しなければならない。N-PDU の未使用のバイトは、`CANTP_PADDING_BYTE`（ECUC_CanTp_00298 を参照）で更新されなければならない。（SRS_Can_01073）

**[SWS_CanTp_00349]** Tx N-SDU に対して `CanTpTxPaddingActivation` が `CANTP_ON` に等しく、進行中の送信で、その Tx N-SDU のための FC の N-PDU を、`CanTp_RxIndication()` の呼び出しで受け取り、この FC の長さが 8 バイトより短い（すなわち `PduInfoPtr.SduLength < 8`）場合、CanTp モジュールは、結果 `E_NOT_OK` で `PduR_CanTpTxConfirmation()` を呼んで、送信セッションを中止しなければならない。ランタイムエラーコード `CANTP_E_PADDING` を Default Error Tracer へ報告しなければならない。

**[SWS_CanTp_00351]** `CanIf_Transmit()` で送信するデータ長が、取りうる DLC の値（0〜8、12、16、20、24、32、48、64）に合わない場合、CanTp は、次に大きい有効な DLC を送信に使い、未使用のバイトを `CANTP_PADDING_BYTE`（ECUC_CanTp_00298 を参照）の値で初期化しなければならない。（SRS_Can_01073）

理由: ISO 11898-1:2015 の DLC の値 9〜15 は、最大 64 バイトまでの CAN フレームのペイロード長の、非線形な離散値に割り当てられている。初期化されていないデータの送信を防ぐため、送信する N_PDU のサイズが、ISO 11898-1:2015 の DLC の表に定められた離散的な長さのいずれにも等しくない場合、8 より大きい DLC の値では、CAN フレームのデータのパディングが必須である。DLC の値 9〜15 では、必須のパディングだけを使うべきである。

次の図は、ISO フレームを表す。

> （図 12「ISO フレームの構成」: 画像。訳なし。）

#### 7.3.9 想定外の N-PDU の到着の扱い

想定外の N-PDU が到着したときの CAN Transport Layer の動作は、処理中の N-SDU の通信方向の種別に大きく依存する。

**[SWS_CanTp_00057]** 想定外のフレームを受信した場合、CanTp モジュールは、下の表に従って動作しなければならない。（SRS_Can_01082、SRS_Can_01117、SRS_Can_01149）

**[SWS_CanTp_00290]** これらの表は、CanTp の実際の内部状態（CanTp の状態）を考慮している。表 1 は半二重の実装での動作を、表 2 は全二重のチャネルでの動作を定める。（SRS_Can_01117、SRS_Can_01149）

受信した N-PDU は、その N-PDU を受信した時点で進行中の受信または送信と、同じアドレス情報（N_AI）を含むものと理解しなければならない。

> **注（再構成）**: 原典の PDF の 2 つの表は、テキストの抽出で崩れていたため、行と列の対応を復元して表にした。元の表と食い違う可能性があるので、重要な箇所は原典の表 1・表 2（46〜47 ページ）で確認すること。

**表 1: 半二重のチャネルでの N-PDU の到着の扱い**

| CanTp の状態 | SF の N-PDU | FF の N-PDU | CF の N-PDU | FC の N-PDU | 未知の N-PDU |
|---|---|---|---|---|---|
| 分割送信が進行中 | 無視する | 無視する | 無視する | 待っていれば、FC の N-PDU を処理する。そうでなければ無視する | 無視する |
| 分割受信が進行中 | 現在の受信を終了し、パラメータ Result を `E_NOT_OK` にした通知を上位層へ出し、SF の N-PDU を新しい受信の開始として処理する | 現在の受信を終了し、パラメータ Result を `E_NOT_OK` にした通知を上位層へ出し、FF の N-PDU を新しい受信の開始として処理する | 待っていれば、進行中の受信の中で CF の N-PDU を処理し、必要な検査（例: SN が正しい順序か）を行う | 無視する | 無視する |
| アイドル（注） | SF の N-PDU を新しい受信の開始として処理する | FF の N-PDU を新しい受信の開始として処理する | 無視する | 無視する | 無視する |

注: アイドル = `CANTP_ON.CANTP_RX_WAIT` かつ `CANTP_ON.CANTP_TX_WAIT`。

**表 2: 全二重のチャネルでの N-PDU の到着の扱い**

| CanTp の状態 | SF の N-PDU | FF の N-PDU | CF の N-PDU | FC の N-PDU | 未知の N-PDU |
|---|---|---|---|---|---|
| 分割送信が進行中 | 受信が進行中なら、下の行（分割受信が進行中）に従って処理する。そうでなければ、SF の N-PDU を新しい受信の開始として処理する | 受信が進行中なら、下の行に従って処理する。そうでなければ、FF の N-PDU を新しい受信の開始として処理する | 受信が進行中なら、下の行に従って処理する。そうでなければ無視する | 待っていれば、FC の N-PDU を処理する。そうでなければ無視する | 無視する |
| 分割受信が進行中 | 現在の受信を終了し、パラメータ Result を `E_NOT_OK` にした通知を上位層へ出し、SF の N-PDU を新しい受信の開始として処理する | 現在の受信を終了し、パラメータ Result を `E_NOT_OK` にした通知を上位層へ出し、FF の N-PDU を新しい受信の開始として処理する | 進行中の受信の中で CF の N-PDU を処理し、必要な検査（例: SN が正しい順序か）を行う | 送信が進行中なら、上の行（分割送信が進行中）に従って処理する。そうでなければ無視する | 無視する |
| アイドル | SF の N-PDU を新しい受信の開始として処理する | FF の N-PDU を新しい受信の開始として処理する | 無視する | 無視する | 無視する |

### 7.4 エラーの分類

この項では、この基本ソフトウェアのライフサイクルの間に起こりうる複数のエラーの種類を、CanTp モジュールがどのように扱うかを説明する。

**[SWS_CanTp_00008]** エラーや例外が起きたとき、CanTp モジュールは、現在のモジュールの状態（図 4「CAN Transport Layer のライフサイクル」を参照）を変更せず、単にエラーイベントを報告しなければならない。（SRS_BSW_00339）

**[SWS_CanTp_00291]** プロダクションエラーの場合、Diagnostic Event Manager モジュールが（Function Inhibition Manager を介して）適切な処置（例: 呼び出し元のモジュールの状態の変更）を行う。

#### 7.4.1 開発エラー

**[SWS_CanTp_00293]** CanTp モジュールは、次のエラーを検出できなければならない。

> **注（再構成）**: 原典の表は、テキストの抽出で崩れていたため、エラーコードの値の並びから対応を復元した。

| エラーの種類 | 関連性 | 関連するエラーコード | 値 [hex] |
|---|---|---|---|
| API サービスが誤ったパラメータで呼ばれた。例: `CanTp_Transmit` が、コンフィギュレーションされていない PDU 識別子、または受信 PDU の識別子で呼ばれた。次の組み合わせでありうる: `CANTP_E_PARAM_CONFIG`、`CANTP_E_PARAM_ID`、`CANTP_E_PARAM_POINTER`、`CANTP_E_INIT_FAILED`、`CANTP_E_UNINIT`、`CANTP_E_INVALID_TX_ID`、`CANTP_E_INVALID_RX_ID` | 開発 | `CANTP_E_PARAM_CONFIG` | 0x01 |
| 同上 | 開発 | `CANTP_E_PARAM_ID` | 0x02 |
| API サービスが NULL ポインタで呼ばれた。このエラーの場合、API サービスは、この開発エラーを報告する以外の処理をせず、直ちに戻らなければならない。 | 開発 | `CANTP_E_PARAM_POINTER` | 0x03 |
| モジュールの初期化に失敗した。例: `CanTp_Init()` が、ポストビルドで無効なポインタで呼ばれた。 | 開発 | `CANTP_E_INIT_FAILED` | 0x04 |
| モジュールの初期化なしで API サービスが使われた。`CanTp_Init()` と `CanTp_GetVersionInfo()` 以外のどの API の呼び出しでも、CanTp が `CANTP_OFF` の状態の場合。 | 開発 | `CANTP_E_UNINIT` | 0x20 |
| 無効な送信 PDU 識別子（例: 存在しない Tx PDU 識別子でサービスが呼ばれた） | 開発 | `CANTP_E_INVALID_TX_ID` | 0x30 |
| 無効な受信 PDU 識別子（例: 存在しない Rx PDU 識別子でサービスが呼ばれた） | 開発 | `CANTP_E_INVALID_RX_ID` | 0x40 |

#### 7.4.2 ランタイムエラー

**[SWS_CanTp_00352]** CanTp モジュールは、次のランタイムエラーを検出できなければならない。

> **注（再構成）**: 原典の表は、テキストの抽出で崩れていたため、エラーコードの並びから対応を復元した。

| エラーの種類 | 関連性 | 関連するエラーコード | 値 [hex] |
|---|---|---|---|
| 8 バイトより短い長さで PDU を受信した（すなわち `PduInfoPtr.SduLength < 8`） | ランタイム | `CANTP_E_PADDING` | 0x70 |
| 機能アドレッシングでコンフィギュレーションされた Tx I-PDU に対して `CanTp_Transmit()` が呼ばれ、長さのパラメータが、メッセージを SF で送れないことを示している | ランタイム | `CANTP_E_INVALID_TATYPE` | 0x90 |
| 要求された操作がサポートされない。送信／受信の処理中でない N-SDU への、送信／受信のキャンセル要求 | ランタイム | `CANTP_E_OPER_NOT_SUPPORTED` | 0xA0 |
| 受信または送信の間の、プロトコルのタイムアウトエラー以外の、実装固有のエラーの場合に報告されるイベント | ランタイム | `CANTP_E_COM` | 0xB0 |
| 受信の間の、プロトコルのタイムアウトエラーの場合に報告されるイベント | ランタイム | `CANTP_E_RX_COM` | 0xC0 |
| 送信の間の、プロトコルのタイムアウトエラーの場合に報告されるイベント | ランタイム | `CANTP_E_TX_COM` | 0xD0 |

#### 7.4.3 一時的な故障

一時的な故障は無い。

#### 7.4.4 プロダクションエラー

プロダクションエラーは無い。

#### 7.4.5 拡張プロダクションエラー

拡張プロダクションエラーは無い。

### 7.5 エラーの検出

**[SWS_CanTp_00161]** BSW モジュールが初期化されているかを示す静的なステータス変数は、そのモジュールのいずれかの API が呼ばれる前に、値 0 で初期化されるべきである。BSW モジュールの初期化関数は、静的なステータス変数を、0 でない値に設定する。この変数は、API を呼ぶ前にモジュールが初期化されたかを検査するために使われる。（SRS_BSW_00406）

### 7.6 エラーの通知

**[SWS_CanTp_00294]** 開発エラーが起きた場合、CanTp モジュールは、値 `E_NOT_OK` を返し、（開発エラーの検出が有効なら）Default Error Tracer モジュールを介してエラーを報告しなければならない。

**[SWS_CanTp_00229]** As、Bs、Cs、Ar、Br、Cr のタイムアウトのためにタスクが中止された場合、CanTp モジュールは、DET エラー `CANTP_E_RX_COM`（受信の操作の場合）または `CANTP_E_TX_COM`（送信の操作の場合）を発生させなければならない。それ以外のプロトコルエラーのためにタスクが中止された場合、CanTp モジュールは、ランタイムエラーコード `CANTP_E_COM` を Default Error Tracer へ発生させなければならない。

### 7.7 AUTOSAR のデバッグの概念

詳細は、SWS_BSWGeneral の 7.1.17 項「デバッグのサポート」を参照すること。

## 8 API 仕様

### 8.1 インポートする型

この章では、次のファイルから取り込まれるすべての型を示す。

**[SWS_CanTp_00209]**

| モジュール | インポートする型 |
|---|---|
| ComStack_Types | `BufReq_ReturnType`、`PduIdType`、`PduInfoType`、`PduLengthType`、`RetryInfoType`、`TPParameterType` |
| Std_Types | `Std_ReturnType`、`Std_VersionInfoType` |

AUTOSAR の通信スタックで一貫した API を得るため、基本的な型が定義されている。これらの型は、CAN Transport Layer が、PDU Router および CAN Interface 層と通信するために使われる。詳細は、AUTOSAR の COM スタックの API 仕様に、これらの基本的な型が詳しく示されている。

これらの AUTOSAR の標準の型は、型の再定義なしに使われる。

**[SWS_CanTp_00002]** 実装上の理由で、追加の型を定義しなければならない場合、CanTp モジュールは、これらの型に `CanTp_<TypeName>Type` という名前を付けなければならない。ここで `<TypeName>` は、次の規則に従った、この型の名前である。

- アンダースコアを使わない。
- 各単語の最初の文字は大文字、続く文字は小文字。

（SRS_BSW_00353）

**[SWS_CanTp_00296]** CanTp モジュールは、実装固有の型が、CanTp の外から「見えない」ことを保証しなければならない。そうしないと、アーキテクチャ全体が壊れる。

### 8.2 型の定義

#### 8.2.1 CanTp_ConfigType

**[SWS_CanTp_00340]**

| 項目 | 内容 |
|---|---|
| 名前 | `CanTp_ConfigType` |
| 型 | 構造体 |
| 範囲 | 実装固有 |
| 説明 | ポストビルドのコンフィギュレーションパラメータのためのデータ構造の型。 |

ポストビルドのコンフィギュレーションパラメータのための、実装固有のデータ構造の型である。

### 8.3 関数の定義

上位層のモジュールへ提供される関数の一覧である。

#### 8.3.1 CanTp_Init

**[SWS_CanTp_00208]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_Init` |
| 構文 | `void CanTp_Init(const CanTp_ConfigType* CfgPtr)` |
| サービス ID [hex] | 0x01 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `CfgPtr`: CanTp のポストビルドのコンフィギュレーションデータへのポインタ。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | この関数は、CanTp モジュールを初期化する。 |

（SRS_BSW_00101、SRS_BSW_00358、SRS_BSW_00414）

電源投入後、CanTp は `CANTP_OFF` と呼ばれる状態にある（[SWS_CanTp_00168] を参照）。この状態では、CanTp はまだコンフィギュレーションされておらず、したがって、どの通信タスクも実行できない。

関数 `CanTp_Init` は、与えられたコンフィギュレーションセットで CAN Transport Layer のすべてのグローバル変数を初期化し、アイドルの状態（状態は `CANTP_ON` で、送信も受信も進行していない）にする（[SWS_CanTp_00170] と [SWS_CanTp_00030] を参照）。

関数 `CanTp_Init` に戻り値が無いのは、コンフィギュレーションデータのエラーが、コンフィギュレーションの時点で（例: コンフィギュレーションツールにより）検出されるべきであるためである。また、ハードウェアエラーが起きた場合は、エラーマネージャのモジュールを介して報告される。

**[SWS_CanTp_00199]** CanTp モジュールの環境は、CanTp モジュールを以降の処理に使う前に、`CanTp_Init` を呼ばなければならない。

初期化関数のパラメータの検査は、BSW General [13] の中で規定されている。

#### 8.3.2 CanTp_GetVersionInfo

**[SWS_CanTp_00210]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_GetVersionInfo` |
| 構文 | `void CanTp_GetVersionInfo(Std_VersionInfoType* versioninfo)` |
| サービス ID [hex] | 0x07 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | `versioninfo`: このモジュールのバージョン情報を格納する場所を示す。 |
| 戻り値 | なし |
| 説明 | この関数は、CanTp モジュールのバージョン情報を返す。 |

> **注（再構成）**: 原典の表は、テキストの抽出で行がずれていたため、項目の対応を復元した。

**[SWS_CanTp_00319]** DET が有効な場合、関数 `CanTp_GetVersionInfo` は、引数が NULL ポインタであれば `CANTP_E_PARAM_POINTER` エラーを発生させ、何もせずに戻らなければならない。

#### 8.3.3 CanTp_Shutdown

**[SWS_CanTp_00211]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_Shutdown` |
| 構文 | `void CanTp_Shutdown(void)` |
| サービス ID [hex] | 0x02 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | この関数は、CanTp モジュールをシャットダウンするために呼ばれる。 |

**[SWS_CanTp_00202]** 関数 `CanTp_Shutdown` は、保留中のすべてのトランスポートプロトコルの接続を閉じ、すべてのリソースを解放し、CanTp モジュールを `CANTP_OFF` の状態にしなければならない。

**[SWS_CanTp_00200]** 関数 `CanTp_Shutdown` は、保留中のフレームの送信または受信についての通知を、発生させてはならない。

#### 8.3.4 CanTp_Transmit

**[SWS_CanTp_00212]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_Transmit` |
| 構文 | `Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x49 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信する PDU の識別子。<br>`PduInfoPtr`: PDU のデータの長さとポインタ、およびメタデータへのポインタ。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 送信要求が受理された。<br>`E_NOT_OK`: 送信要求が受理されなかった。 |
| 説明 | PDU の送信を要求する。 |

> **注（再構成）**: 原典の表は、テキストの抽出で行がずれていたため、項目の対応を復元した。

**[SWS_CanTp_00231]** データが、対応する N-PDU に収まる場合、関数 `CanTp_Transmit()` は、SF の N-PDU を送らなければならない。（SRS_Can_01071、SRS_Can_01074）

**[SWS_CanTp_00232]** データが、対応する N-PDU に収まらない場合、関数 `CanTp_Transmit()` は、複数フレームの送信セッションを開始しなければならない。（SRS_Can_01071、SRS_Can_01074）

**[SWS_CanTp_00354]** N-PDU の最大の Tx 長は、EcuC のコンフィギュレーションパラメータ `PduLength` から導かなければならない。このパラメータは、ISO 15765-2 の TX_DL に相当する。（SRS_Can_01163）

**[SWS_CanTp_00204]** 送信要求が完了したとき、CanTp モジュールは、コールバック `PduR_CanTpTxConfirmation` を呼んで、上位層へ通知しなければならない。

**[SWS_CanTp_00205]** エラー（オーバーフロー、N_As のタイムアウト、N_Bs のタイムアウトなど）が起きた場合、CanTp モジュールは、送信要求を中止し、適切なエラーの結果値でコールバック関数 `PduR_CanTpTxConfirmation` を呼ばなければならない。

エラーの結果値は、ISO 15765 に従って定義されている。この型の定義は、ComStackTypes の文書（AUTOSAR_SWS_ComStackTypes、8.1.5 節）に示されている。

**[SWS_CanTp_00206]** 現在動作中の CAN Transport Layer のセッションで使われている N-SDU の識別子に対して、サービス `CanTp_Transmit` が呼ばれた場合、関数 `CanTp_Transmit` は、その要求を拒否しなければならない。

**[SWS_CanTp_00298]** CanTp のバッファリングの能力は限られているため、送信する N-SDU のペイロードは、内部へコピーされない。CAN Transport Layer は、サービス `PduR_CanTpCopyTxData` を介して、上位層から直接データを得る。

したがって、データの一貫性を保証するために、上位層（例: DCM、PduRouter、AUTOSAR COM）は、確認の通知が起きるまで、このメモリ領域をロックしなければならない。

**[SWS_CanTp_00299]** 上位層が、メタデータを持たない N-SDU のために、この関数を呼ぶとき、`PduInfoPtr` が示す構造体のうち、データ長の情報だけが使われなければならない。その値は、送信する N-SDU のペイロード長を示す。メタデータを持つ N-SDU では、長さの情報に加えて、`PduInfoPtr` を介して渡されるメタデータも関係する。実際の Tx データを得るために、CAN Transport Layer は、サービス `PduR_CanTpCopyTxData` を呼ばなければならない。

**[SWS_CanTp_00321]** DET が有効な場合、関数 `CanTp_Transmit` は、引数 `PduInfoPtr` が NULL ポインタであれば `CANTP_E_PARAM_POINTER` エラーを発生させ、何もせずに戻らなければならない。

#### 8.3.5 CanTp_CancelTransmit

**[SWS_CanTp_00246]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_CancelTransmit` |
| 構文 | `Std_ReturnType CanTp_CancelTransmit(PduIdType TxPduId)` |
| サービス ID [hex] | 0x4a |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: キャンセルする PDU の識別。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: キャンセルが、宛先のモジュールにより正常に実行された。<br>`E_NOT_OK`: キャンセルが、宛先のモジュールにより拒否された。 |
| 説明 | 下位層の通信モジュールで進行中の PDU の送信の、キャンセルを要求する。 |

このサービスは、サービス `CanTp_Transmit` を呼んで、すでに送信を要求した N-SDU の送信をキャンセルする。

**[SWS_CanTp_00254]** 開発エラーの検出が有効な場合、関数 `CanTp_CancelTransmit` は、パラメータ `TxPduId` の有効性を検査しなければならない。パラメータの値が無効な場合、関数 `CanTp_CancelTransmit` は、開発エラー `CANTP_E_PARAM_ID` を発生させ、`E_NOT_OK` を返さなければならない（[SWS_CanTp_00294] を参照）。パラメータの値が、送信の処理中でない N-SDU への、送信のキャンセル要求を示している場合、CanTp モジュールは、ランタイムエラーコード `CANTP_E_OPER_NOT_SUPPORTED` を Default Error Tracer へ報告し、サービスは `E_NOT_OK` を返さなければならない。

**[SWS_CanTp_00256]** サービスが `E_OK` を返す場合、CanTp は、現在の N-SDU の送信を中止しなければならない。

**[SWS_CanTp_00255]** サービス `CanTp_CancelTransmit` が正常に実行された場合、CanTp は、通知の結果 `E_NOT_OK` で `PduR_CanTpTxConfirmation` を呼ばなければならない。

#### 8.3.6 CanTp_CancelReceive

**[SWS_CanTp_00257]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_CancelReceive` |
| 構文 | `Std_ReturnType CanTp_CancelReceive(PduIdType RxPduId)` |
| サービス ID [hex] | 0x4c |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `RxPduId`: キャンセルする PDU の識別。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: キャンセルが、宛先のモジュールにより正常に実行された。<br>`E_NOT_OK`: キャンセルが、宛先のモジュールにより拒否された。 |
| 説明 | 下位層のトランスポートプロトコルのモジュールで進行中の PDU の受信の、キャンセルを要求する。 |

サービス `CanTp_CancelReceive` は、First Frame の受信と、それに続く `PduR_StartOfReception` の呼び出しで始まった N-SDU の受信をキャンセルする。関数が戻ったとき、指定された N-SDU の識別子で進行中の受信は、もう無い。

**[SWS_CanTp_00260]** 開発エラーの検出が有効な場合、関数 `CanTp_CancelReceive` は、パラメータ `RxPduId` の有効性を検査しなければならない。パラメータの値が無効な場合、関数 `CanTp_CancelReceive` は、開発エラー `CANTP_E_PARAM_ID` を発生させ、`E_NOT_OK` を返さなければならない（[SWS_CanTp_00294] を参照）。パラメータの値が、受信の処理中でない N-SDU への、受信のキャンセル要求を示している場合、CanTp モジュールは、ランタイムエラーコード `CANTP_E_OPER_NOT_SUPPORTED` を Default Error Tracer へ報告し、サービスは `E_NOT_OK` を返さなければならない。

**[SWS_CanTp_00261]** サービスが `E_OK` を返す場合、CanTp は、現在の N-SDU の受信を中止しなければならない。

**[SWS_CanTp_00262]** CanTp は、Single Frame の受信の場合、または CanTp が N-SDU の最後の Consecutive Frame を受信している最中の場合（すなわち、最後の Consecutive Frame のために N_Cr のタイムアウトが開始された後に、サービスが呼ばれた場合）、受信のキャンセルの要求を拒否しなければならない。この場合、CanTp は `E_NOT_OK` を返さなければならない。

**[SWS_CanTp_00263]** サービス `CanTp_CancelReceive` が正常に実行された場合、CanTp は、通知の結果 `E_NOT_OK` で `PduR_CanTpRxIndication` を呼ばなければならない。

#### 8.3.7 CanTp_ChangeParameter

**[SWS_CanTp_00302]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_ChangeParameter` |
| 構文 | `Std_ReturnType CanTp_ChangeParameter(PduIdType id, TPParameterType parameter, uint16 value)` |
| サービス ID [hex] | 0x4b |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `id`: パラメータの変更の対象となる PDU の識別。<br>`parameter`: 変更するパラメータの ID。<br>`value`: パラメータの新しい値。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: パラメータが正常に変更された。<br>`E_NOT_OK`: パラメータの変更が拒否された。 |
| 説明 | 特定のトランスポートプロトコルのパラメータ（例: ブロックサイズ）の変更を要求する。 |

サービス `CanTp_ChangeParameter` は、受信する各 N-SDU に結び付けられた、受信パラメータ BS と STmin の値を変更するために使われる。

このサービスの実装は、コンフィギュレーションパラメータ `CanTpChangeParameterApi` に依存する（すなわち、パラメータが `TRUE` に設定されているとき、サービスは実装されなければならない）。

**[SWS_CanTp_00303]** パラメータの変更は、関連する N-SDU が受信の処理中でない場合にだけ可能である。すなわち、FF の受信の後から、関連する N-SDU の最後の CF の受信の通知までの間は、パラメータの値を変更できない。

**[SWS_CanTp_00304]** 受信の処理中の N-SDU について、パラメータの変更が要求された場合、サービス `CanTp_ChangeParameter` は、直ちに `E_NOT_OK` を返し、パラメータの値は変更されない。

**[SWS_CanTp_00338]** メタデータを持つ N-SDU（汎用の接続を示す）に対して `CanTp_ChangeParameter` が呼ばれたとき、変更はすべての汎用の接続に適用され、以降のすべての受信に使われなければならない。

**[SWS_CanTp_00305]** 開発エラーの検出が有効な場合、関数 `CanTp_ChangeParameter` は、関数のパラメータ（識別子、パラメータ、要求された値）の有効性を検査しなければならない。いずれかのパラメータの値が無効な場合、関数 `CanTp_ChangeParameter` は、開発エラー `CANTP_E_PARAM_ID` を発生させ、`E_NOT_OK` を返さなければならない（[SWS_CanTp_00294] を参照）。

#### 8.3.8 CanTp_ReadParameter

**[SWS_CanTp_00323]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_ReadParameter` |
| 構文 | `Std_ReturnType CanTp_ReadParameter(PduIdType id, TPParameterType parameter, uint16* value)` |
| サービス ID [hex] | 0x0b |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `id`: 受信パラメータを読む対象の、受信した N-SDU の識別子。<br>`parameter`: 値を読む対象のパラメータの指定（BS または STmin）。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `value`: パラメータの値が渡されるポインタ。 |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: 要求が受理された。<br>`E_NOT_OK`: 要求が受理されなかった。 |
| 説明 | このサービスは、指定した N-SDU の、受信パラメータ BS と STmin の現在の値を読むために使われる。 |

このサービスの実装は、コンフィギュレーションパラメータ `CanTpReadParameterApi` に依存する（すなわち、パラメータが `TRUE` に設定されているとき、サービスは実装されなければならない）。

**[SWS_CanTp_00324]** 開発エラーの検出が有効な場合、関数 `CanTp_ReadParameter` は、関数のパラメータ（`Id` と `Parameter`）の有効性を検査しなければならない。いずれかのパラメータの値が無効な場合、関数 `CanTp_ReadParameter` は、開発エラー `CANTP_E_PARAM_ID` を発生させ、`E_NOT_OK` を返さなければならない（[SWS_CanTp_00294] を参照）。

#### 8.3.9 メイン関数

**[SWS_CanTp_00213]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_MainFunction` |
| 構文 | `void CanTp_MainFunction(void)` |
| サービス ID [hex] | 0x06 |
| 説明 | CAN TP をスケジューリングするためのメイン関数。 |

**[SWS_CanTp_00164]** CAN TP をスケジューリングするためのメイン関数（スケジューリングのエントリポイント）。メイン関数は、必要な呼び出し周期に応じて、Schedule Manager または Free Running Timer モジュールから呼ばれる。`CanTp_MainFunction` は、CAN TP のタイムアウト N_As、N_Bs、N_Cs、N_Ar、N_Br、N_Cr、および STmin の処理に関わる。（SRS_BSW_00424、SRS_BSW_00373）

**[SWS_CanTp_00300]** 関数 `CanTp_MainFunction` は、コンフィギュレーションパラメータ `CanTpMainFunctionPeriod` の影響を受ける。

### 8.4 コールバック通知

下位層のモジュールへ提供される関数の一覧である。

#### 8.4.1 CanTp_RxIndication

**[SWS_CanTp_00214]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_RxIndication` |
| 構文 | `void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x42 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `RxPduId`: 受信した PDU の ID。<br>`PduInfoPtr`: 受信した PDU の長さ（`SduLength`）、PDU を含むバッファへのポインタ（`SduDataPtr`）、およびこの PDU に関するメタデータを含む。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |

CanIf モジュールは、Rx の CAN L-PDU の受信が成功した後に、この関数を呼ばなければならない。

データは、PDU 構造体 `PduInfoType` を介して、CanTp によりコピーされる。この場合、L-PDU のバッファはグローバルではなく、CAN Transport Layer の中に分散している。

動的な Rx N-PDU の場合は、`PduInfoPtr` にメタデータも含まれることに注意。

**[SWS_CanTp_00235]** 関数 `CanTp_RxIndication` は、割り込みのコンテキストで呼べなければならない（CAN の受信割り込みから呼ばれうる）。

**[SWS_CanTp_00322]** DET が有効な場合、関数 `CanTp_RxIndication` は、引数 `PduInfoPtr` が NULL ポインタであれば `CANTP_E_PARAM_POINTER` エラーを発生させ、何もせずに戻らなければならない。

#### 8.4.2 CanTp_TxConfirmation

**[SWS_CanTp_00215]**

| 項目 | 内容 |
|---|---|
| サービス名 | `CanTp_TxConfirmation` |
| 構文 | `void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)` |
| サービス ID [hex] | 0x40 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信された PDU の ID。<br>`result`: `E_OK`: PDU が送信された。`E_NOT_OK`: PDU の送信に失敗した。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |

CanIf モジュールは、TP に関する CAN フレーム（SF、FF、CF、FC）が CAN ネットワークを通して送信された後に、関数 `CanTp_TxConfirmation` を呼ばなければならない。

**[SWS_CanTp_00236]** 関数 `CanTp_TxConfirmation` は、割り込みのコンテキストで呼べなければならない（CAN の送信割り込みから呼ばれうる）。

### 8.5 期待するインタフェース

この章では、他のモジュールに要求されるすべてのインタフェースを列挙する。

#### 8.5.1 必須のインタフェース

この章では、モジュールの中核の機能を満たすために必要な、すべてのインタフェースを定義する。

**[SWS_CanTp_00216]**

| API 関数 | 説明 |
|---|---|
| `CanIf_Transmit` | PDU の送信を要求する。 |
| `Det_ReportRuntimeError` | ランタイムエラーを報告するサービス。コールアウトがコンフィギュレーションされていれば、このコールアウトが呼ばれなければならない。 |
| `PduR_CanTpCopyRxData` | I-PDU のセグメント（N-PDU）の受信データを、上位層へ渡すために呼ばれる関数。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。残りのバッファのサイズは、`bufferSizePtr` が示す位置に書かれる。 |
| `PduR_CanTpCopyTxData` | I-PDU のセグメント（N-PDU）の送信データを取得するために呼ばれる関数。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。ただし `retry->TpDataState` が `TP_DATARETRY` の場合を除く。この場合、関数は、`retry->TxTpDataCnt` が示す現在の位置からのオフセットで始まるデータのコピーをやり直す。残りのデータのサイズは、`availableDataPtr` が示す位置に書かれる。 |
| `PduR_CanTpRxIndication` | TP API を介して I-PDU が受信された後に呼ばれる。結果が、送信が成功したかを示す。 |
| `PduR_CanTpStartOfReception` | N-SDU の受信の開始時に呼ばれる関数。N-SDU は、複数の N-PDU に分割される場合（FF と、それに続く 1 つ以上の CF）と、1 つの N-PDU から成る場合（SF）がある。`TpSduLength` が 0 で呼ばれたとき、このサービスは、現在利用できる最大のバッファサイズを提供しなければならない。 |
| `PduR_CanTpTxConfirmation` | I-PDU がそのネットワークで送信された後に呼ばれる関数。結果が、送信が成功したかを示す。 |

#### 8.5.2 オプションのインタフェース

この章では、モジュールのオプションの機能を満たすために必要な、インタフェースを定義する。

**[SWS_CanTp_00217]**

| API 関数 | 説明 |
|---|---|
| `Det_ReportError` | 開発エラーを報告するサービス。 |

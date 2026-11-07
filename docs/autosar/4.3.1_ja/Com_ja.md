# Communication（Com）仕様書 日本語訳

- 原典: AUTOSAR CP Release 4.3.1 `AUTOSAR_SWS_COM.pdf`（Document ID 015）
- 範囲: 第7章（機能仕様）と第8章（API 仕様）。第1〜6章（導入、関連文書、制約、依存関係、トレーサビリティ）、第9章（シーケンス図）、第10章以降（コンフィギュレーション）は対象外。
- 凡例: `[SWS_Com_xxxxx]` は原典の条項 ID。`（SRS_…）` は原典が条項に付けている上位要求の ID。`ECUC_Com_xxxxx` はコンフィギュレーションパラメータの ID。`[13]`、`[17]` などは、原典の参考文献の番号（[17] は OSEK/VDX COM、ISO 17356-4:2005）。
- 図は訳せないため、位置に注記だけを残している。図の内容は原典を参照すること。
- 「学習メモ」は原文の訳ではなく、このプロジェクトの実装と照らすための訳者の解釈。

---

## 7 機能仕様

### 7.1 はじめに

（原典に本文なし）

### 7.2 一般的な機能

#### 7.2.1 AUTOSAR COM の基礎

ISO 17356-4:2005「Road vehicles -- Open interface for embedded automotive applications -- Part 4: OSEK/VDX Communication (COM)」が、AUTOSAR COM モジュールの機能の基礎である。この文書では、[17] として参照する。

**[SWS_Com_00010]** AUTOSAR COM モジュールは、[SWS_Com_00013] に挙げた機能と API を除き、[17] のすべての機能とすべての API を実装しなければならない。（SRS_Com_02037）

**[SWS_Com_00011]** この AUTOSAR COM 仕様が、[17] の定義とは異なる形で機能を定義している場合、AUTOSAR COM モジュールは、この AUTOSAR COM 仕様で定義された機能を実装しなければならない。（SRS_Com_02037）

**[SWS_Com_00012]** AUTOSAR COM モジュールは、さらに、この AUTOSAR COM 仕様で定義され、[17] に含まれないすべての機能を実装しなければならない。（SRS_Com_02037）

**[SWS_Com_00013]** AUTOSAR COM モジュールは、[17] の次の機能を実装してもよい。特定の AUTOSAR COM モジュールでそれらが実装される場合、コンフィギュレーションで、デフォルトでは無効にしなければならない。これは、特定の実装が提供する、他のすべての追加の機能にも当てはまる。（SRS_Com_02037）

> **注（再構成）**: 原典の表 1「AUTOSAR COM モジュールで除外される [17] の機能」は、テキストの抽出で、3 つの列の内容が混ざっていた。機能の名前と理由は読み取れたが、「[17] の関連する API」の列は、機能との対応を確定できなかった。API の名前だけを表の後ろにまとめた。重要な箇所は原典の表 1（29〜30 ページ）で確認すること。

| [17] の機能 | 理由 |
|---|---|
| 受信したネットワークメッセージ（I-PDU の中）を、複数のメッセージデータオブジェクトへ割り当てること（1:n の分割の仕組み） | 不要。RTE が行う（[13] を参照）。 |
| 内部のメッセージを、複数のメッセージデータオブジェクトへ割り当てること（1:n の分割の仕組み） | 不要。RTE が行う（[13] を参照）。 |
| ローカルにだけ送るメッセージを、外部の送信メッセージオブジェクトと、内部の受信メッセージオブジェクトの両方へ割り当てること（1:n の分割の仕組み） | 不要。RTE により保証される（[13] を参照）。 |
| M:1 の送信。複数の送信元からのメッセージを、同じ 1 つのメッセージオブジェクトへ割り当てること | 不要。RTE が行う（[13] を参照）。 |
| キューイングされたメッセージ | （原典の抽出で理由を確定できず） |
| サイズゼロのメッセージ | これらなしで、通信を設定できる。 |
| 通知の仕組み TASK、FLAG、EVENT | 機能は、`Com_TriggerTransmit` で部分的にカバーされる。 |
| I-PDU の中での、メッセージの重なり | 不要。RTE が行う（[13] を参照）。 |
| OIL の使用 | AUTOSAR COM モジュールのコンフィギュレーションに、OIL は使われない。 |
| アプリケーションモード | 使用例が無く、危険な概念。 |
| 起動時の動作 | `Com_Init`、`Com_DeInit`、`Com_IpduGroupStart`、`Com_IpduGroupStop` に置き換えられた。 |
| 周期メッセージの開始と停止 | 使用例が無い。I-PDU グループの仕組みで実現される。 |
| 再入可能性 | AUTOSAR の API の呼び出しは、すべてが再入可能ではない。8.3 節を参照。 |
| 間接 NM へのインタフェース | 不要。 |
| 送信側のフィルタリング | 使用例が無い。フィルタ条件は、送信モードの選択に使われるが、信号のフィルタリングは無い。 |
| ネットワーク順のメッセージのコールアウト | AUTOSAR COM モジュールは、AUTOSAR のインタフェースが定義された I-PDU のコールアウトだけをサポートする。独自の解決策を避けるためである。 |
| CPU 順のメッセージのコールアウト | 同上。 |
| エラーフックのルーチン | AUTOSAR COM モジュールは、エラーフックを使う代わりに、エラー報告のための独自のインタフェースを定義している。 |
| コールバックのルーチンのインタフェース | AUTOSAR COM モジュールで使われるコールバック関数のシグネチャは、AUTOSAR COM モジュールの仕様の中で、明示的に定義される。 |
| 内部の通信 | 不要。RTE により保証される（[13] を参照）。 |

表の「[17] の関連する API」の列に現れる名前（機能との対応は未確定）: `GetMessageStatus`、`SendZeroMessage`、`GetComApplicationMode`、`StartCOM`、`StopCOM`、`StartCOMExtensions`、`InitMessage`、`StartPeriodic`、`StopPeriodic`、`I_MessageTransfer`、`I_MessageTimeOut`、`COMErrorHook`、`COMError_Name1_Name2` マクロ、`COMErrorGetServiceId`、`COMCallback`、`SendMessage`、`ReceiveMessage`。

#### 7.2.2 信号の値

AUTOSAR COM モジュールが送る、または受信する信号は、表 2 で定義された値を取りうる。

**表 2: 信号の取りうる値**

| 信号の値 | 備考 |
|---|---|
| 初期値（init value） | 詳細は 7.3.1.4 節を参照。 |
| データ無効値（Data invalid value） | 詳細は 7.3.4 節を参照。 |
| `<value>` | 通常の場合: 初期化の段階の後の有効な値で、AUTOSAR COM モジュールが送る、または AUTOSAR COM モジュールが受信するもの。 |

#### 7.2.3 エンディアン変換と符号拡張

**[SWS_Com_00675]** AUTOSAR COM モジュールは、次のデータ型をサポートしなければならない。

- `boolean`
- `uint8`
- `uint16`
- `uint32`
- `uint64`
- `sint8`
- `sint16`
- `sint32`
- `sint64`
- `uint8[n]`
- `float32`
- `float64`

型 `uint8[n]` は、`ComSignalType` の `UINT8_N` または `UINT8_DYN` のいずれかに割り当てられる。（SRS_Com_02078、SRS_Com_02094）

**[SWS_Com_00007]** AUTOSAR COM モジュールは、サポートするすべての符号付き・符号なしの整数のデータ型（[SWS_Com_00675] を参照）に対して、エンディアン変換をサポートしなければならない。（SRS_Com_02078）

**[SWS_Com_00810]** AUTOSAR COM モジュールは、整数でないデータ型（例: `uint8[n]` や `float32`）を、同じサイズの整数のデータ型と同じように扱うか、`ComSignalEndianness` が `OPAQUE` にコンフィギュレーションされている場合は、内容を解釈せずにそのままにしなければならない。（SRS_Com_02078）

**[SWS_Com_00472]** AUTOSAR COM モジュールは、不透明（opaque）なデータを `uint8[n]` として解釈し、常に n バイトの大きさの信号へ割り当てなければならない。（SRS_Com_02078）

不透明なデータのエンディアンでは、変換を `OPAQUE` にコンフィギュレーションする必要がある（ECUC_Com_00157 を参照）。

**[SWS_Com_00674]** AUTOSAR COM モジュールは、[17] の 2.4 章で定義されたエンディアン変換を、符号付きのデータ型へ拡張しなければならない。（SRS_Com_02078）

[17] の 2.4 章は、符号なしのデータ型のエンディアン変換を定義している。関連するコンフィギュレーションは、第 10 章にある。ECUC_Com_00127 と ECUC_Com_00157 も参照すること。

**[SWS_Com_00829]** AUTOSAR COM モジュールは、[17] の 2.4 章で定義されたエンディアン変換を、信号グループの信号へ拡張しなければならない。（SRS_Com_02086）

AUTOSAR COM は、エンディアン変換に関して、信号とグループ信号を同じように扱う。

**[SWS_Com_00008]** AUTOSAR COM モジュールは、受信したデータを、受信信号の `ComSignalType` のサイズへ拡張しなければならない（符号拡張）。（SRS_Com_02086）

符号付きデータのプラットフォーム固有の表現を、考慮する必要がある。符号付きデータの負の値は、正しく割り当てられる。

例: 10 ビットの符号付き信号を受信し、`Com_ReceiveSignal` で 16 ビットの符号付き整数変数へコピーする。(−3) の十進数を受信した場合、受信した 10 ビットの信号の値は `1111111101b` である。16 ビットの整数変数へコピーするとき、値は `1111111111111101b` へ拡張される。

**[SWS_Com_00723]** AUTOSAR COM モジュールは、信号の初期値（`ComSignalInitValue`）を、その `ComSignalType` のサイズへ拡張しなければならない。（SRS_Com_02086）

送信側では、送信される値が常に符号拡張済みであることが保証されているため、AUTOSAR COM モジュールが符号拡張を行う必要は無いことに注意。[SWS_Com_00785] も参照すること。

**[SWS_Com_00579]** AUTOSAR COM モジュールは、`float32` と `float64` のデータ型の符号拡張をサポートしてはならない。（SRS_Com_02086）

**[SWS_Com_00221]** AUTOSAR COM モジュールは、送信側で、I-PDU のコールアウトの前に、エンディアン変換を行わなければならない。概要は 7.15 節を参照。（SRS_Com_02078）

**[SWS_Com_00352]** AUTOSAR COM モジュールは、受信側で、フィルタリングと通知の検出を行う前に、符号拡張とエンディアン変換を行わなければならない。（SRS_Com_02078、SRS_Com_02086）

**[SWS_Com_00580]** AUTOSAR COM モジュールは、エンディアン変換以外の追加の変換を、`ComSignalType` が `FLOAT32` または `FLOAT64` の信号に対してサポートしてはならない。すなわち、エンディアン変換はサポートされなければならないが、小数部、指数部、符号、バイアス値の複雑な変換や正規化は、サポートされない。（SRS_Com_02078）

#### 7.2.4 フィルタリング

**[SWS_Com_00694]** AUTOSAR COM モジュールは、各フィルタ条件を、真または偽のいずれかに評価しなければならない。（SRS_Com_02037）

**[SWS_Com_00695]** AUTOSAR COM モジュールは、受信側でだけ、信号を除外（フィルタアウト）しなければならない。（SRS_Com_02037）

**[SWS_Com_00602]** AUTOSAR COM モジュールは、送信側では、送信モード条件（TMC）のためにフィルタリングの仕組みを使わなければならないが、送信側で信号を除外してはならない。（SRS_Com_02083）

送信モードの選択（TMS）は、7.3.3.2 節と 7.3.3.3 節を参照。

AUTOSAR COM モジュールは、[17] で定義されたもののうち、次の `ComFilterAlgorithm` だけを提供する（ECUC_Com_00146 を参照）。

- `ALWAYS`
- `NEVER`
- `MASKED_NEW_EQUALS_X`
- `MASKED_NEW_DIFFERS_X`
- `MASKED_NEW_DIFFERS_MASKED_OLD`
- `NEW_IS_WITHIN`
- `NEW_IS_OUTSIDE`
- `ONE_EVERY_N`

複雑さを減らすため、AUTOSAR COM モジュールは、[17] で定義されたすべてのフィルタをサポートするわけではない。サポートされないフィルタは、廃止されたものか、他のフィルタの特殊な場合である。例えば、フィルタ `NEW_IS_DIFFERENT` は、マスクをすべて立てた `MASKED_NEW_DIFFERS_MASKED_OLD` の特殊な場合である。

**[SWS_Com_00325]** AUTOSAR COM モジュールは、[SWS_Com_00380] と [SWS_Com_00439] で定義された例外を考慮して、ECUC_Com_00146 に挙げられたすべてのフィルタの仕組みをサポートしなければならない。（SRS_Com_02037）

**[SWS_Com_00380]** `ComSignalType` が `FLOAT32`、`FLOAT64`、`UINT8_N`、または `UINT8_DYN` の信号では、AUTOSAR COM モジュールは、`ALWAYS`、`NEVER`、または `ONE_EVERY_N` にコンフィギュレーションされた `ComFilterAlgorithm` だけをサポートしなければならない。（SRS_Com_02037）

**[SWS_Com_00439]** `ComSignalType` が `BOOLEAN` にコンフィギュレーションされた信号では、AUTOSAR COM モジュールは、次のいずれかにコンフィギュレーションされた `ComFilterAlgorithm` だけをサポートしなければならない。

- `ALWAYS`
- `NEVER`
- `MASKED_NEW_EQUALS_X`
- `MASKED_NEW_DIFFERS_X`
- `MASKED_NEW_DIFFERS_MASKED_OLD`
- `ONE_EVERY_N`

（SRS_Com_02037）

**[SWS_Com_00764]** `ComBitSize` が 0 にコンフィギュレーションされた信号とグループ信号では、AUTOSAR COM モジュールは、フィルタアルゴリズム `Masked_New_Differs_Masked_Old` をサポートしてはならない。（SRS_Com_02037）

**[SWS_Com_00866]** コンフィギュレーションされたフィルタ条件が `MASKED_NEW_DIFFERS_X`、`MASKED_NEW_EQUALS_X`、`NEW_IS_OUTSIDE`、または `NEW_IS_WITHIN` の場合、AUTOSAR COM モジュールは、フィルタのパラメータの下位 `ComBitSize` ビットだけを使って、受信フィルタを計算しなければならない。（SRS_Com_02037）

**[SWS_Com_00273]** AUTOSAR COM モジュールが、受信側で信号を除外した場合、すなわち、フィルタ条件が偽に評価された場合、AUTOSAR COM モジュールは、その信号を破棄し、処理してはならない。[SWS_Com_00303] も参照すること。（SRS_Com_02037）

**[SWS_Com_00836]** AUTOSAR COM モジュールが、受信側でグループ信号を除外した場合、すなわち、フィルタ条件が偽に評価された場合、AUTOSAR COM モジュールは、信号グループ全体を破棄し、処理してはならない。（SRS_Com_02037）

**[SWS_Com_00132]** AUTOSAR COM モジュールは、ECUC_Com_00146 で定義されたフィルタの仕組みを、符号付きのデータ型に対してもサポートしなければならない。（SRS_Com_02037）

対応する信号を送信 API が書き込む前にフィルタが評価される場合、この信号のフィルタの状態を決める方法が必要である。フィルタの中には、評価に new_value を必要とするものがある。しかし、new_value は、送信 API を使って信号が更新された後でなければ得られない。したがって、最初の送信が行われるまでの期間に、フィルタが new_value として使う値を定義する必要がある。

**[SWS_Com_00603]** AUTOSAR COM モジュールは、起動時に、各信号について、フィルタの仕組みの old_value を `ComSignalInitValue`（ECUC_Com_00170）に設定しなければならない。[17] も参照すること。（SRS_Com_02037）

**[SWS_Com_00604]** アプリケーションが、ある信号のフィルタの仕組みの new_value をまだ更新していない間、AUTOSAR COM モジュールは、その信号の new_value として `ComSignalInitValue` を使わなければならない。（SRS_Com_02037）

次の 2 つの要求は、フィルタの old_value の更新に関する [17] の定義を明確にする。

**[SWS_Com_00302]** AUTOSAR COM モジュールが、ある信号についてフィルタを真に評価した場合（値が除外されない場合）、AUTOSAR COM モジュールは、その信号の値を old_value に置かなければならない（[17] の定義どおり）。（SRS_Com_02037）

**[SWS_Com_00303]** 値がフィルタにかけられるとき、フィルタが値の通過を許可しない場合（すなわち、フィルタが偽に評価される場合）、AUTOSAR COM モジュールは、その値を old_value に置いてはならない（[17] の定義どおり）。（SRS_Com_02037）

**[SWS_Com_00231]** `ComFilterAlgorithm` が `ONE_EVERY_N` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、次のようにしなければならない。

- `OCCURRENCE == PERIOD` のとき、`OCCURRENCE` を 0 に設定する。
- `OCCURRENCE == OFFSET` のとき、`FILTER` を真に設定する。
- フィルタの処理の後に、`OCCURRENCE` を増やす。

（SRS_Com_02037）

`OCCURRENCE`、`FILTER`、`OFFSET`、`PERIOD` の定義は、[17] を参照すること。

信号に `ComFilterAlgorithm` を `ONE_EVERY_N` にコンフィギュレーションすると、信号は、フィルタが `PERIOD` 回呼ばれるごとに 1 回、フィルタを通過する（すなわち、フィルタが真を返す）。`OFFSET` のパラメータが 0 の場合、フィルタが最初に使われたときに、信号の通過が許可される（すなわち、フィルタが真を返す）。`OFFSET` が 0 より大きい場合、フィルタが真を返す前に、複数の信号が通過しなければならない。

I-PDU が、パラメータ `initialize` を真にした `Com_IpduGroupStart` で開始された場合も、`OCCURRENCE` は 0 に設定される。[SWS_Com_00787] を参照。

この定義は、[17] の `ONE_EVERY_N` フィルタの説明を明確にするために存在する。

関連するコンフィギュレーションの項目は、第 10 章にある。ECUC_Com_00339 を参照。

**[SWS_Com_00793]** 受信フィルタ `MASKED_NEW_DIFFERS_MASKED_OLD` がコンフィギュレーションされた信号では、AUTOSAR COM モジュールは、この信号に受信デッドライン監視のタイムアウトが起きた後に、この信号で最初に受信した値を、その値がフィルタの基準を通過した場合と同じように扱わなければならない。（SRS_Com_02037）

したがって、AUTOSAR COM モジュールは、対応する I-PDU に Rx デッドラインのタイムアウトが起きた後は、フィルタ `MASKED_NEW_DIFFERS_MASKED_OLD` について、どの値も通過させる。

#### 7.2.5 信号ゲートウェイ

AUTOSAR COM モジュールは、信号と信号グループを 1:n の形で転送するための、統合された信号ゲートウェイを提供する。

経路の関係は、コンフィギュレーションコンテナ `ComGwMapping`（ECUC_Com_00544 を参照）で、静的にコンフィギュレーションされる。

図 5 と図 6 に示すように、統合された信号ゲートウェイは、ゲートウェイされる信号の送信元としてコンフィギュレーションされた、すべての信号または信号グループの、受信側として動作する。

信号ゲートウェイは、経路付けのための信号または信号グループを受信した後、直ちに、それらの信号または信号グループの送信側として動作する。これは、図 4 と図 6 に示されている。統合された信号ゲートウェイが、信号／信号グループを転送する場合と、ソフトウェアコンポーネントが送る場合とで、信号の処理は異ならない。

ゲートウェイの関係は静的にコンフィギュレーションされるため、最適化された信号ゲートウェイは、具体的なコンフィギュレーションでは、処理の段階をいくつか省略してもよい。例えば、特定の場合には、エンディアン変換は不要である。もちろん、そのような最適化が、論理的な動作の違いや、対象のバスでの表現の違いにつながってはならない。最適化は、信号ゲートウェイの性能を高めるためだけに、導入してよい。

図 5 に示すように、受信した信号または信号グループは、ローカルの ECU で受信されると同時に、ゲートウェイの動作の送信元になりうる。

図 4 と図 5 は、RTE が受信する信号グループに対してだけ、シャドウバッファを示している。統合された信号ゲートウェイは、もちろん、信号グループを一貫して扱うことを保証しなければならないが、それをどのように実装しなければならないかは、あらかじめ決められていない。

**[SWS_Com_00370]** AUTOSAR COM モジュールの信号ゲートウェイは、信号の経路付けの機能が不要な場合、大きさがゼロになるまで縮小されなければならない。（SRS_PduR_06064）

### 7.3 通常動作

#### 7.3.1 起動時の動作

この章では、`Com_Init` の間に実行される動作を説明する。

**[SWS_Com_00217]** AUTOSAR COM モジュールは、`Com_Init`（[SWS_Com_00432]）の実行中に、各 I-PDU を、まずバイト単位で `ComTxIPduUnusedAreasDefault` の値により、次にビット単位で、含まれる信号の初期値（`ComSignalInitValue`）と更新ビット（[SWS_Com_00117] を参照）に従って、初期化しなければならない。（SRS_BSW_00101）

##### 7.3.1.1 前提条件

C の初期化コード（スタートアップコードとも呼ばれる）は、グローバル変数と静的変数を、初期値で初期化する。これは、AUTOSAR COM モジュールのサービスが呼ばれる前に、実行されていなければならない。

##### 7.3.1.2 初期化

**[SWS_Com_00128]** AUTOSAR COM モジュールの初期化関数 `Com_Init`（[SWS_Com_00432]）は、スタートアップコードでまだ初期化されていない、すべての内部データ（例: C の構造体）を初期化しなければならない。（SRS_BSW_00101）

**[SWS_Com_00328]** AUTOSAR COM モジュールの初期化関数 `Com_Init`（[SWS_Com_00432]）は、ECU 間の通信を有効にしてはならない。（SRS_BSW_00101）

この初期化の章は、完全ではない。AUTOSAR COM モジュールの機能のうち、いくつかの初期化の詳細は、それぞれの機能の章で説明される。

##### 7.3.1.3 I-PDU の未使用領域の初期化

**[SWS_Com_00015]** AUTOSAR COM モジュールは、I-PDU の中の未使用の領域を、コンフィギュレーションパラメータ `ComTxIPduUnusedAreasDefault`（ECUC_Com_00017）で決まる値（例: `0xFF`）で埋めなければならない。（SRS_BSW_00101、SRS_Com_02042）

##### 7.3.1.4 信号と更新ビットの初期化

**[SWS_Com_00098]** AUTOSAR COM モジュールは、n ビットの大きさの信号型の各信号を、送信側でも受信側でも、そのコンフィギュレーションパラメータ `ComSignalInitValue`（ECUC_Com_00170）の下位 n ビットで初期化しなければならない。（SRS_BSW_00101）

コンフィギュレーションされた `ComSignalInitValue`（ECUC_Com_00170）は、関連する I-PDU の中の信号の初期化にも使われる。[SWS_Com_00217] を参照。

信号の `ComSignalInitValue`（ECUC_Com_00170）は、その `ComSignalDataInvalidValue`（ECUC_Com_00391）と同一にできる。これらは、信号ごとに異なってもよい。

**[SWS_Com_00117]** AUTOSAR COM モジュールは、初期化の間に、すべての更新ビットをクリアしなければならない。[SWS_Com_00059] も参照すること。（SRS_BSW_00101、SRS_Com_02030、SRS_Com_02058）

##### 7.3.1.5 I-PDU グループの初期化

**[SWS_Com_00444]** デフォルトでは、すべての I-PDU グループは停止（stopped）の状態であり、`Com_Init`（[SWS_Com_00432]）の呼び出しで自動的に開始されてはならない。（SRS_Com_00218）


#### 7.3.2 終了処理（De-Initialization）

AUTOSAR COM モジュールは、COM 層の終了処理のために、API 関数 `Com_DeInit`（[SWS_Com_00130]）を提供する。これは、層の終了処理の後は、AUTOSAR COM モジュールを介した通信ができなくなり、開始されたすべての I-PDU グループが停止されることを意味する。[SWS_Com_00129] も参照すること。

#### 7.3.3 通信モード

この章では、AUTOSAR COM モジュールの中の信号の流れを定義する。さらに、AUTOSAR COM モジュールが提供する、さまざまな送信モードを定義する。7.3.3.2 節は、1 つの I-PDU について、2 つの送信モードを切り替える仕組みを定義する。信号の複製は、7.3.3.4 節で定義する。

AUTOSAR COM モジュールが扱える、通信の使用例の例は、第 12 章に示されている。

##### 7.3.3.1 転送プロパティと I-PDU の送信モード

###### 7.3.3.1.1 信号

AUTOSAR COM モジュールは、信号に対する複数の転送プロパティと、I-PDU に対する複数の送信モードをサポートする。この章の定義は、[17] の 2.3.3 章の定義に基づき、それを補完するものである。

**[SWS_Com_00330]** `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、`ComTransferProperty` が `TRIGGERED` の信号の送信要求があるたびに、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の `ComTxModeNumberOfRepetitions` 回に 1 を加えた回数の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00767]** `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、`ComTransferProperty` が `TRIGGERED_WITHOUT_REPETITION` の信号の送信要求があるたびに、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の 1 回の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00734]** `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、`ComTransferProperty` が `TRIGGERED_ON_CHANGE` の信号の送信要求があったとき、新しく送られた信号が、ローカルに保存された値（最後に送った値、または初期値）と、長さまたは値が異なる場合、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の `ComTxModeNumberOfRepetitions` 回に 1 を加えた回数の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00768]** `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、`ComTransferProperty` が `TRIGGERED_ON_CHANGE_WITHOUT_REPETITION` の信号の送信要求があったとき、新しく送られた信号が、ローカルに保存された値（最後に送った値、または初期値）と、長さまたは値が異なる場合、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の 1 回の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00762]** AUTOSAR COM モジュールは、`ComBitSize` が 0 にコンフィギュレーションされた信号とグループ信号に対して、転送プロパティ `TRIGGERED_ON_CHANGE` と `TRIGGERED_ON_CHANGE_WITHOUT_REPETITION` をサポートしてはならない。（SRS_Com_02037）

転送プロパティ `TRIGGERED_ON_CHANGE` と `TRIGGERED_ON_CHANGE_WITHOUT_REPETITION` のサポートは、特定の信号型に限定されない。したがって、取りうるすべての信号型でサポートされる。これには、バイト配列の型（例: `UINT8_N`）も含まれる。

送信の複製の仕組みの詳細は、7.3.3.4 節で規定する。

**[SWS_Com_00135]** AUTOSAR COM モジュールは、`ComTxModeMode`（ECUC_Com_00137）が `NONE` の I-PDU の送信を開始してはならない。（SRS_Com_02083）

`ComTxModeMode` が `NONE` の I-PDU は、`Com_TriggerTransmit` を介して要求できる。

I-PDU に関連付けられた保留中の信号は、何らかの理由で I-PDU の送信が起動されたときに送信される。例えば、同じ I-PDU の中の `ComTransferProperty` が `TRIGGERED` の信号が送られたとき、または、`ComTxModeMode` が `PERIODIC` または `MIXED` のために、I-PDU の送信が予定されたとき。

I-PDU のバスのタイミングは、上述の送信モードと転送プロパティに組み合わせた、RTE の送信要求で制御できる。さらに、PDU Router は、特に FlexRay と LIN の場合に、サービス `Com_TriggerTransmit` で制御できる。後者の場合、PDU Router は、AUTOSAR COM モジュールに I-PDU の送信を要求する。

関数 `Com_TriggerTransmit` は、送信モードに関係なく、どの I-PDU に対しても呼べる。これにより、LIN と FlexRay は、特に散発的な通信のために、利用できるすべての送信モードを使える。NM も、ユーザデータを送るために、この仕組みを使う。

###### 7.3.3.1.2 信号グループ

AUTOSAR COM では、信号グループとグループ信号にも、転送プロパティを持たせることができる。これは、送信モードと組み合わせて、信号グループまたはグループ信号が更新された場合に、I-PDU を送り出すかを決める。

**[SWS_Com_00741]** `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、`ComTransferProperty` が `TRIGGERED` の信号グループの送信要求があるたびに、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の `ComTxModeNumberOfRepetitions` 回に 1 を加えた回数の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00769]** `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、`ComTransferProperty` が `TRIGGERED_WITHOUT_REPETITION` の信号グループの送信要求があるたびに、AUTOSAR COM モジュールは、遅くとも次のメイン関数の中で、割り当てられた I-PDU の 1 回の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00742]** 独自の `ComTransferProperty` がコンフィギュレーションされた信号を含まない、`ComTransferProperty` が `TRIGGERED_ON_CHANGE` の信号グループについて: `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、そのような信号グループの送信要求があるたびに、新しく送られたグループ信号の少なくとも 1 つが、ローカルに保存された値（最後に送った値、または初期値）と、長さまたは値が異なる場合、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の `ComTxModeNumberOfRepetitions` 回に 1 を加えた回数の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00743]** 独自の `ComTransferProperty` がコンフィギュレーションされた信号を含む、`ComTransferProperty` が `TRIGGERED_ON_CHANGE` の信号グループについて: `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、そのような信号グループの送信要求があるたびに、`ComTransferProperty` が `TRIGGERED_ON_CHANGE` でコンフィギュレーションされた、新しく送られたグループ信号の少なくとも 1 つが、ローカルに保存された値（最後に送った値、または初期値）と、長さまたは値が異なる場合、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の `ComTxModeNumberOfRepetitions` 回に 1 を加えた回数の送信を開始しなければならない。（SRS_Com_02083）

**[SWS_Com_00770]** `ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられた、`ComTransferProperty` が `TRIGGERED_ON_CHANGE_WITHOUT_REPETITION` の信号グループの送信要求があったとき、新しく送られたグループ信号の少なくとも 1 つが、ローカルに保存された値（最後に送った値、または初期値）と、長さまたは値が異なる場合、AUTOSAR COM モジュールは、直ちに（遅くとも次のメイン関数の中で）、割り当てられた I-PDU の 1 回の送信を開始しなければならない。（SRS_Com_02083）

##### 7.3.3.2 特定の I-PDU の送信モードの選択

I-PDU は、信号を運ぶ。I-PDU には複数の信号が含まれうるため、以降では、特定の I-PDU に含まれる信号の状態から、I-PDU の送信モードを導く方法を定義する。AUTOSAR COM モジュールでは、各 I-PDU に、2 つの異なる送信モードを静的にコンフィギュレーションできる（[SWS_Com_00032] を参照）。ある時点で有効な I-PDU の送信モードは、この I-PDU に割り当てられた信号の値だけを使って選択される。2 つの送信モードの一方の選択に寄与する、1 つの I-PDU の信号と、送信モードの選択に使われる条件は、静的にコンフィギュレーションされる（[SWS_Com_00676] を参照）。

送信モードの選択では、AUTOSAR COM モジュールは、グループ信号を通常の信号と同じように扱う。

**[SWS_Com_00676]** TMS の評価のために、AUTOSAR COM モジュールは、コンフィギュレーションに `ComFilter` コンテナが含まれる、すべての（グループ）信号を考慮しなければならない。`ComSignal` と `ComGroupSignal` に含まれるコンテナを参照すること。（SRS_Com_02041）

`ComIPduDirection` が `SEND` にコンフィギュレーションされた I-PDU の信号が、コンフィギュレーションコンテナ `ComFilter` を含む場合、その信号は、この I-PDU の TMS に寄与すると呼ばれる。その信号のコンフィギュレーションコンテナに `ComFilter` が割り当てられていない場合、その信号は TMS に寄与しない。

**[SWS_Com_00677]** I-PDU の中のどの信号も、TMS の計算に寄与しない場合、AUTOSAR COM モジュールは、この I-PDU の TMS を真に評価しなければならない。（SRS_Com_02084、SRS_Com_02041）

**[SWS_Com_00678]** AUTOSAR COM モジュールが、寄与する信号の TMC を真に評価した場合、AUTOSAR COM モジュールは、TMS を真に評価しなければならない。（SRS_Com_02084、SRS_Com_02041）

**[SWS_Com_00679]** AUTOSAR COM モジュールが、寄与する信号のどの TMC も真に評価しなかった場合、AUTOSAR COM モジュールは、TMS を偽に評価しなければならない。（SRS_Com_02084、SRS_Com_02041）

**[SWS_Com_00605]** AUTOSAR COM モジュールは、各 I-PDU に、送信モード選択器（Transmission Mode Selector）を定義しなければならない。TMS の定義は上記を参照。（SRS_Com_02084）

**[SWS_Com_00245]** AUTOSAR COM モジュールは、含まれる信号が `Com_SendSignal` または `Com_SendSignalGroup` の呼び出しで更新された後に、I-PDU の TMS を再計算しなければならない。（SRS_Com_02084）

**[SWS_Com_00763]** 条件 `MASKED_NEW_DIFFERS_MASKED_OLD` がコンフィギュレーションされた送信モードの計算では、AUTOSAR COM モジュールは、下位 `ComBitSize` ビットだけを使わなければならない。（SRS_Com_02084）

**[SWS_COM_00813]** 条件 `MASKED_NEW_DIFFERS_X`、`MASKED_NEW_EQUALS_X`、`NEW_IS_OUTSIDE`、または `NEW_IS_WITHIN` がコンフィギュレーションされた送信モードの計算では、AUTOSAR COM モジュールは、コンフィギュレーションされた `ComSignalType` のすべてのビットを使わなければならない。（SRS_Com_02084）

`ComFilterAlgorithm` が `ALWAYS` にコンフィギュレーションされた信号は、対応する I-PDU の TMS を、常に真に設定することに注意。したがって、TMS に寄与する信号を定義するときは、注意が必要である。

**[SWS_Com_00032]** I-PDU の TMS が真に評価される場合、AUTOSAR COM モジュールは、その I-PDU に、コンフィギュレーションコンテナ `ComTxModeTrue`（ECUC_Com_00455）で定義された `ComTxModeMode` を使わなければならない。（SRS_Com_02082、SRS_Com_02084）

**[SWS_Com_00799]** I-PDU の TMS が偽に評価される場合、AUTOSAR COM モジュールは、その I-PDU に、コンフィギュレーションコンテナ `ComTxModeFalse`（ECUC_Com_00454）で定義された `ComTxModeMode` を使わなければならない。（SRS_Com_02082、SRS_Com_02084）

**[SWS_Com_00238]** 2 つの TMS の状態のそれぞれで、信号の転送プロパティと I-PDU の送信モードの組み合わせの規則は、[17] の 2.3 節で定義されたとおりに適用されなければならない。（SRS_Com_02082）

**[SWS_Com_00239]** I-PDU の TMS の状態が変わったとき、AUTOSAR COM モジュールは、新たに有効になった送信モードを、直ちに（遅くとも次のメイン関数の中で）使わなければならない。すなわち、AUTOSAR COM モジュールは、まずモードの変更を行い、その後で、モードの変更によって生じる `PduR_ComTransmit` の呼び出しを実行しなければならない。（SRS_Com_02082）

**[SWS_Com_00244]** TMS の変更が I-PDU の送信モードの変更を引き起こす場合、AUTOSAR COM モジュールは、送信モード `PERIODIC` と `MIXED` の周期のタイマを、再開しなければならない。（SRS_Com_02082）

**[SWS_Com_00495]** `Com_SendSignal` または `Com_SendSignalGroup` の呼び出しの結果、開始済みの I-PDU の送信モードが `PERIODIC` または `MIXED` へ変わる場合、AUTOSAR COM モジュールは、遅くとも次のメイン関数の中で、`PduR_ComTransmit` の呼び出しで、新しい送信の周期を開始しなければならない。送信は、送信モードの切り替えを引き起こした信号または信号グループの転送プロパティに関係なく、開始されなければならない。最小遅延時間は、引き続き守らなければならない。図 5「AUTOSAR COM モジュールの受信のインタラクションモデル」も参照すること。（SRS_Com_02082）

`ComTxModeTimeOffset` は、考慮されない。これは、明示的な I-PDU の送信モードの切り替え（例: `Com_IpduGroupStart` や `Com_SwitchIpduTxMode`）でだけ考慮される。

**[SWS_Com_00582]** TMS の変更が、送信モード `DIRECT` への変更を引き起こす場合、下位層への、即時の（MDT によりずらされる場合を除き、遅くとも次のメイン関数の中での）直接の／n 回の送信が開始されなければならない。（SRS_Com_02082）

上記の要求は、`DIRECT` への TMS の切り替えが、`ComTransferProperty` が `PENDING` の信号により引き起こされた場合の動作を明確にする。

I-PDU の `ComTxModeMode` が `NONE` にコンフィギュレーションされている場合、AUTOSAR COM による送信は開始されない。これは、例えば TMS が偽に評価される場合に、I-PDU の送信を防ぐために使える。

**[SWS_Com_00478]** AUTOSAR COM モジュールは、`Com_MainFunctionTx` の 1 回の呼び出しの中で、I-PDU を最大で 1 回だけ送り出さなければならない。（SRS_Com_02083）

##### 7.3.3.3 信号の流れと送信モードの選択

特定の信号についての RTE からの送信要求の後、信号は、コンフィギュレーションで定義されたとおり、適切な I-PDU のバッファへ書き込まれ、I-PDU の送信モードの選択が、7.3.3.2 節に従って行われる。

図 3 は、信号の流れを示す。

> （図 3「AUTOSAR COM モジュールの論理的な信号の流れ。1 つの I-PDU（IPDUk）に割り当てられた 2 つの信号（Signal1 と Signal2）について示す」: 図。訳なし。原典を参照。
> 　読み取れる内容: 各信号について、送信モード条件 C(Signal, I-PDUk)（条件は [17] のフィルタと同じ）の評価と、I-PDU の中の信号の更新が行われ、その結果から、I-PDUk の送信モード選択器（TMS）が評価される。RX 側からの信号ベースのゲートウェイの信号は、エンディアン変換と符号拡張の後に入る。）

##### 7.3.3.4 信号の送信要求の複製

RTE の送信要求について、`ComTxModeMode` が `DIRECT` または `MIXED` での、送信要求の繰り返しの回数は、コンフィギュレーションパラメータ `ComTxModeNumberOfRepetitions`（ECUC_Com_00281）で定義される。

**[SWS_Com_00467]** `ComRetryFailedTransmitRequests` が `TRUE` に設定されておらず、`ComTxModeMode` が `DIRECT` または `MIXED` で、`ComTxModeNumberOfRepetitions` が 0 に設定された I-PDU が、送信のために起動された場合、AUTOSAR COM モジュールは、確認の結果に関係なく、この I-PDU に対して `PduR_ComTransmit` を 1 回だけ呼び出さなければならない。（SRS_Com_02083）

`ComTxModeNumberOfRepetitions` を 0 にコンフィギュレーションすると、[17] の元の直接の送信モードを模倣する。

**[SWS_Com_00279]** 一連の n 回の送信の途中で（例: 5 回の繰り返しのうち 3 回目の後。[SWS_Com_00305] を参照）、RTE から新しい送信要求を受け取った場合、AUTOSAR COM モジュールは、残りの送信の繰り返しをキャンセルし、遅くとも次のメイン関数の中で、新しい要求の処理を開始しなければならない。付録 A の使用例 4 を参照。（SRS_Com_02080）

**[SWS_Com_00305]** `ComTxModeMode` が `DIRECT` または `MIXED` で、`ComTxModeNumberOfRepetitions` が 0 より大きい I-PDU を送る場合、AUTOSAR COM モジュールは、この送信要求に対する、`ComTxModeNumberOfRepetitions` に 1 を加えた回数の送信成功の確認を受け取るまで、周期 `ComTxModeRepetitionPeriod` で、`PduR_ComTransmit` を周期的に呼ばなければならない。（SRS_Com_02083）

[SWS_Com_00305] は、CAN が、これらの I-PDU のためのキューを持たないことを前提としている。CAN ドライバ、インタフェース、ハードウェアの間のやり取りには、まれな状況で、余分な送信を引き起こしうる競合状態がある。

N 回の送信が進行している間に、下位層が `E_NOT_OK` を返した場合、`ComRetryFailedTransmitRequests` が真に設定されていなければ、このエラー通知は無視される（[SWS_Com_00773] も参照）。[SWS_Com_00305] が規定するように、N 回の送信では、確認された送信だけが数えられるため、誤った送信要求は、安全に無視できる。

N 回の送信が、送信モード `MIXED` で、`MIXED` の送信モードの周期的な送信の後に、確認が保留中のまま要求された場合、周期的な送信の確認は、N 回の送信に割り当てられる。この場合、N 回の要求の新しい値の送信は、n−1 回だけが観測される。送信デッドライン監視のタイマは、N 回の要求が完了した後に、早くてもそのときに、リセットされる。これは、送信デッドライン監視のタイマを、送信モード `MIXED` と N 回の送信と組み合わせてコンフィギュレーションするときに、守らなければならない。

**[SWS_Com_00494]** 送信モード `MIXED` の中で、N 回の送信要求が、`MIXED` の送信の周期的な部分と重なる場合、周期的な送信は、N 回の送信要求の対応する送信として数えられなければならない。（SRS_Com_02083）

**[SWS_Com_00392]** N 回の送信が完了する前に、送信デッドライン監視のタイムアウトが起きた場合、AUTOSAR COM モジュールは、この N 回の送信のために、それ以上の送信を開始してはならない。（SRS_Com_02083）

最小遅延時間は、[17] の 2.3.4 章で定義されたとおり、常に考慮される。

起動時のバーストを避けるために、I-PDU ごとに時間のオフセットをコンフィギュレーションできる。詳細は ECUC_Com_00180 を参照。

2 回の繰り返しの間の時間は、コンフィギュレーションパラメータ `ComTxModeRepetitionPeriod`（ECUC_Com_00282）で設定される。

送信モードの変更が、トリガされた信号を送ることで、送信モード `MIXED` の開始につながり、`ComTxModeNumberOfRepetitions` が 1 以上にコンフィギュレーションされている場合、送信モード `MIXED` の開始時に、PDU Router への送信要求が、少なくとも `ComTxModeNumberOfRepetitions` に 1 を加えた回数、行われる。[SWS_Com_00305] も参照すること。

##### 7.3.3.5 失敗した送信要求の再試行

**[SWS_Com_00773]** `ComRetryFailedTransmitRequests` が `TRUE` に設定されている場合、`PduR_ComTransmit` の戻り値を評価しなければならない。`PduR_ComTransmit` の戻り値が `E_OK` に等しくない場合、AUTOSAR COM モジュールは、送られなかった I-PDU について、次の `Com_MainFunctionTx` の中で、再び `PduR_ComTransmit` を呼ばなければならない。（SRS_Com_02037）

その間に I-PDU が更新された場合は、新しいデータが送られる。

[SWS_Com_00773] は、同じ失敗した送信要求に対して、複数回の再試行につながりうる。

[SWS_Com_00773] は、送信モードの変更の影響を受けない。

[SWS_Com_00773] は、起こりうる繰り返しの周期に影響しない。繰り返しは、再試行が行われなかったかのように起こる。

**[SWS_Com_00774]** AUTOSAR COM モジュールは、送信デッドライン監視を、起こりうる再試行とは独立に開始しなければならない。その後の再試行は、送信デッドライン監視に影響してはならない。（SRS_Com_02037）

**[SWS_Com_00775]** `ComRetryFailedTransmitRequests` が `TRUE` に設定され、I-PDU に送信デッドライン監視のタイムアウトが起きた場合、AUTOSAR COM モジュールは、この I-PDU の保留中の送信要求を、すべて失効させなければならない。（SRS_Com_02037）

**[SWS_Com_00776]** 周期的な送信のサイクルタイマは、常に最初の送信の試行で開始しなければならない。（SRS_Com_02037）

#### 7.3.4 信号の無効化

AUTOSAR COM モジュールは、送信側が、対応する信号の有効な値を提供できないこと（例: センサが故障している場合）を示せるようにする。AUTOSAR COM モジュールでは、コンフィギュレーションの間に `ComSignalDataInvalidValue`（ECUC_Com_00391）を定義できる。

##### 7.3.4.1 無効化された信号の送信

**[SWS_Com_00099]** `Com_InvalidateSignal` の呼び出しで、AUTOSAR COM モジュールは、内部で、コンフィギュレーションされた `ComSignalDataInvalidValue`（ECUC_Com_00391）を使って `Com_SendSignal` を実行しなければならない。（SRS_Com_02077）

`ComTransferProperty` と送信モードが、`ComSignalDataInvalidValue` のバス上での送信を決める。データ無効値を使って内部で実行される `Com_SendSignal` により、データ無効値が、フィルタと TMS の現在の値として使われる。

データ無効値は、グループ信号ごとにコンフィギュレーションされる。ECUC_Com_00520 を参照。

VFB は、複合データ型に対して 1 つの属性だけを定義する。したがって、無効化された複合データ型の、無効化された信号グループへの最適な割り当ては、信号グループのすべてのグループ信号を無効化することである。そのため、RTE は、`Com_InvalidateSignalGroup`（[SWS_Com_00557]）の呼び出しで、信号グループ全体を無効化するよう要求することもできる。

##### 7.3.4.2 無効化された信号の受信

**[SWS_Com_00680]** コンフィギュレーションされた `ComSignalDataInvalidValue`（ECUC_Com_00391）が信号について受信され、この信号の `ComDataInvalidAction`（ECUC_Com_00314）が `NOTIFY` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、コンフィギュレーションされた `ComInvalidNotification` 関数（ECUC_Com_00315）を介して、RTE へ通知しなければならない。この場合、フィルタリングや通常の信号通知などの、他の信号の処理は行われてはならない。（SRS_Com_02079）

受信デッドライン監視のタイマは、無効な信号または信号グループを受信した場合も、再開される。[SWS_Com_00738] を参照。

**[SWS_Com_00681]** コンフィギュレーションされた `ComSignalDataInvalidValue`（ECUC_Com_00391）が信号について受信され、この信号の `ComDataInvalidAction`（ECUC_Com_00314）が `REPLACE` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、信号の値を、コンフィギュレーションされた `ComSignalInitValue`（ECUC_Com_00170）で置き換えなければならない。置き換えの後、フィルタリングや通知などの通常の信号の処理は、`ComSignalDataInvalidValue` の代わりに `ComSignalInitValue` が受信されたかのように行われなければならない。（SRS_Com_02079、SRS_Com_02087）

**[SWS_Com_00682]** コンフィギュレーションされた `ComSignalDataInvalidValue`（ECUC_Com_00391）が、信号グループの少なくとも 1 つのグループ信号について受信され、この信号グループの `ComDataInvalidAction`（ECUC_Com_00314）が `NOTIFY` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、コンフィギュレーションされた `ComInvalidNotification` 関数（ECUC_Com_00315）を介して、RTE へ通知しなければならない。この場合、通常の通知などの、他の信号グループ／グループ信号の処理は行われてはならない。（SRS_Com_02079）

**[SWS_Com_00683]** コンフィギュレーションされた `ComSignalDataInvalidValue`（ECUC_Com_00391）が、信号グループの少なくとも 1 つのグループ信号について受信され、この信号グループの `ComDataInvalidAction`（ECUC_Com_00314）が `REPLACE` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、この信号グループのすべてのグループ信号を、コンフィギュレーションされた `ComSignalInitValue` の値で置き換えなければならない。置き換えの後、受信通知などの通常の信号グループ／グループ信号の処理は、すべてのグループ信号について `ComSignalInitValue` が受信されたかのように行われなければならない。（SRS_Com_02087）

**[SWS_Com_00717]** コンフィギュレーションされた `ComSignalDataInvalidValue` が信号について受信され、その `ComDataInvalidAction` が `NOTIFY` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、受信した `ComSignalDataInvalidValue` を、信号オブジェクトへ保存してはならない。（SRS_Com_02079）

次の `Com_ReceiveSignal` の呼び出しは、最後に受信した有効な信号を返し、まだ信号が受信されていない場合は、`ComSignalInitValue` を返す。

**[SWS_Com_00718]** コンフィギュレーションされた `ComSignalDataInvalidValue` が、信号グループの少なくとも 1 つのグループ信号について受信され、その `ComDataInvalidAction` が `NOTIFY` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、受信したグループ信号のどれも、信号オブジェクトへ保存してはならない。（SRS_Com_02079）

次の `Com_ReceiveSignalGroup` の呼び出しは、最後に受信した有効なグループ信号を、シャドウバッファへコピーし、信号グループがまだ受信されていない場合は、`ComSignalInitValue` をコピーする。

> **学習メモ（原文でなく訳者の解釈）**: このプロジェクトの実装と照らすときの手がかり。
> - 送信モードの選択は、TMS の評価（[SWS_Com_00676]〜[00679]）→ 送信モードの決定（[SWS_Com_00032]/[00799]）→ 切り替え時のタイマの再開（[SWS_Com_00244]）の順。TMS に寄与するのは、`ComFilter` を持つ信号だけ（[SWS_Com_00676]）。
> - N 回の送信は、PduR からの「確認」の回数で数える（[SWS_Com_00305]）。失敗は `ComRetryFailedTransmitRequests` が真のときだけ再試行する（[SWS_Com_00773]）。
> - 無効値の受信は、`NOTIFY` なら値を保存しない（[SWS_Com_00717]）、`REPLACE` なら初期値に置き換えて通常の処理を続ける（[SWS_Com_00681]）。

#### 7.3.5 I-PDU の扱い

##### 7.3.5.1 I-PDU グループの定義

I-PDU グループには、次の規則が適用される。

1. I-PDU は、どの I-PDU グループにも属せる。
2. I-PDU は、開始された I-PDU グループに属する場合（[SWS_Com_00771] を参照）、または、どの I-PDU グループにも属さず、暗黙のうちに開始された場合（[SWS_Com_00840] を参照）に、アクティブ（開始済み）である。
3. I-PDU グループの最大数は、プリコンパイルでコンフィギュレーションできる。
4. I-PDU グループには、同じ方向（すなわち、送信または受信）の I-PDU だけを含めてよい。[SWS_Com_00871] を参照。

規則 1 と 3 は、COM のコンフィギュレーションでサポートされる。サポートされる I-PDU グループの最大数は、`ComSupportedIpduGroups`（ECUC_Com_00710）でコンフィギュレーションできる。

**[SWS_Com_00771]** I-PDU は、それが属する I-PDU グループの少なくとも 1 つがアクティブ（開始済み）であれば、アクティブ（開始済み）である。（SRS_Com_00218）

AUTOSAR COM モジュールにとって、I-PDU グループの入れ子は、純粋に概念上のものであり、適切なコンフィギュレーション、または AUTOSAR COM モジュールの使い方で、解決しなければならない。

例えば、I-PDU「BUS1 RX Function1」が I-PDU グループ「BUS1 RX」に属し、I-PDU グループ「BUS1 RX」が I-PDU グループ「BUS1」に含まれる場合、I-PDU「BUS1 RX Function1」も、I-PDU グループ「BUS1」に含まれなければならない。このような依存関係は、コンフィギュレーションの時点で解決する必要がある。

繰り返しになるが、COM モジュールは、I-PDU のグループ化を知らず、扱わない。I-PDU のグループ化は、コンフィギュレーションで、または呼び出し元のモジュールで、扱わなければならない。

さらに、I-PDU グループの状態の扱いは、すべて AUTOSAR COM モジュールの外（例: Basic Software Mode Manager の中）で行われることが期待される。状態が変わった場合、I-PDU グループの状態を管理するモジュールが、一貫して、`Com_IpduGroupStart` と `Com_IpduGroupStop` で I-PDU グループを開始または停止する。

どの I-PDU グループにも割り当てられていない I-PDU の状態は変更できないため、暗黙のうちに開始される。

**[SWS_Com_00840]** I-PDU がどの I-PDU グループにも割り当てられていない場合、AUTOSAR COM は、この I-PDU を `Com_Init` の中で、パラメータ `Initialize` を真にした `Com_IpduGroupStart` で開始されたかのように、開始しなければならない（7.3.5.2 節を参照）。（SRS_Com_00218）

AUTOSAR COM は、そのような I-PDU を暗黙のうちに停止することが無く、I-PDU がどの I-PDU グループにも割り当てられていないため、そのような I-PDU は、停止できない。

##### 7.3.5.2 I-PDU グループの開始

デフォルトでは、すべての I-PDU グループは停止している（[SWS_Com_00444] を参照）。`Com_IpduGroupStart` の呼び出しは、I-PDU グループが以前に停止していた場合に、それを開始する。

**[SWS_Com_00114]** I-PDU が `Com_IpduGroupStart` で開始された場合、AUTOSAR COM モジュールは、その信号と信号グループの送信／受信を許可しなければならない。表 4 も参照。（SRS_Com_00218）

**[SWS_Com_00787]** I-PDU が `Com_IpduGroupStart` で開始された場合、AUTOSAR COM モジュールは、この I-PDU の次の属性を、常に初期化しなければならない。

1. 送信モード `DIRECT` または `MIXED` の I-PDU の `ComMinimumDelayTime`
2. 0 でない `ComFirstTimeout` がコンフィギュレーションされた、すべての信号の、すべての受信デッドライン監視タイマを再開する。
3. すべての送信デッドライン監視タイマをキャンセルし、I-PDU の有効化の後に最初に送信タイマが開始されるときの値として、（コンフィギュレーションされていれば）`ComFirstTimeout` を使う。
4. 含まれるすべての更新ビットをクリアする。
5. `ComFilterAlgorithm` が `ONE_EVERY_N` のフィルタの `OCCURRENCE` をリセットする。
6. `ComIPduDirection` が `SEND` にコンフィギュレーションされた I-PDU の、I-PDU カウンタを 0 に設定する。
7. `ComIPduDirection` が `RECEIVED` にコンフィギュレーションされた I-PDU では、次に到着する任意の I-PDU カウンタを受け付ける。

（SRS_Com_00218）

**[SWS_Com_00222]** I-PDU が、パラメータ `Initialize` を真にした `Com_IpduGroupStart` で開始された場合、AUTOSAR COM モジュールは、[SWS_Com_00787] に加えて、この I-PDU の次の属性を初期化しなければならない。

1. I-PDU のデータ（[SWS_Com_00217] で定義されたとおり）
2. 含まれる信号グループのシャドウバッファ
3. 各信号について、フィルタの仕組みの old_value を `ComSignalInitValue` に
4. 送信モード `PERIODIC` または `MIXED` の I-PDU の、`ComTxModeTimePeriod` と `ComTxModeTimeOffset`

（SRS_Com_00218）

**[SWS_Com_00223]** I-PDU が `Com_IpduGroupStart` で開始された場合、AUTOSAR COM モジュールは、その送信モードを、現在のデータの内容に従って決めなければならない。（SRS_Com_00218）

**[SWS_Com_00228]** ある場合には、I-PDU が、含まれるすべての信号が書き込まれる前に、`Com_IpduGroupStart` で開始される。この場合、AUTOSAR COM モジュールは、欠けている信号のデータに、`ComSignalInitValue` を使わなければならない。（SRS_Com_00218）

**[SWS_Com_00229]** I-PDU が開始されたとき、その I-PDU の 1 つ以上の信号が、すでに上位層により送信 API のいずれかで書き込まれている場合、AUTOSAR COM モジュールは、I-PDU の TMS を決めるために、最も新しく得られた値を使わなければならない。（SRS_Com_00218）

**[SWS_Com_00733]** I-PDU が `Com_IpduGroupStart` で開始され、この I-PDU が、デッドライン監視がコンフィギュレーションされた信号（ECUC_Com_00183、ECUC_Com_00263）を含む場合、AUTOSAR COM モジュールは、`initialize` パラメータの値に関係なく、これらの信号のデッドライン監視を開始しなければならない。（SRS_Com_00218）

注: I-PDU が `Com_IpduGroupStart` で開始され、パラメータ `initialize` が偽に設定されている場合、内部バッファは変更されず、I-PDU は、最後に設定された値、または初期値を含む。特に、I-PDU が初めて開始され、`initialize` が偽に設定されている場合、COM は、`Com_Init` により設定された初期値で動作する。

**[SWS_Com_00877]** I-PDU がどの I-PDU グループにも含まれない場合、それは、COM の初期化の間に開始される。その開始時の送信モードは、TMS に寄与する信号の `ComSignalInitValue` に従って評価されなければならない。（SRS_Com_00218）

この場合、COM の初期化の前に信号を書き込めないため、評価は、すでにコンフィギュレーションの時点で行える。

##### 7.3.5.3 I-PDU グループの停止

`Com_IpduGroupStop` の呼び出しは、I-PDU グループが以前に開始されていた場合に、それを停止する。

I-PDU の送信を無効にするためには、この I-PDU を含むすべての I-PDU グループを停止する必要がある。したがって、リッスンオンリーモードを実装するには、送信 I-PDU を含むすべての I-PDU グループを停止する必要がある。I-PDU の受信も停止してよい。

**[SWS_Com_00334]** 関数 `Com_SendSignal`、`Com_SendSignalGroup`、または `Com_InvalidateSignal` の呼び出しにより、AUTOSAR COM モジュールは、停止している I-PDU についても、内部バッファの値を更新しなければならない。表 3 も参照。（SRS_Com_00218）

停止している I-PDU に書き込まれた信号が、I-PDU が停止していなければこの I-PDU の送信を引き起こすものだった場合、このトリガは保存されない。対応する I-PDU グループを再開した後、そのような古いトリガは、I-PDU の即時の送信につながらない。

**[SWS_Com_00777]** I-PDU が `Com_IpduGroupStop` で停止された場合、AUTOSAR COM モジュールは、この I-PDU の未処理の送信要求をすべてキャンセルしなければならない。これには、`ComRetryFailedTransmitRequests` に関する、起こりうる再試行のキャンセルも含まれる。（SRS_Com_00218）

**[SWS_Com_00115]** I-PDU が `Com_IpduGroupStop` で停止された場合、AUTOSAR COM モジュールは、保留中のすべての確認について、デッドライン監視をキャンセルしなければならない。（SRS_Com_00218）

**[SWS_Com_00800]** AUTOSAR COM モジュールは、停止している I-PDU の送信確認を、すべて無視しなければならない。（SRS_Com_00218）

AUTOSAR COM モジュールは、関数 `Com_TriggerTransmit` の呼び出しを禁止できない。ただし、I-PDU が停止している場合、関数 `Com_TriggerTransmit` は `E_NOT_OK` を返す。[SWS_Com_00001] を参照。

**[SWS_Com_00684]** I-PDU が `Com_IpduGroupStop` で停止された場合、AUTOSAR COM モジュールは、その受信処理を無効にしなければならない。（SRS_Com_00218）

**[SWS_Com_00713]** 大きな I-PDU が `Com_IpduGroupStop` で停止された場合、AUTOSAR COM モジュールは、受信の処理を止め、部分的に受信した I-PDU を無視しなければならない。（SRS_Com_00218）

**[SWS_Com_00685]** I-PDU が `Com_IpduGroupStop` で停止された場合、AUTOSAR COM モジュールは、そのデッドライン監視をキャンセルしなければならない。（SRS_Com_00218）

**[SWS_Com_00479]** I-PDU が `Com_IpduGroupStop` で停止された場合、AUTOSAR COM モジュールは、停止された I-PDU の、送信済みで確認されていない信号／信号グループについて、コンフィギュレーションされた `ComErrorNotification`（ECUC_Com_00499）を、直ちに呼び出さなければならない。（SRS_Com_00218）

**[SWS_Com_00714]** 大きな I-PDU の送信がすでに進行中のときに停止された場合、AUTOSAR COM モジュールは、送信の処理を直ちに止めなければならない。（SRS_Com_00218）

表 3 は、停止している I-PDU の動作の概要を示す。

> **注（再構成）**: 原典の表 3・表 4 は、テキストの抽出で、受信側（RX）と送信側（TX）の 2 つの列の内容が混ざっていた。内容から、列の対応を復元した。重要な箇所は原典の表 3・表 4（51〜52 ページ）で確認すること。

**表 3: 停止している I-PDU の動作**

| 受信側（RX） | 送信側（TX） |
|---|---|
| RX デッドライン監視を無効にする。 | 送信を無効にする。 |
| （該当なし） | TX デッドライン監視を無効にする。 |
| `Com_RxIndication` は、RTE へ何もせず、I-PDU も保存しない。 | `Com_TxConfirmation` を無視する。 |
| `Com_ReceiveSignal` と `Com_ReceiveSignalGroup` で、戻りコード `COM_SERVICE_NOT_AVAILABLE` を返し、データとして、最後に分かっている値（または初期値）を返す。 | `Com_SendSignal`、`Com_SendSignalGroup`、`Com_InvalidateSignal` の呼び出しでは、AUTOSAR COM モジュールの内部バッファの値は更新されるが、戻りコード `COM_SERVICE_NOT_AVAILABLE` が返される。 |
| （該当なし） | 未処理の送信要求（例: N 回の送信）はキャンセルされる。 |
| （該当なし） | `Com_TriggerTransmit` で、戻りコード `E_NOT_OK` を返す。 |
| （該当なし） | 周期的な送信（TX）: それ以上送らない。 |

表 4 は、開始している I-PDU の動作の概要を示す。

**表 4: 開始している I-PDU の動作**

| 受信側（RX） | 送信側（TX） |
|---|---|
| `Initialize == true` の場合、タイムアウトを再初期化する（`ComSignalFirstTimeout`、`ComSignalTimeout`）。 | `Com_InvalidateSignal`、`Com_SendSignal`、`Com_SendSignalGroup` に対して、通常の反応をする。 |
| `Com_RxIndication` / `Com_TpRxIndication` に対して、通常の反応をする。 | 次の送信まで、送信タイムアウトの通知は無い。 |
| `Com_ReceiveSignal` と `Com_ReceiveSignalGroup` に対して、通常の反応をする。 | `Com_TxConfirmation` / `Com_TpTxConfirmation` に対して、通常の反応をする。 |
| （該当なし） | `Com_TriggerTransmit` に対して、通常の反応をする。 |
| （該当なし） | 周期的な送信（TX）: 0 から開始する。 |

##### 7.3.5.4 信号の通知（I-PDU の展開）

割り込み駆動のシステムとポーリングのシステムの両方をサポートするため、信号の通知をいつ行うかを、コンフィギュレーションできる。`ComIPduSignalProcessing` でコンフィギュレーションできる、2 つの信号通知モード `IMMEDIATE` と `DEFERRED` がある。ECUC_Com_00119 を参照。

**[SWS_Com_00300]** I-PDU の `ComIPduSignalProcessing` が `IMMEDIATE` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、含まれる信号と信号グループについて、コンフィギュレーションされた `ComNotifications` を、それぞれ `Com_RxIndication` または `Com_TpRxIndication` 関数の中で呼び出さなければならない。（SRS_Com_02046）

**[SWS_Com_00301]** I-PDU の `ComIPduSignalProcessing` が `DEFERRED` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、まず、`Com_RxIndication` 関数または関連する TP の受信関数の中で、I-PDU のデータを、PduR から COM の中へコピーしなければならない。次に、AUTOSAR COM モジュールは、含まれる信号と信号グループについて、コンフィギュレーションされた `ComNotifications` を、次の `Com_MainFunctionRx` の呼び出しの間に、非同期に呼び出さなければならない。（SRS_Com_02046）

`DEFERRED` モードで、遅延された展開が行われる前に `Com_ReceiveSignal` が呼ばれた場合、更新されていない前の値が返される。

両方の通知のオプションのシーケンスチャートは、9.3 章にある。これらのモードのコンフィギュレーションは、ECUC_Com_00119 で定義される。

**[SWS_Com_00574]** I-PDU を展開するとき、AUTOSAR COM モジュールは、受信したデータ長（`PduInfoPtr->SduLength`）を検査し、完全に受信された信号だけを展開し、`ComNotification` で通知しなければならない。（SRS_Com_02046）

**[SWS_Com_00870]** パラメータ `ComPduIdRef` から導かれる長さより小さい I-PDU を受信した場合、サイズ 0 の信号は、その設定された位置が、部分的に受信した I-PDU の受信したサイズ以下である場合に限り、受信されなければならない。（SRS_Com_02046）

**[SWS_Com_00794]** 期待より小さい I-PDU を受信した結果、コンフィギュレーションされた更新ビットを持たない信号を受信した場合、AUTOSAR COM モジュールは、この信号を、その更新ビットが立っていたかのように扱い、そのような信号を、更新されたものとして解釈しなければならない。（SRS_Com_02046）

受信した I-PDU の長さが、コンフィギュレーションされた／期待される I-PDU の長さより小さい場合、信号が部分的に更新されることを防ぐ必要がある。一方で、完全に受信されたすべての信号は、受信され、上位層へ通知されるべきである。

**[SWS_Com_00575]** I-PDU を展開するとき、AUTOSAR COM モジュールは、受信したデータ長（`PduInfoPtr->SduLength`）を検査し、信号グループが部分的にだけ受信された場合、そのような信号グループと、含まれるすべてのグループ信号は、展開されてはならず、`ComNotification` で通知されてはならない。（SRS_Com_02046）

上記の要求は、一貫性のない信号グループの受信、したがって一貫性のない複合データ型の受信を防ぐ。

受信したデータ長が期待より大きい場合、AUTOSAR COM モジュールは、コンフィギュレーションされていない信号の追加の受信データを、コピーも処理もしない。

##### 7.3.5.5 最小遅延タイマ（MDT）

AUTOSAR COM モジュールは、[17] で定義された最小遅延タイマの仕組みを引き継ぐ。この仕組みは、この章で明確化され、詳細化される。

I-PDU が開始されたとき、MDT は再初期化される。したがって、I-PDU の停止と開始を素早く繰り返すことで、MDT に違反しうる。

送信デッドライン監視タイマの動作は、最小遅延時間の監視により生じる、送信の遅延の影響を受けない。

**[SWS_Com_00471]** `ComMinimumDelayTime` が省略されているか、0 にコンフィギュレーションされている場合、最小遅延時間の監視は、行われてはならない。（SRS_Com_02037）

**[SWS_Com_00789]** `ComEnableMDTForCyclicTransmission`（ECUC_Com_00788）が偽にコンフィギュレーションされている場合、次の場合は、最小遅延タイマを（再）開始してはならない。

- `ComTxModeMode` が `PERIODIC` の I-PDU
- `ComTxModeMode` が `MIXED` の I-PDU の周期的な部分
- `ComTxModeNumberOfRepetitions > 0` の場合の、I-PDU の繰り返しの送信

（SRS_Com_02037）

最小遅延タイマが現在開始されていない場合、最小遅延タイマが満了することはありえない。したがって、最適化された実装では、例えば、`ComEnableMDTForCyclicTransmission` が偽にコンフィギュレーションされている場合、最小遅延タイマが開始されることがないため、周期的な I-PDU の最小遅延時間の監視を、完全に省略してもよい。

**[SWS_Com_00698]** I-PDU の `ComMinimumDelayTime` が 0 より大きくコンフィギュレーションされ、`ComRetryFailedTransmitRequests` が `FALSE` に設定されている場合、AUTOSAR COM モジュールは、`PduR_ComTransmit` を介したその I-PDU の PDU Router への送信時に、最小遅延時間のカウンタをロードし、開始しなければならない。（SRS_Com_02037）

**[SWS_Com_00828]** I-PDU の `ComMinimumDelayTime` が 0 より大きくコンフィギュレーションされ、`ComRetryFailedTransmitRequests` が `TRUE` に設定されている場合、AUTOSAR COM モジュールは、`PduR_ComTransmit` を介したその I-PDU の PDU Router への送信時に、`PduR_ComTransmit` が `E_OK` を返す場合に限り、最小遅延時間のカウンタをロードし、開始しなければならない。（SRS_Com_02037）

**[SWS_Com_00469]** I-PDU の `ComMinimumDelayTime` が 0 より大きくコンフィギュレーションされている場合、AUTOSAR COM モジュールは、送信確認の成功時に、その I-PDU の、すでに動作していてまだ経過していない最小遅延時間のカウンタを、`ComMinimumDelayTime` で（再）ロードしなければならない。（SRS_Com_02037）

動作中の最小遅延タイマは、その I-PDU の TX 確認の成功を受信したときに、再ロードされる。ただし、確認の受信の時点で、すでに `ComMinimumDelayTime` より長く送信が遅延されていた場合を除く。通常の場合、ロードされて開始された最小遅延が満了しない限り、AUTOSAR COM モジュールによる、その I-PDU のそれ以上の送信は無い。[17] の図 2-4、2-5、2-7 も参照すること。ただし、いくつかの例外がある。[SWS_Com_00475] に従い、`Com_TriggerTransmit` は、最小遅延タイマに干渉しない。さらに、最小遅延タイマは、送信デッドライン監視タイマが満了した場合にリセットされる（[17] の 2.3.4 章を参照）。加えて、I-PDU グループを開始すると、含まれる I-PDU の最小遅延タイマがリセットされる。

**[SWS_Com_00812]** 送信デッドライン監視タイマが満了したために、I-PDU の最小遅延タイマがリセットされ、この I-PDU の保留された要求がある場合、AUTOSAR COM モジュールは、この I-PDU を直ちに送信しなければならない。（SRS_Com_02037）

#### 7.3.6 デッドライン監視

信号のデッドライン監視は、[17] で定義されている。

信号グループのデッドライン監視では、信号グループは、信号と同じように扱われる。デッドライン監視のパラメータ `ComFirstTimeout` と `ComTimeout` は、コンフィギュレーションコンテナ `ComSignal` または `ComSignalGroup` で定義できる。

対応するタイムアウト通知のコールバック関数は、コンフィギュレーションコンテナ `ComSignal` または `ComSignalGroup` の、パラメータ `ComTimeoutNotification` で定義できる。

**[SWS_Com_00333]** 信号または信号グループのコンフィギュレーションパラメータ `ComTimeout` が省略されているか、0 にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、この信号または信号グループを監視してはならない。`ComFirstTimeout` も無視しなければならない。（SRS_Com_02089）

##### 7.3.6.1 受信デッドライン監視

受信デッドライン監視は、7.3.5.1 節と同様に、I-PDU グループの制御で、有効・無効にできる。

**[SWS_Com_00772]** I-PDU の受信デッドライン監視は、その I-PDU が、受信デッドライン監視が有効な I-PDU グループに含まれる場合に限り、有効である。そうでない場合、I-PDU の受信デッドライン監視は無効である。（SRS_Com_00192）

**[SWS_Com_00292]** 更新ビットを持つ信号に、受信デッドライン監視がコンフィギュレーションされている場合、AUTOSAR COM モジュールは、更新ビットを持つ各信号／信号グループについて、個別の受信デッドライン監視を行わなければならない。`ComTimeout` のコンフィギュレーションは、ECUC_Com_00263 を参照。（SRS_Com_02089、SRS_Com_02058）

**[SWS_Com_00290]** AUTOSAR COM モジュールは、更新ビットを持たない信号について、I-PDU 単位の受信デッドライン監視を行わなければならない。（SRS_Com_02089、SRS_Com_02058）

**[SWS_Com_00291]** 同じ I-PDU の中の、更新ビットを持たないすべての信号と信号グループについて、AUTOSAR COM モジュールは、関連する信号と信号グループの、コンフィギュレーションされた 0 でない最小のタイムアウトパラメータ（`ComFirstTimeout`、`ComTimeout`）を使って、I-PDU の受信デッドライン監視を行わなければならない。（SRS_Com_02089、SRS_Com_02058）

受信デッドライン監視がコンフィギュレーションされた I-PDU の中のすべての信号が、更新ビットもコンフィギュレーションされている場合、I-PDU 単位の受信デッドライン監視を行う必要は無い。

Rx タイムアウトの場合、パラメータ `ComRxDataTimeoutAction` が、AUTOSAR COM モジュールが、信号／信号グループの値を、初期値または代替値で置き換えるか、最後に受信した値を維持するかを決める。ECUC_Com_00412 を参照。

**[SWS_Com_00470]** `ComRxDataTimeoutAction` が `REPLACE` に設定されている場合（ECUC_Com_00412 を参照）、信号の受信デッドライン監視タイマが満了したとき、AUTOSAR COM モジュールは、信号の値を、その `ComSignalInitValue`（ECUC_Com_00170 を参照）で置き換えなければならない。（SRS_Com_02087、SRS_Com_02088）

`ComSignalInitValue` が最後に受信した値を置き換える場合、最後に受信した値は上書きされ、失われる。したがって、AUTOSAR COM モジュールは、新しい値が受信されるまで、この信号について `ComSignalInitValue` を返す。

**[SWS_Com_00875]** `ComRxDataTimeoutAction` が `SUBSTITUTE` に設定されている場合（ECUC_Com_00412 を参照）、信号の受信デッドライン監視タイマが満了したとき、AUTOSAR COM モジュールは、信号の値を、その `ComTimeoutSubstitutionValue`（ECUC_Com_10006 を参照）で置き換えなければならない。（SRS_Com_02088）

**[SWS_Com_00500]** 信号または信号グループの受信デッドライン監視タイマが満了し、この信号または信号グループの `ComRxDataTimeoutAction` が省略されているか、`NONE` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、信号、または信号グループの信号を、それぞれ置き換えてはならない。（SRS_Com_02087）

**[SWS_Com_00513]** `ComRxDataTimeoutAction` が `REPLACE` に設定されている場合（ECUC_Com_00412 を参照）、信号グループの受信デッドライン監視タイマが満了したとき、AUTOSAR COM モジュールは、含まれるすべてのグループ信号の値を、その `ComSignalInitValue`（ECUC_Com_00170 を参照）で置き換えなければならない。（SRS_Com_02041）

Rx タイムアウトの通知は、ECUC_Com_00412 とは別に、組み合わせてコンフィギュレーションできる。

**[SWS_Com_00876]** `ComRxDataTimeoutAction` が `SUBSTITUTE` に設定されている場合（ECUC_Com_00412 を参照）、信号グループの受信デッドライン監視タイマが満了したとき、AUTOSAR COM モジュールは、含まれるすべてのグループ信号の値を、その `ComTimeoutSubstitutionValue`（ECUC_Com_10006 を参照）で置き換えなければならない。（SRS_Com_02088）

**[SWS_Com_00715]** 受信デッドライン監視がコンフィギュレーションされた（大きな）I-PDU を正常に受信したとき、AUTOSAR COM モジュールは、関数 `Com_RxIndication` または `Com_TpRxIndication` の呼び出し時に、この（大きな）I-PDU の受信デッドライン監視タイマをリセットしなければならない。（SRS_Com_02058）

**[SWS_Com_00716]** 信号または信号グループのコンフィギュレーションパラメータ `ComFirstTimeout` が省略されているか、0 にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、対応する I-PDU の開始から最初の受信までの、この信号または信号グループの受信を、それぞれ監視してはならない。（SRS_Com_02058）

**[SWS_Com_00738]** I-PDU 単位の受信デッドライン監視の仕組みは、信号の値を考慮してはならない。（SRS_Com_02089）

###### 7.3.6.1.1 受信デッドライン監視の有効化・無効化

I-PDU の受信デッドライン監視が無効のときにタイマが満了しても、RTE へエラーの通知は行われない。

受信デッドライン監視を無効にしても、I-PDU の受信は止まらない。

**[SWS_Com_00224]** I-PDU の受信デッドライン監視が `Com_EnableReceptionDM` で有効にされた場合、AUTOSAR COM モジュールは、含まれる信号と信号グループの受信デッドライン監視タイマを、コンフィギュレーションされた `ComFirstTimeout` の値に設定しなければならない。（SRS_Com_00192）

**[SWS_Com_00486]** AUTOSAR COM モジュールは、I-PDU の受信デッドライン監視が、この I-PDU についてすでに有効になっている場合、`Com_EnableReceptionDM` による、I-PDU の受信デッドライン監視を有効にする設定を、黙って無視しなければならない。（SRS_Com_00192）

受信デッドライン監視を有効にすることは、デッドライン監視の満了のタイムアウト通知が、コンフィギュレーションされた `ComTimeoutNotification`（ECUC_Com_00552）を持つ信号または信号グループについて、RTE へ通知されることを意味する。

**[SWS_Com_00534]** `Com_EnableReceptionDM` または `Com_DisableReceptionDM` が、Tx の I-PDU を含む I-PDU グループに対して呼び出された場合、AUTOSAR COM モジュールは、この要求を黙って無視しなければならない。（SRS_Com_00192）

**[SWS_Com_00225]** AUTOSAR COM モジュールは、I-PDU の受信デッドライン監視が、この I-PDU についてすでに無効になっている場合、`Com_DisableReceptionDM` による、I-PDU の受信デッドライン監視を無効にする設定を、黙って無視しなければならない。（SRS_Com_00192）

##### 7.3.6.2 送信デッドライン監視

AUTOSAR COM の送信デッドライン監視の一般的な考え方は、下位層とバスを監視することであり、COM モジュール自身を監視することではない。したがって、送信監視タイマは、一般に、COM モジュールが I-PDU を下位層へ送るときに開始される。これは、送信モード、転送プロパティ、更新ビットから独立している。一般に、送信監視タイマは、現在動作中の場合、再開もリセットもされない。

**[SWS_Com_00481]** AUTOSAR COM モジュールは、送信デッドライン監視（コンフィギュレーションされていれば）を、すべての信号と信号グループについて、転送プロパティ、および信号／信号グループが属する I-PDU の送信モードから独立して、行わなければならない。（SRS_Com_02037）

AUTOSAR COM モジュールは、送信デッドライン監視に関して、`ComTransferProperty` が `PENDING` の信号と `TRIGGERED` の信号を区別しない。

**[SWS_Com_00445]** I-PDU の、関連する信号／信号グループに、異なる `ComTimeout` パラメータがコンフィギュレーションされている場合、AUTOSAR COM モジュールは、最も小さい値を、I-PDU の送信デッドライン監視のタイムアウトパラメータとして使わなければならない。（SRS_Com_02037）

送信デッドライン監視は、送信確認の生成をサポートするバスについてだけ、AUTOSAR COM モジュールでコンフィギュレーションされるべきである。そうでなければ、送信デッドライン監視は、常に送信エラーを通知する。

**[SWS_Com_00878]** AUTOSAR COM は、信号（グループ）が（I-PDU の中で）下位層へ送られた場合、タイマがすでに動作していない限り、コンフィギュレーションされた、その信号（グループ）の送信デッドライン監視タイマを開始しなければならない。（SRS_Com_02037）

[SWS_Com_00878] は、起こりうる戻りコードを考慮しないため、送信要求が直ちに失敗した場合でも、タイマは開始される。

タイマがすでに動作している場合、新しい送信要求は、動作中のタイマをリセットも再開もせず、現在の未処理の送信要求の監視が続けられる。特に規定がない限り、タイマは、I-PDU の送信要求の可能性のあるきっかけに関係なく開始される。そのようなきっかけは、例えば、信号の周期的な送信要求、送信モードの切り替え、または `Com_TriggerIPduSend` による明示的な I-PDU の送信要求である。

**[SWS_Com_00879]** 送信デッドライン監視タイマは、この I-PDU の送信デッドライン監視サービスの（再）開始の後に、タイマが初めて開始される場合は、コンフィギュレーションされた `ComFirstTimeout` の値で開始され、そうでなければ `ComTimeout` の値で開始されなければならない。（SRS_Com_02037）

**[SWS_Com_00880]** AUTOSAR COM が I-PDU の送信確認を受信したとき、含まれるすべての信号と信号グループについて、動作中のすべての送信デッドライン監視タイマをキャンセルしなければならない。（SRS_Com_02037）

**[SWS_Com_00696]** 送信モード `NONE` だけの I-PDU に、送信デッドライン監視がコンフィギュレーションされている場合、AUTOSAR COM モジュールは、この I-PDU の送信デッドライン監視タイマを、I-PDU が属する I-PDU グループの開始時に開始しなければならない。（SRS_Com_02037）

**[SWS_Com_00835]** 送信モード `NONE` と別の送信モードを持つ I-PDU に、送信デッドライン監視がコンフィギュレーションされている場合、送信デッドライン監視は、送信モード `NONE` が有効な間は、常に無効にされなければならない。（SRS_Com_02037）

**[SWS_Com_00697]** 送信モード `NONE` だけの I-PDU に、送信デッドライン監視がコンフィギュレーションされている場合、AUTOSAR COM モジュールは、送信確認の成功を受信したときに、この I-PDU の送信デッドライン監視タイマをリセットしなければならない。（SRS_Com_02037）

信号グループの場合、送信デッドライン監視は、信号グループ全体にだけコンフィギュレーションでき、グループ信号にはコンフィギュレーションできない。ECUC_Com_00345 と ECUC_Com_00520 を参照。

**[SWS_Com_00708]** `ComIPduCancellationSupport` が `TRUE` にコンフィギュレーションされた I-PDU について、送信デッドライン監視タイマが満了した場合、AUTOSAR COM モジュールは、その I-PDU に対して `PduR_ComCancelTransmit` を呼び出さなければならない。（SRS_Com_02107）

`PduR_ComCancelTransmit` の呼び出しは、Com の通常のタイムアウトの処理（コンフィギュレーションされていれば SW-C へ通知する）に加えて行われる。AUTOSAR COM は、`PduR_ComCancelTransmit` の戻りコードを無視する。

**[SWS_Com_00304]** 送信デッドライン監視タイマが経過したとき、すなわち、I-PDU の送信確認の成功が時間内に無かったとき、AUTOSAR COM モジュールは、含まれる信号または信号グループについて、コンフィギュレーションされたすべての `ComTimeoutNotification` を呼んで、RTE へ通知しなければならない。ECUC_Com_00552 を参照。（SRS_Com_02037）

送信デッドライン監視タイマが切れた場合、理由に関係なく、タイムアウトの通知がある。例えば、送信が I-PDU のコールアウトにより除外された場合でも、通知は行われる。

###### 7.3.6.2.1 N 回の送信モードでの送信デッドライン監視

[17] で定義されているとおり、対応する信号または信号グループに送信デッドライン監視がコンフィギュレーションされている場合、監視タイマは、それぞれ `Com_SendSignal` または `Com_SendSignalGroup` の呼び出しの完了時に開始されなければならない。

送信モード `DIRECT` と `MIXED` では、すべての `ComTxModeNumberOfRepetitions` 回の要求が、コンフィギュレーションされた期間の中で行えることを、確実にするべきである（7.3.3.4 節を参照）。

[17] で定義されているとおり、監視タイマが満了した場合、その失敗について、コンフィギュレーションされた通知の仕組みで、RTE に通知される。

**[SWS_Com_00308]** `ComTxModeMode` が `DIRECT` で、`ComTxModeNumberOfRepetitions > 0` の I-PDU では、AUTOSAR COM モジュールは、`ComTxModeNumberOfRepetitions` に 1 を加えた回数の、送信確認の成功を受信した後に、送信デッドライン監視タイマをキャンセルしなければならない。（SRS_Com_02083）

タイマが、`ComTxModeNumberOfRepetitions` に 1 を加えた回数の確認の後にキャンセルされた場合、送信は成功であり、その後、送信確認が RTE へ送られる。[SWS_Com_00305] も参照すること。

**[SWS_Com_00739]** `ComTxModeMode` が `DIRECT` で、`ComTxModeNumberOfRepetitions > 0` の I-PDU では、AUTOSAR COM モジュールは、この I-PDU の別の送信要求が開始された場合、すでに動作中の送信デッドライン監視タイマをリセットしなければならない。（SRS_Com_02083）

> **学習メモ（原文でなく訳者の解釈）**: このプロジェクトの実装と照らすときの手がかり。
> - I-PDU グループの開始時に常に初期化するもの（[SWS_Com_00787]）と、`Initialize == true` のときだけ追加で初期化するもの（[SWS_Com_00222]）は別の組。受信デッドライン監視の再開は、`Initialize` の値に関係なく行われる（[SWS_Com_00733]）。
> - 受信デッドライン監視の有効・無効は、I-PDU グループの開始・停止とは別の軸（[SWS_Com_00772]、`Com_EnableReceptionDM` / `Com_DisableReceptionDM`）。
> - 停止している I-PDU への `Com_SendSignal` は、内部バッファを更新して `COM_SERVICE_NOT_AVAILABLE` を返す（[SWS_Com_00334]、表 3）。

### 7.4 信号グループ — 複合データ型

AUTOSAR の複合データ型の概念をサポートするため、AUTOSAR COM モジュールは、信号グループを提供する。AUTOSAR COM モジュールは、複合データ型に必要な一貫性を提供するために、信号グループを一貫して送受信する。

信号グループは、静的にコンフィギュレーションできる。各信号グループには、シンボリックな名前をコンフィギュレーションできる。コンフィギュレーションの詳細は ECUC_Com_00345 を参照。

AUTOSAR COM モジュールは、シャドウバッファの仕組みによって、信号グループの一貫性を実現する。すなわち、RTE は、シャドウバッファの中のグループ信号へアクセスする。シャドウバッファを I-PDU と同期させる必要がある場合、RTE は、`Com_SendSignalGroup` または `Com_ReceiveSignalGroup` で、これを明示的に起動できる。同期は、アトミックに行われる。

信号グループの一貫性への別のアプローチは、信号グループ配列 API の `Com_SendSignalGroupArray`（[SWS_Com_00851]）と `Com_ReceiveSignalGroupArray`（[SWS_Com_00854]）を有効にすることである。このアプローチでの相違点は、7.4.6 節で説明する。

#### 7.4.1 初期化

**[SWS_Com_00484]** `Com_Init` の呼び出しにより、AUTOSAR COM モジュールは、送信側の信号グループのシャドウバッファを初期化しなければならない。（SRS_BSW_00101）

正しく作られた SWC が、`Com_ReceiveSignalGroup` の呼び出しの前に、グループ信号を読もうとすることは想定されないため、[SWS_Com_00484] は送信側にだけ適用される。

#### 7.4.2 送信

信号グループに属する信号について、`Com_SendSignal` または `Com_InvalidateSignal` が呼ばれた場合、AUTOSAR COM は、この信号グループのシャドウバッファだけを更新する。I-PDU の内容が変わらない限り、TMS の評価などの、それ以上の I-PDU の処理は不要である。

**[SWS_Com_00050]** 信号グループについて `Com_SendSignalGroup` が呼ばれた場合、AUTOSAR COM モジュールは、シャドウバッファを I-PDU バッファへ、アトミックにコピーしなければならない。（SRS_Com_02041）

`group_x` に属する、2 つのグループ信号 `signal_a` と `signal_b` の例:

```c
/* a をシャドウバッファへコピー */
Com_SendSignal (signal_a, &a);

/* b をシャドウバッファへコピー */
Com_SendSignal (signal_b, &b);

/* シャドウバッファを I-PDU へコピー */
Com_SendSignalGroup (group_x);
```

#### 7.4.3 受信

グループ信号は、信号グループのデータが `Com_ReceiveSignalGroup` でシャドウバッファへコピーされた後に、関数 `Com_ReceiveSignal` を呼んで、シャドウバッファから受信できる。

**[SWS_Com_00051]** 信号グループについて `Com_ReceiveSignalGroup` が呼ばれた場合、AUTOSAR COM モジュールは、データを I-PDU バッファからシャドウバッファへ、アトミックにコピーしなければならない。（SRS_Com_02041）

`group_x` に属する、2 つのグループ信号 `signal_a` と `signal_b` の例:

```c
/* I-PDU をシャドウバッファへコピー */
Com_ReceiveSignalGroup (group_x);

/* a をシャドウバッファからコピー */
Com_ReceiveSignal (signal_a, &a);

/* b をシャドウバッファからコピー */
Com_ReceiveSignal (signal_b, &b);
```

#### 7.4.4 通知

表 5 は、すべての通知関数が、信号グループに対してだけコンフィギュレーションでき、個々のグループ信号に対してはできないことを示す。7.3 章は、信号と信号グループの通知方法の機能的な論理を定義する。コンフィギュレーションできる通知関数のプロトタイプは、8.6.3.1 節で定義する。

#### 7.4.5 信号グループの属性

表 5 は、信号グループの属性の概要を示す。

> **注（再構成）**: 原典の表 5 は、テキストの抽出で、行の対応が崩れていた。「グループ信号ごと」と「信号グループごと」の列を、内容と順序から復元した。「通知」の各行の「はい／いいえ」と、`Invalid Notification` の行、データアクセスの行は、確定できなかったため（推定）と付けた。重要な箇所は原典の表 5（62 ページ）で確認すること。

**表 5: 信号グループの属性**

| 属性 | グループ信号ごと | 信号グループごと |
|---|---|---|
| 更新ビット | なし | あり。グループ全体に関連付けられる（7.8 章を参照） |
| 信号の通知（送信側） | なし | あり |
| 信号の通知（受信側） | なし | あり |
| エラーの通知（送信側） | なし | あり |
| タイムアウトの通知（送信側） | なし | あり。[SWS_Com_00201] を参照（推定） |
| タイムアウトの通知（受信側） | なし | あり。[SWS_Com_00200]、[SWS_Com_00557] を参照（推定） |
| 無効の通知（受信側） | なし（推定） | あり（推定） |
| データアクセス（受信側） | あり（推定） | なし（推定） |
| データアクセス（送信側） | あり（推定） | なし（推定） |
| データのフィルタリング（受信側） | あり。7.2.4 節を参照 | — |
| データのフィルタリング（送信側） | なし | — |
| 送信側の TMS | 各信号について、TMS の選択の定義に従う（7.3.3.2 節を参照） | — |

#### 7.4.6 信号グループの UINT8 配列によるアクセス

信号グループの一貫性への別のアプローチは、COM のコンフィギュレーションパラメータ `ComSignalGroupArrayAccess`（ECUC_Com_10003）で、信号グループ配列 API の `Com_SendSignalGroupArray`（[SWS_Com_00851]）と `Com_ReceiveSignalGroupArray`（[SWS_Com_00854]）を有効にすることである。このとき、複合データのシリアライズは、COM モジュールの外で行われるが、COM がグループ信号を I-PDU に配置するときと同じレイアウトになる。このアプローチでは、シャドウバッファは使われない。シリアライズされたデータは、信号グループの uint8 配列による表現として、COM モジュールへ直接渡される。

**[SWS_Com_00841]** 信号グループの UINT8 配列によるアクセスは、次の前提条件がすべて成り立つ場合にだけ使わなければならない。

- 複合データに、固定サイズのデータ型だけを使う。
- 信号グループが、I-PDU へバイト境界に揃えて割り当てられている。
- 信号グループが、他の信号に分断されていない（ただし、隙間を含んでもよい）。

（SRS_Com_02112）

**[SWS_Com_00842]** 信号グループに `ComSignalGroupArrayAccess`（ECUC_Com_10003）が定義されている場合、その信号グループには、シャドウバッファを割り当ててはならない。（SRS_Com_02112）

**[SWS_Com_00843]** 信号グループに `ComSignalGroupArrayAccess`（ECUC_Com_10003）が定義されている場合、その信号グループについて、次の API の呼び出しはサポートされない。

- その信号グループのいずれかのグループ信号に対する `Com_SendSignal` と `Com_ReceiveSignal`
- その信号グループに対する `Com_SendSignalGroup` と `Com_ReceiveSignalGroup`

（SRS_Com_02112）

上記の要求は、配列アクセスの信号グループでは、特定の API がサポートされないことを定義する。すなわち、これらの API は、配列アクセスの信号グループについて呼んではならない。不正に呼ばれた場合、動作は未定義である。

##### 7.4.6.1 基本機能

AUTOSAR のトランスフォーマのアプローチが使われる場合、シリアライズとそれ以降のデータ処理は、すでにトランスフォーマのチェーンで行われている。トランスフォーマのチェーンは、すでに通信データの uint8 配列による表現を扱っているため、信号グループの uint8 配列によるアクセスにより、すでにシリアライズされたデータを使い、COM が再びデータをシリアライズする必要なしに、それを I-PDU へ配置できる。

AUTOSAR System Template [12] は、どの信号グループを uint8 配列によるアプローチで扱うかを定義する。System Template が uint8 配列によるアプローチを有効にする場合、ECU のコンフィギュレーションパラメータ `ComSignalGroupArrayAccess`（ECUC_Com_10003）が真に設定される。

**[SWS_Com_00844]** 通信データの uint8 配列による表現の開始位置は、この信号グループの中で、`ComBitPosition` が最小のグループ信号の、バイトオフセットでなければならない。（SRS_Com_02112）

**[SWS_Com_00845]** 通信データの uint8 配列による表現の長さは、この `ComSignalGroup` のすべての `ComGroupSignal` が、`ComIPdu` の中で占めるバイト数でなければならない。（SRS_Com_02112）

##### 7.4.6.2 初期化

**[SWS_Com_00850]** `Com_Init` の呼び出しにより、AUTOSAR COM モジュールは、`ComSignalGroupArrayAccess`（ECUC_Com_10003）が真にコンフィギュレーションされた信号グループの、対応する I-PDU バッファの部分を、含まれるすべての `ComGroupSignal` の `ComSignalInitValue` に基づいて初期化しなければならない。（SRS_Com_02112、SRS_BSW_00101）

##### 7.4.6.3 送信

**[SWS_Com_00846]** `Com_SendSignalGroupArray` が呼ばれるとき、信号グループの uint8 配列による表現へのデータポインタが渡される。AUTOSAR COM モジュールは、[SWS_Com_00844] で定義された開始位置と、[SWS_Com_00845] で定義されたサイズに基づいて、データを I-PDU へコピーしなければならない。（SRS_Com_02112）

**[SWS_Com_00847]** AUTOSAR COM モジュールは、`Com_SendSignalGroupArray` の呼び出しを、次の項目に関して、`Com_SendSignalGroup` と同じように扱わなければならない。

- 7.3.3.1.2 節 信号グループ（転送プロパティと I-PDU の送信モード）
- 7.3.3.2 節 特定の I-PDU の送信モードの選択
- 7.3.3.3 節 信号の流れと送信モードの選択

（SRS_Com_02112）

信号グループが uint8 配列による表現で渡されるため、送信モードの選択は、グループ信号を解析するときに、このデータの表現に対応する必要がある。

送信モードの選択を簡単にするため、[SWS_Com_00602] のフィルタのうち、一部だけがサポートされる。次の 2 つのフィルタは、`Com_SendSignalGroupArray` の TMS のサポートから除外される。

- `NEW_IS_WITHIN`
- `NEW_IS_OUTSIDE`

**[SWS_Com_00848]** `Com_SendSignalGroupArray` の呼び出しは、送信モードの選択（TMS）のために、次のフィルタをサポートしなければならない。

- `ALWAYS`
- `NEVER`
- `MASKED_NEW_EQUALS_X`
- `MASKED_NEW_DIFFERS_X`
- `MASKED_NEW_DIFFERS_MASKED_OLD`
- `ONE_EVERY_N`

（SRS_Com_02112）

##### 7.4.6.4 受信

**[SWS_Com_00849]** `Com_ReceiveSignalGroupArray` が呼ばれるとき、信号グループの uint8 配列による表現へのデータポインタが渡される。AUTOSAR COM モジュールは、[SWS_Com_00844] で定義された開始位置と、[SWS_Com_00845] で定義されたサイズに基づいて、I-PDU のデータを、データポインタの位置へコピーしなければならない。（SRS_Com_02112）

### 7.5 大きなデータ型

大きな信号とは、基盤の通信プロトコルの 1 つの L-PDU に収まらない大きさの信号である。例えば、CAN での大きな信号は、8 バイト（CAN FD では 64 バイト）を超える。FlexRay では、1 つのフレームに複数の I-PDU を含められるため、状況はより複雑である。したがって、信号が FlexRay のフレームのサイズを超えない場合でも、FlexRay にとっては大きな信号になりうる。

AUTOSAR での大きな信号には、基盤のバスのトランスポートプロトコルで送信される、大きな I-PDU をコンフィギュレーションする必要がある。大きな I-PDU が、1 つ以上の大きな信号を含むことは必須ではない。この概念を使うと、I-PDU の長さ、したがって信号のサイズは、バス固有のトランスポートプロトコルに従って制限される。ECUC_Com_00437 を参照。

`UINT8_N` は、8 バイトより大きい静的なサイズの信号をサポートする、唯一の信号型である。動的な長さの信号は、7.6 節を参照。

AUTOSAR COM モジュールは、基盤のバスの特性を知らないため、I-PDU を 1 つの L-PDU（例: 1 つの CAN フレーム）で送信できるのか、TP を介して送信する必要があるのかを、コンフィギュレーションする必要がある。ECUC_Com_00761 も参照すること。

AUTOSAR COM モジュールは、バスに依存しない PDU Router のインタフェースを使うため、大きすぎる I-PDU が特定のバスのトランスポートプロトコルへ送られることを、防ぐことも検出することもできない。ただし、AUTOSAR COM モジュールは、基盤の TP の N-SDU の最大長を使えるようにするため、AUTOSAR COM モジュールが、追加の長さの制限を導入することはない。

#### 7.5.1 大きな信号／I-PDU の送信

AUTOSAR COM モジュールは、トランスポートプロトコルを使う上位層のモジュールのための、汎用の PduR の API を介して、大きな I-PDU を送る。AUTOSAR COM モジュールと PDU Router のインタフェースの詳細は、7.7 章を参照。[10] によれば、呼び出しの順序は次のとおりである。

- `PduR_ComTransmit`: 送信要求を開始するため
- `Com_CopyTxData` への 1 回以上のコールバック: データをコピーするため
- `Com_TpTxConfirmation` へのコールバック: 送信を確認するため

**[SWS_Com_00662]** `PduR_ComTransmit` による大きな I-PDU の送信の開始が成功した後、AUTOSAR COM モジュールは、送信の成功、または送信が中止されたことを示すエラーが通知されない限り、この I-PDU の内部の送信バッファのデータを変更してはならない。（SRS_Com_02095）

AUTOSAR COM モジュールの中の I-PDU は、静的にコンフィギュレーションされる。したがって、非常に大きな I-PDU であっても、I-PDU のメモリ全体が確保される。このアプローチは、大きなデータ型の扱いの複雑さを減らすために採用された。

使われるトランスポートプロトコルが、診断などの他のサービスにも使われる場合、複数のユーザをサポートしなければならない。これは、少なくとも CAN TP と FlexRay TP でサポートされる。

**[SWS_Com_00863]** 大きな I-PDU が現在送信されていて、同じ I-PDU が再び送信のために起動された場合、AUTOSAR COM は、追加の送信要求をスキップし、ランタイムエラー `COM_E_SKIPPED_TRANSMISSION` を報告しなければならない。（SRS_Com_02095）

これは、例えば、大きな I-PDU が周期的に送り出され、送信が次の周期を超えて遅れた場合に起こりうる。

#### 7.5.2 大きな信号／I-PDU の受信

AUTOSAR COM モジュールは、トランスポートプロトコルを使う上位層のモジュールのための、汎用の PDU Router の API を介して、大きな I-PDU を受信する。AUTOSAR COM モジュールと PDU Router のインタフェースの詳細は、7.8 章を参照。[10] によれば、呼び出しの順序は次のとおりである。

- `Com_StartOfReception` の呼び出し: 受信要求を開始するため
- `Com_CopyRxData` への 1 回以上のコールバック: 受信データを、AUTOSAR COM モジュールの内部の受信バッファへコピーするため
- `Com_TpRxIndication` の呼び出し: 受信処理の正常な、または異常な終了を知らせるため

**[SWS_Com_00838]** `Com_TpRxIndication` がパラメータ `result` の値 `E_NOT_OK` で呼ばれた場合、AUTOSAR COM モジュールは、受信した大きな I-PDU のすべての信号を、無効化された信号とまったく同じように扱わなければならない。すなわち、AUTOSAR COM モジュールは、含まれるすべての信号と信号グループについて、コンフィギュレーションされたすべての `ComDataInvalidAction` を実行しなければならない。（SRS_Com_02095）

### 7.6 動的な長さの信号

動的な長さの信号とは、実行時に長さが変わりうる信号である。動的な長さの信号の最大長は、コンフィギュレーションの時点で指定する必要がある。動的な長さの信号は、動的な長さの I-PDU で送信しなければならない。

**[SWS_Com_00753]** 動的な長さの信号では、AUTOSAR COM モジュールは、`ComSignalType` の `UINT8_DYN` だけをサポートしなければならない。（SRS_Com_02094、SRS_Com_02098）

型を UINT8 配列の型に制限することは、動的な長さの信号の配置を、バイト境界にも制限する。

大きな信号を含む可能性のある、他のすべての信号は、動的な長さの信号の前に詰めなければならない（[SWS_Com_00754]、[SWS_Com_00755]、[SWS_Com_00756] を参照）。したがって、動的な長さの信号の長さを、I-PDU の中に符号化する必要は無い。受信側では、動的な長さの信号の長さは、受信した I-PDU の長さから計算できる。

動的な長さの信号に、更新ビットをコンフィギュレーションすることは許される。この場合、更新ビットは、動的な長さの信号の前に置かなければならない。[SWS_Com_00755] も参照すること。

AUTOSAR COM モジュールには、信号グループを、大きな信号や動的な長さの信号と組み合わせて使うことを防ぐ制限は無い。

動的な長さの信号は、トランスポートプロトコルでの送信に限定されない。ただし、動的な長さの信号を含む I-PDU の最大長は、バス固有の制限を守らなければならない。

CAN FD で送る、動的な長さの信号を含む I-PDU では、IpduM の Multi PDU Mapping 機能を使うことが推奨される。これにより、CAN FD のフレームの離散的なデータ長のために、受信した長さを誤って解釈することを避けられる。

#### 7.6.1 動的な長さの信号／I-PDU の送信

動的な長さの信号を送るには、RTE が、送信の呼び出しで実際の長さを指定する必要がある。対応する API は `Com_SendDynSignal`（[SWS_Com_00627]）である。

**[SWS_Com_00757]** `Com_SendDynSignal` の呼び出し時に、AUTOSAR COM モジュールは、対応する動的な長さの I-PDU の長さを、動的な長さの信号を含む最小の長さに設定しなければならない。動的な信号の長さは、API `Com_SendDynSignal` で渡された入力パラメータ `Length` の値に等しくなければならない。（SRS_Com_02093）

AUTOSAR COM モジュールは、動的な長さの I-PDU を、通常の I-PDU または大きな I-PDU として送れる。いずれの場合も、動的な I-PDU の長さを下位層へ渡す必要がある。

**[SWS_Com_00832]** AUTOSAR COM モジュールが、動的な長さの I-PDU のために `PduR_ComTransmit` を呼ぶとき、`PduInfoType` の `SduLength` は、含まれる動的な長さの信号の最後の更新（まだ送られていない場合は初期値）に従って設定されなければならない。（SRS_Com_02093）

#### 7.6.2 動的な長さの信号／I-PDU の受信

動的な長さの信号を受信するには、RTE に実際のサイズを知らせる必要がある。

**[SWS_Com_00758]** 動的な長さの I-PDU の受信時に、AUTOSAR COM モジュールは、含まれる動的な長さの信号の長さを、次のように計算しなければならない。

```
<動的な信号の長さ（バイト）> = <受信した I-PDU の長さ（バイト）>
                              − <動的な長さの信号の ComBitPosition> / 8
```

（SRS_Com_02093）

`Com_ReceiveDynSignal`（[SWS_Com_00690]）は、[SWS_Com_00758] で定義されたとおりに計算された、動的な長さの信号の長さを返す。

動的な長さの信号の長さは、I-PDU の受信した長さから導かれるため、AUTOSAR COM モジュールは、動的な長さの I-PDU と、メタデータを持つ I-PDU の組み合わせをサポートしない。[SWS_Com_00817] を参照。

### 7.7 AUTOSAR COM モジュールと PDU Router のインタフェース

[17] は、COM と下位層のインタフェースを定義していない。AUTOSAR では、AUTOSAR COM モジュールがインタフェースする下位層は、PDU Router だけである。

AUTOSAR COM モジュールは、PDU Router を、I-PDU の種類により、2 つの異なるモードで使う。すなわち、バス固有のトランスポートプロトコルで運ばれるか、そうでないかである。I-PDU を TP で送るかどうかは、コンフィギュレーションパラメータ `ComIPduType` でコンフィギュレーションされる。

**[SWS_Com_00138]** AUTOSAR COM モジュールは、`PduR_ComTransmit` 関数を呼んで、I-PDU を送り出さなければならない。（SRS_Com_02037）

**[SWS_Com_00759]** AUTOSAR COM モジュールが、`ComIPduType` が `NORMAL` にコンフィギュレーションされた I-PDU を送り出す場合、AUTOSAR COM モジュールは、TP を使わないモジュールのための Com のインタフェースを使って、PDU Router から呼ばれることを期待しなければならない。表 6 の「通常の I-PDU」の列を参照。（SRS_Com_02096）

**[SWS_Com_00760]** AUTOSAR COM モジュールが、`ComIPduType` が `TP` にコンフィギュレーションされた I-PDU を送り出す場合、AUTOSAR COM モジュールは、TP を使うモジュールのための Com のインタフェースを使って、PDU Router から呼ばれることを期待しなければならない。表 6 の「TP の I-PDU」の列を参照。（SRS_Com_02096）

9.1 章のインタラクション図は、通常の I-PDU について、PDU Router モジュールと AUTOSAR COM モジュールの間のやり取りを示す。

表 6 は、コンフィギュレーションされた `ComIPduType` の値に関して、具体的な I-PDU に、AUTOSAR COM モジュールと PDU Router の間のどの API が使われるかの概要を示す。API の説明自体は、第 8 章で規定される。

**表 6: `ComIPduType` に関する、PDU Router との API**

| API | 通常の I-PDU | TP の I-PDU |
|---|---|---|
| `PduR_ComTransmit` | ○ | ○ |
| `Com_TxConfirmation` | ○ | — |
| `Com_RxIndication` | ○ | — |
| `Com_TriggerTransmit` | ○ | — |
| `Com_TpTxConfirmation` | — | ○ |
| `Com_TpRxIndication` | — | ○ |
| `Com_StartOfReception` | — | ○ |
| `Com_CopyRxData` | — | ○ |
| `Com_CopyTxData` | — | ○ |

### 7.8 更新ビット

信号／信号グループの受信側が、送信側が送る前に、この信号／信号グループのデータを更新したかを識別できるようにするため、AUTOSAR COM モジュールは、更新ビットをサポートする。

更新ビットは、送信側の RTE が、その信号を含む I-PDU が PDU Router へ送られる前に、信号の値を更新したかを示す。

更新ビットは、`ComTxModeNumberOfRepetitions` が 1 以上の送信モード `DIRECT` が使われる場合は、許されない（[SWS_Com_00310] を参照）。

送信側と受信側のコンフィギュレーションにより、各信号、または各信号グループに、別々に、最大で 1 つの更新ビットを追加できる。更新ビットの位置は、コンフィギュレーションパラメータ `ComUpdateBitPosition`（ECUC_Com_00257）でコンフィギュレーションできる。`ComUpdateBitPosition` は、コンフィギュレーションコンテナ `ComSignal` に含まれる。したがって、信号／信号グループと、対応する更新ビットが、常に同じ I-PDU の一部であることが保証される。

**[SWS_Com_00055]** AUTOSAR COM モジュールは、更新ビットを、内部でだけ扱い、信号または信号グループの一部として扱ってはならない。（SRS_Com_02030）

更新ビットは、AUTOSAR のソフトウェアコンポーネントから、直接は見えず、アクセスもできない。

**[SWS_Com_00059]** AUTOSAR COM モジュールは、更新ビットを、表 7 で定義されたとおりに解釈しなければならない。（SRS_BSW_00101、SRS_Com_02030）

**表 7: 更新ビットの解釈**

| 更新ビット | 意味 |
|---|---|
| 0 | クリアされている／データは更新されていない |
| 1 | セットされている／データは更新された |

#### 7.8.1 送信側

更新ビットの初期化は、[SWS_Com_00117] で定義されている。

**[SWS_Com_00061]** RTE が `Com_SendSignal` を呼んで信号の値を更新した場合、AUTOSAR COM モジュールは、この信号の更新ビットをセットしなければならない。（SRS_Com_02030）

**[SWS_Com_00801]** RTE が `Com_SendSignalGroup` を呼んで信号グループを更新した場合、AUTOSAR COM モジュールは、この信号グループの更新ビットをセットしなければならない。（SRS_Com_02030）

**[SWS_Com_00062]** I-PDU のパラメータ `ComTxIPduClearUpdateBit` が `Transmit` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、この I-PDU が `PduR_ComTransmit` で送り出され、`PduR_ComTransmit` が `E_OK` を返した後に、含まれるすべての信号と信号グループの、すべての更新ビットをクリアしなければならない。（SRS_Com_02030）

**[SWS_Com_00577]** I-PDU のパラメータ `ComTxIPduClearUpdateBit` が `Confirmation` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、この I-PDU が `PduR_ComTransmit` で送り出され、`PduR_ComTransmit` が `E_OK` を返し、I-PDU が正常に確認された後に、含まれるすべての信号と信号グループの、すべての更新ビットをクリアしなければならない。（SRS_Com_02030）

**[SWS_Com_00578]** I-PDU のパラメータ `ComTxIPduClearUpdateBit` が `TriggerTransmit` にコンフィギュレーションされている場合、AUTOSAR COM モジュールは、この I-PDU の内容が `Com_TriggerTransmit` により正常に要求された後に、含まれるすべての信号と信号グループの、すべての更新ビットをクリアしなければならない。（SRS_Com_02030）

#### 7.8.2 受信側

**[SWS_Com_00324]** 更新ビットが付いた信号または信号グループを受信した場合、AUTOSAR COM モジュールは、その信号の更新ビットがセットされている場合にだけ、この信号を処理（すなわち、フィルタ、通知、信号ベースのゲートウェイ、バイトスワップ）しなければならない。（SRS_Com_02030）

**[SWS_Com_00802]** 更新ビットが付いた信号または信号グループを受信し、更新ビットがセットされていない場合、AUTOSAR COM モジュールは、この信号を破棄しなければならない。（SRS_Com_02030）

信号が更新されていない場合、信号は信号ゲートウェイを介して経路付けされず、破棄されるだけである。

RTE が、更新ビットのクリアされた信号を読んだ場合、初期値、または最後に受信した値が返される。

**[SWS_Com_00067]** AUTOSAR COM モジュールは、信号に更新ビットが付いていて、その更新ビットの値がセットされている場合、信号／信号グループを、更新されたものと解釈しなければならない。（SRS_Com_02030）

更新ビットを持つ信号での、受信デッドライン監視の動作は、7.3.6.1 節を参照。

### 7.9 データシーケンス制御

AUTOSAR COM モジュールは、I-PDU カウンタの形で、データシーケンス制御の仕組みを提供する。順序の外れた I-PDU、すなわち、繰り返された I-PDU や、欠落した I-PDU の後に受信した I-PDU は、検出され、破棄される。

I-PDU カウンタはオプションで、I-PDU ごとにコンフィギュレーションできる。対応するコンフィギュレーションコンテナは、ECUC_Com_00592 を参照。

#### 7.9.1 送信側

**[SWS_Com_00687]** `ComIPduCounter` がコンフィギュレーションされた、`ComIPduDirection` が `SEND` の、すべての I-PDU について、AUTOSAR COM モジュールは、I-PDU が `Com_Init` で初期化された後、または `Com_IpduGroupStart` で再初期化された後に、I-PDU カウンタを 0 に設定しなければならない。（SRS_Com_02101）

**[SWS_Com_00688]** AUTOSAR COM モジュールは、I-PDU が `PduR_ComTransmit` で送り出される直前に、I-PDU の I-PDU カウンタを、カウンタのラップアラウンドを考慮して 1 だけ増やさなければならない。`PduR_ComTransmit` の戻り値が `E_OK` に等しくない場合、I-PDU カウンタの更新は、（I-PDU カウンタを 1 だけ減らして）取り消されなければならない。（SRS_Com_02101）

`Com_TriggerTransmit` が I-PDU カウンタを含む I-PDU を取得する場合、インクリメントは行われない。したがって、I-PDU カウンタを `Com_TriggerTransmit` と組み合わせて使う場合は、注意が必要である。対応する `PduR_ComTransmit` の後に、`Com_TriggerTransmit` が複数回呼ばれた場合、すべての I-PDU が、同じカウンタ値を含む。したがって、遅延された `TriggerTransmit` による送信は動作しうるが、下位層主導の純粋な `TriggerTransmit` の使用例は、I-PDU カウンタとの組み合わせでは動作しない。

#### 7.9.2 受信側

**[SWS_Com_00587]** `ComIPduCounter` がコンフィギュレーションされた、`ComIPduDirection` が `RECEIVE` の、すべての I-PDU について、AUTOSAR COM モジュールは、I-PDU が `Com_Init` で初期化された後、または `Com_IpduGroupStart` で再初期化された後は、I-PDU カウンタの値に関係なく、到着する任意の I-PDU を受け付けなければならない。（SRS_Com_02099）

**[SWS_Com_00588]** I-PDU カウンタを含む I-PDU の受信時に、AUTOSAR COM モジュールは、次に期待する値を、受信した値の次の値（カウンタのラップアラウンドを考慮）に設定しなければならない。（SRS_Com_02101）

例: I-PDU カウンタが 4 ビットの場合、取りうる値は 0〜15 である。4 ビットのカウンタを持つ I-PDU を受信し、受信したカウンタ値が 15 の場合、次に期待する値は 0 である。新しい期待値は、受信した値が実際の期待値と一致するかに関係なく、更新される必要がある。

**[SWS_Com_00590]** I-PDU カウンタを含む I-PDU の受信時に、この I-PDU のレプリケーションがコンフィギュレーションされておらず、次のいずれかの場合、AUTOSAR COM モジュールは、[SWS_Com_00588] で定義されたとおりに次に期待する値を設定した後、I-PDU を破棄しなければならない。

- 受信した I-PDU カウンタ < 期待する I-PDU カウンタ、または
- 受信した I-PDU カウンタ > 期待する I-PDU カウンタ + `ComIPduCounterThreshold`

（カウンタのラップアラウンドを考慮する。）（SRS_Com_02102）

**[SWS_Com_00727]** 期待する I-PDU カウンタと受信した I-PDU カウンタの不一致が検出された場合、AUTOSAR COM モジュールは、この不一致を、コンフィギュレーションされた `ComIPduCounterErrorNotification`（ECUC_Com_00003）通知関数で通知しなければならない。（SRS_Com_02102）

カウンタエラーの通知は、コンフィギュレーションされた `ComIPduCounterThreshold` とは独立に起動される。

この I-PDU カウンタエラー通知関数のシグネチャは、[SWS_Com_00726] で定義されている。

例: 表 8 の例では、I-PDU カウンタは 4 ビット幅（値 0〜15）で、`ComIPduCounterThreshold` は 1 にコンフィギュレーションされている。

> **注（再構成）**: 原典の表 8 は、テキストの抽出で、「動作」の列の行がずれていた。規則（[SWS_Com_00590]）から、各行の動作を復元した。

**表 8: I-PDU カウンタの扱いの例**

| 期待する I-PDU カウンタ | 受信したカウンタ値 | 動作 |
|---|---|---|
| 6 | 6 | 通常の受信 |
| 6 | 7 | 通常の受信 |
| 6 | 8 | 破棄 |
| 6 | 1 | 破棄 |
| 15 | 15 | 通常の受信 |
| 15 | 0 | 通常の受信 |
| 15 | 1 | 破棄 |

### 7.10 通信の保護

安全に関する I-PDU は、データの破損と欠落を防ぐため、複数の L-PDU に複製される必要があるかもしれない。AUTOSAR では、I-PDU の複製（レプリケーション）は、PDU Router により実現される。複製された I-PDU の比較と多数決は、AUTOSAR COM で行われる。複製された I-PDU には、比較のためのチェックサムや署名が無いため、受信側で、I-PDU 全体を保存して比較する必要がある。AUTOSAR COM では、`ComIPduReplicaRef`（ECUC_Com_00601）で、追加の 2 つの複製された I-PDU をコンフィギュレーションできる。

適格な受信のために必要な、等しく受信された I-PDU の数は、`ComIPduReplicationQuorum`（ECUC_Com_00600）で、1 から 3 の間でコンフィギュレーションできる。

PDU Router モジュールは、TP モジュールを介して送受信され、送信に複数のフレームを必要とする I-PDU の、1:n の経路付けをサポートしない。したがって、AUTOSAR COM スタックは、現在、大きな、または動的な長さの I-PDU やデータ型の複製をサポートしない。

#### 7.10.1 送信側

複製自体は PduR により行われるため、AUTOSAR COM に、送信側の特別な扱いは無い。ただし、複製された I-PDU には、I-PDU カウンタがコンフィギュレーションされていることを、確実にしなければならない。[SWS_Com_00834] を参照。これは、受信側での多数決が、I-PDU カウンタに依存するために必要である。

#### 7.10.2 受信側

受信側では、AUTOSAR COM モジュールは、複数の複製された I-PDU について、多数決を行う。AUTOSAR COM モジュールは、正常に確認された I-PDU の信号だけを、RTE へ渡す。上述のとおり、複製された I-PDU には、I-PDU カウンタをコンフィギュレーションする必要がある。ただし、`ComIPduCounterThreshold` は 0 にコンフィギュレーションしなければならない。ECUC_Com_00595 を参照。これら 2 つの機能を混ぜて使う使用例は無く、多数決がより複雑になるためである。

**[SWS_Com_00596]** AUTOSAR COM モジュールが、複製された I-PDU の同一のコピーを、少なくとも `ComIPduReplicationQuorum` 個受信した場合に限り、この I-PDU の信号や信号グループを RTE へ提供しなければならない。（SRS_Com_02103、SRS_Com_02105、SRS_Com_02106）

複製された I-PDU は、AUTOSAR COM モジュールのコンフィギュレーションの中では、通常の I-PDU としてコンフィギュレーションされる。送信側のファンアウトは、PduR のコンフィギュレーションでコンフィギュレーションされる。受信側では、さらに、PDU-R の PDU の最大 2 つの複製（ECUC_Com_00601 `ComIPduReplicaRef` を参照）と、多数決を成功させるために必要な、同一に受信した I-PDU の数（ECUC_Com_00600 `ComIPduReplicationQuorum` を参照）を、AUTOSAR COM モジュールのコンフィギュレーションの中でコンフィギュレーションできる。

**[SWS_Com_00597]** AUTOSAR COM モジュールが、複製された I-PDU の同一のコピーを、少なくとも `ComIPduReplicationQuorum` 個受信した場合、この I-PDU の信号や信号グループについて、RTE へ 1 回だけ通知しなければならない。（SRS_Com_02103、SRS_Com_02105、SRS_Com_02106）

I-PDU カウンタは、7.9 章で定義されたとおり、複製された各 I-PDU について別々に扱われる。

I-PDU には特別なチェックサムが無いため、比較は、I-PDU 全体をバイトごとに比較して行われることが期待される。

I-PDU の複製では、`ComIPduCounterThreshold` を 0 にコンフィギュレーションする必要がある。[SWS_Com_00834] を参照。したがって、多数決を成功させるために I-PDU を保存するには、`ComIPduReplicaRef` + 1 個のバッファで十分である。

#### 7.10.3 制約

ファンアウトに PduR を使う、すでにある機能を再利用するアプローチのため、システムを正しく設定するよう、注意が必要である。PduR は、複製された I-PDU を、異なるバスへファンアウトできる。受信側では、これらの I-PDU は、同じ I-PDU ID で COM へ通知される。ただし、`Com_RxIndication`（[SWS_Com_00123]）は、同じ I-PDU ID に対して、再入可能に呼んではならない。これを防ぐ簡単な方法は、すべての I-PDU が同じインタフェースモジュールで受信されるようにすることで、その結果、すべての複製に同じバスを使うことになる。現在の AUTOSAR スタックでは、これが推奨される方法である。ただし、システムインテグレータは、他の手段で受信経路を同期させてもよい。

さらに、上位層から I-PDU への更新を防ぐ仕組みが無いため、この通信保護のアプローチは、分離された送信、すなわち `Com_TriggerTransmit` で要求される I-PDU との組み合わせでは動作しない。

### 7.11 信号ゲートウェイ

信号ゲートウェイは、AUTOSAR COM モジュールの統合された一部である。周期タスクの呼び出しを除き、外部のモジュールは、信号ゲートウェイへアクセスできない。

AUTOSAR COM モジュールの信号ゲートウェイは、（グループ）信号と信号グループを扱う。

AUTOSAR COM モジュールの信号ゲートウェイは、コンフィギュレーションコンテナ `ComGwMapping`（ECUC_Com_00544）に対する、静的な経路付けだけをサポートする。経路付けされる信号や信号グループの内容に依存せずに、経路をコンフィギュレーションすることだけが可能である。

信号または信号グループの宛先は、`ComGwDestinationDescription` コンテナ（ECUC_Com_00549）ごと、または `ComGwSignal` の参照（ECUC_Com_00551）で、コンフィギュレーションできる。送信モードは、I-PDU の他の関連する信号の TMC に基づいて選択される。TMS の計算に寄与する信号が無い場合、送信モード `ComTxModeTrue`（[SWS_Com_00677]）が使われる。

**[SWS_Com_00377]** AUTOSAR COM モジュールの信号ゲートウェイは、経路付けされる信号／信号グループの値を、コンフィギュレーションに従って、送信用の信号／信号グループへコピーしなければならない。ECUC_Com_00544 を参照。（SRS_PduR_06055、SRS_PduR_06089）

`ComGwMapping` のコンフィギュレーションでは、1 つの送信元の信号／信号グループから、0 個（信号ゲートウェイの機能なし）以上の宛先への、経路付け（1:n）をコンフィギュレーションできる。

AUTOSAR COM モジュールの信号ゲートウェイは、大きな I-PDU に含まれる信号や信号グループの処理をサポートしない。[SWS_Com_00598] を参照。

**[SWS_Com_00872]** 信号ゲートウェイは、受信側で、次の I-PDU／信号の処理段階をサポートしなければならない（図 6 も参照）。

1. I-PDU 単位の監視のために、受信デッドライン監視タイマをリセットする。
2. I-PDU のコールアウト
3. 更新ビットの検査
4. エンディアン変換と符号拡張

（SRS_PduR_06055、SRS_PduR_06089）

**[SWS_Com_00873]** 信号ゲートウェイは、送信側で、次の I-PDU／信号の処理段階をサポートしなければならない（図 6 も参照）。

1. 更新ビットのセット
2. エンディアン変換と符号拡張
3. I-PDU の送信モードの選択
4. I-PDU のコールアウト

（SRS_PduR_06055、SRS_PduR_06089）

#### 7.11.1 信号の扱い

**[SWS_Com_00357]** AUTOSAR COM モジュールは、経路付けされる信号を、受信した I-PDU から送信 I-PDU へ転送しなければならない。コンフィギュレーションは、ECUC_Com_00544 のコンフィギュレーションコンテナ `ComGwMapping` を参照。（SRS_PduR_06002）

経路付けされる信号が送信 I-PDU へ転送された後に、受信 I-PDU を停止しても、送信 I-PDU も停止しない限り、送信 I-PDU の送信は防がれない。

**[SWS_Com_00360]** 経路付けされる受信した信号のエンディアンが、関連する宛先の信号のエンディアンと異なる場合、AUTOSAR COM モジュールは、そのエンディアンを、宛先の信号のエンディアンへ変換しなければならない。（SRS_PduR_06061）

#### 7.11.2 信号グループの扱い

**[SWS_Com_00361]** AUTOSAR COM モジュールは、経路付けされる信号グループを、受信した I-PDU から送信 I-PDU へ転送しなければならない。コンフィギュレーションコンテナ `ComGwMapping`（ECUC_Com_00544）も参照。（SRS_PduR_06002、SRS_PduR_06056、SRS_PduR_06061）

経路付けされる信号グループが送信 I-PDU へ転送された後に、受信 I-PDU を停止しても、送信 I-PDU も停止しない限り、送信 I-PDU の送信は防がれない。

**[SWS_Com_00383]** AUTOSAR COM モジュールは、信号グループを一貫した形で経路付けしなければならない。したがって、AUTOSAR COM モジュールは、経路付けの操作の間、信号グループのデータを、一貫した 1 組のデータとして転送しなければならない。（SRS_PduR_06056）

**[SWS_Com_00735]** AUTOSAR COM モジュールは、送信元の信号グループのグループ信号の部分集合を、縮小された宛先の信号グループへ、一貫して経路付けすることをサポートしなければならない。（SRS_PduR_06056）

**[SWS_Com_00833]** AUTOSAR COM モジュールは、送信元の信号グループの個々のグループ信号を、（信号グループの一部ではない）個々の宛先の信号へ、経路付けすることをサポートしなければならない。（SRS_PduR_06056）

**[SWS_Com_00362]** 経路付けされる受信したグループ信号のエンディアンが、関連する宛先の（グループ）信号のエンディアンと異なる場合、AUTOSAR COM モジュールは、そのエンディアンを、宛先の（グループ）信号のエンディアンへ変換しなければならない。（SRS_PduR_06061）

#### 7.11.3 タイムアウトした信号と信号グループの経路付け

**[SWS_Com_00701]** AUTOSAR COM モジュールの信号ゲートウェイは、コンフィギュレーションされた受信デッドライン監視のタイムアウトが満了していても、信号と信号グループを経路付けしなければならない。（SRS_PduR_06089）

時間内に受信されなかった信号または信号グループの場合も、AUTOSAR COM モジュールの信号ゲートウェイは、これらの信号または信号グループを、そのまま経路付けする。

#### 7.11.4 更新ビットの扱い

**[SWS_Com_00702]** 受信した信号／信号グループと、宛先の信号／信号グループの両方に、更新ビット（`ComUpdateBitPosition`）がコンフィギュレーションされ、受信した信号／信号グループの更新ビットがセットされている場合、AUTOSAR COM モジュールは、その信号／信号グループを、更新ビットをセットして経路付けし、宛先の信号／信号グループの更新ビットを、送った後にクリアしなければならない。（SRS_Com_02030）

**[SWS_Com_00703]** 受信した信号／信号グループと、宛先の信号／信号グループの両方に、更新ビット（`ComUpdateBitPosition`）がコンフィギュレーションされ、受信した信号／信号グループの更新ビットがセットされていない場合、AUTOSAR COM モジュールは、この信号／信号グループを経路付けしてはならない。（SRS_Com_02030）

**[SWS_Com_00704]** 受信した信号／信号グループに更新ビット（`ComUpdateBitPosition`）がコンフィギュレーションされているが、宛先の信号に更新ビットがコンフィギュレーションされておらず、更新ビットがセットされている場合、AUTOSAR COM モジュールは、この信号／信号グループを、更新ビットなしで経路付けしなければならない。（SRS_Com_02030）

**[SWS_Com_00705]** 受信した信号／信号グループに更新ビット（`ComUpdateBitPosition`）がコンフィギュレーションされているが、宛先の信号に更新ビットがコンフィギュレーションされておらず、更新ビットがセットされていない場合、AUTOSAR COM モジュールは、この信号／信号グループを経路付けしてはならない。（SRS_Com_02030）

**[SWS_Com_00706]** 受信した信号／信号グループに更新ビット（`ComUpdateBitPosition`）がコンフィギュレーションされておらず、宛先の信号／信号グループに更新ビットがコンフィギュレーションされている場合、AUTOSAR COM モジュールは、新しい信号／信号グループが受信されたときに、宛先の信号の更新ビットをセットし、宛先の信号／信号グループの送信後にクリアしなければならない。（SRS_Com_02030）

#### 7.11.5 信号ゲートウェイの分離

I-PDU の受信に使われる割り込みルーチンを、予測できない、場合によっては高価な時間の使用から守るため、AUTOSAR COM モジュールの信号ゲートウェイを、割り込みルーチンから分離する必要がある。

**[SWS_Com_00359]** AUTOSAR COM モジュールは、信号ゲートウェイのすべての関数を、関数呼び出し `Com_MainFunctionRouteSignals`（[SWS_Com_00400]）の間に実行しなければならない。この関数呼び出しの間に、AUTOSAR COM モジュールの信号ゲートウェイは、受信した、経路付けされる信号と信号グループを検査し、関連する受信 I-PDU から、関連する送信 I-PDU へ転送しなければならない。（SRS_BSW_00425、SRS_BSW_00432）

**[SWS_Com_00466]** `Com_MainFunctionRouteSignals` の中で、AUTOSAR COM モジュールは、転送プロパティと送信モードを評価し、経路付けの動作を、次の順序で行わなければならない（図 3 も参照）。

1. ゲートされるすべての信号を、送信元から宛先の I-PDU へコピーする。
2. ゲートされるすべての信号の TMC を評価する。
3. 宛先の I-PDU の TMS を評価する。

（SRS_BSW_00432）

宛先の I-PDU に、新しく受信した、`ComTransferProperty` が `TRIGGERED` のゲートされる信号が含まれる場合、送信は、`Com_MainFunctionTx` または `Com_MainFunctionRouteSignals` のどちらでも行いうる。

**[SWS_Com_00539]** AUTOSAR COM モジュールは、`Com_MainFunctionRouteSignals` の 1 回の呼び出しの間に、I-PDU を最大で 1 回だけ送り出さなければならない。（SRS_PduR_06055）

### 7.12 エラーの分類

#### 7.12.1 開発エラー

**[SWS_Com_00803]** API サービスが誤ったパラメータで呼ばれた。

- エラーコード: `COM_E_PARAM`
- 値 [hex]: 0x01

（SRS_BSW_00337）

**[SWS_Com_00804]** `Com_Init` で AUTOSAR COM モジュールが初期化される前、または `Com_Deinit` の呼び出しの後に、`Com_GetStatus` 以外の他の API サービスが呼ばれた場合のエラーコード。

- エラーコード: `COM_E_UNINIT`
- 値 [hex]: 0x02

（SRS_BSW_00337）

**[SWS_Com_00805]** NULL ポインタの検査。

- エラーコード: `COM_E_PARAM_POINTER`
- 値 [hex]: 0x03

（SRS_BSW_00414）

**[SWS_Com_00837]** 無効なコンフィギュレーションセットの選択。

- エラーコード: `COM_E_INIT_FAILED`
- 値 [hex]: 0x04

（SRS_BSW_00414）

#### 7.12.2 ランタイムエラー

**[SWS_Com_00864]** 送信要求がスキップされた。

- エラーコード: `COM_E_SKIPPED_TRANSMISSION`
- 値 [hex]: 0x05

（SRS_BSW_00452）

#### 7.12.3 一時的な故障

一時的な故障は無い。

#### 7.12.4 プロダクションエラー

プロダクションエラーは無い。

#### 7.12.5 拡張プロダクションエラー

拡張プロダクションエラーは無い。

### 7.13 エラーの通知

**[SWS_Com_00442]** 開発エラーが検出されたとき、Default Error Tracer の関数 `Det_ReportError` を、次の引数で呼ばなければならない。

- AUTOSAR COM の `ModuleId` として 50
- `InstanceId` として 0
- `ApiId` として、エラーが検出された AUTOSAR COM モジュールの API のサービス ID
- `ErrorId` として、7.12.1 章で定義されたエラー ID

（SRS_BSW_00369、SRS_BSW_00385、SRS_BSW_00327、SRS_PduR_06098、SRS_PduR_06099）

### 7.14 エラー処理

AUTOSAR COM は、デッドライン監視による通信の監督をサポートする（7.3.6 章を参照）。さらに、AUTOSAR COM は、データシーケンス制御（7.9 章を参照）と通信の保護（7.10 章を参照）をサポートする。

呼び出された API のエラーと戻り値に関する、AUTOSAR COM モジュールの具体的な反応は、対応する機能要求の中で述べられている。

### 7.15 AUTOSAR COM モジュールのインタラクションモデル

この章は、[17] の「インタラクション層の機能モデル」の章に対応する。次の図は、外部からの受信と外部への送信について、インタラクション層の動作を示す。完全な機能が示されているが、どの部分が具体的な実装に存在する／使われるかは、コンフィギュレーションに依存する。

**[SWS_Com_00396]** 受信した信号または信号グループには、さまざまな処理の段階をコンフィギュレーションできる。AUTOSAR COM モジュールは、コンフィギュレーションされた処理の段階を、次の順序で実行しなければならない。

1. I-PDU 単位の監視のために、受信デッドライン監視タイマをリセットする。
2. 更新ビットを検査する。
3. エンディアン変換と符号拡張
4. データの無効化
5. 受信フィルタリング
6. 信号単位の監視のために、受信デッドライン監視タイマをリセットする。
7. 通知

（SRS_Com_02037）

**[SWS_Com_00744]** `ComSignal` または `ComSignalGroup` に、`ComRxDataTimeoutAction` と `ComTimeoutNotification` の両方がコンフィギュレーションされている場合、AUTOSAR COM モジュールは、まず、コンフィギュレーションされた `ComRxDataTimeoutAction` を実行し、その後、コンフィギュレーションされた `ComTimeoutNotification` を呼ばなければならない。（SRS_Com_02089）

> （図 4「AUTOSAR COM モジュールの送信のインタラクションモデル」、図 5「受信のインタラクションモデル」、図 6「統合された信号ゲートウェイのインタラクションモデル」: 処理の流れの図。訳なし。原典を参照。
> 　読み取れる内容: 送信は、RTE の `Com_SendSignal` / `Com_SendSignalGroup` → 更新ビットのセット → エンディアン変換と符号拡張 → 信号の転送プロパティ → I-PDU の送信モードの選択 → I-PDU のコールアウト → `PduR_ComTransmit` の順で、`Com_TxConfirmation` で送信デッドライン監視が止まり、`Com_CbkTxAck` / `Com_CbkTxTOut` で RTE へ通知される。受信は、`Com_RxIndication` → I-PDU 単位の受信デッドライン監視タイマのリセット → I-PDU のコールアウト → 更新ビットの検査 → エンディアン変換と符号拡張 → データの無効化 → 受信フィルタリング → 信号単位の受信デッドライン監視タイマのリセット → 通知の検出、の順（[SWS_Com_00396] と同じ）。
> 　図 6 の注: 受信側のエンディアン変換と符号拡張は、TMS に正しいデータ形式を渡すために必要で、Rx バスのエンディアンが CPU のエンディアンと異なる場合にだけ必要である。送信側のエンディアン変換は、Rx バスのエンディアンが Tx バスのエンディアンと異なる場合にだけ必要である。具体的なコンフィギュレーションでは、最適化されたゲートウェイの信号の流れが、処理の段階を 1 つ以上省略してもよい。）

## 8 API 仕様

### 8.1 インポートする型

この章では、次のファイルから取り込まれるすべての型を示す。

**[SWS_Com_00609]**

| モジュール | インポートする型 |
|---|---|
| ComStack_Types | `BufReq_ReturnType`、`PduIdType`、`PduInfoType`、`PduLengthType`、`RetryInfoType` |
| Std_Types | `Std_ReturnType`、`Std_VersionInfoType` |

（SRS_BSW_00301）

### 8.2 型の定義

#### 8.2.1 Com_StatusType

**[SWS_Com_00819]**

| 項目 | 内容 |
|---|---|
| 名前 | `Com_StatusType` |
| 型 | 列挙型 |
| 範囲 | `COM_INIT`: AUTOSAR COM モジュールは初期化されており、使える。<br>`COM_UNINIT`（0x00）: AUTOSAR COM モジュールは初期化されていないか、使えない。これは、リセット後のデフォルト値でなければならない。この状態は、値 0 を持たなければならない。 |
| 説明 | API サービス `Com_GetStatus()` が返すステータス値。 |

（SRS_BSW_00335）

#### 8.2.2 Com_SignalIdType

**[SWS_Com_00820]**

| 項目 | 内容 |
|---|---|
| 名前 | `Com_SignalIdType` |
| 型 | `uint16` |
| 範囲 | 0..`<SignalIdmax>`（0 から始まる整数） |
| 説明 | AUTOSAR COM モジュールの信号オブジェクトの識別子。 |

（SRS_Com_02037、SRS_BSW_00441）

#### 8.2.3 Com_SignalGroupIdType

**[SWS_Com_00821]**

| 項目 | 内容 |
|---|---|
| 名前 | `Com_SignalGroupIdType` |
| 型 | `uint16` |
| 範囲 | 0..`<SignalGroupIdmax>`（0 から始まる整数） |
| 説明 | AUTOSAR COM モジュールの信号グループオブジェクトの識別子。 |

（SRS_Com_02041、SRS_BSW_00441）

#### 8.2.4 Com_IpduGroupIdType

**[SWS_Com_00822]**

| 項目 | 内容 |
|---|---|
| 名前 | `Com_IpduGroupIdType` |
| 型 | `uint16` |
| 範囲 | 0..`<IpduGroupIdmax>`（0 から始まる整数。`IpduGroupIdmax` < `ComSupportedIPduGroups`） |
| 説明 | AUTOSAR COM の I-PDU グループオブジェクトの識別子。 |

（SRS_Com_00218、SRS_BSW_00441）

#### 8.2.5 Com_ConfigType

**[SWS_Com_00825]**

| 項目 | 内容 |
|---|---|
| 名前 | `Com_ConfigType` |
| 型 | 構造体 |
| 範囲 | 実装固有（初期化データ構造の内容は、実装固有である） |
| 説明 | COM の初期化データを含むデータ構造の型。 |

（SRS_BSW_00404、SRS_BSW_00441）

### 8.3 関数の定義

**[SWS_Com_00321]** 再入不可の関数は、再入して呼ばれているかを検査する必要は無い。（SRS_BSW_00312）

COM の関数の多くは、具体的なオブジェクトに関して再入不可である。例えば、`Com_SendSignal` は、同じ信号については再入不可だが、異なる信号については再入可能である。したがって、AUTOSAR COM モジュールの呼び出し元が複数ある場合は、例えば、互いに素な信号と I-PDU グループの集合を使うなどして、システムレベルで同期させる必要がある。

#### 8.3.1 戻りコード

AUTOSAR COM モジュールは、専用の COM の戻り値の型を定義しない。API サービスは、[5] で定義された `Std_ReturnType` を使うか、表 9 に従って割り当てられた `uint8` の値で、エラーを返す。

AUTOSAR COM モジュールの戻りコードは、表 9 で定義される。

**[SWS_Com_00865]**

> **注（再構成）**: 原典の表 9 は、テキストの抽出で、値と説明の対応が崩れていた。値の並び（0x00、0x01、0x80、0x81）から対応を復元した。

| 名前 | 説明 | 型 | 値 | 定義場所 |
|---|---|---|---|---|
| `E_OK` | サービスが受理された | `#define` | 0x00 | `Std_Types.h` |
| `E_NOT_OK` | サービスの呼び出しが失敗した | `#define` | 0x01 | `Std_Types.h` |
| `COM_SERVICE_NOT_AVAILABLE` | サービスが現在利用できない。例: 対応する I-PDU グループが停止している（または開発エラーが検出された） | `#define` | 0x80 | `Com.h` |
| `COM_BUSY` | この関数の正常な実行が、リソースが使用中のため、現在できない。例: 大きな I-PDU のバッファがロックされている。大きな I-PDU の信号にアクセスされるたびに、`COM_BUSY` が返されてもよいが、返さなければならないわけではない。 | `#define` | 0x81 | `Com.h` |

（SRS_BSW_00348、SRS_BSW_00377、SRS_BSW_00441）

#### 8.3.2 起動と制御のサービス

##### 8.3.2.1 Com_Init

**[SWS_Com_00432]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_Init` |
| 構文 | `void Com_Init(const Com_ConfigType* config)` |
| サービス ID [hex] | 0x01 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `config`: AUTOSAR COM モジュールのコンフィギュレーションデータへのポインタ。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、以降の処理のために、AUTOSAR COM モジュール層の、内部と外部のインタフェースと変数を初期化する。この関数を呼んだ後も、ECU 間の通信は、まだ無効のままである。 |

（SRS_BSW_00344、SRS_BSW_00404、SRS_BSW_00405、SRS_BSW_00101、SRS_BSW_00358、SRS_BSW_00414）

AUTOSAR COM モジュールの動作は、`Com_Init` が正しく呼ばれるまでは、規定されない。

`Com_Init` の注意点: `Com_Init` が、AUTOSAR COM モジュールの他の関数をプリエンプトすることは、想定されていない。AUTOSAR COM モジュールは、システムの他の部分が、`Com_Init` をそのように呼ばないことを、前提にできる。

##### 8.3.2.2 Com_DeInit

**[SWS_Com_00130]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_DeInit` |
| 構文 | `void Com_DeInit(void)` |
| サービス ID [hex] | 0x02 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | このサービスは、ECU 間の通信を止める。開始されたすべての I-PDU グループは停止され、必要であれば、`Com_Init` が呼ばれた後に、再び開始しなければならない。`Com_DeInit` の呼び出しにより、AUTOSAR COM モジュールは、初期化されていない状態になる。 |

（SRS_BSW_00336）

**[SWS_Com_00129]** AUTOSAR COM モジュールの関数 `Com_DeInit` は、開始されたすべての I-PDU グループを停止しなければならない。（SRS_BSW_00336）

`Com_DeInit` の注意点: `Com_DeInit` が、AUTOSAR COM モジュールの関数をプリエンプトすることは、想定されていない。AUTOSAR COM モジュールは、システムの他の部分が、`Com_DeInit` をそのように呼ばないことを、前提にできる。

##### 8.3.2.3 Com_IpduGroupStart

**[SWS_Com_91001]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_IpduGroupStart` |
| 構文 | `void Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean initialize)` |
| サービス ID [hex] | 0x03 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる I-PDU グループでは再入可能。同じ I-PDU グループでは再入不可。 |
| 入力パラメータ | `IpduGroupId`: 開始する I-PDU グループの Id<br>`initialize`: この I-PDU グループの I-PDU のデータの初期化を要求するフラグ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | あらかじめコンフィギュレーションされた I-PDU グループを開始する。例えば、周期的な I-PDU は、`Com_IpduGroupStart()` の呼び出しの後、周期的に送り出される。`Initialize` が真の場合、I-PDU グループのすべての I-PDU は、I-PDU グループが開始される前に、（再）初期化されなければならない。すなわち、COM の起動後のように動作しなければならない。例えば、フィルタオブジェクトの old_value や、信号グループのシャドウバッファが、（再）初期化される必要がある。 |

（SRS_Com_00218）

##### 8.3.2.4 Com_IpduGroupStop

**[SWS_Com_91002]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_IpduGroupStop` |
| 構文 | `void Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId)` |
| サービス ID [hex] | 0x04 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる I-PDU グループでは再入可能。同じ I-PDU グループでは再入不可。 |
| 入力パラメータ | `IpduGroupId`: 停止する I-PDU グループの Id |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | あらかじめコンフィギュレーションされた I-PDU グループを停止する。例えば、周期的な I-PDU は、`Com_IpduGroupStop()` の呼び出しの後、停止される。 |

（SRS_Com_00218）

##### 8.3.2.5 Com_EnableReceptionDM

**[SWS_Com_91004]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_EnableReceptionDM` |
| 構文 | `void Com_EnableReceptionDM(Com_IpduGroupIdType IpduGroupId)` |
| サービス ID [hex] | 0x06 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる I-PDU グループでは再入可能。同じ I-PDU グループでは再入不可。 |
| 入力パラメータ | `IpduGroupId`: 受信 DM を有効にする I-PDU グループの Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 指定した I-PDU グループの中の I-PDU について、受信デッドライン監視を有効にする。 |

（SRS_Com_00192）

##### 8.3.2.6 Com_DisableReceptionDM

**[SWS_Com_91003]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_DisableReceptionDM` |
| 構文 | `void Com_DisableReceptionDM(Com_IpduGroupIdType IpduGroupId)` |
| サービス ID [hex] | 0x05 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる I-PDU グループでは再入可能。同じ I-PDU グループでは再入不可。 |
| 入力パラメータ | `IpduGroupId`: 受信 DM を無効にする I-PDU グループの Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 指定した I-PDU グループの中の I-PDU について、受信デッドライン監視を無効にする。 |

（SRS_Com_00192）

##### 8.3.2.7 Com_GetStatus

**[SWS_Com_00194]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_GetStatus` |
| 構文 | `Com_StatusType Com_GetStatus(void)` |
| サービス ID [hex] | 0x07 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Com_StatusType`<br>`COM_UNINIT`: AUTOSAR COM モジュールは初期化されておらず、使えない。<br>`COM_INIT`: AUTOSAR COM モジュールは初期化されており、使える。 |
| 説明 | AUTOSAR COM モジュールのステータスを返す。 |

（SRS_BSW_00331）

##### 8.3.2.8 Com_GetVersionInfo

**[SWS_Com_00426]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_GetVersionInfo` |
| 構文 | `void Com_GetVersionInfo(Std_VersionInfoType* versioninfo)` |
| サービス ID [hex] | 0x09 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | なし |
| 入出力パラメータ | なし |
| 出力パラメータ | `versioninfo`: このモジュールのバージョン情報を格納する場所へのポインタ。 |
| 戻り値 | なし |
| 説明 | このモジュールのバージョン情報を返す。 |

（SRS_BSW_00407、SRS_BSW_00003）

`Com_GetVersionInfo` のコンフィギュレーションは、[SWS_Com_00026] を参照。

#### 8.3.3 通信サービス

##### 8.3.3.1 Com_SendSignal

**[SWS_Com_00197]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_SendSignal` |
| 構文 | `uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)` |
| サービス ID [hex] | 0x0a |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ信号では再入不可。異なる信号では再入可能。 |
| 入力パラメータ | `SignalId`: 送る信号の Id。<br>`SignalDataPtr`: 送信する信号データへの参照。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: 大きなデータ型の扱いで、TP バッファがロックされている場合。 |
| 説明 | サービス `Com_SendSignal` は、`SignalId` で識別される信号オブジェクトを、パラメータ `SignalDataPtr` が参照する信号で更新する。 |

（SRS_Com_02037）

**[SWS_Com_00624]** サービス `Com_SendSignal` は、`SignalId` で識別される信号オブジェクトを、パラメータ `SignalDataPtr` が参照する信号で更新しなければならない。（SRS_Com_02037）

**[SWS_Com_00625]** 更新された信号の `ComTransferProperty` が `TRIGGERED` で、`ComTxModeMode` が `DIRECT` または `MIXED` の I-PDU に割り当てられている場合、`Com_SendSignal` は、送信が他の COM の仕組みで遅らされたり妨げられたりしない限り、その I-PDU の即時の送信（遅くとも次のメイン関数の中）を行わなければならない。（SRS_Com_02037）

関連する仕組みは、例えば、最小遅延タイマ（`ComMinimumDelayTime`）である。

##### 8.3.3.2 Com_SendDynSignal

**[SWS_Com_00627]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_SendDynSignal` |
| 構文 | `uint8 Com_SendDynSignal(Com_SignalIdType SignalId, const void* SignalDataPtr, uint16 Length)` |
| サービス ID [hex] | 0x21 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ信号では再入不可。異なる信号では再入可能。 |
| 入力パラメータ | `SignalId`: 送る信号の Id。<br>`SignalDataPtr`: 送信する信号データへの参照。<br>`Length`: 動的な長さの信号の長さ。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`E_NOT_OK`: `Length` が、この送る信号のコンフィギュレーションされた `ComSignalLength` より大きい場合。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: TP バッファがロックされている場合。 |
| 説明 | サービス `Com_SendDynSignal` は、`SignalId` で識別される信号オブジェクトを、パラメータ `SignalDataPtr` が参照する信号で更新する。 |

（SRS_Com_02095）

**[SWS_Com_00628]** サービス `Com_SendDynSignal` は、`SignalId` で識別される信号オブジェクトを、パラメータ `SignalDataPtr` が参照する信号で更新しなければならない。（SRS_Com_02095）

**[SWS_Com_00629]** 更新された信号（[SWS_Com_00628] を参照）の `ComTransferProperty` が `TRIGGERED` の場合、関数 `Com_SendDynSignal` は、信号が送信モード `Periodic` の I-PDU に詰められている場合を除き、その信号に関連付けられた I-PDU の即時の送信（遅くとも次のメイン関数の中）を行わなければならない。（SRS_Com_02095）

**[SWS_Com_00630]** 更新された信号（[SWS_Com_00628] を参照）の `ComTransferProperty` が `PENDING` の場合、関数 `Com_SendDynSignal` は、その信号に関連付けられた I-PDU の送信を行ってはならない。（SRS_Com_02095）

動的な長さの信号の最小の長さに、制限は無い。したがって、動的な長さの信号が長さゼロであることは許される。

##### 8.3.3.3 Com_ReceiveSignal

**[SWS_Com_00198]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_ReceiveSignal` |
| 構文 | `uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)` |
| サービス ID [hex] | 0x0b |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ信号では再入不可。異なる信号では再入可能。 |
| 入力パラメータ | `SignalId`: 受信する信号の Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `SignalDataPtr`: 受信した信号データを格納する場所への参照。 |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: 大きなデータ型の扱いで、TP バッファがロックされている場合。 |
| 説明 | `Com_ReceiveSignal` は、`SignalId` で識別される信号のデータを、`SignalDataPtr` で指定された場所へコピーする。 |

（SRS_Com_02037）

**[SWS_Com_00631]** サービス `Com_ReceiveSignal` は、`SignalId` で識別される信号オブジェクトのデータを、`SignalDataPtr` が参照する位置へコピーしなければならない。（SRS_Com_02037）

`Com_ReceiveSignal` の注意点: AUTOSAR COM モジュールは、`SignalDataPtr` が、指定された `SignalId` の信号のデータ型に関して、正しく整列していることを前提にする。`Com_ReceiveSignal` の呼び出し元が、これを保証する必要がある。

##### 8.3.3.4 Com_ReceiveDynSignal

**[SWS_Com_00690]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_ReceiveDynSignal` |
| 構文 | `uint8 Com_ReceiveDynSignal(Com_SignalIdType SignalId, void* SignalDataPtr, uint16* Length)` |
| サービス ID [hex] | 0x22 |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ信号では再入不可。異なる信号では再入可能。 |
| 入力パラメータ | `SignalId`: 受信する信号の Id。 |
| 入出力パラメータ | `Length`: 入力時: 受信できる最大長。出力時: 動的な長さの信号の長さ。 |
| 出力パラメータ | `SignalDataPtr`: 受信した信号データを格納する場所への参照。 |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`E_NOT_OK`: `Length`（入力パラメータとして）が、動的な長さの信号の受信した長さより小さい場合。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: TP バッファがロックされている場合。 |
| 説明 | `Com_ReceiveDynSignal` は、`SignalId` で識別される信号のデータを、`SignalDataPtr` で指定された場所へコピーし、動的な長さの信号の長さを、パラメータ `Length` が指す位置に格納する。 |

（SRS_Com_02092、SRS_Com_02095）

**[SWS_Com_00711]** サービス `Com_ReceiveDynSignal` は、`SignalId` で識別される信号オブジェクトのデータを、`SignalDataPtr` が参照する位置へコピーしなければならない。（SRS_Com_02092）

**[SWS_Com_00712]** サービス `Com_ReceiveDynSignal` は、`SignalId` で識別される動的な長さの信号の、計算された長さ（[SWS_Com_00758] を参照）を、パラメータ `Length` で返さなければならない。（SRS_Com_02092）

**[SWS_Com_00724]** `Length`（入力パラメータとして）が、動的な長さの信号の受信した長さより小さい場合、`Com_ReceiveDynSignal` は、`SignalDataPtr` が参照する位置へ、データをコピーしてはならない。（SRS_Com_02092）

##### 8.3.3.5 Com_SendSignalGroup

**[SWS_Com_00200]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_SendSignalGroup` |
| 構文 | `uint8 Com_SendSignalGroup(Com_SignalGroupIdType SignalGroupId)` |
| サービス ID [hex] | 0x0d |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じグループ信号では再入不可。異なるグループ信号では再入可能。 |
| 入力パラメータ | `SignalGroupId`: 送る信号グループの Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: 大きなデータ型の扱いで、TP バッファがロックされている場合。 |
| 説明 | サービス `Com_SendSignalGroup` は、関連するシャドウバッファの内容を、関連する I-PDU へコピーする。 |

（SRS_Com_02041）

**[SWS_Com_00635]** サービス `Com_SendSignalGroup` は、パラメータ `SignalGroupId` が参照するシャドウバッファの内容を、関連する I-PDU へコピーしなければならない。（SRS_Com_02041）

**[SWS_Com_00637]** `Com_SendSignalGroup` のコンフィギュレーション: この呼び出しの前に、信号グループがコンフィギュレーションされていなければならない。詳細は ECUC_Com_00345 を参照。（SRS_Com_02041）

##### 8.3.3.6 Com_ReceiveSignalGroup

**[SWS_Com_00201]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_ReceiveSignalGroup` |
| 構文 | `uint8 Com_ReceiveSignalGroup(Com_SignalGroupIdType SignalGroupId)` |
| サービス ID [hex] | 0x0e |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じグループ信号では再入不可。異なるグループ信号では再入可能。 |
| 入力パラメータ | `SignalGroupId`: 受信する信号グループの Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: 大きなデータ型の扱いで、TP バッファがロックされている場合。 |
| 説明 | サービス `Com_ReceiveSignalGroup` は、受信した信号グループを、I-PDU からシャドウバッファへコピーする。 |

（SRS_Com_02041）

**[SWS_Com_00638]** サービス `Com_ReceiveSignalGroup` は、受信した信号グループを、I-PDU からシャドウバッファへコピーしなければならない。（SRS_Com_02041）

この呼び出しの後、グループ信号は、`Com_ReceiveSignal` を呼んで、シャドウバッファから RTE へコピーできる。

**[SWS_Com_00639]** `Com_ReceiveSignalGroup` のコンフィギュレーション: この呼び出しの前に、信号グループがコンフィギュレーションされていなければならない。詳細は ECUC_Com_00345 を参照。（SRS_Com_02041）

**[SWS_Com_00461]** AUTOSAR COM モジュールは、`Com_ReceiveSignalGroup` の呼び出しで、I-PDU が停止していて `COM_SERVICE_NOT_AVAILABLE` が返される場合でも、I-PDU の最後に分かっているデータ、まだ書き込まれていない場合は `ComSignalInitValue` を、常にシャドウバッファへコピーしなければならない。（SRS_Com_02041）

##### 8.3.3.7 Com_SendSignalGroupArray

**[SWS_Com_00851]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_SendSignalGroupArray` |
| 構文 | `uint8 Com_SendSignalGroupArray(Com_SignalGroupIdType SignalGroupId, const uint8* SignalGroupArrayPtr)` |
| サービス ID [hex] | 0x23 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ信号グループでは再入不可。異なる信号グループでは再入可能。 |
| 入力パラメータ | `SignalGroupId`: 送る信号グループの Id。<br>`SignalGroupArrayPtr`: 信号グループの配列への参照。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: 大きなデータ型の扱いで、TP バッファがロックされている場合。 |
| 説明 | サービス `Com_SendSignalGroupArray` は、渡された `SignalGroupArrayPtr` の内容を、関連する I-PDU へコピーする。渡されるデータは、信号グループの配列による表現に対応していなければならない。 |

（SRS_Com_02112、SRS_Com_02041）

**[SWS_Com_00852]** サービス `Com_SendSignalGroupArray` は、`SignalGroupArrayPtr` が指す内容を、関連する I-PDU へコピーしなければならない。（SRS_Com_02112、SRS_Com_02041）

**[SWS_Com_00853]** API `Com_SendSignalGroupArray` は、コンフィギュレーションスイッチ `ComEnableSignalGroupArrayApi` が有効な場合にだけ、利用できなければならない。（SRS_Com_00177）

##### 8.3.3.8 Com_ReceiveSignalGroupArray

**[SWS_Com_00854]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_ReceiveSignalGroupArray` |
| 構文 | `uint8 Com_ReceiveSignalGroupArray(Com_SignalGroupIdType SignalGroupId, uint8* SignalGroupArrayPtr)` |
| サービス ID [hex] | 0x24 |
| 同期／非同期 | 同期 |
| 再入可能性 | 同じ信号グループでは再入不可。異なる信号グループでは再入可能。 |
| 入力パラメータ | `SignalGroupId`: 受信する信号グループの Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `SignalGroupArrayPtr`: 受信した信号グループの配列を格納する場所への参照。 |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止していた（または、開発エラーのためサービスが失敗した）。<br>`COM_BUSY`: 大きなデータ型の扱いで、TP バッファがロックされている場合。 |
| 説明 | サービス `Com_ReceiveSignalGroupArray` は、受信した信号グループの配列による表現を、PDU から `SignalGroupArrayPtr` が指す場所へコピーする。 |

（SRS_Com_02112、SRS_Com_02041）

**[SWS_Com_00855]** サービス `Com_ReceiveSignalGroupArray` は、受信した信号グループの配列による表現を、I-PDU から `SignalGroupArrayPtr` が指す場所へコピーしなければならない。（SRS_Com_02112、SRS_Com_02041）

**[SWS_Com_00856]** API `Com_ReceiveSignalGroupArray` は、コンフィギュレーションスイッチ `ComEnableSignalGroupArrayApi` が有効な場合にだけ、利用できなければならない。（SRS_Com_00177）

**[SWS_Com_00857]** AUTOSAR COM モジュールは、`Com_ReceiveSignalGroupArray` の呼び出しで、I-PDU が停止していて `COM_SERVICE_NOT_AVAILABLE` が返される場合でも、I-PDU の最後に分かっているデータ、まだ書き込まれていない場合は `ComSignalInitValue` を、常に `SignalGroupArrayPtr` へコピーしなければならない。（SRS_Com_02041）

##### 8.3.3.9 Com_InvalidateSignal

**[SWS_Com_00203]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_InvalidateSignal` |
| 構文 | `uint8 Com_InvalidateSignal(Com_SignalIdType SignalId)` |
| サービス ID [hex] | 0x10 |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ信号では再入不可。異なる信号では再入可能。 |
| 入力パラメータ | `SignalId`: 無効化する信号の Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止している、指定した `signalId` に `ComSignalDataInvalidValue` がコンフィギュレーションされていない、または開発エラーのためサービスが失敗した。<br>`COM_BUSY`: TP バッファがロックされている場合。 |
| 説明 | サービス `Com_InvalidateSignal` は、指定した `SignalId` の信号を、コンフィギュレーションされた `ComSignalDataInvalidValue` に値を設定することで、無効化する。 |

（SRS_Com_02077）

信号の無効化の仕組みの詳細は、7.4.4 節を参照。

**[SWS_Com_00642]** 信号データを無効化した後、関数 `Com_InvalidateSignal` は、内部で `Com_SendSignal` の呼び出しを行わなければならない。詳細は [SWS_Com_00099] を参照。（SRS_Com_02077）

**[SWS_Com_00643]** `Com_InvalidateSignal` は、指定した `SignalId` の信号に `ComSignalDataInvalidValue`（ECUC_Com_00391）がコンフィギュレーションされていない場合、`COM_SERVICE_NOT_AVAILABLE` を返さなければならない。（SRS_Com_02077）

##### 8.3.3.10 Com_InvalidateSignalGroup

**[SWS_Com_00557]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_InvalidateSignalGroup` |
| 構文 | `uint8 Com_InvalidateSignalGroup(Com_SignalGroupIdType SignalGroupId)` |
| サービス ID [hex] | 0x1b |
| 同期／非同期 | 非同期 |
| 再入可能性 | 同じ信号グループでは再入不可。異なる信号グループでは再入可能。 |
| 入力パラメータ | `SignalGroupId`: 無効化する信号グループの Id。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `uint8`<br>`E_OK`: サービスが受理された。<br>`COM_SERVICE_NOT_AVAILABLE`: 対応する I-PDU グループが停止している、どのグループ信号にも `ComSignalDataInvalidValue` がコンフィギュレーションされていない、または開発エラーのためサービスが失敗した。<br>`COM_BUSY`: TP バッファがロックされている場合。 |
| 説明 | サービス `Com_InvalidateSignalGroup` は、指定した `SignalGroupId` の信号グループのすべてのグループ信号を、コンフィギュレーションされた `ComSignalDataInvalidValue` に値を設定することで、無効化する。 |

（SRS_Com_02077）

信号の無効化の仕組みの詳細は、7.4.4 節を参照。

**[SWS_Com_00645]** 信号グループのデータを無効化した後、関数 `Com_InvalidateSignalGroup` は、内部で `Com_SendSignalGroup` の呼び出しを行わなければならない。（SRS_Com_02077）

##### 8.3.3.11 Com_TriggerIPDUSend

**[SWS_Com_00348]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_TriggerIPDUSend` |
| 構文 | `Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)` |
| サービス ID [hex] | 0x17 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `PduId`: 送信のために起動する I-PDU の I-PDU-ID |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: I-PDU が送信のために起動された。<br>`E_NOT_OK`: I-PDU が停止していて、送信を起動できなかった。 |
| 説明 | `Com_TriggerIPDUSend` の呼び出しにより、指定した ID の I-PDU が、送信のために起動される。 |

（SRS_Com_02037）

**[SWS_Com_00861]** `Com_TriggerIPDUSend` の呼び出しにより、AUTOSAR COM は、I-PDU が開始されている場合に限り、指定した ID の I-PDU を送信のために起動しなければならない。（SRS_Com_02037）

停止している I-PDU が送信のために起動された場合、`E_NOT_OK` が返されるだけである。停止している I-PDU では、(再)開始された後の実行のために、トリガは保存されない。

**[SWS_Com_00388]** I-PDU が、関数 `Com_TriggerIPDUSend` または `Com_TriggerIPDUSendWithMetaData` のために送信されるとき、AUTOSAR COM モジュールは、最小遅延時間だけを考慮しなければならない。すなわち、AUTOSAR COM モジュールは、必要であれば送信を延期し、送信の場合には最小遅延タイマをリセットしなければならない。AUTOSAR COM モジュールは、`ComTxModeNumberOfRepetitions` などの、他の送信モードに関するパラメータを、考慮してはならない。（SRS_Com_02037）

**[SWS_Com_00492]** `Com_TriggerIPDUSend` または `Com_TriggerIPDUSendWithMetaData` により起動された I-PDU に、I-PDU コールアウト（ECUC_Com_00387 を参照）がコンフィギュレーションされている場合、関数 `Com_TriggerIPDUSend` または `Com_TriggerIPDUSendWithMetaData` は、それぞれ、この I-PDU コールアウトも呼ばなければならない。（SRS_Com_02037）

関数 `Com_TriggerIPDUSend` の中で、別の `Com_TriggerIPDUSend` の呼び出しが行われる場合、アプリケーションのプログラマは、無限の再帰を作らないように注意しなければならない。同じ `ComPduId` で `Com_TriggerIPDUSend` を再び呼ぶ直接の再帰も、より長い再帰の経路を持つ間接の再帰も、理論上は起こりうる。前の段落は、`Com_TriggerIPDUSendWithMetaData`、およびこの 2 つの I-PDU 起動関数の組み合わせにも当てはまる。

**[SWS_Com_00867]** 大きな I-PDU が `Com_TriggerIPDUSend` で送信のために起動され、大きな I-PDU のバッファが現在ロックされている場合、AUTOSAR COM は、送信要求を保存し、バッファが再び利用できるようになったときに I-PDU を送信しなければならない。（SRS_Com_02037）

##### 8.3.3.12 Com_TriggerIPDUSendWithMetaData

**[SWS_Com_00858]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_TriggerIPDUSendWithMetaData` |
| 構文 | `Std_ReturnType Com_TriggerIPDUSendWithMetaData(PduIdType PduId, const uint8* MetaData)` |
| サービス ID [hex] | 0x28 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入不可 |
| 入力パラメータ | `PduId`: 送信のために起動する I-PDU の I-PDU-ID<br>`MetaData`: 起動された送信要求のメタデータへのポインタ |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: I-PDU が送信のために起動された。<br>`E_NOT_OK`: I-PDU が停止していて、送信を起動できなかった。 |
| 説明 | `Com_TriggerIPDUSendWithMetaData` の呼び出しにより、AUTOSAR COM モジュールは、指定した ID の I-PDU の内部のメタデータを、指定された位置から、この I-PDU のグローバルにコンフィギュレーションされた `MetaDataType` の長さに従ってコピーすることで更新する。その後、I-PDU が送信のために起動される。 |

（SRS_Com_02037）

**[SWS_Com_00862]** `Com_TriggerIPDUSendWithMetaData` の呼び出しにより、AUTOSAR COM は、I-PDU が開始されている場合に限り、指定した ID の I-PDU を送信のために起動しなければならない。（SRS_Com_02037）

停止している I-PDU が送信のために起動された場合、`E_NOT_OK` が返されるだけである。COM には、トリガを保存し、I-PDU が(再)開始された後に実行する仕組みは無い。

**[SWS_Com_00868]** 大きな I-PDU が `Com_TriggerIPDUSendWithMetaData` で送信のために起動され、大きな I-PDU のバッファが現在ロックされている場合、AUTOSAR COM は、指定された `MetaData` とともに送信要求を保存し、バッファが再び利用できるようになったときに I-PDU を送信しなければならない。（SRS_Com_02037）

##### 8.3.3.13 Com_SwitchIpduTxMode

**[SWS_Com_00784]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_SwitchIpduTxMode` |
| 構文 | `void Com_SwitchIpduTxMode(PduIdType PduId, boolean Mode)` |
| サービス ID [hex] | 0x27 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `PduId`: 送信モードを変更する I-PDU の Id。<br>`Mode`: 設定する送信モード。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | サービス `Com_SwitchIpduTxMode` は、`PduId` が参照する I-PDU の送信モードを `Mode` に設定する。送信モードが変わる場合、新しいモードは直ちに有効になる（[SWS_Com_00239] を参照）。要求された送信モードが、この I-PDU ですでに有効だった場合、呼び出しは効果を持たない。 |

（SRS_Com_02082）

**[SWS_Com_00881]** I-PDU の送信モードが `Com_SwitchIpduTxMode` で明示的に設定されたとき、AUTOSAR COM は、この I-PDU の周期的な送信を、`ComTxModeTimeOffset` だけ遅らせなければならない。（SRS_Com_02082）

信号ベースの TMS と、`Com_SwitchIpduTxMode` による明示的な TMS を、同じ I-PDU で混ぜて使うと、この I-PDU に対する `Com_SendSignal` または `Com_SendSignalGroup` の呼び出しの間に、信号ベースの TMS が、手動で設定したモードを元に戻しうることに注意すること。したがって、1 つの I-PDU に明示的な TMS と手動の TMS を混ぜることは、使う場合には、慎重に設計しなければならない。

### 8.4 コールバック関数と通知

#### 8.4.1 Com_TriggerTransmit

**[SWS_Com_00001]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_TriggerTransmit` |
| 構文 | `Std_ReturnType Com_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x41 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信が要求される SDU の ID。 |
| 入出力パラメータ | `PduInfoPtr`: SDU のデータのコピー先のバッファへのポインタ（`SduDataPtr`）と、`SduLength` に利用できるバッファサイズを含む。戻るとき、サービスは、コピーした SDU のデータの長さを `SduLength` に示す。 |
| 出力パラメータ | なし |
| 戻り値 | `Std_ReturnType`<br>`E_OK`: SDU がコピーされ、`SduLength` がコピーされたバイト数を示す。<br>`E_NOT_OK`: SDU のデータはコピーされていない。`PduInfoPtr` は、NULL ポインタや無効なデータを指している可能性があるため、使ってはならない。 |
| 説明 | この API の中で、上位層のモジュール（呼び出されるモジュール）は、利用できるデータが、`PduInfoPtr->SduLength` で報告されたバッファサイズに収まるかを検査しなければならない。収まる場合は、データを `PduInfoPtr->SduDataPtr` が与えるバッファへコピーし、実際にコピーしたデータの長さを `PduInfoPtr->SduLength` に更新しなければならない。収まらない場合は、`PduInfoPtr` を変更せずに `E_NOT_OK` を返す。 |

（SRS_Com_02045）

**[SWS_Com_00647]** 関数 `Com_TriggerTransmit` の中で、AUTOSAR COM モジュールは、I-PDU の送信バッファの内容を、`PduInfoPtr->SduDataPtr` が与える L-PDU バッファへコピーし、`PduInfoPtr->SduLength` を、コピーしたデータの長さに合わせて更新しなければならない。（SRS_Com_02045）

**[SWS_Com_00869]** 与えられた `PduInfoPtr->SduLength` が実際の PDU 長より小さい場合、`Com_TriggerTransmit` は、データをコピーせず、`E_NOT_OK` を返さなければならない。（SRS_Com_02045）

**[SWS_Com_00475]** `Com_TriggerTransmit` は、I-PDU の最小遅延時間の影響を受けず、最小遅延タイマをリセットしてはならない。ECUC_Com_00181 を参照。（SRS_Com_02045）

関数 `Com_TriggerTransmit` の動作は、コンフィギュレーションされた送信モードから独立している。

使用例: この関数は、例えば LIN マスタが LIN フレームを送り出すために使われる。この場合、トリガによる送信は、マスタのスケジュール表自体、または受信した LIN ヘッダにより開始できる。（FlexRay の静的な部分（FlexRay のグローバル時間に同期）で PDU の送信を要求する FlexRay インタフェースも、この関数を使う。）

PDU Router が I-PDU を正常に送信すると、PDU Router は `Com_TxConfirmation` を呼ぶ。

`Com_TriggerTransmit` の注意点: この関数は、割り込みのコンテキストで呼ばれることがある。

#### 8.4.2 Com_RxIndication

**[SWS_Com_00123]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_RxIndication` |
| 構文 | `void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x42 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `RxPduId`: 受信した PDU の ID。<br>`PduInfoPtr`: 受信した PDU の長さ（`SduLength`）、PDU を含むバッファへのポインタ（`SduDataPtr`）、およびこの PDU に関するメタデータを含む。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールからの、受信した PDU の通知。 |

（SRS_Com_02043）

`Com_RxIndication` の注意点: この関数は、割り込みのコンテキストで呼ばれることがある。

#### 8.4.3 Com_TpRxIndication

**[SWS_Com_00650]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_TpRxIndication` |
| 構文 | `void Com_TpRxIndication(PduIdType id, Std_ReturnType result)` |
| サービス ID [hex] | 0x45 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 受信した I-PDU の識別。<br>`result`: 受信の結果。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | TP API を介して I-PDU が受信された後に呼ばれる。結果が、送信が成功したかを示す。 |

（SRS_Com_02095）

`Com_TpRxIndication` の注意点: この関数は、割り込みのコンテキストで呼ばれることがある。

#### 8.4.4 Com_TxConfirmation

**[SWS_Com_00124]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_TxConfirmation` |
| 構文 | `void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)` |
| サービス ID [hex] | 0x40 |
| 同期／非同期 | 同期 |
| 再入可能性 | 異なる PduId では再入可能。同じ PduId では再入不可。 |
| 入力パラメータ | `TxPduId`: 送信された PDU の ID。<br>`result`: `E_OK`: PDU が送信された。`E_NOT_OK`: PDU の送信に失敗した。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | 下位層の通信インタフェースのモジュールが、PDU の送信、または PDU の送信の失敗を確認する。 |

（SRS_Com_02044）

`Com_TxConfirmation` の注意点: この関数は、割り込みのコンテキスト（例: 送信割り込み）で呼ばれることがある。

#### 8.4.5 Com_TpTxConfirmation

**[SWS_Com_00725]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_TpTxConfirmation` |
| 構文 | `void Com_TpTxConfirmation(PduIdType id, Std_ReturnType result)` |
| サービス ID [hex] | 0x48 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 送信された I-PDU の識別。<br>`result`: I-PDU の送信の結果。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | この関数は、I-PDU がそのネットワークで送信された後に呼ばれる。結果が、送信が成功したかを示す。 |

（SRS_Com_02095）

#### 8.4.6 Com_StartOfReception

**[SWS_Com_00691]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_StartOfReception` |
| 構文 | `BufReq_ReturnType Com_StartOfReception(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength, PduLengthType* bufferSizePtr)` |
| サービス ID [hex] | 0x46 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: I-PDU の識別。<br>`info`: トランスポートプロトコルの I-PDU の受信の、最初のフレームまたはシングルフレームの、ペイロードデータ（プロトコル情報を除く）とペイロード長、およびこの PDU に関するメタデータを含む、`PduInfoType` 構造体へのポインタ。最初／シングルフレームのデータもメタデータも利用できない場合、このパラメータは `NULL_PTR` に設定される。<br>`TpSduLength`: 受信する N-SDU の全長。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `bufferSizePtr`: 受信側のモジュールで利用できる受信バッファ。このパラメータは、トランスポートプロトコルのモジュールで、ブロックサイズ（BS）を計算するために使われる。 |
| 戻り値 | `BufReq_ReturnType`<br>`BUFREQ_OK`: 接続が受理された。`bufferSizePtr` が利用できる受信バッファを示し、受信が続けられる。要求されたサイズのバッファが利用できない場合、`bufferSizePtr` で受信バッファサイズ 0 が示されなければならない。<br>`BUFREQ_E_NOT_OK`: 接続が拒否された。受信は中止される。`bufferSizePtr` は変更されない。<br>`BUFREQ_E_OVFL`: 必要な長さのバッファを提供できない。受信は中止される。`bufferSizePtr` は変更されない。 |
| 説明 | この関数は、N-SDU の受信の開始時に呼ばれる。N-SDU は、複数の N-PDU に分割される場合（FF と、それに続く 1 つ以上の CF）と、1 つの N-PDU から成る場合（SF）がある。`TpSduLength` が 0 で呼ばれたとき、このサービスは、現在利用できる最大のバッファサイズを提供しなければならない。 |

（SRS_Com_02095）

**[SWS_Com_00654]** `ComRxPduId` が大きな I-PDU に対応し、`TpSduLength` が、この I-PDU のコンフィギュレーションされた長さより大きい場合、関数 `Com_StartOfReception` は `BUFREQ_E_OVFL` を返さなければならない。（SRS_Com_02095）

**[SWS_Com_00655]** `ComRxPduId` が動的な長さの I-PDU に対応し、`TpSduLength` が、この I-PDU のコンフィギュレーションされた最大長より大きい場合、関数 `Com_StartOfReception` は `BUFREQ_E_OVFL` を返さなければならない。（SRS_Com_02095）

**[SWS_Com_00657]** AUTOSAR COM モジュールの内部バッファが現在ロックされておらず、パラメータ `TpSduLength` が、期待される（最大の）長さを超えない場合、関数 `Com_StartOfReception` は、ハンドル `ComRxPduId` の I-PDU の、AUTOSAR COM モジュールの内部の I-PDU バッファの全体のサイズを、`RxBufferSizePtr` が示す位置へ書かなければならない。（SRS_Com_02095）

**[SWS_Com_00721]** `ComRxPduId` が停止している I-PDU を指す場合、`Com_StartOfReception` は `BUFREQ_E_NOT_OK` を返さなければならない。（SRS_Com_02095）

**[SWS_Com_00818]** `Com_StartOfReception` が `BUFREQ_E_NOT_OK` または `BUFREQ_E_OVFL` を返した場合、AUTOSAR COM モジュールは、`Com_TpRxIndication` の呼び出しを期待してはならず、したがって、内部バッファをロックしてはならない。（SRS_Com_02095）

**[SWS_Com_00874]** `Com_StartOfReception` が `TpSduLength` を 0 にして呼ばれた場合、AUTOSAR COM モジュールは、この I-PDU のコンフィギュレーションされた（最大の）長さを、`bufferSizePtr` を介して返さなければならない。（SRS_Com_02095）

`TpSduLength` が 0 であることは、メッセージ長が不明であることを示す。

#### 8.4.7 Com_CopyRxData

**[SWS_Com_00692]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_CopyRxData` |
| 構文 | `BufReq_ReturnType Com_CopyRxData(PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr)` |
| サービス ID [hex] | 0x44 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 受信した I-PDU の識別。<br>`info`: コピー元のバッファ（`SduDataPtr`）と、コピーするバイト数（`SduLength`）を与える。`SduLength` が 0 のとき、上位層のモジュールで現在利用できるバッファの量を問い合わせるために使える。この場合、`SduDataPtr` は `NULL_PTR` でもよい。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `bufferSizePtr`: データがコピーされた後に利用できる受信バッファ。 |
| 戻り値 | `BufReq_ReturnType`<br>`BUFREQ_OK`: データが正常にコピーされた。<br>`BUFREQ_E_NOT_OK`: エラーが起きたため、データはコピーされなかった。 |
| 説明 | この関数は、I-PDU のセグメント（N-PDU）の受信データを、上位層へ渡すために呼ばれる。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。残りのバッファのサイズは、`bufferSizePtr` が示す位置に書かれる。 |

（SRS_Com_02095）

`Com_CopyRxData` の注意点: この関数は、割り込みのコンテキストで呼ばれることがある。したがって、データの一貫性を確実にしなければならない。

**[SWS_Com_00782]** `Com_CopyRxData` は、与えられた `PduId` が停止している I-PDU を指す場合、データをコピーせず、`BUFREQ_E_NOT_OK` を返さなければならない。（SRS_Com_00218）

#### 8.4.8 Com_CopyTxData

**[SWS_Com_00693]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_CopyTxData` |
| 構文 | `BufReq_ReturnType Com_CopyTxData(PduIdType id, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr)` |
| サービス ID [hex] | 0x43 |
| 同期／非同期 | 同期 |
| 再入可能性 | 再入可能 |
| 入力パラメータ | `id`: 送信された I-PDU の識別。<br>`info`: コピー先のバッファ（`SduDataPtr`）と、コピーするバイト数（`SduLength`）を与える。十分な送信データが利用できない場合、上位層のモジュールはデータをコピーせず、`BUFREQ_E_BUSY` が返される。下位層のモジュールは、呼び出しを再試行してもよい。`SduLength` が 0 のとき、`retry` パラメータの状態の変化を示す、または上位層のモジュールで現在利用できるデータの量を問い合わせるために使える。この場合、`SduDataPtr` は `NULL_PTR` でもよい。<br>`retry`: このパラメータは、送信されたデータを確認する、または送信の問題の後にデータを再送するために使われる。`retry` パラメータが `NULL_PTR` の場合、送信データは、コピーされた後、直ちにバッファから取り除いてよいことを示す。そうでない場合、`retry` パラメータは、有効な `RetryInfoType` の要素を指さなければならない。`TpDataState` が `TP_CONFPENDING` を示す場合、以前にコピーされたデータは、エラーからの回復のために利用できるよう、TP バッファに残らなければならない。`TP_DATACONF` は、この呼び出しの前にコピーされたすべてのデータが確認され、TP バッファから取り除いてよいことを示す。この API の呼び出しでコピーされたデータは除かれ、後で確認される。`TP_DATARETRY` は、この API の呼び出しが、エラーから回復するために、以前にコピーしたデータをコピーすることを示す。この場合、`TxTpDataCnt` は、現在のデータのコピー位置からのオフセット（バイト単位）を指定する。 |
| 入出力パラメータ | なし |
| 出力パラメータ | `availableDataPtr`: 上位層のモジュールの Tx バッファで利用できる、残りのバイト数を示す。`availableDataPtr` は、動的なペイロード長をサポートする TP モジュール（例: FrIsoTp）が、後続の CF のサイズを決めるために使える。 |
| 戻り値 | `BufReq_ReturnType`<br>`BUFREQ_OK`: データが、要求されたとおり完全に送信バッファへコピーされた。<br>`BUFREQ_E_BUSY`: 必要な量の Tx データが利用できないため、要求を満たせなかった。下位層のモジュールは、この呼び出しを後で再試行してもよい。データはコピーされていない。<br>`BUFREQ_E_NOT_OK`: データはコピーされていない。要求が失敗した。 |
| 説明 | この関数は、I-PDU のセグメント（N-PDU）の送信データを取得するために呼ばれる。この関数の呼び出しのたびに、I-PDU のデータの次の部分が渡される。ただし `retry->TpDataState` が `TP_DATARETRY` の場合を除く。この場合、関数は、`retry->TxTpDataCnt` が示す現在の位置からのオフセットで始まるデータのコピーをやり直す。残りのデータのサイズは、`availableDataPtr` が示す位置に書かれる。 |

（SRS_Com_02095）

`Com_CopyTxData` の注意点: この関数は、割り込みのコンテキストで呼ばれることがある。

**[SWS_Com_00783]** `Com_CopyTxData` は、与えられた `PduId` が停止している I-PDU を指す場合、データをコピーせず、`BUFREQ_E_NOT_OK` を返さなければならない。（SRS_Com_00218）

### 8.5 スケジュールされる関数

#### 8.5.1 Com_MainFunctionRx

**[SWS_Com_00398]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_MainFunctionRx` |
| 構文 | `void Com_MainFunctionRx(void)` |
| サービス ID [hex] | 0x18 |
| 説明 | この関数は、AUTOSAR COM モジュールの受信処理のうち、PDU-R から呼ばれる COM の関数（例: `Com_RxIndication`）の中で直接扱われないものを処理する。 |

（SRS_BSW_00425、SRS_BSW_00432）

**[SWS_Com_00664]** AUTOSAR COM モジュールが、以前に `Com_Init` の呼び出しで初期化されていない場合、`Com_MainFunctionRx` の呼び出しは、単に戻らなければならない。（SRS_BSW_00425）

`Com_MainFunctionRx` のコンフィギュレーション: ECUC_Com_00186 を参照。

#### 8.5.2 Com_MainFunctionTx

**[SWS_Com_00399]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_MainFunctionTx` |
| 構文 | `void Com_MainFunctionTx(void)` |
| サービス ID [hex] | 0x19 |
| 説明 | この関数は、AUTOSAR COM モジュールの送信の動作のうち、RTE から呼ばれる COM の関数（例: `Com_SendSignal`）の中で直接扱われないものを処理する。 |

（SRS_BSW_00425、SRS_BSW_00432）

**[SWS_Com_00665]** AUTOSAR COM モジュールが、以前に `Com_Init` の呼び出しで初期化されていない場合、`Com_MainFunctionTx` の呼び出しは、単に戻らなければならない。（SRS_BSW_00425）

コンフィギュレーション: ECUC_Com_00186 を参照。

#### 8.5.3 Com_MainFunctionRouteSignals

**[SWS_Com_00400]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_MainFunctionRouteSignals` |
| 構文 | `void Com_MainFunctionRouteSignals(void)` |
| サービス ID [hex] | 0x1a |
| 説明 | AUTOSAR COM モジュールの信号ゲートウェイの部分を呼んで、経路付けされる、受信した信号を転送する。 |

（SRS_BSW_00425、SRS_BSW_00432）

**[SWS_Com_00666]** AUTOSAR COM モジュールが、以前に `Com_Init` の呼び出しで初期化されていない場合、`Com_MainFunctionRouteSignals` の呼び出しは、単に戻らなければならない。（SRS_BSW_00425）

`Com_MainFunctionRouteSignals` の注意点:

- 連続する 2 回の呼び出しの間の時間（おそらく、関連するタスク／スレッドの周期）は、信号ゲートウェイの遅延に直接影響する。
- この呼び出しの挿入は、受信割り込みと信号ゲートウェイのタスクを分離するために必要である。
- 通常、この関数を呼ぶ周期タスク／スレッドがコンフィギュレーションされる。このタスク／スレッドの周期は、信号ゲートウェイの遅延に直接影響する。

### 8.6 期待するインタフェース

#### 8.6.1 必須のインタフェース

この章では、モジュールの中核の機能を満たすために必要な、すべてのインタフェースを定義する。

**[SWS_Com_00669]**

| API 関数 | 説明 |
|---|---|
| `PduR_ComTransmit` | PDU の送信を要求する。 |

（SRS_BSW_00384）

#### 8.6.2 オプションのインタフェース

この章では、モジュールのオプションの機能を満たすために必要な、すべてのインタフェースを定義する。

**[SWS_Com_00670]**

> **注（再構成）**: 原典の表は、テキストの抽出で、API 関数と説明の対応が崩れていた。各 API の役割から対応を復元した。

| API 関数 | 説明 |
|---|---|
| `Det_ReportError` | 開発エラーを報告するサービス。 |
| `Det_ReportRuntimeError` | ランタイムエラーを報告するサービス。コールアウトがコンフィギュレーションされていれば、このコールアウトが呼ばれなければならない。 |
| `PduR_ComCancelTransmit` | 下位層の通信モジュールで進行中の PDU の送信の、キャンセルを要求する。 |

（SRS_BSW_00384）

API 関数 `Det_ReportError` に関連するコンフィギュレーションパラメータは、`ComConfigurationUseDet`（ECUC_Com_00141）である。

API 関数 `PduR_ComCancelTransmit` は、少なくとも 1 つの I-PDU に `ComIPduCancellationSupport` が `TRUE` にコンフィギュレーションされている場合は、含まれていなければならない。ECUC_Com_00709 を参照。

#### 8.6.3 コンフィギュレーションできるインタフェース

##### 8.6.3.1 コールバック／通知関数

コンフィギュレーションできるインタフェースの注意点: コールバックルーチンは、割り込みレベルまたはタスクレベルのいずれかで動作する。したがって、割り込みサービスルーチンとタスクのそれぞれについての、システム関数の使用に関する OS の制限が適用される。

以降の関数はすべて、同期で、再入可能性は「問わない（don't care）」、入力・入出力・出力パラメータは無し（`Com_CbkCounterErr` を除く）、戻り値は無しである。

| 関数 | 条項 | 説明と対応関係 |
|---|---|---|
| `void Com_CbkTxAck(void)` | [SWS_Com_00468] | [17] の通知クラス 2 に相当する。メッセージを含む I-PDU の送信が成功した直後に呼ばれる。送信側だけで呼ばれる。信号と信号グループにコンフィギュレーションできる。`Rte_COMCbkTAck_<sn>` または `Rte_COMCbkTAck_<sg>` に対応する。コールバック関数の名前は、`ComNotification`（ECUC_Com_00498）でコンフィギュレーションしなければならない。 |
| `void Com_CbkTxErr(void)` | [SWS_Com_00491] | [17] の通知クラス 4 に相当する。対応する I-PDU グループが停止しているために、送信ができない場合に呼ばれる。送信側だけで呼ばれる。`Rte_COMCbkTErr_<sn>` または `Rte_COMCbkTErr_<sg>` に対応する。コールバック関数の名前は、`ComErrorNotification`（ECUC_Com_00499）でコンフィギュレーションしなければならない。 |
| `void Com_CbkTxTOut(void)` | [SWS_Com_00554] | [17] の通知クラス 4 に相当する。デッドライン監視の仕組みにより、メッセージの送信エラーが検出された直後に呼ばれる。送信側だけで呼ばれる。信号と信号グループにコンフィギュレーションできる。`Rte_COMCbkTxTOut_<sn>` または `Rte_COMCbkTxTOut_<sg>` に対応する。コールバック関数の名前は、`ComTimeoutNotification`（ECUC_Com_00552）でコンフィギュレーションしなければならない。 |
| `void Com_CbkRxAck(void)` | [SWS_Com_00555] | [17] の通知クラス 1 に相当する。メッセージが受信メッセージオブジェクトに格納された直後に呼ばれる。受信側だけで呼ばれる。信号と信号グループにコンフィギュレーションできる。`Rte_COMCbk_<sn>` または `Rte_COMCbk_<sg>` に対応する。コールバック関数の名前は、`ComNotification`（ECUC_Com_00498）でコンフィギュレーションしなければならない。 |
| `void Com_CbkRxTOut(void)` | [SWS_Com_00556] | [17] の通知クラス 3 に相当する。デッドライン監視の仕組みにより、メッセージの受信エラーが検出された直後に呼ばれる。受信側だけで呼ばれる。信号と信号グループにコンフィギュレーションできる。`Rte_COMCbkRxTOut_<sn>` または `Rte_COMCbkRxTOut_<sg>` に対応する。コールバック関数の名前は、`ComTimeoutNotification`（ECUC_Com_00552 を参照）でコンフィギュレーションしなければならない。 |
| `void Com_CbkInv(void)` | [SWS_Com_00536] | [SWS_Com_00680] に対応する。無効な信号または信号グループを受信した後に呼ばれる。受信側だけで呼ばれる。信号、グループ信号、信号グループにコンフィギュレーションできる。`Rte_COMCbkInv_<sn>`（信号とグループ信号）または `Rte_COMCbkInv_<sg>` に対応する。コールバック関数の名前は、`ComInvalidNotification`（ECUC_Com_00315）でコンフィギュレーションしなければならない。（SRS_Com_02079 も参照） |

（上記 6 関数とも、SRS_BSW_00359、SRS_BSW_00360）

**[SWS_Com_00726]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_CbkCounterErr` |
| 構文 | `void Com_CbkCounterErr(PduIdType ComPduId, uint8 ExpectedCounter, uint8 ReceivedCounter)` |
| 同期／非同期 | 同期 |
| 再入可能性 | 問わない（don't care） |
| 入力パラメータ | `ComPduId`: カウンタの不一致が検出された I-PDU の Id<br>`ExpectedCounter`: 期待するカウンタ値<br>`ReceivedCounter`: 受信したカウンタ値 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | なし |
| 説明 | この関数は、AUTOSAR COM モジュールが、期待するものと異なる I-PDU カウンタを受信した場合に、直ちに呼ばれる。 |

（SRS_Com_02102）

`Com_CbkCounterErr` は、受信側だけで呼ばれる。I-PDU カウンタごとにコンフィギュレーションできる。コールバック関数の名前は、`ComIPduCounterErrorNotification`（ECUC_Com_00003）でコンフィギュレーションしなければならない。

##### 8.6.3.2 I-PDU コールアウト関数

[SWS_Com_00013] に述べたとおり、ネットワーク順のメッセージのコールアウトと、CPU 順のメッセージのコールアウトは、AUTOSAR COM モジュールではサポートされない。したがって、AUTOSAR COM モジュールがサポートする唯一のコールアウトの方法は、I-PDU コールアウトである。AUTOSAR COM モジュールは、送信側と受信側で、I-PDU コールアウトをサポートする。

[17] に規定されているとおり、I-PDU コールアウトが `false` を返した場合、I-PDU は、それ以上処理されない。

**[SWS_Com_00700]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_RxIpduCallout` |
| 構文 | `boolean Com_RxIpduCallout(PduIdType PduId, const PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x2A |
| 同期／非同期 | 同期 |
| 再入可能性 | 問わない（don't care） |
| 入力パラメータ | `PduId`: 受信した I-PDU の ID。<br>`PduInfoPtr`: 受信した I-PDU の長さ（`SduLength`）と、I-PDU のデータへのポインタ（`SduDataPtr`）を含む。 |
| 入出力パラメータ | なし |
| 出力パラメータ | なし |
| 戻り値 | `boolean`<br>`true`: I-PDU は通常どおり処理される。<br>`false`: I-PDU は、それ以上処理されない。 |
| 説明 | 受信側の I-PDU コールアウトは、ユーザ定義の受信フィルタリングの仕組みを実装するために、コンフィギュレーションできる。 |

（SRS_Com_02037）

コールアウト関数の名前は、`ComIPduCallout`（ECUC_Com_00387）でコンフィギュレーションしなければならない。

**[SWS_Com_00816]** AUTOSAR COM モジュールは、受信した I-PDU のすべてのデータ（すなわち、PduR が提供した I-PDU の全体）を、`Com_RxIpduCallout` に渡さなければならない。（SRS_Com_02037）

**[SWS_Com_00346]**

| 項目 | 内容 |
|---|---|
| サービス名 | `Com_TxIpduCallout` |
| 構文 | `boolean Com_TxIpduCallout(PduIdType PduId, PduInfoType* PduInfoPtr)` |
| サービス ID [hex] | 0x29 |
| 同期／非同期 | 同期 |
| 再入可能性 | 問わない（don't care） |
| 入力パラメータ | `PduId`: 送信する I-PDU の ID。 |
| 入出力パラメータ | `PduInfoPtr`: 送信する I-PDU の長さ（`SduLength`）と、I-PDU のデータへのポインタ（`SduDataPtr`）を含む。 |
| 出力パラメータ | なし |
| 戻り値 | `boolean`<br>`true`: I-PDU は通常どおり処理される。<br>`false`: I-PDU は、それ以上処理されない。 |
| 説明 | 送信側の I-PDU コールアウトは、例えば、ユーザ定義の送信フィルタリングや、送り出す I-PDU に対する、ユーザ定義の送信前の処理を実装するために、コンフィギュレーションできる。 |

（SRS_Com_02037）

コールアウト関数の名前は、`ComIPduCallout`（ECUC_Com_00387）または `ComIPduTriggerTransmitCallout`（ECUC_Com_00765）のいずれかで、コンフィギュレーションしなければならない。

AUTOSAR COM モジュールは、I-PDU コールアウトの中の動作で I-PDU が起動されたときに起こりうる再帰を、検出も防止もする手段を実装しない。望ましくない、または終わりのない I-PDU コールアウトの連鎖を避けることは、I-PDU コールアウトの実装者と、システムインテグレータだけの責任である。

**[SWS_Com_00381]** AUTOSAR COM モジュールは、`Com_TriggerIPDUSend`、`Com_TriggerIPDUSendWithMetaData`、`Com_SendSignal`、`Com_SendSignalGroup` 以外の、AUTOSAR COM モジュールの API が、I-PDU コールアウトの中から呼ばれることを、サポートしてはならない。（SRS_Com_02037）

**[SWS_Com_00766]** I-PDU に `ComIPduTriggerTransmitCallout` がコンフィギュレーションされていて、I-PDU が開始されている場合、AUTOSAR COM モジュールは、この I-PDU の `Com_TriggerTransmit` の実行のたびに、この I-PDU コールアウトを呼ばなければならない。（SRS_Com_02037）

**[SWS_Com_00395]** `Com_TriggerTransmit` が呼ばれるとき、AUTOSAR COM モジュールは、`ComIPduTriggerTransmitCallout` の戻り値を無視しなければならない。（SRS_Com_02037）

**[SWS_Com_00719]** `ComIPduDirection` が `SEND` にコンフィギュレーションされた I-PDU に `ComIPduCallout` がコンフィギュレーションされている場合、AUTOSAR COM モジュールは、この I-PDU コールアウトを、I-PDU が `PduR_ComTransmit` で送信される直前に、直接呼ばなければならない。（SRS_Com_02037）

TP の I-PDU では、I-PDU コールアウト関数は、通常の I-PDU の I-PDU コールアウト関数と同様に扱われ、送信側と受信側の両方でサポートされる。例えば、要求 [SWS_Com_00719] は、コンフィギュレーションされた `ComIPduType` を考慮しないことに注意。

**[SWS_Com_00720]** 大きな I-PDU を TP インタフェースで正常に受信したとき、I-PDU コールアウトは、`Com_TpRxIndication` の呼び出しの中で行われなければならない。（SRS_Com_02095）

**[SWS_Com_00839]** `Com_TpRxIndication` のコンテキストでの I-PDU コールアウトが `false` を返した場合、I-PDU は、正しく受信されなかったものとみなされ、したがって、含まれるすべての信号は、無効化された信号とまったく同じように扱われなければならない。すなわち、AUTOSAR COM モジュールは、含まれるすべての信号と信号グループについて、コンフィギュレーションされたすべての `ComDataInvalidAction` を実行しなければならない。（SRS_Com_02095）

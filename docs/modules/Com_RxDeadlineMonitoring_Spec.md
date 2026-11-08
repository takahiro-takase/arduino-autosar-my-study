# Com 受信デッドライン監視（Reception Deadline Monitoring）仕様調査

AUTOSAR CP R4.3.1 の SWS Com（原文 `docs/autosar/4.3.1/AUTOSAR_SWS_COM.pdf`）の全体から、受信デッドライン監視に関わる要求を、開始・継続・終了・満了時の動作に分けて整理したもの。和訳版（`docs/autosar/4.3.1_ja/Com_ja.md`）とも突き合わせ、番号と内容に食い違いが無いことを確認した。

この文書は仕様の整理であり、実装（`src/Bsw/Com/`）との突き合わせは含まない。

## 1. 有効・無効の前提

| 要求 | 内容 |
|---|---|
| SWS_Com_00772 | I-PDU の監視が有効なのは、監視が有効な I-PDU グループに含まれる場合に限る（"if and only if"）。そうでなければ無効。 |

- 有効・無効の切り替えは、I-PDU グループ単位の API で行う（7.3.6.1 の冒頭、7.3.5.1 と同様）。
- 複数グループに所属する I-PDU で、グループごとに有効・無効が食い違う場合の扱いは、原文にも記述が無い。解釈は実装側の判断になる。

## 2. 開始

| 要求 | 内容 |
|---|---|
| SWS_Com_00787（項目 2） | `Com_IpduGroupStart` で I-PDU を開始すると、`ComFirstTimeout` が 0 でないすべての信号の、受信デッドライン監視タイマを再開する。 |
| SWS_Com_00733 | 同じ開始で、`Initialize` の値に関係なく、デッドライン監視を開始する。 |
| SWS_Com_00224 | `Com_EnableReceptionDM` で有効にしたとき、含まれる信号と信号グループのタイマを `ComFirstTimeout` に設定する。 |
| SWS_Com_00486 | すでに有効な I-PDU への `Com_EnableReceptionDM` は、黙って無視する（エラーも DET 報告もしない。タイマも再ロードしない）。 |
| SWS_Com_00840 | どのグループにも属さない I-PDU は、`Com_Init` の中で、`Initialize = true` の開始と同じ扱いで開始する。この I-PDU は止められない。 |

## 3. リセット（監視の継続）

| 要求 | 内容 |
|---|---|
| SWS_Com_00715 | 受信に成功したとき、`Com_RxIndication` または `Com_TpRxIndication` で、I-PDU のタイマをリセットする。 |
| SWS_Com_00738 | I-PDU 単位の監視は、信号の値を考慮しない。 |

- 7.3.1 の注記: 無効な信号（データ無効値）や信号グループを受信した場合も、タイマは再開される。
- 受信処理の順序（要求番号なし）: I-PDU 単位の監視のリセットは受信処理の早い段階（コールアウトの前）、信号単位の監視のリセットは、受信フィルタリングの後、通知の前に行う。

## 4. 終了

| 要求 | 内容 |
|---|---|
| SWS_Com_00225 | すでに無効な I-PDU への `Com_DisableReceptionDM` は、黙って無視する。 |
| SWS_Com_00534 | Tx の I-PDU を含むグループへの `Com_EnableReceptionDM` と `Com_DisableReceptionDM` は、黙って無視する。 |

7.3.6.1.1 の地の文（要求番号なし）:
- 無効のときにタイマが満了しても、RTE へのエラー通知は行わない。
- 無効にしても、I-PDU の受信は止まらない。
- 有効にすることは、`ComTimeoutNotification` を持つ信号と信号グループについて、満了の通知が RTE へ行われることを意味する。

`Com_IpduGroupStop` による停止（7.3.5.3 節）:

| 要求 | 内容 |
|---|---|
| SWS_Com_00685 | I-PDU を停止すると、デッドライン監視をキャンセルする。 |
| SWS_Com_00684 | 停止すると、受信処理を無効にする。 |
| SWS_Com_00713 | 大きな I-PDU を停止すると、受信処理を止め、途中まで受信した分を無視する。 |

- 表 3（停止した I-PDU の動作）にも、受信側は "Disable RX deadline monitoring"、送信側は "Disable TX deadline monitoring" とある。
- SWS_Com_00115 は、停止時に**未確認の送信**（TX）のデッドライン監視をキャンセルする要求で、受信側の話ではない。同 00479 は、未確認の送信信号に対する `ComErrorNotification` の即時呼び出し。

## 5. 監視の対象

| 要求 | 内容 |
|---|---|
| SWS_Com_00333 | `ComTimeout` が省略または 0 のとき、その信号（信号グループ）は監視しない。`ComFirstTimeout` も無視する。 |
| SWS_Com_00716 | `ComFirstTimeout` が省略または 0 のとき、I-PDU の開始から最初の受信までは監視しない。 |
| SWS_Com_00290 | 更新ビットを持たない信号は、I-PDU 単位で監視する。 |
| SWS_Com_00291 | 更新ビットを持たない信号と信号グループは、0 でない最小のタイムアウト（`ComFirstTimeout`、`ComTimeout`）で、I-PDU を監視する。 |
| SWS_Com_00292 | 更新ビットを持つ信号（信号グループ）は、それぞれ個別に監視する。 |

- すべての信号が更新ビットを持つ I-PDU では、I-PDU 単位の監視は不要。

## 6. 満了時の動作

| 要求 | 条件（`ComRxDataTimeoutAction`） | 動作 |
|---|---|---|
| SWS_Com_00470 | REPLACE（信号） | `ComSignalInitValue` で置き換える。 |
| SWS_Com_00875 | SUBSTITUTE（信号） | `ComTimeoutSubstitutionValue` で置き換える。 |
| SWS_Com_00500 | NONE または省略 | 置き換えない。 |
| SWS_Com_00513 | REPLACE（信号グループ） | 含まれるすべてのグループ信号を `ComSignalInitValue` で置き換える。 |
| SWS_Com_00876 | SUBSTITUTE（信号グループ） | 含まれるすべてのグループ信号を `ComTimeoutSubstitutionValue` で置き換える。 |
| SWS_Com_00744 | アクションと通知の両方を設定 | 先に `ComRxDataTimeoutAction` を実行し、その後に `ComTimeoutNotification` を呼ぶ。 |

## 7. API

| API | 要求番号 | サービス ID | 引数 |
|---|---|---|---|
| `Com_EnableReceptionDM` | SWS_Com_91004 | 0x06 | `Com_IpduGroupIdType IpduGroupId` |
| `Com_DisableReceptionDM` | SWS_Com_91003 | 0x05 | `Com_IpduGroupIdType IpduGroupId` |

どちらも、指定した I-PDU グループの中の I-PDU について、受信デッドライン監視を有効・無効にする。

## 8. 原文に規定が無く、解釈が分かれる点

1. **複数グループに所属する I-PDU**: SWS_Com_00772 は「有効なグループに含まれる場合に限り有効」と述べる。いずれか 1 つのグループが有効なら有効なのか、すべてのグループが有効でなければならないのかは決まらない。
2. **SWS_Com_00486 の位置づけ**: 複数グループに限らず、「有効化は冪等（べきとう）であるべき」「タイマを再ロードしない」という規定。
3. **停止中に有効化・無効化を呼んだ場合**、および、停止前に無効化した I-PDU を再開したときの監視の状態: 規定は見つからなかった。

## 9. 本プロジェクトの実装との突き合わせ（`src/Bsw/Com/Com.c`）

| 要求 | 結果 | 実装の該当箇所 |
|---|---|---|
| SWS_Com_00224（有効化でタイマを `ComFirstTimeout` に設定） | 満たす | `Com_EnableReceptionDM` → `Com_ResetRxDeadlineMonitoring`（`Com_RxLastMs` を現在時刻へ、`Com_RxUsingFirstTimeout = 1`、満了フラグをクリア） |
| SWS_Com_00486（すでに有効なら無視、再ロードしない） | 満たす | `Com_RxDmEnabled != 0` なら `continue`（テスト `EnableWhileAlreadyEnabledKeepsRunningTimer`） |
| SWS_Com_00225（すでに無効なら無視） | 満たす | フラグを 0 にするだけで、他の状態を変えない（冪等。重複呼び出しのテストは無い） |
| SWS_Com_00534（Tx の I-PDU を含むグループは無視） | 満たす | `Com_IpduGroupHasTxMember`（テスト `GroupWithTxMemberIsIgnored`） |
| SWS_Com_00787 項目 2、00733（開始でタイマ再開、`Initialize` に非依存） | 満たす | `Com_IpduGroupStart` が `Initialize` の分岐の外で `Com_ResetRxDeadlineMonitoring` を呼ぶ |
| SWS_Com_00685（停止で監視をキャンセル） | 満たす | `Com_RxIPduStarted = 0` の間、`Com_MainFunctionRx` が監視を評価しない。満了フラグは残るが、再開時にクリアされる |
| SWS_Com_00772（有効の条件） | 実装独自の構成 | I-PDU ごとの `Com_RxDmEnabled`（既定 1）と、グループの起動状態の AND。下記参照 |

**複数グループの論点は、本実装では発生しない**: `Com_IPduConfigType.IpduGroupId` は 1 つの値で、1 つの I-PDU が複数のグループに所属する設定を表せない。したがって「どれか 1 つか、すべてか」という解釈の分かれは、現状は問題にならない。複数所属をサポートするときに、あらためて決める必要がある。

**仕様との構成の差**: 仕様はグループ単位の有効・無効（`Com_EnableReceptionDM(IpduGroupId)`）と述べるが、実装はその結果を I-PDU ごとのフラグで持つ。外から見える動作（グループ ID で操作し、グループ内の Rx I-PDU に適用）は同じ。

**今回の見直しで直したもの**: `Com.c` と `Com.h` の Doxygen にあった要求番号の誤記（有効化は `SWS_Com_91004`、無効化は `SWS_Com_91003`。以前は `91001の対` と書かれていた）。

## 10. 調査の出典

- 原文: `docs/autosar/4.3.1/AUTOSAR_SWS_COM.pdf`（Document ID 015、R4.3.1）。7.3.5（I-PDU グループ）、7.3.6.1（受信デッドライン監視）、8.3.2（API）を中心に、Com の章全体を検索した。
- 和訳: `docs/autosar/4.3.1_ja/Com_ja.md`（7.3.6.1 節）。

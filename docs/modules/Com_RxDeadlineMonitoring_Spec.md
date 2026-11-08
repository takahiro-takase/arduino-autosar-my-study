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

SWS_Com_00115（`Com_IpduGroupStop` での停止）は 7.3.5.3 節にあるが、停止時にデッドライン監視をどう扱うかを直接述べた記述は見つからなかった。

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
| `Com_EnableReceptionDM` | SWS_Com_91002（※） | 0x04 | `Com_IpduGroupIdType IpduGroupId` |
| `Com_DisableReceptionDM` | SWS_Com_91003 | 0x05 | `Com_IpduGroupIdType IpduGroupId` |

※ 91002 は、91003 の並びからの推定。有効化側の見出し行は未確認。

どちらも、指定した I-PDU グループの中の I-PDU について、受信デッドライン監視を有効・無効にする。

## 8. 未解決・解釈が分かれる点

1. **複数グループに所属する I-PDU**: SWS_Com_00772 は「有効なグループに含まれる場合に限り有効」と述べる。いずれか 1 つのグループが有効なら有効なのか、すべてのグループが有効でなければならないのかは決まらない。
2. **停止時の扱い**: `Com_IpduGroupStop` で I-PDU を停止したとき、タイマを止めるか、状態を保持するかを述べた記述は見つからなかった。
3. **SWS_Com_00486 の位置づけ**: 複数グループに限らず、「有効化は冪等（べきとう）であるべき」「タイマを再ロードしない」という規定。

## 9. 調査の出典

- 原文: `docs/autosar/4.3.1/AUTOSAR_SWS_COM.pdf`（Document ID 015、R4.3.1）。7.3.5（I-PDU グループ）、7.3.6.1（受信デッドライン監視）、8.3.2（API）を中心に、Com の章全体を検索した。
- 和訳: `docs/autosar/4.3.1_ja/Com_ja.md`（7.3.6.1 節）。

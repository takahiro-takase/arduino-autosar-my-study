# E2EMon（ネットワーク健全性モニタ、独自 CDD 相当）

> [README](../../README.md) の「[E2E 保護](../../README.md#e2e-p01)」節から分離。

`E2EXf_InverseTransformP05()`（[`E2EXf_Notes.md`](./E2EXf_Notes.md) 参照）が検出した E2E エラーは、
これまで Dem への DTC 報告にしか使われていませんでした。これとは別に、
EngineInfo/AbsInfo 受信の E2E 検証結果を集計し、CAN バス上へブロードキャストする
「ネットワーク健全性テレメトリ」を独立したモジュール `E2EMon`（`src/Bsw/E2EMon/`）
として実装しています。

**なぜ E2EXf や Rte に直接手を入れないのか**: 実際の AUTOSAR 開発では、RTE・
E2E Transformer（E2EXf）はいずれも ARXML 設定からコード生成ツールが生成する
成果物であり、生成後のファイルを手で書き換えても次回生成で上書きされます
（E2E の検証・保護アルゴリズム自体も、Vector 等が提供する認証済みライブラリで
あることが多く、ソース改変は現実的ではありません）。COM スタックに独自要件を
足したい場合、実務では標準モジュール自体を無改造のまま、**独自の CDD
（Complex Device Driver）を新規に書き、標準モジュールが持つ設定可能な通知フック
経由で配線する**のが一般的なパターンです。本プロジェクトにはコード生成ツールが
無いため Rte.c/E2EXf.c は「生成コードの手書き代用品」ですが、E2EMon はその代用品
すら改変せず、独立した CDD 相当のモジュールとして追加しています。

```
Rte_COMRxInd_EngineInfo()/AbsInfo()（Rte.c、E2EXf_InverseTransformP05() 呼び出し直後）:
  E2EMon_NotifyCheckResultP05(checkStatus) を呼ぶ
    ← 実 AUTOSAR で言う「ARXML で設定した OnDataReceived 通知フックが RTE から
       生成され、独自 CDD の関数を呼ぶ」という接続方式を模したもの

E2EMon_NotifyCheckResultP05()（E2EMon.c）:
  status == ERROR（CRC不一致）?
    YES → E2EMon_CrcErrorCount++（0xFF で飽和）
  status == WRONGSEQUENCE または REPEATED ?
    YES → E2EMon_SequenceErrorCount++（0xFF で飽和）
  Com_SendSignal(COM_SIGNAL_E2E_CRC_ERR_COUNT, &E2EMon_CrcErrorCount)
  Com_SendSignal(COM_SIGNAL_E2E_SEQ_ERR_COUNT, &E2EMon_SequenceErrorCount)
    ← 値をセットするだけで、送信タイミングには一切関与しない
```

**送信スケジューリングは Com 自身の責務（PERIODIC 送信モード）**: 当初は
MeterStatus フレームに相乗りさせる案も検討しましたが、実務では「オペレーショナルな
ステータスフレームに診断テレメトリを混ぜない」「周期送信のスケジューリングは
CDD 自身ではなく Com モジュールの責務」という判断が一般的なため、専用の新規
I-PDU（`E2EHealthStatus`、CAN ID 0x220）として分離しました。Com には新たに
`ComTxModeMode` 相当の `TxModeMode` 設定（`COM_TX_MODE_DIRECT` / `_MIXED` / `_PERIODIC`）
を追加し、`E2EHealthStatus` は `COM_TX_MODE_PERIODIC` として `Com_MainFunctionTx()`
が自分の周期タイマ（既定 6000ms、`COM_TX_PERIOD_E2EHEALTH_MS`）で自動送信します。
E2EMon は `Com_SendSignal()` を呼ぶだけで、送信タイミングには一切関与しません
（詳細は [`Com_Notes.md`](./Com_Notes.md) 参照）。
テレメトリ自体の破損を監視ツールが検出できるよう、この `E2EHealthStatus` には
E2E Profile05 保護を付与しています（詳細は [`E2EXf_Notes.md`](./E2EXf_Notes.md) の
「送信側（Protect）— E2EHealthStatus」参照。E2EMon 自身は E2E 保護の存在を一切知りません）。

**Dem の ExtendedData（故障確定回数）との違い**: Dem の ExtendedData
（UDS SID 0x19/06 で読み出せる、DTC ごとの確定 FAILED 回数）は NvM 永続化される
「デバウンス確定後」の累積回数です。一方この E2E エラーカウンタは、デバウンスを
介さない生の検証結果（1 フレームごとの ERROR/WRONGSEQUENCE/REPEATED）を
そのまま数えており、UDS でポーリングせずとも CAN バス上の他 ECU・監視ツールが
`E2EHealthStatus` を受信するだけでリアルタイムに観測できる、という違いがあります。

## E2EHealthStatus フレームレイアウト（CAN ID 0x220 / DLC=5 / PERIODIC / E2E Profile05 保護）

```
byte[0-1] : E2E CRC16（多項式 0x1021、リトルエンディアン、SWS_E2E_00400/00406 準拠）
byte[2]   : E2E Counter（8bit フル値）
byte[3]   : E2ECrcErrCount（EngineInfo/AbsInfo 受信の E2E CRC 不一致累積数、0-255 で飽和）
byte[4]   : E2ESeqErrCount（EngineInfo/AbsInfo 受信の E2E シーケンス異常累積数、0-255 で飽和）
```

**動作確認方法**: uds_tester で EngineInfo/AbsInfo の byte[0-1]（CRC16）を意図的に
誤った値にして送信すると、`E2EHealthStatus` の byte[3]（crcErr）が実機ログ・
uds_tester の受信モニター双方で 1 ずつ増えることが確認できます（uds_tester の
「E2EHealthStatus (0x220)」受信モニターは `crcErr=N seqErr=M` の形式で表示します）。
カウンタ飛びを起こすと byte[4]（seqErr）が増えます。6000ms 周期で自動送信される
ため、値が変化していなくても定期的にフレームが流れ続けることも確認できます
（1000ms だとシリアルログの出力量が多く流れてしまうため 6000ms を既定値としています）。

```
[1019ms] INFO  Com: TX iPdu=2 [XX XX 00 00 00]  # E2EHealthStatus、6000ms 周期で自動送信
[7019ms] INFO  Com: TX iPdu=2 [YY YY 01 00 00]  # Counter が 1 に進む

# EngineInfo の CRC を意図的に誤らせて送信した直後
[8501ms] WARN  E2EXf: InverseTransformP05 NG DemEvent=9 st=7  ← st=7: ERROR（CRC不一致）
[8502ms] INFO  E2EMon: (内部カウンタ更新、次回 PERIODIC 送信まではログなし)
[13019ms] INFO  Com: TX iPdu=2 [ZZ ZZ 02 01 00]  # crcErr が 0→1 に増加
```

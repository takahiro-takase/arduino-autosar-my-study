# Com

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

シグナルのビット単位パック／アンパックと送受信タイミング制御を担う（TxModeMode:
DIRECT/MIXED/PERIODIC・TMS・MDT・受信フィルタ・I-PDU Group・Signal Gateway 等）。
E2E には一切関知しない（E2E Transformer 方式、`Com.c` 本体に E2E は埋め込まれない）。
ComFilterAlgorithm/TxModeMode によるシグナルの送信要否判定、Signal Group と
ComTransferProperty、TMS（Transmission Mode Selector）、MDT、Tx確定コールバック
（Com_CbkTxAck）、Update Bit、RX Signal Group、ComRxDataTimeoutAction、Rx無効値検知
（ComDataInvalidAction）、RX ComFilterAlgorithm によるプラウジビリティチェック、
受信デッドライン監視、Signal Gateway（Com_GatewayRoute）など、Com モジュール単体で
完結する実装詳細を以下にまとめます。

**MeterStatus（CAN 0x200）の可視化用ミラー拡張（本プロジェクト独自）**: 実機に
RPM・水温・警告灯の物理表示器が無いため、`uds_tester` の仮想メータ表示タブが
`MeterStatus` 1 フレームだけをデコードすれば済むよう、`byte[2]` に
`WarningStatus`（CAN 0x210）と同値の警告灯 3bit、`byte[3-4]` に `EngineInfo`
（CAN 0x100）の検証済み `EngineSpeed` と同値の 16bit、`byte[5]` に同じく
`EngineInfo` の検証済み `CoolantTemp` と同値の 8bit をそれぞれミラー追加した
（DLC 2→5→6）。`App_EngineManager`/`App_WarningIndicator` が
`Rte_Write_MeterStatus_EngineSpeed()`/`Rte_Write_MeterStatus_CoolantTemp()`/
`Rte_Write_MeterStatus_RunLamp()` 等の専用ポート経由で書き込む
（`Com_SendSignal()` を直接呼ばない、既存の `EngineState`/`RunLamp` 等と同じ
RTE 経由の作法）。値が `WarningStatus`/`EngineInfo` と重複することは、1 フレーム
完結という可視化ツール側の要件を優先した意図的なトレードオフ。

## ComFilterAlgorithm と TxModeMode（送信要否・タイミングを Com 自身が判断する）

実車の AUTOSAR Com は、ASW が値をセットする（`Com_SendSignal`）ことと、実際に CAN へ
いつ送信すべきかを判断すること（`ComFilterAlgorithm` + `ComTxModeMode`）を分離しています。
本プロジェクトでも `EngineState` にこの分離を適用しました。

```
ComFilterAlgorithm = MASKED_NEW_DIFFERS_MASKED_OLD（Mask = 0xFF、8bit 全体を比較）
TxModeMode         = MIXED（MeterStatus）

Com_SendSignal(ENGINE_STATE, value):
  TX バッファへ value をパック（常に実行、ASW から見た値は常に最新）
  (value & Mask) != (前回のフィルタ比較値 & Mask) ?
    YES → Com_RequestTxOnChange(MeterStatus) を呼ぶ
            → Com_TxPending[MeterStatus] = 1 を立てるだけ
              （PduR_Transmit は呼ばない。呼び出し元の Runnable は
              ここで一切ブロッキングしない）
    NO  → 何もしない（バッファは更新済みだが送信要求は立たない）

Com_MainFunction()（Os の 100ms タスクから周期的に呼ばれる。WdgM 非監視）:
  MeterStatus について、
    Com_TxPending[MeterStatus] == 1 、
    または最終送信からの経過時間 >= TxPeriodMs（周期フロア間隔）？
      YES → CommunicationControl 抑制中でなければ送信する
              （TxTransformCbk があれば呼んだ上で PduR_Transmit
              → CanIf_Transmit → Can_Write の SPI 送信まで完了する）
      NO  → 何もしない
```

**なぜ実送信を `Com_MainFunction()` に一元化したか**: `Com_SendSignal()` の
呼び出し元は `App_EngineManager_Run()` であり、WdgM の Deadline Supervision
（実行時間の上限監視）対象の Runnable です。もし `Com_SendSignal()` の中で
`PduR_Transmit()`（→ MCP2515 への SPI 送信）まで同期的に呼び切ると、バス輻輳等で
SPI 送信が想定より長引いた場合に Runnable 自体の実行時間が伸び、Deadline
Supervision の誤検出につながりかねません。`Com_MainFunction()` は WdgM の
監視対象外のタスクのため、実送信をそちらへディスパッチすることで、SPI 送信の
所要時間が ASW Runnable の実行時間に影響しない設計にしています（SWS_Com_00734
等が要求する「次回メイン関数までに送信を開始する」という猶予の範囲内）。

**なぜ周期フロアが必要か**: 単純に「変化時のみ送信」（DIRECT）にすると、`EngineState` が
長時間同じ値（例: RUNNING が続く）のときフレームが完全に途絶えてしまいます。
実車でも同様の理由（新規に起動した受信 ECU や、直前のフレームを取りこぼした
受信 ECU への配慮）で、変化がなくても一定周期で再送し続ける「MIXED 送信モード」が
一般的です。本プロジェクトでは `COM_TX_PERIOD_METERSTATUS_FLOOR_MS`（既定 9000ms）で
これを実時間ベースに再現しています。

**責務分離の効果**: 以前は「変化したら送る」という判断を ASW（`App_EngineManager`）が
毎サイクル無条件に送信トリガ API を呼ぶことで暗黙的に実現していました
（実際には無条件に送信していただけで、判断はしていませんでした）。今は ASW は
「値をセットする」だけを行い（送信をトリガする API 自体が存在しない）、実際に
いつ送るかは Com 層が `ComFilterAlgorithm`/`TxModeMode` の設定だけで完結して決めます。
将来 ASW のロジックを変更しても Com の設定（`Com_PBCfg.c`）だけで送信頻度のポリシーを
調整できます。

## Com 設定（`Com_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `COM_TX_PERIOD_METERSTATUS_FLOOR_MS` | 9000 | MeterStatus（MIXED）の周期フロア間隔 [ms]。変化がなくてもこの間隔で強制送信する |
| `COM_TX_PERIOD_E2EHEALTH_MS` | 6000 | E2EHealthStatus（PERIODIC）の送信周期 [ms] |
| `COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS` | 2000 | WarningStatus の TMS が true（FAULT/ABS 点灯中）のときの周期フロア間隔 [ms] |
| `COM_TX_MIN_DELAY_WARNINGSTATUS_MS` | 100 | WarningStatus の MDT（変化時送信の最小送信間隔）[ms]。周期フロアには適用しない |

## Com Signal Group（複数シグナルの一括コミット）

`MeterStatus` の `EngineState` は 1 I-PDU に 1 シグナルしかありませんが、実車の Com には
「1 つの I-PDU に属する複数シグナルを、ASW が個別に `Com_SendSignal()` した瞬間ではなく、
すべて揃った時点でまとめて確定させたい」というニーズがあります（Signal Group、
SWS_Com_00050 相当）。バラバラのタイミングで実 TX バッファへ反映すると、途中経過の
不整合な組み合わせ（例: RunLamp だけ新しい値、FaultLamp は古い値のまま）が一瞬でも
CAN へ送信されうるためです。本プロジェクトでは `WarningStatus`（3 本の警告灯）で
これを実装しました。

```
Com_IPduConfigType.IsSignalGroup = 1（WarningStatus, IPduId=1、TMS 付き DIRECT⇔MIXED）

Com_SendSignal(RUN_LAMP/FAULT_LAMP/ABS_LAMP, value)  ← App_WarningIndicator が 3 回呼ぶ
  所属 I-PDU の IsSignalGroup を検索
    == 1 → Com_TxShadowBuffer[1] へパックするのみ（実 TX バッファは変更しない、
            ComFilterAlgorithm 判定もしない）
            ComTransferProperty=TRIGGERED_ON_CHANGE のメンバーのみ、前回値との
            比較（マスクなしの生値比較、Com_FilterLastValue[] を流用）で
            Com_GroupTriggerPending[1] を立てる（詳細は次項）
    == 0 → 従来どおり Com_TxBuffer へ直接パック（MeterStatus はこちら）

Com_SendSignalGroup(GroupId=1)   ← 3 シグナルすべて設定し終えた後に 1 回呼ぶ
  Com_TxShadowBuffer[1] を Com_TxBuffer[1] へバイト単位でコピー（確定コミット。
  PENDING/TRIGGERED_ON_CHANGE を問わず全メンバー分をコピーする）
  Com_GroupTriggerPending[1] を判定
    立っている → Com_RequestTxOnChange(WarningStatus) を呼び、フラグをクリア
                  → Com_TxPending[WarningStatus] = 1 を立てるだけ
                    （次回 Com_MainFunction() で送信、周期フロアなし）
    立っていない → 何もしない
```

**なぜシャドウバッファが必要か**: `Com_SendSignal()` を 3 回呼ぶ間、CAN 送信タスクが
（この実装は非プリエンプティブなので実際には割り込まれませんが）割り込んで
送信を行ってしまうと、3 本のうち更新済みのものと未更新のものが混在した状態で
送信されてしまいます。シャドウバッファへ一旦貯めてから `Com_SendSignalGroup()`
で一括コピーすることで、この不整合な中間状態が実 TX バッファ（延いては CAN バス）
へ現れることはありません。

## ComTransferProperty（Signal Group メンバーごとの送信トリガー宣言）

「3 本のうちどれか 1 本でも変わればコミット全体を送信する」という要否判定を、
以前は `Com_SendSignalGroup()` が前回コミット値とのバイト単位比較（`changed` 判定）で
代用していました。この方式には概念上の欠陥があります。Signal Group の各メンバーには
本来、実 AUTOSAR の `ComTransferProperty`（SWS_Com_00742/00743）として「自分の変化が
送信の引き金になる（TRIGGERED_ON_CHANGE）か、値を保持するだけで他メンバーの送信に
便乗する（PENDING）か」を個別に宣言できるはずですが、バイト単位比較ではこの区別を
表現できず、グループ内の全メンバーが事実上 TRIGGERED_ON_CHANGE 相当になってしまいます。

本実装ではこれを是正し、メンバーごとに `Com_SignalConfigType.TransferProperty`
（`COM_TRANSFER_PROPERTY_PENDING` / `COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE` の
2 値。実 AUTOSAR は他に TRIGGERED/TRIGGERED_ON_CHANGE_WITHOUT_REPETITION も持つが、
本実装は WarningStatus の用途に必要な最小限のみ実装）を宣言できるようにしました。
`RunLamp`/`FaultLamp`/`AbsLamp` は現状すべて `TRIGGERED_ON_CHANGE` のため、挙動自体は
バイト比較方式のときと変わりません（動機は TMS/MDT と同じく、実利より仕様忠実性）。
`COM_TRANSFER_PROPERTY_PENDING` を使う具体例は本設定にはありませんが、「他メンバーの
送信には毎回便乗させたいが、自分の変化単独では送信させたくない」低優先度シグナルを
将来追加する際に使えます。

この判定は `ComFilterAlgorithm`（シグナル単位、1 シグナル = 1 I-PDU 向け）とは別の
独立した仕組みです。`Com_SendSignal()` は Signal Group メンバーに対して
`ComFilterAlgorithm` を一切評価せず、`ComTransferProperty` の判定だけを行います。

## TMS（Transmission Mode Selector、I-PDU 単位で 2 つの送信モードを自動切り替え）

ここまでの `TxModeMode` は I-PDU ごとに 1 つだけ固定で設定していました。実車の
AUTOSAR Com は、1 つの I-PDU に **2 組**の送信モード設定（`ComTxModeTrue` /
`ComTxModeFalse`）を持たせ、どちらが有効かを TMS（Transmission Mode Selector）の
評価結果で自動的に切り替えられます（SWS_Com_00032/00799）。`WarningStatus` に
この仕組みを適用しました。

```
WarningStatus (IPduId=1):
  TxModeMode      = DIRECT （TMS=false のとき使う。ComTxModeFalse 相当）
  TxModeModeTrue  = MIXED  （TMS=true  のとき使う。ComTxModeTrue 相当）
  TxPeriodMsTrue  = COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS（既定 2000ms）

TMS 寄与シグナル（TmsContributor=1）:
  FaultLamp: ComFilterAlgorithm=MASKED_NEW_DIFFERS_X, Mask=0x01, FilterX=0
  AbsLamp  : ComFilterAlgorithm=MASKED_NEW_DIFFERS_X, Mask=0x01, FilterX=0
  （RunLamp は寄与しない = TmsContributor=0）

Com_RecalcTms(WarningStatus)  ← Com_SendSignalGroup() のコミット直後に毎回呼ばれる
  tmsTrue = false
  FaultLamp について: (値 & Mask) != FilterX  ← 値=1 なら true
    true なら tmsTrue = true
  AbsLamp について: 同様に評価、true なら tmsTrue = true
                     （SWS_Com_00678: 1 つでも真なら TMS 全体が true）
  Com_TmsState[WarningStatus] = tmsTrue

Com_EffectiveTxModeMode(WarningStatus):
  Com_TmsState[WarningStatus] ? TxModeModeTrue : TxModeMode
```

**なぜ FaultLamp/AbsLamp だけを TMS 対象にしたか**: RunLamp（エンジン稼働中の表示）は
正常運転そのものであり、途中参加した監視ツールが多少取りこぼしても実害がありません。
一方 FaultLamp/AbsLamp が示す異常状態は、途中参加したツールにも確実に伝わってほしい
という考えで、この 2 つだけを TMS の判定材料にしています。

**具体的に変わる挙動**: AbsActive が単独で立った（FAULT は起きていない）場合、
AbsLamp は点滅せず点灯しっぱなしになります。TMS が無ければ DIRECT のまま
なので、ABS 作動の立ち上がり時に 1 回フレームが送信された後は、AbsActive が
落ちるまで再送されません。この間に監視ツールを新規接続すると、ABS 作動中
であることを知る手段がありません。TMS により AbsLamp 点灯中は MIXED へ
自動的に切り替わるため、`COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS`
（既定 2000ms）ごとに再送され続け、途中参加したツールも確実に把握できます
（FaultLamp は 500ms 点滅中でありバイト自体が毎周期変化するため、この効果は
主に AbsLamp 単独点灯時に意味を持ちます）。

**TMS 変化時の即時送信について（2026-08 対応済み）**: 実 AUTOSAR は「TMS の
変化によりモードが切り替わったら、その変化を起こしたシグナルの
ComTransferProperty によらず無条件に即座に送信する」ことを要求します
（SWS_Com_00495）。当初この特別扱いは独立には実装しておらず、`FaultLamp`/
`AbsLamp` が TMS 寄与シグナル（`TmsContributor=1`）であると同時に
`ComTransferProperty=TRIGGERED_ON_CHANGE` でもあるという設定の偶然の一致に
よって、たまたま結果的に満たされているだけの状態でした（もし将来 TMS 寄与
シグナルを `PENDING` に設定した場合、そのシグナル単体の変化は
`Com_GroupTriggerPending` を立てないため、TMS だけが切り替わって送信が
起きない、という潜在バグになり得ました）。

これを是正し、`Com_RecalcTms()` が「今回の呼び出しで `Com_TmsState[]` が
変化したか」を戻り値で返すようにしたうえで、`Com_SendSignal()`（非 Signal
Group）・`Com_SendSignalGroup()` の両方で、通常の送信トリガー
（`passesFilter`/`Com_GroupTriggerPending`）と TMS 遷移（戻り値）を独立した
OR 条件として合成し、どちらか一方でも成立すれば `Com_RequestTxOnChange()` を
呼ぶように変更しました。update-bit のセット条件はこれとは切り離したまま
（TMS 遷移そのものはシグナル値の更新を意味しないため、`passesFilter`/
`Com_GroupTriggerPending` のみで判定する）です。

**ログ例**:
```
[10123ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:1]     # ABS 作動開始
[10124ms] INFO  Com: TX iPdu=1 [04]                 # 即座に送信（TMS: false→true）
[12124ms] INFO  Com: TX iPdu=1 [04]                 # 周期フロア（2000ms 後、値は不変）
[14124ms] INFO  Com: TX iPdu=1 [04]                 # 周期フロア（継続中）
[15200ms] INFO  WarnInd: [RUN:1 FAULT:0 ABS:0]     # ABS 作動終了
[15201ms] INFO  Com: TX iPdu=1 [00]                 # 即座に送信（TMS: true→false）
                                                     # 以降は DIRECT に戻り、変化なしでは再送されない
```

## MDT（ComMinimumDelayTime、変化時送信の最小送信間隔）

DIRECT/MIXED I-PDU は値が変化するたびに送信要求（`Com_TxPending[]`）が立ちますが、
信号源が高頻度で変化し続けると、その分だけ CAN バスへの送信も連続してしまいます。
実 AUTOSAR の Com は、I-PDU ごとに `ComMinimumDelayTime`（MDT）を設定でき、
直近の実送信からこの時間が経過するまでは変化時送信を保留するバス負荷保護機構を
持っています。`WarningStatus` にこれを適用しました。

```
WarningStatus (IPduId=1):
  MinDelayMs = COM_TX_MIN_DELAY_WARNINGSTATUS_MS（既定 100ms）

Com_MainFunction()（DIRECT/MIXED 共通、周期フロアには適用しない）:
  mdtElapsed = (経過時間 >= MinDelayMs)
  changeDue  = Com_TxPending[WarningStatus] && mdtElapsed
  due        = changeDue || floorDue（MIXED の周期フロア。MDT の影響を受けない）

  due さもなくば:
    何もしない（Com_TxPending は立てたまま破棄しない → 次回以降 MDT 満了時に送信）
```

**「破棄」ではなく「保留」であることが重要**: MDT 未満で変化検知があっても
`Com_TxPending[]` はクリアされません。次回以降の `Com_MainFunction()`
（100ms 周期）で MDT が満了していれば、そのとき初めて送信されます
（SWS_Com_00471/00698/00789 準拠。値そのものを取りこぼすわけではなく、
「送るタイミングを遅らせるだけ」という設計です）。

**なぜ周期フロアには適用しないか**: 実 AUTOSAR は既定
（`ComEnableMDTForCyclicTransmission=false`）で MIXED の周期部分・PERIODIC には
そもそも MDT タイマを起動しません（SWS_Com_00789）。周期送信は既に
`TxPeriodMs` 自身がバス負荷の上限を決めているため、MDT による追加の間引きは
不要という考え方です。本実装もこれに倣い、`floorDue` は `mdtElapsed` の
影響を受けません。

**この実装で MDT が実際に保留を発生させる場面はあるか**: 正直に言うと、
現状の ASW 呼び出しパターンでは稀です。`App_WarningIndicator_Run()` は
500ms 周期でしか `Com_SendSignalGroup()` を呼ばないため、
`COM_TX_MIN_DELAY_WARNINGSTATUS_MS`（100ms）を下回る間隔で変化が連続することは
起こりません。つまり本プロジェクトの現在の信号源だけを見れば、MDT は
「普段は効かない保護的な既定値」です。それでも、(1) 実車では複数の CDD や
より高頻度な信号源が同じ I-PDU に寄与しうるため Com 自身がこの保護を持つ
意味があること、(2) 本実装のロジック自体は正しく機能すること（`Com_TxPending`
の保留・次回満了時送信という一連の流れ）を確認する目的で、Com の標準機能として
実装しています。

## ComTxModeNumberOfRepetitions（変化時送信の冗長再送）

DIRECT モードの I-PDU は、値が変化した瞬間に1回だけ送信します。しかし実
AUTOSAR の Com は、送信1回だけでは CAN バスの過渡的な輻輳やノイズでフレームを
1本丸ごと失うリスクがあると考え、`ComTxModeNumberOfRepetitions`
（`ComTxModeRepetitionPeriod` と対）という冗長送信の仕組みを用意しています
（[SWS_Com_00305]: "If `ComTxModeNumberOfRepetitions` is configured to a value
greater than 0 the Com module shall call `PduR_ComTransmit` ... in a cycle time
of `ComTxModeRepetitionPeriod` until `ComTxModeNumberOfRepetitions`+1 successful
confirmations have been received."）。`0`（既定）なら通常どおり1回だけ送信し
ます（[SWS_Com_00467]）。新たな送信要求は進行中の再送シーケンスをキャンセル
して再スタートし（[SWS_Com_00279]）、I-PDU Group の停止も再送シーケンスを
キャンセルします（[SWS_Com_00392]）。

これまでの `Com_IPduConfigType`/`Com.c` にはこの機構が一切存在せず、
`Com_MainFunction()` の DIRECT/MIXED 送信判定は「変化トリガー || MIXED
周期フロア」のみでした。今回、`ImmobilizerStatus`（IPduId=3、CAN 0x230）に
適用しました。

```
ImmobilizerStatus (IPduId=3):
  NumberOfRepetitions = 2U
  RepetitionPeriodMs  = COM_TX_REPETITION_PERIOD_IMMOBILIZERSTATUS_MS（既定 100ms）

Com_RequestTxOnChange()（Com_SendSignal()/Com_SendSignalGroup() 共通の
送信要求トリガー、Signal Gateway が ImmobilizerCmd 受信のたびに呼ぶ）:
  Com_TxPending[ImmobilizerStatus] = 1
  Com_TxRepeatsRemaining[ImmobilizerStatus] = NumberOfRepetitions（無条件上書き）

Com_MainFunction()（100ms 周期）:
  changeDue = Com_TxPending[...] && mdtElapsed
  repeatDue = (TxModeMode==DIRECT) && (Com_TxRepeatsRemaining[...] > 0)
              && (経過時間 >= RepetitionPeriodMs)
  due       = changeDue || floorDue || repeatDue

  due なら:
    Com_TxPending[...] = 0; Com_TxLastSentMs[...] = now
    TxEnabled==0 なら Com_DoTransmit() を呼ばずに次の I-PDU へ
    （このとき Com_TxRepeatsRemaining は減らさない）
    repeatDue && !changeDue のときのみ Com_TxRepeatsRemaining[...] -= 1
    Com_DoTransmit()（実送信）

結果: t=0（変化検知）で初回送信、t=100ms/200ms で再送、
      計3回（NumberOfRepetitions+1）送信して停止する。
```

**DIRECT モード限定にした理由**: MIXED の周期フロア（`floorDue`）や TMS
（`TxModeModeTrue`/`TxPeriodMsTrue`）との相互作用を避けるためです。仮に
`WarningStatus`（DIRECT だが TMS で MIXED へ遷移しうる）に誤って
`NumberOfRepetitions` を設定してしまうと、TMS 中の周期フロアと再送タイマーが
同じ `Com_TxLastSentMs` を取り合いながら無調整で二重に発火しかねません。
これを設定規約だけに頼らず、`Com_MainFunction()` 側で
`mode == COM_TX_MODE_DIRECT` の実行時ガードとしてコードにも担保しています
（`Com.c` の `Com_CbkRxAck`（[SWS_Com_00555]）実装にある
`ipdu->IsSignalGroup != 0U` という「設定判別フィールドに対する実行時ガード」
と同じ考え方です）。

**残り再送回数のデクリメントを、確認 (Com_TxConfirmation) ではなく
Com_MainFunction() の dispatch 時点で行う理由**: [SWS_Com_00305] の原文は
「確認 (`Com_TxConfirmation`) が `NumberOfRepetitions`+1 回届くまで」再送する、
という書き方です。しかし本コードベースでは `Can_Write()` は TX 確認を
即座には呼ばず、`swPduHandle` を保留キューへ積むだけで、実際の
`CanIf_TxConfirmation()` 呼び出しは **別タスク** `Can_MainFunction_Write()`
（1ms 周期）がキューをドレインするタイミングで行われます（`Can.c` 冒頭コメント
参照）。`Com_MainFunction()` は 100ms 周期の別タスクのため、「確認が届くまで
待つ」ロジックを `Com_MainFunction()` 単独では組めません。また、確認到達の
たびに単純にデクリメントする素朴な実装は、初回送信の確認も1回としてカウント
してしまうため、`NumberOfRepetitions+1` 回ではなく `NumberOfRepetitions` 回
しか送信されずに止まってしまうオフバイワンの不具合になります。そのため、
`Com_MainFunction()` が実際に `Com_DoTransmit()` を呼んだ時点で
`Com_TxRepeatsRemaining[]` を減らす設計にしています。本コードベースでは
送信失敗（`E_NOT_OK`）の経路が実質存在しない（Com_TxConfirmation() の
`\note` 参照）ため、この簡略化による実質的な挙動差はありません。

**dispatch 時点デクリメントに残っていた2つの不具合（`/code-review` で発見・
是正、実装直後のセッションで修正）**:

1. **オフバイワン（初回送信が再送1回分として誤カウントされる）**:
   `repeatDue` は `changeDue`/`floorDue` と同じ `Com_TxLastSentMs`（直近の
   *実送信* からの経過時間）を基準にしています。新規送信要求
   （`Com_RequestTxOnChange()`）が来た時点ではこのタイムスタンプをリセット
   しない（MDT がここと同じ基準を使っており、要求時刻起点にすると MDT の
   意味が変わってしまうため）。そのため、前回の実送信から
   `RepetitionPeriodMs` 以上経ってから新しい変化が来ると——`ImmobilizerStatus`
   のようにまばらにしか変化しない I-PDU では通常の状況——その「初回」送信の
   時点で `changeDue` と `repeatDue` が偶然同時に真になり、単純に
   `repeatDue` だけを条件にデクリメントすると初回送信が再送1回分として
   誤って消費されてしまいます。結果、計3回ではなく2回で止まってしまい、
   `/simplify` 時点で追加したテストは `FakeMillis_Reset()` 直後に送信する
   （経過時間が常に 0）ケースしか検証していなかったため検出できませんでした。
   **対応**: デクリメント条件を `repeatDue && !changeDue`（純粋に再送だけが
   理由で dispatch した場合に限る）へ変更。回帰テスト
   `RepetitionSequence_OK_InitialSendDoesNotConsumeRepeatBudgetEvenWhenElapsedAlreadyExceedsPeriod`
   を追加。
2. **CommunicationControl 無効中に残り回数を空費する**: `Com_TxLastSentMs`
   の更新は `Com_TxEnabled==0` でも行われる（既存の SWS_Com_00777 対応と
   同じ扱い）ため、送信抑制中も `repeatDue` は周期的に真になり得ますが、
   `Com_DoTransmit()` 自体は呼ばれません。当初の実装はデクリメントを
   `Com_TxEnabled` チェックより前に置いていたため、抑制中に
   `Com_TxRepeatsRemaining` だけが空費され、抑制解除後に本来送るべき冗長
   送信が1本も残っていない、という事態になり得ました。安全上重要な通知を
   確実に届けるための機能が、診断ツールによる一時的な送信抑制で無力化されて
   しまっては本末転倒です。**対応**: デクリメントを `Com_TxEnabled==0` の
   早期 `continue` より後（＝実際に `Com_DoTransmit()` を呼ぶ直前）へ移動。
   回帰テスト
   `RepetitionSequence_OK_DoesNotConsumeBudgetWhileCommunicationControlDisabled`
   を追加。

上記2件はどちらも `Com_TxRepeatsRemaining[]` を「いつ減らすか」の条件だけの
問題で、`due` 自体の判定ロジックや `Com_RequestTxOnChange()` 側のリセット
ロジックには変更はありません。

**`Com_RequestTxOnChange()` 側のガード（もう1件の指摘）**: 上記とは別に、
`Com_RequestTxOnChange()` は当初 `mode != PERIODIC` であれば
（＝MIXED でも）`Com_TxRepeatsRemaining` を無条件にセットしていました。
消費側（`Com_MainFunction()` の `repeatDue`）は `TxModeMode==DIRECT` に
限定しているため、設定側だけがガードされておらず、「仮に TMS 対応の
I-PDU に誤って `NumberOfRepetitions` を設定してしまうと、MIXED の間に
セットされた残り回数が古いまま残り、後で TMS が DIRECT へ遷移した際に
古いタイマー基準で不意に再送が復活しかねない」という理論上の隙が
ありました。`Com_RequestTxOnChange()` 側も `mode == COM_TX_MODE_DIRECT`
のときのみセットし、それ以外では明示的に 0 へクリアするよう是正しました
（`Com_MainFunction()` 側のガードと対称）。`ImmobilizerStatus` は
`TmsContributor` を持つシグナルが無く本番では TMS 自体が起きないため、
これは本番設定では到達しない防御的な修正です（既存のTX I-PDU4本すべてが
既にこの防御を試験する専用フィクスチャの余地を使い切っているため
（`COM_TX_IPDU_MAX=4`）、専用の回帰テストは追加していません）。

**この機能は実際に発動するか**: 発動します。`ImmobilizerStatus` は Signal
Gateway 経由（RX `ImmobilizerCmd` の LOCK/UNLOCK）で送信要求が立つため、
UDS ツールで `ImmobilizerCmd` を送るたびに、CAN ID 0x230 のフレームが約
100ms 間隔で3回（初回+再送2回）送信されるのを CAN スニファ（Cangaroo 等）
や `uds_tester` の rx_monitor、既存の `Com_DoTransmit()` デバッグログ
（`TX iPdu=... [...]`）で確認できます。

**`Com_IpduGroupStop()` によるキャンセルは実機で発動するか**: 発動しません。
`ImmobilizerStatus` は `IpduGroupId=COM_IPDU_GROUP_NONE`（どの I-PDU Group にも
属さず常時有効）のため、`Com_IpduGroupStop()` の対象になることが構造的に
ありません（`WarningStatus` の `TxErrCbk` と同じパターン、上記参照）。
[SWS_Com_00392] への準拠自体は `Com_IpduGroupStop()` 内に実装済みで、
ユニットテスト（test-only setter `Com_Test_SetTxRepeatsRemaining()` を使い、
実際に `NumberOfRepetitions` を設定した停止可能グループの I-PDU を新規に
用意しなくても内部カウンタを直接注入して検証）のみが検証手段です。

## ComNotification拡張（Tx確定コールバック、Com_CbkTxAck）

これまでの `Com_TxConfirmation()` は PduR から送信完了を受け取ってログ出力する
だけで、それより上位（ASW/Rte）へは何も伝えていませんでした。実 AUTOSAR の
Com は、I-PDU の送信が実際に成功した際、そこに含まれるシグナル/Signal Group
ごとに個別のコールバック（`Com_CbkTxAck`、SWS_Com_00468。実車の RTE 生成名は
`Rte_COMCbkTAck_<sn>`/`<sg>`）を呼び出せます。`ComRxDataTimeoutAction`/
`ComDataInvalidAction` が RX 側の「値の異常」を扱う機能だったのに対し、これは
TX 側の「送信できたことの確認」を扱う機能です。

本プロジェクトでは `MeterStatus` の `EngineState`（非 Signal Group）と
`WarningStatus`（Signal Group）の両方に `TxAckCbk` を設定しました。

```
Com_SignalConfigType (EngineState):
  TxAckCbk = Rte_COMCbkTAck_EngineState

Com_TxConfirmation(TxPduId=0/*MeterStatus*/, result=E_OK)  ← PduR から呼ばれる
  IsSignalGroup==0 のため、Com_ConfigPtr->Signals[] を走査
    sig->Direction==TX かつ sig->IPduId == TxPduId かつ sig->TxAckCbk != NULL
    のものすべてについて sig->TxAckCbk() を呼ぶ  ← EngineState だけでなく、
                          同じ I-PDU の全 TX シグナルが対象（本設定では
                          EngineState のみ）

Com_IPduConfigType (WarningStatus_Tx):
  TxAckCbk = Rte_COMCbkTAck_WarningStatus

Com_TxConfirmation(TxPduId=1/*WarningStatus*/, result=E_OK)
  IsSignalGroup==1 のため、Signals[] は走査せず
  ipdu->TxAckCbk（Rte_COMCbkTAck_WarningStatus）をグループ単位で 1 回だけ呼ぶ
  （RunLamp/FaultLamp/AbsLamp のどのメンバーが送信を引き起こしたかは問わない）
```

**シグナル単位への統一を是正（2026-08）**: 実 AUTOSAR は signal 単位・signal
group 単位で別々のコールバック名（`Rte_COMCbkTAck_<sn>`/`<sg>`）を持てます
（SWS_Com_00468: "It can be configured for signals and signal groups"、
`ComNotification` = ECUC_Com_00498 は `ComSignal`・`ComSignalGroup` 双方に
独立したコンテナとして存在することを仕様書で確認済み）。当初の実装は
`TmsContributor`/`TransferProperty`/`RxDataTimeoutAction` 等これまでの
フィールドと同じ発想で、シグナル単位の `TxAckCbk` のみに統一していました。
Signal Group（`WarningStatus`）のメンバーに `TxAckCbk` を設定した場合、
`Com_ConfigPtr->Signals[]` を素直に走査するだけの実装だったため、そのメンバー
個別に（本来 1 回のはずが最大メンバー数回）呼ばれてしまう簡略化でした。

`/code-review` で「本番設定に Signal Group の `TxAckCbk` の実利用例が無く
未検証のまま」と指摘されたのを機に、`Com_IPduConfigType` にグループ単位の
`TxAckCbk` フィールドを新設し、`Com_TxConfirmation()` を「I-PDU が
Signal Group なら `Com_IPduConfigType.TxAckCbk` をグループ単位で 1 回、
そうでなければ従来どおりシグナル単位で走査」という分岐に是正しました。
あわせて `WarningStatus` に `Rte_COMCbkTAck_WarningStatus`（`MeterStatus`/
`EngineState` の `Rte_COMCbkTAck_EngineState` と対になる、Signal Group 単位の
実装例）を追加し、この経路が実際に発動することを実機で確認できるようにして
います。

**レビューで見つかった問題（RX/TX の方向誤認）**: 初期実装は
`sig->IPduId == TxPduId` だけで走査対象を絞っており、方向（RX/TX）を
確認していませんでした。RX I-PDU と TX I-PDU の `IPduId` は別々の値空間で
（どちらも 0 始まり）数値が重複します（例: RX の `EngineInfo=0` と TX の
`MeterStatus=0`）。そのため `Com_TxConfirmation(TxPduId=0, ...)`
（`MeterStatus` の送信確認）が呼ばれると、本来対象であるべき TX 側の
`EngineState` だけでなく、たまたま `IPduId=0` である RX 側の
`EngineInfo`（`EngineSpeed`/`CoolantTemp`/`EngineOnFlag`）まで走査対象に
入ってしまいます。当時はどの RX シグナルにも `TxAckCbk` が設定されていな
かったため実害はありませんでしたが、コード上は何も防いでおらず、将来
誰かが（コピペ等で）RX シグナルに `TxAckCbk` を設定してしまうと、無関係な
TX I-PDU の送信完了のたびにそのコールバックが静かに誤発火する
（クラッシュも DET ログも出ないため発見しづらい）バグになり得ました。

`Com_SendSignal()`/`Com_ReceiveSignal()` は `SignalId` で 1 エントリを検索
してから `Com_FindTxIPdu()`/`Com_FindRxIPdu()` で方向を確認する設計のため
この曖昧さの影響を受けませんが、`Com_TxConfirmation()` は逆に
「`Signals[]` 全体を `IPduId` だけで検索する」という、本実装で初めて
現れた走査パターンだったために問題が顕在化しました。

修正として `Com_SignalConfigType` に `Direction`（`Com_SignalDirectionType`:
`COM_SIGNAL_DIRECTION_RX`/`_TX`）フィールドを新設し、全 12 シグナルへ明示的に
設定したうえで、`Com_TxConfirmation()` の走査条件に
`sig->Direction == COM_SIGNAL_DIRECTION_TX` を追加しました。実 AUTOSAR には
対応パラメータがありません（ComSignal は必ずどちらか一方の ComIPdu に構造的に
含まれるため、そもそもこの曖昧さが発生しない）。本プロジェクトが RX/TX 共通の
1 本の配列に平坦化した簡略設計を採用したことで生じた、本プロジェクト固有の
補正です。

**このコールバックはこの送信でシグナル自体が変化したかどうかを問わない**:
`Com_CbkTxAck` は「I-PDU の送信が成功した」ことの通知であり、含まれる個々の
シグナルの値がこの送信で変化したかどうかとは無関係です（SWS_Com_00468:
"called immediately after successful transmission of the I-PDU containing
the message"）。`EngineState` が変化していなくても、`MeterStatus` が周期
フロア（MIXED モード）で再送されるたびに `Rte_COMCbkTAck_EngineState()` が
呼ばれます。

**割り込み安全性について（前節の教訓を踏まえた確認）**: `Com_TxConfirmation()`
は `Can_MainFunction_Write()`（Os の 100ms タスク）→ `CanIf_TxConfirmation()`
→ `PduR_CanIfTxConfirmation()` という経路で同期的に呼ばれます。前節の
`ComInvalidNotification` とは異なり、この経路上には SchM 排他エリア
（割り込み禁止区間）が存在しないことを実際にコードを辿って確認済みです。
したがって `TxAckCbk` の中で `DET_LOGI` のような Serial 出力を行っても、
前節の WDT リセット障害と同じ問題は起きません。

**この機能は実際に発動するか**: 発動します。`MeterStatus` は `TxModeMode=MIXED`
（値変化時の即時送信 + 周期フロア再送）のため、通常運用で確実に送信され続け、
そのたびに `Com_TxConfirmation()` → `Rte_COMCbkTAck_EngineState()` が呼ばれます。
ログに `Com: TxConf id=0` の直後に `Rte: MeterStatus TX ack (EngineState)`
が出力されることを実機で確認できます。`WarningStatus` も RunLamp（エンジン
稼働中は常時 1）の変化だけで通常運用中に送信され続けるため同様に発動し、
`Com: TxConf id=1` の直後に `Rte: WarningStatus TX ack (group)` が
（RunLamp/FaultLamp/AbsLamp どれが変化したかによらず）1 回だけ出力されます。

**対になる `TxErrCbk`（Com_CbkTxErr）も同時に是正（2026-08）**: `Com_CbkTxAck`
と全く同じ理由で、`Com_CbkTxErr`（SWS_Com_00491: "corresponds to
Rte_COMCbkTErr_<sn> or Rte_COMCbkTErr_<sg> respectively"）も signal 単位/
signal group 単位で別名を持てます。`Com_IpduGroupStop()`（送信済み・未確認の
まま I-PDU Group が停止された場合に発火、[SWS_Com_00479]/[SWS_Com_00491]）も
`TxAckCbk`と同じくメンバーシグナル単位で走査する簡略実装だったため、
`Com_IPduConfigType` にグループ単位の `TxErrCbk` を追加し、`TxAckCbk` と同じ
`IsSignalGroup` 分岐で是正しました。

**この機能は実際に発動するか**: 発動しません。`Com_IpduGroupStop()` の対象に
なるのは `IpduGroupId != COM_IPDU_GROUP_NONE` の I-PDU のみですが、本設定で
実際に I-PDU Group（`COM_IPDU_GROUP_TELEMETRY`）に所属するのは
`E2EHealthStatus`（非 Signal Group）だけで、唯一の Signal Group である
`WarningStatus` は `IpduGroupId=COM_IPDU_GROUP_NONE`（常時有効）です。
つまり本プロジェクトの現在の構成では、Signal Group の `TxErrCbk` が実際に
呼ばれる経路が構造的に存在しません（`WarningStatus` を停止可能な I-PDU
Group へ所属させる設定変更が必要ですが、それ自体が「常時有効なダッシュボード
表示」という意図と反するため見送っています）。`ComRxDataTimeoutAction` の
REPLACE 等と同じく、動機は実利より仕様忠実性であり、修正の正しさは
`test/test_chain/Bsw_TxChain_test.cpp` のユニットテスト（Signal Group 用の
I-PDU Group を持つテスト専用設定で `Com_IpduGroupStop()` を直接呼ぶ）でのみ
検証しています。

**共通配送ロジックへの集約（2026-08、`/code-review` 指摘）**: `TxAckCbk`
（`Com_TxConfirmation()`）と `TxErrCbk`（`Com_IpduGroupStop()`）は「Signal
Group ならグループ単位で 1 回、そうでなければ TX シグナル単位で走査」という
配送ロジックが完全に同型であるにもかかわらず、対応する 2 つの関数へ別々に
実装していました。レビューで「将来 3 つ目の類似コールバックを追加する際、
片方だけ修正して他方を直し忘れるリスクがある」と指摘され、`Com.c` 内の
`Com_InvokeTxNotification()` ヘルパーへ集約しました。呼び出し元は
`COM_TX_NOTIFY_ACK`/`COM_TX_NOTIFY_ERR`（`Com_TxNotifyKindType`）を渡すだけ
になり、配送ロジック自体は 1 箇所のみで保守します（挙動そのものは変わって
いません）。

当初は「シグナルから対象コールバックを取り出す」役割を関数ポインタ経由の
アクセサ（`Com_GetSignalTxAckCbk`/`Com_GetSignalTxErrCbk`）で抽象化していま
したが、`/simplify` の Altitude/Simplification 両観点から独立に「呼び出し
箇所 2 つ・選択肢 2 つの重複解消にしては過剰で、本ファイルの他の箇所
（`Com_TmsState[ipduId] ? ipdu->TxModeModeTrue : ipdu->TxModeMode` 等）が
使う直接フィールドアクセス＋三項演算子という既存スタイルと不整合」と指摘
され、`Com_TxNotifyKindType` 列挙型＋三項演算子（`(kind==COM_TX_NOTIFY_ACK)
? sig->TxAckCbk : sig->TxErrCbk`）の形へ整理しました。関数ポインタ抽象化は
「選択肢が実際に複数種類あり、かつ呼び出し側が種類を知らないまま扱いたい」
場合にのみ正当化されるのであって、単に「2 択を 1 箇所にまとめたい」だけなら
過剰である、という教訓です。

**追記（TX 送信デッドライン監視、後述）**: 上で予告した「将来3つ目の類似
コールバックを追加する際」が実際に起きました。`Com_CbkTxTOut`
（`TxTOutCbk`）の追加に伴い `Com_TxNotifyKindType` へ `COM_TX_NOTIFY_TOUT`
を加え、`Com_InvokeTxNotification()` 内の三項演算子2箇所を switch 文へ
書き換えています。3種とも呼び出し側がコンパイル時に知っている固定種別で
あることは変わらないため、関数ポインタテーブルのような抽象化は依然として
過剰と判断し、switch 文への置き換えに留めました（上記の教訓の延長）。
詳細は「TX 送信デッドライン監視」節参照。

## ComNotification拡張（Rx確定コールバック、Com_CbkRxAck）

`Com_CbkTxAck`/`Com_CbkTxErr` が TX 側の「送信できたこと/できなかったこと」の
通知だったのに対し、RX 側にも対になる通知があります。`Com_CbkRxAck`
（SWS_Com_00555: "This callback represents notification class 1 ... It is
called immediately after the message has been stored in the receiving
message object. ... It can be configured for signals and signal groups.
Com_CbkRxAck corresponds to Rte_COMCbk_<sn> or Rte_COMCbk_<sg>
respectively."）は、Com が受信バッファへバイト列を格納した直後に呼ばれる
通知で、`ComNotification`（ECUC_Com_00498）は TX 側と同じ設定コンテナが
`ComSignal`・`ComSignalGroup` 双方に独立して存在します。

本プロジェクトは 2026-08 の TxAckCbk/TxErrCbk 是正の後にこちらを調査し、
**シグナル単位・シグナルグループ単位いずれの `Com_CbkRxAck` も一切実装
していなかった**ことが判明しました（既存の `Com_IPduConfigType.
RxIndicationCbk` は E2E 検証・SecOC 連携・Signal Gateway 起点として使う
本プロジェクト独自の I-PDU 単位汎用フックであり、実 AUTOSAR の
`Com_RxIpduCallout`（真偽値を返し受理/拒否できる別機構、`ComIPduCallout`）
とも `Com_CbkRxAck` とも異なるものです）。TX 側は既存のシグナル単位
`TxAckCbk` にグループ単位を追加しただけでしたが、RX 側はシグナル単位・
グループ単位とも新設しています。

`EngineInfo` の `EngineOnFlag`（非 Signal Group）と `AbsInfo`（Signal
Group）の両方に `RxAckCbk` を設定しました。

```
Com_SignalConfigType (EngineOnFlag):
  RxAckCbk = Rte_COMCbk_EngineOnFlag

Com_RxIndication(RxPduId=0/*EngineInfo*/, ...)  ← PduR から呼ばれる
  バッファ格納後、シグナル単位デッドライン監視リセットループの中で
  （下記「共通配送ロジックへの統合を見送った理由」参照）
    sig->Direction==RX かつ sig->IPduId==0 かつ
    このシグナルの全ビット範囲が recvLen 以内（Com_SigTimedOut リセットと
    同じ条件） かつ ipdu->IsSignalGroup==0 かつ sig->RxAckCbk != NULL
    のものすべてについて sig->RxAckCbk() を呼ぶ

Com_IPduConfigType (AbsInfo_Rx):
  RxAckCbk = Rte_COMCbk_AbsInfo

Com_RxIndication(RxPduId=1/*AbsInfo*/, ...)
  IsSignalGroup==1 のため、シグナルループへ入る前に
  ipdu->RxAckCbk（Rte_COMCbk_AbsInfo）をグループ単位で 1 回だけ呼ぶ
  （VehicleSpeed/BrakeActive/AbsActive のどれが変化したかは問わない）
```

**なぜ `Com_RxIndication()` 内、`RxIndicationCbk` より前に発火させるか**:
`AbsInfo` のような RX Signal Group では、シャドウバッファへの確定コピーを
行う `Com_ReceiveSignalGroup()` は `RxIndicationCbk`（`Rte_COMRxInd_AbsInfo()`）
の内部から、**E2E 検証成功後にのみ**呼ばれます。つまり E2E 検証に失敗した
フレームでは一度も呼ばれません。ここに `Com_CbkRxAck` を紐付けると、
「バッファに格納した」という Com 自身の事実と無関係に、上位層（E2E）の
都合で通知が抑制されてしまい、SWS_Com_00555 の原文
（"immediately after the message has been stored"）に反します。そのため
`Com_RxIndication()` 自身の生バッファ書き込み（`Com_RxBuffer` への
コピー）を根拠にし、`RxIndicationCbk` より前の時点で発火させています。
Signal Group は短フレーム破棄（[SWS_Com_00575]、受信長が DLC 未満なら
グループ全体を丸ごと不採用）が生バッファ書き込みより前に判定されるため、
この発火時点に到達していれば必ずグループ全体が格納済みであり、
グループレベルでの発火に曖昧さはありません。

**命名の衝突と解消（2026-08 是正）**: 実 AUTOSAR が生成する RTE 関数名は
`Rte_COMCbk_<sn>`/`<sg>` ですが、当初この名前は既に `RxIndicationCbk`
向けに使われていました（旧 `Rte_COMCbk_EngineInfo`/`Rte_COMCbk_AbsInfo`/
`Rte_COMCbk_SecureCommand`、いずれも E2E 検証等を行う別の汎用フック）。
そのまま実 AUTOSAR の命名規則に従うと、`AbsInfo` のように Signal Group 名と
I-PDU 名が一致するケースで、全く別の意味を持つ 2 つのコールバックが同じ
関数名を要求することになり衝突します。当初はこれを避けるため `TxAckCbk`
（`Rte_COMCbkTAck_<sn>/<sg>` の代わりに `Rte_COMTxAck_*`）と同じ発想で、
RX 側も `Rte_COMRxAck_<name>` という独自命名で回避していました。

これは AUTOSAR IF のシグネチャ（関数名）を仕様書に一致させたいという方針の
もとで是正しました。衝突の根本原因は `RxIndicationCbk` 側が
`Com_CbkRxAck`/`Com_CbkTxAck` 等とは異なり実 AUTOSAR の標準コールバックでは
ない（本プロジェクト独自の I-PDU 単位汎用フック）ことなので、**衝突を
避けるために名前を変えるべきは `RxIndicationCbk` 側**と判断し、こちらを
`Rte_COMRxInd_<name>`（`Rte_COMRxInd_EngineInfo`/`Rte_COMRxInd_AbsInfo`/
`Rte_COMRxInd_SecureCommand`）へ改名しました。これにより空いた
`Rte_COMCbk_<sn>`/`<sg>` を `Com_CbkRxAck` 側が仕様どおりに使えるように
なり（`Rte_COMCbk_EngineOnFlag`/`Rte_COMCbk_AbsInfo`）、あわせて
`TxAckCbk`/`TxTOutCbk` も仕様どおりの `Rte_COMCbkTAck_<sn>/<sg>`/
`Rte_COMCbkTxTOut_<sn>/<sg>` へ改名しています（詳細・全対応表は本節末尾の
「AUTOSAR IF シグネチャ整合（2026-08）」参照）。

**部分受信時のゲーティング**: 非 Signal Group シグナルの `RxAckCbk` は、
そのシグナルの全ビット範囲が実際に受信できたバイト数（`recvLen`）以内に
収まっている場合のみ呼ばれます（[SWS_Com_00574]、`Com_RxIndication()` 内の
`Com_SigTimedOut` リセット判定と全く同じ `lastByte <= recvLen` 判定式を
共有）。範囲外だったシグナルは「受信した」とみなさないという既存の基準を
そのまま踏襲しています。部分受信自体は、`CanIf` の設定 DLC が本プロジェクト
の全 RX I-PDU で Com の設定 DLC と常に一致しているため、実機では到達しない
経路です（`Com_RxIndication()` の doc コメント参照）。この機能に固有の
ロジック（`lastByte <= recvLen` によるゲーティング）は、`Com_RxIndication()`
を直接呼ぶユニットテストでのみ検証しています（詳細は下記）。

**この機能は実際に発動するか**: 発動します（`TxErrCbk` とは異なります）。
`EngineInfo`/`AbsInfo` はいずれも通常運用で継続的に受信されるデッドライン
監視対象の RX I-PDU のため、フレームを受信するたびに確実に発火します。
ログに `Com: RX iPdu=0 [...]` の直後に `Rte: EngineInfo RX ack
(EngineOnFlag)`、`Com: RX iPdu=1 [...]` の直後に `Rte: AbsInfo RX ack
(group)` が出力されることを実機で確認できます。`RxIndicationCbk` より前に
呼ばれるため、E2E CRC が壊れているフレームでもこれらのログは出力されます
（続く E2E 検証ログとセットで見ることで、「バイト列は届いたが妥当性検証は
別軸」という設計を実機ログ上でも確認できます）。

**共通配送ロジックへの統合を見送った理由**: TX 側の
`Com_InvokeTxNotification()`（`Com_TxNotifyKindType` で ACK/ERR を判別する
共通ヘルパー）とは統合していません。RX 側には呼び出し元が
`Com_RxIndication()` の 1 箇所のみ、通知の種類も `RxAckCbk` のみで、
判別すべき「2 択」がそもそも存在しないためです。

**専用ヘルパー関数として独立させることも見送った経緯（2026-08、
`/code-review` 指摘）**: 当初の実装は `Com_InvokeRxAck()` という専用の
static 関数を新設し、`Com_RxIndication()` から `RxIndicationCbk` の直前で
呼ぶ形にしていました。しかしこの関数の非 Signal Group 分岐は、
`Com_RxIndication()` 側に既にある「シグナル単位デッドライン監視リセット」
ループ（`Com_SigTimedOut[s]=0U` を `lastByte <= recvLen` の条件で行う
ループ）と、フィルタ条件（`Direction==RX && IPduId==ipdu->IPduId`）も
`lastByte` の計算式も完全に同一の、**2 本目の全シグナル走査**でした。
CAN RX フレームを受信するたびに `Signals[]` を 2 回走査することになり、
かつ「完全に受信できたか」（SWS_Com_00574）という同じ判定ロジックが
2 箇所に存在するため、将来どちらか一方だけを修正すると
`Com_SigTimedOut` リセットと `RxAckCbk` 発火の基準が静かにずれる、という
指摘を受けました。

対応として `Com_InvokeRxAck()` を廃止し、その処理を
「Signal Group ならループに入る前にグループ単位で 1 回呼ぶ」＋
「シグナル単位デッドライン監視リセットループ自体に `RxAckCbk` 呼び出しを
組み込む（`lastByte <= recvLen` の判定を使い回す）」という形で
`Com_RxIndication()` 本体へ統合しました。これにより全シグナル走査は
1 回のみになり、「完全に受信できたか」の判定ロジックも 1 箇所に
一本化されています。

## Update Bit（送信側が実際に更新したかを示す1ビット）

これまでの機能はいずれも「値そのもの」（無効値パターン、タイムアウト、送信成功）
に関するものでした。update-bit（実 AUTOSAR 7.8 章）はこれらとは別の軸で、
「送信側がこのシグナル/シグナルグループを実際に更新して送ったかどうか」を示す
1 ビットです（SWS_Com_00055: シグナル/グループの値そのものとは独立に、Com が
内部でのみ扱う）。共有 I-PDU に複数の送信元・複数のシナリオが値を書き込みうる
構成や、周期送信と変化時送信を併用する MIXED モードで、「このフィールドは今回の
フレームで本当に新しい値が入っているか」を受信側が区別したい場合に使います。

実 AUTOSAR の `ComUpdateBitPosition`（ECUC_Com_00257）は `ComSignal`
（非 Signal Group の単一シグナル、SWS_Com_00061）・`ComSignalGroup`
（グループ全体、SWS_Com_00801）双方に同名パラメータとして存在します。本実装は
これを `Com_IPduConfigType.UpdateBitPosition` という I-PDU 単位のフィールド 1 個に
簡略化していますが、値のセット元が「非 Signal Group なら `Com_SendSignal()`
（SWS_Com_00061）」「Signal Group なら `Com_SendSignalGroup()`（SWS_Com_00801）」の
どちらであっても、クリア（`Com_DoTransmit()`）は同じコードで扱います。

現状の適用状況:

- **`MeterStatus`/`EngineState`（TX、非 Signal Group）: 適用済み・実機検証済み**。
  `UpdateBitPosition=8`（byte[1] bit0）を設定しています。`TxModeMode=MIXED`
  のため、「今回の送信は実際の値変化によるものか、単なる周期フロア再送か」を
  受信側が区別できる、という update-bit 本来の動機がそのまま当てはまる例です。
- **`WarningStatus`（TX、Signal Group）: 適用済み・実機検証済み**。
  `UpdateBitPosition=3`（byte[0] bit3。RunLamp/FaultLamp/AbsLamp が使う
  bit0-2 の次の空きビットのため DLC 拡張は不要）を設定しています。
  TMS=true（FAULT/ABS 点灯中）時の MIXED 周期フロア再送と、実際に警告灯が
  変化したことによる送信とを区別できる、Signal Group 単位の実装例です。

```
送信側 非 Signal Group（Com_SendSignal）:
  実バッファへ反映（既存処理）
  ComFilterAlgorithm が「変化あり」と判定した場合のみ、
  UpdateBitPosition が 0xFF 以外ならそのビットをセット
  （SWS_Com_00061。本実装独自の条件付け、詳細は次項）
送信側 Signal Group（Com_SendSignalGroup）:
  実バッファへ反映（既存処理）
  Com_GroupTriggerPending（TRIGGERED_ON_CHANGE メンバーが実際に変化したか）
  が立った場合のみ、UpdateBitPosition が 0xFF 以外ならそのビットをセット
  （SWS_Com_00801。こちらも本実装独自の条件付け、詳細は次項）
送信側（Com_DoTransmit、Com_MainFunction() から。Signal Group かどうかを問わない）:
  PduR_Transmit() で実送信（この時点でビット=1 のまま送信される）
  ret==E_OK のときのみ、送信直後にビットをクリア
  （SWS_Com_00062: ComTxIPduClearUpdateBit=Transmit）
```

**動作確認方法**: `uds_tester` の「EngineStatus (0x200)」rx_monitor ボタンに
`upd=0/1` の表示を追加しました。ダッシュボードの RUNNING/STARTING/FAULT 等の
状態遷移で `EngineState` が変化した直後は `upd=1`、その後
`COM_TX_PERIOD_METERSTATUS_FLOOR_MS`（既定 9000ms）間隔で値が変化せずに再送
される周期フロアフレームでは `upd=0` になることを確認できます。同様に
「WarningStatus (0x210)」rx_monitor ボタンにも `upd=0/1` の表示を追加しました。
FAULT/ABS 点灯による TMS=true（MIXED 切り替え）中、警告灯が実際に変化した
送信は `upd=1`、`COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS` 間隔の周期フロア
再送は `upd=0` になります。Cangaroo で CAN 0x200 の byte[1] bit0（生の 2 バイト
目、MSB）・CAN 0x210 の byte[0] bit4 を直接観察することでも同様に確認できます。

**レビューで見つかった問題（非 Signal Group の update-bit が常に 1 のままだった）**:
初期実装は SWS_Com_00061 の原文どおり「`Com_SendSignal()` が呼ばれるたびに
無条件でセットする」としていましたが、実機検証で `upd` が常に `1` のままになる
不具合が見つかりました。

原因は ASW（`App_EngineManager_Run()`）の呼び出しパターンとの相互作用です。
本プロジェクトの ASW は「値が変わったかどうか」を判定せず、毎サイクル無条件に
`Rte_Write_EngineStatus_EngineState()`（→ `Com_SendSignal()`）を呼び、実際に
送信するかどうかの判断は Com の `ComFilterAlgorithm` に完全に委ねる設計です
（前述「責務分離の効果」）。SWS_Com_00061 を文字どおり実装すると、実送信
（イベント駆動・周期フロアいずれも）から次の実送信までの間に、ASW が
`Com_SendSignal()` を何度も（無変化のまま）呼び続けるため、`Com_DoTransmit()`
がクリアした直後には必ず再セットされてしまい、update-bit が「実際に変化した
かどうか」を一切表せなくなっていました。

対策として、非 Signal Group の update-bit セットを `ComFilterAlgorithm` の
「変化あり」判定（`passesFilter`、`Com_RequestTxOnChange()` を呼ぶかどうかと
同じ判断軸）に条件づけました。ASW 側の「常に書き込む」設計はそのまま変えず、
Com 側が既に持っている「これは実際の変化か」の判断をそのまま update-bit にも
使い回す形です。これは SWS_Com_00061 の文字どおりの実装ではありませんが、
本プロジェクトの ASW 呼び出し規約のもとで update-bit 本来の目的（実際に更新
されたかどうかを示す）を満たすための意図的な調整です。

Signal Group 側（`Com_SendSignalGroup()`）も同種の問題を抱えていました。
`App_WarningIndicator_Run()` も毎サイクル無条件に
`Rte_SendSignalGroup_WarningStatus()`（→ `Com_SendSignalGroup()`）を呼ぶ同じ
設計のため、`WarningStatus` に `UpdateBitPosition` を設定した時点で
（非 Signal Group と同じ理由により）update-bit が常に 1 のままになることは
実装前から予見できました。そのため `WarningStatus` へ update-bit を適用する
のと同時に、`Com_SendSignalGroup()` 側も対策済みです: セットの条件を
`Com_GroupTriggerPending`（`ComTransferProperty=TRIGGERED_ON_CHANGE` の
メンバーが実際に変化したかどうか、`Com_RequestTxOnChange()` を呼ぶかどうかと
同じ判断軸）に条件づけました。非 Signal Group 側の `passesFilter` と対になる
考え方です。

**なぜ ComTxIPduClearUpdateBit=Transmit のみか**: 実 AUTOSAR は Transmit
（`PduR_ComTransmit` 呼び出し直後にクリア）/ Confirmation（送信確認後にクリア）/
TriggerTransmit（トリガ送信後にクリア）の 3 択ですが、本実装は Transmit のみ
実装しています。本プロジェクトの送信経路は `Com_MainFunction()` →
`Com_DoTransmit()` → `PduR_Transmit()` が同期的に SPI 送信まで完了する
（実際の送信完了と `PduR_ComTransmit` 呼び出しの間に有意な時間差がない）ため、
3 つのタイミングの違いを意味のある形で再現できないという判断です。

**レビューで見つかった問題（update-bit クリアが送信結果を無視していた）**:
このコード自体は、実際に `WarningStatus` へ試験的に適用していた開発中の段階で
見つかった問題を修正済みです。初期実装は `Com_DoTransmit()` 内で
`PduR_Transmit()` の戻り値 `ret` を見ずに update-bit を無条件でクリアして
いました。しかし本節が引用する SWS_Com_00062 の原文は "after this I-PDU was
sent out via PduR_ComTransmit **and PduR_ComTransmit returned E_OK**" であり、
送信成功時のみクリアすべきと明記されています。当時のコードコメントは
「戻り値によらず無条件でクリアする」と、この食い違いを自覚的に開示しないまま
断定していました。

具体的な失敗シナリオ（`WarningStatus` に適用していた場合を想定）: この I-PDU
は `TxModeModeTrue=MIXED`（TMS=true 時に周期フロア再送あり）のため、
(1) `FaultLamp` 点灯で `Com_SendSignalGroup()` が update-bit をセット、
(2) バス輻輳等で最初の送信が失敗（`ret=E_NOT_OK`）しても update-bit だけが
クリアされてしまう、(3) データ自体はバッファに残ったまま次の周期フロアで
再送され、そちらは成功する——という流れになると、実際には初めて正常配信
された新データにもかかわらず、受信側から見ると update-bit が 0（＝「未更新」）
のフレームとして届いてしまいます。

同じファイル内の `Com_TxConfirmation()` は `if (result != E_OK ...) return;`
と正しく結果をチェックしており、この判定パターン自体は目新しいものでは
ありませんでした。修正は `if (ret == E_OK && ...)` という条件の追加のみです。

この修正は `MeterStatus`（非 Signal Group）の送信経路でも共通に使われるため、
Signal Group への適用有無に関わらず活きています。

## RX Signal Group（複数シグナルの一貫したスナップショット読み出し）

ここまでの Signal Group（`Com Signal Group`/`ComTransferProperty` 節）は TX 側の
話でした。実 AUTOSAR の Com は RX 側にも対称の仕組みを持ちます:
`Com_ReceiveSignalGroup()`（SWS_Com_00201）が I-PDU バッファの内容を
「RX シャドウバッファ」へアトミックにコピーし（SWS_Com_00051/00638）、
以降の `Com_ReceiveSignal()` 呼び出しはこのシャドウバッファを読みます。
これにより、同じグループに属する複数シグナルを別々の `Com_ReceiveSignal()`
呼び出しで読む間に新しいフレームが届いても（`Com_RxIndication()` が
`Com_RxBuffer` を上書きしても）、読み取り側は常にこの呼び出し時点の一貫した
スナップショットを見ます。

本プロジェクトでは `AbsInfo`（RX IPduId=1、`VehicleSpeed`/`BrakeActive`/`AbsActive`
の 3 シグナル）に `IsSignalGroup=1` を設定し、これを実装しました。

```
Com_RxIndication(AbsInfo)  ← CAN フレーム受信のたびに呼ばれる
  Com_RxBuffer[1] を更新、Com_RxTimedOut[1] = 0 にリセット
  RxIndicationCbk（Rte_COMRxInd_AbsInfo）を呼ぶ

Rte_COMRxInd_AbsInfo()
  Com_ReceiveSignalGroupArray(1, buf) で生バイト列を取得 → E2E 検証
  検証 OK なら:
    Com_ReceiveSignalGroup(1U)
      Com_RxBuffer[1] を Com_RxShadowBuffer[1] へコピー（確定コピー）
      Com_RxShadowTimedOut[1] = Com_RxTimedOut[1]（この時点は 0）
    Com_ReceiveSignal(VEHICLE_SPEED, ...)  ← Com_RxShadowBuffer[1] から読む
    Com_ReceiveSignal(BRAKE_ACTIVE,  ...)  ← 同上
    Com_ReceiveSignal(ABS_ACTIVE,    ...)  ← 同上
```

**なぜ TX 側と対称の実装にしたか**: `Com_SendSignalGroup()`（TX 側）と同じ
「シャドウバッファ経由でしか実データへ触れない」設計を RX 側にも適用することで、
Signal Group という概念が送受信どちらの向きでも同じ形（アトミックなコミット/
コピーの単位）で表現できることを確認する目的です。実装は
`Com_FindTxIPdu()`/`Com_TxShadowBuffer` に対する `Com_FindRxIPdu()`/
`Com_RxShadowBuffer` というほぼ 1 対 1 の対称構造になっています。

**タイムアウト判定もスナップショットする理由**: `Com_ReceiveSignal()` が
グループメンバーに対して見るのは、ライブの `Com_RxTimedOut[]` ではなく
`Com_ReceiveSignalGroup()` 実行時点でスナップショットした
`Com_RxShadowTimedOut[]` です。ライブの値を都度参照すると、同じグループの
複数メンバーを読む間にデッドライン監視タイムアウトが発生した場合、
1 本目は成功・2 本目は `E_NOT_OK` という不整合が起こり得ます（せっかく
データはアトミックにコピーしても、可否判定だけが後から変わってしまう）。
スナップショットすることで、データと可否判定を 1 つの一貫した単位として
扱えます。

**この実装で実際に不整合が起きる場面はあるか**: 正直に言うと、現状はありません。
`Com_ReceiveSignalGroup(1U)` の呼び出しと、それに続く 3 回の `Com_ReceiveSignal()`
呼び出しはすべて `Rte_COMRxInd_AbsInfo()` という 1 つの同期呼び出し列の中で行われ、
かつこの関数自体が `Com_RxIndication()` からフレーム受信直後・`Com_RxTimedOut`
リセット直後に呼ばれます。したがってこのシーケンスの実行中にタイムアウトが
新規発生することはなく（本実装は非プリエンプティブなため、他のタスクがこの間に
割り込むこともありません）、シャドウバッファなしでも同じ結果になります。
`TMS`/`MDT`/`ComTransferProperty` と同じく、動機は実利より仕様忠実性です。
シャドウバッファが実際に意味を持つのは、ASW Runnable が `Com_ReceiveSignal()`
をフレーム受信タイミングとは無関係な任意のタイミングで直接呼ぶような構成
（本プロジェクトの E2E Transformer 方式とは異なり、RTE ミラーを経由しない構成）
です。

## ComRxDataTimeoutAction（受信タイムアウト時のシグナル値の扱い）

RX 受信デッドライン監視（次節）がタイムアウトを検出したあと、`Com_ReceiveSignal()`
がそのシグナルに対してどう振る舞うかは、実 AUTOSAR では `ComRxDataTimeoutAction`
（ECUC_Com_00412）で設定できます: 何もしない（NONE）、`ComSignalInitValue`
（起動時の初期値）で置き換える（REPLACE）、`ComTimeoutSubstitutionValue`
（このシグナル専用の代替値）で置き換える（SUBSTITUTE）の 3 択です。SUBSTITUTE の
根拠要求は対象が非グループシグナルか Signal Group メンバーかで異なり、前者は
SWS_Com_00875、後者（今回の VehicleSpeed）は SWS_Com_00876 です。

本プロジェクトはこれまで NONE 相当の動作（`Com_ReceiveSignal()` が値を書き込まず
`E_NOT_OK` を返すだけ）のみをサポートしており、「タイムアウト中は安全な代替値を
返す」判断は常に呼び出し元（ASW/RTE）任せでした。これに `COM_RX_TIMEOUT_ACTION_SUBSTITUTE`
を追加し、`AbsInfo` の `VehicleSpeed`（RX Signal Group メンバー）に適用しました。

```
Com_SignalConfigType (VehicleSpeed):
  RxDataTimeoutAction      = COM_RX_TIMEOUT_ACTION_SUBSTITUTE
  TimeoutSubstitutionValue = 0xFFFF  （655.35 km/h 相当、物理的にあり得ない値）

Com_ReceiveSignal(VEHICLE_SPEED, &out)  ← AbsInfo がタイムアウト中の場合
  RxDataTimeoutAction を確認
    COM_RX_TIMEOUT_ACTION_SUBSTITUTE → I-PDU バッファは読まず、
                                        out = 0xFFFF を書き込んで E_OK を返す
    COM_RX_TIMEOUT_ACTION_NONE（既定） → 何も書き込まず E_NOT_OK を返す（既存の全シグナルの挙動）
```

**なぜ REPLACE ではなく SUBSTITUTE を実装したか**: 本プロジェクトは
`ComSignalInitValue` という設定概念自体を持たず、RX バッファは `Com_Init()` で
単純にゼロクリアするのみです。REPLACE は「タイムアウト時 = 起動直後と同じ
初期値（0）」を返すだけなので、停車中で本当に速度が 0 の場合と区別がつきません。
SUBSTITUTE なら 0xFFFF のような、通常運用では絶対に出現しない値を割り当てられる
ため、「値が来ていない」ことを呼び出し元が確実に判別できます。

**この実装で実際に SUBSTITUTE が発動する場面はあるか**: 正直に言うと、
現状はありません。`VehicleSpeed` を読む `Com_ReceiveSignal()` 呼び出しは
`Rte_COMRxInd_AbsInfo()` の中の 1 箇所のみで、これは `Com_RxIndication()` から
フレーム受信直後・`Com_RxTimedOut` リセット直後に同期的に呼ばれるため
（RX Signal Group 節で述べた理由と全く同じ）、この呼び出し自体がタイムアウト中に
実行されることはありません。加えて、この経路はそもそも E2E 検証済みの値を
`Rte_AbsInfoMirror` へコピーするだけで、ASW（`App_EngineManager` 等）は
`Rte_AbsInfoMirror` 経由でしか値を読まず、`Com_ReceiveSignal()` を直接呼ぶことも
ありません。

**「ASW の呼び出し方を変えれば発動するようになるか」— 実は Signal Group の場合は
それだけでは不十分**: `VehicleSpeed` は Signal Group メンバーのため、
`Com_ReceiveSignal()` が見る「タイムアウトしているか」は、呼び出し時点の
ライブな `Com_RxTimedOut` ではなく、直近の `Com_ReceiveSignalGroup()` 呼び出し
時点でスナップショットした `Com_RxShadowTimedOut` です（`Com_ReceiveSignalGroup()`
自身は呼び出し時点のライブな `Com_RxTimedOut` を見るため、SWS_Com_00876 の
「タイマ満了時」という要求はこの関数自身の呼び出し時点では正しく満たされます）。
つまり、たとえ将来 ASW が E2E Transformer のような中間ミラーを介さず
`Com_ReceiveSignal()` を直接、フレーム受信タイミングとは無関係な任意のタイミングで
呼ぶ構成に変えたとしても、その直前に `Com_ReceiveSignalGroup()` を呼ばない限り、
「タイムアウトが実際に発生した瞬間」ではなく「最後に `Com_ReceiveSignalGroup()`
を呼んだ時点」の状態しか観測できません。これは本プロジェクトの呼び出し方の
都合ではなく、Signal Group が「`Com_ReceiveSignal()` はシャドウバッファのみを
読み、ライブな I-PDU バッファ・ライブなタイムアウト状態には一切触れない」という
設計そのものに由来する構造的な性質です（実 AUTOSAR も同様。RX Signal Group 節で
引用した「the RTE accesses the group signals in the shadow buffer」参照）。

`TMS`/`MDT`/`ComTransferProperty`/RX Signal Group と同じく、動機は実利より
仕様忠実性です。

## Rx無効値検知（ComSignalDataInvalidValue/ComDataInvalidAction）

`ComRxDataTimeoutAction`（前節）が「一定時間フレームが来ない」という**時間ベース**
の異常を扱うのに対し、こちらは「フレームは正常に届いているが、中身の値そのものが
送信元によって明示的に『無効』とマークされている」という**値ベース**の異常を
扱います。実車ではセンサ断線・短絡を検知した ECU が、物理的にありえない特定の
ビットパターン（例: 8bit センサ値の 0xFF）を意図的に送信する、という運用がよく
使われます。実 AUTOSAR は `ComSignalDataInvalidValue`（ECUC_Com_00391）で
この「無効値パターン」を、`ComDataInvalidAction`（ECUC_Com_00314、NOTIFY/REPLACE）
で受信時の振る舞いを設定できます。

本プロジェクトは `EngineInfo` の `CoolantTemp`（8bit、非 Signal Group）に
`COM_DATA_INVALID_ACTION_NOTIFY` を適用しました。

```
Com_SignalConfigType (CoolantTemp):
  DataInvalidAction      = COM_DATA_INVALID_ACTION_NOTIFY
  InvalidValue           = 0xFF  （水温センサ異常マーカー）
  InvalidNotificationCbk = Rte_COMInvalidNotify_CoolantTemp

Com_ReceiveSignal(COOLANT_TEMP, &out)  ← CoolantTemp のバイトが 0xFF の場合
  DataInvalidAction を確認
    COM_DATA_INVALID_ACTION_NOTIFY → 受信値を signal object へ格納しない
                                      （Com_RxLastValidValue を更新しない）。
                                      out = 直近の有効値を書き込んで E_OK。
                                      Com_RxInvalidNotifyPending[] を立てる
                                      （InvalidNotificationCbk はまだ呼ばない）
    COM_DATA_INVALID_ACTION_NONE（既定） → 無効値チェックをせず、受信値を
                                            そのまま返す（既存の全シグナルの挙動）

Com_MainFunction()  ← 次回の Os 100ms タスク呼び出し
  Com_RxInvalidNotifyPending[] が立っているシグナルについて
  InvalidNotificationCbk()（Rte_COMInvalidNotify_CoolantTemp）を呼ぶ
```

**なぜ REPLACE ではなく NOTIFY を実装したか**: `ComRxDataTimeoutAction` の
REPLACE を実装しなかった理由と全く同じです。実 AUTOSAR の REPLACE
（SWS_Com_00681）はシグナルを `ComSignalInitValue` へ置き換えますが、本
プロジェクトはその設定概念自体を持ちません。NOTIFY（SWS_Com_00680/00717）は
「シグナルオブジェクトへ格納しない＝直近の有効値を返し続ける」という、
`ComSignalInitValue` に依存しない動作のため、こちらのみ実装しています。

**なぜ「直近の有効値」を返せるのか（新規追加した内部状態）**: これまでの
`Com_ReceiveSignal()` は I-PDU バッファ（または RX シャドウバッファ）を毎回
その場でアンパックするだけで、シグナル単位の「最後に確定した値」を別途保持して
いませんでした。SWS_Com_00717 の「次回の Com_ReceiveSignal は直近の有効値を
返す」を実現するには、無効値を弾いたあとの値をどこかに憶えておく必要があるため、
`Com_FilterLastValue`（TX 側フィルタ用）と対になる `Com_RxLastValidValue`
（シグナルごとの直近有効値キャッシュ）を新設しました。無効値を検知した呼び出しは
このキャッシュを更新せず、そうでない呼び出しは毎回更新します。

**この機能は実際に発動するか**: これまでの `SUBSTITUTE`/RX Signal Group とは
違い、正直に言うと **これは実際に発動します**。`CoolantTemp` を読む
`Com_ReceiveSignal()` は `Rte_COMRxInd_EngineInfo()`（RxIndicationCbk）内で、
E2E 検証に成功した**すべての** `EngineInfo` フレームに対して毎回呼ばれます。
すなわち、送信元が `CoolantTemp=0xFF` を含む（E2E CRC/Counter は正しい）
フレームを送信しさえすれば、この無効値検知ロジックは通常運用の経路で確実に
通過します（タイムアウト系の機能のように「このコールサイトは条件的に
到達しない」という限界がありません）。`tools/uds_tester/config.json` の
EngineInfo プリセットに「水温センサ異常 (0xFF)」を追加済みで、実機で
送信すると `Rte_EngineInfoMirror.temp` が更新されず、ログに
`Rte: CoolantTemp invalid value received (sensor fault pattern)` が出力される
ことを確認できます。

**実機検証で見つかった障害（コールバックを即時実行しない理由）**: 開発初期の
実装は、`Com_ReceiveSignal()` が無効値を検知した、まさにその場で
`InvalidNotificationCbk()`（`Rte_COMInvalidNotify_CoolantTemp()`、中身は
`DET_LOGW` による Serial 出力）を同期的に呼んでいました。実機で
`CoolantTemp=0xFF` のフレームを送信したところ、フレーム受信直後に
WDT（ウォッチドッグタイマ）リセットが発生しました。

原因は次の通りです。`CoolantTemp` を読む `Com_ReceiveSignal()` の呼び出しは、
`Rte_COMRxInd_EngineInfo()` 内の `SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA()` /
`SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA()` の間（`Rte_EngineInfoMirror` を
`Rte_Read_*` との競合から守るための排他区間）で行われます。この排他エリアの
実体は Arduino の `noInterrupts()`/`interrupts()`、すなわち**グローバル割り込み
禁止**です（`SchM_Hw.cpp` 参照）。この区間の内側で `Serial.print()` 系の
`DET_LOGW` を呼んだ結果、UART 送信バッファが埋まった時点で「送信完了割り込みが
バッファを空けるのを待つ」ループに入りましたが、割り込みそのものが禁止されている
ため空きが永久に生まれず、事実上の無限ループになりました。CAN 受信処理や WdgM の
リフレッシュもすべて止まるため、最終的に WDT が満了してリセットに至りました。

修正として、`InvalidNotificationCbk()` の実呼び出しを `Com_ReceiveSignal()` の
呼び出しスタックフレームから切り離し、`Com_RxInvalidNotifyPending[]` フラグを
立てるだけにして、実際の呼び出しは必ず `Com_MainFunction()`（Os の 100ms
タスク、割り込み禁止区間の外）側で行うようにしました。これは `Com_TxPending`
（`Com_SendSignal()` から `PduR_Transmit()` の実行を切り離す、既存の仕組み）と
全く同じ設計思想です。この教訓は `ComInvalidNotification` に限らず一般化できます:
**Com のコールバック系フック（`RxIndicationCbk`/`TxTransformCbk`/
`InvalidNotificationCbk` 等）は、それ自身がどの呼び出しコンテキスト（割り込み
禁止区間の内側かどうか）から呼ばれるか呼び出し側の事情に左右されるため、
ブロッキングする可能性のある処理（Serial 出力、長時間のループ等）を含めては
ならない。**

## RX ComFilterAlgorithm（受信フィルタ、プラウジビリティチェック）

`ComFilterAlgorithm` はこれまで TX シグナルの送信要否判定・TMS 評価にのみ
使っていました（`COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD`/
`COM_FILTER_MASKED_NEW_DIFFERS_X`）。実 AUTOSAR の仕様書を確認すると、
これらは「TMC（Transmission Mode Condition）の評価」であり値そのものは
破棄しないのに対し、値を実際に「破棄」するフィルタリングは RX 専用の概念だと
明記されています。

```
[SWS_Com_00695] The AUTOSAR COM module shall filter out signals only at
receiver side. (SRS_Com_02037)

[SWS_Com_00602] The AUTOSAR COM module shall use filtering mechanisms on
sender side for Transmission Mode Conditions (TMC) but it shall not filter
out signals on sender side. (SRS_Com_02083)
```

つまり、これまで TX 側で使っていた `ComFilterAlgorithm` は仕様上正しい用法
（TMC 評価）だった一方、「受信値そのものを検証して怪しければ捨てる」という
本来の意味でのフィルタリングは未実装でした。この非対称性を解消する形で、
RX シグナル向けに `COM_FILTER_NEW_IS_WITHIN` を追加しました。

### 適用例 — EngineSpeed のプラウジビリティチェック

`EngineSpeed`（RX、非 Signal Group、CAN 0x100 経由）に
`FilterAlgorithm=COM_FILTER_NEW_IS_WITHIN`、`FilterMin=0`、`FilterMax=8000`
（本プロジェクト想定エンジンのレッドライン相当 rpm）を設定しました。
`VehicleSpeed`（`ComRxDataTimeoutAction`節で既出）ではなくこちらを選んだ
理由は、`VehicleSpeed` は RX Signal Group メンバーで `Com_ReceiveSignal()`
が `Rte_COMRxInd_AbsInfo()`（フレーム受信直後の 1 箇所）でしか実際には
呼ばれず、既に「SUBSTITUTE は現状のアーキテクチャでは実際には発動しない」
という限界が記録済みだったのに対し、`EngineSpeed` は非グループシグナルで
`Rte_COMRxInd_EngineInfo()` から毎フレーム実際に `Com_ReceiveSignal()` が
呼ばれるため、このフィルタが確実に効くからです。

```c
[SWS_Com_00273] If the AUTOSAR COM module filters out a signal on receiver
side, i.e. filter condition evaluates to false, the AUTOSAR COM module shall
discard that signal and shall not process it.
```

`Com_ReceiveSignal()` は範囲外の値を検知すると、`ComDataInvalidAction` と
同じ「シグナルオブジェクトへ格納しない」動作（`Com_RxLastValidValue[s]` を
更新せず、直近の合格値を返し続ける）を行います。通知コールバック
（`FilterRejectCbk`、実 AUTOSAR の `ComNotification` 相当）の実呼び出しは、
`ComDataInvalidAction`/`InvalidNotificationCbk` と全く同じ理由・同じ仕組みで
次回 `Com_MainFunction()` まで遅延します（`Com_ReceiveSignal()` は
`Rte_COMRxInd_EngineInfo()` の `SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()`
内側から呼ばれるため、その場でコールバックを直接呼ぶと WDT リセットを
引き起こしうる教訓を踏襲）。

### 明示する簡略化（RXフィルタ）

- 実 AUTOSAR には他に `NEW_IS_OUTSIDE`/`MASKED_NEW_EQUALS_X`/`NEVER`/
  `ONE_EVERY_N` もありますが、「物理的にあり得ない受信値を弾く」という
  具体的なシナリオに最小限必要な `NEW_IS_WITHIN` のみ実装しています。
- `ComSignalInitValue`（`Com_SignalConfigType.InitValue`）は実装済みです。
  `[SWS_Com_00603]`（起動時 old_value を ComSignalInitValue にする）どおり、
  `Com_Init()`/`Com_IpduGroupStart(initialize=true)` で `Com_RxLastValidValue`
  を InitValue から初期化します（既定 0 のシグナルは従来どおりゼロクリアと
  同じ結果になります）。`ComRxDataTimeoutAction`/`ComDataInvalidAction` の
  `REPLACE` アクションもこの InitValue を使って実装済みです。
- RX Signal Group への適用（`[SWS_Com_00836]`: グループ全体を破棄する）は
  未実装です（動機は上記の通り、実際に効くシナリオが非グループシグナルに
  あったため）。

### 動作確認方法（RXフィルタ）

`uds_tester` の EngineInfo ボタンに「回転数センサ異常 (65535rpm、RXフィルタ
で拒否されるはず)」プリセットを追加しました。送信すると、Arduino ログに
次のように出力されます（E2E CRC/Counter はプリセット選択時に自動計算される
ため、E2E 検証自体は正常に通ります）。

```
[NNNNms] INFO  Com: RX iPdu=0 [.. .. FF FF 50 80]
[NNNNms] WARN  Rte: EngineSpeed out of plausible range, rejected by RX filter (kept last valid value)
```

このとき `App_EngineManager` が読む `EngineSpeed` は直前の正常値のまま
更新されないため、後続の `Dem: FreezeFrame ev=6 spd=...` ログの `spd` にも
`65535` は一切現れません（プリセットを正常値に戻して送信すれば、通常どおり
即座に反映されます）。

## 受信デッドライン監視（COM Deadline Monitoring）

COM モジュールが各 RX I-PDU の受信間隔を監視し、設定タイムアウト内にフレームが届かない場合に
上位層へエラーを通知します（AUTOSAR SWS_COM_00398 準拠）。

```
エンジン ECU がフレームを送り続けている間
  ↓ 受信のたびに
  Com_RxIndication() → Com_RxLastMs[0] = millis()   ← タイマリセット

100 ms ごとに（Task 5）
  Com_MainFunction()
    now - Com_RxLastMs[0] >= 5000 ms?
      YES → Com_RxTimedOut[0] = 1
             WARN ログ出力

3000 ms ごとに（Task 2）
  App_EngineManager_Run()
    Rte_Read_SpeedSensor_EngineSpeed()
      → Com_ReceiveSignal()
          Com_RxTimedOut[0] == 1 → return E_NOT_OK
    E_NOT_OK を検知
      → DEM_EVENT_COMM_TIMEOUT FAILED 報告
      → ENGINE_STATE_FAULT 遷移
      → LED 点滅（App_WarningIndicator がそのまま動く）
```

### タイムアウト設定値（`Com_Cfg.h`）

| I-PDU | 定数 | 既定値 | フォールバック動作 |
|-------|------|--------|-----------------|
| EngineInfo (0x100) | `COM_TIMEOUT_ENGINE_INFO_MS` | 5000 ms | STARTING/RUNNING → FAULT |
| AbsInfo (0x110) | `COM_TIMEOUT_ABS_INFO_MS` | 5000 ms | AbsActive が 0 に戻り ABS 警告消灯 |

### タイムアウト確認手順

1. RUNNING 状態に遷移させてから EngineInfo の送信を止める
2. 5 秒後：`WARN Com: RX timeout iPdu=0 (5000ms)` が出力される
3. さらに最大 3 秒後（次の Runnable 起動時）：`WARN AppEng: ->FAULT comm timeout` が出力される
4. LED が点滅に変わる
5. UDS SID 0x19 で DTC 0x000105 (COMM_TIMEOUT) が取得できる
6. EngineInfo を再送すると Com_RxTimedOut がリセットされ、次の Runnable サイクルで復帰する

### Com_CbkRxTOut（デッドライン検出の RTE 通知、2026-08 対応）

上記の検出ロジック自体（`Com_RxTimedOut[]`/`Com_SigTimedOut[]`、`Com_ReceiveSignal()`
が `E_NOT_OK` を返す経路）は当初から実装済みでしたが、検出した「瞬間」を
上位層へ明示的に通知する RTE コールバック（実 AUTOSAR の `Com_CbkRxTOut`、
[SWS_Com_00536]/[SWS_Com_00556]: "called immediately after a message
reception error has been detected by the deadline monitoring mechanism"。
RTE 生成名 `Rte_COMCbkRxTOut_<sn>`/`<sg>`、`Com_CbkTxTOut` と同じ
`ComTimeoutNotification`=ECUC_Com_00552 を共有する RX 側）は、実は
**シグナル単位の骨格だけ既に実装されていて、コールバックが本番設定で
一切配線されていなかった**ことが分かりました（旧 `Com_SignalConfigType.
TimeoutNotificationCbk`、`EngineSpeed`/`CoolantTemp`/`EngineOnFlag` 全てで
`FirstTimeoutMs`/`TimeoutMs` は既に設定済みで実際に `Com_SigTimedOut[]` を
立てていたが、コールバック自体は全設定で `NULL`）。加えてグループ単位
（AUTOSAR 本来は非 Signal Group が signal 単位、Signal Group はグループ
単位で発火——上の「タイムアウト設定値」表の `Com_RxTimedOut[]` 系）は
コールバックのフィールド自体が存在しませんでした。

これを是正し、シグナル単位のフィールドを `TimeoutNotificationCbk` から
`RxTOutCbk`（`TxTOutCbk` 等と同じ命名規則）へ改名した上で、
`Com_IPduConfigType` にグループ単位の `RxTOutCbk` を新設しました
（`TxTOutCbk` のシグナル単位/グループ単位の二重化と対称）。

```
EngineOnFlag（非 Signal Group、シグナル単位）:
  RxTOutCbk = Rte_COMCbkRxTOut_EngineOnFlag
  Com_MainFunction() のシグナル単位ループが Com_SigTimedOut[s] を
  新規に立てた瞬間、1回だけ呼ぶ

AbsInfo（Signal Group、グループ単位）:
  RxTOutCbk = Rte_COMCbkRxTOut_AbsInfo
  Com_MainFunction() の I-PDU 単位ループが Com_RxTimedOut[id] を
  新規に立てた瞬間、ipdu->IsSignalGroup!=0 を条件に1回だけ呼ぶ
```

**この機能は実際に発動するか**: **発動します（実機検証済み）**。`EngineInfo`/
`AbsInfo` いずれも本番で継続的に受信されるデッドライン監視対象であり、
「タイムアウト確認手順」節の手順（送信元シミュレータを止める）を
そのまま踏襲するだけで実機で確実に発動します。既存の
`WARN Com: RX timeout sig=... iPdu=...`/`WARN Com: RX timeout iPdu=...`
ログの直後に、それぞれ `Rte: EngineInfo RX deadline timeout
(EngineOnFlag)`/`Rte: AbsInfo RX deadline timeout (group)` が出力される
ことを確認できます。実機ログ抜粋:
```
[6157ms] WARN  Com: Com_MainFunction: RX timeout iPdu=0 (5000ms, first)
[6163ms] WARN  Com: Com_MainFunction: RX timeout iPdu=1 (5000ms, first)
[6169ms] WARN  Rte: Rte_COMCbkRxTOut_AbsInfo: AbsInfo RX deadline timeout (group)
[6177ms] WARN  Com: Com_MainFunction: RX timeout sig=0 iPdu=0 (5000ms, first)
[6183ms] WARN  Com: Com_MainFunction: RX timeout sig=1 iPdu=0 (5000ms, first)
[6190ms] WARN  Com: Com_MainFunction: RX timeout sig=2 iPdu=0 (5000ms, first)
[6197ms] WARN  Rte: Rte_COMCbkRxTOut_EngineOnFlag: EngineInfo RX deadline timeout (EngineOnFlag)
```
I-PDU 単位ループがシグナル単位ループより先に実行されるため、グループ単位
（`AbsInfo`）の通知がシグナル単位（`EngineOnFlag`）の通知より先に出力される
（`Com_MainFunction()` 内のループ順序どおり）。`Com_CbkTxTOut`（発動しない）とは対照的に、TX/RX
両方の送受信デッドライン監視コールバックのうち、実機で実際に発動するのは
こちら側のみです（理由: TX 側は Bus-Off が `Can_Write()` を同期的に
失敗させるため「送信済み・未確認」状態自体が生まれないのに対し、RX 側は
単に「相手が送ってこない」だけで確実に成立するシナリオのため）。

## TX 送信デッドライン監視（Com_CbkTxTOut）

RX 側の受信デッドライン監視（上記）と対をなす、TX 側の送信確認デッドライン
監視です。`PduR_Transmit()` へ送信を渡した後、対応する `Com_TxConfirmation()`
が一定時間内に届かなければ `Com_CbkTxTOut`（`TxTOutCbk`）を発火します。

```
[SWS_Com_00878] "The AUTOSAR COM shall start a configured transmission
deadline monitoring timer of a signal (group) if it is sent (within an
I-PDU) to the lower layer, unless the timer is already running."
[SWS_Com_00879] 初回は ComFirstTimeout、以降（1 サイクル完了後）は
ComTimeout でタイマを始動する（RX 側の First/Timeout 分離と対称）。
[SWS_Com_00880] "When the AUTOSAR COM receives a transmit confirmation for
an I-PDU, it shall cancel all running transmission deadline monitoring
timers"（成功/失敗を問わない）。
[SWS_Com_00554]（Com_CbkTxTOut 定義）"called immediately after a message
transmission error has been detected by the deadline monitoring
mechanism ... called on sender side only. It can be configured for
signals and signal groups."
```

これまでの `Com_IPduConfigType.FirstTimeoutMs`/`TimeoutMs` は明示的に RX 専用
と文書化されており（TX I-PDU では 0 を設定する規約）、TX 側の時間ベース監視は
一切実装されていませんでした。あったのは `Com_IpduGroupStop()` 契機の
`TxErrCbk`（送信済み・未確認のまま I-PDU Group が停止された場合のみ発火）
だけで、「確認が一定時間届かない」という時間ベースの検出手段は存在しません
でした。今回、RX 専用の `FirstTimeoutMs`/`TimeoutMs` とは別軸の
`TxFirstTimeoutMs`/`TxTimeoutMs`/`TxTOutCbk` を新設し、`MeterStatus`
（TX IPduId=0）に適用しました。

```
MeterStatus (IPduId=0):
  TxFirstTimeoutMs = TxTimeoutMs = COM_TX_TIMEOUT_METERSTATUS_MS（既定 2000ms）

Com_DoTransmit()（PduR_Transmit() が E_OK を返した場合のみ）:
  Com_TxConfPending[0] が 0→1 に遷移する瞬間だけ
  Com_TxConfPendingSinceMs[0] = now（[SWS_Com_00878] "unless already running"。
  MIXED 周期フロアや NumberOfRepetitions による再送で Com_DoTransmit() が
  重複して呼ばれても、既に確認待ちならタイマは延命しない）

Com_MainFunction()（TX ディスパッチループの後段）:
  Com_TxEnabled（CommunicationControl 有効中）かつ Com_TxIPduStarted[0]
  かつ Com_TxConfPending[0] のときのみ評価（RX 監視の Com_RxEnabled ゲートと
  同じ理由。当初は TX 側にこのゲートが無く、/code-review で指摘・是正した）
  threshold = Com_TxUsingFirstTimeout[0] ? TxFirstTimeoutMs : TxTimeoutMs
  (now - Com_TxConfPendingSinceMs[0]) >= threshold かつ未発火なら:
    Com_TxTimedOut[0] = 1; TxTOutCbk() を呼ぶ（EngineState シグナル単位）

Com_TxConfirmation()（成功/失敗を問わず、確認到達のたび）:
  Com_TxConfPending[0] = 0; Com_TxTimedOut[0] = 0
  Com_TxUsingFirstTimeout[0] = 0（以降は steady TxTimeoutMs を使う）
```

**残り再送回数のデクリメント（前節）と同じ理由で、タイマの発火判定は
dispatch/confirmation の同期性に依存しない設計にしている**: `Can_Write()`
は TX 確認を即座には呼ばず、`swPduHandle` を保留キューへ積むだけで、実際の
`CanIf_TxConfirmation()` 呼び出しは**別タスク** `Can_MainFunction_Write()`
（1ms 周期）がキューをドレインするタイミングで行われます（`Can.c` 冒頭
コメント参照）。`Com_MainFunction()` は 100ms 周期の別タスクのため、両者は
非同期です。このタイマ機構はそもそも「確認が届くまで待つ」ことを目的とした
仕組みなので、この非同期性自体は問題になりません（`Com_TxConfPendingSinceMs`
というタイムスタンプで経過時間を測るだけで、確認の到着タイミングに依存しない）。

**`Com_InvokeTxNotification()` への3つ目のコールバック種別追加**:
`TxAckCbk`/`TxErrCbk` の共通配送ロジックへの集約を説明した節で「将来3つ目の
類似コールバックを追加する際、片方だけ修正して他方を直し忘れるリスクが
ある」と予告していた通りの状況になりました。`Com_TxNotifyKindType` に
`COM_TX_NOTIFY_TOUT` を追加し、`Com_InvokeTxNotification()` 内の2箇所の
三項演算子（「呼び出し先が2種類しかないため三項演算子で十分」という
2026-08 レビュー時点の判断根拠）を switch 文へ変更しました。3種とも
呼び出し側がコンパイル時に知っている固定種別のままであることは変わらない
ため、関数ポインタテーブルのような抽象化へは寄せていません（同じ判断基準の
延長）。

**この機能は実際に発動するか**: **発動しません。** `Com_DoTransmit()` →
`PduR_Transmit()` → `CanIf_Transmit()` → `Can_Write()` は同期的に完結し
（SPI 送信自体は MCP2515 とのブロッキング通信）、Bus-Off 中は `Can_Write()`
が `CanState != CAN_CS_STARTED` により**送信そのものを同期的に失敗**させます
（`Can.c:404-405`）。つまり Bus-Off 中は `Com_DoTransmit()` の `ret` が
`E_NOT_OK` となり、`Com_TxConfPending[]` はそもそもセットされず、「送信済み・
未確認」という監視対象状態自体が発生しません。当初「Bus-Off 中に確認が
届かなくなるため実機で発火しうる」と考えてこの機能を選定しましたが、
実装前にコールチェーンを直接追跡した結果この前提が誤りだったと判明し、
ユーザーと合意の上でこの結論を受け入れて実装を継続しました。唯一の理論上の
発火経路は、TX 確認キュー `Can_TxConfQueue`（`CAN_TX_CONF_QUEUE_SIZE=4`）が
溢れて確認通知だけが握りつぶされるケース（`Can_Write()` の既存コメント
「万一キューが満杯の場合は、この確認通知だけを諦める」参照）ですが、
`Can_MainFunction_Write()` が 1ms 周期でこのキューをドレインしており、
本プロジェクトの実際の送信頻度（最速でも Nm フレームの 1000ms 間隔）では
天文学的に起こりにくく、事実上到達不能です。`Com_CbkTxErr`（TxErrCbk）と
同じ位置づけ——仕様忠実性とユニットテストによる検証を目的とした実装であり、
実機での動作確認は行っていません。

## AUTOSAR IF シグネチャ整合（2026-08）

`Com_CbkTxAck`/`Com_CbkTxErr`/`Com_CbkTxTOut`/`Com_CbkRxAck` の4系統は、
過去のセッションで RTE 生成名を実 AUTOSAR の命名規則から意図的にずらして
実装していました（`TxAckCbk`/`TxTOutCbk` は「本プロジェクト独自の短縮形に
揃える」、`RxAckCbk` は「`RxIndicationCbk` が既に `Rte_COMCbk_<name>` を
使っていたための衝突回避」）。ユーザーから「AUTOSAR IF のシグネチャは
仕様書と一致させたい」という方針が示されたため、ローカルの
`docs/autosar/4.3.1/AUTOSAR_SWS_COM.pdf`（`pdftotext` で全文抽出し各 SWS 項番を
直接確認）を典拠に、全 RTE 生成名を仕様どおりへ是正しました。

| コールバック | SWS 項番 | 仕様書が定める RTE 名 | 旧実装名 | 新実装名 |
|---|---|---|---|---|
| `Com_CbkTxAck` | [SWS_Com_00468] | `Rte_COMCbkTAck_<sn>`/`<sg>` | `Rte_COMTxAck_*` | `Rte_COMCbkTAck_*`（仕様どおり） |
| `Com_CbkTxErr` | [SWS_Com_00491] | `Rte_COMCbkTErr_<sn>`/`<sg>` | （本番未使用） | （変更なし、本番未使用のまま） |
| `Com_CbkTxTOut` | [SWS_Com_00554] | `Rte_COMCbkTxTOut_<sn>`/`<sg>` | `Rte_COMTxTOut_*` | `Rte_COMCbkTxTOut_*`（仕様どおり） |
| `Com_CbkRxAck` | [SWS_Com_00555] | `Rte_COMCbk_<sn>`/`<sg>` | `Rte_COMRxAck_*` | `Rte_COMCbk_*`（仕様どおり） |

`Com_CbkRxAck` が仕様どおりの `Rte_COMCbk_<sn>/<sg>` を名乗るには、その名前を
先に使っていた `RxIndicationCbk`（実 AUTOSAR の標準コールバックではない、
本プロジェクト独自の I-PDU 単位汎用フック）側を退避させる必要がありました。
`RxIndicationCbk` は仕様上の命名規則を持たない独自フックなので、衝突を
避けるための改名はこちら側が担うのが筋と判断し、`Rte_COMRxInd_<name>`
（`Rte_COMRxInd_EngineInfo`/`Rte_COMRxInd_AbsInfo`/`Rte_COMRxInd_SecureCommand`）
へ改名しました。

**副産物として見つかった未実装のコールバック（2026-08、別ラウンドで対応済み）**:
この調査で `Com_CbkRxTOut`（[SWS_Com_00556]、RX 側デッドライン監視の
タイムアウト通知、RTE 名 `Rte_COMCbkRxTOut_<sn>/<sg>`）という、
`Com_CbkTxTOut` と対をなす仕様上のコールバックが未実装であることも
判明しました。当時の受信デッドライン監視（`Com_RxTimedOut[]`）は内部
フラグを立てて `Com_ReceiveSignal()` が `E_NOT_OK` を返すのみで、明示的な
コールバック通知は行っていませんでした。命名整合とは独立した機能ギャップ
だったため対応を見送っていましたが、直後のラウンドで実装しました
（詳細は「受信デッドライン監視」節の「Com_CbkRxTOut」小節参照）。

## Com_RxIpduCallout（受信I-PDU単位のフィルタリングフック）

これまでの RX 側ゲートは、いずれも**バッファへ格納した後**（`RxIndicationCbk`/
`Com_CbkRxAck`/`ComFilterAlgorithm(NEW_IS_WITHIN)`/`ComDataInvalidAction`）か、
**アンパック済みのシグナル値**（後2者）に対するものでした。実 AUTOSAR の
Com には、これらより手前——PduR から渡された生バイト列そのもの、バッファ
書き込み前——で受理/拒否を判定できる `Com_RxIpduCallout` という機構があります。

```
[SWS_Com_00700] The I-PDU callout on receiver side can be configured to
implement user-defined receive filtering mechanisms.
[SWS_Com_00816] The AUTOSAR COM module shall forward all data of the
received I-PDU (i.e. the complete I-PDU as provided by the PduR) in the
Com_RxIpduCallout.
（戻り値 false: "I-PDU will not be processed any further"）
```

Signal Gateway 節で `[SWS_Com_00872]` の RX 処理段階（1: デッドライン監視
タイマ再始動、2: I-PDU callout、3: update-bit 確認、4: エンディアン変換）を
引用した際、段階2は「`Com_RxIndication()` の既存処理が概念上占めている」と
説明していましたが、実際に `Com_RxIpduCallout` 相当の機構自体は存在して
いませんでした。今回、`SecureCommand`（RX IPduId=2、`ImmobilizerCmd`）に
これを適用しました。

**段階の実行順について（実装当初の逆順を /code-review で発見・是正、
2026-08）**: `[SWS_Com_00872]` は段階1（タイマ再始動）→段階2（callout）の
順で列挙しています。実装当初はこれを逆にしていました——callout を最初に
評価し、拒否された場合はタイマ再始動を含めて一切処理しない、という形で、
既存の Signal Group 短小フレーム破棄（`[SWS_Com_00575]`）と同じ「拒否＝
一切処理しない」という一貫した扱いに見えました。しかしこれは実 AUTOSAR の
意図とは逆で、**実機ログで実際に問題が顕在化しました**: `RxIpduCalloutCbk`
が同じフレームを繰り返し拒否し続ける状況（`uds_tester` で「Reserved異常」
プリセットを送り続けた場合）で、物理的にはバスもフレーム到着も正常なのに、
デッドライン監視が「相手が沈黙している」と誤ってタイムアウト扱いにして
しまうことが分かりました（ユーザーからの指摘: 「実Autosarだとタイムアウト
しないけど、このプログラムだとタイムアウトする」）。

是正として、デッドライン監視タイマのリセット（段階1）を `Com_RxIndication()`
の冒頭、callout（段階2）や `[SWS_Com_00575]` 短小フレーム破棄より前へ
移動しました。根拠は `[SWS_Com_00715]`（"the AUTOSAR COM module shall
reset the reception deadline monitoring timer ... at invocation of the
function Com_RxIndication" — リセットは `Com_RxIndication()` が呼ばれた
という事実だけに懸かる）と `[SWS_Com_00738]`（"shall not take the values
of the signals into account"、無効なシグナル/シグナルグループ受信時も
タイマは再始動されると明記）です。「フレームが物理的に届いた」という
事実（タイマの役割）と「中身が受理可能か」（callout/短小フレーム破棄の
役割）を独立させ、実 AUTOSAR の段階順と一致させています。バッファの
更新自体は従来どおり callout/短小フレーム破棄で止まります（＝「受信の
事実」と「値の反映」を分離）。

**この是正が副次的に伴う2つの挙動変化（/code-review で指摘、いずれも
上記2つの SWS 要求に沿った意図した変化）**:

1. `[SWS_Com_00575]` の Signal Group 短小フレーム破棄も、今後はタイマを
   リセットするようになりました。従来は「破棄側が先に `return` する」
   ため、この破棄パスはタイマリセットのコードへ一度も到達していません
   でした（コード自体は `[SWS_Com_00738]` を引用しつつ、実際には
   Signal Group の破棄には適用されていなかった、という食い違いが
   存在していました）。回帰テスト
   `ComMainFunction_NG_GroupShortFrameDiscardStillResetsDeadlineTimer`
   で検証済みです。
2. `RxIpduCalloutCbk` に拒否された受信であっても、`Com_RxUsingFirstTimeout`
   は steady 状態へ遷移するようになりました。「初回受信」の定義が
   「バッファへ格納できたか」ではなく「`Com_RxIndication()` が呼ばれた
   こと」であるためです。`FirstTimeoutMs`（起動直後の猶予）と
   `TimeoutMs`（定常状態）に異なる値を設定した I-PDU で、拒否され
   続ける callout を持たせると、2回目以降は `TimeoutMs` 側のしきい値が
   使われます。回帰テスト
   `ComMainFunction_OK_RejectedFrameStillTransitionsToSteadyTimeout`
   で検証済みです。

```
SecureCommand (IPduId=2):
  RxIpduCalloutCbk = Rte_COMRxIpduCallout_SecureCommand

Com_RxIndication(RxPduId=2, ...)  ← SecOC が MAC・フレッシュネス検証成功後に呼ぶ
  Com_RxIPduStarted チェックの直後、まずデッドライン監視タイマを
  リセットする（[SWS_Com_00872] 段階1。以降 RxIpduCalloutCbk が拒否
  しても巻き戻さない）
  続けて RxAckCbk/RxIndicationCbk のいずれよりも前に
    RxIpduCalloutCbk(SduDataPtr, SduLength) を呼ぶ（段階2）
      byte[1]（Reserved、本来常に 0x00）が非0 → 0（false）を返す
        → Com_RxIndication() が即座に return（バッファ・通知は処理
          しないが、タイマは既にリセット済みのまま）
      0x00 → 1（true）を返す → 通常どおり処理を続行
```

**なぜ SecOC 検証済みのフレームにさらにこの層が必要か**: SecOC が保証する
のは「送信元の真正性」と「再送でないこと」（MAC・フレッシュネス）のみで、
ペイロードの業務レベルの妥当性（Reserved 領域が本当に規約どおり 0 か）は
関知しません。認証済みだが壊れた/仕様違反のペイロードを送ってくる KeyFobEcu
（実装バグ、あるいは意図的な攻撃）を想定すると、認証と業務バリデーションは
独立した層であるべきという設計です。

**既存のゲートとの違い（唯一「格納そのものを拒否できる」層）**: `RxIndicationCbk`/
`Com_CbkRxAck` はバッファ格納**後**の通知のため、格納自体は止められません。
`ComFilterAlgorithm(NEW_IS_WITHIN)`/`ComDataInvalidAction` は個々のシグナル
単位で「直近の有効値を返す」という代替であり、I-PDU 全体の受理そのものを
拒否するわけではありません。`Com_RxIpduCallout` だけが、バッファ格納・
`RxAckCbk`/`RxIndicationCbk` 通知を丸ごと止められます（ただしデッドライン
監視タイマのリセットは上記のとおり段階1で既に完了しているため対象外——
「フレームは物理的に届いた」という事実自体はなかったことにしません）。

**この機能は実際に発動するか**: **発動します**。SecOC の MAC・フレッシュネス
検証は「送信元が正しい鍵を持っているか」「再送でないか」だけを見ており、
ペイロードの中身までは検証しないため、`uds_tester` の「ImmobilizerCmd」プリセット
に追加した「Reserved異常」（`data=[0x01, 0xFF]`）を送信すると、SecOC 認証は
正常に通過した上で `Com_RxIpduCallout` に拒否されることを実機で確認できます。
ログに `SecOC: RxInd: iPdu=0 verified OK` の直後、通常の
`Com: RX iPdu=2 [...]` ログが出ないまま
`Rte: SecureCommand rejected by RxIpduCallout (Reserved=0xFF)` が出力され、
`ImmobilizerStatus`（Signal Gateway 転送先）も更新されないことで、
「認証は通ったが業務バリデーションで拒否された」という2層の防御が実際に
機能していることが確認できます。

### 明示する簡略化（Com_RxIpduCallout）

- 送信側対の `Com_TxIpduCallout`（[SWS_Com_00346]）は実装済みです。詳細は
  次節「Com_TxIpduCallout」を参照してください。
- 戻り値の型は AUTOSAR の `boolean` ではなく、本プロジェクトの他の 1/0
  フラグ群と同じ `uint8` を使っています（`Platform_Types.h` に `boolean`
  型自体を持たない、本プロジェクト全体の簡略化）。

## Com_TxIpduCallout（送信I-PDU単位のフィルタリングフック、2026-08 追加）

`Com_RxIpduCallout`（前節）の送信側対です。RX 側は「バッファへ格納する前に
拒否できる唯一の層」でしたが、TX 側は「PduR へ渡す前に送信を止められる
唯一の層」という同じ位置づけになります。

```
[SWS_Com_00346] The I-PDU callout on sender side can be configured for
example to implement user-defined transmission filtering or user-defined
pre-transmission-processing of the outgoing I-PDU.
[SWS_Com_00719] the AUTOSAR COM module shall invoke this I-PDU callout
diretly before the I-PDU is transmitted via PduR_ComTransmit.
（戻り値 false: "I-PDU will not be processed any further"）
```

`Com_DoTransmit()` 内、`TxTransformCbk`（E2E/CRC 等の送信直前変換）適用後・
`PduR_Transmit()` 呼び出し直前に、実際に送信される最終バイト列を渡して
呼びます（RX 側が「PduR から渡された生バイト列」を見るのと対称に、TX 側は
「PduR へこれから渡す最終バイト列」を見ます）。

**適用先: `ImmobilizerStatus`（TX IPduId=3）**。Signal Gateway
（`Com_GwMappingData`）が SecOC 検証済みの `ImmobilizerCmd`（RX、
`SecureCommand` フレーム）を SWC/Rte を介さず直接転送する専用フレームです。
`Rte_COMRxIpduCallout_SecureCommand`（RX 側の既存ゲート）は byte[1]
（Reserved）しか検証せず、`ImmobilizerCmd` 本体（byte[0]）が定義済みの値域
（`0x00`=LOCK / `0x01`=UNLOCK）に収まっているかまでは見ていません。SecOC の
MAC 検証を通過した送信元が実装バグ等で `0x00`/`0x01` 以外の値を送っても、
これまでは何の検査も経ないままゲートウェイが他 ECU へブロードキャスト
していました。`Rte_COMTxIpduCallout_ImmobilizerStatus` を送信直前に置くことで、
この抜け穴を塞いでいます。

```
ImmobilizerStatus (IPduId=3):
  TxIpduCalloutCbk = Rte_COMTxIpduCallout_ImmobilizerStatus

Com_DoTransmit(ipdu=3, ...)  ← Com_MainFunction() の TX ディスパッチから呼ぶ
  TxTransformCbk は未設定のためスキップ
  TxIpduCalloutCbk(Com_TxBuffer[3], DLC=1) を呼ぶ（PduR_Transmit() 直前）
    byte[0] が 0x00/0x01 以外 → 0（false）を返す
      → Com_DoTransmit() が E_NOT_OK で即 return
        （PduR_Transmit() 自体を呼ばない。Com_TxConfPending もセット
          しない＝TX 送信デッドライン監視の誤発火を防ぐ。update-bit も
          クリアしない＝次回の実送信で改めて「更新あり」を伝える）
    0x00/0x01 → 1（true）を返す → 通常どおり PduR_Transmit() へ進む
```

**RX 側との非対称性（1点）**: RX 側は拒否されてもデッドライン監視タイマの
リセット（[SWS_Com_00872] 段階1）だけは必ず行う（「フレームは物理的に
届いた」という事実は取り消さない）設計でした。TX 側にこれと対称な概念は
存在しません——そもそも「実際には送信していない」ため、TX 送信デッドライン
監視（`Com_CbkTxTOut`）のタイマを起動する理由自体がなく、`Com_TxConfPending`
を意図的にセットしないことで自然に「監視対象外」となります（RX のように
「タイマだけは動かし続ける」特別扱いが不要）。

**この機能は実際に発動するか**: 実機での確認は未実施です（`ImmobilizerCmd`
の送信元となる別 ECU が無く、正規の SecOC 鍵で `0x00`/`0x01` 以外の値を
意図的に送る手段が `uds_tester` 側に無いため）。回帰テスト
（`Bsw_TxChain_test.cpp` の `ComMainFunction_OK_AcceptedByTxIpduCalloutTransmitsNormally`/
`ComMainFunction_NG_RejectedByTxIpduCalloutDiscardsTransmission`）でのみ
検証済みです。

**既知の制約: 拒否が続くと最終的に静かになる（/code-review で指摘）**:
`ImmobilizerStatus` は `TxModeMode=DIRECT`（変化検知のみで送信）+
`NumberOfRepetitions=2`（初回+再送2回=計3回）という設定です。同一の異常値
（例: `0x05`）が繰り返し届いても、2回目以降は `Com_SendSignal()` の変化検知
フィルタ（`COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD`）が「値は変化していない」
と判定するため `Com_TxPending`/`Com_TxRepeatsRemaining` は再アームされません。
つまり最初の約 `RepetitionPeriodMs × NumberOfRepetitions` の間だけ拒否ログ
（`Rte_COMTxIpduCallout_ImmobilizerStatus` の WARN）が3回出た後、同じ異常値が
たとえ届き続けても以降は一切ログが出ません。これは `TxIpduCalloutCbk` 自体の
問題ではなく、DIRECT モード+変化検知フィルタという既存の一般的な仕組みの
性質です（`TxErrCbk` 等の「拒否され続けている」ことを継続的に知らせる仕組みは
本 I-PDU には設定していません）。実機で `ImmobilizerCmd` を送れる送信元が
存在しない現状では実害はありませんが、将来この構成を流用する場合は
注意してください。

## update-bit の受信側判定（discard）

update-bit の概要、および TX 側（非 Signal Group・Signal Group）のセット/クリアの
仕組みは前述の「Update Bit」を参照してください。ここでは Signal Group の
RX 側判定ロジック（discard）のみを説明します。

```
受信側（Com_ReceiveSignalGroup、Signal Group のみ）:
  UpdateBitPosition が 0xFF 以外なら、I-PDU バッファの該当ビットを確認
    0（未更新）→ 何もせず E_OK で戻る（SWS_Com_00802: "discard"。
                  シャドウバッファ・タイムアウトスナップショットとも
                  直近の確定コピー内容のまま）
    1（更新済み）→ 通常どおり確定コピー（既存処理）
```

**受信側の「discard」は E_NOT_OK ではなく E_OK を返す**: `Com_ReceiveSignalGroup()`
は update-bit=0 の場合、何も更新せずに `E_OK` を返します。これは
`ComDataInvalidAction=NOTIFY` の「無効値を検知しても直近の有効値を返して
`E_OK`」という設計判断と揃えたものです（`E_NOT_OK` は「呼び出し自体が失敗した/
現在タイムアウト中」を意味する既存の用法と一貫させるため）。

**適用状況**: `AbsInfo`（RX、Signal Group）は現状
`UpdateBitPosition = 0xFFU`（未適用）です。実車のこの種のフレームは高頻度・
固定周期で常に新鮮なセンサ値を送り続けるのが通常で、鮮度管理は既存の受信
デッドライン監視で十分カバーされるため、update-bit を使う動機が薄いという
従来からの判断は変わっていません。したがって上記の discard ロジックは今のところ
実機で経路を通りません（TX 側のセット/クリアのみが `MeterStatus`/`WarningStatus`
を通じて実機検証済みです）。

## Signal Gateway（Com_GatewayRoute、SWC を介さないシグナル転送）

`docs/AUTOSAR_SWS_COM.pdf` 7.2.5/7.11 章が定義する **Signal Gateway** を実装しました。
RX シグナルの値を、SWC/Rte を一切介さずに Com 内部で直接 TX シグナルへ転送する
仕組みです。

```
The AUTOSAR COM module provides an integrated Signal Gateway for forwarding
signals and signal groups in a 1:n manner ... After the Signal Gateway
received signal or signal groups for routing, it acts immediately as a
sender for these signals ... The signal processing does not differ if the
integrated Signal Gateway forwards a signal ... or if a Software Component
sends it.
```

この最後の一文（"the signal processing does not differ..."）をそのまま実装に
反映し、`Com_GatewayRoute()` は RX バッファから生値をアンパックした後、
**SWC が直接呼ぶのと全く同じ `Com_SendSignal()`** を内部で呼び出します
（フィルタ・TMS・送信要否判定は `Com_SendSignal()` 既存のロジックがそのまま
適用されるため、ゲートウェイ専用の特別なパス分岐は不要です）。

### 適用例 — ImmobilizerCmd（SecOC 検証済み）→ ImmobilizerStatus

SecOC 検証に成功した `ImmobilizerCmd`（RX、CAN 0x120、KeyFobEcu 想定）を、
新規フレーム `ImmobilizerStatus`（TX、CAN 0x230）へ直接転送します。

```
KeyFobEcu → SecOC（MAC・フレッシュネス検証） → Com_RxIndication(ComRxIPduId=2)
  → Rte_COMRxInd_SecureCommand()（ログのみ、既存のデモ用グルー）
  → Com_GatewayRoute()（新規）
      RX バッファから ImmobilizerCmd の生値をアンパック
      → Com_SendSignal(COM_SIGNAL_IMMOBILIZER_STATUS, &value)
          （SWC が直接呼ぶ場合と全く同じ経路。COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD
            により値が変化したときだけ次回 Com_MainFunction() で送信）
  → CAN 0x230 (ImmobilizerStatus) 送信
```

**このシナリオを選んだ理由**: `ImmobilizerCmd` は SecOC が PduR レベルで
MAC・フレッシュネスを検証した**後**にしか `Com_RxIndication()` へ届きません
（検証失敗フレームは SecOC が握りつぶし、Com は一切見ません）。つまり
`Com_RxBuffer` に載っている時点で既に認証済みのデータであることが保証されて
おり、それを生のバッファから直接転送しても（＝`Com_ReceiveSignal()` の
ComDataInvalidAction 等のゲートを経由しなくても）安全です。これは実車の
セキュリティゲートウェイ ECU が担う典型的な役割そのもので、「暗号処理の重い
認証は 1 箇所（この場合は SecOC）に集約し、他の内部 ECU は認証済みの単純な
信号だけを受け取ればよい（SecOC/AES-CMAC を実装する必要がない）」という
構成を体現しています。

（対照的に `EngineInfo`/`AbsInfo` は E2E 保護されていますが、E2E 検証は
`RxIndicationCbk`（`Rte_COMRxInd_EngineInfo` 等）側の責務であり、`Com_RxBuffer`
自体は E2E 検証の成否に関わらず常に最新の受信バイト列を保持します。この
違いにより、EngineSpeed 等を同じ方式でゲートウェイすると **E2E 未検証の
値を転送してしまう**リスクがあるため、今回は意図的に対象から外しています。）

### RX 側処理段階と実装の対応

`[SWS_Com_00872]` が定義する RX 側処理段階（1: デッドライン監視タイマ再始動、
2: I-PDU callout、3: update-bit 確認、4: エンディアン変換）のうち、本実装は
1〜2 を `Com_RxIndication()` の既存処理（`RxIndicationCbk` 呼び出しまで）が
担い、`Com_GatewayRoute()` はその直後（4 のエンディアン変換に相当する
アンパック）から始まります。3（update-bit 確認）は本実装の適用対象
（非 Signal Group シグナル同士）には存在しないため該当しません。
（2026-08 追記: 執筆時点では段階2は概念上の対応付けに過ぎず、`Com_RxIpduCallout`
自体は未実装でした。直後のラウンドで実装したため、現在は文字どおり
段階2を担う実装が存在します。詳細は後述の「Com_RxIpduCallout」節参照。
Signal Gateway 自体（`ImmobilizerCmd`→`ImmobilizerStatus`）は `SecureCommand`
に対する `RxIpduCalloutCbk` の拒否判定より後段のため、拒否された場合は
ゲートウェイも動作しません。）

`Com_ReceiveSignal()` が経由する `ComRxDataTimeoutAction`/`ComDataInvalidAction`/
`ComFilterAlgorithm(NEW_IS_WITHIN)` はいずれも `[SWS_Com_00872]` の処理段階に
含まれておらず、ゲートウェイは経由しません。`[SWS_Com_00701]`
「デッドライン監視タイムアウト中でもゲートウェイはルーティングを行う」とも
整合します（本実装はフレーム受信直後の同期呼び出しのため、そもそも
タイムアウト状態になり得ません）。

### 明示する簡略化（Signal Gateway）

- **非 Signal Group のシグナル同士（1:1）のみサポート**します。Signal Group の
  ゲートウェイ（`[SWS_Com_00361]`/`[SWS_Com_00383]`: グループを一貫した集合と
  して転送する要求）や update-bit 連動（`[SWS_Com_00702]`〜`[SWS_Com_00706]`）は、
  具体的な実機検証シナリオが無いため未実装です。
- **1:n のうち n=1 のみ設定**しています（`Com_GwMappingType` 自体は 1 つの
  ソースシグナルに対し複数のマッピングエントリを追加すれば 1:n に対応できる
  設計ですが、具体的な複数転送シナリオが無いため config は 1 件のみ）。
- **`ImmobilizerStatus` フレーム自体には E2E/SecOC いずれの保護も付与していません**
  （内部バスの「素の」ブロードキャストという位置づけ。認証はゲートウェイの
  入力側で既に完了しているため）。

### 動作確認方法（Signal Gateway）

`uds_tester` で「ImmobilizerCmd」ボタンから UNLOCK/LOCK を送信すると、Arduino
ログに次のように出力されます。

```
[NNNNms] INFO  SecOC: RxInd: iPdu=0 verified OK (freshness=N)
[NNNNms] INFO  Com: RX iPdu=2 [01 00]
[NNNNms] WARN  Rte: ImmobilizerCmd: UNLOCK (authenticated via SecOC)
[NNNNms] INFO  Com: Gateway src=12 -> dst=13 value=1
[NNNNms] INFO  Com: TX iPdu=3 [01]
[NNNNms] INFO  PduR: TX src=4 canif=5
[NNNNms] INFO  CanIf: TX id=5 can=0x230
[NNNNms] INFO  Can_Hw: TX OK id=0x230 dlc=1 [01]
```

## 開発の経緯（実機で見つかった不具合・設計変更）

> 現在の仕様を理解するだけなら読む必要はありません。実機検証で見つかった
> 不具合や、その結果としての設計変更の経緯を時系列でまとめています。

### CommunicationControl 実装時の仕様不整合

UDS CommunicationControl (SID 0x28) の実装時、2 つの仕様不整合が見つかった
（現在の仕様は [`Dcm_Notes.md`](./Dcm_Notes.md#communicationcontrolsid-0x28) 参照）。

**Rx 無効中の受信デッドライン監視**: 当初、`Com_MainFunction()`（受信デッドライン
監視、100ms 周期）は `Com_RxEnabled` を一切参照していなかった。SWS_Com_00684/
SWS_Com_00685（`Com_IpduGroupStop` により I-PDU が止められた間は受信処理だけでなく
デッドライン監視自体も無効化することを要求）に反しており、意図的に
CommunicationControl で受信を止めているだけなのに、`TimeoutMs` を超えて
無効化し続けると `Com_RxTimedOut` が誤って立ち、上位層（RTE/ASW）へ
「通信異常」として伝わってしまっていた。`Com_MainFunction()` は
`Com_RxEnabled==0` の間は監視自体を評価しないよう修正した。

あわせて、再度有効化（`RxEnabled` が 0→1）した瞬間に全 RX I-PDU の
`Com_RxLastMs`（最終受信時刻）と `Com_RxTimedOut` をリセットするようにした
（SWS_Com_00787: `Com_IpduGroupStart` 時にデッドライン監視タイマを
再始動する要求に対応）。これをしないと、`TimeoutMs`（EngineInfo/AbsInfo
とも 5000ms）以上の時間受信を無効化していた場合、再有効化した直後の
`Com_MainFunction()` 呼び出しで「無効化前の古い `Com_RxLastMs`」のまま
即座にタイムアウト判定されてしまう（実際にはまだ新しいフレームを
1 つも受信できていない段階で）。

**Tx 無効中の送信トリガー保持**: 当初、`Com_TriggerIPDUSend()` は Tx 抑制中も
`Com_TxUpdatePending`・周期フロアのカウンタをあえて保持し、「再度有効化された
瞬間に無効化中の更新が送信される」設計にしていた（コメント上は意図的な設計と
していたが、仕様とは逆方向の判断だった）。しかし SWS_Com_00777「停止中の
I-PDU の送信要求はキャンセルしなければならない」、および SWS_Com_00334 の
説明文「停止中に発生した送信トリガーは保持されず、再開しても古いトリガーで
即座に送信されることはない」に反していた。`Com_TriggerIPDUSend()` は
Tx 抑制中を検出した時点で `Com_TxUpdatePending`/`Com_TxCyclesSinceSent` を
破棄するよう修正した。

`uds_tester` の「ImmobilizerStatus (0x230, Signal Gateway)」受信モニターも
`(UNLOCK)`/`(LOCK)` を表示し、`ImmobilizerCmd` の送信直後に追従して更新される
ことが確認できます。

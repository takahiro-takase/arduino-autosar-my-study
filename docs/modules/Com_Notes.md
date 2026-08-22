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
  TxAckCbk = Rte_COMTxAck_EngineState

Com_TxConfirmation(TxPduId=0/*MeterStatus*/, result=E_OK)  ← PduR から呼ばれる
  IsSignalGroup==0 のため、Com_ConfigPtr->Signals[] を走査
    sig->Direction==TX かつ sig->IPduId == TxPduId かつ sig->TxAckCbk != NULL
    のものすべてについて sig->TxAckCbk() を呼ぶ  ← EngineState だけでなく、
                          同じ I-PDU の全 TX シグナルが対象（本設定では
                          EngineState のみ）

Com_IPduConfigType (WarningStatus_Tx):
  TxAckCbk = Rte_COMTxAck_WarningStatus

Com_TxConfirmation(TxPduId=1/*WarningStatus*/, result=E_OK)
  IsSignalGroup==1 のため、Signals[] は走査せず
  ipdu->TxAckCbk（Rte_COMTxAck_WarningStatus）をグループ単位で 1 回だけ呼ぶ
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
あわせて `WarningStatus` に `Rte_COMTxAck_WarningStatus`（`MeterStatus`/
`EngineState` の `Rte_COMTxAck_EngineState` と対になる、Signal Group 単位の
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
フロア（MIXED モード）で再送されるたびに `Rte_COMTxAck_EngineState()` が
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
そのたびに `Com_TxConfirmation()` → `Rte_COMTxAck_EngineState()` が呼ばれます。
ログに `Com: TxConf id=0` の直後に `Rte: MeterStatus TX ack (EngineState)`
が出力されることを実機で確認できます。`WarningStatus` も RunLamp（エンジン
稼働中は常時 1）の変化だけで通常運用中に送信され続けるため同様に発動し、
`Com: TxConf id=1` の直後に `Rte: WarningStatus TX ack (group)` が
（RunLamp/FaultLamp/AbsLamp どれが変化したかによらず）1 回だけ出力されます。

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
  RxIndicationCbk（Rte_COMCbk_AbsInfo）を呼ぶ

Rte_COMCbk_AbsInfo()
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
呼び出しはすべて `Rte_COMCbk_AbsInfo()` という 1 つの同期呼び出し列の中で行われ、
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
`Rte_COMCbk_AbsInfo()` の中の 1 箇所のみで、これは `Com_RxIndication()` から
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
`Com_ReceiveSignal()` は `Rte_COMCbk_EngineInfo()`（RxIndicationCbk）内で、
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
`Rte_COMCbk_EngineInfo()` 内の `SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA()` /
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
が `Rte_COMCbk_AbsInfo()`（フレーム受信直後の 1 箇所）でしか実際には
呼ばれず、既に「SUBSTITUTE は現状のアーキテクチャでは実際には発動しない」
という限界が記録済みだったのに対し、`EngineSpeed` は非グループシグナルで
`Rte_COMCbk_EngineInfo()` から毎フレーム実際に `Com_ReceiveSignal()` が
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
`Rte_COMCbk_EngineInfo()` の `SchM_Enter/Exit_Rte_MIRROR_EXCLUSIVE_AREA()`
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
  → Rte_COMCbk_SecureCommand()（ログのみ、既存のデモ用グルー）
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
`RxIndicationCbk`（`Rte_COMCbk_EngineInfo` 等）側の責務であり、`Com_RxBuffer`
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

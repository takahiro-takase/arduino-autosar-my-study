# CAN 通信スタック 詳細

> [README](../README.md) の「[CAN 通信スタック](../README.md#can-stack)」節から分離した詳細です。
> 原本は [docs/archive/README_2026-10-03.md](archive/README_2026-10-03.md) にも残しています。

<a id="tx-processing"></a>
## Tx 処理（Com → PduR → CanIf → Can の順）

<a id="tx-processing-normal"></a>
### 通常（E2E なし）

```
Com_SendSignal()/Com_SendSignalGroup()   ← ASW から呼ばれる。TX バッファへ pack するだけ
  ┊  (Com_TxPending 経由。次回 Com_MainFunctionTx() の 100ms tick まで非同期に待機)
  ↓
Com_MainFunctionTx()                        ← ここから下は同期呼び出し連鎖
  → PduR_ComTransmit()
    → TransmitOverrideFct 未設定（現状の全 TX I-PDU）:
        CanIf_Transmit() → Can_Write()（SPI 送信完了までここで同期完了）
```

> `TransmitOverrideFct`（PduR の TX 経路に SecOC 等の中間モジュールを挟む機構）を
> 使う TX I-PDU は現状ない。E2EHealthStatus は Profile05（CRC16）保護により DLC が
> classic CAN の 8byte 上限を超えるため、SecOC を介在させる余地がない（詳細は
> 「E2E 保護」節および [SecOC_Notes.md](modules/SecOC_Notes.md) 参照）。それでも `PduR_TxRoutingPathType.
> TransmitOverrideFct` フィールド・`SecOC_IfTransmit()` 自体は削除せず、
> 学習用リファレンス実装として残している。

<a id="tx-processing-e2e"></a>
### E2E（E2EHealthStatus 送信）

`Com_MainFunctionTx()` の TxTransformCbk フック（[E2E 保護](#e2e-p01) 参照）を経由して、
Protect 処理が通常のチェーンへ割り込みます。TxTransformCbk を使う TX I-PDU は
現状 E2EHealthStatus のみです。

```
Com_MainFunctionTx()
  → TxTransformCbk があれば呼ぶ    ← Rte_COMTransform_E2EHealthStatus()
                                     → E2EXf_E2EHealthStatus() → E2E_P05Protect()
  → PduR_ComTransmit() → CanIf_Transmit() → Can_Write()   （以降は「通常」と同じ）
```

<a id="rx-processing"></a>
## Rx 処理（Can → CanIf → PduR → Com の順）

<a id="rx-processing-normal"></a>
### 通常（E2E なし）

```
Can_Isr()                        ← 真の割り込み。ペンディングフラグを立てるだけ
  ┊  (Can_RxIrqPending 経由。次回 Os スケジューラ tick まで非同期に待機)
  ↓
Can_MainFunction_Read()          ← フラグをドレイン、SPI 読み出し（ここから下は同期呼び出し連鎖）
  → CanIf_RxIndication()         ← CAN ID → PduId（論理 PDU）へ変換
    → PduR_CanIfRxIndication() (= PduR_ComRxIndication())
      → 宛先ごとにマルチキャスト:
          Com_RxIndication()         ← EngineInfo/AbsInfo（RxIndicationCbk 経由で E2E 検証、後述）
          CanTp_RxIndication()       ← UDS 診断要求（複数フレーム対応）
          SecOC_RxIndication()     ← ImmobilizerCmd
            → Csm_MacVerify() → Com_RxIndication()
```

> `SecOC_RxIndication()` → `Com_RxIndication()` は常に到達するわけではなく、
> `Csm_MacVerify()` による認証成功時のみ呼ばれる（失敗時はログのみで Com へは
> 転送しない）。この認証ゲート自体の詳細は
> [`docs/modules/SecOC_Notes.md`](modules/SecOC_Notes.md#アーキテクチャ--e2e-transformer-方式とは異なる理由)
> を参照。

<a id="rx-processing-e2e"></a>
### E2E（EngineInfo/AbsInfo 受信）

`Com_RxIndication()` の RxIndicationCbk フック（[E2E 保護](#e2e-p01) 参照）を経由して、
Check 処理が通常のチェーンへ割り込みます。EngineInfo/AbsInfo いずれも Profile05 です。

```
Com_RxIndication()                 ← EngineInfo/AbsInfo（RxIndicationCbk 経由）
  → Rte_COMRxInd_EngineInfo/AbsInfo()
    → E2EXf_Inv_EngineInfo()/E2EXf_Inv_AbsInfo() → E2E_P05Check()
```

<a id="rx-processing-timeout"></a>
### デッドライン監視（受信タイムアウト）

上記2つは「フレームが届いた」ときのチェーンだが、こちらは逆に「フレームが
届かなくなった」ことを検知するチェーン（`AUTOSAR_SWS_COM.pdf` 7.3.6
「Deadline Monitoring」相当）。100ms 周期タスクが検知し、実際に値が
置き換わるのは次に `Com_ReceiveSignal()` が呼ばれたとき、という2段階に
なっている。

```
[100ms 周期タスク] Os_SchedulerStep() → Com_MainFunctionRx()
  → (now - Com_RxLastMs[iPdu]) がしきい値（ComTimeout/ComFirstTimeout）以上なら
      Com_SigTimedOut[signal] を立てる（WARN ログ、ここが検知点）
  ┊  (Com_SigTimedOut というフラグ経由。次に Com_ReceiveSignal() が
  ┊   呼ばれるまで非同期に待機)
  ↓
Com_ReceiveSignal()                ← Rte 等から呼ばれる（同期）
  → ComRxDataTimeoutAction に応じて返す値を決定:
      SUBSTITUTE : ComTimeoutSubstitutionValue で置換（例: VehicleSpeed→0xFFFF）
      REPLACE    : ComInitValue で置換
      NONE（既定）: E_NOT_OK（呼び出し元は自分の初期値を使う）
```

TX 処理の `Com_TxPending`（`Com_SendSignal()` が立てて `Com_MainFunctionTx()` が
読む）と同じ「立てる側／読む側が別々のタイミングで動く」非同期境界だが、
向きが逆になっている点に注意（こちらは周期タスクが立てて、on-demand 呼び出し
が読む）。`Com_MainFunctionRx()` はあくまで 100ms ごとのポーリングでしきい値
超過を確認するだけで、しきい値ちょうどの瞬間に発火する割り込みではない
（検知は最大約100ms 遅れうる）。

<a id="rx-processing-length-check"></a>
### 受信長チェックの多層防御

設定 DLC に満たない短小フレームは、まず `CanIf_RxIndication()`
が棄却する（SWS_CANIF_00026 相当、本来この責務は CanIf 層にある）。仮に何らかの理由で
ここを通過しても、`Com_RxIndication()`・`CanTp_RxIndication()`（SF/FF/CF/FC 各フレーム）
がそれぞれ独立に自分の期待長を検証する。1 箇所だけに頼らず各層が自分の責務として
検証するのは、本プロジェクトで実際に発生した「短いフレームで上位層バッファに
新旧混在の破損データが残る」バグ（Com のシグナル固定長アクセス等）を踏まえた設計判断
である。`Com_RxIndication()` の受信長チェックは AUTOSAR 本来の仕様（SWS_Com_00574/
00575/00870）に準拠したシグナル単位の部分受理を実装している: Signal Group は
一貫性のないスナップショットを公開しないため受信できたバイト数が DLC 未満なら
グループ全体を棄却するが（SWS_Com_00575）、非 Signal Group の I-PDU は受信できた
バイト範囲に収まるシグナルのみを部分的に受理し、範囲外のシグナルは前回受信値の
まま据え置く（SWS_Com_00574/00870）。前述の「新旧混在の破損データ」バグは
Com_ReceiveSignal/Com_SendSignal が BitSize に関わらず常に 4 バイト読み書きして
呼び出し元のスタックを破壊していたことが根本原因であり、本対応（未受信バイトに
触れず前回値をそのまま保持する）とは別の問題である（詳細は `Com.c` のコメント参照）。
なお `CanIf_PBCfg.c` の各 RxPdu の `.Dlc` は現状 Com 側の `ipdu->DLC` と同じ値に
設定されているため、`Com_RxIndication()` の部分受理パスは短小フレームが CanIf 層で
先に棄却されることで実運用上到達しない（2026-07 時点で実機確認済み）。検証するには
CanIf 側の `.Dlc` を一時的に緩める必要がある。

<a id="e2e-p01"></a>
## E2E 保護（EngineInfo/AbsInfo 受信・E2EHealthStatus 送信ともに Profile05）

AUTOSAR E2E (End-to-End) による保護です。CAN バスの電気的エラーでは検出できない
**データ破壊・フレーム脱落・フレーム重複・誤ルーティング**を、CRC と送信カウンタの 2 種類の
保護要素で検出します。本プロジェクトでは 3 方向に適用しており、EngineInfo/AbsInfo の受信・
E2EHealthStatus の送信いずれも `src/Bsw/E2E/E2E_P05.c` の CRC16+8bit カウンタ
**Profile05** を使用します。

> CRC8+4bit カウンタの **Profile01**（`src/Bsw/E2E/E2E_P01.c`）用の
> `E2EMon_NotifyCheckResult()`/`Rte_MapE2EStatus()` は削除せず、学習用リファレンス実装として意図的に
> 残しています（現在は呼び出し元がゼロ）。

> **統合方式（E2E Transformer）:** Com は E2E の存在を一切関知しません。AUTOSAR が定義する
> 3 通りの E2E 統合方式のうち「E2E Transformer」（`docs/autosar/4.3.1/AUTOSAR_SWS_E2ELibrary.pdf` 12.4 節、
> R4.2.1 以降）を模しており、CRC/Counter の検証・付与は Com の外側（`Rte` 層 +
> `src/Bsw/E2EXf/`）が担います。Com から BSW 層をまたいだ責務を切り離す設計です
> （詳細は本セクション内の「Com モジュールとの統合」を参照）。
>
> **E2EXf 自身の初期化状態ガード（SWS_E2EXf_00130/00133/00151）:** E2EXf は下位の
> `E2E_P01CheckStateType`/`E2E_P01ProtectStateType`（フレームごとの Check/Protect 状態）
> とは別に、「E2EXf_Init() が呼ばれたか」というモジュール自身の初期化状態を
> `E2EXf.c` の静的フラグで保持します。`E2EXf_PBCfg_Init()`（`EcuM_Init()` から
> `Com_Init()` の直後に呼ばれる）が各 I-PDU の State を初期化した最後に
> `E2EXf_Init()` を呼んでこのフラグを立てます。`E2EXf_Inv_EngineInfo()`/
> `E2EXf_Inv_AbsInfo()`/`E2EXf_E2EHealthStatus()` はこのフラグが立つ前に呼ばれると安全側（E_NOT_OK／no-op）で
> 早期 return するため、将来 `EcuM_Init()` の呼び出し順序が変わって初期化前に
> フレーム受信経路が有効になっても、未初期化 State（ゼロクリアされた BSS のまま）
> を使って誤判定することがありません。

- **EngineInfo（CAN 0x100、受信）**: エンジン ECU から受信するフレームを`E2E_P05Check`で検証
  （本セクション前半）。EngineSpeed（回転数）は実車ではメータ表示だけでなく変速制御・
  トラクションコントロール・オーバーレブ保護等、複数の機能が参照しうる値のため、
  一般的なエンジン ECU の周期送信フレームを模して保護を付与しています
- **AbsInfo（CAN 0x110、受信）**: ABS ECU から受信するフレームを`E2E_P05Check`で検証（本セクション前半）
- **E2EHealthStatus（CAN 0x220、送信）**: 本 ECU（メータ ECU）が送信する、EngineInfo/AbsInfo
  受信側の E2E 検証エラー累積数を伝えるネットワーク健全性テレメトリに `E2E_P05Protect`で
  Counter・CRC16 を付加（本セクション後半の E2EMon サブセクション参照）。
  監視ツールがこのテレメトリ自体の破損を検出できるようにするため、検出能力の高い
  Profile05 単体保護を採用しています（詳細は「送信側（Protect）— E2EHealthStatus」参照）

> MeterStatus（CAN 0x200、送信）・WarningStatus（CAN 0x210、送信）には E2E 保護を
> 付与していません。MeterStatus は EngineInfo/AbsInfo を Com が既に検証した**後**に
> メータ ECU 自身が導出する二次データ（エンジン状態の要約）、WarningStatus も同様に
> 警告灯の点灯状態という二次データであり、実車でも一次センサ値ほど厳密な保護が
> 付与されないことが多いため、素の（E2E 保護なしの）シグナル送信の実装例として
> 意図的に残しています。

<a id="ipdu-group"></a>
### I-PDU Group（Com_IpduGroupStart/Stop、通信のライフサイクル制御）

**個別の I-PDU 単位**で起動/停止できます。

```
[SWS_Com_00444] 既定では全 I-PDU Group は停止状態
[SWS_Com_00840] どの I-PDU Group にも属さない I-PDU は Com_Init() で常に
                起動済み扱いになり、二度と停止できない
```

`Com_IPduConfigType.IpduGroupId`（既定値 `COM_IPDU_GROUP_NONE`）で所属を設定します。
本プロジェクトでは **E2EHealthStatus（TX）**を「テレメトリ」I-PDU Group
（`COM_IPDU_GROUP_TELEMETRY`）に、**EngineInfo/AbsInfo（RX）**を「センサーRX」
I-PDU Group（`COM_IPDU_GROUP_SENSOR_RX`、2026-08 追加）に所属させています。
その他の I-PDU（MeterStatus/WarningStatus/ImmobilizerCmd/ImmobilizerStatus）は
どの I-PDU Group にも属させていません（＝常に有効、`Com_IpduGroupStart/Stop()` の影響を受けない）。

E2EHealthStatus は、診断監視用のネットワーク健全性テレメトリで、車両の基本動作には不要な「非重要」
通信であるため、独立して停止できる対象として選びました。EngineInfo/AbsInfo は、Bus-Sleep
（ComM が NO_COMMUNICATION へ離脱する真の物理スリープ）中も受信デッドライン監視が止まらず、
意図的な通信断のたびに「RX timeout」警告 + Dem FAILED DTC が誤って記録される問題があったため、
BswM が FULL_COM 到達で起動・NO_COMMUNICATION 到達で停止するよう分離しました
（SILENT_COMMUNICATION 中は受信自体が生きているため停止対象に含めていません。詳細は
[`Com_Notes.md`](modules/Com_Notes.md) 参照）。

<a id="ipdu-group-caller"></a>
### 呼び出し元は BswM（実 AUTOSAR の標準構成）

`docs/autosar/4.3.1/AUTOSAR_SWS_COM.pdf` [7.3.5.1] は次のように述べています。

```
Once again, the COM module does not know or handle any grouping of I-PDUs...
it is expected that the complete state handling of I-PDU groups is done
outside of the AUTOSAR COM module, e.g. within the Basic Software Mode Manager.
```

つまり「どの I-PDU がどの Group に属するか」は Com の設定（`Com_PBCfg.c`）が持ち、
「いつ Group を起動/停止するか」は **BswM が呼ぶ**、というのが実 AUTOSAR の標準的な
役割分担です。実際、`docs/autosar/4.3.1/AUTOSAR_SWS_BSWModeManager.pdf` にも
`BswMPduGroupSwitch` という専用の ActionList 項目種別が定義されています。

```
[SWS_BswM_00273] When a BswMPduGroupSwitch action is executed, the BswM
shall call Com_IpduGroupStart for each BswMEnabledPduGroupRef, and call
Com_IpduGroupStop for each BswMDisabledPduGroupRef.
```

これに倣い、`BswM_ActionType` に `BSWM_ACTION_PDU_GROUP_START`/`_STOP`
（既存の `BSWM_ACTION_ACTIVATE`/`_DEACTIVATE`——Os タスクの有効/無効化——とは別の
アクション種別）を用意し、以下のルールで I-PDU Group「テレメトリ」
（E2EHealthStatus、Rule 3〜5）と「センサーRX」（EngineInfo/AbsInfo、Rule 6/7）を
制御しています（`src/Bsw/BswM/BswM_PBCfg.c`）。

| Rule | トリガ | アクション |
|---|---|---|
| Rule 3 | EcuM==RUN `AND` ComM==FULL_COMMUNICATION | `Com_IpduGroupStart(TELEMETRY, initialize=false)` |
| Rule 4 | EcuM → POST_RUN | `Com_IpduGroupStop(TELEMETRY)` |
| Rule 5 | ComM==SILENT_COMMUNICATION `OR` ComM==NO_COMMUNICATION | `Com_IpduGroupStop(TELEMETRY)` |
| Rule 6 | EcuM==RUN `AND` ComM==FULL_COMMUNICATION | `Com_IpduGroupStart(SENSOR_RX, initialize=false)` |
| Rule 7 | ComM==NO_COMMUNICATION（真の物理スリープのみ） | `Com_IpduGroupStop(SENSOR_RX)` |

既存の Rule 0（RUN→全タスク有効化）・Rule 1（POST_RUN→アプリタスク無効化）への
変更はありません（`BswM_ExecuteRules()` は条件を満たす全ルールを実行するため、
Rule 0 と Rule 3 は RUN 遷移のたびに両方発火します）。Rule 3 が単一条件ではなく
AND 複合条件なのは、ComM のチャネルモードが EcuM の RUN/POST_RUN とは独立して
変化しうるため（Bus-Off 中の SILENT_COMMUNICATION 等）、CAN チャネルが実際に
FULL_COMMUNICATION でなければ E2EHealthStatus を送信してもバスに届かないから
です。Rule 5 はその対になる停止条件（ComM がチャネルを離脱したら即座にテレメトリ
を止める）です。Rule 6/7（受信側）は、受信専用の SILENT_COMMUNICATION 中も受信は生きているため
停止条件を NO_COMMUNICATION だけに絞り、POST_RUN でも止めません（Rule 4 に相当するルールは
ありません）。

<a id="ipdu-group-behavior"></a>
### Com_IpduGroupStart/Stop が実際に行うこと

- **Start**（[SWS_Com_00787]）: RX は受信デッドライン監視タイマを再始動
  （最終受信時刻を現在時刻へリセット）。TX は MDT/周期タイマの基準時刻を
  再始動し、update-bit をクリアし、現在のデータ内容から TMS を再評価する
  （[SWS_Com_00223]）。`initialize=true` の場合は追加で I-PDU バッファ・
  Signal Group のシャドウバッファをゼロ初期化する（[SWS_Com_00222]）。
- **Stop**（[SWS_Com_00684]/[SWS_Com_00685]）: RX は受信処理・デッドライン
  監視の両方を無効化する（`Com_RxIndication()` がフレームを受信してもバッファ
  を更新せずに棄却する）。TX は保留中の送信要求をキャンセルする
  （[SWS_Com_00777]）。停止中の TX 確認（`Com_TxConfirmation()`）も無視する
  （[SWS_Com_00800]）。
- **`Com_SendSignal()`/`Com_ReceiveSignal()` 自体は停止中でも内部バッファを
  更新・参照できる**（[SWS_Com_00334]）。「値をセットする」ことと「実際に
  送受信するタイミング」は独立した責務であり、E2EMon は E2EHealthStatus が
  現在起動中か停止中かを一切意識せず `Com_SendSignal()` を呼び続けます。

既存の `Com_SetCommunicationEnabled()`（診断 CommunicationControl 用）とは
**独立した、直交する抑制機構**です。実際に送受信処理が行われるのは両方が
有効な場合のみ（AND 条件）で、意図的に統合していません（本プロジェクトの
UDS 0x28 実装は「全 I-PDU 一括」のままの方が既存のテストが安全に保たれるため）。

<a id="ipdu-group-verification"></a>
### 動作確認方法

実機ログで、EcuM が RUN → POST_RUN → SHUTDOWN → （ウェイクアップ）→ RUN と
遷移する際、I-PDU Group の開始/停止が連動する様子が確認できます。ログ例は
「[シリアルモニタ出力例](archive/README_2026-10-03.md#serial-log-example)」の
「I-PDU Group 開始/停止（RUN/POST_RUN/SHUTDOWN 連動）」を参照してください。

<a id="can-comm-management"></a>
## CAN 通信状態管理（ComM / CanSM / CanNm）

CAN バス通信の有効・無効（NO_COM/FULL_COM）を管理する ComM、CAN コントローラの
状態遷移（Bus-Off 回復・スリープ/ウェイクアップ）を担う CanSM、ネットワーク
マネジメントを担う CanNm の3モジュールに加え、ComM と CanNm の間に入るバス非依存の
調整層 Nm（ComM からの `Nm_NetworkRequest()`/`Nm_NetworkRelease()` と、CanNm からの
状態通知 `Nm_NetworkMode()` 等を中継するだけの薄い層）をまとめます。実 AUTOSAR でも
これらは Com/PduR と同じ「Communication Services」クラスタに属し、EcuM/BswM/WdgM
（System Services、[ECU 管理層](../README.md#ecu-management)参照）とは別グループです。

このスタックを構成する各モジュール（ComM/CanSM/CanNm）の本プロジェクトでの役割は、
上記「[モジュール一覧](../README.md#module-list)」表の「概要」列（リンク先の `docs/modules/`
配下の個別ノート）を参照してください。CanSM は6状態・多数の条件分岐を持つ
状態機械のため、状態遷移図を
[`CanSM_Notes.md`（状態遷移）](modules/CanSM_Notes.md#状態遷移)に用意しています。

<a id="processing-flow-comm"></a>
### 処理の流れ（コールチェーン）

AUTOSAR では「上から下への要求 (Request)」と「下から上への通知 (Indication)」が分離されています。
Bus-Off 回復・ウェイクアップ検証は CanSM が中心となって EcuM/ComM/CanNm/Can と連携するため、
特定の 1 モジュールに閉じた話ではなく、ここでモジュール横断のコールチェーンとしてまとめます。
EcuM/BswM が関わる箇所は「← EcuM が ComM へ要求」のように図中に個別注釈しています。

```
【起動時】
EcuM_Init → ComM_RequestComMode(FULL_COM)   ← EcuM が ComM へ要求（上→下）
              └→ CanSM_RequestComMode(FULL_COM)（→ CanIf_SetControllerMode(CAN_CS_STARTED)）
                   └→ ComM_BusSM_ModeIndication(FULL_COM)  ← CanSM が ComM へ通知（下→上）
                        ├→ EcuM_RequestRUN(ECUM_USER_COMM)
                        └→ Nm_NetworkRequest() → CanNm_NetworkRequest()

【Bus-Off 検出時（回復試行の前、SWS_CanSM_00521）】
CanIf_ControllerBusOff → CanSM_ControllerBusOff
  受け付けるのは CANSM_STATE_FULL_COM と CANSM_STATE_SILENT_COM の 2 つのみ
  （SILENT_COM は PDU チャネルの TX 抑制だけでコントローラは稼働中のため、
   この状態でも Bus-Off は実際に起こりうる）。直前の状態は CanSM_PreBusOffState に記録する
  └→ CanIf_SetControllerMode(CAN_CS_STOPPED)（→ Can_SetControllerMode(CAN_T_STOP)）
       └→ ComM_BusSM_ModeIndication(SILENT_COM)  ← CanSM が ComM へ通知（下→上）
            ├→ Nm_NetworkRelease()  ← CanNm の送信試行を止める（Can_Write() に拒否され続けて
            │                          NM-Timeout を報告し続けるのを防ぐ）
            └→ （EcuM_RequestRUN/ReleaseRUN はいずれも呼ばない → RUN 維持）

【Bus-Off 回復試行時（L1/L2 バックオフで無期限に継続）】
CanSM_MainFunction（10ms タスク）
  └→ CanIf_SetControllerMode(CAN_CS_STARTED)（→ Can_SetControllerMode(CAN_T_START)）で再起動を試行
       └→ 復帰先は Bus-Off 発生直前の状態（CanSM_PreBusOffState）で分岐する
          ├─ 発生時 FULL_COM だった場合: PDU モードを CANIF_ONLINE へ戻し CanSM state → FULL_COM
          │    └→ ComM_BusSM_ModeIndication(FULL_COM)  ← CanSM が ComM へ通知（下→上）
          │         ├→ ComM_EcuMRunMode が既に FULL_COMMUNICATION のため
          │         │  EcuM_RequestRUN() は呼ばない（RUN は Bus-Off 中も維持されたまま）
          │         └→ Nm_NetworkRequest() → CanNm_NetworkRequest()  ← 送信再開
          │            （Bus-Off 発生時に CanNm の協調スリープ待ち = ComM_NmReleasePending
          │             だった場合は、CanNm を起こさず ComM_RetryNmReleaseAfterBusOff() で
          │             解放をやり直す）
          └─ 発生時 SILENT_COM だった場合: CanSM state → SILENT_COM
               （PDU モードは Bus-Off 中も触っていないため CANIF_TX_OFFLINE のまま）
               └→ ComM_BusSM_ModeIndication(SILENT_COM)
  （L1 リトライ超過で L2 へ降格した時点で Dem へ FAILED、回復成功時に PASSED を報告する。
    RUN の状態には影響しない）

【ボランタリスリープ突入時（エンジン OFF 継続、復帰経路あり）】
App_EngineManager_Run（3000ms タスク、ENGINE_STATE_OFF が5周期継続）
  └→ Rte_Call_ComM_RequestComMode(NO_COM)   ← ASW が ComM へ要求（上→下）
       └→ ComM_RequestComMode(COMM_USER_0, NO_COM)
            └→ 集約結果が NO_COM（Dcm も extendedSession でない場合のみ）
                 └→ Nm_NetworkRelease() → CanNm_NetworkRelease() のみ送る
                    （ComM_ChannelMode は FULL_COM のまま。CanSM はまだ何もしない）
                      CanNm が Repeat Message → Ready Sleep → Prepare Bus-Sleep → Bus-Sleep Mode と
                      自律的に遷移する（他ノードの NM フレームがあれば延期される）
                      ├→ Prepare Bus-Sleep 到達: Nm_PrepareBusSleepMode() → ComM_Nm_PrepareBusSleepMode()
                      │    └→ CanSM_RequestComMode(SILENT_COM)  ← PDU の TX を停止
                      └→ Bus-Sleep Mode 到達: Nm_BusSleepMode() → ComM_Nm_BusSleepMode()
                           └→ CanSM_RequestComMode(NO_COM) → CanIf_SetControllerMode(CAN_CS_SLEEP)
                                （→ Can_SetControllerMode(CAN_T_SLEEP)）
                                └→ ComM_BusSM_ModeIndication(NO_COM)
                                     └→ EcuM_ReleaseRUN(ECUM_USER_COMM)
                                          └→ EcuM: RUN → POST_RUN → (5秒後) → SHUTDOWN

【ボランタリスリープからのウェイクアップ時 — 1st phase: 検知（CAN バス活動を検知）】
Can_Isr（INT ピン立ち下がりの真のハードウェア割り込み。SHUTDOWN 中も常に有効）
  └→ Can_WakeupIrqPending フラグをセットするのみ（SPI/Serial は行わない）
       └→ Can_MainFunction_Wakeup（1ms タスク、SHUTDOWN 中もこのタスクだけは動き続ける）
            がフラグをドレインし EcuM_CheckWakeup(ECUM_WKSOURCE_CAN)  ← Can が EcuM へ直接通知（[SWS_Can_00271]）
                 └→ CanSM_ControllerModeIndication(0)
                      └→ Can_SetControllerMode(CAN_T_WAKEUP)   ← SLEEP→STOPPED (Listen-Only) のみ
                           └→ CanSM: NO_COM → WAKEUP_VALIDATING（ComM/EcuM へはまだ何も通知しない）

【2nd phase-a: 検証成功（検証タイマ内に有効な CAN フレームを受信）】
Can_MainFunction_Read（Listen-Only になったため通常の RX ドレインが有効。
                        SHUTDOWN 中もこのタスクだけは動き続ける）
  └→ CanIf_RxIndication()                     ← 受信フレームを検出
       └→ CanSM_RxIndication(0)                ← 全受信フレームで無条件に呼ばれる
            └→ CanSM: WAKEUP_VALIDATING → FULL_COM（Can_SetControllerMode(CAN_T_START)）
                 └→ ComM_BusSM_ModeIndication(FULL_COM)
                      └→ EcuM_RequestRUN(ECUM_USER_COMM)
                           └→ EcuM: SHUTDOWN → RUN（全タスク再有効化）
       └→ （同じフレームがそのまま PduR/Com/Dcm 等へも配信される）

【2nd phase-b: 検証失敗（検証タイマ超過、ノイズによる誤ウェイクアップ）】
CanSM_MainFunction（10ms タスク、SHUTDOWN 中も動き続ける）
  └→ CANSM_WAKEUP_VALIDATION_MS 超過を検出
       └→ Can_SetControllerMode(CAN_T_SLEEP)   ← STOPPED→SLEEP、ウェイクアップ割り込み再武装
            └→ CanSM: WAKEUP_VALIDATING → NO_COM（ComM/EcuM は一切関与せず、静かに再スリープ）
```

<a id="can-controller-sleep"></a>
### CAN コントローラのスリープ制御（Can / CanSM / CanNm 横断）

ComM/CanSM/CanNm 各モジュールの本プロジェクトでの役割は、上記
「[モジュール一覧](../README.md#module-list)」表の「概要」列（リンク先の `docs/modules/`
配下の個別ノート）を参照してください。以下の2節（CAN コントローラの実スリープ・
ボランタリスリープとウェイクアップ）は Can/CanSM/CanNm 横断の内容ですが、
スリープ判断の起点（`App_EngineManager_Run()` → `ComM_RequestComMode`）や
ウェイクアップ成功時の `EcuM_RequestRUN()` など、EcuM/BswM が関わる箇所は
以下のコールチェーン図中に個別に注釈しています。

#### CAN コントローラの実スリープ（`Can_SetControllerMode(CAN_T_SLEEP)`）

`Can.c` の `CAN_T_SLEEP`/`CAN_T_WAKEUP` 遷移（MCP2515 を実際にスリープさせる
`Can_Hw_SetMode(CAN_HW_MODE_SLEEP)`）は、唯一の経路として ComM の NO_COM
要求に端を発する `CanNm`（CanNm 状態機械）の協調スリープから実際にスリープします。

`App_EngineManager_Run()` が `ENGINE_STATE_OFF` の継続を検知して
`ComM_RequestComMode(COMM_USER_0, NO_COM)` を要求し、ComM の集約結果が実際に
NO_COM になった場合（`Dcm` も extendedSession でないことが条件）、
ComM が `Nm_NetworkRelease()`（→ `CanNm_NetworkRelease()`）を呼びます。ここで CanSM は
まだ物理スリープしません。`CanNm` が Ready Sleep → Prepare Bus-Sleep → Bus-Sleep
Mode と自律的に遷移し（他ノードからの NM フレーム受信があればその都度延期
される）、実際に Bus-Sleep Mode へ到達した時点で CanNm が `Nm_BusSleepMode()` →
`ComM_Nm_BusSleepMode()` で ComM へ通知し、ComM が `CanSM_RequestComMode(NO_COM)` を呼んで
初めて CanSM が実スリープを行います（その手前の Prepare Bus-Sleep 到達時には、
`ComM_Nm_PrepareBusSleepMode()` が `CanSM_RequestComMode(SILENT_COM)` で TX だけを先に止めます）。MCP2515 の CAN バス活動による
ウェイクアップ割り込み（`mcp_can` の `setSleepWakeup()`）を事前に有効化して
からスリープするため、バス活動があれば自律的に起床できます。詳細は次項
「ボランタリスリープとウェイクアップ」および後述「CanNm（ネットワークマネジメント）」
を参照してください。

> Bus-Off 回復（後述の「Bus-Off 回復シーケンス」参照）は L1/L2 バックオフで
> 無期限にリトライを継続する設計のため、CAN コントローラを実際にスリープさせる
> ことはありません（`Can_T_STOP`/`Can_T_START` の間を往復するのみ）。AUTOSAR
> 仕様（SWS_CanSM_00514/00515/00636）には「回復を諦めて二度と復帰しない」状態は
> 存在せず、この無期限リトライ設計に至った経緯は
> [`CanSM_Notes.md`](modules/CanSM_Notes.md#bus-off-回復断念設計の撤去) を参照してください。

#### ボランタリスリープとウェイクアップ

CAN コントローラを実際にスリープさせる唯一の経路（ボランタリスリープ）について、
スリープ判断からウェイクアップまでの一連の流れを詳しく説明します。

**スリープ判断（`App_EngineManager.c`）**

```
App_EngineManager_Run()（3000ms 周期）:
  ENGINE_STATE_OFF が続いている ?
    YES → s_offCycles++
           s_offCycles >= APP_ENGINE_SLEEP_OFF_CYCLES (既定 5、実質15秒) ?
             YES → Rte_Call_ComM_RequestComMode(NO_COM)
    NO  → s_offCycles = 0、Rte_Call_ComM_RequestComMode(FULL_COM)
```

`ComM_RequestComMode(COMM_USER_0, NO_COM)` は、Dcm が `ComM_DCM_ActiveDiagnostic()`
で extendedSession 中を通知し続けている間は無効化されます
（ComM のユーザ・診断アクティブ通知調停、前述の「ComM（通信マネージャ）」セクション参照）。
つまり「エンジンが止まっていて、かつ診断ツールも繋がっていない」ときだけ
実際にスリープします。

**ウェイクアップ検出とウェイクアップ検証（`Can_Isr()` / `Can_MainFunction_Read/Wakeup()` / `CanSM.c`）**

MCP2515 はスリープ中に CAN バス活動を検知すると、ソフトウェアの関与なしに
自律的に Listen-Only モードへ遷移し INT ピンをアサートします
（`setSleepWakeup(1)` で事前に有効化済み）。しかしこの WAKIF は電気的ノイズ等
でも誤って立ちうるため、INT アサートを検出しただけで即座に FULL_COM へ復帰
せず、AUTOSAR EcuM の **Wakeup Validation Protocol** に相当する 2 段階の
手順を踏みます。

```
Can_Isr()（INT ピン立ち下がりの真のハードウェア割り込み、SHUTDOWN 中も常に有効）:
  CAN_CS_SLEEP 中 ?
    YES → Can_WakeupIrqPending フラグをセットするのみ
    NO  → Can_RxIrqPending フラグをセットするのみ
  （SPI 通信・Serial ログ・CanIf 呼び出しはここでは一切行わない。理由は
   Can.c ファイル冒頭のコメントを参照）

Can_MainFunction_Wakeup()（1ms 周期、SHUTDOWN 中も動作）:
  CAN_CS_SLEEP 中 かつ Can_WakeupIrqPending ?
    YES → フラグをクリアし EcuM_CheckWakeup(ECUM_WKSOURCE_CAN) → CanSM_ControllerModeIndication(0)
            → Can_SetControllerMode(CAN_T_WAKEUP)   ← SLEEP→STOPPED (Listen-Only) のみ
            → CanSM: NO_COM → WAKEUP_VALIDATING（ComM/EcuM へはまだ通知しない）

Can_MainFunction_Read()（1ms 周期、SHUTDOWN 中も動作）:
  CAN_CS_SLEEP 中でない かつ Can_RxIrqPending ?
    YES → フラグをクリアし、受信バッファが空になるまでドレイン
          （Listen-Only になった直後からは、この関数が受信フレームを処理する）
       → 受信フレームがあれば CanIf_RxIndication() → CanSM_RxIndication(0)
            CANSM_STATE_WAKEUP_VALIDATING 中 ?
              YES → 検証成功: Can_SetControllerMode(CAN_T_START) → CANSM_STATE_FULL_COM
                     → ComM_BusSM_ModeIndication(FULL_COM) → EcuM_RequestRUN
              NO  → 通常運用中は何もしない

CanSM_MainFunction()（10ms 周期、SHUTDOWN 中も動作）:
  CANSM_STATE_WAKEUP_VALIDATING 中 ?
    YES → CANSM_WAKEUP_VALIDATION_MS (既定 2000ms) 超過 ?
            YES → 検証失敗: Can_SetControllerMode(CAN_T_SLEEP)（再スリープ、割り込み再武装）
                   → CANSM_STATE_NO_COM（ComM/EcuM は一切関与しない）
```

`CanSM_RxIndication()` は AUTOSAR SWS_CanSM の `CanSMRxIndicationUsed` 設定に相当し、
CanIf が受信したフレーム**すべて**について（特定の PDU に一致するかどうかに
関わらず）呼び出されます。「何らかの構造的に正しい CAN フレームを実際に
受信できた」こと自体が、直前のウェイクアップが本物のバス活動だった証拠になる
という考え方です。

検証成功の判定に使われたフレーム自体も失われません。`CanSM_RxIndication()` の
直後に続く `CanIf_RxIndication()` の通常の PDU 振り分け処理でそのまま
PduR/Com/Dcm 等へ配信されます。

想定されるログ例（スリープ突入 → ウェイクアップ検証成功・ノイズによる誤ウェイクアップ）は
「[シリアルモニタ出力例](archive/README_2026-10-03.md#serial-log-example)」の
「スリープ / ウェイクアップ（想定されるログ例、実機未検証）」を参照してください。

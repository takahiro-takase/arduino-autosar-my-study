# Dcm（診断通信マネージャ、UDS ISO 14229-1 / ISO 15765-2）

> [README](../../README.md) の「[診断スタック](../../README.md#diag-stack)」節から分離。

Dcm (Diagnostic Communication Manager) が UDS サービスを処理し、
CanTp (CAN Transport Protocol) が ISO 15765-2 のフレーム分割・組立を担います。
対応 SID 一覧・DID 一覧・IOControl 調停方式・CommunicationControl・
RoutineControl・RequestDownload 等のシーケンス・S3 タイマ・SecurityAccess の
詳細を以下にまとめます。

## 診断フレームルーティング

```
外部テスター（Cangaroo 等）
  │  CAN 0x7E0  [UDS 要求 / FC]
  ↓
MCP2515 → Can → CanIf（CanId=0x7E0 → RxPduId=1）
                  ↓
                PduR（パス 1: CanTp 専用ルート）
                  ↓
                CanTp_RxIndication()
                  SF → UDS ペイロードを即時渡し
                  FF → FC 送信、CF 待ち
                  CF → バッファ組立、完成後に渡し
                  FC → TX 側の CF 送信を再開
                  ↓
                Dcm_ComIndication() → UDS サービス処理
                  ↓
                CanTp_Transmit()
                  ≤7B → SF 送信
                  ≥8B → FF 送信 → FC 待ち → CF 送信
                  ↓
                PduR_Transmit(SrcPduId=1) → CanIf → Can
  │  CAN 0x7E8  [UDS 応答 / FC]
  ↓
外部テスター
```

CAN 0x100（EngineInfo）・0x110（AbsInfo）・0x7E0（診断要求）は PduR でルートが分離されており、互いに干渉しません。

## 対応 UDS サービス

テスト時に毎回フレーム構造を探し回らずに済むよう、SID×SubFunc 単位で送信フレームの
固定バイト・可変バイトをまとめます。byte0 は CanTp の SF PCI（UDS ペイロード長）です。
0x2E（後述）以外の要求ペイロードは 7 バイト以内のため SF で送信できます
（応答が長くなるケースは個別に後述します）。
**正応答 SID は ISO 14229-1 の共通規則により常に「要求 SID + 0x40」**
（例: 0x10→0x50、0x27→0x67）のため、表には記載しない。

| SID<br>サービス名 | Def | Ext | SubFunc | 要求フレーム（byte0=PCI） | 可変バイト・備考 |
|---|---|---|---|---|---|
| 0x10<br>DiagnosticSessionControl | ○ | ○ | 0x01<br>(Default) | `02 10 01 00 00 00 00 00` | — |
|  |  |  | 0x03<br>(Extended) | `02 10 03 00 00 00 00 00` | S3タイマ起動 |
| 0x11<br>ECUReset | ○ | ○ | 0x01<br>(hardReset) | `02 11 01 00 00 00 00 00` | — |
|  |  |  | 0x03<br>(softReset) | `02 11 03 00 00 00 00 00` | — |
| 0x14<br>ClearDiagnosticInformation | × | ○ | — | `04 14 FF FF FF 00 00 00` | byte2-4=groupOfDTC<br>・0xFFFFFF=全クリア<br>・DTCコード指定=1件クリア<br>**SecurityAccess Level1 必須**（未認証は NRC 0x33） |
| 0x19<br>ReadDTCInformation | ○ | ○ | 0x01<br>(件数取得) | `03 19 01 MM 00 00 00 00` | byte3=statusMask |
|  |  |  | 0x02<br>(DTC一覧取得) | `03 19 02 MM 00 00 00 00` | byte3=statusMask |
|  |  |  | 0x04<br>(FreezeFrame取得) | `06 19 04 HH MM LL RR 00` | byte3-5=DTCコード<br>byte6=recordNumber（固定0x01） |
|  |  |  | 0x06<br>(ExtendedData取得) | `06 19 06 HH MM LL RR 00` | byte3-5=DTCコード<br>byte6=recordNumber（固定0x01） |
|  |  |  | 0x0A<br>(サポートDTC一覧取得) | `02 19 0A 00 00 00 00 00` | 追加パラメータなし。statusMask による絞り込みを一切行わず、本 ECU が対応する DEM_EVENT_COUNT 件全てを返す（後述） |
|  |  |  | 0x14<br>(FaultDetectionCounter取得) | `02 19 14 00 00 00 00 00` | 追加パラメータなし。0x0A と同じ全件取得だが、応答に statusAvailMask を含まない点が 0x02/0x0A と異なる（後述） |
| 0x22<br>ReadDataByIdentifier | ○ | ○ | — | `03 22 HH LL 00 00 00 00` | byte2-3=DID（0x0101/0x0102/0x0103/0x0104） |
| 0x27<br>SecurityAccess | × | ○ | 0x01<br>(requestSeed) | `02 27 01 00 00 00 00 00` | seed 2 バイト |
|  |  |  | 0x02<br>(sendKey) | `04 27 02 HH LL 00 00 00` | byte2-3=key（big-endian） |
| 0x2E<br>WriteDataByIdentifier | × | ○ | — | FF+CF（後述、SF 不可） | DID=0x0104 (TestPattern) 固定 8 バイト、DID=0x0108 (CryptoKeyUpdate) keyName(1)+key(16)=17バイトのみ対応<br>**SecurityAccess Level1 必須**（未認証は NRC 0x33） |
| 0x2F<br>InputOutputControlByIdentifier | × | ○ | 0x00-0x03<br>(controlOptionRecord) | `04 2F HH LL OO 00 00 00`<br>(shortTermAdjustment のみ `05 2F HH LL 03 SS 00 00`) | byte2-3=DID（0x0105/0x0106/0x0107）<br>byte4=controlOptionRecord<br>byte5=controlState（shortTermAdjustmentのみ、0/1）<br>**SecurityAccess 不要**（0x14/0x2E と異なり車両制御・NVM書換を伴わないため） |
| 0x28<br>CommunicationControl | × | ○ | 0x00-0x03<br>(controlType) | `03 28 CC TT 00 00 00 00` | byte2=controlType（0x00 enableRxAndTx/0x01 enableRxAndDisableTx/0x02 disableRxAndEnableTx/0x03 disableRxAndTx）<br>byte3=communicationType（0x01 通常通信/0x02 NM通信/0x03 両方）<br>拡張アドレス指定(0x04/0x05)・サブネット指定は非対応<br>**SecurityAccess 不要**（0x2F と同じ理由） |
| 0x31<br>RoutineControl | × | ○ | 0x01<br>(startRoutine) | `04 31 01 02 03 00 00 00` | RID=0x0203 (EngineHealthCheck) のみ対応<br>**SecurityAccess 不要**（センサ読み取りのみで車両制御・NVM書換を伴わないため） |
|  |  |  | 0x02<br>(stopRoutine) | `04 31 02 02 03 00 00 00` | 未開始 (IDLE) で呼ぶと NRC 0x24 |
|  |  |  | 0x03<br>(requestRoutineResults) | `04 31 03 02 03 00 00 00` | 実行中は結果未確定 (RUNNING)、完了後は PASS/FAIL を返す（後述） |
| 0x34<br>RequestDownload | × | ○ | — | FF+CF（後述、SF 不可、11バイト） | byte2=dataFormatIdentifier（0x00 固定）<br>byte3=addressAndLengthFormatIdentifier（例 0x44=アドレス4B+サイズ4B）<br>以降=memoryAddress+memorySize<br>**SecurityAccess Level1 必須**（未認証は NRC 0x33） |
| 0x36<br>TransferData | × | ○ | — | `0N 36 CC DD DD ...`（データ長により SF/FF+CF） | byte2=blockSequenceCounter（0x01開始）<br>byte3-=転送データ（実データは保持せずチェックサムのみ計算、後述） |
| 0x37<br>RequestTransferExit | × | ○ | — | `01 37 00 00 00 00 00 00` | 応答にチェックサム（後述） |
| 0x3E<br>TesterPresent | ○ | ○ | 0x00 | `02 3E 00 00 00 00 00 00` | S3タイマ維持 |

Def/Ext 列は `Dcm_SidSessionTable[]`（Dcm_Cbk.c）の設定そのもので、SID 単位
（SubFunc 単位ではない）の制約のため SID 行にのみ記載する。×の場合、該当セッションで
要求すると各ハンドラに到達する前に NRC 0x7F（serviceNotSupportedInActiveSession）で拒否される。
非対応サービスは NRC 0x11（serviceNotSupported）で応答します。
statusMask の代表値: `0x08`=confirmedDTC のみ / `0xFF`=全件。
0x19/04 の応答は 18 バイトと SF の 7 バイト制限を超えるため CanTp が FF+CF に分割します
（詳細は [`Dem_Notes.md`](./Dem_Notes.md) の「FreezeFrame」節）。0x19/02 も 2 件以上ヒットすると同様にマルチフレームになります。

### 0x19/0A reportSupportedDTC（2026-08 追加、GitHub Issue #122）

外部の方から「0x19 の subFunc 0x0A も対応してほしい」という要望
（Issue #122）を受けて追加しました。ISO 14229-1 の定義上、0x0A は
0x02（reportDTCByStatusMask）と応答フォーマットは同じですが、
**statusMask による絞り込みを一切行わない**点が異なります
（"the server shall report ... regardless of their status"）。

`Dem_GetAllDTCs()`（0x01/0x02 が使う既存関数）はステータスバイトと
`statusMask` の AND が非ゼロの DTC のみを返すため、一度も故障判定が
完了していない（`DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR` 以外のビットが
すべて 0 の）DTC は、どんな `statusMask` を渡しても列挙できません
（`(status & statusMask)` は該当ビットが 0 なら常に 0 のため）。
「本 ECU がそもそもどの DTC に対応しているか」を問う 0x0A の要求には
これでは応えられないため、絞り込みを一切行わず `Dem_DtcTable[]` の
全件（`DEM_EVENT_COUNT` 件）を無条件に返す `Dem_GetSupportedDTCs()`
を新設し、`Dcm_HandleReadDtcSupported()` から呼んでいます
（詳細は `Dem.h`/`Dem.c` の該当コメント参照）。

**ユニットテストについて**: 本プロジェクトは従来、Dcm/Dem を実機 +
`uds_tester` の手動検証のみで確認しており、ユニットテストが存在
しませんでした（Com/Can とは異なる扱い）。今回、この状況を変えて
`[env:native_dcm]`（`platformio.ini`・`test/test_dcm/` 参照）を新設し、
`Dcm_ComIndication()` に生の UDS バイト列を渡して `CanTp_Transmit()`
（フェイクでキャプチャ）の応答を検証する形で 0x19 の主要 subFunc を
カバーしました。実機での動作確認はこれとは別に行います。

**uds_tester での動作確認**: `config.json` に「対応DTC全件取得 (0x19/0A)」
ボタン（`02 19 0A`）を追加しました。応答デコード実装
（`app.py::_decode_dtc_response`）で 0x02 と共通のDTCリスト解析ロジックを
使うようにした際、既存の 0x02 側にも「statusAvailMask バイトを1件目の
DTCの先頭バイトとして誤読する」1バイトのオフセットずれが見つかったため、
あわせて修正しています（0x02 は DTC 0 件の応答が多く、これまで実害が
表面化していませんでした）。

**実機ログで発覚したバグ（CanTp 側のバッファサイズ）**: 実機検証で
subFunc 0x0A の応答が一切送信されない不具合が見つかりました。
`Dcm_TxBuf` は `DEM_EVENT_COUNT` 変化に自動追従するサイズだった一方、
下流の `CanTp_Transmit()` が独自に持つ TX バッファ上限（固定値 32 バイト）
が連動しておらず、`DEM_EVENT_COUNT=10` での 0x0A 応答（43 バイト）を
常に「invalid len」で拒否していました。`[env:native_dcm]` のユニット
テストは `CanTp_fake.c` を使うためこの層のチェックを再現しておらず、
検出できませんでした。詳細と修正内容は
[`CanTp_Notes.md`](./CanTp_Notes.md) の該当節を参照してください。

`CANTP_TX_BUFFER_SIZE` を修正後（最終的に48バイト。ISO-TPのフレーム境界に
一致し、`DEM_EVENT_COUNT` の今後の増加にも余裕を持たせた値。詳細は
[`CanTp_Notes.md`](./CanTp_Notes.md) 参照）、実機で再検証済みです。
FF（len=43）が受理され、CF×6（sn=1〜6）まで正しく送信完了
（`CanTp_SendNextCF: TX done`）し、応答バイト列を手動デコードすると
`DEM_EVENT_COUNT=10` 件全ての DTC レコードが正しい順序で組み立てられて
いることを確認しました。

### 0x19/0x14 reportDTCFaultDetectionCounter（2026-09 追加）

`Dem_GetFaultDetectionCounter()`（[SWS_Dem_00203]、デバウンスカウンタ生値
-128〜127を返すgetter）新設に伴い追加。DTC 一覧の取得自体は 0x0A と同じ
`Dem_GetSupportedDTCs()` を使うが、応答フォーマットが 0x02/0x0A と異なり
`DTCStatusAvailabilityMask` バイトを含まない（ISO 14229-1 の
`reportDTCFaultDetectionCounter` はそもそもステータス概念を扱わないため）。
実装当初 subFunc 値を 0x0B と誤って割り当てていたが、ISO 14229-1 では
0x0B は別サービス（reportFirstTestFailedDTC）であるため `/code-review` の
指摘で 0x14 に訂正した（`Dcm_Cfg.h`/`Dcm_Cbk.c` 参照）。

## DID 一覧（0x22 ReadDataByIdentifier）

| DID    | データ      | 型                                            | 単位 |
|--------|------------|-----------------------------------------------|------|
| 0x0101 | EngineSpeed | uint16, big-endian                           | rpm  |
| 0x0102 | CoolantTemp | uint8                                        | ℃   |
| 0x0103 | EngineState | uint8（0=OFF / 1=STARTING / 2=RUNNING / 3=FAULT） | —  |

## DID 一覧（0x2F InputOutputControlByIdentifier）

0x22/0x2E とは別の DID 空間として扱う（`Dcm_ReadDid()` には含まれず、0x22 で読み出すことはできない）。

| DID    | データ    | 型            | 対応する物理出力 |
|--------|----------|---------------|-----------------|
| 0x0105 | RunLamp   | uint8 (0/1)  | RUNNING LED (D6) |
| 0x0106 | FaultLamp | uint8 (0/1)  | FAULT LED (D7)   |
| 0x0107 | AbsLamp   | uint8 (0/1)  | ABS LED (D8)     |

## フレーム例（シングルフレーム）

**セッション切替（ExtendedDiagnosticSession）:**
```
送信 → 0x7E0: [02 10 03 00 00 00 00 00]
受信 ← 0x7E8: [06 50 03 00 19 01 F4 00]
```

**EngineSpeed 読み出し（DID 0x0101）:**
```
送信 → 0x7E0: [03 22 01 01 00 00 00 00]
受信 ← 0x7E8: [05 62 01 01 HH LL 00 00]  ← HH:LL が rpm 値（big-endian）
```

**ECUReset（hardReset）:**
```
送信 → 0x7E0: [02 11 01 00 00 00 00 00]
受信 ← 0x7E8: [02 51 01 00 00 00 00 00]
```

**RunLamp を強制点灯（shortTermAdjustment, DID 0x0105）:**
```
送信 → 0x7E0: [05 2F 01 05 03 01 00 00]
受信 ← 0x7E8: [05 6F 01 05 03 01 00 00]  ← byte5=controlStatusRecord（適用後の実際のレベル）
```

**RunLamp の診断制御を解除して ASW に戻す（returnControlToECU）:**
```
送信 → 0x7E0: [04 2F 01 05 00 00 00 00]
受信 ← 0x7E8: [04 6F 01 05 00 LL 00 00]  ← LL は解除直後にまだ出力されていた値
```

## ランプ IOControl の実現方式（Rte でのオーバーライド調停）

`App_WarningIndicator_Run()`（500ms 周期）は、Dcm の存在を知らないまま従来どおり
`Rte_Call_LedRunning_SetLevel()` 等を毎サイクル呼び続けます。Dcm が診断制御中の間、
この呼び出しの引数を「無視」して固定値を出力し続けることで IOControl を実現しています。

```
Rte_Call_LedRunning_SetLevel(aswLevel)  ← ASW が 500ms ごとに呼ぶ（Dcm の存在を知らない）
  Rte_Lamp_ArbitrateAndWrite(RTE_LAMP_RUN, aswLevel):
    Rte_LampOverrideActive[RUN] == 0 (通常) ?
      YES → effectiveLevel = aswLevel               ← ASW の値がそのまま反映される
      NO  → effectiveLevel = Rte_LampOverrideValue[RUN]  ← Dcm が固定した値を優先
    IoHwAb_LedRunning_SetLevel(effectiveLevel)

Dcm_HandleIoControl(RunLamp, shortTermAdjustment, level=1):
  Rte_IoControl_Lamp_ShortTermAdjustment(RTE_LAMP_RUN, 1)
    Rte_LampOverrideActive[RUN] = 1
    Rte_LampOverrideValue[RUN]  = 1
    IoHwAb_LedRunning_SetLevel(1)   ← 応答を返す前に即座に物理出力へ反映

Dcm_HandleIoControl(RunLamp, returnControlToECU):
  Rte_IoControl_Lamp_ReturnControlToEcu(RTE_LAMP_RUN)
    Rte_LampOverrideActive[RUN] = 0
    ← 次回の App_WarningIndicator_Run() 呼び出し（最大500ms後）で ASW の計算値に復帰
```

**なぜ ASW ではなく Rte で調停するか**: ASW（SW-C）に「Dcm が制御中かどうか」を
判定させると、SW-C が BSW モジュール（Dcm）の存在を知ることになり、AUTOSAR の
層分離原則に反します。本プロジェクトでは Com の `ComFilterAlgorithm`（値が変化した
ときだけ送信するかどうかを Com 自身が決める）や Signal Group（複数シグナルの
コミットタイミングを Com 自身が決める）と同じ設計判断を、CAN 送信ではなく
物理出力の調停に適用しました。「ASW は要求するだけ、実際に反映するかどうかは
BSW/RTE が決める」という責務分離を、通信スタックだけでなく I/O 制御にも
一貫して適用したことになります。

**4つの controlOptionRecord の使い分け**:
- `returnControlToECU`（0x00）: オーバーライド解除のみ。物理出力への書き込みは行わず、
  次の ASW サイクルに委ねる（レイテンシは最大 500ms）。
- `resetToDefault`（0x01）: デフォルト値（消灯）に固定し、制御は診断側が保持し続ける
  （`returnControlToECU` を呼ぶまで ASW には戻らない）。ISO 14229-1 上は解釈に幅がある
  パラメータのため、本実装ではこの方針を採用したことをコメントに明記している。
- `freezeCurrentState`（0x02）: 新しい値を指定せず、「今まさに出力されている値」
  （`Rte_LampLastLevel`）をそのまま固定する。FAULT LED が点滅中に発行すると、
  その瞬間の点灯/消灯状態で止まる。
- `shortTermAdjustment`（0x03）: `controlState`（1 バイト、0/1）で明示的に値を指定する。

## CommunicationControl（SID 0x28）

診断セッション中に通信の送受信そのものを無効化する UDS サービスです。実車では
リプログラミング（フラッシュ書き換え）の直前など、通常のバス通信が診断作業の
邪魔になる場面で使われます。

**controlType（サブ機能）と communicationType の組み合わせ**:

```
要求: [0x28, controlType, communicationType]
応答: [0x68, controlType]

controlType（Rx/Tx の有効/無効）:
  0x00 enableRxAndTx           rx=1 tx=1（既定状態への復帰）
  0x01 enableRxAndDisableTx    rx=1 tx=0
  0x02 disableRxAndEnableTx    rx=0 tx=1
  0x03 disableRxAndTx          rx=0 tx=0
  0x04/0x05（拡張アドレス指定）は非対応 → NRC 0x12 (subFunctionNotSupported)
  （本 ECU はゲートウェイではなく単一ネットワークのみのため、サブネット別の
   制御は行わない）

communicationType（対象。ビット0=通常通信、ビット1=NM通信）:
  0x01 通常通信（Com）のみ
  0x02 ネットワークマネジメント通信（Nm）のみ
  0x03 両方
  上記以外（サブネット指定を含む）→ NRC 0x31 (requestOutOfRange)
```

**対象と非対象**: `communicationType` の bit0（通常通信）は Com モジュール
（EngineInfo/AbsInfo 受信、MeterStatus/WarningStatus 送信）を、bit1（NM通信）は
Nm モジュール（CAN 0x400 送信）を制御します。診断通信そのもの（CanTp/Dcm）は
対象外です。これは仕様上も本質的な要件です — もし診断通信まで無効化してしまうと、
テスターが再度 CommunicationControl を送って復帰させる手段そのものを失ってしまいます。

**Rx 無効時の挙動**: `Com_RxIndication()` が受信フレームを無視します（バッファ・
タイムアウトタイマとも更新しません）。**Tx 無効時の挙動**: DIRECT/MIXED/PERIODIC
いずれの I-PDU も、実送信（`PduR_Transmit()`）は `Com_MainFunctionTx()` 内で行われ、
`Com_TxEnabled==0` の間はここで抑制されます（詳細は次項）。TX バッファの値自体は
`Com_SendSignal()` が既に更新済みのため失われず、再開後に実際に値が変化した時、
または通常の周期フロアに新たに達した時に初めて送信されます。

**Rx 無効中の受信デッドライン監視**: `Com_MainFunctionRx()`（受信デッドライン監視、
100ms 周期）は `Com_RxEnabled==0` の間、監視自体を評価しない
（SWS_Com_00684/SWS_Com_00685: `Com_IpduGroupStop` により I-PDU が止められた
間は受信処理だけでなくデッドライン監視自体も無効化する要求への対応）。
意図的に CommunicationControl で受信を止めているだけなのに、無効化を続けたことで
`Com_RxTimedOut` が誤って立ち「通信異常」と判定されることはない。

あわせて、再度有効化（`RxEnabled` が 0→1）した瞬間に全 RX I-PDU の
`Com_RxLastMs`（最終受信時刻）と `Com_RxTimedOut` をリセットする
（SWS_Com_00787: `Com_IpduGroupStart` 時にデッドライン監視タイマを
再始動する要求に対応）。これにより、無効化前の古いタイマのまま再有効化直後に
即座にタイムアウト判定される、ということは起きない。

**Tx 無効中の送信トリガー破棄**: DIRECT/MIXED I-PDU の変化検知は `Com_TxPending[]`
（「次回 `Com_MainFunctionTx()` で送信すべき変化あり」フラグ）に記録されます。
`Com_MainFunctionTx()` はこのフラグが立っている（または周期フロアに達した）I-PDU を
見つけるたび、`Com_TxEnabled` の値によらず無条件に `Com_TxPending[]` をクリアし
`Com_TxLastSentMs` を更新した**上で**、`Com_TxEnabled==0` なら実送信をスキップします。
つまり Tx 抑制中に変化があっても、そのフラグは Com_MainFunctionTx() の次回巡回で
消費されて捨てられるだけで、後から蒸し返されることはありません
（SWS_Com_00777「停止中の I-PDU の送信要求はキャンセルしなければならない」、
SWS_Com_00334「停止中に発生した送信トリガーは保持されず、再開しても古い
トリガーで即座に送信されることはない」への対応）。再開後に実際に値が変化した時、
または通常の周期フロアに新たに達した時に初めて送信される（＝仕様どおり「即座には
送信されない」）。

**セキュリティ方針**: 0x2F と同様に SecurityAccess は要求しません（車両制御や
NVM 書き換えを伴わないため）。ただし通信を止めるという操作的な影響の大きさから、
0x2F/0x31 と同じく extendedSession 限定とします。

**セッション復帰時のリセット**: `Dcm_CommControlReset()` が defaultSession への
遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）で Rx/Tx を enableRxAndTx へ
自動的に戻します。`Dcm_SecurityLock()`・`Dcm_RoutineAbort()` と同じ「セッションが
変われば診断側の一時状態は破棄する」という方針です。これを怠ると、通信を無効化した
まま診断ツールが切断された場合、ECU が二度と正常に通信できなくなってしまいます。

**動作確認方法**: uds_tester の「CommunicationControl (0x28)」ボタンで
`disableRxAndTx / normal (03/01)` を送信すると、MeterStatus/WarningStatus の送信と
EngineInfo/AbsInfo の受信が止まることが CAN ログで確認できます。
`enableRxAndTx / normal (00/01)` で元に戻ります。

## RoutineControl（SID 0x31、EngineHealthCheck）

0x2F（IOControl）が要求受信と同時に結果を確定するのに対し、0x31（RoutineControl）は
「開始（startRoutine）」と「結果取得（requestRoutineResults）」が別々のサービス呼び出しに
分かれる、UDS で頻出の非同期処理パターンを実装しています。本実装で対応する唯一の
RoutineID `0x0203`（`DCM_RID_ENGINE_HEALTH_CHECK`、実車の RID ではなく学習用の独自定義）は、
「EngineSpeed/CoolantTemp が正常範囲内かを判定する自己診断」を模しており、開始から
3 秒（`DCM_ROUTINE_DURATION_MS`）経過して初めて結果が確定します。

**状態機械（IDLE / RUNNING / COMPLETED）:**

```
startRoutine (31 01)
  IDLE/COMPLETED → RUNNING（開始時刻を記録）
  RUNNING 中の再要求 → NRC 0x22 conditionsNotCorrect（多重起動不可）

Dcm_MainFunction()（1000ms 周期）:
  RUNNING かつ経過時間 >= 3000ms ?
    YES → EngineSpeed <= 8000rpm かつ CoolantTemp <= 100℃ を判定
          RUNNING → COMPLETED（結果を確定）

requestRoutineResults (31 03)
  IDLE            → NRC 0x24 requestSequenceError（未開始）
  RUNNING         → routineStatusRecord=[0x00]（実行中、結果未確定）
  COMPLETED       → routineStatusRecord=[0x01, PASS(0x01)/FAIL(0x00)]
                    （結果は次の startRoutine まで何度でも取得可能）

stopRoutine (31 02)
  IDLE            → NRC 0x24（開始していないものは停止できない）
  RUNNING/COMPLETED → IDLE に戻す
```

defaultSession への遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）で
実行中・完了済みのルーチンは破棄され IDLE に戻ります（SecurityAccess の
再ロックと同じ「セッションが変われば診断側の一時状態は破棄する」という方針）。

**フレーム例:**
```
startRoutine:
送信 → 0x7E0: [04 31 01 02 03 00 00 00]
受信 ← 0x7E8: [04 71 01 02 03 00 00 00]

requestRoutineResults（実行中）:
送信 → 0x7E0: [04 31 03 02 03 00 00 00]
受信 ← 0x7E8: [05 71 03 02 03 00 00 00]   ← byte5=0x00 (RUNNING)

requestRoutineResults（完了後、PASS）:
送信 → 0x7E0: [04 31 03 02 03 00 00 00]
受信 ← 0x7E8: [06 71 03 02 03 01 01 00]   ← byte5=0x01 (COMPLETED), byte6=0x01 (PASS)
```

**動作確認方法**: uds_tester の「EngineHealthCheck (RID 0203)」ボタンで
startRoutine プリセットを送信後、3 秒待たずに requestRoutineResults を送ると
「実行中 (running)」、3 秒経過後に送ると「完了 (PASS/FAIL)」と表示されます。

## RequestDownload/TransferData/RequestTransferExit（SID 0x34/0x36/0x37）

実車 ECU の代表的な機能である「UDS 経由でのソフトウェア再書き込み（フラッシュ
ブートローダ）」のシーケンスを模擬します。**実際にフラッシュへ書き込むわけでは
ありません**（Arduino 自身のファームウェアを書き換えることはしない、プロトコルの
状態遷移を学ぶためのシミュレーションです）。受信データそのものも保持せず、
(1) blockSequenceCounter の順序検証、(2) 受信バイト数の集計、(3) 全受信バイトの
簡易チェックサム（XOR）計算、の3点だけを行います。

**状態機械（IDLE / DOWNLOADING）:**

```
RequestDownload (34)
  SecurityAccess Level1 未アンロック → NRC 0x33 securityAccessDenied
  DOWNLOADING 中の再要求            → NRC 0x22 conditionsNotCorrect（多重開始不可）
  dataFormatIdentifier ≠ 0x00       → NRC 0x31（圧縮・暗号化は非対応）
  memorySize が 0 または上限超       → NRC 0x31
  受理 → 受信バイト数=0・チェックサム=0・期待カウンタ=0x01 にリセットし DOWNLOADING へ
         maxNumberOfBlockLength（既定16バイト）を応答

TransferData (36)  ← memorySize に達するまで繰り返す
  IDLE 中の要求                     → NRC 0x24 requestSequenceError（未開始）
  blockSequenceCounter が期待値と不一致 → NRC 0x73 wrongBlockSequenceCounter
  累計受信サイズが memorySize を超過 → NRC 0x71 transferDataSuspended
  受理 → 受信バイト数に加算、チェックサム更新（XOR）
         期待カウンタを+1（0xFF の次は 0x00、その次はまた 0x01）

RequestTransferExit (37)
  IDLE 中の要求                     → NRC 0x24（未開始）
  累計受信サイズ ≠ memorySize        → NRC 0x22（転送未完了）
  一致 → DOWNLOADING → IDLE に戻し、チェックサムを応答
```

defaultSession への遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）で
進行中の転送は破棄され IDLE に戻ります（RoutineControl と同じ「セッションが
変われば診断側の一時状態は破棄する」という方針）。SecurityAccess は
RequestDownload でのみ判定します。TransferData/RequestTransferExit は
`Dcm_TransferState` が RequestDownload 経由でしか DOWNLOADING にならないため、
個別の再チェックは不要です。

**blockSequenceCounter の巡回規則（ISO 14229-1）**: 0x01 から開始し、
0xFF に達したら次は 0x00、その次はまた 0x01 へ戻ります（0x00 が初期値になる
ことはなく、巡回の一部としてのみ出現します）。

**なぜチェックサムを返すのか**: 実務の書き込みシーケンスでも、
RequestTransferExit の応答（transferResponseParameterRecord、内容は
manufacturer-specific）でチェックサムや CRC を返し、テスタ側が転送全体の
整合性を確認できるようにすることがよくあります。本実装ではその一例として
単純な XOR チェックサムを採用しています。

**フレーム例**（UDS ペイロード表記。memorySize=20 バイトを 10 バイトずつ
2 ブロックに分けて転送。応答はいずれも SF のため先頭に実際の CAN PCI
バイトを付けて示す）:
```
RequestDownload（UDS ペイロード 11 バイト、SF 不可のため FF+CF）:
送信 → 0x7E0: [34 00 44 00 00 00 00 00 00 00 14]
                     └┘ └───────┬───────┘ └───┬───┘
                  format  memoryAddress=0  memorySize=0x14(20)
受信 ← 0x7E8: [04 74 20 00 10]  (SF、4バイト)
                     └┘ └──┬──┘
              lengthFmt  maxNumberOfBlockLength=16

TransferData（1ブロック目、UDS ペイロード 12 バイト、FF+CF、counter=0x01）:
送信 → 0x7E0: [36 01 11 22 33 44 55 66 77 88 99 AA]
受信 ← 0x7E8: [02 76 01]  (SF、2バイト)

TransferData（2ブロック目、UDS ペイロード 12 バイト、FF+CF、counter=0x02）:
送信 → 0x7E0: [36 02 BB CC DD EE FF 00 11 22 33 44]
受信 ← 0x7E8: [02 76 02]  (SF、2バイト)

RequestTransferExit（UDS ペイロード 1 バイト、SF）:
送信 → 0x7E0: [01 37]
受信 ← 0x7E8: [02 77 44]  (SF、2バイト。0x44=全20バイトの XOR チェックサム)
```

**動作確認方法**: uds_tester の「Software Download (0x34/0x36/0x37)」グループで
RequestDownload → TransferData（ブロック1）→ TransferData（ブロック2）→
RequestTransferExit の順に送信すると、上記のシーケンスが実機で確認できます。
「TransferData 異常系」ボタンの「Wrong Counter」プリセットを RequestDownload
直後に送ると counter 不一致で NRC 0x73 が、「宣言サイズ超過」プリセットを
ブロック1・2送信済みの後に送ると NRC 0x71 が返ることも確認できます。

## S3 タイマ（セッションタイムアウト）

ISO 14229-1 では、defaultSession 以外（本実装では ExtendedDiagnosticSession）の間に
診断要求が一定時間（S3、既定 5000ms）途絶えると、テスターが離脱したとみなして
ECU が自動的に defaultSession へ復帰します。SID 0x3E（TesterPresent）は、
他に送るべき要求がないときにこの自動失効を防ぐためだけに存在するサービスです。

```
Dcm_ComIndication（要求受信時、SID を問わず毎回）:
  Dcm_LastActivityMs = millis()        ← S3 タイマをリセット

Dcm_MainFunction（1000ms 周期、Os Task 8）:
  session != Default かつ
  millis() - Dcm_LastActivityMs >= 5000ms (DCM_S3_TIMEOUT_MS) ?
    YES → session = Default
          INFO: "S3 timeout -> session=Default"
```

ExtendedDiagnosticSession に切り替えた後、5 秒以上どの診断要求も送らずに放置すると、
セッションが自動的に Default に戻ります。SID 0x22 等のセッション依存サービスは
本実装ではセッションを問わず応答するため動作に影響しませんが、シリアルログで
S3 タイマの遷移を確認できます。

## SecurityAccess（SID 0x27、Level1）

ClearDiagnosticInformation（0x14、DTC 履歴の消去）は誤操作・悪用の影響が大きいため、
SecurityAccess の Level1（subFunc 0x01/0x02）でアンロックしないと NRC 0x33
（securityAccessDenied）で拒否されます。requestSeed → sendKey の 2 段階チャレンジ
レスポンス方式は ISO 14229-1 標準の認証フローです。

```
1. requestSeed (27 01)
     ECU が seed（millis() 由来、毎回変化）を発行し、
     「seed 発行済み・key 未受信」状態にする。
     既にアンロック済みなら ISO 14229-1 の作法通り allZeroSeed (0x0000) を返す
     （sendKey は不要、テスター側はこれを見てアンロック済みと判断する）。

2. sendKey (27 02 <keyH> <keyL>)
     テスターは seed から key を計算して送信する。
     本実装の変換式（学習用の単純な例）:
         key = seed XOR DCM_SECURITY_KEY_MASK (0xA55A)
     一致 → Level1 アンロック、0x67 正応答。
     不一致 → NRC 0x35 (invalidKey)。
     3 回連続失敗 → NRC 0x36 (exceededNumberOfAttempts) を返し、
       以後 10 秒間 (DCM_SECURITY_DELAY_MS) requestSeed 自体を
       NRC 0x37 (requiredTimeDelayNotExpired) で拒否する（ブルートフォース対策）。

3. defaultSession へ遷移（明示要求・S3 タイムアウト・ECUReset のいずれも）すると
   Level1 は再ロックされる。ただし連続失敗回数・ロックアウト中フラグは
   セッションをまたいで保持する（セッション往復の繰り返しでロックアウトを
   回避できないようにするため）。
```

**注意:** `key = seed XOR 固定マスク` は仕組みを学ぶための最小限の例です。
量産 ECU は OEM 固有の非公開アルゴリズムや暗号学的アルゴリズムを使用するため、
本実装のロジックを実運用に転用しないでください。

## SID × セッション許可テーブル

`SecurityAccess は extendedSession 限定`という制約は、当初
`Dcm_HandleSecurityAccess` の中に個別にハードコードされていました。
他のサービス（ClearDTC 等）にも同種のセッション制約が増えていくと、
「どの SID がどのセッションで使えるか」が各ハンドラに分散して見通しにくくなります。
そこで AUTOSAR の `DcmDspSessionRow` コンフィグに相当する一覧テーブルへ一般化し、
`Dcm_ComIndication()` が SID ディスパッチの**前**に全 SID 共通で判定するようにしました。

```c
/* Dcm_Cbk.c */
static const Dcm_SidSessionRowType Dcm_SidSessionTable[] =
{
    { DCM_SID_CLEAR_DTC,        DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_SECURITY_ACCESS,  DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_WRITE_DATA,       DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_IO_CONTROL,       DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_COMM_CONTROL,     DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_ROUTINE_CONTROL,  DCM_SESSION_MASK_EXTENDED },
};
```

テーブルに掲載のない SID はセッション制約なしとみなされ、defaultSession でも応答します。
0x14・0x27・0x28・0x2E・0x2F・0x31 のみ extendedSession 限定とし、defaultSession で要求すると各ハンドラに
到達する前に NRC 0x7F（serviceNotSupportedInActiveSession）で拒否されます
（各 SID の制約は前述の「対応 UDS サービス」表の Def/Ext 列を参照）。

このテーブルはセッション制約のみを一元管理し、SecurityAccess（追加の認証）が
必要かどうかは各ハンドラが個別に判定します（`Dcm_HandleClearDtc` / `Dcm_HandleWriteDataById`
先頭の `Dcm_SecurityLevel == 0U` チェック）。0x2F はテーブルには載るが
SecurityAccess チェックは行わないため、extendedSession でありさえすれば
認証なしで実行できます。これは「セッション制約」と「認証要求」が独立した
直交する保護軸であることを示す例でもあります。0x14/0x2E（車両データの恒久的な
消去・書き換え）は両方必須、0x2F（ダッシュボードランプの一時的な出力オーバーライド）
はセッションのみで十分、という判断です。

新しいサービスにセッション制約を追加する場合は、ハンドラ内に判定を書き足すのではなく
`Dcm_SidSessionTable[]` に行を追加するだけで済みます。

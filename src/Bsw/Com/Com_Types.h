/**
 * \file    Com_Types.h
 * \brief   通信マネージャ型定義 (AUTOSAR SWS_COM 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COM_TYPES_H
#define COM_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

// -------------------------------------------------------
// 基本ID型
// -------------------------------------------------------
typedef uint8 Com_SignalIdType;
typedef uint8 Com_IPduIdType;
typedef uint8 Com_IpduGroupIdType;

/* I-PDU が所属する I-PDU Group を持たないことを示すセンチネル値
 * （UpdateBitPosition の 0xFF センチネルと同じ規約）。
 * [SWS_Com_00840]: I-PDU Group に属さない I-PDU は Com_Init() 時に常に
 * 開始済み（Started）として扱われ、Com_IpduGroupStart/Stop() の対象になる
 * ことも、Com_IpduGroupStop() で停止されることも一切ない（永久に有効）。
 * Com_IPduConfigType.IpduGroupId 参照。 */
#define COM_IPDU_GROUP_NONE  0xFFU

// -------------------------------------------------------
// シグナルのエンディアン
//
//   BIG_ENDIAN (Motorola)
//     BitPosition = シグナルの MSB 位置
//     ビットは BitPosition から BitPosition+BitSize-1 へ連続して格納
//     例: EngineSpeed 16bit, BitPosition=0
//         → bit0(MSB)〜bit15(LSB) が byte[0]上位〜byte[1]下位
//
//   LITTLE_ENDIAN (Intel)
//     BitPosition = シグナルの LSB 位置
//     ビットは BitPosition から BitPosition+BitSize-1 へ連続して格納
//     例: 16bit, BitPosition=0
//         → bit0(LSB)〜bit15(MSB) が byte[0]下位〜byte[1]上位
//
//   ※ ビット番号の定義（この実装全体で統一）
//      bit 0 = byte[0] の MSB、bit 7 = byte[0] の LSB
//      bit 8 = byte[1] の MSB、...（ネットワークビット順）
// -------------------------------------------------------
typedef enum
{
    COM_BIG_ENDIAN    = 0,  // Motorola byte order（CAN標準）
    COM_LITTLE_ENDIAN = 1   // Intel byte order
} Com_SignalEndianType;

// -------------------------------------------------------
// シグナルフィルタアルゴリズム（ComFilterAlgorithm 相当）
//
// 実 AUTOSAR では ComFilterAlgorithm は 3 つの異なる目的で使われる:
//   (1) ComTransferProperty=TRIGGERED_ON_CHANGE の判定材料（TX シグナル）
//       （signal(group) の送信要求時、値が変化していれば送信を開始する。
//       本実装では COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD が Com_SendSignal()
//       内でこの役割を担い、Com_RequestTxOnChange() を呼ぶかどうかを決める）
//   (2) TMS（Transmission Mode Selector）の評価材料（TX シグナル）
//       （SWS_Com_00676-00679: この I-PDU が今 ComTxModeTrue/False の
//       どちらを使うべきかを、TmsContributor=1 のシグナルの条件評価
//       （OR 合成）で決める。本実装では COM_FILTER_MASKED_NEW_DIFFERS_X が
//       この役割を担う。詳細は Com_IPduConfigType の TxModeModeTrue 参照）
//   (3) 受信フィルタ（RX シグナル、SWS_Com_00273/00695）
//       （[SWS_Com_00695] 「filter out signals only at receiver side」の
//       とおり、実 AUTOSAR で「値そのものを破棄する」フィルタリングは
//       RX 専用の概念である。(1)(2) は TX 側で ComFilterAlgorithm を
//       Transmission Mode Condition（TMC）の評価に使うだけで、値そのものは
//       破棄しない（[SWS_Com_00602]）。本実装では COM_FILTER_NEW_IS_WITHIN が
//       この役割を担い、Com_ReceiveSignal() が範囲外の値を「破棄」して
//       直近の合格値を返し続ける（詳細は FilterMin/FilterMax、
//       Com_SignalConfigType の FilterRejectCbk 参照）。
// これら 3 つは実 AUTOSAR でも独立した仕組みであり、同じシグナルが複数の
// 用途に使われるとは限らない（本実装でも、変化時送信の判定用シグナルと TMS 用
// シグナルは別々に設定できる。RX シグナルは (1)(2) と排他、TX シグナルは
// (3) と排他 — Direction が RX/TX どちらかで実質的に決まる）。
//
// なお Signal Group メンバーの「送信を引き起こすか」判定は
// ComFilterAlgorithm ではなく Com_TransferPropertyType（下記）が担う。
// 非 Signal Group シグナルでは ComFilterAlgorithm（用途 (1)）がそのまま
// 送信要否を決めるため、Signal Group とそれ以外で判定の仕組みが異なる点に
// 注意（実 AUTOSAR でも同様の構造）。
//
//   COM_FILTER_ALWAYS                     : 常に更新とみなす（フィルタなし、既定）
//   COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD
//     : (新値 & Mask) != (前回値 & Mask) のときだけ更新とみなす（用途 (1)）
//   COM_FILTER_MASKED_NEW_DIFFERS_X
//     : (新値 & Mask) != FilterX のときだけ TMC（Transmission Mode Condition）
//       を true とみなす（用途 (2)。SWS_COM_00813 の MASKED_NEW_DIFFERS_X 相当。
//       実 AUTOSAR は他に MASKED_NEW_EQUALS_X/NEW_IS_WITHIN/NEW_IS_OUTSIDE も
//       持つが、本実装では TMS 判定に最小限必要なこの 1 種類のみ実装する）
//   COM_FILTER_NEW_IS_WITHIN
//     : FilterMin <= 新値 <= FilterMax のときだけフィルタ条件が真（用途 (3)、
//       実 AUTOSAR の NEW_IS_WITHIN 相当）。真でなければ Com_ReceiveSignal()
//       はその受信値を「破棄」し、シグナルオブジェクトへ格納しない
//       （SWS_Com_00273）。実 AUTOSAR は他に NEW_IS_OUTSIDE/
//       MASKED_NEW_EQUALS_X/NEVER/ONE_EVERY_N も持つが、本実装は「物理的に
//       あり得ない受信値をプラウジビリティチェックで弾く」という具体的な
//       シナリオに最小限必要なこの 1 種類のみ実装する）
// -------------------------------------------------------
typedef enum
{
    COM_FILTER_ALWAYS = 0,
    COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD = 1,
    COM_FILTER_MASKED_NEW_DIFFERS_X = 2,
    COM_FILTER_NEW_IS_WITHIN = 3
} Com_FilterAlgorithmType;

// -------------------------------------------------------
// ComTransferProperty（SWS_Com_00742/00743 相当、Signal Group メンバー専用）
//
//   非 Signal Group のシグナルは、そのシグナル単体が I-PDU の送信要否を
//   決めるため「送信を引き起こすか」を別フィールドで宣言する必要がない
//   （ComFilterAlgorithm の評価結果がそのまま Com_RequestTxOnChange() 呼び出し
//   の可否になる。Com_SendSignal() 参照）。
//
//   一方 Signal Group は複数シグナルが 1 つの I-PDU 送信を共有するため、
//   「グループ内のどのメンバーの変化が送信の引き金になるか」を
//   メンバーごとに区別する必要がある。実 AUTOSAR の ComTransferProperty は
//   PENDING/TRIGGERED/TRIGGERED_ON_CHANGE/TRIGGERED_ON_CHANGE_WITHOUT_REPETITION
//   の 4 値を持つが、本実装は具体的な使用シーン（WarningStatus の各警告灯）に
//   必要な最小限としてこの 2 値のみ実装する。
//
//   COM_TRANSFER_PROPERTY_PENDING (既定)
//     : このメンバー自身の値変化では Signal Group の送信を引き起こさない。
//       値は Com_SendSignalGroup() のコミット時に他メンバー同様バッファへ
//       反映されるが、「送信のトリガー」にはならない（SWS_Com_00743:
//       他の TRIGGERED* メンバーが送信を引き起こしたときに便乗して運ばれる）。
//   COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE
//     : Com_SendSignal() 呼び出しのたびに前回値と比較し、変化していれば
//       Signal Group 全体の送信を引き起こす（SWS_Com_00742）。
//
//   実装上の注意: この「前回値との比較」は ComFilterAlgorithm/Mask/FilterX
//   とは独立した仕組みである（Com_SendSignal() 内で Com_FilterLastValue[] を
//   使い、マスクなしの生値同士を比較する）。同じシグナルが TmsContributor=1
//   として ComFilterAlgorithm=COM_FILTER_MASKED_NEW_DIFFERS_X を TMS 評価に
//   使っていても（例: FaultLamp/AbsLamp）、ComTransferProperty の判定とは
//   競合しない（TMS は「どの送信モードを使うか」、ComTransferProperty は
//   「今回のコミットで送信を引き起こすか」という別々の問いに答えるため）。
// -------------------------------------------------------
typedef enum
{
    COM_TRANSFER_PROPERTY_PENDING             = 0,
    COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE = 1
} Com_TransferPropertyType;

// -------------------------------------------------------
// ComRxDataTimeoutAction（ECUC_Com_00412 相当、RX シグナル専用）
//
//   RX 受信デッドライン監視タイマが満了した際、Com_ReceiveSignal() が
//   そのシグナルに対してどう振る舞うかを決める。
//
//   COM_RX_TIMEOUT_ACTION_NONE (既定)
//     : 何も置き換えない。本実装は実 AUTOSAR の「最後の正常値を黙って
//       返し続ける」動作ではなく、より安全側に倒した簡略化として
//       Com_ReceiveSignal() が E_NOT_OK を返し値を書き込まない
//       （呼び出し元が事前にセットした初期値 = 安全値をそのまま使わせる）。
//       既存のすべてのシグナルはこの既定動作のまま変更されない。
//   COM_RX_TIMEOUT_ACTION_SUBSTITUTE
//     : タイムアウト中、Com_ReceiveSignal() は I-PDU バッファ（正常受信時の
//       実データ）を読まず、代わりにこのシグナルの `TimeoutSubstitutionValue`
//       を書き込んで E_OK を返す（ECUC_Com_10006: ComTimeoutSubstitutionValue
//       相当）。根拠要求は対象シグナルが Signal Group のメンバーかどうかで
//       異なる: 非グループシグナルは SWS_Com_00875（"the reception deadline
//       monitoring timer of a signal expires"）、Signal Group メンバーは
//       SWS_Com_00876（"...of a signal group expires"）。
//
//       Signal Group メンバーの場合の注意: この「タイマ満了」判定は
//       Com_ReceiveSignal() 呼び出し時点のライブな Com_RxTimedOut[] ではなく、
//       直近の Com_ReceiveSignalGroup() 呼び出し時点でスナップショットした
//       Com_RxShadowTimedOut[] を見る（Com_ReceiveSignalGroup() 自身は
//       呼び出し時点のライブな Com_RxTimedOut[] を見ており、この点は
//       SWS_Com_00876 と整合する）。これは本プロジェクトの呼び出し方（Rte
//       が常にフレーム受信直後にしか読まない）に起因する制約ではなく、
//       Signal Group が「Com_ReceiveSignal() はシャドウバッファのみを読み、
//       ライブな I-PDU バッファ・ライブなタイムアウト状態には一切触れない」
//       という設計そのものに由来する（実 AUTOSAR も同様: 3212-3216 行
//       "the RTE accesses the group signals in the shadow buffer" 参照）。
//       したがって、たとえ将来 ASW がフレーム受信タイミングと無関係な任意の
//       時刻で Com_ReceiveSignal() を呼ぶ構成に変えても、その直前に
//       Com_ReceiveSignalGroup() を呼ばない限り、実際にタイムアウトが
//       発生した瞬間ではなく「最後に Com_ReceiveSignalGroup() を呼んだ時点」
//       の状態しか観測できない。
//
//   実 AUTOSAR にはもう 1 つ REPLACE（SWS_Com_00470: シグナルの
//   ComSignalInitValue で置き換える）があるが、本実装は ComSignalInitValue
//   という設定概念自体を持たない（RX バッファは Com_Init() で単純に
//   ゼロクリアするのみ）。REPLACE は「タイムアウト時 = 起動直後と同じ
//   初期値」を返すだけで、本物のゼロ値（例: 停車中の VehicleSpeed=0）との
//   区別がつかない。一方 SUBSTITUTE は起動直後の初期値とは異なる、明確に
//   「異常」とわかる値（例: VehicleSpeed に 0xFFFF）を設定できるため、
//   本実装ではこちらのみを実装する。
// -------------------------------------------------------
typedef enum
{
    COM_RX_TIMEOUT_ACTION_NONE       = 0,
    COM_RX_TIMEOUT_ACTION_SUBSTITUTE = 1
} Com_RxDataTimeoutActionType;

// -------------------------------------------------------
// ComDataInvalidAction（ECUC_Com_00314 相当、RX シグナル専用）
//
//   ComRxDataTimeoutAction（上記）が「一定時間フレームが来ない」という
//   時間ベースの異常を扱うのに対し、こちらは「フレームは届いているが、
//   中身の値そのものが送信元によって明示的に『無効』とマークされている」
//   という値ベースの異常を扱う（例: センサ断線を検知した ECU が、
//   物理的にあり得ない特定ビットパターンを意図的に送信する）。
//   受信したシグナル値が、このシグナルに設定した `InvalidValue`
//   （ECUC_Com_00391: ComSignalDataInvalidValue 相当）と一致した場合の
//   Com_ReceiveSignal() の振る舞いを決める。
//
//   COM_DATA_INVALID_ACTION_NONE (既定)
//     : 無効値チェックを行わない。受信値をそのまま返す（既存の全シグナルの
//       挙動のまま変更されない）。
//   COM_DATA_INVALID_ACTION_NOTIFY
//     : 受信値が InvalidValue と一致する間、Com_ReceiveSignal() はその値を
//       シグナルの内部状態へ反映せず、直近の有効値を返し続ける
//       （SWS_Com_00680/00717: "shall not store the received
//       ComSignalDataInvalidValue into the signal object... The next call to
//       Com_ReceiveSignal will return the last valid received signal"）。
//       あわせて `InvalidNotificationCbk`（ECUC_Com_00315:
//       ComInvalidNotification 相当）が非 NULL なら呼び出す。ただし
//       Com_ReceiveSignal() の呼び出しスタックフレームで同期的に呼ぶのでは
//       なく、次回 Com_MainFunction() から呼ぶ（Com_ReceiveSignal() は
//       割り込み禁止区間から呼ばれることがあり、その中でコールバックを
//       直接呼ぶとコールバックのブロッキング処理が割り込み禁止を長引かせ
//       うるため。詳細は Com.c の Com_RxInvalidNotifyPending 宣言コメント
//       参照）。
//
//   実 AUTOSAR にはもう 1 つ REPLACE（SWS_Com_00681: ComSignalInitValue で
//   置き換えたうえで通常のシグナル処理を続行する）があるが、
//   Com_RxDataTimeoutActionType の REPLACE を実装しなかった理由と同じく、
//   本実装は ComSignalInitValue という設定概念自体を持たないため未実装。
// -------------------------------------------------------
typedef enum
{
    COM_DATA_INVALID_ACTION_NONE   = 0,
    COM_DATA_INVALID_ACTION_NOTIFY = 1
} Com_DataInvalidActionType;

// -------------------------------------------------------
// TX 送信モード（ComTxModeMode 相当、簡略版）
//
//   実 AUTOSAR の ComTxModeMode は DIRECT/PERIODIC/MIXED/NONE を持つ。
//   本実装は以下の 3 値をサポートする。いずれも ASW/CDD は
//   Com_SendSignal() / Com_SendSignalGroup() で値を更新するだけでよく、
//   送信要否・タイミングの判断は一切 Com 自身が行う
//   （実車の Com と同じ「値の生成」と「送信タイミング」の責務分離。
//   SWS_Com_00734/00742/00743: DIRECT/MIXED は変化検知した signal(group)の
//   send request が「次回メイン関数までに」送信を開始させる。本実装では
//   実際の PduR_Transmit() 呼び出しを必ず Com_MainFunction() 側で行う
//   ことで、この「次回メイン関数まで」の猶予をそのまま「ASW Runnable の
//   スタックフレームで SPI 送信までブロッキングさせない」設計に利用している）。
//
//   COM_TX_MODE_DIRECT   : ComFilterAlgorithm を通過した変化があると
//     次回 Com_MainFunction() で送信する。周期フロアは持たない
//     （WarningStatus が使用。他 ECU の制御判断に使われないダッシュボード
//     表示用ミラー情報のため、取りこぼしても実害が小さいと判断した）。
//
//   COM_TX_MODE_MIXED    : DIRECT と同じ変化時送信に加えて、
//     一定時間（TxPeriodMs）変化がなくても周期フロアとして再送する
//     （MeterStatus が使用。他 ECU がエンジン状態を判断材料にするため、
//     1 回きりのイベント送信だけでは起動直後の受信側や瞬断からの復帰後に
//     いつまでも古い値のままになりうる。実 MIXED モードの簡略版で、
//     フロアタイマーは直近の送信［変化時送信/フロアいずれも］でリセットする
//     簡略化のため、実車の ComMinimumDelayTime による MDT 満了待ちでの
//     厳密な位相シフト［Fig.16/17 の td 分の遅延］までは再現しない。MDT
//     自体（変化時送信の最小間隔）は MinDelayMs として別途サポートする）。
//
//   COM_TX_MODE_PERIODIC : 値の変化には反応せず、Com_MainFunction()
//     が TxPeriodMs 周期で常に送信する（E2EHealthStatus が使用）。
//
//   1 つの I-PDU に上記モードを 1 つだけ固定で持たせるのではなく、TMS
//   （Transmission Mode Selector）により状況に応じて 2 つのモードを
//   自動切り替えすることもできる（WarningStatus が使用。通常は DIRECT、
//   FAULT/ABS 警告灯が点灯中は MIXED へ切り替わる。詳細は
//   Com_IPduConfigType の TxModeModeTrue の説明を参照）。
// -------------------------------------------------------
typedef enum
{
    COM_TX_MODE_MIXED    = 0,
    COM_TX_MODE_PERIODIC = 1,
    COM_TX_MODE_DIRECT   = 2
} Com_TxModeModeType;

// -------------------------------------------------------
// I-PDU 設定（1エントリ = 1つの I-PDU）
//
//   IPduId    : COM 内の I-PDU インデックス（0始まり）
//   DLC       : PDU のバイト長 = 内部バッファサイズ
//   PduRId    : PduR 空間での ID
//               RX → Com_RxIndication に渡される DestPduId と一致させる
//               TX → PduR_Transmit に渡す SrcPduId と一致させる
//   TimeoutMs : RX 受信デッドライン [ms]（DaVinci: ComRxDeadlineMonitoringPeriod）
//               0 = 監視無効。TX I-PDU では 0 を設定すること。
//   IsSignalGroup : TX/RX 両方の I-PDU で使用。
//               TX: 1 = Signal Group（Com_SendSignal はシャドウバッファへ
//               書き込むのみとし、Com_SendSignalGroup() でまとめて実バッファへ
//               確定コミットする）。0 = 通常の直接送信（Com_SendSignal が
//               その場で実バッファへ書き込む、既存の挙動）。
//               RX: 1 = Signal Group（Com_ReceiveSignal はこの I-PDU の
//               シグナルを RX シャドウバッファから読む。Com_ReceiveSignalGroup()
//               が I-PDU バッファ → RX シャドウバッファへ確定コピーするまで
//               更新されない）。0 = 通常の直接受信（Com_ReceiveSignal が
//               I-PDU バッファを直接読む、既存の挙動）。詳細は
//               Com_ReceiveSignalGroup() の \AUTOSARReq 参照。
//   TxModeMode / TxPeriodMs : TX I-PDU のみ使用（DaVinci: ComTxModeFalse /
//               ComTxModeTimePeriodFactor）。TMS（下記）が false と評価された
//               ときに使うモード。TMS を持たない（TmsContributor なシグナルが
//               存在しない）I-PDU では常にこちらだけを使う。COM_TX_MODE_DIRECT
//               では TxPeriodMs は未使用。MIXED では周期フロア間隔として、
//               PERIODIC では厳密な送信周期として TxPeriodMs [ms] を使う。
//   TxModeModeTrue / TxPeriodMsTrue : TX I-PDU のみ使用（DaVinci:
//               ComTxModeTrue / ComTxModeTimePeriodFactor）。TMS（Transmission
//               Mode Selector、SWS_Com_00032/00799/00676-00679 相当）が true と
//               評価されたときに使うモード。TMS は、この I-PDU に属する
//               シグナルのうち TmsContributor=1 のものを対象に
//               ComFilterAlgorithm=COM_FILTER_MASKED_NEW_DIFFERS_X を評価し、
//               いずれか 1 つでも真なら true（SWS_Com_00678、OR 合成）、
//               1 つも無ければ false（SWS_Com_00679）とする。Com_SendSignal()/
//               Com_SendSignalGroup() のたびに再評価する（SWS_Com_00245）。
//               ASW/CDD は TMS の存在を一切意識せず、値をセットするだけでよい。
//   RxIndicationCbk : RX I-PDU のみ使用。非NULL なら Com_RxIndication() が
//               バッファ更新後に呼ぶ（実 AUTOSAR の ComNotification /
//               RTE 生成コールバックに相当）。E2E Transformer 等、
//               「フレーム受信の都度」処理が必要な上位層向けの汎用フックで
//               あり、Com はここで何が実行されるか一切関知しない
//               （IPduId のハードコード比較を Com.c 本体に埋め込まないため）。
//   TxTransformCbk  : TX I-PDU のみ使用。非NULL なら実際の送信直前（DIRECT/
//               MIXED はイベント駆動の送信直前、PERIODIC は Com_MainFunction()
//               内部の周期送信直前）に、実 TX バッファへのポインタと長さを
//               渡して呼ぶ。E2E Transformer が Counter/CRC をバッファへ
//               書き込む等の「送信直前の最終変換」に使う汎用フック。
//   MinDelayMs      : TX I-PDU のみ使用（DaVinci: ComMinimumDelayTime）。
//               DIRECT/MIXED I-PDU の「変化時送信」に対する最小送信間隔 [ms]。
//               0 = MDT 監視なし（SWS_Com_00471）。直近の実送信から
//               MinDelayMs 未満しか経過していない場合、Com_TxPending が
//               立っていても実送信を保留する（破棄はしない。次回
//               Com_MainFunction() で経過時間を再判定し、満了次第送信する）。
//               MIXED の周期フロア送信・PERIODIC の周期送信には適用しない
//               （SWS_Com_00789: ComEnableMDTForCyclicTransmission が既定
//               false の場合、MIXED の周期部分・PERIODIC には MDT タイマ
//               自体を起動しない、という実 AUTOSAR の既定動作に合わせている）。
//   UpdateBitPosition : Signal Group（IsSignalGroup=1）のみ使用（DaVinci:
//               ComUpdateBitPosition、ECUC_Com_00257 相当）。この Signal
//               Group 用 update-bit の I-PDU 内ビット位置（ネットワークビット
//               順、Com_UnpackSignal/Com_PackSignal と同じ規約）。
//               0xFF = update-bit なし（既定。実 AUTOSAR の「省略時は
//               update-bit なし」に対応）。
//
//               update-bit（7.8 章、SWS_Com_00055 他）: 送信側が「このグループ
//               の値を実際に更新して送った」ことを受信側へ伝える 1 ビット
//               （値そのものとは独立。SWS_Com_00055: シグナル/グループの
//               一部としてではなく Com 内部でのみ扱う）。
//                 TX（送信側、SWS_Com_00801）: Com_SendSignalGroup() が呼ばれる
//                   たびにこのビットを 1 にセットする。クリアタイミングは
//                   実 AUTOSAR の ComTxIPduClearUpdateBit（Transmit/
//                   Confirmation/TriggerTransmit の 3 択）のうち、本実装は
//                   Transmit のみ実装する（SWS_Com_00062: PduR_ComTransmit
//                   呼び出し直後にクリア。Confirmation/TriggerTransmit は
//                   未実装。詳細は Com_DoTransmit() 参照）。
//                 RX（受信側、SWS_Com_00324/00802）: Com_ReceiveSignalGroup()
//                   はこのビットが 0 の場合、受信データを「破棄」する
//                   （シャドウバッファ・タイムアウトスナップショットとも
//                   直近の状態のまま更新しない）。1 の場合のみ通常どおり
//                   確定コピーする。
//   IpduGroupId : この I-PDU が所属する I-PDU Group（DaVinci: ComIPduGroup
//               への参照、7.3.5 章）。COM_IPDU_GROUP_NONE（既定）を設定すると
//               どの I-PDU Group にも属さないものとして扱われ、Com_Init() で
//               常に開始済み（Started）となり、Com_IpduGroupStart/Stop() の
//               対象にならない（[SWS_Com_00840]）。それ以外の値を設定すると、
//               その I-PDU は既定で停止（Started=0）状態で初期化され
//               （[SWS_Com_00444]: I-PDU Group は既定で全て停止状態）、
//               Com_IpduGroupStart(IpduGroupId, ...) が呼ばれるまで
//               送信/受信処理が行われない。停止中の RX I-PDU は
//               Com_RxIndication() が受信処理自体を無効化し（[SWS_Com_00684]）、
//               受信デッドライン監視も評価しない（[SWS_Com_00685]）。停止中の
//               TX I-PDU は Com_MainFunction() が実送信を行わず、保留中の
//               送信要求は Com_IpduGroupStop() の時点でキャンセルされる
//               （[SWS_Com_00777]）。Com_SendSignal()/Com_ReceiveSignal() 自体は
//               停止中でも内部バッファを更新・参照できる（[SWS_Com_00334]、
//               「値のセット」と「送信/受信タイミング」は独立した責務のため）。
// -------------------------------------------------------
typedef struct
{
    Com_IPduIdType     IPduId;
    uint8              DLC;
    PduIdType          PduRId;
    uint16             TimeoutMs;
    uint8              IsSignalGroup;
    Com_TxModeModeType TxModeMode;
    uint16             TxPeriodMs;
    Com_TxModeModeType TxModeModeTrue;
    uint16             TxPeriodMsTrue;
    uint16             MinDelayMs;
    uint8              UpdateBitPosition;
    Com_IpduGroupIdType IpduGroupId;
    void (*RxIndicationCbk)(void);
    void (*TxTransformCbk)(uint8* Data, uint8 Length);
} Com_IPduConfigType;

// -------------------------------------------------------
// Com_SignalDirectionType（本プロジェクト独自拡張。DaVinci 対応パラメータなし）
//
//   実 AUTOSAR には対応する設定パラメータが無い。実 AUTOSAR では ComSignal は
//   必ずどちらか一方の ComIPdu（RX/TX で別コンテナ）に一意に含まれる構造の
//   ため、「このシグナルが RX/TX どちらか」を別途持つ必要がない。
//
//   本プロジェクトは RX/TX 共通の 1 本の配列（Com_ConfigType.Signals）に
//   全シグナルを平坦に並べ、所属 I-PDU は IPduId（数値）で示す簡略設計を
//   採っている。ところが RX I-PDU と TX I-PDU の IPduId は別々の値空間で
//   0 から振られており数値が重複する（例: RX の EngineInfo=0 と TX の
//   MeterStatus=0）。そのため「IPduId が一致する」だけでは、そのシグナルが
//   問い合わせた方向（RX か TX か）のものかを判別できない。
//
//   この Direction フィールドは、Com_TxConfirmation() が Com_ConfigPtr->
//   Signals[] 全体を IPduId だけで走査して TxAckCbk 対象を探す際に必要になった
//   （Com_SendSignal()/Com_ReceiveSignal() は SignalId で個別の 1 エントリを
//   検索してから Com_FindTxIPdu()/Com_FindRxIPdu() で方向を確認する設計のため、
//   この曖昧さの影響を受けない。走査型の処理でのみ問題になる）。
// -------------------------------------------------------
typedef enum
{
    COM_SIGNAL_DIRECTION_RX = 0,
    COM_SIGNAL_DIRECTION_TX = 1
} Com_SignalDirectionType;

// -------------------------------------------------------
// Signal 設定（1エントリ = 1つのシグナル）
//
//   SignalId    : アプリ層が使うシグナル識別子（Com_ReceiveSignal / Com_SendSignal のキー）
//   Direction   : このシグナルが RX/TX どちらの I-PDU に属するか。
//                 詳細は Com_SignalDirectionType 参照。
//   IPduId      : このシグナルが属する I-PDU の ID（Direction 側の値空間。
//                 RX と TX で数値が重複しうるため、Direction とセットで
//                 初めて一意に I-PDU を特定できる）
//   BitPosition : I-PDU 内の先頭ビット位置
//                 big-endian  → MSB のビット番号
//                 little-endian → LSB のビット番号
//   BitSize     : シグナルのビット長（1〜32）
//   Endian      : ビットの詰め方向
//   FilterAlgorithm / Mask / FilterX : TX シグナルの送信要否フィルタ・TMS 評価
//               フィルタ。FilterX は COM_FILTER_MASKED_NEW_DIFFERS_X 専用の
//               比較値（MASKED_NEW_DIFFERS_MASKED_OLD では未使用）。
//   FilterAlgorithm(NEW_IS_WITHIN) / FilterMin / FilterMax / FilterRejectCbk :
//               RX シグナルの受信フィルタ（Com_FilterAlgorithmType の用途 (3)
//               参照）。COM_FILTER_NEW_IS_WITHIN のときのみ FilterMin/FilterMax
//               を参照し、範囲外の受信値を破棄する。FilterRejectCbk は NULL 可
//               （通知不要なら未設定でよい。DataInvalidAction の
//               InvalidNotificationCbk と同じ理由で、Com_MainFunction() から
//               呼ばれる — Com_ReceiveSignal() が割り込み禁止区間から呼ばれる
//               ことがあるため。Com.c の Com_RxFilterRejectPending 宣言コメント
//               参照）。
//   TmsContributor : TX シグナルのみ使用。1 = このシグナルの
//               ComFilterAlgorithm 評価結果が、所属 I-PDU の TMS
//               （Com_IPduConfigType.TxModeModeTrue 参照）へ寄与する
//               （SWS_Com_00676）。0 = TMS 計算に一切関与しない（既定）。
//   TransferProperty : Signal Group（所属 I-PDU が IsSignalGroup=1）の
//               メンバーのみ使用。詳細は Com_TransferPropertyType 参照。
//               Signal Group でないシグナルでは未使用（FilterAlgorithm が
//               単独で送信要否を決めるため）。
//   RxDataTimeoutAction / TimeoutSubstitutionValue : RX シグナルのみ使用。
//               詳細は Com_RxDataTimeoutActionType 参照。
//               COM_RX_TIMEOUT_ACTION_SUBSTITUTE のときのみ
//               TimeoutSubstitutionValue を参照する。
//   DataInvalidAction / InvalidValue / InvalidNotificationCbk : RX シグナル
//               のみ使用。詳細は Com_DataInvalidActionType 参照。
//               COM_DATA_INVALID_ACTION_NOTIFY のときのみ InvalidValue /
//               InvalidNotificationCbk を参照する。InvalidNotificationCbk は
//               NULL 可（通知不要なら未設定でよい）。
//   TxAckCbk    : TX シグナルのみ使用。非 NULL なら、このシグナルが属する
//               I-PDU の送信が成功するたびに Com_TxConfirmation() から
//               呼ばれる（Com_CbkTxAck、SWS_Com_00468 相当）。このシグナル
//               自体の値がその送信で変化したかどうかは問わない。NULL 可
//               （通知不要なら未設定でよい）。
// -------------------------------------------------------
typedef struct
{
    Com_SignalIdType            SignalId;
    Com_SignalDirectionType     Direction;
    Com_IPduIdType              IPduId;
    uint8                       BitPosition;
    uint8                       BitSize;
    Com_SignalEndianType        Endian;
    Com_FilterAlgorithmType     FilterAlgorithm;
    uint32                      Mask;
    uint32                      FilterX;
    uint32                      FilterMin;
    uint32                      FilterMax;
    void (*FilterRejectCbk)(void);
    uint8                       TmsContributor;
    Com_TransferPropertyType    TransferProperty;
    Com_RxDataTimeoutActionType RxDataTimeoutAction;
    uint32                      TimeoutSubstitutionValue;
    Com_DataInvalidActionType   DataInvalidAction;
    uint32                      InvalidValue;
    void (*InvalidNotificationCbk)(void);
    void (*TxAckCbk)(void);
} Com_SignalConfigType;

// -------------------------------------------------------
// Signal Gateway（ComGwMapping、7.2.5/7.11 章）
//
//   RX シグナルの値を、SWC/Rte を一切介さずに Com 内部で直接 TX シグナルへ
//   転送する仕組み（[SWS_Com_00357]）。実 AUTOSAR の記述どおり
//   "the signal processing does not differ if the integrated Signal Gateway
//   forwards a signal ... or if a Software Component sends it"（7.2.5）を
//   そのまま実装で表現するため、本実装は「RX バッファから生値をアンパック
//   → Com_SendSignal(DestSignalId, ...) を呼ぶ」という、SWC が自分で
//   Com_SendSignal() を呼ぶのと全く同じ経路を内部で流用する（フィルタ・TMS・
//   送信要否判定は Com_SendSignal() 側の既存ロジックがそのまま適用される）。
//
//   RX 側の処理段階は [SWS_Com_00872] のとおり「1) デッドライン監視タイマ
//   再始動 2) I-PDU callout（RxIndicationCbk） 3) update-bit 確認
//   4) エンディアン変換」の順であり、ComDataInvalidAction/ComFilterAlgorithm
//   （NEW_IS_WITHIN 等、Com_ReceiveSignal() 側のみのゲート）は経由しない
//   （[SWS_Com_00701]: デッドライン監視タイムアウト中でもゲートウェイは
//   ルーティングを行う、という要求とも整合する。本実装はフレーム受信直後の
//   同期呼び出しのため、そもそもタイムアウト状態になり得ない）。
//
//   本実装は非 Signal Group のシグナル同士（1 RX シグナル → 1 TX シグナル）
//   のみをサポートする。Signal Group のゲートウェイ（[SWS_Com_00361]/
//   [SWS_Com_00383]: グループを一貫した集合として転送する要求）や
//   update-bit 連動（[SWS_Com_00702]〜[SWS_Com_00706]）は、具体的な
//   実機検証シナリオが無いため未実装（本プロジェクトの他機能と同じ
//   「実利より学習効果、ただし検証可能な範囲に留める」方針）。
//
//   SrcSignalId  : ゲートウェイ元の RX シグナル ID
//                  （Com_SignalConfigType.Direction=RX のシグナルであること）。
//   DestSignalId : ゲートウェイ先の TX シグナル ID
//                  （Com_SignalConfigType.Direction=TX のシグナルであること）。
// -------------------------------------------------------
typedef struct
{
    Com_SignalIdType SrcSignalId;
    Com_SignalIdType DestSignalId;
} Com_GwMappingType;

// -------------------------------------------------------
// COM 全体設定（Com_Init に渡す）
//
//   RX / TX それぞれに IPdu テーブルを持つ。
//   Signal テーブルは RX / TX 共通の 1 本の配列。RX I-PDU と TX I-PDU の
//   IPduId は別々の値空間（どちらも 0 始まり、数値が重複しうる）のため、
//   IPduId 単体では所属 I-PDU を一意に特定できない。Direction とセットで
//   区別する（Com_SignalDirectionType 参照）。
// -------------------------------------------------------
typedef struct
{
    const Com_IPduConfigType*   RxIPdus;       // RX I-PDU テーブル
    uint8                       RxIPduCount;
    const Com_IPduConfigType*   TxIPdus;       // TX I-PDU テーブル
    uint8                       TxIPduCount;
    const Com_SignalConfigType* Signals;        // シグナルテーブル
    uint8                       SignalCount;
    const Com_GwMappingType*    GwMappings;     // Signal Gateway ルーティングテーブル
    uint8                       GwMappingCount;
} Com_ConfigType;

#endif

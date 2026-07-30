/**
 * \file    Nm_Cfg.h
 * \brief   ネットワークマネジメント プリコンパイル設定 (AUTOSAR SWS_CANNM 準拠)
 * \details Nm が送受信する NM PDU のパラメータと、CanNm 状態機械のタイマ値を
 *          定義する。
 *
 *          本プロジェクトでの位置づけ:
 *            実車の CanNm は、各 ECU が周期的に「生存している」ことをバス上に
 *            示し、クラスタ内の全 ECU が NM フレームの送信を止めたときにのみ
 *            バススリープへ移行できる、という合意形成（協調スリープ）の仕組みを
 *            提供する。本プロジェクトは AUTOSAR_SWS_CANNetworkManagement.pdf の
 *            状態機械（Network Mode の Repeat Message/Normal Operation/Ready
 *            Sleep の3内部状態、Prepare Bus-Sleep Mode、Bus-Sleep Mode）を
 *            実装し、他ノード（uds_tester が模擬する「仮想他ECU」）からの
 *            NM フレーム受信が自ノードのスリープ判断に反映されることを実機で
 *            検証できるようにする。
 *
 *          対応除外（本プロジェクトのスコープ外）:
 *            - Partial Networking（部分ネットワーク起動、7.11章）
 *            - NM Coordinator Sync（複数ネットワーク協調シャットダウン、7.9.7章）
 *            - User Data（NM PDU にアプリデータを相乗りさせる機能、7.9.2章）
 *            - Remote Sleep Indication（CanNm_CheckRemoteSleepIndication、
 *              7.9.1章）・SetSleepReadyBit（7.9.7章関連）
 *            - Passive Mode（7.9.3章。本 ECU は常に能動的に NM フレームを送信する）
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様（docs/4.3.1/AUTOSAR_SWS_CANNetworkManagement.pdf）
 *          を参考にした学習用実装です。AUTOSAR 認証済み実装ではなく、製品への
 *          適用は想定していません。
 */
#ifndef NM_CFG_H
#define NM_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_CanNm 7.14.1 Development Errors 表に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * CAN Network Management (CanNm) に割り当てられた固定値 31 を使う。
 *
 * 本モジュールは実際の CanNm と異なり NetworkHandle/PDU-ID 等の引数を
 * 一切取らない単一チャネル固定の簡略設計のため、範囲チェック系の DET コード
 * （CANNM_E_INVALID_CHANNEL/CANNM_E_INVALID_PDUID）は対象が存在しない。
 * ----------------------------------------------------------------------- */

/** AUTOSAR CAN Network Management の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 31） */
#define NM_MODULE_ID  31U

/** 開発エラーコード（SWS_CanNm 7.14.1 表より実測して確認済み） */
#define NM_E_UNINIT           0x01U  /* [SWS_CanNm_00002/00191]: 未初期化時の API 呼び出し */
#define NM_E_NET_START_IND    0x04U  /* [SWS_CanNm_00336]: Bus-Sleep Mode 中に NM PDU を受信 */
#define NM_E_NETWORK_TIMEOUT  0x11U  /* [SWS_CanNm_00193/00194]: Repeat Message/Normal
                                      *  Operation State で NM-Timeout Timer が満了 */
#define NM_E_PARAM_POINTER    0x12U  /* NULL ポインタチェック */

/** ApiId（値は docs/4.3.1/AUTOSAR_SWS_CANNetworkManagement.pdf の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define NM_API_ID_INIT                     0x00U
#define NM_API_ID_NETWORK_REQUEST          0x02U
#define NM_API_ID_NETWORK_RELEASE          0x03U
#define NM_API_ID_REPEAT_MESSAGE_REQUEST   0x08U
#define NM_API_ID_GET_STATE                0x0BU
/** Nm_SetTxEnabled は本プロジェクト独自の診断連携関数（UDS 0x28
 *  CommunicationControl 用、有効/無効を1つの bool 引数で扱う）であり、
 *  AUTOSAR 標準の CanNm_DisableCommunication(0x0c)/CanNm_EnableCommunication(0x0d)
 *  に 1:1 対応しないため、その代表として前者の値を踏襲する。 */
#define NM_API_ID_SET_TX_ENABLED           0x0CU
#define NM_API_ID_MAIN_FUNCTION            0x13U
#define NM_API_ID_TX_CONFIRMATION          0x40U
#define NM_API_ID_RX_INDICATION            0x42U
#define NM_API_ID_DEINIT                   0x10U
#define NM_API_ID_GET_VERSION_INFO         0xF1U

/** バージョン情報（CanNm_GetVersionInfo、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define NM_VENDOR_ID          0U
#define NM_SW_MAJOR_VERSION   1U
#define NM_SW_MINOR_VERSION   0U
#define NM_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * NM フレーム送受信パラメータ
 * ----------------------------------------------------------------------- */

/** Nm_MainFunction() の呼び出し周期 [ms]（Os_PBCfg.c のタスク周期と一致させること）。
 *  MeterStatus (3000ms) より短い周期で、バス上の生存確認を高頻度に行うという
 *  NM 本来の性質を反映する。
 *
 *  実 CanNm の Message Cycle Timer（`CanNmMsgCycleTime`、[SWS_CanNm_00032]/
 *  [SWS_CanNm_00040]。Repeat Message/Normal Operation State での周期送信を
 *  NM-Timeout Timer とは独立に駆動する専用タイマ）を兼ねる簡略化。本来は
 *  NM_TIMEOUT_MS よりも十分短い専用のタイマにすべきだが、本プロジェクトは
 *  Nm_MainFunction() 自体の呼び出し周期をそのまま Message Cycle Time として
 *  流用する（Nm.c 参照）。 */
#define NM_CYCLE_MS  1000UL

/** NM PDU のバイト長。byte[0]=Control Bit Vector, byte[1]=Source Node Identifier
 *  （[SWS_CanNm_00074]/[SWS_CanNm_00075] のデフォルト配置。User Data は
 *  本プロジェクトでは対応除外のため0バイト）。 */
#define NM_DLC  2U

/** 本 ECU（メータ ECU）の NM ノード識別子 */
#define NM_SOURCE_NODE_ID  0x01U

/** Control Bit Vector のビット位置（[SWS_CanNm_00045] 実測。本プロジェクトは
 *  Bit0（Repeat Message Request）のみ扱う。Bit3(Coordinator Sleep)/Bit4(Active
 *  Wakeup)/Bit6(Partial Network) は対応除外の機能に対応するビットのため未使用。 */
#define NM_CBV_BIT_REPEAT_MESSAGE_REQUEST  0x01U

/**
 * CanIf に登録した本 NM フレームの TxPduId。
 * CanIf_PBCfg.c の CanIf_TxPduConfigData 配列インデックスと一致させること
 * （Nm は PduR/Com を経由せず CanIf_Transmit() を直接呼ぶため、
 * 実車の CanNm と同様に Com スタックとは独立して動作する）。
 */
#define NM_CANIF_TX_PDU_ID  2U

/**
 * CanIf に登録した本 NM フレームの RxPduId（CanIf_RxPduConfigData 配列
 * インデックスと一致させること）。CanIf は HOH/CanId でエントリを検索して
 * Nm_RxIndication() を直接呼ぶため、この値自体は Nm 側では単一チャネル
 * ゆえ実質未使用（受信ログ表示にのみ使う）。
 */
#define NM_CANIF_RX_PDU_ID  4U

/* -----------------------------------------------------------------------
 * CanNm 状態機械タイマ値 [ms]
 * ([SWS_CanNm_00102]/[SWS_CanNm_00109]/[SWS_CanNm_00115] 準拠。
 *  実車では全ノードで同一値に設定することが要求される
 *  [Note after SWS_CanNm_00126] が、本プロジェクトは自ノードのみ実装のため
 *  値の選定は任意。NM_CYCLE_MS の周期で少なくとも1回は正常送信/受信できる
 *  余裕を持たせつつ、実機での検証待ち時間が長くなりすぎない値とした。)
 * ----------------------------------------------------------------------- */

/** NM-Timeout Timer。この時間内に NM PDU の送信確認または受信がなければ、
 *  Repeat Message/Normal Operation State では再送信、Ready Sleep State では
 *  Prepare Bus-Sleep Mode へ遷移する。 */
#define NM_TIMEOUT_MS  3000UL

/** Repeat Message State の滞在時間（CanNmRepeatMessageTime 相当）。 */
#define NM_REPEAT_MESSAGE_MS  1500UL

/** Prepare Bus-Sleep Mode の滞在時間（CanNmWaitBusSleepTime 相当）。 */
#define NM_WAIT_BUS_SLEEP_MS  1500UL

#endif /* NM_CFG_H */

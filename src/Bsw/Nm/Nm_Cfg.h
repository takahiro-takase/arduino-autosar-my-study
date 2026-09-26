/**
 * \file    Nm_Cfg.h
 * \brief   ネットワークマネジメントインタフェース プリコンパイル設定
 *          (AUTOSAR SWS_NetworkManagementInterface 準拠)
 * \details Nm（汎用・バス非依存の Network Management Interface）が使う
 *          DET 定数・ApiId・バージョン情報・チャネルハンドル値を定義する。
 *
 *          本プロジェクトでの位置づけ:
 *            実 AUTOSAR の Nm は ComM と各バス固有の Network Management
 *            モジュール（CanNm/FrNm/LinNm 等）との間の抽象化層であり、
 *            複数ネットワークにまたがる協調シャットダウン（NM Coordinator
 *            機能、7.2章）を担う。本プロジェクトは CAN 1 チャネルのみの
 *            単一 ECU 構成のため、NM Coordinator 機能は対応除外とし、
 *            Base functionality（7.1章）のみを実装する。
 *
 *          対応除外（本プロジェクトのスコープ外）:
 *            - NM Coordinator 機能全般（7.2章。Nm_MainFunction は
 *              [SWS_Nm_00279]/[SWS_Nm_00020] により NM Coordinator 機能
 *              専用のため未実装。Nm_SynchronizationPoint/
 *              Nm_CoordReadyToSleepIndication/Cancellation も同様）
 *            - Nm_PassiveStartUp（Passive Mode。CanNm 側が本プロジェクトの
 *              既存の対応除外方針により常に能動送信のため対応する相手がない）
 *            - Nm_SetUserData/GetUserData/GetPduData（User Data。CanNm 側の
 *              既存の対応除外方針と同じ）
 *            - Nm_CheckRemoteSleepIndication（Remote Sleep Indication。
 *              CanNm 側の既存の対応除外方針と同じ）
 *            - Nm_RemoteSleepIndication/RemoteSleepCancellation（NM
 *              Coordinator 内部専用のコールバック、[SWS_Nm_00192]/[00193]
 *              「Coordinator 無効時は空実装でよい」との明記あり）
 *            - Nm_PduRxIndication/StateChangeNotification/
 *              RepeatMessageIndication/TxTimeoutException/
 *              CarWakeUpIndication（8.4.2節「Extra」、いずれも「AUTOSAR の
 *              どのモジュールにも要求されない OEM 拡張向け」と明記され、
 *              本プロジェクトの CanNm.c もこれらを呼ばない）
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様
 *          （docs/autosar/4.3.1/AUTOSAR_SWS_NetworkManagementInterface.pdf）
 *          を参考にした学習用実装です。AUTOSAR 認証済み実装ではなく、
 *          製品への適用は想定していません。
 */
#ifndef NM_CFG_H
#define NM_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_Nm 7.6.1 Development Errors 表（Table 7.2）に基づく開発エラーコード。
 * ModuleId は AUTOSAR_TR_BSWModuleList（Release 4.3.1、docs/ 配下）の
 * 「List of Basic Software Modules」表で Network Management Interface (Nm)
 * に割り当てられた固定値 29 を使う（CanNm の 31 とは別モジュールのため
 * 別値）。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Network Management Interface の ModuleId（固定値 29） */
#define NM_MODULE_ID  29U

/** 開発エラーコード（Table 7.2 より実測して確認済み） */
#define NM_E_UNINIT           0x00U  /* [SWS_Nm_00232]: 未初期化時の API 呼び出し */
#define NM_E_INVALID_CHANNEL  0x01U  /* [SWS_Nm_00130]等: Channel が NM_MAIN_NETWORK_HANDLE 以外 */
#define NM_E_PARAM_POINTER    0x02U  /* NULL ポインタチェック */

/** ApiId（値は 8.3/8.4 章の「Service ID[hex]」記載を実測して確認済み） */
#define NM_API_ID_INIT                       0x00U
#define NM_API_ID_NETWORK_REQUEST            0x02U
#define NM_API_ID_NETWORK_RELEASE            0x03U
#define NM_API_ID_DISABLE_COMMUNICATION      0x04U
#define NM_API_ID_ENABLE_COMMUNICATION       0x05U
#define NM_API_ID_REPEAT_MESSAGE_REQUEST     0x09U
#define NM_API_ID_GET_NODE_IDENTIFIER        0x0AU
#define NM_API_ID_GET_LOCAL_NODE_IDENTIFIER  0x0BU
#define NM_API_ID_GET_STATE                  0x0EU
#define NM_API_ID_GET_VERSION_INFO           0x0FU
#define NM_API_ID_NETWORK_START_INDICATION   0x11U
#define NM_API_ID_NETWORK_MODE               0x12U
#define NM_API_ID_PREPARE_BUS_SLEEP_MODE     0x13U
#define NM_API_ID_BUS_SLEEP_MODE             0x14U

/** バージョン情報（CanNm 等の既存モジュールと同じ命名規則） */
#define NM_VENDOR_ID          0U
#define NM_SW_MAJOR_VERSION   1U
#define NM_SW_MINOR_VERSION   0U
#define NM_SW_PATCH_VERSION   0U

/** 本プロジェクトが持つ唯一の NM チャネルのハンドル値。呼び出し元はこの値を
 *  渡すこと。これ以外の値は NM_E_INVALID_CHANNEL として拒否される
 *  （CanNm 側の CANNM_MAIN_NETWORK_HANDLE と同じ値 0 だが、Nm/CanNm は
 *  別モジュールのため別定数として持つ）。 */
#define NM_MAIN_NETWORK_HANDLE  0U

#endif /* NM_CFG_H */

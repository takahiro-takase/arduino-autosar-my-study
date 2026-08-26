/**
 * \file    CanIf_Cfg.h
 * \brief   CAN インタフェース プリコンパイル設定 (AUTOSAR SWS_CANInterface 準拠)
 * \details CAN インタフェースモジュールのプリコンパイル設定を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *          TX / RX PDU テーブルのエントリ数をコンパイル時定数として提供し、
 *          CanIf_PBCfg.c での配列サイズ指定に使用する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANIF_CFG_H
#define CANIF_CFG_H

#include "Platform_Types.h"

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_CANInterface 7.27.1 Development Errors 表に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * CAN Interface (CanIf) に割り当てられた固定値 60 を使う。
 *
 * CanIf の「未初期化時」チェック（SWS_CANIF_00421/00412/00431 等）は、
 * Com/Can と異なり Det_ReportError を要求していない（「実行せず黙って
 * 戻る」とのみ規定）。そのため各関数の CanIf_ConfigPtr==NULL チェックは
 * 既存どおり DET 報告なしの早期 return のままとし、以下のパラメータ
 * 検証（範囲外 ID・NULL ポインタ）のみを Det_ReportError 対象とする。
 * ----------------------------------------------------------------------- */

/** AUTOSAR CAN Interface の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 60） */
#define CANIF_MODULE_ID  60U

/** 開発エラーコード（7.27.1 表より、実際に使用する分のみ） */
#define CANIF_E_PARAM_CANID         10U  /* [SWS_CANIF_00417]: Hoh 一致だが Mailbox->CanId が不一致 */
#define CANIF_E_PARAM_HOH           12U  /* [SWS_CANIF_00416]: Mailbox->Hoh に一致する設定なし */
#define CANIF_E_PARAM_CONTROLLERID  15U  /* [SWS_CANIF_00429 等]: ControllerId が範囲外 */
#define CANIF_E_PARAM_POINTER       20U  /* [SWS_CANIF_00320/00419 等]: NULL ポインタチェック */
#define CANIF_E_PARAM_LPDU          13U  /* [SWS_CANIF_00410]: CanTxPduId が範囲外 */
#define CANIF_E_INVALID_TXPDUID     50U  /* [SWS_CANIF_00319]: TxPduId が範囲外 */
#define CANIF_E_INVALID_RXPDUID     60U  /* [SWS_CANIF_00325]: CanIfRxSduId が範囲外、または
                                          *   CANIF_READRXPDU_DATA（ReadRxPduDataEnabled）未設定 */
#define CANIF_E_INIT_FAILED         80U  /* CAN Interface initialisation failed。
                                          *   RxPduCount/TxPduCount がコンパイル時上限
                                          *   （CANIF_RX_PDU_MAX 等）を超える設定を拒否する際に使う
                                          *   （Com_Cfg.h の COM_E_INIT_FAILED と同じ位置づけ）。 */
#define CANIF_E_PARAM_PDU_MODE      22U  /* [SWS_CANIF_00860]: CanIf_SetPduMode() の
                                          *   PduModeRequest が CanIf_PduModeType の
                                          *   定義値以外 */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define CANIF_API_ID_INIT                0x01U
#define CANIF_API_ID_DEINIT              0x02U
#define CANIF_API_ID_TRANSMIT            0x49U
#define CANIF_API_ID_TX_CONFIRMATION     0x13U
#define CANIF_API_ID_RX_INDICATION       0x14U
#define CANIF_API_ID_CONTROLLER_BUSOFF   0x16U
#define CANIF_API_ID_GET_VERSION_INFO    0x0BU
#define CANIF_API_ID_READ_RX_PDU_DATA    0x06U
#define CANIF_API_ID_SET_PDU_MODE        0x03U
#define CANIF_API_ID_GET_PDU_MODE        0x0AU

/** バージョン情報（SWS_CANInterface、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define CANIF_VENDOR_ID          0U
#define CANIF_SW_MAJOR_VERSION   1U
#define CANIF_SW_MINOR_VERSION   0U
#define CANIF_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * プリコンパイル設定定数
 * ----------------------------------------------------------------------- */

/** TX PDU テーブルのエントリ数（送信 CAN フレーム種別数）
 *  TxPduId=0: EngineState    (CAN 0x200, COM)
 *  TxPduId=1: UDS 診断応答   (CAN 0x7E8, DCM)
 *  TxPduId=2: NM フレーム    (CAN 0x400, Nm。PduR/Com を経由せず直接呼び出す)
 *  TxPduId=3: WarningStatus  (CAN 0x210, COM Signal Group)
 *  TxPduId=4: E2EHealthStatus (CAN 0x220, COM PERIODIC)
 *  TxPduId=5: ImmobilizerStatus (CAN 0x230, COM DIRECT。Signal Gateway 転送先) */
#define CANIF_TX_PDU_COUNT  6U

/** RX PDU テーブルのエントリ数（受信 CAN フレーム種別数）
 *  DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg ノード数
 *  RxPduId=0: EngineInfo     (CAN 0x100, COM)   エンジン ECU
 *  RxPduId=1: UDS 診断要求   (CAN 0x7E0, DCM)   診断ツール
 *  RxPduId=2: AbsInfo        (CAN 0x110, COM)   ABS ECU
 *  RxPduId=3: ImmobilizerCmd (CAN 0x120, SecOC) KeyFobEcu 想定
 *  RxPduId=4: NM フレーム    (CAN 0x400, Nm。PduR/Com を経由せず直接呼び出す) */
#define CANIF_RX_PDU_COUNT  5U

/** CanIf_ReadRxPduData()（2026-08 追加）の内部バッファ配列サイズ。
 *  Com_Cfg.h の COM_RX_IPDU_MAX と同じ規約（CANIF_RX_PDU_COUNT に連動）。
 *  この配列は native_chain バイナリ全体で共有される固定サイズのため、
 *  テストローカルの CanIf_ConfigType.RxPduCount がこれを超えないこと
 *  （feedback_test_chain_ipdu_id_ceiling 参照。超過は範囲外書き込みによる
 *  無関係なテストの原因不明なハングを引き起こす）。 */
#define CANIF_RX_PDU_MAX    CANIF_RX_PDU_COUNT

/** CAN classic の最大データ長 [byte]（CAN FD は本プロジェクト未対応、
 *  project_can_fd_postponed 参照）。CanIf_ReadRxPduData() の内部バッファ
 *  1 エントリのサイズに使う。 */
#define CANIF_MAX_DLC        8U

/** CanIf_SetPduMode()/CanIf_GetPduMode()（2026-08 追加）の内部状態配列
 *  CanIf_ControllerPduMode[] のサイズ。Can_Cfg.h の CAN_CONTROLLER_COUNT と
 *  同じ値（本プロジェクトは単一コントローラ構成のため 1）。CanIf は
 *  Can_Cfg.h に依存しない既存方針のため、値を複製する形で独立して持つ。 */
#define CANIF_CONTROLLER_MAX 1U

#endif /* CANIF_CFG_H */

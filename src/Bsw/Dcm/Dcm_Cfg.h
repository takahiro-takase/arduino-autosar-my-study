/**
 * \file    Dcm_Cfg.h
 * \brief   DCM プリコンパイル設定 (AUTOSAR SWS_DCM 準拠)
 * \details DCM モジュールのコンパイル時定数を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *
 *          本プロジェクトで対応するサービス:
 *            0x10 DiagnosticSessionControl (Default / Extended)
 *            0x11 ECUReset (hardReset / softReset)
 *            0x14 ClearDiagnosticInformation (groupOfDTC=0xFFFFFF)
 *            0x19 ReadDTCInformation (subFunc 0x01/0x02) — マルチフレーム対応
 *            0x22 ReadDataByIdentifier (DID 0x0101-0x0103)
 *            0x3E TesterPresent
 *
 *          ISO 15765-2 (CAN TP) トランスポート層は CanTp モジュールが担当する。
 *          本モジュールは PCI バイトを含まない生 UDS ペイロードのみを扱う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DCM_CFG_H
#define DCM_CFG_H

/* -----------------------------------------------------------------------
 * UDS セッション識別子 (ISO 14229-1 Table 14)
 * ----------------------------------------------------------------------- */
#define DCM_SESSION_DEFAULT   0x01U  /**< defaultSession          */
#define DCM_SESSION_EXTENDED  0x03U  /**< extendedDiagnosticSession */

/* -----------------------------------------------------------------------
 * UDS サービス識別子 (ISO 14229-1 Table 3)
 * ----------------------------------------------------------------------- */
#define DCM_SID_SESSION_CTRL    0x10U  /**< DiagnosticSessionControl      */
#define DCM_SID_ECU_RESET       0x11U  /**< ECUReset                      */
#define DCM_SID_CLEAR_DTC       0x14U  /**< ClearDiagnosticInformation    */
#define DCM_SID_READ_DTC_INFO   0x19U  /**< ReadDTCInformation            */
#define DCM_SID_READ_DATA       0x22U  /**< ReadDataByIdentifier          */
#define DCM_SID_TESTER_PRESENT  0x3EU  /**< TesterPresent                 */
#define DCM_SID_NEGATIVE_RESP   0x7FU  /**< NegativeResponse              */

/* -----------------------------------------------------------------------
 * ReadDTCInformation (SID 0x19) サブ機能
 * ----------------------------------------------------------------------- */
#define DCM_DTC_SUBFUNC_REPORT_COUNT  0x01U  /**< reportNumberOfDTCByStatusMask */
#define DCM_DTC_SUBFUNC_REPORT_BY_MASK 0x02U /**< reportDTCByStatusMask         */

/** ISO 14229-1 DTC フォーマット識別子 (0x01 = ISO 15031-6 / SAE J2012) */
#define DCM_DTC_FORMAT_ISO15031         0x01U

/* -----------------------------------------------------------------------
 * UDS 否定応答コード (ISO 14229-1 Table A.1)
 * ----------------------------------------------------------------------- */
#define DCM_NRC_SERVICE_NOT_SUPPORTED      0x11U  /**< serviceNotSupported      */
#define DCM_NRC_SUB_FUNC_NOT_SUPPORTED     0x12U  /**< subFunctionNotSupported  */
#define DCM_NRC_CONDITIONS_NOT_CORRECT     0x22U  /**< conditionsNotCorrect     */
#define DCM_NRC_REQUEST_OUT_OF_RANGE       0x31U  /**< requestOutOfRange        */

/* -----------------------------------------------------------------------
 * データ識別子 (DID) 定義
 * ISO 14229-1 の製造者定義領域 (0x0100-0xFEFF) を使用。
 * ----------------------------------------------------------------------- */
#define DCM_DID_ENGINE_SPEED   0x0101U  /**< EngineSpeed: uint16, rpm */
#define DCM_DID_COOLANT_TEMP   0x0102U  /**< CoolantTemp: uint8,  ℃  */
#define DCM_DID_ENGINE_STATE   0x0103U  /**< EngineState: uint8,  enum */

/* -----------------------------------------------------------------------
 * ECUReset サブ機能
 * ----------------------------------------------------------------------- */
#define DCM_RESET_HARD  0x01U  /**< hardReset  */
#define DCM_RESET_SOFT  0x03U  /**< softReset  */

/* -----------------------------------------------------------------------
 * DiagnosticSessionControl 応答の P2 タイミングパラメータ
 * ----------------------------------------------------------------------- */
#define DCM_SESSION_P2_HIGH   0x00U  /**< P2  = 0x0019 = 25 ms        */
#define DCM_SESSION_P2_LOW    0x19U
#define DCM_SESSION_P2X_HIGH  0x01U  /**< P2* = 0x01F4 × 10ms = 5 s  */
#define DCM_SESSION_P2X_LOW   0xF4U

/* -----------------------------------------------------------------------
 * CanTp 送信 N-SDU ID (DCM 診断応答)
 * CanTp_Cfg.h の CANTP_TX_SDU_ID と同値。CanTp_Transmit() 呼び出しに使用する。
 * ----------------------------------------------------------------------- */
/* DCM は CanTp_Cfg.h をインクルードして CANTP_TX_SDU_ID を直接参照するため、
 * ここでは重複定義を避ける。                                                 */

#endif /* DCM_CFG_H */

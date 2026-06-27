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
 *            0x14 ClearDiagnosticInformation (groupOfDTC=0xFFFFFF で全クリア、
 *              特定 DTC コード指定で 1 件クリア) — extendedSession + SecurityAccess Level1 必須
 *            0x19 ReadDTCInformation (subFunc 0x01/0x02/0x04) — マルチフレーム対応
 *            0x22 ReadDataByIdentifier (DID 0x0101-0x0103)
 *            0x27 SecurityAccess (subFunc 0x01 requestSeed / 0x02 sendKey) — extendedSession 限定
 *            0x3E TesterPresent (S3 タイマ維持)
 *
 *          SID × セッション許可は Dcm_Cbk.c の Dcm_SidSessionTable[]
 *          (AUTOSAR DcmDspSessionRow に相当) で一元管理する。テーブルに掲載のない
 *          SID はセッション制約なし（全セッションで許可）とみなす。
 *
 *          ISO 15765-2 (CAN TP) トランスポート層は CanTp モジュールが担当する。
 *          本モジュールは PCI バイトを含まない生 UDS ペイロードのみを扱う。
 *
 *          S3 タイマ: defaultSession 以外の間、診断要求が DCM_S3_TIMEOUT_MS
 *          (既定 5000ms) 以上途絶えると defaultSession へ自動復帰する。
 *          Dcm_MainFunction() が周期的に監視する。
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

/** S3 タイマ (ISO 14229-1): defaultSession 以外で、この時間内に診断要求が
 *  1 件も来なければ defaultSession へ自動遷移する。Dcm_MainFunction が監視する。 */
#define DCM_S3_TIMEOUT_MS  60000UL

/* -----------------------------------------------------------------------
 * SID × セッション許可マスク (AUTOSAR DcmDspSessionRow に相当)
 * Dcm_SidSessionTable[] (Dcm_Cbk.c) の AllowedSessionMask 列で使用する。
 * ----------------------------------------------------------------------- */
#define DCM_SESSION_MASK_DEFAULT   0x01U  /**< bit0: defaultSession で許可            */
#define DCM_SESSION_MASK_EXTENDED  0x02U  /**< bit1: extendedDiagnosticSession で許可 */
#define DCM_SESSION_MASK_ALL       (DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_EXTENDED)

/* -----------------------------------------------------------------------
 * UDS サービス識別子 (ISO 14229-1 Table 3)
 * ----------------------------------------------------------------------- */
#define DCM_SID_SESSION_CTRL    0x10U  /**< DiagnosticSessionControl      */
#define DCM_SID_ECU_RESET       0x11U  /**< ECUReset                      */
#define DCM_SID_CLEAR_DTC       0x14U  /**< ClearDiagnosticInformation    */
#define DCM_SID_READ_DTC_INFO   0x19U  /**< ReadDTCInformation            */
#define DCM_SID_READ_DATA       0x22U  /**< ReadDataByIdentifier          */
#define DCM_SID_SECURITY_ACCESS 0x27U  /**< SecurityAccess                */
#define DCM_SID_TESTER_PRESENT  0x3EU  /**< TesterPresent                 */
#define DCM_SID_NEGATIVE_RESP   0x7FU  /**< NegativeResponse              */

/* -----------------------------------------------------------------------
 * SecurityAccess (SID 0x27) サブ機能 (ISO 14229-1 Table 44; Level1 のみ対応)
 * ----------------------------------------------------------------------- */
#define DCM_SEC_SUBFUNC_REQUEST_SEED  0x01U  /**< requestSeed (Level1) */
#define DCM_SEC_SUBFUNC_SEND_KEY      0x02U  /**< sendKey     (Level1) */

/** seed→key 変換に使う固定 XOR マスク。
 *  学習用の単純な例であり、実運用の ECU では使用しないこと
 *  （量産では暗号学的または OEM 固有の非公開アルゴリズムを用いる）。 */
#define DCM_SECURITY_KEY_MASK       0xA55AU

/** sendKey 連続失敗の許容回数。これに達すると DCM_SECURITY_DELAY_MS の間
 *  requestSeed 自体を拒否する (NRC 0x37) ブルートフォース対策。 */
#define DCM_SECURITY_MAX_ATTEMPTS   3U

/** 試行回数超過後、再度 requestSeed を受け付けるまでの待機時間 [ms]。 */
#define DCM_SECURITY_DELAY_MS       10000UL

/* -----------------------------------------------------------------------
 * ReadDTCInformation (SID 0x19) サブ機能
 * ----------------------------------------------------------------------- */
#define DCM_DTC_SUBFUNC_REPORT_COUNT  0x01U  /**< reportNumberOfDTCByStatusMask */
#define DCM_DTC_SUBFUNC_REPORT_BY_MASK 0x02U /**< reportDTCByStatusMask         */
#define DCM_DTC_SUBFUNC_REPORT_SNAPSHOT 0x04U /**< reportDTCSnapshotRecordByDTCNumber */

/** ISO 14229-1 DTC フォーマット識別子 (0x01 = ISO 15031-6 / SAE J2012) */
#define DCM_DTC_FORMAT_ISO15031         0x01U

/* -----------------------------------------------------------------------
 * FreezeFrame (SID 0x19/04) 関連定数
 * 本実装はイベントごとに 1 レコードのみ保持する学習用簡略化を行う。
 * ----------------------------------------------------------------------- */
/** 本実装で対応する唯一のスナップショットレコード番号 */
#define DCM_FREEZEFRAME_RECORD_NUMBER  0x01U
/** FreezeFrame に含む DID 数（EngineSpeed / CoolantTemp / EngineState 固定） */
#define DCM_FREEZEFRAME_DID_COUNT      0x03U

/* -----------------------------------------------------------------------
 * UDS 否定応答コード (ISO 14229-1 Table A.1)
 * ----------------------------------------------------------------------- */
#define DCM_NRC_SERVICE_NOT_SUPPORTED      0x11U  /**< serviceNotSupported      */
#define DCM_NRC_SUB_FUNC_NOT_SUPPORTED     0x12U  /**< subFunctionNotSupported  */
#define DCM_NRC_CONDITIONS_NOT_CORRECT     0x22U  /**< conditionsNotCorrect     */
#define DCM_NRC_REQUEST_SEQUENCE_ERROR     0x24U  /**< requestSequenceError (seed なしで sendKey 等) */
#define DCM_NRC_REQUEST_OUT_OF_RANGE       0x31U  /**< requestOutOfRange        */
#define DCM_NRC_SECURITY_ACCESS_DENIED     0x33U  /**< securityAccessDenied (Level1 未取得)     */
#define DCM_NRC_INVALID_KEY                0x35U  /**< invalidKey (sendKey の鍵が不一致)        */
#define DCM_NRC_EXCEEDED_NUM_ATTEMPTS      0x36U  /**< exceededNumberOfAttempts (連続失敗超過)  */
#define DCM_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED  0x37U  /**< requiredTimeDelayNotExpired (待機中) */
/** serviceNotSupportedInActiveSession。値は DCM_SID_NEGATIVE_RESP (0x7F) と
 *  たまたま同じ 0x7F だが、これは応答フレーム 3 バイト目の NRC であり、
 *  1 バイト目のネガティブレスポンス SID マーカーとは別物（意味も使用箇所も異なる）。 */
#define DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION  0x7FU

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

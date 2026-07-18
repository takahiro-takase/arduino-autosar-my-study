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
 *            0x19 ReadDTCInformation (subFunc 0x01/0x02/0x04/0x06) — マルチフレーム対応
 *            0x22 ReadDataByIdentifier (DID 0x0101-0x0103, 0x0104)
 *            0x27 SecurityAccess (subFunc 0x01 requestSeed / 0x02 sendKey) — extendedSession 限定
 *            0x2E WriteDataByIdentifier (DID 0x0104 のみ) — extendedSession +
 *              SecurityAccess Level1 必須。要求が 11 バイトと SF の 7 バイト制限を
 *              超えるため CanTp の複数フレーム受信 (FF+CF) を実機検証する目的の DID
 *              （学習用。実際の車両データではない）
 *            0x2F InputOutputControlByIdentifier (DID 0x0105-0x0107、
 *              RunLamp/FaultLamp/AbsLamp) — extendedSession 限定
 *              （SecurityAccess は要求しない。ダッシュボードランプの一時的な
 *              点灯確認のみで、車両制御や NVM 書き換えを伴わないため）。
 *              returnControlToECU/resetToDefault/freezeCurrentState/
 *              shortTermAdjustment の4つの controlOptionRecord に対応
 *            0x28 CommunicationControl (controlType 0x00-0x03、
 *              communicationType 0x01/0x02/0x03) — extendedSession 限定
 *              （SecurityAccess は要求しない。0x2F と同じ理由）。
 *              通常通信(Com)・ネットワークマネジメント通信(Nm) の送受信を
 *              個別に有効/無効化する。拡張アドレス指定 (controlType 0x04/0x05)
 *              や特定サブネット指定は非対応（本 ECU は単一ネットワークの
 *              非ゲートウェイ ECU のため）
 *            0x31 RoutineControl (RID 0x0203 EngineHealthCheck のみ) —
 *              extendedSession 限定。startRoutine/stopRoutine/
 *              requestRoutineResults の3サブ機能に対応。開始後
 *              DCM_ROUTINE_DURATION_MS 経過してから Dcm_MainFunction() が
 *              完了判定を行う非同期処理（IOControl とは異なり結果は
 *              即座に確定しない）
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
 *  1 件も来なければ defaultSession へ自動遷移する。Dcm_MainFunction が監視する。
 *  SWS_Dcm_00143: S3Server = 5s は固定値として仕様に明記されている。
 *  しかし、Tester Present メッセージの受信ログが多くなるため、60s にする。 */
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
#define DCM_SID_WRITE_DATA      0x2EU  /**< WriteDataByIdentifier         */
#define DCM_SID_IO_CONTROL      0x2FU  /**< InputOutputControlByIdentifier */
#define DCM_SID_COMM_CONTROL    0x28U  /**< CommunicationControl          */
#define DCM_SID_ROUTINE_CONTROL 0x31U  /**< RoutineControl                */
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
#define DCM_DTC_SUBFUNC_REPORT_COUNT     0x01U  /**< reportNumberOfDTCByStatusMask */
#define DCM_DTC_SUBFUNC_REPORT_BY_MASK   0x02U  /**< reportDTCByStatusMask         */
#define DCM_DTC_SUBFUNC_REPORT_SNAPSHOT  0x04U  /**< reportDTCSnapshotRecordByDTCNumber */
#define DCM_DTC_SUBFUNC_REPORT_EXTDATA   0x06U  /**< reportExtendedDataRecordByDTCNumber */

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
 * ExtendedData (SID 0x19/06) 関連定数
 * 本実装は故障確定回数カウンタ 1 件のみ保持する学習用簡略化を行う。
 * ----------------------------------------------------------------------- */
/** 本実装で対応する唯一の ExtendedData レコード番号 */
#define DCM_EXTENDED_DATA_RECORD_NUMBER  0x01U

/* -----------------------------------------------------------------------
 * UDS 否定応答コード (ISO 14229-1 Table A.1)
 * ----------------------------------------------------------------------- */
#define DCM_NRC_SERVICE_NOT_SUPPORTED      0x11U  /**< serviceNotSupported      */
#define DCM_NRC_SUB_FUNC_NOT_SUPPORTED     0x12U  /**< subFunctionNotSupported  */
/** incorrectMessageLengthOrInvalidFormat。0x2E WriteDataByIdentifier の
 *  データ長固定 DID で、要求データ長が一致しない場合に使用する
 *  （他ハンドラの「必須バイト数不足」は既存どおり 0x22 で代替する）。 */
#define DCM_NRC_INCORRECT_MESSAGE_LENGTH   0x13U
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

/** TestPattern: uint8[DCM_DID_TEST_PATTERN_LENGTH], 読み書き可能 (0x22 / 0x2E)。
 *  実際の車両データではなく、CanTp の複数フレーム要求受信 (FF+CF) を
 *  実機で検証するために用意した学習用 DID。0x2E の要求ペイロードが
 *  SID(1)+DID(2)+data(8)=11 バイトとなり SF の 7 バイト制限を超える。 */
#define DCM_DID_TEST_PATTERN          0x0104U
#define DCM_DID_TEST_PATTERN_LENGTH   8U

/** RunLamp/FaultLamp/AbsLamp: uint8 (0/1)。0x2F InputOutputControlByIdentifier
 *  専用の DID（0x22 ReadDataByIdentifier / 0x2E WriteDataByIdentifier では
 *  未対応、Dcm_ReadDid() には含まれない）。WarningStatus (CAN 0x210) の
 *  Signal Group メンバーと同じ3灯を、診断側から強制点灯/消灯できるようにする。 */
#define DCM_DID_RUN_LAMP     0x0105U
#define DCM_DID_FAULT_LAMP   0x0106U
#define DCM_DID_ABS_LAMP     0x0107U

/* -----------------------------------------------------------------------
 * InputOutputControlByIdentifier (SID 0x2F) controlOptionRecord
 * (ISO 14229-1 Table 209)
 * ----------------------------------------------------------------------- */
#define DCM_IOCTRL_RETURN_CONTROL_TO_ECU  0x00U  /**< 診断制御を解除し ASW に制御を返す */
#define DCM_IOCTRL_RESET_TO_DEFAULT       0x01U  /**< デフォルト値(消灯)に固定          */
#define DCM_IOCTRL_FREEZE_CURRENT_STATE   0x02U  /**< 現在の出力値のまま固定            */
#define DCM_IOCTRL_SHORT_TERM_ADJUSTMENT  0x03U  /**< controlState (1byte) の値に固定  */

/* -----------------------------------------------------------------------
 * CommunicationControl (SID 0x28) controlType サブ機能 (ISO 14229-1 Table 273)
 * 0x04/0x05 (enhanced address information 付き) は非対応。本 ECU は
 * ゲートウェイではなく単一ネットワークのみのため、サブネット別制御は不要
 * （Dcm_HandleCommunicationControl() 参照）。
 * ----------------------------------------------------------------------- */
#define DCM_COMMCTRL_ENABLE_RX_TX           0x00U  /**< enableRxAndTx           */
#define DCM_COMMCTRL_ENABLE_RX_DISABLE_TX   0x01U  /**< enableRxAndDisableTx    */
#define DCM_COMMCTRL_DISABLE_RX_ENABLE_TX   0x02U  /**< disableRxAndEnableTx    */
#define DCM_COMMCTRL_DISABLE_RX_TX          0x03U  /**< disableRxAndTx          */

/* -----------------------------------------------------------------------
 * CommunicationControl (SID 0x28) communicationType (ISO 14229-1 Table 274)
 * bit0=通常通信(Com)、bit1=ネットワークマネジメント通信(Nm)。
 * bit2-3 は予約、bit4-7 はサブネット番号だが、本 ECU は単一ネットワークのみ
 * のため 0x01/0x02/0x03 以外（サブネット指定を含む）は非対応とする。
 * ----------------------------------------------------------------------- */
#define DCM_COMMTYPE_NORMAL       0x01U  /**< normal communication messages           */
#define DCM_COMMTYPE_NM           0x02U  /**< network management communication messages */
#define DCM_COMMTYPE_NORMAL_AND_NM (DCM_COMMTYPE_NORMAL | DCM_COMMTYPE_NM)

/* -----------------------------------------------------------------------
 * RoutineControl (SID 0x31) サブ機能 (ISO 14229-1 Table 366)
 * ----------------------------------------------------------------------- */
#define DCM_ROUTINE_SUBFUNC_START            0x01U  /**< startRoutine             */
#define DCM_ROUTINE_SUBFUNC_STOP             0x02U  /**< stopRoutine              */
#define DCM_ROUTINE_SUBFUNC_REQUEST_RESULTS  0x03U  /**< requestRoutineResults    */

/** EngineHealthCheck: EngineSpeed/CoolantTemp が正常範囲内かを一定時間かけて
 *  判定する学習用ルーチン（実車の RID ではなく、本プロジェクト独自定義）。
 *  実際の自己診断ルーチン（アクチュエータテスト等）は結果確定に時間を要する
 *  ことが多く、それを Dcm_MainFunction() による非同期完了判定で模擬する。 */
#define DCM_RID_ENGINE_HEALTH_CHECK   0x0203U

/** startRoutine から DCM_ROUTINE_DURATION_MS 経過すると
 *  Dcm_MainFunction() がルーチンを完了させ、結果を確定する。 */
#define DCM_ROUTINE_DURATION_MS   3000UL

/** EngineHealthCheck の合否判定基準。App_EngineManager.c の
 *  COOLANT_OVERHEAT_THRESHOLD と同じ考え方（過熱していない）に加え、
 *  センサ値がセンサレンジ上限を超えていない（飽和・断線等の異常値でない）ことを見る。 */
#define DCM_ROUTINE_MAX_ENGINE_SPEED   8000U  /**< rpm。EngineSpeed_t (uint16) と比較 */
#define DCM_ROUTINE_MAX_COOLANT_TEMP   100U   /**< ℃。CoolantTemp_t (uint8) と比較   */

/** requestRoutineResults の routineStatusRecord 先頭バイト（本実装独自の
 *  manufacturer-specific フォーマット。ISO 14229-1 は内容を規定しない）。 */
#define DCM_ROUTINE_RESULT_RUNNING     0x00U  /**< まだ実行中（結果未確定） */
#define DCM_ROUTINE_RESULT_COMPLETED  0x01U  /**< 完了（後続 1 バイトが合否） */

/** routineStatusRecord の合否バイト（RESULT_COMPLETED の後続に付く） */
#define DCM_ROUTINE_PASS  0x01U
#define DCM_ROUTINE_FAIL  0x00U

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

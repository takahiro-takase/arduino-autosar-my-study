/**
 * \file    Dcm_PBCfg.c
 * \brief   DCM ポストビルドコンフィグ 定義
 * \details DID の表（Dcm_DidTable）を定義する。値は config/data/Dcm.json から生成する（下の生成領域）。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに相当する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Dcm_PBCfg.h"
#include "Rte.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* @@GEN-BEGIN dcm-did-table  生成元: config/data/Dcm.json  （自動生成: 手編集禁止） */
/** DID の表（0x22 / 0x2E / 0x2F が引く）。Dcm.c の Dcm_FindDid() が先頭から探索する */
const Dcm_DidConfigType Dcm_DidTable[DCM_DID_COUNT] =
{
    /* DCM_DID_ENGINE_SPEED */
    { DCM_DID_ENGINE_SPEED, 2U, Dcm_ReadDidEngineSpeed, NULL, DCM_DID_IOCONTROL_NONE },
    /* DCM_DID_COOLANT_TEMP */
    { DCM_DID_COOLANT_TEMP, 1U, Dcm_ReadDidCoolantTemp, NULL, DCM_DID_IOCONTROL_NONE },
    /* DCM_DID_ENGINE_STATE */
    { DCM_DID_ENGINE_STATE, 1U, Dcm_ReadDidEngineState, NULL, DCM_DID_IOCONTROL_NONE },
    /* DCM_DID_TEST_PATTERN */
    { DCM_DID_TEST_PATTERN, DCM_DID_TEST_PATTERN_LENGTH, Dcm_ReadDidTestPattern, Dcm_WriteDidTestPattern, DCM_DID_IOCONTROL_NONE },
    /* DCM_DID_RUN_LAMP */
    { DCM_DID_RUN_LAMP, 1U, NULL, NULL, RTE_LAMP_RUN },
    /* DCM_DID_FAULT_LAMP */
    { DCM_DID_FAULT_LAMP, 1U, NULL, NULL, RTE_LAMP_FAULT },
    /* DCM_DID_ABS_LAMP */
    { DCM_DID_ABS_LAMP, 1U, NULL, NULL, RTE_LAMP_ABS },
    /* DCM_DID_CRYPTO_KEY_UPDATE */
    { DCM_DID_CRYPTO_KEY_UPDATE, DCM_DID_CRYPTO_KEY_UPDATE_LENGTH, NULL, Dcm_WriteDidCryptoKeyUpdate, DCM_DID_IOCONTROL_NONE },
    /* DCM_DID_VIN */
    { DCM_DID_VIN, DCM_VIN_LENGTH, Dcm_ReadDidVin, NULL, DCM_DID_IOCONTROL_NONE }
};
/* @@GEN-END dcm-did-table */

/* @@GEN-BEGIN dcm-sid-session-table  生成元: config/data/Dcm.json  （自動生成: 手編集禁止） */
/** テーブルに掲載のない SID はセッション制約なし（全セッションで許可）とみなす。
 *  DTC 履歴の消去 (0x14)・セキュリティ認証 (0x27)・データ書き込み (0x2E) のみ、
 *  誤操作・悪用の影響が大きいため extendedSession 限定とする学習用の最小構成。
 *  0x2F (IOControl) も診断専用の一時的な出力制御であり通常操作ではないため
 *  extendedSession 限定とするが、車両制御や NVM 書き換えを伴わないため
 *  SecurityAccess までは要求しない（0x14/0x2E との保護レベルの違いを
 *  意図的に設けている）。0x31 (RoutineControl) も同様の理由
 *  （EngineHealthCheck はセンサ値を読むだけで NVM 書き換え・車両制御を
 *  伴わない）で extendedSession 限定・SecurityAccess 不要とする。
 *  0x28 (CommunicationControl) も同様に extendedSession 限定・SecurityAccess
 *  不要とする（通信そのものを止める操作的な影響はあるが、車両制御や NVM
 *  書き換えは伴わないため 0x2F/0x31 と同じ保護レベル）。
 *  0x34/0x36/0x37 (RequestDownload/TransferData/RequestTransferExit) は
 *  0x14/0x2E と同じ「誤操作・悪用の影響が大きい」保護レベルのため
 *  extendedSession 限定とする（SecurityAccess は 0x34 のみで判定する。
 *  Dcm.c 冒頭のコメント参照）。
 *  0x85 (ControlDTCSetting) も 0x28/0x2F/0x31 と同じ保護レベル
 *  （DTC 記録という診断上重要な機能への操作的な影響はあるが、車両制御や
 *  NVM 書き換えは伴わないため SecurityAccess までは要求しない）で
 *  extendedSession 限定とする。
 *  実際の AUTOSAR では DcmDspSessionRow がサービス・サブ機能単位で
 *  この表をコンフィギュレーションツールから生成する。 */
const Dcm_SidSessionRowType Dcm_SidSessionTable[DCM_SID_SESSION_COUNT] =
{
    { DCM_SID_CLEAR_DTC, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_SECURITY_ACCESS, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_WRITE_DATA, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_IO_CONTROL, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_COMM_CONTROL, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_ROUTINE_CONTROL, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_REQUEST_DOWNLOAD, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_TRANSFER_DATA, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_REQUEST_TRANSFER_EXIT, DCM_SESSION_MASK_EXTENDED },
    { DCM_SID_CONTROL_DTC_SETTING, DCM_SESSION_MASK_EXTENDED }
};
/* @@GEN-END dcm-sid-session-table */

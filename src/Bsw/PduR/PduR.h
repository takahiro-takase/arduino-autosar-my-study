/**
 * \file    PduR.h
 * \brief   PDU ルータ 公開インタフェース (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_H
#define PDUR_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "PduR_Types.h"
#include "PduR_Cfg.h"
#include "PduR_CanIf.h"
#include "PduR_COM.h"
#include "PduR_CanTp.h"
#include "PduR_SecOC.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* SWS_PduR_00334 */
void           PduR_Init(const PduR_PBConfigType* ConfigPtr);
/* SWS_PduR_00338: PduR_Init と並び PDUR_E_UNINIT 報告の対象外である唯一の例外 API
 * (SWS_PduR_00119)。 */
void           PduR_GetVersionInfo(Std_VersionInfoType* versioninfo);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef PDUR_UNIT_TEST
/**
 * \brief   [テスト専用] PduR_ConfigPtr を未初期化状態（NULL）へ戻す。
 *
 * \details PduR には Mcu 同様 DeInit() に相当する API が無く、`PduR_ConfigPtr`
 *          は native_chain_tests バイナリ全体で共有される static のため、
 *          一度 `PduR_Init()` を呼ぶと他のテストファイルの実行順に関わらず
 *          「未初期化状態」を再現できなくなる（`Bsw_PduR_SecOCTxConfirmation_test.cpp`
 *          冒頭コメント参照。同ファイルはこの制約のため PDUR_E_UNINIT の
 *          検証自体を諦めていた）。native 環境のホストテストからのみ使用する。
 *          `PDUR_UNIT_TEST` は CMakeLists.txt の native_chain ターゲットでのみ
 *          定義され、実機ビルド（`uno_r4`）では定義されないため、実機の
 *          `PduR.h`/`PduR.c` には一切含まれない（AUTOSAR 標準外の関数。
 *          `Mcu_Test_ResetInitState()` と同じ設計方針）。
 */
void PduR_Test_ResetInitState(void);
#endif

#ifdef __cplusplus
}
#endif

#endif

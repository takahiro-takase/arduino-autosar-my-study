/**
 * \file    Wrap_Csm.h
 * \brief   `src/Bsw/Csm/Csm.c` 内の `Csm_MacGenerate()` を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h` 冒頭コメント参照
 *          （「wrap 対象の関数が定義されている元の src ファイル 1 つにつき 1 ファイル」）。
 *          既定動作は `__real_Csm_MacGenerate()` へのパススルー。
 *
 *          Csm.h の公開 API 全体ではなく、SecOC の TX の経路（SecOC_MainFunctionTx()）の、Csm の失敗時の扱いを
 *          確認するために必要な `Csm_MacGenerate()` だけを wrap 対象にしている（本番の Csm のジョブは、
 *          MAC の検証用だけで、生成用のジョブを持たないので、実体では、成功も、BUSY のような回復可能な失敗も、
 *          作れない）。他の Csm の関数が必要になったときに、本ファイルへ足す。
 *
 *          回数閾値方式の故障注入（`Wrap_Com.h` 参照）: `FailFromCallCount_Csm_MacGenerate` に N を設定すると、
 *          `CallCount_Csm_MacGenerate` が N 以上になった回から（その回を含め、以降ずっと）
 *          `ForcedReturn_Csm_MacGenerate` を返す（`__real_` は呼ばない）。N=1 なら、「次の呼び出しから、常に
 *          ForcedReturn を返す」。ForcedReturn を E_OK にすると、実体を呼ばずに「成功」を返せる（MAC の出力先は
 *          書き換えない）。
 *
 *          変数名は `<種類>_<Module>_<関数名>`、`WrapCsm_Reset()` で全状態を一括で初期化する
 *          （`Wrap_Com.h` と同じ規則）。
 */
#ifndef WRAP_CSM_H
#define WRAP_CSM_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "Csm.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** `FailFromCallCount_Csm_MacGenerate` の「無効（常にパススルー）」を表す番兵値。 */
#define WRAP_CSM_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

extern uint32 CallCount_Csm_MacGenerate;
extern uint32 FailFromCallCount_Csm_MacGenerate;
extern Std_ReturnType ForcedReturn_Csm_MacGenerate;   /**< 既定 E_NOT_OK */

/* ======================================================================
 * Functions
 * ====================================================================== */

/** 呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。各テストケースの開始時（SetUp()）に 1 回呼ぶ。 */
void WrapCsm_Reset(void);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* WRAP_CSM_H */

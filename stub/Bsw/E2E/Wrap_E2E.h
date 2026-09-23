/**
 * \file    Wrap_E2E.h
 * \brief   `src/Bsw/E2E/E2E.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。2026-09-21、命名・故障注入方式は
 *          [[reference_wrap_stub_naming_convention]] の標準テンプレートへ
 *          統一した（`<種類>_<Module>_<関数名>`、回数閾値方式の
 *          `FailFromCallCount_E2E_SMCheck`。旧 `ForceFail` bool は廃止）。
 *
 * \par E2E_SMCheck
 * `E2EXf_ReportSMVerdict()`（E2EXf.c）が `E2E_SMCheck()` の戻り値が
 * `E2E_E_OK` 以外の場合に通る「到達しないはず」の防御分岐
 * （`E2EXf_PBCfg_Init()` が全インスタンスに対し `E2E_SMCheckInit()` を
 * 呼んでから使うため、`E2E_E_WRONGSTATE` は起きないはず）を検証するために
 * 導入（2026-09、`test/Bsw_E2EXf_SMCheckFailure_test.cpp` 参照。
 * 元は試作環境 `[env:native_chain_wrap]` の `Wrap_E2E_SMCheck.h` として
 * 新設し、本ファイルへ統合した）。E2EXf.c/E2E.c の実体は保ったまま、
 * 境界の1関数だけをピンポイントで失敗させる。既定動作は `__real_...` への
 * パススルー（Wrap_CanIf.h の設計方針を参照）。
 */
#ifndef WRAP_E2E_H
#define WRAP_E2E_H

#include "Std_Types.h"
#include "E2E.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_E2E_SMCheck` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ======================================================================
 *  External Variables
 * ====================================================================== */
/** `__wrap_E2E_SMCheck()` が呼ばれた回数。 */
extern uint32 CallCount_E2E_SMCheck;
extern uint32 CallCount_E2E_SMCheckInit;
extern uint32 CallCount_E2E_GetVersionInfo;

/** `WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に
 *  `__real_E2E_SMCheck()` へパススルー。それ以外の値を設定すると、
 *  `CallCount_E2E_SMCheck` がこの値に達した回から（以降ずっと）
 *  `ForcedReturn_E2E_SMCheck` を返す（本物は一切呼ばない、StatePtr にも
 *  触れない）。 */
extern uint32 FailFromCallCount_E2E_SMCheck;
extern uint32 FailFromCallCount_E2E_SMCheckInit;

/** 閾値到達後に返す戻り値（既定 E2E_E_WRONGSTATE）。 */
extern Std_ReturnType ForcedReturn_E2E_SMCheck;
extern Std_ReturnType ForcedReturn_E2E_SMCheckInit;

/** 呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapE2E_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_E2E_H */

/**
 * \file    Wrap_E2E.h
 * \brief   `src/Bsw/E2E/E2E.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `test/stub/` 配下の構成規則は `test/stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。
 *
 * \par E2E_SMCheck
 * `E2EXf_ReportSMVerdict()`（E2EXf.c）が `E2E_SMCheck()` の戻り値が
 * `E2E_E_OK` 以外の場合に通る「到達しないはず」の防御分岐
 * （`E2EXf_PBCfg_Init()` が全インスタンスに対し `E2E_SMCheckInit()` を
 * 呼んでから使うため、`E2E_E_WRONGSTATE` は起きないはず）を検証するために
 * 導入（2026-09、`test/test_chain/Bsw_E2EXf_SMCheckFailure_test.cpp` 参照。
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

/** `__wrap_E2E_SMCheck()` が呼ばれた回数。 */
extern uint32 WrapE2ESMCheck_CallCount;

/** 0（既定）: `__real_E2E_SMCheck()` へパススルー。
 *  0 以外: 呼び出し元へ即座に `WrapE2ESMCheck_ForcedReturn` を返す
 *  （本物は一切呼ばない、StatePtr にも触れない）。 */
extern uint8 WrapE2ESMCheck_ForceFail;

/** ForceFail 時に返す戻り値（既定 E2E_E_WRONGSTATE）。 */
extern Std_ReturnType WrapE2ESMCheck_ForcedReturn;

/** 各テストケースの開始時に呼び、呼び出し回数・強制失敗フラグを初期状態へ戻す。 */
void WrapE2ESMCheck_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_E2E_H */

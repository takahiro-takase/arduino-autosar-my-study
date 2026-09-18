/**
 * \file    Wrap_E2E_SMCheck.h
 * \brief   `-Wl,--wrap=E2E_SMCheck` によるフォールトインジェクション制御用
 *          アクセサ（`Wrap_CanIf_SetControllerMode.h` と同じ方式の第2適用例）。
 * \details `E2EXf_ReportSMVerdict()`（E2EXf.c）が `E2E_SMCheck()` の戻り値が
 *          `E2E_E_OK` 以外の場合に通る「到達しないはず」の防御分岐
 *          （`E2EXf_PBCfg_Init()` が全インスタンスに対し `E2E_SMCheckInit()` を
 *          呼んでから使うため、`E2E_E_WRONGSTATE` は起きないはず）を検証する。
 *          E2EXf.c/E2E.c の実体は保ったまま、境界の1関数だけをピンポイントで
 *          失敗させる。既定動作は `__real_...` へのパススルー
 *          （`Wrap_CanIf_SetControllerMode.h` の設計方針を参照）。
 */
#ifndef WRAP_E2E_SMCHECK_H
#define WRAP_E2E_SMCHECK_H

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

#endif /* WRAP_E2E_SMCHECK_H */

/**
 * \file    Wrap_ComM.h
 * \brief   `src/Bsw/ComM/ComM.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクション/呼び出し隔離のアクセサ群。
 *
 * \details `test/stub/` 配下の構成規則は `test/stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。
 *
 * \par ComM_DCM_ActiveDiagnostic / ComM_DCM_InactiveDiagnostic
 * 2026-09、test_dcm を native_chain へ統合した際に新設。既定（`Reset()`後）は
 * `Wrap_CanIf.h`/`Wrap_E2E.h`/`Wrap_Dem.h` と同じく `__real_...` への
 * パススルーで、`Bsw_SleepCoordination_test.cpp` の
 * `DcmActiveDiagnostic_OK_KeepsFullComEvenWhenUser0RequestsNoCom` 等が
 * 検証する「診断アクティブ中は FULL_COM を維持する」という実際の
 * 通信管理連鎖（ComM_ComputeAggregatedMode()→ComM_ApplyAggregatedRequest()→
 * 場合により Nm/CanSM まで）はそのまま機能する。
 *
 * 一方、`test/test_chain/Bsw_Dcm_*_test.cpp`（UDS 診断シーケンス自体の検証が
 * 目的で、通信管理は対象外。旧 `test/test_dcm/ComM_fake.c` も同じ理由の
 * 何もしないリンクスタブだった）は `WrapComM_DcmDiagnosticSuppressed` を
 * `SetUp()`で1に立て、`TearDown()`で0に戻すことで、自分のテスト実行中
 * だけこの2関数を無害化する（Can/CanIf/CanSM/Nm の初期化有無に依存させない
 * ため）。他のテストファイルはこのフラグの存在を意識する必要はない
 * （既定値0＝パススルーのまま）。
 */
#ifndef WRAP_COMM_H
#define WRAP_COMM_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 1: `ComM_DCM_ActiveDiagnostic()`/`ComM_DCM_InactiveDiagnostic()` を
 *  呼ばれても何もしないよう隔離する（`__real_...` を一切呼ばない）。
 *  0（既定）: `__real_...` へパススルーする。 */
extern uint8 WrapComM_DcmDiagnosticSuppressed;

/** 既定値（0＝パススルー）へ戻す。 */
void WrapComM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_COMM_H */

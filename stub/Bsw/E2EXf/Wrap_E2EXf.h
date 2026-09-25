/**
 * \file    Wrap_E2EXf.h
 * \brief   `src/Bsw/E2EXf/E2EXf.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/Can/Wrap_Can.h` 冒頭コメント参照
 *          （「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `E2EXf.h` が宣言する公開API全7関数を wrap 対象とする
 *          （2026-09 是正: `E2EXf_InverseTransform`/`E2EXf_InverseTransformP05`/
 *          `E2EXf_Transform`/`E2EXf_TransformP05`という汎用関数から、
 *          `E2EXf_Inv_EngineInfo`/`E2EXf_Inv_AbsInfo`/`E2EXf_E2EHealthStatus`
 *          というインスタンス専用関数へ移行したことに伴い改訂。P01版の
 *          汎用参考実装は廃止済み）。
 *
 *          戻り値を持つ3関数（`E2EXf_Inv_EngineInfo`/`E2EXf_Inv_AbsInfo`/
 *          `E2EXf_E2EHealthStatus`）には「指定した呼び出し回数以降は常に
 *          失敗を返す」という回数閾値方式の故障注入を実装する。
 *          `FailFromCallCount_E2EXf_Xxx`（既定
 *          `WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_E2EXf_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_E2EXf_Xxx` を返すようになる（本物は
 *          一切呼ばず、出力引数にも触れない）。
 *
 *          戻り値を持たない4関数（Init/DeInit/GetVersionInfo）は故障注入
 *          する戻り値が無いため、呼び出し回数のみを記録する。
 *
 *          変数名・故障注入方式・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_E2EXF_H
#define WRAP_E2EXF_H

#include "Std_Types.h"
#include "E2EXf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_E2EXf_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（公開 API 全7関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_E2EXf_E2EHealthStatus;
extern uint32 CallCount_E2EXf_Inv_EngineInfo;
extern uint32 CallCount_E2EXf_Inv_AbsInfo;
extern uint32 CallCount_E2EXf_Init;
extern uint32 CallCount_E2EXf_DeInit;
extern uint32 CallCount_E2EXf_GetVersionInfo;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ3関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_E2EXf_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_E2EXf_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_E2EXf_Xxx` を返す。 */
extern uint32 FailFromCallCount_E2EXf_E2EHealthStatus;
extern uint32 FailFromCallCount_E2EXf_Inv_EngineInfo;
extern uint32 FailFromCallCount_E2EXf_Inv_AbsInfo;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ3関数のみ）
 * ---------------------------------------------------------------------- */
extern uint8 ForcedReturn_E2EXf_E2EHealthStatus;   /**< 既定 E_SAFETY_HARD_RUNTIMEERROR */
extern uint8 ForcedReturn_E2EXf_Inv_EngineInfo;    /**< 既定 E_SAFETY_HARD_RUNTIMEERROR */
extern uint8 ForcedReturn_E2EXf_Inv_AbsInfo;       /**< 既定 E_SAFETY_HARD_RUNTIMEERROR */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapE2EXf_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_E2EXF_H */

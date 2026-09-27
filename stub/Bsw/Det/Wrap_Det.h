/**
 * \file    Wrap_Det.h
 * \brief   `src/Bsw/Det/Det.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h` 冒頭コメント
 *          参照（「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `Det.h` が宣言する `Det_Xxx` 公開API全6関数を wrap 対象とする
 *          （`Log_Write`/`Log_HexStr` は本 wrap 機構自体の実装に使う
 *          ロギング基盤であり、これを wrap すると `__wrap_Log_Write` 内で
 *          トレース出力に `Log_Write()` を使うと無限再帰になるため対象外とする）。
 *
 *          戻り値を持つ3関数（ReportError/ReportRuntimeError/
 *          ReportTransientFault）には「指定した呼び出し回数以降は常に失敗を
 *          返す」という回数閾値方式の故障注入を実装する。
 *          `FailFromCallCount_Det_Xxx`（既定
 *          `WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_Det_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_Det_Xxx` を返すようになる。
 *
 *          戻り値を持たない3関数（Init/Start/GetVersionInfo）は故障注入する
 *          戻り値が無いため、呼び出し回数のみ記録する。
 *
 *          変数名・故障注入方式・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_DET_H
#define WRAP_DET_H

#include "Std_Types.h"
#include "Det.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_Det_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（公開API全6関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_Det_Init;
extern uint32 CallCount_Det_ReportError;
extern uint32 CallCount_Det_Start;
extern uint32 CallCount_Det_ReportRuntimeError;
extern uint32 CallCount_Det_ReportTransientFault;
extern uint32 CallCount_Det_GetVersionInfo;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ3関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_Det_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_Det_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_Det_Xxx` を返す。 */
extern uint32 FailFromCallCount_Det_ReportError;
extern uint32 FailFromCallCount_Det_ReportRuntimeError;
extern uint32 FailFromCallCount_Det_ReportTransientFault;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ3関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_Det_ReportError;            /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Det_ReportRuntimeError;     /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Det_ReportTransientFault;   /**< 既定 E_NOT_OK */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapDet_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_DET_H */

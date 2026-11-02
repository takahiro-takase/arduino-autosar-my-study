/**
 * \file    Wrap_PduR.h
 * \brief   `src/Bsw/PduR/PduR.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          PduR.c の実際に実体化された API（IF と TP）を、すべて wrap 対象とする
 *          （`PduR_<User:Xxx>` テンプレート名の未実装分は対象外）。
 *
 *          戻り値を持つ関数（ComTransmit/SecOCTransmit/DcmTransmit/CanTpStartOfReception/CanTpCopyRxData/CanTpCopyTxData）には
 *          「指定した呼び出し回数以降は常に失敗を返す」という回数閾値方式の
 *          故障注入を実装する。`FailFromCallCount_PduR_Xxx`
 *          （既定 `Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 *          `CallCount_PduR_Xxx` が N 以上になった回から（その回を含め、
 *          以降ずっと）`ForcedReturn_PduR_Xxx` を返すようになる。N=1 を
 *          設定すれば「次の呼び出しから即座に失敗」という単純な即時強制失敗
 *          としても使える（他 wrap ファイルの `ForceFail` 相当）ため、本ファイル
 *          では `ForceFail` 単体のフラグは持たない（2026-09-20、ユーザーとの
 *          相談の上、素朴な bool より汎用的な本方式へ一本化することを選択）。
 *          「無効」を 0 ではなく `UINT32_MAX` 相当の番兵値で表すことで、
 *          呼び出し側の判定を `CallCount >= FailFromCallCount` の1条件だけに
 *          単純化できる（2026-09-20、ユーザー提案により `!= 0` ガードを撤去）。
 *
 *          戻り値を持たない関数（Init/GetVersionInfo/ComRxIndication/
 *          CanIfTxConfirmation/SecOCTxConfirmation）は故障注入する戻り値が
 *          無いため、呼び出し回数のみを記録する。
 *
 *          2026-11 に、TP の API 6 関数（DcmTransmit、CanTpStartOfReception/CopyRxData/CopyTxData は
 *          戻り値あり、CanTpRxIndication/CanTpTxConfirmation は戻り値なし）を追加した。同じ規則で、
 *          戻り値を持つ関数だけが、回数閾値の故障注入を持つ。
 *
 *          変数名は `<種類>_<Module>_<関数名>`（種類=CallCount/
 *          FailFromCallCount/ForcedReturn を先頭側に置く）で統一する
 *          （2026-09-20、ユーザー提案により関数名を先頭に置く旧来の
 *          `Wrap<Module><関数名>_<種類>` から変更。同じ種類の変数を1箇所に
 *          まとめて宣言でき、型も種類ごとに揃うため可読性が上がる。「Wrap」
 *          接頭辞は冗長なため付けない。今後新規追加する `Wrap_XXX.c` は
 *          本ファイルと同じ命名規則へ統一する）。
 *
 *          全関数とも本ファイル1つが対象で、実際に使うテスト（想定:
 *          `test/Bsw/PduR/`）も1つに閉じるため、他 wrap ファイルのような
 *          関数ごとの個別 `Reset()` ではなく、全状態を一括で初期状態へ戻す
 *          `WrapPduR_Reset()` を1つだけ持つ（2026-09-20、ユーザー提案により
 *          個別 Reset の代わりに採用）。各テストケースは SetUp() で
 *          `WrapPduR_Reset()` を1回呼び、`FailFromCallCount_PduR_Xxx` を
 *          Arrange 区間でのみ立てること（他のテストケースの挙動を暗黙に
 *          変えないため）。
 */
#ifndef Wrap_PduR_H
#define Wrap_PduR_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "PduR_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** `FailFromCallCount_PduR_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * 呼び出し回数（wrap した全関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_PduR_Init;
extern uint32 CallCount_PduR_GetVersionInfo;
extern uint32 CallCount_PduR_ComTransmit;
extern uint32 CallCount_PduR_SecOCTransmit;
extern uint32 CallCount_PduR_ComRxIndication;
extern uint32 CallCount_PduR_CanIfTxConfirmation;
extern uint32 CallCount_PduR_SecOCTxConfirmation;
extern uint32 CallCount_PduR_DcmTransmit;
extern uint32 CallCount_PduR_CanTpStartOfReception;
extern uint32 CallCount_PduR_CanTpCopyRxData;
extern uint32 CallCount_PduR_CanTpRxIndication;
extern uint32 CallCount_PduR_CanTpCopyTxData;
extern uint32 CallCount_PduR_CanTpTxConfirmation;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ関数のみ）
 * ---------------------------------------------------------------------- */
/** `Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_PduR_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_PduR_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_PduR_Xxx` を返す。 */
extern uint32 FailFromCallCount_PduR_ComTransmit;
extern uint32 FailFromCallCount_PduR_SecOCTransmit;
extern uint32 FailFromCallCount_PduR_DcmTransmit;
extern uint32 FailFromCallCount_PduR_CanTpStartOfReception;
extern uint32 FailFromCallCount_PduR_CanTpCopyRxData;
extern uint32 FailFromCallCount_PduR_CanTpCopyTxData;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_PduR_ComTransmit;
extern Std_ReturnType ForcedReturn_PduR_SecOCTransmit;
extern Std_ReturnType ForcedReturn_PduR_DcmTransmit;
extern BufReq_ReturnType ForcedReturn_PduR_CanTpStartOfReception;
extern BufReq_ReturnType ForcedReturn_PduR_CanTpCopyRxData;
extern BufReq_ReturnType ForcedReturn_PduR_CanTpCopyTxData;

/* ======================================================================
 * Functions
 * ====================================================================== */

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapPduR_Reset(void);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* Wrap_PduR_H */

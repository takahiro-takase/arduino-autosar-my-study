/**
 * \file    Wrap_CanTp.h
 * \brief   `src/Bsw/CanTp/CanTp.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `test/stub/` 配下の構成規則は `test/stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *          命名・故障注入方式は [[reference_wrap_stub_naming_convention]] の
 *          標準テンプレートに従う。
 *
 *          AUTOSAR 仕様（SWS_CanTp）が定義する CanTp.c の公開 IF 関数を
 *          全数 wrap 対象とする（`CanTp.h` 参照。`CanTp_IsTxBusy()` は
 *          AUTOSAR 標準外の本プロジェクト独自拡張 API だが、上位層の
 *          事前確認パスを検証するために対象に含める）。
 *
 *          戻り値を持つ2関数（Transmit/IsTxBusy）には「指定した呼び出し
 *          回数以降は常に強制値を返す」という回数閾値方式の故障注入を実装
 *          する。`FailFromCallCount_CanTp_Xxx`
 *          （既定 `WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定
 *          すると、`CallCount_CanTp_Xxx` が N 以上になった回から（その回を
 *          含め、以降ずっと）`ForcedReturn_CanTp_Xxx` を返すようになる。
 *
 *          戻り値を持たない5関数（Init/RxIndication/TxConfirmation/
 *          MainFunction/GetVersionInfo）は故障注入する戻り値が無いため、
 *          呼び出し回数のみを記録する。
 *
 *          7関数すべて本ファイル1つが対象のため、他 wrap ファイルのような
 *          関数ごとの個別 `Reset()` ではなく、全状態を一括で初期状態へ戻す
 *          `WrapCanTp_Reset()` を1つだけ持つ。各テストケースは SetUp() で
 *          `WrapCanTp_Reset()` を1回呼び、`FailFromCallCount_CanTp_Xxx` を
 *          Arrange 区間でのみ立てること（他のテストケースの挙動を暗黙に
 *          変えないため）。
 *
 *          `CanTp_Transmit()` は `LastData_CanTp_Transmit`/
 *          `LastLength_CanTp_Transmit` に呼び出し時の `PduInfoPtr` の内容を
 *          複製する（`__real_...` の成否に関わらず、呼び出しの都度キャプチャ
 *          する）。Dcm から見て「DCM がどんな UDS 応答を送ろうとしたか」を
 *          ブラックボックスに検証する既存テスト方式（旧 `Fake_CanTp.c` の
 *          `FakeCanTp_TxBuf`/`FakeCanTp_TxLength` 相当）を、CanTp.c 実体
 *          リンク化後も維持するために導入した（2026-09-22、CanTp.c を
 *          Fake から実体リンクへ切り替えた際に追加）。
 */
#ifndef WRAP_CANTP_H
#define WRAP_CANTP_H

#include "Std_Types.h"
#include "CanTp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_CanTp_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（AUTOSAR IF 全7関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_CanTp_Init;
extern uint32 CallCount_CanTp_Transmit;
extern uint32 CallCount_CanTp_IsTxBusy;
extern uint32 CallCount_CanTp_RxIndication;
extern uint32 CallCount_CanTp_TxConfirmation;
extern uint32 CallCount_CanTp_MainFunction;
extern uint32 CallCount_CanTp_GetVersionInfo;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ2関数のみ）
 * ---------------------------------------------------------------------- */
/** `WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_CanTp_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_CanTp_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_CanTp_Xxx` を返す。 */
extern uint32 FailFromCallCount_CanTp_Transmit;
extern uint32 FailFromCallCount_CanTp_IsTxBusy;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ2関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_CanTp_Transmit; /**< 既定 E_NOT_OK */
extern boolean        ForcedReturn_CanTp_IsTxBusy; /**< 既定 TRUE（送信中扱い） */

/* ----------------------------------------------------------------------
 * 直近の呼び出し内容のキャプチャ（CanTp_Transmit のみ）
 * ---------------------------------------------------------------------- */
/** 直近の `CanTp_Transmit()` 呼び出しで渡された `PduInfoPtr->SduDataPtr` の
 *  複製（先頭 `LastLength_CanTp_Transmit` バイトが有効）。 */
extern uint8 LastData_CanTp_Transmit[CANTP_TX_BUFFER_SIZE];

/** 直近の `CanTp_Transmit()` 呼び出しで渡された `PduInfoPtr->SduLength`。 */
extern PduLengthType LastLength_CanTp_Transmit;

/** すべての関数呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapCanTp_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_CANTP_H */

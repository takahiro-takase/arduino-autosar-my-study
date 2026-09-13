/**
 * \file    Bsw_EcuM_fake.h
 * \brief   EcuM.h（ComM の RUN 要求先／Can のウェイクアップ通知先）のテスト用
 *          スパイ実装の宣言
 * \details ComM.c は FULL_COM/NO_COM 確定のたびに EcuM_RequestRUN()/
 *          EcuM_ReleaseRUN() を呼ぶ。本テストの対象は Nm↔CanSM↔ComM の
 *          協調スリープ状態遷移ロジックであり、EcuM 側の RUN/POST_RUN/
 *          SHUTDOWN 状態機械までは対象に含めないため、呼び出し回数・引数を
 *          記録するだけのフェイクに差し替える（Bsw_ComM_fake.h と同じ境界の
 *          考え方）。
 *
 *          `EcuM_CheckWakeup()`（2026-09-05 新設、[SWS_Can_00271]。旧
 *          `CanIf_ControllerWakeup()` の役割を引き継ぐ）のみ例外的に、
 *          呼び出し記録に加えて実 `CanSM_ControllerModeIndication()`
 *          （native_chain に実体リンク済み）へそのまま委譲する。実 EcuM.c は
 *          BswM/WdgM/NvM/Crypto 等 native_chain がリンクしない大量の依存を
 *          持つためフェイクに差し替えざるを得ないが、`EcuM_CheckWakeup()`
 *          自体の実装は「CanSM への一行委譲」のみであり、これをただの
 *          スパイにしてしまうと `Bsw_WakeupChain_test.cpp` が検証していた
 *          ウェイクアップ検証チェーン全体（CanSM の状態機械）がここで
 *          途切れてしまう。他の RUN/POST_RUN 系 API（本物の状態機械を持つ）
 *          とは境界の性質が異なると判断し、この関数だけ委譲する。
 */
#ifndef BSW_ECUM_FAKE_H
#define BSW_ECUM_FAKE_H

#include "Std_Types.h"
#include "EcuM.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeEcuM_RequestRUNCount;
extern uint32 FakeEcuM_ReleaseRUNCount;
extern uint32 FakeEcuM_CheckWakeupCount;

/** 直近の呼び出しの引数。 */
extern EcuM_UserType FakeEcuM_LastRequestUser;
extern EcuM_UserType FakeEcuM_LastReleaseUser;
extern EcuM_WakeupSourceType FakeEcuM_LastWakeupSource;

/** 各テストケースの開始時に呼び、記録をすべて初期状態に戻す。 */
void FakeEcuM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSW_ECUM_FAKE_H */

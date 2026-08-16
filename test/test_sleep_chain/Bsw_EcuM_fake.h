/**
 * \file    Bsw_EcuM_fake.h
 * \brief   EcuM.h（ComM の RUN 要求先）のテスト用スパイ実装の宣言
 * \details ComM.c は FULL_COM/NO_COM 確定のたびに EcuM_RequestRUN()/
 *          EcuM_ReleaseRUN() を呼ぶ。本テストの対象は Nm↔CanSM↔ComM の
 *          協調スリープ状態遷移ロジックであり、EcuM 側の RUN/POST_RUN/
 *          SHUTDOWN 状態機械までは対象に含めないため、呼び出し回数・引数を
 *          記録するだけのフェイクに差し替える（Bsw_ComM_fake.h と同じ境界の
 *          考え方）。
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

/** 直近の呼び出しの引数。 */
extern EcuM_UserType FakeEcuM_LastRequestUser;
extern EcuM_UserType FakeEcuM_LastReleaseUser;

/** 各テストケースの開始時に呼び、記録をすべて初期状態に戻す。 */
void FakeEcuM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSW_ECUM_FAKE_H */

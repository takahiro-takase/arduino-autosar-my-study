/**
 * \file    Bsw_EcuM_fake.h
 * \brief   EcuM.h のテスト用フェイク実装の宣言（Can.c から見た上位層コールバック）
 * \details Can.c は CAN バスの自律的なウェイクアップ検知時に
 *          EcuM_CheckWakeup()（[SWS_Can_00271]、2026-09-05 新設。旧
 *          `CanIf_ControllerWakeup()` の役割を引き継ぐ）を呼ぶ。
 *          `[env:native]` は CanSM.c を一切リンクしない Can.c 単体の隔離検証
 *          環境（Bsw_CanIf_fake.h 冒頭コメント参照）のため、
 *          `[env:native_chain]`（test/test_chain/Bsw_EcuM_fake.h）の実
 *          CanSM_ControllerModeIndication() への委譲版とは異なり、呼び出し
 *          記録のみのフェイクにする。
 */
#ifndef BSW_ECUM_FAKE_H
#define BSW_ECUM_FAKE_H

#include "EcuM.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeEcuM_CheckWakeupCount;

/** 直近の呼び出し引数。 */
extern EcuM_WakeupSourceType FakeEcuM_LastWakeupSource;

/** 各テストケースの開始時に呼び、記録をすべてクリアする。 */
void FakeEcuM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSW_ECUM_FAKE_H */

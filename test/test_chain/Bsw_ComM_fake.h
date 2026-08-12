/**
 * \file    Bsw_ComM_fake.h
 * \brief   ComM.h（CanSM の通知先）のテスト用スパイ実装の宣言
 * \details CanSM.c は状態遷移のたびに `ComM_BusSMIndication()` を呼ぶ。
 *          Bsw_SleepChain_test.cpp/Bsw_WakeupChain_test.cpp の対象は CanSM
 *          自身のスリープ/ウェイクアップ判断ロジックであり、ComM 側の
 *          複数ユーザ調停・Dcm 連携までは対象に含めない（CanIf.c が
 *          CanSM_RxIndication() 等をハードコードで呼ぶため CanSM.c 自体は
 *          `[env:native_chain]` 全体で実体をリンクせざるを得ないが、その
 *          呼び出し先である ComM は境界としてフェイクに差し替えられる。
 *          詳細は Bsw_TxChain_test.cpp 冒頭コメント参照）。そのため ComM.c は
 *          本テストではリンクせず、呼び出し回数・引数を記録するだけの
 *          フェイクに差し替える。
 */
#ifndef BSW_COMM_FAKE_H
#define BSW_COMM_FAKE_H

#include "Std_Types.h"
#include "ComM.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeComM_BusSMIndicationCount;

/** 直近の ComM_BusSMIndication() 呼び出しの引数。 */
extern uint8         FakeComM_LastNetwork;
extern ComM_ModeType FakeComM_LastMode;

/** 各テストケースの開始時に呼び、記録をすべて初期状態に戻す。 */
void FakeComM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSW_COMM_FAKE_H */

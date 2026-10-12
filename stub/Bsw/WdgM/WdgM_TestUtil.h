/**
 * \file    WdgM_TestUtil.h
 * \brief   WdgM のテスト共通の補助関数
 * \details WdgM_MainFunction() は、WDGM_SUPERVISION_REFERENCE_CYCLE 回の呼び出しごとに
 *          1 回だけ Alive/Logical/Deadline の判定サイクルを評価する（毎回の呼び出しで
 *          HW ウォッチドッグのリフレッシュも行う）。判定サイクル 1 回分を進めたい
 *          テストは、WdgM_Init() 直後（または前回の判定の直後）の状態から、
 *          WdgM_RunSupervisionCycle() を呼ぶこと。
 */
#ifndef WDGM_TESTUTIL_H
#define WDGM_TESTUTIL_H

#include "WdgM.h"
#include "WdgM_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** WdgM_MainFunction() を基準周期（WDGM_SUPERVISION_REFERENCE_CYCLE）回呼び、判定サイクルを 1 回評価する。 */
static inline void WdgM_RunSupervisionCycle(void)
{
    for (uint8 i = 0U; i < WDGM_SUPERVISION_REFERENCE_CYCLE; i++)
    {
        WdgM_MainFunction();
    }
}

#ifdef __cplusplus
}
#endif

#endif /* WDGM_TESTUTIL_H */

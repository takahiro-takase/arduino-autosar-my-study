/**
 * \file    BswM_fake.h
 * \brief   Dcm_Cbk.c の BswM 依存（BswM_Dcm_CommunicationMode_CurrentState、
 *          UDS SID 0x28 CommunicationControl 経由、2026-09-05 のシグネチャ
 *          準拠サーベイで Com/Nm 直接呼び出しから置き換え済み）を
 *          満たすだけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 * \details BswM.c 自体（ルールエンジン本体）はどの native 環境にも一度も
 *          リンクされたことが無い既存の制約（uno_r4 ビルドと手動検証のみ）を
 *          踏襲し、本フェイクは「Dcm が正しい Dcm_CommunicationModeType 値で
 *          BswM を呼んだか」を検証する境界とする（BswM 側のルール発火・
 *          Com/Nm への実際の反映自体はテスト対象外）。
 */
#ifndef BSWM_FAKE_H
#define BSWM_FAKE_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Dcm_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern NetworkHandleType         FakeBswM_LastNetwork;
extern Dcm_CommunicationModeType FakeBswM_LastMode;
extern uint32                    FakeBswM_CallCount;

/** 各テストケースの開始時に呼び、直近の呼び出し記録をクリアする。 */
void FakeBswM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSWM_FAKE_H */

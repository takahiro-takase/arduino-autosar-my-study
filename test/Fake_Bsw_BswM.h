/**
 * \file    Fake_Bsw_BswM.h
 * \brief   BswM.h のテスト用スパイ実装の宣言。BswM.c 自体（ルールエンジン
 *          本体）はどの native 環境にも一度もリンクされたことが無い既存の
 *          制約（uno_r4 ビルドと手動検証のみ）のため、呼び出し元ごとに
 *          境界となる関数だけを呼び出し記録付きのフェイクに差し替える。
 *
 * \details ComM.c はチャネルモードが変化するたびに BswM_ComM_CurrentMode()
 *          を呼ぶ。本テストの対象は Nm↔CanSM↔ComM の協調スリープ状態遷移
 *          ロジックであり、BswM 側のルールエンジンまでは対象に含めないため、
 *          呼び出し回数・引数を記録するだけのフェイクに差し替える
 *          （Bsw_ComM_fake.h と同じ境界の考え方）。
 *
 *          2026-09、test_dcm 統合に伴い、Dcm_Cbk.c から見た通知先
 *          BswM_Dcm_CommunicationMode_CurrentState()（UDS SID 0x28
 *          CommunicationControl 経由）のフェイクも本ファイルへ統合した
 *          （旧 test/test_dcm/BswM_fake.h/.c）。ComM.c 用のアクセサと
 *          対象関数が異なるため、`FakeBswM_LastMode`（ComM_ModeType）との
 *          衝突を避けて `FakeBswM_LastDcmCommunicationMode`
 *          （Dcm_CommunicationModeType）という別名にしてある。
 */
#ifndef FAKE_BSW_BSWM_H
#define FAKE_BSW_BSWM_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "ComM.h"
#include "Dcm_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeBswM_ComM_CurrentModeCount;

/** 直近の BswM_ComM_CurrentMode() 呼び出しの引数。 */
extern uint8         FakeBswM_LastChannel;
extern ComM_ModeType FakeBswM_LastMode;

extern uint32 FakeBswM_DcmCommunicationModeCurrentStateCount;

/** 直近の BswM_Dcm_CommunicationMode_CurrentState() 呼び出しの引数。 */
extern NetworkHandleType         FakeBswM_LastNetwork;
extern Dcm_CommunicationModeType FakeBswM_LastDcmCommunicationMode;

/** 各テストケースの開始時に呼び、記録をすべて初期状態に戻す。 */
void FakeBswM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_BSW_BSWM_H */

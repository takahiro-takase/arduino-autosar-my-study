/**
 * \file    Wrap_BswM.h
 * \brief   `src/Bsw/BswM/BswM.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクションのアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *          命名・故障注入方式は [[reference_wrap_stub_naming_convention]] の
 *          標準テンプレートに従う。
 *
 *          旧 `Fake_Bsw_BswM.c/h`（BswM.c 自体＝ルールエンジン本体を一度も
 *          リンクせず、呼び出し境界を丸ごと差し替えるフェイク）を、
 *          BswM.c 実体リンクへ切り替えた上での wrap 化に置き換えた
 *          （2026-09-22）。`BswM.c` は `Os_SetTaskActive()`（`src/Os/Os.c`）
 *          にのみ依存するが、`Os_Init()` を呼ばない限り `Os_Cfg==NULL` の
 *          ガードで即座に no-op を返す（`Os_GetTimeMs()`→`Gpt_GetTimeElapsed()`
 *          にすら到達しない）ため安全。同様に `BswM_Init()` を呼ばない限り
 *          `BswM_Cfg==NULL` のガードでルール評価自体が一切走らない
 *          （`BswM.c` 参照）。本ファイルの wrap は、既存テストが必要として
 *          いた「呼び出し回数・引数のキャプチャ」を実体リンク後も維持する
 *          ためのもので、`BswM_Init()` を呼んでルールエンジン本体を実際に
 *          動かす用途は対象外（呼びたいテストは各自 `BswM_Init()` を呼べば
 *          よい。その場合も wrap は素通しで実体へ届く）。
 *
 *          AUTOSAR 仕様（SWS_BswM）が定義する `BswM.h` の公開 IF 全6関数を
 *          対象とする。すべて戻り値を持たないため、回数閾値方式の故障注入は
 *          実装せず、呼び出し回数の記録のみ行う。
 *
 *          `BswM_ComM_CurrentMode()`/`BswM_Dcm_CommunicationMode_CurrentState()`
 *          には、旧 `Fake_Bsw_BswM.c` の `FakeBswM_LastChannel`/`FakeBswM_LastMode`/
 *          `FakeBswM_LastNetwork`/`FakeBswM_LastDcmCommunicationMode` 相当の
 *          直近引数キャプチャを追加する。
 *
 *          6関数すべて本ファイル1つが対象のため、他 wrap ファイルのような
 *          関数ごとの個別 `Reset()` ではなく、全状態を一括で初期状態へ戻す
 *          `WrapBswM_Reset()` を1つだけ持つ。各テストケースは SetUp() で
 *          `WrapBswM_Reset()` を1回呼ぶこと。
 */
#ifndef WRAP_BSWM_H
#define WRAP_BSWM_H

#include "Std_Types.h"
#include "BswM.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * 呼び出し回数（AUTOSAR IF 全6関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_BswM_Init;
extern uint32 CallCount_BswM_Deinit;
extern uint32 CallCount_BswM_GetVersionInfo;
extern uint32 CallCount_BswM_EcuM_CurrentState;
extern uint32 CallCount_BswM_ComM_CurrentMode;
extern uint32 CallCount_BswM_Dcm_CommunicationMode_CurrentState;

/* ----------------------------------------------------------------------
 * 直近の呼び出し内容のキャプチャ
 * ---------------------------------------------------------------------- */
/** 直近の `BswM_ComM_CurrentMode()` 呼び出しの引数。 */
extern NetworkHandleType LastChannel_BswM_ComM_CurrentMode;
extern ComM_ModeType     LastMode_BswM_ComM_CurrentMode;

/** 直近の `BswM_Dcm_CommunicationMode_CurrentState()` 呼び出しの引数。 */
extern NetworkHandleType         LastNetwork_BswM_Dcm_CommunicationMode_CurrentState;
extern Dcm_CommunicationModeType LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState;

/** すべての関数呼び出し回数・直近引数キャプチャを初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapBswM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_BSWM_H */

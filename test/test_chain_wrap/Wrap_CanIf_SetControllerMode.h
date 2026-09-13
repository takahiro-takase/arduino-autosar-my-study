/**
 * \file    Wrap_CanIf_SetControllerMode.h
 * \brief   `-Wl,--wrap=CanIf_SetControllerMode` によるフォールトインジェクション
 *          制御用アクセサ（コールチェーン方式テスト刷新の試作、README 未反映）。
 * \details GNU ld の `--wrap` は、最終リンク後のバイナリ内で対象シンボルへの
 *          全呼び出し元（本ファイルの場合 CanSM.c/CanIf.c 双方を含む、
 *          `[env:native_chain_wrap]` にリンクされる全 .o が対象）を
 *          `__wrap_CanIf_SetControllerMode()` へ差し替える。本物の定義
 *          （CanIf.c 内の `CanIf_SetControllerMode()`）は `__real_...` という
 *          名前で引き続き呼び出せる。
 *
 *          `--wrap` はリンク単位全体に効く一括置換であり、特定のテストケース
 *          だけを狙って差し替える仕組みではない。そのため既定動作は
 *          `__real_...` への単純な委譲（パススルー）とし、他のテストケースの
 *          挙動を暗黙に変えないようにしている。個々のテストは
 *          `WrapCanIfSetControllerMode_ForceFail` を Arrange 区間でのみ立てて
 *          狙った箇所だけ故障注入し、Act 直後に倒す。
 *
 *          この方式の利点は、CanIf.c/Can.c を丸ごとフェイクへ差し替えることなく
 *          （＝コールチェーンの残り全区間は実体のまま）、ピンポイントで
 *          1 関数だけ失敗させられる点にある。本プロジェクトの
 *          `native_chain`（実体リンク）と `native`/`native_wdgm` 等
 *          （個別モジュールをフェイクで隔離）という二極化した既存構成に対し、
 *          「実体を保ったまま特定の失敗経路だけ作る」第三の選択肢を試作する。
 */
#ifndef WRAP_CANIF_SETCONTROLLERMODE_H
#define WRAP_CANIF_SETCONTROLLERMODE_H

#include "Std_Types.h"
#include "CanIf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `__wrap_CanIf_SetControllerMode()` が呼ばれた回数。 */
extern uint32 WrapCanIfSetControllerMode_CallCount;

/** 0（既定）: `__real_CanIf_SetControllerMode()` へパススルー。
 *  0 以外: 呼び出し元へ即座に E_NOT_OK を返す（本物は一切呼ばない）。 */
extern uint8 WrapCanIfSetControllerMode_ForceFail;

/** 各テストケースの開始時に呼び、呼び出し回数・強制失敗フラグを初期状態へ戻す。 */
void WrapCanIfSetControllerMode_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_CANIF_SETCONTROLLERMODE_H */

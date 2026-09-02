/**
 * \file    Can.h
 * \brief   CAN ドライバ 公開インタフェース (AUTOSAR SWS_Can 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CAN_H
#define CAN_H

#include "Platform_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "Can_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void           Can_Init(const Can_ConfigType* Config);
void           Can_GetVersionInfo(Std_VersionInfoType* versioninfo);
Can_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition);
Std_ReturnType Can_GetControllerErrorState(uint8 Controller, Can_ErrorStateType* ErrorStatePtr);
void           Can_DisableControllerInterrupts(uint8 Controller);
void           Can_EnableControllerInterrupts(uint8 Controller);
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void           Can_MainFunction_Read(void);
void           Can_MainFunction_Write(void);
void           Can_MainFunction_BusOff(void);
void           Can_MainFunction_Wakeup(void);

#ifdef CAN_UNIT_TEST
/**
 * \brief   [テスト専用] 内部状態 CanState を取得する。
 *
 * \details native 環境のホストテスト（`test/test_native/`）からのみ使用する
 *          アクセサ。`CAN_UNIT_TEST` は `[env:native]` の `build_flags` でのみ
 *          定義され、実機ビルド（`uno_r4`）では定義されないため、実機の
 *          `Can.h`/`Can.c` には一切含まれない（AUTOSAR 標準外の関数）。
 */
Can_ControllerStateType Can_Test_GetControllerState(void);
void Can_Test_SetControllerState(Can_ControllerStateType state);
void Can_Test_SetConfigPtr(Can_ConfigType* config);

/** [テスト専用] ソフトウェア bus-off フォールバックカウンタ Can_TxErrCount の
 *  取得・リセット。Can_Init() ではリセットされない（実装参照）ため、テストの
 *  SetUp() で毎回明示的にリセットしないとテストケースをまたいで値が持ち越される。 */
uint8 Can_Test_GetTxErrCount(void);
void  Can_Test_ResetTxErrCount(void);
#endif

#ifdef __cplusplus
}
#endif

#endif

/**
 * \file    Dcm.h
 * \brief   DCM 公開インタフェース (AUTOSAR SWS_DCM 準拠)
 * \details EcuM が呼び出す DCM_Init() を宣言する。
 *          PduR から呼び出されるコールバック群は Dcm_Cbk.h で宣言する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DCM_H
#define DCM_H

#include "Dcm_Cbk.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   DCM モジュールを初期化する。
 *
 * \details セッション状態を Default Session にリセットする
 *          (AUTOSAR SWS_Dcm_00769)。
 *          EcuM_Init() から Com_Init() の後に呼び出すこと。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* DCM_H */

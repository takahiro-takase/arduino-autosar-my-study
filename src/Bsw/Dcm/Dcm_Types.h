/**
 * \file    Dcm_Types.h
 * \brief   DCM 型定義 (AUTOSAR SWS_DCM 準拠)
 * \details BswM.h 等、Dcm.h/Dcm_Cbk.h 一式を丸ごと引き込みたくない他モジュールの
 *          ヘッダから軽量に参照できるよう、型定義のみをここへ分離する
 *          （Com_Types.h/CanIf_Types.h と同じ設計パターン）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DCM_TYPES_H
#define DCM_TYPES_H

#include "Platform_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   UDS SID 0x28 (CommunicationControl) の要求内容を表す通信モード型
 *          [SWS_Dcm_00981]。
 *
 * \details controlType（Rx/Tx の有効/無効の組み合わせ、4 通り）と
 *          communicationType（normal/networkManagement/両方、3 通り）の
 *          全 12 通りの組み合わせを表す。値は
 *          `controlType + (communicationType - 1) * 4` で一意に決まる
 *          （`DCM_COMMCTRL_*`/`DCM_COMMTYPE_*` は `Dcm_Cbk.c` 参照）。
 *          `BswM_Dcm_CommunicationMode_CurrentState()` へ渡す。
 */
typedef uint8 Dcm_CommunicationModeType;

#define DCM_ENABLE_RX_TX_NORM             0x00U  /**< normal: Rx/Tx とも有効化 */
#define DCM_ENABLE_RX_DISABLE_TX_NORM     0x01U  /**< normal: Rx 有効化・Tx 無効化 */
#define DCM_DISABLE_RX_ENABLE_TX_NORM     0x02U  /**< normal: Rx 無効化・Tx 有効化 */
#define DCM_DISABLE_RX_TX_NORM            0x03U  /**< normal: Rx/Tx とも無効化 */
#define DCM_ENABLE_RX_TX_NM               0x04U  /**< NM通信: Rx/Tx とも有効化 */
#define DCM_ENABLE_RX_DISABLE_TX_NM       0x05U  /**< NM通信: Rx 有効化・Tx 無効化 */
#define DCM_DISABLE_RX_ENABLE_TX_NM       0x06U  /**< NM通信: Rx 無効化・Tx 有効化 */
#define DCM_DISABLE_RX_TX_NM              0x07U  /**< NM通信: Rx/Tx とも無効化 */
#define DCM_ENABLE_RX_TX_NORM_NM          0x08U  /**< normal+NM: Rx/Tx とも有効化 */
#define DCM_ENABLE_RX_DISABLE_TX_NORM_NM  0x09U  /**< normal+NM: Rx 有効化・Tx 無効化 */
#define DCM_DISABLE_RX_ENABLE_TX_NORM_NM  0x0AU  /**< normal+NM: Rx 無効化・Tx 有効化 */
#define DCM_DISABLE_RX_TX_NORM_NM         0x0BU  /**< normal+NM: Rx/Tx とも無効化 */

#ifdef __cplusplus
}
#endif

#endif /* DCM_TYPES_H */

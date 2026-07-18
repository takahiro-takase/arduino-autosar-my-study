/**
 * \file    E2EXf_PBCfg.h
 * \brief   E2E Transformer ポストビルド設定 公開インタフェース
 *
 * \details 各 I-PDU に対応する E2EXf_RxConfigType/E2EXf_TxConfigType
 *          インスタンスを公開する。Rte.c が RxIndicationCbk/TxTransformCbk
 *          の実体（Rte_COMCbk_ 系 / Rte_COMTransform_ 系）から参照する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1/4.2.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2EXF_PBCFG_H
#define E2EXF_PBCFG_H

#include "E2EXf.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const E2EXf_RxConfigType E2EXf_EngineInfoRxCfg;
extern const E2EXf_RxConfigType E2EXf_AbsInfoRxCfg;
extern const E2EXf_TxConfigType E2EXf_E2EHealthStatusTxCfg;

/**
 * \brief   E2EXf の全 Check/Protect ステートを初期化し、E2EXf モジュール
 *          自身の初期化完了もマークする。
 *
 * \details EcuM_Init() から Com_Init() の後に 1 度だけ呼び出すこと。
 *          以前は Com_Init() が E2ECheckState/E2EProtectState を直接
 *          初期化していたが、Com が E2E を関知しなくなったため、この
 *          初期化は E2EXf 側の責務として移設した。
 *          各 I-PDU の State 初期化後、最後に E2EXf_Init()（SWS_E2EXf_00130
 *          の初期化状態フラグを立てる）を呼ぶ。
 */
void E2EXf_PBCfg_Init(void);

#ifdef __cplusplus
}
#endif

#endif

/**
 * \file    E2EXf_PBCfg.h
 * \brief   E2E Transformer ポストビルド設定 公開インタフェース
 *
 * \details 各 I-PDU に対応する E2EXf_RxConfigTypeP05/E2EXf_TxConfigTypeP05
 *          インスタンスを公開する。`E2EXf.c` の各インスタンス専用関数
 *          （`E2EXf_Inv_EngineInfo()`等）が内部で直接参照する
 *          （2026-09 是正、以前は Rte.c 側の RxIndicationCbk/TxTransformCbk
 *          実体が直接参照していたが、Config を引数に取らないインスタンス
 *          専用関数へ変更したため、参照元は E2EXf.c 内へ移った）。
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

extern const E2EXf_RxConfigTypeP05 E2EXf_EngineInfoRxCfg;
extern const E2EXf_RxConfigTypeP05 E2EXf_AbsInfoRxCfg;
extern const E2EXf_TxConfigTypeP05 E2EXf_E2EHealthStatusTxCfgP05;

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

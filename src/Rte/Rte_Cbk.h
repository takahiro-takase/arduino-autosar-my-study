/**
 * \file    Rte_Cbk.h
 * \brief   RTE が提供するコールバック関数のプロトタイプ宣言
 * \details [SWS_Rte_03795]/[SWS_Rte_03796] に従い、COM が呼び出すコールバック
 *          （受信通知・送信確認・タイムアウト通知・I-PDU コールアウト・
 *          Transformer グルー）のプロトタイプを Rte.h とは別ファイルに
 *          まとめる。Com_PBCfg.c は [SWS_Com_00220]（ComUserCbkHeaderFile）に
 *          相当するユーザコールバックヘッダとして本ファイルをインクルードし、
 *          Rte.c は定義側として本ファイルをインクルードする（宣言と定義の
 *          署名の食い違いをコンパイラに検出させるため）。
 *
 *          SecOC の検証結果通知コールアウト
 *          （SecOCVerificationStatusCallout）も RTE が提供する関数のため、
 *          本ファイルに併記する（本プロジェクトのまとめ方であり、
 *          SWS_Rte_03796 が直接規定する対象は COM コールバックのみ）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * 
ote    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef RTE_CBK_H
#define RTE_CBK_H

#include "Std_Types.h"
#include "SecOC_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void    Rte_COMRxInd_EngineInfo(void);
void    Rte_COMRxInd_AbsInfo(void);
void    Rte_COMTransform_E2EHealthStatus(uint8* Data, uint8 Length);
void    Rte_COMInvalidNotify_CoolantTemp(void);
void    Rte_COMFilterReject_EngineSpeed(void);
void    Rte_COMCbkTAck_EngineState(void);
void    Rte_COMCbkTxTOut_EngineState(void);
void    Rte_COMCbkTAck_WarningStatus(void);
void    Rte_COMCbkTxTOut_WarningStatus(void);
void    Rte_COMCbk_EngineOnFlag(void);
void    Rte_COMCbk_AbsInfo(void);
void    Rte_COMRxInd_SecureCommand(void);
void    Rte_COMCbkRxTOut_EngineOnFlag(void);
void    Rte_COMCbkRxTOut_AbsInfo(void);
boolean Rte_COMRxIpduCallout_SecureCommand(const uint8* SduDataPtr, uint8 SduLength);
boolean Rte_COMTxIpduCallout_ImmobilizerStatus(const uint8* SduDataPtr, uint8 SduLength);
void    Rte_SecOCVerificationStatus_ImmobilizerCmd(SecOC_VerificationStatusType status);

#ifdef __cplusplus
}
#endif

#endif /* RTE_CBK_H */

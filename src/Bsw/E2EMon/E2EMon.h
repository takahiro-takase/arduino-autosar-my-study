/**
 * \file    E2EMon.h
 * \brief   E2E 検証ネットワーク健全性モニタ 公開インタフェース
 * \details 実 AUTOSAR の標準モジュールには存在しない、独自の CDD
 *          （Complex Device Driver）相当のモジュール。EngineInfo/AbsInfo の
 *          E2E 検証結果（E2EXf/Rte 層が算出）を購読し、CRC 不一致・
 *          シーケンス異常の累積回数を集計して、Com シグナル経由で
 *          ブロードキャストする「ネットワーク健全性テレメトリ」を提供する。
 *
 *          実務での設計判断（ASW ではなく PF/BSW 側へ配置）を反映し、
 *          標準モジュール（E2EXf/Rte/Com）自体は無改造のまま、
 *          「独自の CDD を書き、標準モジュールの通知フック経由で配線する」
 *          という AUTOSAR で一般的なパターンを模している。
 *          送信タイミングのスケジューリングは本モジュールの責務ではなく、
 *          Com 自身の PERIODIC 送信モード（Com_Cfg.h の
 *          COM_TX_PERIOD_E2EHEALTH_MS）が担う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2EMON_H
#define E2EMON_H

#include "Std_Types.h"
#include "E2E_P01.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   E2EMon モジュールを初期化する。
 *
 * \details 累積カウンタ（CRC 不一致回数・シーケンス異常回数）を 0 へ
 *          リセットする。RAM のみのカウンタのため、NvM からの復元は行わない。
 *
 * \pre        Com_Init() が正常に完了していること
 *             （E2EMon_NotifyCheckResult() が Com_SendSignal() を呼ぶため）。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2EMon_Init(void);

/**
 * \brief   E2E チェック結果を通知する。
 *
 * \details EngineInfo/AbsInfo いずれの E2E 検証結果でも、フレーム受信の都度
 *          呼び出される想定（Rte.c の Rte_COMCbk_EngineInfo()/AbsInfo() から
 *          呼ばれる。実 AUTOSAR で言う「ARXML で設定した OnDataReceived
 *          通知フックが RTE から生成され、独自 CDD の関数を呼ぶ」という
 *          接続方式を模したもの）。
 *
 *          `E2E_P01STATUS_WRONGCRC` は CRC 不一致累積カウンタを、
 *          `E2E_P01STATUS_WRONGSEQUENCE`/`E2E_P01STATUS_REPEATED` は
 *          シーケンス異常累積カウンタをそれぞれ +1 する（0xFF で飽和）。
 *          その他の状態（OK/INITIAL/SYNC/OKSOMELOST/ERROR）はどちらの
 *          カウンタも変化させない。カウンタ値は毎回 Com_SendSignal() で
 *          COM_SIGNAL_E2E_CRC_ERR_COUNT/COM_SIGNAL_E2E_SEQ_ERR_COUNT へ
 *          反映する（実際に CAN へ送信するタイミングは Com 自身の
 *          PERIODIC モードが独立に判断する）。
 *
 * \param[in]  status  E2E_P01Check() の生の検証結果（8 状態）。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2EMon_NotifyCheckResult(E2E_P01StatusType status);

#ifdef __cplusplus
}
#endif

#endif /* E2EMON_H */

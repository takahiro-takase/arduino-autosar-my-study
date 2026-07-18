/**
 * \file    E2EMon.c
 * \brief   E2E 検証ネットワーク健全性モニタ 実装
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "E2EMon.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "Det.h"

#define TAG "E2EMon"

/* E2EXf/Rte 層（標準モジュール）とは完全に独立した、本 CDD 自身が保持する
 * 累積カウンタ。RAM のみ・0xFF で飽和（Dem の ExtendedData 累積カウンタと
 * 同じ方針）。EngineInfo/AbsInfo 双方からの通知を合算する。 */
static uint8 E2EMon_CrcErrorCount      = 0U;
static uint8 E2EMon_SequenceErrorCount = 0U;

void E2EMon_Init(void)
{
    E2EMon_CrcErrorCount      = 0U;
    E2EMon_SequenceErrorCount = 0U;
    DET_LOGI(TAG, "Init ok");
}

void E2EMon_NotifyCheckResult(E2E_P01StatusType status)
{
    if (status == E2E_P01STATUS_WRONGCRC)
    {
        if (E2EMon_CrcErrorCount < 0xFFU)
            E2EMon_CrcErrorCount++;
    }
    else if ((status == E2E_P01STATUS_WRONGSEQUENCE) || (status == E2E_P01STATUS_REPEATED))
    {
        if (E2EMon_SequenceErrorCount < 0xFFU)
            E2EMon_SequenceErrorCount++;
    }

    /* 値をセットするだけで、送信タイミングには一切関与しない。実際に
     * CAN へ送信するかどうか・いつ送信するかは Com 自身の PERIODIC
     * モード（Com_MainFunction()）が独立に判断する。 */
    (void)Com_SendSignal(COM_SIGNAL_E2E_CRC_ERR_COUNT, &E2EMon_CrcErrorCount);
    (void)Com_SendSignal(COM_SIGNAL_E2E_SEQ_ERR_COUNT, &E2EMon_SequenceErrorCount);
}

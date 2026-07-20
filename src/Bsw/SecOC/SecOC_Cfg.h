/**
 * \file    SecOC_Cfg.h
 * \brief   SecOC プリコンパイル設定 (AUTOSAR SWS_SecureOnboardCommunication 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SECOC_CFG_H
#define SECOC_CFG_H

#include "Platform_Types.h"

/** RX Secured I-PDU テーブルのエントリ数
 *  [0]=ImmobilizerCmd (CAN 0x120, KeyFobEcu からの想定) */
#define SECOC_RX_PDU_COUNT  1U

/** TX Secured I-PDU テーブルのエントリ数
 *  [0]=E2EHealthStatus (CAN 0x220、E2E 保護済みペイロードをさらに認証) */
#define SECOC_TX_PDU_COUNT  1U

#endif /* SECOC_CFG_H */

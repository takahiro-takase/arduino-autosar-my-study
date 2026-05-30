/**
 * \file    ComM_Cfg.h
 * \brief   ComM プリコンパイル設定 (AUTOSAR SWS_ComM 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COMM_CFG_H
#define COMM_CFG_H

/** 管理チャネル数 */
#define COMM_CHANNEL_COUNT  1U

/** 管理ユーザ数 */
#define COMM_USER_COUNT     1U

/** チャネル 0 ID */
#define COMM_CHANNEL_0      0U

/** ユーザ 0 ID（EcuM / アプリが通信モードを要求する際に使用） */
#define COMM_USER_0         0U

#endif /* COMM_CFG_H */

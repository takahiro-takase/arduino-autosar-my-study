/**
 * \file    EcuM_Cfg.h
 * \brief   ECU ステートマネージャ プリコンパイル設定 (AUTOSAR SWS_EcuStateManager 準拠)
 * \details EcuM RUN フェーズを要求できるユーザと POST_RUN タイムアウト値を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに相当する。
 *
 *          RUN ユーザ:
 *            ECUM_USER_COMM — ComM が CAN バスを FULL_COM で使用中は RUN を要求する。
 *                             Bus-Off 回復断念時に RUN を解放し、
 *                             EcuM が POST_RUN → SHUTDOWN へ遷移するトリガになる。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ECUM_CFG_H
#define ECUM_CFG_H

/** RUN 要求ユーザ総数 */
#define ECUM_USER_COUNT              1U

/** RUN 要求ユーザ ID: ComM チャネル 0 */
#define ECUM_USER_COMM               0U

/** POST_RUN フェーズタイムアウト (ms)
 *  この時間内に誰も RUN を要求しなければ SHUTDOWN へ遷移する。 */
#define ECUM_POST_RUN_TIMEOUT_MS     5000UL

#endif /* ECUM_CFG_H */

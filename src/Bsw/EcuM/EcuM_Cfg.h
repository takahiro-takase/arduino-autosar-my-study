/**
 * \file    EcuM_Cfg.h
 * \brief   ECU ステートマネージャ プリコンパイル設定 (AUTOSAR SWS_EcuStateManager 準拠)
 * \details EcuM RUN フェーズを要求できるユーザと POST_RUN タイムアウト値を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに相当する。
 *
 *          RUN ユーザ:
 *            ECUM_USER_COMM — ComM が CAN バスを FULL_COM で使用中は RUN を要求する。
 *                             App_EngineManager によるボランタリスリープ突入時
 *                             (NO_COM 要求) に RUN を解放し、EcuM が
 *                             POST_RUN → SHUTDOWN へ遷移するトリガになる。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ECUM_CFG_H
#define ECUM_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * [SWS_EcuM_04032]: SWS_EcuStateManager の Development Error コード値は
 * AUTOSAR 標準では固定されておらず「実装時に割り当てる」と規定されている
 * （Com/Can 等、他の多くのモジュールとは異なる点）。本実装では以下の値を
 * 独自に割り当てる。ModuleId も同様に SWS 本文には明記されないため、
 * AUTOSAR_TR_BSWModuleList（Release 4.3.1、docs/ 配下）の
 * 「List of Basic Software Modules」表で ECU State Manager (EcuM) に
 * 割り当てられた固定値 10 を使う。
 * ----------------------------------------------------------------------- */

/** AUTOSAR ECU State Manager の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 10） */
#define ECUM_MODULE_ID  10U

/** 開発エラーコード（値は実装時割り当て。実際に使用する分のみ定義） */
#define ECUM_E_INVALID_PAR              0x02U  /* 不正なパラメータ（user が範囲外 等） */
#define ECUM_E_NULL_POINTER             0x03U  /* [Table 7]: NULL ポインタチェック（値は実装時割り当て） */
#define ECUM_E_MULTIPLE_RUN_REQUESTS    0x20U  /* [SWS_EcuM_04125]: 同一ユーザの多重 RUN 要求 */
#define ECUM_E_MISMATCHED_RUN_RELEASE   0x21U  /* [SWS_EcuM_04127]: 対応する要求のない解放 */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define ECUM_API_ID_INIT           0x01U
#define ECUM_API_ID_REQUEST_RUN    0x03U
#define ECUM_API_ID_RELEASE_RUN    0x04U
#define ECUM_API_ID_GET_VERSION_INFO 0x00U

/** バージョン情報（SWS_EcuM_GetVersionInfo、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define ECUM_VENDOR_ID          0U
#define ECUM_SW_MAJOR_VERSION   1U
#define ECUM_SW_MINOR_VERSION   0U
#define ECUM_SW_PATCH_VERSION   0U

/** RUN 要求ユーザ総数 */
#define ECUM_USER_COUNT              1U

/** RUN 要求ユーザ ID: ComM チャネル 0 */
#define ECUM_USER_COMM               0U

/** POST_RUN フェーズタイムアウト (ms)
 *  この時間内に誰も RUN を要求しなければ SHUTDOWN へ遷移する。 */
#define ECUM_POST_RUN_TIMEOUT_MS     5000UL

#endif /* ECUM_CFG_H */

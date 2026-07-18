/**
 * \file    E2EXf.h
 * \brief   E2E Transformer 公開インタフェース (AUTOSAR SWS_E2ELibrary 12.4 準拠)
 *
 * \details AUTOSAR が定義する 3 通りの E2E 統合方式のうち「E2E Transformer」
 *          （docs/AUTOSAR_SWS_E2ELibrary.pdf 12.4 節、AUTOSAR R4.2.1 以降）を
 *          模した薄いラッパー。Com は E2E の存在を一切知らず（Com_Types.h の
 *          RxIndicationCbk / TxTransformCbk 汎用フック経由で呼ばれるだけ）、
 *          実際の CRC/Counter 検証・付与は本モジュールが E2E_P01.c（Profile 01
 *          の実処理）へ委譲する形で行う。
 *
 *          実 AUTOSAR の Transformer は RTE 生成コードが「Transformer チェーン」
 *          を自動生成するが、本プロジェクトには RTE ジェネレータが無いため、
 *          Rte.c が Com_ReceiveSignalGroupArray()/Com_IsRxTimedOut() 経由で
 *          明示的にこのモジュールの API を呼び出す、静的に書き下した相当品として
 *          実装している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1/4.2.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2EXF_H
#define E2EXF_H

#include "Std_Types.h"
#include "E2E_P01.h"
#include "Dem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * RX 側（Inverse Transformer）設定
 * 1 I-PDU につき 1 インスタンス。DemEventId はどの Dem イベントへ結果を
 * 報告するかを設定側で決め、E2EXf.c 本体には IPduId のハードコード比較を
 * 埋め込まない（Com.c で確立した既存の設計方針を踏襲）。
 * ----------------------------------------------------------------------- */
typedef struct
{
    const E2E_P01ConfigType* E2EConfig;
    E2E_P01CheckStateType*   CheckState;
    Dem_EventIdType          DemEventId;
} E2EXf_RxConfigType;

/* -----------------------------------------------------------------------
 * TX 側（Transformer）設定
 * ----------------------------------------------------------------------- */
typedef struct
{
    const E2E_P01ConfigType* E2EConfig;
    E2E_P01ProtectStateType* ProtectState;
} E2EXf_TxConfigType;

/**
 * \brief   E2EXf モジュール自身を初期化済み状態にする。
 *
 * \details SWS_E2EXf_00130: E2E Transformer は「E2EXf_Init() が呼ばれたか」
 *          という初期化状態を、下位の E2E_P01 Check/ProtectState とは別に
 *          自身で保持しなければならない。SWS_E2EXf_00133/00151 により、
 *          未初期化のまま E2EXf_InverseTransform()/E2EXf_Transform() が
 *          呼ばれた場合は処理を行わず安全側で早期 return する。
 *
 *          `E2EXf_PBCfg_Init()`（`src/Bsw/E2EXf/E2EXf_PBCfg.c`）が各 I-PDU の
 *          E2E_P01Check/ProtectState を初期化した最後に、本関数を呼んで
 *          初期化完了をマークする。
 *
 * \pre        EcuM_Init() から Com_Init() の後、フレーム受信・送信が
 *             始まる前に呼び出すこと。
 */
void E2EXf_Init(void);

/**
 * \brief   RX I-PDU バイト列に対する E2E Inverse Transform（検証）を行う。
 *
 * \details E2E_P01Check() を呼び、結果を Dem_ReportErrorStatus() で
 *          Config->DemEventId へ報告する。OK/OKSOMELOST/SYNC/INITIAL の
 *          4状態はいずれも CRC が正しい（データ自体は信頼できる）ため
 *          E_OK を返す。SYNC は WRONGSEQUENCE 検知後の再ロック中で
 *          シーケンスの継続性はまだ完全には確定していないが、個々の
 *          フレームの CRC・カウンタ自体は正常範囲内なのでデータは
 *          使用してよいと判断している。
 *          REPEATED（重複）・WRONGCRC・WRONGSEQUENCE・ERROR は E_NOT_OK。
 *
 * \param[in]  Config       RX 側設定。NULL 禁止。
 * \param[in]  Buffer       検証対象の I-PDU バイト列。NULL 禁止。
 * \param[in]  Length       Buffer のバイト数。
 * \param[out] CheckStatus  E2E_P01Check() の生の 8 状態を受け取る。NULL 禁止。
 *                          呼び出し元（Rte.c）が Rte_IStatusType へマッピング
 *                          し直すための詳細情報で、Dem への報告方針（PASSED/
 *                          FAILED の 2値化）には影響しない。
 *
 * \retval  E_OK      検証に合格した。呼び出し元は Buffer の内容を使ってよい。
 * \retval  E_NOT_OK  検証に失敗した、または E2EXf_Init() 未呼び出し
 *                    （SWS_E2EXf_00133 相当）。呼び出し元は Buffer の内容を
 *                    破棄すべき（前回の有効値を保持し続けるか、タイムアウト
 *                    経由でフェイルセーフへ移行する）。
 */
Std_ReturnType E2EXf_InverseTransform(const E2EXf_RxConfigType* Config, const uint8* Buffer, uint8 Length,
                                       E2E_P01StatusType* CheckStatus);

/**
 * \brief   TX I-PDU バイト列に対する E2E Transform（Counter/CRC 付与）を行う。
 *
 * \details E2E_P01Protect() を呼び、Buffer へ Counter・CRC8 を書き込む。
 *          E2EXf_Init() 未呼び出しの場合は何もしない（SWS_E2EXf_00133 相当）。
 *
 * \param[in]     Config  TX 側設定。NULL 禁止。
 * \param[in,out] Buffer  変換対象の I-PDU バイト列（上書きされる）。NULL 禁止。
 * \param[in]     Length  Buffer のバイト数。
 */
void E2EXf_Transform(const E2EXf_TxConfigType* Config, uint8* Buffer, uint8 Length);

#ifdef __cplusplus
}
#endif

#endif

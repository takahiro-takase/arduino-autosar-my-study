/**
 * \file    CanTp.h
 * \brief   CAN トランスポートプロトコル 公開インタフェース
 *          (AUTOSAR SWS_CanTp 準拠, ISO 15765-2)
 * \details CanIf/PduR と DCM の間に位置し、Single Frame (SF) /
 *          First Frame (FF) / Consecutive Frame (CF) /
 *          Flow Control (FC) の送受信を仲介する。
 *
 *          RX フロー:
 *            PduR_CanIfRxIndication → CanTp_RxIndication
 *            CanTp: SF/FF → Dcm_StartOfReception、各フレームのデータ → Dcm_CopyRxData、
 *                   完了 → Dcm_TpRxIndication（組み立ては、Dcm の受信バッファの上で行う）
 *
 *          TX フロー:
 *            Dcm → CanTp_Transmit (応答の長さだけ)
 *            CanTp: SF/FF/各 CF のデータ → Dcm_CopyTxData、フレームを送信 → PduR_CanTpTransmit → CanIf → CAN 0x7E8、
 *                   完了 → Dcm_TpTxConfirmation
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANTP_H
#define CANTP_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "CanTp_Cfg.h"
#include "ComStack_Types.h"
#include "Std_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/**
 * \brief   CanTp_Init() の設定引数型（不透明型）。
 *
 * \details SWS_CanTp_00208 は post-build 設定データへのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を持たない
 *          ため、中身を定義しない不透明型とし、ポインタとしてのみ扱う
 *          （`KeyM_ConfigType` と同じ簡略化パターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct CanTp_ConfigType_Tag CanTp_ConfigType;

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/**
 * \brief   CanTp モジュールを初期化する。
 *
 * \details RX/TX チャネルの状態を IDLE にリセットする (SWS_CanTp_00208)。
 *          EcuM_Init() から PduR_Init() の後に呼び出すこと。
 *
 * \param[in]  CfgPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                     持たないため）。
 *
 * \AUTOSARReq     {SWS_CanTp_00208}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_Init(const CanTp_ConfigType* CfgPtr);

/**
 * \brief   DCM の UDS 応答を CAN トランスポートフレームに分割して送信する。
 *
 * \details PduInfoPtr の SduLength <= 7 なら Single Frame として送信
 *          (SWS_CanTp_00231)。8 バイト以上なら First Frame + Consecutive Frame
 *          に分割し (SWS_CanTp_00232)、Flow Control 受信後に CF を順次送信する。
 *
 * \param[in]  TxSduId     TX N-SDU ID (CANTP_TX_SDU_ID)。本実装は 1 チャネル固定。
 * \param[in]  PduInfoPtr  送信する UDS 応答の長さ（SduLength）。データ（SduDataPtr）は使わず、
 *                         フレームを組み立てる時に Dcm_CopyTxData() で取り出す。
 *                         SduLength は 4095 を超えてはならない（FF の長さ欄は 12 bit）。
 *
 * \retval  E_OK      送信要求を受け付けた (SF) または FF を送信した (MF)。
 * \retval  E_NOT_OK  NULL ポインタ、長さ超過、TX チャネルがビジー、
 *                    または SF/FF の物理送信が同期的に失敗した
 *                    (PduR/CanIf/Can_Write が拒否した)。
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanTp_Transmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   タイムアウト監視と CF 送信を周期的に処理する。
 *
 * \details EcuM_MainFunction から毎ループ呼び出す。以下を担当する:
 *          - TX WAIT_FC 状態: N_Bs タイムアウト検出 → TX 中断
 *          - TX SEND_CF 状態: STmin 経過後に次の CF を送信
 *          - RX WAIT_CF 状態: N_Cr タイムアウト検出 → RX 中断
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_MainFunction(void);

/**
 * \brief   CanTp モジュールのバージョン情報を取得する。
 *
 * \details CanTp_Init と並び、未初期化時でも CANTP_E_UNINIT を報告しない
 *          例外 API（CanTp_Cfg.h 冒頭コメント参照）のため、初期化状態は
 *          確認せず NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_GetVersionInfo(Std_VersionInfoType* versioninfo);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/**
 * \brief   PduR から配信された受信 CAN フレームを処理する。
 *
 * \details フレームタイプ (SF/FF/CF/FC) を判定し、
 *          SF/FF/CF のデータを、Dcm の TP の API（Dcm_StartOfReception / Dcm_CopyRxData / Dcm_TpRxIndication）で渡す
 *          (SWS_CanTp_00214)。
 *          FC フレームは TX 側の FC 待ちを解除する。
 *
 * \param[in]  RxPduId     受信 PDU ID (本実装では単一チャネルのため未使用)。
 * \param[in]  PduInfoPtr  受信した 8 バイト CAN フレームへのポインタ。
 *
 * \ServiceID      {0x42}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   PduR からの送信完了通知を受け取る。
 *
 * \details CanIf がフレーム送信を完了すると PduR を経由して呼び出される
 *          (SWS_CanTp_00236)。本実装では現在 no-op（意図的。詳細は CanTp.c の
 *          実装コメント参照）: 本プロジェクトの非同期 TX 確認経路は真の送信
 *          失敗を表現できず、result は実質的に常に E_OK しか渡ってこない。
 *          実際に到達可能な送信失敗は CanTp_SendFrame() の同期的な戻り値と
 *          CF 送信の N_As 相当タイムアウトで別途検出している。
 *
 * \param[in]  TxPduId  送信完了した PDU の ID。
 * \param[in]  result   送信結果 (E_OK / E_NOT_OK。上記理由により本経路では
 *                      実質的に常に E_OK)。
 *
 * \ServiceID      {0x40}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef CANTP_UNIT_TEST
/**
 * \brief   [テスト専用] CanTp_Initialized を未初期化状態へ戻す。
 *
 * \details CanTp には DeInit() に相当する API が無く、`CanTp_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          一度 `CanTp_Init()` を呼ぶと他のテストファイルの実行順に関わらず
 *          「未初期化状態」を再現できなくなる。native 環境のホストテストから
 *          のみ使用する。`CANTP_UNIT_TEST` は CMakeLists.txt の native_chain
 *          ターゲットでのみ定義され、実機ビルド（`uno_r4`）では定義されない
 *          ため、実機の `CanTp.h`/`CanTp.c` には一切含まれない（AUTOSAR
 *          標準外の関数。`Mcu_Test_ResetInitState()` と同じ設計方針）。
 */
void CanTp_Test_ResetInitState(void);

/**
 * \brief   [テスト専用] TX チャネルが、送信の途中（FC 待ち、または CF の送信中）か。
 *
 * \details 以前は公開 API（CanTp_IsTxBusy）だったが、Dcm が TP の送信確認（Dcm_TpTxConfirmation）で
 *          自分の送信状態を持つようになり、不要になった。マルチフレーム送信の途中・完了を、
 *          テストが確認するためだけに残している。
 *
 * \retval  TRUE   送信の途中。
 * \retval  FALSE  IDLE。
 */
boolean CanTp_Test_IsTxBusy(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CANTP_H */

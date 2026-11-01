/**
 * \file    SecOC.h
 * \brief   Secure Onboard Communication 公開インタフェース (AUTOSAR SWS_SecOC 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SECOC_H
#define SECOC_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "SecOC_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * SecOC_VerifyStatusOverride() の overrideStatus 引数値（[SWS_SecOC_00122]）
 * ----------------------------------------------------------------------- */
#define SECOC_OVERRIDE_FAIL_INDEFINITE  0U   /**< VerifyStatus を Fail に強制（解除まで継続） */
#define SECOC_OVERRIDE_FAIL_COUNTED     1U   /**< VerifyStatus を Fail に強制（指定回数のみ） */
#define SECOC_OVERRIDE_CANCEL           2U   /**< オーバーライドを解除                        */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* overrideStatus=41 (Pass 強制) は本プロジェクトでは非対応。
 * SecOC_VerifyStatusOverride() の Doxygen 参照。 */
/**
 * \brief   SecOC モジュールを初期化する。
 *
 * \details 設定ポインタを保存し、各 Secured I-PDU のフレッシュネス状態
 *          （最後に受理した Freshness Value）を初期化する。
 *          AES-128 の自己診断は Crypto_Init()（Csm/CryIf/Crypto レイヤ、
 *          EcuM_Init() から本関数より前に呼ばれる）が担う。
 *          config->RxPduCount/TxPduCount が本プロジェクトのテーブル上限
 *          （SECOC_RX_PDU_COUNT/SECOC_TX_PDU_COUNT）を超える場合は
 *          `SECOC_E_INIT_FAILED` を報告し初期化を行わない
 *          （[SWS_SecOC_00101]。2026-09 追加。以前はログのみで DET 報告
 *          していなかった）。
 *
 * \param[in]  config  SecOC 設定構造体。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_SecOC_00101}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void SecOC_Init(const SecOC_ConfigType* config);

/**
 * \brief   SecOC モジュールを未初期化状態に戻す。
 *
 * \details 設定ポインタを NULL に戻すとともに、全ての内部グローバル変数
 *          (RX 側のフレッシュネス状態・VerifyStatusOverride 設定、TX 側の
 *          保留フラグ・フレッシュネスカウンタ)と Secured I-PDU のバッファ
 *          (TX Authentic I-PDU バッファ)をクリアする（[SWS_SecOC_00157]、
 *          2026-09 追加。以前は設定ポインタの NULL 化のみで内部状態が
 *          残ったままだった）。未初期化状態で呼ばれた場合は
 *          SECOC_E_UNINIT を報告し何もしない。
 *
 * \AUTOSARReq     {SWS_SecOC_00161, SWS_SecOC_00157}
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void SecOC_DeInit(void);

/**
 * \brief   PduR から呼ばれる、Secured I-PDU 受信時の検証エントリポイント。
 *
 * \details `PduR_RxIndicationFctType` と同じシグネチャを持ち、PduR の
 *          `PduR_RxDestType.RxIndFct` へ直接登録できる（Com_RxIndication /
 *          CanTp_RxIndication と同じ立ち位置の PduR 宛先モジュール）。
 *
 *          Secured I-PDU を Authentic Payload / Freshness Value / 切り詰め
 *          MAC に分離し、Csm_MacVerify()（Csm/CryIf/Crypto レイヤ経由で
 *          AES-128-CMAC を再計算）で MAC が一致するか検証する
 *          （[SWS_SecOC_00192] SecOC Profile 1）。続けてフレッシュネス
 *          が単調増加しているか（リプレイでないか）を確認する。両方成功した
 *          場合のみ、Authentic Payload を `Com_RxIndication()` へ転送する。
 *          いずれかに失敗した場合はログ出力のみ行い、Com へは一切転送しない
 *          （検証されていないデータを上位層へ渡さないことが本モジュールの
 *          存在意義そのものであるため）。
 *
 *          `Csm_MacVerify()` 自体がサービスを提供できなかった場合（MAC
 *          不一致という正常な検証結果とは別の、下位層自体の異常）は
 *          `SECOC_E_CRYPTO_FAILURE` を報告する（[SWS_SecOC_00166]。
 *          2026-09 追加）。
 *
 *          検証結果は、`SecOC_RxPduConfigType.VerificationStatusCallout` が
 *          設定されていれば `VerificationStatusPropagationMode` に従って
 *          都度通知する（[SWS_SecOC_00048]/[SWS_SecOC_00119]、2026-09-20
 *          追加）。報告する種別は `SecOC_VerifyStatusOverride()` 適用後の
 *          最終判定に基づく（詳細は `SecOC_VerificationResultType` 参照）。
 *
 * \param[in]  RxPduId     検証対象の SecOC RX Secured I-PDU ID
 *                         （SecOC_RxPduConfigType.SecOCRxPduId と照合する）。
 * \param[in]  PduInfoPtr  受信した Secured I-PDU のデータと長さ。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_SecOC_00166, SWS_SecOC_00048, SWS_SecOC_00119}
 * \ServiceID      {0x42}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
void SecOC_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   指定 Freshness Value の VerifyStatus を強制上書きする（[SWS_SecOC_00122]）。
 *
 * \details 実仕様はテスト・診断目的で「実際の認証結果に関わらず検証結果を
 *          強制する」任意サービス。CSM による実際の MAC 検証・フレッシュネス
 *          検証は本関数の呼び出し有無に関わらず常に行われる（本実装は
 *          `SecOC_RxIndication()` 内で実際の検証結果を求めたのち、本関数が
 *          設定したオーバーライド状態を適用して最終判定とする）。
 *
 *          実仕様は個別の Freshness Value を識別する `freshnessValueId`
 *          という専用の設定要素（Freshness Value Manager 相当）を持つが、
 *          本プロジェクトはそのような機構自体を持たず、Freshness Value は
 *          RX Secured I-PDU ごとに 1 対 1 で紐づく（`SecOC_RxPduConfigType`
 *          参照）。そのため本実装では `freshnessValueID` を対象の
 *          `SecOCRxPduId`（`SecOC_RxIndication()` の `RxPduId` と同じ値）と
 *          みなして扱う（学習用簡略化）。
 *
 *          `overrideStatus=41`（VerifyStatus を Pass に強制）は、実仕様
 *          自身が既定 FALSE の `SecOCEnableForcedPassOverride`
 *          （[ECUC_SecOC_00051]）で無効化されている危険な機能（未検証データを
 *          無条件に信頼させる）であり、本プロジェクトはこのコンフィグ切替を
 *          持たないため、常に非対応として `E_NOT_OK` を返す（安全側の既定を
 *          採用）。`overrideStatus=0`（無期限 Fail 強制）/`1`（回数指定 Fail
 *          強制）/`2`（解除）のみサポートする。
 *
 * \param[in]  freshnessValueID            対象の SecOCRxPduId
 *                                         （上記の簡略化参照）。
 * \param[in]  overrideStatus              SECOC_OVERRIDE_FAIL_INDEFINITE(0) /
 *                                         SECOC_OVERRIDE_FAIL_COUNTED(1) /
 *                                         SECOC_OVERRIDE_CANCEL(2) のいずれか。
 *                                         それ以外（41 含む）は非対応。
 * \param[in]  numberOfMessagesToOverride  overrideStatus=1 のときのみ有効。
 *                                         強制 Fail を適用する残りメッセージ数。
 *
 * \retval  E_OK      正常に設定/解除した。
 * \retval  E_NOT_OK  未初期化、freshnessValueID に一致する RX PDU が無い、
 *                    または overrideStatus が非対応の値。
 *
 * \AUTOSARReq     {SWS_SecOC_00122}
 * \ServiceID      {0x0b}
 * \Reentrancy     {Non Reentrant for the same FreshnessValueID. Reentrant for
 *                  different FreshnessValueIDs.}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType SecOC_VerifyStatusOverride(uint16 freshnessValueID, uint8 overrideStatus,
                                          uint8 numberOfMessagesToOverride);

/**
 * \brief   PduR から呼ばれる、Authentic I-PDU 送信要求のエントリポイント。
 *
 * \details `PduR_TxTransmitFctType` と同じシグネチャを持ち、PduR の
 *          `PduR_TxRoutingPathType.TransmitOverrideFct` へ直接登録できる
 *          （[7.4.1] "Authentication during direct transmission" の ad-hoc
 *          transmission フロー相当）。Authentic I-PDU を内部バッファへ
 *          コピーするだけで、Freshness/MAC の計算は一切行わず即座に E_OK を
 *          返す（[SWS_SecOC_00057]/[SWS_SecOC_00058]）。実際の Secured I-PDU
 *          組み立ては次回 SecOC_MainFunctionTx() で行う（[SWS_SecOC_00060]〜
 *          [SWS_SecOC_00062]）。
 *
 * \param[in]  TxPduId     送信対象の SecOC TX Secured I-PDU ID
 *                         （SecOC_TxPduConfigType.SecOCTxPduId と照合する）。
 * \param[in]  PduInfoPtr  送信する Authentic I-PDU のデータと長さ。NULL 禁止。
 *                         SduLength は対象エントリの AuthenticPduLength と
 *                         一致していること。
 *
 * \retval  E_OK      Authentic I-PDU を内部バッファへコピーした。
 * \retval  E_NOT_OK  SecOC 未初期化、PduInfoPtr が NULL、一致するエントリなし、
 *                    または SduLength が AuthenticPduLength と不一致。
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType SecOC_IfTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   下位層（CanIf 等）からの TX 完了通知エントリポイント。
 *
 * \details `PduR_TxRoutingPathType.ConfFct` へ直接登録できる（[7.4.1]
 *          "Authentication during direct transmission" の ad-hoc
 *          transmission フロー相当。[SWS_SecOC_00063]/[SWS_SecOC_00064]）。
 *          `TxPduId` に一致する TX エントリが見つかれば、結果を
 *          `PduR_SecOCTxConfirmation()` 経由で元の送信元（Com 等）へ中継する
 *          （[SWS_SecOC_00063]）。
 *
 *          [SWS_SecOC_00064] が要求する「Secured I-PDU を保持するバッファの
 *          解放」について: 本実装の TX バッファ（`SecOC_TxAuthenticBuffer`）は
 *          `SecOC_MainFunctionTx()` が `PduR_SecOCTransmit()` へコピーした
 *          時点で既に解放済み扱い（次の `SecOC_IfTransmit()` 呼び出しで
 *          上書きしてよい）であり、送信完了通知を待って別途ロック解除する
 *          仕組みは持たない。実運用への影響は無い（現状 TX 方向で SecOC を
 *          使う PDU が無いため。`SECOC_TX_PDU_COUNT` 参照）。
 *
 * \param[in]  TxPduId  送信完了が通知された SecOC TX Secured I-PDU ID
 *                      （SecOC_TxPduConfigType.SecOCTxPduId と照合する）。
 * \param[in]  result   E_OK: 送信成功。E_NOT_OK: 送信失敗。
 *
 * \AUTOSARReq     {SWS_SecOC_00126}
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
void SecOC_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/**
 * \brief   組み立て済みの Secured I-PDU を、下位層（PduR 経由で CanIf など）へ複製する（トリガ送信。[SWS_SecOC_00068]）。
 *
 * \details 下位層が、送信の直前に、PduR_<Lo>TriggerTransmit を通して、SecOC へ Secured I-PDU を取りに来る
 *          （[7.4.2] 節 "Authentication during triggered transmission"）。SecOC は、上位層の役割を自分で持ち、
 *          SecOC_MainFunctionTx() が組み立てて保持している Secured I-PDU（Authentic I-PDU、Freshness Value、
 *          切り詰めた MAC）を、呼び出し元のバッファへ複製し、SduLength を Secured I-PDU の長さへ更新する。
 *          TriggerTransmit の呼び出しを、上位層へは転送しない。
 *
 *          複製した Secured I-PDU は、下位層へ渡した時点で解放する（同じ Freshness Value の Secured I-PDU を
 *          2 回送ると、受信側がリプレイとして棄却するため）。その送信が確認された（SecOC_TxConfirmation() が
 *          E_OK で呼ばれた）ときに、保持している Authentic I-PDU を、新しく設定されたものとして扱い、次の
 *          SecOC_MainFunctionTx() で再び認証する（[SWS_SecOC_00069]）。下位層が、上位層の更新を待たずに、
 *          再び要求しても、新しい Freshness Value の Secured I-PDU を渡せる。
 *
 *          保持している Secured I-PDU が無い（まだ組み立てていない、複製済み、または、新しい Authentic I-PDU を
 *          受け取って、組み立て直し待ち）ときは、何も複製せず E_NOT_OK を返す（[SWS_SecOC_00150]）。
 *          バッファが Secured I-PDU の長さより小さいときも、何も複製せず、PduInfoPtr を変えずに E_NOT_OK を返す。
 *
 *          本番の設定は、TX 方向で SecOC を使う PDU を持たない（`SECOC_TX_PDU_COUNT` 参照）ので、本番コードからは呼ばれない。
 *
 * \param[in]     TxPduId     データを要求された SecOC TX Secured I-PDU の ID（SecOC_TxPduConfigType.SecOCTxPduId）。
 * \param[in,out] PduInfoPtr  SduDataPtr=複製先のバッファ、SduLength=その大きさ。戻るとき、複製した長さ。
 *                            NULL 禁止。SduDataPtr も NULL 禁止。
 *
 * \retval  E_OK      Secured I-PDU を複製した。SduLength が複製した長さを示す。
 * \retval  E_NOT_OK  未初期化、NULL ポインタ、TxPduId が不正、保持している Secured I-PDU が無い、またはバッファが小さい。
 *                    何も複製していない。
 *
 * \AUTOSARReq     {SWS_SecOC_00127, SWS_SecOC_00068, SWS_SecOC_00069, SWS_SecOC_00150}
 * \ServiceID      {0x41}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType SecOC_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr);

/**
 * \brief   周期実行関数。保留中の TX Secured I-PDU を組み立てて送信する。
 *
 * \details SecOC_IfTransmit() が内部バッファへコピーした Authentic I-PDU に
 *          対し、Freshness Value（自身が保持する単調増加カウンタ）と
 *          Csm_MacGenerate()（Csm/CryIf/Crypto レイヤ経由の AES-128-CMAC）を
 *          計算し、Secured I-PDU を組み立てて PduR_SecOCTransmit() で送信する。
 *          保留中の TX Secured I-PDU がなければ何もしない。
 *
 *          Csm_MacGenerate() が失敗したとき（[SWS_SecOC_00155]/[SWS_SecOC_00166]/[SWS_SecOC_00225]〜[SWS_SecOC_00228]）:
 *          MAC が未設定の Secured I-PDU は、送らず、保持もせず、Freshness Value も進めない。
 *          - 回復可能なエラー（CRYPTO_E_BUSY、CRYPTO_E_QUEUE_FULL）: 認証の組み立ての試行の回数（送信要求ごと。
 *            新しい送信要求で 0）を増やし、SecOC_TxPduConfigType.AuthenticationBuildAttempts（0 は 1 回だけ試す）に
 *            達していなければ、次の周期で、Freshness Value と MAC の計算をやり直す。
 *          - 上記以外の失敗（E_NOT_OK など。回復可能なエラーではない）、または、試行の回数が上限に達したとき:
 *            Authentic I-PDU を内部のバッファから捨てて、送信要求を取り消し（[SWS_SecOC_00229]）、上位層へは送信の失敗
 *            （PduR_SecOCTxConfirmation() の E_NOT_OK）を通知し、SECOC_E_CRYPTO_FAILURE を DET へ報告する。
 *
 * \pre        SecOC_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_SecOC_00176, SWS_SecOC_00155, SWS_SecOC_00166, SWS_SecOC_00225, SWS_SecOC_00226,
 *                  SWS_SecOC_00227, SWS_SecOC_00228, SWS_SecOC_00229}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void SecOC_MainFunctionTx(void);

/**
 * \brief   周期実行関数。RX 方向の認証・検証処理を行う（[SWS_SecOC_00171]）。
 *
 * \details 実仕様は RX 方向の認証・検証処理を非同期の検証キュー越しに周期
 *          処理する構成も許容するが、本プロジェクトの `SecOC_RxIndication()`
 *          は受信の都度 `Csm_MacVerify()` を SINGLECALL で同期実行し検証を
 *          完結させる設計のため、本関数が処理すべき保留中の RX ジョブは
 *          存在しない。そのため未初期化チェックのみを行う薄い実装とする
 *          （[SWS_SecOC_00172] 「未初期化なら何もせず戻る」は満たす）。
 *
 * \pre        SecOC_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_SecOC_00171}
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void SecOC_MainFunctionRx(void);

/**
 * \brief   SecOC モジュールのバージョン情報を取得する。
 *
 * \details SecOC_Init と並び、未初期化時でも SECOC_E_UNINIT を報告しない
 *          例外 API（他 BSW モジュールと共通の慣例）のため、初期化状態は
 *          確認せず NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void SecOC_GetVersionInfo(Std_VersionInfoType* versioninfo);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* SECOC_H */

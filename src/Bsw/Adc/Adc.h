/**
 * \file    Adc.h
 * \brief   ADC ドライバ 公開インタフェース (AUTOSAR SWS_ADC 準拠)
 * \details MCAL ADC モジュールの公開 API を定義する。
 *          呼び出し元（IoHwAb）は本ヘッダのみをインクルードし、
 *          Adc_Hw.h / Arduino API を直接参照しない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ADC_H
#define ADC_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "Adc_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/**
 * \brief   Adc_Init() の設定引数型（不透明型）。
 *
 * \details SWS_Adc_00365 は post-build 設定データへのポインタを要求する。
 *          本プロジェクトは単一チャネル・即時読み取りの簡略実装で
 *          post-build バリアント切替を持たないため、中身を定義しない
 *          不透明型とし、ポインタとしてのみ扱う（`KeyM_ConfigType` と
 *          同じパターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct Adc_ConfigType_Tag Adc_ConfigType;

/** [SWS_Adc_00356] ADC チャネルグループの ID（Adc_Cfg.h の ADC_GROUP_*）。 */
typedef uint8 Adc_GroupType;

/** [SWS_Adc_00359] 変換結果の型（10-bit の ADC 生値を 16 ビットで保持する）。 */
typedef uint16 Adc_ValueGroupType;

/** [SWS_Adc_00360] グループの変換の状態。 */
typedef enum
{
    ADC_IDLE = 0,            /**< 変換を始めていない、または結果を読み出し済み */
    ADC_BUSY,                /**< 変換中 */
    ADC_COMPLETED,           /**< 一部の変換が完了（本プロジェクトは使わない） */
    ADC_STREAM_COMPLETED     /**< 変換が完了し、結果を読み出せる */
} Adc_StatusType;

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
 * \brief   ADC ドライバを初期化する。
 *
 * \details ハードウェアの初期化は不要（Adc_Hw_ReadChannel は Arduino analogRead() を
 *          都度呼ぶだけ）だが、ドライバの初期化済みフラグを立て、全グループを ADC_IDLE・
 *          結果バッファ未登録の状態にする。これ以外の API（Adc_SetupResultBuffer /
 *          Adc_StartGroupConversion / Adc_ReadGroup / Adc_GetGroupStatus）は、本関数の
 *          呼び出し前は ADC_E_UNINIT を報告する。本関数は再度呼ぶと、結果バッファの
 *          登録も消える（二重初期化の ADC_E_ALREADY_INITIALIZED は検出しない）。
 *          シグネチャは [SWS_Adc_00365] に合わせている。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                        持たないため）。
 *
 * \AUTOSARReq     {SWS_Adc_00365}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Adc_Init(const Adc_ConfigType* ConfigPtr);

/**
 * \brief   グループの結果バッファを登録する（変換を始める前に必要）。
 *
 * \details [SWS_Adc_00420]/[SWS_Adc_00421]: 登録後でなければ、そのグループの変換は始められない
 *          （Adc_StartGroupConversion() が ADC_E_BUFFER_UNINIT を報告する）。
 *          グループが ADC_IDLE でないときは、実行時エラー ADC_E_BUSY を報告して何もしない
 *          （[SWS_Adc_00433]）。
 *
 * \note    本プロジェクトは 1 グループ・1 チャネル・ストリーミングなしのため、変換結果は Adc.c 内部の
 *          領域に保持し、Adc_ReadGroup() が呼び出し元の DataBufferPtr へコピーして返す。
 *          ここで登録したポインタは、登録済みかどうかの確認だけに使う（引数が const のため、
 *          仕様どおりドライバから書き込まない）。
 *
 * \param[in]  Group          対象の ADC チャネルグループ（ADC_GROUP_*）。
 * \param[in]  DataBufferPtr  結果バッファの先頭アドレス。NULL 禁止（[SWS_Adc_00457]）。
 *
 * \retval  E_OK      登録した。
 * \retval  E_NOT_OK  未初期化・グループが範囲外・ポインタが NULL・グループが ADC_IDLE でない。
 *
 * \AUTOSARReq     {SWS_Adc_00419, SWS_Adc_00420, SWS_Adc_00421, SWS_Adc_00423, SWS_Adc_00433, SWS_Adc_00457}
 * \ServiceID      {0x0c}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Adc_SetupResultBuffer(Adc_GroupType Group, const Adc_ValueGroupType* DataBufferPtr);

/**
 * \brief   ADC ドライバを未初期化の状態に戻す。
 *
 * \details [SWS_Adc_00366]。どれかのグループが ADC_IDLE でない（変換中、または変換が完了して結果を
 *          読み出す前）ときは、実行時エラー ADC_E_BUSY（[SWS_Adc_00112]）を報告して、何もしない。
 *          全グループが ADC_IDLE のときは、結果バッファの登録を消し、初期化済みフラグを下ろす。
 *          以降、Adc_Init() を呼ぶまで、他の API は ADC_E_UNINIT を報告する。
 *
 * \AUTOSARReq     {SWS_Adc_00366, SWS_Adc_00112}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Adc_DeInit(void);

/**
 * \brief   グループの変換を開始する。
 *
 * \details [SWS_Adc_00061]: 要求されたグループの全チャネルの変換を開始する。本プロジェクトのグループは、
 *          ソフトウェアトリガ・ワンショット・単一アクセスで、Adc_Hw_ReadChannel()（Arduino の
 *          analogRead()）の呼び出しが同期的に完了するため、戻ったときには状態が
 *          ADC_STREAM_COMPLETED になっている。
 *          エラー時は、何もせず戻る（結果は返さない）。
 *          ・未初期化: ADC_E_UNINIT（[SWS_Adc_00295]）
 *          ・グループが範囲外: ADC_E_PARAM_GROUP（[SWS_Adc_00125]）
 *          ・結果バッファが未登録: ADC_E_BUFFER_UNINIT（[SWS_Adc_00424]）
 *          ・グループが ADC_IDLE でない: 実行時エラー ADC_E_BUSY（[SWS_Adc_00346]）
 *
 * \param[in]  Group  変換を開始するグループ（ADC_GROUP_*）。
 *
 * \AUTOSARReq     {SWS_Adc_00367, SWS_Adc_00061, SWS_Adc_00125, SWS_Adc_00295, SWS_Adc_00346, SWS_Adc_00424}
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void Adc_StartGroupConversion(Adc_GroupType Group);

/**
 * \brief   グループの直近の変換結果を読み出す。
 *
 * \details [SWS_Adc_00075]: 直近に完了した変換の結果を、DataBufferPtr へ書き込む。
 *          グループが ADC_STREAM_COMPLETED のとき、読み出しで ADC_IDLE へ戻る
 *          （ソフトウェアトリガのワンショットで、変換が自動的に止まるグループ、[SWS_Adc_00330]）。
 *          グループが ADC_IDLE（変換を始めていない、または読み出し済み）のときは、
 *          実行時エラー ADC_E_IDLE（[SWS_Adc_00388]）を報告して E_NOT_OK を返す。
 *          未初期化は ADC_E_UNINIT、グループが範囲外は ADC_E_PARAM_GROUP（[SWS_Adc_00152]）、
 *          ポインタが NULL は ADC_E_PARAM_POINTER を報告する。
 *
 * \param[in]  Group          読み出すグループ（ADC_GROUP_*）。
 * \param[out] DataBufferPtr  変換結果（ADC 生値 0〜1023）の格納先。NULL 禁止。
 *
 * \retval  E_OK      結果を書き込んだ。
 * \retval  E_NOT_OK  結果がない、またはエラー。
 *
 * \AUTOSARReq     {SWS_Adc_00369, SWS_Adc_00075, SWS_Adc_00152, SWS_Adc_00296, SWS_Adc_00330, SWS_Adc_00388}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType* DataBufferPtr);

/**
 * \brief   グループの変換の状態を返す。
 *
 * \details [SWS_Adc_00220]/[SWS_Adc_00221]: 変換を始める前、または結果を読み出した後は ADC_IDLE、
 *          変換が完了して結果を読み出す前は ADC_STREAM_COMPLETED を返す。
 *          未初期化（ADC_E_UNINIT）またはグループが範囲外（ADC_E_PARAM_GROUP）のときは、
 *          エラーを報告して ADC_IDLE を返す。
 *
 * \param[in]  Group  状態を知りたいグループ（ADC_GROUP_*）。
 *
 * \return  グループの変換の状態（Adc_StatusType）。
 *
 * \AUTOSARReq     {SWS_Adc_00374, SWS_Adc_00220, SWS_Adc_00221}
 * \ServiceID      {0x09}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group);

/**
 * \brief   ADC ドライバのバージョン情報を取得する。
 *
 * \details 初期化状態チェックは行わず（Adc_Init() の前でも呼べる）、NULL ポインタ
 *          チェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Adc_GetVersionInfo(Std_VersionInfoType* versioninfo);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ADC_H */

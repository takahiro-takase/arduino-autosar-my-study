/**
 * \file    E2E.h
 * \brief   E2E ライブラリ共通公開インタフェース (AUTOSAR SWS_E2ELibrary 準拠)
 * \details E2E_P01/E2E_P05 のどちらのプロファイルにも属さない、ライブラリ
 *          全体で1つだけ存在する API（[SWS_E2E_00032]）をここに置く。
 *          プロファイル別の Protect/Check API は E2E_P01.h/E2E_P05.h 参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2E_H
#define E2E_H

#include "Std_Types.h"
#include "E2E_Cfg.h"
#include "E2E_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   E2E ライブラリのバージョン情報を取得する。
 *
 * \details E2E_P01/E2E_P05 のどちらか一方ではなく、ライブラリ全体として
 *          1つだけ存在する（[SWS_E2E_00032]）。[SWS_E2E_00216] によりライブラリ
 *          は DET/DEM を一切呼んではならないため、NULL の場合も
 *          Det_ReportError() は呼ばず、何もせず戻る（サイレントガード）。
 *
 * \param[out]  VersionInfo  バージョン情報の格納先。NULL の場合は何もしない。
 *
 * \AUTOSARReq     {SWS_E2E_00032}
 * \ServiceID      {0x14}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2E_GetVersionInfo(Std_VersionInfoType* VersionInfo);

/**
 * \brief   E2E ステートマシンを初期化する（[SWS_E2E_00353]）。
 *
 * \details `StatePtr->ProfileStatusWindow` は呼び出し元が事前に
 *          `ConfigPtr->WindowSize` バイト分の配列を割り当て、ポインタを
 *          設定しておくこと（本関数はポインタ自体は変更しない）。
 *          [SWS_E2E_00370]: `StatePtr`/`ConfigPtr` が NULL の場合は何もせず
 *          `E2E_E_INPUTERR_NULL` を返す。それ以外の場合、
 *          `ProfileStatusWindow[]` を全て `E2E_P_NOTAVAILABLE` で初期化し、
 *          `WindowTopIndex`/`OkCount`/`ErrorCount` を 0、`SMState` を
 *          `E2E_SM_NODATA` に設定する。
 *
 * \param[out]  StatePtr   初期化するステートマシン状態。NULL 禁止。
 * \param[in]   ConfigPtr  ステートマシン設定。NULL 禁止。
 *
 * \retval  E2E_E_OK             正常完了。
 * \retval  E2E_E_INPUTERR_NULL  StatePtr または ConfigPtr が NULL。
 *
 * \note    [SWS_E2E_00216] によりライブラリは DET/DEM を一切呼んではならない
 *          ため、NULL の場合も Det_ReportError() は呼ばない。
 *
 * \AUTOSARReq     {SWS_E2E_00353, SWS_E2E_00370, SWS_E2E_00467}
 * \ServiceID      {0x31}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType E2E_SMCheckInit(E2E_SMCheckStateType* StatePtr, const E2E_SMConfigType* ConfigPtr);

/**
 * \brief   E2E ステートマシンを1サイクル分進める（[SWS_E2E_00340]）。
 *
 * \details `E2E_PxxMapStatusToSM()`（プロファイル別、[SWS_E2E_00382]/
 *          [SWS_E2E_00452]）が変換したプロファイル非依存の判定結果
 *          `ProfileStatus` を受け取り、直近 `ConfigPtr->WindowSize` 回分の
 *          判定結果に基づいて通信路全体の健全性
 *          （`E2E_SM_NODATA`/`INIT`/`VALID`/`INVALID`）を判定する。
 *          単発フレームの成否ではなく、複数フレームにわたる履歴で判断する
 *          ことで、単発の CRC 化け等による瞬間的な誤検出を吸収する
 *          （[SWS_E2E_00345]/[SWS_E2E_00466]、状態遷移の詳細は Doxygen 内部
 *          コメント参照）。
 *
 *          [SWS_E2E_00371]: `StatePtr`/`ConfigPtr` が NULL の場合は何もせず
 *          `E2E_E_INPUTERR_NULL` を返す。`StatePtr->SMState` が
 *          `E2E_SM_DEINIT`（`E2E_SMCheckInit()` 未呼び出し）の場合は
 *          `E2E_E_WRONGSTATE` を返す。
 *
 * \param[in]     ProfileStatus  1サイクル分のプロファイル非依存判定結果
 *                               （`E2E_PxxMapStatusToSM()` の戻り値）。
 * \param[in]     ConfigPtr      ステートマシン設定。NULL 禁止。
 * \param[in,out] StatePtr       ステートマシン状態。NULL 禁止。
 *                               `E2E_SMCheckInit()` 済みであること。
 *
 * \retval  E2E_E_OK             正常完了（判定結果は `StatePtr->SMState` 参照）。
 * \retval  E2E_E_INPUTERR_NULL  StatePtr または ConfigPtr が NULL。
 * \retval  E2E_E_WRONGSTATE     StatePtr->SMState が E2E_SM_DEINIT のまま。
 *
 * \note    [SWS_E2E_00216] によりライブラリは DET/DEM を一切呼んではならない
 *          ため、上記いずれの場合も Det_ReportError() は呼ばない。
 *
 * \warning [SWS_E2E_00343] の値定義は `E2E_SM_VALID=0x00`・`E2E_SM_DEINIT=0x01`
 *          であるため、`StatePtr` をゼロ初期化しただけでは `E2E_SM_DEINIT`
 *          にはならず、誤って `E2E_SM_VALID` 扱いになってしまう
 *          （`E2E_E_WRONGSTATE` によるガードが効かず、初期化されていない
 *          `ProfileStatusWindow` へアクセスしうる）。呼び出し元は必ず
 *          `E2E_SMCheckInit()` を明示的に呼んでから本関数を使うこと。
 *
 * \AUTOSARReq     {SWS_E2E_00340, SWS_E2E_00371, SWS_E2E_00345, SWS_E2E_00466}
 * \ServiceID      {0x30}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType E2E_SMCheck(E2E_PCheckStatusType ProfileStatus, const E2E_SMConfigType* ConfigPtr,
                            E2E_SMCheckStateType* StatePtr);

#ifdef __cplusplus
}
#endif

#endif /* E2E_H */

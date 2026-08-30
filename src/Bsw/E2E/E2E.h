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

#ifdef __cplusplus
}
#endif

#endif /* E2E_H */

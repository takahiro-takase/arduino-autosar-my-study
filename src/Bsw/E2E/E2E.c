/**
 * \file    E2E.c
 * \brief   E2E ライブラリ共通実装 (AUTOSAR SWS_E2ELibrary 準拠)
 * \details E2E.h 冒頭のコメント参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "E2E.h"
#include "Det.h"

#define TAG "E2E"

void E2E_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    DET_LOGT(TAG, "called");
    if (VersionInfo == NULL)
    {
        /* [SWS_E2E_00216]: ライブラリは DET/DEM を呼んではならないため、
         * Det_ReportError() は呼ばずサイレントに戻る（E2E.h 参照）。 */
        return;
    }

    VersionInfo->vendorID         = E2E_VENDOR_ID;
    VersionInfo->moduleID         = E2E_MODULE_ID;
    VersionInfo->sw_major_version = E2E_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = E2E_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = E2E_SW_PATCH_VERSION;
}

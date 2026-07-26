/**
 * \file    Dio.c
 * \brief   デジタル入出力 MCAL 実装 (AUTOSAR SWS_Dio 準拠)
 * \details AUTOSAR Dio モジュールの実装。Arduino 依存コードを持たない純粋 C ファイル。
 *          ハードウェア操作は Dio_Hw.cpp（Arduino GPIO ラッパー）へ委譲する。
 *          上位層は本ファイルの存在を知らず、Dio.h の API のみを使用する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Dio.h"
#include "Dio_Hw.h"
#include "Det.h"

/**
 * \brief   指定チャネルへ出力レベルを書き込む。
 *
 * \param[in]  channelId  書き込み先チャネル ID (Arduino ピン番号)。
 * \param[in]  level      出力レベル (DIO_HIGH / DIO_LOW)。
 *
 * \pre        Dio_InitChannel() で対象チャネルを出力モードに設定済みであること。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level)
{
    Dio_Hw_WriteChannel(channelId, level);
}

/**
 * \brief   指定チャネルの入力レベルを読み取る。
 *
 * \param[in]  channelId  読み取り元チャネル ID (Arduino ピン番号)。
 *
 * \return  DIO_HIGH または DIO_LOW。
 *
 * \pre        Port_Init() で対象チャネルを入力モードに設定済みであること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType channelId)
{
    return Dio_Hw_ReadChannel(channelId);
}

void Dio_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    if (VersionInfo == NULL)
    {
        Det_ReportError(DIO_MODULE_ID, 0U, DIO_API_ID_GET_VERSION_INFO, DIO_E_PARAM_POINTER);
        return;
    }

    VersionInfo->vendorID         = DIO_VENDOR_ID;
    VersionInfo->moduleID         = DIO_MODULE_ID;
    VersionInfo->sw_major_version = DIO_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = DIO_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = DIO_SW_PATCH_VERSION;
}

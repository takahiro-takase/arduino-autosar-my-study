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

#define TAG "Dio"

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
    DET_LOGT(TAG, "called");
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
    DET_LOGT(TAG, "called");
    return Dio_Hw_ReadChannel(channelId);
}

/**
 * \brief   指定チャネルの出力レベルを反転し、反転後のレベルを返す。
 *
 * \details [SWS_Dio_00191] の出力チャネル向け挙動のみを実装する
 *          （Dio.h 冒頭のコメント参照）。
 *
 * \param[in]  channelId  対象チャネル ID (Arduino ピン番号)。
 *
 * \return  反転後の出力レベル (DIO_HIGH / DIO_LOW)。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType channelId)
{
    DET_LOGT(TAG, "called");
    Dio_LevelType level = (Dio_Hw_ReadChannel(channelId) == DIO_HIGH) ? DIO_LOW : DIO_HIGH;
    Dio_Hw_WriteChannel(channelId, level);
    return level;
}

/** ポート1個分の設定（チャネル配列の先頭とその幅）。 */
typedef struct
{
    const Dio_ChannelType* channels;
    uint8                  count;
} Dio_PortConfigType;

/** DIO_PORT_LED_GROUP を構成するチャネル（配列インデックス = bit位置、LSBが先頭）。
 *  Dio_Cfg.h の DIO_PORT_LED_GROUP 設定コメント参照。 */
static const Dio_ChannelType Dio_PortLedGroupChannels[] =
{
    DIO_CHANNEL_LED_RUNNING,  /* bit0 */
    DIO_CHANNEL_LED_FAULT,    /* bit1 */
    DIO_CHANNEL_LED_WARNING   /* bit2 */
};

/** Dio_PortType の値をそのまま添字とするポート設定テーブル（Port.c の
 *  Port_PinConfig[PORT_PIN_COUNT] と同じ設計）。要素数が Dio_Cfg.h の
 *  DIO_PORT_COUNT と食い違えば初期化子の過不足でコンパイルエラーになる。
 *  ポートを追加する際は Dio_Cfg.h の DIO_PORT_ 定数と DIO_PORT_COUNT の
 *  両方をここと同時に更新すること。 */
static const Dio_PortConfigType Dio_PortConfig[DIO_PORT_COUNT] =
{
    /* DIO_PORT_LED_GROUP */
    { Dio_PortLedGroupChannels, (uint8)(sizeof(Dio_PortLedGroupChannels) / sizeof(Dio_PortLedGroupChannels[0])) }
};

const Dio_ChannelGroupType Dio_ChannelGroupRunFault =
{
    DIO_CHANNELGROUP_RUN_FAULT_PORT,
    DIO_CHANNELGROUP_RUN_FAULT_MASK,
    DIO_CHANNELGROUP_RUN_FAULT_OFFSET
};

/**
 * \brief   PortId に対応するチャネル配列とその幅を取得する。
 * \param[out] widthOut  チャネル数の格納先（PortId が範囲外の場合は未変更）。
 * \return  チャネル配列の先頭（PortId が範囲外の場合は NULL）。
 */
static const Dio_ChannelType* Dio_GetPortChannels(Dio_PortType PortId, uint8* widthOut)
{
    if (PortId >= DIO_PORT_COUNT)
    {
        return NULL;
    }

    *widthOut = Dio_PortConfig[PortId].count;
    return Dio_PortConfig[PortId].channels;
}

/** channels[0..width) を bit0 起点で読み取り、1つの Dio_PortLevelType へ合成する。 */
static Dio_PortLevelType Dio_ReadPortLevel(const Dio_ChannelType* channels, uint8 width)
{
    Dio_PortLevelType level = 0U;
    for (uint8 i = 0U; i < width; i++)
    {
        if (Dio_Hw_ReadChannel(channels[i]) == DIO_HIGH)
        {
            level |= (Dio_PortLevelType)(1U << i);
        }
    }
    return level;
}

/** level の bit0 起点の各ビットを channels[0..width) へ順に書き込む。 */
static void Dio_WritePortLevel(const Dio_ChannelType* channels, uint8 width, Dio_PortLevelType level)
{
    for (uint8 i = 0U; i < width; i++)
    {
        Dio_Hw_WriteChannel(channels[i], ((level >> i) & 0x01U) ? DIO_HIGH : DIO_LOW);
    }
}

/**
 * \brief   PortId を解決し、失敗時は DET へ DIO_E_PARAM_INVALID_PORT_ID を報告する。
 * \details Dio_ReadPort/Dio_WritePort で共通の「解決失敗時は DET 報告して
 *          抜ける」処理をまとめる（/simplify reuse指摘: 4関数で同じ5行が
 *          コピペされていた）。
 * \param[out] widthOut  成功時のみポート幅を書き込む。
 * \return  成功時は対象ポートのチャネル配列、失敗時は NULL（この時点で
 *          DET へは報告済み）。
 */
static const Dio_ChannelType* Dio_ResolvePortOrReportDet(Dio_PortType PortId, uint8 ApiId, uint8* widthOut)
{
    const Dio_ChannelType* channels = Dio_GetPortChannels(PortId, widthOut);
    if (channels == NULL)
    {
        Det_ReportError(DIO_MODULE_ID, 0U, ApiId, DIO_E_PARAM_INVALID_PORT_ID);
    }
    return channels;
}

/**
 * \brief   group の port を解決し、(mask << offset) がポート幅に収まっているか検証する。
 * \details offset だけでなく mask の上位ビットもポート幅を超えないことを
 *          確認する（/code-review 指摘: offset 単体の比較では mask が
 *          ポート幅をまたぐ不正なグループを検出できなかった）。
 * \param[out] widthOut        検証成功時のみポート幅を書き込む。
 * \param[out] shiftedMaskOut  検証成功時のみ (mask << offset) を書き込む
 *                             （呼び出し元が書き込みマスクとして再利用できる
 *                             よう、検証時に導出した値をそのまま渡す。
 *                             不要な呼び出し元は NULL を渡してよい）。
 * \return  検証成功時は対象ポートのチャネル配列、失敗時は NULL。
 */
static const Dio_ChannelType* Dio_ResolveGroupChannels(const Dio_ChannelGroupType* group, uint8* widthOut,
                                                        Dio_PortLevelType* shiftedMaskOut)
{
    uint8 width = 0U;
    const Dio_ChannelType* channels = Dio_GetPortChannels(group->port, &width);
    if (channels == NULL)
    {
        return NULL;
    }

    uint16 shiftedMask = (uint16)((uint16)group->mask << group->offset);
    if ((shiftedMask >> width) != 0U)
    {
        return NULL;
    }

    *widthOut = width;
    if (shiftedMaskOut != NULL)
    {
        *shiftedMaskOut = (Dio_PortLevelType)shiftedMask;
    }
    return channels;
}

/**
 * \brief   ChannelGroupIdPtr を解決し、失敗時は DET へ報告する。
 * \details Dio_ReadChannelGroup/Dio_WriteChannelGroup で共通の
 *          「NULL チェック→グループ解決→失敗時 DET 報告」処理をまとめる
 *          （/simplify reuse指摘）。
 * \param[out] widthOut        成功時のみポート幅を書き込む。
 * \param[out] shiftedMaskOut  Dio_ResolveGroupChannels() 参照。不要なら NULL。
 * \return  成功時は対象ポートのチャネル配列、失敗時は NULL（この時点で
 *          DET へは報告済み）。
 */
static const Dio_ChannelType* Dio_ResolveGroupOrReportDet(const Dio_ChannelGroupType* group, uint8 ApiId,
                                                           uint8* widthOut, Dio_PortLevelType* shiftedMaskOut)
{
    if (group == NULL)
    {
        Det_ReportError(DIO_MODULE_ID, 0U, ApiId, DIO_E_PARAM_POINTER);
        return NULL;
    }

    const Dio_ChannelType* channels = Dio_ResolveGroupChannels(group, widthOut, shiftedMaskOut);
    if (channels == NULL)
    {
        Det_ReportError(DIO_MODULE_ID, 0U, ApiId, DIO_E_PARAM_INVALID_GROUP);
    }
    return channels;
}

Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId)
{
    DET_LOGT(TAG, "called");
    uint8 width = 0U;
    const Dio_ChannelType* channels = Dio_ResolvePortOrReportDet(PortId, DIO_API_ID_READ_PORT, &width);
    if (channels == NULL)
    {
        return 0U;
    }

    return Dio_ReadPortLevel(channels, width);
}

void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level)
{
    DET_LOGT(TAG, "called");
    uint8 width = 0U;
    const Dio_ChannelType* channels = Dio_ResolvePortOrReportDet(PortId, DIO_API_ID_WRITE_PORT, &width);
    if (channels == NULL)
    {
        return;
    }

    Dio_WritePortLevel(channels, width, Level);
}

Dio_PortLevelType Dio_ReadChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr)
{
    DET_LOGT(TAG, "called");
    uint8 width = 0U;
    const Dio_ChannelType* channels =
        Dio_ResolveGroupOrReportDet(ChannelGroupIdPtr, DIO_API_ID_READ_CHANNEL_GROUP, &width, NULL);
    if (channels == NULL)
    {
        return 0U;
    }

    Dio_PortLevelType portLevel = Dio_ReadPortLevel(channels, width);
    return (Dio_PortLevelType)((portLevel >> ChannelGroupIdPtr->offset) & ChannelGroupIdPtr->mask);
}

void Dio_WriteChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr, Dio_PortLevelType Level)
{
    DET_LOGT(TAG, "called");
    uint8 width = 0U;
    Dio_PortLevelType shiftedMask = 0U;
    const Dio_ChannelType* channels =
        Dio_ResolveGroupOrReportDet(ChannelGroupIdPtr, DIO_API_ID_WRITE_CHANNEL_GROUP, &width, &shiftedMask);
    if (channels == NULL)
    {
        return;
    }

    /* 読み取り→マスク合成→書き込み（非アトミック。Dio_FlipChannel と同じ制約）。
     * mask外のチャネルは元の値のまま保持する（[SWS_Dio_00040]）。 */
    Dio_PortLevelType current      = Dio_ReadPortLevel(channels, width);
    Dio_PortLevelType shiftedLevel = (Dio_PortLevelType)((Level & ChannelGroupIdPtr->mask) << ChannelGroupIdPtr->offset);
    Dio_PortLevelType newLevel     = (Dio_PortLevelType)((current & (Dio_PortLevelType)~shiftedMask) | shiftedLevel);

    Dio_WritePortLevel(channels, width, newLevel);
}

void Dio_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    DET_LOGT(TAG, "called");
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

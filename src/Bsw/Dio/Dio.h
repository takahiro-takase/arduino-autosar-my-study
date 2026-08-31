/**
 * \file    Dio.h
 * \brief   デジタル入出力 公開インタフェース (AUTOSAR SWS_Dio 準拠)
 * \details MCAL 層のデジタル I/O 抽象化 API を提供する。
 *          ピン値の読み書き（DIO_HIGH / DIO_LOW）のみを担当する。
 *          ピン方向（INPUT / OUTPUT）の設定は Port モジュールの責務であり、
 *          Port_Init() が事前に完了していることを前提とする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DIO_H
#define DIO_H

#include "Std_Types.h"
#include "Dio_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** チャネル ID 型 (Arduino ピン番号に対応) */
typedef uint8 Dio_ChannelType;

/** チャネル出力レベル型 */
typedef uint8 Dio_LevelType;

/** ポート ID 型 [SWS_Dio_00183]。実体は Dio_Cfg.h の DIO_PORT_* 定数参照。 */
typedef uint8 Dio_PortType;

/** ポート値型 [SWS_Dio_00186]。本プロジェクトが定義するポートはいずれも
 *  8ch 以下のため uint8 で十分。 */
typedef uint8 Dio_PortLevelType;

/**
 * \brief   チャネルグループ定義型 [SWS_Dio_00184]。
 * \details port/mask/offset は Dio_Cfg.h の DIO_CHANNELGROUP_* 定数を使って
 *          呼び出し側で組み立てる（本プロジェクトは事前定義済みの const
 *          インスタンスを持たない。Dio.h 冒頭の設定コメント参照）。
 */
typedef struct
{
    Dio_PortType port;    /**< グループが属するポート ID */
    uint8        mask;    /**< グループの位置を示すマスク（LSB 側に整列） */
    uint8        offset;  /**< ポート内でのグループの位置（LSBから数えたビット位置） */
} Dio_ChannelGroupType;

#define DIO_HIGH  1U  /**< 出力 HIGH (3.3V / 5V) */
#define DIO_LOW   0U  /**< 出力 LOW  (GND) */

/** ChannelGroup 例の事前定義済みインスタンス（Dio_Cfg.h の
 *  DIO_CHANNELGROUP_RUN_FAULT_* から Dio.c が構築、実 AUTOSAR の
 *  コンフィグツールが生成する `DioConf_DioChannelGroup_*` に相当）。 */
extern const Dio_ChannelGroupType Dio_ChannelGroupRunFault;

/**
 * \brief   指定チャネルへ出力レベルを書き込む。
 *
 * \param[in]  channelId  書き込み先チャネル ID (Arduino ピン番号)。
 * \param[in]  level      出力レベル (DIO_HIGH / DIO_LOW)。
 *
 * \pre        Port_Init() で対象チャネルを出力モードに設定済みであること。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level);

/**
 * \brief   指定チャネルの入力レベルを読み取る。
 *
 * \param[in]  channelId  読み取り元チャネル ID (Arduino ピン番号)。
 *
 * \return  DIO_HIGH または DIO_LOW。
 *
 * \pre        Port_Init() で対象チャネルを入力モード (PORT_PIN_IN / PORT_PIN_IN_PULLUP)
 *             に設定済みであること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType channelId);

/**
 * \brief   指定チャネルの出力レベルを反転し、反転後のレベルを返す。
 *
 * \details 本プロジェクトの Dio はチャネルの入出力方向を追跡していない
 *          （方向設定は Port モジュールの責務であり、Dio 側に問い合わせ手段が
 *          ない）ため、出力チャネル向けの挙動（[SWS_Dio_00191]:
 *          読み取り→反転→書き込みし、反転後の値を返す）のみを実装する。
 *          入力チャネルへの適用は想定しない（[SWS_Dio_00192]/[SWS_Dio_00193]
 *          が規定する「入力チャネルでは物理出力に影響を与えない」動作は
 *          本実装では保証されない）。
 *
 * \note       読み取り→反転→書き込みは非アトミックである。SWS 上は
 *             Reentrant だが、同一チャネルへ割り込みコンテキスト等から
 *             同時に呼ばれた場合、片方の反転が失われる可能性がある
 *             （本プロジェクトは現状 Dio チャネルを割り込みから操作しない
 *             ため実害はないが、将来そのような呼び出し元を追加する場合は
 *             Com.c/Can.c 等が使う SchM_Enter/Exit による排他が必要）。
 *
 * \param[in]  channelId  対象チャネル ID (Arduino ピン番号)。
 *
 * \return  反転後の出力レベル (DIO_HIGH / DIO_LOW)。
 *
 * \pre        Port_Init() で対象チャネルを出力モードに設定済みであること。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType channelId);

/**
 * \brief   指定ポートの全チャネルのレベルを読み取る。
 *
 * \details 本プロジェクトの「ポート」は実 MCU のポートレジスタではなく、
 *          Dio_Cfg.h の DIO_PORT_* で定義した論理チャネル群である。
 *          Dio.h 冒頭の設定コメント参照。
 *
 * \param[in]  PortId  読み取り対象のポート ID (DIO_PORT_*)。
 *
 * \return  ポート内全チャネルのレベル（bit0 がグループ先頭チャネル）。
 *          PortId が未定義の場合は DIO_E_PARAM_INVALID_PORT_ID を報告し 0 を返す。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);

/**
 * \brief   指定ポートの全チャネルへレベルを書き込む。
 *
 * \details 本プロジェクトの Dio はチャネルの入出力方向を追跡していないため、
 *          [SWS_Dio_00035]/[SWS_Dio_00108] が規定する「入力チャネルは変更
 *          しない」動作は保証されない。ポート構成は出力チャネルのみで
 *          あることが前提（Dio_Cfg.h 冒頭の設定コメント参照）。
 *
 * \param[in]  PortId  書き込み先のポート ID (DIO_PORT_*)。
 * \param[in]  Level   書き込む値（bit0 がグループ先頭チャネル）。
 *
 * \pre        PortId が未定義の場合は DIO_E_PARAM_INVALID_PORT_ID を報告し何もしない。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);

/**
 * \brief   ポート内の隣接する一部チャネル（チャネルグループ）のレベルを読み取る。
 *
 * \details [SWS_Dio_00092]/[SWS_Dio_00093] の通り、マスクした上で LSB に
 *          揃えてシフトした値を返す。
 *
 * \param[in]  ChannelGroupIdPtr  読み取り対象のチャネルグループ定義。NULL 禁止。
 *
 * \return  マスク・シフト済みのレベル値。NULL の場合は DIO_E_PARAM_POINTER、
 *          port/offset が不正な場合は DIO_E_PARAM_INVALID_GROUP を報告し 0 を返す。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dio_PortLevelType Dio_ReadChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr);

/**
 * \brief   ポート内の隣接する一部チャネル（チャネルグループ）へレベルを書き込む。
 *
 * \details [SWS_Dio_00040] の通り、ポート内の残り (mask外) のチャネルは
 *          変更しない（読み取り→マスク合成→書き込みで実現。非アトミック）。
 *          [SWS_Dio_00090]/[SWS_Dio_00091] の通り、Level は LSB 基準の値として
 *          マスク・シフトしてから対象ビット位置へ書き込む。
 *
 * \param[in]  ChannelGroupIdPtr  書き込み対象のチャネルグループ定義。NULL 禁止。
 * \param[in]  Level              書き込む値（LSB 基準、mask でトリムされる）。
 *
 * \pre        NULL の場合は DIO_E_PARAM_POINTER、port/offset が不正な場合は
 *             DIO_E_PARAM_INVALID_GROUP を報告し何もしない。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_WriteChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr, Dio_PortLevelType Level);

/**
 * \brief   DIO ドライバのバージョン情報を取得する。
 *
 * \details 本プロジェクトの Dio に初期化状態の概念はないため（実 SWS_Dio にも
 *          Dio_Init は存在しない）、NULL ポインタチェックのみ行う。
 *
 * \param[out]  VersionInfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x12}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_GetVersionInfo(Std_VersionInfoType* VersionInfo);

#ifdef __cplusplus
}
#endif

#endif /* DIO_H */

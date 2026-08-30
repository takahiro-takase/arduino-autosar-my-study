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

#define DIO_HIGH  1U  /**< 出力 HIGH (3.3V / 5V) */
#define DIO_LOW   0U  /**< 出力 LOW  (GND) */

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

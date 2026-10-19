/**
 * \file    StbM.h
 * \brief   同期時間ベースマネージャ 公開インタフェース (AUTOSAR SWS_StbM 準拠、最小版)
 * \details 時刻（全域時間）を保持し、上位・下位モジュールへ提供する。時刻の同期方法
 *          （CAN なら CanTSyn）を知らない。
 *
 *          時刻の考え方:
 *            仮想ローカル時間 = Os カウンタ（ECU の稼働時間）を ns に換算した値（分解能 1ms）。
 *            全域時間 = 仮想ローカル時間 + 時間ベースごとのオフセット。
 *            時刻を設定する（StbM_SetGlobalTime / StbM_BusSetGlobalTime）と、そのときの全域時間が
 *            与えた値になるようにオフセットを更新する。
 *
 *          使い方:
 *            1. EcuM_Init 内で StbM_Init() を呼ぶ（CanTSyn_Init() より前）。
 *            2. Os スケジューラが STBM_MAIN_FUNCTION_PERIOD_MS ごとに StbM_MainFunction() を呼ぶ
 *               （同期喪失のタイムアウトの監視）。
 *            3. 時間マスターは StbM_GetCurrentTime() で時刻を読み、時間スレーブ側のモジュール
 *               （CanTSyn）は受信した時刻を StbM_BusSetGlobalTime() で渡す。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef STBM_H
#define STBM_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "StbM_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** StbM_TimeBaseStatusType のビット（[SWS_StbM_00239]）。 */
#define STBM_TIMEBASE_STATUS_TIMEOUT          0x01U  /**< 同期メッセージの受信タイムアウト */
#define STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY  0x04U  /**< 時間ゲートウェイの下位ドメインに基づく時刻 */
#define STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE 0x08U  /**< 一度でも全域時間に同期した */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/** 同期時間ベースの ID（[SWS_StbM_00142]）。 */
typedef uint16 StbM_SynchronizedTimeBaseType;

/** 時間ベースの状態（ビットフィールド、[SWS_StbM_00239]）。 */
typedef uint8 StbM_TimeBaseStatusType;

/** ns のみの時刻（[SWS_StbM_00194]）。仮想ローカル時間の ns 部分（約 4.29 秒で循環）。 */
typedef uint32 StbM_TimeStampRawType;

/** 時刻（[SWS_StbM_00241]）。秒は 48bit（secondsHi が上位 16bit、seconds が下位 32bit）。 */
typedef struct
{
    StbM_TimeBaseStatusType timeBaseStatus;  /**< 時間ベースの状態 */
    uint32                  nanoseconds;     /**< 時刻のナノ秒部分（0〜999999999） */
    uint32                  seconds;         /**< 時刻の秒部分の下位 32bit */
    uint16                  secondsHi;       /**< 時刻の秒部分の上位 16bit */
} StbM_TimeStampType;

/** ユーザデータ（[SWS_StbM_00243]）。 */
typedef struct
{
    uint8 userDataLength;  /**< ユーザデータのバイト数 */
    uint8 userByte0;
    uint8 userByte1;
    uint8 userByte2;
} StbM_UserDataType;

/** 受信時の追加情報（[SWS_StbM_00384]）。 */
typedef struct
{
    uint32 pathDelay;  /**< 伝搬遅延 [ns] */
} StbM_MeasurementType;

/**
 * \brief   StbM_Init() の設定引数型（不透明型）。
 *
 * \details 本プロジェクトは post-build 設定を持たないため、中身を定義しない不透明型とし、
 *          ポインタとしてのみ扱う（`CanNm_ConfigType` と同じ簡略化パターン）。
 */
typedef struct StbM_ConfigType_Tag StbM_ConfigType;

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
 * \brief   StbM を初期化する。マスターの時間ベースは、全域時間 = 仮想ローカル時間（ECU の
 *          稼働時間）で同期済み（GLOBAL_TIME_BASE）になる。スレーブの時間ベースは未同期。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を持たないため）。
 *
 * \AUTOSARReq     {SWS_StbM_00052}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void StbM_Init(const StbM_ConfigType* ConfigPtr);

/**
 * \brief   StbM のバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL を渡すと STBM_E_PARAM_POINTER を
 *                           DET へ報告し、何も書かない。
 *
 * \AUTOSARReq     {SWS_StbM_00066}
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void StbM_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   時間ベースの現在の全域時間を取得する。
 *
 * \param[in]  timeBaseId  時間ベースの ID（STBM_TIME_BASE_COUNT 未満）。範囲外は STBM_E_PARAM。
 * \param[out] timeStamp   現在の時刻と、時間ベースの状態。NULL は STBM_E_PARAM_POINTER。
 * \param[out] userData    ユーザデータ。NULL は STBM_E_PARAM_POINTER。本実装はユーザデータを
 *                         保持しないため、長さ 0 を返す。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00195}
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetCurrentTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                   StbM_TimeStampType* timeStamp,
                                   StbM_UserDataType* userData);

/**
 * \brief   時間ベースの仮想ローカル時間の ns 部分を取得する（時間差の測定の起点に使う）。
 *
 * \param[in]  timeBaseId      時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[out] timeStampRawPtr 仮想ローカル時間の ns 部分（約 4.29 秒で循環）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00205}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetCurrentTimeRaw(StbM_SynchronizedTimeBaseType timeBaseId,
                                      StbM_TimeStampRawType* timeStampRawPtr);

/**
 * \brief   現在の仮想ローカル時間と、StbM_GetCurrentTimeRaw() で取った時刻との差 [ns] を取得する。
 *
 * \param[in]  timeBaseId        時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[in]  givenTimeStamp    差の基準にする時刻（StbM_GetCurrentTimeRaw() の値）。
 * \param[out] timeStampDiffPtr  現在 - 基準 [ns]（循環を考慮した符号なしの差）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00209}
 * \ServiceID      {0x0A}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetCurrentTimeDiff(StbM_SynchronizedTimeBaseType timeBaseId,
                                       StbM_TimeStampRawType givenTimeStamp,
                                       StbM_TimeStampRawType* timeStampDiffPtr);

/**
 * \brief   時間マスターが、時間ベースの全域時間を設定する。
 *
 * \details 設定した時点の全域時間が timeStamp になるようにオフセットを更新し、GLOBAL_TIME_BASE を
 *          立て、TIMEOUT を下ろす。
 *
 * \param[in]  timeBaseId  時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[in]  timeStamp   設定する時刻。NULL は STBM_E_PARAM_POINTER。
 * \param[in]  userData    ユーザデータ（本実装は保持しない。NULL でもよい）。
 * \return     E_OK=設定できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00213}
 * \ServiceID      {0x0B}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_SetGlobalTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                  const StbM_TimeStampType* timeStamp,
                                  const StbM_UserDataType* userData);

/**
 * \brief   バスから受信した全域時間を、時間ベースへ反映する（時間スレーブ側のモジュールが呼ぶ）。
 *
 * \details 受信した時刻に伝搬遅延 pathDelay を足した値が、いまの全域時間になるようにオフセットを
 *          更新し、GLOBAL_TIME_BASE を立て、TIMEOUT を下ろす。timeStamp->timeBaseStatus の
 *          SYNC_TO_GATEWAY は、時間ベースの状態へ引き継ぐ。
 *
 * \param[in]  timeBaseId     時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[in]  timeStampPtr   受信した時刻。NULL は STBM_E_PARAM_POINTER。
 * \param[in]  userDataPtr    ユーザデータ（本実装は保持しない。NULL でもよい）。
 * \param[in]  measureDataPtr 追加情報（伝搬遅延）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=反映できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00233}
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_BusSetGlobalTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                     const StbM_TimeStampType* timeStampPtr,
                                     const StbM_UserDataType* userDataPtr,
                                     const StbM_MeasurementType* measureDataPtr);

/**
 * \brief   時間ベースの状態を取得する。
 *
 * \param[in]  timeBaseId         時間ベースの ID。範囲外は STBM_E_PARAM。
 * \param[out] syncTimeBaseStatus 同期時間ベースの状態。NULL は STBM_E_PARAM_POINTER。
 * \param[out] offsetTimeBaseStatus オフセット時間ベースの状態（本実装はオフセット時間ベースを
 *                         持たないため、常に 0）。NULL は STBM_E_PARAM_POINTER。
 * \return     E_OK=取得できた。E_NOT_OK=引数の不正、または未初期化。
 *
 * \AUTOSARReq     {SWS_StbM_00263}
 * \ServiceID      {0x14}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType StbM_GetTimeBaseStatus(StbM_SynchronizedTimeBaseType timeBaseId,
                                      StbM_TimeBaseStatusType* syncTimeBaseStatus,
                                      StbM_TimeBaseStatusType* offsetTimeBaseStatus);

/**
 * \brief   同期喪失のタイムアウトを監視する。
 *
 * \details GLOBAL_TIME_BASE が立っていて、タイムアウトが設定された時間ベース
 *          （STBM_SYNC_LOSS_TIMEOUT_MS_*）が、最後の更新から設定時間以上更新されていなければ、
 *          TIMEOUT ビットを立てる（[SWS_StbM_00179]）。次の更新で下りる。
 *
 * \AUTOSARReq     {SWS_StbM_00057}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void StbM_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* STBM_H */

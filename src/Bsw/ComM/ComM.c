/**
 * \file    ComM.c
 * \brief   通信マネージャ 実装 (AUTOSAR SWS_ComM 準拠)
 * \details CAN 通信スタックの通信モードを状態機械で管理する。
 *          ユーザからの要求を調停し、チャネルを
 *          NO_COM / SILENT_COM / FULL_COM の 3 状態間で遷移させる。
 *
 *          状態遷移図:
 *
 *            ┌──────────────────────────────────────────────────┐
 *            │           COMM_NO_COMMUNICATION                  │
 *            │  (CAN バス停止: Can_SetControllerMode(CAN_T_STOP)) │
 *            └────────────┬──────────────────────┬─────────────┘
 *             FULL_COM 要求↓                    ↑NO_COM 要求
 *            ┌────────────▼──────────────────────▼─────────────┐
 *            │          COMM_FULL_COMMUNICATION                 │
 *            │  (CAN バス起動: Can_SetControllerMode(CAN_T_START))│
 *            └────────────┬──────────────────────┬─────────────┘
 *           SILENT_COM 要求↓                    ↑FULL_COM 要求
 *            ┌────────────▼──────────────────────▼─────────────┐
 *            │        COMM_SILENT_COMMUNICATION                 │
 *            │  (受信専用: Can_SetControllerMode(CAN_T_STOP))    │
 *            └──────────────────────────────────────────────────┘
 *
 *          本実装の簡略化:
 *            - 1 チャネル / 1 ユーザ固定（調停は不要）
 *            - NM（Network Manager）連携なし
 *            - バス スリープ / ウェイクアップ 未対応
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "ComM.h"
#include "Can.h"
#include "Det.h"

#define TAG "ComM"

/* チャネルごとの現在の通信モード */
static ComM_ModeType ComM_ChannelMode[COMM_CHANNEL_COUNT];

/* ユーザごとの要求モード（調停のために保持）*/
static ComM_ModeType ComM_UserRequest[COMM_USER_COUNT];

/**
 * \brief   ComM モジュールを初期化する。
 *
 * \details 全チャネルを COMM_NO_COMMUNICATION 状態に設定する。
 *          CAN バスは本関数では起動しない。
 *          ComM_RequestComMode() で COMM_FULL_COMMUNICATION を要求することで
 *          初めて CAN バスがアクティブになる。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Init(void)
{
    uint8 i;
    for (i = 0U; i < COMM_CHANNEL_COUNT; i++)
        ComM_ChannelMode[i] = COMM_NO_COMMUNICATION;
    for (i = 0U; i < COMM_USER_COUNT; i++)
        ComM_UserRequest[i] = COMM_NO_COMMUNICATION;
    DET_LOGI(TAG, "Init ch=%u", (unsigned)COMM_CHANNEL_COUNT);
}

/**
 * \brief   ユーザが通信モードを要求する。
 *
 * \details ユーザの要求を記録し、チャネルへの反映（調停）を行う。
 *          本実装は 1 ユーザ / 1 チャネルのため調停はパススルー。
 *          チャネルの状態遷移に応じて Can_SetControllerMode() を呼び出す。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode)
{
    if (User >= COMM_USER_COUNT)
        return E_NOT_OK;
    if (ComMode > COMM_FULL_COMMUNICATION)
        return E_NOT_OK;

    ComM_UserRequest[User] = ComMode;

    /* 本実装: ユーザ 0 → チャネル 0 の 1:1 マッピング */
    ComM_ModeType current = ComM_ChannelMode[0U];
    if (current == ComMode)
        return E_OK;

    switch (ComMode)
    {
        case COMM_FULL_COMMUNICATION:
            Can_SetControllerMode(0U, CAN_T_START);
            ComM_ChannelMode[0U] = COMM_FULL_COMMUNICATION;
            DET_LOGI(TAG, "ch0 NO_COM/SILENT->FULL_COM");
            break;

        case COMM_SILENT_COMMUNICATION:
            if (current == COMM_FULL_COMMUNICATION)
                Can_SetControllerMode(0U, CAN_T_STOP);
            ComM_ChannelMode[0U] = COMM_SILENT_COMMUNICATION;
            DET_LOGI(TAG, "ch0 ->SILENT_COM");
            break;

        case COMM_NO_COMMUNICATION:
            if (current == COMM_FULL_COMMUNICATION)
                Can_SetControllerMode(0U, CAN_T_STOP);
            ComM_ChannelMode[0U] = COMM_NO_COMMUNICATION;
            DET_LOGI(TAG, "ch0 ->NO_COM");
            break;

        default:
            return E_NOT_OK;
    }

    return E_OK;
}

/**
 * \brief   ユーザの現在の通信モードを取得する。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)
{
    if (User >= COMM_USER_COUNT || ComMode == NULL)
        return E_NOT_OK;
    /* ユーザ 0 → チャネル 0 の現在モードを返す */
    *ComMode = ComM_ChannelMode[0U];
    return E_OK;
}

/**
 * \brief   ComM 周期処理。
 *
 * \details 本実装はスタブ。NM 連携・バス スリープは未対応。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_MainFunction(void)
{
    /* NM / バス スリープ未対応のため NOP */
}

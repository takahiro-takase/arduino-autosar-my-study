/**
 * \file    CanSM.c
 * \brief   CAN ステートマネージャ 実装 (AUTOSAR SWS_CanSM 準拠)
 * \details CAN ネットワークの通信モード遷移と Bus-Off 回復シーケンスを管理する。
 *
 *          内部状態機械:
 *
 *            CANSM_STATE_NO_COM
 *              ↓ RequestComMode(FULL_COM) → Can_SetControllerMode(CAN_T_START)
 *            CANSM_STATE_FULL_COM  ←────────────────────────┐
 *              ↓ CanSM_ControllerBusOff()                    │ 回復成功
 *            CANSM_STATE_BUS_OFF                             │
 *              ↓ T_REC 経過 (MainFunction)                   │
 *              → Can_SetControllerMode(CAN_T_START) ─────────┘
 *              → 連続失敗 > MAX_RETRIES → 回復中止
 *
 *          Bus-Off 回復シーケンス（AUTOSAR T_BSM_BUSOFF_RECOVERY 準拠）:
 *            1. Bus-Off 検出 → コントローラ停止・タイマ起動
 *            2. T_REC ms 待機（CanSM_MainFunction が監視）
 *            3. コントローラ再起動 → FULL_COM に復帰 → Dem へ PASSED 報告
 *            4. 再度 Bus-Off が発生した場合は試行回数をカウント
 *            5. 最大試行回数超過で回復中止 → Dem へ FAILED 報告 (DEM_EVENT_CAN_BUSOFF)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "CanSM.h"
#include "Can.h"
#include "Dem.h"
#include "Det.h"

#define TAG "CanSM"

/* Arduino wiring.c（C リンケージ）で定義 */
extern unsigned long millis(void);

/* -----------------------------------------------------------------------
 * 内部型定義
 * ----------------------------------------------------------------------- */
typedef enum
{
    CANSM_STATE_NO_COM,      /* 通信停止 */
    CANSM_STATE_SILENT_COM,  /* 受信専用 */
    CANSM_STATE_FULL_COM,    /* 全二重通信（正常動作） */
    CANSM_STATE_BUS_OFF      /* Bus-Off 回復中 */
} CanSM_InternalStateType;

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */
static CanSM_InternalStateType CanSM_State;
static unsigned long           CanSM_BusOffTimerMs;   /* Bus-Off 検出時刻 */
static uint8                   CanSM_BusOffRetries;   /* 回復試行回数 */
static uint8                   CanSM_BusOffGaveUp;    /* 最大試行超過フラグ */

/**
 * \brief   CanSM モジュールを初期化する。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_Init(void)
{
    CanSM_State          = CANSM_STATE_NO_COM;
    CanSM_BusOffTimerMs  = 0UL;
    CanSM_BusOffRetries  = 0U;
    CanSM_BusOffGaveUp   = 0U;
    DET_LOGI(TAG, "Init");
}

/**
 * \brief   ネットワークの通信モード遷移を要求する。
 *
 * \details ComM から呼び出される。Bus-Off 回復中は E_NOT_OK を返す。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_RequestComMode(CanSM_NetworkHandleType network, ComM_ModeType mode)
{
    if (network >= CANSM_CHANNEL_COUNT)
        return E_NOT_OK;

    /* Bus-Off 回復中は上位からのモード変更を受け付けない */
    if (CanSM_State == CANSM_STATE_BUS_OFF)
    {
        DET_LOGW(TAG, "RequestComMode ignored: BusOff recovery in progress");
        return E_NOT_OK;
    }

    switch (mode)
    {
        case COMM_FULL_COMMUNICATION:
            Can_SetControllerMode(0U, CAN_T_START);
            CanSM_State         = CANSM_STATE_FULL_COM;
            CanSM_BusOffRetries = 0U;
            DET_LOGI(TAG, "->FULL_COM");
            /* 通信確立を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする */
            Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
            ComM_BusSMIndication(network, COMM_FULL_COMMUNICATION);
            break;

        case COMM_SILENT_COMMUNICATION:
            if (CanSM_State == CANSM_STATE_FULL_COM)
                Can_SetControllerMode(0U, CAN_T_STOP);
            CanSM_State = CANSM_STATE_SILENT_COM;
            DET_LOGI(TAG, "->SILENT_COM");
            ComM_BusSMIndication(network, COMM_SILENT_COMMUNICATION);
            break;

        case COMM_NO_COMMUNICATION:
            if (CanSM_State == CANSM_STATE_FULL_COM)
                Can_SetControllerMode(0U, CAN_T_STOP);
            CanSM_State = CANSM_STATE_NO_COM;
            DET_LOGI(TAG, "->NO_COM");
            ComM_BusSMIndication(network, COMM_NO_COMMUNICATION);
            break;

        default:
            return E_NOT_OK;
    }

    return E_OK;
}

/**
 * \brief   ネットワークの現在の通信モードを取得する。
 *
 * \details Bus-Off 状態は COMM_NO_COMMUNICATION として報告する。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_GetCurrentComMode(CanSM_NetworkHandleType network, ComM_ModeType* mode)
{
    if (network >= CANSM_CHANNEL_COUNT || mode == NULL)
        return E_NOT_OK;

    switch (CanSM_State)
    {
        case CANSM_STATE_FULL_COM:   *mode = COMM_FULL_COMMUNICATION;   break;
        case CANSM_STATE_SILENT_COM: *mode = COMM_SILENT_COMMUNICATION; break;
        default:                     *mode = COMM_NO_COMMUNICATION;     break;
    }
    return E_OK;
}

/**
 * \brief   Bus-Off 通知コールバック（CanIf → CanSM の通知経路）。
 *
 * \details CAN コントローラが Bus-Off 状態を検出したとき CanIf 経由で呼ばれる。
 *          コントローラを即座に停止し、T_REC タイマを起動する。
 *          CanSM_MainFunction が T_REC ms 後にコントローラの再起動を試みる。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerBusOff(uint8 ControllerId)
{
    (void)ControllerId;

    if (CanSM_State != CANSM_STATE_FULL_COM)
        return;

    Can_SetControllerMode(0U, CAN_T_STOP);

    CanSM_State         = CANSM_STATE_BUS_OFF;
    CanSM_BusOffTimerMs = millis();
    CanSM_BusOffGaveUp  = 0U;

    DET_LOGW(TAG, "BusOff detected! retry=%u/%u recovery in %ums",
             (unsigned)CanSM_BusOffRetries,
             (unsigned)CANSM_BUSOFF_MAX_RETRIES,
             (unsigned)CANSM_BUSOFF_RECOVERY_MS);
}

/**
 * \brief   CanSM 周期処理（Bus-Off 回復タイマ管理）。
 *
 * \details Bus-Off 状態のとき CANSM_BUSOFF_RECOVERY_MS 経過後に
 *          コントローラの再起動を試みる。再起動成功時は
 *          Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, PASSED) を報告する
 *          （CanSM_RequestComMode を経由しない自動復帰のため、ここで明示的に報告する）。
 *          再起動後に再度 Bus-Off が発生すると CanSM_ControllerBusOff() が
 *          呼ばれ、試行回数がインクリメントされる（リトライ回数は次回の
 *          CanSM_RequestComMode(FULL_COM) までリセットされない）。
 *          CANSM_BUSOFF_MAX_RETRIES を超えた場合は回復を中止し、
 *          Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, FAILED) を報告する。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_MainFunction(void)
{
    if (CanSM_State != CANSM_STATE_BUS_OFF)
        return;

    if (CanSM_BusOffGaveUp)
        return;

    if ((millis() - CanSM_BusOffTimerMs) < CANSM_BUSOFF_RECOVERY_MS)
        return;

    /* T_REC 経過: 回復試行 */
    CanSM_BusOffRetries++;

    if (CanSM_BusOffRetries > CANSM_BUSOFF_MAX_RETRIES)
    {
        DET_LOGE(TAG, "BusOff: max retries (%u) exceeded, recovery stopped",
                 (unsigned)CANSM_BUSOFF_MAX_RETRIES);
        CanSM_BusOffGaveUp = 1U;
        /* 回復断念を DTC として記録（FreezeFrame には断念時点の車両状態が残る） */
        Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
        /* 回復断念 → ComM に NO_COM を通知 → EcuM_ReleaseRUN → POST_RUN へ */
        ComM_BusSMIndication(0U, COMM_NO_COMMUNICATION);
        return;
    }

    DET_LOGI(TAG, "BusOff: restart attempt %u/%u",
             (unsigned)CanSM_BusOffRetries,
             (unsigned)CANSM_BUSOFF_MAX_RETRIES);

    Can_SetControllerMode(0U, CAN_T_START);
    CanSM_State = CANSM_STATE_FULL_COM;
    /* 回復成功を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする */
    Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
    /* 回復成功 → ComM に FULL_COM を通知 → EcuM_RequestRUN → RUN へ戻る */
    ComM_BusSMIndication(0U, COMM_FULL_COMMUNICATION);
    /* 再度 Bus-Off が発生すれば CanIf → CanSM_ControllerBusOff() が呼ばれる */
}

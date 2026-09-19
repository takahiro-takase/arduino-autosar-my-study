/**
 * \file    Rte_fake.c
 * \brief   Dcm_Cbk.c の Rte 依存（SID 0x22/0x2F 用の Read/IoControl ポート）を
 *          満たすだけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 *          本物の Rte.c は Com/E2E/SecOC/App_* まで巨大な依存グラフを
 *          引き込むため（他 chain テストと同じ理由）リンクしない。
 */
#include "Rte.h"

Rte_IStatusType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data)
{
    if (data != NULL)
        *data = 0U;
    return RTE_E_OK;
}

Rte_IStatusType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data)
{
    if (data != NULL)
        *data = 0U;
    return RTE_E_OK;
}

Std_ReturnType Rte_Read_EngineStatus_EngineState(EngineState_t* data)
{
    if (data != NULL)
        *data = 0U;
    return E_OK;
}

Std_ReturnType Rte_IoControl_Lamp_ReturnControlToEcu(Rte_LampIdType lamp)
{
    (void)lamp;
    return E_OK;
}

Std_ReturnType Rte_IoControl_Lamp_ResetToDefault(Rte_LampIdType lamp)
{
    (void)lamp;
    return E_OK;
}

Std_ReturnType Rte_IoControl_Lamp_FreezeCurrentState(Rte_LampIdType lamp)
{
    (void)lamp;
    return E_OK;
}

Std_ReturnType Rte_IoControl_Lamp_ShortTermAdjustment(Rte_LampIdType lamp, uint8 level)
{
    (void)lamp;
    (void)level;
    return E_OK;
}

Std_ReturnType Rte_IoControl_Lamp_GetCurrentLevel(Rte_LampIdType lamp, uint8* level)
{
    (void)lamp;
    if (level != NULL)
        *level = 0U;
    return E_OK;
}

Std_ReturnType Rte_Call_LedRunning_SetLevel(uint8 level)
{
    (void)level;
    return E_OK;
}

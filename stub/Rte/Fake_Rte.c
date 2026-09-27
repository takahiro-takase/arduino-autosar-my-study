/**
 * \file    Fake_Rte.c
 * \brief   Dcm.c の Rte 依存（SID 0x22/0x2F/0x31 用の Read/IoControl ポート）と
 *          SecOC_PBCfg.c が参照する VerificationStatusCallout を満たすだけの
 *          最小リンクスタブ。SID 0x19 の処理には関与しない。
 *          本物の Rte.c は Com/E2E/SecOC/App_* まで巨大な依存グラフを
 *          引き込むため（他 chain テストと同じ理由）リンクしない。
 */
#include "Rte.h"
#include "SecOC_Types.h"

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

/* SecOC_PBCfg.c から extern 宣言経由で VerificationStatusCallout として
 * 参照される（Rte.c 本体側の同名関数コメント参照）。ここでは呼び出し記録の
 * 必要が無いため（native_chain の SecOC テストは Csm/CryIf/Crypto 経由の
 * 実 MAC 検証結果そのものを検証する）、何もしない no-op で足りる。 */
void Rte_SecOCVerificationStatus_ImmobilizerCmd(SecOC_VerificationStatusType status)
{
    (void)status;
}

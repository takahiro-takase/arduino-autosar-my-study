/**
 * \file    Hal_Can_Hw_fake.c
 * \brief   Can_Hw.h（MCP2515 境界）のテスト用フェイク実装
 * \details Hal_Can_Hw_fake.h 冒頭のコメント参照。
 */
#include "Hal_Can_Hw_fake.h"

uint32 FakeCanHw_InitCount            = 0U;
uint32 FakeCanHw_SendCount            = 0U;
uint32 FakeCanHw_ReadCount            = 0U;
uint32 FakeCanHw_InitMaskCount        = 0U;
uint32 FakeCanHw_InitFilterCount      = 0U;
uint32 FakeCanHw_SetModeCount         = 0U;
uint32 FakeCanHw_CheckReceiveCount    = 0U;
uint32 FakeCanHw_IsBusOffCount        = 0U;
uint32 FakeCanHw_GetErrorStateCount   = 0U;
uint32 FakeCanHw_IsWakeupPendingCount = 0U;
uint32 FakeCanHw_AttachRxIsrCount     = 0U;

uint32_t FakeCanHw_LastSendId     = 0U;
uint8_t  FakeCanHw_LastSendDlc    = 0U;
uint8_t  FakeCanHw_LastSendData[8] = { 0U };

Can_Hw_Mode FakeCanHw_LastMode = CAN_HW_MODE_LISTEN_ONLY;

void (*FakeCanHw_AttachedIsr)(void) = NULL;

uint32_t FakeCanHw_RxPendingCount = 0U;
uint32_t FakeCanHw_RxId           = 0U;
uint8_t  FakeCanHw_RxDlc          = 0U;
uint8_t  FakeCanHw_RxData[8]      = { 0U };

Can_Hw_ReturnType FakeCanHw_InitReturn            = CAN_HW_OK;
Can_Hw_ReturnType FakeCanHw_SendReturn            = CAN_HW_OK;
Can_Hw_ReturnType FakeCanHw_IsBusOffReturn        = CAN_HW_FAIL;
Can_Hw_ReturnType FakeCanHw_IsWakeupPendingReturn = CAN_HW_FAIL;

uint8_t           FakeCanHw_ErrorState           = 0U;
Can_Hw_ReturnType FakeCanHw_GetErrorStateReturn   = CAN_HW_OK;

void FakeCanHw_Reset(void)
{
    FakeCanHw_InitCount            = 0U;
    FakeCanHw_SendCount            = 0U;
    FakeCanHw_ReadCount            = 0U;
    FakeCanHw_InitMaskCount        = 0U;
    FakeCanHw_InitFilterCount      = 0U;
    FakeCanHw_SetModeCount         = 0U;
    FakeCanHw_CheckReceiveCount    = 0U;
    FakeCanHw_IsBusOffCount        = 0U;
    FakeCanHw_GetErrorStateCount   = 0U;
    FakeCanHw_IsWakeupPendingCount = 0U;
    FakeCanHw_AttachRxIsrCount     = 0U;

    FakeCanHw_LastSendId  = 0U;
    FakeCanHw_LastSendDlc = 0U;
    for (uint8_t i = 0U; i < 8U; i++)
        FakeCanHw_LastSendData[i] = 0U;

    FakeCanHw_LastMode    = CAN_HW_MODE_LISTEN_ONLY;
    FakeCanHw_AttachedIsr = NULL;

    FakeCanHw_RxPendingCount = 0U;
    FakeCanHw_RxId           = 0U;
    FakeCanHw_RxDlc          = 0U;
    for (uint8_t i = 0U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;

    FakeCanHw_InitReturn            = CAN_HW_OK;
    FakeCanHw_SendReturn            = CAN_HW_OK;
    FakeCanHw_IsBusOffReturn        = CAN_HW_FAIL;
    FakeCanHw_IsWakeupPendingReturn = CAN_HW_FAIL;

    FakeCanHw_ErrorState           = 0U;
    FakeCanHw_GetErrorStateReturn  = CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz)
{
    (void)csPin;
    (void)baudrate;
    (void)crystalFreqMhz;
    FakeCanHw_InitCount++;
    return FakeCanHw_InitReturn;
}

Can_Hw_ReturnType Can_Hw_Send(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    FakeCanHw_SendCount++;
    FakeCanHw_LastSendId  = id;
    FakeCanHw_LastSendDlc = dlc;
    for (uint8_t i = 0U; i < dlc && i < 8U; i++)
        FakeCanHw_LastSendData[i] = data[i];
    return FakeCanHw_SendReturn;
}

Can_Hw_ReturnType Can_Hw_Read(uint32_t* id, uint8_t* dlc, uint8_t* data)
{
    FakeCanHw_ReadCount++;

    if (FakeCanHw_RxPendingCount == 0U)
        return CAN_HW_FAIL;

    FakeCanHw_RxPendingCount--;
    *id  = FakeCanHw_RxId;
    *dlc = FakeCanHw_RxDlc;
    for (uint8_t i = 0U; i < FakeCanHw_RxDlc && i < 8U; i++)
        data[i] = FakeCanHw_RxData[i];

    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_InitMask(uint8_t num, uint8_t ext, uint32_t mask)
{
    (void)num;
    (void)ext;
    (void)mask;
    FakeCanHw_InitMaskCount++;
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_InitFilter(uint8_t num, uint8_t ext, uint32_t filter)
{
    (void)num;
    (void)ext;
    (void)filter;
    FakeCanHw_InitFilterCount++;
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_SetMode(Can_Hw_Mode mode)
{
    FakeCanHw_SetModeCount++;
    FakeCanHw_LastMode = mode;
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_CheckReceive(void)
{
    FakeCanHw_CheckReceiveCount++;
    return (FakeCanHw_RxPendingCount > 0U) ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_IsBusOff(void)
{
    FakeCanHw_IsBusOffCount++;
    return FakeCanHw_IsBusOffReturn;
}

Can_Hw_ReturnType Can_Hw_IsWakeupPending(void)
{
    FakeCanHw_IsWakeupPendingCount++;
    return FakeCanHw_IsWakeupPendingReturn;
}

Can_Hw_ReturnType Can_Hw_GetErrorState(uint8_t* errorStateOut)
{
    FakeCanHw_GetErrorStateCount++;
    if (FakeCanHw_GetErrorStateReturn == CAN_HW_OK)
        *errorStateOut = FakeCanHw_ErrorState;
    return FakeCanHw_GetErrorStateReturn;
}

Can_Hw_ReturnType Can_Hw_AttachRxIsr(uint8_t intPin, void (*isr)(void))
{
    (void)intPin;
    FakeCanHw_AttachRxIsrCount++;
    FakeCanHw_AttachedIsr = isr;
    return CAN_HW_OK;
}

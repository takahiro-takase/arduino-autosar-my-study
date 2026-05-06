#ifndef CAN_H
#define CAN_H

#include "Platform_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32 filterId;
    uint32 mask;
} Can_FilterConfigType;

typedef uint8 Can_CrystalFreqType;
#define CAN_CRYSTAL_8MHZ  ((Can_CrystalFreqType)8U)
#define CAN_CRYSTAL_16MHZ ((Can_CrystalFreqType)16U)
#define CAN_CRYSTAL_20MHZ ((Can_CrystalFreqType)20U)

typedef struct
{
    Can_FilterConfigType filter;
    uint8               csPin;
    uint8               intPin;
    uint32              baudrate;
    Can_CrystalFreqType crystalFreq;
} Can_ConfigType;

void           Can_Init(const Can_ConfigType* Config);
void           Can_SetControllerMode(Can_ControllerStateType mode);
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void           Can_MainFunction_Read(void);
void           Can_Isr(void);

#ifdef __cplusplus
}
#endif

#endif

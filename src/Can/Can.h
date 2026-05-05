#ifndef CAN_H
#define CAN_H

#include "Platform_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

// AUTOSAR 風 Config（簡易版）
typedef struct
{
    uint32 filterId; // 受信したい ID
    uint32 mask;     // マスク
} Can_FilterConfigType;

// クリスタル周波数（MHz 単位）
// MCP2515 ライブラリ固有の定数（MCP_8MHZ 等）はここに出さない。
// Mcp2515_Wrapper 内部で変換する。
typedef uint8 Can_CrystalFreqType;
#define CAN_CRYSTAL_8MHZ  ((Can_CrystalFreqType)8U)
#define CAN_CRYSTAL_16MHZ ((Can_CrystalFreqType)16U)
#define CAN_CRYSTAL_20MHZ ((Can_CrystalFreqType)20U)

typedef struct
{
    Can_FilterConfigType filter;
    uint8               csPin;       // SPI チップセレクトピン番号
    uint8               intPin;      // MCP2515 の INT ピン番号（Can_Isr で使用）
    uint32              baudrate;
    Can_CrystalFreqType crystalFreq; // クリスタル発振周波数（MHz）
} Can_ConfigType;

void           Can_Init(const Can_ConfigType* Config);
void           Can_SetControllerMode(Can_ControllerStateType mode);
// AUTOSAR SWS_Can_00233: Can_Write(Hth, PduInfo)
// Hth: Hardware Transmit Handle（使用する TX バッファ識別子）
// PduInfo: 送信データ（id / length / sdu を含む Can_PduType へのポインタ）
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void           Can_MainFunction_Read(void);
void           Can_Isr(void);

#endif
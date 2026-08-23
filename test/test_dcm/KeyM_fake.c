/**
 * \file    KeyM_fake.c
 * \brief   Dcm_Cbk.c の KeyM 依存（SID 0x2E CryptoKeyUpdate DID）を満たす
 *          だけの最小リンクスタブ。SID 0x19 の処理には関与しない。
 */
#include "KeyM.h"

Std_ReturnType KeyM_Start(KeyM_StartType StartType,
                          const uint8* RequestData, uint16 RequestDataLength,
                          uint8* ResponseData, uint16* ResponseDataLength)
{
    (void)StartType;
    (void)RequestData;
    (void)RequestDataLength;
    (void)ResponseData;
    (void)ResponseDataLength;
    return E_NOT_OK;
}

Std_ReturnType KeyM_Update(const uint8* KeyNamePtr, uint16 KeyNameLength,
                           const uint8* RequestDataPtr, uint16 RequestDataLength,
                           uint8* ResultDataPtr, uint16 ResultDataMaxLength)
{
    (void)KeyNamePtr;
    (void)KeyNameLength;
    (void)RequestDataPtr;
    (void)RequestDataLength;
    (void)ResultDataPtr;
    (void)ResultDataMaxLength;
    return E_NOT_OK;
}

Std_ReturnType KeyM_Finalize(const uint8* RequestDataPtr, uint16 RequestDataLength,
                             uint8* ResponseDataPtr, uint16 ResponseMaxDataLength)
{
    (void)RequestDataPtr;
    (void)RequestDataLength;
    (void)ResponseDataPtr;
    (void)ResponseMaxDataLength;
    return E_NOT_OK;
}

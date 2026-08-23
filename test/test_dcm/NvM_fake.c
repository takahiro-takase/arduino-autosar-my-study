/**
 * \file    NvM_fake.c
 * \brief   Dem.c の NvM 依存を満たすだけの最小リンクスタブ。
 * \details Dem_Init() は NVM_BLOCK_ID_DEM_MAGIC を読み、マジックバイトが
 *          一致しなければ「初回起動」として全イベントを初期状態にする
 *          （Dem.c の Dem_Init() 実装参照）。NvM_ReadBlock() が常に
 *          E_NOT_OK を返し、出力バッファにも触れないことで、この
 *          「初回起動」パスを毎テスト実行で決定的に再現する（＝EEPROM
 *          永続化のロジック自体は本テストの対象外）。
 */
#include "NvM.h"

void NvM_Init(const NvM_ConfigType* ConfigPtr)
{
    (void)ConfigPtr;
}

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr)
{
    (void)BlockId;
    (void)NvM_DstPtr;
    return E_NOT_OK;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr)
{
    (void)BlockId;
    (void)NvM_SrcPtr;
    return E_OK;
}

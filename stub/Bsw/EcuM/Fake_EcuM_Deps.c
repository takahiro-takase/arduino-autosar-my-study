/**
 * \file    Fake_EcuM_Deps.c
 * \brief   実 EcuM.c を native_chain へリンクするための未解決シンボルの穴埋め
 * \details EcuM_Init() は多数のモジュールの PBCfg・初期化関数を参照するが、
 *          native_chain の対象は EcuM_Init() の起動シーケンスではなく
 *          Det_ReportError() を伴う異常系の入力検証である。このため
 *          EcuM_Init() からのみ参照されるシンボルを、リンクを通すためだけの
 *          空定義として置く。EcuM_Init() 自体はテストから呼ばないこと。
 */
#include "Std_Types.h"
#include "Can_PBCfg.h"
#include "CanIf_PBCfg.h"
#include "Com_PBCfg.h"
#include "Gpt_PBCfg.h"
#include "PduR_PBCfg.h"
#include "Os_PBCfg.h"
#include "E2EMon.h"
#include "Rte.h"
#include "App_GptDemo.h"

const Can_ConfigType       Can_Config;
const CanIf_ConfigType     CanIf_Config;
const Com_ConfigType       Com_Config;
const Gpt_ConfigType       Gpt_Config;
const PduR_PBConfigType    PduR_Config;
const Os_ConfigType        Os_Config;

void E2EMon_Init(void)
{
}

void App_GptDemo_Init(void)
{
}

Std_ReturnType Rte_Start(void)
{
    return E_OK;
}

void Rte_Init_EngineManager(void)
{
}

void Rte_Init_WarningIndicator(void)
{
}

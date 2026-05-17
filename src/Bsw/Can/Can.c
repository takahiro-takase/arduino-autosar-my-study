/**
 * \file    Can.c
 * \brief   CAN ドライバ (AUTOSAR SWS_Can 準拠)
 * \details AUTOSAR CanDrv API を MCP2515 上に実装する。
 *          Mcp2515_Wrapper 経由でハードウェアを操作し、
 *          AUTOSAR 4.x SWS_Can 仕様に準拠する。
 *          Arduino UNO 向けに一部を簡略化している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Can.h"
#include "Mcp2515_Wrapper.h"
#include "CanIf.h"
#include "Det.h"

/* Arduino wiring.c（C リンケージ）で定義 */
extern int digitalRead(uint8 pin);

static const Can_ConfigType*   Can_ConfigPtr = NULL;
static Can_ControllerStateType CanState      = CAN_CS_UNINIT;

/**
 * \brief   CAN ドライバを初期化する。
 *
 * \details MCP2515 ハードウェアを初期化し、受信フィルタ・マスクを
 *          すべて設定したうえでコントローラを CAN_CS_STOPPED 状態に
 *          移行する (AUTOSAR SWS_Can_00246)。
 *          初期化に失敗した場合は無限ループで停止する。
 *
 * \param[in]  Config  CAN ドライバ設定構造体へのポインタ。
 *                     NULL 禁止。
 *
 * \pre        SPI ペリフェラルがこの呼び出しより前に初期化済みであること。
 * \note       他のすべての Can_* API より先に、システム起動時に
 *             1 回だけ呼び出すこと。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_Init(const Can_ConfigType* Config)
{
    Det_LogP(PSTR("[Can_Init] Initializing CAN..."));

    Can_ConfigPtr = Config;

    if (Mcp2515_Init(Config->csPin, Config->baudrate, Config->crystalFreq) != MCP2515_WRAPPER_OK)
    {
        Det_LogP(PSTR("[Can_Init] FAIL"));
        while (1)
            ;
    }

    Det_LogP(PSTR("[Can_Init] CAN Initialized successfully"));

    Mcp2515_InitMask(0, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(0, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(1, 0, Config->filter.filterId << 16);
    Mcp2515_InitMask(1, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(2, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(3, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(4, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(5, 0, Config->filter.filterId << 16);

    Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
    CanState = CAN_CS_STOPPED;
}

/**
 * \brief   CAN コントローラの状態遷移を要求する。
 *
 * \details AUTOSAR の状態遷移を対応する MCP2515 動作モードへ
 *          マッピングする (AUTOSAR SWS_Can_00017, SWS_Can_00230)。
 *          - CAN_T_START  : CAN_CS_STOPPED → CAN_CS_STARTED (通常モード)
 *          - CAN_T_STOP   : CAN_CS_STARTED → CAN_CS_STOPPED (受信専用モード)
 *          - CAN_T_SLEEP  : CAN_CS_STOPPED → CAN_CS_SLEEP   (スリープモード)
 *          - CAN_T_WAKEUP : CAN_CS_SLEEP   → CAN_CS_STOPPED (受信専用モード)
 *
 * \param[in]  Controller  CAN コントローラのインデックス。
 *                         本実装はコントローラ 0 のみ対応。
 *                         0 以外を指定すると CAN_NOT_OK を返す。
 * \param[in]  Transition  要求する状態遷移 (Can_StateTransitionType)。
 *
 * \retval  CAN_OK      遷移が正常に適用された。
 * \retval  CAN_NOT_OK  Controller が無効、または未対応の Transition 値。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Can_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition)
{
    if (Controller != 0U)
        return CAN_NOT_OK;

    switch (Transition)
    {
    case CAN_T_START:
        Mcp2515_SetMode(MCP2515_MODE_NORMAL);
        CanState = CAN_CS_STARTED;
        break;
    case CAN_T_STOP:
        Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
        CanState = CAN_CS_STOPPED;
        break;
    case CAN_T_SLEEP:
        Mcp2515_SetMode(MCP2515_MODE_SLEEP);
        CanState = CAN_CS_SLEEP;
        break;
    case CAN_T_WAKEUP:
        Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
        CanState = CAN_CS_STOPPED;
        break;
    default:
        return CAN_NOT_OK;
    }

    return CAN_OK;
}

/**
 * \brief   CAN フレームの送信を要求する。
 *
 * \details PDU を MCP2515 の送信バッファに渡し、送信成功後に
 *          CanIf_TxConfirmation() で上位層へ通知する
 *          (AUTOSAR SWS_Can_00016)。
 *          コントローラが CAN_CS_STARTED 状態でない場合は
 *          即座に CAN_NOT_OK を返す。
 *
 * \param[in]  Hth      ハードウェア送信ハンドル。MCP2515 が TX バッファを
 *                      自動選択するため、この実装では使用しない。
 * \param[in]  PduInfo  送信する PDU へのポインタ。NULL 禁止。
 *                      使用メンバー: id, length, sdu, swPduHandle。
 *
 * \retval  CAN_OK      フレームが受理され、送信に成功した。
 * \retval  CAN_NOT_OK  コントローラ未起動、または MCP2515 送信失敗。
 * \retval  CAN_BUSY    この実装では返さない（MCP2515 が自動リトライ）。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant (different Hth)}
 * \Synchronicity  {Synchronous}
 */
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    (void)Hth;

    if (CanState != CAN_CS_STARTED)
        return CAN_NOT_OK;

    if (Mcp2515_Send(PduInfo->id, PduInfo->length, PduInfo->sdu) != MCP2515_WRAPPER_OK)
        return CAN_NOT_OK;

    Det_PrintP(PSTR("[Can_Write] Sent ID=0x"));
    Det_PrintHex(PduInfo->id);
    Det_PrintP(PSTR(" Data="));
    for (int i = 0; i < PduInfo->length; i++)
    {
        Det_PrintHex(PduInfo->sdu[i]);
        Det_PrintP(PSTR(" "));
    }
    Det_Newline();

    CanIf_TxConfirmation(PduInfo->swPduHandle);

    return CAN_OK;
}

/**
 * \brief   ポーリング方式で受信フレームを処理する。
 *
 * \details MCP2515 の受信ステータスレジスタを確認し、フレームが
 *          存在する場合に読み出して CanIf_RxIndication() で
 *          上位層へ通知する (AUTOSAR SWS_Can_00108)。
 *          メインループまたはタスクスケジューラから定期的に
 *          呼び出すことを想定している。
 *
 * \pre        Can_Init() が正常に完了していること。
 * \pre        コントローラが CAN_CS_STARTED 状態であること。
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_Read(void)
{
    if (CanState != CAN_CS_STARTED)
        return;

    if (Mcp2515_CheckReceive() == MCP2515_WRAPPER_OK)
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == MCP2515_WRAPPER_OK)
        {
            Can_HwType  mailbox = { .CanId = rxId, .Hoh = 0U, .ControllerId = 0U };
            PduInfoType pduInfo = { .SduDataPtr = buf, .SduLength = (PduLengthType)len };
            CanIf_RxIndication(&mailbox, &pduInfo);
        }
    }
}

/**
 * \brief   MCP2515 INT ピンを監視する割り込みサービスルーティン（擬似 ISR）。
 *
 * \details INT ピン（アクティブ LOW）をポーリングし、アサートされている
 *          場合に受信フレームを読み出して CanIf_RxIndication() で
 *          上位層へ通知する (AUTOSAR SWS_Can_00396)。
 *          Arduino の loop() から呼び出し、ハードウェア ISR の代替として
 *          機能する。
 *
 * \pre        Can_Init() が正常に完了していること。
 * \note       AUTOSAR 標準外の API。完全な AUTOSAR OS 環境では
 *             OS に登録された ISR カテゴリ 2 ハンドラとして実装される。
 *             INT ピン番号は Can_ConfigType::intPin から取得する。
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_Isr(void)
{
    if (Can_ConfigPtr == NULL)
        return;

    if (!digitalRead(Can_ConfigPtr->intPin))
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == MCP2515_WRAPPER_OK)
        {
            Can_HwType  mailbox = { .CanId = rxId, .Hoh = 0U, .ControllerId = 0U };
            PduInfoType pduInfo = { .SduDataPtr = buf, .SduLength = (PduLengthType)len };
            CanIf_RxIndication(&mailbox, &pduInfo);
        }
    }
}

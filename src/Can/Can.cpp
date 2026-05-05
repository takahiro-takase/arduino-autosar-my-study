#include <SPI.h>
#include "Can.h"
#include "Mcp2515_Wrapper.h"
#include "CanIf.h" // AUTOSAR SWS_Can_00396: CanDrv は受信時に CanIf_RxIndication を呼ぶ

static const Can_ConfigType*   Can_ConfigPtr = nullptr;
static Can_ControllerStateType CanState      = CAN_CS_UNINIT;

// -----------------------------
// AUTOSAR API：Can_Init(Config)
// CAN ドライバの初期化
// 完了後の状態は CAN_CS_STOPPED（AUTOSAR SWS_Can_00246）
// 通信開始は Can_SetControllerMode(CAN_CS_STARTED) で明示的に行う
// -----------------------------
void Can_Init(const Can_ConfigType* Config)
{
    Serial.println("[Can_Init] Initializing CAN...");

    Can_ConfigPtr = Config;

    // CONFIG モードで初期化
    // CAN.begin(mode, speed, clock) の mode の意味
    // MCP_ANY    : 標準ID/拡張IDを全受信（フィルタ無効になるので注意）
    // MCP_STDEXT : 標準ID/拡張IDを受信（フィルタ有効） ← フィルタ使用時はこれ
    // MCP_STD    : 標準IDのみ受信（フィルタ有効） ← [Can_Init] FAIL
    // MCP_EXT    : 拡張IDのみ受信（フィルタ有効）
    if (Mcp2515_Init(Config->csPin, Config->baudrate, Config->crystalFreq) != Mcp2515_ReturnType::OK)
    {
        Serial.println("[Can_Init] FAIL");
        while (1)
            ;
    }

    Serial.println("[Can_Init] CAN Initialized successfully");

    // フィルタ設定（init_Mask/init_Filt は内部で CONFIG モードに切り替えて設定する）
    Mcp2515_InitMask(0, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(0, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(1, 0, Config->filter.filterId << 16);
    Mcp2515_InitMask(1, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(2, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(3, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(4, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(5, 0, Config->filter.filterId << 16);

    // AUTOSAR 準拠: Init 完了後は STOPPED 状態
    // TX/RX は行わず、LISTEN_ONLY モード（受信専用）でハードウェアを待機させる
    Mcp2515_SetMode(Mcp2515_Mode::LISTEN_ONLY);
    CanState = CAN_CS_STOPPED;
}

// -----------------------------
// AUTOSAR API：Can_SetControllerMode()
// コントローラの状態を明示的に切り替える
//   CAN_CS_STARTED  → NORMAL モード（TX/RX 可）
//   CAN_CS_STOPPED  → LISTEN_ONLY モード（TX 不可、コンフィグ保持）
//   CAN_CS_SLEEP    → SLEEP モード（低電力）
// -----------------------------
void Can_SetControllerMode(Can_ControllerStateType mode)
{
    switch (mode)
    {
    case CAN_CS_STARTED:
        Mcp2515_SetMode(Mcp2515_Mode::NORMAL);
        break;
    case CAN_CS_STOPPED:
        // SLEEP とは異なり低電力ではないが TX を禁止する
        Mcp2515_SetMode(Mcp2515_Mode::LISTEN_ONLY);
        break;
    case CAN_CS_SLEEP:
        Mcp2515_SetMode(Mcp2515_Mode::SLEEP);
        break;
    default:
        return;
    }

    CanState = mode;
}

// -----------------------------
// AUTOSAR API：Can_Write(Hth, PduInfo)
// Hth     : Hardware Transmit Handle（TX バッファ識別子、今回は未使用）
// PduInfo : 送信 PDU（id / length / sdu）
// -----------------------------
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    (void)Hth; // MCP2515 は TX バッファを自動選択するため使用しない

    if (CanState != CAN_CS_STARTED)
        return CAN_NOT_OK;

    if (Mcp2515_Send(PduInfo->id, PduInfo->length, PduInfo->sdu) != Mcp2515_ReturnType::OK)
        return CAN_NOT_OK;

    Serial.print("[Can_Write] Sent ID=0x");
    Serial.print(PduInfo->id, HEX);
    Serial.print(" Data=");
    for (int i = 0; i < PduInfo->length; i++)
    {
        Serial.print(PduInfo->sdu[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // AUTOSAR SWS_Can_00016: 送信完了を CanIf に通知する
    // swPduHandle は CanIf_Transmit が書き込んだ TxPduId をそのまま返す
    CanIf_TxConfirmation(PduInfo->swPduHandle);

    return CAN_OK;
}

// -----------------------------
// AUTOSAR API：Can_MainFunction_Read()
// 受信ポーリング処理（タスクから周期的に呼ぶ）
// -----------------------------
void Can_MainFunction_Read(void)
{
    if (CanState != CAN_CS_STARTED)
        return;

    if (Mcp2515_CheckReceive() == Mcp2515_ReturnType::OK)
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == Mcp2515_ReturnType::OK)
        {
            // AUTOSAR SWS_Can_00396: 受信フレームを CanIf に通知する
            // HRH=0: 今回は RXB0/RXB1 を区別せず単一グループとして扱う
            CanIf_RxIndication(0, rxId, len, buf);
        }
    }
}

// -----------------------------
// AUTOSAR API：Can_Isr()
// MCP2515 の INT ピンを監視し、受信フレームを取り出す
// INT ピンはアクティブ LOW（LOW = 割り込み発生中）
// -----------------------------
void Can_Isr(void)
{
    if (Can_ConfigPtr == nullptr)
        return;

    if (!digitalRead(Can_ConfigPtr->intPin))
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == Mcp2515_ReturnType::OK)
        {
            // AUTOSAR SWS_Can_00396: 受信フレームを CanIf に通知する
            CanIf_RxIndication(0, rxId, len, buf);
        }
    }
}

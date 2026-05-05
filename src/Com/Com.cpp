#include <Arduino.h>
#include "Com.h"
#include "PduR.h"

static const Com_ConfigType* Com_ConfigPtr = nullptr;

// 受信バッファ：最後に受け取った PDU のバイト列を保持する
static uint8 Com_RxBuffer[8];

// 送信バッファ：シグナル値を詰めてから PduR_Transmit に渡す
static uint8 Com_TxBuffer[8];

// -------------------------------------------------------
// Com_Init
// -------------------------------------------------------
void Com_Init(const Com_ConfigType* Config)
{
    if (Config == nullptr)
    {
        Serial.println("[Com_Init] ERROR: Config is NULL");
        return;
    }
    Com_ConfigPtr = Config;

    for (uint8 i = 0; i < 8; i++)
    {
        Com_RxBuffer[i] = 0;
        Com_TxBuffer[i] = 0;
    }

    Serial.println("[Com_Init] initialized");
    Serial.print("  Signals: ");
    Serial.println(Config->SignalCount);
    for (uint8 s = 0; s < Config->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Config->Signals[s];
        Serial.print("    SignalId=");
        Serial.print(sig->SignalId);
        Serial.print(" IPduId=");
        Serial.print(sig->IPduId);
        Serial.print(" offset=");
        Serial.print(sig->ByteOffset);
        Serial.print(" len=");
        Serial.println(sig->ByteLength);
    }
}

// -------------------------------------------------------
// Com_RxIndication
// PduR から届いた PDU バイト列を RxBuffer にコピーし、
// シグナル単位でログ出力する。
// -------------------------------------------------------
void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    if (Com_ConfigPtr == nullptr || PduInfoPtr == nullptr)
    {
        return;
    }

    // PDU バイト列を RxBuffer にコピー
    for (PduLengthType i = 0; i < PduInfoPtr->SduLength && i < 8; i++)
    {
        Com_RxBuffer[i] = PduInfoPtr->SduDataPtr[i];
    }

    // シグナルごとに値を表示
    Serial.print("[Com_RxIndication] PduId=");
    Serial.print(PduId);
    Serial.println(" -> signals:");
    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->IPduId != PduId)
        {
            continue; // このPDUに属さないシグナルはスキップ
        }

        // ByteLength 分のバイトを uint32 に展開（big-endian）
        uint32 value = 0;
        for (uint8 b = 0; b < sig->ByteLength; b++)
        {
            value = (value << 8) | Com_RxBuffer[sig->ByteOffset + b];
        }

        Serial.print("  SignalId=");
        Serial.print(sig->SignalId);
        Serial.print(" value=");
        Serial.println(value);
    }
}

// -------------------------------------------------------
// Com_TxConfirmation
// PduR から届く送信完了通知。
// -------------------------------------------------------
void Com_TxConfirmation(PduIdType PduId)
{
    Serial.print("[Com_TxConfirmation] PduId=");
    Serial.print(PduId);
    Serial.println(" : TX complete");
}

// -------------------------------------------------------
// Com_SendSignal
// TX バッファの ByteOffset 位置に SignalDataPtr の値を書き込む。
// 実際の送信は Com_TriggerIPDUSend で行う。
// -------------------------------------------------------
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr)
{
    if (Com_ConfigPtr == nullptr || SignalDataPtr == nullptr)
    {
        return E_NOT_OK;
    }

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
        {
            continue;
        }

        // TxBuffer の該当バイトへ書き込み（big-endian）
        for (uint8 b = 0; b < sig->ByteLength; b++)
        {
            Com_TxBuffer[sig->ByteOffset + b] = SignalDataPtr[b];
        }
        return E_OK;
    }

    return E_NOT_OK;
}

// -------------------------------------------------------
// Com_TriggerIPDUSend
// TxBuffer 全体を PduR_Transmit に渡して 1 フレームで送信する。
// 複数の Com_SendSignal を呼んでからこれを 1 回呼ぶことで、
// 全シグナルを 1 CAN フレームにまとめて送れる。
// -------------------------------------------------------
Std_ReturnType Com_TriggerIPDUSend(void)
{
    if (Com_ConfigPtr == nullptr)
    {
        return E_NOT_OK;
    }

    PduInfoType pduInfo = {
        .SduDataPtr = Com_TxBuffer,
        .SduLength  = Com_ConfigPtr->TxDlc
    };
    return PduR_Transmit(Com_ConfigPtr->TxIPduId, &pduInfo);
}

// -------------------------------------------------------
// Com_ReceiveSignal
// RxBuffer から最後に受信したシグナル値を読み出す。
// -------------------------------------------------------
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr)
{
    if (Com_ConfigPtr == nullptr || SignalDataPtr == nullptr)
    {
        return E_NOT_OK;
    }

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
        {
            continue;
        }

        for (uint8 b = 0; b < sig->ByteLength; b++)
        {
            SignalDataPtr[b] = Com_RxBuffer[sig->ByteOffset + b];
        }
        return E_OK;
    }

    return E_NOT_OK;
}

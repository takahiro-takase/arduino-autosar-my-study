#include <Arduino.h>
#include "Com.h"
#include "PduR.h"

// -------------------------------------------------------
// 内部バッファ（Step 3 以降で実装する）
// -------------------------------------------------------
#define COM_IPDU_MAX_DLC  8
#define COM_RX_IPDU_MAX   1
#define COM_TX_IPDU_MAX   1

static const Com_ConfigType* Com_ConfigPtr = nullptr;
static uint8 Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8 Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

// -------------------------------------------------------
// Com_Init
// -------------------------------------------------------
void Com_Init(const Com_ConfigType* Config)
{
    // --- 1. NULL チェック ---
    if (Config == nullptr)
    {
        Serial.println("[Com_Init] ERROR: Config is NULL");
        return;
    }

    // --- 2. バッファ範囲チェック ---
    //    IPduId を配列インデックスとして使うため、
    //    登録数が静的バッファサイズを超えていないか確認する
    if (Config->RxIPduCount > COM_RX_IPDU_MAX)
    {
        Serial.println("[Com_Init] ERROR: RxIPduCount exceeds COM_RX_IPDU_MAX");
        return;
    }
    if (Config->TxIPduCount > COM_TX_IPDU_MAX)
    {
        Serial.println("[Com_Init] ERROR: TxIPduCount exceeds COM_TX_IPDU_MAX");
        return;
    }

    // --- 3. 設定ポインタ保存 ---
    Com_ConfigPtr = Config;

    // --- 4. RX / TX バッファをゼロクリア ---
    //    未受信状態でゴミ値が読めないようにするため
    for (uint8 i = 0; i < COM_RX_IPDU_MAX; i++)
    {
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
        {
            Com_RxBuffer[i][j] = 0;
        }
    }
    for (uint8 i = 0; i < COM_TX_IPDU_MAX; i++)
    {
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
        {
            Com_TxBuffer[i][j] = 0;
        }
    }

    // --- 5. 登録内容をダンプ（デバッグ用）---
    Serial.println("[Com_Init] initialized");

    Serial.print("  RX I-PDUs: ");
    Serial.println(Config->RxIPduCount);
    for (uint8 i = 0; i < Config->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Config->RxIPdus[i];
        Serial.print("    [");
        Serial.print(i);
        Serial.print("] IPduId=");
        Serial.print(ipdu->IPduId);
        Serial.print(" DLC=");
        Serial.print(ipdu->DLC);
        Serial.print(" PduRId=");
        Serial.println(ipdu->PduRId);
    }

    Serial.print("  TX I-PDUs: ");
    Serial.println(Config->TxIPduCount);
    for (uint8 i = 0; i < Config->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Config->TxIPdus[i];
        Serial.print("    [");
        Serial.print(i);
        Serial.print("] IPduId=");
        Serial.print(ipdu->IPduId);
        Serial.print(" DLC=");
        Serial.print(ipdu->DLC);
        Serial.print(" PduRId=");
        Serial.println(ipdu->PduRId);
    }

    Serial.print("  Signals: ");
    Serial.println(Config->SignalCount);
    for (uint8 s = 0; s < Config->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Config->Signals[s];
        Serial.print("    [");
        Serial.print(s);
        Serial.print("] SignalId=");
        Serial.print(sig->SignalId);
        Serial.print(" IPduId=");
        Serial.print(sig->IPduId);
        Serial.print(" BitPos=");
        Serial.print(sig->BitPosition);
        Serial.print(" BitSize=");
        Serial.print(sig->BitSize);
        Serial.print(" Endian=");
        Serial.println(sig->Endian == COM_BIG_ENDIAN ? "BIG" : "LITTLE");
    }
}

// -------------------------------------------------------
// Com_RxIndication
// -------------------------------------------------------
void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    // --- 1. 初期化・NULL チェック ---
    if (Com_ConfigPtr == nullptr || PduInfoPtr == nullptr)
    {
        return;
    }

    // --- 2. PduRId で RX I-PDU テーブルを線形探索 ---
    //    PduR が渡す PduId（DestPduId）= Com_IPduConfigType.PduRId
    //    名前空間変換: PduR の DestPduId → COM 内部の IPduId
    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];

        if (ipdu->PduRId != PduId)
        {
            continue;
        }

        // --- 3. RX バッファにバイト列をコピー ---
        //    DLC と受信長の小さい方だけコピーしてバッファ溢れを防ぐ
        //    Signal の抽出はここでは行わない（遅延抽出）
        const uint8 copyLen = (PduInfoPtr->SduLength < ipdu->DLC)
                              ? PduInfoPtr->SduLength
                              : ipdu->DLC;

        for (uint8 b = 0; b < copyLen; b++)
        {
            Com_RxBuffer[ipdu->IPduId][b] = PduInfoPtr->SduDataPtr[b];
        }

        // --- 4. ログ出力 ---
        Serial.print("[Com_RxIndication]");
        Serial.print(" PduRId=");
        Serial.print(PduId);
        Serial.print(" -> IPduId=");
        Serial.print(ipdu->IPduId);
        Serial.print(" raw=[");
        for (uint8 b = 0; b < copyLen; b++)
        {
            if (b > 0) { Serial.print(" "); }
            if (Com_RxBuffer[ipdu->IPduId][b] < 0x10) { Serial.print("0"); }
            Serial.print(Com_RxBuffer[ipdu->IPduId][b], HEX);
        }
        Serial.println("]");

        return; // PduRId は一意のため、一致後は探索終了
    }

    // --- 5. 一致エントリなし ---
    Serial.print("[Com_RxIndication] no I-PDU for PduRId=");
    Serial.println(PduId);
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr)
{
    (void)SignalId;
    (void)SignalDataPtr;
    return E_NOT_OK; // Step 5 で実装
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr)
{
    (void)SignalId;
    (void)SignalDataPtr;
    return E_NOT_OK; // Step 5 で実装
}

Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType IPduId)
{
    (void)IPduId;
    return E_NOT_OK; // Step 5 で実装
}

void Com_TxConfirmation(PduIdType PduId)
{
    Serial.print("[Com_TxConfirmation] PduId=");
    Serial.print(PduId);
    Serial.println(" : TX complete");
}

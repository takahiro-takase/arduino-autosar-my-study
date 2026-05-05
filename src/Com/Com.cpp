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

// -------------------------------------------------------
// [内部ヘルパー] Com_UnpackSignal
// PDU バッファからシグナル値をビット単位で取り出す。
//
// ビット番号の規約: bit0 = byte[0]のMSB、左から右へ連番
//   byteIdx = bitPos / 8
//   shift   = 7 - (bitPos % 8)   ← 7=MSB, 0=LSB
//
// big-endian : MSB から順にビットを積み上げる (value = (value<<1)|bit)
// little-endian: LSB から順にビットを配置する  (value |= bit << i)
//
// 戻り値はホストバイトオーダー（AVRはlittle-endian）の uint32
// -------------------------------------------------------
static uint32 Com_UnpackSignal(const uint8* buf,
                                uint8 bitPos,
                                uint8 bitSize,
                                Com_SignalEndianType endian)
{
    uint32 value = 0;
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 pos      = bitPos + i;
        const uint8 byteIdx  = pos / 8;
        const uint8 shift    = 7 - (pos % 8);
        const uint8 bit      = (buf[byteIdx] >> shift) & 1U;

        if (endian == COM_BIG_ENDIAN)
        {
            value = (value << 1) | bit; // MSB 優先：左から右へ積み上げ
        }
        else
        {
            value |= ((uint32)bit << i); // LSB 優先：i ビット目に配置
        }
    }
    return value;
}

// -------------------------------------------------------
// [内部ヘルパー] Com_PackSignal
// シグナル値（uint32）を PDU バッファの指定ビット位置に書き込む。
// 対象ビット以外は変更しない（RMW: Read-Modify-Write）。
//
// big-endian : i=0 が MSB → value の上位ビットから取り出す
// little-endian: i=0 が LSB → value の下位ビットから取り出す
// -------------------------------------------------------
static void Com_PackSignal(uint8* buf,
                            uint8 bitPos,
                            uint8 bitSize,
                            Com_SignalEndianType endian,
                            uint32 value)
{
    for (uint8 i = 0; i < bitSize; i++)
    {
        uint8 bit;
        if (endian == COM_BIG_ENDIAN)
        {
            bit = (value >> (bitSize - 1U - i)) & 1U; // MSB から順に取り出す
        }
        else
        {
            bit = (value >> i) & 1U; // LSB から順に取り出す
        }

        const uint8 pos     = bitPos + i;
        const uint8 byteIdx = pos / 8;
        const uint8 shift   = 7 - (pos % 8);

        if (bit)
        {
            buf[byteIdx] |=  (uint8)(1U << shift); // 対象ビットを 1 にセット
        }
        else
        {
            buf[byteIdx] &= (uint8)~(1U << shift); // 対象ビットを 0 にクリア
        }
    }
}

// -------------------------------------------------------
// Com_ReceiveSignal
// RX バッファからシグナル値をビット抽出し、ホストバイトオーダーで返す。
//
// SignalDataPtr には uint32 相当（4バイト）のバッファを渡すこと。
// AVR の場合はリトルエンディアンなので、例えば:
//   uint16 rpm; Com_ReceiveSignal(SIGNAL_ENGINE_SPEED, (uint8*)&rpm);
// と書けばそのまま使える。
// -------------------------------------------------------
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr)
{
    // --- 1. 初期化・NULL チェック ---
    if (Com_ConfigPtr == nullptr || SignalDataPtr == nullptr)
    {
        return E_NOT_OK;
    }

    // --- 2. SignalId でシグナルテーブルを線形探索 ---
    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
        {
            continue;
        }

        // --- 3. RX バッファからビット抽出 ---
        const uint32 value = Com_UnpackSignal(
            Com_RxBuffer[sig->IPduId],
            sig->BitPosition,
            sig->BitSize,
            sig->Endian
        );

        // --- 4. ホストバイトオーダー（little-endian）で出力 ---
        SignalDataPtr[0] = (uint8)(value);
        SignalDataPtr[1] = (uint8)(value >>  8);
        SignalDataPtr[2] = (uint8)(value >> 16);
        SignalDataPtr[3] = (uint8)(value >> 24);

        return E_OK;
    }

    return E_NOT_OK;
}

// -------------------------------------------------------
// Com_SendSignal
// ホストバイトオーダーのシグナル値を TX バッファの指定ビット位置へパック。
// 他のシグナルのビットは変更しない（RMW 設計）。
//
// SignalDataPtr には uint32 相当（4バイト）のバッファを渡すこと。
// 例: uint16 rpm = 1500; Com_SendSignal(SIGNAL_ENGINE_SPEED, (uint8*)&rpm);
// -------------------------------------------------------
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr)
{
    // --- 1. 初期化・NULL チェック ---
    if (Com_ConfigPtr == nullptr || SignalDataPtr == nullptr)
    {
        return E_NOT_OK;
    }

    // --- 2. SignalId でシグナルテーブルを線形探索 ---
    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
        {
            continue;
        }

        // --- 3. ホストバイトオーダー（little-endian）で入力を読む ---
        const uint32 value = (uint32)SignalDataPtr[0]
                           | ((uint32)SignalDataPtr[1] <<  8)
                           | ((uint32)SignalDataPtr[2] << 16)
                           | ((uint32)SignalDataPtr[3] << 24);

        // --- 4. TX バッファへビットパック（他シグナルは上書きしない）---
        Com_PackSignal(
            Com_TxBuffer[sig->IPduId],
            sig->BitPosition,
            sig->BitSize,
            sig->Endian,
            value
        );

        return E_OK;
    }

    return E_NOT_OK;
}

// -------------------------------------------------------
// Com_TriggerIPDUSend
// TX バッファを PduR_Transmit に渡して 1 フレームで送信する。
// 複数の Com_SendSignal を呼んでからこれを 1 回呼ぶことで
// 全シグナルを 1 CAN フレームにまとめられる。
// -------------------------------------------------------
Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType IPduId)
{
    // --- 1. 初期化チェック ---
    if (Com_ConfigPtr == nullptr)
    {
        return E_NOT_OK;
    }

    // --- 2. IPduId で TX I-PDU テーブルを検索 ---
    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if (ipdu->IPduId != IPduId)
        {
            continue;
        }

        // --- 3. 送信内容をログ出力 ---
        Serial.print("[Com_TriggerIPDUSend] IPduId=");
        Serial.print(IPduId);
        Serial.print(" PduRId=");
        Serial.print(ipdu->PduRId);
        Serial.print(" data=[");
        for (uint8 b = 0; b < ipdu->DLC; b++)
        {
            if (b > 0) { Serial.print(" "); }
            if (Com_TxBuffer[IPduId][b] < 0x10) { Serial.print("0"); }
            Serial.print(Com_TxBuffer[IPduId][b], HEX);
        }
        Serial.println("]");

        // --- 4. PduR_Transmit で送信 ---
        PduInfoType pduInfo = {
            .SduDataPtr = Com_TxBuffer[IPduId],
            .SduLength  = ipdu->DLC
        };
        return PduR_Transmit(ipdu->PduRId, &pduInfo);
    }

    Serial.print("[Com_TriggerIPDUSend] no TX I-PDU for IPduId=");
    Serial.println(IPduId);
    return E_NOT_OK;
}

void Com_TxConfirmation(PduIdType PduId)
{
    Serial.print("[Com_TxConfirmation] PduId=");
    Serial.print(PduId);
    Serial.println(" : TX complete");
}

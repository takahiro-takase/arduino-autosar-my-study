/**
 * \file    Com.c
 * \brief   通信マネージャ (AUTOSAR SWS_COM 準拠)
 * \details シグナルベースの通信を行う AUTOSAR COM API 層を実装する。
 *          RX/TX I-PDU バッファを管理し、設定可能なビットエンディアン
 *          (Motorola/Intel) でシグナルのパック・アンパックを行う。
 *          AUTOSAR 4.3.1 SWS_COM 仕様に準拠し、Arduino UNO 向けに
 *          バッファ数固定・締め切り監視なし・更新ビットなしに簡略化している。
 */

#include "Com.h"
#include "PduR.h"
#include "Det.h"

#define COM_IPDU_MAX_DLC  8U
#define COM_RX_IPDU_MAX   1U
#define COM_TX_IPDU_MAX   1U

static const Com_ConfigType* Com_ConfigPtr = NULL;
static uint8 Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8 Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

/**
 * \brief   COM モジュールを初期化し、すべての I-PDU バッファをクリアする。
 *
 * \details 設定ポインタを保存し、RX/TX の全 I-PDU バッファをゼロクリアして
 *          シグナル設定テーブルをログ出力する (AUTOSAR SWS_Com_00864)。
 *          RX または TX の I-PDU 数がコンパイル時上限を超える場合は
 *          初期化を中断する。
 *
 * \param[in]  config  COM 設定構造体へのポインタ。NULL 禁止。
 *
 * \pre        PduR_Init() が正常に完了していること。
 * \note       COM_RX_IPDU_MAX / COM_TX_IPDU_MAX はコンパイル時定数で 1 に固定。
 *             それを超える設定は拒否される。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_Init(const Com_ConfigType* config)
{
    if (config == NULL)
    {
        Det_LogP(PSTR("[Com_Init] E: config NULL"));
        return;
    }
    if (config->RxIPduCount > COM_RX_IPDU_MAX)
    {
        Det_LogP(PSTR("[Com_Init] E: RxIPduCount>max"));
        return;
    }
    if (config->TxIPduCount > COM_TX_IPDU_MAX)
    {
        Det_LogP(PSTR("[Com_Init] E: TxIPduCount>max"));
        return;
    }

    Com_ConfigPtr = config;

    for (uint8 i = 0; i < COM_RX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_RxBuffer[i][j] = 0U;

    for (uint8 i = 0; i < COM_TX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_TxBuffer[i][j] = 0U;

    Det_LogP(PSTR("[Com_Init] OK"));

    Det_PrintP(PSTR("  RxIPdus="));
    Det_PrintDec(config->RxIPduCount);
    Det_PrintP(PSTR(" TxIPdus="));
    Det_PrintDec(config->TxIPduCount);
    Det_PrintP(PSTR(" Signals="));
    Det_PrintDec(config->SignalCount);
    Det_Newline();

    for (uint8 s = 0; s < config->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &config->Signals[s];
        Det_PrintP(PSTR("  Sig["));
        Det_PrintDec(sig->SignalId);
        Det_PrintP(PSTR("] IPdu="));
        Det_PrintDec(sig->IPduId);
        Det_PrintP(PSTR(" Bit="));
        Det_PrintDec(sig->BitPosition);
        Det_PrintP(PSTR("/"));
        Det_PrintDec(sig->BitSize);
        Det_PrintP(PSTR(" "));
        Det_LogP(sig->Endian == COM_BIG_ENDIAN ? PSTR("BIG") : PSTR("LITTLE"));
    }
}

/**
 * \brief   受信した I-PDU ペイロードを内部 RX バッファへコピーする。
 *
 * \details PduR がフレームを受信した際に呼び出される。
 *          RxPduId（PduR 名前空間）に一致する RX I-PDU エントリを検索し、
 *          ペイロードを対応する RX バッファスロットへコピーしてログ出力する
 *          (AUTOSAR SWS_Com_00442)。
 *          この呼び出し後、Com_ReceiveSignal() でシグナル値を取得できる。
 *
 * \param[in]  RxPduId     受信 I-PDU の PduR 層 PDU ID。
 *                         Com_IPduConfigType エントリの検索に使用する。
 * \param[in]  PduInfoPtr  受信 PDU のデータと長さへのポインタ。NULL 禁止。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (Com_ConfigPtr == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->PduRId != RxPduId)
            continue;

        const uint8 copyLen = (PduInfoPtr->SduLength < ipdu->DLC)
                              ? (uint8)PduInfoPtr->SduLength : ipdu->DLC;

        for (uint8 b = 0; b < copyLen; b++)
            Com_RxBuffer[ipdu->IPduId][b] = PduInfoPtr->SduDataPtr[b];

        Det_PrintP(PSTR("[Com_RxInd] IPdu="));
        Det_PrintDec(ipdu->IPduId);
        Det_PrintP(PSTR(" raw=["));
        for (uint8 b = 0; b < copyLen; b++)
        {
            if (b > 0U) Det_Print(" ");
            if (Com_RxBuffer[ipdu->IPduId][b] < 0x10U) Det_Print("0");
            Det_PrintHex(Com_RxBuffer[ipdu->IPduId][b]);
        }
        Det_LogP(PSTR("]"));
        return;
    }

    Det_PrintP(PSTR("[Com_RxInd] no IPdu PduRId="));
    Det_PrintDec(RxPduId);
    Det_Newline();
}

/* -----------------------------------------------------------------------
 * 内部ヘルパー — AUTOSAR COM 公開 API の範囲外
 * ----------------------------------------------------------------------- */

/**
 * \brief   ネットワークビット順でバイトバッファからビットフィールドを取り出す。
 *
 * \details ビット番号の定義: bit 0 = byte[0] の MSB、bit 7 = byte[0] の LSB、
 *          bit 8 = byte[1] の MSB、...（ネットワーク / Motorola 順）。
 *          COM_BIG_ENDIAN では最初に読んだビットが結果の MSB になり、
 *          COM_LITTLE_ENDIAN では最初に読んだビットが LSB になる。
 *
 * \param[in]  buf      読み取り元バイトバッファ。
 * \param[in]  bitPos   開始ビット位置（ネットワークビット順）。
 * \param[in]  bitSize  取り出すビット数（1〜32）。
 * \param[in]  endian   ビット重みの方向 (COM_BIG_ENDIAN / COM_LITTLE_ENDIAN)。
 *
 * \return  アンパックしたシグナル値（uint32）。
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static uint32 Com_UnpackSignal(const uint8* buf,
                                uint8 bitPos,
                                uint8 bitSize,
                                Com_SignalEndianType endian)
{
    uint32 value = 0U;
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 pos = bitPos + i;
        const uint8 bit = (buf[pos / 8U] >> (7U - (pos % 8U))) & 1U;
        if (endian == COM_BIG_ENDIAN)
            value = (value << 1U) | bit;
        else
            value |= ((uint32)bit << i);
    }
    return value;
}

/**
 * \brief   ネットワークビット順でバイトバッファのビットフィールドに値を書き込む。
 *
 * \details Com_UnpackSignal() と同じネットワークビット番号定義に従い、
 *          bitPos から bitSize ビット分の value を buf へ書き込む。
 *          対象ビット以外の buf の内容は保持される。
 *
 * \param[in,out] buf      書き込み先バイトバッファ。
 * \param[in]     bitPos   開始ビット位置（ネットワークビット順）。
 * \param[in]     bitSize  書き込むビット数（1〜32）。
 * \param[in]     endian   ビット重みの方向 (COM_BIG_ENDIAN / COM_LITTLE_ENDIAN)。
 * \param[in]     value    パックするシグナル値。下位 bitSize ビットのみ使用する。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_PackSignal(uint8* buf,
                            uint8 bitPos,
                            uint8 bitSize,
                            Com_SignalEndianType endian,
                            uint32 value)
{
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 bit   = (endian == COM_BIG_ENDIAN)
                            ? (uint8)((value >> (bitSize - 1U - i)) & 1U)
                            : (uint8)((value >> i) & 1U);
        const uint8 pos   = bitPos + i;
        const uint8 shift = 7U - (pos % 8U);
        if (bit)
            buf[pos / 8U] |=  (uint8)(1U << shift);
        else
            buf[pos / 8U] &= (uint8)~(1U << shift);
    }
}

/**
 * \brief   RX I-PDU バッファからシグナル値を取り出す。
 *
 * \details シグナル設定テーブルの SignalId に一致するエントリを検索し、
 *          ビット位置・サイズ・エンディアンに従って内部 RX バッファから
 *          アンパックする (AUTOSAR SWS_Com_00194)。
 *          アンパックした値は BitSize にかかわらず、常に 4 バイトの
 *          リトルエンディアン整数として SignalDataPtr へ書き込む。
 *
 * \param[in]  SignalId      読み取るシグナルの ID。
 *                           シグナル設定テーブルのエントリと一致すること。
 * \param[out] SignalDataPtr 出力バッファへのポインタ。4 バイト以上必要。
 *                           リトルエンディアン uint32 として書き込まれる。
 *                           NULL 禁止。
 *
 * \retval  E_OK      シグナルが見つかり、SignalDataPtr へ値を書き込んだ。
 * \retval  E_NOT_OK  COM 未初期化、SignalDataPtr が NULL、
 *                    またはシグナル設定テーブルに SignalId が存在しない。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        このシグナルが属する I-PDU で Com_RxIndication() が
 *             少なくとも 1 回呼ばれていること。
 * \note       戻り値型は SWS_Com_00194 に従い uint8。
 *             E_OK / E_NOT_OK の値（0x00 / 0x01）は RTE が使う
 *             Std_ReturnType と互換性がある。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    uint8* dataPtr = (uint8*)SignalDataPtr;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

        const uint32 value = Com_UnpackSignal(
            Com_RxBuffer[sig->IPduId],
            sig->BitPosition, sig->BitSize, sig->Endian);

        dataPtr[0] = (uint8)(value);
        dataPtr[1] = (uint8)(value >>  8U);
        dataPtr[2] = (uint8)(value >> 16U);
        dataPtr[3] = (uint8)(value >> 24U);
        return E_OK;
    }
    return E_NOT_OK;
}

/**
 * \brief   TX I-PDU バッファへシグナル値をパックする。
 *
 * \details シグナル設定テーブルの SignalId に一致するエントリを検索し、
 *          ビット位置・サイズ・エンディアンに従って内部 TX バッファへ
 *          パックする (AUTOSAR SWS_Com_00171)。
 *          SignalDataPtr から 4 バイトのリトルエンディアン整数として
 *          値を読み取り、BitSize に関係なく該当ビットのみ書き換える。
 *          I-PDU は即座には送信されない。送信するには
 *          Com_TriggerIPDUSend() を呼び出すこと。
 *
 * \param[in]  SignalId      書き込むシグナルの ID。
 *                           シグナル設定テーブルのエントリと一致すること。
 * \param[in]  SignalDataPtr シグナル値へのポインタ。4 バイト以上で
 *                           リトルエンディアン順。NULL 禁止。
 *
 * \retval  E_OK      シグナルが見つかり、TX バッファへ値をパックした。
 * \retval  E_NOT_OK  COM 未初期化、SignalDataPtr が NULL、
 *                    またはシグナル設定テーブルに SignalId が存在しない。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \note       戻り値型は SWS_Com_00171 に従い uint8。
 *             E_OK / E_NOT_OK の値（0x00 / 0x01）は RTE が使う
 *             Std_ReturnType と互換性がある。
 *
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    const uint8* dataPtr = (const uint8*)SignalDataPtr;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

        const uint32 value = (uint32)dataPtr[0]
                           | ((uint32)dataPtr[1] <<  8U)
                           | ((uint32)dataPtr[2] << 16U)
                           | ((uint32)dataPtr[3] << 24U);

        Com_PackSignal(Com_TxBuffer[sig->IPduId],
                       sig->BitPosition, sig->BitSize, sig->Endian, value);
        return E_OK;
    }
    return E_NOT_OK;
}

/**
 * \brief   TX I-PDU を PduR 経由で即座に送信する。
 *
 * \details PduId で TX I-PDU 設定を検索し、内部 TX バッファを指す
 *          PduInfoType を構築して PduR_Transmit() を呼び出す
 *          (AUTOSAR SWS_Com_00725)。転送前に TX バッファの内容をログ出力する。
 *
 * \param[in]  PduId  送信する I-PDU の COM ハンドル。
 *                    TX I-PDU 設定テーブルの IPduId と一致すること。
 *
 * \retval  E_OK      I-PDU が PduR_Transmit() に正常に転送された。
 * \retval  E_NOT_OK  COM 未初期化、PduId が見つからない、
 *                    または PduR_Transmit() が失敗した。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        送信前に Com_SendSignal() で TX バッファへ値を設定しておくこと。
 *
 * \ServiceID      {0x17}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    if (Com_ConfigPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if ((PduIdType)ipdu->IPduId != PduId)
            continue;

        Det_PrintP(PSTR("[Com_TxSend] IPdu="));
        Det_PrintDec(PduId);
        Det_PrintP(PSTR(" data=["));
        for (uint8 b = 0; b < ipdu->DLC; b++)
        {
            if (b > 0U) Det_Print(" ");
            if (Com_TxBuffer[PduId][b] < 0x10U) Det_Print("0");
            Det_PrintHex(Com_TxBuffer[PduId][b]);
        }
        Det_LogP(PSTR("]"));

        PduInfoType pduInfo = {
            .SduDataPtr = Com_TxBuffer[PduId],
            .SduLength  = ipdu->DLC
        };
        return PduR_Transmit(ipdu->PduRId, &pduInfo);
    }

    Det_PrintP(PSTR("[Com_TxSend] no TX IPdu="));
    Det_PrintDec(PduId);
    Det_Newline();
    return E_NOT_OK;
}

/**
 * \brief   TX I-PDU の送信完了を COM へ通知する。
 *
 * \details CAN フレームの送信完了後に PduR から呼び出される
 *          (AUTOSAR SWS_Com_00695)。
 *          この実装ではログ出力のみ行い、リトライや締め切り処理は行わない。
 *
 * \param[in]  TxPduId  送信が完了した TX I-PDU の PduR 層 PDU ID。
 * \param[in]  result   CanIf から転送された送信結果。
 *                      E_OK = 成功、E_NOT_OK = 失敗。
 *                      TX リトライやエラーカウンタを実装しないため未使用。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \note       result を上位層へ転送しない理由は、SWS_COM 仕様の
 *             Com_TxConfirmation が result 引数を持たないため。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)result;
    Det_PrintP(PSTR("[Com_TxConf] TxPduId="));
    Det_PrintDec(TxPduId);
    Det_Newline();
}

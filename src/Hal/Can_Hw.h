/**
 * \file    Can_Hw.h
 * \brief   Can ハードウェア依存層 内部インタフェース (MCP2515 向け)
 * \details Can.c（純粋 C）と Can_Hw.cpp（mcp_can C++ ラッパー）の境界を定義する。
 *          このヘッダは Can モジュール内部専用であり、上位層から直接参照しない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CAN_HW_H
#define CAN_HW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CAN_HW_OK   = 0,
    CAN_HW_FAIL = 1
} Can_Hw_ReturnType;

typedef enum
{
    CAN_HW_MODE_NORMAL      = 0, /* 通常動作（TX/RX 可）        */
    CAN_HW_MODE_LISTEN_ONLY = 1, /* 受信専用（TX 不可）         */
    CAN_HW_MODE_SLEEP       = 2, /* 低電力スリープ（バス活動でのウェイクアップ割込み有効） */
    CAN_HW_MODE_LOOPBACK    = 3  /* 内部ループバック（テスト用） */
} Can_Hw_Mode;

Can_Hw_ReturnType Can_Hw_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz);
Can_Hw_ReturnType Can_Hw_Send(uint32_t id, uint8_t dlc, const uint8_t* data);
Can_Hw_ReturnType Can_Hw_Read(uint32_t* id, uint8_t* dlc, uint8_t* data);
Can_Hw_ReturnType Can_Hw_InitMask(uint8_t num, uint8_t ext, uint32_t mask);
Can_Hw_ReturnType Can_Hw_InitFilter(uint8_t num, uint8_t ext, uint32_t filter);
Can_Hw_ReturnType Can_Hw_SetMode(Can_Hw_Mode mode);
Can_Hw_ReturnType Can_Hw_CheckReceive(void);
Can_Hw_ReturnType Can_Hw_IsBusOff(void);

/** MCP2515 の EFLG レジスタから、Bus-Off/Error-Passive/Active の3状態を
 *  導出して *errorStateOut に格納する（0=Active, 1=Passive, 2=Bus-Off。
 *  Can_GeneralTypes.h の Can_ErrorStateType と同じ数値のため、呼び出し元は
 *  そのままキャストして使ってよい）。このヘッダは AUTOSAR 型に依存しない
 *  方針（Can_Hw.h 冒頭コメント参照）のため、値そのものは uint8_t で返す。
 *  \retval  CAN_HW_FAIL  ドライバ未初期化。*errorStateOut は変更しない。 */
Can_Hw_ReturnType Can_Hw_GetErrorState(uint8_t* errorStateOut);

/** SLEEP 中の INT ピン（アクティブ LOW）を直接ポーリングし、ウェイクアップ
 *  要因がまだアサートされているかを返す。Can_Hw_AttachRxIsr() が登録した
 *  ピン番号を内部で覚えているため、呼び出し側はピン番号を意識しない。
 *  \retval  CAN_HW_OK    INT ピンが LOW（ウェイクアップ要因がアサート中）
 *  \retval  CAN_HW_FAIL  INT ピンが HIGH（アサートなし） */
Can_Hw_ReturnType Can_Hw_IsWakeupPending(void);

/** MCP2515 の INT ピンを立ち下がりエッジのハードウェア割り込みとして登録する。
 *  isr はこの登録以降、真の割り込みコンテキストで呼ばれる。 */
Can_Hw_ReturnType Can_Hw_AttachRxIsr(uint8_t intPin, void (*isr)(void));

/** Can_Hw_AttachRxIsr() が登録した INT ピンの割り込みを一時的に切り離す
 *  （`detachInterrupt()`）。MCP2515 自体はバス活動の受信を継続するため、
 *  Can_MainFunction_Read() 等のポーリング経路には影響しない
 *  （Can_DisableControllerInterrupts() が本関数を使う理由は Can.h 参照）。 */
Can_Hw_ReturnType Can_Hw_DisableRxIsr(void);

/** Can_Hw_DisableRxIsr() で切り離した INT ピンの割り込みを、
 *  Can_Hw_AttachRxIsr() が記憶している ISR で再登録する。 */
Can_Hw_ReturnType Can_Hw_EnableRxIsr(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_HW_H */

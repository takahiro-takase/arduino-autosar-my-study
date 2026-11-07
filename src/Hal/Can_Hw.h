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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <stdint.h>

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

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

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/**
 * \brief   CAN FD の設定を渡す（Can_Hw_Init() の前に呼ぶ）。
 * \details fdEnabled=0 ならクラシック CAN 2.0（最大 8 byte）。fdEnabled=1 なら、8 byte を超える長さを
 *          CAN FD フレームで送受信する。fdBaudrate はデータ部のビットレート [bit/s]（通常のビットレートの整数倍）、
 *          bitRateSwitch=1 のとき FD フレームの送信でデータ部のビットレートを切り替える（BRS）。
 *          CAN FD に対応しない実装は、fdEnabled=1 を渡されたら CAN_HW_FAIL を返す。
 */
Can_Hw_ReturnType Can_Hw_ConfigureFd(uint8_t fdEnabled, uint32_t fdBaudrate, uint8_t bitRateSwitch);
Can_Hw_ReturnType Can_Hw_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz);
Can_Hw_ReturnType Can_Hw_Send(uint32_t id, uint8_t dlc, const uint8_t* data);
Can_Hw_ReturnType Can_Hw_Read(uint32_t* id, uint8_t* dlc, uint8_t* data);
Can_Hw_ReturnType Can_Hw_InitMask(uint8_t num, uint8_t ext, uint32_t mask);
Can_Hw_ReturnType Can_Hw_InitFilter(uint8_t num, uint8_t ext, uint32_t filter);
Can_Hw_ReturnType Can_Hw_SetMode(Can_Hw_Mode mode);
Can_Hw_ReturnType Can_Hw_CheckReceive(void);
Can_Hw_ReturnType Can_Hw_IsBusOff(void);

/** MCP2515 の EFLG レジスタの受信オーバーフロー（RX0OVR/RX1OVR）を確認し、立っていれば
 *  フラグを消して知らせる（読んで消す）。受信バッファが埋まっているところへ新しいフレームが届き、
 *  取りこぼした（上書きされた、または捨てられた）ことを表す。フラグは MCU が消さないと立ったままになる。
 *  mcp_can ライブラリにはレジスタを書く公開 API が無いため、BIT MODIFY を SPI で直接送って消す
 *  （Can_Hw_Init() の csPin を覚えている）。
 *  \retval  CAN_HW_OK    受信オーバーフローがあった（フラグを消した）
 *  \retval  CAN_HW_FAIL  オーバーフローなし、またはドライバ未初期化 */
Can_Hw_ReturnType Can_Hw_TakeRxOverflow(void);

/** MCP2515 の送信バッファ（TXB0〜TXB2）に残っている、未送信のフレームを取り消す（TXREQ を消す）。
 *  Can_Hw_Send() が失敗（ACK が返らない、バッファが空かないなど）したとき、フレームは送信バッファに残り続け、
 *  バスの状況が変わると、あとから送出される（古いフレームが遅れて出る）。また、3 つのバッファがすべて残ると、
 *  以降の送信が常に失敗する。「失敗＝送られていない」にするため、失敗のあとに呼ぶ。
 *  mcp_can の abortTX() は、ABAT ビットを立てるだけで、消す公開 API が無く、以降の送信を止めたままにしてしまうため使わず、
 *  各 TXBnCTRL の TXREQ だけを BIT MODIFY で消す（SPI を直接使う。Can_Hw_TakeRxOverflow() と同じ手順）。
 *  \retval  CAN_HW_OK    取り消しを送った
 *  \retval  CAN_HW_FAIL  ドライバ未初期化 */
Can_Hw_ReturnType Can_Hw_AbortTx(void);

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

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* CAN_HW_H */

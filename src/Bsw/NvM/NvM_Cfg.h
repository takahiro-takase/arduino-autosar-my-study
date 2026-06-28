/**
 * \file    NvM_Cfg.h
 * \brief   NvM プリコンパイル設定 (AUTOSAR SWS_NvM 準拠)
 * \details NvM が管理するブロックの ID・EEPROM アドレス・サイズを定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 *          本プロジェクトの EEPROM レイアウト (Arduino UNO 内蔵 1KB の先頭 29 バイト):
 *          各ブロックはデータ本体直後に CRC8 (SAE J1850) を 1 バイト付加する
 *          (NvM.c の NvM_CalcCrc8() / NvM_CrcAddress() 参照)。
 *            Addr 0x0000: NVM_BLOCK_ID_DEM_MAGIC    データ (1 byte) — DEM 有効マーカー
 *            Addr 0x0001: NVM_BLOCK_ID_DEM_MAGIC    CRC   (1 byte)
 *            Addr 0x0002: NVM_BLOCK_ID_DEM_STATUS   データ (8 bytes) — DEM イベントステータス
 *            Addr 0x000A: NVM_BLOCK_ID_DEM_STATUS   CRC   (1 byte)
 *            Addr 0x000B: NVM_BLOCK_ID_DEM_AGING    データ (8 bytes) — DEM 経年回復(Aging)カウンタ
 *            Addr 0x0013: NVM_BLOCK_ID_DEM_AGING    CRC   (1 byte)
 *            Addr 0x0014: NVM_BLOCK_ID_DEM_EXTENDED データ (8 bytes) — DEM 故障確定回数(ExtendedData)
 *            Addr 0x001C: NVM_BLOCK_ID_DEM_EXTENDED CRC   (1 byte)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NVM_CFG_H
#define NVM_CFG_H

/* -----------------------------------------------------------------------
 * ブロック ID
 * NvM_ReadBlock() / NvM_WriteBlock() の第 1 引数に渡す。
 * ----------------------------------------------------------------------- */
#define NVM_BLOCK_ID_DEM_MAGIC    0U  /**< DEM 有効マーカー (1 byte)         */
#define NVM_BLOCK_ID_DEM_STATUS   1U  /**< DEM イベントステータス (8 bytes)   */
#define NVM_BLOCK_ID_DEM_AGING    2U  /**< DEM 経年回復(Aging)カウンタ (8 bytes) */
#define NVM_BLOCK_ID_DEM_EXTENDED 3U  /**< DEM 故障確定回数 ExtendedData (8 bytes) */
#define NVM_BLOCK_COUNT           4U  /**< 管理ブロック総数                   */

/* -----------------------------------------------------------------------
 * EEPROM 先頭アドレス (各ブロックの物理格納先)
 * ----------------------------------------------------------------------- */
/* 各ブロックのデータ本体直後 (BaseNumber + Length) に CRC 1 バイトが入るため、
 * 後続ブロックの先頭アドレスは前ブロックの (データ長+1) ずつ後ろにずれる。 */
#define NVM_BLOCK_DEM_MAGIC_EEPROM_ADDR    0x0000U  /**< マジックバイトのアドレス (CRC は 0x0001) */
#define NVM_BLOCK_DEM_STATUS_EEPROM_ADDR   0x0002U  /**< ステータステーブルの先頭アドレス (CRC は 0x000A) */
#define NVM_BLOCK_DEM_AGING_EEPROM_ADDR    0x000BU  /**< Aging カウンタの先頭アドレス (CRC は 0x0013) */
#define NVM_BLOCK_DEM_EXTENDED_EEPROM_ADDR 0x0014U  /**< 故障確定回数の先頭アドレス (CRC は 0x001C) */

/* -----------------------------------------------------------------------
 * ブロックサイズ (bytes)
 * ----------------------------------------------------------------------- */
#define NVM_BLOCK_DEM_MAGIC_LENGTH     1U  /**< マジックバイト: 1 byte           */
#define NVM_BLOCK_DEM_STATUS_LENGTH    8U  /**< DEM_EVENT_COUNT = 8 イベント分   */
#define NVM_BLOCK_DEM_AGING_LENGTH     8U  /**< DEM_EVENT_COUNT = 8 イベント分   */
#define NVM_BLOCK_DEM_EXTENDED_LENGTH  8U  /**< DEM_EVENT_COUNT = 8 イベント分   */

#endif /* NVM_CFG_H */

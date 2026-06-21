/**
 * \file    NvM_Cfg.h
 * \brief   NvM プリコンパイル設定 (AUTOSAR SWS_NvM 準拠)
 * \details NvM が管理するブロックの ID・EEPROM アドレス・サイズを定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 *          本プロジェクトの EEPROM レイアウト (Arduino UNO 内蔵 1KB の先頭 9 バイト):
 *            Addr 0x0000: NVM_BLOCK_ID_DEM_MAGIC  (1 byte)  — DEM 有効マーカー
 *            Addr 0x0001: NVM_BLOCK_ID_DEM_STATUS (8 bytes) — DEM イベントステータス
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
#define NVM_BLOCK_COUNT           2U  /**< 管理ブロック総数                   */

/* -----------------------------------------------------------------------
 * EEPROM 先頭アドレス (各ブロックの物理格納先)
 * ----------------------------------------------------------------------- */
#define NVM_BLOCK_DEM_MAGIC_EEPROM_ADDR   0x0000U  /**< マジックバイトのアドレス      */
#define NVM_BLOCK_DEM_STATUS_EEPROM_ADDR  0x0001U  /**< ステータステーブルの先頭アドレス */

/* -----------------------------------------------------------------------
 * ブロックサイズ (bytes)
 * ----------------------------------------------------------------------- */
#define NVM_BLOCK_DEM_MAGIC_LENGTH    1U  /**< マジックバイト: 1 byte           */
#define NVM_BLOCK_DEM_STATUS_LENGTH   8U  /**< DEM_EVENT_COUNT = 8 イベント分   */

#endif /* NVM_CFG_H */

/**
 * \file    NvM_Cfg.h
 * \brief   NvM プリコンパイル設定 (AUTOSAR SWS_NvM 準拠)
 * \details NvM が管理するブロックの ID・EEPROM アドレス・サイズを定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 *          本プロジェクトの EEPROM レイアウト (Arduino UNO 内蔵 1KB の先頭 46 バイト):
 *          各ブロックはデータ本体直後に CRC8 (SAE J1850) を 1 バイト付加する
 *          (NvM.c の NvM_CalcCrc8() / NvM_CrcAddressForBase() 参照)。
 *            Addr 0x0000: NVM_BLOCK_ID_DEM_MAGIC    データ (1 byte) — DEM 有効マーカー
 *            Addr 0x0001: NVM_BLOCK_ID_DEM_MAGIC    CRC   (1 byte)
 *            Addr 0x0002: NVM_BLOCK_ID_DEM_STATUS   データ (10 bytes) — DEM イベントステータス
 *            Addr 0x000C: NVM_BLOCK_ID_DEM_STATUS   CRC   (1 byte)
 *            Addr 0x000D: NVM_BLOCK_ID_DEM_AGING    データ (10 bytes) — DEM 経年回復(Aging)カウンタ
 *            Addr 0x0017: NVM_BLOCK_ID_DEM_AGING    CRC   (1 byte)
 *            Addr 0x0018: NVM_BLOCK_ID_DEM_EXTENDED データ (10 bytes) — DEM 故障確定回数(ExtendedData)
 *                         プライマリ面
 *            Addr 0x0022: NVM_BLOCK_ID_DEM_EXTENDED CRC   (1 byte) — プライマリ面
 *            Addr 0x0023: NVM_BLOCK_ID_DEM_EXTENDED データ (10 bytes) — ミラー面
 *                         （冗長ブロック。詳細は NvM.h の「冗長ブロック」参照）
 *            Addr 0x002D: NVM_BLOCK_ID_DEM_EXTENDED CRC   (1 byte) — ミラー面
 *
 *          DEM_EXTENDED（故障確定回数、UDS SID 0x19/06 で読み出せる車両生涯の
 *          累積値）のみ冗長ブロック化している。1 バイトの書き込み不良で
 *          この累積履歴を丸ごと失う（ROM デフォルト＝全 0 へ復元される）ことを
 *          避けるため。MAGIC/STATUS/AGING は従来どおりシングルコピーのまま
 *          （STATUS/AGING は当該操作サイクル・エンジンサイクルの経過で
 *          自然に再構築されていく性質のデータであり、冗長化の投資対効果が
 *          相対的に低いと判断した）。
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
#define NVM_BLOCK_ID_DEM_STATUS   1U  /**< DEM イベントステータス (9 bytes)   */
#define NVM_BLOCK_ID_DEM_AGING    2U  /**< DEM 経年回復(Aging)カウンタ (9 bytes) */
#define NVM_BLOCK_ID_DEM_EXTENDED 3U  /**< DEM 故障確定回数 ExtendedData (9 bytes) */
#define NVM_BLOCK_COUNT           4U  /**< 管理ブロック総数                   */

/* -----------------------------------------------------------------------
 * EEPROM 先頭アドレス (各ブロックの物理格納先)
 * ----------------------------------------------------------------------- */
/* 各ブロックのデータ本体直後 (BaseNumber + Length) に CRC 1 バイトが入るため、
 * 後続ブロックの先頭アドレスは前ブロックの (データ長+1) ずつ後ろにずれる。 */
#define NVM_BLOCK_DEM_MAGIC_EEPROM_ADDR    0x0000U  /**< マジックバイトのアドレス (CRC は 0x0001) */
#define NVM_BLOCK_DEM_STATUS_EEPROM_ADDR   0x0002U  /**< ステータステーブルの先頭アドレス (CRC は 0x000C) */
#define NVM_BLOCK_DEM_AGING_EEPROM_ADDR    0x000DU  /**< Aging カウンタの先頭アドレス (CRC は 0x0017) */
#define NVM_BLOCK_DEM_EXTENDED_EEPROM_ADDR 0x0018U  /**< 故障確定回数の先頭アドレス・プライマリ面 (CRC は 0x0022) */
#define NVM_BLOCK_DEM_EXTENDED_MIRROR_EEPROM_ADDR 0x0023U  /**< 故障確定回数のミラー面 (CRC は 0x002D)。
                                                             *   冗長ブロック（NvM_PBCfg.c の .Redundant=1）専用 */

/* -----------------------------------------------------------------------
 * ブロックサイズ (bytes)
 * ----------------------------------------------------------------------- */
#define NVM_BLOCK_DEM_MAGIC_LENGTH     1U   /**< マジックバイト: 1 byte           */
#define NVM_BLOCK_DEM_STATUS_LENGTH    10U  /**< DEM_EVENT_COUNT = 10 イベント分  */
#define NVM_BLOCK_DEM_AGING_LENGTH     10U  /**< DEM_EVENT_COUNT = 10 イベント分  */
#define NVM_BLOCK_DEM_EXTENDED_LENGTH  10U  /**< DEM_EVENT_COUNT = 10 イベント分  */

/** 設定済みブロックの中で最大の NvMNvBlockLength。
 *  冗長ブロックの読み込み検証（NvM.c の NvM_LoadAndVerifyBlock()）が
 *  ミラー面の内容を一時的に保持するスタック上のスクラッチバッファの
 *  サイズとして使う。新しいブロックを追加してこれより大きくする場合は
 *  あわせて更新すること。 */
#define NVM_MAX_BLOCK_LENGTH  10U

#endif /* NVM_CFG_H */

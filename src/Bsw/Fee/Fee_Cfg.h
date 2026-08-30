/**
 * \file    Fee_Cfg.h
 * \brief   Flash EEPROM Emulation プリコンパイル設定 (AUTOSAR SWS_Fee 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef FEE_CFG_H
#define FEE_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Flash EEPROM Emulation (Fee) に割り当てられた固定値 21 を使う。
 *
 * 開発エラーコード・ApiId は docs/4.3.1/AUTOSAR_SWS_FlashEEPROMEmulation.pdf
 * ([SWS_Fee_00010] 7.2.1 Development Errors 表、8 章 Service ID[hex]) を
 * 実測して確認済み。本実装が持たない API（Fee_InvalidateBlock/
 * Fee_EraseImmediateBlock/Fee_JobEndNotification/Fee_JobErrorNotification、
 * いずれも学習用簡略化のため未実装。BlockNumber テーブルを持たない設計上の
 * 制約は Fee.h 冒頭のコメント参照）に対応するエラーコードは定義しない。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Flash EEPROM Emulation の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 21） */
#define FEE_MODULE_ID  21U

/** 開発エラーコード（[SWS_Fee_00010] 7.2.1/7.2.2 表より、実際に使用する分のみ）
 *  FEE_E_INVALID_BLOCK_LEN は本来 BlockNumber/BlockOffset を持つ Fee_Read/
 *  Fee_Write 向けの「オフセット+長さがブロック範囲を超える」検査用だが、
 *  本実装は BlockNumber テーブルを持たず Length のみを引数に取るため
 *  （Fee.h 冒頭のコメント参照）、単純な Length==0 検査にのみ流用する。 */
#define FEE_E_UNINIT             0x01U  /**< 未初期化時の API 呼び出し */
#define FEE_E_PARAM_POINTER      0x04U  /**< NULL ポインタ            */
#define FEE_E_INVALID_BLOCK_LEN  0x05U  /**< Length=0                 */
#define FEE_E_BUSY               0x06U  /**< 既にジョブ処理中に新規 Write を要求（Runtime Error） */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。実装している API は
 *  [SWS_Fee_00084]〜[SWS_Fee_00191] の Service ID[hex] と一致させた。
 *  Fee_WriteImmediate は AUTOSAR 非標準（実 AUTOSAR では FeeImmediateData
 *  ブロック属性として Fee_Write に統合される。Fee.h 冒頭のコメント参照）の
 *  ため、実仕様が 0x00〜0x09 を使い切った直後の未使用値 0x0A を割り当てる。 */
#define FEE_API_ID_INIT              0x00U
#define FEE_API_ID_SET_MODE           0x01U
#define FEE_API_ID_READ               0x02U
#define FEE_API_ID_WRITE              0x03U
#define FEE_API_ID_CANCEL             0x04U
#define FEE_API_ID_GET_STATUS         0x05U
#define FEE_API_ID_GET_JOB_RESULT     0x06U
#define FEE_API_ID_GET_VERSION_INFO   0x08U
#define FEE_API_ID_MAIN_FUNCTION      0x12U
#define FEE_API_ID_WRITE_IMMEDIATE    0x0AU  /**< AUTOSAR 非標準（Fee.h 参照） */

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define FEE_VENDOR_ID          0U
#define FEE_SW_MAJOR_VERSION   1U
#define FEE_SW_MINOR_VERSION   0U
#define FEE_SW_PATCH_VERSION   0U

#endif /* FEE_CFG_H */

/**
 * \file    NvM.h
 * \brief   Non-Volatile Memory Manager 公開インタフェース (AUTOSAR SWS_NvM 準拠)
 * \details BSW モジュールが EEPROM の読み書きを直接行わず、
 *          NvM の抽象化 API を介してアクセスするためのインタフェースを公開する。
 *
 *          本実装の設計方針:
 *            - 各ブロックは NvM が管理する RAM ミラーを持つ。
 *            - NvM_Init() で全ブロックの EEPROM 内容を RAM ミラーへ展開する。
 *            - NvM_ReadBlock() は RAM ミラーから呼び出し元バッファへコピーする。
 *            - NvM_WriteBlock() は呼び出し元バッファを RAM ミラーへコピーし、
 *              変化バイトのみ EEPROM へ書き戻す。
 *            - 各ブロックのデータ本体直後に AUTOSAR Crc8 (SAE J1850) の CRC を
 *              1 バイト付加して保存する。NvM_Init() で検証し、不一致なら
 *              ROM デフォルト値（未設定なら全 0）へ自動復元する
 *              (NvM_RestoreBlockDefaults() からも明示的に呼べる)。
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
 *            - 非同期ジョブキューなし (同期処理のみ)
 *            - NvM_MainFunction / コールバック通知なし
 *            - 冗長ブロック（ミラー/2重化）なし。CRC による破損検出と
 *              デフォルト復元のみ対応
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NVM_H
#define NVM_H

#include "Std_Types.h"
#include "NvM_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */

/** ブロック ID 型 (NVM_BLOCK_ID_* 定数を渡す) */
typedef uint8 NvM_BlockIdType;

/**
 * \brief   ブロック記述子 — 1 ブロックの物理・論理属性を保持する。
 * \details AUTOSAR の NvMBlockDescriptor に相当する。
 *          コンフィギュレーションツールが NvM_PBCfg.c に生成するテーブルの要素型。
 */
typedef struct
{
    uint16      NvMNvBlockBaseNumber;  /**< EEPROM 先頭アドレス（データ本体）。
                                         *   CRC 1 バイトはこの直後
                                         *   (NvMNvBlockBaseNumber + NvMNvBlockLength)
                                         *   に保存する。                            */
    uint16      NvMNvBlockLength;      /**< データ本体サイズ (bytes、CRC バイトは含まない) */
    void*       RamBlockDataAddress;   /**< RAM ミラーポインタ (NvM が管理)              */
    const void* RomBlockDataAddress;   /**< CRC 不一致時に復元する ROM デフォルト値。
                                         *   NULL の場合は全 0 で代替する
                                         *   (AUTOSAR の NvMBlockDescriptor と同様、
                                         *   デフォルト未設定のブロックを許容する)。      */
} NvM_BlockDescriptorType;

/**
 * \brief   NvM ポストビルドコンフィグ型
 * \details NvM_Init() に渡すコンフィグ構造体。
 *          NvM_PBCfg.c でインスタンス化される。
 */
typedef struct
{
    const NvM_BlockDescriptorType*  Blocks;     /**< ブロック記述子配列の先頭   */
    uint8                           NumBlocks;  /**< 管理ブロック数             */
} NvM_ConfigType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   NvM を初期化し、全ブロックの EEPROM 内容を RAM ミラーへ展開する。
 * \details EcuM_Init() から最初期 (Can_Init より前) に呼び出すこと。
 *          以降、NvM_ReadBlock() / NvM_WriteBlock() が使用可能になる。
 *          各ブロックは読み込み直後に CRC を検証する。EEPROM のビット化けや
 *          書き込み中の電源断などで保存値が壊れていた場合、自動的に
 *          NvM_RestoreBlockDefaults() と同等の処理（ROM デフォルト値、
 *          未設定なら全 0 へ復元し EEPROM へ書き戻す）を行う。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void NvM_Init(const NvM_ConfigType* ConfigPtr);

/**
 * \brief   指定ブロックの RAM ミラー内容を NvM_DstPtr へコピーする。
 * \details NvM_Init() 完了後に RAM ミラーは最新 EEPROM 値を保持している。
 *          EEPROM への追加アクセスは発生しない。
 *
 * \param[in]  BlockId      ブロック ID (NVM_BLOCK_ID_* 定数)。
 * \param[out] NvM_DstPtr   データのコピー先。NULL 禁止。
 *
 * \retval  E_OK      正常完了。
 * \retval  E_NOT_OK  BlockId が範囲外、または NvM_DstPtr が NULL。
 *
 * \ServiceID      {0x16}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr);

/**
 * \brief   NvM_SrcPtr の内容を RAM ミラーへコピーし、EEPROM へ永続化する。
 * \details eeprom_update_block により変化バイトのみ物理書き込みを行う。
 *          RAM ミラーは常に最新状態に保たれる。書き込んだデータに対する
 *          CRC も併せて再計算し、データ本体直後の 1 バイトへ保存する。
 *
 * \param[in]  BlockId      ブロック ID (NVM_BLOCK_ID_* 定数)。
 * \param[in]  NvM_SrcPtr   書き込みデータの元アドレス。NULL 禁止。
 *
 * \retval  E_OK      正常完了。
 * \retval  E_NOT_OK  BlockId が範囲外、または NvM_SrcPtr が NULL。
 *
 * \ServiceID      {0x0D}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr);

/**
 * \brief   指定ブロックを ROM デフォルト値（未設定なら全 0）へ復元する。
 * \details RAM ミラーへデフォルト値をコピーし、CRC を再計算して
 *          EEPROM へ書き戻す。NvM_Init() が CRC 不一致を検出した際に
 *          内部的に呼ぶのと同じ処理を、明示的に呼び出せるようにする
 *          AUTOSAR の NvM_RestoreBlockDefaults() 相当の API。
 *
 * \param[in]  BlockId  ブロック ID (NVM_BLOCK_ID_* 定数)。
 *
 * \retval  E_OK      正常完了。
 * \retval  E_NOT_OK  BlockId が範囲外。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId);

#ifdef __cplusplus
}
#endif

#endif /* NVM_H */

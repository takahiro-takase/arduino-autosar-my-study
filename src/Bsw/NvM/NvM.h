/**
 * \file    NvM.h
 * \brief   Non-Volatile Memory Manager 公開インタフェース (AUTOSAR SWS_NvM 準拠)
 * \details BSW モジュールが EEPROM の読み書きを直接行わず、
 *          NvM の抽象化 API を介してアクセスするためのインタフェースを公開する。
 *
 *          本実装の設計方針:
 *            - 各ブロックは NvM が管理する RAM ミラーを持つ。
 *            - NvM_Init() で全ブロックの EEPROM 内容を RAM ミラーへ展開する
 *              (起動時、Os スケジューラ開始前のため同期処理のまま)。
 *            - NvM_ReadBlock() は RAM ミラーから呼び出し元バッファへコピーする。
 *            - NvM_WriteBlock() / NvM_RestoreBlockDefaults() は呼び出し元の
 *              データを RAM ミラーへ即座に反映したのち、実際の EEPROM 書き込みは
 *              「ジョブ保留」としてマークするだけで、その場ではブロックしない
 *              (AUTOSAR SWS_NvM_00449 相当の非同期ジョブキュー)。
 *            - NvM_MainFunction() が周期的に、保留中のブロックのデータ本体+CRC を
 *              **1 呼び出しにつき 1 バイトずつ** EEPROM へ書き込む。1 ブロック分
 *              (最大 10 バイト) が完了すると次の保留ブロックへ移る。
 *            - 各ブロックのデータ本体直後に AUTOSAR Crc8 (SAE J1850) の CRC を
 *              1 バイト付加して保存する。NvM_Init() で検証し、不一致なら
 *              ROM デフォルト値（未設定なら全 0）へ自動復元する。
 *
 *          なぜ非同期化したか:
 *            当初 NvM_WriteBlock() は EEPROM への書き込みも含めて同期処理
 *            だった。Renesas RA の EEPROM ライブラリ（フラッシュエミュレーション）
 *            はバイト単位の書き込みでも消去・書き込みサイクルを伴うため、
 *            9 バイト超のブロックを同期的に書くと数百 ms 協調スケジューラが
 *            停止し、他タスク（WdgM の Deadline Supervision 等）を巻き込んで
 *            実際に HW ウォッチドッグリセットを引き起こすことが実機で判明した。
 *            1 呼び出し 1 バイトの非同期ジョブキューへ変更することで、
 *            NvM_WriteBlock() 自体は即座に返り、実際のブロッキング時間は
 *            NvM_MainFunction() 1 回あたり高々 1 バイト分に抑えられる。
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
 *            - 冗長ブロック（ミラー/2重化）なし。CRC による破損検出と
 *              デフォルト復元のみ対応
 *            - ジョブは常に 1 個ずつ、投入順 (FIFO) に順次処理（優先度なし、
 *              複数ジョブの並行処理なし）
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
 * \brief   ブロックの直近のジョブ結果。
 * \details AUTOSAR NvM_RequestResultType (SWS_NvM_00426) の一部に相当する。
 *          本実装で使用するのは以下の 3 値のみ（学習用簡略化）。
 */
typedef enum
{
    NVM_REQ_OK      = 0U,  /**< 直近のジョブが正常完了した                 */
    NVM_REQ_PENDING = 1U,  /**< ジョブがキュー投入済み、まだ完了していない */
    NVM_REQ_NOT_OK  = 2U   /**< BlockId が無効、またはジョブ未実行         */
} NvM_RequestResultType;

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
 * \brief   NvM_SrcPtr の内容を RAM ミラーへ即座に反映し、EEPROM への
 *          書き込みジョブを保留キューへ積む。
 *
 * \details RAM ミラーの更新は同期的（呼び出し直後から NvM_ReadBlock() で
 *          新しい値が読める）。一方、実際の EEPROM 書き込みは
 *          NvM_MainFunction() が非同期に 1 バイトずつ行うため、本関数自体は
 *          即座に返る（AUTOSAR SWS_NvM_00449「WriteBlock はジョブを積むだけ」
 *          に相当）。同じブロックに対する書き込みジョブが既に処理中だった
 *          場合は、進行中のジョブを破棄して最新データから書き直す
 *          （書きかけの古いデータと新しいデータが混在する「ちぎれ書き」を防ぐ）。
 *          完了したかどうかは NvM_GetErrorStatus() で確認できる。
 *
 * \param[in]  BlockId      ブロック ID (NVM_BLOCK_ID_* 定数)。
 * \param[in]  NvM_SrcPtr   書き込みデータの元アドレス。NULL 禁止。
 *
 * \retval  E_OK      ジョブを受け付けた（書き込み完了を意味しない）。
 * \retval  E_NOT_OK  BlockId が範囲外、または NvM_SrcPtr が NULL。
 *
 * \ServiceID      {0x0D}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr);

/**
 * \brief   指定ブロックを ROM デフォルト値（未設定なら全 0）へ復元する。
 *
 * \details RAM ミラーへデフォルト値を即座に反映し、NvM_WriteBlock() と同じ
 *          非同期ジョブキュー経由で EEPROM へ書き戻す。NvM_Init() が
 *          CRC 不一致を検出した際に内部的に行う復元処理
 *          （こちらは起動時のため同期処理のまま）とは異なり、本 API は
 *          実行中に明示的に呼び出すことを想定した
 *          AUTOSAR の NvM_RestoreBlockDefaults() 相当の API。
 *
 * \param[in]  BlockId  ブロック ID (NVM_BLOCK_ID_* 定数)。
 *
 * \retval  E_OK      ジョブを受け付けた（書き込み完了を意味しない）。
 * \retval  E_NOT_OK  BlockId が範囲外。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId);

/**
 * \brief   ブロックの直近のジョブ結果を取得する。
 *
 * \details AUTOSAR NvM_GetErrorStatus() (SWS_NvM_00426) 相当。
 *          NvM_WriteBlock() / NvM_RestoreBlockDefaults() の完了を
 *          明示的に確認したい場合に使う（本プロジェクトの既存呼び出し元は
 *          いずれも fire-and-forget で戻り値を確認しないが、API としては
 *          提供する）。
 *
 * \param[in]  BlockId  ブロック ID (NVM_BLOCK_ID_* 定数)。
 *
 * \retval  NVM_REQ_OK       直近のジョブが完了済み。
 * \retval  NVM_REQ_PENDING  ジョブが保留中（キュー内、または処理中）。
 * \retval  NVM_REQ_NOT_OK   BlockId が範囲外。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
NvM_RequestResultType NvM_GetErrorStatus(NvM_BlockIdType BlockId);

/**
 * \brief   NvM 周期処理。保留中の書き込みジョブを 1 バイトずつ処理する。
 *
 * \details Os スケジューラから周期的に呼び出す。保留ジョブが無ければ
 *          何もしない。ジョブがあれば、対象ブロックのデータ本体または
 *          CRC のうち未書き込みの 1 バイトだけを EEPROM へ書き込み、
 *          ブロック全体（データ本体+CRC）を書き終えたら次の保留ブロックへ
 *          移る。1 回の呼び出しで実際にブロッキングするのは高々 1 バイト分の
 *          EEPROM 書き込み時間のみに抑えられる。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void NvM_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* NVM_H */

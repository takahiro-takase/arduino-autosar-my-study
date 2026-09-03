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
 *              ([SWS_NvM_00208] 相当の非同期ジョブキュー)。
 *            - NvM_MainFunction() は保留中のブロックについて、データ本体・CRC の
 *              順に MemIf_Write() ジョブを投げては完了を待つ（2 フェーズ）。
 *              1 呼び出しにつき 1 バイトずつ書き込んでいるのは NvM 自身ではなく、
 *              さらに下の Fee（MemIf_MainFunction() 経由）である。1 ブロック分
 *              (最大 10 バイト、冗長ブロックはプライマリ→ミラーの順で 2 面分)
 *              が完了すると次の保留ブロックへ移る（詳細は下記「なぜ非同期化
 *              したか」および NvM.c ファイル冒頭のコメント参照）。
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
 *            NvM_WriteBlock() 自体は RAM ミラー更新のみ行って即座に返り、
 *            実際の物理バイト単位の書き込みは MemIf（さらにその下の Fee）が
 *            非同期ジョブとして 1 バイトずつ進める。NvM.c はブロック・CRC・
 *            冗長化という「意味」のレイヤーのみを扱い、バイト単位の
 *            ブロッキング回避そのものは Fee の責務とする
 *            （詳細は NvM.c ファイル冒頭のコメント参照）。
 *
 *          冗長ブロック (Redundant Block):
 *            ブロックごとに `NvM_BlockDescriptorType.Redundant` で
 *            冗長化（2 面化）を選択できる（AUTOSAR NvMBlockManagementType の
 *            NATIVE/REDUNDANT 相当。DATASET は未対応）。有効にすると
 *            データ本体+CRC のセットを 2 か所の EEPROM アドレス（プライマリ／
 *            ミラー）へ保持する。書き込みは必ずプライマリ→ミラーの順で
 *            完全に書き終えてから次のブロックへ移るため、書き込み途中の
 *            電源断で「片方だけ」不完全になっても、もう片方は必ず
 *            （直前の完了済みの内容のまま、または今回の新しい内容で）
 *            CRC 整合した状態を保つ。読み込み時は両面の CRC を検証し、
 *            片方だけ不一致なら正常な方の内容で RAM ミラーを復元した上で
 *            破損した面を同じ内容で書き直す（自己修復）。両方とも不一致
 *            なら通常のブロックと同様デフォルト値へ復元する。
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
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
 * \details AUTOSAR NvM_RequestResultType (SWS_NvM_00470) の一部に相当する。
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
    uint8       Redundant;             /**< 1 = 冗長ブロック（2 面化、AUTOSAR
                                         *   NvMBlockManagementType=NVM_BLOCK_REDUNDANT
                                         *   相当）。0 = 従来のシングルコピー
                                         *   （NVM_BLOCK_NATIVE 相当、既定）。          */
    uint16      NvMNvBlockBaseNumberMirror; /**< Redundant=1 のときのみ使用。ミラー面
                                         *   （2 つ目のコピー）の EEPROM 先頭アドレス。
                                         *   CRC はこの直後
                                         *   (NvMNvBlockBaseNumberMirror + NvMNvBlockLength)
                                         *   に保存する。Redundant=0 では未使用。       */
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
 * \ServiceID      {0x00}
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
 * \ServiceID      {0x06}
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
 *          NvM_MainFunction()/Fee が非同期に行うため、本関数自体は
 *          即座に返る（[SWS_NvM_00208]「WriteBlock はジョブを積むだけ」
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
 * \ServiceID      {0x07}
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
 * \param[in]   BlockId      ブロック ID (NVM_BLOCK_ID_* 定数)。
 * \param[out]  NvM_DestPtr  復元したデフォルト値の追加コピー先。NULL 可。
 *
 * \note       NvM_DestPtr が非 NULL の場合は RAM ミラー更新に加えて
 *             追加でコピーするだけで、仕様の either/or ([SWS_NvM_00435]) は
 *             未実装。
 *
 * \retval  E_OK      ジョブを受け付けた（書き込み完了を意味しない）。
 * \retval  E_NOT_OK  BlockId が範囲外。
 *
 * \AUTOSARReq     {SWS_NvM_00456, SWS_NvM_00012, SWS_NvM_00224, SWS_NvM_00267,
 *                  SWS_NvM_00902}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId, void* NvM_DestPtr);

/**
 * \brief   ブロックの書き込み保護を設定/解除する（[SWS_NvM_00450]）。
 *
 * \details 実仕様は設定時点(`NvMBlockWriteProt`)の既定保護や、一度だけ書き込み
 *          可能で以後は明示解除禁止となる `NvMWriteBlockOnce` ブロック種別を
 *          持つが、本プロジェクトはそのような書き込み一度きりブロックの概念
 *          自体を持たないため、本関数が唯一の保護設定手段であり、常に
 *          （設定値に関わらず）有効/無効を切り替えられる（[SWS_NvM_00325]
 *          相当の簡略化。[SWS_NvM_00577]/[SWS_NvM_00398] の禁止条件は対象外）。
 *
 *          保護状態は RAM 上の管理情報としてのみ保持し（EEPROM には保存
 *          しない）、`NvM_Init()` で常に「保護なし」へ戻る（実仕様の
 *          「リセット時は NvMWriteBlockOnce ブロックの保護のみクリアされる」
 *          という規定とは異なるが、本プロジェクトは電源断からの復電時に
 *          常にブロックの内容を EEPROM から再展開するため、保護設定も
 *          RAM 状態の一部として同様に初期化し直すのが一貫している）。
 *
 *          保護中のブロックは `NvM_WriteBlock()`/`NvM_RestoreBlockDefaults()`
 *          が `E_NOT_OK` を返して書き込みを拒否する（[SWS_NvM_00217]）。
 *          実仕様が要求する production error `NVM_E_WRITE_PROTECTED`
 *          （Dem 経由）は、本プロジェクトが NvM の production error を
 *          Dem に配線する仕組み自体を持たないため報告しない
 *          （DET ログのみ出力）。
 *
 * \param[in]  BlockId            ブロック ID (NVM_BLOCK_ID_* 定数)。
 * \param[in]  ProtectionEnabled  0 以外: 保護を有効化。0: 保護を解除。
 *
 * \retval  E_OK      正常に設定/解除した。
 * \retval  E_NOT_OK  未初期化、または BlockId が範囲外。
 *
 * \AUTOSARReq     {SWS_NvM_00450, SWS_NvM_00016, SWS_NvM_00325}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_SetBlockProtection(NvM_BlockIdType BlockId, uint8 ProtectionEnabled);

/**
 * \brief   ブロックの直近のジョブ結果を取得する。
 *
 * \details NvM_WriteBlock() / NvM_RestoreBlockDefaults() の完了を
 *          明示的に確認したい場合に使う（本プロジェクトの既存呼び出し元は
 *          いずれも fire-and-forget で戻り値を確認しないが、API としては
 *          提供する）。
 *
 * \param[in]   BlockId           ブロック ID (NVM_BLOCK_ID_* 定数)。
 * \param[out]  RequestResultPtr  ジョブ結果の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  RequestResultPtr が NULL、未初期化、または BlockId が
 *                    範囲外（NULL の場合を除き *RequestResultPtr は
 *                    NVM_REQ_NOT_OK）。
 *
 * \note       戻り値の Std_ReturnType と *RequestResultPtr の
 *             NvM_RequestResultType は別の enum。混同して比較しないこと。
 *
 * \AUTOSARReq     {SWS_NvM_00451, SWS_NvM_00610, SWS_NvM_00611, SWS_NvM_00612}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType* RequestResultPtr);

/**
 * \brief   NvM 周期処理。保留中の書き込みジョブを進める。
 *
 * \details Os スケジューラから周期的に呼び出す。保留ジョブが無ければ
 *          何もしない。ジョブがあれば、対象ブロックのデータ本体・CRC の順に
 *          MemIf_Write() ジョブを開始しては完了を待ち（NVM_PHASE_BODY/
 *          NVM_PHASE_CRC の 2 フェーズ）、ブロック全体を書き終えたら次の
 *          保留ブロックへ移る。実際に EEPROM へ 1 バイトずつ書き込んでいるのは
 *          本関数ではなく、さらに下の Fee（MemIf_MainFunction() 経由、Os
 *          スケジューラから NvM_MainFunction() とは独立に呼ばれる）であり、
 *          本関数自体はジョブの開始・完了確認のみでブロッキングしない
 *          （詳細は NvM.c ファイル冒頭のコメント参照）。
 *
 * \ServiceID      {0x0E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void NvM_MainFunction(void);

/**
 * \brief   NvM モジュールのバージョン情報を取得する。
 *
 * \details 他 BSW モジュールと共通の慣例により、未初期化時でもエラー報告
 *          しない例外 API のため、初期化状態は確認せず NULL ポインタ
 *          チェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_NvM_00452}
 * \ServiceID      {0x0F}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void NvM_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* NVM_H */

/**
 * \file    E2EXf.h
 * \brief   E2E Transformer 公開インタフェース (AUTOSAR SWS_E2ELibrary 12.4 準拠)
 *
 * \details AUTOSAR が定義する 3 通りの E2E 統合方式のうち「E2E Transformer」
 *          （`docs/autosar/4.3.1/AUTOSAR_SWS_E2ETransformer.pdf`、AUTOSAR
 *          R4.2.1 以降）を模した薄いラッパー。Com は E2E の存在を一切知らず
 *          （Com_Types.h の RxIndicationCbk / TxTransformCbk 汎用フック経由で
 *          呼ばれるだけ）、実際の CRC/Counter 検証・付与は本モジュールが
 *          E2E_P05.c（現在の実配線先。E2E_P01.c は実 PDU を持たない参考実装
 *          として `test/Bsw/E2E/Bsw_E2E_test.cpp` の `E2EP01Test` からのみ
 *          検証される）へ委譲する形で行う。
 *
 *          実 AUTOSAR の Transformer は RTE 生成コードが `E2EXf_<transformerId>`/
 *          `E2EXf_Inv_<transformerId>`（[SWS_E2EXf_00020]/[00025]、8.3節の
 *          Syntax）という、E2E 保護対象のデータエレメント（`<transformerId>`）
 *          ごとに個別の関数を自動生成するが、本プロジェクトには RTE
 *          ジェネレータが無いため、`Rte.c` が
 *          Com_ReceiveSignalGroupArray()/Com_IsRxTimedOut() 経由で明示的に
 *          このモジュールの API を呼び出す、静的に書き下した相当品として
 *          実装している（2026-09 是正: 以前は Config 構造体を引数に取る
 *          汎用関数 1 個で全インスタンスを賄っていたが、`<transformerId>`
 *          には実データエレメント名（EngineInfo/AbsInfo/E2EHealthStatus）を
 *          そのまま使い、インスタンスごとに独立した関数
 *          （`E2EXf_Inv_EngineInfo()`等）へ書き直した。戻り値も
 *          [SWS_E2EXf_00027] のニブルパック契約に合わせている。詳細な経緯は
 *          git 履歴のコミットログ参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1/4.2.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2EXF_H
#define E2EXF_H

#include "Std_Types.h"
#include "E2E_Types.h"
#include "E2E_P05.h"
#include "Dem.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   E2EXf_Init() の設定引数型（不透明型）。
 *
 * \details SWS_E2EXf_00035 は post-build 設定構造体へのポインタ（post-build
 *          selectable の場合）または NULL（link-time の場合）を要求する。
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を
 *          持たないため、中身を定義しない不透明型とし、ポインタとしてのみ
 *          扱う（`CanSM_ConfigType`/`KeyM_ConfigType` と同じ簡略化パターン）。
 */
typedef struct E2EXf_ConfigType_Tag E2EXf_ConfigType;

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_E2EXf_00137 の Development Errors 表に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * E2E Transformer (E2EXf) に割り当てられた固定値 176 を使う。
 *
 * `E2EXf_Inv_EngineInfo()`/`E2EXf_Inv_AbsInfo()`/`E2EXf_E2EHealthStatus()` は
 * 実際の generic API `E2EXf_<transformerId>`/`E2EXf_Inv_<transformerId>`
 * （RTE 生成コードがトランスフォーマー、すなわちデータエレメントごとに
 * 実体化する）に対応するインスタンス専用関数（2026-09 是正、詳細は
 * 各関数の Doxygen 参照）。
 * ----------------------------------------------------------------------- */

/** AUTOSAR E2E Transformer の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 176） */
#define E2EXF_MODULE_ID  176U

/** 開発エラーコード（SWS_E2EXf_00137 表より、実際に使用する分のみ）。
 *  E2EXF_E_PARAM は非ポインタ引数（Length 等）の異常、E2EXF_E_PARAM_POINTER
 *  はポインタ引数の NULL に対応する（表の記載どおり区別する）。 */
#define E2EXF_E_UNINIT        0x01U
#define E2EXF_E_PARAM         0x03U
#define E2EXF_E_PARAM_POINTER 0x04U

/** [SWS_E2EXf_00152]/[00153]: `E2EXf_Inv_<transformerId>`（本プロジェクトの
 *  `E2EXf_Inv_EngineInfo()`/`E2EXf_Inv_AbsInfo()`）はパラメータ異常・
 *  未初期化検出時、通常の Std_ReturnType (E_OK/E_NOT_OK) ではなく本値を
 *  返すべきと規定されている（実 AUTOSAR では TransformerTypes.h で定義
 *  される値だが、本プロジェクトは同ヘッダを持たないためここで定義する）。
 *  いずれの規定も設定パラメータ XfrmDevErrorDetect が有効な場合のみ適用
 *  されるが、本プロジェクトは同設定自体を持たず常時有効とみなす
 *  （他モジュールの DevErrorDetect 系設定と同じ簡略化方針）。
 *  対になる forward 側の規定 [SWS_E2EXf_00150]/[00151]（`E2EXf_<transformerId>`、
 *  本プロジェクトの `E2EXf_E2EHealthStatus()`）も同じ値を返す。 */
#define E_SAFETY_HARD_RUNTIMEERROR ((Std_ReturnType)0xFFU)

/** [SWS_E2EXf_00027]: `E2E_SMCheck()` が `E2E_E_OK` 以外を返した場合
 *  （[SWS_E2EXf_00027]/[SWS_E2E_00216]参照、`E2EXf_ReportSMVerdict()`の
 *  Doxygen参照）の戻り値。「安全性の判定（SMState）は確定できなかったが、
 *  保護されていない生データ自体は使用可能」ことを表す（0x77、実測値）。
 *  正常系（`E2E_SMCheck()` 自体は成功）の戻り値は [SWS_E2EXf_00027] の
 *  ニブルパック規定に従う（`E2EXf_PackReturn()` 参照）。本値はその契約とは
 *  独立した単一の特殊値（ニブル分解しない）として、パック値と同じ
 *  `uint8` 戻り値の範囲にそのまま追加できる。 */
#define E_SAFETY_SOFT_RUNTIMEERROR ((Std_ReturnType)0x77U)

/** ApiId（値は SWS 8.x 章の「Service ID[hex]」記載を実測して確認済み） */
#define E2EXF_API_ID_INIT               0x01U
#define E2EXF_API_ID_DEINIT             0x02U
#define E2EXF_API_ID_TRANSFORM          0x03U
#define E2EXF_API_ID_INVERSE_TRANSFORM  0x04U
#define E2EXF_API_ID_GET_VERSION_INFO   0x00U

/** バージョン情報（SWS_E2EXf_00036、Com 等の既存モジュールと同じ命名規則） */
#define E2EXF_VENDOR_ID          0U
#define E2EXF_SW_MAJOR_VERSION   1U
#define E2EXF_SW_MINOR_VERSION   0U
#define E2EXF_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * RX 側（Inverse Transformer）内部状態 — E2E Profile 05
 *
 * \note  2026-09 是正: 実 AUTOSAR の `E2EXf_Inv_<transformerId>`/
 *        `E2EXf_<transformerId>` はインスタンス（データエレメント）ごとに
 *        個別の関数として生成され、本プロジェクトのように「Config構造体を
 *        引数で受け取る汎用関数」にはならない
 *        （[SWS_E2EXf_00020]/[SWS_E2EXf_00025]、8.3.1/8.3.2節の Syntax
 *        参照。詳細な議論は git 履歴のコミットログ参照）。この struct は
 *        以前は `E2EXf_InverseTransformP05()` の引数型だったが、現在は
 *        `E2EXf.c` の各インスタンス専用関数（`E2EXf_Inv_EngineInfo()`等）が
 *        `E2EXf_PBCfg.c` の対応するインスタンスを直接参照するための内部
 *        表現としてのみ使う（公開 API の引数には登場しない）。
 *
 * \note  WaitForFirstData: 公式の E2E_P05CheckStateType には Profile01 の
 *        WaitForFirstData/INITIAL に相当するフィールドが無い（E2E_P05.c は
 *        意図的にこれを実装しない、仕様に忠実なライブラリとして維持している。
 *        test/test_e2e_p05/ の
 *        FirstCheckAfterInitIsRepeatedBecauseBothStartAtCounterZero テスト
 *        参照）。しかし実運用では、起動直後に送信元 ECU が既に稼働中で
 *        Counter が 0 以外から始まっていることが十分あり得るため、E2E_P05.c
 *        をそのまま繋ぐと起動直後の最初の（CRC は正しい）フレームが
 *        REPEATED/WRONGSEQUENCE と誤判定され、DEM_DEBOUNCE_LIMIT=1 の設定と
 *        相まって即座に誤った DTC が確定してしまう。そのため Profile01 の
 *        WaitForFirstData 相当の「初回受信の特別扱い」を、ライブラリ本体
 *        ではなく統合層であるこの E2EXf 層で補う
 *        （`E2EXf.c` の各インスタンス関数参照）。NULL の場合はこの特別扱いを
 *        行わない（EngineHealthStatus 用など、将来 Check を使うが初回受信の
 *        意味を持たないインスタンスのため）。
 * ----------------------------------------------------------------------- */
typedef struct
{
    const E2E_P05ConfigType* E2EConfig;
    E2E_P05CheckStateType*   CheckState;
    Dem_EventIdType          DemEventId;
    uint8*                   WaitForFirstData;
    /** E2E ステートマシン設定・状態（[SWS_E2EXf_00028]、E2EXf.c の各
     *  インスタンス関数参照）。NULL 不可。 */
    const E2E_SMConfigType*  SMConfig;
    E2E_SMCheckStateType*    SMState;
} E2EXf_RxConfigTypeP05;

/* -----------------------------------------------------------------------
 * TX 側（Transformer）内部状態 — E2E Profile 05
 * EngineHealthStatus (CAN 0x220) が使用する。上記 E2EXf_RxConfigTypeP05 の
 * 注記と同じ理由で、公開 API の引数ではなく `E2EXf.c` 内部の表現としてのみ
 * 使う。
 * ----------------------------------------------------------------------- */
typedef struct
{
    const E2E_P05ConfigType* E2EConfig;
    E2E_P05ProtectStateType* ProtectState;
} E2EXf_TxConfigTypeP05;

/**
 * \brief   E2EXf モジュール自身を初期化済み状態にする。
 *
 * \details SWS_E2EXf_00130: E2E Transformer は「E2EXf_Init() が呼ばれたか」
 *          という初期化状態を、下位の E2E_P05 Check/ProtectState とは別に
 *          自身で保持しなければならない。SWS_E2EXf_00133/00151 により、
 *          未初期化のまま各インスタンス関数（`E2EXf_Inv_EngineInfo()`等）が
 *          呼ばれた場合は処理を行わず安全側で早期 return する。
 *
 *          `E2EXf_PBCfg_Init()`（`src/Bsw/E2EXf/E2EXf_PBCfg.c`）が各 I-PDU の
 *          E2E_P05Check/ProtectState を初期化した最後に、本関数を呼んで
 *          初期化完了をマークする。
 *
 * \pre        EcuM_Init() から Com_Init() の後、フレーム受信・送信が
 *             始まる前に呼び出すこと。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build
 *                        設定を持たないため。実 AUTOSAR 仕様は
 *                        SWS_E2EXf_00035 で post-build selectable の場合の
 *                        設定構造体ポインタを要求するが、link-time variant
 *                        の場合は NULL でよいと明記されている）。
 *
 * \AUTOSARReq     {SWS_E2EXf_00035}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2EXf_Init(const E2EXf_ConfigType* ConfigPtr);

/**
 * \brief   E2EXf モジュールを未初期化状態に戻す。
 *
 * \details SWS_E2EXf_00148: モジュール初期化状態を FALSE に戻す。
 *          SWS_E2EXf_00146: 未初期化状態で呼ばれた場合は何もせず、
 *          E2EXF_E_UNINIT を Det_ReportError() へ報告する。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2EXf_DeInit(void);

/**
 * \brief   EngineInfo (CAN 0x100) 受信データに対する E2E Inverse Transform
 *          （検証）を行う。実 AUTOSAR の `E2EXf_Inv_<transformerId>`
 *          （[SWS_E2EXf_00025]、8.3.2節）に対応するインスタンス専用関数。
 *
 * \details 2026-09 是正: 従来は `E2EXf_RxConfigType`/`E2EXf_RxConfigTypeP05`
 *          を引数で受け取る汎用関数（`E2EXf_InverseTransform()`/
 *          `E2EXf_InverseTransformP05()`）だったが、実 AUTOSAR の
 *          `E2EXf_Inv_<transformerId>` はデータエレメント（本関数の場合
 *          EngineInfo）ごとに個別生成される関数であり、Config を引数で
 *          受け取る設計にはならない（[SWS_E2EXf_00020]/[00025]）。本関数は
 *          `E2EXf_PBCfg.c` の `E2EXf_EngineInfoRxCfg` を直接参照する
 *          インスタンス専用実装へ変更した（AbsInfo 用の
 *          `E2EXf_Inv_AbsInfo()` とは実装を共有せず、それぞれ独立に完結
 *          させている。実 RTE 生成コードの構造に倣う）。
 *
 *          E2E_P05Check() を呼び、結果を `E2E_P05MapStatusToSM()` で汎用
 *          ステータスへ変換する。OK/OKSOMELOST は E2E_P_OK（今回のフレーム
 *          は使ってよい）、REPEATED/WRONGSEQUENCE/ERROR は不合格へ写像する。
 *
 *          `E2EXf_EngineInfoRxCfg.WaitForFirstData` が真の場合、CRC が正しい
 *          （ERROR 以外の）最初の呼び出しに限り、生の判定結果に関わらず
 *          OK として扱い（`*CheckStatus` も OK に書き換える）、フラグを
 *          落とす（Profile01 の WaitForFirstData/INITIAL に相当する初回
 *          受信の特別扱いを E2EXf 層で補うもの。E2EXf_RxConfigTypeP05 の
 *          宣言コメント参照）。
 *
 *          上記の「今回のフレームが使えるか」という毎回の判定（戻り値低位
 *          ニブル・`*CheckStatus`）とは別に、`E2E_SMCheck()`
 *          （[SWS_E2EXf_00028]/[00029]、直近 WindowSize 回分の健全性を
 *          判定するステートマシン）を呼ぶ。その結果が `E2E_SM_VALID`/
 *          `E2E_SM_INVALID` に確定したときのみ Dem_SetEventStatus() の
 *          PASSED/FAILED を報告する。`E2E_SM_NODATA`/`E2E_SM_INIT`
 *          （判定材料が揃うまでの起動直後）の間は Dem 報告を保留する
 *          （[SWS_E2E_00345] の状態図が "do NOT use data" と規定する状態
 *          であり、まだ確定した健全性判定を返せないため）。
 *
 *          [SWS_E2EXf_00027]/8.3.2節の規定どおり、戻り値は「上位ニブル=
 *          E2E_SMStateType（VALID=0/NODATA=2/INIT=3/INVALID=4）、下位ニブル
 *          =E2E_PCheckStatusType（プロファイル非依存チェック結果）」の
 *          パック値とする（2026-09 是正、以前は単純な `Std_ReturnType`
 *          E_OK/E_NOT_OK のみだった）。呼び出し元（`Rte.c`）は本来の
 *          「フレーム単体の合否」（下位ニブル）だけでなく「SMState=VALID
 *          に確定するまではデータを使わない」（[SWS_E2E_00345]）という
 *          仕様の要求も、`ret == E_OK`（0x00、両ニブルとも0）の1条件だけで
 *          自然に満たせる（詳細な生の判定結果 `CheckStatus` は Rte.c 側の
 *          より細かい `Rte_IStatusType` 分類にのみ使う、下記 \param 参照）。
 *          `E2E_SMCheck()` 自体が失敗した場合（到達しないはずの防御的経路）
 *          は、ニブルパックせず `E_SAFETY_SOFT_RUNTIMEERROR`（0x77）を
 *          そのまま返す。
 *
 * \note    `bufferLength`/`inputBuffer`/`inputBufferLength` は仕様の
 *          Syntax（8.3.2節）に合わせてシグネチャへ含めているが、本
 *          プロジェクトは in-place 変換（かつ全 PDU 固定長）のみを実装し
 *          out-of-place 変換は行わないため、`inputBuffer`/
 *          `inputBufferLength` は未使用（常に無視する）。`bufferLength`
 *          には呼び出し完了時の実使用長（= 固定の DataLength）を書き込む。
 *
 * \note    `CheckStatus` は仕様の Syntax には無い本プロジェクト独自の
 *          追加出力引数（2026-09、Rte.c が必要とする詳細な生ステータス
 *          （OKSOMELOST 等、プロファイル非依存写像では失われる情報）を
 *          残すための暫定対応。将来的にはグローバル変数経由での公開へ
 *          置き換える予定 — 詳細はコミットログ参照）。
 *
 * \param[in,out] buffer             検証対象の I-PDU バイト列（in-place、
 *                                   固定長）。NULL 禁止。
 * \param[out]    bufferLength       実使用長を受け取る。NULL 禁止。
 * \param[in]     inputBuffer        未使用（out-of-place 変換非対応）。
 * \param[in]     inputBufferLength  未使用（同上）。
 * \param[out]    CheckStatus        E2E_P05Check() の生の 6 状態を受け取る
 *                                   （仕様外の追加出力引数、上記 \note 参照）。
 *                                   NULL 禁止。
 *
 * \retval  0x00 (E_OK)                 SMState=VALID かつ今回のフレームも
 *                                      合格。呼び出し元は buffer の内容を
 *                                      使ってよい。
 * \retval  0x02/0x03/0x05/0x20〜0x45   [SWS_E2EXf_00027] のニブルパック値
 *                                      （SMState/チェック結果の組み合わせ）。
 * \retval  E_SAFETY_HARD_RUNTIMEERROR  buffer/bufferLength/CheckStatus が
 *                                      NULL、または E2EXf_Init() 未呼び出し
 *                                      （[SWS_E2EXf_00152]/[00153]）。
 * \retval  E_SAFETY_SOFT_RUNTIMEERROR  E2E_SMCheck() が失敗した（到達しない
 *                                      はずの防御的な経路。[SWS_E2EXf_00027]）。
 *
 * \AUTOSARReq     {SWS_E2EXf_00020, SWS_E2EXf_00025, SWS_E2EXf_00152,
 *                  SWS_E2EXf_00153, SWS_E2EXf_00009, SWS_E2EXf_00027,
 *                  SWS_E2EXf_00028, SWS_E2EXf_00029}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 E2EXf_Inv_EngineInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer, uint32 inputBufferLength,
                            E2E_P05StatusType* CheckStatus);

/**
 * \brief   AbsInfo (CAN 0x110) 受信データに対する E2E Inverse Transform
 *          （検証）を行う。`E2EXf_Inv_EngineInfo()` の同名コメント参照
 *          （`E2EXf_EngineInfoRxCfg` の代わりに `E2EXf_AbsInfoRxCfg` を
 *          参照する以外は同一実装）。
 *
 * \param[in,out] buffer             検証対象の I-PDU バイト列。NULL 禁止。
 * \param[out]    bufferLength       実使用長を受け取る。NULL 禁止。
 * \param[in]     inputBuffer        未使用（out-of-place 変換非対応）。
 * \param[in]     inputBufferLength  未使用（同上）。
 * \param[out]    CheckStatus        E2E_P05Check() の生の 6 状態を受け取る
 *                                   （仕様外の追加出力引数）。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_E2EXf_00020, SWS_E2EXf_00025, SWS_E2EXf_00152,
 *                  SWS_E2EXf_00153, SWS_E2EXf_00009, SWS_E2EXf_00027,
 *                  SWS_E2EXf_00028, SWS_E2EXf_00029}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 E2EXf_Inv_AbsInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer, uint32 inputBufferLength,
                         E2E_P05StatusType* CheckStatus);

/**
 * \brief   E2EHealthStatus (CAN 0x220) 送信データに対する E2E Transform
 *          （Counter/CRC16 付与）を行う。実 AUTOSAR の
 *          `E2EXf_<transformerId>`（[SWS_E2EXf_00020]、8.3.1節）に対応する
 *          インスタンス専用関数。
 *
 * \details 2026-09 是正の経緯は `E2EXf_Inv_EngineInfo()` の同名コメント
 *          参照（Check 側と対称、`E2EXf_TxConfigTypeP05`/汎用関数
 *          `E2EXf_TransformP05()` を廃止し、`E2EXf_E2EHealthStatusTxCfgP05`
 *          を直接参照するインスタンス専用実装へ変更）。E2E_P05Protect() を
 *          呼び、buffer へ Counter・CRC16 を書き込む。
 *
 *          Protect 側にはステートマシン（E2E_SMCheck）が無いため戻り値の
 *          ニブルパックは行わない（[SWS_E2EXf_00032]/8.3.1節の Return value
 *          も E_OK/E_SAFETY_SOFT_RUNTIMEERROR/E_SAFETY_HARD_RUNTIMEERROR の
 *          単純な3値）。本実装では E_SAFETY_SOFT_RUNTIMEERROR を返す経路は
 *          無い（判定不能な状態自体が Protect 側には存在しないため）。
 *
 * \note    `bufferLength`/`inputBuffer`/`inputBufferLength` の扱いは
 *          `E2EXf_Inv_EngineInfo()` の同名 \note 参照。
 *
 * \param[in,out] buffer             変換対象の I-PDU バイト列（in-place、
 *                                   固定長、上書きされる）。NULL 禁止。
 * \param[out]    bufferLength       実使用長を受け取る。NULL 禁止。
 * \param[in]     inputBuffer        未使用（out-of-place 変換非対応）。
 * \param[in]     inputBufferLength  未使用（同上）。
 *
 * \retval  E_OK                       変換を実行した。
 * \retval  E_SAFETY_HARD_RUNTIMEERROR 未初期化、または buffer/bufferLength
 *                                     が NULL 等のパラメータ異常
 *                                     （[SWS_E2EXf_00150]/[00151]）。
 *
 * \AUTOSARReq     {SWS_E2EXf_00020, SWS_E2EXf_00032, SWS_E2EXf_00150,
 *                  SWS_E2EXf_00151}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 E2EXf_E2EHealthStatus(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer, uint32 inputBufferLength);

/**
 * \brief   E2EXf モジュールのバージョン情報を取得する。
 *
 * \details SWS_E2EXf_00137 のエラー表が明記するとおり、GetVersionInfo は
 *          「Init 未実行/DeInit 後でも E2EXF_E_UNINIT を報告しない」唯一の
 *          例外 API である。そのため本関数は初期化状態を確認しない。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \retval  なし（SWS_E2EXf_00149: NULL の場合は E2EXF_E_PARAM_POINTER を報告
 *          し、何も書き込まずに戻る）。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2EXf_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif

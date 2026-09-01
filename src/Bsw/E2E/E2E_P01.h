/**
 * \file    E2E_P01.h
 * \brief   E2E Profile 01 保護ライブラリ (AUTOSAR SWS_E2ELibrary 準拠)
 * \details AUTOSAR E2E Profile 01 の送信保護（Protect）・受信チェック（Check）
 *          機能を提供する。CRC8 (SAE J1850 / 多項式 0x1D / 初期値 0x00 /
 *          最終 XOR 0x00、SWS_E2E_00083 準拠) と 4 ビットカウンタにより
 *          データ化け・消失・重複・誤ルーティングを検出する。
 *
 *          本プロジェクトでの適用対象（AUTOSAR 標準バリアント 1A に準拠したレイアウト。
 *          CRC が先頭バイト、Counter がそれに続く 1 バイトの下位 4bit という配置は
 *          SWS_E2E_00227 の固定レイアウトそのものである）:
 *            AbsInfo (CAN ID 0x110, DLC=5, 受信側で Check)
 *              byte[0]   : CRC8
 *              byte[1]   : Counter  (下位 4bit。上位 4bit は未使用)
 *              byte[2-4] : 元データ (VehicleSpeed, BrakeActive, AbsActive)
 *            MeterStatus (CAN ID 0x200, DLC=3, 送信側で Protect)
 *              byte[0]   : CRC8
 *              byte[1]   : Counter  (下位 4bit。上位 4bit は未使用)
 *              byte[2]   : 元データ (EngineState)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2E_P01_H
#define E2E_P01_H

#include "Std_Types.h"
#include "E2E_Types.h"

/* -----------------------------------------------------------------------
 * E2E P01 チェック結果ステータス
 * AUTOSAR E2E_P01CheckStatusType (SWS_E2E_00022) に準拠した 8 値。
 * 値は公式仕様のビットパターンと一致させている（ビット OR で複数状態を
 * 表現する用途を想定した設計だが、本実装では単一値のみ返す）。
 * ERROR (0x80) のみ本実装独自の拡張で、NULL ポインタ・DLC 不足など
 * 入力パラメータ異常を表す。公式仕様ではこの 8 状態は関数の戻り値
 * （E2E_P01StatusType）ではなく E2E_P01CheckStateType.Status
 * （inout パラメータ）で返される。関数自体の戻り値は E2E_Types.h の
 * Std_ReturnType 拡張値（E2E_E_OK 等）である（下記 API 参照）。
 * ----------------------------------------------------------------------- */
typedef enum
{
    E2E_P01STATUS_OK            = 0x00U, /**< 正常（Counter が前回 +1、CRC 正）              */
    E2E_P01STATUS_NONEWDATA     = 0x01U, /**< 前回チェック以降、新規データなし（本実装では未使用、下記注記参照） */
    E2E_P01STATUS_WRONGCRC      = 0x02U, /**< CRC 不一致（データ化け・誤ルーティング）        */
    E2E_P01STATUS_SYNC          = 0x03U, /**< 異常検知後の再同期中（CRC 正・Counter 進行も正常だが継続性未確定） */
    E2E_P01STATUS_INITIAL       = 0x04U, /**< 初回受信（カウンタ基準未確立）                  */
    E2E_P01STATUS_REPEATED      = 0x08U, /**< 同一カウンタが連続（重複受信）                  */
    E2E_P01STATUS_OKSOMELOST    = 0x20U, /**< CRC 正・Counter 進行も許容範囲内だが一部消失あり */
    E2E_P01STATUS_WRONGSEQUENCE = 0x40U, /**< カウンタ飛びが許容超過（過剰消失、再同期を開始） */
    E2E_P01STATUS_ERROR         = 0x80U  /**< 非標準拡張: NULL ポインタ・DLC 不足等の入力異常  */
} E2E_P01StatusType;

/* -----------------------------------------------------------------------
 * E2E P01 設定構造体
 * DaVinci: /ActiveEcuC/E2EXf/[E2EXf_Profile01]
 * ----------------------------------------------------------------------- */
typedef struct
{
    uint16 DataID;           /**< PDU 識別 ID (例: 0x0110)            */
    uint8  DataLength;       /**< 保護対象 PDU 全体バイト数 (CRC 含む) */
    uint8  MaxDeltaCounter;  /**< 許容するカウンタ飛び幅 (通常 1〜3)。Protect 側では未使用 */
    uint8  CounterOffset;    /**< Counter バイトの PDU 内オフセット    */
    uint8  CRCOffset;        /**< CRC バイトの PDU 内オフセット        */
    uint8  SyncCounterInit;  /**< WRONGSEQUENCE 検知後、OK/OKSOMELOST へ復帰するまでに
                                   必要な連続正常受信回数（E2E_P01Check、7.3.9 章、
                                   主要求 SWS_E2E_00196。SyncCounterInit 単体の
                                   専用要求番号はなく、この値自体は 8.2.1.1 章の
                                   E2E_P01ConfigType 定義で導入される）。
                                   Protect 側では未使用 */
} E2E_P01ConfigType;

/* -----------------------------------------------------------------------
 * E2E P01 受信ステートマシン状態
 * DaVinci: E2E_P01CheckStateType
 *
 * \note  公式仕様の `E2E_P01CheckStateType` にはこのほか `MaxDeltaCounter`
 *        （呼び出しごとに増加する動的な許容幅）・`NewDataAvailable` /
 *        `NoNewOrRepeatedDataCounter` が定義されている。本実装は
 *        Com_RxIndication() からフレーム受信時にのみ Check を呼び出す
 *        （＝呼ぶ時点で必ず新規データがある）設計のため、「Check は呼ばれた
 *        が新規データがない」状況が発生せず、これらのフィールドは実質的に
 *        不要と判断し実装していない（NONEWDATA が定義はされるが到達しない
 *        のはこのため）。
 * ----------------------------------------------------------------------- */
typedef struct
{
    uint8             LastValidCounter; /**< 最後に受け付けたカウンタ値       */
    E2E_P01StatusType Status;           /**< 直前チェックの結果               */
    uint8             WaitForFirstData; /**< 1=初期化直後、まだデータ未受信   */
    uint8             SyncCounter;      /**< 再同期の残り回数 (0=通常運用中、
                                              >0=WRONGSEQUENCE 検知後の再ロック中) */
} E2E_P01CheckStateType;

/* -----------------------------------------------------------------------
 * E2E P01 送信ステートマシン状態
 * DaVinci: E2E_P01ProtectStateType
 * ----------------------------------------------------------------------- */
typedef struct
{
    uint8 Counter;  /**< 次回送信するカウンタ値 (4bit、Protect 呼び出しごとに +1) */
} E2E_P01ProtectStateType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief  E2E P01 受信ステートを初期化する。
 *
 * \param[out] State  初期化する受信ステート。NULL 禁止。
 * \return     E2E_E_OK: 正常完了。E2E_E_INPUTERR_NULL: State が NULL。
 *
 * \AUTOSARReq     {SWS_E2E_00390}
 * \ServiceID      {0x1c}
 */
Std_ReturnType E2E_P01CheckInit(E2E_P01CheckStateType *State);

/**
 * \brief  受信データの E2E P01 チェックを実行する。
 *
 * \details 検証結果（8 状態）は本関数の戻り値ではなく State->Status に
 *          書き込まれる。呼び出し元は Data が Config->DataLength バイト
 *          以上の有効な領域を指すことを保証すること（[SWS_E2E_00047] の
 *          Length 引数は本実装では持たず、実仕様どおり Config->DataLength
 *          が唯一の長さ情報である）。
 *
 * \param[in]  Config  E2E P01 設定構造体。NULL 禁止。
 * \param[io]  State   受信ステート。NULL 禁止。
 * \param[in]  Data    受信 PDU バッファ。NULL 禁止、Config->DataLength
 *                     バイト以上であること。
 * \return     E2E_E_OK: チェックを実行した（結果は State->Status 参照）。
 *             E2E_E_INPUTERR_NULL: Config/State/Data のいずれかが NULL。
 *
 * \AUTOSARReq     {SWS_E2E_00047}
 * \ServiceID      {0x02}
 */
Std_ReturnType E2E_P01Check(
    const E2E_P01ConfigType *Config,
    E2E_P01CheckStateType   *State,
    const uint8             *Data);

/**
 * \brief  E2E P01 送信ステートを初期化する。
 *
 * \param[out] State  初期化する送信ステート。NULL 禁止。
 * \return     E2E_E_OK: 正常完了。E2E_E_INPUTERR_NULL: State が NULL。
 *
 * \ServiceID      {0x1b}
 */
Std_ReturnType E2E_P01ProtectInit(E2E_P01ProtectStateType *State);

/**
 * \brief  送信データに E2E P01 保護（Counter・CRC8）を付与する。
 *
 * \details Data[Config->CounterOffset] に現在の Counter を書き込んだ後、
 *          DataID・Data[0..CRCOffset-1]・Data[CRCOffset+1..DataLength-1]
 *          (CRC バイト自身を除く PDU 全体) から CRC8 を計算して
 *          Data[Config->CRCOffset] へ書き込む。呼び出しごとに Counter を
 *          +1 する（4bit リングカウンタ）。呼び出し元は Data が
 *          Config->DataLength バイト以上の有効な領域を指すことを
 *          保証すること（E2E_P01Check() と同じ理由で Length 引数は持たない）。
 *
 * \param[in]     Config  E2E P01 設定構造体。NULL 禁止。
 * \param[in,out] State   送信ステート。NULL 禁止。
 * \param[in,out] Data    送信 PDU バッファ（Counter/CRC バイトを上書きする）。
 *                        NULL 禁止、Config->DataLength バイト以上であること。
 * \return        E2E_E_OK: 正常完了。E2E_E_INPUTERR_NULL: Config/State/Data
 *                のいずれかが NULL。
 *
 * \ServiceID      {0x01}
 */
Std_ReturnType E2E_P01Protect(
    const E2E_P01ConfigType *Config,
    E2E_P01ProtectStateType *State,
    uint8                   *Data);

/**
 * \brief  E2E_P01Check() の詳細な8状態を、プロファイル非依存の汎用結果へ変換する。
 *
 * \details [SWS_E2E_00382]〜[SWS_E2E_00384]。`profileBehavior` により2種類の
 *          マッピング表を切り替える(実仕様の `boolean` 型は本プロジェクトに
 *          存在しないため、他の1/0フラグ群と同じ `uint8` で代用。
 *          E2E_Types.h 参照)。
 *            - `profileBehavior`=1 (TRUE、R4.2以降の挙動):
 *              {OK, OKSOMELOST, SYNC}→OK, {WRONGSEQUENCE, INITIAL}→WRONGSEQUENCE
 *            - `profileBehavior`=0 (FALSE、R4.2より前の挙動):
 *              {OK, OKSOMELOST, INITIAL}→OK, {WRONGSEQUENCE, SYNC}→WRONGSEQUENCE
 *          （WRONGCRC→ERROR、REPEATED→REPEATED、NONEWDATA→NONEWDATAは共通）
 *          `CheckReturn` が E2E_E_OK 以外の場合は `Status` に関わらず
 *          `E2E_P_ERROR` を返す。
 *          [SWS_E2E_00216] によりライブラリはDet/Dem/RTEを呼び出さないため、
 *          本関数もポインタ引数を持たず（NULL チェック不要）、内部で
 *          Det_ReportError() を一切呼ばない。
 *
 * \param[in]  CheckReturn      E2E_P01Check() の戻り値。
 * \param[in]  Status           E2E_P01Check() が書き込んだ State->Status。
 * \param[in]  profileBehavior  1=R4.2以降の挙動、0=それ以前の挙動。
 *
 * \return  プロファイル非依存のチェック結果。
 *
 * \AUTOSARReq     {SWS_E2E_00382}
 * \ServiceID      {0x1d}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
E2E_PCheckStatusType E2E_P01MapStatusToSM(
    Std_ReturnType    CheckReturn,
    E2E_P01StatusType Status,
    uint8             profileBehavior);

#endif /* E2E_P01_H */

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

/* -----------------------------------------------------------------------
 * E2E P01 チェック結果ステータス
 * AUTOSAR E2E_P01CheckStatusType (SWS_E2E_00022) に準拠した 8 値。
 * 値は公式仕様のビットパターンと一致させている（ビット OR で複数状態を
 * 表現する用途を想定した設計だが、本実装では単一値のみ返す）。
 * ERROR (0x80) のみ本実装独自の拡張で、NULL ポインタ・DLC 不足など
 * 入力パラメータ異常を表す（公式仕様では戻り値型が別になっている箇所）。
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
                                   必要な連続正常受信回数（SWS_E2E_00019）。
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
 * \param[out] State  初期化する受信ステート。NULL 禁止。
 */
void E2E_P01CheckInit(E2E_P01CheckStateType *State);

/**
 * \brief  受信データの E2E P01 チェックを実行する。
 *
 * \param[in]  Config  E2E P01 設定構造体。NULL 禁止。
 * \param[io]  State   受信ステート。NULL 禁止。
 * \param[in]  Data    受信 PDU バッファ。NULL 禁止。
 * \param[in]  Length  受信 PDU バイト数。
 * \return     E2E_P01StatusType チェック結果。8 状態のうち NONEWDATA は
 *             本実装の呼び出し方式（フレーム受信時にのみ呼ばれる）では
 *             到達しない。
 */
E2E_P01StatusType E2E_P01Check(
    const E2E_P01ConfigType *Config,
    E2E_P01CheckStateType   *State,
    const uint8             *Data,
    uint8                    Length);

/**
 * \brief  E2E P01 送信ステートを初期化する。
 * \param[out] State  初期化する送信ステート。NULL 禁止。
 */
void E2E_P01ProtectInit(E2E_P01ProtectStateType *State);

/**
 * \brief  送信データに E2E P01 保護（Counter・CRC8）を付与する。
 *
 * \details Data[Config->CounterOffset] に現在の Counter を書き込んだ後、
 *          DataID・Data[0..CRCOffset-1]・Data[CRCOffset+1..DataLength-1]
 *          (CRC バイト自身を除く PDU 全体) から CRC8 を計算して
 *          Data[Config->CRCOffset] へ書き込む。呼び出しごとに Counter を
 *          +1 する（4bit リングカウンタ）。
 *
 * \param[in]     Config  E2E P01 設定構造体。NULL 禁止。
 * \param[in,out] State   送信ステート。NULL 禁止。
 * \param[in,out] Data    送信 PDU バッファ（Counter/CRC バイトを上書きする）。NULL 禁止。
 * \param[in]     Length  送信 PDU バイト数。Config->DataLength 未満の場合は何もしない。
 */
void E2E_P01Protect(
    const E2E_P01ConfigType *Config,
    E2E_P01ProtectStateType *State,
    uint8                   *Data,
    uint8                    Length);

#endif /* E2E_P01_H */

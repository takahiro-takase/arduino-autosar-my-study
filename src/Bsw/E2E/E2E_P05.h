/**
 * \file    E2E_P05.h
 * \brief   E2E Profile 05 保護ライブラリ (AUTOSAR SWS_E2ELibrary 準拠)
 * \details AUTOSAR E2E Profile 05 の送信保護（Protect）・受信チェック（Check）
 *          機能を提供する。CRC16 (多項式 0x1021、SWS_E2E_00400 準拠) と
 *          8 ビットカウンタ (0-255 循環、予約値なし) によりデータ化け・消失・
 *          重複を検出する。
 *
 *          Profile 01 との主な違い（docs/AUTOSAR_SWS_E2ELibrary.pdf 7.6節参照）:
 *            - CRC が 8bit → 16bit になり、オーバーヘッドが 2byte → 3byte
 *              (CRC16 2byte + Counter 1byte) に増える。
 *            - Counter が 4bit(0-14循環、15は予約値) → 8bit(0-255循環、
 *              予約値なし) になる。ラップアラウンドの mod-15 補正が不要になり、
 *              単純な uint8 の自然なラップアラウンドで済む。
 *            - DataID を CRC に投入する位置が「データの前」(Profile01) から
 *              「データの後」(Profile05、SWS_E2E_00399/00406) に変わる。
 *              投入順序自体 (下位バイト→上位バイト) は Profile01 と同じ
 *              (SWS_E2E_00406 の擬似コードで確認済み)。
 *            - INITIAL/SYNC 状態・SyncCounter 再ロック機構が無い
 *              (E2E_P05ConfigType に SyncCounterInit 相当のフィールドが
 *              存在しない、E2E_P05CheckStateType もシンプル)。初回受信の
 *              特別扱いが無く、初回の E2E_P05Check() 呼び出しも他の呼び出しと
 *              全く同じ delta 計算にそのまま乗る (SWS_E2E_00411-00416)。
 *
 *          本プロジェクトでの適用対象:
 *            EngineHealthStatus (CAN ID 0x220, DLC=5, 送信側で Protect)
 *              byte[0-1] : CRC16 (リトルエンディアン)
 *              byte[2]   : Counter (8bit フル値)
 *              byte[3-4] : 元データ (CrcErrCount, SeqErrCount)
 *            EngineInfo (CAN ID 0x100, DLC=7, 受信側で Check)
 *            AbsInfo    (CAN ID 0x110, DLC=6, 受信側で Check)
 *          Check() は 2026-08 に EngineInfo/AbsInfo の受信検証で実配線された
 *          （`src/Bsw/E2EXf/E2EXf.c` の `E2EXf_InverseTransformP05()` 経由、
 *          呼び出し元は `src/Rte/Rte.c` の `Rte_COMCbk_EngineInfo()`/
 *          `Rte_COMCbk_AbsInfo()`）。EngineHealthStatus 自体は TX のみのため
 *          Check() の呼び出し元にはならない。
 *
 *          本ファイルが実装する Check() 自体は INITIAL 相当の初回受信の
 *          特別扱いを持たない仕様に忠実な実装のままだが（下記 5 章）、
 *          実運用ではそれが原因で起動直後の最初のフレームが誤判定されうる
 *          （送信元 ECU のカウンタが 0 から始まっているとは限らないため）。
 *          このギャップは本ライブラリではなく E2EXf 層
 *          （`E2EXf_RxConfigTypeP05.WaitForFirstData`）で補っている。
 *
 *          CRC16 の開始値について: AUTOSAR SWS_E2ELibrary 本文(7.6.5節)は
 *          「開始値・XOR値は CRC Library 仕様書を参照」としか書いておらず、
 *          本プロジェクトには CRC Library の仕様書 PDF が無いため直接確認は
 *          できない。ただし SWS_E2E_00406 の擬似コードに
 *          `Crc_StartValue16: 0xFFFF` と明記されている（一次資料で確認済み）
 *          ため、本実装もこれに従う。E2E_P01.c が「Crc_CalculateCRC8() の
 *          0xFF 相殺トリックを再現する必要はなく素の 0x00 開始でよい」と
 *          判断したのと同様、本実装の E2E_CalcCrc16() は自動補正の無い素の
 *          実装なので、そのまま crc=0xFFFFU を開始値として渡せば
 *          SWS_E2E_00406 の計算結果と一致する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2E_P05_H
#define E2E_P05_H

#include "Std_Types.h"

/* -----------------------------------------------------------------------
 * E2E P05 チェック結果ステータス
 * AUTOSAR E2E_P05CheckStatusType (8.2.4.4節、Figure 8-8) に準拠した 6 値。
 * Profile01 (E2E_P01StatusType) とはビットパターンが異なる点に注意
 * (ERROR は 0x07、Profile01 は 0x80)。INITIAL/SYNC に相当する状態は無い。
 * ----------------------------------------------------------------------- */
typedef enum
{
    E2E_P05STATUS_OK            = 0x00U, /**< 正常（Counter が前回 +1、CRC 正）              */
    E2E_P05STATUS_NONEWDATA     = 0x01U, /**< 新規データなし（本実装の呼び出し方式では未使用） */
    E2E_P05STATUS_REPEATED      = 0x08U, /**< 同一カウンタが連続（重複受信）                  */
    E2E_P05STATUS_OKSOMELOST    = 0x20U, /**< CRC 正・Counter 進行も許容範囲内だが一部消失あり */
    E2E_P05STATUS_WRONGSEQUENCE = 0x40U, /**< カウンタ飛びが許容超過（過剰消失）              */
    E2E_P05STATUS_ERROR         = 0x07U  /**< CRC 不一致、または NULL ポインタ・DLC 不足等の入力異常 */
} E2E_P05StatusType;

/* -----------------------------------------------------------------------
 * E2E P05 設定構造体
 * DaVinci: /ActiveEcuC/E2EXf/[E2EXf_Profile05]
 * ----------------------------------------------------------------------- */
typedef struct
{
    uint16 DataID;           /**< PDU 識別 ID (例: 0x0220)。CRC 計算にのみ使用され PDU には含まれない */
    uint8  DataLength;       /**< 保護対象 PDU 全体バイト数 (CRC16 2byte 含む)。公式の
                                   E2E_P05ConfigType.DataLength はビット単位(8の倍数)だが、
                                   E2E_P01ConfigType.DataLength と同じ簡略化でバイト単位にしている */
    uint8  MaxDeltaCounter;  /**< 許容するカウンタ飛び幅。Protect 側では未使用 */
    uint8  Offset;           /**< E2E ヘッダ (CRC16+Counter の 3byte) が始まる PDU 内バイトオフセット。
                                   公式の Offset はビット単位(8の倍数)だが同上の理由でバイト単位。
                                   標準バリアント (ヘッダが PDU 先頭) は 0 固定 */
} E2E_P05ConfigType;

/* -----------------------------------------------------------------------
 * E2E P05 送信ステートマシン状態
 * DaVinci: E2E_P05ProtectStateType (8.2.4.2節)
 * ----------------------------------------------------------------------- */
typedef struct
{
    uint8 Counter;  /**< 次回送信するカウンタ値 (8bit フル値、Protect 呼び出しごとに +1、
                          0xFF の次は 0。Profile01 と違い予約値なし) */
} E2E_P05ProtectStateType;

/* -----------------------------------------------------------------------
 * E2E P05 受信ステートマシン状態
 * DaVinci: E2E_P05CheckStateType (8.2.4.3節)
 * ----------------------------------------------------------------------- */
typedef struct
{
    uint8             Counter; /**< 最後に受け付けたカウンタ値 */
    E2E_P05StatusType Status;  /**< 直前チェックの結果        */
} E2E_P05CheckStateType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief  E2E P05 送信ステートを初期化する。
 * \param[out] State  初期化する送信ステート。NULL 禁止。
 */
void E2E_P05ProtectInit(E2E_P05ProtectStateType *State);

/**
 * \brief  送信データに E2E P05 保護（Counter・CRC16）を付与する。
 *
 * \details Data[Config->Offset+2] に現在の Counter を書き込んだ後、
 *          Data[Offset+2..DataLength-1]（Counter を含みユーザーデータまで）
 *          と DataID (下位バイト→上位バイトの順) から CRC16 を計算して
 *          Data[Config->Offset..Offset+1] へリトルエンディアンで書き込む
 *          (SWS_E2E_00405/00406/00407)。呼び出しごとに Counter を +1 する
 *          (8bit リングカウンタ、SWS_E2E_00409)。
 *
 * \param[in]     Config  E2E P05 設定構造体。NULL 禁止。
 * \param[in,out] State   送信ステート。NULL 禁止。
 * \param[in,out] Data    送信 PDU バッファ（Counter/CRC バイトを上書きする）。NULL 禁止。
 * \param[in]     Length  送信 PDU バイト数。Config->DataLength 未満の場合は何もしない。
 */
void E2E_P05Protect(
    const E2E_P05ConfigType *Config,
    E2E_P05ProtectStateType *State,
    uint8                   *Data,
    uint8                    Length);

/**
 * \brief  E2E P05 受信ステートを初期化する。
 * \param[out] State  初期化する受信ステート。NULL 禁止。
 */
void E2E_P05CheckInit(E2E_P05CheckStateType *State);

/**
 * \brief  受信データの E2E P05 チェックを実行する。
 *
 * \details SWS_E2E_00411-00416 の擬似コードに準拠。CRC が一致しない場合は
 *          Counter 側の状態を一切変更しない（次に CRC が正しいフレームが
 *          来た時点で通常通り判定する、E2E_P01Check() と同じ方針）。
 *          Profile01 と異なり INITIAL 相当の特別扱いは無く、初回呼び出しも
 *          通常の delta 計算にそのまま乗る（ヘッダ冒頭コメント参照）。
 *
 * \param[in]  Config  E2E P05 設定構造体。NULL 禁止。
 * \param[io]  State   受信ステート。NULL 禁止。
 * \param[in]  Data    受信 PDU バッファ。NULL 禁止。
 * \param[in]  Length  受信 PDU バイト数。Config->DataLength と一致しない場合は ERROR。
 * \return     E2E_P05StatusType チェック結果。
 */
E2E_P05StatusType E2E_P05Check(
    const E2E_P05ConfigType *Config,
    E2E_P05CheckStateType   *State,
    const uint8              *Data,
    uint8                     Length);

#endif /* E2E_P05_H */

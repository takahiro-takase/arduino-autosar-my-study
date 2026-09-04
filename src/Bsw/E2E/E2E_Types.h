/**
 * \file    E2E_Types.h
 * \brief   E2E ライブラリ共通の戻り値定義 (AUTOSAR SWS_E2ELibrary 準拠)
 * \details [SWS_E2E_00047] は、E2E_P01Protect/Check・E2E_P05Protect/Check・
 *          各 XxxInit すべてが共通で使う `Std_ReturnType` の拡張値レンジを
 *          規定する（Csm/Crypto 等、他モジュールの「Std_ReturnType の拡張値
 *          レンジ」と同じパターン）。実際の検証結果（OK/WRONGCRC/REPEATED 等）
 *          はこの戻り値ではなく、各関数の State 引数（inout）の `Status`
 *          フィールドで返される。この戻り値はあくまで「関数呼び出し自体が
 *          正常に完了したか」を示す。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2E_TYPES_H
#define E2E_TYPES_H

#include "Std_Types.h"

/** [SWS_E2E_00047] Std_ReturnType の拡張値（値は仕様書 7.3 章 Development/
 *  Runtime Error 表の実測値）。E2E_E_WRONGSTATE は `E2E_SMCheck()`
 *  （[SWS_E2E_00371]、`E2E_SM_DEINIT` 状態での呼び出し）が返す。
 *  E2E_E_INPUTERR_WRONG は E2E_P05Protect() の Length 不足検出に使用する
 *  （E2E_P01Protect() は Length 引数自体を持たないため対象外）。 */
#define E2E_E_OK              0x00U
#define E2E_E_INPUTERR_NULL   0x13U
#define E2E_E_INPUTERR_WRONG  0x17U
#define E2E_E_INTERR          0x19U
#define E2E_E_WRONGSTATE      0x1AU

/**
 * \brief   プロファイル非依存のチェック結果型 [SWS_E2E_00347]。
 * \details `E2E_PxxMapStatusToSM()` が各プロファイル固有の詳細ステータス
 *          (`E2E_P01StatusType`/`E2E_P05StatusType` 等) を、`E2E_SMCheck()`
 *          が使う共通形式へ変換した結果を表す。値は仕様書のビットパターンと
 *          一致させている（`E2E_P_NOTAVAILABLE` はバッファの初期化値専用で
 *          どのプロファイルからも返されない、との仕様注記の通り本実装でも
 *          返却しない）。
 */
typedef enum
{
    E2E_P_OK            = 0x00U, /**< このサイクルのチェックは成功（カウンタ検証含む） */
    E2E_P_REPEATED      = 0x01U, /**< カウンタが直前と同一（重複データ）                */
    E2E_P_WRONGSEQUENCE = 0x02U, /**< カウンタ検証以外は成功だが、許容超過のカウンタ飛び */
    E2E_P_ERROR         = 0x03U, /**< カウンタ以外のエラー（CRC不一致・長さ不正等）      */
    E2E_P_NOTAVAILABLE  = 0x04U, /**< 未受信（バッファの初期化値専用。返却されない）      */
    E2E_P_NONEWDATA     = 0x05U  /**< 新規データなし                                     */
} E2E_PCheckStatusType;

/**
 * \brief   E2E ステートマシンの状態型 [SWS_E2E_00343]（値は仕様書の
 *          UML図注記に基づく）。
 */
typedef enum
{
    E2E_SM_VALID   = 0x00U, /**< 通信は許容範囲内 — データを使用してよい          */
    E2E_SM_DEINIT  = 0x01U, /**< E2E_SMCheckInit() 呼び出し前の初期状態           */
    E2E_SM_NODATA  = 0x02U, /**< 初回受信待ち — データを使用しない                */
    E2E_SM_INIT    = 0x03U, /**< 初回受信後、判定確定前 — データを使用しない      */
    E2E_SM_INVALID = 0x04U  /**< 通信は許容範囲外 — データを使用しない            */
} E2E_SMStateType;

/**
 * \brief   E2E ステートマシンの実行時状態 [SWS_E2E_00343]。
 *
 * \details `ProfileStatusWindow` は呼び出し元が `E2E_SMConfigType.WindowSize`
 *          バイト分確保した配列を指すポインタ（実仕様通り、本構造体自体には
 *          配列を持たせない）。`E2E_SMCheckInit()` を呼ぶ前に呼び出し元が
 *          このポインタを設定しておくこと。
 */
typedef struct
{
    uint8*          ProfileStatusWindow; /**< 直近 WindowSize 回分の判定結果を保持する
                                          *   循環バッファ（呼び出し元が確保）。       */
    uint8           WindowTopIndex;      /**< 次に書き込むバッファ位置                */
    uint8           OkCount;             /**< バッファ内の E2E_P_OK 件数              */
    uint8           ErrorCount;          /**< バッファ内の E2E_P_ERROR 件数           */
    E2E_SMStateType SMState;             /**< 現在のステートマシン状態                */
} E2E_SMCheckStateType;

/**
 * \brief   E2E ステートマシンの設定 [SWS_E2E_00342]。
 */
typedef struct
{
    uint8 WindowSize;           /**< 監視ウィンドウのサイズ（ProfileStatusWindow の要素数） */
    uint8 MinOkStateInit;       /**< INIT→VALID 昇格に必要な最小 OK 件数        */
    uint8 MaxErrorStateInit;    /**< INIT に留まれる最大 ERROR 件数              */
    uint8 MinOkStateValid;      /**< VALID 維持に必要な最小 OK 件数              */
    uint8 MaxErrorStateValid;   /**< VALID 維持を許す最大 ERROR 件数             */
    uint8 MinOkStateInvalid;    /**< INVALID→VALID 復帰に必要な最小 OK 件数      */
    uint8 MaxErrorStateInvalid; /**< INVALID→VALID 復帰を許す最大 ERROR 件数     */
} E2E_SMConfigType;

#endif /* E2E_TYPES_H */

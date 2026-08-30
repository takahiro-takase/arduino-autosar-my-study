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
 *  Runtime Error 表の実測値）。E2E_E_WRONGSTATE（プログラムフロー監視違反。
 *  本実装は未実装のため未使用）のみ定義のみで実際には報告しない。
 *  E2E_E_INPUTERR_WRONG は E2E_P05Protect() の Length 不足検出に使用する
 *  （E2E_P01Protect() は Length 引数自体を持たないため対象外）。 */
#define E2E_E_OK              0x00U
#define E2E_E_INPUTERR_NULL   0x13U
#define E2E_E_INPUTERR_WRONG  0x17U
#define E2E_E_INTERR          0x19U
#define E2E_E_WRONGSTATE      0x1AU

#endif /* E2E_TYPES_H */

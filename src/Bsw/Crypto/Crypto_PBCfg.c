/**
 * \file    Crypto_PBCfg.c
 * \brief   Crypto Driver ポストビルド設定データ
 * \details 鍵テーブルの実体を定義する。実際の AUTOSAR 環境ではコンフィギュ
 *          レーションツールが生成するファイルに相当する。
 *
 *          鍵管理の簡略化: 実車は KeyM 等による鍵のプロビジョニング・保護
 *          （耐タンパ格納等）が必須だが、本実装は学習のため固定鍵を
 *          ソースコードへ直接埋め込む簡略化を行っている（本番運用では
 *          絶対に行ってはならない）。元は SecOC_PBCfg.c に直接持っていた
 *          鍵データを、Csm/CryIf/Crypto レイヤ分離の一環として Crypto
 *          Driver 側（本来の所有者）へ移設した。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Crypto_PBCfg.h"

const uint8 Crypto_KeyTable[CRYPTO_KEY_COUNT][CRYPTO_AES128_KEY_SIZE] = {
    /* CRYPTO_KEY_IMMOBILIZER_CMD: ASCII で "KeyFobSecret!!!!" と読める値にして
     * あり、デバッグ時にログ上のバイト列から鍵だと一目でわかるようにしている
     * （学習用の意図的な選択。実車の鍵は当然このような可読値にしない）。 */
    {
        0x4BU,0x65U,0x79U,0x46U,0x6FU,0x62U,0x53U,0x65U,
        0x63U,0x72U,0x65U,0x74U,0x21U,0x21U,0x21U,0x21U
    },
    /* CRYPTO_KEY_E2E_HEALTH_STATUS: ASCII で "TelemetryKey!!!!" と読める値
     * （上記コメント参照。ImmobilizerCmd とは別の鍵であることを一目でわかる
     * ようにするための学習用の意図的な選択）。 */
    {
        0x54U,0x65U,0x6CU,0x65U,0x6DU,0x65U,0x74U,0x72U,
        0x79U,0x4BU,0x65U,0x79U,0x21U,0x21U,0x21U,0x21U
    }
};

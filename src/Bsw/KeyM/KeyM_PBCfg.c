/**
 * \file    KeyM_PBCfg.c
 * \brief   Key Manager ポストビルド設定データ
 * \details 鍵名テーブルの実体を定義する。KeyM_Update() は鍵名しか知らず、
 *          「どの鍵名がどの Csm 側 keyId に対応するか」はこのテーブルが決める
 *          （Csm_PBCfg.c が jobId→鍵の対応を持つのと同じ間接参照パターン）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.4.0 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "KeyM_PBCfg.h"
#include "Crypto_Cfg.h"

const KeyM_CryptoKeyConfigType KeyM_CryptoKeyConfigData[KEYM_CRYPTO_KEY_COUNT] = {
    {
        .KeyName         = KEYM_CRYPTO_KEY_NAME_IMMOBILIZER_CMD,
        .CsmKeyTargetRef = CRYPTO_KEY_IMMOBILIZER_CMD
    }
    /* KEYM_CRYPTO_KEY_NAME_E2E_HEALTH_STATUS は E2EHealthStatus の SecOC 撤去に
     * 伴い削除済み（対応する CRYPTO_KEY_E2E_HEALTH_STATUS が無くなったため）。 */
};

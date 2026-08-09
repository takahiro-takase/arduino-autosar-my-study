/**
 * \file    Csm_PBCfg.c
 * \brief   Crypto Service Manager ポストビルド設定データ
 * \details CsmJob 設定テーブルの実体を定義する。SecOC は jobId しか知らず、
 *          「どのプリミティブでどの鍵を使うか」はこのテーブルが決める
 *          （元は SecOC_PBCfg.c が鍵バイト列を直接持っていたが、
 *          Csm/CryIf/Crypto レイヤ分離の一環でこの間接参照に置き換えた）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Csm_PBCfg.h"
#include "Crypto_Cfg.h"

const Csm_JobConfigType Csm_JobConfigData[CSM_JOB_COUNT] = {
    {
        .JobId       = CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY,
        .Service     = CRYPTO_MACVERIFY,
        .CryptoKeyId = CRYPTO_KEY_IMMOBILIZER_CMD
    }
};

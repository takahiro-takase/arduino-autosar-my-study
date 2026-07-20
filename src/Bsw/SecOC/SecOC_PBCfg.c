/**
 * \file    SecOC_PBCfg.c
 * \brief   SecOC ポストビルド設定データ (AUTOSAR SWS_SecureOnboardCommunication 準拠)
 * \details SecOC のポストビルド設定インスタンス SecOC_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当する。
 *
 *          本プロジェクトの設定（メータ ECU 想定）:
 *            RX Secured I-PDU 0: ImmobilizerCmd (CAN ID 0x120, DLC=6,
 *              新規想定ノード KeyFobEcu からのイモビライザー解除コマンド)
 *              SecOC Profile 1 (24Bit-CMAC-8Bit-FV、
 *              docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf [SWS_SecOC_00192]) 準拠:
 *                byte[0]   : ImmobilizerCmd (Authentic payload, 0x00=LOCK/0x01=UNLOCK)
 *                byte[1]   : Reserved (Authentic payload, 常に 0x00。将来の鍵ID等を想定)
 *                byte[2]   : Freshness Value (8bit、切り詰めなし)
 *                byte[3-5] : 切り詰め MAC (AES-128-CMAC 128bit 出力の上位24bit)
 *              検証成功時、byte[0-1]（Authentic Payload）のみを
 *              Com_RxIndication(ComRxPduId=2) へ転送する
 *              （Com は Freshness/MAC の存在を一切知らない）。
 *
 *          鍵管理の簡略化: 実車は KeyM 等による鍵のプロビジョニング・保護
 *          （耐タンパ格納等）が必須だが、本実装は学習のため固定鍵を
 *          ソースコードへ直接埋め込む簡略化を行っている（本番運用では
 *          絶対に行ってはならない）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "SecOC_PBCfg.h"
#include "SecOC_Cfg.h"

/* AES-128 鍵（16 バイト固定値、上記コメント参照）。
 * ASCII で "KeyFobSecret!!!!" と読める値にしてあり、デバッグ時に
 * ログ上のバイト列から鍵だと一目でわかるようにしている
 * （学習用の意図的な選択。実車の鍵は当然このような可読値にしない）。 */
static const uint8 SecOC_Key_ImmobilizerCmd[SECOC_AES128_KEY_SIZE] = {
    0x4BU,0x65U,0x79U,0x46U,0x6FU,0x62U,0x53U,0x65U,
    0x63U,0x72U,0x65U,0x74U,0x21U,0x21U,0x21U,0x21U
};

static const SecOC_RxPduConfigType SecOC_RxPduConfigData[SECOC_RX_PDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * RX Secured I-PDU 0: ImmobilizerCmd
         * --------------------------------------------------------------- */
        .SecOCRxPduId       = 0U,      /* PduR_PBCfg.c の該当 RxDest.DestPduId と一致 */
        .DataId             = 0x0120U, /* SecOCDataId。CAN ID と同値にして
                                        * 対応関係を分かりやすくする（E2E の
                                        * DataID 割り当てと同じ方針） */
        .AuthenticPduLength = 2U,      /* byte[0]=ImmobilizerCmd, byte[1]=Reserved */
        .FreshnessOffset    = 2U,
        .FreshnessLength    = 1U,      /* 8bit、切り詰めなし（SecOC_Types.h 参照） */
        .MacOffset          = 3U,
        .MacTxLength        = 3U,      /* 24bit（SecOC Profile 1） */
        .SecuredPduLength   = 6U,      /* 2 + 1 + 3 */
        .Key                = SecOC_Key_ImmobilizerCmd,
        .ComRxPduId         = 2U       /* Com RX IPduId=2 (SecureCommand_Rx) */
    }
};

const SecOC_ConfigType SecOC_Config = {
    .RxPdus     = SecOC_RxPduConfigData,
    .RxPduCount = SECOC_RX_PDU_COUNT
};

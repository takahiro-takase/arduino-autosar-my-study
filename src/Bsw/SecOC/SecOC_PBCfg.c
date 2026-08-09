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
 *            TX Secured I-PDU: 現在無し (SECOC_TX_PDU_COUNT=0)。
 *              以前は E2EHealthStatus (CAN ID 0x220) を「内側=E2E（意図しない
 *              誤り検出）、外側=SecOC（意図的な改ざん・なりすまし検出）」という
 *              二重防御で保護していたが、E2E の検出能力を高めるため
 *              Profile01(CRC8)→Profile05(CRC16) へ切り替えるにあたり、
 *              classic CAN の DLC=8 上限に収まらなくなったため SecOC 側を撤去し
 *              E2E Profile05 単体保護とした（E2EXf_PBCfg.c/PduR_PBCfg.c 参照）。
 *
 *          鍵管理の簡略化: 実車は KeyM 等による鍵のプロビジョニング・保護
 *          （耐タンパ格納等）が必須だが、本実装は学習のため固定鍵を
 *          ソースコードへ直接埋め込む簡略化を行っている（本番運用では
 *          絶対に行ってはならない）。ただし本 PBCfg は「どの鍵を使うか」を
 *          知らない（Csm/CryIf/Crypto レイヤ分離により、鍵バイト列の実体は
 *          Crypto_PBCfg.c へ移設済み）。ここでは CsmJobId（Csm_PBCfg.c の
 *          Csm_JobConfigData を検索するキー）のみを指定する。現状 RX の
 *          ImmobilizerCmd のみが SecOC を使うため、鍵は Csm 側の設定
 *          （CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY → CRYPTO_KEY_IMMOBILIZER_CMD）
 *          で表現している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "SecOC_PBCfg.h"
#include "SecOC_Cfg.h"
#include "Csm_Cfg.h"

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
        .CsmJobId           = CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY,
        .ComRxPduId         = 2U       /* Com RX IPduId=2 (SecureCommand_Rx) */
    }
};

/* TX 方向で SecOC を使う PDU は現在無い (SECOC_TX_PDU_COUNT=0、SecOC_Cfg.h 参照)。
 * サイズ0の配列宣言 (`T arr[0]`) は ISO C が認めていない GNU 拡張であり、
 * 空初期化子 `={}` はさらに C23 相当で、より厳格な C dialect や別コンパイラでの
 * ビルドに対して脆い。配列自体を宣言せず、ポインタを NULL・件数を 0 にする
 * （SecOC.c 側は TxPduCount を上限にループするため NULL を参照することはない）。
 * TX PDU が実際に追加された時点で、配列宣言とこの初期化子を書き戻すこと。 */
const SecOC_ConfigType SecOC_Config = {
    .RxPdus     = SecOC_RxPduConfigData,
    .RxPduCount = SECOC_RX_PDU_COUNT,
    .TxPdus     = NULL,
    .TxPduCount = SECOC_TX_PDU_COUNT
};

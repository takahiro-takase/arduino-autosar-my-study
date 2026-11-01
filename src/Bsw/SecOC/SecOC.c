/**
 * \file    SecOC.c
 * \brief   Secure Onboard Communication 実装 (AUTOSAR SWS_SecureOnboardCommunication 準拠)
 * \details PduR のルーティング経路上に直接挟まる、RX（受信検証）専用の簡易 SecOC。
 *          E2E とは異なり、Com のコールバックフック（RxIndicationCbk 等）経由
 *          ではなく、PduR の宛先モジュール（PduR_RxDestType、CanTp/Com と同じ
 *          立ち位置）として実装している。これは E2E Transformer 方式が E2E
 *          固有の統合方式であり、SecOC には対応する「Com フック経由」の
 *          統合方式が実 AUTOSAR に存在しないため（SecOC は常に独立した PduR
 *          モジュールとして構成される）。
 *
 *          Secured I-PDU を受信すると、Authentic Payload / Freshness Value /
 *          切り詰め MAC に分離し、Csm_MacVerify()（Csm/CryIf/Crypto レイヤ
 *          経由で AES-128-CMAC を再計算）で MAC が一致するか照合、続けて
 *          フレッシュネスの単調増加を確認する（リプレイ攻撃対策）。
 *          SecOC 自身は鍵にも AES/CMAC の実装にも一切アクセスしない
 *          （Csm/CryIf/Crypto レイヤ分離の詳細は src/Bsw/Csm,CryIf,Crypto
 *          配下を参照）。両方成功した場合のみ Authentic Payload を
 *          Com_RxIndication() へ転送する。検証に失敗したデータは Com へ
 *          一切渡さない。RX 専用フレーム（ImmobilizerCmd）は「外部の
 *          KeyFobEcu から受信する」想定で、送信側は uds_tester（Python、
 *          pycryptodome で本物の AES-CMAC を計算する）が模擬する。
 *
 *          TX（自ら Secured I-PDU を生成して送信する）方向の機構自体も実装して
 *          いるが、現在これを使う PDU は無い（SECOC_TX_PDU_COUNT=0、
 *          SecOC_Cfg.h/SecOC_PBCfg.c 参照）。以前は E2EHealthStatus（CAN 0x220）
 *          がこの経路で保護されていたが、E2E を Profile01（CRC8）から
 *          Profile05（CRC16）へ強化した際に DLC が classic CAN の 8byte 上限を
 *          超えるため撤去した（詳細は docs/modules/SecOC_Notes.md、
 *          docs/modules/E2E_Profile5_Notes.md 参照）。TX Pdu が実際に追加されるまで、
 *          `SecOC_MainFunctionTx()` の TX ループは毎回 0 回実行で終わる。
 *          RX とは異なり PduR の TX 経路
 *          （PduR_TxRoutingPathType.TransmitOverrideFct）に中間モジュールとして
 *          挟まる構成（[7.4.1] "Authentication during direct transmission"）。
 *          Com が PduR_ComTransmit() を呼ぶと SecOC_IfTransmit() が Authentic
 *          I-PDU を内部バッファへコピーして即座に返り（[SWS_SecOC_00058]）、
 *          次回 SecOC_MainFunctionTx() で Freshness/MAC を計算して Secured
 *          I-PDU を組み立て、PduR_SecOCTransmit() で CanIf まで送り届ける
 *          （[SWS_SecOC_00060]〜[SWS_SecOC_00062]）。
 *
 *          新鮮性値（Freshness Value）の生成・検証について（意図的な簡略化、
 *          2026-09 サーベイで検討・見送り確定）: 実仕様は
 *          `SecOCQueryFreshnessValue`（CFUNC/RTE）に応じて、SecOC が
 *          `SecOC_GetTxFreshness()`/`SecOC_GetRxFreshness()`（またはRTE経由の
 *          `FreshnessManagement_GetTxFreshness()`等）という外部コールアウト
 *          関数を必ず呼び出す構造を規定する（[SWS_SecOC_00221]〜[00224]）。
 *          さらに、そのコールアウトが E_BUSY を返した場合の認証ビルド/検証
 *          試行カウンタによるリトライ機構（[SWS_SecOC_00225]〜[00237]）と、
 *          送信成功を通知する `SecOC_SPduTxConfirmation()`（[SWS_SecOC_00232]/
 *          [00233]）まで一式で構成される。本実装はこの外部コールアウト境界
 *          を設けず、`SecOC_RxIndication()`/`SecOC_MainFunctionTx()` 内で
 *          `SecOC_LastFreshness[]`/`SecOC_TxFreshness[]` という単調増加
 *          カウンタを直接参照・更新する（完全同期処理のため E_BUSY が原理的
 *          に発生しない設計であり、上記リトライ機構自体が本質的に不要）。
 *          この関数境界の欠如自体は仕様乖離だが、上記一式をフル実装するのは
 *          本プロジェクトの規模に対して過大と判断し、意図的な簡略化として
 *          維持することとした。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "SecOC.h"
#include "SecOC_Cfg.h"
#include "Csm.h"
#include "Com.h"
#include "PduR_SecOC.h"
#include "Det.h"
#include <string.h>

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "SecOC"

/* Authenticator 対象データ（DataId | AuthenticPayload | FreshnessValue）を
 * 組み立てる作業バッファの上限。CAN フレーム（最大 DLC=8）に収まる構成である
 * 限り、2(DataId) + 8(AuthenticPayload 最大) + 4(Freshness 最大) = 14 で
 * 十分収まるが、余裕を見て 16（AES の 1 ブロック長）とする。 */
#define SECOC_AUTH_INPUT_MAX  16U

/* TX Authentic I-PDU を保持するバッファの上限。CAN DLC 上限 8 バイトから
 * Freshness(最低1B)+MAC(最低1B) を除いた分以下に収まる。現在 TX 方向で
 * SecOC を使う PDU は無い（上記ファイル冒頭コメント参照）が、将来 TX Pdu が
 * 追加された際にも対応できるよう CAN DLC 上限ベースの余裕を持たせたまま
 * 残している。 */
#define SECOC_TX_AUTH_BUF_MAX  8U

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static const SecOC_ConfigType* SecOC_ConfigPtr = NULL;

/* Secured I-PDU ごとの最後に受理した Freshness Value（リプレイ検知用）。
 * SecOC_HasBaseline が 0 の間は「まだ 1 度も検証成功していない」ことを示し、
 * 初回受信時はどんな Freshness Value でも（単調増加チェックをスキップして）
 * 受理する。 */
static uint8 SecOC_LastFreshness[SECOC_RX_PDU_COUNT];
static uint8 SecOC_HasBaseline[SECOC_RX_PDU_COUNT];

/* オーバーライド未設定を示すセンチネル値（実仕様の overrideStatus 値
 * 0/1/2/41 のいずれとも衝突しない）。 */
#define SECOC_OVERRIDE_NONE  0xFFU

/* Secured I-PDU (= SecOC_VerifyStatusOverride() の freshnessValueID) ごとの
 * VerifyStatus 強制状態。SecOC_VerifyStatusOverride() の Doxygen 参照。
 * SecOC_OverrideStatus: SECOC_OVERRIDE_NONE または SECOC_OVERRIDE_FAIL_INDEFINITE/
 * SECOC_OVERRIDE_FAIL_COUNTED のいずれか（41=Pass強制は非対応のため格納されない）。
 * SecOC_OverrideRemaining: SECOC_OVERRIDE_FAIL_COUNTED のときのみ有効な残り回数。 */
static uint8 SecOC_OverrideStatus[SECOC_RX_PDU_COUNT];
static uint8 SecOC_OverrideRemaining[SECOC_RX_PDU_COUNT];

/* TX Secured I-PDU ごとの内部状態。
 * SecOC_TxAuthenticBuffer : SecOC_IfTransmit() がコピーした Authentic I-PDU。
 * SecOC_TxPending         : 「次回 SecOC_MainFunctionTx() で変換・送信すべき
 *                           データがある」フラグ（Com_TxPending と同じ
 *                           「実処理を呼び出し元のスタックフレームから切り離す」
 *                           設計思想。もっとも SecOC_IfTransmit() は割り込み
 *                           禁止区間から呼ばれないため WDT リセットの懸念は
 *                           ないが、[SWS_SecOC_00060]〜[SWS_SecOC_00062] が
 *                           要求する「scheduled main function で変換する」
 *                           という仕様上のタイミングそのものを再現するために
 *                           あえて遅延させる）。
 * SecOC_TxFreshness       : 送信側が保持する単調増加カウンタ（次回送信に使う
 *                           値）。8bit で折り返す。
 *
 * 現在 SECOC_TX_PDU_COUNT は 0（TX 方向で SecOC を使う PDU が無い）。
 * サイズ0の配列宣言 (`T arr[0]`) は ISO C が認めていない GNU 拡張のため、
 * 下記の配列は SECOC_TX_STATE_STORAGE_COUNT（最低 1）で確保する。実際の
 * 有効要素数は SECOC_TX_PDU_MAX（既定は SECOC_TX_PDU_COUNT。下を参照）で、
 * 全ループがこれを上限にするため、確保だけされた添字0は SECOC_TX_PDU_MAX>0 に
 * なるまで一切参照されない（無害）。 */
#ifndef SECOC_TX_PDU_MAX
/* SecOC_Init() が受け付ける TX Secured I-PDU の最大数。既定は SECOC_TX_PDU_COUNT（本番の設定の数）。
 * 単体テストのビルド（CMakeLists.txt）は、TX 方向を確認するために、これを 1 へ上書きする。 */
#define SECOC_TX_PDU_MAX  SECOC_TX_PDU_COUNT
#endif
#define SECOC_TX_STATE_STORAGE_COUNT  (((SECOC_TX_PDU_MAX) > 0U) ? (SECOC_TX_PDU_MAX) : 1U)
static uint8 SecOC_TxAuthenticBuffer[SECOC_TX_STATE_STORAGE_COUNT][SECOC_TX_AUTH_BUF_MAX];
static uint8 SecOC_TxPending[SECOC_TX_STATE_STORAGE_COUNT];
static uint8 SecOC_TxFreshness[SECOC_TX_STATE_STORAGE_COUNT];

/* SecOC_TxSecuredBuffer : SecOC_MainFunctionTx() が組み立てた Secured I-PDU（Authentic I-PDU、Freshness Value、
 *                         切り詰めた MAC）。SecOC_TriggerTransmit() が、下位層へ複製する（[SWS_SecOC_00068]）。
 * SecOC_TxSecuredValid    : SecOC_TxSecuredBuffer に、組み立て済みの Secured I-PDU がある（1）。
 *                         SecOC_MainFunctionTx() の組み立てで 1、SecOC_Init()/SecOC_DeInit() で 0。 */
static uint8 SecOC_TxSecuredBuffer[SECOC_TX_STATE_STORAGE_COUNT][SECOC_TX_AUTH_BUF_MAX];
static uint8 SecOC_TxSecuredValid[SECOC_TX_STATE_STORAGE_COUNT];

/* SecOC_TxTriggered : 組み立てた Secured I-PDU を、SecOC_TriggerTransmit() が下位層へ渡した（トリガ送信で取り出された）。
 *                     SecOC_TxConfirmation() が E_OK で呼ばれたとき、1 なら、保持している Authentic I-PDU を、
 *                     上位層が新しく設定したものとして扱い、次の SecOC_MainFunctionTx() で再び認証する
 *                     （[SWS_SecOC_00069]、7.4.2 節）。取り出された Secured I-PDU は、下位層へ渡した時点で、解放する。
 *                     SecOC_Init()/SecOC_DeInit() で 0。 */
static uint8 SecOC_TxTriggered[SECOC_TX_STATE_STORAGE_COUNT];

/* SecOC_TxBuildAttempts : 送信要求ごとの、認証の組み立ての試行の回数（authentication build counter）。
 *                         Csm_MacGenerate() が回復可能なエラーを返したときに増やす（[SWS_SecOC_00225]〜[SWS_SecOC_00227]）。
 *                         新しい送信要求（SecOC_IfTransmit()）で 0（[SWS_SecOC_00226]）、認証の成功でも 0
 *                         （成功の後の、トリガ送信の確認による再認証を、新しい送信要求として扱うため。[SWS_SecOC_00069]）。 */
static uint16 SecOC_TxBuildAttempts[SECOC_TX_STATE_STORAGE_COUNT];

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * SecOC_FindRxPdu
 * ---------------------------------------------------------------------- */

/**
 * \brief   SecOC_RxPduConfigType テーブルから SecOCRxPduId に一致するエントリを検索する。
 *
 * \details Com_FindRxIPdu() 等、本プロジェクトの他モジュールと同じ「配列添字に
 *          暗黙依存せず、明示 ID フィールドで線形検索する」方針に倣う。
 *
 * \param[in]   rxPduId    検索する SecOC RX Secured I-PDU の ID。
 * \param[out]  tableIndex 見つかった場合、テーブル内インデックスを書き込む
 *                         （フレッシュネス状態配列の添字に使う）。NULL 禁止。
 *
 * \return  一致するエントリへのポインタ。見つからない場合は NULL。
 */
static const SecOC_RxPduConfigType* SecOC_FindRxPdu(PduIdType rxPduId, uint8* tableIndex)
{
    for (uint8 i = 0U; i < SecOC_ConfigPtr->RxPduCount; i++)
    {
        if (SecOC_ConfigPtr->RxPdus[i].SecOCRxPduId == rxPduId)
        {
            *tableIndex = i;
            return &SecOC_ConfigPtr->RxPdus[i];
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------------
 * SecOC_ApplyVerifyStatusOverride
 * ---------------------------------------------------------------------- */

/**
 * \brief   実際の検証結果に SecOC_VerifyStatusOverride() のオーバーライド設定を適用する。
 *
 * \details [SWS_SecOC_00122] の Note「オーバーライド中でも CSM による実際の
 *          検証は継続する」を踏まえ、呼び出し元（SecOC_RxIndication()）は
 *          実際の MAC/フレッシュネス検証を必ず先に完了させ、その結果を
 *          `actualPass` として渡すこと。回数指定オーバーライド
 *          (SECOC_OVERRIDE_FAIL_COUNTED) は呼び出しのたびに残り回数を消費し、
 *          0 に達したら自動的に解除する。
 *
 * \param[in]  tableIndex  SecOC_FindRxPdu() で得たテーブルインデックス。
 * \param[in]  actualPass  実際の検証結果 (1=成功, 0=失敗)。
 *
 * \return  最終的な VerifyStatus (1=成功として扱う, 0=失敗として扱う)。
 *          本実装は overrideStatus=41 (Pass 強制) を非対応とするため、
 *          `actualPass` を成功から失敗へ強制することはあっても、その逆
 *          （失敗を成功にする）ことはない。
 */
static uint8 SecOC_ApplyVerifyStatusOverride(uint8 tableIndex, uint8 actualPass)
{
    const uint8 ov = SecOC_OverrideStatus[tableIndex];

    if (ov == SECOC_OVERRIDE_NONE)
    {
        return actualPass;
    }

    if (ov == SECOC_OVERRIDE_FAIL_INDEFINITE)
    {
        return 0U;
    }

    /* ここに来るのは SECOC_OVERRIDE_FAIL_COUNTED のみ
     * (41=Pass 強制は SecOC_VerifyStatusOverride() 側で常に拒否するため、
     *  SecOC_OverrideStatus[] へ格納されることがない)。 */
    if (SecOC_OverrideRemaining[tableIndex] > 0U)
    {
        SecOC_OverrideRemaining[tableIndex]--;
        if (SecOC_OverrideRemaining[tableIndex] == 0U)
        {
            SecOC_OverrideStatus[tableIndex] = SECOC_OVERRIDE_NONE;
        }
        return 0U;
    }

    SecOC_OverrideStatus[tableIndex] = SECOC_OVERRIDE_NONE;
    return actualPass;
}

/* ----------------------------------------------------------------------
 * SecOC_FindTxPdu
 * ---------------------------------------------------------------------- */

/**
 * \brief   SecOC_TxPduConfigType テーブルから SecOCTxPduId に一致するエントリを検索する。
 *
 * \details SecOC_FindRxPdu() の TX 版。同じ「配列添字に暗黙依存せず、明示 ID
 *          フィールドで線形検索する」方針に倣う。
 *
 * \param[in]   txPduId    検索する SecOC TX Secured I-PDU の ID。
 * \param[out]  tableIndex 見つかった場合、テーブル内インデックスを書き込む
 *                         （TX 状態配列の添字に使う）。NULL 禁止。
 *
 * \return  一致するエントリへのポインタ。見つからない場合は NULL。
 */
static const SecOC_TxPduConfigType* SecOC_FindTxPdu(PduIdType txPduId, uint8* tableIndex)
{
    for (uint8 i = 0U; i < SecOC_ConfigPtr->TxPduCount; i++)
    {
        if (SecOC_ConfigPtr->TxPdus[i].SecOCTxPduId == txPduId)
        {
            *tableIndex = i;
            return &SecOC_ConfigPtr->TxPdus[i];
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------------
 * SecOC_Init
 * ---------------------------------------------------------------------- */

void SecOC_Init(const SecOC_ConfigType* config)
{
    if (config == NULL)
    {
        DET_LOGE(TAG, "Init E: config NULL");
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_INIT, SECOC_E_PARAM_POINTER);
        return;
    }
    if (config->RxPduCount > SECOC_RX_PDU_COUNT)
    {
        /* [SWS_SecOC_00101]: 設定パラメータ不正時は SECOC_E_INIT_FAILED を
         * 報告する（2026-09 追加。以前はログのみでDET報告していなかった。
         * SecOC_Cfg.h の SECOC_E_INIT_FAILED コメント参照）。 */
        DET_LOGE(TAG, "Init E: RxPduCount>max");
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_INIT, SECOC_E_INIT_FAILED);
        return;
    }
    if (config->TxPduCount > SECOC_TX_PDU_MAX)
    {
        /* [SWS_SecOC_00101]（RxPduCount と同じ理由。2026-09 追加） */
        DET_LOGE(TAG, "Init E: TxPduCount>max");
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_INIT, SECOC_E_INIT_FAILED);
        return;
    }

    SecOC_ConfigPtr = config;

    for (uint8 i = 0U; i < SECOC_RX_PDU_COUNT; i++)
    {
        SecOC_LastFreshness[i]    = 0U;
        SecOC_HasBaseline[i]      = 0U;
        SecOC_OverrideStatus[i]   = SECOC_OVERRIDE_NONE;
        SecOC_OverrideRemaining[i] = 0U;
    }

    /* cppcheck-suppress unsignedLessThanZero */
    for (uint8 i = 0U; i < SECOC_TX_PDU_MAX; i++)
    {
        SecOC_TxPending[i]      = 0U;
        SecOC_TxFreshness[i]    = 0U;
        SecOC_TxSecuredValid[i] = 0U;
        SecOC_TxTriggered[i]    = 0U;
        SecOC_TxBuildAttempts[i] = 0U;
    }

    DET_LOGI(TAG, "Init ok RX=%u TX=%u", (unsigned)config->RxPduCount, (unsigned)config->TxPduCount);
}

/* ----------------------------------------------------------------------
 * SecOC_DeInit
 * ---------------------------------------------------------------------- */

void SecOC_DeInit(void)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_DEINIT, SECOC_E_UNINIT);
        return;
    }

    /* [SWS_SecOC_00157]: 全ての内部グローバル変数と Secured I-PDU の
     * バッファをクリアする。RX 側の状態(SecOC_Init() が初期化する分)に加え、
     * TX バッファ(Init は未クリア、SecOC.c 冒頭コメント参照)も対象に含める。
     * TX 側は SECOC_TX_PDU_COUNT ではなく実際の配列確保サイズ
     * (SECOC_TX_STATE_STORAGE_COUNT) を使い、将来 TX Pdu が追加された場合にも
     * 取りこぼしなくクリアされるようにする。 */
    for (uint8 i = 0U; i < SECOC_RX_PDU_COUNT; i++)
    {
        SecOC_LastFreshness[i]     = 0U;
        SecOC_HasBaseline[i]       = 0U;
        SecOC_OverrideStatus[i]    = SECOC_OVERRIDE_NONE;
        SecOC_OverrideRemaining[i] = 0U;
    }

    /* cppcheck-suppress unsignedLessThanZero */
    for (uint8 i = 0U; i < SECOC_TX_STATE_STORAGE_COUNT; i++)
    {
        SecOC_TxPending[i]      = 0U;
        SecOC_TxFreshness[i]    = 0U;
        SecOC_TxSecuredValid[i] = 0U;
        SecOC_TxTriggered[i]    = 0U;
        SecOC_TxBuildAttempts[i] = 0U;
    }
    (void)memset(SecOC_TxAuthenticBuffer, 0, sizeof(SecOC_TxAuthenticBuffer));
    (void)memset(SecOC_TxSecuredBuffer, 0, sizeof(SecOC_TxSecuredBuffer));

    SecOC_ConfigPtr = NULL;
    DET_LOGI(TAG, "DeInit ok");
}

/* ----------------------------------------------------------------------
 * SecOC_RxIndication
 * ---------------------------------------------------------------------- */

void SecOC_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_RX_INDICATION, SECOC_E_UNINIT);
        return;
    }

    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL))
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_RX_INDICATION, SECOC_E_PARAM_POINTER);
        return;
    }

    uint8 tableIndex = 0U;
    const SecOC_RxPduConfigType* cfg = SecOC_FindRxPdu(RxPduId, &tableIndex);
    if (cfg == NULL)
    {
        DET_LOGW(TAG, "RxInd W: no matching SecOC RX PDU for id=%u", (unsigned)RxPduId);
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_RX_INDICATION, SECOC_E_INVALID_PDU_SDU_ID);
        return;
    }

    if (PduInfoPtr->SduLength < cfg->SecuredPduLength)
    {
        DET_LOGW(TAG, "RxInd W: iPdu=%u length %u < expected %u, discard",
                 (unsigned)RxPduId, (unsigned)PduInfoPtr->SduLength, (unsigned)cfg->SecuredPduLength);
        return;
    }

    const uint16 authInputLen = (uint16)(2U + cfg->AuthenticPduLength + cfg->FreshnessLength);
    if (authInputLen > SECOC_AUTH_INPUT_MAX)
    {
        DET_LOGE(TAG, "RxInd E: iPdu=%u authInputLen=%u exceeds buffer",
                 (unsigned)RxPduId, (unsigned)authInputLen);
        return;
    }

    const uint8* secured = PduInfoPtr->SduDataPtr;

    /* DataToAuthenticator = DataId(2byte, Big Endian) | Authentic Payload |
     * Complete Freshness Value
     * (docs/autosar/4.3.1/AUTOSAR_SWS_SecureOnboardCommunication.pdf [7.1.1.2] 1707行、
     * Big Endian は [SWS_SecOC_00011]) */
    uint8 authInput[SECOC_AUTH_INPUT_MAX];
    authInput[0] = (uint8)(cfg->DataId >> 8);
    authInput[1] = (uint8)(cfg->DataId & 0xFFU);
    for (uint8 b = 0U; b < cfg->AuthenticPduLength; b++)
    {
        authInput[2U + b] = secured[b];
    }
    for (uint8 b = 0U; b < cfg->FreshnessLength; b++)
    {
        authInput[2U + cfg->AuthenticPduLength + b] = secured[cfg->FreshnessOffset + b];
    }

    /* 切り詰め MAC は AES-CMAC 128bit 出力の上位（MSB側）MacTxLength バイトを
     * 比較する（[SWS_SecOC_00192]、Figure 5 "truncated down to the most
     * significant bits"）。実際の再計算・定数時間比較は Csm_MacVerify() →
     * CryIf_ProcessJob() → Crypto_ProcessJob() が担う（SecOC は鍵にも
     * AES/CMAC の実装にも一切アクセスしない）。macLength は Csm_MacVerify()
     * の規約によりビット単位で渡す（Csm.c 参照）。 */
    Crypto_VerifyResultType verifyResult = CRYPTO_E_VER_NOT_OK;
    const Std_ReturnType csmRet = Csm_MacVerify(cfg->CsmJobId, CRYPTO_OPERATIONMODE_SINGLECALL,
                                                 authInput, authInputLen,
                                                 &secured[cfg->MacOffset], (uint32)cfg->MacTxLength * 8U,
                                                 &verifyResult);
    if (csmRet != E_OK)
    {
        /* [SWS_SecOC_00166]: CSM から認証/検証サービス自体を最終的に得られ
         * なかった場合（MAC 不一致という正常な検証結果とは別の、下位層
         * （Csm/CryIf/Crypto）自体の異常）は SECOC_E_CRYPTO_FAILURE を
         * 報告する（2026-09 追加。以前はこの区別をせず両者を「MAC
         * verification failed」の WARN ログに一括していた）。 */
        DET_LOGE(TAG, "RxInd E: iPdu=%u Csm_MacVerify() service failed", (unsigned)RxPduId);
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_RX_INDICATION, SECOC_E_CRYPTO_FAILURE);
    }

    const uint8 macOk = (csmRet == E_OK && (verifyResult == CRYPTO_E_VER_OK)) ? 1U : 0U;
    if ((macOk == 0U) && (csmRet == E_OK))
    {
        DET_LOGW(TAG, "RxInd W: iPdu=%u MAC verification failed (tampered or wrong key)",
                 (unsigned)RxPduId);
    }

    /* フレッシュネス検証（リプレイ検知）。本実装は FreshnessLength=1（8bit）
     * 固定のみサポートする（SecOC_Types.h の簡略化に関する説明参照）。
     * 単調増加チェックは折り返し（wraparound）を考慮した「半区間」判定
     * （received-last の差が 1〜127 なら「より新しい」とみなす）を用いる。
     * 差が 0（完全一致 = リプレイ）または 128 以上（古い方向、または折り返し
     * 境界で新旧が確定できない）は拒否する。初回受信（SecOC_HasBaseline=0）は
     * 比較対象がないため、この検証自体をスキップして無条件に受理する。
     * MAC 検証が既に失敗している場合はフレッシュネスの正否は最終判定に影響
     * しない（actualPass は macOk との論理積のため）ので判定自体を省略する
     * （攻撃者制御下の任意バイトに対する「replay or stale」の紛らわしい
     * 二重ログを防ぐ）。 */
    const uint8 freshness = secured[cfg->FreshnessOffset];
    uint8 freshnessOk = 0U;
    if (macOk != 0U)
    {
        freshnessOk = 1U;
        if (SecOC_HasBaseline[tableIndex] != 0U)
        {
            const uint8 delta = (uint8)(freshness - SecOC_LastFreshness[tableIndex]);
            if ((delta == 0U) || (delta >= 128U))
            {
                freshnessOk = 0U;
                DET_LOGW(TAG, "RxInd W: iPdu=%u freshness check failed (replay or stale, got=%u last=%u)",
                         (unsigned)RxPduId, (unsigned)freshness, (unsigned)SecOC_LastFreshness[tableIndex]);
            }
        }
    }

    /* [SWS_SecOC_00122]: SecOC_VerifyStatusOverride() が設定した強制状態を、
     * 実際の検証結果 (actualPass) に適用して最終判定 (effectivePass) を得る
     * (SecOC_ApplyVerifyStatusOverride() の Doxygen 参照)。 */
    const uint8 actualPass    = (((macOk != 0U) && (freshnessOk != 0U)) ? 1U : 0U);
    const uint8 effectivePass = SecOC_ApplyVerifyStatusOverride(tableIndex, actualPass);

    /* [SWS_SecOC_00048]/[SWS_SecOC_00119]: 検証の都度、設定済みなら
     * VerificationStatusCallout() を呼ぶ。報告する種別は effectivePass
     * （SecOC_VerifyStatusOverride() 適用後）に基づく——[SWS_SecOC_00243]の
     * 注記「オーバーライド使用中は Freshness Management が実際の結果と
     * 食い違いうる」のとおり、実仕様上もオーバーライドの影響を受ける通知
     * だからである。オーバーライドで Fail に強制された場合の具体的な理由は
     * MAC/Freshness いずれかに一意に決まらないため、汎用の
     * SECOC_VERIFICATIONFAILURE を報告する（macOk=0 の場合と結果的に同じ
     * 分類のため else 側にまとめる。/simplify 指摘で4分岐から3分岐に整理）。
     * コールアウト未設定（大多数の RX PDU）では分類自体が無駄になるため、
     * NULL チェックの内側でのみ計算する。 */
    if (cfg->VerificationStatusCallout != NULL)
    {
        const SecOC_VerificationResultType verificationStatus =
            (effectivePass != 0U)  ? SECOC_VERIFICATIONSUCCESS :
            ((freshnessOk == 0U) && (macOk != 0U)) ? SECOC_FRESHNESSFAILURE :
            SECOC_VERIFICATIONFAILURE;

        const uint8 propagate =
            ((cfg->VerificationStatusPropagationMode == SECOC_VERIFICATION_STATUS_PROPAGATION_BOTH)
             || ((cfg->VerificationStatusPropagationMode == SECOC_VERIFICATION_STATUS_PROPAGATION_FAILURE_ONLY)
                 && (verificationStatus != SECOC_VERIFICATIONSUCCESS))) ? 1U : 0U;
        if ((propagate) != 0U)
        {
            const SecOC_VerificationStatusType status = {
                .freshnessValueID  = (uint16)RxPduId,  /* SecOC_VerifyStatusOverride()と同じ簡略化 */
                .verificationStatus = verificationStatus,
                .secOCDataId       = cfg->DataId
            };
            cfg->VerificationStatusCallout(status);
        }
    }

    /* フレッシュネス基準値は「実際に」検証が成功した場合 (actualPass) に
     * 更新する。Com への転送可否は下記の effectivePass に従うが、Fail に
     * 強制された場合でも actualPass が真（＝本物の正当なトラフィック）なら
     * 基準値は進めておかないと、オーバーライド解除後に基準値が古いまま残り、
     * 以後の正当なメッセージまで「リプレイ/古い」と誤判定してしまう。 */
    if (actualPass != 0U)
    {
        SecOC_LastFreshness[tableIndex] = freshness;
        SecOC_HasBaseline[tableIndex]   = 1U;
    }

    if (effectivePass == 0U)
    {
        if (actualPass != 0U)
        {
            DET_LOGW(TAG, "RxInd W: iPdu=%u verification succeeded but forced to Fail by SecOC_VerifyStatusOverride()",
                     (unsigned)RxPduId);
        }
        return;
    }

    DET_LOGI(TAG, "RxInd: iPdu=%u verified OK (freshness=%u)", (unsigned)RxPduId, (unsigned)freshness);

    /* 検証成功: Authentic Payload（Secured I-PDU の先頭 AuthenticPduLength
     * バイト）のみを Com へ渡す。Freshness/MAC バイトは Com の関知するところ
     * ではない（E2E Transformer 方式で Com が E2E の存在を知らないのと
     * 同じ設計思想）。 */
    PduInfoType authenticPduInfo = {
        .SduDataPtr = PduInfoPtr->SduDataPtr,
        .SduLength  = cfg->AuthenticPduLength
    };
    Com_RxIndication(cfg->ComRxPduId, &authenticPduInfo);
}

/* ----------------------------------------------------------------------
 * SecOC_VerifyStatusOverride
 * ---------------------------------------------------------------------- */

/**
 * \brief   指定 Freshness Value の VerifyStatus を強制上書きする（[SWS_SecOC_00122]）。
 *
 * \details 実仕様はテスト・診断目的で「実際の認証結果に関わらず検証結果を
 *          強制する」任意サービス。CSM による実際の MAC 検証・フレッシュネス
 *          検証は本関数の呼び出し有無に関わらず常に行われる（本実装は
 *          `SecOC_RxIndication()` 内で実際の検証結果を求めたのち、本関数が
 *          設定したオーバーライド状態を適用して最終判定とする）。
 *
 *          実仕様は個別の Freshness Value を識別する `freshnessValueId`
 *          という専用の設定要素（Freshness Value Manager 相当）を持つが、
 *          本プロジェクトはそのような機構自体を持たず、Freshness Value は
 *          RX Secured I-PDU ごとに 1 対 1 で紐づく（`SecOC_RxPduConfigType`
 *          参照）。そのため本実装では `freshnessValueID` を対象の
 *          `SecOCRxPduId`（`SecOC_RxIndication()` の `RxPduId` と同じ値）と
 *          みなして扱う（学習用簡略化）。
 *
 *          `overrideStatus=41`（VerifyStatus を Pass に強制）は、実仕様
 *          自身が既定 FALSE の `SecOCEnableForcedPassOverride`
 *          （[ECUC_SecOC_00051]）で無効化されている危険な機能（未検証データを
 *          無条件に信頼させる）であり、本プロジェクトはこのコンフィグ切替を
 *          持たないため、常に非対応として `E_NOT_OK` を返す（安全側の既定を
 *          採用）。`overrideStatus=0`（無期限 Fail 強制）/`1`（回数指定 Fail
 *          強制）/`2`（解除）のみサポートする。
 *
 * \param[in]  freshnessValueID            対象の SecOCRxPduId
 *                                         （上記の簡略化参照）。
 * \param[in]  overrideStatus              SECOC_OVERRIDE_FAIL_INDEFINITE(0) /
 *                                         SECOC_OVERRIDE_FAIL_COUNTED(1) /
 *                                         SECOC_OVERRIDE_CANCEL(2) のいずれか。
 *                                         それ以外（41 含む）は非対応。
 * \param[in]  numberOfMessagesToOverride  overrideStatus=1 のときのみ有効。
 *                                         強制 Fail を適用する残りメッセージ数。
 *
 * \retval  E_OK      正常に設定/解除した。
 * \retval  E_NOT_OK  未初期化、freshnessValueID に一致する RX PDU が無い、
 *                    または overrideStatus が非対応の値。
 *
 * \AUTOSARReq     {SWS_SecOC_00122}
 * \ServiceID      {0x0b}
 * \Reentrancy     {Non Reentrant for the same FreshnessValueID. Reentrant for
 *                  different FreshnessValueIDs.}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType SecOC_VerifyStatusOverride(uint16 freshnessValueID, uint8 overrideStatus,
                                          uint8 numberOfMessagesToOverride)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_VERIFY_STATUS_OVERRIDE, SECOC_E_UNINIT);
        return E_NOT_OK;
    }

    uint8 tableIndex = 0U;
    if (SecOC_FindRxPdu((PduIdType)freshnessValueID, &tableIndex) == NULL)
    {
        DET_LOGW(TAG, "VerifyStatusOverride W: no matching SecOC RX PDU for freshnessValueID=%u",
                 (unsigned)freshnessValueID);
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_VERIFY_STATUS_OVERRIDE, SECOC_E_INVALID_PDU_SDU_ID);
        return E_NOT_OK;
    }

    switch (overrideStatus)
    {
        case SECOC_OVERRIDE_FAIL_INDEFINITE:
            SecOC_OverrideStatus[tableIndex]    = SECOC_OVERRIDE_FAIL_INDEFINITE;
            SecOC_OverrideRemaining[tableIndex] = 0U;
            DET_LOGW(TAG, "iPdu=%u VerifyStatus forced to Fail (indefinite)", (unsigned)freshnessValueID);
            break;

        case SECOC_OVERRIDE_FAIL_COUNTED:
            SecOC_OverrideStatus[tableIndex]    = SECOC_OVERRIDE_FAIL_COUNTED;
            SecOC_OverrideRemaining[tableIndex] = numberOfMessagesToOverride;
            DET_LOGW(TAG, "iPdu=%u VerifyStatus forced to Fail for %u message(s)",
                     (unsigned)freshnessValueID, (unsigned)numberOfMessagesToOverride);
            break;

        case SECOC_OVERRIDE_CANCEL:
            SecOC_OverrideStatus[tableIndex]    = SECOC_OVERRIDE_NONE;
            SecOC_OverrideRemaining[tableIndex] = 0U;
            DET_LOGI(TAG, "iPdu=%u VerifyStatus override canceled", (unsigned)freshnessValueID);
            break;

        default:
            /* overrideStatus=41 (強制 Pass) は [ECUC_SecOC_00051]
             * SecOCEnableForcedPassOverride の既定値 FALSE に倣い、本プロジェクトは
             * 対応するコンフィグ切替を持たないため常に非対応として扱う（未検証
             * データを無条件に信頼させる危険な機能のため、安全側の既定を採用。
             * SecOC.h の Doxygen 参照）。それ以外の未知の値も含めここで拒否する。 */
            DET_LOGW(TAG, "VerifyStatusOverride W: iPdu=%u unsupported overrideStatus=%u",
                     (unsigned)freshnessValueID, (unsigned)overrideStatus);
            return E_NOT_OK;
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * SecOC_IfTransmit
 * ---------------------------------------------------------------------- */

Std_ReturnType SecOC_IfTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_IF_TRANSMIT, SECOC_E_UNINIT);
        return E_NOT_OK;
    }

    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL))
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_IF_TRANSMIT, SECOC_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    uint8 tableIndex = 0U;
    const SecOC_TxPduConfigType* cfg = SecOC_FindTxPdu(TxPduId, &tableIndex);
    if (cfg == NULL)
    {
        DET_LOGW(TAG, "IfTransmit W: no matching SecOC TX PDU for id=%u", (unsigned)TxPduId);
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_IF_TRANSMIT, SECOC_E_INVALID_PDU_SDU_ID);
        return E_NOT_OK;
    }

    if ((PduInfoPtr->SduLength != cfg->AuthenticPduLength)
        || (cfg->AuthenticPduLength > SECOC_TX_AUTH_BUF_MAX))
    {
        DET_LOGE(TAG, "IfTransmit E: iPdu=%u length %u != expected %u",
                 (unsigned)TxPduId, (unsigned)PduInfoPtr->SduLength, (unsigned)cfg->AuthenticPduLength);
        return E_NOT_OK;
    }

    /* [SWS_SecOC_00058]: Authentic I-PDU を内部バッファへコピーするだけに
     * 留め、Freshness/MAC の計算は次回 SecOC_MainFunctionTx() まで遅延する
     * （[SWS_SecOC_00060]〜[SWS_SecOC_00062]）。 */
    for (uint8 b = 0U; b < cfg->AuthenticPduLength; b++)
    {
        SecOC_TxAuthenticBuffer[tableIndex][b] = PduInfoPtr->SduDataPtr[b];
    }
    SecOC_TxPending[tableIndex] = 1U;
    SecOC_TxBuildAttempts[tableIndex] = 0U;   /* [SWS_SecOC_00226]: 新しい送信要求の最初の処理で、試行の回数を 0 にする */
    /* 新しい Authentic I-PDU を受け取ったので、保持している古い Secured I-PDU は、次の SecOC_MainFunctionTx() が
     * 組み立て直すまで、下位層へ渡さない（SecOC_TriggerTransmit() は E_NOT_OK。[SWS_SecOC_00150]）。 */
    SecOC_TxSecuredValid[tableIndex] = 0U;

    return E_OK;
}

/* ----------------------------------------------------------------------
 * SecOC_TxConfirmation
 * ---------------------------------------------------------------------- */

/**
 * \brief   下位層からの TX 完了通知エントリポイント。
 *
 * \AUTOSARReq     {SWS_SecOC_00126}
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
void SecOC_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_TX_CONFIRMATION, SECOC_E_UNINIT);
        return;
    }

    uint8 tableIndex = 0U;
    const SecOC_TxPduConfigType* cfg = SecOC_FindTxPdu(TxPduId, &tableIndex);
    if (cfg == NULL)
    {
        DET_LOGW(TAG, "TxConfirmation W: no matching SecOC TX PDU for id=%u", (unsigned)TxPduId);
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_TX_CONFIRMATION, SECOC_E_INVALID_PDU_SDU_ID);
        return;
    }

    DET_LOGI(TAG, "TxConfirmation: iPdu=%u result=%u", (unsigned)TxPduId, (unsigned)result);

    /* [SWS_SecOC_00063]: 結果を元の送信元（Com 等）へ中継する。
     * [SWS_SecOC_00064] のバッファ解放については SecOC.h の宣言側コメント
     * 参照（本実装は既に解放済み扱いのため追加処理不要）。 */
    PduR_SecOCTxConfirmation(cfg->PduRSrcPduId, result);

    /* [SWS_SecOC_00069]: トリガ送信で、Secured I-PDU を下位層へ渡し（SecOC_TriggerTransmit()）、その送信が確認された
     * ときは、保持している Authentic I-PDU を、上位層が新しく設定したものとして扱い、次の SecOC_MainFunctionTx() で
     * 再び認証する（新しい Freshness Value の Secured I-PDU を作る。7.4.2 節の「繰り返しの送信」）。
     * 直接の送信（SecOC_TriggerTransmit() を使わない）では、再び認証しない。 */
    if (SecOC_TxTriggered[tableIndex] != 0U)
    {
        SecOC_TxTriggered[tableIndex] = 0U;
        if (result == E_OK)
        {
            SecOC_TxPending[tableIndex] = 1U;
        }
    }
}

/* ----------------------------------------------------------------------
 * SecOC_MainFunctionTx
 * ---------------------------------------------------------------------- */

void SecOC_MainFunctionTx(void)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_MAIN_FUNCTION_TX, SECOC_E_UNINIT);
        return;
    }

    for (uint8 t = 0U; t < SecOC_ConfigPtr->TxPduCount; t++)
    {
        if (!SecOC_TxPending[t])
        {
            continue;
        }

        SecOC_TxPending[t] = 0U;

        const SecOC_TxPduConfigType* cfg = &SecOC_ConfigPtr->TxPdus[t];

        const uint16 authInputLen = (uint16)(2U + cfg->AuthenticPduLength + cfg->FreshnessLength);
        if (authInputLen > SECOC_AUTH_INPUT_MAX)
        {
            DET_LOGE(TAG, "MainFunction E: iPdu=%u authInputLen=%u exceeds buffer",
                     (unsigned)cfg->SecOCTxPduId, (unsigned)authInputLen);
            continue;
        }
        if (cfg->SecuredPduLength > SECOC_TX_AUTH_BUF_MAX)
        {
            DET_LOGE(TAG, "MainFunction E: iPdu=%u SecuredPduLength=%u exceeds buffer",
                     (unsigned)cfg->SecOCTxPduId, (unsigned)cfg->SecuredPduLength);
            continue;
        }

        const uint8 freshness = SecOC_TxFreshness[t];

        /* DataToAuthenticator = DataId(2byte, Big Endian) | Authentic Payload |
         * Complete Freshness Value（SecOC_RxIndication() と対称のロジック）。 */
        uint8 authInput[SECOC_AUTH_INPUT_MAX];
        authInput[0] = (uint8)(cfg->DataId >> 8);
        authInput[1] = (uint8)(cfg->DataId & 0xFFU);
        for (uint8 b = 0U; b < cfg->AuthenticPduLength; b++)
        {
            authInput[2U + b] = SecOC_TxAuthenticBuffer[t][b];
        }
        for (uint8 b = 0U; b < cfg->FreshnessLength; b++)
        {
            authInput[2U + cfg->AuthenticPduLength + b] = freshness;
        }

        /* Secured I-PDU = Authentic Payload | Freshness Value | 切り詰め MAC
         * （SecOC_RxPduConfigType の FreshnessOffset/MacOffset と同じレイアウト
         * 規約。Authentic のすぐ後ろに Freshness、その後ろに MAC が続く）。 */
        /* 組み立て中の Secured I-PDU（作業用）。MAC の計算が成功してから、保持するバッファへ移す
         * （保持するバッファには、MAC が付いた完全な Secured I-PDU だけを置く）。 */
        uint8 secured[SECOC_TX_AUTH_BUF_MAX];
        for (uint8 b = 0U; b < cfg->AuthenticPduLength; b++)
        {
            secured[b] = SecOC_TxAuthenticBuffer[t][b];
        }
        for (uint8 b = 0U; b < cfg->FreshnessLength; b++)
        {
            secured[cfg->FreshnessOffset + b] = freshness;
        }

        /* Csm_MacGenerate() が AES-128-CMAC を計算し、切り詰め済み MAC を
         * secured[MacOffset..] へ直接書き込む（Csm/CryIf/Crypto レイヤ経由。
         * SecOC は鍵にも AES/CMAC の実装にも一切アクセスしない）。 */
        uint32 macLen = (uint32)cfg->MacTxLength;
        const Std_ReturnType macRet = Csm_MacGenerate(cfg->CsmJobId, CRYPTO_OPERATIONMODE_SINGLECALL,
                                                       authInput, authInputLen,
                                                       &secured[cfg->MacOffset], &macLen);
        if (macRet != E_OK)
        {
            /* MAC を作れなかった。MAC が未設定の Secured I-PDU を、送らず、保持もしない
             * （下位層へ渡すと、受信側に不正なフレームとして棄却される）。Freshness Value も進めない。
             * [SWS_SecOC_00227]: 回復可能なエラー（CRYPTO_E_BUSY、CRYPTO_E_QUEUE_FULL）なら、試行の回数を
             * 増やす（E_NOT_OK は、回復可能なエラーではない）。 */
            const uint8 recoverable = ((macRet == CRYPTO_E_BUSY) || (macRet == CRYPTO_E_QUEUE_FULL)) ? 1U : 0U;
            if (recoverable != 0U)
            {
                SecOC_TxBuildAttempts[t]++;
            }

            /* SecOCAuthenticationBuildAttempts の値。0（省略）は 1 回だけ試す。 */
            const uint16 limit = (cfg->AuthenticationBuildAttempts != 0U) ? cfg->AuthenticationBuildAttempts : 1U;

            if ((recoverable != 0U) && (SecOC_TxBuildAttempts[t] < limit))
            {
                /* [SWS_SecOC_00228]: 試行の回数が上限に達していないので、次の SecOC_MainFunctionTx() で、
                 * Freshness Value と MAC の計算をやり直す。 */
                SecOC_TxPending[t] = 1U;
                DET_LOGW(TAG, "MainFunction W: iPdu=%u Csm_MacGenerate() recoverable error %u (attempt %u/%u), retry",
                         (unsigned)cfg->SecOCTxPduId, (unsigned)macRet,
                         (unsigned)SecOC_TxBuildAttempts[t], (unsigned)limit);
                continue;
            }

            /* [SWS_SecOC_00155]/[SWS_SecOC_00166]/[SWS_SecOC_00229]: 認証を作れないと確定した（E_NOT_OK、または、
             * 試行の回数が上限に達した）。Authentic I-PDU を、内部のバッファから捨てて、送信要求を取り消し、
             * SECOC_E_CRYPTO_FAILURE を報告する。上位層（Com 等）には、送信の失敗（E_NOT_OK）を、送信確認として
             * 通知する（SecOC_TxConfirmation() が失敗の結果を中継するのと同じ経路。通知しないと、上位層は、
             * 送信の確認を、タイムアウトまで待ち続ける）。 */
            DET_LOGE(TAG, "MainFunction E: iPdu=%u Csm_MacGenerate() failed (ret=%u), authentic I-PDU dropped",
                     (unsigned)cfg->SecOCTxPduId, (unsigned)macRet);
            for (uint8 b = 0U; b < SECOC_TX_AUTH_BUF_MAX; b++)
            {
                SecOC_TxAuthenticBuffer[t][b] = 0U;
            }
            SecOC_TxSecuredValid[t]  = 0U;
            SecOC_TxTriggered[t]     = 0U;
            PduR_SecOCTxConfirmation(cfg->PduRSrcPduId, E_NOT_OK);
            (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_MAIN_FUNCTION_TX, SECOC_E_CRYPTO_FAILURE);
            continue;
        }

        SecOC_TxBuildAttempts[t] = 0U;
        SecOC_TxFreshness[t] = (uint8)(freshness + 1U);
        for (uint8 b = 0U; b < cfg->SecuredPduLength; b++)
        {
            SecOC_TxSecuredBuffer[t][b] = secured[b];
        }
        SecOC_TxSecuredValid[t] = 1U;   /* 組み立て済みの Secured I-PDU を、保持する（[SWS_SecOC_00068]） */

        DET_LOGI(TAG, "MainFunction: iPdu=%u secured OK (freshness=%u)",
                 (unsigned)cfg->SecOCTxPduId, (unsigned)freshness);

        PduInfoType securedPduInfo = {
            .SduDataPtr = secured,
            .SduLength  = cfg->SecuredPduLength
        };
        (void)PduR_SecOCTransmit(cfg->PduRSrcPduId, &securedPduInfo);
    }
}

/* ----------------------------------------------------------------------
 * SecOC_MainFunctionRx
 * ---------------------------------------------------------------------- */

/**
 * \brief   周期実行関数。RX 方向の認証・検証処理を行う（[SWS_SecOC_00171]）。
 *
 * \AUTOSARReq     {SWS_SecOC_00171}
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void SecOC_MainFunctionRx(void)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_MAIN_FUNCTION_RX, SECOC_E_UNINIT);
        return;
    }

    /* SecOC_RxIndication() が受信の都度 Csm_MacVerify() を SINGLECALL で
     * 同期実行して検証を完結させる設計のため、本関数が処理すべき保留中の
     * RX ジョブは存在しない（SecOC.h の本関数 Doxygen コメント参照）。 */
}

/* ----------------------------------------------------------------------
 * SecOC_GetVersionInfo
 * ---------------------------------------------------------------------- */

void SecOC_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_GET_VERSION_INFO, SECOC_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = SECOC_VENDOR_ID;
    versioninfo->moduleID         = SECOC_MODULE_ID;
    versioninfo->sw_major_version = SECOC_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = SECOC_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = SECOC_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * SecOC_TpTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_CancelReceive
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_IfCancelTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_TpCancelTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_ChangeParameter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_AssociateKey
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_FreshnessValueRead
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_FreshnessValueWrite
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_TpRxIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_TpTxConfirmation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_TriggerTransmit
 * ---------------------------------------------------------------------- */

/**
 * \brief   組み立て済みの Secured I-PDU を、下位層（PduR 経由で CanIf など）へ複製する（トリガ送信。[SWS_SecOC_00068]）。
 *
 * \details 下位層が、送信の直前に、PduR_<Lo>TriggerTransmit を通して、SecOC へ Secured I-PDU を取りに来る
 *          （[7.4.2] 節 "Authentication during triggered transmission"）。SecOC は、上位層の役割を自分で持ち、
 *          SecOC_MainFunctionTx() が組み立てて保持している Secured I-PDU を、呼び出し元のバッファへ複製し、
 *          SduLength を Secured I-PDU の長さへ更新する。トリガ送信の呼び出しを、上位層へは転送しない。
 *
 *          複製した Secured I-PDU は、下位層へ渡した時点で解放する（同じ Freshness Value の Secured I-PDU を、
 *          2 回送ると、受信側がリプレイとして棄却するため）。その送信が確認された（SecOC_TxConfirmation() が
 *          E_OK で呼ばれた）ときに、保持している Authentic I-PDU を、新しく設定されたものとして扱い、次の
 *          SecOC_MainFunctionTx() で再び認証する（[SWS_SecOC_00069]。下位層が、上位層の更新を待たずに、
 *          再び要求しても、新しい Freshness Value の Secured I-PDU を渡せる）。
 *
 *          保持している Secured I-PDU が無い（まだ組み立てていない、複製済み、または、新しい Authentic I-PDU を
 *          受け取って、組み立て直し待ち）ときは、何も複製せず E_NOT_OK を返す（[SWS_SecOC_00150]）。
 *          バッファが Secured I-PDU の長さより小さいときも、何も複製せず、PduInfoPtr を変えずに E_NOT_OK を返す。
 *
 * \param[in]     TxPduId     データを要求された SecOC TX Secured I-PDU の ID（SecOC_TxPduConfigType.SecOCTxPduId）。
 * \param[in,out] PduInfoPtr  SduDataPtr=複製先のバッファ、SduLength=その大きさ。戻るとき、複製した長さ。
 *                            NULL 禁止。SduDataPtr も NULL 禁止。
 *
 * \retval  E_OK      Secured I-PDU を複製した。SduLength が複製した長さを示す。
 * \retval  E_NOT_OK  未初期化、NULL ポインタ、TxPduId が不正、保持している Secured I-PDU が無い、またはバッファが小さい。
 *                    何も複製していない。
 *
 * \pre        SecOC_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_SecOC_00127, SWS_SecOC_00068, SWS_SecOC_00069, SWS_SecOC_00150}
 * \ServiceID      {0x41}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType SecOC_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)
{
    if (SecOC_ConfigPtr == NULL)
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_TRIGGER_TRANSMIT, SECOC_E_UNINIT);
        return E_NOT_OK;
    }

    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL))
    {
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_TRIGGER_TRANSMIT, SECOC_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    uint8 tableIndex = 0U;
    const SecOC_TxPduConfigType* cfg = SecOC_FindTxPdu(TxPduId, &tableIndex);
    if (cfg == NULL)
    {
        DET_LOGW(TAG, "TriggerTransmit W: no matching SecOC TX PDU for id=%u", (unsigned)TxPduId);
        (void)Det_ReportError(SECOC_MODULE_ID, 0U, SECOC_API_ID_TRIGGER_TRANSMIT, SECOC_E_INVALID_PDU_SDU_ID);
        return E_NOT_OK;
    }

    /* [SWS_SecOC_00150]: 渡せる Secured I-PDU が無いときは、E_NOT_OK。 */
    if (SecOC_TxSecuredValid[tableIndex] == 0U)
    {
        DET_LOGD(TAG, "TriggerTransmit iPdu=%u: no secured I-PDU available", (unsigned)TxPduId);
        return E_NOT_OK;
    }

    /* 保持するバッファに収まらない長さの設定は、SecOC_MainFunctionTx() が拒否するので、ここへは来ない。念のため、
     * 複製のループが、バッファの外を読まないように、同じ上限で守る。 */
    if (cfg->SecuredPduLength > SECOC_TX_AUTH_BUF_MAX)
    {
        return E_NOT_OK;
    }

    /* 収まらないときは、何もコピーせず、PduInfoPtr も変えない。 */
    if (PduInfoPtr->SduLength < cfg->SecuredPduLength)
    {
        DET_LOGW(TAG, "TriggerTransmit W: iPdu=%u buffer too small (%u < %u)",
                 (unsigned)TxPduId, (unsigned)PduInfoPtr->SduLength, (unsigned)cfg->SecuredPduLength);
        return E_NOT_OK;
    }

    /* [SWS_SecOC_00068] */
    for (uint8 b = 0U; b < cfg->SecuredPduLength; b++)
    {
        PduInfoPtr->SduDataPtr[b] = SecOC_TxSecuredBuffer[tableIndex][b];
    }
    PduInfoPtr->SduLength = cfg->SecuredPduLength;

    /* 下位層へ渡したので、保持している Secured I-PDU を解放する。送信の確認で、再び認証する（[SWS_SecOC_00069]）。 */
    SecOC_TxSecuredValid[tableIndex] = 0U;
    SecOC_TxTriggered[tableIndex]    = 1U;

    return E_OK;
}

/* ----------------------------------------------------------------------
 * SecOC_CopyRxData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_CopyTxData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_StartOfReception
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_GetRxFreshness
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_GetRxFreshnessAuthData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_GetTxFreshness
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_GetTxFreshnessTruncData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_SPduTxConfirmation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * SecOC_VerificationStatusCallout
 * ---------------------------------------------------------------------- */

/* 未実装 */

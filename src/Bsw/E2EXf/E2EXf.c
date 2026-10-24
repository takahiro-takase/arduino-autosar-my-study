/**
 * \file    E2EXf.c
 * \brief   E2E Transformer 実装 (AUTOSAR SWS_E2ELibrary 12.4 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1/4.2.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
/* ======================================================================
 * Includes
 * ====================================================================== */

#include "E2EXf.h"
#include "E2EXf_PBCfg.h"
#include "E2E.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "E2EXf"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* E2EXf モジュール自身の初期化状態（SWS_E2EXf_00130 準拠）。
 * E2E_P05CheckStateType/E2E_P05ProtectStateType（下位の Profile 層）の
 * 初期化とは別に、Transformer 層自身が「E2EXf_Init() が呼ばれたか」を
 * 保持する必要がある（SWS_E2EXf_00133/00151）。EcuM_Init() の呼び出し
 * 順序が将来変わり、E2EXf_PBCfg_Init() より前にフレーム受信経路が
 * 有効になってしまった場合でも、初期化前の State（WaitForFirstData=0
 * の未初期化 BSS のまま）を使って誤判定することを防ぐ。
 * 本プロジェクトの他 BSW モジュール（Com_ConfigPtr 等）と同じ
 * 「未初期化アクセスを防ぐ」方針に合わせている。 */
static uint8 E2EXf_Initialized = 0U;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static Std_ReturnType E2EXf_ReportSMVerdict(Dem_EventIdType DemEventId, E2E_PCheckStatusType ProfileStatus,
                                             const E2E_SMConfigType* SMConfig, E2E_SMCheckStateType* SMState);
static uint8 E2EXf_PackReturn(E2E_SMStateType SMState, E2E_PCheckStatusType CheckStatus);
static uint8 E2EXf_InvP05(const E2EXf_RxConfigTypeP05* Cfg, const char* Name, const uint8* buffer, uint32* bufferLength,
                           uint32 inputBufferLength, E2E_P05StatusType* CheckStatus);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * E2EXf_<transformerId>
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * E2EXf_E2EHealthStatus
 * ---------------------------------------------------------------------- */

uint8 E2EXf_E2EHealthStatus(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer, uint32 inputBufferLength)
{
    (void)inputBuffer;   /* out-of-place 変換は使用しない（E2EXf.h の \note 参照） */

    if (bufferLength == NULL)
    {
        /* [SWS_E2EXf_00150] */
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (!E2EXf_Initialized)
    {
        /* [SWS_E2EXf_00151] */
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_UNINIT);
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    /* [SWS_E2EXf_00102]: buffer が NULL（かつ長さが 0 でも）送信するデータが無いので、変換できない（簡略化:
     * 仕様は「NULL かつ長さ 0」を「新しいデータが無い」有効な組み合わせとするが、送信側にはその使い道が無い）。 */
    if (buffer == NULL)
    {
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    /* [SWS_E2EXf_00111]/[SWS_E2EXf_00162 相当]: 長さが、E2E の保護する PDU 長（DataLength）と合うこと。
     * 本プロジェクトの in-place 変換は、E2E ヘッダ（Counter・CRC16）の領域を、PDU の中に最初から持つので、
     * inputBufferLength は PDU 全体の長さ（= DataLength）。E2E_P05Protect() が長さを検査して、足りなければ
     * E2E_E_INPUTERR_WRONG を返す。 */
    Std_ReturnType protectRet = E2E_E_INPUTERR_WRONG;   /* uint16 に収まらない長さは、切り詰めず、長さ異常のままにする */
    if (inputBufferLength <= 0xFFFFU)
    {
        protectRet = E2E_P05Protect(E2EXf_E2EHealthStatusTxCfgP05.E2EConfig, E2EXf_E2EHealthStatusTxCfgP05.ProtectState,
                                     buffer, (uint16)inputBufferLength);
    }
    if (protectRet != E2E_E_OK)
    {
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM);
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    *bufferLength = E2EXf_E2EHealthStatusTxCfgP05.E2EConfig->DataLength;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * E2EXf_Inv_<transformerId>
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * E2EXf_Inv_EngineInfo
 * ---------------------------------------------------------------------- */

/* cppcheck-suppress constParameterPointer */
uint8 E2EXf_Inv_EngineInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer, uint32 inputBufferLength,
                            E2E_P05StatusType* CheckStatus)
{
    (void)inputBuffer;        /* out-of-place 変換は使用しない（E2EXf.h 冒頭 \note 参照） */

    return E2EXf_InvP05(&E2EXf_EngineInfoRxCfg, "EngineInfo", buffer, bufferLength, inputBufferLength, CheckStatus);
}

/* ----------------------------------------------------------------------
 * E2EXf_Inv_AbsInfo
 * ---------------------------------------------------------------------- */

/* cppcheck-suppress constParameterPointer */
uint8 E2EXf_Inv_AbsInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer, uint32 inputBufferLength,
                         E2E_P05StatusType* CheckStatus)
{
    (void)inputBuffer;

    return E2EXf_InvP05(&E2EXf_AbsInfoRxCfg, "AbsInfo", buffer, bufferLength, inputBufferLength, CheckStatus);
}

/* ----------------------------------------------------------------------
 * E2EXf_Init
 * ---------------------------------------------------------------------- */

/**
 * \AUTOSARReq     {SWS_E2EXf_00035}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2EXf_Init(const E2EXf_ConfigType* ConfigPtr)
{
    (void)ConfigPtr;  /* 常に NULL（post-build 設定を持たないため。E2EXf.h 参照） */
    E2EXf_Initialized = 1U;
}

/* ----------------------------------------------------------------------
 * E2EXf_DeInit
 * ---------------------------------------------------------------------- */

void E2EXf_DeInit(void)
{
    if (!E2EXf_Initialized)
    {
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_DEINIT, E2EXF_E_UNINIT);
        return;
    }

    E2EXf_Initialized = 0U;

    DET_LOGI(TAG, "DeInit ok");
}


/* ----------------------------------------------------------------------
 * E2EXf_GetVersionInfo
 * ---------------------------------------------------------------------- */

void E2EXf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_GET_VERSION_INFO, E2EXF_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = E2EXF_VENDOR_ID;
    versioninfo->moduleID         = E2EXF_MODULE_ID;
    versioninfo->sw_major_version = E2EXF_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = E2EXF_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = E2EXF_SW_PATCH_VERSION;
}

/* ======================================================================
 * Call-back notifications
 * ====================================================================== */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * E2EXf_InvP05
 * ---------------------------------------------------------------------- */

/**
 * \brief   Profile05 の E2E Inverse Transform（`E2EXf_Inv_<transformerId>`）の共通処理。
 *
 * \details `E2EXf_Inv_EngineInfo()`/`E2EXf_Inv_AbsInfo()` は、使う設定（`E2EXf_RxConfigTypeP05`）だけが違い、
 *          処理は完全に同じなので、この関数へ委ねる（仕様も、変換関数が内部の共通関数を呼んでよいとする）。
 *
 *          入力の規則（[SWS_E2EXf_00105]/[SWS_E2EXf_00104]）:
 *            - buffer が NULL で、inputBufferLength が 0 以外 → E_SAFETY_HARD_RUNTIMEERROR（ポインタ異常）。
 *            - buffer が NULL で、inputBufferLength が 0 → 「新しいデータが無い」を表す有効な組み合わせ。
 *              E2E_P05Check() へ NULL・0 を渡して、E2E_P05STATUS_NONEWDATA を得る（初回受信の特別扱い
 *              （WaitForFirstData）は消費しない）。ステートマシンへは E2E_P_NONEWDATA として渡り、
 *              *bufferLength は 0（[SWS_E2EXf_00114]）。
 *            - それ以外は、inputBufferLength を E2E_P05Check() へ渡す（[SWS_E2EXf_00104]）。長さが設定の DataLength と
 *              合わなければ、E2E_P05Check() が E2E_E_INPUTERR_WRONG を返し、E_SAFETY_HARD_RUNTIMEERROR とする。
 *
 *          処理の続き（WaitForFirstData、E2E_SMCheck()、Dem への報告、戻り値のニブルパック）は、
 *          `E2EXf_Inv_EngineInfo()` の説明（E2EXf.h）のとおり。
 *
 * \param[in]  Cfg               使う受信側の設定（インスタンスごと）。NULL 禁止。
 * \param[in]  Name              ログに出すインスタンス名。NULL 禁止。
 * \param[in]  buffer            検証する I-PDU のバイト列（in-place。この関数は書き換えない）。NULL は「新しいデータが無い」（長さ 0 のとき）。
 * \param[out] bufferLength      実使用長を受け取る。NULL 禁止。
 * \param[in]  inputBufferLength buffer の長さ [byte]。E2E ヘッダ込みの PDU 全体の長さ（= DataLength）。
 * \param[out] CheckStatus       E2E_P05Check() の生の 6 状態を受け取る。NULL 禁止。
 *
 * \retval  0x00 (E_OK)                 SMState=VALID かつ今回のフレームも合格。
 * \retval  0x02/0x03/0x05/0x20〜0x45   [SWS_E2EXf_00027] のニブルパック値。
 * \retval  E_SAFETY_HARD_RUNTIMEERROR  引数の異常、または E2EXf_Init() 未呼び出し。
 * \retval  E_SAFETY_SOFT_RUNTIMEERROR  E2E_SMCheck() が失敗した（到達しないはずの防御的な経路）。
 */
static uint8 E2EXf_InvP05(const E2EXf_RxConfigTypeP05* Cfg, const char* Name, const uint8* buffer, uint32* bufferLength,
                           uint32 inputBufferLength, E2E_P05StatusType* CheckStatus)
{
    if ((CheckStatus == NULL) || (bufferLength == NULL))
    {
        /* [SWS_E2EXf_00152] */
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (!E2EXf_Initialized)
    {
        /* [SWS_E2EXf_00153] */
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_UNINIT);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if ((buffer == NULL) && (inputBufferLength != 0U))
    {
        /* [SWS_E2EXf_00105] */
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    const uint8 noNewData = (buffer == NULL) ? 1U : 0U;   /* ここまでで、buffer が NULL なら inputBufferLength は 0 */

    if (inputBufferLength > 0xFFFFU)
    {
        /* E2E_P05Check() の長さ引数は uint16。範囲外は、切り詰めて別の長さに見えないよう、先に弾く */
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    /* [SWS_E2EXf_00104]: 設定の DataLength ではなく、受け取った長さ inputBufferLength を渡す */
    const Std_ReturnType checkRet = E2E_P05Check(Cfg->E2EConfig, Cfg->CheckState, buffer, (uint16)inputBufferLength);
    if (checkRet != E2E_E_OK)
    {
        /* E2E_E_INPUTERR_NULL（設定・状態が NULL、通常は到達しない）か E2E_E_INPUTERR_WRONG（長さが DataLength と違う）。
         * [SWS_E2EXf_00152] */
        (void)Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM,
                              (checkRet == E2E_E_INPUTERR_WRONG) ? E2EXF_E_PARAM : E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }
    E2E_P05StatusType status = Cfg->CheckState->Status;

    /* Profile05にはProfile01のWaitForFirstData/INITIAL相当の初回受信の特別扱いが
     * 無い(E2E_P05.c は仕様に忠実な実装として意図的にこれを持たない)。しかし
     * 実運用では起動直後、送信元ECUが既に稼働中でCounterが0以外から始まっている
     * ことが十分あり得るため、そのまま繋ぐと最初のフレーム(CRCは正しい)が
     * REPEATED/WRONGSEQUENCEと誤判定され、DEM_DEBOUNCE_LIMIT=1の設定と相まって
     * 即座に誤ったDTCが確定してしまう。CRCさえ正しければ「通信路そのものは
     * 正常」と判断し、最初の1回に限りOKへ格上げする(E2E_P05Check()側は既に
     * 内部でCounterを受信値へ同期済みのため、2回目以降は通常のdelta判定に
     * 自然に戻る)。「新しいデータが無い」呼び出しは、実際にフレームを受け取った
     * わけではないので、この特別扱いを消費しない。 */
    if ((noNewData == 0U) && (Cfg->WaitForFirstData != NULL) && (*Cfg->WaitForFirstData != 0U)
        && (status != E2E_P05STATUS_ERROR))
    {
        status = E2E_P05STATUS_OK;
        *Cfg->WaitForFirstData = 0U;
    }

    *CheckStatus  = status;
    *bufferLength = (noNewData != 0U) ? 0U : Cfg->E2EConfig->DataLength;   /* [SWS_E2EXf_00114]（簡略化は E2EXf.h の \note 参照） */

    /* 「今回のフレームが使えるか」の判定（acceptable、戻り値下位ニブル）は
     * Dem への報告方針とは別物（下記参照）。 */
    const E2E_PCheckStatusType profileStatus = E2E_P05MapStatusToSM(E2E_E_OK, status);
    const uint8 acceptable = (profileStatus == E2E_P_OK) ? 1U : 0U;

    if (!acceptable)
    {
        DET_LOGW(TAG, "Inv_%s NG DemEvent=%u st=%u", Name, (unsigned)Cfg->DemEventId, (unsigned)status);
    }

    /* [SWS_E2EXf_00028]/[00029]: 通信路全体の直近 WindowSize 回分の健全性を
     * E2E_SMCheck() のステートマシンで判定し、その結果が VALID/INVALID に
     * 確定したときのみ Dem へ報告する（共通処理は E2EXf_ReportSMVerdict()
     * 参照）。 */
    const Std_ReturnType smVerdict = E2EXf_ReportSMVerdict(Cfg->DemEventId, profileStatus, Cfg->SMConfig, Cfg->SMState);
    if (smVerdict != E2E_E_OK)
    {
        /* [SWS_E2EXf_00027]: E2E_SMCheck() 自体が失敗した場合はニブルパック
         * せずそのまま返す（E2EXf.h の関数コメント参照）。CheckStatus は
         * 既に上で確定済み（生データの合否判定自体は行えている）ため
         * 書き換えない。 */
        return smVerdict;
    }

    /* [SWS_E2EXf_00027] 準拠のニブルパック（上位=SMState、下位=profileStatus）。
     * ret==0x00 は「SMState=VALID かつ今回のフレームも合格」を意味し、
     * [SWS_E2E_00345] の "do NOT use data" 規定（NODATA/INIT/INVALID中は
     * データを使わない）を、呼び出し元が単一の等値比較だけで満たせる。 */
    return E2EXf_PackReturn(Cfg->SMState->SMState, profileStatus);
}

/* ----------------------------------------------------------------------
 * E2EXf_ReportSMVerdict
 * ---------------------------------------------------------------------- */

/**
 * \brief   [SWS_E2EXf_00028]/[00029] の共通部分（P01/P05 いずれからも呼ばれる、
 *          プロファイル非依存の後処理）: `E2E_SMCheck()` を呼び、結果が
 *          VALID/INVALID に確定したときのみ Dem へ PASSED/FAILED を報告する。
 *
 * \details E2EXf_Inv_EngineInfo()/E2EXf_Inv_AbsInfo() の同名コメント参照。
 *          各インスタンス専用関数固有の処理（E2E_P05Check()・acceptable
 *          判定・WaitForFirstData 等）は各関数に残し、この後処理部分だけを
 *          共通化する（全インスタンスで完全に同一のロジックのため）。
 *
 * \param[in]     DemEventId    報告先の Dem イベント ID。
 * \param[in]     ProfileStatus `E2E_PxxMapStatusToSM()` が返したプロファイル
 *                              非依存の判定結果。
 * \param[in]     SMConfig      ステートマシン設定。NULL 禁止（呼び出し元で
 *                              確認済みであること）。
 * \param[in,out] SMState       ステートマシン状態。NULL 禁止（同上）。
 *
 * \retval  E2E_E_OK                     E2E_SMCheck() が成功した（Dem 報告は
 *                                      SMState->SMState に応じて実施・保留の
 *                                      いずれか）。
 * \retval  E_SAFETY_SOFT_RUNTIMEERROR  E2E_SMCheck() が失敗した（[SWS_E2EXf_00027]、
 *                                      呼び出し元はこの値をそのまま自身の
 *                                      戻り値として使ってよい。2026-09 追加、
 *                                      E2EXf.h の同名 `\note` 参照。自己
 *                                      /simplify 指摘: 呼び出し元は生の
 *                                      E2E_SMCheck() 戻り値を使わず常に
 *                                      E_SAFETY_SOFT_RUNTIMEERROR へ差し替える
 *                                      だけなので、ここで直接返す方が単純）。
 */
static Std_ReturnType E2EXf_ReportSMVerdict(Dem_EventIdType DemEventId, E2E_PCheckStatusType ProfileStatus,
                                             const E2E_SMConfigType* SMConfig, E2E_SMCheckStateType* SMState)
{
    const Std_ReturnType smRet = E2E_SMCheck(ProfileStatus, SMConfig, SMState);
    if (smRet != E2E_E_OK)
    {
        /* 到達しないはずの経路（E2EXf_PBCfg_Init() が全インスタンスに対し
         * E2E_SMCheckInit() を呼んでから使うため、E2E_E_WRONGSTATE
         * （E2E_SMCheckInit() 未実施）は起きないはず）。E2E_SMCheck() は
         * [SWS_E2E_00216] により DET/DEM を呼べないため、ここで代わりに
         * 記録する。SMState は VALID にも INVALID にもならないため、以降
         * Dem 報告は保留され続ける（フェイルセーフ側に倒れる）。 */
        DET_LOGE(TAG, "ReportSMVerdict E: E2E_SMCheck failed ret=%u DemEvent=%u",
                 (unsigned)smRet, (unsigned)DemEventId);
        return E_SAFETY_SOFT_RUNTIMEERROR;
    }

    switch (SMState->SMState)
    {
        case E2E_SM_VALID:
            (void)Dem_SetEventStatus(DemEventId, DEM_EVENT_STATUS_PASSED);
            break;
        case E2E_SM_INVALID:
            (void)Dem_SetEventStatus(DemEventId, DEM_EVENT_STATUS_FAILED);
            break;
        default:
            /* NODATA/INIT: 判定材料が揃うまでの起動直後、Dem 報告を保留する。 */
            break;
    }

    return E2E_E_OK;
}

/* ----------------------------------------------------------------------
 * E2EXf_PackReturn
 * ---------------------------------------------------------------------- */

/**
 * \brief   [SWS_E2EXf_00027]/8.3.2節の戻り値パック規則
 *          （上位ニブル=E2E_SMStateType、下位ニブル=E2E_PCheckStatusType）
 *          に従い、`E2EXf_ReportSMVerdict()` が正常に終えた（=E2E_SMCheck()
 *          自体は成功した）後の SMState/CheckStatus を1バイトへ合成する。
 *
 * \details `E2E_SMStateType`（VALID=0/DEINIT=1/NODATA=2/INIT=3/INVALID=4）と
 *          `E2E_PCheckStatusType`（OK=0/REPEATED=1/WRONGSEQUENCE=2/ERROR=3/
 *          NOTAVAILABLE=4/NONEWDATA=5）は、いずれも仕様の戻り値表
 *          （0x00/0x20/0x30/0x40 系の上位ニブル、0/1/2/3/5 の下位ニブル）と
 *          数値がそのまま一致するため、単純なビットシフト+OR で仕様準拠の
 *          パック値になる。`E2E_SMCheck()` 自体が失敗した場合（呼び出し元が
 *          `E_SAFETY_SOFT_RUNTIMEERROR` を直接返す経路）は本関数を通らない。
 *
 * \param[in]  SMState      E2E_SMCheck() 確定後のステートマシン状態。
 * \param[in]  CheckStatus  E2E_PxxMapStatusToSM() が返したプロファイル
 *                          非依存のチェック結果。
 *
 * \return  [SWS_E2EXf_00027] 準拠のパック済み戻り値。
 */
static uint8 E2EXf_PackReturn(E2E_SMStateType SMState, E2E_PCheckStatusType CheckStatus)
{
    return (uint8)(((uint8)SMState << 4) | (uint8)CheckStatus);
}

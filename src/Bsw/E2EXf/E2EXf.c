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
#include "E2EXf.h"
#include "E2E.h"
#include "Det.h"

#define TAG "E2EXf"

/* E2EXf モジュール自身の初期化状態（SWS_E2EXf_00130 準拠）。
 * E2E_P01CheckStateType/E2E_P01ProtectStateType（下位の Profile 層）の
 * 初期化とは別に、Transformer 層自身が「E2EXf_Init() が呼ばれたか」を
 * 保持する必要がある（SWS_E2EXf_00133/00151）。EcuM_Init() の呼び出し
 * 順序が将来変わり、E2EXf_PBCfg_Init() より前にフレーム受信経路が
 * 有効になってしまった場合でも、初期化前の State（WaitForFirstData=0
 * の未初期化 BSS のまま）を使って誤判定することを防ぐ。
 * 本プロジェクトの他 BSW モジュール（Com_ConfigPtr 等）と同じ
 * 「未初期化アクセスを防ぐ」方針に合わせている。 */
static uint8 E2EXf_Initialized = 0U;

/**
 * \brief   [SWS_E2EXf_00028]/[00029] の共通部分（P01/P05 いずれからも呼ばれる、
 *          プロファイル非依存の後処理）: `E2E_SMCheck()` を呼び、結果が
 *          VALID/INVALID に確定したときのみ Dem へ PASSED/FAILED を報告する。
 *
 * \details E2EXf_InverseTransform()/E2EXf_InverseTransformP05() の同名コメント
 *          参照。P01/P05 のプロファイル固有処理（E2E_PxxCheck()・acceptable
 *          判定・WaitForFirstData 等）は各関数に残し、この後処理部分だけを
 *          共通化する（両プロファイルで完全に同一のロジックのため）。
 *
 * \param[in]     DemEventId    報告先の Dem イベント ID。
 * \param[in]     ProfileStatus `E2E_PxxMapStatusToSM()` が返したプロファイル
 *                              非依存の判定結果。
 * \param[in]     SMConfig      ステートマシン設定。NULL 禁止（呼び出し元で
 *                              確認済みであること）。
 * \param[in,out] SMState       ステートマシン状態。NULL 禁止（同上）。
 */
static void E2EXf_ReportSMVerdict(Dem_EventIdType DemEventId, E2E_PCheckStatusType ProfileStatus,
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
        return;
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
}

/**
 * \AUTOSARReq     {SWS_E2EXf_00035}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void E2EXf_Init(const E2EXf_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    (void)ConfigPtr;  /* 常に NULL（post-build 設定を持たないため。E2EXf.h 参照） */
    E2EXf_Initialized = 1U;
}

void E2EXf_DeInit(void)
{
    DET_LOGT(TAG, "called");
    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_DEINIT, E2EXF_E_UNINIT);
        return;
    }

    E2EXf_Initialized = 0U;

    DET_LOGI(TAG, "DeInit ok");
}

Std_ReturnType E2EXf_InverseTransform(const E2EXf_RxConfigType* Config, const uint8* Buffer, uint8 Length,
                                      E2E_P01StatusType* CheckStatus)
{
    DET_LOGT(TAG, "called");
    if (CheckStatus == NULL)
    {
        /* [SWS_E2EXf_00152]（本関数は E2EXf_Inv_<transformerId> 相当のため
         * 00150 ではなく 00152 が対応する規定。00150/00151 は forward 関数
         * E2EXf_<transformerId>（本プロジェクトの E2EXf_Transform()、void
         * のため対象外）向け）。パラメータ異常検出時は E_NOT_OK ではなく
         * E_SAFETY_HARD_RUNTIMEERROR を返すべき（2026-09 是正）。 */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (!E2EXf_Initialized)
    {
        /* [SWS_E2EXf_00153] */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_UNINIT);
        *CheckStatus = E2E_P01STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (Config == NULL || Config->E2EConfig == NULL || Config->CheckState == NULL || Buffer == NULL
        || Config->SMConfig == NULL || Config->SMState == NULL)
    {
        /* [SWS_E2EXf_00152] */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P01STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    /* E2E_P01Check() は SWS_E2E_00047 準拠で Length 引数を持たないため
     * （Config->DataLength のみが唯一の長さ情報）、呼び出し元がここで
     * バッファ長を検証する。 */
    if (Length < Config->E2EConfig->DataLength)
    {
        /* [SWS_E2EXf_00152] */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM);
        *CheckStatus = E2E_P01STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (E2E_P01Check(Config->E2EConfig, Config->CheckState, Buffer) != E2E_E_OK)
    {
        /* Config->E2EConfig/CheckState/Buffer はここまでで NULL でないことを
         * 確認済みのため、通常は到達しない（E2E_E_INPUTERR_NULL の防御）。
         * [SWS_E2EXf_00152] */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P01STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }
    const E2E_P01StatusType status = Config->CheckState->Status;
    *CheckStatus = status;

    /* [SWS_E2E_00476] (profileBehavior=FALSE、R4.2より前の挙動) に基づき
     * 汎用ステータスへ変換して「今回のフレームが使えるか」を判定する。
     * CheckReturn はここまでで E2E_E_OK であることを確認済み。FALSE 表では
     * INITIAL→OK（初回受信は正常な起動シーケンスであり故障ではない）、
     * SYNC→WRONGSEQUENCE（WRONGSEQUENCE 検知後の再ロック中はまだ回復未確定
     * として不合格のまま扱う。個々のフレームの CRC・カウンタ自体は正常範囲内
     * だが、SyncCounterInit 回分の連続正常受信が完了するまでは「復旧候補」に
     * すぎず、再ロック機構の本来の目的（回復確認まで安易に正常扱いしない）
     * と整合させるため、以前は SYNC も合格扱いにしていたのを変更した）。
     * この acceptable は「今回の Buffer を呼び出し元が使ってよいか」だけを
     * 表し、Dem への PASSED/FAILED 報告方針とは別物（下記参照）。 */
    const E2E_PCheckStatusType profileStatus = E2E_P01MapStatusToSM(E2E_E_OK, status, 0U);
    const uint8 acceptable = (profileStatus == E2E_P_OK);

    if (!acceptable)
        DET_LOGW(TAG, "InverseTransform NG DemEvent=%u st=%u", (unsigned)Config->DemEventId, (unsigned)status);

    /* [SWS_E2EXf_00028]/[00029]: 通信路全体の直近 WindowSize 回分の健全性を
     * E2E_SMCheck() のステートマシンで判定し、その結果が VALID/INVALID に
     * 確定したときのみ Dem へ報告する（E2EXf.h の関数コメント参照。共通処理は
     * E2EXf_ReportSMVerdict() 参照）。 */
    E2EXf_ReportSMVerdict(Config->DemEventId, profileStatus, Config->SMConfig, Config->SMState);

    return acceptable ? E_OK : E_NOT_OK;
}

Std_ReturnType E2EXf_InverseTransformP05(const E2EXf_RxConfigTypeP05* Config, const uint8* Buffer, uint8 Length,
                                          E2E_P05StatusType* CheckStatus)
{
    DET_LOGT(TAG, "called");
    if (CheckStatus == NULL)
    {
        /* [SWS_E2EXf_00152]: パラメータ異常検出時は E_NOT_OK ではなく
         * E_SAFETY_HARD_RUNTIMEERROR を返すべき（2026-09 是正）。 */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (!E2EXf_Initialized)
    {
        /* [SWS_E2EXf_00153] */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_UNINIT);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (Config == NULL || Config->E2EConfig == NULL || Config->CheckState == NULL || Buffer == NULL
        || Config->SMConfig == NULL || Config->SMState == NULL)
    {
        /* [SWS_E2EXf_00152] */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }

    if (E2E_P05Check(Config->E2EConfig, Config->CheckState, Buffer, Length) != E2E_E_OK)
    {
        /* Config->E2EConfig/CheckState/Buffer はここまでで NULL でないことを
         * 確認済みのため E2E_E_INPUTERR_NULL は到達しない。2026-09-06 是正の
         * E2E_E_INPUTERR_WRONG（Length が Config->E2EConfig->DataLength と
         * 不一致）は理論上ここに落ちるが、本プロジェクトの呼び出し元
         * （Rte.c、EngineInfo/AbsInfo）は固定長でしか呼ばないため現状は
         * 到達しない。到達した場合 Dem_SetEventStatus() を呼ばずに return
         * する（上の E2EXf_InverseTransform()（Profile01 版）の Length
         * チェック分岐（81-86 行目）も同様に Dem_SetEventStatus() を呼ばずに
         * return しており、両プロファイルで挙動は対称）。[SWS_E2EXf_00152] */
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_SAFETY_HARD_RUNTIMEERROR;
    }
    E2E_P05StatusType status = Config->CheckState->Status;

    /* Profile05にはProfile01のWaitForFirstData/INITIAL相当の初回受信の特別扱いが
     * 無い(E2E_P05.c は仕様に忠実な実装として意図的にこれを持たない)。しかし
     * 実運用では起動直後、送信元ECUが既に稼働中でCounterが0以外から始まっている
     * ことが十分あり得るため、そのまま繋ぐと最初のフレーム(CRCは正しい)が
     * REPEATED/WRONGSEQUENCEと誤判定され、DEM_DEBOUNCE_LIMIT=1の設定と相まって
     * 即座に誤ったDTCが確定してしまう。CRCさえ正しければ「通信路そのものは
     * 正常」と判断し、最初の1回に限りOKへ格上げする(E2E_P05Check()側は既に
     * 内部でCounterを受信値へ同期済みのため、2回目以降は通常のdelta判定に
     * 自然に戻る)。EngineHealthStatus 用など WaitForFirstData が NULL の
     * インスタンスにはこの特別扱いを適用しない。 */
    if ((Config->WaitForFirstData != NULL) && (*Config->WaitForFirstData != 0U) && (status != E2E_P05STATUS_ERROR))
    {
        status = E2E_P05STATUS_OK;
        *Config->WaitForFirstData = 0U;
    }

    *CheckStatus = status;

    /* 「今回のフレームが使えるか」の判定（acceptable）は Dem への報告方針とは
     * 別物（E2EXf_InverseTransform() の同名コメント参照）。 */
    const E2E_PCheckStatusType profileStatus = E2E_P05MapStatusToSM(E2E_E_OK, status);
    const uint8 acceptable = (profileStatus == E2E_P_OK);

    if (!acceptable)
        DET_LOGW(TAG, "InverseTransformP05 NG DemEvent=%u st=%u", (unsigned)Config->DemEventId, (unsigned)status);

    /* [SWS_E2EXf_00028]/[00029]（E2EXf_InverseTransform() の同名コメント参照。
     * 共通処理は E2EXf_ReportSMVerdict() 参照）。 */
    E2EXf_ReportSMVerdict(Config->DemEventId, profileStatus, Config->SMConfig, Config->SMState);

    return acceptable ? E_OK : E_NOT_OK;
}

void E2EXf_Transform(const E2EXf_TxConfigType* Config, uint8* Buffer, uint8 Length)
{
    DET_LOGT(TAG, "called");
    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_UNINIT);
        return;
    }

    if (Config == NULL || Config->E2EConfig == NULL || Config->ProtectState == NULL || Buffer == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return;
    }

    /* E2E_P01Protect() は SWS_E2E_00047 準拠で Length 引数を持たないため
     * （Config->DataLength のみが唯一の長さ情報）、呼び出し元がここで
     * バッファ長を検証する（不足時は何もしない、旧実装と同じ挙動）。 */
    if (Length < Config->E2EConfig->DataLength)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM);
        return;
    }

    (void)E2E_P01Protect(Config->E2EConfig, Config->ProtectState, Buffer);
}

void E2EXf_TransformP05(const E2EXf_TxConfigTypeP05* Config, uint8* Buffer, uint8 Length)
{
    DET_LOGT(TAG, "called");
    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_UNINIT);
        return;
    }

    if (Config == NULL || Config->E2EConfig == NULL || Config->ProtectState == NULL || Buffer == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return;
    }

    (void)E2E_P05Protect(Config->E2EConfig, Config->ProtectState, Buffer, Length);
}

void E2EXf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_GET_VERSION_INFO, E2EXF_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = E2EXF_VENDOR_ID;
    versioninfo->moduleID         = E2EXF_MODULE_ID;
    versioninfo->sw_major_version = E2EXF_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = E2EXF_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = E2EXF_SW_PATCH_VERSION;
}

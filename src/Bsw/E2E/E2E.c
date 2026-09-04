/**
 * \file    E2E.c
 * \brief   E2E ライブラリ共通実装 (AUTOSAR SWS_E2ELibrary 準拠)
 * \details E2E.h 冒頭のコメント参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "E2E.h"
#include "Det.h"

#define TAG "E2E"

void E2E_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    DET_LOGT(TAG, "called");
    if (VersionInfo == NULL)
    {
        /* [SWS_E2E_00216]: ライブラリは DET/DEM を呼んではならないため、
         * Det_ReportError() は呼ばずサイレントに戻る（E2E.h 参照）。 */
        return;
    }

    VersionInfo->vendorID         = E2E_VENDOR_ID;
    VersionInfo->moduleID         = E2E_MODULE_ID;
    VersionInfo->sw_major_version = E2E_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = E2E_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = E2E_SW_PATCH_VERSION;
}

/**
 * \brief   `E2E_SMCheck()` 内部ステップ（[SWS_E2E_00466]）。ProfileStatus を
 *          循環バッファへ記録し、OkCount/ErrorCount を再集計する。
 *
 * \details 仕様書自身が「これは論理ステップであり、別関数として実装する
 *          必要はない」と明記するが、`E2E_SMCheck()` の4状態すべてで同一の
 *          処理が必要なため、共通化のために static ヘルパーとして実装する。
 *
 * \param[in]     ProfileStatus  今回のサイクルの判定結果。
 * \param[in]     ConfigPtr      呼び出し元が NULL でないことを確認済み。
 * \param[in,out] StatePtr       呼び出し元が NULL でないことを確認済み。
 */
static void E2E_SMAddStatus(E2E_PCheckStatusType ProfileStatus, const E2E_SMConfigType* ConfigPtr,
                             E2E_SMCheckStateType* StatePtr)
{
    StatePtr->ProfileStatusWindow[StatePtr->WindowTopIndex] = (uint8)ProfileStatus;

    uint8 okCount    = 0U;
    uint8 errorCount = 0U;
    for (uint8 i = 0U; i < ConfigPtr->WindowSize; i++)
    {
        if (StatePtr->ProfileStatusWindow[i] == (uint8)E2E_P_OK)
            okCount++;
        else if (StatePtr->ProfileStatusWindow[i] == (uint8)E2E_P_ERROR)
            errorCount++;
    }
    StatePtr->OkCount    = okCount;
    StatePtr->ErrorCount = errorCount;

    if (StatePtr->WindowTopIndex == (uint8)(ConfigPtr->WindowSize - 1U))
        StatePtr->WindowTopIndex = 0U;
    else
        StatePtr->WindowTopIndex++;
}

/**
 * \brief   E2E ステートマシンを初期化する（[SWS_E2E_00353]）。
 *
 * \details `StatePtr->ProfileStatusWindow` は呼び出し元が事前に
 *          `ConfigPtr->WindowSize` バイト分の配列を割り当て、ポインタを
 *          設定しておくこと（本関数はポインタ自体は変更しない）。
 *          [SWS_E2E_00370]: `StatePtr`/`ConfigPtr` が NULL の場合は何もせず
 *          `E2E_E_INPUTERR_NULL` を返す。それ以外の場合、
 *          `ProfileStatusWindow[]` を全て `E2E_P_NOTAVAILABLE` で初期化し、
 *          `WindowTopIndex`/`OkCount`/`ErrorCount` を 0、`SMState` を
 *          `E2E_SM_NODATA` に設定する。
 *
 * \param[out]  StatePtr   初期化するステートマシン状態。NULL 禁止。
 * \param[in]   ConfigPtr  ステートマシン設定。NULL 禁止。
 *
 * \retval  E2E_E_OK             正常完了。
 * \retval  E2E_E_INPUTERR_NULL  StatePtr または ConfigPtr が NULL。
 *
 * \note    [SWS_E2E_00216] によりライブラリは DET/DEM を一切呼んではならない
 *          ため、NULL の場合も Det_ReportError() は呼ばない。
 *
 * \AUTOSARReq     {SWS_E2E_00353, SWS_E2E_00370, SWS_E2E_00467}
 * \ServiceID      {0x31}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType E2E_SMCheckInit(E2E_SMCheckStateType* StatePtr, const E2E_SMConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    if (StatePtr == NULL || ConfigPtr == NULL)
        return E2E_E_INPUTERR_NULL;

    for (uint8 i = 0U; i < ConfigPtr->WindowSize; i++)
        StatePtr->ProfileStatusWindow[i] = (uint8)E2E_P_NOTAVAILABLE;

    StatePtr->WindowTopIndex = 0U;
    StatePtr->OkCount        = 0U;
    StatePtr->ErrorCount     = 0U;
    StatePtr->SMState        = E2E_SM_NODATA;

    return E2E_E_OK;
}

/**
 * \brief   E2E ステートマシンを1サイクル分進める（[SWS_E2E_00340]）。
 *
 * \details `E2E_PxxMapStatusToSM()`（プロファイル別、[SWS_E2E_00382]/
 *          [SWS_E2E_00452]）が変換したプロファイル非依存の判定結果
 *          `ProfileStatus` を受け取り、直近 `ConfigPtr->WindowSize` 回分の
 *          判定結果に基づいて通信路全体の健全性
 *          （`E2E_SM_NODATA`/`INIT`/`VALID`/`INVALID`）を判定する
 *          （[SWS_E2E_00345]/[SWS_E2E_00466]）。
 *
 *          状態遷移（仕様書 7.11.2 の状態図を PDF のベクタ座標解析で確定。
 *          pdftotext の単純抽出では複数状態の遷移ラベルが入り乱れて
 *          一意に読み取れなかったため）:
 *            - E2E_SM_DEINIT:  常に E2E_E_WRONGSTATE を返す（状態遷移なし）。
 *            - E2E_SM_NODATA:  ProfileStatus が ERROR でも NONEWDATA でも
 *              なければ E2E_SM_INIT へ（この遷移では AddStatus を呼ばない、
 *              仕様図に AddStatus ラベルが無いことを確認済み）。それ以外は
 *              NODATA に留まる。
 *            - E2E_SM_INIT:    AddStatus 後、
 *              (ErrorCount<=MaxErrorStateInit && OkCount>=MinOkStateInit)
 *              なら VALID へ、ErrorCount>MaxErrorStateInit なら INVALID へ、
 *              それ以外は INIT に留まる。
 *            - E2E_SM_VALID:   AddStatus 後、
 *              (ErrorCount<=MaxErrorStateValid && OkCount>=MinOkStateValid)
 *              なら VALID を維持、それ以外は INVALID へ。
 *            - E2E_SM_INVALID: AddStatus 後、
 *              (ErrorCount<=MaxErrorStateInvalid && OkCount>=MinOkStateInvalid)
 *              なら VALID へ、それ以外は INVALID に留まる。
 *
 * \param[in]     ProfileStatus  1サイクル分のプロファイル非依存判定結果。
 * \param[in]     ConfigPtr      ステートマシン設定。NULL 禁止。
 * \param[in,out] StatePtr       ステートマシン状態。NULL 禁止。
 *                               `E2E_SMCheckInit()` 済みであること。
 *
 * \retval  E2E_E_OK             正常完了（判定結果は `StatePtr->SMState` 参照）。
 * \retval  E2E_E_INPUTERR_NULL  StatePtr または ConfigPtr が NULL。
 * \retval  E2E_E_WRONGSTATE     StatePtr->SMState が E2E_SM_DEINIT のまま。
 *
 * \note    [SWS_E2E_00216] によりライブラリは DET/DEM を一切呼んではならない
 *          ため、上記いずれの場合も Det_ReportError() は呼ばない。
 *
 * \warning [SWS_E2E_00343] の値定義は `E2E_SM_VALID=0x00`・`E2E_SM_DEINIT=0x01`
 *          であるため、`StatePtr` をゼロ初期化しただけでは `E2E_SM_DEINIT`
 *          にはならず、誤って `E2E_SM_VALID` 扱いになってしまう
 *          （`E2E_E_WRONGSTATE` によるガードが効かず、初期化されていない
 *          `ProfileStatusWindow` へアクセスしうる）。呼び出し元は必ず
 *          `E2E_SMCheckInit()` を明示的に呼んでから本関数を使うこと。
 *
 * \AUTOSARReq     {SWS_E2E_00340, SWS_E2E_00371, SWS_E2E_00345, SWS_E2E_00466}
 * \ServiceID      {0x30}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType E2E_SMCheck(E2E_PCheckStatusType ProfileStatus, const E2E_SMConfigType* ConfigPtr,
                            E2E_SMCheckStateType* StatePtr)
{
    DET_LOGT(TAG, "called");
    if (StatePtr == NULL || ConfigPtr == NULL)
        return E2E_E_INPUTERR_NULL;

    switch (StatePtr->SMState)
    {
        case E2E_SM_DEINIT:
            return E2E_E_WRONGSTATE;

        case E2E_SM_NODATA:
            if (ProfileStatus != E2E_P_ERROR && ProfileStatus != E2E_P_NONEWDATA)
                StatePtr->SMState = E2E_SM_INIT;
            break;

        case E2E_SM_INIT:
            E2E_SMAddStatus(ProfileStatus, ConfigPtr, StatePtr);
            if (StatePtr->ErrorCount <= ConfigPtr->MaxErrorStateInit
                && StatePtr->OkCount >= ConfigPtr->MinOkStateInit)
            {
                StatePtr->SMState = E2E_SM_VALID;
            }
            else if (StatePtr->ErrorCount > ConfigPtr->MaxErrorStateInit)
            {
                StatePtr->SMState = E2E_SM_INVALID;
            }
            /* else: INIT に留まる（まだ OK 件数が閾値未達） */
            break;

        case E2E_SM_VALID:
            E2E_SMAddStatus(ProfileStatus, ConfigPtr, StatePtr);
            if (!(StatePtr->ErrorCount <= ConfigPtr->MaxErrorStateValid
                  && StatePtr->OkCount >= ConfigPtr->MinOkStateValid))
            {
                StatePtr->SMState = E2E_SM_INVALID;
            }
            /* else: VALID を維持 */
            break;

        case E2E_SM_INVALID:
            E2E_SMAddStatus(ProfileStatus, ConfigPtr, StatePtr);
            if (StatePtr->ErrorCount <= ConfigPtr->MaxErrorStateInvalid
                && StatePtr->OkCount >= ConfigPtr->MinOkStateInvalid)
            {
                StatePtr->SMState = E2E_SM_VALID;
            }
            /* else: INVALID に留まる */
            break;

        default:
            return E2E_E_INTERR;
    }

    return E2E_E_OK;
}

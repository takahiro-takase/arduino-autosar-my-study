/**
 * \file    Det.c
 * \brief   Default Error Tracer 実装 (ログレベル判定・メッセージ整形)
 *
 * \details 出力そのもの (Arduino Serial) は Hal/Det_Hw.cpp に委譲し、本ファイルは
 *          Arduino API を一切参照しない（詳細は Det_Hw.h 参照）。
 *
 *          出力フォーマット:
 *            [<ms>ms] LEVEL TAG: func: message\r\n
 *            LEVEL は 5 文字固定 (ERROR/WARN /INFO /TRACE/DEBUG) で列が揃う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <stdarg.h>
#include <stdio.h>
#include "Det.h"
#include "Det_Hw.h"

#define TAG "Det"

/**
 * \brief   Det モジュールを初期化する（[SWS_Det_00008]）。
 *
 * \details 実仕様は「内部変数の設定」を初期化の目的として挙げるが、本
 *          プロジェクトの Det はログ出力の都度その場でフォーマットするだけの
 *          ステートレスな実装であり、初期化を要する内部変数を一切持たない
 *          （`Det_ReportError()`/`Log_Write()` は本関数の呼び出し有無に
 *          関わらず常に動作する。他の全 BSW モジュールの `_Init()` 内で
 *          発生しうる初期化前エラーも report できる必要があるため、この
 *          「常に動作する」という既存の挙動はあえて変更しない）。そのため
 *          本関数は仕様上の呼び出し位置（`EcuM_Init()` の先頭、他の全 BSW
 *          モジュール初期化より前）に配線するだけの no-op とする。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build
 *                        設定を持たないため）。
 *
 * \AUTOSARReq     {SWS_Det_00008}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Det_Init(const Det_ConfigType* ConfigPtr)
{
    (void)ConfigPtr;  /* 常に NULL（Det.h 参照）。初期化を要する内部変数を持たないため no-op */
    DET_LOGT(TAG, "called");
    DET_LOGI(TAG, "Init ok");
}

void Log_Write(LogLevel lvl, const char* tag, const char* func, const char* fmt, ...)
{
    if (lvl > DET_LOG_LEVEL) return;  /* DET_LOG_LEVEL より重要度が低いログは抑制 */

    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Det_Hw_PrintLogLine(lvl, tag, func, buf);
}

/**
 * \brief   開発エラーを標準化された形式で通知する（AUTOSAR Det_ReportError 準拠）。
 *
 * \details 上記の DET_LOG* マクロ（呼び出し元固有の自由文字列、人間が読むための
 *          詳細情報）とは別のチャネルとして、ModuleId/InstanceId/ApiId/ErrorId
 *          という標準化された 4 つ組を報告する。呼び出し元は通常、詳細を
 *          伝える DET_LOGE(...) と、この Det_ReportError(...) の両方を呼ぶ
 *          （情報を失わずに標準準拠のエラー通知も行うため）。
 *
 *          本実装は実 AUTOSAR の「コールアウトフック登録・実行停止」等の
 *          高度な機能を持たない学習用の簡略実装で、単に
 *          `[<ms>ms] DET M=<ModuleId> I=<InstanceId> API=0x<ApiId> ERR=0x<ErrorId>`
 *          という 1 行を出力するのみ。DET_LOG_LEVEL によるレベル抑制の対象外
 *          （開発エラーは常に最重要度のため）。
 *
 * \param[in]  ModuleId    エラーを検出したモジュールの AUTOSAR ModuleId
 *                         （例: Com = 50）。
 * \param[in]  InstanceId  マルチインスタンスモジュールのインスタンス番号。
 *                         シングルインスタンスモジュールは 0 を渡す。
 * \param[in]  ApiId       エラーを検出した API のサービス ID
 *                         （呼び出し元モジュールの SWS で定義される値。
 *                         本プロジェクトでは各関数の Doxygen \ServiceID タグと
 *                         一致させる）。
 * \param[in]  ErrorId     検出した開発エラーの ID
 *                         （呼び出し元モジュールの SWS で定義される値）。
 *
 * \return  Std_ReturnType。実 AUTOSAR は「値を返すが実際には使われない」と
 *          規定しているため、本実装は常に E_OK を返す。
 *
 * \AUTOSARReq     {SWS_Det_00009}
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    /* DET_LOG_LEVEL によるレベル抑制は行わない（開発エラーは常に最重要度）。
     * DET_LOGE(...) と同じ書式のヘルパー（例: 2桁 0 埋め 16 進）を独自に持たず、
     * Serial の HEX 出力（Print::print(v, HEX)）をそのまま使う。0x0 のような
     * 1 桁出力になる場合があるが、ApiId/ErrorId は本プロジェクトの規模では
     * いずれも 1 バイト範囲に収まるため実用上の判読性は損なわない。 */
    Det_Hw_PrintDetError(ModuleId, InstanceId, ApiId, ErrorId);

    /* [SWS_Det_00009]: 戻り値は互換性のためだけに存在し、実際には使われない。 */
    return E_OK;
}

/**
 * \brief   Det モジュールを起動する（[SWS_Det_00010]）。
 *
 * \details 実仕様は「Det の環境（統合者）が Det 自身のセルフテストを
 *          トリガする」用途を想定するが、本プロジェクトはそのようなセルフ
 *          テスト機構を持たないため no-op とする（実仕様自身も「起動時
 *          呼び出しを要しない Det 実装では空でよい」と明記している）。
 *
 * \AUTOSARReq     {SWS_Det_00010}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Det_Start(void)
{
    DET_LOGT(TAG, "called");
    /* セルフテスト機構を持たないため no-op（Det.h 参照）。 */
}

void Log_HexStr(char* dst, uint8_t dstSize,
                const uint8_t* src, uint8_t srcLen)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t pos = 0U;
    for (uint8_t i = 0U; i < srcLen && (pos + 3U) < dstSize; i++)
    {
        if (i > 0U) dst[pos++] = ' ';
        dst[pos++] = hex[src[i] >> 4U];
        dst[pos++] = hex[src[i] & 0x0FU];
    }
    dst[pos] = '\0';
}

/**
 * \brief   Det モジュールのバージョン情報を取得する。
 *
 * \details 他 BSW モジュールと共通の慣例により、未初期化時でもエラー報告
 *          しない例外 API のため、初期化状態は確認せず NULL ポインタ
 *          チェックのみ行う（Det_Init()/Det_ReportError() 自身も内部状態を
 *          持たず常に動作するため、この点は他モジュール以上に自明である）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_Det_00011}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Det_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(DET_MODULE_ID, 0U, DET_API_ID_GET_VERSION_INFO, DET_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = DET_VENDOR_ID;
    versioninfo->moduleID         = DET_MODULE_ID;
    versioninfo->sw_major_version = DET_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = DET_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = DET_SW_PATCH_VERSION;
}

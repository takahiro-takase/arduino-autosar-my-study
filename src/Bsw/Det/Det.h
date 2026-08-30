/**
 * \file    Det.h
 * \brief   Default Error Tracer 公開インタフェース (AUTOSAR DET 簡易実装)
 *
 * \details ログ出力フォーマット:
 *            [<ms>ms] ERROR <TAG>: message
 *            [<ms>ms] WARN  <TAG>: message
 *            [<ms>ms] INFO  <TAG>: message
 *            [<ms>ms] TRACE <TAG>: message
 *            [<ms>ms] DEBUG <TAG>: message
 *
 *          TRACE は関数コールチェーンの確認専用（関数先頭に機械的に置く、
 *          AUTOSAR 仕様書記載 I/F 関数・内部関数を問わない）。INFO と DEBUG の
 *          間に位置させ、DET_LOG_LEVEL を TRACE にすれば DEBUG の詳細ログ
 *          （フレームデータ等）を混ぜずにコールチェーンだけを追える。既定
 *          （DET_LOG_LEVEL=LOG_I）の実機では出力されず、ホストテスト環境
 *          （[env:native]/[env:native_chain]、DET_LOG_LEVEL=LOG_T）でのみ
 *          DET_LOG_VERBOSE=1 と併用して確認する（Det_Cfg.h 参照）。
 *
 *          各ソースファイルで TAG を定義して使用する:
 *            #define TAG "Can"
 *            DET_LOGI(TAG, "Init ok baud=%d", 500000);
 *            DET_LOGE(TAG, "controller %d failed", id);
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DET_H
#define DET_H

#include <stdint.h>
#include "Platform_Types.h"
#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * ログレベル
 * ----------------------------------------------------------------------- */

typedef enum
{
    LOG_E = 0,  /**< Error   — エラー (abort / NRC / NULL ptr) */
    LOG_W,      /**< Warning — 警告  (BUSY / no route)         */
    LOG_I,      /**< Info    — 通常情報 (Init / 状態遷移)      */
    LOG_T,      /**< Trace   — 関数コールチェーンの確認専用（関数先頭で機械的に出す） */
    LOG_D       /**< Debug   — 詳細  (フレームデータ等)        */
} LogLevel;

#include "Det_Cfg.h"

/* -----------------------------------------------------------------------
 * バッファサイズ (メッセージ部; ヘッダは Serial で直接出力)
 * ----------------------------------------------------------------------- */

#ifndef LOG_BUF_SIZE
#  define LOG_BUF_SIZE 64U
#endif

/* -----------------------------------------------------------------------
 * コア出力関数
 * ----------------------------------------------------------------------- */

/**
 * "[<ms>ms] LEVEL TAG/func: msg\r\n" を 1 行出力する。
 * func は呼び出し元の関数名（DET_LOGx マクロが __func__ を自動的に渡す。
 * 呼び出し元で明示的に指定する必要はない）。fmt は printf 形式の可変長引数。
 */
void Log_Write(LogLevel lvl, const char* tag, const char* func, const char* fmt, ...);

/**
 * バイト列 src を "XX XX XX ..." 形式の hex 文字列として dst に書き込む。
 * dst は少なくとも (3 * srcLen) バイト必要。末尾 '\0' を付加する。
 */
void Log_HexStr(char* dst, uint8_t dstSize,
                const uint8_t* src, uint8_t srcLen);

/* -----------------------------------------------------------------------
 * 便利マクロ
 * ----------------------------------------------------------------------- */

#define DET_LOGE(tag, fmt, ...)  Log_Write(LOG_E, tag, __func__, fmt, ##__VA_ARGS__)
#define DET_LOGW(tag, fmt, ...)  Log_Write(LOG_W, tag, __func__, fmt, ##__VA_ARGS__)
#define DET_LOGI(tag, fmt, ...)  Log_Write(LOG_I, tag, __func__, fmt, ##__VA_ARGS__)
#define DET_LOGT(tag, fmt, ...)  Log_Write(LOG_T, tag, __func__, fmt, ##__VA_ARGS__)
#define DET_LOGD(tag, fmt, ...)  Log_Write(LOG_D, tag, __func__, fmt, ##__VA_ARGS__)

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
Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);

/**
 * \brief   Det モジュールのバージョン情報を取得する。
 *
 * \details 他 BSW モジュールと共通の慣例により、未初期化時でもエラー報告
 *          しない例外 API のため、初期化状態は確認せず NULL ポインタ
 *          チェックのみ行う（Det 自身に Init は存在しないため、この点は他
 *          モジュール以上に自明である）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_Det_00011}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Det_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* DET_H */

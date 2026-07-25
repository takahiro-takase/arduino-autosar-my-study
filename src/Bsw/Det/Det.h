/**
 * \file    Det.h
 * \brief   Default Error Tracer 公開インタフェース (AUTOSAR DET 簡易実装)
 *
 * \details ログ出力フォーマット:
 *            [<ms>ms] ERROR <TAG>: message
 *            [<ms>ms] WARN  <TAG>: message
 *            [<ms>ms] INFO  <TAG>: message
 *            [<ms>ms] DEBUG <TAG>: message
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
#include <avr/pgmspace.h>
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
 * "[<ms>ms] LEVEL TAG: msg\r\n" を 1 行出力する。
 * tag_P / fmt_P は PROGMEM ポインタ (マクロが PSTR でラップ)。
 * msg は printf 形式の可変長引数。
 */
void Log_Write(LogLevel lvl, PGM_P tag_P, PGM_P fmt_P, ...);

/**
 * バイト列 src を "XX XX XX ..." 形式の hex 文字列として dst に書き込む。
 * dst は少なくとも (3 * srcLen) バイト必要。末尾 '\0' を付加する。
 */
void Log_HexStr(char* dst, uint8_t dstSize,
                const uint8_t* src, uint8_t srcLen);

/* -----------------------------------------------------------------------
 * 便利マクロ
 * ----------------------------------------------------------------------- */

#define DET_LOGE(tag, fmt, ...)  Log_Write(LOG_E, PSTR(tag), PSTR(fmt), ##__VA_ARGS__)
#define DET_LOGW(tag, fmt, ...)  Log_Write(LOG_W, PSTR(tag), PSTR(fmt), ##__VA_ARGS__)
#define DET_LOGI(tag, fmt, ...)  Log_Write(LOG_I, PSTR(tag), PSTR(fmt), ##__VA_ARGS__)
#define DET_LOGD(tag, fmt, ...)  Log_Write(LOG_D, PSTR(tag), PSTR(fmt), ##__VA_ARGS__)

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

#ifdef __cplusplus
}
#endif

#endif /* DET_H */

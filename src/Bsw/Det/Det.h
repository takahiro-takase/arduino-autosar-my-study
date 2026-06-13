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

#ifdef __cplusplus
}
#endif

#endif /* DET_H */

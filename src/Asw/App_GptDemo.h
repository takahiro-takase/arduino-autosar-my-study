/**
 * \file    App_GptDemo.h
 * \brief   Gpt 実 HW タイマ動作確認用 SW-C 公開インタフェース
 * \details Gpt モジュール（src/Bsw/Gpt/）の実機動作を検証するための、
 *          最小構成の常駐デモ SW-C。1 秒周期の HW タイマ割り込みが
 *          実際に発生していることを、既存の DET ログのタイムスタンプと
 *          突き合わせて目視確認できるようにする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef APP_GPT_DEMO_H
#define APP_GPT_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Gpt デモ SW-C を初期化する。
 *
 * \details Gpt_StartTimer(GPT_CHANNEL_0, 1000U) で 1000 tick（Channel 0 は
 *          1000Hz 設定のため 1000 ms）周期の連続タイマを開始し、
 *          Gpt_EnableNotification() で割り込み通知を有効化する。
 *          EcuM_Init() から Gpt_Init() の後に一度だけ呼び出すこと。
 */
void App_GptDemo_Init(void);

/**
 * \brief   Gpt 通知カウンタと Gpt_GetTimeElapsed() を DET ログへ出力する Runnable。
 *
 * \details OS の周期タスクから呼び出すこと（メインループ/非 ISR コンテキスト）。
 *          App_GptDemo_OnTick() が加算した volatile カウンタを読み取るだけの
 *          軽量処理であり、Serial 出力自体はここでのみ行う
 *          （App_GptDemo_OnTick() は ISR セーフのためログを出さない。
 *          App_GptDemo.c 冒頭のコメント参照）。
 */
void App_GptDemo_Run(void);

/**
 * \brief   Gpt 通知コールバック（Gpt_PBCfg.c の Channel 0 GptNotification）。
 *
 * \details ISR コンテキストから直接呼ばれるため、volatile カウンタの
 *          インクリメントのみを行う。Serial/DET 出力・Os 呼び出し等の
 *          ブロッキング処理は絶対に行わないこと（App_GptDemo.c 冒頭の
 *          コメント参照）。
 */
void App_GptDemo_OnTick(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_GPT_DEMO_H */

/**
 * \file    App_TestMsg.h
 * \brief   テスト用メッセージ (TestMsg, CAN 0x300) 送信 SW-C 公開インタフェース
 * \details 信号表（config/data/can_signals.json）へ信号を追加する手順を
 *          試すための、テスト専用の最小 SW-C。1 秒ごとに +1 するカウンタを
 *          TestMsg.TestCounter へ書き込む。CAN Tool の受信モニタに
 *          信号名つきで表示されるため、追加した信号の値を目視で確認できる。
 *          手順は tools/configurator/README.md の「信号を追加する練習」を参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef APP_TEST_MSG_H
#define APP_TEST_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   TestMsg.TestCounter を 1 つ進めて Rte 経由で書き込む Runnable。
 *
 * \details OS の周期タスク（1000 ms）から呼び出すこと。カウンタは uint8 で、
 *          255 の次は 0 に戻る。TestMsg は Com で PERIODIC 送信に設定して
 *          あるため、値の書き込みとは独立に周期（1000 ms）で送信される。
 *
 * \note    テスト専用。ECU の本来の機能には関与しない。
 */
void App_TestMsg_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TEST_MSG_H */

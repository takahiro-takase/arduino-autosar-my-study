/**
 * \file    Os_PBCfg.c
 * \brief   OS ポストビルドコンフィグ 定義
 * \details プロジェクトで使用するタスクテーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル
 *          に相当する。
 *
 *          タスク一覧:
 *            Task 0: Can_MainFunction_Read         1 ms  — RX 割り込みペンディングのドレイン
 *            Task 1: CanTp_MainFunction            1 ms  — タイムアウト監視・CF 送信
 *            Task 2: Rte_ScheduleRunnables      3000 ms  — エンジン Runnable 起動
 *            Task 3: Rte_ScheduleWarningIndicator 500 ms — 警告灯 Runnable 起動
 *            Task 4: CanSM_MainFunction            10 ms  — Bus-Off 回復タイマ監視
 *            Task 5: Com_MainFunction             100 ms  — RX 受信デッドライン監視
 *            Task 6: IoHwAb_MainFunction           10 ms  — ボタンデバウンスサンプリング
 *            Task 7: WdgM_MainFunction           6000 ms  — Alive/Logical/Deadline 判定
 *            Task 8: Dcm_MainFunction            1000 ms  — S3 セッションタイムアウト監視
 *            Task 9: FiM_MainFunction             100 ms  — 機能抑止状態の再評価
 *            Task 10: WdgM_TriggerHwWatchdog      1000 ms  — HW ウォッチドッグ trigger（リフレッシュ）
 *            Task 11: Nm_MainFunction             1000 ms  — NM フレーム送信（ComM FULL_COM 中のみ）
 *            Task 12: NvM_MainFunction             10 ms  — 保留中 EEPROM 書き込みジョブを1バイトずつ処理
 *            Task 13: Can_MainFunction_Write        1 ms  — 保留中 TX 確認 (CanIf_TxConfirmation) をドレイン
 *            Task 14: Can_MainFunction_BusOff       1 ms  — Bus-Off (EFLG.TXBO) ポーリング
 *            Task 15: Can_MainFunction_Wakeup       1 ms  — SLEEP 中のウェイクアップペンディングのドレイン
 *            Task 16: SecOC_MainFunctionTx           100 ms  — TX Secured I-PDU の Freshness/MAC 計算・送信
 *            Task 17: MemIf_MainFunction            10 ms  — 保留中 EEPROM 書き込みジョブを1バイトずつ処理 (Fee)
 *            Task 18: App_GptDemo_Run             2000 ms  — Gpt 実 HW タイマ通知カウンタのログ出力（動作確認用）
 *            Task 19: ComM_MainFunction            100 ms  — 現状 NOP（ComMNmVariant=FULL では
 *                                                             [SWS_ComM_00888] によりヒステリシス
 *                                                             タイマ不要。ComM.c 参照）
 *
 *          CAN 受信が真のハードウェア割り込み (Can_Isr(), INT ピン立ち下がりで
 *          attachInterrupt 起動) になったことに伴い、旧 Task 0 (Can_Isr の
 *          ポーリング呼び出し) を廃止し、AUTOSAR が定義する 3 つの独立した
 *          Can_MainFunction_xxx (Read/BusOff/Wakeup) へ分離した
 *          (詳細は Can.c ファイル冒頭のコメントを参照)。
 *
 *          周期の根拠:
 *            Can_MainFunction_Read / CanTp_MainFunction は CAN フレーム到着の
 *            応答性と CanTp タイムアウト精度のため 1 ms とする。
 *            Can_MainFunction_BusOff / Can_MainFunction_Wakeup も、旧 Can_Isr
 *            が担っていた検出精度をそのまま維持するため同じ 1 ms とする。
 *            App_EngineManager_Run は RTE コントラクト (3 秒周期) に従い 3000 ms とする。
 *            App_WarningIndicator_Run は LED 点滅半周期 (500 ms ON/OFF) のため 500 ms とする。
 *            CanSM_MainFunction は Bus-Off 回復タイマの精度を確保するため 10 ms とする。
 *            IoHwAb_MainFunction はデバウンス閾値 40ms (4 サンプル × 10ms) を実現するため 10 ms とする。
 *            WdgM_TriggerHwWatchdog は WdgM_MainFunction (判定, 6000ms) とは別に、
 *            HW ウォッチドッグのタイムアウト (4000ms) より十分短い周期で
 *            リフレッシュするため 1000 ms とする。Renesas RA4M1 の IWDT 最大
 *            タイムアウト（約 5592ms）が判定サイクル (6000ms) より短く、
 *            判定サイクルに直接同期できないための分離（詳細は WdgM_Cfg.h の
 *            WDGM_HW_WATCHDOG_TIMEOUT_MS コメントを参照）。
 *            Nm_MainFunction は MeterStatus (3000ms) より高頻度な 1000 ms とし、
 *            WdgM_TriggerHwWatchdog と同じ「中頻度の BSW ハウスキーピング」
 *            周期に揃えている（詳細は Nm_Cfg.h を参照）。
 *            NvM_MainFunction は CanSM_MainFunction / IoHwAb_MainFunction と同じ
 *            10 ms とする（ブロック・CRC・冗長化のオーケストレーションのみで
 *            EEPROM I/O は発生しないため軽量）。MemIf_MainFunction も同じ
 *            10 ms とし、実際の物理バイト書き込み（Fee が 1 呼び出しあたり
 *            1 バイトのみ）を進める。両タスクとも Task 12/17 として同一パス内
 *            (インデックス昇順) で毎回実行されるため、NvM がジョブを開始した
 *            ティックのうちに MemIf 側の最初の 1 バイトも書かれる。1 ブロック
 *            最大 10 バイト（データ本体+CRC）を 100ms 以内に書き終えられ、
 *            DTC 確定から永続化完了までの遅延を実用上問題ない範囲に抑えつつ、
 *            1 呼び出しあたりの処理は EEPROM 1 バイト分の書き込みのみに
 *            抑えられる（詳細は NvM.c / MemIf.c を参照）。
 *            Can_MainFunction_Write は Can_Isr / CanTp_MainFunction と同じ 1 ms
 *            とする。TX 確認 (CanIf_TxConfirmation) は元々 Can_Write() の
 *            呼び出しと同期していたため、遅延を体感できない範囲に抑える
 *            （詳細は Can.c ファイル冒頭のコメントを参照）。
 *            SecOC_MainFunctionTx は Com_MainFunction と同じ 100 ms とする
 *            （周期自体は RX 方向の SecOC_IfRxIndication() とは無関係で、
 *            TX 方向専用のタスク。現在 TX 方向で SecOC を使う PDU は無い
 *            〈SECOC_TX_PDU_COUNT=0、SecOC.c 冒頭コメント参照〉ため、
 *            毎回 0 回実行で終わる no-op になっている）。
 *            この 100ms という値自体は、以前 E2EHealthStatus（PERIODIC
 *            送信周期 6000ms）を TX 方向で SecOC 保護していた際に、
 *            SecOC_IfTransmit() が Authentic I-PDU をバッファへコピーして
 *            から実際に CAN へ送信されるまでの遅延を無視できる範囲に抑える
 *            目的で選んだもの。E2EHealthStatus は E2E Profile05 単体保護へ
 *            切り替えた際に SecOC を撤去したため、この根拠は現在は当てはまら
 *            ない（将来 TX 方向で SecOC を使う PDU が追加された時点で、その
 *            PDU の送信周期を基準に周期を選び直すこと）。
 *            App_GptDemo_Run は Gpt Channel 0（1000ms 周期の HW タイマ割り込み）
 *            が実際に発火しているかを DET ログで確認するための動作確認用タスク
 *            であり、通知そのものより高頻度で読む必要はないため、Gpt Channel 0
 *            の周期の倍の 2000 ms とし、複数回の通知を跨いでカウンタが
 *            単調増加していることを目視確認しやすくしている。
 *            ComM_MainFunction は以前スケジューラに未登録で一度も呼ばれて
 *            いなかった（2026-08 のスペック監査で発見）。現状は
 *            Com_MainFunction/FiM_MainFunction/SecOC_MainFunctionTx と同じ
 *            100 ms ティアに揃えているが、[SWS_ComM_00888] のとおり本プロジェクトの
 *            構成（ComMNmVariant=FULL 相当）では中身は NOP のため、この周期
 *            自体に現状意味はない（詳細は ComM.c 参照）。BswM のタスク
 *            有効/無効制御（BswM_Cfg.h の BSWM_TASK_MASK_*）には含めていない
 *            （Task 18: App_GptDemo_Run と同じ扱い。NOP である以上、RUN/
 *            SHUTDOWN いずれの状態でも動いていて実害が無いため、既存の
 *            BSWM_TASK_MASK_ALL 等のビットマスクを手計算で拡張するリスクを
 *            負う理由がない）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Os_PBCfg.h"
#include "Os_Cfg.h"

/* タスク本体の前方宣言 (各モジュールのヘッダをここでインクルードする代わりに
 * extern 宣言を使用し、Os が各 BSW モジュールに依存しないようにする)  */
extern void Can_MainFunction_Read(void);
extern void CanTp_MainFunction(void);
extern void Rte_ScheduleRunnables(void);
extern void Rte_ScheduleWarningIndicator(void);
extern void CanSM_MainFunction(void);
extern void Com_MainFunction(void);
extern void IoHwAb_MainFunction(void);
extern void WdgM_MainFunction(void);
extern void Dcm_MainFunction(void);
extern void FiM_MainFunction(void);
extern void WdgM_TriggerHwWatchdog(void);
extern void Nm_MainFunction(void);
extern void NvM_MainFunction(void);
extern void Can_MainFunction_Write(void);
extern void Can_MainFunction_BusOff(void);
extern void Can_MainFunction_Wakeup(void);
extern void SecOC_MainFunctionTx(void);
extern void MemIf_MainFunction(void);
extern void App_GptDemo_Run(void);
extern void ComM_MainFunction(void);

/* -----------------------------------------------------------------------
 * タスクテーブル
 * インデックスがそのままタスク ID に対応する。
 * 実行順序はインデックス昇順 (優先度なし)。
 * ----------------------------------------------------------------------- */
static const Os_TaskType Os_TaskTable[OS_TASK_COUNT] =
{
    /* Task 0 */ { Can_MainFunction_Read,        1U    },  /* 1 ms    : RX 割り込みペンディングのドレイン */
    /* Task 1 */ { CanTp_MainFunction,           1U    },  /* 1 ms    : CanTp タイムアウト監視    */
    /* Task 2 */ { Rte_ScheduleRunnables,        3000U },  /* 3000 ms : エンジン Runnable         */
    /* Task 3 */ { Rte_ScheduleWarningIndicator, 500U  },  /* 500 ms  : 警告灯 Runnable           */
    /* Task 4 */ { CanSM_MainFunction,           10U   },  /* 10 ms   : BusOff 回復タイマ監視     */
    /* Task 5 */ { Com_MainFunction,             100U  },  /* 100 ms  : COM 受信デッドライン監視  */
    /* Task 6 */ { IoHwAb_MainFunction,          10U   },  /* 10 ms   : ボタンデバウンスサンプリング */
    /* Task 7 */ { WdgM_MainFunction,            6000U },  /* 6000 ms : Alive/Logical/Deadline 判定 */
    /* Task 8 */ { Dcm_MainFunction,             1000U },  /* 1000 ms : S3 セッションタイムアウト監視 */
    /* Task 9 */ { FiM_MainFunction,             100U  },  /* 100 ms  : 機能抑止状態の再評価      */
    /* Task 10 */ { WdgM_TriggerHwWatchdog,      1000U },  /* 1000 ms : HW ウォッチドッグ trigger */
    /* Task 11 */ { Nm_MainFunction,             1000U },  /* 1000 ms : NM フレーム送信           */
    /* Task 12 */ { NvM_MainFunction,              10U  },  /* 10 ms   : 保留中 EEPROM ジョブ処理  */
    /* Task 13 */ { Can_MainFunction_Write,          1U  },  /* 1 ms    : 保留中 TX 確認をドレイン  */
    /* Task 14 */ { Can_MainFunction_BusOff,         1U  },  /* 1 ms    : Bus-Off ポーリング        */
    /* Task 15 */ { Can_MainFunction_Wakeup,         1U  },  /* 1 ms    : ウェイクアップペンディングのドレイン */
    /* Task 16 */ { SecOC_MainFunctionTx,            100U  },  /* 100 ms  : TX Secured I-PDU の Freshness/MAC 計算・送信 */
    /* Task 17 */ { MemIf_MainFunction,             10U  },  /* 10 ms   : 保留中 EEPROM ジョブ処理 (Fee 物理バイト書き込み) */
    /* Task 18 */ { App_GptDemo_Run,              2000U  },  /* 2000 ms : Gpt 実 HW タイマ通知カウンタのログ出力 (動作確認用) */
    /* Task 19 */ { ComM_MainFunction,             100U  }   /* 100 ms  : 現状 NOP（SWS_ComM_00888、ComM.c 参照） */
};

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が Os_Init に渡す)
 * ----------------------------------------------------------------------- */
const Os_ConfigType Os_Config =
{
    Os_TaskTable,
    OS_TASK_COUNT
};

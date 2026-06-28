/**
 * \file    Dem_Cfg.h
 * \brief   DEM プリコンパイル設定 (AUTOSAR SWS_DEM 準拠)
 * \details DEM モジュールのコンパイル時定数を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 *          本プロジェクトで管理するイベント:
 *            DEM_EVENT_ENGINE_OVERHEAT      — 冷却水温過熱 (temp >= 100°C)
 *            DEM_EVENT_ENGINE_STALL         — エンジン失速 (speed < 100 RPM)
 *            DEM_EVENT_ENGINE_SPEED_NO_FLAG — フラグなし回転検出
 *            DEM_EVENT_STARTING_TIMEOUT     — 起動タイムアウト (5 秒超過)
 *            DEM_EVENT_COMM_TIMEOUT         — EngineInfo 受信タイムアウト
 *            DEM_EVENT_BUTTON_STUCK         — 警告確認ボタン固着 (5 秒以上押下)
 *            DEM_EVENT_ADC_VOLT_LOW         — ADC センサ電圧低下 (< 1000 mV)
 *            DEM_EVENT_CAN_BUSOFF           — CAN Bus-Off 回復断念（最大試行超過）
 *
 *          EEPROM レイアウト (Arduino UNO 内蔵 EEPROM 1KB の先頭 17 バイト使用):
 *            Addr 0x00: マジックバイト (0xDE = 有効な DEM データ)
 *            Addr 0x01-0x08: イベント 0-7 ステータスバイト (NVM_BLOCK_ID_DEM_STATUS)
 *            Addr 0x09-0x10: イベント 0-7 経年回復(Aging)カウンタ (NVM_BLOCK_ID_DEM_AGING)
 *
 *          経年回復 (Aging):
 *            CONFIRMED（確定）した DTC は、再故障せずに DEM_AGING_CYCLES_THRESHOLD 回の
 *            操作サイクル（起動〜次回起動）を経ると自動的に CONFIRMED が解除される
 *            (Dem_Init() が起動ごとに前サイクルの結果を評価する)。
 *            実車では数十サイクル単位が一般的だが、本プロジェクトでは実機での動作
 *            確認を電源再投入数回で行えるよう小さい値にしている。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DEM_CFG_H
#define DEM_CFG_H

/* -----------------------------------------------------------------------
 * イベント ID 定義
 * App_EngineManager の状態ハンドラが Dem_ReportErrorStatus() に渡す。
 * ----------------------------------------------------------------------- */
#define DEM_EVENT_ENGINE_OVERHEAT       0U  /**< 冷却水温過熱                     */
#define DEM_EVENT_ENGINE_STALL          1U  /**< エンジン失速                     */
#define DEM_EVENT_ENGINE_SPEED_NO_FLAG  2U  /**< フラグなし回転検出               */
#define DEM_EVENT_STARTING_TIMEOUT      3U  /**< 起動タイムアウト                 */
#define DEM_EVENT_COMM_TIMEOUT          4U  /**< EngineInfo 受信タイムアウト      */
#define DEM_EVENT_BUTTON_STUCK          5U  /**< 警告確認ボタン固着（5 秒以上押下）*/
#define DEM_EVENT_ADC_VOLT_LOW          6U  /**< ADC センサ電圧低下（< 1000 mV）  */
#define DEM_EVENT_CAN_BUSOFF            7U  /**< CAN Bus-Off 回復断念             */
#define DEM_EVENT_COUNT                 8U  /**< イベント総数                     */

/** 経年回復 (Aging): CONFIRMED した DTC が、再故障せずに連続でこの回数分の
 *  操作サイクルを経過すると自動的に CONFIRMED を解除する。
 *  全イベント共通の閾値とする（学習用簡略化。実車ではイベントごとに
 *  個別設定できる DemIndicatorAttribute 相当の仕組みがある）。 */
#define DEM_AGING_CYCLES_THRESHOLD      3U

/* -----------------------------------------------------------------------
 * DTC コード (24-bit, ISO 14229-1)
 * 製造者定義領域を使用 (byte1=0x00, byte2-3=0x01xx)
 * ----------------------------------------------------------------------- */
#define DEM_DTC_ENGINE_OVERHEAT         0x000101UL  /**< 冷却水温過熱             */
#define DEM_DTC_ENGINE_STALL            0x000102UL  /**< エンジン失速             */
#define DEM_DTC_ENGINE_SPEED_NO_FLAG    0x000103UL  /**< フラグなし回転           */
#define DEM_DTC_STARTING_TIMEOUT        0x000104UL  /**< 起動タイムアウト         */
#define DEM_DTC_COMM_TIMEOUT            0x000105UL  /**< EngineInfo 受信タイムアウト */
#define DEM_DTC_BUTTON_STUCK            0x000106UL  /**< 警告確認ボタン固着       */
#define DEM_DTC_ADC_VOLT_LOW            0x000107UL  /**< ADC センサ電圧低下       */
#define DEM_DTC_CAN_BUSOFF              0x000108UL  /**< CAN Bus-Off 回復断念     */

/* -----------------------------------------------------------------------
 * デバウンス (counter-based debouncing)
 * イベントごとに確定 (FAILED/PASSED) に必要なカウンタ絶対値を個別設定する
 * (実車の DemDebounceAlgorithmClass — イベントごとに別アルゴリズム/閾値を
 * 持てる — に相当)。FAILED 報告でカウンタ+1、PASSED 報告で-1 し、
 * ±閾値 に達した瞬間に DTC ステータスを確定する。
 *
 * 閾値の決め方:
 *   モニタ（報告元）が既に十分な持続性チェックを行ってから報告する場合は 1
 *   （例: IoHwAb の 5 秒固着判定、CanSM の 3 回リトライ後の断念—これらは
 *   報告そのものが「十分粘った結果」であり、Dem 側で重ねて debounce すると
 *   二重チェックになり確定が不必要に遅れる、あるいは確定不可能になる）。
 *   モニタが単発の閾値超えをそのまま報告する場合は 2 以上
 *   （例: エンジン系の温度/回転数の瞬時しきい値判定）。
 * ----------------------------------------------------------------------- */
#define DEM_DEBOUNCE_LIMIT_ENGINE_OVERHEAT       2  /**< 瞬時しきい値判定のため複数回要求 */
#define DEM_DEBOUNCE_LIMIT_ENGINE_STALL          2  /**< 瞬時しきい値判定のため複数回要求 */
#define DEM_DEBOUNCE_LIMIT_ENGINE_SPEED_NO_FLAG  2  /**< 瞬時しきい値判定のため複数回要求 */
#define DEM_DEBOUNCE_LIMIT_STARTING_TIMEOUT      2  /**< 瞬時しきい値判定のため複数回要求 */
#define DEM_DEBOUNCE_LIMIT_COMM_TIMEOUT          2  /**< 毎サイクル報告のため数百ms〜数秒で確定 */
#define DEM_DEBOUNCE_LIMIT_BUTTON_STUCK          1  /**< IoHwAb が 5 秒固着判定済み。二重チェック不要 */
#define DEM_DEBOUNCE_LIMIT_ADC_VOLT_LOW          2  /**< 毎サイクル報告のため数十 ms で確定 */
#define DEM_DEBOUNCE_LIMIT_CAN_BUSOFF            1  /**< CanSM が 3 回リトライ済み。二重チェック不要 */

/* -----------------------------------------------------------------------
 * DTC ステータスビットマスク (ISO 14229-1 Annex B)
 * ----------------------------------------------------------------------- */
#define DEM_STATUS_TEST_FAILED               0x01U  /**< bit0: testFailed                       */
#define DEM_STATUS_TF_THIS_OP_CYCLE          0x02U  /**< bit1: testFailedThisOperationCycle      */
#define DEM_STATUS_PENDING                   0x04U  /**< bit2: pendingDTC                        */
#define DEM_STATUS_CONFIRMED                 0x08U  /**< bit3: confirmedDTC (NvM 保存)           */
#define DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR 0x10U  /**< bit4: testNotCompletedSinceLastClear    */
#define DEM_STATUS_FAILED_SINCE_CLEAR        0x20U  /**< bit5: testFailedSinceLastClear          */
#define DEM_STATUS_NOT_COMPLETED_THIS_CYCLE  0x40U  /**< bit6: testNotCompletedThisOperationCycle */

/** SID 0x19 応答の statusAvailabilityMask: bits 0,2,3,4,5 をサポート */
#define DEM_STATUS_AVAILABILITY_MASK         0x2DU

/* -----------------------------------------------------------------------
 * NvM 関連定数
 * EEPROM アドレスは NvM_Cfg.h (NVM_BLOCK_DEM_* 定数) で管理する。
 * DEM はアドレスを知らず、NvM_BlockIdType (NVM_BLOCK_ID_DEM_*) でアクセスする。
 * ----------------------------------------------------------------------- */
#define DEM_NVM_MAGIC_BYTE  0xDEU  /**< EEPROM 有効データのマーカー (DEM 固有知識) */

/** SID 0x14 で "全 DTC クリア" を指定するグループコード */
#define DEM_GROUP_ALL_DTCS                   0xFFFFFFUL

#endif /* DEM_CFG_H */

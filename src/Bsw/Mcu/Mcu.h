/**
 * \file    Mcu.h
 * \brief   MCU Driver 公開インタフェース (AUTOSAR SWS_Mcu 準拠)
 * \details main.cpp / 他 BSW モジュールが MCU 固有のリセット原因検出を
 *          Mcu_Hw の型を直接知らずに扱えるようにする、MCU 抽象化層。
 *
 *          本実装の設計方針（実 AUTOSAR SWS_Mcu との差分）:
 *            - Mcu_InitClock() / Mcu_DistributePllClock() / Mcu_GetPllStatus() /
 *              Mcu_SetMode() / Mcu_InitRamSection() / Mcu_GetRamState() は
 *              実装しない。Arduino フレームワークが setup() 呼び出し前に
 *              クロック初期化を完了させており、本プロジェクトは複数電源
 *              モードや RAM セクション初期化もモデル化しないため。
 *            - Mcu_PerformReset() は実装する（2026-09 追加）。UDS 0x11
 *              ECUReset（Dcm_HandleEcuReset()）から、正応答送信後に呼ぶ
 *              唯一の呼び出し元。Cortex-M4 の NVIC_SystemReset() を発行する
 *              薄いラッパー（実処理は Mcu_Hw.c 参照）。
 *            - 上記により対応 API は Mcu_Init() / Mcu_GetResetReason() /
 *              Mcu_GetResetRawValue() / Mcu_PerformReset() / Mcu_GetVersionInfo()
 *              の 5 つのみ。
 *
 *          Mcu_Hw との呼び出し順序（重要）:
 *            Mcu_Hw_ReadAndClearResetReason()（Mcu_Hw.h 参照）はリセット
 *            原因レジスタを読み取ると同時にクリアするため、1 起動につき
 *            1 回しか呼べない。ブートローダ起因の WDT 無限リセットループ
 *            対策として、この読み取り＋クリアは他の何よりも先（Serial
 *            初期化より前）に行う必要がある（main.cpp 冒頭のコメント参照）。
 *            そのため Mcu_Init() は main.cpp の setup() 冒頭、
 *            Serial.begin() より前に呼ぶこと。この制約により Mcu_Init() は
 *            DET_LOGx/Det_ReportError を一切呼ばない（Serial.begin() 前に
 *            Serial.print() 系を呼んだ場合の Renesas RA USB-CDC 実装の挙動が
 *            未検証であり、最悪ハングして本関数が防ぐべき WDT 無限リセット
 *            ループ対策そのものを崩しかねないため。2026-08 のレビューで
 *            指摘・修正。詳細は Mcu.c 参照）。診断用のログは、呼び出し元の
 *            main.cpp が Serial 初期化後に Mcu_GetResetRawValue() 経由で
 *            別途出す。
 *
 *          Mcu_RawResetType のビット割当（本プロジェクト独自定義）:
 *            実 AUTOSAR は「HW のリセットステータスレジスタの生値」と
 *            規定するが（[SWS_Mcu_00235]）、Mcu_Hw は生レジスタ値を意図的に
 *            隠しデコード済みの Mcu_Hw_ResetReasonType（Watchdog/BrownOut/
 *            External/PowerOn の 4 フラグ）だけを返す設計のため（Mcu_Hw.h
 *            参照）、本実装ではこの 4 フラグを 1 バイトへビット詰めした値を
 *            「生値」として代用する:
 *              bit0 = Watchdog, bit1 = BrownOut, bit2 = External, bit3 = PowerOn
 *            （bit4-7 は常に 0）。Mcu_GetResetReason() が返す単一の
 *            Mcu_ResetType では複数同時要因を表現できないため、複数要因が
 *            同時に立つケース（電源投入直後に BrownOut も同時検出される等）
 *            を診断したい場合は Mcu_GetResetRawValue() を使うこと。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MCU_H
#define MCU_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "Mcu_Cfg.h"
#include "Mcu_PBCfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** Mcu_RawResetType のビット割当。Mcu.c（エンコード側）と、診断ログで
 *  個別フラグへデコードする呼び出し元（main.cpp 等）の両方がこの定義を
 *  共有する（2026-08 のレビューで、main.cpp 側がシフト量をハードコードで
 *  複製しており Mcu.c 側の割当と結びついていない問題を指摘され、この
 *  単一の定義に統一した）。 */
#define MCU_RAW_RESET_WATCHDOG_BIT  (1U << 0)
#define MCU_RAW_RESET_BROWNOUT_BIT  (1U << 1)
#define MCU_RAW_RESET_EXTERNAL_BIT  (1U << 2)
#define MCU_RAW_RESET_POWERON_BIT   (1U << 3)

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/**
 * \brief   リセット種別（[SWS_Mcu_00252] Mcu_ResetType の値域）。
 *
 * \details MCU_SW_RESET は本実装では返されない（Mcu_Hw がソフトウェア
 *          リセットを検出しないため。Mcu_Hw.h 参照）。Mcu_PerformReset()
 *          （NVIC_SystemReset() 発行）による再起動後も同様で、Renesas RA の
 *          RSTSR1.SWRF フラグを Mcu_Hw_ReadAndClearResetReason() が読んで
 *          いないため、次回起動時は他の要因（Watchdog/PowerOn 等）が
 *          立っていなければ MCU_RESET_UNDEFINED になる（UDS 0x11 ECUReset
 *          由来か物理リセットボタン由来かを起動後ログから区別したい場合は
 *          将来的な拡張が必要、現状は非対応）。BrownOut/External 単独検出時の
 *          扱いは Mcu_GetResetReason() のコメント参照。
 */
typedef enum
{
    MCU_POWER_ON_RESET = 0,  /**< 電源投入リセット（既定） */
    MCU_WATCHDOG_RESET,      /**< ウォッチドッグタイムアウトによるリセット */
    MCU_SW_RESET,            /**< ソフトウェアリセット（本実装では未使用） */
    MCU_RESET_UNDEFINED      /**< リセット原因不明・未初期化 */
} Mcu_ResetType;

/**
 * \brief   リセット原因の生値（[SWS_Mcu_00253] Mcu_RawResetType）。
 * \details ビット割当は Mcu.h 冒頭のコメント参照（本プロジェクト独自定義）。
 */
typedef uint8 Mcu_RawResetType;

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/**
 * \brief   Mcu モジュールを初期化し、リセット原因を読み取る。
 *
 * \details コンフィグを記録し（[SWS_Mcu_00026]。本プロジェクトはクロック/
 *          RAM セクション設定を持たないためプレースホルダの記録のみ）、
 *          Mcu_Hw_ReadAndClearResetReason() を呼んでリセット原因を
 *          1 度だけキャッシュする。呼び出しタイミングの制約は本ファイル
 *          冒頭のコメント参照。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_Mcu_00153, SWS_Mcu_00026}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Mcu_Init(const Mcu_ConfigType* ConfigPtr);

/**
 * \brief   Mcu_Init() 時にキャッシュしたリセット原因を返す。
 *
 * \details Watchdog フラグが立っていれば MCU_WATCHDOG_RESET、そうでなく
 *          PowerOn フラグが立っていれば MCU_POWER_ON_RESET、どちらでも
 *          なければ MCU_RESET_UNDEFINED を返す（BrownOut/External が単独で
 *          立っている場合を含む。Mcu_ResetType には対応する値がないため。
 *          複数要因を区別したい場合は Mcu_GetResetRawValue() を使うこと）。
 *
 * \retval  MCU_POWER_ON_RESET / MCU_WATCHDOG_RESET / MCU_RESET_UNDEFINED
 *          （[SWS_Mcu_00005]）。
 * \retval  MCU_RESET_UNDEFINED  Mcu_Init() 前に呼ばれた場合（[SWS_Mcu_00133]。
 *          あわせて MCU_E_UNINIT を報告する。[SWS_Mcu_00125]）。
 *
 * \AUTOSARReq     {SWS_Mcu_00158, SWS_Mcu_00005, SWS_Mcu_00133, SWS_Mcu_00125}
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Mcu_ResetType Mcu_GetResetReason(void);

/**
 * \brief   Mcu_Init() 時にキャッシュしたリセット原因を生値（ビット詰め）で返す。
 *
 * \details ビット割当は Mcu.h 冒頭のコメント参照。
 *
 * \retval  0x00-0x0F  ビット詰めされたリセット原因（[SWS_Mcu_00006]）。
 * \retval  0xFF       Mcu_Init() 前に呼ばれた場合（実装依存の非ゼロ値。
 *          [SWS_Mcu_00135]。あわせて MCU_E_UNINIT を報告する。
 *          [SWS_Mcu_00125]）。
 *
 * \AUTOSARReq     {SWS_Mcu_00159, SWS_Mcu_00006, SWS_Mcu_00135, SWS_Mcu_00125}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Mcu_RawResetType Mcu_GetResetRawValue(void);

/**
 * \brief   MCU をハードウェア機能でリセットする（[SWS_Mcu_00160]）。
 *
 * \details 呼び出し元へ戻らない（Cortex-M4 の NVIC_SystemReset() 発行後、
 *          MCU 全体が再起動するため。実処理は Mcu_Hw_PerformReset() 参照）。
 *          現状唯一の呼び出し元は UDS 0x11 ECUReset
 *          （Dcm_HandleEcuReset()。実車のリセットボタン押下と同等の効果を
 *          UDS 経由で起こしたいという要望から 2026-09 追加。Dcm 側が正応答
 *          送信後に短い遅延を挟んでから呼ぶことで、応答フレームの物理送信を
 *          先に完了させる）。
 *
 * \retval  なし（戻らない）。
 * \retval  （エラー時のみ戻る）未初期化の場合は MCU_E_UNINIT を報告して
 *          即座にリターンする（[SWS_Mcu_00125]、[SWS_Mcu_00145]）。
 *
 * \AUTOSARReq     {SWS_Mcu_00160, SWS_Mcu_00143, SWS_Mcu_00144, SWS_Mcu_00145}
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Mcu_PerformReset(void);

/**
 * \brief   Mcu モジュールのバージョン情報を取得する。
 *
 * \details [SWS_Mcu_00125] の対象外（Mcu_Init 前でも MCU_E_UNINIT を
 *          報告しない例外 API。他モジュールと共通の慣例）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_Mcu_00162}
 * \ServiceID      {0x09}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Mcu_GetVersionInfo(Std_VersionInfoType* versioninfo);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef MCU_UNIT_TEST
/**
 * \brief   [テスト専用] Mcu_Initialized/Mcu_CachedRawReset を未初期化状態へ戻す。
 *
 * \details 実 AUTOSAR SWS_Mcu には Mcu_DeInit() に相当する API が無く（本実装も
 *          意図的に追加していない、ファイル冒頭コメント参照）、他のテスト
 *          ファイル（例: Bsw_DcmStack_SID11_EcuReset_test.cpp）がそれぞれの
 *          SetUp() で Mcu_Init() を呼ぶため、native_chain_tests のリンク順・
 *          実行順に関わらず「未初期化状態からの呼び出し」を確実に再現するには
 *          本関数が必要（`Mcu_Init()` は一度呼ぶと二度と未初期化へ戻せない）。
 *          native 環境のホストテストからのみ使用する。`MCU_UNIT_TEST` は
 *          CMakeLists.txt の native_chain ターゲットでのみ定義され、実機ビルド
 *          （`uno_r4`）では定義されないため、実機の `Mcu.h`/`Mcu.c` には一切
 *          含まれない（AUTOSAR 標準外の関数。`Can_Test_SetControllerState()`
 *          等と同じ設計方針）。
 */
void Mcu_Test_ResetInitState(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MCU_H */

/**
 * \file    Mcu.c
 * \brief   MCU Driver 実装 (AUTOSAR SWS_Mcu 準拠)
 * \details 実際のリセット原因レジスタアクセスは Mcu_Hw 層に委譲し、本ファイルは
 *          MCU 固有のヘッダを直接知らない（Wdg.c が Wdg_Hw に委譲するのと同じ
 *          境界の引き方）。Mcu_Hw_ReadAndClearResetReason() は 1 起動につき
 *          1 回しか呼べない（読み取りと同時にレジスタをクリアするため）ため、
 *          Mcu_Init() で読み取った結果をキャッシュし、以降の
 *          Mcu_GetResetReason()/Mcu_GetResetRawValue() はキャッシュを返す
 *          だけにする（呼び出しタイミングの制約は Mcu.h 冒頭のコメント参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Mcu.h"
#include "Mcu_Hw.h"
#include "Det.h"

#define TAG "Mcu"

/* Mcu_Init() だけは DET_LOGT を含め DET_LOGx を一切呼ばない。
 * 理由は上記ファイル冒頭のコメント参照（Serial.begin() 前に呼ばれるため）。
 * 他の関数は Serial.begin() 後にのみ呼ばれるため TRACE を追加している。 */

static uint8 Mcu_Initialized = 0U;

/** Mcu_Init() が Mcu_Hw_ReadAndClearResetReason() から読み取ったリセット原因。
 *  ビット割当（MCU_RAW_RESET_*_BIT）は Mcu.h 参照。呼び出し元がデコードにも
 *  使う値のため、ビット定義自体は非公開にせず Mcu.h 側に置いている。 */
static Mcu_RawResetType Mcu_CachedRawReset = 0U;

/** Mcu_Init() 前に Mcu_GetResetRawValue() が呼ばれた場合の実装依存の
 *  非ゼロ値（[SWS_Mcu_00135]）。上記ビット割当では表現しえない値
 *  （bit4-7 も含めて全ビット 1）を選び、有効な生値と衝突しないようにする。 */
#define MCU_RAW_RESET_UNINIT_VALUE  0xFFU

void Mcu_Init(const Mcu_ConfigType* ConfigPtr)
{
    /* 本関数は Serial.begin() より前に呼ばれる設計のため（Mcu.h 冒頭の
     * コメント参照）、DET_LOGx/Det_ReportError は一切使わない。どちらも
     * 内部で Serial.print()/println() を無条件に（ログレベルによる抑制も
     * 素通りして）呼ぶため（Det_Hw.cpp 参照）、begin() 前の Serial への
     * 呼び出しが Renesas RA の USB-CDC 実装でどう振る舞うか未検証であり、
     * 最悪の場合ここでハングすると、この直後に行うはずの
     * Mcu_Hw_DisableWatchdogAtBoot() に到達できず、本関数が本来防ぐべき
     * WDT 無限リセットループ対策そのものを崩してしまう
     * （2026-08 のレビューで指摘、実害の有無を確認するまでもなくログを
     * 撤去した。呼び出し元の main.cpp が Serial 初期化後に
     * Mcu_GetResetRawValue() 経由で同じ内容を診断ログ出力するため、
     * ここでのログは元々冗長でもあった）。 */

    /* レジスタの読み取り+クリアは ConfigPtr の妥当性とは独立した HW 側の
     * 副作用であり、これ自体がブートループ対策の本体（クリアしないと
     * 次回リセット時に古いフラグが残り原因判定を誤る。Mcu_Hw.h 参照）。
     * ConfigPtr が NULL でも省略せず必ず実行する（2026-08 のレビューで、
     * 以前は NULL チェックを先に行い、NULL の場合にこのクリアごと
     * スキップしてしまう順序になっていた問題を指摘・修正）。 */
    const Mcu_Hw_ResetReasonType hwReason = Mcu_Hw_ReadAndClearResetReason();

    Mcu_CachedRawReset = 0U;
    if (hwReason.Watchdog) Mcu_CachedRawReset |= MCU_RAW_RESET_WATCHDOG_BIT;
    if (hwReason.BrownOut) Mcu_CachedRawReset |= MCU_RAW_RESET_BROWNOUT_BIT;
    if (hwReason.External) Mcu_CachedRawReset |= MCU_RAW_RESET_EXTERNAL_BIT;
    if (hwReason.PowerOn)  Mcu_CachedRawReset |= MCU_RAW_RESET_POWERON_BIT;

    /* ConfigPtr が NULL の場合は上記のキャッシュ更新までは行うが、モジュール
     * を「初期化済み」とはみなさない（現状唯一の呼び出し元 main.cpp は常に
     * &Mcu_Config を渡すため到達しないが、防御的に残す）。GetResetReason()/
     * GetResetRawValue() は Mcu_Initialized を見て MCU_E_UNINIT を報告する
     * ため、キャッシュ済みの値が NULL 経路で外部から観測されることはない。 */
    if (ConfigPtr == NULL)
    {
        return;
    }

    Mcu_Initialized = 1U;
}

Mcu_ResetType Mcu_GetResetReason(void)
{
    DET_LOGT(TAG, "called");
    if (!Mcu_Initialized)
    {
        Det_ReportError(MCU_MODULE_ID, 0U, MCU_API_ID_GET_RESET_REASON, MCU_E_UNINIT);
        return MCU_RESET_UNDEFINED;
    }

    if (Mcu_CachedRawReset & MCU_RAW_RESET_WATCHDOG_BIT)
        return MCU_WATCHDOG_RESET;
    if (Mcu_CachedRawReset & MCU_RAW_RESET_POWERON_BIT)
        return MCU_POWER_ON_RESET;

    /* BrownOut/External が単独で立っている場合を含む。Mcu_ResetType の
     * 値域にはこれらに対応する値がないため（Mcu.h 冒頭のコメント参照）。 */
    return MCU_RESET_UNDEFINED;
}

Mcu_RawResetType Mcu_GetResetRawValue(void)
{
    DET_LOGT(TAG, "called");
    if (!Mcu_Initialized)
    {
        Det_ReportError(MCU_MODULE_ID, 0U, MCU_API_ID_GET_RESET_RAW_VALUE, MCU_E_UNINIT);
        return MCU_RAW_RESET_UNINIT_VALUE;
    }

    return Mcu_CachedRawReset;
}

void Mcu_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(MCU_MODULE_ID, 0U, MCU_API_ID_GET_VERSION_INFO, MCU_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = MCU_VENDOR_ID;
    versioninfo->moduleID         = MCU_MODULE_ID;
    versioninfo->sw_major_version = MCU_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = MCU_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = MCU_SW_PATCH_VERSION;
}

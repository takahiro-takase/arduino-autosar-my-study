/**
 * \file    Wdg.h
 * \brief   Watchdog Driver 公開インタフェース (AUTOSAR SWS_Wdg 準拠)
 * \details Renesas RA (Arduino UNO R4) の実 HW ウォッチドッグ（IWDT、RA の
 *          WDT ライブラリ経由）を、WdgIf 経由で WdgM から利用可能にする
 *          下位ドライバ。
 *
 *          本実装の設計方針:
 *            - Wdg_Init() は実 AUTOSAR の [SWS_Wdg_00001]（デフォルトモード
 *              とタイムアウトを即座に有効化する）とは異なり、コンフィグの
 *              記録と内部状態の WDG_IDLE 遷移のみ行い、HW への実書き込みは
 *              行わない（Wdg_Hw には触れない）。実際の HW 有効化は
 *              Wdg_SetMode(WDGIF_FAST_MODE) が呼ばれるまで遅延される。
 *              これは WdgM が「他の全 BSW モジュール初期化完了後、最後に
 *              有効化する」という既存の設計（初期化処理自体が HW
 *              ウォッチドッグのタイムアウトに巻き込まれないようにするため）
 *              を壊さないための意図的な仕様逸脱。
 *            - Wdg_SetMode(WDGIF_OFF_MODE) は E_NOT_OK を返す。Renesas RA4M1
 *              の IWDT は一度有効化すると FSP からの無効化手段がないため
 *              （Wdg_Hw.cpp 参照）、無効化要求は物理的に受理できない。
 *              実 AUTOSAR の拡張プロダクションエラー WDG_E_DISABLE_REJECTED
 *              に相当する状況だが、本プロジェクトはプロダクションエラーの
 *              仕組み自体を持たないため DET_LOGW のみで通知する。
 *              呼び出し元 (WdgIf 経由の WdgM) は戻り値を見て HW が実際には
 *              無効化されていないことを前提に振る舞う必要はない
 *              （WdgM 側は WdgM_SupervisionSuppressed という別のソフトウェア
 *              フラグで「監視結果を無視する」ことを実現しており、HW が
 *              物理的に止まったかどうかには依存しない設計。詳細は WdgM.c の
 *              「HW ウォッチドッグ連携」コメント参照）。
 *            - WDGIF_SLOW_MODE は本プロジェクトが構成しない未使用モード
 *              （WdgIf_Types.h 参照）。要求された場合は開発エラー
 *              WDG_E_PARAM_MODE を報告し E_NOT_OK を返す。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDG_H
#define WDG_H

#include "Std_Types.h"
#include "WdgIf_Types.h"
#include "Wdg_Cfg.h"
#include "Wdg_PBCfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Wdg モジュールを初期化する。
 *
 * \details コンフィグを記録し内部状態を WDG_IDLE 相当へ遷移させる。
 *          HW ウォッチドッグ自体はまだ有効化しない（Wdg.h 冒頭のコメント
 *          参照）。EcuM_Init() 内で WdgM_Init() より前に呼ぶこと。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Wdg_Init(const Wdg_ConfigType* ConfigPtr);

/**
 * \brief   ウォッチドッグを指定モードへ切り替える。
 *
 * \details WdgIf_SetMode() 経由でのみ呼ばれることを想定する
 *          （WdgM は WdgIf 経由でのみ Wdg を呼び、直接は呼ばない）。
 *
 * \param[in]  Mode  WDGIF_OFF_MODE / WDGIF_FAST_MODE のいずれか
 *                   （WDGIF_SLOW_MODE は本プロジェクト未対応）。
 *
 * \retval  E_OK      WDGIF_FAST_MODE への切替に成功した。
 * \retval  E_NOT_OK  未初期化、Mode が範囲外、または WDGIF_OFF_MODE
 *                    （HW 制約により常に拒否される。Wdg.h 冒頭のコメント参照）。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Wdg_SetMode(WdgIf_ModeType Mode);

/**
 * \brief   ウォッチドッグのトリガカウンタをリフレッシュする。
 *
 * \details 実 AUTOSAR は timeout 引数でトリガカウンタの残り時間を動的に
 *          再設定できるが、本プロジェクトの HW（Renesas RA4M1 IWDT）は
 *          API 経由でのタイムアウト窓の動的変更に対応しないため、timeout
 *          の値によらず Wdg_Hw_Refresh() で単純にリフレッシュするのみ
 *          （学習用簡略化）。timeout が WdgM_Cfg.h の
 *          WDGM_HW_WATCHDOG_TIMEOUT_MS（Wdg_Config.DefaultTimeoutMs）を
 *          超える場合は WDG_E_PARAM_TIMEOUT を報告する（[SWS_Wdg_00146]）。
 *
 * \param[in]  timeout  トリガカウンタに設定するタイムアウト値 [ms]。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Wdg_SetTriggerCondition(uint16 timeout);

/**
 * \brief   Wdg モジュールのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Wdg_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* WDG_H */

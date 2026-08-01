/**
 * \file    WdgIf.h
 * \brief   Watchdog Interface 公開インタフェース (AUTOSAR SWS_WdgIf 準拠)
 * \details WdgM（上位）と Wdg（下位ドライバ）の間に位置するルーティング層。
 *          実 AUTOSAR は Device 引数で複数の Wdg インスタンスへ振り分けるが、
 *          本プロジェクトは物理ウォッチドッグが Renesas RA4M1 の IWDT 1 個
 *          のみのため、Device 引数の妥当性チェック後は唯一の下位ドライバへ
 *          実質パススルーする（CryIf → Crypto、MemIf → Fee と同じ簡略化）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGIF_H
#define WDGIF_H

#include "Std_Types.h"
#include "WdgIf_Types.h"
#include "WdgIf_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   対応する下位ドライバへモード切替をディスパッチする。
 *
 * \details [SWS_WdgIf_00042]。実体は Wdg_SetMode() への委譲
 *          （Service ID は下位ドライバの Wdg_SetMode と揃えて 0x01。
 *          [SWS_WdgIf_00042] 冒頭注記のとおり、WdgIf の Service ID は
 *          対応する Wdg 側 Service ID と揃える設計のため 0 始まりではない）。
 *
 * \param[in]  Device  WDGIF_DEVICE_0 のみ有効。
 * \param[in]  WdgMode 要求するモード。
 *
 * \retval  E_OK      モード切替に成功した。
 * \retval  E_NOT_OK  Device が不正、または下位ドライバがモード切替を拒否した
 *                    （本プロジェクトの HW は一度有効化すると WDGIF_OFF_MODE
 *                    へ戻せないため、無効化要求は常にこの戻り値になる）。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgIf_SetMode(WdgIf_DeviceType Device, WdgIf_ModeType WdgMode);

/**
 * \brief   対応する下位ドライバへリフレッシュ（trigger）要求をディスパッチする。
 *
 * \details [SWS_WdgIf_00044]。実体は Wdg_SetTriggerCondition() への委譲。
 *
 * \param[in]  Device   WDGIF_DEVICE_0 のみ有効。
 * \param[in]  Timeout  トリガカウンタに設定するタイムアウト値 [ms]。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgIf_SetTriggerCondition(WdgIf_DeviceType Device, uint16 Timeout);

/**
 * \brief   WdgIf モジュールのバージョン情報を取得する。
 *
 * \details [SWS_WdgIf_00046]。
 *
 * \param[out]  VersionInfoPtr  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgIf_GetVersionInfo(Std_VersionInfoType* VersionInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* WDGIF_H */

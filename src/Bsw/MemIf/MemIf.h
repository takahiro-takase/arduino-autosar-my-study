/**
 * \file    MemIf.h
 * \brief   Memory Abstraction Interface 公開インタフェース (AUTOSAR SWS_MemIf 準拠)
 * \details NvM（上位）と Fee（下位ドライバ）の間に位置するルーティング層。
 *          本プロジェクトが対応する MCU は Renesas RA (Arduino UNO R4) のみ
 *          （AVR/UNO 無印は初代のプログラムサイズ制限により移行済み。対になる
 *          Ea モジュールは削除済み）のため、下位ドライバは常に Fee
 *          （フラッシュエミュレーション EEPROM）1 個のみ存在する。実 AUTOSAR は
 *          Device 引数で複数の Fee/Ea インスタンスへジョブを振り分けるが、
 *          本プロジェクトは「Device 引数の妥当性チェック後、唯一の下位ドライバ
 *          Fee へ実質パススルー」という構成になる（CryIf → Crypto の関係と
 *          同様、NvM は MemIf 経由でのみ Fee を呼び、直接は呼ばない）。
 *
 *          実 AUTOSAR の MemIf との違い（MemIf_Init/MemIf_MainFunction）:
 *          [SWS_MemIf_00018]/[SWS_MemIf_00019] は「メモリ抽象化モジュールが
 *          1 個しか構成されていない場合、MemIf は下位モジュール API への
 *          マッピングのみを行うマクロ集合として実装してよい」と規定しており、
 *          実際に MemIf_Init/MemIf_MainFunction という API は SWS_MemIf に
 *          存在しない（EcuM が Fee_Init() を、Os が Fee_MainFunction() を
 *          直接呼ぶ設計）。本実装があえてこの 2 関数を追加しているのは、
 *          NvM/EcuM/Os_PBCfg.c を Fee という具体名から切り離し、MemIf という
 *          抽象名だけを意識させるためであり、AUTOSAR 非標準の拡張である
 *          （MemIf_Cfg.h 参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MEMIF_H
#define MEMIF_H

#include "Std_Types.h"
#include "MemIf_Types.h"
#include "MemIf_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   MemIf モジュールおよび唯一の下位ドライバ (Fee) を初期化する。
 *
 * \details 実 AUTOSAR の MemIf には Init 関数が存在しない（EcuM が Fee_Init()を
 *          直接呼ぶ設計。[SWS_MemIf_00019] 参照）。本プロジェクト独自の拡張
 *          （MemIf.c 冒頭のコメント参照）。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void MemIf_Init(void);

/**
 * \brief   MemIf モジュールのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void MemIf_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   対応する下位ドライバへ読み込みをディスパッチする。詳細は
 *          Fee_Read() 参照（同期 API）。
 *
 * \param[in]   Device         MEMIF_DEVICE_0 のみ有効。
 * \param[in]   Address        読み込み開始アドレス。
 * \param[out]  DataBufferPtr  読み込み先バッファ。NULL 禁止。
 * \param[in]   Length         読み込むバイト数。0 禁止。
 *
 * \retval  E_OK      正常完了。
 * \retval  E_NOT_OK  Device が不正、または下位ドライバが失敗。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType MemIf_Read(MemIf_DeviceType Device, uint16 Address, uint8* DataBufferPtr, uint16 Length);

/**
 * \brief   対応する下位ドライバへ非同期書き込みジョブをディスパッチする。
 *          詳細は Fee_Write() 参照。
 *
 * \param[in]  Device         MEMIF_DEVICE_0 のみ有効。
 * \param[in]  Address        書き込み開始アドレス。
 * \param[in]  DataBufferPtr  書き込み元データ。NULL 禁止。
 * \param[in]  Length         書き込むバイト数。0 禁止。
 *
 * \retval  E_OK      ジョブを受け付けた。
 * \retval  E_NOT_OK  Device が不正、または下位ドライバが失敗（既にビジー等）。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType MemIf_Write(MemIf_DeviceType Device, uint16 Address, const uint8* DataBufferPtr, uint16 Length);

/**
 * \brief   対応する下位ドライバへ同期（即時）書き込みをディスパッチする。
 *          詳細は Fee_WriteImmediate() 参照（AUTOSAR 非標準 API。
 *          MemIf_Cfg.h 参照）。
 *
 * \param[in]  Device         MEMIF_DEVICE_0 のみ有効。
 * \param[in]  Address        書き込み開始アドレス。
 * \param[in]  DataBufferPtr  書き込み元データ。NULL 禁止。
 * \param[in]  Length         書き込むバイト数。0 禁止。
 *
 * \retval  E_OK      正常完了。
 * \retval  E_NOT_OK  Device が不正、または下位ドライバが失敗。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType MemIf_WriteImmediate(MemIf_DeviceType Device, uint16 Address, const uint8* DataBufferPtr, uint16 Length);

/**
 * \brief   対応する下位ドライバの処理中ジョブを中断する。詳細は
 *          Fee_Cancel() 参照。
 *
 * \param[in]  Device  MEMIF_DEVICE_0 のみ有効。
 *
 * \retval  E_OK      正常に中断した。
 * \retval  E_NOT_OK  Device が不正。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType MemIf_Cancel(MemIf_DeviceType Device);

/**
 * \brief   対応する下位ドライバのビジー状態を取得する。
 *
 * \param[in]  Device  MEMIF_DEVICE_0 のみ有効。
 *
 * \return  MEMIF_UNINIT / MEMIF_IDLE / MEMIF_BUSY。Device が不正な場合も
 *          MEMIF_UNINIT を返す。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
MemIf_StatusType MemIf_GetStatus(MemIf_DeviceType Device);

/**
 * \brief   対応する下位ドライバの直近のジョブ結果を取得する。
 *
 * \param[in]  Device  MEMIF_DEVICE_0 のみ有効。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
MemIf_JobResultType MemIf_GetJobResult(MemIf_DeviceType Device);

/**
 * \brief   MemIf 周期処理。唯一の下位ドライバの MainFunction を呼び出す。
 *
 * \details Os スケジューラから NvM_MainFunction() とは独立に周期的に
 *          呼び出すこと（NvM_MainFunction() はブロック/CRC/冗長化の
 *          管理のみ行い、実際の物理バイト書き込みの進行はこちらが担う。
 *          詳細は NvM.c 冒頭のコメント参照）。実 AUTOSAR の MemIf には
 *          MainFunction が存在しない（Os が Fee_MainFunction() を直接
 *          スケジューリングする設計）。
 *          本プロジェクト独自の拡張（MemIf.c 冒頭のコメント参照）。
 *
 * \ServiceID      {0x0A}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void MemIf_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMIF_H */

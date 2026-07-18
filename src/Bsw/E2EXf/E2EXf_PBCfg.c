/**
 * \file    E2EXf_PBCfg.c
 * \brief   E2E Transformer ポストビルド設定データ
 *
 * \details E2E Profile 01 の設定・ステートを I-PDU 単位で定義し、
 *          E2EXf_RxConfigType/E2EXf_TxConfigType としてまとめる。
 *          以前は Com_PBCfg.c が Com_IPduConfigType の E2EConfig/
 *          E2ECheckState/E2EProtectState/E2EDemEventId フィールドとして
 *          直接保持していたが、E2E Transformer 方式への移行に伴い
 *          Com から独立したこちらへ移設した（Com は E2E を関知しない）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1/4.2.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "E2EXf_PBCfg.h"
#include "Dem_Cfg.h"

/* -----------------------------------------------------------------------
 * EngineInfo (RX IPduId=0, CAN 0x100)
 * DaVinci: /ActiveEcuC/E2EXf/EngineInfo_Rx_E2EXf
 *
 * EngineSpeed（回転数）は実車ではメータ表示だけでなく変速制御・トラクション
 * コントロール・オーバーレブ保護等、複数の機能が参照しうる値のため、
 * 一般的なエンジン ECU の周期送信フレームを模して E2E 保護を付与する。
 * ----------------------------------------------------------------------- */
static const E2E_P01ConfigType E2EXf_EngineInfoCfg = {
    0x0100U,  /* DataID          : PDU 識別子 (CAN ID と一致させるのが一般的) */
    6U,       /* DataLength      : PDU 全体バイト数 (CRC 1B + Counter 1B + シグナル 4B) */
    1U,       /* MaxDeltaCounter : 許容カウンタ飛び幅 (1=連続受信を前提) */
    1U,       /* CounterOffset   : byte[1] 下位 4bit がカウンタ (AUTOSAR 標準バリアント 1A) */
    0U,       /* CRCOffset       : byte[0] が CRC8 (AUTOSAR 標準バリアント 1A、SWS_E2E_00227) */
    2U        /* SyncCounterInit : WRONGSEQUENCE 検知後、OK へ戻るまでに必要な連続正常受信回数 */
};
static E2E_P01CheckStateType E2EXf_EngineInfoState;

const E2EXf_RxConfigType E2EXf_EngineInfoRxCfg = {
    .E2EConfig  = &E2EXf_EngineInfoCfg,
    .CheckState = &E2EXf_EngineInfoState,
    .DemEventId = DEM_EVENT_E2E_ENGINEINFO
};

/* -----------------------------------------------------------------------
 * AbsInfo (RX IPduId=1, CAN 0x110)
 * DaVinci: /ActiveEcuC/E2EXf/AbsInfo_Rx_E2EXf
 * ----------------------------------------------------------------------- */
static const E2E_P01ConfigType E2EXf_AbsInfoCfg = {
    0x0110U,  /* DataID          : PDU 識別子 (CAN ID と一致させるのが一般的) */
    5U,       /* DataLength      : PDU 全体バイト数 (CRC 1B + Counter 1B + シグナル 3B) */
    1U,       /* MaxDeltaCounter : 許容カウンタ飛び幅 (1=連続受信を前提) */
    1U,       /* CounterOffset   : byte[1] 下位 4bit がカウンタ (AUTOSAR 標準バリアント 1A) */
    0U,       /* CRCOffset       : byte[0] が CRC8 (AUTOSAR 標準バリアント 1A、SWS_E2E_00227) */
    2U        /* SyncCounterInit : WRONGSEQUENCE 検知後、OK へ戻るまでに必要な連続正常受信回数 */
};
static E2E_P01CheckStateType E2EXf_AbsInfoState;

const E2EXf_RxConfigType E2EXf_AbsInfoRxCfg = {
    .E2EConfig  = &E2EXf_AbsInfoCfg,
    .CheckState = &E2EXf_AbsInfoState,
    .DemEventId = DEM_EVENT_E2E_ABSINFO
};

/* -----------------------------------------------------------------------
 * MeterStatus (TX IPduId=0, CAN 0x200)
 * DaVinci: /ActiveEcuC/E2EXf/MeterStatus_Tx_E2EXf
 * エンジン運転状態 (EngineState) は他 ECU の判断に使われうるため、
 * 一般的なメータ ECU のイグニッション/運転状態配信を模して E2E 保護を付与する。
 * ----------------------------------------------------------------------- */
static const E2E_P01ConfigType E2EXf_MeterStatusCfg = {
    0x0200U,  /* DataID          : PDU 識別子 (CAN ID と一致させるのが一般的) */
    3U,       /* DataLength      : PDU 全体バイト数 (CRC 1B + Counter 1B + シグナル 1B) */
    0U,       /* MaxDeltaCounter : Protect 側では未使用 */
    1U,       /* CounterOffset   : byte[1] 下位 4bit がカウンタ (AUTOSAR 標準バリアント 1A) */
    0U,       /* CRCOffset       : byte[0] が CRC8 (AUTOSAR 標準バリアント 1A、SWS_E2E_00227) */
    0U        /* SyncCounterInit : Protect 側では未使用 */
};
static E2E_P01ProtectStateType E2EXf_MeterStatusState;

const E2EXf_TxConfigType E2EXf_MeterStatusTxCfg = {
    .E2EConfig    = &E2EXf_MeterStatusCfg,
    .ProtectState = &E2EXf_MeterStatusState
};

void E2EXf_PBCfg_Init(void)
{
    E2E_P01CheckInit(&E2EXf_EngineInfoState);
    E2E_P01CheckInit(&E2EXf_AbsInfoState);
    E2E_P01ProtectInit(&E2EXf_MeterStatusState);

    /* 各 State の初期化が完了した最後に、E2EXf モジュール自身の初期化状態
     * (SWS_E2EXf_00130) を TRUE にする。E2EXf_InverseTransform()/
     * E2EXf_Transform() はこれより前に呼ばれても安全側で早期 return する。 */
    E2EXf_Init();
}

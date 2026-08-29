/**
 * \file    E2EXf_PBCfg.c
 * \brief   E2E Transformer ポストビルド設定データ
 *
 * \details E2E Profile 01/05 の設定・ステートを I-PDU 単位で定義し、
 *          E2EXf_RxConfigType(P01)/E2EXf_RxConfigTypeP05/E2EXf_TxConfigTypeP05
 *          としてまとめる。EngineInfo/AbsInfo(RX)/E2EHealthStatus(TX)は
 *          いずれも Profile05 を使用しており、E2EXf_RxConfigType(P01)自体は
 *          呼び出し元ゼロの参考実装として残している（E2EXf_TxConfigType と
 *          同じ理由）。
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
 * 以前は E2E Profile01(CRC8+4bitカウンタ、DLC=6) だったが、CRC 検出能力を
 * 高めるため E2E Profile05(CRC16+8bitカウンタ、DLC=7) へ切り替えた
 * （EngineHealthStatus の TX 側と同じ理由）。
 * ----------------------------------------------------------------------- */
static const E2E_P05ConfigType E2EXf_EngineInfoCfgP05 = {
    0x0100U,  /* DataID          : PDU 識別子 (CAN ID と一致させるのが一般的) */
    7U,       /* DataLength      : PDU 全体バイト数 (CRC16 2B + Counter 1B + シグナル 4B) */
    1U,       /* MaxDeltaCounter : 許容カウンタ飛び幅 (1=連続受信を前提) */
    0U        /* Offset          : E2E ヘッダ(CRC16+Counter)は PDU 先頭 */
};
static E2E_P05CheckStateType E2EXf_EngineInfoStateP05;
/* Profile05にはINITIAL相当が無いため、E2EXf層で初回受信の特別扱いを行う
 * ためのフラグ(E2EXf_RxConfigTypeP05.WaitForFirstData 宣言コメント参照)。 */
static uint8 E2EXf_EngineInfoWaitForFirstDataP05;

const E2EXf_RxConfigTypeP05 E2EXf_EngineInfoRxCfg = {
    .E2EConfig        = &E2EXf_EngineInfoCfgP05,
    .CheckState       = &E2EXf_EngineInfoStateP05,
    .DemEventId       = DEM_EVENT_E2E_ENGINEINFO,
    .WaitForFirstData = &E2EXf_EngineInfoWaitForFirstDataP05
};

/* -----------------------------------------------------------------------
 * AbsInfo (RX IPduId=1, CAN 0x110)
 * DaVinci: /ActiveEcuC/E2EXf/AbsInfo_Rx_E2EXf
 * 以前は E2E Profile01(CRC8+4bitカウンタ、DLC=5) だったが、EngineInfo と
 * 同じ理由で E2E Profile05(CRC16+8bitカウンタ、DLC=6) へ切り替えた。
 * ----------------------------------------------------------------------- */
static const E2E_P05ConfigType E2EXf_AbsInfoCfgP05 = {
    0x0110U,  /* DataID          : PDU 識別子 (CAN ID と一致させるのが一般的) */
    6U,       /* DataLength      : PDU 全体バイト数 (CRC16 2B + Counter 1B + シグナル 3B) */
    1U,       /* MaxDeltaCounter : 許容カウンタ飛び幅 (1=連続受信を前提) */
    0U        /* Offset          : E2E ヘッダ(CRC16+Counter)は PDU 先頭 */
};
static E2E_P05CheckStateType E2EXf_AbsInfoStateP05;
/* EngineInfo と同じ理由(E2EXf_EngineInfoWaitForFirstDataP05 参照)。 */
static uint8 E2EXf_AbsInfoWaitForFirstDataP05;

const E2EXf_RxConfigTypeP05 E2EXf_AbsInfoRxCfg = {
    .E2EConfig        = &E2EXf_AbsInfoCfgP05,
    .CheckState       = &E2EXf_AbsInfoStateP05,
    .DemEventId       = DEM_EVENT_E2E_ABSINFO,
    .WaitForFirstData = &E2EXf_AbsInfoWaitForFirstDataP05
};

/* -----------------------------------------------------------------------
 * E2EHealthStatus (TX IPduId=2, CAN 0x220)
 * DaVinci: /ActiveEcuC/E2EXf/E2EHealthStatus_Tx_E2EXf
 * E2EMon（CDD 相当）が発行するネットワーク健全性テレメトリ自体も、
 * 監視ツールが誤ったカウンタ値を信用してしまわないよう E2E 保護を付与する。
 * 以前は E2E Profile01(+SecOC 二重保護、DLC=8) だったが、CRC 検出能力を
 * 高めるため E2E Profile05(CRC16、DLC=5) 単体に切り替えた。SecOC は撤去
 * した(PduR_PBCfg.c のパス3、SecOC_PBCfg.c 参照)。
 * ----------------------------------------------------------------------- */
static const E2E_P05ConfigType E2EXf_E2EHealthStatusCfgP05 = {
    0x0220U,  /* DataID          : PDU 識別子 (CAN ID と一致させるのが一般的) */
    5U,       /* DataLength      : PDU 全体バイト数 (CRC16 2B + Counter 1B + シグナル 2B) */
    0U,       /* MaxDeltaCounter : Protect 側では未使用 */
    0U        /* Offset          : E2E ヘッダ(CRC16+Counter)は PDU 先頭 */
};
static E2E_P05ProtectStateType E2EXf_E2EHealthStatusStateP05;

const E2EXf_TxConfigTypeP05 E2EXf_E2EHealthStatusTxCfgP05 = {
    .E2EConfig    = &E2EXf_E2EHealthStatusCfgP05,
    .ProtectState = &E2EXf_E2EHealthStatusStateP05
};

void E2EXf_PBCfg_Init(void)
{
    E2E_P05CheckInit(&E2EXf_EngineInfoStateP05);
    E2E_P05CheckInit(&E2EXf_AbsInfoStateP05);
    E2EXf_EngineInfoWaitForFirstDataP05 = 1U;
    E2EXf_AbsInfoWaitForFirstDataP05    = 1U;
    E2E_P05ProtectInit(&E2EXf_E2EHealthStatusStateP05);

    /* 各 State の初期化が完了した最後に、E2EXf モジュール自身の初期化状態
     * (SWS_E2EXf_00130) を TRUE にする。E2EXf_InverseTransform()/
     * E2EXf_Transform() はこれより前に呼ばれても安全側で早期 return する。 */
    E2EXf_Init(NULL);
}

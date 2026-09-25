/**
 * \file    E2EXf_PBCfg.c
 * \brief   E2E Transformer ポストビルド設定データ
 *
 * \details E2E Profile 05 の設定・ステートを I-PDU 単位で定義し、
 *          E2EXf_RxConfigTypeP05/E2EXf_TxConfigTypeP05 としてまとめる
 *          （EngineInfo/AbsInfo(RX)/E2EHealthStatus(TX)、いずれも Profile05）。
 *          これらの struct は `E2EXf.c` の各インスタンス専用関数
 *          （`E2EXf_Inv_EngineInfo()`等）が内部で直接参照する表現であり、
 *          公開 API の引数には登場しない（2026-09 是正、E2EXf.h 冒頭コメント
 *          参照）。E2E Profile 01（`E2E_P01.c`）は実 PDU を持たない参考実装
 *          のため、対応する Config struct・インスタンスはここに存在しない
 *          （`test/Bsw/E2E/Bsw_E2E_test.cpp` の `E2EP01Test` が
 *          `E2E_P01.c` 単体を直接検証する）。
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
#include "E2E.h"

/* -----------------------------------------------------------------------
 * E2E ステートマシン設定（[SWS_E2EXf_00028]、E2E_SMConfigType）
 * 全 RX インスタンス共通。仕様書はしきい値の具体的な数値を規定しない
 * （アプリケーション/インテグレータが選ぶ設定パラメータ、ECUC_E2E_*）ため、
 * 以下は本プロジェクト独自の判断:
 *   - WindowSize=3: Arduino UNO R4 の限られた RAM を踏まえ、判定に必要な
 *     最小限の履歴数（インスタンスあたり ProfileStatusWindow 3 byte +
 *     E2E_SMCheckStateType 本体のみ）に絞った。`E2EXF_SM_WINDOW_SIZE` と
 *     しても定義し、各インスタンスの `ProfileStatusWindow` 配列サイズを
 *     ここへ連動させる（値のハードコード重複を避ける）。
 *   - Init系(2/1): 起動直後、直近3回中2回以上OKで初期同期完了とみなす
 *     （1回はエラー相性が悪くても許容）。
 *   - Valid系(2/1): 直近3回中1回のCRC/カウンタ異常（例: ノイズによる単発
 *     ビット化け）までは通信健全とみなし DTC を確定させない。これが本設定
 *     追加の主目的（以前は E2E_SMCheck() 自体を呼んでおらず、単発異常が
 *     即座に FAILED に直結していた、2026-09 是正）。2回連続なら INVALID。
 *   - Invalid系(2/1): VALID復帰もVALID維持と同じ基準で対称に統一した。
 * ----------------------------------------------------------------------- */
#define E2EXF_SM_WINDOW_SIZE 3U

static const E2E_SMConfigType E2EXf_SMConfigDefault = {
    E2EXF_SM_WINDOW_SIZE, /* WindowSize */
    2U, /* MinOkStateInit       */
    1U, /* MaxErrorStateInit    */
    2U, /* MinOkStateValid      */
    1U, /* MaxErrorStateValid   */
    2U, /* MinOkStateInvalid    */
    1U  /* MaxErrorStateInvalid */
};

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
/* E2E ステートマシン状態（E2EXf_SMConfigDefault 参照）。 */
static uint8 E2EXf_EngineInfoSMWindow[E2EXF_SM_WINDOW_SIZE];
static E2E_SMCheckStateType E2EXf_EngineInfoSMState = { E2EXf_EngineInfoSMWindow, 0U, 0U, 0U, E2E_SM_DEINIT };

const E2EXf_RxConfigTypeP05 E2EXf_EngineInfoRxCfg = {
    .E2EConfig        = &E2EXf_EngineInfoCfgP05,
    .CheckState       = &E2EXf_EngineInfoStateP05,
    .DemEventId       = DEM_EVENT_E2E_ENGINEINFO,
    .WaitForFirstData = &E2EXf_EngineInfoWaitForFirstDataP05,
    .SMConfig         = &E2EXf_SMConfigDefault,
    .SMState          = &E2EXf_EngineInfoSMState
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
/* EngineInfo と同じ理由(E2EXf_EngineInfoSMState 参照)。 */
static uint8 E2EXf_AbsInfoSMWindow[E2EXF_SM_WINDOW_SIZE];
static E2E_SMCheckStateType E2EXf_AbsInfoSMState = { E2EXf_AbsInfoSMWindow, 0U, 0U, 0U, E2E_SM_DEINIT };

const E2EXf_RxConfigTypeP05 E2EXf_AbsInfoRxCfg = {
    .E2EConfig        = &E2EXf_AbsInfoCfgP05,
    .CheckState       = &E2EXf_AbsInfoStateP05,
    .DemEventId       = DEM_EVENT_E2E_ABSINFO,
    .WaitForFirstData = &E2EXf_AbsInfoWaitForFirstDataP05,
    .SMConfig         = &E2EXf_SMConfigDefault,
    .SMState          = &E2EXf_AbsInfoSMState
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
    /* [SWS_E2E_00353]: E2E_SMCheckInit() を明示的に呼ぶ（呼ばないまま
     * ゼロ初期化のみに頼ると E2E_SM_VALID(0x00) と誤認する、E2E.h の
     * E2E_SMCheck() 宣言側コメント参照）。 */
    (void)E2E_SMCheckInit(&E2EXf_EngineInfoSMState, &E2EXf_SMConfigDefault);
    (void)E2E_SMCheckInit(&E2EXf_AbsInfoSMState, &E2EXf_SMConfigDefault);
    E2E_P05ProtectInit(&E2EXf_E2EHealthStatusStateP05);

    /* 各 State の初期化が完了した最後に、E2EXf モジュール自身の初期化状態
     * (SWS_E2EXf_00130) を TRUE にする。E2EXf_Inv_EngineInfo()等の各
     * インスタンス関数はこれより前に呼ばれても安全側で早期 return する。 */
    E2EXf_Init(NULL);
}

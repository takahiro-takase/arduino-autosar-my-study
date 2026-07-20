/**
 * \file    Rte.c
 * \brief   ランタイム環境 (AUTOSAR SWS_RTE 準拠)
 * \details SW-Component (SW-C) と BSW の COM モジュール間のデータ交換を
 *          仲介する AUTOSAR RTE API 層を実装する。
 *          型付きの Read/Write ポートアクセサと、Arduino の millis() タイマで
 *          駆動するシンプルな Runnable スケジューラを提供する。
 *          AUTOSAR 4.3.1 SWS_RTE の API 命名規則
 *          (Rte_Read_<p>_<o> / Rte_Write_<p>_<o>) に準拠する。
 *          シングルインスタンス SW-C のため、オプション引数 Rte_Instance は
 *          省略している。ただし EngineInfo/AbsInfo の Read ポートは E2E
 *          Transformer チェーンを持つため、戻り値は Std_ReturnType ではなく
 *          Rte_IStatusType（Rte_Type.h）とし、E2E の検証結果（OK/ハード
 *          エラー/ソフトエラー）を SWC まで伝える（詳細は下記
 *          「E2E Transformer 方式の RX ミラー」を参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Rte.h"
#include "Com.h"
#include "IoHwAb.h"
#include "FiM.h"
#include "SchM.h"
#include "E2EXf.h"
#include "E2EXf_PBCfg.h"
#include "E2EMon.h"
#include "Det.h"

#define TAG "Rte"

/* シグナル ID は Com_Cfg.h の COM_SIGNAL_* を使用（重複定義を排除） */

/* App_EngineManager.c が定義する SW-C Runnable の前方宣言 */
extern void App_EngineManager_Run(void);

/* App_WarningIndicator.c が定義する SW-C Runnable の前方宣言 */
extern void App_WarningIndicator_Run(void);

/* EngineState の内部ミラー変数。
 * Rte_Write_EngineStatus_EngineState() が書き込み、
 * Dcm の Rte_Read_EngineStatus_EngineState() が読み出す。
 * COM TX バッファは Com_ReceiveSignal で読めないため、ここで保持する。 */
static EngineState_t Rte_EngineStateMirror = ENGINE_STATE_OFF;

/* -----------------------------------------------------------------------
 * E2E Transformer 方式の RX ミラー
 *
 * E2E Transformer 方式では、Com はペイロードの妥当性を一切検証しない
 * （Com_Types.h の RxIndicationCbk 説明参照）。かわりに、フレーム受信の
 * 都度呼ばれる Rte_COMCbk_*() が E2EXf_InverseTransform() で検証し、
 * 合格した場合のみここのミラーを更新する。Rte_Read_*() はタイムアウト
 * 判定 (Com_IsRxTimedOut()) だけを Com に問い合わせ、値自体はこのミラー
 * から返す。
 *
 * E2E チェックが失敗しても Com のタイムアウトタイマ自体はフレーム到達で
 * リセットされる（Com は妥当性を関知しないため）。よってこのミラーの
 * 「更新しない」という判断こそが、E2E 違反時に古い値を使い続けさせる
 * フェイルセーフの実体になる（Com_RxTimedOut は物理フレームが本当に
 * 途絶えた場合にのみ発火する別軸の保護）。
 *
 * Rte_EngineInfoStatus / Rte_AbsInfoStatus（Rte_IStatusType）:
 * 上記のミラー更新可否とは別に、直近の E2E_P01Check() 結果を
 * RTE_E_OK / RTE_E_SOFT_TRANSFORMER_ERROR（OKSOMELOST）/
 * RTE_E_HARD_TRANSFORMER_ERROR（それ以外の異常）へ分類して保持する。
 * Rte_Read_*() はこれを Com_IsRxTimedOut() と合成し、SWC が「タイムアウト
 * なのか」「E2E で弾かれたのか（データは信頼できないので前回値のまま）」
 * 「E2E 上は使用可だが一部フレーム消失があったのか」を区別できるようにする
 * （実 AUTOSAR の Rte_IStatusType が担う役割の簡略版）。SWC がこの区別を
 * 必要としない場合は、従来通り戻り値を無視してデータだけ読んでもよい
 * （*data は RTE_E_OK 以外でも常に「読める最善の値」を返す。詳細は各
 * Rte_Read_* のコメント参照）。起動直後の初期値は RTE_E_OK とし、
 * まだ一度もフレームを受信していない段階（Com_IsRxTimedOut() もまだ
 * false）での挙動を変更前と同じにしている。
 *
 * 合成の優先順位（重要）: Rte_Read_*() は必ず Com_IsRxTimedOut() を先に
 * 判定し、タイムアウトでなければ Rte_EngineInfoStatus/Rte_AbsInfoStatus を
 * そのまま返す。Rte_EngineInfoStatus 等は「最後にフレームを受信した瞬間」
 * にしか更新されないラッチであり、Com_IsRxTimedOut() は Com_MainFunction()
 * が周期的に評価する「今まさに生きているか」の独立した軸である。もし
 * ラッチを先に見てしまうと、E2E ハードエラーを起こしたフレームを最後に
 * 通信が本当に途絶えた場合（配線断は E2E 異常と通信途絶を同時に招きやすい）、
 * ラッチされた RTE_E_HARD_TRANSFORMER_ERROR が永久に優先され、
 * Com_IsRxTimedOut() が真に切り替わっても RTE_E_COM_STOPPED が一切
 * 浮上しなくなる（＝COMM_TIMEOUT の FAULT 遷移が永久にマスクされる）。
 * 実 AUTOSAR の優先順位規定（[SWS_Rte_08594]、複数要因が「同一呼び出しで
 * 同時に新規発生した」場合を想定: HARD_TRANSFORMER_ERROR > COM_STOPPED >
 * SOFT_TRANSFORMER_ERROR）をそのまま適用しない。本実装は E2E チェックを
 * フレーム受信時に非同期に行い結果をラッチする設計のため、「過去の一時点の
 * ラッチ」と「現在も継続する物理層の状態」を比較する場面では、常に
 * 後者（生きている情報）を優先する。
 * ----------------------------------------------------------------------- */
typedef struct
{
    EngineSpeed_t   speed;
    CoolantTemp_t   temp;
    EngineOnFlag_t  onFlag;
} Rte_EngineInfoMirrorType;
static Rte_EngineInfoMirrorType Rte_EngineInfoMirror;
static Rte_IStatusType Rte_EngineInfoStatus = RTE_E_OK;

typedef struct
{
    VehicleSpeed_t speed;
    BrakeActive_t  brake;
    AbsActive_t    abs;
} Rte_AbsInfoMirrorType;
static Rte_AbsInfoMirrorType Rte_AbsInfoMirror;
static Rte_IStatusType Rte_AbsInfoStatus = RTE_E_OK;

/**
 * \brief   E2E_P01StatusType（8状態）を Rte_IStatusType（3値+OK）へ写像する。
 *
 * \details OK/INITIAL/SYNC は「CRC正・データ信頼可」として RTE_E_OK。
 *          OKSOMELOST は「データ信頼可だが一部消失あり」として
 *          RTE_E_SOFT_TRANSFORMER_ERROR。それ以外（REPEATED/WRONGCRC/
 *          WRONGSEQUENCE/ERROR）は「データ不信」として
 *          RTE_E_HARD_TRANSFORMER_ERROR。E2EXf_InverseTransform() の
 *          PASSED/FAILED 二値判定（Dem 報告用）とは独立した、SWC 向けの
 *          より詳細な分類である点に注意。
 */
static Rte_IStatusType Rte_MapE2EStatus(E2E_P01StatusType status)
{
    switch (status)
    {
        case E2E_P01STATUS_OK:
        case E2E_P01STATUS_INITIAL:
        case E2E_P01STATUS_SYNC:
            return RTE_E_OK;
        case E2E_P01STATUS_OKSOMELOST:
            return RTE_E_SOFT_TRANSFORMER_ERROR;
        default:
            return RTE_E_HARD_TRANSFORMER_ERROR;
    }
}

/**
 * \brief   EngineInfo (RX IPduId=0) フレーム受信の都度呼ばれる E2E Transformer フック。
 *
 * \details Com_PBCfg.c の RxIndicationCbk として登録される。
 *          E2EXf_InverseTransform() が失敗した場合はミラーを更新せず、
 *          前回の有効値をそのまま使い続けさせる。E2E チェックの生の結果は
 *          `E2EMon_NotifyCheckResult()`（CDD 相当の独立モジュール、
 *          src/Bsw/E2EMon/）へも通知する。これは実 AUTOSAR で言う
 *          「ARXML で設定した OnDataReceived 通知フックが RTE から生成され、
 *          独自 CDD の関数を呼ぶ」という接続方式を模したもの（本プロジェクトは
 *          RTE ジェネレータが無いため Rte.c が手書きでこの呼び出しを担う）。
 *
 * \note    Com_PBCfg.c から extern 宣言経由で RxIndicationCbk として
 *          参照されるため non-static。Rte.h には公開しない（RTE の
 *          公式 API ではなく、Com→Rte 間の内部グルーのため）。
 */
void Rte_COMCbk_EngineInfo(void)
{
    uint8 buf[6];
    if (Com_ReceiveSignalGroupArray(0U, buf) != E_OK)
        return;

    E2E_P01StatusType checkStatus;
    const Std_ReturnType ret = E2EXf_InverseTransform(&E2EXf_EngineInfoRxCfg, buf, 6U, &checkStatus);
    Rte_EngineInfoStatus = Rte_MapE2EStatus(checkStatus);
    E2EMon_NotifyCheckResult(checkStatus);
    if (ret != E_OK)
        return;

    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    (void)Com_ReceiveSignal(COM_SIGNAL_ENGINE_SPEED,   &Rte_EngineInfoMirror.speed);
    (void)Com_ReceiveSignal(COM_SIGNAL_COOLANT_TEMP,   &Rte_EngineInfoMirror.temp);
    (void)Com_ReceiveSignal(COM_SIGNAL_ENGINE_ON_FLAG, &Rte_EngineInfoMirror.onFlag);
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();
}

/**
 * \brief   CoolantTemp が無効値（0xFF）で受信されたことを通知する。
 *
 * \details Com_PBCfg.c の CoolantTemp シグナル設定（DataInvalidAction=
 *          COM_DATA_INVALID_ACTION_NOTIFY）から InvalidNotificationCbk として
 *          登録される（実 AUTOSAR の ComInvalidNotification、ECUC_Com_00315
 *          相当）。Com_ReceiveSignal(COM_SIGNAL_COOLANT_TEMP, ...) が受信値と
 *          InvalidValue の一致を検知した「次回」の Com_MainFunction() から
 *          呼ばれる（SWS_Com_00680/00717。同期呼び出しにしていない理由は
 *          Com.c の Com_RxInvalidNotifyPending 宣言コメント参照 — この関数が
 *          行う Serial 出力は、Com_ReceiveSignal() の呼び出し元によっては
 *          割り込み禁止区間内で実行されると WDT リセットを引き起こしうる
 *          ため）。この関数自体は「異常が起きたことをログへ残す」以上のことは
 *          行わない（ログ出力のみの CDD 相当。E2EMon のように独自カウンタを
 *          持って TX シグナルへ反映するような発展はスコープ外）。
 *
 * \note    Com_PBCfg.c から extern 宣言経由で InvalidNotificationCbk として
 *          参照されるため non-static。Rte.h には公開しない（Rte_COMCbk_*
 *          と同じく Com→Rte 間の内部グルーのため）。
 */
void Rte_COMInvalidNotify_CoolantTemp(void)
{
    DET_LOGW(TAG, "CoolantTemp invalid value received (sensor fault pattern)");
}

/**
 * \brief   MeterStatus フレームの送信成功を通知する（EngineState の TxAck）。
 *
 * \details Com_PBCfg.c の EngineState シグナル設定（TxAckCbk）から登録される
 *          （実 AUTOSAR の Com_CbkTxAck、ComNotification = ECUC_Com_00498
 *          相当）。呼ばれるのは Com_TxConfirmation() が MeterStatus
 *          （TX IPduId=0）の送信成功を検出した直後（SWS_Com_00468）。
 *          Com_TxConfirmation() は Can_MainFunction_Write()（Os の 100ms
 *          タスク）から同期的に呼ばれ、この経路上に割り込み禁止区間は
 *          存在しないため（Com.c の Com_TxConfirmation() doc 参照）、
 *          ここで Serial 出力（DET_LOGI）を行っても Rx 無効値検知で発生した
 *          WDT リセット障害と同じ問題は起きない。
 *
 * \note    Com_PBCfg.c から extern 宣言経由で TxAckCbk として参照されるため
 *          non-static。Rte.h には公開しない（他の Rte_COM* グルーと同じ
 *          理由）。
 */
void Rte_COMTxAck_EngineState(void)
{
    DET_LOGI(TAG, "MeterStatus TX ack (EngineState)");
}

/**
 * \brief   SecureCommand (RX IPduId=2) 受信の都度呼ばれる。
 *
 * \details Com_PBCfg.c の ImmobilizerCmd シグナル設定（RxIndicationCbk）から
 *          登録される。呼ばれるのは SecOC（src/Bsw/SecOC/）が MAC・フレッシュ
 *          ネス検証に成功し、`Com_RxIndication(2, ...)` を直接呼んだ直後のみ
 *          （検証に失敗したデータは Com へ一切渡らないため、このコールバック
 *          自体が呼ばれない。SecOC_IfRxIndication() 参照）。すなわちこのログが
 *          出力されること自体が「認証済みコマンドである」ことを意味する。
 *          `Rte_COMTxAck_EngineState()` と同じ理由で、ここで Serial 出力
 *          （DET_LOGW）を直接行っても安全（この呼び出しチェーン
 *          Can_MainFunction_Read → ... → SecOC_IfRxIndication →
 *          Com_RxIndication → このコールバック、の間に割り込み禁止区間は
 *          存在しない）。
 *
 *          この関数自体はログ出力のみを行う。ドア施錠制御等の実ハードウェア
 *          反応は本実装のスコープ外（`Rte_COMInvalidNotify_CoolantTemp` 等と
 *          同じ最小デモパターン。Com/SecOC/PduR のアーキテクチャ学習が主目的
 *          のため、ASW 側の反応まで作り込むことはしない）。
 *
 * \note    Com_PBCfg.c から extern 宣言経由で RxIndicationCbk として
 *          参照されるため non-static。Rte.h には公開しない（他の Rte_COM*
 *          グルーと同じ理由）。
 */
void Rte_COMCbk_SecureCommand(void)
{
    uint8 cmd = 0U;
    if (Com_ReceiveSignal(COM_SIGNAL_IMMOBILIZER_CMD, &cmd) != E_OK)
        return;

    if (cmd == 0x01U)
        DET_LOGW(TAG, "ImmobilizerCmd: UNLOCK (authenticated via SecOC)");
    else
        DET_LOGW(TAG, "ImmobilizerCmd: LOCK (authenticated via SecOC)");
}

/**
 * \brief   AbsInfo (RX IPduId=1) フレーム受信の都度呼ばれる E2E Transformer フック。
 *
 * \details Com_PBCfg.c の RxIndicationCbk として登録される。
 *
 *          AbsInfo は RX Signal Group（IsSignalGroup=1、Com_PBCfg.c 参照）
 *          でもあるため、VehicleSpeed/BrakeActive/AbsActive を読む前に
 *          Com_ReceiveSignalGroup(1U) で I-PDU バッファを RX シャドウバッファへ
 *          確定コピーする（Com_ReceiveSignal() はこのグループのメンバーに
 *          対して、Com_RxBuffer ではなくこのシャドウバッファを読む）。
 *          この呼び出し自体はフレーム受信直後・Com_RxTimedOut リセット後に
 *          同期的に実行されるため、実際にタイムアウト中でこの一貫性保証が
 *          意味を持つ場面はない（既に E2E チェックを通過した新鮮なフレームの
 *          直後であるため）。TMS/MDT/ComTransferProperty と同じく、動機は
 *          実利より仕様忠実性（SWS_Com_00201/00051/00638 相当）。
 *
 * \note    Rte_COMCbk_EngineInfo() と同じ理由で non-static。
 */
void Rte_COMCbk_AbsInfo(void)
{
    uint8 buf[5];
    if (Com_ReceiveSignalGroupArray(1U, buf) != E_OK)
        return;

    E2E_P01StatusType checkStatus;
    const Std_ReturnType ret = E2EXf_InverseTransform(&E2EXf_AbsInfoRxCfg, buf, 5U, &checkStatus);
    Rte_AbsInfoStatus = Rte_MapE2EStatus(checkStatus);
    E2EMon_NotifyCheckResult(checkStatus);
    if (ret != E_OK)
        return;

    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    (void)Com_ReceiveSignalGroup(1U);
    (void)Com_ReceiveSignal(COM_SIGNAL_VEHICLE_SPEED, &Rte_AbsInfoMirror.speed);
    (void)Com_ReceiveSignal(COM_SIGNAL_BRAKE_ACTIVE,  &Rte_AbsInfoMirror.brake);
    (void)Com_ReceiveSignal(COM_SIGNAL_ABS_ACTIVE,    &Rte_AbsInfoMirror.abs);
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();
}

/**
 * \brief   E2EHealthStatus (TX IPduId=2) 送信直前に呼ばれる E2E Transformer フック。
 *
 * \details Com_PBCfg.c の TxTransformCbk として登録される。COM_TX_MODE_PERIODIC
 *          のため、Com_MainFunction() が自分の周期タイマで送信を決定した際に
 *          このフックが呼ばれる（DIRECT/MIXED I-PDU のイベント駆動送信と
 *          同じ「送信直前の最終変換」の仕組みをそのまま再利用している）。
 *          実 TX バッファへ Counter・CRC8 を書き込む。E2EMon（CDD 相当）は
 *          Com_SendSignal() で値をセットするだけで、この E2E 保護の存在自体を
 *          一切知らない（MeterStatus における App_EngineManager と同じ関係）。
 *
 * \note    Rte_COMCbk_EngineInfo() と同じ理由で non-static。
 */
void Rte_COMTransform_E2EHealthStatus(uint8* Data, uint8 Length)
{
    E2EXf_Transform(&E2EXf_E2EHealthStatusTxCfg, Data, Length);
}

/* -----------------------------------------------------------------------
 * ランプ IOControl（Dcm SID 0x2F 用）の内部状態
 *
 * Rte_LampOverrideActive[lamp] == 0: ASW (App_WarningIndicator) が制御中。
 *   Rte_Call_Led*_SetLevel() の引数がそのまま IoHwAb へ反映される（従来どおり）。
 * Rte_LampOverrideActive[lamp] == 1: Dcm が診断制御中。
 *   Rte_Call_Led*_SetLevel() の引数は無視され、Rte_LampOverrideValue[lamp] が
 *   代わりに出力され続ける。ASW 自身はこの分岐の存在を知らない。
 * ----------------------------------------------------------------------- */
static uint8 Rte_LampOverrideActive[RTE_LAMP_COUNT];
static uint8 Rte_LampOverrideValue[RTE_LAMP_COUNT];
/* 実際に IoHwAb へ出力された最新レベル（ASW 経由・Dcm 強制のいずれも反映後の値）。
 * freezeCurrentState が「現在の物理出力値」を読み出すため、および Dcm 応答の
 * controlStatusRecord 構築のために保持する。 */
static uint8 Rte_LampLastLevel[RTE_LAMP_COUNT];

/**
 * \brief   ランプ ID に対応する IoHwAb 出力関数へ値を書き込む。
 *
 * \param[in]  lamp   対象ランプ (RTE_LAMP_RUN/FAULT/ABS)。
 * \param[in]  level  出力レベル (0/1)。
 *
 * \retval  E_OK      正常に書き込んだ。
 * \retval  E_NOT_OK  lamp が範囲外。
 */
static Std_ReturnType Rte_Lamp_WriteHw(Rte_LampIdType lamp, uint8 level)
{
    switch (lamp)
    {
    case RTE_LAMP_RUN:   return IoHwAb_LedRunning_SetLevel(level);
    case RTE_LAMP_FAULT: return IoHwAb_LedFault_SetLevel(level);
    case RTE_LAMP_ABS:   return IoHwAb_Led_SetLevel(level);
    default:             return E_NOT_OK;
    }
}

/**
 * \brief   ASW からの要求レベルとオーバーライド状態を調停して IoHwAb へ出力する。
 *
 * \details オーバーライド中は aswLevel を無視して Rte_LampOverrideValue を出力する。
 *          Rte_Call_LedRunning_SetLevel() 等 (ASW 向け C/S ポート) から呼ばれる。
 *
 * \param[in]  lamp      対象ランプ。
 * \param[in]  aswLevel  ASW が要求したレベル。
 */
static Std_ReturnType Rte_Lamp_ArbitrateAndWrite(Rte_LampIdType lamp, uint8 aswLevel)
{
    const uint8 effectiveLevel = Rte_LampOverrideActive[lamp]
                                  ? Rte_LampOverrideValue[lamp] : aswLevel;
    Rte_LampLastLevel[lamp] = effectiveLevel;
    return Rte_Lamp_WriteHw(lamp, effectiveLevel);
}

/**
 * \brief   オーバーライドを有効化し、指定レベルを即座に IoHwAb へ出力する。
 *
 * \details resetToDefault / shortTermAdjustment から使用する。
 *
 * \param[in]  lamp   対象ランプ。
 * \param[in]  level  固定する出力レベル。
 */
static Std_ReturnType Rte_Lamp_ForceAndWrite(Rte_LampIdType lamp, uint8 level)
{
    Rte_LampOverrideActive[lamp] = 1U;
    Rte_LampOverrideValue[lamp]  = level;
    Rte_LampLastLevel[lamp]      = level;
    return Rte_Lamp_WriteHw(lamp, level);
}

/**
 * \brief   SpeedSensor 要求ポートから EngineSpeed シグナルを読み取る。
 *
 * \details E2E Transformer 方式（Rte_COMCbk_EngineInfo() 参照）。
 *          Com_ReceiveSignal() は経由せず、値自体は Rte_EngineInfoMirror
 *          （E2E 検証済みデータのみが反映される）から返す。*data は
 *          いずれの戻り値でも「読める最善の値」（前回の正常値、または
 *          RTE_E_HARD_TRANSFORMER_ERROR 時点では初期値ゼロ）を書き込む。
 *
 * \param[out] data  エンジン回転数を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  RTE_E_OK                     正常に読み取った。
 * \retval  RTE_E_COM_STOPPED            RX デッドライン監視タイムアウト中。
 * \retval  RTE_E_HARD_TRANSFORMER_ERROR 直近の E2E チェックが不合格
 *                                       （データ不信、フェイルセーフ判定は
 *                                       呼び出し元の責務）。
 * \retval  RTE_E_SOFT_TRANSFORMER_ERROR 直近の E2E チェックは合格したが
 *                                       一部フレーム消失を検出。
 *
 * \pre        Com_Init() と Com_RxIndication() が呼ばれていること。
 *
 * \ServiceID      {0xF2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Rte_IStatusType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_EngineInfoMirror.speed;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();

    if (Com_IsRxTimedOut(0U))
        return RTE_E_COM_STOPPED;
    return Rte_EngineInfoStatus;
}

/**
 * \brief   TempSensor 要求ポートから CoolantTemp シグナルを読み取る。
 *
 * \details E2E Transformer 方式（Rte_COMCbk_EngineInfo() 参照）。
 *          Rte_Read_SpeedSensor_EngineSpeed() と同じ設計。
 *
 * \param[out] data  冷却水温を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  RTE_E_OK / RTE_E_COM_STOPPED / RTE_E_HARD_TRANSFORMER_ERROR /
 *          RTE_E_SOFT_TRANSFORMER_ERROR  Rte_Read_SpeedSensor_EngineSpeed() 参照。
 *
 * \pre        Com_Init() と Com_RxIndication() が呼ばれていること。
 *
 * \ServiceID      {0xF3}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Rte_IStatusType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_EngineInfoMirror.temp;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();

    if (Com_IsRxTimedOut(0U))
        return RTE_E_COM_STOPPED;
    return Rte_EngineInfoStatus;
}

/**
 * \brief   EngineStatus 要求ポートから EngineOnFlag シグナルを読み取る。
 *
 * \details E2E Transformer 方式（Rte_COMCbk_EngineInfo() 参照）。
 *          Rte_Read_SpeedSensor_EngineSpeed() と同じ設計。
 *
 * \param[out] data  エンジン起動フラグを受け取る変数へのポインタ
 *                   （0 = 停止、1 = 起動）。NULL 禁止。
 *
 * \retval  RTE_E_OK / RTE_E_COM_STOPPED / RTE_E_HARD_TRANSFORMER_ERROR /
 *          RTE_E_SOFT_TRANSFORMER_ERROR  Rte_Read_SpeedSensor_EngineSpeed() 参照。
 *
 * \pre        Com_Init() と Com_RxIndication() が呼ばれていること。
 *
 * \ServiceID      {0xF4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Rte_IStatusType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_EngineInfoMirror.onFlag;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();

    if (Com_IsRxTimedOut(0U))
        return RTE_E_COM_STOPPED;
    return Rte_EngineInfoStatus;
}

/**
 * \brief   EngineStatus 提供ポートへ EngineState シグナルを書き込む。
 *
 * \details Com_SendSignal() 経由で EngineState 値を COM の TX I-PDU バッファへ
 *          パックする (AUTOSAR SWS_RTE の Rte_Write_<p>_<o> パターン)。
 *          MeterStatus は TxModeMode=MIXED のため、Com が値の変化を検知した
 *          場合は次回 Com_MainFunction()（Os の 100ms タスク）で送信される
 *          （呼び出し元が別途送信をトリガする必要はなく、この呼び出し自体は
 *          PduR_Transmit() を呼ばないため、SPI 送信でブロッキングしない）。
 *
 * \param[in]  state  書き込むエンジン状態
 *                    (OFF / STARTING / RUNNING / FAULT)。
 *
 * \retval  E_OK      COM の TX バッファへ正常にパックした。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0xF5}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Write_EngineStatus_EngineState(EngineState_t state)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    Rte_EngineStateMirror = state;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();
    uint8 val = (uint8)state;
    return Com_SendSignal(COM_SIGNAL_ENGINE_STATE, &val);
}

/**
 * \brief   EngineStatus 提供ポートから EngineState を読み取る。
 *
 * \details App_EngineManager が Rte_Write_EngineStatus_EngineState() で
 *          書き込んだ最新値を返す。DCM の DID 0x0103 読み出しに使用する。
 *
 * \param[out] data  エンジン状態を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK     常に成功。
 * \retval  E_NOT_OK data が NULL。
 *
 * \ServiceID      {0xF8}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_EngineStatus_EngineState(EngineState_t* data)
{
    if (data == NULL)
        return E_NOT_OK;
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_EngineStateMirror;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();
    return E_OK;
}

/**
 * \brief   マッピングされた SW-C Runnable を起動する。
 *
 * \details OS タスク (Task 2, 3000 ms 周期) から呼び出される。
 *          実行周期の管理は Os_PBCfg.c のタスクテーブルが担うため、
 *          この関数は App_EngineManager_Run() を無条件に呼び出すだけでよい。
 *
 *          AUTOSAR OS 環境では OsTask が直接 Runnable を呼び出すが、
 *          本実装では RTE が仲介することで SW-C と OS の直接依存を断つ。
 *
 * \pre        App_EngineManager_Init() が正常に完了していること。
 *
 * \ServiceID      {0xF7}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Rte_ScheduleRunnables(void)
{
    App_EngineManager_Run();
}

/**
 * \brief   WarningIndicator SW-C の EngineState ポートを読み取る。
 *
 * \details App_EngineManager が書き込んだ Rte_EngineStateMirror を返す。
 *          同一ミラー変数を複数の SW-C が読み取れるよう、
 *          WarningIndicator 用の独立したポートとして定義する
 *          (AUTOSAR Sender/Receiver の n:1 受信パターン)。
 *
 * \param[out] data  エンジン状態を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に読み取った。
 * \retval  E_NOT_OK  data が NULL。
 *
 * \ServiceID      {0xF9}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_WarningIndicator_EngineState(EngineState_t* data)
{
    return Rte_Read_EngineStatus_EngineState(data);
}

/**
 * \brief   WarningIndicator SW-C の Runnable を起動する。
 *
 * \details OS タスク (Task 3, 500 ms 周期) から呼び出される。
 *          App_WarningIndicator_Run() を無条件に呼び出す。
 *
 * \pre        App_WarningIndicator_Init() が正常に完了していること。
 *
 * \ServiceID      {0xFA}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Rte_ScheduleWarningIndicator(void)
{
    App_WarningIndicator_Run();
}

/**
 * \brief   警告灯 LED レベル設定の Client/Server ポート。
 *
 * \details SW-C (App_WarningIndicator) から呼び出され、
 *          Rte_Lamp_ArbitrateAndWrite() 経由で IoHwAb_Led_SetLevel() へ委譲する。
 *          C/S ポートにより SW-C は IoHwAb の存在を知らない
 *          (AUTOSAR SWS_RTE の Rte_Call_<p>_<o> パターン)。
 *          Dcm が SID 0x2F でこのランプをオーバーライド中の間、level 引数は
 *          無視され、代わりにオーバーライド値が出力される（ASW はこれを知らない）。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFB}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_Led_SetLevel(uint8 level)
{
    return Rte_Lamp_ArbitrateAndWrite(RTE_LAMP_ABS, level);
}

/**
 * \brief   RUNNING LED (D6) レベル設定の Client/Server ポート。
 *
 * \details SW-C (App_WarningIndicator) から呼び出され、
 *          Rte_Lamp_ArbitrateAndWrite() 経由で IoHwAb_LedRunning_SetLevel() へ委譲する。
 *          Dcm オーバーライド中の挙動は Rte_Call_Led_SetLevel() と同様。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFC}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_LedRunning_SetLevel(uint8 level)
{
    return Rte_Lamp_ArbitrateAndWrite(RTE_LAMP_RUN, level);
}

/**
 * \brief   FAULT LED (D7) レベル設定の Client/Server ポート。
 *
 * \details SW-C (App_WarningIndicator) から呼び出され、
 *          Rte_Lamp_ArbitrateAndWrite() 経由で IoHwAb_LedFault_SetLevel() へ委譲する。
 *          Dcm オーバーライド中の挙動は Rte_Call_Led_SetLevel() と同様。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFD}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_LedFault_SetLevel(uint8 level)
{
    return Rte_Lamp_ArbitrateAndWrite(RTE_LAMP_FAULT, level);
}

/**
 * \brief   診断制御 (Dcm SID 0x2F) を解除し、ASW に制御を返す。
 *
 * \details オーバーライドフラグを下ろすのみ。ASW (App_WarningIndicator) が
 *          次回 Runnable 実行時 (最大 500ms 後) に自身の計算値を再度出力する。
 *
 * \param[in]  lamp  対象ランプ。
 *
 * \retval  E_OK      正常に解除した。
 * \retval  E_NOT_OK  lamp が範囲外。
 *
 * \note       AUTOSAR 非標準 API 名。実際の AUTOSAR では DcmDspDidControl の
 *             ReturnControlToEcuFnc として RTE が生成する関数に相当する。
 *
 * \ServiceID      {0xE4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_IoControl_Lamp_ReturnControlToEcu(Rte_LampIdType lamp)
{
    if (lamp >= RTE_LAMP_COUNT)
        return E_NOT_OK;
    Rte_LampOverrideActive[lamp] = 0U;
    return E_OK;
}

/**
 * \brief   診断制御でランプをデフォルト値 (消灯) に固定する。
 *
 * \details returnControlToEcu が呼ばれるまで、ASW の要求値は無視され続ける。
 *
 * \param[in]  lamp  対象ランプ。
 *
 * \retval  E_OK      正常に固定した。
 * \retval  E_NOT_OK  lamp が範囲外。
 *
 * \note       AUTOSAR 非標準 API 名。ResetToDefaultFnc に相当。
 *
 * \ServiceID      {0xE5}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_IoControl_Lamp_ResetToDefault(Rte_LampIdType lamp)
{
    if (lamp >= RTE_LAMP_COUNT)
        return E_NOT_OK;
    return Rte_Lamp_ForceAndWrite(lamp, 0U);
}

/**
 * \brief   現在の物理出力値のままランプを固定する。
 *
 * \details Rte_LampLastLevel（直前に実際に IoHwAb へ出力された値）を
 *          そのままオーバーライド値として採用する。
 *
 * \param[in]  lamp  対象ランプ。
 *
 * \retval  E_OK      正常に固定した。
 * \retval  E_NOT_OK  lamp が範囲外。
 *
 * \note       AUTOSAR 非標準 API 名。FreezeCurrentStateFnc に相当。
 *
 * \ServiceID      {0xE6}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_IoControl_Lamp_FreezeCurrentState(Rte_LampIdType lamp)
{
    if (lamp >= RTE_LAMP_COUNT)
        return E_NOT_OK;
    Rte_LampOverrideValue[lamp]  = Rte_LampLastLevel[lamp];
    Rte_LampOverrideActive[lamp] = 1U;
    return E_OK;
}

/**
 * \brief   診断制御でランプを指定レベルに固定する。
 *
 * \param[in]  lamp   対象ランプ。
 * \param[in]  level  固定する出力レベル (0/1)。
 *
 * \retval  E_OK      正常に固定した。
 * \retval  E_NOT_OK  lamp が範囲外。
 *
 * \note       AUTOSAR 非標準 API 名。ShortTermAdjustmentFnc に相当。
 *
 * \ServiceID      {0xE7}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_IoControl_Lamp_ShortTermAdjustment(Rte_LampIdType lamp, uint8 level)
{
    if (lamp >= RTE_LAMP_COUNT)
        return E_NOT_OK;
    return Rte_Lamp_ForceAndWrite(lamp, level);
}

/**
 * \brief   現在 IoHwAb へ出力されている実際のレベルを取得する。
 *
 * \details Dcm が SID 0x2F の正応答 (controlStatusRecord) を構築するために使う。
 *
 * \param[in]   lamp   対象ランプ。
 * \param[out]  level  出力レベルの格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  lamp が範囲外、または level が NULL。
 *
 * \ServiceID      {0xE8}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_IoControl_Lamp_GetCurrentLevel(Rte_LampIdType lamp, uint8* level)
{
    if (lamp >= RTE_LAMP_COUNT || level == NULL)
        return E_NOT_OK;
    *level = Rte_LampLastLevel[lamp];
    return E_OK;
}

/**
 * \brief   警告確認ボタン押下状態取得の Client/Server ポート。
 *
 * \details SW-C (App_EngineManager) から呼び出され、
 *          IoHwAb_Button_GetLevel() へ委譲する。
 *          C/S ポートにより SW-C は Dio チャネル番号やプルアップ配線を知らない
 *          (AUTOSAR SWS_RTE の Rte_Call_<p>_<o> パターン)。
 *
 * \param[out] level  押下状態 (0=解放, 1=押下)。NULL 禁止。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFF}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_Button_GetLevel(uint8* level)
{
    return IoHwAb_Button_GetLevel(level);
}

/**
 * \brief   ADC センサ電圧値取得の Client/Server ポート。
 *
 * \details SW-C (App_EngineManager) から呼び出され、
 *          IoHwAb_Adc_GetValue_mV() へ委譲する。
 *          C/S ポートにより SW-C はチャネル番号や ADC スケーリングを知らない
 *          (AUTOSAR SWS_RTE の Rte_Call_<p>_<o> パターン)。
 *
 * \param[out] mv  変換済み電圧値 [mV]。NULL 禁止。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFE}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_Adc_GetValue_mV(uint16* mv)
{
    return IoHwAb_Adc_GetValue_mV(mv);
}

/**
 * \brief   機能許可状態取得の Client/Server ポート。
 *
 * \details SW-C (App_EngineManager / App_WarningIndicator) から呼び出され、
 *          FiM_GetFunctionPermission() へ委譲する。
 *          C/S ポートにより SW-C は FiM の判定テーブル構造を知らない
 *          (AUTOSAR SWS_RTE の Rte_Call_<p>_<o> パターン)。
 *
 * \param[in]   functionId  機能 ID (FIM_FID_*)。
 * \param[out]  status      1=許可 / 0=抑止 の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  functionId が範囲外、または status が NULL。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_FiM_GetFunctionPermission(uint8 functionId, uint8* status)
{
    return FiM_GetFunctionPermission(functionId, status);
}

/**
 * \brief   VehicleSensor 要求ポートから VehicleSpeed シグナルを読み取る。
 *
 * \details E2E Transformer 方式（Rte_COMCbk_AbsInfo() 参照）。
 *          Com_ReceiveSignal() は経由せず、値自体は Rte_AbsInfoMirror
 *          （E2E 検証済みデータのみが反映される、0.01 km/h 単位）から返す。
 *          シグナルの発信元は ABS ECU (CAN ID 0x110)。
 *
 * \param[out] data  車速を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  RTE_E_OK / RTE_E_COM_STOPPED / RTE_E_HARD_TRANSFORMER_ERROR /
 *          RTE_E_SOFT_TRANSFORMER_ERROR  Rte_Read_SpeedSensor_EngineSpeed() 参照。
 *
 * \ServiceID      {0xFC}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Rte_IStatusType Rte_Read_VehicleSensor_VehicleSpeed(VehicleSpeed_t* data)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_AbsInfoMirror.speed;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();

    if (Com_IsRxTimedOut(1U))
        return RTE_E_COM_STOPPED;
    return Rte_AbsInfoStatus;
}

/**
 * \brief   BrakeSensor 要求ポートから BrakeActive シグナルを読み取る。
 *
 * \details E2E Transformer 方式（Rte_COMCbk_AbsInfo() 参照）。
 *          Rte_Read_VehicleSensor_VehicleSpeed() と同じ設計
 *          （0=解除 / 1=作動）。シグナルの発信元は ABS ECU (CAN ID 0x110)。
 *
 * \param[out] data  ブレーキ作動フラグを受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  RTE_E_OK / RTE_E_COM_STOPPED / RTE_E_HARD_TRANSFORMER_ERROR /
 *          RTE_E_SOFT_TRANSFORMER_ERROR  Rte_Read_SpeedSensor_EngineSpeed() 参照。
 *
 * \ServiceID      {0xFD}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Rte_IStatusType Rte_Read_BrakeSensor_BrakeActive(BrakeActive_t* data)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_AbsInfoMirror.brake;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();

    if (Com_IsRxTimedOut(1U))
        return RTE_E_COM_STOPPED;
    return Rte_AbsInfoStatus;
}

/**
 * \brief   AbsSensor 要求ポートから AbsActive シグナルを読み取る。
 *
 * \details E2E Transformer 方式（Rte_COMCbk_AbsInfo() 参照）。
 *          Rte_Read_VehicleSensor_VehicleSpeed() と同じ設計
 *          （0=非作動 / 1=ABS 作動中）。シグナルの発信元は ABS ECU (CAN ID 0x110)。
 *
 * \param[out] data  ABS 作動フラグを受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  RTE_E_OK / RTE_E_COM_STOPPED / RTE_E_HARD_TRANSFORMER_ERROR /
 *          RTE_E_SOFT_TRANSFORMER_ERROR  Rte_Read_SpeedSensor_EngineSpeed() 参照。
 *
 * \ServiceID      {0xFE}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Rte_IStatusType Rte_Read_AbsSensor_AbsActive(AbsActive_t* data)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_AbsInfoMirror.abs;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();

    if (Com_IsRxTimedOut(1U))
        return RTE_E_COM_STOPPED;
    return Rte_AbsInfoStatus;
}

/**
 * \brief   WarningStatus 提供ポートへ RunLamp シグナルを書き込む。
 *
 * \details Com_SendSignal() 経由で RunLamp 値をシャドウバッファへパックする
 *          (AUTOSAR SWS_RTE の Rte_Write_<p>_<o> パターン)。
 *          WarningStatus (CAN 0x210) は Signal Group のため、この関数だけでは
 *          実 TX バッファへ反映されない。RunLamp/FaultLamp/AbsLamp すべてを
 *          書き込んだ後、Rte_SendSignalGroup_WarningStatus() を呼び出すこと
 *          （TxModeMode=DIRECT のため、そのコミットで変化があれば即座に
 *          送信される）。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK      COM のシャドウバッファへ正常にパックした。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0xE0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Write_WarningStatus_RunLamp(uint8 level)
{
    return Com_SendSignal(COM_SIGNAL_RUN_LAMP, &level);
}

/**
 * \brief   WarningStatus 提供ポートへ FaultLamp シグナルを書き込む。
 *
 * \details Rte_Write_WarningStatus_RunLamp() と同様。詳細はそちらを参照。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK      COM のシャドウバッファへ正常にパックした。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0xE1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Write_WarningStatus_FaultLamp(uint8 level)
{
    return Com_SendSignal(COM_SIGNAL_FAULT_LAMP, &level);
}

/**
 * \brief   WarningStatus 提供ポートへ AbsLamp シグナルを書き込む。
 *
 * \details Rte_Write_WarningStatus_RunLamp() と同様。詳細はそちらを参照。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK      COM のシャドウバッファへ正常にパックした。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0xE2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Write_WarningStatus_AbsLamp(uint8 level)
{
    return Com_SendSignal(COM_SIGNAL_ABS_LAMP, &level);
}

/**
 * \brief   WarningStatus Signal Group をシャドウバッファから確定コミットする。
 *
 * \details Com_SendSignalGroup() をラップし、SW-C (App_WarningIndicator) が
 *          COM の I-PDU ID を意識せずに Signal Group をコミットできるようにする
 *          (AUTOSAR 非標準 API)。RunLamp/FaultLamp/AbsLamp すべてを
 *          Rte_Write_WarningStatus_*() で設定した後に呼び出すこと。
 *          WarningStatus は TxModeMode=DIRECT のため、このコミットで変化が
 *          検知されれば次回 Com_MainFunction() で送信される（呼び出し元が
 *          別途送信をトリガする必要はなく、この呼び出し自体は
 *          PduR_Transmit() を呼ばないため、SPI 送信でブロッキングしない）。
 *
 * \retval  E_OK      COM の実 TX バッファへ正常にコミットした。
 * \retval  E_NOT_OK  COM 未初期化、または WarningStatus の I-PDU ID が
 *                    見つからない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0xE3}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_SendSignalGroup_WarningStatus(void)
{
    return Com_SendSignalGroup(1U);
}

/**
 * \brief   通信モード要求の Client/Server ポート（ComM_USER_0 として要求）。
 *
 * \details SW-C (App_EngineManager) から呼び出され、
 *          ComM_RequestComMode(COMM_USER_0, mode) へ委譲する。
 *          エンジン OFF が一定サイクル継続したときの通信不要判断
 *          （ボランタリスリープ）に使う。
 *
 * \param[in]  mode  要求する通信モード (COMM_FULL_COMMUNICATION 等)。
 *
 * \retval  E_OK      要求を受理した（チャネルが実際に遷移したとは限らない）。
 * \retval  E_NOT_OK  ComM 未初期化、または mode が不正。
 *
 * \ServiceID      {0xE9}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_ComM_RequestComMode(ComM_ModeType mode)
{
    return ComM_RequestComMode(COMM_USER_0, mode);
}

/**
 * \brief   現在の通信モード取得の Client/Server ポート（ComM_USER_0 について）。
 *
 * \details SW-C (App_EngineManager) から呼び出され、
 *          ComM_GetCurrentComMode(COMM_USER_0, mode) へ委譲する。
 *          通信スリープからの復帰直後を検出するために使う。
 *
 * \param[out] mode  現在の通信モードを受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  ComM 未初期化、または mode が NULL。
 *
 * \ServiceID      {0xEA}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_ComM_GetCurrentComMode(ComM_ModeType* mode)
{
    return ComM_GetCurrentComMode(COMM_USER_0, mode);
}

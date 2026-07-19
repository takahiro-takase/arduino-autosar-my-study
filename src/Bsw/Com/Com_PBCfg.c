/**
 * \file    Com_PBCfg.c
 * \brief   通信マネージャ ポストビルド設定データ (AUTOSAR SWS_COM 準拠)
 * \details COM モジュールのポストビルド設定インスタンス Com_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、I-PDU / シグナルのレイアウト情報を実装コードから分離して管理する。
 *
 *          本プロジェクトの設定（メータ ECU 想定）:
 *            RX I-PDU 0 (IPduId=0): CAN ID 0x100, DLC=6  EngineInfo  (エンジン ECU、E2E P01 保護)
 *              byte[0]: E2E CRC8 / byte[1]: E2E Counter (下位4bit)
 *              （AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠のレイアウト）
 *              Signal 0: EngineSpeed   16 bit  BitPos=16  BigEndian
 *              Signal 1: CoolantTemp    8 bit  BitPos=32  BigEndian
 *              Signal 2: EngineOnFlag   1 bit  BitPos=40  BigEndian
 *            RX I-PDU 1 (IPduId=1): CAN ID 0x110, DLC=5  AbsInfo     (ABS ECU, E2E P01 保護)
 *              byte[0]: E2E CRC8 / byte[1]: E2E Counter (下位4bit)
 *              （AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠のレイアウト）
 *              IsSignalGroup=1（RX Signal Group、Com_ReceiveSignalGroup で
 *              3 信号を一括して一貫したスナップショットとして読む。
 *              Rte_COMCbk_AbsInfo が RxIndicationCbk として確定コピーする）
 *              Signal 4: VehicleSpeed  16 bit  BitPos=16  BigEndian  0.01 km/h
 *                RxDataTimeoutAction=SUBSTITUTE（タイムアウト中は 0xFFFF を返す）
 *              Signal 5: BrakeActive    1 bit  BitPos=32  BigEndian  0=解除/1=作動
 *              Signal 6: AbsActive      1 bit  BitPos=33  BigEndian  0=非作動/1=作動
 *            TX I-PDU 0 (IPduId=0): CAN ID 0x200, DLC=1  MeterStatus
 *              (メータ ECU、E2E 保護なし、TxModeMode=MIXED。
 *              ComFilterAlgorithm=MASKED_NEW_DIFFERS_MASKED_OLD で値変化を
 *              検知すると次回 Com_MainFunction() で送信、変化がなくても
 *              Com_Cfg.h の COM_TX_PERIOD_METERSTATUS_FLOOR_MS 間隔で
 *              周期フロア送信する)
 *              Signal 3: EngineState    8 bit  BitPos= 0  BigEndian
 *            TX I-PDU 1 (IPduId=1): CAN ID 0x210, DLC=1  WarningStatus (メータ ECU、Signal Group)
 *              TMS（Transmission Mode Selector）を持つ I-PDU:
 *                通常（FaultLamp/AbsLamp 消灯 = TMS false）は TxModeMode=DIRECT。
 *                ダッシュボード表示用ミラー情報のため周期フロアを持たず、
 *                値変化時のみ次回 Com_MainFunction() で送信する。
 *                FaultLamp/AbsLamp のいずれかが点灯中（TMS true）は
 *                TxModeModeTrue=MIXED へ自動切り替えし、
 *                COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS 間隔で周期フロア送信する。
 *              Signal 7: RunLamp        1 bit  BitPos= 0  BigEndian  TransferProperty=TRIGGERED_ON_CHANGE
 *              Signal 8: FaultLamp      1 bit  BitPos= 1  BigEndian  TmsContributor=1  TransferProperty=TRIGGERED_ON_CHANGE
 *              Signal 9: AbsLamp        1 bit  BitPos= 2  BigEndian  TmsContributor=1  TransferProperty=TRIGGERED_ON_CHANGE
 *              IsSignalGroup=1（Com_SendSignalGroup で 3 信号を一括コミット）。
 *              3 灯とも ComTransferProperty=TRIGGERED_ON_CHANGE（各灯どれか 1 つでも
 *              変化すれば送信を引き起こす。COM_TRANSFER_PROPERTY_PENDING を
 *              使う例は本設定には無いが、フィールド自体は他メンバーの送信に
 *              便乗させたい灯があれば追加できる）。
 *            TX I-PDU 2 (IPduId=2): CAN ID 0x220, DLC=4  E2EHealthStatus
 *              (メータ ECU、TxModeMode=COM_TX_MODE_PERIODIC、E2E P01 保護)
 *              byte[0]: E2E CRC8 / byte[1]: E2E Counter (下位4bit)
 *              （AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠のレイアウト）
 *              Signal 10: E2ECrcErrCount  8 bit  BitPos=16  BigEndian
 *              Signal 11: E2ESeqErrCount  8 bit  BitPos=24  BigEndian
 *              値は E2EMon（CDD 相当、src/Bsw/E2EMon/）が Com_SendSignal() で
 *              更新し、送信タイミング自体は Com 自身の PERIODIC モードが
 *              6000ms 周期で自動的に判断する（ASW/CDD は関与しない）。
 *              E2EMon 自身は E2E 保護の存在を一切知らない（MeterStatus における
 *              App_EngineManager と同じ関係。E2E 保護対象は当初 MeterStatus
 *              だったが、監視ツールがネットワーク健全性テレメトリ自体の破損を
 *              検出できるよう E2EHealthStatus 側へ移した）。
 *
 *          E2E P01 の設定・ステート実体（DataID/Counter・CRC オフセット等）は
 *          E2E Transformer 方式への移行に伴い src/Bsw/E2EXf/E2EXf_PBCfg.c へ
 *          移設した。ここでは RxIndicationCbk/TxTransformCbk 経由で Rte 層の
 *          グルー関数（Rte_COMCbk_EngineInfo 等）を紐付けるのみで、
 *          Com_PBCfg.c 自体は E2E の詳細を一切保持しない。
 *
 * =====================================================================
 * DaVinci Configurator 対応表（各フィールドと GUI パラメータの対応）
 * =====================================================================
 *
 * [Com_IPduConfigType] ←→ /ActiveEcuC/Com/ComConfig/[ComIPdu]
 *   .IPduId  ←→ ComIPduHandleId   （I-PDU の識別番号）
 *   .DLC     ←→ ComIPduLength     （I-PDU のバイト長）
 *   .PduRId  ←→ ComIPduPduRef     （PduR の対応 PDU へのリンク）
 *   .IsSignalGroup ←→ （本プロジェクト独自拡張。DaVinci では ComSignalGroup の
 *                      有無で暗黙的に表現される）1 = Signal Group。
 *                      TX I-PDU では Com_SendSignalGroup()、RX I-PDU では
 *                      Com_ReceiveSignalGroup() で確定コミット/コピーする対象
 *
 * [Com_SignalConfigType] ←→ /ActiveEcuC/Com/ComConfig/[ComSignal]
 *   .SignalId    ←→ ComHandleId          （シグナルの識別番号）
 *   .IPduId      ←→ ComIPduRef           （所属する I-PDU へのリンク）
 *   .BitPosition ←→ ComBitPosition       （PDU バッファ内のビット開始位置）
 *   .BitSize     ←→ ComBitSize           （シグナルのビット長）
 *   .Endian      ←→ ComSignalEndianness  （OPAQUE=BigEndian / INTEL=LittleEndian）
 *   .FilterAlgorithm ←→ ComFilterAlgorithm （TX シグナルのみ。RX シグナルは既定の ALWAYS のまま未使用）
 *   .Mask            ←→ ComFilterMask
 *   .TransferProperty ←→ ComTransferProperty （Signal Group メンバーのみ使用）
 *   .RxDataTimeoutAction      ←→ ComRxDataTimeoutAction      （RX シグナルのみ使用）
 *   .TimeoutSubstitutionValue ←→ ComTimeoutSubstitutionValue （RxDataTimeoutAction=SUBSTITUTE のみ使用）
 *
 * =====================================================================
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Com_PBCfg.h"
#include "Com_Cfg.h"

/* Rte 層の E2E Transformer 呼び出しグルー関数（Rte.c で定義）。
 * Os_PBCfg.c 等と同じく、レイヤ違反（Com が Rte.h を include する）を
 * 避けるためローカル extern 宣言で参照する。 */
extern void Rte_COMCbk_EngineInfo(void);
extern void Rte_COMCbk_AbsInfo(void);
extern void Rte_COMTransform_E2EHealthStatus(uint8* Data, uint8 Length);

/* -----------------------------------------------------------------------
 * RX I-PDU テーブル
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu] (Direction=RECEIVE)
 * Com_RxIndication() が受信 PDU をバッファに格納する際に参照する。
 * ----------------------------------------------------------------------- */
static const Com_IPduConfigType Com_RxIPduConfigData[COM_RX_IPDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * RX IPduId=0: EngineInfo フレーム (エンジン ECU 送信、E2E P01 保護)
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineInfo_Rx
         * --------------------------------------------------------------- */
        .IPduId    = 0U,                        /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 6U,                        /* DaVinci: ComIPduLength    - I-PDU バイト長
                                                 *          byte[0]=E2E CRC, byte[1]=E2E Counter, byte[2-5]=シグナル */
        .PduRId    = 0U,                        /* DaVinci: ComIPduPduRef    - PduR が Com_RxIndication へ渡す DestPduId
                                                 *          (PduR_PBCfg.c PduR_RxDests_Path0[0].DestPduId と一致させること) */
        .TimeoutMs = COM_TIMEOUT_ENGINE_INFO_MS,/* DaVinci: ComRxDeadlineMonitoringPeriod
                                                 *          エンジン ECU からの受信が途絶えたと判断するまでの時間 */
        .RxIndicationCbk = Rte_COMCbk_EngineInfo /* DaVinci: /ActiveEcuC/E2EXf/EngineInfo_Rx_E2EXf
                                                 *          （E2E Transformer 呼び出しは Rte 層が担う） */
    },
    {
        /* ---------------------------------------------------------------
         * RX IPduId=1: AbsInfo フレーム (ABS ECU 送信、E2E P01 保護)
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsInfo_Rx
         * IsSignalGroup=1（RX Signal Group）: VehicleSpeed/BrakeActive/AbsActive
         * の 3 シグナルを Com_ReceiveSignalGroup() 経由で一括して一貫したスナップ
         * ショットとして読む。Rte_COMCbk_AbsInfo() が RxIndicationCbk として
         * フレーム受信の都度呼ばれ、その中で確定コピーする。
         * --------------------------------------------------------------- */
        .IPduId    = 1U,                       /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 5U,                       /* DaVinci: ComIPduLength    - I-PDU バイト長
                                                *          byte[0]=E2E CRC, byte[1]=E2E Counter, byte[2-4]=シグナル */
        .PduRId    = 1U,                       /* DaVinci: ComIPduPduRef    - PduR が Com_RxIndication へ渡す DestPduId
                                                *          (PduR_PBCfg.c PduR_RxDests_Path2[0].DestPduId と一致させること) */
        .TimeoutMs = COM_TIMEOUT_ABS_INFO_MS,  /* DaVinci: ComRxDeadlineMonitoringPeriod
                                                *          ABS ECU からの受信が途絶えたと判断するまでの時間 */
        .IsSignalGroup = 1U,                   /* RX Signal Group（Com_ReceiveSignalGroup で確定コピー） */
        .RxIndicationCbk = Rte_COMCbk_AbsInfo  /* DaVinci: /ActiveEcuC/E2EXf/AbsInfo_Rx_E2EXf
                                                *          （E2E Transformer 呼び出しは Rte 層が担う） */
    }
};

/* -----------------------------------------------------------------------
 * TX I-PDU テーブル
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu] (Direction=SEND)
 * Com_MainFunction()（DIRECT/MIXED の変化時送信・PERIODIC/MIXED の周期送信、
 * いずれもこの関数のみが実送信を行う）が送信要求を PduR へ転送する際に参照する。
 * ----------------------------------------------------------------------- */
static const Com_IPduConfigType Com_TxIPduConfigData[COM_TX_IPDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * TX IPduId=0: MeterStatus フレーム (メータ ECU 送信、E2E 保護なし)
         * DaVinci: /ActiveEcuC/Com/ComConfig/MeterStatus_Tx
         * MIXED: EngineState は他 ECU（盗難防止・ボディ制御等）が判断材料に
         * 使いうるため、変化時送信に加えて周期フロアで再送し続ける
         * （起動直後や瞬断復帰後の受信側が古い値のままにならないようにする）。
         * --------------------------------------------------------------- */
        .IPduId    = 0U,  /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 1U,  /* DaVinci: ComIPduLength    - I-PDU バイト長（byte[0]=EngineState のみ） */
        .PduRId    = 0U,  /* DaVinci: ComIPduPduRef    - PduR TX パス 0 へのリンク */
        .TimeoutMs = 0U,  /* TX I-PDU のため監視無効 */
        .IsSignalGroup = 0U, /* 直接送信（既存の挙動のまま） */
        .TxModeMode = COM_TX_MODE_MIXED, /* DaVinci: ComTxModeMode = MIXED
                                          *          (Com_SendSignal() が変化検知時に Com_TxPending を立て、
                                          *          次回 Com_MainFunction() で送信) */
        .TxPeriodMs = COM_TX_PERIOD_METERSTATUS_FLOOR_MS /* DaVinci: ComTxModeTimePeriodFactor（周期フロア間隔） */
    },
    {
        /* ---------------------------------------------------------------
         * TX IPduId=1: WarningStatus フレーム (メータ ECU 送信、Signal Group)
         * DaVinci: /ActiveEcuC/Com/ComConfig/WarningStatus_Tx
         * App_WarningIndicator が制御する 3 本の警告灯（RUNNING/FAULT/ABS）を
         * 1 つの Signal Group としてまとめて送信する。
         * TMS（Transmission Mode Selector）を持つ I-PDU:
         *   通常（FaultLamp/AbsLamp とも消灯 = TMS false）は DIRECT。
         *   ダッシュボード表示用の LED ミラー情報であり、他 ECU の制御判断に
         *   使う想定がないため、周期フロアを持たず変化時のみ送信する
         *   （取りこぼしても次の変化で追いつけるため実害が小さいと判断）。
         *   FaultLamp/AbsLamp のいずれかが点灯中（TMS true）は MIXED に
         *   自動切り替えする。警告状態は他 ECU・監視ツールが途中から参加
         *   しても把握できてほしいため、周期フロアで再送し続ける。
         *   どの信号が TMS に寄与するかは Signal 8/9（FAULT_LAMP/ABS_LAMP）の
         *   .TmsContributor=1 で設定する（RunLamp は寄与しない）。
         * MDT（ComMinimumDelayTime）: 変化時送信に最小送信間隔を設ける
         * バス輻輳保護。周期フロアには適用されない（Com.c 参照）。
         * --------------------------------------------------------------- */
        .IPduId    = 1U,  /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 1U,  /* DaVinci: ComIPduLength    - I-PDU バイト長（3bit のみ使用） */
        .PduRId    = 2U,  /* DaVinci: ComIPduPduRef    - PduR TX パス 2 へのリンク
                           *          (PduR_Transmit の SrcPduId は COM/CanTp で共通の名前空間のため、
                           *          CanTp が使用する 1U と衝突しないよう 2U を割り当てる) */
        .TimeoutMs = 0U,  /* TX I-PDU のため監視無効 */
        .IsSignalGroup = 1U, /* Signal Group（Com_SendSignalGroup で確定コミット） */
        .TxModeMode     = COM_TX_MODE_DIRECT, /* DaVinci: ComTxModeMode = ComTxModeFalse
                                               *          (TMS false: 通常時) */
        .TxModeModeTrue = COM_TX_MODE_MIXED,  /* DaVinci: ComTxModeTrue
                                               *          (TMS true: FAULT/ABS 点灯中) */
        .TxPeriodMsTrue = COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS,
        .MinDelayMs     = COM_TX_MIN_DELAY_WARNINGSTATUS_MS /* DaVinci: ComMinimumDelayTime */
    },
    {
        /* ---------------------------------------------------------------
         * TX IPduId=2: E2EHealthStatus フレーム (メータ ECU 送信、PERIODIC、E2E P01 保護)
         * DaVinci: /ActiveEcuC/Com/ComConfig/E2EHealthStatus_Tx
         * E2EMon（CDD 相当、src/Bsw/E2EMon/）が E2E 検証エラーの累積数を
         * 保持し、値が変化するたびに Com_SendSignal() で更新する。
         * 送信タイミングは Com 自身の PERIODIC モードが担うため、E2EMon 自身は
         * 周期送信のスケジューリングに一切関与しない（実 AUTOSAR の Com の
         * PERIODIC 送信モードと同じ責務分離）。ネットワーク健全性テレメトリ
         * 自体の破損を監視ツールが検出できるよう E2E P01 保護を付与する
         * （TxTransformCbk 経由、Rte 層が E2EXf を呼ぶ点は MeterStatus と同じ）。
         * --------------------------------------------------------------- */
        .IPduId     = 2U,    /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC        = 4U,    /* DaVinci: ComIPduLength    - byte[0]=E2E CRC, byte[1]=E2E Counter,
                              *          byte[2]=CrcErrCount, byte[3]=SeqErrCount */
        .PduRId     = 3U,    /* DaVinci: ComIPduPduRef    - PduR TX パス 3 へのリンク */
        .TimeoutMs  = 0U,    /* TX I-PDU のため監視無効 */
        .IsSignalGroup = 0U, /* 直接送信 */
        .TxModeMode = COM_TX_MODE_PERIODIC,      /* DaVinci: ComTxModeMode = PERIODIC */
        .TxPeriodMs = COM_TX_PERIOD_E2EHEALTH_MS, /* DaVinci: ComTxModeTimePeriodFactor */
        .TxTransformCbk = Rte_COMTransform_E2EHealthStatus /* DaVinci: /ActiveEcuC/E2EXf/E2EHealthStatus_Tx_E2EXf
                                                *          （E2E Transformer 呼び出しは Rte 層が担う） */
    }
};

/* -----------------------------------------------------------------------
 * シグナルテーブル（RX + TX 共通）
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComSignal]
 * Com_ReceiveSignal() / Com_SendSignal() がビットパック・アンパックに使用する。
 * シグナル ID は Com_Cfg.h の COM_SIGNAL_* 定数と対応している。
 *
 * ComBitPosition の数え方（BigEndian/OPAQUE）:
 *   byte[0] の MSB = ビット位置 0、byte[0] の LSB = ビット位置 7
 *   byte[1] の MSB = ビット位置 8 ...
 * ----------------------------------------------------------------------- */
static const Com_SignalConfigType Com_SignalConfigData[COM_SIGNAL_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * Signal 0: EngineSpeed  RX 16bit  CAN 0x100 byte[2-3]
         * （byte[0]=E2E CRC8, byte[1]=E2E Counter を先頭に配置する
         *   AUTOSAR 標準バリアント 1A レイアウトのため、シグナルは byte[2] から）
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineSpeed_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_SPEED, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 16U,                     /* DaVinci: ComBitPosition      */
        .BitSize     = 16U,                     /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 1: CoolantTemp  RX 8bit  CAN 0x100 byte[4]
         * DaVinci: /ActiveEcuC/Com/ComConfig/CoolantTemp_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_COOLANT_TEMP, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 32U,                     /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                      /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 2: EngineOnFlag  RX 1bit  CAN 0x100 byte[5] bit7
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineOnFlag_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_ON_FLAG, /* DaVinci: ComHandleId       */
        .IPduId      = 0U,                        /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 40U,                       /* DaVinci: ComBitPosition    */
        .BitSize     = 1U,                        /* DaVinci: ComBitSize        */
        .Endian      = COM_BIG_ENDIAN             /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 3: EngineState  TX 8bit  CAN 0x200 byte[0]
         * （E2E 保護なしのため、シグナルは byte[0] から始まる）
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineState_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_STATE, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → MeterStatus_Tx */
        .BitPosition = 0U,                      /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                      /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN,          /* DaVinci: ComSignalEndianness = OPAQUE */
        .FilterAlgorithm = COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD, /* DaVinci: ComFilterAlgorithm
                                                 *          値が変化したときだけ送信要求とみなす */
        .Mask            = 0xFFU                /* DaVinci: ComFilterNewValue/ComFilterMask 相当（8bit 全体を比較） */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 4: VehicleSpeed  RX 16bit  CAN 0x110 byte[2-3]  0.01 km/h
         * （byte[0]=E2E CRC8, byte[1]=E2E Counter を先頭に配置する
         *   AUTOSAR 標準バリアント 1A レイアウトのため、シグナルは byte[2] から）
         * DaVinci: /ActiveEcuC/Com/ComConfig/VehicleSpeed_Rx
         * RX Signal Group（AbsInfo_Rx）メンバー。Com_ReceiveSignalGroup(1U) 経由
         * でのみ最新化される（Rte_COMCbk_AbsInfo 参照）。
         * RxDataTimeoutAction=SUBSTITUTE: タイムアウト中は 0xFFFF（655.35 km/h
         * 相当、物理的にあり得ない値）を返す。停車中の実データ 0 と、
         * 通信途絶時の「値不明」を明確に区別するため（既定の NONE のまま
         * だと E_NOT_OK になるだけで、速度そのものは返らない）。
         * 注意: この VehicleSpeed を読む Com_ReceiveSignal() は
         * Rte_COMCbk_AbsInfo()（RxIndicationCbk）内の 1 箇所のみで、
         * フレーム受信・E2E 検証成功直後にしか呼ばれないため、この
         * SUBSTITUTE 分岐は現状のアーキテクチャでは実際には発動しない
         * （実際のフェイルセーフは Rte_Read_* 側の Com_IsRxTimedOut() が
         * 別途担う）。動機は実利より仕様忠実性。詳細は README.md の
         * 「ComRxDataTimeoutAction」節を参照。
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_VEHICLE_SPEED, /* DaVinci: ComHandleId        */
        .IPduId      = 1U,                       /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 16U,                      /* DaVinci: ComBitPosition     */
        .BitSize     = 16U,                      /* DaVinci: ComBitSize         */
        .Endian      = COM_BIG_ENDIAN,           /* DaVinci: ComSignalEndianness = OPAQUE */
        .RxDataTimeoutAction     = COM_RX_TIMEOUT_ACTION_SUBSTITUTE, /* DaVinci: ComRxDataTimeoutAction */
        .TimeoutSubstitutionValue = 0xFFFFU      /* DaVinci: ComTimeoutSubstitutionValue */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 5: BrakeActive  RX 1bit  CAN 0x110 byte[4] bit7
         *   0=ブレーキ解除, 1=ブレーキ作動
         * DaVinci: /ActiveEcuC/Com/ComConfig/BrakeActive_Rx
         * RX Signal Group（AbsInfo_Rx）メンバー。VehicleSpeed と同様。
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_BRAKE_ACTIVE, /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 32U,                     /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 6: AbsActive  RX 1bit  CAN 0x110 byte[4] bit6
         *   0=ABS 非作動, 1=ABS 作動中
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsActive_Rx
         * RX Signal Group（AbsInfo_Rx）メンバー。VehicleSpeed と同様。
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ABS_ACTIVE,   /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 33U,                     /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 7: RunLamp  TX 1bit  CAN 0x210 byte[0] bit7  (Signal Group メンバー)
         * DaVinci: /ActiveEcuC/Com/ComConfig/RunLamp_Tx
         * ComTransferProperty=TRIGGERED_ON_CHANGE: 自身の点灯/消灯だけで
         * WarningStatus の送信を引き起こす（他の灯の変化を待たない）。
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_RUN_LAMP,     /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → WarningStatus_Tx */
        .BitPosition = 0U,                      /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN,          /* DaVinci: ComSignalEndianness = OPAQUE */
        .TransferProperty = COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE /* DaVinci: ComTransferProperty */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 8: FaultLamp  TX 1bit  CAN 0x210 byte[0] bit6  (Signal Group メンバー)
         * DaVinci: /ActiveEcuC/Com/ComConfig/FaultLamp_Tx
         * TMS 寄与シグナル: 点灯（値=1）で WarningStatus の TMS を true にする
         * （MASKED_NEW_DIFFERS_X、Mask=0x01・FilterX=0 → 値!=0 で真）。
         * ComTransferProperty=TRIGGERED_ON_CHANGE: TMS 判定（FilterAlgorithm）
         * とは独立に、自身の点灯/消灯だけで送信も引き起こす。
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_FAULT_LAMP,   /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → WarningStatus_Tx */
        .BitPosition = 1U,                      /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN,          /* DaVinci: ComSignalEndianness = OPAQUE */
        .FilterAlgorithm = COM_FILTER_MASKED_NEW_DIFFERS_X, /* DaVinci: ComFilterAlgorithm（TMS 評価用） */
        .Mask            = 0x01U,
        .FilterX         = 0U,
        .TmsContributor  = 1U,
        .TransferProperty = COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE /* DaVinci: ComTransferProperty */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 9: AbsLamp  TX 1bit  CAN 0x210 byte[0] bit5  (Signal Group メンバー)
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsLamp_Tx
         * TMS 寄与シグナル: FaultLamp と同様（値=1 で WarningStatus の TMS を true に）。
         * ComTransferProperty=TRIGGERED_ON_CHANGE: FaultLamp と同様。
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ABS_LAMP,     /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → WarningStatus_Tx */
        .BitPosition = 2U,                      /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN,          /* DaVinci: ComSignalEndianness = OPAQUE */
        .FilterAlgorithm = COM_FILTER_MASKED_NEW_DIFFERS_X, /* DaVinci: ComFilterAlgorithm（TMS 評価用） */
        .Mask            = 0x01U,
        .FilterX         = 0U,
        .TmsContributor  = 1U,
        .TransferProperty = COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE /* DaVinci: ComTransferProperty */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 10: E2ECrcErrCount  TX 8bit  CAN 0x220 byte[2]
         * （byte[0]=E2E CRC8, byte[1]=E2E Counter を先頭に配置する
         *   AUTOSAR 標準バリアント 1A レイアウトのため、シグナルは byte[2] から）
         * DaVinci: /ActiveEcuC/Com/ComConfig/E2ECrcErrCount_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_E2E_CRC_ERR_COUNT, /* DaVinci: ComHandleId     */
        .IPduId      = 2U,                      /* DaVinci: ComIPduRef → E2EHealthStatus_Tx */
        .BitPosition = 16U,                      /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                       /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN            /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 11: E2ESeqErrCount  TX 8bit  CAN 0x220 byte[3]
         * DaVinci: /ActiveEcuC/Com/ComConfig/E2ESeqErrCount_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_E2E_SEQ_ERR_COUNT, /* DaVinci: ComHandleId     */
        .IPduId      = 2U,                      /* DaVinci: ComIPduRef → E2EHealthStatus_Tx */
        .BitPosition = 24U,                      /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                       /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN            /* DaVinci: ComSignalEndianness = OPAQUE */
    }
};

/* -----------------------------------------------------------------------
 * COM ポストビルド設定インスタンス
 * Com_Init() の引数として渡す。
 * ----------------------------------------------------------------------- */
const Com_ConfigType Com_Config = {
    .RxIPdus     = Com_RxIPduConfigData,
    .RxIPduCount = COM_RX_IPDU_COUNT,
    .TxIPdus     = Com_TxIPduConfigData,
    .TxIPduCount = COM_TX_IPDU_COUNT,
    .Signals     = Com_SignalConfigData,
    .SignalCount  = COM_SIGNAL_COUNT
};

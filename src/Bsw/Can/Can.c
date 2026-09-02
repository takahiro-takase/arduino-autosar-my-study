/**
 * \file    Can.c
 * \brief   CAN ドライバ (AUTOSAR SWS_Can 準拠)
 * \details AUTOSAR CanDrv API を MCP2515 上に実装する。
 *          Can_Hw.cpp（MCP2515 ハードウェアラッパー）経由でハードウェアを操作し、
 *          AUTOSAR 4.x SWS_Can 仕様に準拠する。
 *          Arduino UNO 向けに一部を簡略化している。
 *
 *          TX 確認の非同期化 (SWS_Can_00016):
 *            仕様は CanIf_TxConfirmation() を「TX 割り込みハンドラから」または
 *            「ポーリングモードでは Can_MainFunction_Write() の中から」呼ぶことを
 *            求めている。Can_Write() が呼び出し元と同一スタックフレーム内で
 *            即座に CanIf_TxConfirmation() を呼ぶと、将来 TxConfirmation の
 *            延長線上（Com_TxConfirmation 等）に「送信失敗時は即座に再送する」
 *            ような処理が足された場合、そのまま Can_Write() を再帰呼び出しする
 *            経路になってしまう（バスオフ+HW ウォッチドッグリセットの過去バグと
 *            同系統のスタック深化リスク）。Can_Write() は送信成功を「保留」として
 *            キューに積むだけにし、実際の CanIf_TxConfirmation() 呼び出しは
 *            Can_MainFunction_Write()（Os スケジューラから独立に呼ばれる別タスク）
 *            まで遅延させることで、この結合を断ち切る。
 *
 *          RX の割り込み化:
 *            従来 Can_Isr() は Os スケジューラから 1 ms ごとにポーリング呼び出し
 *            される「疑似 ISR」で、INT ピンを digitalRead() で確認していた。
 *            本実装では Can_Hw_AttachRxIsr()（Can_Init 内）で INT ピンの
 *            立ち下がりエッジに Can_Isr() を真のハードウェア割り込みとして
 *            登録し、Os スケジューラの周期に関係なく即座に起動されるようにした。
 *
 *            ただし Can_Isr() 自体は「ペンディングフラグを立てるだけ」に留め、
 *            SPI 通信 (Can_Hw_Read) も Serial ログ (DET_LOG) も一切行わない。
 *            理由は 2 つ:
 *              (1) SPI バス排他: MCP2515 は SPI 接続のため、CS ピン制御を伴う
 *                  複数バイトの読み書きが 1 トランザクションとして完結する
 *                  必要がある。メインループ側の Can_Write()（TX、SPI 経由）が
 *                  トランザクション途中で割り込みにプリエンプトされ、割り込み
 *                  側が同じ SPI バスへ別トランザクションを割り込ませると、
 *                  双方が破壊される。ISR 側で SPI を使わなければこの競合は
 *                  そもそも発生しない。
 *              (2) 処理時間の上限: CanIf_RxIndication() から先は PduR/Com/CanTp/
 *                  Dcm まで連鎖し、UDS SID 処理（RoutineControl 等）まで含まれ
 *                  得る。これを割り込みハンドラの中で行うと、他の割り込みや
 *                  Serial 送信バッファの空き待ちなどで ISR の実行時間が事実上
 *                  無制限になり得る（本セッションで繰り返し修正してきた
 *                  「同期呼び出し連鎖のスタック/ブロッキングリスク」と同種の問題）。
 *
 *            実際の SPI 読み出しと CanIf_RxIndication() 呼び出しは、ペンディング
 *            フラグを見てメインループのタスク Can_MainFunction_Read() が行う
 *            （AUTOSAR SWS_Can_00396・SWS_Can_00012 参照: 「呼び出しコンテキストが
 *            ISR か Can_MainFunction_Read かは実装依存であり、コールバックは
 *            いずれの場合も ISR から呼ばれたかのように実装してよい」）。
 *            ペンディングフラグの読み出し＋クリアは、フラグを立てる Can_Isr()
 *            と競合するため SchM_Enter/Exit_Can_IRQFLAG_EXCLUSIVE_AREA()
 *            （実体は noInterrupts()/interrupts()）で保護する。
 *
 *            Bus-Off ポーリング (Can_MainFunction_BusOff) とスリープ中の
 *            ウェイクアップ検出 (Can_MainFunction_Wakeup) も、旧 Can_Isr() が
 *            1 つの関数にまとめて行っていたものを AUTOSAR の定義どおり
 *            個別の Can_MainFunction_xxx へ分離した。
 *
 *          実機検証で得られた教訓（意図的な二重化）:
 *            実機検証の初回テストで、Can_Isr() が一度も起動されていないように
 *            見える現象が発生した（診断用に一時的に追加したカウンタが常に 0）。
 *            後日の再テストでは同じカウンタが正常にインクリメントしており、
 *            割り込み自体は機能することを確認した。初回テストで 0 のままだった
 *            直接の原因は特定できていない（その時点でバス上に実際のフレームが
 *            流れていなかった可能性が高い）。
 *
 *            この経緯を踏まえ、Can_MainFunction_Read() / Can_MainFunction_Wakeup()
 *            は「割り込みが本当に発火するか」に正しさを依存させない設計とした:
 *              - Can_MainFunction_Read() は Can_RxIrqPending の有無に関わらず
 *                毎回無条件に Can_Hw_CheckReceive() でドレインする
 *                （CheckReceive は INT ピンではなく SPI 経由のステータス
 *                レジスタ読み出しのため、割り込みの成否に関係なく正しく動く）。
 *              - Can_MainFunction_Wakeup() は Can_WakeupIrqPending に加えて
 *                digitalRead(intPin) の直接ポーリングも併用する（旧実装と同じ
 *                フォールバック）。
 *            Can_Isr()・ペンディングフラグ・SchM 排他エリアの構造はそのまま
 *            残り、割り込みが発火すればより低遅延に反応できる「ボーナス経路」
 *            として機能するが、たとえ割り込みが何らかの理由で発火しなくても
 *            ポーリング側だけで正しく動作することが実機で確認できている。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "Det.h"
#include "SchM.h"

#define TAG "Can"

/* Can_Hw_AttachRxIsr() へ渡すコールバックとしてのみ参照される（Can.c 内で
 * のみ使う内部関数のため Can.h には公開しない）。定義は本ファイル末尾。 */
static void Can_Isr(void);

static const Can_ConfigType*    Can_ConfigPtr  = NULL;
/** Can_Isr()（真の割り込みコンテキスト）と Can_MainFunction_xxx()（メインループ）
 *  の両方から読み書きされるため volatile。 */
static volatile Can_ControllerStateType CanState = CAN_CS_UNINIT;

/* -----------------------------------------------------------------------
 * RX/ウェイクアップ ペンディングフラグ
 *
 * Can_Isr()（真のハードウェア割り込み、INT ピン立ち下がりで起動）が
 * セットするだけで、実際の SPI 読み出し・CanIf 通知はメインループの
 * Can_MainFunction_Read() / Can_MainFunction_Wakeup() まで遅延させる
 * （理由はファイル冒頭のコメントを参照）。
 * 読み出し＋クリアは SchM_Enter/Exit_Can_IRQFLAG_EXCLUSIVE_AREA() で
 * 保護し、フラグセットの取りこぼしを防ぐ。
 * ----------------------------------------------------------------------- */
static volatile uint8 Can_RxIrqPending     = 0U;
static volatile uint8 Can_WakeupIrqPending = 0U;

/* -----------------------------------------------------------------------
 * Bus-Off ソフトウェア補完カウンタ
 *
 * 一次検出: Can_MainFunction_BusOff() が EFLG.TXBO ビット（getError()）をポーリング
 *           → AUTOSAR SWS_Can 準拠、ハードウェア Bus-Off 到達時に確実に検出
 *
 * 二次検出（本カウンタ）: 連続 TX 失敗回数でソフトウェア的に Bus-Off を判断
 *   → mcp_can の sendMsgBuf() がタイムアウト/TXERR 早期リターンにより
 *     TEC が 256（Bus-Off 閾値）に到達する前に TX リトライを止めるため、
 *     TXBO=1 が発生しない MCP2515 + mcp_can 環境向けの補完。
 * ----------------------------------------------------------------------- */
static uint8 Can_TxErrCount = 0U;
#define CAN_BUSOFF_TX_ERR_THRESHOLD  5U

/** Can_DisableControllerInterrupts()/Can_EnableControllerInterrupts() の
 *  ネストカウンタ（[SWS_Can_00202]: N 回 Disable したら N 回 Enable するまで
 *  実際には再有効化しない）。0 の間だけ実際に Can_Hw_DisableRxIsr() を呼び、
 *  0 に戻った瞬間だけ Can_Hw_EnableRxIsr() を呼ぶ。 */
static uint8 Can_InterruptDisableNestCount = 0U;

/* -----------------------------------------------------------------------
 * TX 確認 (CanIf_TxConfirmation) 保留キュー
 *
 * Can_Write() は送信成功時に swPduHandle をこのキューへ積むだけで即座に
 * 返り、実際の CanIf_TxConfirmation() 呼び出しは Can_MainFunction_Write()
 * まで遅延させる（ファイル冒頭のコメント参照）。
 * サイズは CanIf の TX PDU 数 (CANIF_TX_PDU_COUNT=4) に合わせる。
 * Os_SchedulerStep() は 1 回のスケジューラパスにつき各タスクを高々 1 回しか
 * 呼ばないため、Can_MainFunction_Write() が次に呼ばれるまでに積まれる
 * 保留件数はこの数を超えない想定。万一超えた場合は最も古い保留を
 * 上書きせず破棄し、DET へエラーを報告する（沈黙した取りこぼしを避ける）。
 * ----------------------------------------------------------------------- */
#define CAN_TX_CONF_QUEUE_SIZE  4U

static PduIdType Can_TxConfQueue[CAN_TX_CONF_QUEUE_SIZE];
static uint8     Can_TxConfHead = 0U;  /**< 次に取り出すエントリの index */
static uint8     Can_TxConfTail = 0U;  /**< 次に積むエントリの index     */
static uint8     Can_TxConfLen  = 0U;  /**< キュー内の有効エントリ数     */


/**
 * \brief   CAN ドライバを初期化する。
 *
 * \details MCP2515 ハードウェアを初期化し、受信フィルタ・マスクを
 *          すべて設定したうえでコントローラを CAN_CS_STOPPED 状態に
 *          移行する。初期化に失敗した場合は無限ループで停止する。
 *
 * \param[in]  Config  CAN ドライバ設定構造体へのポインタ。
 *                     NULL 禁止。
 *
 * \pre        SPI ペリフェラルがこの呼び出しより前に初期化済みであること。
 * \note       他のすべての Can_* API より先に、システム起動時に
 *             1 回だけ呼び出すこと。
 *
 * \AUTOSARReq     {SWS_Can_00246}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_Init(const Can_ConfigType* Config)
{
    DET_LOGI(TAG, "Init...");

    if (Config == NULL)
    {
        DET_LOGE(TAG, "Init: NULL Config");
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_INIT, CAN_E_PARAM_POINTER);
        return;
    }

    Can_ConfigPtr = Config;
    Can_TxConfHead = 0U;
    Can_TxConfTail = 0U;
    Can_TxConfLen  = 0U;
    Can_InterruptDisableNestCount = 0U;

    if (Can_Hw_Init(Config->csPin, Config->baudrate, Config->crystalFreq) != CAN_HW_OK)
    {
        DET_LOGE(TAG, "Init FAIL");
        while (1)
            ;
    }

    DET_LOGI(TAG, "Init ok");

    Can_Hw_InitMask(0, 0, Config->filter.mask << 16);
    Can_Hw_InitFilter(0, 0, Config->filter.filterId << 16);
    Can_Hw_InitFilter(1, 0, Config->filter.filterId << 16);
    Can_Hw_InitMask(1, 0, Config->filter.mask << 16);
    Can_Hw_InitFilter(2, 0, Config->filter.filterId << 16);
    Can_Hw_InitFilter(3, 0, Config->filter.filterId << 16);
    Can_Hw_InitFilter(4, 0, Config->filter.filterId << 16);
    Can_Hw_InitFilter(5, 0, Config->filter.filterId << 16);

    Can_Hw_SetMode(CAN_HW_MODE_LISTEN_ONLY);
    CanState = CAN_CS_STOPPED;

    /* Can_ConfigPtr 設定・CanState 初期値確定後に登録する。
     * これより前に INT ピンが立ち下がっても Can_Isr() は Can_ConfigPtr==NULL
     * で即座に return するため安全だが、登録自体をここまで遅らせることで
     * 「ISR が有効な時点では Can モジュールは必ず初期化済み」を保証する。 */
    Can_Hw_AttachRxIsr(Config->intPin, Can_Isr);
}

/**
 * \brief   コントローラを受信専用モード（Listen-Only）へ遷移させる。
 *
 * \details CAN_T_STOP（CAN_CS_STARTED → CAN_CS_STOPPED）と CAN_T_WAKEUP
 *          （CAN_CS_SLEEP → CAN_CS_STOPPED）は遷移元状態の妥当性チェックが
 *          異なる（Can_SetControllerMode() 参照）が、実際に適用する HW モード
 *          と CanState は同一のため、その部分だけを共通化する。
 */
static void Can_EnterListenOnly(void)
{
    DET_LOGT(TAG, "called");
    Can_Hw_SetMode(CAN_HW_MODE_LISTEN_ONLY);
    CanState = CAN_CS_STOPPED;
}

/**
 * \brief   CAN コントローラの状態遷移を要求する。
 *
 * \details AUTOSAR の状態遷移を対応する MCP2515 動作モードへ
 *          マッピングする。標準の 4 遷移に加え、本プロジェクト固有の
 *          協調スリープ（CanSM.c 参照）のための 5 番目の遷移を持つ。
 *          - CAN_T_START  : CAN_CS_STOPPED → CAN_CS_STARTED (通常モード)。
 *                           CAN_CS_STARTED から再度呼ばれた場合は冪等な
 *                           no-op として許可する（CanSM がボランタリ
 *                           スリープへの移行中に FULL_COM 要求を取り消す
 *                           ケースで実際に発生する。CanSM_RequestComMode()
 *                           参照）。
 *          - CAN_T_STOP   : CAN_CS_STARTED → CAN_CS_STOPPED (受信専用モード)
 *          - CAN_T_SLEEP  : CAN_CS_STOPPED → CAN_CS_SLEEP   (スリープモード)。
 *                           CAN_CS_STARTED からの直接遷移も許可する
 *                           （Nm 協調スリープ: ComM は Nm が実際に Bus-Sleep
 *                           Mode へ到達するまで CanSM へ NO_COM を伝えず
 *                           コントローラを稼働させ続け、到達した瞬間に
 *                           CanSM_RequestComMode(NO_COM) 経由で CAN_T_STOP を
 *                           経由せず直接 SLEEP させる。ComM.c/CanSM.c 参照。
 *                           実 AUTOSAR の標準遷移図にはない本プロジェクト
 *                           固有の拡張）。
 *          - CAN_T_WAKEUP : CAN_CS_SLEEP   → CAN_CS_STOPPED (受信専用モード)
 *
 *          上記のとおり CAN_T_START/CAN_T_STOP/CAN_T_SLEEP は複数の
 *          遷移元状態を許容する設計だが、CAN_CS_SLEEP からの
 *          CAN_T_START/CAN_T_STOP だけは、まず CAN_T_WAKEUP で
 *          CAN_CS_STOPPED へ戻らないと到達できない状態であり、
 *          どの呼び出し元もこの組み合わせを使わない。同様に CAN_T_WAKEUP
 *          は CAN_CS_SLEEP 以外から呼ぶ意味がない。呼び出し元の実装ミスを
 *          検出できるよう、この 2 点のみ状態を検証する（SWS_Can_00200/
 *          00409-00412。2026-08 のスペック監査で「一切検証していない」
 *          ことが判明したが、標準 4 遷移図をそのまま強制すると上記の
 *          協調スリープ／ボランタリスリープ取り消しという実際に使われている
 *          機能を壊すため、検証対象は安全に追加できる範囲に絞った）。
 *
 * \param[in]  Controller  CAN コントローラのインデックス。
 *                         本実装はコントローラ 0 のみ対応。
 *                         0 以外を指定すると CAN_NOT_OK を返す。
 * \param[in]  Transition  要求する状態遷移 (Can_StateTransitionType)。
 *
 * \retval  CAN_OK      遷移が正常に適用された。
 * \retval  CAN_NOT_OK  Controller が無効、未対応の Transition 値、または
 *                       CAN_CS_SLEEP 中の CAN_T_START/CAN_T_STOP、
 *                       CAN_CS_SLEEP 以外での CAN_T_WAKEUP。
 *
 * \AUTOSARReq     {SWS_Can_00017, SWS_Can_00200, SWS_Can_00230}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Can_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_SET_CONTROLLER_MODE, CAN_E_UNINIT);
        return CAN_NOT_OK;
    }

    if (Controller != 0U)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_SET_CONTROLLER_MODE, CAN_E_PARAM_CONTROLLER);
        return CAN_NOT_OK;
    }

    switch (Transition)
    {
    case CAN_T_START:
        if (CanState == CAN_CS_SLEEP)
        {
            DET_LOGE(TAG, "SetControllerMode E: T_START invalid from SLEEP (WAKEUP required first)");
            Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_SET_CONTROLLER_MODE, CAN_E_TRANSITION);
            return CAN_NOT_OK;
        }
        Can_Hw_SetMode(CAN_HW_MODE_NORMAL);
        //Can_Hw_SetMode(CAN_HW_MODE_LOOPBACK);  // ← 単体テスト用（通常はコメントアウト）
        CanState = CAN_CS_STARTED;
        break;
    case CAN_T_STOP:    /* CAN_CS_STARTED → CAN_CS_STOPPED */
        if (CanState == CAN_CS_SLEEP)
        {
            DET_LOGE(TAG, "SetControllerMode E: T_STOP invalid from SLEEP (WAKEUP required first)");
            Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_SET_CONTROLLER_MODE, CAN_E_TRANSITION);
            return CAN_NOT_OK;
        }
        Can_EnterListenOnly();
        break;
    case CAN_T_WAKEUP:  /* CAN_CS_SLEEP → CAN_CS_STOPPED（同じ受信専用モードへの遷移） */
        if (CanState != CAN_CS_SLEEP)
        {
            DET_LOGE(TAG, "SetControllerMode E: T_WAKEUP invalid from state=%u (not SLEEP)",
                     (unsigned)CanState);
            Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_SET_CONTROLLER_MODE, CAN_E_TRANSITION);
            return CAN_NOT_OK;
        }
        Can_EnterListenOnly();
        break;
    case CAN_T_SLEEP:
        Can_Hw_SetMode(CAN_HW_MODE_SLEEP);
        CanState = CAN_CS_SLEEP;
        break;
    default:
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_SET_CONTROLLER_MODE, CAN_E_TRANSITION);
        return CAN_NOT_OK;
    }

    return CAN_OK;
}

/**
 * \brief   指定 CAN コントローラの割り込みを全て無効化する。
 *
 * \details 本プロジェクトの CAN 受信は MCP2515 の INT ピン割り込み
 *          （`Can_Hw_AttachRxIsr()`）で駆動するが、`Can_MainFunction_Read()`
 *          自体は割り込みの有無に関わらず毎周期ポーリングする二重化設計
 *          （ファイル冒頭コメント参照）のため、本関数は MCU 側の割り込み
 *          （`detachInterrupt()`）のみを切り離す簡略実装とする。MCP2515
 *          自体はバス活動の受信を継続するため、切り離し中でも次回の
 *          `Can_MainFunction_Read()` で正しく処理される。
 *
 * \param[in]  Controller  CAN コントローラのインデックス。
 *                         本実装はコントローラ 0 のみ対応。
 *
 * \AUTOSARReq     {SWS_Can_00231, SWS_Can_00049, SWS_Can_00202, SWS_Can_00205, SWS_Can_00206}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_DisableControllerInterrupts(uint8 Controller)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_DISABLE_CONTROLLER_INTERRUPTS, CAN_E_UNINIT);
        return;
    }

    if (Controller != 0U)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_DISABLE_CONTROLLER_INTERRUPTS, CAN_E_PARAM_CONTROLLER);
        return;
    }

    /* [SWS_Can_00202]: N 回 Disable したら N 回 Enable するまで実際には
     * 再有効化しない。最初の1回だけ実際に切り離す。 */
    if (Can_InterruptDisableNestCount == 0U)
        (void)Can_Hw_DisableRxIsr();
    Can_InterruptDisableNestCount++;
}

/**
 * \brief   Can_DisableControllerInterrupts() で無効化した割り込みを再有効化する。
 *
 * \details [SWS_Can_00202] に従い、ネストカウンタが 0 に戻った時だけ実際に
 *          再登録する。対応する Disable 呼び出しより多く呼ばれた分は無視する。
 *
 * \param[in]  Controller  CAN コントローラのインデックス。
 *                         本実装はコントローラ 0 のみ対応。
 *
 * \AUTOSARReq     {SWS_Can_00232, SWS_Can_00202}
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_EnableControllerInterrupts(uint8 Controller)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_ENABLE_CONTROLLER_INTERRUPTS, CAN_E_UNINIT);
        return;
    }

    if (Controller != 0U)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_ENABLE_CONTROLLER_INTERRUPTS, CAN_E_PARAM_CONTROLLER);
        return;
    }

    if (Can_InterruptDisableNestCount == 0U)
        return;  /* 対応する Disable より多く呼ばれた分は無視する */

    Can_InterruptDisableNestCount--;
    if (Can_InterruptDisableNestCount == 0U)
        (void)Can_Hw_EnableRxIsr();
}

/**
 * \brief   CAN コントローラのエラー状態 (Active/Passive/Bus-Off) を取得する。
 *
 * \details [SWS_CANIF_91001] の CanIf_GetControllerErrorState() が「対応する
 *          CAN ドライバのサービスを呼ぶ」と規定する、その CAN ドライバ側
 *          サービスに相当する。実 AUTOSAR SWS_Can 4.3.1 は本関数に相当する
 *          Service を規定していない（CanIf 側 API のみが定義されている）ため
 *          AUTOSAR 非標準の拡張だが、CanIf から呼べる実体が必要なため用意する。
 *          MCP2515 の EFLG レジスタ（Bus-Off/TX Error-Passive ビット）から
 *          導出する（Can_Hw_GetErrorState() 参照）。
 *
 * \param[in]   Controller     対象コントローラ ID (0 固定)。
 * \param[out]  ErrorStatePtr  エラー状態の格納先。NULL 禁止。
 *
 * \retval  E_OK      ErrorStatePtr へ格納した。
 * \retval  E_NOT_OK  未初期化、Controller が範囲外、または ErrorStatePtr が NULL。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Can_GetControllerErrorState(uint8 Controller, Can_ErrorStateType* ErrorStatePtr)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_GET_CONTROLLER_ERROR_STATE, CAN_E_UNINIT);
        return E_NOT_OK;
    }

    if (Controller != 0U)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_GET_CONTROLLER_ERROR_STATE, CAN_E_PARAM_CONTROLLER);
        return E_NOT_OK;
    }

    if (ErrorStatePtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_GET_CONTROLLER_ERROR_STATE, CAN_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    uint8_t rawState;
    if (Can_Hw_GetErrorState(&rawState) != CAN_HW_OK)
        return E_NOT_OK;

    *ErrorStatePtr = (Can_ErrorStateType)rawState;
    return E_OK;
}

/**
 * \brief   CAN フレームの送信を要求する。
 *
 * \details PDU を MCP2515 の送信バッファに渡し、送信成功時は swPduHandle を
 *          TX 確認保留キューへ積むだけで即座に返る。実際の
 *          CanIf_TxConfirmation() 呼び出しは Can_MainFunction_Write() まで
 *          遅延させる（ファイル冒頭のコメント参照。呼び出し元と同一スタック
 *          フレーム内で上位層へ通知すると、将来 TxConfirmation 側に処理が
 *          足された際に Can_Write() を再帰呼び出しする経路になりうるため）。
 *          コントローラが CAN_CS_STARTED 状態でない場合は
 *          即座に CAN_NOT_OK を返す。PduInfo->length が 8 バイトを超える場合も
 *          同様に拒否する（[SWS_Can_00218]、CAN_E_PARAM_DLC）。
 *
 * \param[in]  Hth      ハードウェア送信ハンドル。MCP2515 が TX バッファを
 *                      自動選択するため、この実装では使用しない。
 * \param[in]  PduInfo  送信する PDU へのポインタ。NULL 禁止。
 *                      使用メンバー: id, length, sdu, swPduHandle。
 *                      length は 8 バイト以下であること。
 *
 * \retval  CAN_OK      フレームが受理され、送信に成功した。
 * \retval  CAN_NOT_OK  コントローラ未起動、length が 8 バイト超過、
 *                      または MCP2515 送信失敗。
 * \retval  CAN_BUSY    この実装では返さない（MCP2515 が自動リトライ）。
 *
 * \AUTOSARReq     {SWS_Can_00016}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant (different Hth)}
 * \Synchronicity  {Synchronous}
 */
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    (void)Hth;

    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_WRITE, CAN_E_UNINIT);
        return CAN_NOT_OK;
    }

    if (PduInfo == NULL || PduInfo->sdu == NULL)
    {
        DET_LOGE(TAG, "Write: NULL PduInfo/sdu");
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_WRITE, CAN_E_PARAM_POINTER);
        return CAN_NOT_OK;
    }

    if (PduInfo->length > CAN_FRAME_MAX_DLC)
    {
        /* [SWS_Can_00218]: length が 8 バイトを超える場合は CAN_E_PARAM_DLC を
         * 報告して拒否する。上位層（CanIf_Transmit）は設定 DLC（常に 8 以下）
         * で既に SduLength を検証しているため、現状この分岐は到達しないはず
         * だが、PduInfo->sdu は呼び出し元が length 分の実体を持つ保証がなく、
         * ここでチェックせず Can_Hw_Send() まで渡すと sdu バッファのオーバー
         * リードになりうる（2026-08 のスペック監査で指摘、安全網として追加）。 */
        DET_LOGE(TAG, "Write E: length=%u exceeds %u bytes",
                 (unsigned)PduInfo->length, (unsigned)CAN_FRAME_MAX_DLC);
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_WRITE, CAN_E_PARAM_DLC);
        return CAN_NOT_OK;
    }

    if (CanState != CAN_CS_STARTED)
        return CAN_NOT_OK;

    if (Can_Hw_Send(PduInfo->id, PduInfo->length, PduInfo->sdu) != CAN_HW_OK)
    {
        /* 一次検出（EFLG.TXBO）は Can_MainFunction_BusOff() のポーリングが担う。
         * 二次検出: mcp_can 環境で TXBO=1 が発生しない場合の補完。 */
        Can_TxErrCount++;
        if (Can_TxErrCount >= CAN_BUSOFF_TX_ERR_THRESHOLD)
        {
            Can_TxErrCount = 0U;
            DET_LOGW(TAG, "SW BusOff fallback: %u consecutive TX failures",
                     (unsigned)CAN_BUSOFF_TX_ERR_THRESHOLD);
            CanIf_ControllerBusOff(0U);
        }
        return CAN_NOT_OK;
    }

    Can_TxErrCount = 0U;

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), PduInfo->sdu, PduInfo->length);
    DET_LOGI(TAG, "TX id=0x%lX [%s]", (unsigned long)PduInfo->id, hexbuf);

    if (Can_TxConfLen >= CAN_TX_CONF_QUEUE_SIZE)
    {
        /* 万一キューが満杯（想定超過のバースト）の場合は、この確認通知だけを
         * 諦める。物理送信自体は既に成功しているため PDU 自体は失われない。 */
        DET_LOGE(TAG, "TxConf queue full, dropping confirmation for pdu=%u",
                 (unsigned)PduInfo->swPduHandle);
    }
    else
    {
        Can_TxConfQueue[Can_TxConfTail] = PduInfo->swPduHandle;
        Can_TxConfTail = (uint8)((Can_TxConfTail + 1U) % CAN_TX_CONF_QUEUE_SIZE);
        Can_TxConfLen++;
    }

    return CAN_OK;
}

/**
 * \brief   保留中の TX 確認 (CanIf_TxConfirmation) をまとめて処理する。
 *
 * \details Can_Write() が積んだ TX 確認保留キューを全件ドレインし、
 *          投入順に CanIf_TxConfirmation() を呼び出す (SWS_Can_00016:
 *          ポーリングモードでは Can_MainFunction_Write() の中から呼ぶ)。
 *          CanIf_TxConfirmation() 自体はソフトウェア的なコールバック転送のみで
 *          ハードウェアをブロックしないため、NvM の非同期書き込みキューとは
 *          異なり 1 回の呼び出しで全件処理してよい。
 *
 * \pre        Can_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Can_00016}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_Write(void)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_MAIN_FUNCTION_WRITE, CAN_E_UNINIT);
        return;
    }

    while (Can_TxConfLen > 0U)
    {
        PduIdType pduId = Can_TxConfQueue[Can_TxConfHead];
        Can_TxConfHead = (uint8)((Can_TxConfHead + 1U) % CAN_TX_CONF_QUEUE_SIZE);
        Can_TxConfLen--;

        CanIf_TxConfirmation(pduId);
    }
}

/**
 * \brief   MCP2515 INT ピンの立ち下がりエッジで起動する真のハードウェア割り込み。
 *
 * \details Can_Hw_AttachRxIsr()（Can_Init 内）により attachInterrupt() で
 *          登録され、Os スケジューラの周期とは無関係に INT ピンが立ち下がった
 *          瞬間に起動する。SPI 通信・Serial ログ・CanIf 呼び出しは一切行わず、
 *          ペンディングフラグを立てるだけに留める（理由はファイル冒頭の
 *          コメントを参照）。実際の処理は Can_MainFunction_Read() /
 *          Can_MainFunction_Wakeup()（メインループのタスク）に委譲する。
 *
 *          CAN_CS_SLEEP 中は Can_WakeupIrqPending、それ以外は
 *          Can_RxIrqPending をセットする。MCP2515 はスリープ中にバス活動を
 *          検知すると自律的に Listen-Only へ遷移し INT ピンをアサートする
 *          （Can_Hw_SetMode() の CAN_HW_MODE_SLEEP 参照）。この時点では
 *          ウェイクアップ要因となったフレーム自体の受信は保証されない
 *          （モード遷移中に取りこぼされることがある）ため、ここでは読み出さず
 *          「目覚めた」ことだけをフラグで伝える。実際のフレーム受信は
 *          CanSM_ControllerWakeup() が CAN_CS_STARTED へ遷移させた後、
 *          以降の Can_MainFunction_Read() 呼び出しで通常どおり処理される。
 *
 * \pre        Can_Init() が正常に完了していること。
 * \note       AUTOSAR 標準外の API。INT ピン番号は Can_ConfigType::intPin
 *             から取得し、Can_Hw_AttachRxIsr() へ渡す。
 * \note       SWS_Can_00271 が規定する通知先（EcuM_CheckWakeup()）との相違点は
 *             Can_MainFunction_Wakeup() の doc コメントを参照。
 *
 * \AUTOSARReq     {SWS_Can_00396, SWS_Can_00271}
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
static void Can_Isr(void)
{
    if (Can_ConfigPtr == NULL)
        return;

    if (CanState == CAN_CS_SLEEP)
    {
        Can_WakeupIrqPending = 1U;
        return;
    }

    Can_RxIrqPending = 1U;
}

/**
 * \brief   受信フレームをポーリングでドレインする。
 *
 * \details 実機検証で、割り込み (Can_Isr()) が実際に発火するかどうかを
 *          テスト条件によらず保証できないことが分かった（詳細はファイル
 *          冒頭のコメントを参照）。そのため正しさを Can_RxIrqPending
 *          フラグの有無に依存させず、Can_Hw_CheckReceive() が NOT_OK を
 *          返すまで毎回無条件にドレインする設計にしている（フラグが
 *          立っていなくても受信を取りこぼさない）。`Can_Hw_CheckReceive()`
 *          は MCP2515 のステータスレジスタを SPI 経由で読むだけで INT
 *          ピンの実際の状態には依存しないため、割り込みの成否に関係なく
 *          正しく動作する。割り込みが発火した場合は Can_RxIrqPending が
 *          セットされるが、本関数の動作はそれに左右されない。
 *
 *          CAN_CS_SLEEP 中は処理しない（ウェイクアップ検証前のフレームは
 *          CAN_CS_STOPPED = Listen-Only 状態で届くため、対象外なのは
 *          SLEEP のみでよい）。
 *
 * \pre        Can_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Can_00108}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_Read(void)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_MAIN_FUNCTION_READ, CAN_E_UNINIT);
        return;
    }

    if (CanState == CAN_CS_SLEEP)
        return;

    SchM_Enter_Can_IRQFLAG_EXCLUSIVE_AREA();
    Can_RxIrqPending = 0U;
    SchM_Exit_Can_IRQFLAG_EXCLUSIVE_AREA();

    while (Can_Hw_CheckReceive() == CAN_HW_OK)
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Can_Hw_Read(&rxId, &len, buf) != CAN_HW_OK)
            break;

        Can_HwType  mailbox = { .CanId = rxId, .Hoh = 0U, .ControllerId = 0U };
        PduInfoType pduInfo = { .SduDataPtr = buf, .SduLength = (PduLengthType)len };
        CanIf_RxIndication(&mailbox, &pduInfo);
    }
}

/**
 * \brief   ウェイクアップを検出し、上位層へ通知する。
 *
 * \details Can_Isr() が CAN_CS_SLEEP 中にセットする Can_WakeupIrqPending を
 *          確認するが、実機検証で attachInterrupt() が発火しないことが
 *          判明した（Can_MainFunction_Read() のコメント参照）ため、
 *          INT ピンの直接ポーリング（`Can_Hw_IsWakeupPending()`、旧実装と
 *          同じ digitalRead() 方式を Can_Hw 層へ委譲したもの）も
 *          フォールバックとして併用する。いずれか一方でも検出できれば
 *          CanIf_ControllerWakeup() で上位層へ通知する。
 *
 *          BswM は SHUTDOWN 中も本タスクだけは無効化しない
 *          （WdgM_TriggerHwWatchdog・Can_MainFunction_Read・CanSM_MainFunction・
 *          NvM_MainFunction と並ぶ「SHUTDOWN 中も動き続ける必要がある」タスク。
 *          詳細は BswM_Cfg.h の BSWM_TASK_MASK_SHUTDOWN を参照）。実機の割り込みが
 *          CPU のスリープからの復帰トリガそのものであるのと同じ理由で、この
 *          タスクもウェイクアップ検出のためだけには動き続ける必要がある。
 *
 *          仕様との既知の相違点: SWS_Can_00271 が規定する通知先は本来
 *          EcuM_CheckWakeup() （ECU State Manager を直接呼ぶ）だが、本プロジェクトは
 *          EcuM のウェイクアップソース管理を実装しておらず、CanIf 経由で
 *          CanSM_ControllerWakeup() （AUTOSAR EcuM Wakeup Validation Protocol 相当を
 *          CanSM 側で模した検証シーケンス、CanSM.c 参照）へ委譲する設計を
 *          一貫して採る。00271 の引用は「ISR または Can_MainFunction_Wakeup の
 *          いずれかの文脈で通知する」というタイミング要件の部分のみ有効で、
 *          通知先の関数名までは仕様どおりではない。
 *
 * \pre        Can_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Can_00112, SWS_Can_00271}
 * \ServiceID      {0x0A}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_Wakeup(void)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_MAIN_FUNCTION_WAKEUP, CAN_E_UNINIT);
        return;
    }

    if (CanState != CAN_CS_SLEEP)
        return;

    uint8 pending;
    SchM_Enter_Can_IRQFLAG_EXCLUSIVE_AREA();
    pending = Can_WakeupIrqPending;
    Can_WakeupIrqPending = 0U;
    SchM_Exit_Can_IRQFLAG_EXCLUSIVE_AREA();

    if (!pending && Can_Hw_IsWakeupPending() == CAN_HW_OK)
        pending = 1U;

    if (pending)
    {
        DET_LOGI(TAG, "Wakeup detected (INT asserted during SLEEP)");
        CanIf_ControllerWakeup(0U);
    }
}

/**
 * \brief   Bus-Off イベントのポーリングを行う。
 *
 * \details MCP2515 の ERRIE がデフォルト無効で Bus-Off 発生時に INT を
 *          アサートしないため、EFLG.TXBO ビットを直接ポーリングすることで
 *          確実に検出する（割り込みではなくポーリングに拠るイベントのため、
 *          AUTOSAR も本関数を Can_MainFunction_xxx の 1 つとして定義している）。
 *
 * \pre        Can_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Can_00109}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_BusOff(void)
{
    DET_LOGT(TAG, "called");

    if (Can_ConfigPtr == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_MAIN_FUNCTION_BUSOFF, CAN_E_UNINIT);
        return;
    }

    if (CanState == CAN_CS_STARTED && Can_Hw_IsBusOff() == CAN_HW_OK)
    {
        CanIf_ControllerBusOff(0U);
    }
}

/**
 * \brief   CAN ドライバのバージョン情報を取得する。
 *
 * \details Can_Init と並び、未初期化時でも CAN_E_UNINIT を報告しない例外 API
 *          （他 BSW モジュールと共通の慣例）のため、初期化状態は確認せず
 *          NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");

    if (versioninfo == NULL)
    {
        Det_ReportError(CAN_MODULE_ID, 0U, CAN_API_ID_GET_VERSION_INFO, CAN_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = CAN_VENDOR_ID;
    versioninfo->moduleID         = CAN_MODULE_ID;
    versioninfo->sw_major_version = CAN_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = CAN_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = CAN_SW_PATCH_VERSION;
}

#ifdef CAN_UNIT_TEST
Can_ControllerStateType Can_Test_GetControllerState(void)
{
    return CanState;
}
void Can_Test_SetControllerState(Can_ControllerStateType state)
{
    CanState = state;
}
void Can_Test_SetConfigPtr(Can_ConfigType* config)
{
    Can_ConfigPtr = config;
}
uint8 Can_Test_GetTxErrCount(void)
{
    return Can_TxErrCount;
}
void Can_Test_ResetTxErrCount(void)
{
    Can_TxErrCount = 0U;
}
#endif

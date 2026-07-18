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

/* Arduino wiring.c（C リンケージ）で定義。ウェイクアップ検出の
 * フォールバックポーリング（Can_MainFunction_Wakeup）専用に使用する。 */
extern int digitalRead(uint8 pin);


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

    Can_ConfigPtr = Config;
    Can_TxConfHead = 0U;
    Can_TxConfTail = 0U;
    Can_TxConfLen  = 0U;

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
 * \brief   CAN コントローラの状態遷移を要求する。
 *
 * \details AUTOSAR の状態遷移を対応する MCP2515 動作モードへ
 *          マッピングする。
 *          - CAN_T_START  : CAN_CS_STOPPED → CAN_CS_STARTED (通常モード)
 *          - CAN_T_STOP   : CAN_CS_STARTED → CAN_CS_STOPPED (受信専用モード)
 *          - CAN_T_SLEEP  : CAN_CS_STOPPED → CAN_CS_SLEEP   (スリープモード)
 *          - CAN_T_WAKEUP : CAN_CS_SLEEP   → CAN_CS_STOPPED (受信専用モード)
 *
 * \param[in]  Controller  CAN コントローラのインデックス。
 *                         本実装はコントローラ 0 のみ対応。
 *                         0 以外を指定すると CAN_NOT_OK を返す。
 * \param[in]  Transition  要求する状態遷移 (Can_StateTransitionType)。
 *
 * \retval  CAN_OK      遷移が正常に適用された。
 * \retval  CAN_NOT_OK  Controller が無効、または未対応の Transition 値。
 *
 * \AUTOSARReq     {SWS_Can_00017, SWS_Can_00230}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Can_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition)
{
    if (Controller != 0U)
        return CAN_NOT_OK;

    switch (Transition)
    {
    case CAN_T_START:
        Can_Hw_SetMode(CAN_HW_MODE_NORMAL);
        //Can_Hw_SetMode(CAN_HW_MODE_LOOPBACK);  // ← 単体テスト用（通常はコメントアウト）
        CanState = CAN_CS_STARTED;
        break;
    case CAN_T_STOP:
        Can_Hw_SetMode(CAN_HW_MODE_LISTEN_ONLY);
        CanState = CAN_CS_STOPPED;
        break;
    case CAN_T_SLEEP:
        Can_Hw_SetMode(CAN_HW_MODE_SLEEP);
        CanState = CAN_CS_SLEEP;
        break;
    case CAN_T_WAKEUP:
        Can_Hw_SetMode(CAN_HW_MODE_LISTEN_ONLY);
        CanState = CAN_CS_STOPPED;
        break;
    default:
        return CAN_NOT_OK;
    }

    return CAN_OK;
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
 *          即座に CAN_NOT_OK を返す。
 *
 * \param[in]  Hth      ハードウェア送信ハンドル。MCP2515 が TX バッファを
 *                      自動選択するため、この実装では使用しない。
 * \param[in]  PduInfo  送信する PDU へのポインタ。NULL 禁止。
 *                      使用メンバー: id, length, sdu, swPduHandle。
 *
 * \retval  CAN_OK      フレームが受理され、送信に成功した。
 * \retval  CAN_NOT_OK  コントローラ未起動、または MCP2515 送信失敗。
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
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_Write(void)
{
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
 *
 * \AUTOSARReq     {SWS_Can_00396, SWS_Can_00271}
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void Can_Isr(void)
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
    if (Can_ConfigPtr == NULL || CanState == CAN_CS_SLEEP)
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
 *          INT ピンの直接ポーリング（`digitalRead()`、旧実装と同じ方式）も
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
 * \pre        Can_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Can_00112, SWS_Can_00271}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_Wakeup(void)
{
    if (Can_ConfigPtr == NULL || CanState != CAN_CS_SLEEP)
        return;

    uint8 pending;
    SchM_Enter_Can_IRQFLAG_EXCLUSIVE_AREA();
    pending = Can_WakeupIrqPending;
    Can_WakeupIrqPending = 0U;
    SchM_Exit_Can_IRQFLAG_EXCLUSIVE_AREA();

    if (!pending && !digitalRead(Can_ConfigPtr->intPin))
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
 * \ServiceID      {0x0B}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_BusOff(void)
{
    if (CanState == CAN_CS_STARTED && Can_Hw_IsBusOff() == CAN_HW_OK)
    {
        CanIf_ControllerBusOff(0U);
    }
}

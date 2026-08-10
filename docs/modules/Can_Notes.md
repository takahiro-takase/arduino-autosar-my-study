# Can

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

MCP2515 の送受信・Bus-Off 検出・CAN バス活動によるウェイクアップ検出を担う
MCAL 最下層。HW を直接操作する唯一のモジュール。TX 確認の非同期化
（`Can_MainFunction_Write` によるポーリングモード準拠）、および RX の割り込み化
（`Can_Isr` によるハードウェア割り込み化と、割り込み非依存のポーリング二重化）を
中心に、以下で実装判断の背景をまとめます。

<a id="can-tx-async-confirm"></a>
## TX 確認の非同期化（`Can_MainFunction_Write`）

**なぜ非同期化したか**: AUTOSAR の仕様 [SWS_Can_00016] は、`CanIf_TxConfirmation()` を
「TX 割り込みハンドラから」または「ポーリングモードでは `Can_MainFunction_Write()` の
中から」呼ぶことを求めている。しかし当初の実装では、`Can_Write()` が送信成功直後、
呼び出し元と同一スタックフレーム内でそのまま `CanIf_TxConfirmation()` を呼んでいた。

```
（修正前）
Com_MainFunction() → Com_DoTransmit() → PduR_Transmit() → CanIf_Transmit() → Can_Write()
                                                                                → CanIf_TxConfirmation()
                                                                                  → PduR_CanIfTxConfirmation()
                                                                                    → Com_TxConfirmation()
（すべて 1 回の呼び出しチェーン内で完結）
```

これ自体は現状害がないが、将来 `Com_TxConfirmation()` の延長線上（あるいは他の
`TxConfirmFct`）に「送信失敗を検知したら即座に再送する」ような処理が足された場合、
その再送呼び出しがそのまま `Can_Write()` の再帰呼び出しになってしまう。NvM の
非同期書き込みジョブキュー（[DEVLOG参照](../DEVLOG.md#nvm-非同期書き込みジョブキューへの変更経緯)）
と同じ「今は実害がないが将来の変更で踏み抜きやすいスタック深化の地雷」を避ける
考え方で、この結合を断ち切った。

**設計**:

```
Can_Write(Hth, PduInfo):
  Can_Hw_Send() が成功したら、PduInfo->swPduHandle を TX 確認保留キューへ積むだけで
  即座に E_OK 相当（CAN_OK）を返す（CanIf_TxConfirmation() はまだ呼ばない）

Can_MainFunction_Write()（1ms 周期、Os_PBCfg.c Task 13）:
  保留キューが空になるまで、投入順に CanIf_TxConfirmation() を呼び出す
```

NvM の非同期ジョブキュー（1 呼び出し 1 バイトずつ）とは異なり、こちらは
`CanIf_TxConfirmation()` 自体がハードウェアをブロックしないソフトウェア的な
コールバック転送のみのため、1 回の `Can_MainFunction_Write()` 呼び出しで
保留分を全件処理してよい。

**動作への影響**: `CanIf_TxConfirmation()` の呼び出しタイミングが `Can_Write()` から
最大 1ms（Task 13 の周期）遅延するようになるが、`Com_TxConfirmation()`・
`CanTp_TxConfirmation()` のいずれも受け取った結果を使わない no-op のため、
体感できる動作変化はない（この経路は常に E_OK 固定でもある。詳細は
[`CanTp_Notes.md`](./CanTp_Notes.md) の N_As タイムアウトの説明を参照）。

<a id="can-rx-interrupt"></a>
## RX の割り込み化（`Can_Isr` / `Can_MainFunction_Read/BusOff/Wakeup`）

**なぜ割り込み化したか**: 従来 `Can_Isr()` は Os スケジューラから 1ms ごとにポーリング
呼び出しされる「疑似 ISR」で、INT ピンを `digitalRead()` で確認していた。これは
「割り込み」と名乗りながら実態はポーリングであり、AUTOSAR OS が本来持つ「タスクと
ISR は実際にプリエンプトし合う」という関係を体験できていなかった。また、SchM の
排他エリアマクロ（`SchM_Enter_Com_SIGNAL_EXCLUSIVE_AREA()` 等）も「協調スケジューリング
なので NOP でよい」という理由でずっと未使用のまま残っていた。

本変更で `Can_Hw_AttachRxIsr()`（`Can_Init()` 内）が `attachInterrupt()` で INT ピンの
立ち下がりエッジに `Can_Isr()` を真のハードウェア割り込みとして登録し、Os スケジューラの
周期とは無関係に即座に起動されるようにした。

**ISR を最小限に保つ設計判断**: 素直に実装するなら「ISR の中で `CanIf_RxIndication()` まで
呼んでしまう」のが最も単純だが、本実装ではあえてそうしていない。`Can_Isr()` は
ペンディングフラグ（`Can_RxIrqPending` / `Can_WakeupIrqPending`）を立てるだけに留め、
SPI 通信・Serial ログ・CanIf 呼び出しは一切行わない。理由は 2 つ:

1. **SPI バス排他**: MCP2515 は SPI 接続のため、CS ピン制御を伴う複数バイトの読み書きが
   1 トランザクションとして完結する必要がある。メインループ側の `Can_Write()`（TX、SPI
   経由）がトランザクション途中で割り込みにプリエンプトされ、割り込み側が同じ SPI バスへ
   別トランザクションを割り込ませると、双方が破壊されうる。ISR 側で SPI を使わなければ
   この競合はそもそも発生しない。
2. **処理時間の上限**: `CanIf_RxIndication()` から先は PduR/Com/CanTp/Dcm まで連鎖し、
   UDS SID 処理（RoutineControl 等）まで含まれ得る。これを割り込みハンドラの中で行うと、
   ISR の実行時間が事実上無制限になりかねない（本 README で繰り返し出てくる「同期呼び出し
   連鎖のスタック/ブロッキングリスク」と同種の問題）。

実際の SPI 読み出しと `CanIf_RxIndication()` 呼び出しは、ペンディングフラグを見てメイン
ループのタスクが行う（AUTOSAR `SWS_Can_00396`・`SWS_Can_00012` 参照:「呼び出しコンテキストが
ISR か `Can_MainFunction_Read` かは実装依存であり、コールバックはいずれの場合も ISR から
呼ばれたかのように実装してよい」）。これは TX 確認の非同期化（`Can_MainFunction_Write`）と
対になる設計で、CAN モジュールの RX/TX 双方が「イベントは即座に検知するが、重い処理は
専用タスクへ委譲する」という同じパターンに統一されたことになる。

**関数の分離**: 旧 `Can_Isr()` は「RX ポーリング」「Bus-Off ポーリング」「SLEEP 中の
ウェイクアップ検出」の 3 役を 1 つの関数にまとめていたが、AUTOSAR は元々これらを
独立した `Can_MainFunction_xxx` として定義している。これに合わせて分離した。

```
Can_Isr()                     ← 真の割り込み。フラグを立てるだけ
Can_MainFunction_Read()       ← Can_RxIrqPending をドレインし RX 処理 (SWS_Can_00108)
Can_MainFunction_BusOff()     ← EFLG.TXBO を毎回ポーリング (SWS_Can_00109、割り込み非依存)
Can_MainFunction_Wakeup()     ← Can_WakeupIrqPending をドレインしウェイクアップ通知 (SWS_Can_00112)
```

`Can_MainFunction_Read()` のドレインループはフラグではなく `Can_Hw_CheckReceive()` が
NOT_OK を返すまで継続する。MCP2515 の INT はレベル方式（未読フレームが残る限り
アサートされ続ける）ため、連続到着した 2 フレーム目には新たなエッジが立たないことが
あるが、フラグは「立った」ことだけを覚えていれば十分で、実際に何件処理するかは
ドレインループがハードウェアの状態から判断する。

**SchM が初めて意味を持つ**: `Can_Isr()`（割り込みコンテキスト）と
`Can_MainFunction_Read()`/`Can_MainFunction_Wakeup()`（メインループのタスク）は、
ペンディングフラグを介して実際に競合しうる関係になった。フラグの読み出しとクリアを
アトミックに行わないと、その間に割り込みが発生した場合にフラグのセットが失われ、
受信フレーム・ウェイクアップ通知を取りこぼす。これを防ぐため `SchM.h` に新しい排他エリア
`SchM_Enter/Exit_Can_IRQFLAG_EXCLUSIVE_AREA()` を追加し、実体を
`SchM_Hw_EnterExclusiveArea()`/`ExitExclusiveArea()`（`src/Hal/SchM_Hw.cpp`、
`noInterrupts()`/`interrupts()` を呼ぶだけ）とした。既存の `Rte_MIRROR`・`Com_SIGNAL`
排他エリアも同じ実体を指すように変更し、NOP のままだった `SchM.h` が実際に機能するように
なった（Com の RX/TX バッファ自体は現状 `Can_MainFunction_Read()` というメインループの
タスクからのみ触られる設計にしたため、まだ割り込みと競合しないが、Rte 側と同様
将来のための保険として Enter/Exit を残してある）。

> **意図的な二重化**: `Can_MainFunction_Read()`/`Can_MainFunction_Wakeup()` は
> 「割り込みが本当に発火するか」に正しさを依存させない設計にしている。
> - `Can_MainFunction_Read()` は `Can_RxIrqPending` の有無に関わらず、毎回
>   無条件に `Can_Hw_CheckReceive()`（SPI 経由のステータスレジスタ読み出し。
>   INT ピンの実際の状態には依存しない）でドレインする。
> - `Can_MainFunction_Wakeup()` は `Can_WakeupIrqPending` に加えて
>   `digitalRead(intPin)` の直接ポーリングも併用する（旧実装と同じ
>   フォールバック）。
>
> `Can_Isr()`・ペンディングフラグ・`SchM_Enter/Exit_Can_IRQFLAG_EXCLUSIVE_AREA()`
> の構造はそのまま残り、割り込みが発火すればより低遅延に反応できる
> 「ボーナス経路」として機能するが、たとえ割り込みが何らかの理由で発火
> しなくてもポーリング側だけで正しく動作する。単一の検出経路（割り込みのみ）に
> 正しさを委ねず、独立したポーリングでも動作を保証する設計にした経緯は
> [DEVLOG: Can RX 割り込み化の実機検証で得られた教訓](../DEVLOG.md#can-rx-割り込み化の実機検証で得られた教訓) を参照。

## Can_Hw（下位ドライバ実装）

MCP2515 / `mcp_can` C++ ラッパー（RX 割り込み登録 `Can_Hw_AttachRxIsr` を含む）。

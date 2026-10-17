# 単体テスト（ホスト上でのロジック検証）

> [README](../README.md) の「[テスト（動作確認）](../README.md#testing)」節から分離した詳細です。

実 HW（UNO R4）を使わず、Bsw モジュールのロジックだけをホスト PC 上で GoogleTest
により検証します。単一モジュールのテストも複数モジュールにまたがる関数
コールチェーンの検証も含め、`test/`（テスト本体）・`stub/`（差し替え）と `native_chain_tests` という
単一のテストバイナリに集約しています（2026-09、`[env:native]`/`[env:native_dcm]`/
`[env:native_wdgm]`/`[env:native_fim]` という PlatformIO の個別 env に分かれて
いた時期がありましたが、`--wrap` によるフォールトインジェクション
（`CMakeLists.txt` 冒頭コメント参照）を活用してすべて統合しました）。

ビルドは PlatformIO ではなく **CMake + clang++（llvm-mingw）** で行います
（2026-09-20、PlatformIO の `native` プラットフォームが CC/CXX を強制的に
gcc/g++ へ上書きし直してしまい clang++ へ確実に切り替えられないという制約が
あったため移行。`[env:uno_r4]` の実機ビルドは引き続き PlatformIO のまま）。

事前準備として、[llvm-mingw](https://github.com/mstorsjo/llvm-mingw) を導入し、
その `bin` ディレクトリを環境変数 `LLVM_MINGW_BIN` に設定しておく（絶対パスを
リポジトリへハードコードしていないのはマシンごとに設置先が異なるため）。
CMake は VSCode の CMake Tools 拡張などから導入できる。

```bash
# 事前準備（マシンごとに1回）
export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"

# ホスト上でビルド・実行（GoogleTest、実 HW 不要）
cmake --preset native-chain
cmake --build --preset native-chain
./build/native_chain/native_chain_tests.exe  # 単一モジュール検証(Can/Gpt/Dio/Port/Det/E2E等)＋
                                              # Tx/Rx処理コールチェーン(通常/E2E/デッドライン監視)＋
                                              # Dcm/Dem/WdgM/FiM 等すべて含む

# native-chain-coverage プリセット: 同じ対象を MC/DC 含む source-based coverage
# 計測付きでビルドする。使い方は tools/coverage/generate_coverage_report.sh 参照。

DET_LOG_VERBOSE=1 ./build/native_chain/native_chain_tests.exe  # TRACE ログ出力（PowerShell は $env:DET_LOG_VERBOSE = "1"）
```

> **Windows 環境固有の注意（MinGW-w64 のランタイム不整合）**:
> 一部の MinGW-w64 配布物（msvcrt ランタイム版）では、GoogleTest の
> death test 機構経由で `libmingw32.a` 内の UCRT 専用シンボル
> (`__imp_quick_exit`/`__imp__Exit`) が要求され、
> `undefined reference to __imp_quick_exit` 等でリンクに失敗することがある。
> `test/win_quick_exit_stub.cpp` はこの環境向けの回避コード
> （該当シンボルを `std::exit()` へ委譲する自前スタブで満たす）。
> UCRT ランタイム版の MinGW-w64 を使っている場合は本来不要で、
> `__imp_quick_exit`/`__imp__Exit` の多重定義エラーが出たら削除すること。

<a id="test-roles"></a>
## テストの役割分担（単体テストとコールチェーン）

2026-10-16、「モジュールの単体テストにも正常系（OK）を作る」方針とした。それまでは、OK の動作を
コールチェーンのテスト（`ComStack` など）が間接的に確かめ、モジュール単位のテストは NG
（DET 報告や戻り値の異常系）だけの場合が多かった。

| 種類 | 置き場所 | 検証すること |
|------|----------|--------------|
| 単体テスト | `test/Bsw/{Module}/{Module}_{Function}_test.cpp`（関数単位） | そのモジュールの**分岐を OK・NG の両側から網羅**する。判定の各条件が結果に効くこと（MC/DC）を目安にする。依存先は、実体のままにするか `--wrap` で記録・差し替えして、検証対象に集中する |
| コールチェーンのテスト | `test/CallChain/{Stack}Stack/` | **層をまたぐ結線と呼び出しの順序**を確かめる。各モジュールの内部分岐の網羅は目的にしない |

- 単体テストの OK は、そのモジュールの分岐を網羅するためのテストとする。チェーンのテストと同じ正常系を二重に持つ場合は、
  チェーン側を結線の確認に絞る（仕様変更のときに直す箇所を増やさないため）。
- 単体テストが落ちたときは、そのモジュールの不具合。チェーンのテストだけが落ちたときは、層間の結線か呼び出し順の不具合、と
  切り分けられる。
- 完全な単体（依存先をすべて偽物にする）にはしない。隣接モジュールは実体のままにし、必要なときだけ
  `--wrap` で観測・故障注入する現行の方式を続ける（スタブの保守を増やさないため）。
- 作り方の例: `KeyM`・`SecOC`・`EcuM`・`Os`・`Gpt` の OK 系テスト（2026-10 追加）。
  MC/DC の見方と演習は [mcdc_learning.md](mcdc_learning.md) を参照。

### 現状と優先順位（2026-10-16 時点）

OK 系が少ないモジュールから、MC/DC の未成立の条件が多いものを優先して足していく。

- OK が 0 件: Com、Csm、Crypto、CryIf、CanTp、E2EXf、Fee、MemIf、WdgIf
- OK が 1〜2 件: Dem、ComM、CanSM、PduR、Adc

足すたびに、`tools/coverage/generate_coverage_report.sh` で MC/DC の変化を確認する。

<a id="unit-test-chain"></a>
## コールチェーンのテスト（`native_chain_tests`）

<a id="unit-test-tx"></a>
### Tx 処理（Com → PduR → CanIf → Can の順）

[「Tx 処理」コールチェーン](can_stack.md#tx-processing)（`Com_SendSignal()` → …
→ `Com_MainFunctionTx()` → `PduR_ComTransmit()` → `CanIf_Transmit()` →
`Can_Write()`）を複数モジュールにわたって実体（Com.c/PduR.c/CanIf.c/Can.c）で
リンクし、そのまま検証するテストを `test/CallChain/ComStack/` に用意しています
（通常のシグナルは `ComStack_Signal_Tx_test.cpp`、Signal Group や I-PDU Group の停止まわりは
`ComStack_SignalGroup_Tx_test.cpp`。`Com.c`/`PduR.c`/`CanIf.c` それぞれ単体のテストではなく、
[CAN 通信スタック詳細](can_stack.md) のコールチェーン図そのものを実行して理解・確認するのが主目的）。
コールチェーン図に明示されている非同期の切れ目
（`Com_TxPending` というキュー経由で次回 `Com_MainFunctionTx()` まで待機する
箇所）でテストを2つのセグメントに分け、それぞれを個別に実行可能な
`TEST_F` ケースとしている（`--gtest_filter=ComStack_Signal_Tx_*` 等で
絞り込み可）。フェイクは最下層の `Can_Hw` のみ（`stub/Hal/
Fake_Can_Hw.c`）で、CanIf.c が呼ぶ `CanSM_ControllerBusOff()`/
`CanSM_ControllerModeIndication()` 等は CanSM.c 自身を実体でリンクして
処理させている（同じ `native_chain_tests` で CanSM 自身のコールチェーン
（「[CAN コントローラのスリープ制御](can_stack.md#can-controller-sleep)」節）も検証するため、2026-09 に
フェイクから実体リンクへ切り替えた。本チェーンのテスト自体は CanSM の
状態遷移を検証対象にしていないが、実体を混在させても副作用はない）。

[「Tx 処理」の「E2E」](can_stack.md#tx-processing-e2e)（`Com_MainFunctionTx()` →
TxTransformCbk → `E2EXf_E2EHealthStatus()` → `E2E_P05Protect()`）は
`ComStack_E2E_Tx_test.cpp` で別途検証している。本番の TxTransformCbk
（`Rte_COMTransform_E2EHealthStatus()`）は `Rte.c` にあるが、`Rte.c` 自体は
IoHwAb/FiM/App_EngineManager/App_WarningIndicator まで巨大な依存グラフを
引き込むためリンクせず、本番と同じ1行の委譲呼び出しをテスト専用の
TxTransformCbk として定義し、そこから先（E2EXf.c/E2EXf_PBCfg.c/E2E_P05.c）は
実体をそのまま検証する（詳細は `ComStack_E2E_Tx_test.cpp` 冒頭のコメント参照）。

<a id="unit-test-rx"></a>
### Rx 処理（Can → CanIf → PduR → Com の順）

同じ `test/CallChain/ComStack/` に、[「Rx 処理」コールチェーン](can_stack.md#rx-processing)
（`Can_MainFunction_Read()` → `CanIf_RxIndication()` →
`PduR_CanIfRxIndication()`（`PduR_ComRxIndication()` の `#define` エイリアス）→
`Com_RxIndication()`）を検証する `ComStack_Signal_Rx_test.cpp`（通常のシグナル）と
`ComStack_SignalGroup_Rx_test.cpp`（Signal Group・I-PDU Group）もある。Tx処理と異なり
1セグメントにまとめている理由がある: 図中の非同期境界を担う `Can_Isr()` は
`Can.c` 内の `static` 関数でテストから直接呼べず、かつ `Can_MainFunction_Read()`
自身も `Can_RxIrqPending` フラグの有無に関わらず無条件にポーリングする設計
（実機で `attachInterrupt` が初回発火しなかった経緯を踏まえた意図的な
二重防御、`Can.c` 冒頭のコメント参照）のため、フラグは `Com_TxPending` の
ような「後続処理の前提条件」ではない。したがって `Can_MainFunction_Read()` を
起点とする1つのコールチェーンとして検証している（詳細は
`ComStack_Signal_Rx_test.cpp` 冒頭のコメント参照）。

[「Rx 処理」の「E2E」](can_stack.md#rx-processing-e2e)（`Com_RxIndication()` →
RxIndicationCbk → `E2EXf_Inv_EngineInfo()`/`E2EXf_Inv_AbsInfo()` → `E2E_P05Check()`）は
`ComStack_E2E_Rx_test.cpp` で別途検証している。`Com_RxIndication()` を直接
呼ぶところから始め（コールチェーン図もこの粒度で揃えている）、Tx 側と同じ理由で
`Rte.c` はリンクせず、本番の RxIndicationCbk（`Rte_COMRxInd_EngineInfo()`）と
同じ処理をテスト専用の RxIndicationCbk として定義している。CRC 破損時に
`E2E_P05STATUS_ERROR` になることも含めて検証する（詳細は
`ComStack_E2E_Rx_test.cpp` 冒頭のコメント参照）。

[「Rx 処理」の「デッドライン監視」](can_stack.md#rx-processing-timeout)（`Com_MainFunctionRx()`
がしきい値超過を検知 → `Com_SigTimedOut` フラグ経由 → `Com_ReceiveSignal()` が
`ComRxDataTimeoutAction` を適用）は `ComStack_Signal_Rx_test.cpp`（`Timeout` フィクスチャ）と
`ComStack_SignalGroup_Rx_test.cpp` で別途検証している。この非同期境界は Tx 処理の `Com_TxPending` と構造が同じだが、
「立てる側／読む側」が逆（周期タスクが立てて on-demand 呼び出しが読む）ため、
PduR/CanIf/Can/CanSM を一切経由せず Com.c 単体で完結する。フェイクは
`millis()`（`stub/Hal/Fake_Millis.c`）のみで、`Com_RxIndication()`を
直接呼んで「受信していたが途絶えた」状態を作り、`FakeMillis_Value` を
しきい値超過まで進めてから検証する。Tx チェーンと同じくフラグの前後で
2セグメントに分け、フラグの状態自体はテスト専用アクセサ
`Com_Test_GetSigTimedOut()`（`COM_UNIT_TEST` 定義時のみ）で直接観測する
（詳細は `ComStack_Signal_Rx_test.cpp` 冒頭のコメント参照）。

<a id="unit-test-single"></a>
## 単一モジュールのテスト

`Gpt`/`Dio`/`Port`/`Det`/`E2E`/`E2E_P05`/`E2E_P01` のように他モジュールと
コールチェーンを共有しない末端モジュールは、HAL 層（`*_Hw` ファイル）だけを
フェイクに差し替えて単体で検証している（テストファイルは `test/Bsw/Gpt/` の
`Gpt_test.cpp` 等、フェイクは `stub/Hal/Fake_Gpt_Hw.c` 等、
2026-09 に専用 env `[env:native]` から本テストバイナリ（`native_chain_tests`）へ
統合済み）。

ファイル名は、**単体テストは関数単位**で、`test/Bsw/{Module}/{Module}_{Function}_test.cpp`
（例: `CanNm_RxIndication_test.cpp`）とする（2026-10-16 に確定。1 つの関数の OK と NG
を同じファイルに置き、フィクスチャ名は `{Module}_{Function}_Test`。テストが 1〜2 件しかない
関数は、近い機能の関数とまとめてよい）。複数モジュールを跨ぐコールチェーンのテストは
`{Stack}Stack_{Scenario}_test.cpp`（上記「コールチェーンのテスト」参照）で、
`{Scenario}` は**1つの正常系（OK）シナリオ単位**とする（2026-09-20、
`ComStack_TxChain_test.cpp` が1ファイルに多数のシナリオを詰め込んで
肥大化していたのを分割した経緯による。そのシナリオの派生 NG ケースは
同じファイルに同居させる。`DcmStack` は SID 単位）。
既存のモジュール単体テストのうち、関数単位になっていないファイルは、MC/DC を埋めるために
触るときに順次分割する（`CanNm` は 2026-10-16 に分割済み）。
チェーンの例外が `ComStack` で、同年9月にメッセージの種類（Signal / SignalGroup / E2E）×
方向（Tx / Rx）の `ComStack_{MsgType}_{Tx|Rx}_test.cpp` 6 ファイルへ再編している
（COM_TX_IPDU_MAX の制約で、分割するたびに共有設定を複製する必要があったため）。
フェイク/`--wrap` は `stub/` 配下に `src/` のディレクトリ構成を
ミラーリングして `Fake_{Module}.c`/`Wrap_{Module}.c`（HAL 層はフォルダ名
との重複を避け `Fake_{Module}_Hw.c`）で統一している。`Gpt_OnTick()`
（本来 ISR から呼ばれる関数）はテストから直接呼ぶことで、実割り込みなしに
状態機械を駆動している。

`Can_test.cpp`（`src/Bsw/Can/Can.c` 単体）は少し特殊で、`native_chain_tests`
は他のコールチェーンテストのために `CanIf.c`/`CanSM.c` を実体でリンクして
いるため、Can.c が上位層通知として呼ぶ3関数（`CanIf_RxIndication()`/
`CanIf_TxConfirmation()`/`CanIf_ControllerBusOff()`）だけを`--wrap`で
観測する（`stub/Bsw/CanIf/Wrap_CanIf.c`、既定は実体へパススルーしつつ
呼び出し回数・引数を記録。詳細は `CMakeLists.txt` 冒頭コメント参照）。
CanIf/CanSM 側の未初期化ガードにより、本テストが
`CanIf_Init()`/`CanSM_Init()` を呼ばない限りパススルー後の実処理は
静かに no-op になる。

新しい Bsw モジュールのテストを追加する場合は `test/Bsw/{Module}/` に
`{Module}_{Function}_test.cpp`（関数単位。および必要なら `stub/` 配下の
対応する `src/` パスに `Fake_{Module}.c`）を追加し、`CMakeLists.txt` の
`NATIVE_CHAIN_SRC_SOURCES`/`NATIVE_CHAIN_INCLUDE_DIRS` にその実ソースを
積み増す（GoogleTest の `main()` は `test_main.cpp` に集約しているため、
新規テストファイルには `int main()` を書かないこと）。

<a id="test-naming"></a>
## テスト名とフィクスチャ名の付け方

| 種類 | フィクスチャ | テスト名 |
|------|--------------|----------|
| コールチェーン（`ComStack` / `NmStack`） | 共通の準備は `..._Base`、シナリオごとに `..._{シナリオ}_Test`（基底を継承した空クラス） | `OK`（その中の OK が 1 件のとき）/ `OK_{要因}` / `NG_Step{NN}_{失敗する API}_{要因}` |
| UDS の要求応答（`DcmStack`） | 1 つの SID につき 1 ファイル・1 フィクスチャ | `OK_{内容}` / `NG_{NRC名}_{要因}`（応答が返らない場合は `NG_NoResponse_...`） |
| 単一モジュール | 関数ごとに 1 つ（`{Module}_{Function}_Test`） | `{API名}_{OK\|NG}_{要因}` |

- **step の意味**: 通しのシナリオで、別タスク（OS の周期）として実行される単位を `step01`、`step02` … と
  数えます。NG は、失敗が起きる step と、その step で実際に失敗する API 名を名前に入れます。到達できる
  NG がまだ無い step には、その位置に「未実装」の理由をコメントで残します。
- **並び順**: コールチェーンのテストでは、同じフィクスチャの中で、OK を先に、NG を step 順に並べます。
  モジュールの単体テスト（関数単位のファイル）では、**対象の関数の処理の順**に並べます（未初期化 → 引数・
  チャネルの確認 → 状態による分岐 → 正常系の順で、NG が先、OK が後ろ。2026-10-16 に決定）。判定を
  上から順に MC/DC で調べるときに、テストの順とコードの順が一致するためです。既存のファイルは、触るときに
  順次この順へ直します。
- **OK と NG の区分**: NG は、その API 呼び出し自体がエラーを返す、または Det などで異常を報告するケースです。
  異常な入力（拒否されたフレーム、破棄された短いフレームなど）を受けても、API が正常な結果を返し、
  動作が仕様どおりになるケースは OK とします（例: `OK_RejectedFrameStillResetsDeadlineTimer`）。
  どんな異常入力かは、名前の要因部分（`RejectedFrame`、`ShortFrameDiscard` など）で示します。
- **本文の骨格**: 通しのシナリオのテストは、Act に `// step01: ...` のようにコメントを付けて、実行する
  順に呼び出します。その NG で呼ばない step は、`// skip（理由）` とします。

<a id="unit-test-hooks"></a>
## テスト用の仕組み

- **初期化状態のリセット**: DeInit 相当の API が無いモジュールは、`*_UNIT_TEST` が定義された
  ときだけ有効になる `<Module>_Test_ResetInitState()` を持ち、各テストの `SetUp` で未初期化へ戻す
  （対象: CanTp / CryIf / Crypto / Csm / Dcm / Dem / EcuM / Fee / FiM / Mcu / Nm / NvM / Os /
  PduR / Wdg。定義は `CMakeLists.txt` の `add_compile_definitions()`。Can / Com / WdgM も
  `*_UNIT_TEST` を定義しているが、リセット関数ではなく `Can_Test_*` / `Com_Test_*` /
  `WdgM_Test_*` の状態取得・設定用アクセサを持つ）。
  本番ビルド（`pio run -e uno_r4`）ではこれらの関数は含まれない。
- **状態の直接設定**: 内部状態（static 変数）に応じた分岐を、状態遷移の API を経由せずに調べたい
  ときは、`*_UNIT_TEST` ガード付きの `<Module>_Test_SetState()`（対象: EcuM / CanNm）で状態を
  直接設定する（単体テストのみ。遷移そのものの検証はコールチェーンのテストで行う。状態以外の
  値は変えないので、必要なら別途そろえる）。`static` を外して内部の変数に直接触る方式は、
  全モジュールを 1 つのバイナリへリンクするため名前が衝突しうるので採用しない。
- **模擬 EEPROM の自動リセット**: Fee/NvM を実体リンクしているため、`test/test_main.cpp` の
  `FeeHwAutoResetListener`（GoogleTest の `TestEventListener`）が、すべてのテストの開始前に模擬
  EEPROM（`stub/Hal/Fake_Fee_Hw.c`）を消去済み状態に戻す。これにより、各テストは常に「初回起動」から始まる。
- **Det エラー報告の NG テスト**: 各モジュールの `Det_ReportError()` 経路（NULL ポインタ、未初期化、
  範囲外など）は、`{関数名}_NG_{NG要因}` の名前で、モジュールごとに NG のみのテストを置いている。
  構造上到達できない分岐は、テストを作らずその理由をコメントに残している。
- **時刻**: モジュールは `millis()` を直接呼ばず Os のカウンタ API（`GetCounterValue()`）経由で時刻を
  取る。Os 未初期化時は `millis()` を返すため、テストは `FakeMillis_Value`（`stub/Hal/Fake_Millis.c`）を
  進めるだけで時間を操作できる。Os 自身のテストは `test/Bsw/Os/Os_test.cpp`。

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

$env:DET_LOG_VERBOSE = "1"; ./build/native_chain/native_chain_tests.exe # TRACE ログ出力
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

<a id="unit-test-chain"></a>
## コールチェーンのテスト（`native_chain_tests`）

<a id="unit-test-tx"></a>
### Tx 処理（Com → PduR → CanIf → Can の順）

[「Tx 処理」コールチェーン](can_stack.md#tx-processing)（`Com_SendSignal()` → …
→ `Com_MainFunctionTx()` → `PduR_ComTransmit()` → `CanIf_Transmit()` →
`Can_Write()`）を複数モジュールにわたって実体（Com.c/PduR.c/CanIf.c/Can.c）で
リンクし、そのまま検証する `Bsw_ComStack_TxChain_{Scenario}_test.cpp` 群（2026-09-20、
シナリオごとに分割。`ComSendSignal`/`SendSignalGroupArray`/`ComMainFunction`/
`RepetitionSequence`/`TxTOut` 等）を `test/Bsw/ComStack/` に用意しています（`Com.c`/`PduR.c`/
`CanIf.c` それぞれ単体のテストではなく、[CAN 通信スタック詳細](can_stack.md) のコールチェーン図そのものを
実行して理解・確認するのが主目的）。
コールチェーン図に明示されている非同期の切れ目
（`Com_TxPending` というキュー経由で次回 `Com_MainFunctionTx()` まで待機する
箇所）でテストを2つのセグメントに分け、それぞれを個別に実行可能な
`TEST_F` ケースとしている（`--gtest_filter=Bsw_ComStack_TxChain_ComSendSignal_Test.*` 等で
絞り込み可）。フェイクは最下層の `Can_Hw` のみ（`stub/Hal/
Fake_Can_Hw.c`）で、CanIf.c が呼ぶ `CanSM_RxIndication()`/
`CanSM_ControllerModeIndication()` 等は CanSM.c 自身を実体でリンクして
処理させている（同じ `native_chain_tests` で CanSM 自身のコールチェーン
（「[CAN コントローラのスリープ制御](can_stack.md#can-controller-sleep)」節）も検証するため、2026-09 に
フェイクから実体リンクへ切り替えた。本チェーンのテスト自体は CanSM の
状態遷移を検証対象にしていないが、実体を混在させても副作用はない）。

[「Tx 処理」の「E2E」](can_stack.md#tx-processing-e2e)（`Com_MainFunctionTx()` →
TxTransformCbk → `E2EXf_TransformP05()` → `E2E_P05Protect()`）は
`Bsw_ComStack_TxE2EChain_test.cpp` で別途検証している。本番の TxTransformCbk
（`Rte_COMTransform_E2EHealthStatus()`）は `Rte.c` にあるが、`Rte.c` 自体は
IoHwAb/FiM/App_EngineManager/App_WarningIndicator まで巨大な依存グラフを
引き込むためリンクせず、本番と同じ1行の委譲呼び出しをテスト専用の
TxTransformCbk として定義し、そこから先（E2EXf.c/E2EXf_PBCfg.c/E2E_P05.c）は
実体をそのまま検証する（詳細は `Bsw_ComStack_TxE2EChain_test.cpp` 冒頭のコメント参照）。

<a id="unit-test-rx"></a>
### Rx 処理（Can → CanIf → PduR → Com の順）

同じ `test/Bsw/ComStack/` に、[「Rx 処理」コールチェーン](can_stack.md#rx-processing)
（`Can_MainFunction_Read()` → `CanIf_RxIndication()` →
`PduR_CanIfRxIndication()`（`PduR_ComRxIndication()` の `#define` エイリアス）→
`Com_RxIndication()`）を検証する `Bsw_ComStack_RxChain_test.cpp` もある。Tx処理と異なり
1セグメントにまとめている理由がある: 図中の非同期境界を担う `Can_Isr()` は
`Can.c` 内の `static` 関数でテストから直接呼べず、かつ `Can_MainFunction_Read()`
自身も `Can_RxIrqPending` フラグの有無に関わらず無条件にポーリングする設計
（実機で `attachInterrupt` が初回発火しなかった経緯を踏まえた意図的な
二重防御、`Can.c` 冒頭のコメント参照）のため、フラグは `Com_TxPending` の
ような「後続処理の前提条件」ではない。したがって `Can_MainFunction_Read()` を
起点とする1つのコールチェーンとして検証している（詳細は
`Bsw_ComStack_RxChain_test.cpp` 冒頭のコメント参照）。

[「Rx 処理」の「E2E」](can_stack.md#rx-processing-e2e)（`Com_RxIndication()` →
RxIndicationCbk → `E2EXf_InverseTransformP05()` → `E2E_P05Check()`）は
`Bsw_ComStack_RxE2EChain_test.cpp` で別途検証している。`Com_RxIndication()` を直接
呼ぶところから始め（コールチェーン図もこの粒度で揃えている）、Tx 側と同じ理由で
`Rte.c` はリンクせず、本番の RxIndicationCbk（`Rte_COMRxInd_EngineInfo()`）と
同じ処理をテスト専用の RxIndicationCbk として定義している。CRC 破損時に
`E2E_P05STATUS_ERROR` になることも含めて検証する（詳細は
`Bsw_ComStack_RxE2EChain_test.cpp` 冒頭のコメント参照）。

[「Rx 処理」の「デッドライン監視」](can_stack.md#rx-processing-timeout)（`Com_MainFunctionRx()`
がしきい値超過を検知 → `Com_SigTimedOut` フラグ経由 → `Com_ReceiveSignal()` が
`ComRxDataTimeoutAction` を適用）は `Bsw_ComStack_RxTimeoutChain_test.cpp` で別途検証
している。この非同期境界は Tx 処理の `Com_TxPending` と構造が同じだが、
「立てる側／読む側」が逆（周期タスクが立てて on-demand 呼び出しが読む）ため、
PduR/CanIf/Can/CanSM を一切経由せず Com.c 単体で完結する。フェイクは
`millis()`（`stub/Hal/Fake_Millis.c`）のみで、`Com_RxIndication()`を
直接呼んで「受信していたが途絶えた」状態を作り、`FakeMillis_Value` を
しきい値超過まで進めてから検証する。Tx チェーンと同じくフラグの前後で
2セグメントに分け、フラグの状態自体はテスト専用アクセサ
`Com_Test_GetSigTimedOut()`（`COM_UNIT_TEST` 定義時のみ）で直接観測する
（詳細は `Bsw_ComStack_RxTimeoutChain_test.cpp` 冒頭のコメント参照）。

<a id="unit-test-single"></a>
## 単一モジュールのテスト

`Gpt`/`Dio`/`Port`/`Det`/`E2E`/`E2E_P05`/`E2E_P01` のように他モジュールと
コールチェーンを共有しない末端モジュールは、HAL 層（`*_Hw` ファイル）だけを
フェイクに差し替えて単体で検証している（テストファイルは `test/Bsw/Gpt/` の
`Bsw_Gpt_test.cpp` 等、フェイクは `stub/Hal/Fake_Gpt_Hw.c` 等、
2026-09 に専用 env `[env:native]` から本テストバイナリ（`native_chain_tests`）へ
統合済み）。ファイル名は、テストは `test/Bsw/{Module}/` に
`Bsw_{Module}_{Scenario}_test.cpp`（複数モジュールを跨ぐ統合テストは
`Bsw_{Stack}Stack_{Scenario}_test.cpp`、上記「コールチェーンのテスト」参照）で、
`{Scenario}` は**1つの正常系（OK）シナリオ単位**とする（2026-09-20、
`Bsw_ComStack_TxChain_test.cpp` が1ファイルに多数のシナリオを詰め込んで
肥大化していたのを`ComSendSignal`/`SendSignalGroupArray`/`TxTOut` 等15ファイルへ
分割した経緯を踏まえ、以後の新規テストファイルもこの粒度を守る。そのシナリオの
派生 NG ケースは同じファイルに同居させる）。
フェイク/`--wrap` は `stub/` 配下に `src/` のディレクトリ構成を
ミラーリングして `Fake_{Module}.c`/`Wrap_{Module}.c`（HAL 層はフォルダ名
との重複を避け `Fake_{Module}_Hw.c`）で統一している。`Gpt_OnTick()`
（本来 ISR から呼ばれる関数）はテストから直接呼ぶことで、実割り込みなしに
状態機械を駆動している。

`Bsw_Can_test.cpp`（`src/Bsw/Can/Can.c` 単体）は少し特殊で、`native_chain_tests`
は他のコールチェーンテストのために `CanIf.c`/`CanSM.c` を実体でリンクして
いるため、Can.c が上位層通知として呼ぶ3関数（`CanIf_RxIndication()`/
`CanIf_TxConfirmation()`/`CanIf_ControllerBusOff()`）だけを`--wrap`で
観測する（`stub/Bsw/CanIf/Wrap_CanIf.c`、既定は実体へパススルーしつつ
呼び出し回数・引数を記録。詳細は `CMakeLists.txt` 冒頭コメント参照）。
CanIf/CanSM 側の未初期化ガードにより、本テストが
`CanIf_Init()`/`CanSM_Init()` を呼ばない限りパススルー後の実処理は
静かに no-op になる。

新しい Bsw モジュールのテストを追加する場合は `test/Bsw/{Module}/` に
`Bsw_{Module}_{Scenario}_test.cpp`（および必要なら `stub/` 配下の
対応する `src/` パスに `Fake_{Module}.c`）を追加し、`CMakeLists.txt` の
`NATIVE_CHAIN_SRC_SOURCES`/`NATIVE_CHAIN_INCLUDE_DIRS` にその実ソースを
積み増す（GoogleTest の `main()` は `test_main.cpp` に集約しているため、
新規テストファイルには `int main()` を書かないこと）。

<a id="unit-test-hooks"></a>
## テスト用の仕組み

- **初期化状態のリセット**: DeInit 相当の API が無いモジュールは、`*_UNIT_TEST` が定義された
  ときだけ有効になる `<Module>_Test_ResetInitState()` を持ち、各テストの `SetUp` で未初期化へ戻す
  （対象: Can / CanTp / Com / CryIf / Crypto / Csm / Dcm / Dem / Fee / FiM / Mcu / Nm / NvM / Os /
  PduR / Wdg / WdgM。定義は `CMakeLists.txt` の `add_compile_definitions()`）。
  本番ビルド（`pio run -e uno_r4`）ではこれらの関数は含まれない。
- **模擬 EEPROM の自動リセット**: Fee/NvM を実体リンクしているため、`test/test_main.cpp` の
  `FeeHwAutoResetListener`（GoogleTest の `TestEventListener`）が、すべてのテストの開始前に模擬
  EEPROM（`stub/Hal/Fake_Fee_Hw.c`）を消去済み状態に戻す。これにより、各テストは常に「初回起動」から始まる。
- **Det エラー報告の NG テスト**: 各モジュールの `Det_ReportError()` 経路（NULL ポインタ、未初期化、
  範囲外など）は、`{関数名}_NG_{NG要因}` の名前で、モジュールごとに NG のみのテストを置いている。
  構造上到達できない分岐は、テストを作らずその理由をコメントに残している。
- **時刻**: モジュールは `millis()` を直接呼ばず Os のカウンタ API（`GetCounterValue()`）経由で時刻を
  取る。Os 未初期化時は `millis()` を返すため、テストは `FakeMillis_Value`（`stub/Hal/Fake_Millis.c`）を
  進めるだけで時間を操作できる。Os 自身のテストは `test/Bsw/Os/Bsw_Os_test.cpp`。

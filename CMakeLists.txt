# native_chain テスト専用の CMake プロジェクト（GoogleTest によるホスト上での
# BSW ロジック検証。実機向け Arduino ビルドは platformio.ini の [env:uno_r4]
# のみで完結し、本ファイルとは無関係）。
#
# 移行の経緯（2026-09-20）: 従来は PlatformIO の `native` プラットフォームで
# `[env:native_chain]`/`[env:native_chain_coverage]` として運用していたが、
# その SCons ビルダー（~/.platformio/platforms/native/builder/main.py）が
# CC/CXX を無条件に削除し `Tool("gcc")`/`Tool("g++")` で PATH 上の gcc/g++ を
# 再検出し直すため、OS 環境変数経由での clang++ (llvm-mingw) への切り替えが
# 旧 scripts/use_clang_coverage.py の想定通りには機能しなかった
# （LINK/AR/RANLIB/LINKFLAGS の上書きは有効だが CC/CXX 自体はコンパイルに
# 反映されない）。テストコードのビルドを PlatformIO から切り離し、clang++ を
# 確実に使う本 CMake プロジェクトへ完全移行した（旧 env は削除済み）。
#
# 以下は旧 platformio.ini [env:native_chain] 冒頭にあったテスト設計の経緯
# コメントをそのまま移設したもの（PlatformIO の env という語彙のまま
# 残しているが、現在は本 CMake プロジェクトの native_chain_tests ターゲット
# に読み替えてよい）。
#
# コールチェーンテスト全般の方針。README の「Tx処理」「Rx処理」「CAN コント
# ローラのスリープ制御」（実スリープ・ウェイクアップ検証）のようにモジュール
# 横断でコールチェーンを実体（Com.c/PduR.c/CanIf.c/Can.c/CanSM.c）のまま
# リンクして検証したいテストを test/ に置く（Bsw_ComStack_TxChain_test.cpp 参照）。
#
# CanIf.c が CanSM_RxIndication()/ControllerBusOff() を、Can.c が
# EcuM_CheckWakeup() 経由で CanSM_ControllerModeIndication() をハードコードで
# 呼ぶため（2026-09-05、旧 CanIf_ControllerWakeup() 経由から変更。
# Fake_Bsw_EcuM.h 参照）、CanSM.c は実体・フェイクのいずれかをこのバイナリ
# 全体で選ぶ必要がある。Tx/Rx チェーン（Bsw_ComStack_TxChain_test.cpp/
# Bsw_ComStack_RxChain_test.cpp）は CanSM の状態遷移に関与しないが、CanSM 自身の
# コールチェーン（Bsw_NmStack_WakeupChain_test.cpp）や Nm↔CanSM↔ComM 協調スリープ
# （Bsw_NmStack_SleepCoordination_test.cpp）を同じバイナリで検証するため、CanSM.c は
# 実体をリンクしている。
#
# ComM.c/Nm.c も実体でリンクする（2026-08、当初は Nm/CanSM/ComM 協調スリープの
# 移管作業のリスクを既存5チェーンテストから切り離すため一時的に
# `[env:native_sleep_chain]` という別envに分離していたが、実機検証まで完了し
# 安定したため本バイナリへ統合した）。ComM.c が呼ぶ EcuM_RequestRUN()/
# EcuM_ReleaseRUN()/BswM_ComM_CurrentMode() は境界としてフェイクに差し替える
# （test/stub/Bsw/EcuM/Fake_Bsw_EcuM.h・test/stub/Bsw/BswM/Fake_Bsw_BswM.h 参照）。
# Dem は実体でリンクし、Dem_SetEventStatus() のみ `--wrap` で観測・故障注入する
# （test/stub/Bsw/Dem/Wrap_Dem.h 参照、2026-09 に完全フェイクから切り替え）。
# CanSM 状態遷移に関与しない Tx/Rx/E2E チェーンのテストは ComM_Init()/Nm_Init()
# を呼ばないため、これらのモジュールの実体は未初期化のまま（呼ばれない）で無害。
#
# E2E チェーン（README「Tx 処理」「Rx 処理」それぞれの「E2E」節、
# Bsw_ComStack_TxE2EChain_test.cpp/Bsw_ComStack_RxE2EChain_test.cpp）は E2EXf.c/
# E2EXf_PBCfg.c/E2E_P05.c/E2E_P01.c（E2EXf.c が両 Profile を参照するため）を
# 実体でリンクする。本番の TxTransformCbk/RxIndicationCbk
# （Rte_COMTransform_E2EHealthStatus()/Rte_COMCbk_EngineInfo() 等）は Rte.c
# 内にあるが、Rte.c 自体は IoHwAb/FiM/App_EngineManager/App_WarningIndicator
# まで巨大な依存グラフを引き込むため（Bsw_ComStack_TxChain_test.cpp 冒頭コメント
# と同じ理由）リンクしない。各テストファイル内に、本番の
# Rte_COMTransform_E2EHealthStatus()/Rte_COMCbk_EngineInfo() と同じ1行の委譲
# 呼び出しをテスト専用の TxTransformCbk/RxIndicationCbk として定義し、
# E2EXf_TransformP05()/E2EXf_InverseTransformP05() から先は実体をそのまま
# 検証する。E2EMon（E2E 検証結果のテレメトリ集計）はこのチェーンの対象外
# （README の「E2E」節の図に含めていない別軸のため）。
#
# デッドライン監視チェーン（README「Rx 処理」の「デッドライン監視」節、
# Bsw_ComStack_RxTimeoutChain_test.cpp）は PduR/CanIf/Can/CanSM を経由せず Com.c
# 単体で完結するため、フェイクは millis()（Fake_Millis.c）のみで足りる。
#
# GNU ld の `-Wl,--wrap=<symbol>` によるフォールトインジェクション（2026-09
# 新設、以前は [env:native_chain_wrap] という別 env で試作していたが、2ラウンド
# の実績（CanSM Bus-Off回復・E2EXf SMCheck失敗分岐、いずれも
# test/Bsw_CanSM_BusOffRecovery_test.cpp/Bsw_E2EXf_SMCheckFailure_test.cpp 参照）を
# 経て本バイナリへ統合した）。実体を丸ごとフェイクへ差し替えず、境界の1関数
# だけをピンポイントで失敗させたい場合に使う（test/stub/Bsw/CanIf/Wrap_CanIf.h・
# test/stub/Bsw/E2E/Wrap_E2E.h 冒頭コメント参照）。「stub は fake ファイルではなく
# wrap 方式で」という本プロジェクトの方針を体現する。
#
# WdgM（2026-09、旧 [env:native_wdgm] から統合）。WdgM.c は WdgIf.h（下位
# ディスパッチ層）と Det.h/Dem.h にしか依存せず、native_chain の他モジュール
# （Can/CanIf/CanSM/ComM/Nm 等）と一切コールチェーンを共有しないため、
# test_native/test_dcm/test_fim/test_wdgm を test_chain へ統合するという長期
# ゴールの中で最も依存が軽い候補として最初に統合した。WdgIf は他 env に同種
# モジュールが無いため専用フェイク（test/stub/Bsw/WdgIf/Fake_WdgIf.c/h）を
# そのまま持ち込んだ。
#
# Dem/FiM（2026-09、旧 [env:native_fim] から統合、上記 WdgM 統合の直後）。
# test_dcm/test_fim がどちらも Dem.c を実体リンクしているのに対し native_chain
# は Dem_SetEventStatus() の呼び出し記録だけが目的の単純なフェイクを使って
# おり、素直に統合すると CanSM/WdgM の各テストがこのフェイクの挙動（1回呼べば
# 即座に記録される）に依存している一方、Dem を実体化すると同じイベントでも
# デバウンス/DTC確定/EEPROM永続化まで絡む別物になってしまう。
# CanIf_SetControllerMode/E2E_SMCheck と同じ考え方（実体は保ったまま境界の
# 1関数だけ観測する）で `-Wl,--wrap=Dem_SetEventStatus`
# （test/stub/Bsw/Dem/Wrap_Dem.h 参照）を導入し、常に `__real_Dem_SetEventStatus()`
# へパススルーしつつ呼び出し回数・引数を記録する。対象イベント
# （DEM_EVENT_CAN_BUSOFF/DEM_EVENT_WDGM_SUPERVISION）はいずれもデバウンス閾値が
# 1（Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*）のため、既存テストの「1回呼べば確定/
# 回復する」という前提はそのまま成立する。Dem の内部状態自体はテストケースを
# 跨いで永続する static のため、各テストの SetUp() で `Dem_Init(NULL)` を追加
# （test/stub/Bsw/NvM/Fake_NvM.c により常に「初回起動」相当の決定的なリセット
# になる）。FiM.c は Dem.h/Det.h にしか依存しないため、Dem を実体化した後は
# test_fim をそのまま統合するだけで済んだ。
#
# Dcm（2026-09、旧 [env:native_dcm] から統合、上記 Dem/FiM 統合の直後）。
# Dcm_Cbk.c は CanTp/Rte/KeyM の3モジュールへ依存するが、これらは
# native_chain の他モジュールと一切コールチェーンを共有しない純粋なリンク
# スタブ（test/stub/Bsw/CanTp/Fake_CanTp.c/h・test/stub/Rte/Fake_Rte.c・
# test/stub/Bsw/KeyM/Fake_KeyM.c）のため、そのまま持ち込むだけで済んだ。
# BswM は Dcm_Cbk.c から見た通知先 BswM_Dcm_CommunicationMode_CurrentState()
# を、ComM.c 用の既存フェイク Fake_Bsw_BswM.c/h へ統合（対象関数は別だが
# `FakeBswM_Reset`/`FakeBswM_LastMode` のシンボル名が衝突するため、Dcm 側の
# アクセサを `FakeBswM_LastDcmCommunicationMode` 等へ改称して統合）。ComM は
# native_chain が既に実体をリンクしているため（Nm↔CanSM↔ComM 協調スリープ
# 検証用）、Dcm_Cbk.c が呼ぶ `ComM_DCM_ActiveDiagnostic()`/
# `ComM_DCM_InactiveDiagnostic()` を実体へ到達させると、Dcm のテスト対象外
# である通信管理（Nm/CanSM 経由の実際のモード遷移）まで連鎖してしまう。
# `-Wl,--wrap=` でこの2関数を「常に何もしない」よう隔離した
# （test/stub/Bsw/ComM/Wrap_ComM.c参照。CanIf/E2E/Dem の wrap とは異なり、
# 既定パススルーではなく常時no-op という特殊ケース）。
#
# test_native（2026-09、旧 [env:native]/[env:native_coverage] から統合。
# test_native/test_dcm/test_wdgm/test_fim を test_chain へ統合するという長期
# ゴールの最終回）。Gpt/Dio/Port は他モジュールと無縁の末端モジュールのため、
# それぞれの HAL 層フェイク（test/stub/Hal/Fake_Gpt_Hw.c 等）ごとそのまま
# 持ち込むだけで済んだ。Can.c 単体検証（Bsw_Can_test.cpp）だけは、Can.c が
# 上位層通知として呼ぶ `CanIf_RxIndication()`/`CanIf_TxConfirmation()`/
# `CanIf_ControllerBusOff()` の3関数を、native_chain が既に CanIf.c を実体で
# リンクしているため `--wrap` 化する必要があった（test/stub/Bsw/CanIf/
# Wrap_CanIf.c/h へ追記）。3関数とも（a）CanIf/CanSM側に未初期化ガードがあり、
# Bsw_Can_test.cppはCanIf_Init()/CanSM_Init()を呼ばないため実体へパススルー
# しても静かにno-opになるだけ、（b）Bsw_Can_test.cpp自体にDET報告回数の完全
# 一致アサーションが1件も無い、の2点を確認した上で、単純な既定パススルー
# のみで実装した。
#
# フェイクの配置規則（2026-09、test_native/test_dcm 統合が一段落した後に整理）。
# 全 env が native_chain 1つに統合されたことで、「test/ 直下にテストファイルと
# フェイクファイルが混在する」状態になっていた。--wrap ファイル（Wrap_CanIf.h
# 等）は既に src/ の対応する1ファイルにつき1ファイルという規則で test/stub/
# 配下に整理されていたため、単純な呼び出し記録フェイク（Fake_Bsw_BswM.c 等）
# も同じ考え方で test/stub/ 配下（src/ のディレクトリ構成をそのまま反映:
# 例 src/Bsw/CanTp/ → test/stub/Bsw/CanTp/、src/Rte/ → test/stub/Rte/）へ移設
# した。結果として `test/` 直下はテストファイル（`Bsw_{シナリオ名}_test.cpp`）と
# `test_main.cpp`/`win_quick_exit_stub.cpp` のみになった。

cmake_minimum_required(VERSION 3.21)
project(native_chain_tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

option(ENABLE_COVERAGE "Clang source-based coverage（MC/DC含む）を有効化する（native-chain-coverage プリセット用）" OFF)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_compile_definitions(
    CAN_UNIT_TEST
    COM_UNIT_TEST
    DET_LOG_LEVEL=LOG_T
    WDGM_UNIT_TEST
)

# native_chain_tests の -I 一覧。
set(NATIVE_CHAIN_INCLUDE_DIRS
    src/Bsw/Can
    src/Bsw/CanIf
    src/Bsw/CanSM
    src/Bsw/ComM
    src/Bsw/Com
    src/Bsw/PduR
    src/Bsw/Nm
    src/Bsw/EcuM
    src/Bsw/BswM
    src/Bsw/Dcm
    src/Bsw/Dem
    src/Bsw/Det
    src/Bsw/SchM
    src/Bsw/E2EXf
    src/Bsw/E2E
    src/Bsw/WdgM
    src/Bsw/WdgIf
    src/Bsw/FiM
    src/Bsw/NvM
    src/Bsw/CanTp
    src/Bsw/KeyM
    src/Bsw/Gpt
    src/Bsw/Dio
    src/Bsw/Port
    src/Rte
    src/Hal
    test/stub/Bsw/CanIf
    test/stub/Bsw/E2E
    test/stub/Bsw/Dem
    test/stub/Bsw/ComM
    test/stub/Bsw/BswM
    test/stub/Bsw/EcuM
    test/stub/Bsw/CanTp
    test/stub/Bsw/KeyM
    test/stub/Bsw/NvM
    test/stub/Bsw/WdgIf
    test/stub/Rte
    test/stub/Hal
)

# native_chain_tests が実体でリンクする src/ 配下のソース。
set(NATIVE_CHAIN_SRC_SOURCES
    src/Bsw/Det/Det.c
    src/Bsw/Com/Com.c
    src/Bsw/PduR/PduR.c
    src/Bsw/CanIf/CanIf.c
    src/Bsw/Can/Can.c
    src/Bsw/CanSM/CanSM.c
    src/Bsw/ComM/ComM.c
    src/Bsw/Nm/Nm.c
    src/Bsw/E2EXf/E2EXf.c
    src/Bsw/E2EXf/E2EXf_PBCfg.c
    src/Bsw/E2E/E2E_P05.c
    src/Bsw/E2E/E2E_P01.c
    src/Bsw/E2E/E2E.c
    src/Bsw/WdgM/WdgM.c
    src/Bsw/WdgM/WdgM_PBCfg.c
    src/Bsw/Dem/Dem.c
    src/Bsw/FiM/FiM.c
    src/Bsw/FiM/FiM_PBCfg.c
    src/Bsw/Dcm/Dcm_Cbk.c
    src/Bsw/Gpt/Gpt.c
    src/Bsw/Dio/Dio.c
    src/Bsw/Port/Port.c
)

# 同じくリンクする test/stub/ 配下のフェイク/wrapソース。
set(NATIVE_CHAIN_STUB_SOURCES
    test/stub/Bsw/CanIf/Wrap_CanIf.c
    test/stub/Bsw/E2E/Wrap_E2E.c
    test/stub/Bsw/Dem/Wrap_Dem.c
    test/stub/Bsw/ComM/Wrap_ComM.c
    test/stub/Bsw/BswM/Fake_Bsw_BswM.c
    test/stub/Bsw/EcuM/Fake_Bsw_EcuM.c
    test/stub/Bsw/CanTp/Fake_CanTp.c
    test/stub/Bsw/KeyM/Fake_KeyM.c
    test/stub/Bsw/NvM/Fake_NvM.c
    test/stub/Bsw/WdgIf/Fake_WdgIf.c
    test/stub/Rte/Fake_Rte.c
    test/stub/Hal/Fake_Can_Hw.c
    test/stub/Hal/Fake_Det_Hw.c
    test/stub/Hal/Fake_Millis.c
    test/stub/Hal/Fake_SchM_Hw.c
    test/stub/Hal/Fake_Dio_Hw.c
    test/stub/Hal/Fake_Gpt_Hw.c
    test/stub/Hal/Fake_Port_Hw.c
)

# test/ 直下のテストファイル（Bsw_{Module|Stack}_{シナリオ}_test.cpp）を自動収集する
# （旧 PlatformIO 版の test_build_src=yes によるテストソース自動検出、test/ 直下の
# ファイルのみを拾う仕様、[[reference_platformio_test_suite_discovery]] 参照、と
# 同じ挙動を CONFIGURE_DEPENDS 付き GLOB で再現する）。
file(GLOB NATIVE_CHAIN_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/test/*.cpp)

add_executable(native_chain_tests
    ${NATIVE_CHAIN_TEST_SOURCES}
    ${NATIVE_CHAIN_SRC_SOURCES}
    ${NATIVE_CHAIN_STUB_SOURCES}
)

target_include_directories(native_chain_tests PRIVATE ${NATIVE_CHAIN_INCLUDE_DIRS})

# test/test_main.cpp が独自に main()/RUN_ALL_TESTS() を定義しているため、
# 独自 main を持つ gtest_main ではなく gtest のみをリンクする。
target_link_libraries(native_chain_tests PRIVATE gtest)

# GoogleTest は pthread（llvm-mingw では libwinpthread-1.dll への動的リンク）を
# 要求するが、このマシンでは実行時に libwinpthread-1.dll 自体の解決に失敗し
# `error while loading shared libraries: api-ms-win-crt-*.dll` としてクラッシュ
# する（2026-09-20 に実際に確認: pthread を使わない最小構成では発生せず、
# -pthread を足すと再現する）。`-static` で UCRT/pthread ランタイムを丸ごと
# 実行ファイルへ静的リンクし、この動的解決自体を回避する。
target_link_options(native_chain_tests PRIVATE -static)

# native_chain_tests がリンク時に故障注入する -Wl,--wrap= 対象シンボル一覧。
set(NATIVE_CHAIN_WRAP_SYMBOLS
    CanIf_SetControllerMode
    E2E_SMCheck
    Dem_SetEventStatus
    ComM_DCM_ActiveDiagnostic
    ComM_DCM_InactiveDiagnostic
    CanIf_RxIndication
    CanIf_TxConfirmation
    CanIf_ControllerBusOff
)
foreach(wrap_symbol IN LISTS NATIVE_CHAIN_WRAP_SYMBOLS)
    target_link_options(native_chain_tests PRIVATE "-Wl,--wrap=${wrap_symbol}")
endforeach()

if(ENABLE_COVERAGE)
    target_compile_options(native_chain_tests PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
    target_link_options(native_chain_tests PRIVATE -fprofile-instr-generate)
endif()

# CTest 登録: native_chain の各テストは CanSM/CanIf/Can 等の実体を跨いで static
# 状態を共有し、実行順序によってはハングしうる（[[feedback_native_chain_shared_static_hang]]）。
# gtest_discover_tests でテストごとに別プロセス起動へ分割すると、この前提の
# 実行順序が変わり未検証の挙動変化を招くため、pio test と同じ「1バイナリを
# 通しで実行」の単一 CTest として登録する（個別テストの絞り込みは
# `native_chain_tests --gtest_filter=...` を直接呼べばよい）。
enable_testing()
add_test(NAME native_chain_tests COMMAND native_chain_tests)

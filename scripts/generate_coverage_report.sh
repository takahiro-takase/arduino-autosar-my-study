#!/usr/bin/env bash
# カバレッジレポート生成スクリプト（CMake + clang/llvm-mingw、CMakeLists.txt/
# CMakePresets.json の native-chain-coverage プリセット参照）。
#
# 2026-09、PlatformIO native プラットフォームの SCons ビルダーが CC/CXX を
# 強制的に gcc/g++ へ上書きし直す既知の制約のため、テストビルドを CMake へ
# 完全移行した（旧 [env:native_chain_coverage]/scripts/use_clang_coverage.py は
# 削除済み）。
#
# 使い方（事前に環境変数を設定してから呼ぶこと）:
#   export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"
#   bash scripts/generate_coverage_report.sh
set -euo pipefail

if [ -z "${LLVM_MINGW_BIN:-}" ]; then
    echo "ERROR: 環境変数 LLVM_MINGW_BIN が未設定です。本ファイル冒頭のコメント参照。例:" >&2
    echo '    export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"' >&2
    exit 1
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_DIR="build/native_chain_coverage"
BIN="$BUILD_DIR/native_chain_tests.exe"

echo "==> [1/3] configure + build (native-chain-coverage プリセット)..."
cmake --preset native-chain-coverage
cmake --build --preset native-chain-coverage

echo "==> [2/3] テスト実行してプロファイルを採取中..."
rm -f "$BUILD_DIR/native_chain.profraw"
( cd "$BUILD_DIR" && LLVM_PROFILE_FILE="native_chain.profraw" ./native_chain_tests.exe > /dev/null )

rm -f "$BUILD_DIR/coverage.profdata"
"$LLVM_MINGW_BIN/llvm-profdata" merge -sparse "$BUILD_DIR/native_chain.profraw" -o "$BUILD_DIR/coverage.profdata"

echo "==> [3/3] HTML レポートを生成中..."
rm -rf coverage_html
"$LLVM_MINGW_BIN/llvm-cov" show "$BIN" \
    -instr-profile="$BUILD_DIR/coverage.profdata" --show-mcdc \
    -format=html -output-dir=coverage_html \
    -ignore-filename-regex='.*(googletest|[\\/]test[\\/]).*'

echo ""
"$LLVM_MINGW_BIN/llvm-cov" report "$BIN" \
    -instr-profile="$BUILD_DIR/coverage.profdata" --show-mcdc-summary \
    -ignore-filename-regex='.*(googletest|[\\/]test[\\/]).*'

echo ""
echo "==> 完了: coverage_html/index.html をブラウザで開いてください。"

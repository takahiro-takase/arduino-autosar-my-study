#!/usr/bin/env bash
# カバレッジレポート生成スクリプト（clang/llvm-mingw ベースの
# [env:native_chain_coverage]、platformio.ini の当該セクション・
# scripts/use_clang_coverage.py 参照）。
#
# 2026-09、test_native/test_dcm/test_wdgm/test_fim を test_chain へ統合した
# ことに伴い、native env自体が [env:native_chain]（+ _coverage）1つに
# 集約された。以前は複数の *_coverage envのプロファイルを統合していたが、
# 今は単一envのプロファイルをそのまま使うだけで足りる（将来また
# coverage envが増えた場合に備え、複数env統合の仕組み自体はそのまま
# 残してある）。
#
# 対象env（ENVS配列）をビルド・テスト実行し、生成されたプロファイルから
# MC/DC 条件カバレッジを含む HTML レポートを coverage_html/ へ生成する
# （.gitignore 済み、index.html をブラウザで直接開ける）。
#
# 使い方（事前に環境変数を設定してから呼ぶこと）:
#   export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"
#   export CC=x86_64-w64-mingw32-clang
#   export CXX=x86_64-w64-mingw32-clang++
#   bash scripts/generate_coverage_report.sh
set -euo pipefail

if [ -z "${LLVM_MINGW_BIN:-}" ] || [ -z "${CC:-}" ] || [ -z "${CXX:-}" ]; then
    echo "ERROR: 環境変数 LLVM_MINGW_BIN/CC/CXX が未設定です。本ファイル冒頭の" >&2
    echo "  コメント参照。例:" >&2
    echo '    export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"' >&2
    echo '    export CC=x86_64-w64-mingw32-clang' >&2
    echo '    export CXX=x86_64-w64-mingw32-clang++' >&2
    exit 1
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

export PATH="$LLVM_MINGW_BIN:$PATH"

# pio 自体が PATH に無い環境向けのフォールバック
# （[[reference_pio_cli_location]] と同じ既知の事情）。
if ! command -v pio > /dev/null 2>&1; then
    export PATH="$USERPROFILE/.platformio/penv/Scripts:$PATH"
fi

ENVS=(
    native_chain_coverage
)

PIO_ENV_ARGS=()
for e in "${ENVS[@]}"; do
    PIO_ENV_ARGS+=(-e "$e")
done

echo "==> [1/3] 全 ${#ENVS[@]} 環境をビルド・テスト実行中..."
pio test "${PIO_ENV_ARGS[@]}"

echo "==> [2/3] 各環境のプロファイルを取得・統合中..."
PROFILES=()
OBJECT_ARGS=()
FIRST_BIN=""
for e in "${ENVS[@]}"; do
    bin=".pio/build/$e/program.exe"
    ( cd ".pio/build/$e" && LLVM_PROFILE_FILE="$e.profraw" ./program.exe > /dev/null )
    PROFILES+=(".pio/build/$e/$e.profraw")
    if [ -z "$FIRST_BIN" ]; then
        FIRST_BIN="$bin"
    else
        OBJECT_ARGS+=(-object "$bin")
    fi
done

rm -f coverage.profdata
llvm-profdata merge -sparse "${PROFILES[@]}" -o coverage.profdata

echo "==> [3/3] HTML レポートを生成中..."
rm -rf coverage_html
llvm-cov show "$FIRST_BIN" "${OBJECT_ARGS[@]}" \
    -instr-profile=coverage.profdata --show-mcdc \
    -format=html -output-dir=coverage_html \
    -ignore-filename-regex='.*(googletest|[\\/]test[\\/]).*'

echo ""
llvm-cov report "$FIRST_BIN" "${OBJECT_ARGS[@]}" \
    -instr-profile=coverage.profdata --show-mcdc-summary \
    -ignore-filename-regex='.*(googletest|[\\/]test[\\/]).*'

echo ""
echo "==> 完了: coverage_html/index.html をブラウザで開いてください。"

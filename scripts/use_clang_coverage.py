"""
[env:native_coverage] 専用の SCons 拡張スクリプト。

コンパイル自体は CC/CXX という OS 環境変数を PlatformIO の `native`
プラットフォームが読むため、それだけで clang（llvm-mingw）へ切り替わる。
しかしリンクに使う `LINK` は別途固定値（`g++`）へ解決されてしまい、CC/CXX
の上書きだけではリンク工程が切り替わらない（2026-09、実際に
`undefined symbol: __llvm_profile_runtime` として踏んだ。clang++ をリンカ
として使わないと、プロファイリング用ランタイムが自動リンクされないため）。
`extra_scripts` の pre:/post: いずれのタイミングで `env.Replace(CC=...)`
を試しても、この後 PlatformIO の native プラットフォーム側が改めて
CC/CXX を（`gcc`/`g++` 固定で）上書きし直してしまい効かなかった
（実際に検証済み）。そのため本スクリプトでは CC/CXX には触れず、
CC/CXX の上書きは呼び出し側の環境変数に任せ、`LINK`/`AR`/`RANLIB`/
`LINKFLAGS` のみをここで上書きする（native プラットフォーム側は
これらを上書きし直さないため、こちらは有効）。

使い方: 事前に以下をすべて設定してから `pio test -e native_coverage` を
呼ぶこと（llvm-mingw、https://github.com/mstorsjo/llvm-mingw、の bin
ディレクトリへ絶対パスを通す。マシンごとに設置先が異なるため
platformio.ini/本スクリプトへはハードコードしない、
[[reference_pio_cli_location]] と同じ理由）:

    export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"
    export CC="$LLVM_MINGW_BIN/x86_64-w64-mingw32-clang.exe"
    export CXX="$LLVM_MINGW_BIN/x86_64-w64-mingw32-clang++.exe"
    pio test -e native_coverage
"""
Import("env")

import os

llvm_mingw_bin = os.environ.get("LLVM_MINGW_BIN")
if not llvm_mingw_bin or not os.environ.get("CC") or not os.environ.get("CXX"):
    print(
        "\n[native_coverage] ERROR: 環境変数 LLVM_MINGW_BIN/CC/CXX が未設定です。\n"
        "  本ファイル冒頭のコメント参照。例:\n"
        '    export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"\n'
        '    export CC="$LLVM_MINGW_BIN/x86_64-w64-mingw32-clang.exe"\n'
        '    export CXX="$LLVM_MINGW_BIN/x86_64-w64-mingw32-clang++.exe"\n'
    )
    Exit(1)

exe = lambda name: os.path.join(llvm_mingw_bin, name + ".exe")

env.Replace(
    LINK=exe("x86_64-w64-mingw32-clang++"),
    AR=exe("llvm-ar"),
    RANLIB=exe("llvm-ranlib"),
)

# clang はリンク時にも -fprofile-instr-generate を見て初めて
# プロファイリング用ランタイム（libclang_rt.profile 相当）を自動リンクする。
# PlatformIO の build_flags はコンパイル（CCFLAGS 等）にのみ渡り LINKFLAGS
# には伝播しないため、ここで明示的に追加する（2026-09、
# `undefined symbol: __llvm_profile_runtime` として実際に踏んだ）。
env.Append(LINKFLAGS=["-fprofile-instr-generate"])

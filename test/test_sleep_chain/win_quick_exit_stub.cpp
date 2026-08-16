/**
 * \file    win_quick_exit_stub.cpp
 * \brief   この Windows 環境の MinGW-w64 (msvcrt ランタイム版) 専用ワークアラウンド
 * \details libmingw32.a の ucrt_exit_wrappers.o が無条件に参照する
 *          `__imp_quick_exit`/`__imp__Exit`（UCRT 専用の DLL インポート
 *          シンボル）が、msvcrt ランタイムをターゲットにしたこの配布物には
 *          存在せずリンクエラーになる
 *          ("undefined reference to __imp_quick_exit" 等)。
 *          -lucrt を足して本物の UCRT をリンクすると、msvcrt と ucrt という
 *          異なる C ランタイムが同一バイナリに混在してしまい実行時に
 *          クラッシュする（2026-08 に確認済み）。
 *
 *          ここでは実際に DLL をリンクする代わりに、`__imp_quick_exit` という
 *          名前のグローバル関数ポインタ変数を自前で定義し、通常の
 *          `std::exit()` へ委譲するだけのスタブを指す値で満たすことで
 *          リンクエラーを解消する（PE のインポートサンクは「その名前の
 *          ポインタ変数経由で間接呼び出しする」だけの仕組みなので、
 *          実 DLL からのインポートである必要はない）。
 *          GoogleTest の death test 機構がプロセスを終了させる際にだけ
 *          参照される可能性があるパスであり、本プロジェクトのテストは
 *          ASSERT_DEATH 等を使わないため、通常のテスト実行では
 *          このスタブが実際に呼ばれることはない。
 */
#include <cstdlib>

extern "C" void QuickExitStub(int status)
{
    std::exit(status);
}

extern "C" void UnderscoreExitStub(int status)
{
    std::exit(status);
}

extern "C" void (*__imp_quick_exit)(int) = QuickExitStub;
extern "C" void (*__imp__Exit)(int)      = UnderscoreExitStub;

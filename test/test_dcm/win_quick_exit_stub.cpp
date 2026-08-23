/**
 * \file    win_quick_exit_stub.cpp
 * \brief   この Windows 環境の MinGW-w64 (msvcrt ランタイム版) 専用ワークアラウンド
 * \details test/test_chain/win_quick_exit_stub.cpp と同一内容。native 系
 *          env はそれぞれ別バイナリのため、env ごとに複製が必要
 *          （詳細は test/test_chain/win_quick_exit_stub.cpp のコメント参照）。
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

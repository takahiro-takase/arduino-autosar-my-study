/**
 * \file    win_quick_exit_stub.cpp
 * \brief   この Windows 環境の MinGW-w64 (msvcrt ランタイム版) 専用ワークアラウンド
 * \details test/test_chain/win_quick_exit_stub.cpp と同一内容。native 系
 *          env はそれぞれ別バイナリのため、env ごとに複製が必要
 *          （詳細は test/test_chain/win_quick_exit_stub.cpp のコメント参照）。
 */
#include <cstdlib>

/* `_UCRT`（llvm-mingw 等）では本物の __imp_quick_exit/__imp__Exit が既に
 * 存在するため本スタブは無効化する（test/test_native/win_quick_exit_stub.cpp
 * のコメント参照、2026-09 追加）。 */
#ifndef _UCRT

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

#endif /* _UCRT */

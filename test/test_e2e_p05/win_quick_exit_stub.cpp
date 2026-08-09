/**
 * \file    win_quick_exit_stub.cpp
 * \brief   この Windows 環境の MinGW-w64 (msvcrt ランタイム版) 専用ワークアラウンド
 * \details test/test_gpt/win_quick_exit_stub.cpp と同一内容。PlatformIO の
 *          Unit Testing は test/<name>/ ディレクトリごとに独立したテスト
 *          バイナリを作るため、このワークアラウンドもテストディレクトリごとに
 *          必要になる（詳細は test_gpt 版のコメント参照）。
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

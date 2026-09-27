/**
 * \file    Wrap_Dio.h
 * \brief   `src/Bsw/Dio/Dio.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          呼び出し記録のアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h` 冒頭コメント
 *          参照（「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `Dio.h` が宣言する公開API全8関数を wrap 対象とする。全関数の戻り値
 *          は `Dio_LevelType`/`Dio_PortLevelType`（成功/失敗を表さない値取得）
 *          または `void` のため、故障注入は実装せず呼び出し回数のみ記録する
 *          （`Dem.c` の `Dem_GetTranslationType` と同じ扱い）。
 *
 *          変数名・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_DIO_H
#define WRAP_DIO_H

#include "Std_Types.h"
#include "Dio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * 呼び出し回数（公開API全8関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_Dio_ReadChannel;
extern uint32 CallCount_Dio_WriteChannel;
extern uint32 CallCount_Dio_ReadPort;
extern uint32 CallCount_Dio_WritePort;
extern uint32 CallCount_Dio_ReadChannelGroup;
extern uint32 CallCount_Dio_WriteChannelGroup;
extern uint32 CallCount_Dio_GetVersionInfo;
extern uint32 CallCount_Dio_FlipChannel;

/** すべての関数呼び出し回数を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapDio_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_DIO_H */

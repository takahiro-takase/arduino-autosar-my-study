/**
 * \file    Hal_Millis_fake.h
 * \brief   Arduino の millis() のテスト用フェイク実装の宣言。
 * \details test/test_chain/Hal_Millis_fake.h と同一内容（env ごとに別
 *          バイナリのため複製）。Dcm_Cbk.c が S3 タイマ計測に
 *          `extern unsigned long millis(void);` を直接参照する。
 */
#ifndef HAL_MILLIS_FAKE_H
#define HAL_MILLIS_FAKE_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long FakeMillis_Value;

/** 各テストケースの開始時に呼び、0 に戻す。 */
void FakeMillis_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_MILLIS_FAKE_H */

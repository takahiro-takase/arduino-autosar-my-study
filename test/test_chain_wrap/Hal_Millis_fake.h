/**
 * \file    Hal_Millis_fake.h
 * \brief   Arduino の millis() のテスト用フェイク実装の宣言。
 * \details Com.c 等、複数の Bsw モジュールが `extern unsigned long millis(void);`
 *          で Arduino のグローバル関数を直接参照する。実機（uno_r4）では
 *          Arduino フレームワークが提供するが、native 環境にはこの関数自体が
 *          存在しないため、HAL 層のフェイクと同じ考え方で提供する。
 *          既定では固定値を返す（呼び出しごとに時間が進まない）。TxPeriodMs/
 *          MinDelayMs のような時間経過に依存するロジックをテストする場合は
 *          `FakeMillis_Value` を直接書き換えて進める。
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

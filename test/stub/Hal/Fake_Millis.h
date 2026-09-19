/**
 * \file    Fake_Millis.h
 * \brief   Arduino の millis() のテスト用フェイク実装の宣言。
 * \details Com.c 等、複数の Bsw モジュールが `extern unsigned long millis(void);`
 *          で Arduino のグローバル関数を直接参照する。実機（uno_r4）では
 *          Arduino フレームワークが提供するが、native 環境にはこの関数自体が
 *          存在しないため、HAL 層のフェイクと同じ考え方で提供する。
 *          既定では固定値を返す（呼び出しごとに時間が進まない）。TxPeriodMs/
 *          MinDelayMs のような時間経過に依存するロジックをテストする場合は
 *          `FakeMillis_Value` を直接書き換えて進める。
 *
 *          2026-09、`test_chain`/`test_dcm`/`test_fim`/`test_wdgm` の4envで
 *          内容が重複していたため、`Fake_Det_Hw.h` と同じ理由で
 *          `test/stub/Hal/` へ集約した。
 */
#ifndef FAKE_MILLIS_H
#define FAKE_MILLIS_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long FakeMillis_Value;

/** 各テストケースの開始時に呼び、0 に戻す。 */
void FakeMillis_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_MILLIS_H */

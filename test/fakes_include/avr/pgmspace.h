/**
 * \file    avr/pgmspace.h
 * \brief   ホスト(native)環境向け avr/pgmspace.h の最小シム
 * \details Det.h が `#include <avr/pgmspace.h>` を無条件に行うため、
 *          AVR/Renesas RA 以外のホスト GCC でビルドするテスト専用に、
 *          PROGMEM/PSTR/PGM_P を無害な等価物として定義する。
 *          `[env:native]` の `-I test/fakes_include` でのみ検索パスに
 *          乗るため、実機ビルド（uno_r4）には一切影響しない。
 */
#ifndef AVR_PGMSPACE_SHIM_H
#define AVR_PGMSPACE_SHIM_H

#define PROGMEM
#define PSTR(s) (s)

typedef const char* PGM_P;

#endif /* AVR_PGMSPACE_SHIM_H */

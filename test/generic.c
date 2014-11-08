// Copyright 2012 Rui Ueyama <rui314@gmail.com>
// This program is free software licensed under the MIT license.

#include <stdbool.h>
#include "test.h"

#ifdef __8cc__

static void test_basic(void) {
    expect(1, _Generic(5, int: 1, float: 2));
    expectd(3.0, _Generic(5.0, int: 1, float: 2.0, double: 3.0));
}

static void test_arith(void) {
    typedef signed char schar;
    typedef unsigned char uchar;
    typedef unsigned int uint;
    typedef unsigned long ulong;
    typedef long long llong;
    typedef unsigned long long ullong;
    typedef long double ldouble;

    enum { B, SC, UC, I, U, L, UL, LL, ULL, F, D, LD };

#define T(x)                                                                 \
    _Generic(x, bool:B, schar:SC, uchar:UC, int:I, uint:U, long:L, ulong:UL, \
             llong:LL, ullong:ULL, float:F, double:D, ldouble:LD)
    expect(B, T((bool)0));
    expect(SC, T((schar)0));
    expect(UC, T((uchar)0));
    expect(I, T(0));
    expect(U, T(0U));
    expect(L, T(0L));
    expect(UL, T(0UL));
    expect(LL, T(0LL));
    expect(ULL, T(0ULL));
    expect(F, T(0.0F));
    expect(D, T(0.0));
    expect(LD, T(0.0L));
    expect(I, T((bool)0 + (bool)0));
    expect(I, T((char)0 + (char)0));
    expect(I, T((char)0 + (uchar)0));
    expect(I, T(0 + (char)0));
    expect(U, T(0 + 0U));
    expect(L, T(0 + 0L));
    expect(LL, T(0LL + 0));
    expect(LL, T(0L + 0LL));
    expect(LL, T(0LL + 0U));
    expect(UL, T(0UL + 0));
    expect(UL, T(0L + 0UL));
    expect(LL, T(0LL + 0U));
    expect(ULL, T(0LU + 0LL));
    expect(ULL, T(0ULL + 0U));
    expect(ULL, T(0ULL + 0U));
    expect(D, T(0 + 0.0));
    expect(LD, T(0.0L + 0));
#undef T
}

static void test_default(void) {
    expect(1, _Generic(5, default: 1, float: 2));
    expectd(3.0, _Generic(5.0, int: 1, float: 2.0, default: 3.0));
}

static void test_struct() {
    struct t1 { int x, y; } v1;
    struct t2 { int x, y, z; } v2;
    expect(10, _Generic(v1, struct t1: 10, struct t2: 11, default: 12));
    expect(11, _Generic(v2, struct t1: 10, struct t2: 11, default: 12));
    expect(12, _Generic(99, struct t1: 10, struct t2: 11, default: 12));
}

static void test_array() {
    expect(20, _Generic("abc", char[4]: 20, default: 21));
    expect(22, _Generic((int*)NULL, int *: 22, default: 23));
    expect(23, _Generic((int*)NULL, int[1]: 22, default: 23));
}

void testmain(void) {
    print("_Generic");
    test_basic();
    test_arith();
    test_default();
    test_struct();
    test_array();
}

#else

void testmain(void) {
    print("_Generic");
}

#endif

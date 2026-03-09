#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>

/* 表示フォーマット指定 */
typedef enum { FMT_ALL = 0, FMT_DEC, FMT_HEX, FMT_OCT, FMT_BIN } VFmt;

/* 計算結果の型: 整数(int64_t) または 浮動小数点(double) */
typedef struct {
    int     is_float;
    int64_t ival;
    double  dval;
    VFmt    fmt;
    int     warn;  /* 1: 桁溢れにより精度が失われた可能性あり */
} Value;

static inline Value val_int(int64_t v) { Value r = {0, v, 0.0, FMT_ALL, 0}; return r; }
static inline Value val_flt(double  v) { Value r = {1, 0, v,   FMT_ALL, 0}; return r; }
static inline double to_d(Value v)     { return v.is_float ? v.dval : (double)v.ival; }

#endif /* VALUE_H */

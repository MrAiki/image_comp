#ifndef PREDICTIVE_CODING_H_INCLUDED
#define PREDICTIVE_CODING_H_INCLUDED

#include <stdint.h>

/* 引数は予測対象の画素に対する位置を示している */
/*
 * n: 一つ上（north）
 * w: 一つ左（west）
 * nw: 左斜め上（north-west）
 */

/* LOCO-Iによる予測 */
int32_t PredictiveCoding_LOCOI(int32_t w, int32_t n, int32_t nw);

/* LOCO-Iによる予測（改造版） */
int32_t PredictiveCoding_MyLOCOI(int32_t w,
    int32_t n, int32_t nw, int32_t nn, int32_t ww);

#endif /* PREDICTIVE_CODING_H_INCLUDED */

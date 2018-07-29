#include "predictive_coding.h"

/* 2値のうち最大を取得 */
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
/* 2値のうち最小を取得 */
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
/* 絶対値を取得 */
#define ABS(val) (((val) > 0) ? (val) : -(val))

/* LOCO-Iによる予測 */
int32_t PredictiveCoding_LOCOI(int32_t a, int32_t b, int32_t c)
{
  if (b >= MAX(a,c)) {
    return MIN(a,c);
  } else if (b <= MIN(a,c)) {
    return MAX(a,c);
  } 
  
  return a + c - b;
}

/* LOCO-Iによる予測（改造版） */
int32_t PredictiveCoding_MyLOCOI(int32_t w,
    int32_t n, int32_t nw, int32_t nn, int32_t ww)
{
  int32_t predict;

  /* エッジ予測 */
  if (nw >= MAX(n,w)) {
    predict = MIN(n,w);
    if (ABS(nw - MAX(n,w)) > 20) {
      if (n < w) {
        if (nn > n) {
          predict = MAX(n - (nn - n) / 2, 0);
        } 
      } else {
        if (ww > w) {
          predict = MAX(w - (ww - w) / 2, 0);
        } 
      }
    }
    return predict;
  } else if (nw <= MIN(n,w)) {
    predict = MAX(n,w);
    if (ABS(nw - MIN(n,w)) > 20) {
      if (n > w) {
        if (nn < n) {
          predict = MIN(n + (n - nn) / 2, 255);
        } 
      } else {
        if (ww < w) {
          predict = MIN(w + (w - ww) / 2, 255);
        } 
      }
    }
    return predict;
  } 
  
  /* 平面予測 */
  return n + w - nw;
}


#include "lpc.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

int32_t LPC_LevinsonDurbinRecursion(float *lpc_coef, const float *auto_corr, uint32_t max_order)
{
  uint32_t delay, i_delay;
  float lambda;
  float *u_vec, *v_vec, *a_vec, *e_vec;

  if (lpc_coef == NULL || auto_corr == NULL) {
    fprintf(stderr, "Data or result pointer point to NULL. \n");
    return -1;
  }

  /* 
   * 0次自己相関（信号の二乗和）が0に近い場合、入力信号は無音と判定
   * => 予測誤差, LPC係数は全て0として無音出力システムを予測.
   */
  if (fabs(auto_corr[0]) < FLT_EPSILON) {
    for (i_delay = 0; i_delay < max_order+1; ++i_delay) {
      lpc_coef[i_delay] = 0.0f;
    }
    return 0;
  }

  /* 初期化 */
  a_vec = (float *)malloc(sizeof(float) * max_order + 2); /* a_0, a_k+1を含めるとmax_order+2 */
  e_vec = (float *)malloc(sizeof(float) * max_order + 2); /* e_0, e_k+1を含めるとmax_order+2 */
  u_vec = (float *)malloc(sizeof(float) * max_order + 2);
  v_vec = (float *)malloc(sizeof(float) * max_order + 2);
  for (i_delay = 0; i_delay < max_order+2; i_delay++) {
    u_vec[i_delay] = v_vec[i_delay] = a_vec[i_delay] = 0.0f;
  }

  /* 最初のステップの係数をセット */
  a_vec[0] = e_vec[0] = 1.0f;
  a_vec[1] = - auto_corr[1] / auto_corr[0];
  e_vec[1] = auto_corr[0] + auto_corr[1] * a_vec[1];
  u_vec[0] = 1.0f; u_vec[1] = 0.0f; 
  v_vec[0] = 0.0f; v_vec[1] = 1.0f; 

  /* 再帰処理 */
  for (delay = 1; delay < max_order; delay++) {
    lambda = 0.0f;
    for (i_delay = 0; i_delay < delay+1; i_delay++) {
      lambda += a_vec[i_delay] * auto_corr[delay+1-i_delay];
    }
    lambda /= (-e_vec[delay]);
    e_vec[delay+1] = (1 - lambda * lambda) * e_vec[delay];

    /* u_vec, v_vecの更新 */
    for (i_delay = 0; i_delay < delay; i_delay++) {
      u_vec[i_delay+1] = v_vec[delay-i_delay] = a_vec[i_delay+1];
    }
    u_vec[0] = 1.0f; u_vec[delay+1] = 0.0f;
    v_vec[0] = 0.0f; v_vec[delay+1] = 1.0f;

    /* resultの更新 */
    for (i_delay = 0; i_delay < delay + 2; i_delay++) {
       a_vec[i_delay] = u_vec[i_delay] + lambda * v_vec[i_delay];
    }

  }

  /* 結果の取得 */
  memcpy(lpc_coef, a_vec, sizeof(float) * (max_order + 1));

  free(u_vec); free(v_vec);
  free(a_vec); free(e_vec); 

  return 0;
}

int32_t LPC_CalculateAutoCorrelation(float *auto_corr, const float *data, uint32_t num_sample, uint32_t max_order)
{
  int32_t i_sample, delay_time;

  if (max_order > num_sample) {
    fprintf(stderr, "Max order(%u) is larger than number of samples(%u). \n", max_order, num_sample);
    return -1;
  }

  if (auto_corr == NULL || data == NULL) {
    fprintf(stderr, "Data or result pointer point to NULL. \n");
    return -2;
  }

  /* （標本）自己相関の計算 */
  for (delay_time = 0; delay_time < (int32_t)max_order; delay_time++) {
    auto_corr[delay_time] = 0.0f;
    for (i_sample = 0; i_sample < (int32_t)num_sample; i_sample++) {
      /* 係数が0以上の時のみ和を取る */
      if (i_sample - delay_time >= 0) {
        auto_corr[delay_time] += data[i_sample] * data[i_sample-delay_time];
      }
    }
    /* 平均を取る */
    auto_corr[delay_time] /= (num_sample-delay_time);
  }

  return 0;
}

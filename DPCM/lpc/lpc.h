#ifndef LPC_H_INCLUDED
#define LPC_H_INCLUDED

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif 

/*
 * @brief （標本）自己相関の計算
 * @param[in,out] auto_corr  結果の配列.第i要素にi次の自己相関を持ちます。サイズはmax_order+1となる事に注意して下さい
 * @param[in]     data       信号データ（単一時系列データ）
 * @param[in]     num_sample 信号のサンプル数
 * @param[in]     max_order  自己相関の最大次数
 */
int32_t LPC_CalculateAutoCorrelation(float *auto_corr, const float *data, uint32_t num_sample, uint32_t max_order);

/*
 * @brief Levinson-Durbin再帰計算によりLPC係数を求めます
 * @param[in,out] lpc_coef  LPC係数. サイズはmax_order+1になることに注意して下さい
 * @param[in]     auto_corr 自己相関. サイズはmax_order+1になることに注意して下さい
 * @param[in]     max_order 自己相関の最大次数
 * @return 正常に計算できた場合は0を返し、失敗した場合は負値を返します
 */
int32_t LPC_LevinsonDurbinRecursion(float *lpc_coef, const float *auto_corr, uint32_t max_order);

#ifdef __cplusplus
}
#endif

#endif /* LPC_H_INCLUDED */

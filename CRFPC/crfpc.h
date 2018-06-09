#ifndef CRFPC_H_INCLUDED
#define CRFPC_H_INCLUDED

#include <stdint.h>

/* Conditional Random Field for Prediction Coding */

/* デフォルトのコンフィグを設定 */
#define CRFPCTrainer_SetDefaultConfig(p_config) {         \
  struct CRFPCTrainerConfig* p_dummy_config = p_config;   \
  p_dummy_config->learning_rate             = 0.5f;       \
  p_dummy_config->epsilon                   = 1.0e-6;     \
  p_dummy_config->param_variance            = FLT_MAX;    \
}

/* 連想ポテンシャル関数型 */
typedef float (*AssociativePotentialFunc)(uint8_t y, uint8_t x);

/* 相互ポテンシャル関数型 */
typedef float (*InteractivePotentialFunc)(uint8_t y, uint8_t x_i, uint8_t x_j);

/* APIの成否 */
typedef enum CRFPCApiResultTag {
  CRFPC_API_RESULT_OK = 0,
  CRFPC_API_RESULT_NG,
} CRFPCApiResult;

/* モデル */
struct CRFPCModel;

/* モデルのトレーナ */
struct CRFPCTrainer;

/* トレーナ初期化コンフィグ */
struct CRFPCTrainerConfig {
  float  learning_rate;  /* 学習率 */
  float  epsilon;        /* 学習収束判定値 */
  float  param_variance; /* 分散パラメータ（FLT_MAXでほぼ無効に） */
};

/* モデル作成 */
struct CRFPCModel* CRFPCModel_Create(uint32_t dim_input, AssociativePotentialFunc f, InteractivePotentialFunc g);

/* モデル破棄 */
void CRFPCModel_Destroy(struct CRFPCModel* model);

/* 条件付き確率P(y|x)の計算 */
float CRFPCModel_CalculateConditionalProbability(const struct CRFPCModel* model, uint8_t y, const uint8_t* x, uint32_t dim_x);

/* 予測 */
uint8_t CRFPCModel_Predict(const struct CRFPCModel* model, const uint8_t* x, uint32_t dim_x);

/* トレーナの作成 */
struct CRFPCTrainer* CRFPCTrainer_Create(const struct CRFPCTrainerConfig* config);

/* トレーナの破棄 */
void CRFPCTrainer_Destroy(struct CRFPCTrainer* trainter);

/* トレーナに学習サンプルをセット */
/* 学習サンプルは参照渡し */
CRFPCApiResult CRFPCTrainer_SetSample(struct CRFPCTrainer* trainter, const uint8_t** sample_data, uint8_t* sample_label, uint32_t num_samples, uint32_t dim_input);

/* 学習 */
CRFPCApiResult CRFPCTrainer_Learning(struct CRFPCTrainer* trainter, struct CRFPCModel* model, uint32_t max_num_iteration);

#endif /* CRFPC_H_INCLUDED */

#include "crfpc.h"
#include <math.h>
#include <stdlib.h>
#include <float.h>

/* for logoutput */
#include <stdio.h>

/* 最大値/最小値 */
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define MIN(x,y) (((x) > (y)) ? (y) : (x))

/* n C 2 を計算する */
#define CONBINATION_OF_2(n) ((n) * ((n) - 1) / 2)

/* 対象なインデックスへのアクセサ */
#define SYMMETRIC_INDEX_AT(dim, i, j) SYMMETRIC_INDEX_AT_SUB(dim, MAX(i,j), MIN(i,j))
/* アクセサのサブルーチン i > j が前提
 * (((dim) * (j)) - ((j) * (j + 1)) / 2) で行の先頭インデックス、
 * ((i) - (j) - 1) で行オフセット */
#define SYMMETRIC_INDEX_AT_SUB(dim, i, j) ((((dim) * (j)) - ((j) * (j + 1)) / 2) + ((i) - (j) - 1))

/* モデル */
struct CRFPCModel {
  uint32_t                  dim_input;  /* 入力ベクトルの次元 */
  float*                    lambda;     /* 連想ポテンシャルのパラメータ */
  float*                    mu;         /* 相互ポテンシャルのパラメータ */
  AssociativePotentialFunc  f;          /* 連想ポテンシャル */
  InteractivePotentialFunc  g;          /* 相互ポテンシャル */
};

/* モデルのトレーナ */
struct CRFPCTrainer {
  float               learning_rate;    /* 学習率               */
  float               epsilon;          /* 収束判定値           */
  const uint8_t**     sample_data;      /* サンプルデータの参照 */
  const uint8_t*      sample_label;     /* サンプルラベルの参照 */
  uint32_t            dim_input;        /* 入力データの次元     */
  uint32_t            num_samples;      /* サンプル数           */
  float               param_variance;   /* 分散パラメータ       */
};

/* 入力に対するポテンシャルの計算 */
static float CRFPCModel_CalculatePotensial(const struct CRFPCModel* model, uint8_t y, const uint8_t* x, uint32_t dim_x);

/* 正規化定数Z(x)の計算 */
static float CRFPCModel_CalculateNormalizeFactor(const struct CRFPCModel* model, const uint8_t* x, uint32_t dim_x);

/* 対数尤度の計算 */
static float CRFPCTrainer_CalculateLogLikelihood(const struct CRFPCTrainer* trainter, const struct CRFPCModel* model);

/* 連想ポテンシャルの対数尤度勾配の計算 */
static float CRFPCTrainer_CalculateLogDiffLambda(const struct CRFPCTrainer* trainter, const struct CRFPCModel* model, uint32_t lambda_index);

/* 相互ポテンシャルの対数尤度勾配の計算 */
static float CRFPCTrainer_CalculateLogDiffMu(const struct CRFPCTrainer* trainter, const struct CRFPCModel* model, uint32_t i_param_index, uint32_t j_param_index);

/* モデル作成 */
struct CRFPCModel* CRFPCModel_Create(uint32_t dim_input, AssociativePotentialFunc f, InteractivePotentialFunc g)
{
  struct CRFPCModel* model;
  uint32_t i_param;

  /* 引数チェック */
  if (dim_input == 0 
      || f == NULL || g == NULL) {
    return NULL;
  }

  model = malloc(sizeof(struct CRFPCModel));

  model->dim_input  = dim_input;
  model->f          = f;
  model->g          = g;

  model->lambda     = malloc(sizeof(float) * dim_input);
  /* dim_input C 2 の組み合わせ分確保 */
  model->mu         = malloc(sizeof(float) * CONBINATION_OF_2(dim_input));

  /* パラメータを初期化しておく */
  for (i_param = 0; i_param < dim_input; i_param++) {
    model->lambda[i_param] = 0.0f;
  }
  for (i_param = 0; i_param < CONBINATION_OF_2(dim_input); i_param++) {
    model->mu[i_param] = 0.0f;
  }

  return model;
}

/* モデル破棄 */
void CRFPCModel_Destroy(struct CRFPCModel* model)
{
  if (model != NULL) {
    free(model->lambda);
    free(model->mu);
    free(model);
  }
}

/* 条件付き確率P(y|x)の計算 */
float CRFPCModel_CalculateConditionalProbability(const struct CRFPCModel* model,
    uint8_t y, const uint8_t* x, uint32_t dim_x)
{
  float pyx;

  /* 引数チェック */
  if (model == NULL || x == NULL
      || dim_x != model->dim_input) {
    return -1.0f;
  }

  /* P(y,x) の計算 */
  pyx = exp(CRFPCModel_CalculatePotensial(model, y, x, dim_x));
  /* 正規化定数で割る */
  pyx /= CRFPCModel_CalculateNormalizeFactor(model, x, dim_x);

  return pyx;
}

/* 正規化定数Z(x)の計算 */
static float CRFPCModel_CalculateNormalizeFactor(const struct CRFPCModel* model, 
    const uint8_t* x, uint32_t dim_x)
{
  float sum_exp;
  uint16_t y; 

  if (model == NULL || x == NULL
      || dim_x != model->dim_input) {
    return 0.0f;
  }

  sum_exp = 0.0f;
  for (y = 0; y <= 0xFF; y++) {
    sum_exp += exp(CRFPCModel_CalculatePotensial(model, y, x, dim_x));
  }

  return sum_exp;
}

/* 入力に対するポテンシャルの計算 */
static float CRFPCModel_CalculatePotensial(const struct CRFPCModel* model, uint8_t y, const uint8_t* x, uint32_t dim_x)
{
  uint32_t i_param, j_param;
  float potensial;

  if (model == NULL || x == NULL
      || dim_x != model->dim_input) {
    return 0.0f;
  }

  potensial = 0.0f;

  /* 連想ポテンシャルの計算 */
  for (i_param = 0; i_param < model->dim_input; i_param++) {
    potensial += model->lambda[i_param] * model->f((uint8_t)y, x[i_param]);
  }
  /* 相互ポテンシャルの計算 */
  for (i_param = 0; i_param < model->dim_input; i_param++) {
    for (j_param = i_param+1; j_param < model->dim_input; j_param++) {
      potensial 
        += model->mu[SYMMETRIC_INDEX_AT(model->dim_input, i_param, j_param)] * model->g((uint8_t)y, x[i_param], x[j_param]);
    }
  }

  /* 負号をとって返す */
  return -potensial;
}

/* 予測 */
uint8_t CRFPCModel_Predict(const struct CRFPCModel* model, const uint8_t* x, uint32_t dim_x)
{
  float max_prob, prob;
  uint16_t y;
  uint8_t max_y;

  /* 引数チェック */
  if (model == NULL
      || x == NULL || dim_x == 0) {
    return 0;
  }

  /* 最大の確率値を持つyを探す */
  /* 本応用ではポテンシャル比較でも可能 */
  max_prob = -FLT_MAX;
  for (y = 0; y <= 0xFF; y++) {
    prob = CRFPCModel_CalculateConditionalProbability(model, y, x, dim_x);
    if (prob > max_prob) {
      max_prob = prob;
      max_y    = y;
    }
  }

  return max_y;
}

/* トレーナの作成 */
struct CRFPCTrainer* CRFPCTrainer_Create(const struct CRFPCTrainerConfig* config)
{
  struct CRFPCTrainer* trainter;

  /* 引数チェック */
  if (config == NULL) {
    return NULL;
  }

  trainter = malloc(sizeof(struct CRFPCTrainer));

  trainter->learning_rate   = config->learning_rate;
  trainter->epsilon         = config->epsilon;
  trainter->param_variance  = config->param_variance;

  /* サンプルはNULL初期化 */
  trainter->sample_data     = NULL;
  trainter->sample_label    = NULL;

  return trainter;
}

/* トレーナの破棄 */
/* サンプルは勝手に破棄しない */
void CRFPCTrainer_Destroy(struct CRFPCTrainer* trainter)
{
  if (trainter != NULL) {
    free(trainter);
  }
}

/* トレーナに学習サンプルをセット */
/* 学習サンプルの参照は再度セットされるまで残るため注意 */
CRFPCApiResult CRFPCTrainer_SetSample(struct CRFPCTrainer* trainter, const uint8_t** sample_data, uint8_t* sample_label, uint32_t num_samples, uint32_t dim_input)
{
  /* 引数チェック */
  if (trainter == NULL
      || sample_data == NULL
      || sample_label == NULL) {
    return CRFPC_API_RESULT_NG;
  }

  /* サンプルデータとラベルの参照をセット */
  trainter->sample_data   = sample_data;
  trainter->sample_label  = sample_label;
  trainter->num_samples   = num_samples;
  trainter->dim_input     = dim_input;

  return CRFPC_API_RESULT_OK;
}

/* 学習 */
CRFPCApiResult CRFPCTrainer_Learning(struct CRFPCTrainer* trainter, struct CRFPCModel* model, uint32_t max_num_iteration)
{
  uint32_t  iteration_count;
  float     diff_param_rms;
  float     likelihood, prev_likelihood;
  uint32_t  dim;
  float*    diff_lambda;
  float*    diff_mu;

  if (trainter == NULL) {
    return CRFPC_API_RESULT_NG;
  }

  /* 次元の整合性確認 */
  if (trainter->dim_input != model->dim_input) {
    return CRFPC_API_RESULT_NG;
  }

  /* メンバから値を取得しておく */
  dim         = model->dim_input;
  diff_lambda = alloca(sizeof(float) * dim);
  diff_mu     = alloca(sizeof(float) * CONBINATION_OF_2(dim));

  /* 学習ループ */
  iteration_count = 0;
  prev_likelihood = -FLT_MAX;
  while (iteration_count < max_num_iteration) {
    uint32_t  i_param, j_param;
    float     diff_param;

    /* 前の対数尤度値を保存 */
    if (iteration_count > 0) {
      prev_likelihood = likelihood;
    }

    /* 対数尤度計算 */
    likelihood = CRFPCTrainer_CalculateLogLikelihood(trainter, model);

    /* 各パラメータの勾配を計算 */
    diff_param_rms = 0.0f;
    for (i_param = 0; i_param < dim; i_param++) {
      diff_param = CRFPCTrainer_CalculateLogDiffLambda(trainter, model, i_param);
      diff_lambda[i_param] = diff_param;
      diff_param_rms += diff_param * diff_param;
    }
    for (i_param = 0; i_param < dim; i_param++) {
      for (j_param = i_param + 1; j_param < dim; j_param++) {
        diff_param = CRFPCTrainer_CalculateLogDiffMu(trainter, model, i_param, j_param);
        diff_mu[SYMMETRIC_INDEX_AT(dim, i_param, j_param)] = diff_param;
        diff_param_rms += diff_param * diff_param;
      }
    }
    diff_param_rms = sqrt(diff_param_rms / (dim + CONBINATION_OF_2(dim)));

    /* 収束判定 */
    if (diff_param_rms < trainter->epsilon
        || fabsf(prev_likelihood - likelihood) < trainter->epsilon) {
      break;
    }

    /* 進捗を表示 */
    fprintf(stdout, "[%d] Log Likelihood:%f ParamRMS:%f \n", iteration_count, likelihood, diff_param_rms);
    /* パラメータダンプ */
#if 0
    fprintf(stdout, "[%d]", iteration_count);
    for (i_param = 0; i_param < dim; i_param++) {
      fprintf(stdout, "%f ", model->lambda[i_param]);
    }
    for (i_param = 0; i_param < CONBINATION_OF_2(dim); i_param++) {
      fprintf(stdout, "%f ", model->mu[i_param]);
    }
    fprintf(stdout, "\n");
#endif

    /* パラメータ更新 */
    for (i_param = 0; i_param < dim; i_param++) {
      model->lambda[i_param] += trainter->learning_rate * diff_lambda[i_param];
    }
    for (i_param = 0; i_param < dim; i_param++) {
      for (j_param = i_param+1; j_param < dim; j_param++) {
        model->mu[SYMMETRIC_INDEX_AT(dim, i_param, j_param)]
          += trainter->learning_rate * diff_mu[SYMMETRIC_INDEX_AT(dim, i_param, j_param)];
      }
    }

    iteration_count++;
  }

  return CRFPC_API_RESULT_OK;
}

/* 対数尤度の計算 */
static float CRFPCTrainer_CalculateLogLikelihood(const struct CRFPCTrainer* trainter, const struct CRFPCModel* model)
{
  float                     log_like;
  uint32_t                  i_sample, i_param;
  uint32_t                  dim;
  const uint8_t**           sample_data;
  const uint8_t*            sample_label;
  AssociativePotentialFunc  assoc_func;
  InteractivePotentialFunc  inter_func;
  float                     param_sum_square;

  if (trainter == NULL || model == NULL) {
    return 0.0f;
  }

  /* メンバから値を取得しておく */
  dim           = model->dim_input;
  sample_data   = trainter->sample_data;
  sample_label  = trainter->sample_label;
  assoc_func    = model->f;
  inter_func    = model->g;

  log_like = 0.0f;
  for (i_sample = 0; i_sample < trainter->num_samples; i_sample++) {
    /* ポテンシャル */
    log_like -=
      CRFPCModel_CalculatePotensial(model, sample_label[i_sample], sample_data[i_sample], dim);

    /* log(正規化定数) */
    log_like -=
      logf(CRFPCModel_CalculateNormalizeFactor(model, sample_data[i_sample], dim));
  }

  /* 正規化項 */
  /* 連想ポテンシャル分 */
  param_sum_square = 0.0f;
  for (i_param = 0; i_param < dim; i_param++) {
    param_sum_square += model->lambda[i_param] * model->lambda[i_param];
  }
  /* 相互ポテンシャル分 */
  for (i_param = 0; i_param < CONBINATION_OF_2(dim); i_param++) {
    param_sum_square += model->mu[i_param] * model->mu[i_param];
  }
  log_like -= param_sum_square / (2.0f * trainter->param_variance);

  return log_like;
}

/* 連想ポテンシャルの対数尤度勾配の計算 */
static float CRFPCTrainer_CalculateLogDiffLambda(const struct CRFPCTrainer* trainter, const struct CRFPCModel* model, uint32_t lambda_index)
{
  float                     log_diff_like;
  float                     pyz;
  uint32_t                  i_sample;
  uint16_t                  y;
  uint32_t                  dim;
  const uint8_t**           sample_data;
  const uint8_t*            sample_label;
  AssociativePotentialFunc  assoc_func;

  if (trainter == NULL || model == NULL) {
    return 0.0f;
  }

  /* メンバから値を取得しておく */
  dim           = model->dim_input;
  sample_data   = trainter->sample_data;
  sample_label  = trainter->sample_label;
  assoc_func    = model->f;

  log_diff_like = 0.0f;

  for (i_sample = 0; i_sample < trainter->num_samples; i_sample++) {
    /* 連想ポテンシャルの和 */
    log_diff_like -=
      assoc_func(sample_label[i_sample], sample_data[i_sample][lambda_index]);

    /* 連想ポテンシャルの条件付き期待値 */
    pyz = 0.0f;
    for (y = 0; y <= 0xFF; y++) {
      float poten = CRFPCModel_CalculatePotensial(model, y, sample_data[i_sample], dim);
      pyz += assoc_func(y, sample_data[i_sample][lambda_index]) * exp(poten);
    }
    pyz /= CRFPCModel_CalculateNormalizeFactor(model, sample_data[i_sample], dim);

    log_diff_like += pyz;
  }

  /* 正規化項 */
  log_diff_like -= model->lambda[lambda_index] / trainter->param_variance;

  return log_diff_like;
}

/* 相互ポテンシャルの対数尤度勾配の計算 */
static float CRFPCTrainer_CalculateLogDiffMu(const struct CRFPCTrainer* trainter, const struct CRFPCModel* model,
    uint32_t i_param_index, uint32_t j_param_index)
{
  float                     log_diff_like;
  float                     pyz;
  uint32_t                  i_sample;
  uint16_t                  y;
  uint32_t                  dim;
  const uint8_t**           sample_data;
  const uint8_t*            sample_label;
  InteractivePotentialFunc  inter_func;

  if (trainter == NULL || model == NULL) {
    return 0.0f;
  }

  /* メンバから値を取得しておく */
  dim           = model->dim_input;
  sample_data   = trainter->sample_data;
  sample_label  = trainter->sample_label;
  inter_func    = model->g;

  log_diff_like = 0.0f;

  for (i_sample = 0; i_sample < trainter->num_samples; i_sample++) {
    /* 相互ポテンシャルの和 */
    log_diff_like -=
      inter_func(sample_label[i_sample],
          sample_data[i_sample][i_param_index], sample_data[i_sample][j_param_index]);

    /* 相互ポテンシャルの条件付き期待値 */
    pyz = 0.0f;
    for (y = 0; y <= 0xFF; y++) {
      float poten = CRFPCModel_CalculatePotensial(model, y, sample_data[i_sample], dim);
      pyz += inter_func(y, sample_data[i_sample][i_param_index], sample_data[i_sample][j_param_index]) * exp(poten);
    }
    pyz /= CRFPCModel_CalculateNormalizeFactor(model, sample_data[i_sample], dim);

    log_diff_like += pyz;
  }

  /* 正規化項 */
  log_diff_like -= model->mu[SYMMETRIC_INDEX_AT(dim, i_param_index, j_param_index)] / trainter->param_variance;

  return log_diff_like;
}

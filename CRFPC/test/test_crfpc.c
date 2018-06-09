#include "test.h"
#include <stdlib.h>
#include <stdio.h>

/* テスト対象のモジュール */
#include "../crfpc.c"

/* 連想ポテンシャル1: 0を返す */
static float assoc_func_zero(uint8_t y, uint8_t x)
{
  TEST_UNUSED_PARAMETER(y);
  TEST_UNUSED_PARAMETER(x);
  return 0.0f;
}

/* 連想ポテンシャル1: 正規化絶対値 */
/* -> ラプラス分布に近づく。 */
static float assoc_func_abs(uint8_t y, uint8_t x)
{
  return fabsf((float)y - (float)x) / 0xFF;
}

/* 連想ポテンシャル2: 正規化2乗ノルム */
/* -> ガウス分布に近づく。 */
static float assoc_func_2norm(uint8_t y, uint8_t x)
{
  float diff = ((float)y - (float)x);
  return diff * diff / 0xFFFF;
}

/* 相互ポテンシャル1: 0を返す */
static float inter_func_zero(uint8_t y, uint8_t x_i, uint8_t x_j)
{
  TEST_UNUSED_PARAMETER(y);
  TEST_UNUSED_PARAMETER(x_i);
  TEST_UNUSED_PARAMETER(x_j);
  return 0.0f;
}

/* 相互ポテンシャル1: 正規化絶対値 */
static float inter_func_abs(uint8_t y, uint8_t x_i, uint8_t x_j)
{
  TEST_UNUSED_PARAMETER(y);
  return fabsf((float)x_i - (float)x_j) / 0xFF;
}

/* 相互ポテンシャル2: 正規化2乗ノルム */
static float inter_func_2norm(uint8_t y, uint8_t x_i, uint8_t x_j)
{
  TEST_UNUSED_PARAMETER(y);
  float diff = ((float)x_i - (float)x_j);
  return diff * diff / 0xFFFF;
}

int testCRFPCModel_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testCRFPCModel_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* 対象なアクセサのテスト */
void testCRFPCModel_SymmetricAccessTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* dim == 2 */
  Test_AssertEqual(SYMMETRIC_INDEX_AT(2, 0, 1), 0);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(2, 1, 0), 0);

  /* dim == 3 */
  Test_AssertEqual(SYMMETRIC_INDEX_AT(3, 1, 0), 0);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(3, 2, 0), 1);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(3, 1, 2), 2);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(3, 0, 1), 0);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(3, 0, 2), 1);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(3, 2, 1), 2);

  /* dim == 4 */
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 1, 0), 0);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 2, 0), 1);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 3, 0), 2);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 2, 1), 3);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 3, 1), 4);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 3, 2), 5);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 0, 1), 0);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 0, 2), 1);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 0, 3), 2);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 1, 2), 3);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 1, 3), 4);
  Test_AssertEqual(SYMMETRIC_INDEX_AT(4, 2, 3), 5);
}

/* 生成破棄テスト */
void testCRFPCModel_CreateDestroyTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  
  /* 正常系 */
  {
    struct CRFPCModel* model;
    model = CRFPCModel_Create(2, assoc_func_abs, inter_func_abs);
    Test_AssertCondition(model != NULL);
    Test_AssertEqual(model->dim_input, 2);
    Test_AssertCondition(model->f == assoc_func_abs);
    Test_AssertCondition(model->g == inter_func_abs);
    CRFPCModel_Destroy(model);
  }

  /* 失敗系 */
  {
    Test_AssertCondition(CRFPCModel_Create(0, assoc_func_abs, inter_func_abs) == NULL);
    Test_AssertCondition(CRFPCModel_Create(2, NULL, inter_func_abs) == NULL);
    Test_AssertCondition(CRFPCModel_Create(2, assoc_func_abs, NULL) == NULL);
  }
}

/* 確率計算テスト */
void testCRFPCModel_CalculateProbabilityTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  Test_SetFloat32Epsilon(0.001f);

  {
#define TEST_DATA_DIM 3
    uint8_t   x[TEST_DATA_DIM] = {0,};
    uint16_t  y;
    int32_t   is_ok;
    struct CRFPCModel* model;
    model = CRFPCModel_Create(TEST_DATA_DIM, assoc_func_zero, inter_func_zero);

    /* 0を返す定数なので 256 * exp(0) = 256 となるはず */
    Test_AssertFloat32EpsilonEqual(CRFPCModel_CalculateNormalizeFactor(model, x, TEST_DATA_DIM), 256.0f);

    /* すべて当確率 1/256 で出現するはず */
    is_ok = 1;
    for (y = 0; y <= 0xFF; y++) {
      if (fabsf(CRFPCModel_CalculateConditionalProbability(model, y, x, TEST_DATA_DIM) - 1.0f / 256) > 0.001f) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    CRFPCModel_Destroy(model);
#undef TEST_DATA_DIM
  }

}

/* 予測テスト */
void testCRFPCModel_PredictTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  {
#define TEST_DATA_DIM 2
    uint8_t   x[TEST_DATA_DIM] = {0,};
    uint16_t  y;
    uint32_t  i_param, i_x;
    int32_t   is_ok;
    struct CRFPCModel* model;

    model = CRFPCModel_Create(TEST_DATA_DIM, assoc_func_abs, inter_func_zero);

    /* 連想ポテンシャルのパラメータはすべて同一に */
    for (i_param = 0; i_param < TEST_DATA_DIM; i_param++) {
      model->lambda[i_param] = 1.0f;
    }

    /* 連想ポテンシャル関数が差の絶対値なので、
     * 入力したラベルと同一のものが予測されるはず */
    is_ok = 1;
    for (y = 0; y <= 0xFF; y++) {
      for (i_x = 0; i_x < TEST_DATA_DIM; i_x++) {
        x[i_x] = y;
      }
      if (CRFPCModel_Predict(model, x, TEST_DATA_DIM) != y) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    CRFPCModel_Destroy(model);
#undef TEST_DATA_DIM
  }
}

/* 生成破棄テスト */
void testCRFPCTrainer_CreateDestroyTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    struct CRFPCTrainer *trainter;
    struct CRFPCTrainerConfig config;

    CRFPCTrainer_SetDefaultConfig(&config);
    trainter = CRFPCTrainer_Create(&config);
    Test_AssertCondition(trainter != NULL);
    Test_AssertCondition(trainter->sample_data == NULL);
    Test_AssertCondition(trainter->sample_label == NULL);
    Test_AssertFloat32EpsilonEqual(trainter->learning_rate, config.learning_rate);
    Test_AssertFloat32EpsilonEqual(trainter->epsilon, config.epsilon);
    Test_AssertFloat32EpsilonEqual(trainter->param_variance, config.param_variance);

    CRFPCTrainer_Destroy(trainter);
  }
}

/* サブモジュールの単体テスト */
void testCRFPCTrainer_SubModulesTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* サンプルセットのテスト */
  {
#define TEST_NUM_SAMPLE 32
#define TEST_DIM_SAMPLE 3
    struct CRFPCTrainer *trainter;
    struct CRFPCTrainerConfig config;
    uint32_t i_sample, i_dim;
    uint8_t *sample_data[TEST_NUM_SAMPLE];
    uint8_t sample_label[TEST_NUM_SAMPLE];

    /* サンプルデータの割当て */
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      sample_data[i_sample] = (uint8_t *)malloc(sizeof(uint8_t) * TEST_DIM_SAMPLE);
      for (i_dim = 0; i_dim < TEST_DIM_SAMPLE; i_dim++) {
        sample_data[i_sample][i_dim] = rand();
      }
      sample_label[i_sample] = rand();
    }

    CRFPCTrainer_SetDefaultConfig(&config);
    trainter = CRFPCTrainer_Create(&config);
    Test_AssertEqual(CRFPCTrainer_SetSample(trainter, (const uint8_t **)sample_data, sample_label, TEST_NUM_SAMPLE, TEST_DIM_SAMPLE), CRFPC_API_RESULT_OK);
    Test_AssertCondition(trainter->sample_data == (const uint8_t **)sample_data);
    Test_AssertCondition(trainter->sample_label == sample_label);

    /* サンプルデータの解放 */
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      free(sample_data[i_sample]);
    }

    CRFPCTrainer_Destroy(trainter);
#undef TEST_NUM_SAMPLE
#undef TEST_DIM_SAMPLE
  }

  /* 対数尤度/対数尤度勾配の計算テスト */
  {
#define TEST_NUM_SAMPLE 32
#define TEST_DIM_SAMPLE 3
    struct CRFPCTrainer *trainter;
    struct CRFPCModel* model;
    struct CRFPCTrainerConfig config;
    int32_t is_ok;
    uint32_t i_sample, i_dim, j_dim;
    float   test_ret;
    uint8_t *sample_data[TEST_NUM_SAMPLE];
    uint8_t sample_label[TEST_NUM_SAMPLE];

    /* サンプルデータの割当て */
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      sample_data[i_sample] = (uint8_t *)malloc(sizeof(uint8_t) * TEST_DIM_SAMPLE);
      for (i_dim = 0; i_dim < TEST_DIM_SAMPLE; i_dim++) {
        sample_data[i_sample][i_dim] = rand();
      }
      sample_label[i_sample] = rand();
    }

    /* モデルの作成 */
    model = CRFPCModel_Create(TEST_DIM_SAMPLE, assoc_func_abs, inter_func_abs);
    /* 連想ポテンシャルのパラメータはすべて同一に */
    for (i_dim = 0; i_dim < TEST_DIM_SAMPLE; i_dim++) {
      model->lambda[i_dim] = 1.0f;
    }

    /* トレーナの作成 */
    CRFPCTrainer_SetDefaultConfig(&config);
    trainter = CRFPCTrainer_Create(&config);

    /* サンプルをセット */
    Test_AssertEqual(CRFPCTrainer_SetSample(trainter, (const uint8_t **)sample_data, sample_label, TEST_NUM_SAMPLE, TEST_DIM_SAMPLE), CRFPC_API_RESULT_OK);

    /* 非数になっていないか？ */
    test_ret = CRFPCTrainer_CalculateLogLikelihood(trainter, model);
    Test_AssertCondition(!isnan(test_ret) && !isinf(test_ret));

    is_ok = 1;
    for (i_dim = 0; i_dim < TEST_DIM_SAMPLE; i_dim++) {
      test_ret = CRFPCTrainer_CalculateLogDiffLambda(trainter, model, i_dim);
      if (isnan(test_ret) || isinf(test_ret)) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    is_ok = 1;
    for (i_dim = 0; i_dim < TEST_DIM_SAMPLE; i_dim++) {
      for (j_dim = i_dim+1; j_dim < TEST_DIM_SAMPLE; j_dim++) {
        test_ret = CRFPCTrainer_CalculateLogDiffMu(trainter, model, i_dim, j_dim);
        if (isnan(test_ret) || isinf(test_ret)) {
          is_ok = 0;
          break;
        }
      }
    }
    Test_AssertEqual(is_ok, 1);

    /* サンプルデータの解放 */
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      free(sample_data[i_sample]);
    }

    CRFPCModel_Destroy(model);
    CRFPCTrainer_Destroy(trainter);
#undef TEST_NUM_SAMPLE
#undef TEST_DIM_SAMPLE
  }
}

/* 学習テスト */
void testCRFPCTrainer_LearningTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  {
#define TEST_NUM_SAMPLE 8
#define TEST_DIM_SAMPLE 3
    struct CRFPCTrainer*      trainter;
    struct CRFPCModel*        model;
    struct CRFPCTrainerConfig config;
    int32_t   is_ok;
    uint32_t i_sample, i_dim;
    uint8_t *sample_data[TEST_NUM_SAMPLE];
    uint8_t sample_label[TEST_NUM_SAMPLE];
    uint8_t test_label;
    float   prev_likelihood, post_likelihood;

    /* サンプルデータの割当て */
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      sample_data[i_sample] = (uint8_t *)malloc(sizeof(uint8_t) * TEST_DIM_SAMPLE);
    }

    /* サンプル作成 */
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      uint8_t val = i_sample * 0xFF / TEST_NUM_SAMPLE;
      for (i_dim = 0; i_dim < TEST_DIM_SAMPLE; i_dim++) {
        sample_data[i_sample][i_dim] = val;
      }
      sample_label[i_sample] = val;
    }

    /* モデルの作成 */
    model = CRFPCModel_Create(TEST_DIM_SAMPLE, assoc_func_abs, inter_func_abs);
    /* 連想ポテンシャルのパラメータ乱数で */
    for (i_dim = 0; i_dim < TEST_DIM_SAMPLE; i_dim++) {
      model->lambda[i_dim] = 2.0f * ((float)rand() / RAND_MAX - 0.5f);
    }
    for (i_dim = 0; i_dim < CONBINATION_OF_2(TEST_DIM_SAMPLE); i_dim++) {
      model->mu[i_dim] = 2.0f * ((float)rand() / RAND_MAX - 0.5f);
    }

    /* トレーナの作成 */
    CRFPCTrainer_SetDefaultConfig(&config);
    trainter = CRFPCTrainer_Create(&config);

    /* サンプルをセット */
    Test_AssertEqual(CRFPCTrainer_SetSample(trainter, (const uint8_t **)sample_data, sample_label, TEST_NUM_SAMPLE, TEST_DIM_SAMPLE), CRFPC_API_RESULT_OK);

    /* 学習してみる */
    prev_likelihood = CRFPCTrainer_CalculateLogLikelihood(trainter, model);
    Test_AssertEqual(CRFPCTrainer_Learning(trainter, model, 10), CRFPC_API_RESULT_OK);
    post_likelihood = CRFPCTrainer_CalculateLogLikelihood(trainter, model);

    /* 対数尤度値はおかしくなっていないか？ */
    Test_AssertCondition(!isnan(prev_likelihood) && !isinf(prev_likelihood));
    Test_AssertCondition(!isnan(post_likelihood) && !isinf(post_likelihood));

    /* 学習によって尤度は上がるか？ */
    Test_AssertCondition(post_likelihood > prev_likelihood);

    /* 学習の確認 */
    is_ok = 1;
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      test_label = CRFPCModel_Predict(model, sample_data[i_sample], TEST_DIM_SAMPLE);
      if (test_label != sample_label[i_sample]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    /* サンプルデータの解放 */
    for (i_sample = 0; i_sample < TEST_NUM_SAMPLE; i_sample++) {
      free(sample_data[i_sample]);
    }

    CRFPCModel_Destroy(model);
    CRFPCTrainer_Destroy(trainter);
#undef TEST_NUM_SAMPLE
#undef TEST_DIM_SAMPLE
  }
}

void testCRFPC_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("CRFPC Test Suite",
        NULL, testCRFPCModel_Initialize, testCRFPCModel_Finalize);

  Test_AddTest(suite, testCRFPCModel_SymmetricAccessTest);
  Test_AddTest(suite, testCRFPCModel_CreateDestroyTest);
  Test_AddTest(suite, testCRFPCModel_CalculateProbabilityTest);
  Test_AddTest(suite, testCRFPCModel_PredictTest);

  Test_AddTest(suite, testCRFPCTrainer_CreateDestroyTest);
  Test_AddTest(suite, testCRFPCTrainer_SubModulesTest);
  Test_AddTest(suite, testCRFPCTrainer_LearningTest);
}

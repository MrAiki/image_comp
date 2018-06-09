#include "../crfpc.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>

#define MAX_DIM_SAMPLE 64

static char read_buffer[256];

static int32_t get_sample_data(
    const char* filename,
    uint8_t** sample_data, uint8_t* sample_label,
    uint32_t num_samples, uint32_t dim_sample)
{
  FILE      *fp;
  uint32_t  i_dim, i_sample;
  char*     token_ptr;
  uint8_t   sample_buffer[MAX_DIM_SAMPLE];

  if ((fp = fopen(filename, "r")) == NULL) {
    fprintf(stderr, "Failed to open file(%s). \n", filename);
    return 1;
  }

  i_sample = 0;
  while (fgets(read_buffer, sizeof(read_buffer), fp) != NULL) {
    if (i_sample >= num_samples) {
      break;
    }

    token_ptr = strtok(read_buffer, ",");
    i_dim = 0;
    while (token_ptr != NULL) {
      sample_buffer[i_dim++] = strtol(token_ptr, NULL, 10);
      token_ptr = strtok(NULL, ",");
    }

    sample_label[i_sample] = sample_buffer[0];
    memcpy(sample_data[i_sample], &sample_buffer[1], sizeof(uint8_t) * dim_sample);
    i_sample++;
  }

  return 0;
}

static uint8_t predict_pc_naive(uint8_t* sample_data, uint32_t dim_sample)
{
  assert(dim_sample == 1);
  return sample_data[0];
}

static uint8_t predict_pc_avg2(uint8_t* sample_data, uint32_t dim_sample)
{
  assert(dim_sample == 2);
  return sample_data[0] / 2 + sample_data[1] / 2;
}

/* plane predictor */
static uint8_t predict_pc_pp(uint8_t* sample_data, uint32_t dim_sample)
{
  assert(dim_sample == 3);
  uint32_t pp = (uint32_t)sample_data[0] - sample_data[1] + sample_data[2];
  return (uint8_t)pp;
}

static uint8_t predict_pc_avgcoef(uint8_t* sample_data, uint32_t dim_sample)
{
  assert(dim_sample == 3);
  return 0.75f * sample_data[0] - 0.5f * sample_data[1] + 0.75f * sample_data[2];
}

static uint8_t predict_pc_adaptive(uint8_t* sample_data, uint32_t dim_sample)
{
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define MIN(x,y) (((x) > (y)) ? (y) : (x))
  assert(dim_sample == 3);
  if (sample_data[1] >= MAX(sample_data[0], sample_data[2])) {
    return MIN(sample_data[0], sample_data[2]);
  } else if (sample_data[1] <= MIN(sample_data[0], sample_data[2])) {
    return MAX(sample_data[0], sample_data[2]);
  } 
  uint32_t pp = (uint32_t)sample_data[0] - sample_data[1] + sample_data[2];
  return (uint8_t)pp;
#undef MAX
#undef MIN
}

static float assoc_func(uint8_t y, uint8_t x)
{
  return fabsf((float)y - (float)x) / 0xFF;
  // float diff = ((float)y - (float)x);
  // return 1.0f / (exp(-diff) + 1);
}

static float inter_func(uint8_t y, uint8_t x_i, uint8_t x_j)
{
  // return fabsf((float)x_i - (float)x_j) / 0xFF;
  return fabsf((float)y - (float)x_i) * fabsf((float)y - (float)x_j) * fabsf((float)x_i - (float)x_j) / 0xFFFFFF;
  // float diff = fabsf((float)y - (float)x_i) * fabsf((float)y - (float)x_j);
  // return 1.0f / (exp(-diff) + 1);
}

int main(int argc, char** argv)
{
  const uint32_t DIM_SAMPLE = 7;
  const uint32_t NUM_SAMPLE = 3000;
  struct CRFPCModel* model;
  struct CRFPCTrainer* trainter;
  struct CRFPCTrainerConfig config;
  uint8_t **sample_data;
  uint8_t *sample_label;
  uint32_t  i_sample, i_dim;
  float  error, error_pc;

  if (argc != 2) {
    fprintf(stderr, "Usage: prog sample.data \n");
    return 1;
  }

  /* モデル/トレーナ作成 */
  model    = CRFPCModel_Create(DIM_SAMPLE, assoc_func, inter_func);
  CRFPCTrainer_SetDefaultConfig(&config);
  config.learning_rate = 0.002f;
  // config.param_variance = 0.05f;
  trainter = CRFPCTrainer_Create(&config);

  /* サンプルの領域割当 */
  sample_data = (uint8_t **)malloc(sizeof(uint8_t *) * NUM_SAMPLE);
  for (i_sample = 0; i_sample < NUM_SAMPLE; i_sample++) {
    sample_data[i_sample] = (uint8_t *)malloc(sizeof(uint8_t) * DIM_SAMPLE);
  }
  sample_label = (uint8_t *)malloc(sizeof(uint8_t) * NUM_SAMPLE);

  get_sample_data(argv[1], sample_data, sample_label, NUM_SAMPLE, DIM_SAMPLE);

  /* サンプルをセット */
  CRFPCTrainer_SetSample(trainter, sample_data, sample_label, NUM_SAMPLE, DIM_SAMPLE);

  /* 学習... */
  CRFPCTrainer_Learning(trainter, model, 50);

  /* テストしてみる */
  error = 0.0f;
  error_pc = 0.0f;
  for (i_sample = 0; i_sample < NUM_SAMPLE; i_sample++) {
    uint8_t y_predict, y_pc_predict;
    y_predict
      = CRFPCModel_Predict(model, sample_data[i_sample], DIM_SAMPLE);
    y_pc_predict
      = predict_pc_adaptive(sample_data[i_sample], 3);
    error 
      += ((float)y_predict - sample_label[i_sample]) * ((float)y_predict - sample_label[i_sample]);
    error_pc
      += ((float)y_pc_predict - sample_label[i_sample]) * ((float)y_pc_predict - sample_label[i_sample]);
#if 0
    printf("Ans:%d Pred:%d ", sample_label[i_sample], y_predict);
    for (i_dim = 0; i_dim < DIM_SAMPLE; i_dim++) {
      printf("%d,", sample_data[i_sample][i_dim]);
    }
    printf("\n");
#endif
  }
  error /= NUM_SAMPLE; error = sqrt(error);
  error_pc /= NUM_SAMPLE; error_pc = sqrt(error_pc);
  printf("Total:%d RMSErrorByModel:%f RMSErrorPC:%f \n", NUM_SAMPLE, error, error_pc); 

  free(sample_label);
  for (i_sample = 0; i_sample < NUM_SAMPLE; i_sample++) {
    free(sample_data[i_sample]);
  }
  free(sample_data);

  CRFPCTrainer_Destroy(trainter);
  CRFPCModel_Destroy(model);

  return 0;
}

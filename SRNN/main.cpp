#include "SRNNPredictor.hpp"
#include "../pnm/pnm.h"
#include <stdint.h>
#include <stdio.h>

#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define MIN(x,y) (((x) > (y)) ? (y) : (x))

float predict_pc_adaptive(float* sample_data, uint32_t dim_sample)
{
  if (sample_data[1] >= MAX(sample_data[0], sample_data[2])) {
    return MIN(sample_data[0], sample_data[2]);
  } else if (sample_data[1] <= MIN(sample_data[0], sample_data[2])) {
    return MAX(sample_data[0], sample_data[2]);
  } 
  return (sample_data[0] - sample_data[1] + sample_data[2]);
}

// SRNNのテストルーチン
int main(int argc, char** argv)
{
  FILE* fp;
  char  str_buf[256];
  char* str_token;
  const int dim_sample = 6;
  const int max_num_sample = 1000;
  int cnt = 0, dim, i;

  float* sample = new float[max_num_sample * dim_sample];
  float* label  = new float[max_num_sample];
  float* sample_maxmin = new float[dim_sample * 2];

  if (argc != 2) {
    printf( "Usage: prog filename.sampledata \n");
    return -1;
  }

  fp = fopen( argv[1], "r" );
  if( fp == NULL ){
    printf( "%sファイルが開けません \n", argv[1] );
    return -1;
  }

  for (dim = 0; dim < dim_sample; dim++) {
    sample_maxmin[2 * dim + 0] = 255;
    sample_maxmin[2 * dim + 1] = 0;
  }


  while ( fgets(str_buf, sizeof(str_buf), fp) != NULL && cnt < max_num_sample) {
    str_token = strtok(str_buf, ",");
    label[cnt] = atoi(str_token);
    dim = 0;
    while ((str_token = strtok(NULL, ",")) != NULL && dim < dim_sample) {
      sample[cnt * dim_sample + dim++] = atoi(str_token);
    }
    /*
    printf("label : %f sample : %f %f %f %f %f \n", 
        label[cnt], 
        MATRIX_AT(sample,3,cnt,0), 
        MATRIX_AT(sample,3,cnt,1), 
        MATRIX_AT(sample,3,cnt,2),
        MATRIX_AT(sample,3,cnt,3),
        MATRIX_AT(sample,3,cnt,4));
    */
    sample[cnt * dim_sample + 3] = MAX(sample[cnt * dim_sample + 0], sample[cnt * dim_sample + 2]);
    sample[cnt * dim_sample + 4] = MIN(sample[cnt * dim_sample + 0], sample[cnt * dim_sample + 2]);
    sample[cnt * dim_sample + 5] = (sample[cnt * dim_sample + 0] - sample[cnt * dim_sample + 1] + sample[cnt * dim_sample + 2]);
    cnt++;
  }

  /* 高橋治久教授のアドバイス:RNNのダイナミクス(中間層のニューロン数)は多いほど良い */
  SRNNPredictor srnn(dim_sample, 1, 300);
  srnn.set_sample(sample, label, sample_maxmin, cnt, dim_sample, 1);
  srnn.learning();

  // 簡単に確認
  float err_srnn = 0, err_pc = 0;
  printf("label SRNN AdaptivePC \n");
  for (i = 0; i < cnt; i++) {
    float predict, predict_pc;
    srnn.predict(&sample[i * dim_sample], &predict);
    predict_pc = predict_pc_adaptive(&sample[i * dim_sample], 3);
    printf("%f %f %f \n", label[i],
        round(predict) - label[i],
        predict_pc - label[i]);
    err_srnn += powf(label[i] - predict, 2);
    err_pc += powf(label[i] - predict_pc, 2);
  }
  err_srnn = sqrt(err_srnn / cnt);
  err_pc = sqrt(err_pc / cnt);
  printf("ErrSRNN:%f ErrPC:%f \n", err_srnn, err_pc);

  fclose( fp );
  return 0;
}


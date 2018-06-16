#include <stdio.h>

#include "SRNNPredictor.hpp"

// SRNNのテストルーチン
int main(void) {
  FILE* fp;
  const char* fname = "./testdata/testdata.csv";
  char s[100];
  int ret, n1, n2;
  float f1, f2, f3;

  float* sample = new float[250 * 3];
  float* sample_maxmin = new float[3 * 2];
  sample_maxmin[0] = 20;
  sample_maxmin[1] = 0;
  sample_maxmin[2] = 1025;
  sample_maxmin[3] = 980;
  sample_maxmin[4] = 20;
  sample_maxmin[5] = 0;

  fp = fopen( fname, "r" );
  if( fp == NULL ){
    printf( "%sファイルが開けません¥n", fname );
    return -1;
  }

  int cnt = 0;
  while( ( ret = fscanf( fp, "%f,%f,%f", &f1, &f2, &f3) ) != EOF ){
    //printf( "%f %f %f \n", f1, f2, f3 );
    sample[cnt * 3] = f1;
    sample[cnt * 3 + 1] = f2;
    sample[cnt * 3 + 2] = f3;
    printf("sample : %f %f %f \n", MATRIX_AT(sample,3,cnt,0), MATRIX_AT(sample,3,cnt,1), MATRIX_AT(sample,3,cnt,2));
    cnt++;
  }

  /* 高橋治久教授のアドバイス:RNNのダイナミクス(中間層のニューロン数)は多いほど良い */
  SRNNPredictor srnn(3, 3, 100, 240, sample, sample_maxmin);
  srnn.learning();

  fclose( fp );
  return 0;
}

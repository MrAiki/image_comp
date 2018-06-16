#include "lpc.h"
#include "../../pnm/pnm.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_NUM_DIFF 512

/* 横方向に走査 */
void walk_vertical(const struct PNMPicture* pict, float* data);
/* ジグザグスキャン */
void walk_zigzag(const struct PNMPicture* pict, float* data);

int main(int argc, char** argv)
{
  struct PNMPicture* pic; 
  float* data;
  float* auto_corr;
  float* lpc_coef;
  uint32_t AUTOCORR_ORDER;
  uint32_t num_data, i_coef, i_data;
  uint32_t diff[MAX_NUM_DIFF];
  float err, entropy;

  if (argc != 3) {
    fprintf(stderr, "Usage: prog input.pgm corr_order \n");
    return 1;
  }

  pic = PNM_CreatePictureFromFile(argv[1]);
  AUTOCORR_ORDER = atoi(argv[2]);
  num_data = pic->header.width * pic->header.height;
  data = malloc(sizeof(float) * num_data);
  auto_corr = malloc(sizeof(float) * AUTOCORR_ORDER+1);
  lpc_coef = malloc(sizeof(float) * AUTOCORR_ORDER+1);

  walk_vertical(pic, data);
  // walk_zigzag(pic, data);
  LPC_CalculateAutoCorrelation(auto_corr, data, num_data, AUTOCORR_ORDER+1);
  LPC_LevinsonDurbinRecursion(lpc_coef, auto_corr, AUTOCORR_ORDER);

  for (i_coef = 0; i_coef < MAX_NUM_DIFF; i_coef++) {
    diff[i_coef] = 0;
  }

  err = 0.0f;
  for (i_data = AUTOCORR_ORDER; i_data < num_data; i_data++) {
    float predict = 0.0f;
    for (i_coef = 0; i_coef < AUTOCORR_ORDER; i_coef++) {
      predict -= (lpc_coef[i_coef+1] * data[i_data - i_coef - 1]);
    }
    diff[(uint32_t)(round(predict) - data[i_data]) + MAX_NUM_DIFF/2]++;
    err += powf(round(predict) - data[i_data], 2.0f);
    // printf("%d, %f, %f, \n", i_data, predict, data[i_data]);
  }

  entropy = 0.0f;
  for (i_coef = 0; i_coef < MAX_NUM_DIFF; i_coef++) {
    float prob = (float)diff[i_coef] / (num_data-AUTOCORR_ORDER);
    if (diff[i_coef] > 0) {
      entropy -= prob * log2(prob);
    }
  }

  printf("%d, %f \n", AUTOCORR_ORDER, entropy);

  PNM_DestroyPicture(pic);
  free(data);
  free(auto_corr);
  free(lpc_coef);

  return 0;
}

/* 横方向に走査 */
void walk_vertical(const struct PNMPicture* pict, float* data)
{
  int32_t x, y, i_data;
  int32_t delta_x;

  delta_x = 1;
  x = 0;
  i_data = 0;
  for (y = 0; y < (int32_t)pict->header.height; y++) {
    while (x < (int32_t)pict->header.width && x >= 0) {
      // printf("%d \n", PNMPict_GRAY(pict, x, y));
      data[i_data++] = PNMPict_GRAY(pict, x, y);
      x += delta_x;
    }
    delta_x *= -1;
    x += delta_x;
  }
}

static void walk_zigzag_to_up_right(const struct PNMPicture* pict, int32_t *x, int32_t *y, float* data, int32_t* i_data)
{
  while (*x < (int32_t)pict->header.width-1 && *y > 0) {
    // printf("%d \n", PNMPict_GRAY(pict, *x, *y));
    data[(*i_data)++] = PNMPict_GRAY(pict, *x, *y);
    (*x)++; (*y)--;
  }

  if (*x < (int32_t)pict->header.width && *y < (int32_t)pict->header.height) {
    // printf("%d \n", PNMPict_GRAY(pict, *x, *y));
    data[(*i_data)++] = PNMPict_GRAY(pict, *x, *y);
  }

  if (*x < (int32_t)pict->header.width-1) {
    (*x)++; 
  } else {
    (*y)++; 
  }
}

static void walk_zigzag_to_down_left(const struct PNMPicture* pict, int32_t *x, int32_t *y, float* data, int32_t* i_data)
{
  while (*x > 0 && *y < (int32_t)pict->header.height-1) {
    // printf("%d \n", PNMPict_GRAY(pict, *x, *y));
    data[(*i_data)++] = PNMPict_GRAY(pict, *x, *y);
    (*x)--; (*y)++;
  }

  if (*x < (int32_t)pict->header.width 
      && *y < (int32_t)pict->header.height) {
    // printf("%d \n", PNMPict_GRAY(pict, *x, *y));
    data[(*i_data)++] = PNMPict_GRAY(pict, *x, *y);
  }

  if (*y < (int32_t)pict->header.height-1) {
    (*y)++; 
  } else {
    (*x)++; 
  }
}

/* ジグザグスキャン */
void walk_zigzag(const struct PNMPicture* pict, float* data)
{
  int32_t x, y, i_data;

  x = 0; y = 0; i_data = 0;
  while (x < (int32_t)pict->header.width
      && y < (int32_t)pict->header.height) {
    walk_zigzag_to_up_right(pict, &x, &y, data, &i_data);
    walk_zigzag_to_down_left(pict, &x, &y, data, &i_data);
  }
}

#if 0
static void walk_case3_go_horizontal(const struct PNMPicture* pict,
    int32_t *x, int32_t *y, int32_t *x_min, int32_t *x_max, int32_t delta)
{
  while (*x < *x_max-1 && *x >= *x_min) {
    printf("[%d,%d] %d \n", *x, *y, PNMPict_GRAY(pict, *x, *y));
    (*x) += delta;
  }

  printf("[%d,%d,%d,%d] \n", *x, *y, *x_min, *x_max);

  if (delta > 0) {
    (*x_max)--;
  } else {
    (*x_min)++;
  }
}

static void walk_case3_go_vertical(const struct PNMPicture* pict,
    int32_t *x, int32_t *y, int32_t *y_min, int32_t *y_max, int32_t delta)
{
  while (*y < *y_max-1 && *y >= *y_min) {
    printf("[%d,%d] %d \n", *x, *y, PNMPict_GRAY(pict, *x, *y));
    (*y) += delta;
  }

  printf("[%d,%d,%d,%d] \n", *x, *y, *y_min, *y_max);
  
  if (delta > 0) {
    (*y_max)--;
  } else {
    (*y_min)++;
  }
}

/* まきぐそスキャン */
void walk_case3(const struct PNMPicture* pict)
{
  int32_t x, y;
  int32_t x_max, x_min;
  int32_t y_max, y_min;

  x = 0; y = 0;
  x_min = 0; x_max = pict->header.width;
  y_min = 0; y_max = pict->header.height;
  //while (1) {
    walk_case3_go_horizontal(pict, &x, &y, &x_min, &x_max, 1);
    walk_case3_go_vertical(pict, &x, &y, &y_min, &y_max, 1);
    walk_case3_go_horizontal(pict, &x, &y, &x_min, &x_max, -1);
    walk_case3_go_vertical(pict, &x, &y, &y_min, &y_max, -1);
    walk_case3_go_horizontal(pict, &x, &y, &x_min, &x_max, 1);
    walk_case3_go_vertical(pict, &x, &y, &y_min, &y_max, 1);
    walk_case3_go_horizontal(pict, &x, &y, &x_min, &x_max, -1);
    walk_case3_go_vertical(pict, &x, &y, &y_min, &y_max, -1);
  //}
}
#endif

#include "pcac.h"
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>
#include <string.h>

/* for logoutput */
#include <stdio.h>

/* Jacobi法の最大繰り返し回数 */
#define MAX_NUM_JACOBI_ITERATION 100
/* Jacobi法の収束判定値 */
#define CONVERGENCE_MARGIN       (1e-10)

/* 縦横block_sizeのサイズでブロック分割した画像内の座標にアクセスする */
static uint32_t PCAC_PictureBlockIndexAt(const struct PNMPicture* picture, uint32_t block_size, uint32_t block_x, uint32_t block_y, uint32_t x, uint32_t y);

/* ブロック分割した画像内の特定座標の要素の平均を計算する */
static float PCAC_CalculateAverageBlockIndexAt(const struct PNMPicture* picture, uint32_t block_size, uint32_t x, uint32_t y);

/* ブロック分割した画像内のi番目の座標とj番目の座標の積の平均を計算する */
static float PCAC_CalculateMulAverageBlockIndexAt(const struct PNMPicture* picture,
    uint32_t block_size, uint32_t x_i, uint32_t y_i, uint32_t x_j, uint32_t y_j);

/* 固有ベクトルの正規化 */
static void PCAC_NormalizeEigenVectors(float* eigenvectors, uint32_t dim);

/* 対角要素に固有値が並んだ対角行列をもとに、固有値を降順で並び替える
 * 対応する固有ベクトル（列ベクトル）も並べ替える */
static void PCAC_SortEigenVectors(float* eigen_diag_matrix, float* eigenvectors, int32_t dim);

/* 縦横block_sizeのサイズでブロック分割した画像内のelement_indexにアクセスする */
static uint32_t PCAC_PictureBlockIndexAt(const struct PNMPicture* picture, uint32_t block_size, uint32_t block_x, uint32_t block_y, uint32_t x, uint32_t y)
{
  /* 画像全体における座標 */
  uint32_t pict_x, pict_y;

  assert(picture != NULL);
  assert(picture->header.format == PNM_FORMAT_P2
      || picture->header.format == PNM_FORMAT_P5);
  assert(block_size > 0);

  /* アクセスするインデックスの確定 */
  pict_x = block_size * block_x + x;
  pict_y = block_size * block_y + y;

  /* 範囲外アクセス処理 */
  if (pict_x >= picture->header.width) {
    pict_x = picture->header.width - 1;
  } 
  if (pict_y >= picture->header.height) {
    pict_y = picture->header.height - 1;
  }

  return PNMPict_GRAY(picture, pict_x, pict_y);
}

/* ブロック分割した画像内の特定座標の要素の平均を計算する */
static float PCAC_CalculateAverageBlockIndexAt(const struct PNMPicture* picture, uint32_t block_size, uint32_t x, uint32_t y)
{
  uint32_t block_x, block_y;
  float sum;

  assert(picture != NULL);

  sum = 0.0f;
  for (block_y = 0; block_y < PCAC_NUM_BLOCKS_IN_HEIGHT(picture, block_size); block_y++) {
    for (block_x = 0; block_x < PCAC_NUM_BLOCKS_IN_WIDTH(picture, block_size); block_x++) {
      sum += PCAC_PictureBlockIndexAt(picture, block_size, block_x, block_y, x, y);
    }
  }

  return sum / PCAC_NUM_BLOCKS_IN_PICTURE(picture, block_size);
}

/* ブロック分割した画像内のi番目の座標とj番目の座標の積の平均を計算する */
static float PCAC_CalculateMulAverageBlockIndexAt(const struct PNMPicture* picture,
    uint32_t block_size, uint32_t x_i, uint32_t y_i, uint32_t x_j, uint32_t y_j)
{
  uint32_t block_x, block_y;
  float sum;

  assert(picture != NULL);

  sum = 0.0f;
  for (block_y = 0; block_y < PCAC_NUM_BLOCKS_IN_HEIGHT(picture, block_size); block_y++) {
    for (block_x = 0; block_x < PCAC_NUM_BLOCKS_IN_WIDTH(picture, block_size); block_x++) {
      sum += PCAC_PictureBlockIndexAt(picture, block_size, block_x, block_y, x_i, y_i) * PCAC_PictureBlockIndexAt(picture, block_size, block_x, block_y, x_j, y_j);
    }
  }

  return sum / PCAC_NUM_BLOCKS_IN_PICTURE(picture, block_size);
}

/* 分散共分散行列の計算 */
PCACApiResult PCAC_CalculateCovarianceMatrix(const struct PNMPicture* picture, float* varmatrix, uint32_t block_size)
{
  uint32_t x, y;
  uint32_t varmat_dim = block_size * block_size;
  uint32_t x_i, y_i, x_j, y_j;
  float*   average = malloc(sizeof(float) * block_size * block_size);

  if (picture == NULL || varmatrix == NULL) {
    return PCAC_API_RESULT_NG;
  }

  /* 各要素の平均を計算 */
  for (y = 0; y < block_size; y++) {
    for (x = 0; x < block_size; x++) {
      PCAC_MATRIX_AT(average, block_size, x, y) 
        = PCAC_CalculateAverageBlockIndexAt(picture, block_size, x, y);
    }
  }

  /* 分散共分散行列を計算 */
  /* 補足）対称行列なので、半分はコピーでOK */
  for (x = 0; x < varmat_dim; x++) {
    for (y = x; y < varmat_dim; y++) {
      x_i = PCAC_INDEX_TO_BLOCK_X(block_size, x);
      y_i = PCAC_INDEX_TO_BLOCK_Y(block_size, x);
      x_j = PCAC_INDEX_TO_BLOCK_X(block_size, y);
      y_j = PCAC_INDEX_TO_BLOCK_Y(block_size, y);

      PCAC_MATRIX_AT(varmatrix, varmat_dim, x, y)
        = PCAC_MATRIX_AT(varmatrix, varmat_dim, y, x)
        = PCAC_CalculateMulAverageBlockIndexAt(picture, block_size, x_i, y_i, x_j, y_j)
          - PCAC_MATRIX_AT(average, block_size, x_i, y_i) * PCAC_MATRIX_AT(average, block_size, x_j, y_j);
    }
  }

  free(average);

  return PCAC_API_RESULT_OK;
}

/* 固有ベクトルと固有値の計算(Jacobi法) */
/* matrixの対角要素に固有値が降順でセットされ、eigenvectorsに対応する列ベクトルがセットされる */
PCACApiResult PCAC_CalculateEigenVector(float* matrix, float* eigenvectors, uint32_t dim)
{
  uint32_t i, j, k, m;
  uint32_t count;
  double m_ii, m_ij, m_jj, m_ki, m_kj;
  double theta, co, si, co2, si2, cosi;
  double amax, amax0; 

  /* 結果の行列を単位行列で初期化 */
  for(i = 0; i < dim; i++){
    for(j = 0; j < dim; j++){
      if(i == j) {
        PCAC_MATRIX_AT(eigenvectors, dim, i, j) = 1.0f;
      } else { 
        PCAC_MATRIX_AT(eigenvectors, dim, i, j) = 0.0f;
      }
    }
  }

  /* 反復計算 */
  count = 0;
  while (count <= MAX_NUM_JACOBI_ITERATION) {
    /* 非対角要素の最大値を探索 */
    amax = 0.0;
    for (k = 0; k < dim - 1; k++){
      for (m = k + 1; m < dim; m++){
        amax0 = fabs(PCAC_MATRIX_AT(matrix, dim, k, m));
        if (amax0 > amax) {
          i = k;
          j = m;
          amax = amax0;
        }
      }
    }

    /* 収束判定 */
    if (amax <= CONVERGENCE_MARGIN) {
      break; 
    } 

    /* 計算用一時変数を保存 */
    m_ii = PCAC_MATRIX_AT(matrix, dim, i, i);
    m_ij = PCAC_MATRIX_AT(matrix, dim, i, j);
    m_jj = PCAC_MATRIX_AT(matrix, dim, j, j);

    /* 回転角度計算 */
    if (fabs(m_ii - m_jj) < CONVERGENCE_MARGIN){
      theta = 0.25f * M_PI * m_ij / fabs(m_ij);
    } else {
      theta = 0.5f * atan(2.0f * m_ij / (m_ii - m_jj));
    }
    co = cos(theta); si = sin(theta); co2 = co*co; si2 = si*si; cosi=co*si;

    /* 相似変換行列 */
    PCAC_MATRIX_AT(matrix, dim, i, i)
      = co2 * m_ii + 2.0f * cosi * m_ij + si2 * m_jj;
    PCAC_MATRIX_AT(matrix, dim, j, j)
      = si2 * m_ii - 2.0f * cosi * m_ij + co2 * m_jj;
    PCAC_MATRIX_AT(matrix, dim, i, j)
      = PCAC_MATRIX_AT(matrix, dim, j, i)
      = 0.0f;

    for (k = 0; k < dim; k++) {
      if (k != i && k != j){
        m_ki = PCAC_MATRIX_AT(matrix, dim, k, i);
        m_kj = PCAC_MATRIX_AT(matrix, dim, k, j);
        PCAC_MATRIX_AT(matrix, dim, i, k)
          = PCAC_MATRIX_AT(matrix, dim, k, i)
          = co * m_ki + si * m_kj;
        PCAC_MATRIX_AT(matrix, dim, j, k)
          = PCAC_MATRIX_AT(matrix, dim, k, j)
          = -si * m_ki + co * m_kj;
      }
    }

    /* 固有ベクトル更新 */
    for (k = 0; k < dim; k++) {
      m_ki = PCAC_MATRIX_AT(eigenvectors, dim, k, i);
      m_kj = PCAC_MATRIX_AT(eigenvectors, dim, k, j);
      PCAC_MATRIX_AT(eigenvectors, dim, k, i)
        =  co * m_ki + si * m_kj;
      PCAC_MATRIX_AT(eigenvectors, dim, k, j)
        = -si * m_ki + co * m_kj;
    }

    count++;

    /* 結果表示
    printf(" Jacobi> iter=%d", count);
    for (i = 0; i < dim; i++) {
      printf(" %10.6f,", PCAC_MATRIX_AT(matrix, dim, i, i));
    }
    printf("\n");
    */
  }

  /* 固有ベクトルのノルムを1に正規化 */
  PCAC_NormalizeEigenVectors(eigenvectors, dim);

  /* 固有値降順で並び替え */
  PCAC_SortEigenVectors(matrix, eigenvectors, dim);

  return PCAC_API_RESULT_OK;
}

/* 固有ベクトルの正規化 */
static void PCAC_NormalizeEigenVectors(float* eigenvectors, uint32_t dim)
{
  uint32_t row, col;
  float sum;

  for (col = 0; col < dim; col++) {
    /* 2乗ノルムを求める */
    sum = 0.0f;
    for (row = 0; row < dim; row++) {
      sum += PCAC_MATRIX_AT(eigenvectors, dim, row, col) * PCAC_MATRIX_AT(eigenvectors, dim, row, col);
    }
    sum = sqrt(sum);
    /* 2乗ノルムで割る */
    for (row = 0; row < dim; row++) {
      PCAC_MATRIX_AT(eigenvectors, dim, row, col) /= sum;
    }
  }
}

/* 対角要素に固有値が並んだ対角行列をもとに、固有値を降順で並び替える
 * 対応する固有ベクトル（列ベクトル）も並べ替える */
static void PCAC_SortEigenVectors(float* eigen_diag_matrix, float* eigenvectors, int32_t dim)
{
  int32_t i, j, row;
  float   tmp_val;

  assert(eigen_diag_matrix != NULL && eigenvectors != NULL);

  /* バブルソートでいいんだ輪廻上等だろ */
  for (i = 0; i < dim; i++) {
    for (j = dim - 1; j > i; j--) {
      if (PCAC_MATRIX_AT(eigen_diag_matrix, dim, j-1, j-1) 
          < PCAC_MATRIX_AT(eigen_diag_matrix, dim, j, j)) {
        /* 前の固有値の方が小さければスワップ */
        tmp_val = PCAC_MATRIX_AT(eigen_diag_matrix, dim, j, j);
        PCAC_MATRIX_AT(eigen_diag_matrix, dim, j, j)
          = PCAC_MATRIX_AT(eigen_diag_matrix, dim, j-1, j-1);
        PCAC_MATRIX_AT(eigen_diag_matrix, dim, j-1, j-1) = tmp_val;
        /* 列ベクトルも入れ替え */
        for (row = 0; row < dim; row++) {
          tmp_val = PCAC_MATRIX_AT(eigenvectors, dim, row, j);
          PCAC_MATRIX_AT(eigenvectors, dim, row, j)
            = PCAC_MATRIX_AT(eigenvectors, dim, row, j-1);
          PCAC_MATRIX_AT(eigenvectors, dim, row, j-1) = tmp_val;
        }
      }
    }
  }

}

PCACApiResult PCAC_CalculateTransformMatrix(const struct PNMPicture* picture, float* matrix, uint32_t block_size)
{
  PCACApiResult ret;
  uint32_t  varmat_dim    = block_size * block_size;
  float*    eigenvectors  = malloc(sizeof(float) * varmat_dim * varmat_dim);

  /* 画像をブロック分割したときの共分散行列を計算 */
  ret = PCAC_CalculateCovarianceMatrix(picture, matrix, block_size);
  if (ret != PCAC_API_RESULT_OK) {
    return PCAC_API_RESULT_NG;
  }

  /* 共分散行列の固有値と固有ベクトルを計算 */
  ret = PCAC_CalculateEigenVector(matrix, eigenvectors, varmat_dim);
  if (ret != PCAC_API_RESULT_OK) {
    return PCAC_API_RESULT_NG;
  }

#if 0
  {
    uint32_t i_eig;
    printf("EigenVec,");
    for (i_eig = 0; i_eig < varmat_dim; i_eig++) {
      printf(" %f,", PCAC_MATRIX_AT(matrix, varmat_dim, i_eig, i_eig));
    }
    printf("\n");
  }
#endif

  /* 結果をコピー */
  memcpy(matrix, eigenvectors, sizeof(float) * varmat_dim * varmat_dim);

  free(eigenvectors);

  return PCAC_API_RESULT_OK;
}

#ifndef PCAC_H_INCLUDED
#define PCAC_H_INCLUDED

/* Principal Component Analysis (Karhunen-Loeve Transform) for Coding */

#include <stdint.h>
#include "../pnm/pnm.h"

/* 変換行列の次数と要素数をブロックサイズから決める */
#define PCAC_CALC_TRANS_MAT_DIM(block_size) ((block_size) * (block_size))
#define PCAC_CALC_TRANS_MAT_SIZE(block_size) (PCAC_CALC_TRANS_MAT_DIM(block_size) * PCAC_CALC_TRANS_MAT_DIM(block_size))
/* 幅（水平方向）に存在するブロック数を計算 */
#define PCAC_NUM_BLOCKS_IN_WIDTH(pnm, block_size) \
  (((pnm->header.width) + (block_size) - 1) / (block_size))
/* 高さ（垂直方向）に存在するブロック数を計算 */
#define PCAC_NUM_BLOCKS_IN_HEIGHT(pnm, block_size) \
  (((pnm->header.height) + (block_size) - 1) / (block_size))
/* 画像内のブロック数を計算 */
#define PCAC_NUM_BLOCKS_IN_PICTURE(pnm, block_size) \
  (PCAC_NUM_BLOCKS_IN_WIDTH(pnm, block_size) * PCAC_NUM_BLOCKS_IN_HEIGHT(pnm, block_size))

/* 行列アクセサ */
#define PCAC_MATRIX_AT(matrix, width, i, j) (matrix)[(i) * (width) + (j)]

/* ブロック内座標アクセサ */
#define PCAC_INDEX_TO_BLOCK_X(block_size, index) ((index) % (block_size))
#define PCAC_INDEX_TO_BLOCK_Y(block_size, index) ((index) / (block_size))

/* APIの結果 */
typedef enum PCACApiResultTag {
  PCAC_API_RESULT_OK = 0,
  PCAC_API_RESULT_NG,
} PCACApiResult;

/* 変換行列の計算 */
PCACApiResult PCAC_CalculateTransformMatrix(const struct PNMPicture* picture, float* matrix, uint32_t block_size);

/* 分散共分散行列の計算 */
/* PCAC_MATRIX_AT(varmatrix, block_size, i, j) にPCAC_INDEX_TO_BLOCK_X(block_size, i) と PCAC_INDEX_TO_BLOCK_Y(block_size, j) の共分散が得られる  */
PCACApiResult PCAC_CalculateCovarianceMatrix(const struct PNMPicture* picture, float* varmatrix, uint32_t block_size);

/* 固有ベクトルと固有値の計算(Jacobi法) */
/* matrixの対角要素に固有値が降順でセットされ、eigenvectorsに対応する列ベクトルがセットされる */
PCACApiResult PCAC_CalculateEigenVector(float* matrix, float* eigenvectors, uint32_t dim);

#endif /* PCAC_H_INCLUDED */

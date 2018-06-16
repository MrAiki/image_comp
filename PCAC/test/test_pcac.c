#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../pcac.c"

int testPCAC_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testPCAC_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ブロック内座標のアクセステスト */
void testPCAC_PictureBlockIndexAtTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    uint32_t x, y, block_x, block_y;
    const uint32_t TEST_HEIGHT = 10;
    const uint32_t TEST_WIDTH = 10;
    const uint32_t TEST_BLOCK_SIZE = 3;
    int32_t is_ok;
    struct PNMPicture* pict = PNM_CreatePicture(TEST_WIDTH, TEST_HEIGHT);
    pict->header.format     = PNM_FORMAT_P2;

    /* 各画素にインデックス番号をセット */
    for (x = 0; x < TEST_WIDTH; x++) {
      for (y = 0; y < TEST_HEIGHT; y++) {
        PNMPict_GRAY(pict, x, y) = x + y * TEST_HEIGHT;
      }
    }

    is_ok = 1;
    for (block_x = 0;
         block_x < PCAC_NUM_BLOCKS_IN_WIDTH(pict, TEST_BLOCK_SIZE);
         block_x++) {
      for (block_y = 0;
           block_y < PCAC_NUM_BLOCKS_IN_HEIGHT(pict, TEST_BLOCK_SIZE);
           block_y++) {
        for (x = 0; x < TEST_BLOCK_SIZE; x++) {
          for (y = 0; y < TEST_BLOCK_SIZE; y++) {
            /* この場で計算したインデックスと一致するか？ */
            uint32_t realx = block_x * TEST_BLOCK_SIZE + x;
            uint32_t realy = block_y * TEST_BLOCK_SIZE + y;
            if (realx >= TEST_WIDTH) {
              realx = TEST_WIDTH-1;
            }
            if (realy >= TEST_HEIGHT) {
              realy = TEST_HEIGHT-1;
            }
            if (PCAC_PictureBlockIndexAt(pict, TEST_BLOCK_SIZE, block_x, block_y, x, y) != PNMPict_GRAY(pict, realx, realy)) {
              is_ok = 0;
              goto TEST_EXIT;
            }
          }
        }
      }
    }

TEST_EXIT:
    Test_AssertEqual(is_ok, 1);
    PNM_DestroyPicture(pict);
  }
}

void testPCAC_CalculateAverageBlockIndexAtTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    uint32_t x, y;
    const uint32_t TEST_HEIGHT      = 10;
    const uint32_t TEST_WIDTH       = 20;
    const uint32_t TEST_BLOCK_SIZE  = 3;
    int32_t is_ok;
    struct PNMPicture* pict = PNM_CreatePicture(TEST_WIDTH, TEST_HEIGHT);
    pict->header.format     = PNM_FORMAT_P2;

    /* 全て1の画像 */
    for (x = 0; x < TEST_WIDTH; x++) {
      for (y = 0; y < TEST_HEIGHT; y++) {
        PNMPict_GRAY(pict, x, y) = 1;
      }
    }

    /* ブロック内の全ての平均が1で有ることを期待 */
    is_ok = 1;
    for (x = 0; x < TEST_BLOCK_SIZE; x++) {
      for (y = 0; y < TEST_BLOCK_SIZE; y++) {
        if (fabs(PCAC_CalculateAverageBlockIndexAt(pict, TEST_BLOCK_SIZE, x, y) - 1.0f) > 0.00001f) {
          is_ok = 0;
          goto TEST_EXIT;
        }
      }
    }

TEST_EXIT:
    Test_AssertEqual(is_ok, 1);
    PNM_DestroyPicture(pict);
  }
}

void testPCAC_CalculateMulAverageBlockIndexAtTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    uint32_t x, y;
    uint32_t x_i, y_i, x_j, y_j;
    const uint32_t TEST_HEIGHT      = 10;
    const uint32_t TEST_WIDTH       = 20;
    const uint32_t TEST_BLOCK_SIZE  = 3;
    int32_t is_ok;
    struct PNMPicture* pict = PNM_CreatePicture(TEST_WIDTH, TEST_HEIGHT);
    pict->header.format     = PNM_FORMAT_P2;

    /* 全て2の画像 */
    for (x = 0; x < TEST_WIDTH; x++) {
      for (y = 0; y < TEST_HEIGHT; y++) {
        PNMPict_GRAY(pict, x, y) = 2;
      }
    }

    /* ブロック内の全ての積の平均が4で有ることを期待 */
    is_ok = 1;
    for (x_i = 0; x_i < TEST_BLOCK_SIZE; x_i++) {
      for (y_i = 0; y_i < TEST_BLOCK_SIZE; y_i++) {
        for (x_j = 0; x_j < TEST_BLOCK_SIZE; x_j++) {
          for (y_j = 0; y_j < TEST_BLOCK_SIZE; y_j++) {
            if (fabs(PCAC_CalculateMulAverageBlockIndexAt(pict, TEST_BLOCK_SIZE, x_i, y_i, x_j, y_j) - 4.0f) > 0.00001f) {
              is_ok = 0;
              goto TEST_EXIT;
            }
          }
        }
      }
    }

TEST_EXIT:
    Test_AssertEqual(is_ok, 1);
    PNM_DestroyPicture(pict);
  }
}

void testPCAC_CalculateCovarianceMatrixTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    uint32_t x, y;
    const uint32_t TEST_HEIGHT      = 10;
    const uint32_t TEST_WIDTH       = 20;
    const uint32_t TEST_BLOCK_SIZE  = 3;
    const uint32_t TEST_VARMAT_DIM  = TEST_BLOCK_SIZE * TEST_BLOCK_SIZE;
    int32_t is_ok;
    float *varmatrix;
    struct PNMPicture* pict = PNM_CreatePicture(TEST_WIDTH, TEST_HEIGHT);
    pict->header.format     = PNM_FORMAT_P2;
    varmatrix = malloc(sizeof(float) * TEST_VARMAT_DIM * TEST_VARMAT_DIM);

    /* 全て2の画像 */
    for (x = 0; x < TEST_WIDTH; x++) {
      for (y = 0; y < TEST_HEIGHT; y++) {
        PNMPict_GRAY(pict, x, y) = 2;
      }
    }

    /* ブロック内の全ての分散と共分散が0で有ることを期待 */
    is_ok = 1;
    Test_AssertEqual(PCAC_CalculateCovarianceMatrix(pict, varmatrix, TEST_BLOCK_SIZE), PCAC_API_RESULT_OK);
    for (x = 0; x < TEST_VARMAT_DIM; x++) {
      for (y = 0; y < TEST_VARMAT_DIM; y++) {
        if (fabs(PCAC_MATRIX_AT(varmatrix, TEST_VARMAT_DIM, x, y)) > 0.00001f) {
          is_ok = 0;
          goto TEST_EXIT;
        }
      }
    }

TEST_EXIT:
    Test_AssertEqual(is_ok, 1);
    PNM_DestroyPicture(pict);
    free(varmatrix);
  }

  {
    uint32_t x, y;
    const uint32_t TEST_HEIGHT      = 50;
    const uint32_t TEST_WIDTH       = 100;
    const uint32_t TEST_BLOCK_SIZE  = 7;
    const uint32_t TEST_VARMAT_DIM  = TEST_BLOCK_SIZE * TEST_BLOCK_SIZE;
    int32_t is_ok;
    float *varmatrix; 
    struct PNMPicture* pict = PNM_CreatePicture(TEST_WIDTH, TEST_HEIGHT);
    pict->header.format     = PNM_FORMAT_P2;
    varmatrix = malloc(sizeof(float) * TEST_VARMAT_DIM * TEST_VARMAT_DIM);

    /* 0か1かの乱数画像 */
    for (x = 0; x < TEST_WIDTH; x++) {
      for (y = 0; y < TEST_HEIGHT; y++) {
        PNMPict_GRAY(pict, x, y) = rand() % 2;
      }
    }

    /* 分散の絶対値が0.25未満であることを期待 */
    /* 平均0.5で偏差が0.5 => 分散は0.25であってほしい */
    is_ok = 1;
    Test_AssertEqual(PCAC_CalculateCovarianceMatrix(pict, varmatrix, TEST_BLOCK_SIZE), PCAC_API_RESULT_OK);
    for (x = 0; x < TEST_VARMAT_DIM; x++) {
      for (y = 0; y < TEST_VARMAT_DIM; y++) {
        if (fabs(PCAC_MATRIX_AT(varmatrix, TEST_VARMAT_DIM, x, y)) > 0.25f) {
          is_ok = 0;
          goto RAND_TEST_EXIT;
        }
      }
    }

RAND_TEST_EXIT:
    Test_AssertEqual(is_ok, 1);
    PNM_DestroyPicture(pict);
    free(varmatrix);
  }
}

void testPCAC_CalculateEigenVectorTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  Test_SetFloat32Epsilon(0.00001f);

  /* 単位行列の固有ベクトルと固有値を求めてみる */
  {
    int32_t is_ok;
    uint32_t i, j;
    const uint32_t TEST_MAT_DIM = 20;
    float* mat = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);
    float* eig_vec = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);

    /* 単位行列をセット */
    for (i = 0; i < TEST_MAT_DIM; i++) {
      for (j = 0; j < TEST_MAT_DIM; j++) {
        if (i == j) {
          PCAC_MATRIX_AT(mat, TEST_MAT_DIM, i, j) = 1.0f;
        } else {
          PCAC_MATRIX_AT(mat, TEST_MAT_DIM, i, j) = 0.0f;
        }
      }
    }

    Test_AssertEqual(PCAC_CalculateEigenVector(mat, eig_vec, TEST_MAT_DIM), PCAC_API_RESULT_OK);

    /* 単位行列の固有値は全て1.0f,
     * 固有ベクトルを並べた行列は単位行列で有ることを期待 */
    is_ok = 1;
    for (i = 0; i < TEST_MAT_DIM; i++) {
      for (j = 0; j < TEST_MAT_DIM; j++) {
        if (i == j) {
          if (fabs(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, i, j) - 1.0f) > 0.0001f
              || fabs(PCAC_MATRIX_AT(eig_vec, TEST_MAT_DIM, i, j) - 1.0f) > 0.0001f) {
            is_ok = 0;
            goto UNIT_MAT_EIGEN_TEST_EXIT;
          }
        } else {
          if (fabs(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, i, j)) > 0.0001f
              || fabs(PCAC_MATRIX_AT(eig_vec, TEST_MAT_DIM, i, j)) > 0.0001f) {
            is_ok = 0;
            goto UNIT_MAT_EIGEN_TEST_EXIT;
          }
        }
      }
    }

UNIT_MAT_EIGEN_TEST_EXIT:
    Test_AssertEqual(is_ok, 1);
    free(mat);
    free(eig_vec);
  }

  /* [3,4;4,-3] の行列 => 固有値-5,5 */
  {
    const uint32_t TEST_MAT_DIM = 2;
    float* mat = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);
    float* eig_vec = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);

    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 0) =  3.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 1) =  4.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 0) =  4.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 1) = -3.0f;

    Test_AssertEqual(PCAC_CalculateEigenVector(mat, eig_vec, TEST_MAT_DIM), PCAC_API_RESULT_OK);

    /* 固有値チェック */
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 0), 5.0f);
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 1), -5.0f);

    free(mat);
    free(eig_vec);
  }

  /* [2,1,0;1,1,1;0,1,2] の行列 => 固有値0,2,3 */
  {
    const uint32_t TEST_MAT_DIM = 3;
    float* mat = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);
    float* eig_vec = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);

    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 0) = 2.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 1) = 1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 2) = 0.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 0) = 1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 1) = 1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 2) = 1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 2, 0) = 0.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 2, 1) = 1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 2, 2) = 2.0f;

    Test_AssertEqual(PCAC_CalculateEigenVector(mat, eig_vec, TEST_MAT_DIM), PCAC_API_RESULT_OK);

    /* 固有値チェック */
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 0), 3.0f);
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 1), 2.0f);
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 2, 2), 0.0f);

    free(mat);
    free(eig_vec);
  }

  /* [-1,-1,-1;-1,-1,1;-1,1,-1] の行列 => 固有値1,-2(重解) */
  {
    const uint32_t TEST_MAT_DIM = 3;
    float* mat = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);
    float* eig_vec = malloc(sizeof(float) * TEST_MAT_DIM * TEST_MAT_DIM);

    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 0) = -1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 1) = -1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 2) = -1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 0) = -1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 1) = -1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 2) =  1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 2, 0) = -1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 2, 1) =  1.0f;
    PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 2, 2) = -1.0f;

    Test_AssertEqual(PCAC_CalculateEigenVector(mat, eig_vec, TEST_MAT_DIM), PCAC_API_RESULT_OK);

    /* 固有値チェック */
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 0, 0), 1.0f);
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 1), -2.0f);
    Test_AssertFloat32EpsilonEqual(PCAC_MATRIX_AT(mat, TEST_MAT_DIM, 1, 1), -2.0f);

    free(mat);
    free(eig_vec);
  }
}

void testPCAC_CalculateTransformMatrixTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    uint32_t x, y;
    const uint32_t TEST_HEIGHT      = 10;
    const uint32_t TEST_WIDTH       = 20;
    const uint32_t TEST_BLOCK_SIZE  = 8;
    const uint32_t TEST_VARMAT_DIM  = TEST_BLOCK_SIZE * TEST_BLOCK_SIZE;
    float   *transmatrix;
    struct PNMPicture* pict = PNM_CreatePicture(TEST_WIDTH, TEST_HEIGHT);
    pict->header.format     = PNM_FORMAT_P2;
    transmatrix             = malloc(sizeof(float) * TEST_VARMAT_DIM * TEST_VARMAT_DIM);

    /* 全て1の画像 */
    for (x = 0; x < TEST_WIDTH; x++) {
      for (y = 0; y < TEST_HEIGHT; y++) {
        PNMPict_GRAY(pict, x, y) = x;
      }
    }

    Test_AssertEqual(PCAC_CalculateTransformMatrix(pict, transmatrix, TEST_BLOCK_SIZE), PCAC_API_RESULT_OK);

    free(transmatrix);
  }

  {
    const uint32_t TEST_BLOCK_SIZE  = 8;
    const uint32_t TEST_VARMAT_DIM  = TEST_BLOCK_SIZE * TEST_BLOCK_SIZE;
    float   *transmatrix;
    struct PNMPicture* pict = PNM_CreatePictureFromFile("JPEG_bin.pgm");
    transmatrix             = malloc(sizeof(float) * TEST_VARMAT_DIM * TEST_VARMAT_DIM);

    Test_AssertEqual(PCAC_CalculateTransformMatrix(pict, transmatrix, TEST_BLOCK_SIZE), PCAC_API_RESULT_OK);

    free(transmatrix);
  }
}

void testPCAC_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("PCAC Test Suite",
        NULL, testPCAC_Initialize, testPCAC_Finalize);

  Test_AddTest(suite, testPCAC_PictureBlockIndexAtTest);
  Test_AddTest(suite, testPCAC_CalculateAverageBlockIndexAtTest);
  Test_AddTest(suite, testPCAC_CalculateMulAverageBlockIndexAtTest);
  Test_AddTest(suite, testPCAC_CalculateCovarianceMatrixTest);
  Test_AddTest(suite, testPCAC_CalculateEigenVectorTest);
  Test_AddTest(suite, testPCAC_CalculateTransformMatrixTest);
}

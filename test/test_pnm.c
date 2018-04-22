#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../pnm.c"

/* 画像の解答データ */
#include "./data/JPEG_ascii_pbm_answer.h"
#include "./data/JPEG_ascii_pgm_answer.h"
#include "./data/JPEG_ascii_ppm_answer.h"

int testPNM_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testPNM_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

void testPNM_AllocateFreeImageTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 割り当てテスト */
  {
    uint32_t y;
    struct PNMImage* pnm;

    pnm = PNM_AllocateImage(20, 10);
    Test_AssertCondition(pnm != NULL);
    Test_AssertEqual(pnm->width, 20);
    Test_AssertEqual(pnm->height, 10);
    Test_AssertCondition(pnm->img != NULL);

    for (y = 0; y < pnm->height; y++) {
      Test_AssertCondition(pnm->img[y] != NULL);
    }

    PNM_FreeImage(pnm);
  }

  /* 割り当て失敗テスト */
  {
    Test_AssertCondition(PNM_AllocateImage(0, 1) == NULL);
    Test_AssertCondition(PNM_AllocateImage(1, 0) == NULL);
    Test_AssertCondition(PNM_AllocateImage(0, 0) == NULL);
  }

}

void testPNMParser_ParseStringTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 文字パーステスト */
  {
    struct PNMParser parser;
    int32_t ch, test_index;
    FILE* fp;
    const char* test_filename = "./data/for_test_parser.txt";
    const char* test_str = "abc d\te fghi\n\ngk lmn";

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    fputs(test_str, fp);
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForText(&parser, fp);

    /* 一致確認 */
    test_index = 0;
    while ((ch = PNMParser_GetNextCharacter(&parser)) != EOF) {
      Test_AssertEqual(ch, test_str[test_index++]);
    }

    fclose(fp);
  }

  /* コメント付き文字のパース */
  {
    struct PNMParser parser;
    int32_t ch, test_index;
    FILE* fp;
    const char* test_filename = "./data/for_test_parser.txt";
    const char* test_str  = "abc d\te f#ghi\n#\ng#k lmn";
    const char* answer    = "abc d\te f\n\ng";

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    fputs(test_str, fp);
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForText(&parser, fp);

    /* 一致確認 */
    test_index = 0;
    while ((ch = PNMParser_GetNextCharacter(&parser)) != EOF) {
      Test_AssertEqual(ch, answer[test_index++]);
    }

    fclose(fp);
  }

  /* 数値のパース */
  {
    struct PNMParser parser;
    uint32_t i_dig;
    FILE* fp;
    const char* test_filename = "./data/for_test_parser.txt";
    const int answers[3]  = {114514, 364364, 19191893};
    char test_str[200];
    int32_t test;

    sprintf(test_str, " %d \n \t # comment \n %d \n###comm\n\n\n\t\t\n %d", 
        answers[0], answers[1], answers[2]);

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    fputs(test_str, fp);
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForText(&parser, fp);

    /* 一致確認 */
    for (i_dig = 0; i_dig < 3; i_dig++) {
      test = PNMParser_GetNextInteger(&parser);
      Test_AssertEqual(test, answers[i_dig]);
    }

    fclose(fp);
  }
}

void testPNMParser_ParseBinaryTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* ビット単位の読み出し */
  {
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_parser.bin";
    const uint8_t answer[4] = { 0xDE, 0xAD, 0xBE, 0xAF };
    int32_t i, shift, test, ans;

    /* 解答の作成 */
    fp = fopen(test_filename, "wb");
    for (i = 0; i < 4; i++) { fputc(answer[i], fp); }
    fclose(fp);

    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForBinary(&parser, fp);

    /* 一致確認 */
    for (i = 0; i < 4; i++) {
      for (shift = 0; shift < 8; shift++) {
        test = PNMParser_GetBits(&parser, 1);
        ans  = (answer[i] >> (7-shift)) & 1;
        Test_AssertEqual(test, ans);
      }
    }

    fclose(fp);
  }

  /* バイト単位の読み出し */
  {
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_parser.bin";
    const uint8_t answer[4] = { 0xDE, 0xAD, 0xBE, 0xAF };
    int32_t i, test;

    /* 解答の作成 */
    fp = fopen(test_filename, "wb");
    for (i = 0; i < 4; i++) { fputc(answer[i], fp); }
    fclose(fp);

    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForBinary(&parser, fp);

    /* 一致確認 */
    for (i = 0; i < 4; i++) {
      test = PNMParser_GetBits(&parser, 8);
      Test_AssertEqual(test, answer[i]);
    }

    fclose(fp);
  }

  /* 複雑なパターンでテスト */
  {
#define ANSWER_BINARY_SIZE 32
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_parser.bin";
    uint8_t answer[ANSWER_BINARY_SIZE];
    int32_t i, test, ans, shift;

    /* 解答の作成 */
    srand(0);
    fp = fopen(test_filename, "wb");
    for (i = 0; i < ANSWER_BINARY_SIZE; i++) {
      answer[i] = rand() & 0xFF;
      fputc(answer[i], fp);
    }
    fclose(fp);

    /* ビット単位の一致確認 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForBinary(&parser, fp);
    for (i = 0; i < ANSWER_BINARY_SIZE; i++) {
      for (shift = 0; shift < 8; shift++) {
        test = PNMParser_GetBits(&parser, 1);
        ans  = (answer[i] >> (7-shift)) & 1;
        Test_AssertEqual(test, ans);
      }
    }
    fclose(fp);

    /* バイト単位の一致確認 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForBinary(&parser, fp);
    for (i = 0; i < ANSWER_BINARY_SIZE; i++) {
      test = PNMParser_GetBits(&parser, 8);
      Test_AssertEqual(test, answer[i]);
    }
    fclose(fp);
#undef BINARY_SIZE
  }
}

/* PNMヘッダの読み込みテスト */
void testPNMParser_ReadHeaderTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    FILE *fp;
    struct PNMParser parser;
    const char* test_filename = "./data/for_test_header.bin";
    const char* format_strs[] = {"P1", "P2", "P3", "P4", "P5", "P6"};
    const PNMFormat format_ans[]
      = {PNM_P1, PNM_P2, PNM_P3, PNM_P4, PNM_P5, PNM_P6};
    const int32_t test_width = 200;
    const int32_t test_height = 100;
    const int32_t test_max_brightness = 180;
    uint32_t i_test;

    /* 各フォーマットで正しく情報が取れるかテスト */
    for (i_test = 0; 
         i_test < sizeof(format_ans) / sizeof(format_ans[0]);
         i_test++) {
      PNMFormat format;
      PNMError  read_err;
      int32_t width, height, max_brightness;

      /* ヘッダ書き出し */
      fp = fopen(test_filename, "wb");
      if (format_ans[i_test] == PNM_P1
          || format_ans[i_test] == PNM_P4) {
        fprintf(fp, "%s \n# Aikatsu!Aikatsu!\n%d %d",
            format_strs[i_test], 
            test_width, test_height);
      } else {
        fprintf(fp, "%s \n# Aikatsu!Aikatsu!\n%d %d\n%d",
            format_strs[i_test], 
            test_width, test_height, test_max_brightness);
      }
      fclose(fp);

      /* 正しく取得できるか */
      fp = fopen(test_filename, "rb");
      PNMParser_InitializeForText(&parser, fp);
      read_err = PNMParser_ReadHeader(&parser, 
          &format, &width, &height, &max_brightness);
      Test_AssertEqual(read_err, PNM_ERROR_OK);
      Test_AssertEqual(format, format_ans[i_test]);
      Test_AssertEqual(width,  test_width);
      Test_AssertEqual(height, test_height);

      if (format_ans[i_test] != PNM_P1
          && format_ans[i_test] != PNM_P4) {
        Test_AssertEqual(max_brightness, test_max_brightness);
      }
      fclose(fp);
    }
  }
}

/* P1フォーマットの読み込みテスト */
void testPNMParser_ReadP1Test(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* P1フォーマットの読み取りテスト */
  {
#define TESTWIDTH   5
#define TESTHEIGHT  6
    struct PNMImage* image;
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_p1.bin";
    int32_t answer[TESTWIDTH][TESTHEIGHT];
    uint32_t x, y;

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    srand(0);
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        answer[x][y] = (int32_t)(rand() % 2);
        fprintf(fp, "%d ", answer[x][y]);
      }
      fputc('\n', fp);
    }
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForText(&parser, fp);
    image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
    Test_AssertCondition(image != NULL);

    /* 読み込み */
    Test_AssertEqual(
        PNMParser_ReadP1(&parser, image),
        PNM_ERROR_OK);

    /* 一致確認 */
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        Test_AssertEqual(PNMImg_BIT(image,x,y), answer[x][y]);
      }
    }

    /* 解放 */
    PNM_FreeImage(image);
    fclose(fp);
#undef TESTHEIGHT
#undef TESTWIDTH
  }
}

/* P2フォーマットの読み込みテスト */
void testPNMParser_ReadP2Test(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* P2フォーマットの読み取りテスト */
  {
#define TESTWIDTH   5
#define TESTHEIGHT  6
    struct PNMImage* image;
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_p2.bin";
    int32_t answer[TESTWIDTH][TESTHEIGHT];
    uint32_t x, y;

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    srand(0);
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        answer[x][y] = (int32_t)(rand() % 0x100);
        fprintf(fp, "%d ", answer[x][y]);
      }
      fputc('\n', fp);
    }
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForText(&parser, fp);
    image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
    Test_AssertCondition(image != NULL);

    /* 読み込み */
    Test_AssertEqual(
        PNMParser_ReadP2(&parser, image),
        PNM_ERROR_OK);

    /* 一致確認 */
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        Test_AssertEqual(PNMImg_GRAY(image,x,y), answer[x][y]);
      }
    }

    /* 解放 */
    PNM_FreeImage(image);
    fclose(fp);
#undef TESTHEIGHT
#undef TESTWIDTH
  }
}

/* P3フォーマットの読み込みテスト */
void testPNMParser_ReadP3Test(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* P3フォーマットの読み込みテスト */
  {
#define TESTWIDTH   6
#define TESTHEIGHT  5
    struct PNMImage* image;
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_p3.bin";
    int32_t answer[TESTWIDTH][TESTHEIGHT][3];
    uint32_t x, y, c;

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    srand(0);
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        for (c = 0; c < 3; c++) {
          answer[x][y][c] = (int32_t)(rand() % 0x100);
          fprintf(fp, "%d ", answer[x][y][c]);
        }
      }
    }
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForText(&parser, fp);
    image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
    Test_AssertCondition(image != NULL);

    /* 読み込み */
    Test_AssertEqual(
        PNMParser_ReadP3(&parser, image),
        PNM_ERROR_OK);

    /* 一致確認 */
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        Test_AssertEqual(PNMImg_R(image,x,y), answer[x][y][0]);
        Test_AssertEqual(PNMImg_G(image,x,y), answer[x][y][1]);
        Test_AssertEqual(PNMImg_B(image,x,y), answer[x][y][2]);
      }
    }

    /* 解放 */
    PNM_FreeImage(image);
    fclose(fp);
#undef TESTHEIGHT
#undef TESTWIDTH
  }
}

/* P4フォーマットの読み込みテスト */
void testPNMParser_ReadP4Test(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* P4フォーマットの読み込みテスト */
  {
#define TESTWIDTH   16
#define TESTWIDTH_STRIDE PNM_CalcStride(TESTWIDTH, 1)
#define TESTHEIGHT  2
    struct PNMImage* image;
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_p4.bin";
    int32_t answer[TESTWIDTH_STRIDE][TESTHEIGHT];
    uint32_t x, y, shift, ans;

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    srand(0);
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH_STRIDE; x++) {
        answer[x][y] = (int32_t)(rand() % 0x100);
        fputc(answer[x][y], fp);
      }
    }
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForBinary(&parser, fp);
    image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
    Test_AssertCondition(image != NULL);

    /* 読み込み */
    Test_AssertEqual(
        PNMParser_ReadP4(&parser, image),
        PNM_ERROR_OK);

    /* 一致確認 */
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH_STRIDE; x++) {
        for (shift = 0; shift < 8; shift++) {
          ans = (answer[x][y] >> (7-shift)) & 1;
          Test_AssertEqual(PNMImg_BIT(image,8*x+shift,y), ans);
        }
      }
    }

    /* 解放 */
    PNM_FreeImage(image);
    fclose(fp);
#undef TESTHEIGHT
#undef TESTWIDTH_STRIDE
#undef TESTWIDTH
  }
}

/* P5フォーマットの読み込みテスト */
void testPNMParser_ReadP5Test(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* P5フォーマットの読み込みテスト */
  {
#define TESTWIDTH   16
#define TESTWIDTH_STRIDE PNM_CalcStride(TESTWIDTH, 8)
#define TESTHEIGHT  2
    struct PNMImage* image;
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_p5.bin";
    int32_t answer[TESTWIDTH_STRIDE][TESTHEIGHT];
    uint32_t x, y;

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    srand(0);
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH_STRIDE; x++) {
        answer[x][y] = (int32_t)(rand() % 0x100);
        fputc(answer[x][y], fp);
      }
    }
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForBinary(&parser, fp);
    image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
    Test_AssertCondition(image != NULL);

    /* 読み込み */
    Test_AssertEqual(
        PNMParser_ReadP5(&parser, image),
        PNM_ERROR_OK);

    /* 一致確認 */
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        Test_AssertEqual(PNMImg_GRAY(image,x,y), answer[x][y]);
      }
    }

    /* 解放 */
    PNM_FreeImage(image);
    fclose(fp);
#undef TESTHEIGHT
#undef TESTWIDTH_STRIDE
#undef TESTWIDTH
  }
}

/* P6フォーマットの読み込みテスト */
void testPNMParser_ReadP6Test(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* P6フォーマットの読み込みテスト */
  {
#define TESTWIDTH   16
#define TESTHEIGHT  2
    struct PNMImage* image;
    struct PNMParser parser;
    FILE* fp;
    const char* test_filename = "./data/for_test_p6.bin";
    int32_t answer[TESTWIDTH][TESTHEIGHT][3];
    uint32_t x, y, c;

    /* テストファイル作成 */
    fp = fopen(test_filename, "wb");
    srand(0);
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        for (c = 0; c < 3; c++) {
          answer[x][y][c] = (int32_t)(rand() % 0x100);
          fputc(answer[x][y][c], fp);
        }
      }
    }
    fclose(fp);

    /* パーサ初期化 */
    fp = fopen(test_filename, "rb");
    PNMParser_InitializeForBinary(&parser, fp);
    image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
    Test_AssertCondition(image != NULL);

    /* 読み込み */
    Test_AssertEqual(
        PNMParser_ReadP6(&parser, image),
        PNM_ERROR_OK);

    /* 一致確認 */
    for (y = 0; y < TESTHEIGHT; y++) {
      for (x = 0; x < TESTWIDTH; x++) {
        Test_AssertEqual(PNMImg_R(image,x,y), answer[x][y][0]);
        Test_AssertEqual(PNMImg_G(image,x,y), answer[x][y][1]);
        Test_AssertEqual(PNMImg_B(image,x,y), answer[x][y][2]);
      }
    }

    /* 解放 */
    PNM_FreeImage(image);
    fclose(fp);
#undef TESTHEIGHT
#undef TESTWIDTH
  }
}

/* 実画像の読み込みテスト */
void testPNMParser_ReadPNMImageTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* テスト画像の幅と高さ */
  const uint32_t TESTIMG_WIDTH  = 250;
  const uint32_t TESTIMG_HEIGHT = 250;

  /* 読み込みテスト */
  {
    int32_t is_ok;
    uint32_t x, y, imginx;
    struct PNMImage* image;
    uint32_t i_test;
    const char *filename[] = {
      "./data/JPEG_ascii.pbm",
      "./data/JPEG_ascii.pgm",
      "./data/JPEG_ascii.ppm",
      "./data/JPEG_bin.pbm",
      "./data/JPEG_bin.pgm",
      "./data/JPEG_bin.ppm",
    };
    const PNMFormat format_answer[] = {
      PNM_P1, PNM_P2, PNM_P3, PNM_P4, PNM_P5, PNM_P6
    };
    const uint8_t* imgdata_answers[] = {
      JPEG_ascii_pbm_answer,
      JPEG_ascii_pgm_answer,
      JPEG_ascii_ppm_answer,
      JPEG_ascii_pbm_answer,
      JPEG_ascii_pgm_answer,
      JPEG_ascii_ppm_answer,
    };
    const uint32_t NUM_TESTS = sizeof(filename) / sizeof(filename[0]);

    for (i_test = 0; i_test < NUM_TESTS; i_test++) {
      /* 読み込み */
      image = PNM_ReadFile(filename[i_test]);
      Test_AssertCondition(image != NULL);

      /* 正しく取得できたか確認 */
      Test_AssertEqual(image->format, format_answer[i_test]);
      Test_AssertEqual(image->width,  TESTIMG_WIDTH);
      Test_AssertEqual(image->height, TESTIMG_HEIGHT);

      /* 内容確認 */
      is_ok = 1;
      if (format_answer[i_test] == PNM_P1
          || format_answer[i_test] == PNM_P4) {
        for (y = 0; y < image->height; y++) {
          for (x = 0; x < image->width; x++) {
            imginx = y * image->width + x;
            if (PNMImg_BIT(image, x, y) != imgdata_answers[i_test][imginx]) {
              is_ok = 0;
              goto CHECK_END;
            }
          }
        }
      } else if (format_answer[i_test] == PNM_P2
          || format_answer[i_test] == PNM_P5) {
        for (y = 0; y < image->height; y++) {
          for (x = 0; x < image->width; x++) {
            imginx = y * image->width + x;
            if (PNMImg_GRAY(image, x, y) != imgdata_answers[i_test][imginx]) {
              is_ok = 0;
              goto CHECK_END;
            }
          }
        }
      } else if (format_answer[i_test] == PNM_P3
          || format_answer[i_test] == PNM_P6) {
        for (y = 0; y < image->height; y++) {
          for (x = 0; x < image->width; x++) {
            imginx = 3 * (y * image->width + x);
            if (PNMImg_R(image, x, y) != imgdata_answers[i_test][imginx+0]
                || PNMImg_G(image, x, y) != imgdata_answers[i_test][imginx+1]
                || PNMImg_B(image, x, y) != imgdata_answers[i_test][imginx+2]) {
              is_ok = 0;
              goto CHECK_END;
            }
          }
        }
      } else {
        is_ok = 0;
      }

CHECK_END:
      Test_AssertEqual(is_ok, 1);
      PNM_FreeImage(image);
    }

  }

}

/* ビット書き込みテスト */
void testPNMBitWriter_WriteBitTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* ビット単位の書き込み */
  {
    struct PNMBitWriter writer;
    const char* test_filename = "./data/for_test_bitwriter.bin";
    FILE *fp = fopen(test_filename, "wb");

    PNMBitWriter_Initialize(&writer, fp);
    /* 0xAC を書き込む */
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 1, 1), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0, 1), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 1, 1), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0, 1), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 1, 1), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 1, 1), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0, 1), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0, 1), PNM_ERROR_OK);
    fclose(fp);

    /* 内容チェック */
    fp = fopen(test_filename, "rb");
    Test_AssertCondition(fp != NULL);
    Test_AssertEqual(fgetc(fp), 0xAC);
    fclose(fp);
  }

  /* バイト単位の書き込み */
  {
    struct PNMBitWriter writer;
    const char* test_filename = "./data/for_test_bitwriter.bin";
    FILE *fp = fopen(test_filename, "wb");

    PNMBitWriter_Initialize(&writer, fp);
    /* 0xDEADBEAF を書き込む */
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0xDE, 8), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0xAD, 8), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0xBE, 8), PNM_ERROR_OK);
    Test_AssertEqual(PNMBitWriter_PutBits(&writer, 0xAF, 8), PNM_ERROR_OK);
    fclose(fp);

    /* 内容チェック */
    fp = fopen(test_filename, "rb");
    Test_AssertCondition(fp != NULL);
    Test_AssertEqual(fgetc(fp), 0xDE);
    Test_AssertEqual(fgetc(fp), 0xAD);
    Test_AssertEqual(fgetc(fp), 0xBE);
    Test_AssertEqual(fgetc(fp), 0xAF);
    fclose(fp);
  }
}

/* 画像書き込みテスト */
void testPNMBitWriter_WritePNMImageTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* P1フォーマットの書き込みテスト */
  {
#define TESTWIDTH   16
#define TESTHEIGHT  17
    struct PNMImage*  image;
    const char*       test_filename[] = {
      "./data/for_test_write_p1.pbm",
      "./data/for_test_write_p4.pbm",
    };
    const PNMFormat   formats[] = { PNM_P1, PNM_P4 };
    uint32_t          answer[TESTWIDTH][TESTHEIGHT];
    uint32_t          x, y, i_test;
    int32_t           is_ok;

    for (i_test = 0; i_test < sizeof(test_filename) / sizeof(test_filename[0]); i_test++) {
      /* 画像を作ってみる */
      image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
      image->format = formats[i_test];
      srand(0);
      for (y = 0; y < TESTHEIGHT; y++) {
        for (x = 0; x < TESTWIDTH; x++) {
          answer[x][y] = rand() % 2;
          PNMImg_BIT(image, x, y) = answer[x][y];
        }
      }

      /* 書き込み */
      Test_AssertEqual(PNM_WriteFile(test_filename[i_test], image), PNM_APIRESULT_OK);
      PNM_FreeImage(image);

      /* ちゃんと作れているか確認 */
      image = PNM_ReadFile(test_filename[i_test]);
      Test_AssertCondition(image != NULL);
      Test_AssertEqual(image->format, formats[i_test]);

      /* 内容確認 */
      is_ok = 1;
      for (y = 0; y < TESTHEIGHT; y++) {
        for (x = 0; x < TESTWIDTH; x++) {
          if (PNMImg_BIT(image, x, y) != answer[x][y]) {
            is_ok = 0;
            goto CHECK_END_P1P4;
          }
        }
      }
CHECK_END_P1P4:
      Test_AssertEqual(is_ok, 1);
      PNM_FreeImage(image);
    }

#undef TESTWIDTH
#undef TESTHEIGHT
  }

  /* P2フォーマットの書き込みテスト */
  {
#define TESTWIDTH   32
#define TESTHEIGHT  50
    struct PNMImage*  image;
    const char*       test_filename[] = {
      "./data/for_test_write_p2.pgm",
      "./data/for_test_write_p5.pgm",
    };
    const PNMFormat   formats[] = { PNM_P2, PNM_P5 };
    uint32_t          answer[TESTWIDTH][TESTHEIGHT];
    uint32_t          x, y, i_test;
    int32_t           is_ok;

    for (i_test = 0; i_test < sizeof(test_filename) / sizeof(test_filename[0]); i_test++) {
      /* 画像を作ってみる */
      image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
      image->format = formats[i_test];
      image->max_brightness = 0xFF;
      srand(0);
      for (y = 0; y < TESTHEIGHT; y++) {
        for (x = 0; x < TESTWIDTH; x++) {
          answer[x][y] = rand() % 0x100;
          PNMImg_GRAY(image, x, y) = answer[x][y];
        }
      }

      /* 書き込み */
      Test_AssertEqual(PNM_WriteFile(test_filename[i_test], image), PNM_APIRESULT_OK);
      PNM_FreeImage(image);

      /* ちゃんと作れているか確認 */
      image = PNM_ReadFile(test_filename[i_test]);
      Test_AssertCondition(image != NULL);
      Test_AssertEqual(image->format, formats[i_test]);

      /* 内容確認 */
      is_ok = 1;
      for (y = 0; y < TESTHEIGHT; y++) {
        for (x = 0; x < TESTWIDTH; x++) {
          if (PNMImg_GRAY(image, x, y) != answer[x][y]) {
            is_ok = 0;
            goto CHECK_END_P2P5;
          }
        }
      }
CHECK_END_P2P5:
      Test_AssertEqual(is_ok, 1);
      PNM_FreeImage(image);
    }
#undef TESTWIDTH
#undef TESTHEIGHT
  }

  /* P3フォーマットの書き込みテスト */
  {
#define TESTWIDTH   71
#define TESTHEIGHT  50
    struct PNMImage*  image;
    const char*       test_filename[] = {
      "./data/for_test_write_p3.ppm",
      "./data/for_test_write_p6.ppm",
    };
    const PNMFormat   formats[] = { PNM_P3, PNM_P6 };
    uint32_t          answer[TESTWIDTH][TESTHEIGHT][3];
    uint32_t          x, y, i_test;
    int32_t           is_ok;

    for (i_test = 0; i_test < sizeof(test_filename) / sizeof(test_filename[0]); i_test++) {
      /* 画像を作ってみる */
      image = PNM_AllocateImage(TESTWIDTH, TESTHEIGHT);
      image->format = formats[i_test];
      image->max_brightness = 0xFF;
      srand(0);
      for (y = 0; y < TESTHEIGHT; y++) {
        for (x = 0; x < TESTWIDTH; x++) {
          answer[x][y][0] = rand() % 0x100;
          answer[x][y][1] = rand() % 0x100;
          answer[x][y][2] = rand() % 0x100;
          PNMImg_R(image, x, y) = answer[x][y][0];
          PNMImg_G(image, x, y) = answer[x][y][1];
          PNMImg_B(image, x, y) = answer[x][y][2];
        }
      }

      /* 書き込み */
      Test_AssertEqual(PNM_WriteFile(test_filename[i_test], image), PNM_APIRESULT_OK);
      PNM_FreeImage(image);

      /* ちゃんと作れているか確認 */
      image = PNM_ReadFile(test_filename[i_test]);
      Test_AssertCondition(image != NULL);
      Test_AssertEqual(image->format, formats[i_test]);

      /* 内容確認 */
      is_ok = 1;
      for (y = 0; y < TESTHEIGHT; y++) {
        for (x = 0; x < TESTWIDTH; x++) {
          if ((PNMImg_R(image, x, y) != answer[x][y][0])
              || (PNMImg_G(image, x, y) != answer[x][y][1])
              || (PNMImg_B(image, x, y) != answer[x][y][2])) {
            is_ok = 0;
            goto CHECK_END_P3P6;
          }
        }
      }
CHECK_END_P3P6:
      Test_AssertEqual(is_ok, 1);
      PNM_FreeImage(image);
    }

#undef TESTWIDTH
#undef TESTHEIGHT
  }

}

/* 実画像の読み書きテスト */
void testPNMBitWriter_ReadWritePNMImageTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    struct PNMImage*  image;
    struct PNMImage*  test;
    uint32_t i_test;
    uint32_t is_ok, x, y;
    const char*       test_filename[] = {
      "./data/JPEG_ascii.pbm",
      "./data/JPEG_ascii.pgm",
      "./data/JPEG_ascii.ppm",
      "./data/JPEG_bin.pbm",
      "./data/JPEG_bin.pgm",
      "./data/JPEG_bin.ppm",
    };
    const char*       temp_filename = "./data/tmp.bin";

    /* 一回読み書きしたファイルが壊れていないか？ */
    for (i_test = 0; i_test < sizeof(test_filename) / sizeof(test_filename[0]); i_test++) {
      /* テストファイルを読み出し、内容はそのまま別名で書き出す */
      image = PNM_ReadFile(test_filename[i_test]);
      Test_AssertEqual(PNM_WriteFile(temp_filename, image), PNM_APIRESULT_OK);
      /* 書き出したファイルを読む */
      test  = PNM_ReadFile(temp_filename);

      /* 基本情報の一致確認 */
      Test_AssertEqual(image->format, test->format);
      Test_AssertEqual(image->width,  test->width);
      Test_AssertEqual(image->height, test->height);
      if (image->format != PNM_P1 && image->format != PNM_P4) {
        Test_AssertEqual(image->max_brightness, test->max_brightness);
      }

      /* 画素の一致確認 */
      is_ok = 1;
      for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
          if (memcmp(&(image->img[y][x]), &(test->img[y][x]), sizeof(PNMPixel)) != 0) {
            is_ok = 0;
            goto CHECK_END;
          }
        }
      }
CHECK_END:
      Test_AssertEqual(is_ok, 1);
      PNM_FreeImage(image);
      PNM_FreeImage(test);
    }

  }

}

void testPNM_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("PNM Test Suite",
        NULL, testPNM_Initialize, testPNM_Finalize);

  Test_AddTest(suite, testPNM_AllocateFreeImageTest);
  Test_AddTest(suite, testPNMParser_ParseStringTest);
  Test_AddTest(suite, testPNMParser_ParseBinaryTest);
  Test_AddTest(suite, testPNMParser_ReadHeaderTest);
  Test_AddTest(suite, testPNMParser_ReadP1Test);
  Test_AddTest(suite, testPNMParser_ReadP2Test);
  Test_AddTest(suite, testPNMParser_ReadP3Test);
  Test_AddTest(suite, testPNMParser_ReadP4Test);
  Test_AddTest(suite, testPNMParser_ReadP5Test);
  Test_AddTest(suite, testPNMParser_ReadP6Test);
  Test_AddTest(suite, testPNMParser_ReadPNMImageTest);
  Test_AddTest(suite, testPNMBitWriter_WriteBitTest);
  Test_AddTest(suite, testPNMBitWriter_WritePNMImageTest);
  Test_AddTest(suite, testPNMBitWriter_ReadWritePNMImageTest);

}

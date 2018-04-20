#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../pnm.c"

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
    PNMParser_Initialize(&parser, fp);

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
    PNMParser_Initialize(&parser, fp);

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
    PNMParser_Initialize(&parser, fp);

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
    PNMParser_Initialize(&parser, fp);
    /* FIXME:バッファ初期化をどうするか。バイナリ向けの初期化？バッファ分ける？ */
    parser.u_buffer.b.bit_count = 0;

    /* 一致確認 */
    for (i = 0; i < 4; i++) {
      for (shift = 0; shift < 8; shift++) {
        test = PNMParser_GetBit(&parser);
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
    PNMParser_Initialize(&parser, fp);
    /* FIXME:バッファ初期化をどうするか。バイナリ向けの初期化？バッファ分ける？ */
    parser.u_buffer.b.bit_count = 0;

    /* 一致確認 */
    for (i = 0; i < 4; i++) {
      test = PNMParser_GetBits(&parser, 8);
      Test_AssertEqual(test, answer[i]);
    }

    fclose(fp);
  }

  /* 複雑なパターンでテスト */
  {
#define ANSWER_BINARY_SIZE 64
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
    PNMParser_Initialize(&parser, fp);
    parser.u_buffer.b.bit_count = 0;
    for (i = 0; i < ANSWER_BINARY_SIZE; i++) {
      for (shift = 0; shift < 8; shift++) {
        test = PNMParser_GetBit(&parser);
        ans  = (answer[i] >> (7-shift)) & 1;
        Test_AssertEqual(test, ans);
      }
    }
    fclose(fp);

    /* バイト単位の一致確認 */
    fp = fopen(test_filename, "rb");
    PNMParser_Initialize(&parser, fp);
    parser.u_buffer.b.bit_count = 0;
    for (i = 0; i < ANSWER_BINARY_SIZE; i++) {
      test = PNMParser_GetBits(&parser, 8);
      Test_AssertEqual(test, answer[i]);
    }
    fclose(fp);
#undef BINARY_SIZE
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
}

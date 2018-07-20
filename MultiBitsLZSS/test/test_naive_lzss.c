#include "test.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

/* テスト対象のモジュール */
#include "../naive_lzss.c"

int testNaiveLZSS_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testNaiveLZSS_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ハンドル作成テスト */
void testNaiveLZSS_CreateTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    struct NaiveLZSS* lzss = NaiveLZSS_Create(1, 2, 2, 4);
    Test_AssertCondition(lzss != NULL);
    lzss->symbol_bits   = 1;
    lzss->window_size   = 2;
    lzss->min_match_len = 2;
    lzss->max_match_len = 4;
    Test_AssertCondition(lzss->slide_window != NULL);
    Test_AssertEqual(lzss->insert_pos, 0);
    Test_AssertEqual(lzss->lookahead_begin_pos, 0);
    Test_AssertEqual(lzss->search_begin_pos, 0);
    NaiveLZSS_Destroy(lzss);
  }

  /* 失敗テスト */
  {
    Test_AssertCondition(NaiveLZSS_Create(0, 2, 2, 4) == NULL);
    Test_AssertCondition(NaiveLZSS_Create(1, 3, 2, 4) == NULL);
    Test_AssertCondition(NaiveLZSS_Create(1, 2000, 2, 4) == NULL);
    Test_AssertCondition(NaiveLZSS_Create(0, 2, 3, 2) == NULL);
  }
}

/* エンコードテスト */
void testNaiveLZSS_EncodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

}

/* エンコードデコードテスト */
void testNaiveLZSS_EncodeDecodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 短い8ビット符号で符号化してみる */
  {
    struct NaiveLZSS* lzss;
    struct BitStream* strm;
    uint32_t i, is_ok;
    const uint32_t test_symbol[] = {1, 0, 1, 0, 1, 0, 1};
    uint32_t test[7];

    lzss = NaiveLZSS_Create(8, 16, 1, 2);
    strm = BitStream_Open("encdectest1.bin", "wb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Encode(lzss, strm, test_symbol, 7),
        NAIVE_LZSS_APIRESULT_OK);
    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);


    lzss = NaiveLZSS_Create(8, 16, 1, 2);
    strm = BitStream_Open("encdectest1.bin", "rb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Decode(lzss, strm, test, 7),
        NAIVE_LZSS_APIRESULT_OK);
    
    is_ok = 1;
    for (i = 0; i < 7; i++) {
      if (test[i] != test_symbol[i]) {
        is_ok = 0;
        break;
      }
    }

    Test_AssertEqual(is_ok, 1);
    NaiveLZSS_Destroy(lzss);

    BitStream_Close(strm);
  }

  /* 短い8ビット符号で符号化してみる */
  {
    struct NaiveLZSS* lzss;
    struct BitStream* strm;
    uint32_t is_ok, i;
    const uint32_t test_symbol[] = {0, 1, 0, 0, 0, 1, 0, 1, 2};
    uint32_t test[9];

    lzss = NaiveLZSS_Create(8, 16, 1, 2);
    strm = BitStream_Open("encdectest2.bin", "wb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Encode(lzss, strm, test_symbol, 9),
        NAIVE_LZSS_APIRESULT_OK);
    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);


    lzss = NaiveLZSS_Create(8, 16, 1, 2);
    strm = BitStream_Open("encdectest2.bin", "rb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Decode(lzss, strm, test, 9),
        NAIVE_LZSS_APIRESULT_OK);

    is_ok = 1;
    for (i = 0; i < 9; i++) {
      if (test[i] != test_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);
  }

  /* 7ビット符号で符号化してみる */
  /* サイズを超えるものに対応できるか？ */
  {
#define NUM_TEST_SYMBOLS 128
    struct NaiveLZSS* lzss;
    struct BitStream* strm;
    uint32_t is_ok, i;
    uint32_t answer[NUM_TEST_SYMBOLS];
    uint32_t test[NUM_TEST_SYMBOLS];

    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer[i] = i;
    }

    lzss = NaiveLZSS_Create(7, 16, 1, 2);
    strm = BitStream_Open("encdectest3.bin", "wb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Encode(lzss, strm, answer, NUM_TEST_SYMBOLS),
        NAIVE_LZSS_APIRESULT_OK);
    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);


    lzss = NaiveLZSS_Create(7, 16, 1, 2);
    strm = BitStream_Open("encdectest3.bin", "rb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Decode(lzss, strm, test, NUM_TEST_SYMBOLS),
        NAIVE_LZSS_APIRESULT_OK);

    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test[i] != answer[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);
#undef NUM_TEST_SYMBOLS
  }

  /* 7ビット符号で符号化してみる */
  {
#define NUM_TEST_SYMBOLS 128
    struct NaiveLZSS* lzss;
    struct BitStream* strm;
    uint32_t is_ok, i;
    uint32_t answer[NUM_TEST_SYMBOLS];
    uint32_t test[NUM_TEST_SYMBOLS];

    /* 半分の周期で折り返すパターン */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (i < NUM_TEST_SYMBOLS / 2) {
        answer[i] = i / 16;
      } else {
        answer[i] = 128/16 - i / 16;
      }
    }

    lzss = NaiveLZSS_Create(7, 128, 3, 5);
    strm = BitStream_Open("encdectest4.bin", "wb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Encode(lzss, strm, answer, NUM_TEST_SYMBOLS),
        NAIVE_LZSS_APIRESULT_OK);
    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);


    lzss = NaiveLZSS_Create(7, 128, 3, 5);
    strm = BitStream_Open("encdectest4.bin", "rb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Decode(lzss, strm, test, NUM_TEST_SYMBOLS),
        NAIVE_LZSS_APIRESULT_OK);

    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test[i] != answer[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);
#undef NUM_TEST_SYMBOLS
  }

  /* ランダム */
  {
#define NUM_TEST_SYMBOLS 1024
    struct NaiveLZSS* lzss;
    struct BitStream* strm;
    uint32_t is_ok, i;
    uint32_t answer[NUM_TEST_SYMBOLS];
    uint32_t test[NUM_TEST_SYMBOLS];

    /* ランダムな2値シンボル */
    /* ランダム幅が大きいと一致しないので */
    srand(0);
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
        answer[i] = rand() % 2;
    }

    lzss = NaiveLZSS_Create(9, 512, 3, 18);
    strm = BitStream_Open("encdectest5.bin", "wb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Encode(lzss, strm, answer, NUM_TEST_SYMBOLS),
        NAIVE_LZSS_APIRESULT_OK);
    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);


    lzss = NaiveLZSS_Create(9, 512, 3, 18);
    strm = BitStream_Open("encdectest5.bin", "rb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Decode(lzss, strm, test, NUM_TEST_SYMBOLS),
        NAIVE_LZSS_APIRESULT_OK);

    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test[i] != answer[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);
#undef NUM_TEST_SYMBOLS
  }

  /* 実バイナリデータ */
  {
    const char* filename = "syokuji_ramen_abura.pgm";
    struct NaiveLZSS* lzss;
    struct BitStream* strm;
    uint32_t is_ok, i;
    uint32_t* answer;
    uint32_t* test;
    FILE* fp;
    uint32_t fsize;
    struct stat fstat;

    fp = fopen(filename, "rb");
    stat(filename, &fstat);
    fsize = fstat.st_size;

    answer = malloc(sizeof(uint32_t) * fsize);
    test = malloc(sizeof(uint32_t) * fsize);
    for (i = 0; i < fsize; i++) {
      answer[i] = fgetc(fp);
    }

    lzss = NaiveLZSS_Create(8, 8192, 3, 18);
    strm = BitStream_Open("encdectest6.bin", "wb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Encode(lzss, strm, answer, fsize),
        NAIVE_LZSS_APIRESULT_OK);
    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);


    lzss = NaiveLZSS_Create(8, 8192, 3, 18);
    strm = BitStream_Open("encdectest6.bin", "rb", NULL, 0);
    Test_AssertEqual(
        NaiveLZSS_Decode(lzss, strm, test, fsize),
        NAIVE_LZSS_APIRESULT_OK);

    is_ok = 1;
    for (i = 0; i < fsize; i++) {
      if (test[i] != answer[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    NaiveLZSS_Destroy(lzss);
    BitStream_Close(strm);
  }

}

void testNaiveLZSS_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("NaiveLZSS Test Suite",
        NULL, testNaiveLZSS_Initialize, testNaiveLZSS_Finalize);

  Test_AddTest(suite, testNaiveLZSS_CreateTest);
  Test_AddTest(suite, testNaiveLZSS_EncodeTest);
  Test_AddTest(suite, testNaiveLZSS_EncodeDecodeTest);
}

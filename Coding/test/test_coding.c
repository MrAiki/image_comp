#include "test.h"
#include <stdlib.h>
#include <stdio.h>

/* 依存モジュール群 */
#include "../../BitStream/bit_stream.c"
#include "../../AdaptiveHuffman/adaptive_huffman.c"

/* テスト対象のモジュール */
#include "../coding.c"

int testCoding_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testCoding_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ワイル符号化テスト */
void testCoding_WyleCondigTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 入力を復元できるか？ */
  {
#define NUM_TEST_SYMBOLS 40
    int32_t i, is_ok;
    int32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_wylecoding.bin";

    /* 出力 */
    strm = BitStream_Open(test_filename, "w", NULL, 0);
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      int32_t symbol = i - NUM_TEST_SYMBOLS / 2;
      test_symbol[i] = symbol;
      SignedWyle_PutCode(strm, symbol);
    }
    BitStream_Close(strm);

    /* 入力取得 */
    is_ok = 1;
    strm = BitStream_Open(test_filename, "r", NULL, 0);
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (SignedWyle_GetCode(strm) != test_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    BitStream_Close(strm);
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS
  }

  {
#define NUM_TEST_SYMBOLS 40
    int32_t i, is_ok;
    int32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_wylecoding.bin";

    /* 出力 */
    srand(0);
    strm = BitStream_Open(test_filename, "w", NULL, 0);
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      int32_t symbol = rand() & 0xFFFF - 0xFFFF / 2;
      test_symbol[i] = symbol;
      SignedWyle_PutCode(strm, symbol);
    }
    BitStream_Close(strm);

    /* 入力取得 */
    is_ok = 1;
    strm = BitStream_Open(test_filename, "r", NULL, 0);
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (SignedWyle_GetCode(strm) != test_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    BitStream_Close(strm);
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS
  }
}

/* ゴロム符号化テスト */
void testCoding_GolombCondigTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* ライス符号: 入力を復元できるか？ */
  {
#define NUM_TEST_SYMBOLS 512
    int32_t i, is_ok;
    uint32_t m;
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_ricecoding.bin";

    for (m = 1; m <= 64; m *= 2) {
      /* 出力 */
      strm = BitStream_Open(test_filename, "w", NULL, 0);
      for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
        test_symbol[i] = i;
        Golomb_PutCode(strm, m, test_symbol[i]);
      }
      BitStream_Close(strm);

      /* 入力取得 */
      is_ok = 1;
      strm = BitStream_Open(test_filename, "r", NULL, 0);
      for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
        if (Golomb_GetCode(strm, m) != test_symbol[i]) {
          is_ok = 0;
          break;
        }
      }
      BitStream_Close(strm);
      Test_AssertEqual(is_ok, 1);
    }

#undef NUM_TEST_SYMBOLS
  }

  /* ゴロム符号: 入力を復元できるか？ */
  {
#define NUM_TEST_SYMBOLS 512
    int32_t i, is_ok;
    uint32_t m;
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_golombcoding.bin";

    for (m = 1; m < 64; m++) {
      /* 出力 */
      strm = BitStream_Open(test_filename, "w", NULL, 0);
      for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
        test_symbol[i] = i;
        Golomb_PutCode(strm, m, test_symbol[i]);
      }
      BitStream_Close(strm);

      /* 入力取得 */
      is_ok = 1;
      strm = BitStream_Open(test_filename, "r", NULL, 0);
      for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
        if (Golomb_GetCode(strm, m) != test_symbol[i]) {
          is_ok = 0;
          break;
        }
      }
      BitStream_Close(strm);
      Test_AssertEqual(is_ok, 1);
    }

#undef NUM_TEST_SYMBOLS
  }
}

/* Pack Bits符号化テスト */
void testCoding_PackBitsEncodeDecodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 符号復号テスト1: 短いランだけ */
  {
#define NUM_TEST_SYMBOLS 100
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode1.bin";

    /* 全て1のコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = 1;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    PackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    PackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト2: 長いランだけ */
  {
#define NUM_TEST_SYMBOLS 300
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode2.bin";

    /* 全て1のコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = 1;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    PackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    PackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト3: 短い非ランだけ */
  {
#define NUM_TEST_SYMBOLS 100
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode3.bin";

    /* 重複がないコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = i;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    PackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    PackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト4: 長い非ランだけ */
  {
#define NUM_TEST_SYMBOLS 300
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode4.bin";

    /* 重複がないコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = i;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    PackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    PackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト5: ラン -> 非ラン */
  {
#define NUM_TEST_SYMBOLS 600
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode5.bin";

    /* ラン -> 非ラン符号パターン生成 */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (i < NUM_TEST_SYMBOLS / 2) {
        answer_symbol[i] = 1;
        continue;
      }
      answer_symbol[i] = i;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    PackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    PackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト6: 非ラン -> ラン */
  {
#define NUM_TEST_SYMBOLS 600
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode6.bin";

    /* 非ラン -> ラン符号パターン生成 */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (i < NUM_TEST_SYMBOLS / 2) {
        answer_symbol[i] = i;
        continue;
      }
      answer_symbol[i] = 1;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    PackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    PackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }
  
  /* 符号復号テスト7: ランダム */
  {
#define NUM_TEST_SYMBOLS 200
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode7.bin";

    /* ランダム */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = rand() % 3;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    PackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    PackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

}

/* Mineo版Pack Bits符号化テスト */
void testCoding_MineoPackBitsEncodeDecodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 符号復号テスト1: 短いランだけ */
  {
#define NUM_TEST_SYMBOLS 100
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode1.bin";

    /* 全て1のコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = 1;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト2: 長いランだけ */
  {
#define NUM_TEST_SYMBOLS 300
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode2.bin";

    /* 全て1のコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = 1;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト3: 短い非ランだけ */
  {
#define NUM_TEST_SYMBOLS 100
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode3.bin";

    /* 重複がないコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = i;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト4: 長い非ランだけ */
  {
#define NUM_TEST_SYMBOLS 300
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode4.bin";

    /* 重複がないコード */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = i;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト5: ラン -> 非ラン */
  {
#define NUM_TEST_SYMBOLS 600
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode5.bin";

    /* ラン -> 非ラン符号パターン生成 */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (i < NUM_TEST_SYMBOLS / 2) {
        answer_symbol[i] = 1;
        continue;
      }
      answer_symbol[i] = i;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト6: 非ラン -> ラン */
  {
#define NUM_TEST_SYMBOLS 600
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode6.bin";

    /* 非ラン -> ラン符号パターン生成 */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (i < NUM_TEST_SYMBOLS / 2) {
        answer_symbol[i] = i;
        continue;
      }
      answer_symbol[i] = 1;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }
  
  /* 符号復号テスト7: ランダム */
  {
#define NUM_TEST_SYMBOLS 200
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode7.bin";

    /* ランダム */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      answer_symbol[i] = rand() % 3;
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

  /* 符号復号テスト8: 頭と末尾に0が付与されたランダム */
  {
#define NUM_TEST_SYMBOLS 1024
    int32_t i, is_ok;
    uint32_t answer_symbol[NUM_TEST_SYMBOLS];
    uint32_t test_symbol[NUM_TEST_SYMBOLS];
    struct BitStream *strm;
    const char* test_filename = "test_packbitcode8.bin";

    /* 頭と末尾に0がついたランダム */
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (i < 200 || i > 800) {
        answer_symbol[i] = 0;
      } else {
        answer_symbol[i] = rand() % 3;
      }
    }

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    MineoPackBits_Encode(strm,
        answer_symbol, NUM_TEST_SYMBOLS, 4, 1, 8);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);
    MineoPackBits_Decode(strm,
        test_symbol, NUM_TEST_SYMBOLS, 4, 8);
    BitStream_Close(strm);
    is_ok = 1;
    for (i = 0; i < NUM_TEST_SYMBOLS; i++) {
      if (test_symbol[i] != answer_symbol[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

#undef NUM_TEST_SYMBOLS 
  }

}

void testCoding_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("Coding Test Suite",
        NULL, testCoding_Initialize, testCoding_Finalize);

  Test_AddTest(suite, testCoding_WyleCondigTest);
  Test_AddTest(suite, testCoding_GolombCondigTest);
  Test_AddTest(suite, testCoding_PackBitsEncodeDecodeTest);
  Test_AddTest(suite, testCoding_MineoPackBitsEncodeDecodeTest);
}

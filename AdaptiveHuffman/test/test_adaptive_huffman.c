#include "test.h"
#include <stdlib.h>
#include <stdio.h>

/* テスト対象のモジュール */
#include "../adaptive_huffman.c"

int testAdaptiveHuffman_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testAdaptiveHuffman_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* 木の作成テスト */
void testAdaptiveHuffman_CreateHuffmanTreeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 木の作成テスト */
  {
    struct AdaptiveHuffmanTree* tree;

    tree = AdaptiveHuffmanTree_Create();

    Test_AssertCondition(tree != NULL);
    Test_AssertEqual(tree->nodes[tree->leaf[ADAPTIVE_HUFFMAN_END_OF_STREAM]].weight, 1);
    Test_AssertEqual(tree->nodes[tree->leaf[ADAPTIVE_HUFFMAN_ESCAPE]].weight, 1);
    Test_AssertEqual(tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX].weight, 2);

    AdaptiveHuffmanTree_Destroy(tree);
  }

}

/* エンコードテスト */
void testAdaptiveHuffman_EncodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 何か書いてみる */
  {
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    AdaptiveHuffmanApiResult ret;
    const char* filename = "ahuff_encodetest.bin";

    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();

    Test_AssertEqual(tree->leaf[0], ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED);
    ret = AdaptiveHuffman_EncodeSymbol(tree, stream, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    Test_AssertNotEqual(tree->leaf[0], ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED);
    Test_AssertEqual(tree->nodes[tree->leaf[0]].weight, 1);

    ret = AdaptiveHuffman_EncodeSymbol(tree, stream, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    Test_AssertNotEqual(tree->leaf[0], ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED);
    Test_AssertEqual(tree->nodes[tree->leaf[0]].weight, 2);

    Test_AssertEqual(tree->leaf[1], ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED);
    ret = AdaptiveHuffman_EncodeSymbol(tree, stream, 1);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    Test_AssertNotEqual(tree->leaf[1], ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED);
    Test_AssertEqual(tree->nodes[tree->leaf[1]].weight, 1);

    ret = AdaptiveHuffman_EncodeSymbol(tree, stream, 1);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    Test_AssertNotEqual(tree->leaf[1], ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED);
    Test_AssertEqual(tree->nodes[tree->leaf[1]].weight, 2);

    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }

  /* 失敗テスト */
  {
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    AdaptiveHuffmanApiResult ret;
    const char* filename = "ahuff_encodefailtest.bin";

    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();

    ret = AdaptiveHuffman_EncodeSymbol(NULL, stream, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_NG);
    ret = AdaptiveHuffman_EncodeSymbol(tree, NULL, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_NG);
    ret = AdaptiveHuffman_EncodeSymbol(tree, stream, ADAPTIVE_HUFFMAN_NUM_SYMBOLS);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_NG);

    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }

  /* 登場するシンボルを万遍なく書いてみる */
  {
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    const char* filename = "ahuff_outallsymboltest.bin";
    uint32_t symbol, is_ok;

    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();

    for (symbol = 0; symbol < ADAPTIVE_HUFFMAN_NUM_SYMBOLS; symbol++) {
      AdaptiveHuffman_EncodeSymbol(tree, stream, symbol);
    }

    /* 全てのシンボルの重みは1になるか？ */
    is_ok = 1;
    for (symbol = 0; symbol < ADAPTIVE_HUFFMAN_NUM_SYMBOLS; symbol++) {
      if (tree->nodes[tree->leaf[symbol]].weight != 1) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }

  /* 木の再構築処理が起こるまで書いてみる */
  {
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    const char* filename = "ahuff_largeweighttest.bin";
    uint32_t count;
    const uint32_t large_count = ADAPTIVE_HUFFMAN_MAX_WEIGHT + 10;

    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();

    for (count = 0; count < large_count; count++) {
      AdaptiveHuffman_EncodeSymbol(tree, stream, 0);
    }
    /* シンボル0の重みが更新されているか？ */
    /* +1しているのは、RebuildTree時に+1して右シフトされるため */
    Test_AssertEqual(
        tree->nodes[tree->leaf[0]].weight, 
        (ADAPTIVE_HUFFMAN_MAX_WEIGHT >> 1) + 10 + 1);

    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }
}

/* デコードテスト */
void testAdaptiveHuffman_DecodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗テスト */
  {
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    AdaptiveHuffmanApiResult ret;
    const char* filename = "ahuff_decodefailtest.bin";

    stream = BitStream_Open(filename, "rb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();

    ret = AdaptiveHuffman_DecodeSymbol(NULL, stream, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_NG);
    ret = AdaptiveHuffman_DecodeSymbol(tree, NULL, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_NG);
    ret = AdaptiveHuffman_DecodeSymbol(tree, stream, NULL);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_NG);

    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }

}

/* エンコードデコードテスト */
void testAdaptiveHuffman_EncodeDecodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 復号して元に戻るか？ */
  /* 簡単な例 */
  {
    uint32_t symbol_buf;
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    AdaptiveHuffmanApiResult ret;
    const char* filename = "ahuff_encodedecodetest.bin";

    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    AdaptiveHuffman_EncodeSymbol(tree, stream, 0);
    AdaptiveHuffman_EncodeSymbol(tree, stream, 0);
    AdaptiveHuffman_EncodeSymbol(tree, stream, 1);
    AdaptiveHuffman_EncodeSymbol(tree, stream, 1);
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);

    stream = BitStream_Open(filename, "rb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    ret = AdaptiveHuffman_DecodeSymbol(tree, stream, &symbol_buf);
    Test_AssertEqual(symbol_buf, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    ret = AdaptiveHuffman_DecodeSymbol(tree, stream, &symbol_buf);
    Test_AssertEqual(symbol_buf, 0);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    ret = AdaptiveHuffman_DecodeSymbol(tree, stream, &symbol_buf);
    Test_AssertEqual(symbol_buf, 1);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    ret = AdaptiveHuffman_DecodeSymbol(tree, stream, &symbol_buf);
    Test_AssertEqual(symbol_buf, 1);
    Test_AssertEqual(ret, ADAPTIVE_HUFFMAN_APIRESULT_OK);
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }

  /* 偏った例 */
  {
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    const char* filename = "ahuff_largeencodedecode.bin";
    uint32_t count, symbol, is_ok;
    const uint32_t large_count = ADAPTIVE_HUFFMAN_MAX_WEIGHT + 10;

    /* 木の再構築が起こるまで書く */
    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    for (count = 0; count < large_count; count++) {
      AdaptiveHuffman_EncodeSymbol(tree, stream, 2);
    }
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);

    stream = BitStream_Open(filename, "rb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    is_ok = 1;
    for (count = 0; count < large_count; count++) {
      AdaptiveHuffman_DecodeSymbol(tree, stream, &symbol);
      if (symbol != 2) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }

  /* シンボルを万遍なく */
  {
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    const char* filename = "ahuff_uniformencodedecode.bin";
    uint32_t count, symbol, is_ok;

    /* 万遍なく1回ずつ書き込む */
    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    for (symbol = 0; symbol < ADAPTIVE_HUFFMAN_NUM_SYMBOLS; symbol++) {
      AdaptiveHuffman_EncodeSymbol(tree, stream, symbol);
    }
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);

    stream = BitStream_Open(filename, "rb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    is_ok = 1;
    for (count = 0; count < ADAPTIVE_HUFFMAN_NUM_SYMBOLS; count++) {
      AdaptiveHuffman_DecodeSymbol(tree, stream, &symbol);
      if (symbol != count) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
  }

  /* ランダム */
  {
#define NUM_RANDOM_OUTPUT 1024
    struct BitStream* stream;
    struct AdaptiveHuffmanTree* tree;
    const char* filename = "ahuff_randencodedecode.bin";
    uint32_t count, symbol, is_ok;
    uint32_t answer[NUM_RANDOM_OUTPUT];

    stream = BitStream_Open(filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    srand(0);
    for (count = 0; count < ADAPTIVE_HUFFMAN_NUM_SYMBOLS; count++) {
      symbol = rand() % ADAPTIVE_HUFFMAN_NUM_SYMBOLS;
      answer[count] = symbol;
      AdaptiveHuffman_EncodeSymbol(tree, stream, symbol);
    }
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);

    stream = BitStream_Open(filename, "rb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    is_ok = 1;
    for (count = 0; count < ADAPTIVE_HUFFMAN_NUM_SYMBOLS; count++) {
      AdaptiveHuffman_DecodeSymbol(tree, stream, &symbol);
      if (symbol != answer[count]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(stream);
#undef NUM_RANDOM_OUTPUT
  }

  /* 実データ */
  {
    struct BitStream* in_stream;
    struct BitStream* out_stream;
    struct BitStream* test_stream;
    struct AdaptiveHuffmanTree* tree;
    const char* filename     = "32.jpg";
    const char* out_filename = "32_ahuff.bin";
    const char* dec_filename = "32_ahuff_dec.jpg";
    uint64_t bitsbuf;
    uint32_t symbol, is_ok;
    int32_t nbytes;

    /* エンコード */
    in_stream = BitStream_Open(filename, "rb", NULL, 0);
    out_stream = BitStream_Open(out_filename, "wb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    nbytes = 0;
    while (BitStream_GetBits(in_stream, 8, &bitsbuf) == 0) {
      AdaptiveHuffman_EncodeSymbol(tree, out_stream, (uint32_t)bitsbuf);
      nbytes++;
    }
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(in_stream); BitStream_Close(out_stream);

    /* デコード */
    in_stream   = BitStream_Open(out_filename, "rb", NULL, 0);
    out_stream  = BitStream_Open(dec_filename, "wb", NULL, 0);
    test_stream = BitStream_Open(filename, "rb", NULL, 0);
    tree = AdaptiveHuffmanTree_Create();
    /* 一致確認しつつ */
    is_ok = 1;
    while (--nbytes > 0) {
      AdaptiveHuffman_DecodeSymbol(tree, in_stream, &symbol);
      BitStream_PutBits(out_stream, 8, symbol);
      BitStream_GetBits(test_stream, 8, &bitsbuf);
      if (symbol != (uint32_t)bitsbuf) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    AdaptiveHuffmanTree_Destroy(tree);
    BitStream_Close(in_stream); BitStream_Close(out_stream);
  }
}

void testAdaptiveHuffman_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("AdaptiveHuffman Test Suite",
        NULL, testAdaptiveHuffman_Initialize, testAdaptiveHuffman_Finalize);

  Test_AddTest(suite, testAdaptiveHuffman_CreateHuffmanTreeTest);
  Test_AddTest(suite, testAdaptiveHuffman_EncodeTest);
  Test_AddTest(suite, testAdaptiveHuffman_DecodeTest);
  Test_AddTest(suite, testAdaptiveHuffman_EncodeDecodeTest);
}

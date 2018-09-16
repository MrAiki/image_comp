#include "test.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

/* テスト対象のモジュール */
#include "../horc.c"

int testHORC_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testHORC_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ブラックボックステスト */
void testHORC_BlackBoxTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 生成破棄 */
  {
    struct HORC* horc;
    horc = HORC_Create(3, 7);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(2, 7);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(1, 7);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(0, 7);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(3, 8);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(2, 8);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(1, 8);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(0, 8);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(3, 9);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(2, 9);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(1, 9);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(0, 9);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(3, 10);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(2, 10);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(1, 10);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
    horc = HORC_Create(0, 10);
    Test_AssertCondition(horc != NULL);
    HORC_Destroy(horc);
  }

  /* 簡単にエンコード */
  {
    struct HORC* horc;
    struct BitStream* strm;

    const uint32_t encode_data[] = { 0, 1, 0, 0, 1 };
    uint32_t       decode_data[sizeof(encode_data) / sizeof(encode_data[0])];
    const uint32_t num_data = sizeof(encode_data) / sizeof(encode_data[0]);

    strm = BitStream_Open("enctest1.bin", "wb", NULL, 0);
    horc = HORC_Create(2, 8);
    HORC_Encode(horc, strm, encode_data, num_data);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    strm = BitStream_Open("enctest1.bin", "rb", NULL, 0);
    horc = HORC_Create(2, 8);
    HORC_Decode(horc, strm, decode_data, num_data);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    Test_AssertEqual(memcmp(encode_data, decode_data, sizeof(encode_data)), 0);
  }

  /* 7bit */
  {
    struct HORC* horc;
    struct BitStream* strm;
    const uint32_t encode_data[] = { 0, 62, 0, 0, 0, 0, 1, 2, 127, 9, };
    uint32_t       decode_data[sizeof(encode_data) / sizeof(encode_data[0])];
    const uint32_t num_data = sizeof(encode_data) / sizeof(encode_data[0]);

    strm = BitStream_Open("enctest2.bin", "wb", NULL, 0);
    horc = HORC_Create(2, 7);
    HORC_Encode(horc, strm, encode_data, num_data);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    strm = BitStream_Open("enctest2.bin", "rb", NULL, 0);
    horc = HORC_Create(2, 7);
    HORC_Decode(horc, strm, decode_data, num_data);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    Test_AssertEqual(memcmp(encode_data, decode_data, sizeof(encode_data)), 0);
  }

  /* 9bit */
  {
    struct HORC* horc;
    struct BitStream* strm;
    const uint32_t encode_data[] = { 0, 0, 0, 511, 0, 0, 99, 99, 99, 100, 1, 2, 0, 511, 255, 0, 0 };
    uint32_t       decode_data[sizeof(encode_data) / sizeof(encode_data[0])];
    const uint32_t num_data = sizeof(encode_data) / sizeof(encode_data[0]);

    strm = BitStream_Open("enctest3.bin", "wb", NULL, 0);
    horc = HORC_Create(3, 9);
    HORC_Encode(horc, strm, encode_data, num_data);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    strm = BitStream_Open("enctest3.bin", "rb", NULL, 0);
    horc = HORC_Create(3, 9);
    HORC_Decode(horc, strm, decode_data, num_data);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    Test_AssertEqual(memcmp(encode_data, decode_data, sizeof(encode_data)), 0);
  }

  /* 実バイナリデータ */
  {
    struct HORC* horc;
    struct BitStream* strm;
    uint32_t *encode_data, *decode_data;
    uint32_t fsize, i;
    struct stat fstat;
    FILE* fp;
    const char* filename = "syokuji_ramen_abura_structure.pgm";

    stat(filename, &fstat);
    fsize = fstat.st_size;
    fp = fopen(filename, "rb");

    encode_data = malloc(sizeof(uint32_t) * fsize);
    decode_data = malloc(sizeof(uint32_t) * fsize);
    for (i = 0; i < fsize; i++) {
      encode_data[i] = getc(fp);
    }
    fclose(fp);

    strm = BitStream_Open("enctest4.bin", "wb", NULL, 0);
    horc = HORC_Create(1, 8);
    HORC_Encode(horc, strm, encode_data, fsize);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    strm = BitStream_Open("enctest4.bin", "rb", NULL, 0);
    horc = HORC_Create(1, 8);
    HORC_Decode(horc, strm, decode_data, fsize);
    BitStream_Close(strm);
    HORC_Destroy(horc);

    Test_AssertEqual(memcmp(encode_data, decode_data, sizeof(uint32_t) * fsize), 0);

    free(encode_data); free(decode_data);
  }
}

void testHORC_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("HORC Test Suite",
        NULL, testHORC_Initialize, testHORC_Finalize);

  Test_AddTest(suite, testHORC_BlackBoxTest);
}

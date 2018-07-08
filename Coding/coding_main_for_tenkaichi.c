#include "coding.h"
#include "../../BitStream/bit_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_DIFF_ARRAY 512
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

/* アクセサ */
#define SGMPicture_GRAY(sgm, x, y) ((sgm)->gray[(x) + (y) * ((sgm)->width)])

/* 16bitでバイトオーダ入れ替え */
#define SWAP_BYTE_ORDER_16BIT(val) (uint16_t)((((val) >> 8) & 0xFF) | (((val) & 0xFF) << 8))

/* SGMファイル */
struct SGMPicture {
  uint16_t  width;    /* 幅 */
  uint16_t  height;   /* 高さ */
  uint8_t*  gray;     /* グレースケール画素配列 */
};

/* SGMインスタンス作成 */
static struct SGMPicture* SGMPicture_Create(uint16_t width, uint16_t height)
{
  struct SGMPicture* sgm;

  sgm = (struct SGMPicture*)malloc(sizeof(struct SGMPicture));
  sgm->width  = width;
  sgm->height = height;
  sgm->gray = (uint8_t *)malloc(sizeof(uint8_t) * width * height);

  return sgm;
}

/* SGMファイル読み出し */
static struct SGMPicture* SGMPicture_CreateFromFile(const char* filename)
{
  struct BitStream* strm;
  struct SGMPicture* sgm;
  uint64_t bitsbuf;
  uint32_t i;

  if ((strm = BitStream_Open(filename, "rb", NULL, 0)) == NULL) {
    return NULL;
  }

  /* 領域確保 */
  sgm = (struct SGMPicture*)malloc(sizeof(struct SGMPicture));
  BitStream_GetBits(strm, 16, &bitsbuf);
  /* バイトオーダが違う... */
  sgm->width  = SWAP_BYTE_ORDER_16BIT(bitsbuf);
  BitStream_GetBits(strm, 16, &bitsbuf);
  sgm->height = SWAP_BYTE_ORDER_16BIT(bitsbuf);

  sgm->gray = (uint8_t *)malloc(sizeof(uint8_t) * sgm->width * sgm->height);

  // printf("width:%d height:%d \n", sgm->width, sgm->height);

  for (i = 0; i < sgm->width * sgm->height; i++) {
    BitStream_GetBits(strm, 8, &bitsbuf);
    SGMPicture_GRAY(sgm, i % sgm->width, i / sgm->width) = (uint8_t)bitsbuf;
  }

  BitStream_Close(strm);

  return sgm;
}

/* SGMインスタンス破棄 */
static void SGMPicture_Destroy(struct SGMPicture* sgm)
{
  if (sgm != NULL) {
    if (sgm->gray != NULL) {
      free(sgm->gray);
    }
    free(sgm);
  }
}

/* SGMファイル書き出し */
static void SGMPicture_WriteToFile(const char* filename, const struct SGMPicture* sgm)
{
  uint32_t i;
  struct BitStream* strm;

  if ((strm = BitStream_Open(filename, "wb", NULL, 0)) == NULL) {
    return;
  }

  BitStream_PutBits(strm, 16, SWAP_BYTE_ORDER_16BIT(sgm->width));
  BitStream_PutBits(strm, 16, SWAP_BYTE_ORDER_16BIT(sgm->height));
  for (i = 0; i < sgm->width * sgm->height; i++) {
    BitStream_PutBits(strm, 8, sgm->gray[i]);
  }

  BitStream_Close(strm);
}

/* 範囲外アクセス対応版アクセサ */
static int32_t SGMPicture_GetGRAY(
    const struct SGMPicture* sgm, int32_t x, int32_t y)
{
  if (x < 0) {
    x = 0;
  } else if (x >= (int32_t)sgm->width-1) {
    x = sgm->width-1;
  }

  if (y < 0) {
    y = 0;
  } else if (y >= (int32_t)sgm->height-1) {
    y = sgm->height-1;
  }

  return (int32_t)SGMPicture_GRAY(sgm, x, y);
}

static int32_t predict_jpeg2(uint32_t a, uint32_t b, uint32_t c)
{
  if (b >= MAX(a,c)) {
    return MIN(a,c);
  } else if (b <= MIN(a,c)) {
    return MAX(a,c);
  } 
  
  return a + c - b;
}

/* 符号付きシンボル配列を符号なしに変換 */
static void convert_signed_to_unsigned(uint32_t* dst, const int32_t* src, uint32_t num)
{
  uint32_t i;

  for (i = 0; i < num; i++) {
    /* 偶数は負、奇数は正とする */
    if (src[i] <= 0) {
      dst[i] = -2 * src[i];
    } else {
      dst[i] = 2 * src[i] - 1;
    }
  }
}

/* 符号なしシンボル配列を符号付きに変換 */
static void convert_unsigned_to_signed(int32_t* dst, const uint32_t* src, uint32_t num)
{
  uint32_t i;

  for (i = 0; i < num; i++) {
    if (src[i] & 1) {
      dst[i] = src[i] / 2 + 1;
    } else {
      dst[i] = - (int32_t)src[i] / 2;
    }
  }

}

/* エンコード */
static int encode(const char* in_filename,
    uint32_t golomb_m, uint32_t length_bits, uint32_t threshould)
{
  uint32_t x, y;
  struct BitStream* strm;
  struct SGMPicture* sgm;
  uint32_t num_data, i_data;
  int32_t  *diff;
  uint32_t *output_symbol;

  sgm           = SGMPicture_CreateFromFile(in_filename);
  num_data      = sgm->width * sgm->height - 1;     /* 先頭1画素分を除くので-1 */
  diff          = (int32_t *)malloc(sizeof(int32_t) * num_data);
  output_symbol = (uint32_t *)malloc(sizeof(uint32_t) * num_data);

  /* 横縦をDPCM */
  i_data = 0;
  for (x = 1; x < sgm->width; x++) {
    diff[i_data++] = SGMPicture_GetGRAY(sgm, x-1, 0) - SGMPicture_GetGRAY(sgm, x, 0);
  }
  for (y = 1; y < sgm->height; y++) {
    diff[i_data++] = SGMPicture_GetGRAY(sgm, 0, y-1) - SGMPicture_GetGRAY(sgm, 0, y);
  }

  /* あとはJPEG予測 */
  for (y = 1; y < sgm->height; y++) {
    for (x = 1; x < sgm->width; x++) {
      int32_t a = SGMPicture_GetGRAY(sgm, x-1, y);
      int32_t b = SGMPicture_GetGRAY(sgm, x-1, y-1);
      int32_t c = SGMPicture_GetGRAY(sgm,   x, y-1);
      diff[i_data++] = predict_jpeg2(a,b,c) - SGMPicture_GetGRAY(sgm, x, y);
    }
  }
  SGMPicture_Destroy(sgm);

  /* 符号出力 */
  /* ヘッダ部 */
  strm = BitStream_Open("output.bin", "wb", NULL, 0);
  BitStream_PutBits(strm, 8, 'A');
  BitStream_PutBits(strm, 8, 'I');
  BitStream_PutBits(strm, 8, 'K');
  BitStream_PutBits(strm, 8, 'A');
  BitStream_PutBits(strm, 8, 'T');
  BitStream_PutBits(strm, 8, 'S');
  BitStream_PutBits(strm, 8, 'U');
  BitStream_PutBits(strm, 8, '!');
  BitStream_PutBits(strm, 16, sgm->width);
  BitStream_PutBits(strm, 16, sgm->height);
  /* 先頭はそのまま8バイト出力 */
  BitStream_PutBits(strm, 8, SGMPicture_GRAY(sgm, 0, 0));
  /* 差分信号 */
  convert_signed_to_unsigned(output_symbol, diff, num_data);
  PackBits_Encode(strm, output_symbol, num_data, golomb_m, threshould, length_bits);
  BitStream_Close(strm);

  free(diff);
  free(output_symbol);

  return 0;
}

/* デコード */
static int decode(const char* in_filename,
    uint32_t golomb_m, uint32_t length_bits)
{
  uint32_t x, y;
  struct BitStream* strm;
  struct SGMPicture* sgm;
  uint32_t num_data, i_data;
  uint32_t width, height;
  uint64_t bitsbuf;
  int32_t  *diff;
  uint32_t *output_symbol;

  /* 復号 */
  strm = BitStream_Open(in_filename, "rb", NULL, 0);
  /* シグネチャ */
  BitStream_GetBits(strm, 64, &bitsbuf);
  if (   (((bitsbuf >> 56) & 0xFF) != 'A')
      || (((bitsbuf >> 48) & 0xFF) != 'I')
      || (((bitsbuf >> 40) & 0xFF) != 'K')
      || (((bitsbuf >> 32) & 0xFF) != 'A')
      || (((bitsbuf >> 24) & 0xFF) != 'T')
      || (((bitsbuf >> 16) & 0xFF) != 'S')
      || (((bitsbuf >>  8) & 0xFF) != 'U')
      || (((bitsbuf >>  0) & 0xFF) != '!')) {
    fprintf(stderr, "Failed to check signature: %8llx \n", bitsbuf);
    return 1;
  }

  /* 幅 */
  BitStream_GetBits(strm, 16, &bitsbuf);
  width = (uint16_t)bitsbuf;
  /* 高さ */
  BitStream_GetBits(strm, 16, &bitsbuf);
  height = (uint16_t)bitsbuf;

  num_data      = width * height - 1;     /* 先頭1画素分を除くので-1 */
  diff          = (int32_t *)malloc(sizeof(int32_t) * num_data);
  output_symbol = (uint32_t *)malloc(sizeof(uint32_t) * num_data);

  // printf("%d %d \n", width, height);

  /* 復元用画像生成 */
  sgm = SGMPicture_Create(width, height);

  /* 先頭画素 */
  BitStream_GetBits(strm, 8, &bitsbuf);
  SGMPicture_GRAY(sgm, 0, 0) = (uint8_t)bitsbuf;

  /* それ以外の画素はPackBitsにより符号化されている */
  PackBits_Decode(strm, output_symbol, num_data, golomb_m, length_bits);
  BitStream_Close(strm);
  convert_unsigned_to_signed(diff, output_symbol, num_data);

  /* 画素復元 */
  i_data = 0;

  /* 縦横DPCM分 */
  for (x = 1; x < sgm->width; x++) {
    SGMPicture_GRAY(sgm, x, 0)
      = SGMPicture_GetGRAY(sgm, x-1, 0) - diff[i_data++];
  }
  for (y = 1; y < sgm->height; y++) {
    SGMPicture_GRAY(sgm, 0, y)
      = SGMPicture_GetGRAY(sgm, 0, y-1) - diff[i_data++];
  }

  /* JPEG予想分 */
  for (y = 1; y < sgm->height; y++) {
    for (x = 1; x < sgm->width; x++) {
      int32_t a = SGMPicture_GetGRAY(sgm, x-1, y);
      int32_t b = SGMPicture_GetGRAY(sgm, x-1, y-1);
      int32_t c = SGMPicture_GetGRAY(sgm, x,   y-1);
      SGMPicture_GRAY(sgm, x, y)
        = predict_jpeg2(a,b,c) - diff[i_data++];
    }
  }

  SGMPicture_WriteToFile("decode.sgm", sgm);
  SGMPicture_Destroy(sgm);

  free(diff);
  free(output_symbol);

  return 0;
}

int main(int argc, char** argv)
{
  const uint32_t golomb_m     = 3;
  const uint32_t threshould   = 5;
  const uint32_t length_bits  = 9;

  if (argc != 3) {
    fprintf(stderr, "Usage: prog -[cd] file \n");
    return 1;
  }

  if (strcmp(argv[1], "-c") == 0) {
    encode(argv[2], golomb_m, length_bits, threshould);
  } else if (strcmp(argv[1], "-d") == 0) {
    decode(argv[2], golomb_m, length_bits);
  } else {
    return 1;
  }

  return 0;
}

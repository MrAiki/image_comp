#include "coding.h"
#include "../pnm/pnm.h"

#include <stdlib.h>
#include <string.h>

#define NUM_DIFF_ARRAY 512
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

/* 範囲外アクセス対応版アクセサ */
static float PNMPict_GetGRAY(const struct PNMPicture* pic, int32_t x, int32_t y)
{
  if (x < 0) {
    x = 0;
  } else if (x >= (int32_t)pic->header.width) {
    x = pic->header.width-1;
  }

  if (y < 0) {
    y = 0;
  } else if (y >= (int32_t)pic->header.height) {
    y = pic->header.height-1;
  }

  return (float)PNMPict_GRAY(pic, x, y);
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

int main(int argc, char** argv)
{
  uint32_t x, y;
  const uint32_t golomb_m     = 3;
  const uint32_t threshould   = 5;
  const uint32_t length_bits  = 9;
  struct PNMPicture* pic;
  struct PNMPicture* pic_test;
  struct BitStream* strm;
  uint32_t num_data, i_data;
  int32_t  *diff, *diff_test;
  uint32_t *output_symbol;

  uint64_t bitsbuf;
  uint32_t width, height;

  if (argc != 3) {
    fprintf(stderr, "Usage: prog input.pgm output.bin \n");
    return 1;
  }

  pic = PNM_CreatePictureFromFile(argv[1]);
  num_data      = pic->header.width * pic->header.height - 1;     /* 先頭1画素分を除くので-1 */
  diff          = (int32_t *)malloc(sizeof(int32_t) * num_data);
  diff_test     = (int32_t *)malloc(sizeof(int32_t) * num_data);
  output_symbol = (uint32_t *)malloc(sizeof(uint32_t) * num_data);

  /* ヘッダ部 */
  strm = BitStream_Open(argv[2], "wb", NULL, 0);
  BitStream_PutBits(strm, 8, 'A');
  BitStream_PutBits(strm, 8, 'I');
  BitStream_PutBits(strm, 8, 'K');
  BitStream_PutBits(strm, 8, 'A');
  BitStream_PutBits(strm, 8, 'T');
  BitStream_PutBits(strm, 8, 'S');
  BitStream_PutBits(strm, 8, 'U');
  BitStream_PutBits(strm, 8, '!');
  BitStream_PutBits(strm, 16, pic->header.width);
  BitStream_PutBits(strm, 16, pic->header.height);

  /* 先頭はそのまま8バイト出力 */
  BitStream_PutBits(strm, 8, PNMPict_GRAY(pic, 0, 0));

  /* 横縦をDPCM */
  i_data = 0;
  for (x = 1; x < pic->header.width; x++) {
    diff[i_data++] = PNMPict_GetGRAY(pic, x-1, 0) - PNMPict_GetGRAY(pic, x, 0);
  }
  for (y = 1; y < pic->header.height; y++) {
    diff[i_data++] = PNMPict_GetGRAY(pic, 0, y-1) - PNMPict_GetGRAY(pic, 0, y);
  }

  /* あとはJPEG予測 */
  for (y = 1; y < pic->header.height; y++) {
    for (x = 1; x < pic->header.width; x++) {
      uint32_t a = PNMPict_GetGRAY(pic, x-1, y);
      uint32_t b = PNMPict_GetGRAY(pic, x-1, y-1);
      uint32_t c = PNMPict_GetGRAY(pic, x, y-1);
      diff[i_data++] = predict_jpeg2(a,b,c) - PNMPict_GetGRAY(pic, x, y);
    }
  }
  PNM_DestroyPicture(pic);

  /* 符号出力 */
  convert_signed_to_unsigned(output_symbol, diff, num_data);
  PackBits_Encode(strm, output_symbol, num_data, golomb_m, threshould, length_bits);
  BitStream_Close(strm);

  /* 復号 */
  strm = BitStream_Open(argv[2], "rb", NULL, 0);

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
  width = (uint32_t)bitsbuf;
  /* 高さ */
  BitStream_GetBits(strm, 16, &bitsbuf);
  height = (uint32_t)bitsbuf;

  /* 復元用画像生成 */
  pic_test = PNM_CreatePicture(width, height);
  pic_test->header.format = PNM_FORMAT_P5;
  pic_test->header.max_brightness = 255;

  /* 先頭画素 */
  BitStream_GetBits(strm, 8, &bitsbuf);
  PNMPict_GRAY(pic_test, 0, 0) = (uint8_t)bitsbuf;

  /* それ以外の画素はPackBitsにより符号化されている */
  PackBits_Decode(strm, output_symbol, num_data, golomb_m, length_bits);
  BitStream_Close(strm);
  convert_unsigned_to_signed(diff_test, output_symbol, num_data);

  /* 画素復元 */
  i_data = 0;

  /* 縦横DPCM分 */
  for (x = 1; x < pic->header.width; x++) {
    PNMPict_GRAY(pic_test, x, 0)
      = PNMPict_GRAY(pic_test, x-1, 0) - diff_test[i_data++];
  }
  for (y = 1; y < pic->header.height; y++) {
    PNMPict_GRAY(pic_test, 0, y)
      = PNMPict_GRAY(pic_test, 0, y-1) - diff_test[i_data++];
  }

  /* JPEG予想分 */
  for (y = 1; y < pic_test->header.height; y++) {
    for (x = 1; x < pic_test->header.width; x++) {
      uint32_t a = PNMPict_GetGRAY(pic, x-1, y);
      uint32_t b = PNMPict_GetGRAY(pic, x-1, y-1);
      uint32_t c = PNMPict_GetGRAY(pic, x, y-1);
      PNMPict_GRAY(pic_test, x, y)
        = predict_jpeg2(a,b,c) - diff_test[i_data++];
    }
  }

  PNM_WritePictureToFile("test.pgm", pic_test);
  PNM_DestroyPicture(pic_test);

  free(diff);
  free(diff_test);
  free(output_symbol);

  return 0;
}

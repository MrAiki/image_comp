#include "sgm.h"
#include "../BitStream/bit_stream.h"
#include <stdlib.h>

/* 16bitでバイトオーダ入れ替え */
#define SWAP_BYTE_ORDER_16BIT(val) (uint16_t)((((val) >> 8) & 0xFF) | (((val) & 0xFF) << 8))

/* SGMインスタンス作成 */
struct SGMPicture* SGMPicture_Create(uint16_t width, uint16_t height)
{
  struct SGMPicture* sgm;

  sgm = (struct SGMPicture*)malloc(sizeof(struct SGMPicture));
  sgm->width  = width;
  sgm->height = height;
  sgm->gray = (uint8_t *)malloc(sizeof(uint8_t) * width * height);

  return sgm;
}

/* SGMファイル読み出し */
struct SGMPicture* SGMPicture_CreateFromFile(const char* filename)
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

  for (i = 0; i < sgm->width * sgm->height; i++) {
    BitStream_GetBits(strm, 8, &bitsbuf);
    SGMPicture_GRAY(sgm, i % sgm->width, i / sgm->width) = (uint8_t)bitsbuf;
  }

  BitStream_Close(strm);

  return sgm;
}

/* SGMインスタンス破棄 */
void SGMPicture_Destroy(struct SGMPicture* sgm)
{
  if (sgm != NULL) {
    if (sgm->gray != NULL) {
      free(sgm->gray);
    }
    free(sgm);
  }
}

/* SGMファイル書き出し */
void SGMPicture_WriteToFile(const char* filename, const struct SGMPicture* sgm)
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


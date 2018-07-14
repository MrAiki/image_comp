#include "../pnm/pnm.h"
#include "../AdaptiveHuffman/adaptive_huffman.h"
#include "../../BitStream/bit_stream.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* 連鎖の探索方向の数 */
#define NUM_SEARCH_DIRECTIONS 3

/* 連鎖の探索方向 */
static const int32_t search_direction[NUM_SEARCH_DIRECTIONS] = {
  0, -1, 1, 
};

/* 範囲外アクセス対応版アクセサ */
static int32_t PNMPict_GetGRAY(const struct PNMPicture* pic,
    int32_t x, int32_t y)
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

  return (int32_t)PNMPict_GRAY(pic, x, y);
}

/* PICに基づいてエンコード */
static void PIC_Encode(const struct PNMPicture* pic, struct BitStream* stream)
{
  int32_t x, y;
  int32_t s_x, s_y;
  int32_t prev_pixcel, curr_pixcel;
  int32_t i_dir, next_direction;
  const int32_t width   = (int32_t)pic->header.width;
  const int32_t height  = (int32_t)pic->header.height;
  struct PNMPicture* buf_pic;  /* 変化点を保存する一時的なバッファ画像 */

  /* 先頭に画像サイズを出力 */
  BitStream_PutBits(stream, 16, width);
  BitStream_PutBits(stream, 16, height);

  /* バッファを作成 */
  buf_pic = PNM_CreatePicture(width, height);
  buf_pic->header.format = PNM_FORMAT_P5;
  buf_pic->header.max_brightness = 255;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      PNMPict_GRAY(buf_pic, x, y) = 0;
    }
  }

  /* 変化点を求める */
  prev_pixcel = -1;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      curr_pixcel = PNMPict_GRAY(pic, x, y);
      if (prev_pixcel != curr_pixcel) {
        /* 変化点をマーク */
        PNMPict_GRAY(buf_pic, x, y) = 255;
      }
      prev_pixcel = curr_pixcel;
    }
  }

  /* 変化点の連鎖を求める */
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      /* 変化点を発見 */
      if (PNMPict_GRAY(buf_pic, x, y) != 0) {
        /* 変化点をまず記録 */
        BitStream_PutBits(stream, 16, x);
        BitStream_PutBits(stream, 16, y);
        s_x = x; s_y = y;
        /* 連鎖を記録 */
        while (1) {
          BitStream_PutBits(stream, 8, PNMPict_GRAY(pic, s_x, s_y));
          /* 画像の下端に達した時は、連鎖終了 */
          if (s_y >= height-1) {
            BitStream_PutBits(stream, 8, 0); 
            break;
          }
          /* 他の変化点を探す */
          next_direction = -1;
          for (i_dir = 0; i_dir < NUM_SEARCH_DIRECTIONS; i_dir++) {
            if (PNMPict_GetGRAY(buf_pic, s_x + search_direction[i_dir], s_y + 1) == 255) {
              next_direction = i_dir;
              break;
            }
          }
          /* 方向を記録（インデックス+1を保存） */
          BitStream_PutBits(stream, 8, next_direction + 1); 
          if (next_direction == -1) {
            /* 連鎖が見つからなかったので、次の連鎖を探索 */
            break;
          }
          /* 次行こうぜ */
          s_x += search_direction[next_direction];
          s_y++;
        }
      }
    }
  }

  /* 終了フラグとして画像サイズの座標を出力 */
  BitStream_PutBits(stream, 16, width);
  BitStream_PutBits(stream, 16, height);

  PNM_DestroyPicture(buf_pic);

}

/* PICに基づいてデコード */
static void PIC_Decode(struct BitStream* stream, const char* outname)
{
  uint64_t bitsbuf;
  int32_t x, y, s_x, s_y, next_direction;
  int32_t buf_pixel, pixel;
  int32_t width, height;
  struct PNMPicture* buf_pic;  /* 変化点を保存する一時的なバッファ画像 */
  struct PNMPicture* outpic;   /* 復元画像 */

  /* 先頭に画像サイズが埋められている */
  BitStream_GetBits(stream, 16, &bitsbuf);
  width   = (int32_t)bitsbuf;
  BitStream_GetBits(stream, 16, &bitsbuf);
  height  = (int32_t)bitsbuf;

  /* バッファと出力画像の領域確保 */
  outpic  = PNM_CreatePicture(width, height);
  outpic->header.format = PNM_FORMAT_P5;
  outpic->header.max_brightness = 255;
  buf_pic = PNM_CreatePicture(width, height);

  /* バッファのクリア */
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      PNMPict_GRAY(buf_pic, x, y) = 0;
    }
  }

  /* 変化点を読み込む */
  while (1) {
    /* 連鎖始点の座標読み取り */
    BitStream_GetBits(stream, 16, &bitsbuf);
    x = (int32_t)bitsbuf;
    BitStream_GetBits(stream, 16, &bitsbuf);
    y = (int32_t)bitsbuf;
    /* 終了フラグが見つかった */
    if (x == width && y == height) {
      break;
    }

    /* 連鎖データを読み込む */
    s_x = x; s_y = y;
    while (1) {
      /* 変化点データを読み込み */
      BitStream_GetBits(stream, 8, &bitsbuf);
      pixel = (int32_t)bitsbuf;
      /* 画像に保存 */
      PNMPict_GRAY(buf_pic, s_x, s_y) = 255;
      PNMPict_GRAY(outpic,  s_x, s_y) = pixel;
      /* 連鎖の方向を読み込み */
      BitStream_GetBits(stream, 8, &bitsbuf);
      next_direction = (int32_t)bitsbuf;
      /* 連鎖の終了 */
      if (next_direction == 0) {
        break;
      }
      /* 次行こうぜ */
      s_x += search_direction[next_direction-1];
      s_y++;
    }
  }

  /* 変化点から間を塗りつぶす */
  pixel = 0;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      buf_pixel = PNMPict_GRAY(buf_pic, x, y);
      if (buf_pixel > 0) {
        /* 変化点であれば、その画素値を取得 */
        pixel = PNMPict_GRAY(outpic, x, y);
      } else {
        PNMPict_GRAY(outpic, x, y) = pixel;
      }
    }
  }

  /* 書き出し */
  PNM_WritePictureToFile(outname, outpic);

  PNM_DestroyPicture(buf_pic);
  PNM_DestroyPicture(outpic);
}

int main(int argc, char** argv)
{
  struct PNMPicture* pic; 
  struct BitStream* stream;

  if (argc != 4) {
    fprintf(stderr, "Usage: prog -[cd] input output \n");
    return 1;
  }

  if (strcmp(argv[1], "-c") == 0) {
    pic = PNM_CreatePictureFromFile(argv[2]);
    stream = BitStream_Open(argv[3], "wb", NULL, 0);
    PIC_Encode(pic, stream);
    BitStream_Close(stream);
    PNM_DestroyPicture(pic);
  } else if (strcmp(argv[1], "-d") == 0) {
    stream = BitStream_Open(argv[2], "rb", NULL, 0);
    PIC_Decode(stream, argv[3]);
    BitStream_Close(stream);
  } else {
    return 1;
  }

  return 0;
}


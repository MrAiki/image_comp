#ifndef PNM_H_INCLUDED
#define PNM_H_INCLUDED

#include <stdint.h>

/* PNM画像フォーマット */
typedef enum PNMFormatTag {
  PNM_P1,       /* 2値 テキスト(.pbm) */
  PNM_P2,       /* グレースケール テキスト(.pgm) */
  PNM_P3,       /* RGBカラー テキスト(.ppm) */
  PNM_P4,       /* 2値 バイナリ(.pbm) */
  PNM_P5,       /* グレースケール バイナリ(.pgm) */
  PNM_P6,       /* RGBカラー バイナリ(.ppm) */
} PNMFormat;

/* RGBカラー */
struct PNMColor {
  uint8_t r;    /* R */
  uint8_t g;    /* G */
  uint8_t b;    /* B */
};

/* 1画素 */
typedef union PNMPixelTag {
  struct PNMColor  c;  /* RGBカラー */
  uint8_t          g;  /* グレースケール */
  uint8_t          b;  /* 2値(0 or 1) */
} PNMPixel;

/* PNMヘッダ */
struct PNMHeader {
  PNMFormat   format;         /* フォーマット */
  uint32_t    width;          /* 幅 */
  uint32_t    height;         /* 高さ */
  uint32_t    max_brightness; /* 最大輝度値 */
};

/* PNM画像型 */
struct PNMPicture {
  struct PNMHeader  header;   /* ヘッダ */
  PNMPixel**        picture;  /* 画素配列 */
};

/* 結果型 */
typedef enum PNMApiResultTag {
  PNM_APIRESULT_OK = 0,
  PNM_APIRESULT_NG,
} PNMApiResult;

/* 画素アクセサ */
#define PNMPict_BIT(pnm, x, y)  (pnm->picture[(y)][(x)].b)
#define PNMPict_GRAY(pnm, x, y) (pnm->picture[(y)][(x)].g)
#define PNMPict_R(pnm, x, y)    (pnm->picture[(y)][(x)].c.r)
#define PNMPict_G(pnm, x, y)    (pnm->picture[(y)][(x)].c.g)
#define PNMPict_B(pnm, x, y)    (pnm->picture[(y)][(x)].c.b)

/* TODO: ヘッダ読み込み */

/* 幅とサイズを指定して画像作成 */
struct PNMPicture* PNM_CreatePicture(uint32_t width, uint32_t height);

/* ファイルから画像作成 */
struct PNMPicture* PNM_CreatePictureFromFile(const char* filename);

/* 画像の領域解放 */
void PNM_DestroyPicture(struct PNMPicture* picture);

/* ファイル書き込み */
PNMApiResult PNM_WritePictureToFile(const char* filename, const struct PNMPicture* picture);

#endif /* PNM_H_INCLUDED */

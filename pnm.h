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

/* PNM画像型 */
struct PNMImage {
  PNMFormat   format;         /* フォーマット */
  uint32_t    width;          /* 幅 */
  uint32_t    height;         /* 高さ */
  uint32_t    max_brightness; /* 最大輝度値 */
  PNMPixel**  img;            /* 画素 */
};

/* 結果型 */
typedef enum PNMApiResultTag {
  PNM_APIRESULT_OK,
  PNM_APIRESULT_NG,
} PNMApiResult;

/* ファイルオープン */
struct PNMImage* PNM_ReadFile(const char* filename);

/* ファイル書き込み */
void PNM_WriteFile(const char* filename, const struct PNMImage* img);

#endif /* PNM_H_INCLUDED */

#include "pnm.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* パーサが読み込み可能な最大文字数 */
#define PNMPARSER_BUFFER_LENGTH         256
/* パーサが読み込み可能な整数の最大桁数 */
#define PNMPARSER_MAX_NUMBER_OF_DIGHTS  20

/* パーサ */
struct PNMParser {
  FILE*     fp;                               /* 読み込みファイルポインタ */
  uint8_t   buffer[PNMPARSER_BUFFER_LENGTH];  /* バッファ */
  int32_t   read_pos;                         /* バッファ読み込み位置(-1で行の先頭) */
};

/* ファイル読み込み */
static struct PNMImage* PNM_Read(FILE* fp);
/* ファイル書き込み */
static void PNM_Write(FILE* fp, const struct PNMImage* img);
/* パーサの初期化 */
static void PNMParser_Initialize(struct PNMParser* parser, FILE* fp);
/* 次の文字の読み込み */
static int32_t PNMParser_GetNextCharacter(struct PNMParser* parser);
/* 次のトークン文字列の取得 返り値は読み込んだ文字数 */
static int32_t PNMParser_GetNextString(struct PNMParser* parser, uint8_t* buffer, uint32_t buffer_size);
/* 次の整数を読み取る */
static int32_t PNMParser_GetNextInteger(struct PNMParser* parser);

/* P1フォーマットファイルの読み込み */
static PNMApiResult PNMParser_ReadP1(struct PNMParser* parser, struct PNMImage* img);
/* P2フォーマットファイルの読み込み */
static PNMApiResult PNMParser_ReadP2(struct PNMParser* parser, struct PNMImage* img);
/* P3フォーマットファイルの読み込み */
static PNMApiResult PNMParser_ReadP3(struct PNMParser* parser, struct PNMImage* img);

/* 画像の領域割り当て */
static struct PNMImage* PNM_AllocateImage(uint32_t width, uint32_t height);
/* 画像の領域解放 */
void PNM_FreeImage(struct PNMImage* image);

/* ファイルオープン */
struct PNMImage* PNM_ReadFile(const char* filename)
{
  FILE* fp = fopen(filename, "rb");

  if (fp == NULL) {
    fprintf("Failed to open %s \n", filename);
    return NULL;
  }

  return PNM_Read(fp);
}

/* ファイル書き込み */
void PNM_WriteFile(const char* filename, const struct PNMImage* img)
{
  FILE* fp = fopen(filename, "wb");

  if (fp == NULL) {
    fprintf("Failed to open %s \n", filename);
    return NULL;
  }

  return PNM_Write(fp, img);
}

/* ファイル読み込み */
static struct PNMImage* PNM_Read(FILE* fp)
{
  struct PNMParser parser;
  struct PNMImage  *pnm;
  struct PNMPixel  **img;
  uint8_t       header_token[PNMPARSER_MAX_NUMBER_OF_DIGHTS];
  int32_t       width, height, max, i_height, i_width;
  uint8_t       format_int;
  PNMApiResult  read_ret;
  PNMFormat     format;

  /* パーサ初期化 */
  PNMParser_Initialize(&parser, fp);

  /* シグネチャ読み込み */
  PNMParser_GetNextString(&parser,
      header_token, sizeof(header_token));

  /* シグネチャ検査 "P1", "P2", "P3", "P4", "P5", "P6" */
  format_int = header_token[0] - '0';
  if (header_token[0] != 'P'
      || format_int < 1 || format_int > 6
      || strlen(header_token) != 2) {
    fprintf(stderr, "Read error in format signature. \n");
    return NULL;
  }

  /* 列挙型に変換 */
  switch (format_int) {
    case 1: format = PNM_P1; break;
    case 2: format = PNM_P2; break;
    case 3: format = PNM_P3; break;
    case 4: format = PNM_P4; break;
    case 5: format = PNM_P5; break;
    case 6: format = PNM_P6; break;
  }

  /* 輝度の最大値を取得 */
  if (format != PNM_P1 && format != PNM_P4) {
    max = PNMParser_GetNextInteger(&parser, 
        header_token, sizeof(header_token));
    if (max < 0) {
      fprintf(stderr, "Invalid max brightness(%d). \n", max);
      return NULL;
    }
  }

  /* 幅と高さの取得 */
  width   = PNMParser_GetNextInteger(&parser);
  height  = PNMParser_GetNextInteger(&parser);
  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Read error in width(%d) or height(%d). \n", width, height);
    return NULL;
  }

  /* 画像領域の領域確保 */
  pnm = PNM_AllocateImage(width, height);
  if (pnm == NULL) {
    fprintf(stderr, "Failed to allocate image buffer. \n");
    return NULL;
  }
  pnm->format = format;
  pnm->max    = max;
  pnm->img    = img;

  /* タイプに合わせて画素値を取得 */
  switch (format) {
    case PNM_P1:
      read_ret = PNMParser_ReadP1(parser, img);
      break;
    case PNM_P2:
      read_ret = PNMParser_ReadP2(parser, img);
      break;
    case PNM_P3:
      read_ret = PNMParser_ReadP3(parser, img);
      break;
    case PNM_P4:
      read_ret = PNMParser_ReadP4(parser, img);
      break;
    case PNM_P5:
      read_ret = PNMParser_ReadP5(parser, img);
      break;
    case PNM_P6:
      read_ret = PNMParser_ReadP6(parser, img);
      break;
  }

  if (read_ret != PNM_APIRESULT_OK) {
    fprintf(stderr, "Failed to get image data. \n");
    return NULL;
  }

  return pnm;
}

/* 画像の領域割り当て */
static struct PNMImage* PNM_AllocateImage(uint32_t width, uint32_t height)
{
  struct PNMImage  *pnm;
  struct PNMPixel  **img;
  uint32_t i_height;

  /* PNMイメージの割り当て */
  pnm = (struct PNMImage *)malloc(sizeof(struct PNMImage));
  if (pnm == NULL) {
    return NULL;
  }
  pnm->height = height;
  pnm->width  = width;

  /* 画素領域の割り当て */
  img = (struct PNMPixel **)malloc(sizeof(PNMPixel*) * height);
  if (img == NULL) {
    return NULL;
  }
  for (i_height = 0; i_height < height; i_height++) {
    img[i_height] = (struct PNMPixel *)malloc(sizeof(PNMPixel) * width);
    if (img[i_height] == NULL) {
      return NULL;
    }
  }

  return pnm;
}

/* 画像の領域解放 */
void PNM_FreeImage(struct PNMImage* image)
{
  uint32_t i_height;

  if (image == NULL) {
    return;
  }

  for (i_height = 0; i_height < img->height; i_height++) {
    free(img->img[i_height]);
  }

  free(image->img);
  free(image);
}

/* P1フォーマットファイルの読み込み */
static PNMApiResult PNMParser_ReadP1(struct PNMParser* parser, struct PNMImage* image)
{
  int32_t   tmp;
  uint32_t  x, y;

  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      tmp = PNMParser_GetNextCharacter(parser);
      if (tmp == '0') {
        img->img[x][y].b = 0;
      } else if (tmp == '1') {
        image->img[x][y].b = 1;
      } else {
        return PNM_APIRESULT_NG;
      }
    }
  }

  return PNM_APIRESULT_OK;
}

/* P2フォーマットファイルの読み込み */
static PNMApiResult PNMParser_ReadP2(struct PNMParser* parser, struct PNMImage* image);
{
  int32_t   tmp;
  uint32_t  x, y;

  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_APIRESULT_NG; }
      image->img[x][y].g = tmp;
    }
  }

  return PNM_APIRESULT_OK;
}

/* P3フォーマットファイルの読み込み */
static PNMApiResult PNMParser_ReadP3(struct PNMParser* parser, struct PNMImage* image);
{
  int32_t   tmp;
  uint32_t  x, y;

  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_APIRESULT_NG; }
      image->img[x][y].c.r = tmp;

      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_APIRESULT_NG; }
      image->img[x][y].c.g = tmp;

      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_APIRESULT_NG; }
      image->img[x][y].c.b = tmp;
    }
  }

  return PNM_APIRESULT_OK;
}

static PNMApiResult PNMParser_ReadP4(struct PNMParser* parser, struct PNMImage* image);
{
  return PNM_APIRESULT_NG;
}

static PNMApiResult PNMParser_ReadP5(struct PNMParser* parser, struct PNMImage* image);
{
  return PNM_APIRESULT_NG;
}

static PNMApiResult PNMParser_ReadP6(struct PNMParser* parser, struct PNMImage* image);
{
  return PNM_APIRESULT_NG;
}

/* パーサの初期化 */
static void PNMParser_Initialize(struct PNMParser* parser, FILE* fp)
{
  parser->fp        = fp;
  parser->read_pos  = -1;
  fseek(parser->fp, 0, SEEK_SET);
}

/* 次の文字の読み込み */
static int32_t PNMParser_GetNextCharacter(struct PNMParser* parser)
{
  int32_t ch;

  if (parser == NULL) {
    return EOF;
  }

  /* 新しい行の読み込み */
  if (parser->read_pos == -1) {
    if (fgets(parser->buffer, MAX_LEN_SOURCE_LINE, fp) != NULL) {
      line_index = 0; /* インデックスは0に */
    } else {
      /* ファイル末尾に達した */
      return EOF;
    }
  }

  /* バッファから1文字読み込み */
  ch = parser->buffer[parser->read_pos++];

  /* 改行を見たらインデックスを-1にリセット */
  if (ch == '\n' || ch == '#') {
    parser->read_pos = -1;
  }

  return ch;
}

/* 次のトークン文字列の取得 返り値は読み込んだ文字数 */
static int32_t PNMParser_GetNextString(struct PNMParser* parser,
    uint8_t* buffer, uint32_t buffer_size)
{
  uint8_t next_ch;
  uint32_t buffer_pos;

  if (parser == NULL || buffer == NULL) {
    return -1;
  }

  /* 文字列読み取り */
  buffer_pos = 0;
  while (1) {
    next_ch = PNMParser_GetNextCharacter(parser);
    if (next_ch != EOF && !isspace(next_ch)
        && buffer_pos-1 < buffer_size) {
      buffer[buffer_pos++] = next_ch;
    }
  }

  /* 末端をナル文字で埋める */
  buffer[buffer_pos] = '\0';

  return buffer_pos;
}

/* 次の整数を読み取る */
static int32_t PNMParser_GetNextInteger(struct PNMParser* parser)
{
  int32_t ret;
  uint8_t *err;
  uint8_t token_string[PNMPARSER_MAX_NUMBER_OF_DIGHTS];

  /* 文字列の取得 */
  PNMParser_GetNextString(parser,
      token_string, PNMPARSER_MAX_NUMBER_OF_DIGHTS);

  /* 整数へ変換 */
  ret = strtol(token_string, &err, 10);

  /* 変換エラー */
  if (err != '\0') {
    return -1;
  }

  return ret;
}

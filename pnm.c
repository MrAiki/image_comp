#include "pnm.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* パーサが読み込み可能な最大文字数 */
#define PNMPARSER_BUFFER_LENGTH         256
/* パーサが読み込み可能な整数の最大桁数 */
#define PNMPARSER_MAX_NUMBER_OF_DIGHTS  20

/* 下位n_bitsを取得 */
/* 補足）((1 << n_bits) - 1)は下位の数値だけ取り出すマスクになる */
#define PNM_GetLowerBits(n_bits, val) ((val) & ((1 << n_bits) - 1))

/* 内部エラー型 */
typedef enum PNMErrorTag {
  PNM_ERROR_OK = 0,             /* OK */
  PNM_ERROR_NG,                 /* 分類不能な失敗 */
  PNM_ERROR_IO,                 /* 入出力エラー */
  PNM_ERROR_INVALID_PARAMETER,  /* 不正な引数 */
} PNMError;

/* 文字列バッファ */
struct PNMStringBuffer {
  char      string[PNMPARSER_BUFFER_LENGTH];  /* 文字列バッファ */
  int32_t   read_pos;                         /* 文字列読み込み位置 */
};

/* ビットバッファ */
struct PNMBitBuffer {
  uint8_t   bit_buffer;                       /* ビットバッファ */
  int8_t    bit_count;                        /* ビット入力カウント */
};

/* パーサ */
struct PNMParser {
  FILE*     fp;                 /* 読み込みファイルポインタ */
  /* バッファ */
  union {
    struct PNMStringBuffer s;   /* 文字列バッファ */
    struct PNMBitBuffer    b;   /* ビットバッファ */
  } u_buffer;
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
static int32_t PNMParser_GetNextString(struct PNMParser* parser, char* buffer, uint32_t buffer_size);
/* 次の整数を読み取る */
static int32_t PNMParser_GetNextInteger(struct PNMParser* parser);
/* 1ビット取得 */
/* エラー時は負値を返す */
static int32_t PNMParser_GetBit(struct PNMParser* parser);
/* n_bit 取得し、結果を右詰めする */
/* エラー時は負値を返す */
static int32_t PNMParser_GetBits(struct PNMParser* parser, uint8_t n_bits);

/* P1フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP1(struct PNMParser* parser, struct PNMImage* img);
/* P2フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP2(struct PNMParser* parser, struct PNMImage* img);
/* P3フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP3(struct PNMParser* parser, struct PNMImage* img);
/* P4フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP4(struct PNMParser* parser, struct PNMImage* image);
/* P5フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP5(struct PNMParser* parser, struct PNMImage* image);
/* P6フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP6(struct PNMParser* parser, struct PNMImage* image);

/* 画像の領域割り当て */
static struct PNMImage* PNM_AllocateImage(uint32_t width, uint32_t height);
/* 画像の領域解放 */
void PNM_FreeImage(struct PNMImage* image);

/* ファイルオープン */
struct PNMImage* PNM_ReadFile(const char* filename)
{
  FILE* fp = fopen(filename, "rb");

  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s \n", filename);
    return NULL;
  }

  return PNM_Read(fp);
}

/* ファイル書き込み */
void PNM_WriteFile(const char* filename, const struct PNMImage* img)
{
  FILE* fp = fopen(filename, "wb");

  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s \n", filename);
    return;
  }

  return PNM_Write(fp, img);
}

/* ファイル読み込み */
static struct PNMImage* PNM_Read(FILE* fp)
{
  struct PNMParser parser;
  struct PNMImage  *pnm;
  char          header_token[PNMPARSER_MAX_NUMBER_OF_DIGHTS];
  int32_t       width, height, max;
  uint8_t       format_int;
  PNMError      read_err;
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
      || header_token[2] != '\0') {
    fprintf(stderr, "Invalid PNM format signature. \n");
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
    max = PNMParser_GetNextInteger(&parser);
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
  pnm->format         = format;
  pnm->max_brightness = max;

  /* タイプに合わせて画素値を取得 */
  switch (format) {
    case PNM_P1:
      read_err = PNMParser_ReadP1(&parser, pnm);
      break;
    case PNM_P2:
      read_err = PNMParser_ReadP2(&parser, pnm);
      break;
    case PNM_P3:
      read_err = PNMParser_ReadP3(&parser, pnm);
      break;
    case PNM_P4:
      read_err = PNMParser_ReadP4(&parser, pnm);
      break;
    case PNM_P5:
      read_err = PNMParser_ReadP5(&parser, pnm);
      break;
    case PNM_P6:
      read_err = PNMParser_ReadP6(&parser, pnm);
      break;
  }

  if (read_err != PNM_ERROR_OK) {
    fprintf(stderr, "Failed to get image data. \n");
    return NULL;
  }

  return pnm;
}

/* ファイル書き込み */
static void PNM_Write(FILE* fp, const struct PNMImage* img)
{
  return;
}

/* 画像の領域割り当て */
static struct PNMImage* PNM_AllocateImage(uint32_t width, uint32_t height)
{
  struct PNMImage  *pnm;
  struct PNMPixel  **img;
  uint32_t y;

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
  for (y = 0; y < height; y++) {
    img[y] = (struct PNMPixel *)malloc(sizeof(PNMPixel) * width);
    if (img[y] == NULL) {
      return NULL;
    }
  }

  return pnm;
}

/* 画像の領域解放 */
void PNM_FreeImage(struct PNMImage* image)
{
  uint32_t y;

  if (image == NULL) {
    return;
  }

  for (y = 0; y < image->height; y++) {
    free(image->img[y]);
  }

  free(image->img);
  free(image);
}

/* P1フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP1(struct PNMParser* parser, struct PNMImage* image)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || image == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < image->height; y++) {
    for (x = 0; x < image->width; x++) {
      tmp = PNMParser_GetNextCharacter(parser);
      if (tmp == '0') {
        image->img[x][y].b = 0;
      } else if (tmp == '1') {
        image->img[x][y].b = 1;
      } else {
        return PNM_ERROR_NG;
      }
    }
  }

  return PNM_ERROR_OK;
}

/* P2フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP2(struct PNMParser* parser, struct PNMImage* image)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || image == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < image->height; y++) {
    for (x = 0; x < image->width; x++) {
      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      image->img[x][y].g = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P3フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP3(struct PNMParser* parser, struct PNMImage* image)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || image == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < image->height; y++) {
    for (x = 0; x < image->width; x++) {
      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      image->img[x][y].c.r = tmp;

      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      image->img[x][y].c.g = tmp;

      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      image->img[x][y].c.b = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P4フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP4(struct PNMParser* parser, struct PNMImage* image)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || image == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < image->height; y++) {
    for (x = 0; x < image->width; x++) {
      tmp = PNMParser_GetBit(parser);
      if (tmp < 0) { return PNM_ERROR_IO; }
      image->img[x][y].b = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P5フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP5(struct PNMParser* parser, struct PNMImage* image)
{
  int32_t   tmp;
  uint32_t  x, y;

  for (y = 0; y < image->height; y++) {
    for (x = 0; x < image->width; x++) {
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      image->img[x][y].g = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P6フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP6(struct PNMParser* parser, struct PNMImage* image)
{
  int32_t   tmp;
  uint32_t  x, y;

  for (y = 0; y < image->height; y++) {
    for (x = 0; x < image->width; x++) {
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      image->img[x][y].c.r = tmp;
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      image->img[x][y].c.g = tmp;
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      image->img[x][y].c.b = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* パーサの初期化 */
static void PNMParser_Initialize(struct PNMParser* parser, FILE* fp)
{
  parser->fp                  = fp;
  parser->u_buffer.s.read_pos = -1;
}

/* 次の文字の読み込み */
static int32_t PNMParser_GetNextCharacter(struct PNMParser* parser)
{
  int32_t ch;
  struct PNMStringBuffer *buf = &(parser->u_buffer.s);

  /* 新しい行の読み込み */
  if (buf->read_pos == -1) {
    if (fgets(buf->string, PNMPARSER_BUFFER_LENGTH, parser->fp) != NULL) {
      /* 読み込み位置は0に */
      buf->read_pos = 0; 
    } else {
      /* ファイル末尾に達した */
      return EOF;
    }
  }

  /* バッファから1文字読み込み */
  ch = buf->string[buf->read_pos++];

  /* コメント */
  if (ch == '#') {
    /* 改行が表れるまで読み飛ばし */
    do {
      ch = PNMParser_GetNextCharacter(parser);
    } while (ch != '\n');
  }

  /* 改行を見たらインデックスを-1にリセット */
  if (ch == '\n') {
    buf->read_pos = -1;
  }

  return ch;
}

/* 次のトークン文字列の取得 返り値は読み込んだ文字数 */
static int32_t PNMParser_GetNextString(struct PNMParser* parser,
    char* buffer, uint32_t buffer_size)
{
  int32_t next_ch;
  uint32_t buffer_pos;

  if (parser == NULL || buffer == NULL) {
    return -1;
  }

  /* 文字列読み取り */
  buffer_pos = 0;
  next_ch = PNMParser_GetNextCharacter(parser);
  while (next_ch != EOF && !isspace(next_ch)
        && (buffer_pos-1 < buffer_size)) {
      buffer[buffer_pos++] = next_ch;
      next_ch = PNMParser_GetNextCharacter(parser);
  }

  /* 末端をナル文字で埋める */
  buffer[buffer_pos] = '\0';

  return buffer_pos;
}

/* 次の整数を読み取る */
static int32_t PNMParser_GetNextInteger(struct PNMParser* parser)
{
  int32_t ret;
  char    *err;
  char    token_string[PNMPARSER_MAX_NUMBER_OF_DIGHTS];

  /* 文字列の取得 */
  PNMParser_GetNextString(parser,
      token_string, PNMPARSER_MAX_NUMBER_OF_DIGHTS);

  /* 10進整数へ変換 */
  ret = strtol(token_string, &err, 10);

  /* 変換エラー */
  if (err != '\0') {
    return -1;
  }

  return ret;
}

/* 1ビット取得 */
/* エラー時は負値を返す */
static int32_t PNMParser_GetBit(struct PNMParser* parser)
{
  int32_t ch;
  struct PNMBitBuffer *buf = &(parser->u_buffer.b);

  /* 入力ビットカウントを1減らし、バッファの対象ビットを出力 */
  buf->bit_count--;
  if (buf->bit_count >= 0) {
    return (buf->bit_buffer >> buf->bit_count) & 1;
  }

  /* カウンタとバッファの更新 */
  if ((ch = getc(parser->fp)) == EOF) {
    return -1;
  }
  buf->bit_count   = 7;
  buf->bit_buffer  = ch;

  /* 取得したバッファの最上位ビットを出力 */
  return (buf->bit_buffer >> 7) & 1;
}

/* n_bit 取得し、結果を右詰めする */
/* エラー時は負値を返す */
static int32_t PNMParser_GetBits(struct PNMParser* parser, uint8_t n_bits)
{
  int32_t   ch;
  uint16_t  tmp = 0;
  struct PNMBitBuffer *buf = &(parser->u_buffer.b);

  if (parser == NULL) {
    return -1;
  }

  /* 最上位ビットからデータを埋めていく
   * 初回ループではtmpの上位ビットにセット
   * 2回目以降は8bit単位で入力しtmpにセット */
  while (n_bits > buf->bit_count) {
    n_bits  -= buf->bit_count;
    tmp     |= PNM_GetLowerBits(buf->bit_count, buf->bit_buffer) << n_bits;
    /* 1バイト読み込み */
    if ((ch = getc(parser->fp)) == EOF) {
      return -1;
    }
    buf->bit_buffer  = ch;
    buf->bit_count   = 8;
  }

  /* 端数ビットの処理 
   * 残ったビット分をtmpの最上位ビットにセット */
  buf->bit_count -= n_bits;
  tmp            |= PNM_GetLowerBits(n_bits, buf->bit_buffer >> buf->bit_count);

  return tmp;
}

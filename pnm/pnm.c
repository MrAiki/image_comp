#include "pnm.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* パーサの読み込みバッファサイズ */
#define PNMPARSER_BUFFER_LENGTH         256
/* パーサが読み込み可能な整数の最大桁数 */
#define PNMPARSER_MAX_NUMBER_OF_DIGHTS  20

/* 下位n_bitsを取得 */
/* 補足）((1 << n_bits) - 1)は下位の数値だけ取り出すマスクになる */
#define PNM_GetLowerBits(n_bits, val) ((val) & ((1 << (n_bits)) - 1))

/* algnの倍数で切り上げる */
#define PNM_RoundUp(val, algn) ((val + ((algn)-1)) & ~((algn)-1))

/* ストライド（幅当たりバイト数）を求める */
#define PNM_CalcStride(width, psize) (PNM_RoundUp(((psize) * (width)),8) / 8)

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
  uint8_t   bytes[PNMPARSER_BUFFER_LENGTH];   /* ビットバッファ */
  int8_t    bit_count;                        /* ビット入力カウント */
  int32_t   byte_pos;                         /* バイト列読み込み位置 */
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

/* ライタ */
struct PNMBitWriter {
  FILE*     fp;                 /* 書き込みファイルポインタ */
  uint8_t   bit_buffer;         /* 出力途中のビット */
  int8_t    bit_count;          /* 出力カウント     */
};

/* ファイル読み込み */
static struct PNMPicture* PNM_CreatePictureFromFilePointer(FILE* fp);
/* ファイル書き込み */
static PNMError PNM_WritePictureToFilePointer(FILE* fp, const struct PNMPicture* picture);
/* ファイルヘッダ読み込み */
static PNMError PNMParser_ReadHeader(struct PNMParser* parser, 
    struct PNMHeader* header);
/* パーサの初期化 */
static void PNMParser_InitializeForText(struct PNMParser* parser, FILE* fp);
/* パーサの初期化 */
static void PNMParser_InitializeForBinary(struct PNMParser* parser, FILE* fp);
/* 次の文字の読み込み */
static int32_t PNMParser_GetNextCharacter(struct PNMParser* parser);
/* 次のトークン文字列の取得 返り値は読み込んだ文字数 */
static int32_t PNMParser_GetNextString(struct PNMParser* parser, char* buffer, uint32_t buffer_size);
/* 次の整数を読み取る */
static int32_t PNMParser_GetNextInteger(struct PNMParser* parser);
/* n_bit 取得し、結果を右詰めする */
/* エラー時は負値を返す */
static int32_t PNMParser_GetBits(struct PNMParser* parser, uint8_t n_bits);

/* P1フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP1(struct PNMParser* parser, struct PNMPicture* picture);
/* P2フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP2(struct PNMParser* parser, struct PNMPicture* picture);
/* P3フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP3(struct PNMParser* parser, struct PNMPicture* picture);
/* P4フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP4(struct PNMParser* parser, struct PNMPicture* picture);
/* P5フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP5(struct PNMParser* parser, struct PNMPicture* picture);
/* P6フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP6(struct PNMParser* parser, struct PNMPicture* picture);

/* ライタの初期化 */
static void PNMBitWriter_Initialize(struct PNMBitWriter* writer, FILE* fp);
/* valの下位n_bitを書き込む */
static PNMError PNMBitWriter_PutBits(struct PNMBitWriter* writer, uint64_t val, uint8_t n_bits);
/* バッファにたまったビットをクリア */
static void PNMBitWriter_Flush(struct PNMBitWriter* writer);

/* P1フォーマットファイルの書き込み */
static PNMError PNM_WriteP1(FILE* fp, const struct PNMPicture* picture);
/* P2フォーマットファイルの書き込み */
static PNMError PNM_WriteP2(FILE* fp, const struct PNMPicture* picture);
/* P3フォーマットファイルの書き込み */
static PNMError PNM_WriteP3(FILE* fp, const struct PNMPicture* picture);
/* P4フォーマットファイルの書き込み */
static PNMError PNM_WriteP4(FILE* fp, const struct PNMPicture* picture);
/* P5フォーマットファイルの書き込み */
static PNMError PNM_WriteP5(FILE* fp, const struct PNMPicture* picture);
/* P6フォーマットファイルの書き込み */
static PNMError PNM_WriteP6(FILE* fp, const struct PNMPicture* picture);

/* 画像の領域割り当て */
struct PNMPicture* PNM_CreatePicture(uint32_t width, uint32_t height);

/* ファイルオープン */
struct PNMPicture* PNM_CreatePictureFromFile(const char* filename)
{
  FILE* fp = fopen(filename, "rb");

  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s \n", filename);
    return NULL;
  }

  return PNM_CreatePictureFromFilePointer(fp);
}

/* ファイル書き込み */
PNMApiResult PNM_WritePictureToFile(const char* filename, const struct PNMPicture* picture)
{
  PNMError ret;
  FILE* fp = fopen(filename, "wb");

  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s \n", filename);
    return PNM_APIRESULT_NG;
  }

  /* 書き込み */
  ret = PNM_WritePictureToFilePointer(fp, picture);
  fclose(fp);

  if (ret != PNM_ERROR_OK) {
    return PNM_APIRESULT_NG;
  }

  return PNM_APIRESULT_OK;
}

/* ファイル読み込み */
static struct PNMPicture* PNM_CreatePictureFromFilePointer(FILE* fp)
{
  struct PNMParser  parser;
  struct PNMPicture *pnm;
  struct PNMHeader  header;
  PNMError          read_err;

  /* パーサ初期化 */
  PNMParser_InitializeForText(&parser, fp);

  /* ヘッダ読み込み */
  read_err = PNMParser_ReadHeader(&parser, &header);

  /* ヘッダ読み込みエラー */
  if (read_err != PNM_ERROR_OK) {
    fprintf(stderr, "Failed to read header. \n");
    return NULL;
  }

  /* 画像領域の領域確保 */
  pnm = PNM_CreatePicture(header.width, header.height);
  if (pnm == NULL) {
    fprintf(stderr, "Failed to allocate picture buffer. \n");
    return NULL;
  }
  /* ヘッダを設定 */
  pnm->header = header;

  /* タイプに合わせて画素値を取得 */
  switch (header.format) {
    case PNM_FORMAT_P1:
      read_err = PNMParser_ReadP1(&parser, pnm);
      break;
    case PNM_FORMAT_P2:
      read_err = PNMParser_ReadP2(&parser, pnm);
      break;
    case PNM_FORMAT_P3:
      read_err = PNMParser_ReadP3(&parser, pnm);
      break;
    case PNM_FORMAT_P4:
      PNMParser_InitializeForBinary(&parser, parser.fp);
      read_err = PNMParser_ReadP4(&parser, pnm);
      break;
    case PNM_FORMAT_P5:
      PNMParser_InitializeForBinary(&parser, parser.fp);
      read_err = PNMParser_ReadP5(&parser, pnm);
      break;
    case PNM_FORMAT_P6:
      PNMParser_InitializeForBinary(&parser, parser.fp);
      read_err = PNMParser_ReadP6(&parser, pnm);
      break;
  }

  if (read_err != PNM_ERROR_OK) {
    fprintf(stderr, "Failed to get picture data. \n");
    return NULL;
  }

  return pnm;
}

/* ファイル書き込み */
static PNMError PNM_WritePictureToFilePointer(FILE* fp, const struct PNMPicture* picture)
{
  PNMError write_err;
  const struct PNMHeader *header;

  /* 引数チェック */
  if (fp == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  /* ヘッダ読み出し */
  header = &(picture->header);

  /* フォーマット文字列の書き込み */
  switch (header->format) {
    case PNM_FORMAT_P1: fputs("P1\n", fp); break;
    case PNM_FORMAT_P2: fputs("P2\n", fp); break;
    case PNM_FORMAT_P3: fputs("P3\n", fp); break;
    case PNM_FORMAT_P4: fputs("P4\n", fp); break;
    case PNM_FORMAT_P5: fputs("P5\n", fp); break;
    case PNM_FORMAT_P6: fputs("P6\n", fp); break;
    default:
      fprintf(stderr, "Invalid format. \n");
      return PNM_ERROR_NG;
  }

  /* 幅と高さの書き込み */
  fprintf(fp, "%d %d\n", header->width, header->height);

  /* P1,P4以外では最大輝度値を書き込む */
  if (header->format != PNM_FORMAT_P1 && header->format != PNM_FORMAT_P4) {
    if (header->max_brightness > 255) {
      fprintf(stderr, "Unsupported max brightness(%d) \n",
          header->max_brightness);
      return PNM_ERROR_NG;
    }
    fprintf(fp, "%d\n", header->max_brightness);
  }

  /* フォーマット別の書き込みルーチンへ */
  switch (header->format) {
    case PNM_FORMAT_P1: write_err = PNM_WriteP1(fp, picture); break;
    case PNM_FORMAT_P2: write_err = PNM_WriteP2(fp, picture); break;
    case PNM_FORMAT_P3: write_err = PNM_WriteP3(fp, picture); break;
    case PNM_FORMAT_P4: write_err = PNM_WriteP4(fp, picture); break;
    case PNM_FORMAT_P5: write_err = PNM_WriteP5(fp, picture); break;
    case PNM_FORMAT_P6: write_err = PNM_WriteP6(fp, picture); break;
    default:
      fprintf(stderr, "Picture data write error. \n");
      return PNM_ERROR_NG;
  }

  if (write_err != PNM_ERROR_OK) {
    return PNM_ERROR_NG;
  }

  return PNM_ERROR_OK;
}

/* ファイルヘッダ読み込み */
static PNMError PNMParser_ReadHeader(struct PNMParser* parser, 
    struct PNMHeader* header)
{
  char          header_token[PNMPARSER_MAX_NUMBER_OF_DIGHTS];
  uint8_t       format_int;
  PNMFormat     format_tmp;
  int32_t       width_tmp, height_tmp, max_tmp;

  /* シグネチャ読み込み */
  PNMParser_GetNextString(parser,
      header_token, sizeof(header_token));

  /* シグネチャ検査 "P1", "P2", "P3", "P4", "P5", "P6" */
  format_int = header_token[1] - '0';
  if (header_token[0] != 'P'
      || format_int < 1 || format_int > 6
      || header_token[2] != '\0') {
    fprintf(stderr, "Invalid PNM format signature. \n");
    return PNM_ERROR_NG;
  }

  /* 列挙型に変換 */
  switch (format_int) {
    case 1: format_tmp = PNM_FORMAT_P1; break;
    case 2: format_tmp = PNM_FORMAT_P2; break;
    case 3: format_tmp = PNM_FORMAT_P3; break;
    case 4: format_tmp = PNM_FORMAT_P4; break;
    case 5: format_tmp = PNM_FORMAT_P5; break;
    case 6: format_tmp = PNM_FORMAT_P6; break;
    default:
            fprintf(stderr, "Invalid PNM format. \n");
            return PNM_ERROR_NG;
  }

  /* 幅と高さの取得 */
  width_tmp   = PNMParser_GetNextInteger(parser);
  height_tmp  = PNMParser_GetNextInteger(parser);
  if (width_tmp <= 0 || height_tmp <= 0) {
    fprintf(stderr, "Read error in width(%d) or height(%d). \n", width_tmp, height_tmp);
    return PNM_ERROR_NG;
  }

  /* 輝度の最大値を取得 */
  if (format_tmp != PNM_FORMAT_P1 && format_tmp != PNM_FORMAT_P4) {
    max_tmp = PNMParser_GetNextInteger(parser);
    if (max_tmp < 0) {
      fprintf(stderr, "Invalid max brightness(%d). \n", max_tmp);
      return PNM_ERROR_NG;
    } else if (max_tmp > 255) {
      /* 今は1bitより大きい輝度は対応していない */
      fprintf(stderr, "Unsupported max brightness(%d) \n", max_tmp);
      return PNM_ERROR_NG;
    }
  } else {
    max_tmp = 1;
  }

  /* 出力に反映 */
  header->format         = format_tmp;
  header->width          = width_tmp;
  header->height         = height_tmp;
  header->max_brightness = max_tmp;

  return PNM_ERROR_OK;
}

/* 画像の領域割り当て */
struct PNMPicture* PNM_CreatePicture(uint32_t width, uint32_t height)
{
  struct PNMPicture  *pnm;
  PNMPixel  **picture;
  uint32_t y;

  if (width <= 0 || height <= 0) {
    return NULL;
  }

  /* PNMイメージの割り当て */
  pnm = (struct PNMPicture *)malloc(sizeof(struct PNMPicture));
  if (pnm == NULL) {
    return NULL;
  }
  pnm->header.width  = width;
  pnm->header.height = height;

  /* 画素領域の割り当て */
  picture = (PNMPixel **)malloc(sizeof(PNMPixel*) * height);
  if (picture == NULL) {
    return NULL;
  }
  for (y = 0; y < height; y++) {
    /* 0クリアで割り当てておく */
    picture[y] = (PNMPixel *)calloc(width, sizeof(PNMPixel));
    if (picture[y] == NULL) {
      return NULL;
    }
  }

  pnm->picture = picture;
  return pnm;
}

/* 画像の領域解放 */
void PNM_DestroyPicture(struct PNMPicture* picture)
{
  uint32_t y;

  if (picture == NULL) {
    return;
  }

  for (y = 0; y < picture->header.height; y++) {
    free(picture->picture[y]);
  }

  free(picture->picture);
  free(picture);
}

/* P1フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP1(struct PNMParser* parser, struct PNMPicture* picture)
{
  int32_t   ch;
  uint32_t  x, y;

  if (parser == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      /* 空白の読み飛ばし */
      while (isspace(ch = PNMParser_GetNextCharacter(parser))) ;
      if (ch == '0') {
        picture->picture[y][x].b = 0;
      } else if (ch == '1') {
        picture->picture[y][x].b = 1;
      } else {
        return PNM_ERROR_NG;
      }
    }
  }

  return PNM_ERROR_OK;
}

/* P2フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP2(struct PNMParser* parser, struct PNMPicture* picture)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      picture->picture[y][x].g = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P3フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP3(struct PNMParser* parser, struct PNMPicture* picture)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      picture->picture[y][x].c.r = tmp;

      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      picture->picture[y][x].c.g = tmp;

      tmp = PNMParser_GetNextInteger(parser);
      if (tmp < 0) { return PNM_ERROR_NG; }
      picture->picture[y][x].c.b = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P4フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP4(struct PNMParser* parser, struct PNMPicture* picture)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      tmp = PNMParser_GetBits(parser, 1);
      if (tmp < 0) {  return PNM_ERROR_IO; }
      PNMPict_BIT(picture, x, y) = tmp;
    }
    /* HACK:ストライドはバイト単位になるため、
     * 横方向の末端ビットは0が埋まっている. 
     * -> リセットを掛けて読み飛ばす */
    parser->u_buffer.b.bit_count = 0;
  }

  return PNM_ERROR_OK;
}

/* P5フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP5(struct PNMParser* parser, struct PNMPicture* picture)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      PNMPict_GRAY(picture, x, y) = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P6フォーマットファイルの読み込み */
static PNMError PNMParser_ReadP6(struct PNMParser* parser, struct PNMPicture* picture)
{
  int32_t   tmp;
  uint32_t  x, y;

  if (parser == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      PNMPict_R(picture, x, y) = tmp;
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      PNMPict_G(picture, x, y) = tmp;
      tmp = PNMParser_GetBits(parser, 8);
      if (tmp < 0) { return PNM_ERROR_IO; }
      PNMPict_B(picture, x, y) = tmp;
    }
  }

  return PNM_ERROR_OK;
}

/* P1フォーマットファイルの書き込み */
static PNMError PNM_WriteP1(FILE* fp, const struct PNMPicture* picture)
{
  uint32_t x, y;
  int32_t bit;

  if (fp == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      bit = PNMPict_BIT(picture, x, y);
      if (bit != 0 && bit != 1) { 
        return PNM_ERROR_NG;
      }
      fprintf(fp, "%d ", bit);
    }
    fputs("\n", fp);
  }

  return PNM_ERROR_OK;
}

/* P2フォーマットファイルの書き込み */
static PNMError PNM_WriteP2(FILE* fp, const struct PNMPicture* picture)
{
  uint32_t x, y;
  uint32_t gray;

  if (fp == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      gray = PNMPict_GRAY(picture, x, y);
      if (gray > picture->header.max_brightness) { 
        return PNM_ERROR_NG;
      }
      fprintf(fp, "%d ", gray);
    }
    fputs("\n", fp);
  }

  return PNM_ERROR_OK;
}

/* P3フォーマットファイルの書き込み */
static PNMError PNM_WriteP3(FILE* fp, const struct PNMPicture* picture)
{
  uint32_t x, y;
  uint32_t r, g, b;

  if (fp == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      r = PNMPict_R(picture, x, y);
      g = PNMPict_G(picture, x, y);
      b = PNMPict_B(picture, x, y);
      if (r > picture->header.max_brightness
          || g > picture->header.max_brightness
          || b > picture->header.max_brightness) {
        return PNM_ERROR_NG;
      }
      fprintf(fp, "%d %d %d ", r, g, b);
    }
    fputs("\n", fp);
  }

  return PNM_ERROR_OK;
}

/* P4フォーマットファイルの書き込み */
static PNMError PNM_WriteP4(FILE* fp, const struct PNMPicture* picture)
{
  uint32_t x, y;
  uint32_t bit;
  struct PNMBitWriter writer;

  if (fp == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  /* ビットライタ初期化 */
  PNMBitWriter_Initialize(&writer, fp);

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      bit = PNMPict_BIT(picture, x, y);
      if (bit != 0 && bit != 1) { 
        return PNM_ERROR_NG;
      }
      if (PNMBitWriter_PutBits(&writer, bit, 1) != PNM_ERROR_OK) {
        return PNM_ERROR_IO;
      }
    }
    /* ストライドはバイト単位
     * -> 余ったビットを吐き出し、残りは0で埋める */
    PNMBitWriter_Flush(&writer);
  }

  return PNM_ERROR_OK;
}

/* P5フォーマットファイルの書き込み */
static PNMError PNM_WriteP5(FILE* fp, const struct PNMPicture* picture)
{
  uint32_t x, y;
  uint32_t gray;
  struct PNMBitWriter writer;

  if (fp == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  /* ビットライタ初期化 */
  PNMBitWriter_Initialize(&writer, fp);

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      gray = PNMPict_GRAY(picture, x, y);
      if (gray > picture->header.max_brightness) { 
        return PNM_ERROR_NG;
      }
      if (PNMBitWriter_PutBits(&writer, gray, 8) != PNM_ERROR_OK) {
        return PNM_ERROR_IO;
      }
    }
  }

  return PNM_ERROR_OK;
}

/* P6フォーマットファイルの書き込み */
static PNMError PNM_WriteP6(FILE* fp, const struct PNMPicture* picture)
{
  uint32_t x, y;
  uint32_t r, g, b;
  struct PNMBitWriter writer;

  if (fp == NULL || picture == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  /* ビットライタ初期化 */
  PNMBitWriter_Initialize(&writer, fp);

  for (y = 0; y < picture->header.height; y++) {
    for (x = 0; x < picture->header.width; x++) {
      r = PNMPict_R(picture, x, y);
      g = PNMPict_G(picture, x, y);
      b = PNMPict_B(picture, x, y);
      if (r > picture->header.max_brightness
          || g > picture->header.max_brightness
          || b > picture->header.max_brightness) {
        return PNM_ERROR_NG;
      }
      if ((PNMBitWriter_PutBits(&writer, r, 8) != PNM_ERROR_OK)
          || (PNMBitWriter_PutBits(&writer, g, 8) != PNM_ERROR_OK)
          || (PNMBitWriter_PutBits(&writer, b, 8) != PNM_ERROR_OK)) {
        return PNM_ERROR_IO;
      }
    }
  }

  return PNM_ERROR_OK;
}

/* パーサの初期化 */
static void PNMParser_InitializeForText(struct PNMParser* parser, FILE* fp)
{
  parser->fp                  = fp;
  memset(&parser->u_buffer, 0, sizeof(struct PNMStringBuffer));
  parser->u_buffer.s.read_pos = -1;
}

/* パーサの初期化 */
static void PNMParser_InitializeForBinary(struct PNMParser* parser, FILE* fp)
{
  parser->fp                    = fp;
  memset(&parser->u_buffer, 0, sizeof(struct PNMBitBuffer));
  parser->u_buffer.b.byte_pos   = -1;
}

/* 次の文字の読み込み */
static int32_t PNMParser_GetNextCharacter(struct PNMParser* parser)
{
  int32_t ch;
  struct PNMStringBuffer *buf = &(parser->u_buffer.s);

  /* 文字の読み込み */
  if (buf->read_pos == -1
      || buf->read_pos == (PNMPARSER_BUFFER_LENGTH-1)) {
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

  /* バッファ途中のナル文字: ファイル終端 */
  if (ch == '\0') {
    return EOF;
  }

  /* コメント */
  if (ch == '#') {
    /* 改行が表れるまで読み飛ばし */
    do {
      ch = PNMParser_GetNextCharacter(parser);
      /* コメント途中でファイル終端 */
      if (ch == EOF) { return EOF; }
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
  int32_t ch;
  uint32_t buffer_pos;

  if (parser == NULL || buffer == NULL) {
    return -1;
  }

  /* 空白の読み飛ばし */
  while (isspace(ch = PNMParser_GetNextCharacter(parser))) ;

  /* 文字列読み取り */
  buffer_pos = 0;
  while (ch != EOF && !isspace(ch) && (buffer_pos < (buffer_size-1))) {
      buffer[buffer_pos++] = ch;
      ch = PNMParser_GetNextCharacter(parser);
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
  if (err[0] != '\0') {
    return -1;
  }

  return ret;
}

/* n_bit 取得し、結果を右詰めする */
/* エラー時は負値を返す */
static int32_t PNMParser_GetBits(struct PNMParser* parser, uint8_t n_bits)
{
  uint16_t  tmp = 0;
  struct PNMBitBuffer *buf = &(parser->u_buffer.b);

  if (parser == NULL) {
    return -1;
  }

  /* 初回読み込み */
  if (buf->byte_pos == -1) {
      if (fread(buf->bytes, sizeof(uint8_t),
            PNMPARSER_BUFFER_LENGTH, parser->fp) == 0) {
        return -1;
      }
      buf->byte_pos   = 0;
      buf->bit_count  = 8;
  }

  /* 最上位ビットからデータを埋めていく
   * 初回ループではtmpの上位ビットにセット
   * 2回目以降は8bit単位で入力しtmpにセット */
  while (n_bits > buf->bit_count) {
    /* 上位bitから埋めていく */
    n_bits  -= buf->bit_count;
    tmp     |= PNM_GetLowerBits(buf->bit_count, buf->bytes[buf->byte_pos]) << n_bits;

    /* 1バイト読み進める */
    buf->byte_pos++;
    buf->bit_count   = 8;

    /* バッファが一杯ならば、再度読み込み */
    if (buf->byte_pos == PNMPARSER_BUFFER_LENGTH) {
      if (fread(buf->bytes, sizeof(uint8_t),
            PNMPARSER_BUFFER_LENGTH, parser->fp) == 0) {
        return -1;
      }
      buf->byte_pos = 0;
    }
  }

  /* 端数ビットの処理 
   * 残ったビット分をtmpの最上位ビットにセット */
  buf->bit_count -= n_bits;
  tmp            |= PNM_GetLowerBits(n_bits, buf->bytes[buf->byte_pos] >> buf->bit_count);

  return tmp;
}

/* ライタの初期化 */
static void PNMBitWriter_Initialize(struct PNMBitWriter* writer, FILE* fp)
{
  writer->fp          = fp;
  writer->bit_count   = 8;
  writer->bit_buffer  = 0;
}

/* valの下位n_bitを書き込む */
static PNMError PNMBitWriter_PutBits(struct PNMBitWriter* writer, uint64_t val, uint8_t n_bits)
{
  /* 無効な引数 */
  if (writer == NULL) {
    return PNM_ERROR_INVALID_PARAMETER;
  }

  /* valの上位ビットから順次出力
   * 初回ループでは端数（出力に必要なビット数）分を埋め出力
   * 2回目以降は8bit単位で出力 */
  while (n_bits >= writer->bit_count) {
    n_bits -= writer->bit_count;
    writer->bit_buffer |= PNM_GetLowerBits(writer->bit_count, val >> n_bits);

    if (fputc(writer->bit_buffer, writer->fp) == EOF) {
      return PNM_ERROR_IO;
    }

    writer->bit_buffer  = 0;
    writer->bit_count   = 8;
  }

  /* 端数ビットの処理:
   * 残った分をバッファの上位ビットにセット */
  writer->bit_count -= n_bits;
  writer->bit_buffer |= PNM_GetLowerBits(n_bits, val) << writer->bit_count;

  return PNM_ERROR_OK;
}

/* バッファにたまったビットをクリア */
static void PNMBitWriter_Flush(struct PNMBitWriter* writer)
{
  if (writer == NULL) {
    return;
  }

  /* バッファに余ったビットを強制出力 */
  if (writer->bit_count != 8) {
    PNMBitWriter_PutBits(writer, 0, writer->bit_count);
    writer->bit_buffer = 0;
    writer->bit_count  = 8;
  }
}

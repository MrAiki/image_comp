#include "src.h"
#include "predictive_coding.h"
#include "coding_utility.h"
#include "../SGM/sgm.h"
#include "../HORC/horc.h"
#include "../AdaptiveHuffman/adaptive_huffman.h"
#include "../BitStream/bit_stream.h"

#include <stdlib.h>
#include <assert.h>

/* 
 * ランウェイは沢山の人の想いを背負って歩く道
 *                           アイカツ5話,35話
 */

/* 2値のうち最大を取得 */
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
/* 符号付き32bit数値を符号なし32bit数値に一意変換 */
/* 偶数は負、奇数は正とする */
#define SINT32_TO_UINT32(sint) (((sint) <= 0) ? (-((sint) << 1)) : (((sint) << 1) - 1))
/* 符号なし32bit数値を符号付き32bit数値に一意変換 */
#define UINT32_TO_SINT32(uint) (((uint) & 1) ? ((int32_t)((uint) >> 1) + 1) : (-(int32_t)((uint) >> 1)))

/* ランのモード */
typedef enum RunModeTag {
  PACKBITS_MODE_NOT_RUN = 0,    /* 非ラン */
  PACKBITS_MODE_RUN = 1,        /* ラン */
} RunMode;

/* SRCによる符号化処理コア */
static void SRC_EncodeCore(
    struct BitStream* strm, const uint32_t* data, uint32_t num_data, uint32_t threshould);
/* SRCによる復号処理コア */
static void SRC_DecodeCore(struct BitStream* strm, uint32_t* data, uint32_t num_data);
/* ランレングスの取得 */
static uint32_t SRC_GetRunLength(const uint32_t* data, uint32_t pos, uint32_t num_data);
/* 符号付きシンボル配列を符号なしに変換 */
static void SRC_ConvertSignedToUnsigned(uint32_t* dst, const int32_t* src, uint32_t num);
/* 符号なしシンボル配列を符号付きに変換 */
static void SRC_ConvertUnsignedToSigned(int32_t* dst, const uint32_t* src, uint32_t num);
/* 範囲外アクセス対応版アクセサ */
static int32_t SGMPicture_GetGRAY(
    const struct SGMPicture* sgm, int32_t x, int32_t y);

/* SRCによる符号化 */
SRCApiResult SRC_Encode(const char* in_filename, const char* out_filename)
{
  uint32_t x, y;
  struct BitStream* strm;
  struct SGMPicture* sgm;
  uint32_t num_data, i_data;
  int32_t  *diff;
  uint32_t *output_symbol;
  const uint32_t threshould   = 10;

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

  /* あとはLOCO-I予測 */
  for (y = 1; y < sgm->height; y++) {
    for (x = 1; x < sgm->width; x++) {
      int32_t w  = SGMPicture_GetGRAY(sgm, x-1,   y);
      int32_t nw = SGMPicture_GetGRAY(sgm, x-1, y-1);
      int32_t n  = SGMPicture_GetGRAY(sgm,   x, y-1);
      int32_t nn = SGMPicture_GetGRAY(sgm,   x, y-2);
      int32_t ww = SGMPicture_GetGRAY(sgm, x-2,   y);
      diff[i_data++] = PredictiveCoding_MyLOCOI(w,n,nw,nn,ww) - SGMPicture_GetGRAY(sgm, x, y);
    }
  }

  /* 符号出力 */
  /* ヘッダ部 */
  strm = BitStream_Open(out_filename, "wb", NULL, 0);
  BitStream_PutBits(strm, 8, 'S');  /* Shibuki */
  BitStream_PutBits(strm, 8, 'R');  /* Ran-length */
  BitStream_PutBits(strm, 8, 'C');  /* Coding */
  BitStream_PutBits(strm, 16, sgm->width);
  BitStream_PutBits(strm, 16, sgm->height);
  /* 先頭はそのまま1バイト出力 */
  BitStream_PutBits(strm, 8, SGMPicture_GRAY(sgm, 0, 0));
  /* 差分信号 */
  SRC_ConvertSignedToUnsigned(output_symbol, diff, num_data);
  SRC_EncodeCore(strm, output_symbol, num_data, threshould);
  BitStream_Close(strm);

  free(diff);
  free(output_symbol);
  SGMPicture_Destroy(sgm);

  return SRC_APIRESULT_OKOKOK;
}

/* SRCによる復号 */
SRCApiResult SRC_Decode(const char* in_filename, const char* out_filename)
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
  BitStream_GetBits(strm, 24, &bitsbuf);
  if (   (((bitsbuf >> 16) & 0xFF) != 'S')
      || (((bitsbuf >>  8) & 0xFF) != 'R')
      || (((bitsbuf >>  0) & 0xFF) != 'C')) {
    fprintf(stderr, "Failed to check signature: %8llx \n", bitsbuf);
    return SRC_APIRESULT_NG;
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

  /* 復元用画像生成 */
  sgm = SGMPicture_Create(width, height);

  /* 先頭画素 */
  BitStream_GetBits(strm, 8, &bitsbuf);
  SGMPicture_GRAY(sgm, 0, 0) = (uint8_t)bitsbuf;

  /* それ以外の画素はSRCにより符号化されている */
  SRC_DecodeCore(strm, output_symbol, num_data);
  BitStream_Close(strm);

  /* 符号なしを符号付きに戻す */
  SRC_ConvertUnsignedToSigned(diff, output_symbol, num_data);

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

  /* LOCO-I予測分 */
  for (y = 1; y < sgm->height; y++) {
    for (x = 1; x < sgm->width; x++) {
      int32_t w  = SGMPicture_GetGRAY(sgm, x-1,   y);
      int32_t nw = SGMPicture_GetGRAY(sgm, x-1, y-1);
      int32_t n  = SGMPicture_GetGRAY(sgm,   x, y-1);
      int32_t nn = SGMPicture_GetGRAY(sgm,   x, y-2);
      int32_t ww = SGMPicture_GetGRAY(sgm, x-2,   y);
      SGMPicture_GRAY(sgm, x, y) = PredictiveCoding_MyLOCOI(w,n,nw,nn,ww) - diff[i_data++];
    }
  }

  SGMPicture_WriteToFile(out_filename, sgm);
  SGMPicture_Destroy(sgm);

  free(diff);
  free(output_symbol);

  return SRC_APIRESULT_OKOKOK;
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

/* 符号付きシンボル配列を符号なしに変換 */
static void SRC_ConvertSignedToUnsigned(uint32_t* dst, const int32_t* src, uint32_t num)
{
  uint32_t i;

  for (i = 0; i < num; i++) {
    dst[i] = SINT32_TO_UINT32(src[i]);
  }
}

/* 符号なしシンボル配列を符号付きに変換 */
static void SRC_ConvertUnsignedToSigned(int32_t* dst, const uint32_t* src, uint32_t num)
{
  uint32_t i;

  for (i = 0; i < num; i++) {
    dst[i] = UINT32_TO_SINT32(src[i]);
  }

}

/* ランレングスの取得 */
static uint32_t SRC_GetRunLength(const uint32_t* data, uint32_t pos, uint32_t num_data)
{
  uint32_t runlength;

  runlength = 0;
  while (pos < num_data-1 && data[pos] == data[pos+1]) {
    runlength++;
    pos++;
  }

  return runlength;
}

/* SRCによる符号化 */
static void SRC_EncodeCore(
    struct BitStream* strm, const uint32_t* data, uint32_t num_data, uint32_t threshould)
{
  uint32_t pos;
  uint32_t runlength;
  uint32_t head_run, tail_run;
  uint8_t  headtailrun_bits;
  struct AdaptiveHuffmanTree* run_tree;
  struct AdaptiveHuffmanTree* notrun_tree;
  struct HORC* horc = HORC_Create(1, 9);
  uint32_t *encode_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t *run_data    = malloc(sizeof(uint32_t) * num_data);
  uint32_t *notrun_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t encode_data_pos = 0;
  uint32_t run_data_pos = 0, notrun_data_pos = 0;
  uint32_t max_runlength = 0, max_notrunlength = 0, runlength_bits, notrunlength_bits;
  uint32_t run_pos;
  
  /* 先頭の0と末尾の0長さを記録 */
  /* 先頭 */
  head_run = 0;
  if (data[0] == 0) {
    head_run = SRC_GetRunLength(data, 0, num_data);
  }
  /* 末尾 */
  tail_run = 0;
  for (pos = num_data-1; pos > head_run; pos--) {
    if (data[pos] != 0) {
      break;
    }
    tail_run++;
  }
  /* その値を可変長で記録 */
  if (MAX(head_run, tail_run) != 0) {
    headtailrun_bits = CodingUtility_Log2ceil(MAX(head_run, tail_run));
  } else {
    headtailrun_bits = 1;
  }
  BitStream_PutBits(strm, 5, headtailrun_bits);
  BitStream_PutBits(strm, headtailrun_bits, head_run);
  BitStream_PutBits(strm, headtailrun_bits, tail_run);

  /* 開始位置とデータ数の差し替え */
  pos = head_run;
  num_data = num_data - tail_run;

  /* 最初がどちらのモードで始まるのか出力しておく */
  runlength = SRC_GetRunLength(data, pos, num_data);
  if (runlength < threshould || data[pos] != 0) {
    BitStream_PutBit(strm, PACKBITS_MODE_NOT_RUN);
  } else {
    BitStream_PutBit(strm, PACKBITS_MODE_RUN);
  }

  while (pos < num_data) {
    /* ランレングス取得 */
    runlength = SRC_GetRunLength(data, pos, num_data);
    if (data[pos] != 0 || runlength < threshould) {
      /* 非ラン出力モード */
      uint32_t notrunlength, notrun, tmp_pos;
      /* 非ランの出力回数を取得 */
      notrunlength = 0;
      tmp_pos = pos;
      while (((SRC_GetRunLength(data, tmp_pos, num_data) < threshould)
              || (data[tmp_pos] != 0))
              && (tmp_pos < num_data)) {
        notrunlength++; tmp_pos++;
      }

      notrun_data[notrun_data_pos++] = notrunlength;
      if (max_notrunlength < notrunlength) {
        max_notrunlength = notrunlength;
      }
      /* 非ラン分出力 */
      for (notrun = 0; notrun < notrunlength; notrun++) {
        encode_data[encode_data_pos++] = data[pos++];
      }
    } else {
      /* ラン出力モード */
      /* 実際に登録するラン長さはランレングス+1 */
      runlength++;
      /* ランレングスだけ進むのは確定なので先に進めておく */
      pos += runlength;
      run_data[run_data_pos++] = runlength;
      if (max_runlength < runlength) {
        max_runlength = runlength;
      }
    }
  }

  /* ランの差分エンコーディング */
  runlength_bits = CodingUtility_Log2ceil(max_runlength);
  BitStream_PutBits(strm, 16, run_data_pos);
  BitStream_PutBits(strm, 5, runlength_bits);
  /* 差分を取るため1bit付加する */
  run_tree    = AdaptiveHuffmanTree_Create(runlength_bits + 1); 
  BitStream_PutBits(strm, runlength_bits, run_data[0]);
  for (run_pos = 1; run_pos < run_data_pos; run_pos++) {
    AdaptiveHuffman_EncodeSymbol(run_tree, strm,
        SINT32_TO_UINT32((int32_t)run_data[run_pos-1] - (int32_t)run_data[run_pos]));
  }

  /* 非ランの差分エンコーディング */
  notrunlength_bits = CodingUtility_Log2ceil(max_notrunlength);
  BitStream_PutBits(strm, 16, notrun_data_pos);
  BitStream_PutBits(strm, 5, notrunlength_bits);
  /* 差分を取るため1bit付加する */
  notrun_tree = AdaptiveHuffmanTree_Create(notrunlength_bits + 1);
  BitStream_PutBits(strm, notrunlength_bits, notrun_data[0]);
  for (run_pos = 1; run_pos < notrun_data_pos; run_pos++) {
    AdaptiveHuffman_EncodeSymbol(notrun_tree, strm,
        SINT32_TO_UINT32((int32_t)notrun_data[run_pos-1] - (int32_t)notrun_data[run_pos]));
  }

  /* 非ラン部分をHORCエンコード */
  HORC_Encode(horc, strm, encode_data, encode_data_pos);
  HORC_Destroy(horc);

  free(encode_data);
  free(run_data);
  free(notrun_data);
  AdaptiveHuffmanTree_Destroy(run_tree);
  AdaptiveHuffmanTree_Destroy(notrun_tree);
}

/* SRCによる復号 */
static void SRC_DecodeCore(struct BitStream* strm, uint32_t* data, uint32_t num_data)
{
  uint32_t      pos, i;
  RunMode       mode;
  uint32_t      length;
  uint64_t      bitsbuf;
  uint32_t      headtailrun_bits, head_run, tail_run;
  struct HORC* horc = HORC_Create(1, 9);
  uint32_t *decode_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t decode_data_pos = 0;

  struct AdaptiveHuffmanTree* run_tree;
  struct AdaptiveHuffmanTree* notrun_tree;
  uint32_t *run_data    = malloc(sizeof(uint32_t) * num_data);
  uint32_t *notrun_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t run_data_pos = 0, notrun_data_pos = 0;
  uint32_t runlength_bits, notrunlength_bits;
  uint32_t run_pos;

  assert(data != NULL);

  /* 最初にデータを全て1で埋める */
  /* 0とそれ以外を識別するため */
  for (pos = 0; pos < num_data; pos++) {
    data[pos] = 1;
  }

  /* 先頭と末尾の0を取得 */
  BitStream_GetBits(strm, 5, &bitsbuf);
  headtailrun_bits = (uint8_t)bitsbuf;
  BitStream_GetBits(strm, headtailrun_bits, &bitsbuf);
  head_run = (uint32_t)bitsbuf;
  BitStream_GetBits(strm, headtailrun_bits, &bitsbuf);
  tail_run = (uint32_t)bitsbuf;
  for (pos = 0; pos < head_run; pos++) {
    data[pos] = 0;
  }
  for (pos = num_data - tail_run; pos < num_data; pos++) {
    data[pos] = 0;
  }

  /* 開始位置とデータ数の切り替え */
  pos = head_run;
  num_data = num_data - tail_run;

  /* 最初のモード取得 */
  mode = BitStream_GetBit(strm);

  /* ラン情報のデコード */
  BitStream_GetBits(strm, 16, &bitsbuf);
  run_data_pos   = (uint32_t)bitsbuf;
  BitStream_GetBits(strm,  5, &bitsbuf);
  runlength_bits = (uint32_t)bitsbuf;
  run_tree       = AdaptiveHuffmanTree_Create(runlength_bits + 1);
  BitStream_GetBits(strm,  runlength_bits, &bitsbuf);
  run_data[0]    = (uint32_t)bitsbuf;
  for (run_pos = 1; run_pos < run_data_pos; run_pos++) {
    AdaptiveHuffman_DecodeSymbol(run_tree, strm, &length);
    run_data[run_pos] = (int32_t)run_data[run_pos-1] - UINT32_TO_SINT32(length);
  }

  /* 非ラン情報のデコード */
  BitStream_GetBits(strm, 16, &bitsbuf);
  notrun_data_pos = (uint32_t)bitsbuf;
  BitStream_GetBits(strm,  5, &bitsbuf);
  notrunlength_bits  = (uint32_t)bitsbuf;
  notrun_tree     = AdaptiveHuffmanTree_Create(notrunlength_bits + 1);
  BitStream_GetBits(strm,  notrunlength_bits, &bitsbuf);
  notrun_data[0]  = (uint32_t)bitsbuf;
  for (run_pos = 1; run_pos < notrun_data_pos; run_pos++) {
    AdaptiveHuffman_DecodeSymbol(notrun_tree, strm, &length);
    notrun_data[run_pos] = (int32_t)notrun_data[run_pos-1] - UINT32_TO_SINT32(length);
  }

  run_data_pos = notrun_data_pos = 0;
  while (pos < num_data) {
    switch (mode) {
      case PACKBITS_MODE_NOT_RUN:
        length = notrun_data[notrun_data_pos++];
        /* データは埋めずに設定位置だけ動かす */
        pos += length;
        decode_data_pos += length;
        break;
      case PACKBITS_MODE_RUN:
        length = run_data[run_data_pos++];
        /* ランは0が埋まっている */
        for (i = 0; i < length; i++) {
          data[pos++] = 0;
        }
        break;
      default:
        assert(0);
    }

    /* モード切り替え */
    mode = (mode == PACKBITS_MODE_RUN) ?
      PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
  } 

  /* HORCデコード */
  HORC_Decode(horc, strm, decode_data, decode_data_pos);
  decode_data_pos = 0;
  for (pos = 0; pos < num_data; pos++) {
    if (data[pos] != 0) {
      data[pos] = decode_data[decode_data_pos++];
    }
  }

  free(decode_data);
  free(run_data);
  free(notrun_data);
  HORC_Destroy(horc);
  AdaptiveHuffmanTree_Destroy(run_tree);
  AdaptiveHuffmanTree_Destroy(notrun_tree);
}

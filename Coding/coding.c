#include "coding.h"
#include "../AdaptiveHuffman/adaptive_huffman.h"
#include "../LZAH/lzah.h"
#include "../HORC/horc.h"
#include "../../BitStream/bit_stream.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define GET_INT_SIGN(val) (((val) > 0) ? 1 : 0)
#define GET_INT_ABS(val)  (((val) > 0) ? (val) : (-(val)))
/* 符号付き32bit数値を符号なし32bit数値に一意変換 */
/* 偶数は負、奇数は正とする */
#define SINT32_TO_UINT32(sint) (((sint) <= 0) ? (-((sint) << 1)) : (((sint) << 1) - 1))
/* 符号なし32bit数値を符号付き32bit数値に一意変換 */
#define UINT32_TO_SINT32(uint) (((uint) & 1) ? (((uint) >> 1) + 1) : (-(((int32_t)(uint)) >> 1)))

/* 復号モード */
typedef enum PackBitsModeTag {
  PACKBITS_MODE_NOT_RUN = 0,    /* 非ラン */
  PACKBITS_MODE_RUN = 1,        /* ラン */
} PackBitsMode;

/* NLZ(上位ビットから1にぶつかるまでの0の数)を計算する(http://www.nminoru.jp/~nminoru/programming/bitcount.html) */
static int32_t nlz5(uint32_t x)
{
  uint32_t c = 0;
  if (x == 0) return 32;
  if (x & 0xffff0000) { x &= 0xffff0000; c |= 0x10; }
  if (x & 0xff00ff00) { x &= 0xff00ff00; c |= 0x08; }
  if (x & 0xf0f0f0f0) { x &= 0xf0f0f0f0; c |= 0x04; }
  if (x & 0xcccccccc) { x &= 0xcccccccc; c |= 0x02; }
  if (x & 0xaaaaaaaa) { c |= 0x01; }
  return c ^ 31;
}

/* ceil(log2(val)) を計算する */
static uint32_t log2ceil(uint32_t val)
{
  return 32 - nlz5(val - 1);
}

/* 符号部の長さを決定 */
static uint32_t SignedWyle_GetLength(uint32_t abs)
{
  if (abs < 8) {
    return 1;
  }
  
  return log2ceil(abs) - 1;
}

/* 符号付きワイル符号の出力 */
void SignedWyle_PutCode(struct BitStream* strm, int32_t val)
{
  uint32_t i;
  uint32_t sign     = GET_INT_SIGN(val);
  uint32_t abs      = GET_INT_ABS(val);
  uint32_t length   = SignedWyle_GetLength(abs);

  /* 符号部を出力 */
  for (i = 0; i < length; i++) {
    BitStream_PutBit(strm, sign);
  }
  
  /* ビット反転して1ビット出力 */
  BitStream_PutBit(strm, 1 - sign);

  /* 数値部の出力 */
  BitStream_PutBits(strm, length + 2, abs);
}

/* 符号付きワイル符号の取得 */
int32_t SignedWyle_GetCode(struct BitStream* strm)
{
  uint32_t sign;
  uint32_t length;
  uint64_t abs;

  /* 符号ビットを取得 */
  sign = BitStream_GetBit(strm);

  /* 数値部長を取得 */
  length = 3; /* sign分で1 + 符号出力するときに2 */
  while (BitStream_GetBit(strm) == (int32_t)sign) {
    length++;
  }

  /* 絶対値を取得 */
  BitStream_GetBits(strm, length, &abs);

  /* 符号を加味 */
  return (sign == 0) ? -(int32_t)abs : (int32_t)abs;
}

/* ゴロム符号化の出力 */
void Golomb_PutCode(struct BitStream* strm, uint32_t m, uint32_t val)
{
  uint32_t quot;
  uint32_t rest;
  uint32_t b, two_b;
  uint32_t i;

  assert(m != 0);

  /* 商部分長と剰余部分の計算 */
  quot = val / m;
  rest = val % m;

  /* 前半部分の出力(unary符号) */
  for (i = 0; i < quot; i++) {
    BitStream_PutBit(strm, 0);
  }
  BitStream_PutBit(strm, 1);

  /* 剰余部分の出力 */
  if (!(m & (m - 1))) {
    /* mが2の冪: ライス符号化 */
    BitStream_PutBits(strm, log2ceil(m), rest);
    return;
  }

  /* ゴロム符号化 */
  b = log2ceil(m);
  two_b = 1UL << b;
  if (rest < (two_b - m)) {
    BitStream_PutBits(strm, b - 1, rest);
  } else {
    BitStream_PutBits(strm, b, rest + two_b - m);
  }
}

/* ゴロム符号化の取得 */
uint32_t Golomb_GetCode(struct BitStream* strm, uint32_t m) 
{
  uint32_t quot;
  uint64_t rest;
  uint32_t b, two_b;

  assert(m != 0);

  /* 前半のunary符号部分を読み取り */
  quot = 0;
  while (BitStream_GetBit(strm) == 0) {
    quot++;
  }

  /* 剰余部分の読み取り */
  if (!(m & (m - 1))) {
    /* mが2の冪: ライス符号化 */
    BitStream_GetBits(strm, log2ceil(m), &rest);
    return (uint32_t)(quot * m + rest);
  }

  /* ゴロム符号化 */
  b = log2ceil(m);
  two_b = 1UL << b;
  BitStream_GetBits(strm, b - 1, &rest);
  if (rest < (two_b - m)) {
    return (uint32_t)(quot * m + rest);
  } else {
    rest <<= 1;
    rest += BitStream_GetBit(strm);
    return (uint32_t)(quot * m + rest - (two_b - m));
  }
}

/* ランレングスの取得 */
static uint32_t PackBits_GetRunLength(const uint32_t* data, uint32_t pos, uint32_t num_data)
{
  uint32_t runlength;

  runlength = 0;
  while (pos < num_data-1 && data[pos] == data[pos+1]) {
    runlength++;
    pos++;
  }

  return runlength;
}

/* Pack Bitsによる符号化 */
void PackBits_Encode(struct BitStream* strm,
    const uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t threshould, uint32_t length_bits)
{
  uint32_t runlength;
  uint32_t pos;
  const uint32_t MAXLENGTH
    = (1UL << length_bits) - 1;   /* 表現可能な最大長 */

  /* 最初がどちらのモードで始まるのか出力しておく */
  pos = 0;
  runlength = PackBits_GetRunLength(data, pos, num_data);
  if (runlength < threshould) {
    BitStream_PutBit(strm, PACKBITS_MODE_NOT_RUN);
  } else {
    BitStream_PutBit(strm, PACKBITS_MODE_RUN);
  }

  while (pos < num_data) {
    /* ランレングス取得 */
    runlength = PackBits_GetRunLength(data, pos, num_data);
    if (runlength < threshould) {
      /* 非ラン出力モード */
      uint32_t notrunlength, notrun, tmp_pos;
      /* 非ランの出力回数を取得 */
      notrunlength = 0;
      tmp_pos = pos;
      while (PackBits_GetRunLength(data, tmp_pos, num_data) < threshould
          && tmp_pos < num_data) {
        notrunlength++; tmp_pos++;
      }
      // printf("notrun:%d \n", notrunlength);
      /* 非ラン分出力 */
      while (notrunlength >= MAXLENGTH) {
        BitStream_PutBits(strm, length_bits, MAXLENGTH);
        for (notrun = 0; notrun < MAXLENGTH; notrun++) {
          Golomb_PutCode(strm, golomb_m, data[pos++]);
        }
        /* 非ラン継続符号を出力 */
        BitStream_PutBits(strm, length_bits, 0);
        notrunlength -= MAXLENGTH;
      }
      BitStream_PutBits(strm, length_bits, notrunlength);
      for (notrun = 0; notrun < notrunlength; notrun++) {
        Golomb_PutCode(strm, golomb_m, data[pos++]);
      }
    } else {
      /* ラン出力モード */
      uint32_t outsymbol = data[pos];
      /* 実際に登録するラン長さはランレングス+1 */
      runlength++;
      /* ランレングスだけ進むのは確定なので先に進めておく */
      pos += runlength;
      // printf("sym:%d run:%d \n", outsymbol, runlength);
      /* ランレングス出力 */
      while (runlength >= MAXLENGTH) {
        BitStream_PutBits(strm, length_bits, MAXLENGTH);
        Golomb_PutCode(strm, golomb_m, outsymbol);
        /* ラン継続符号を出力 */
        BitStream_PutBits(strm, length_bits, 0);
        runlength -= MAXLENGTH;
      }
      BitStream_PutBits(strm, length_bits, runlength);
      Golomb_PutCode(strm, golomb_m, outsymbol);
      /* 別の符号のランが続く */
      if (PackBits_GetRunLength(data, pos, num_data) >= threshould) {
        BitStream_PutBits(strm, length_bits, 0);
      }
    }
  }

}

/* Pack Bitsによる復号 */
void PackBits_Decode(struct BitStream* strm,
    uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t length_bits)
{
  uint32_t      pos, i;
  PackBitsMode  mode;
  uint64_t      length;
  int32_t       getsymbol;

  assert(data != NULL);

  /* 最初のモード取得 */
  mode = BitStream_GetBit(strm);

  /* 最初のモード切り替えで切り替わるので一回反転する */
  mode = (mode == PACKBITS_MODE_RUN) ?
    PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
  pos = 0;
  while (pos < num_data) {
    /* ラン/非ラン長を取得 */
    BitStream_GetBits(strm, length_bits, &length);
    if (length == 0) {
      /* モード継続: 長さを再取得 */
      BitStream_GetBits(strm, length_bits, &length);
    } else {
      /* モード切り替え */
      mode = (mode == PACKBITS_MODE_RUN) ?
        PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
    }
    switch (mode) {
      case PACKBITS_MODE_NOT_RUN:
        for (i = 0; i < length; i++) {
          data[pos++] = Golomb_GetCode(strm, golomb_m);
        }
        break;
      case PACKBITS_MODE_RUN:
        getsymbol = Golomb_GetCode(strm, golomb_m);
        for (i = 0; i < length; i++) {
          data[pos++] = getsymbol;
        }
        break;
      default:
        assert(0);
    }
  } 

}

static uint32_t run_array[1UL << 9] = { 0, };

/* Mineo版Pack Bitsによる符号化 */
void MineoPackBits_Encode(struct BitStream* strm,
    const uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t threshould, uint32_t length_bits)
{
  uint32_t pos;
  uint32_t runlength;
  uint32_t head_run, tail_run;
  uint8_t  headtailrun_bits;
  const uint32_t MAXLENGTH = (1UL << length_bits) - 1;   /* 表現可能な最大長 */
  struct AdaptiveHuffmanTree* tree = AdaptiveHuffmanTree_Create(length_bits);
  struct AdaptiveHuffmanTree* code_tree[6];
  
  /* 木を複数用意する */
  code_tree[0] = AdaptiveHuffmanTree_Create(9);
  code_tree[1] = AdaptiveHuffmanTree_Create(9);
  code_tree[2] = AdaptiveHuffmanTree_Create(9);
  code_tree[3] = AdaptiveHuffmanTree_Create(9);
  code_tree[4] = AdaptiveHuffmanTree_Create(9);
  code_tree[5] = AdaptiveHuffmanTree_Create(9);

  /* 先頭の0と末尾の0長さを記録 */
  /* 先頭 */
  head_run = 0;
  if (data[0] == 0) {
    head_run = PackBits_GetRunLength(data, 0, num_data);
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
    headtailrun_bits = log2ceil(MAX(head_run, tail_run));
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
  runlength = PackBits_GetRunLength(data, pos, num_data);
  if (runlength < threshould || data[pos] != 0) {
    BitStream_PutBit(strm, PACKBITS_MODE_NOT_RUN);
  } else {
    BitStream_PutBit(strm, PACKBITS_MODE_RUN);
  }

  while (pos < num_data) {
    /* ランレングス取得 */
    runlength = PackBits_GetRunLength(data, pos, num_data);
    if (data[pos] != 0 || runlength < threshould) {
      /* 非ラン出力モード */
      uint32_t notrunlength, notrun, tmp_pos;
      /* 非ランの出力回数を取得 */
      notrunlength = 0;
      tmp_pos = pos;
      while (((PackBits_GetRunLength(data, tmp_pos, num_data) < threshould)
              || (data[tmp_pos] != 0))
              && (tmp_pos < num_data)) {
        notrunlength++; tmp_pos++;
      }

      /*
      printf("notrun:%d \n", notrunlength);
      for (notrun = 0; notrun < notrunlength; notrun++) {
        printf("%d, ", data[pos + notrun]);
      }
      printf("\n");
      */

      /* 非ラン分出力 */
      while (notrunlength >= MAXLENGTH) {
        AdaptiveHuffman_EncodeSymbol(tree, strm, MAXLENGTH);
        run_array[MAXLENGTH]++;
        /* 直前の画素に合わせて使用する木を切り替える */
        for (notrun = 0; notrun < MAXLENGTH; notrun++) {
          if (data[pos-1] == 0) {
            AdaptiveHuffman_EncodeSymbol(code_tree[0], strm, data[pos++]);
          } else if (data[pos-1] <= 2) {
            AdaptiveHuffman_EncodeSymbol(code_tree[1], strm, data[pos++]);
          } else if (log2ceil(data[pos-1]) <= 4) {
            AdaptiveHuffman_EncodeSymbol(
                code_tree[log2ceil(data[pos-1])] , strm, data[pos++]);
          } else {
            AdaptiveHuffman_EncodeSymbol(code_tree[5], strm, data[pos++]);
          }
        }
        /* 非ラン継続符号を出力 */
        AdaptiveHuffman_EncodeSymbol(tree, strm, 0);
        run_array[0]++;
        notrunlength -= MAXLENGTH;
      }
      AdaptiveHuffman_EncodeSymbol(tree, strm, notrunlength);
      run_array[notrunlength]++;
      /* 直前の画素に合わせて使用する木を切り替える */
      for (notrun = 0; notrun < notrunlength; notrun++) {
        if (data[pos-1] == 0) {
          AdaptiveHuffman_EncodeSymbol(code_tree[0], strm, data[pos++]);
        } else if (data[pos-1] <= 2) {
          AdaptiveHuffman_EncodeSymbol(code_tree[1], strm, data[pos++]);
        } else if (log2ceil(data[pos-1]) <= 4) {
            AdaptiveHuffman_EncodeSymbol(
                code_tree[log2ceil(data[pos-1])] , strm, data[pos++]);
        } else {
          AdaptiveHuffman_EncodeSymbol(code_tree[5], strm, data[pos++]);
        }
      }
    } else {
      /* ラン出力モード */
      /* 実際に登録するラン長さはランレングス+1 */
      runlength++;
      // printf("run:%d \n", runlength);
      /* ランレングスだけ進むのは確定なので先に進めておく */
      pos += runlength;
      /* ランレングス出力 */
      while (runlength >= MAXLENGTH) {
        AdaptiveHuffman_EncodeSymbol(tree, strm, MAXLENGTH);
        run_array[MAXLENGTH]++;
        /* ラン継続符号を出力 */
        AdaptiveHuffman_EncodeSymbol(tree, strm, 0);
        run_array[0]++;
        runlength -= MAXLENGTH;
      }
      AdaptiveHuffman_EncodeSymbol(tree, strm, runlength);
      run_array[runlength]++;
    }
  }

  /* ランの分布 */
  {
    uint32_t i;
    for (i = 0; i <= MAXLENGTH; i++) {
      // printf("%d %d \n", i, run_array[i]);
    }
  }

  AdaptiveHuffmanTree_Destroy(code_tree[0]);
  AdaptiveHuffmanTree_Destroy(code_tree[1]);
  AdaptiveHuffmanTree_Destroy(code_tree[2]);
  AdaptiveHuffmanTree_Destroy(code_tree[3]);
  AdaptiveHuffmanTree_Destroy(code_tree[4]);
  AdaptiveHuffmanTree_Destroy(code_tree[5]);
  AdaptiveHuffmanTree_Destroy(tree);
}

/* Mineo版Pack Bitsによる復号 */
void MineoPackBits_Decode(struct BitStream* strm,
    uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t length_bits)
{
  uint32_t      pos, i;
  PackBitsMode  mode;
  uint32_t      length;
  uint64_t      bitsbuf;
  uint32_t      headtailrun_bits, head_run, tail_run;
  struct AdaptiveHuffmanTree* tree = AdaptiveHuffmanTree_Create(length_bits);
  struct AdaptiveHuffmanTree* code_tree[6];
  
  code_tree[0] = AdaptiveHuffmanTree_Create(9);
  code_tree[1] = AdaptiveHuffmanTree_Create(9);
  code_tree[2] = AdaptiveHuffmanTree_Create(9);
  code_tree[3] = AdaptiveHuffmanTree_Create(9);
  code_tree[4] = AdaptiveHuffmanTree_Create(9);
  code_tree[5] = AdaptiveHuffmanTree_Create(9);

  assert(data != NULL);

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
  // printf("head:%d tail:%d \n", head_run, tail_run);

  /* 開始位置とデータ数の切り替え */
  pos = head_run;
  num_data = num_data - tail_run;

  /* 最初のモード取得 */
  mode = BitStream_GetBit(strm);

  /* 最初のモード切り替えで切り替わるので一回反転する */
  mode = (mode == PACKBITS_MODE_RUN) ?
    PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
  while (pos < num_data) {
    /* ラン/非ラン長を取得 */
    AdaptiveHuffman_DecodeSymbol(tree, strm, &length);
    if (length == 0) {
      /* モード継続: 長さを再取得 */
      AdaptiveHuffman_DecodeSymbol(tree, strm, &length);
    } else {
      /* モード切り替え */
      mode = (mode == PACKBITS_MODE_RUN) ?
        PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
    }
    switch (mode) {
      case PACKBITS_MODE_NOT_RUN:
        for (i = 0; i < length; i++) {
          /* 直前の画素に合わせて使用する木を切り替える */
          if (data[pos-1] == 0) {
            AdaptiveHuffman_DecodeSymbol(code_tree[0], strm, &data[pos++]);
          } else if (data[pos-1] <= 2) {
            AdaptiveHuffman_DecodeSymbol(code_tree[1], strm, &data[pos++]);
          } else if (log2ceil(data[pos-1]) <= 4) {
            AdaptiveHuffman_DecodeSymbol(
                code_tree[log2ceil(data[pos-1])] , strm, &data[pos++]);
          } else {
            AdaptiveHuffman_DecodeSymbol(code_tree[5], strm, &data[pos++]);
          }
        }
        break;
      case PACKBITS_MODE_RUN:
        for (i = 0; i < length; i++) {
          data[pos++] = 0;
        }
        break;
      default:
        assert(0);
    }
  } 

  AdaptiveHuffmanTree_Destroy(code_tree[0]);
  AdaptiveHuffmanTree_Destroy(code_tree[1]);
  AdaptiveHuffmanTree_Destroy(code_tree[2]);
  AdaptiveHuffmanTree_Destroy(code_tree[3]);
  AdaptiveHuffmanTree_Destroy(code_tree[4]);
  AdaptiveHuffmanTree_Destroy(code_tree[5]);
  AdaptiveHuffmanTree_Destroy(tree);
}

/* Mineo版Pack Bitsによる符号化(LZSS) */
void MineoPackBitsLZSS_Encode(struct BitStream* strm,
    const uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t threshould, uint32_t length_bits)
{
  uint32_t pos;
  uint32_t runlength;
  uint32_t head_run, tail_run;
  uint8_t  headtailrun_bits;
  const uint32_t MAXLENGTH = (1UL << length_bits) - 1;   /* 表現可能な最大長 */
  struct AdaptiveHuffmanTree* tree = AdaptiveHuffmanTree_Create(length_bits);
  struct NaiveLZSS* lzss = NaiveLZSS_Create(9, 8192, 3, 18);
  uint32_t *encode_data = malloc(sizeof(uint32_t) * 10000);
  uint32_t encode_data_pos = 0;
  
  /* 先頭の0と末尾の0長さを記録 */
  /* 先頭 */
  head_run = 0;
  if (data[0] == 0) {
    head_run = PackBits_GetRunLength(data, 0, num_data);
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
    headtailrun_bits = log2ceil(MAX(head_run, tail_run));
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
  runlength = PackBits_GetRunLength(data, pos, num_data);
  if (runlength < threshould || data[pos] != 0) {
    BitStream_PutBit(strm, PACKBITS_MODE_NOT_RUN);
  } else {
    BitStream_PutBit(strm, PACKBITS_MODE_RUN);
  }

  while (pos < num_data) {
    /* ランレングス取得 */
    runlength = PackBits_GetRunLength(data, pos, num_data);
    if (data[pos] != 0 || runlength < threshould) {
      /* 非ラン出力モード */
      uint32_t notrunlength, notrun, tmp_pos;
      /* 非ランの出力回数を取得 */
      notrunlength = 0;
      tmp_pos = pos;
      while (((PackBits_GetRunLength(data, tmp_pos, num_data) < threshould)
              || (data[tmp_pos] != 0))
              && (tmp_pos < num_data)) {
        notrunlength++; tmp_pos++;
      }

      /* 非ラン分出力 */
      while (notrunlength >= MAXLENGTH) {
        AdaptiveHuffman_EncodeSymbol(tree, strm, MAXLENGTH);
        for (notrun = 0; notrun < MAXLENGTH; notrun++) {
          encode_data[encode_data_pos++] = data[pos++];
        }
        /* 非ラン継続符号を出力 */
        AdaptiveHuffman_EncodeSymbol(tree, strm, 0);
        run_array[0]++;
        notrunlength -= MAXLENGTH;
      }
      AdaptiveHuffman_EncodeSymbol(tree, strm, notrunlength);
      for (notrun = 0; notrun < notrunlength; notrun++) {
        encode_data[encode_data_pos++] = data[pos++];
      }
    } else {
      /* ラン出力モード */
      /* 実際に登録するラン長さはランレングス+1 */
      runlength++;
      // printf("run:%d \n", runlength);
      /* ランレングスだけ進むのは確定なので先に進めておく */
      pos += runlength;
      /* ランレングス出力 */
      while (runlength >= MAXLENGTH) {
        AdaptiveHuffman_EncodeSymbol(tree, strm, MAXLENGTH);
        run_array[MAXLENGTH]++;
        /* ラン継続符号を出力 */
        AdaptiveHuffman_EncodeSymbol(tree, strm, 0);
        run_array[0]++;
        runlength -= MAXLENGTH;
      }
      AdaptiveHuffman_EncodeSymbol(tree, strm, runlength);
      run_array[runlength]++;
    }
  }

  NaiveLZSS_Encode(lzss, strm, encode_data, encode_data_pos);

  free(encode_data);
  NaiveLZSS_Destroy(lzss);
  AdaptiveHuffmanTree_Destroy(tree);
}

/* Mineo版Pack Bitsによる復号(LZSS) */
void MineoPackBitsLZSS_Decode(struct BitStream* strm,
    uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t length_bits)
{
  uint32_t      pos, i;
  PackBitsMode  mode;
  uint32_t      length;
  uint64_t      bitsbuf;
  uint32_t      headtailrun_bits, head_run, tail_run;
  struct AdaptiveHuffmanTree* tree = AdaptiveHuffmanTree_Create(length_bits);
  struct NaiveLZSS* lzss = NaiveLZSS_Create(9, 8192, 3, 18);
  uint32_t *decode_data = malloc(sizeof(uint32_t) * 10000);
  uint32_t decode_data_pos = 0;

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
  // printf("head:%d tail:%d \n", head_run, tail_run);

  /* 開始位置とデータ数の切り替え */
  pos = head_run;
  num_data = num_data - tail_run;

  /* 最初のモード取得 */
  mode = BitStream_GetBit(strm);

  /* 最初のモード切り替えで切り替わるので一回反転する */
  mode = (mode == PACKBITS_MODE_RUN) ?
    PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
  while (pos < num_data) {
    /* ラン/非ラン長を取得 */
    AdaptiveHuffman_DecodeSymbol(tree, strm, &length);
    if (length == 0) {
      /* モード継続: 長さを再取得 */
      AdaptiveHuffman_DecodeSymbol(tree, strm, &length);
    } else {
      /* モード切り替え */
      mode = (mode == PACKBITS_MODE_RUN) ?
        PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
    }
    switch (mode) {
      case PACKBITS_MODE_NOT_RUN:
        /* データは埋めずに設定位置だけ動かす */
        pos += length;
        decode_data_pos += length;
        break;
      case PACKBITS_MODE_RUN:
        for (i = 0; i < length; i++) {
          data[pos++] = 0;
        }
        break;
      default:
        assert(0);
    }
  } 

  /* LZSSデコード */
  NaiveLZSS_Decode(lzss, strm, decode_data, decode_data_pos);

  decode_data_pos = 0;
  for (pos = 0; pos < num_data; pos++) {
    if (data[pos] != 0) {
      data[pos] = decode_data[decode_data_pos++];
    }
  }

  free(decode_data);
  NaiveLZSS_Destroy(lzss);
  AdaptiveHuffmanTree_Destroy(tree);

}

/* Mineo版Pack Bitsによる符号化(LZAH) */
void MineoPackBitsLZAH_Encode(struct BitStream* strm,
    const uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t threshould, uint32_t length_bits)
{
  uint32_t pos;
  uint32_t runlength;
  uint32_t head_run, tail_run;
  uint8_t  headtailrun_bits;
  const uint32_t MAXLENGTH = (1UL << length_bits) - 1;   /* 表現可能な最大長 */
  struct AdaptiveHuffmanTree* tree = AdaptiveHuffmanTree_Create(length_bits);
  struct LZAH* lzah = LZAH_Create(9, 1UL << 18, 4, 259);
  uint32_t *encode_data = malloc(sizeof(uint32_t) * 20000);
  uint32_t encode_data_pos = 0;
  
  /* 先頭の0と末尾の0長さを記録 */
  /* 先頭 */
  head_run = 0;
  if (data[0] == 0) {
    head_run = PackBits_GetRunLength(data, 0, num_data);
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
    headtailrun_bits = log2ceil(MAX(head_run, tail_run));
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
  runlength = PackBits_GetRunLength(data, pos, num_data);
  if (runlength < threshould || data[pos] != 0) {
    BitStream_PutBit(strm, PACKBITS_MODE_NOT_RUN);
  } else {
    BitStream_PutBit(strm, PACKBITS_MODE_RUN);
  }

  while (pos < num_data) {
    /* ランレングス取得 */
    runlength = PackBits_GetRunLength(data, pos, num_data);
    if (data[pos] != 0 || runlength < threshould) {
      /* 非ラン出力モード */
      uint32_t notrunlength, notrun, tmp_pos;
      /* 非ランの出力回数を取得 */
      notrunlength = 0;
      tmp_pos = pos;
      while (((PackBits_GetRunLength(data, tmp_pos, num_data) < threshould)
              || (data[tmp_pos] != 0))
              && (tmp_pos < num_data)) {
        notrunlength++; tmp_pos++;
      }

      /* 非ラン分出力 */
      while (notrunlength >= MAXLENGTH) {
        AdaptiveHuffman_EncodeSymbol(tree, strm, MAXLENGTH);
        for (notrun = 0; notrun < MAXLENGTH; notrun++) {
          encode_data[encode_data_pos++] = data[pos++];
        }
        /* 非ラン継続符号を出力 */
        AdaptiveHuffman_EncodeSymbol(tree, strm, 0);
        run_array[0]++;
        notrunlength -= MAXLENGTH;
      }
      AdaptiveHuffman_EncodeSymbol(tree, strm, notrunlength);
      for (notrun = 0; notrun < notrunlength; notrun++) {
        encode_data[encode_data_pos++] = data[pos++];
      }
    } else {
      /* ラン出力モード */
      /* 実際に登録するラン長さはランレングス+1 */
      runlength++;
      // printf("run:%d \n", runlength);
      /* ランレングスだけ進むのは確定なので先に進めておく */
      pos += runlength;
      /* ランレングス出力 */
      while (runlength >= MAXLENGTH) {
        AdaptiveHuffman_EncodeSymbol(tree, strm, MAXLENGTH);
        run_array[MAXLENGTH]++;
        /* ラン継続符号を出力 */
        AdaptiveHuffman_EncodeSymbol(tree, strm, 0);
        run_array[0]++;
        runlength -= MAXLENGTH;
      }
      AdaptiveHuffman_EncodeSymbol(tree, strm, runlength);
      run_array[runlength]++;
    }
  }

  for (pos = 0; pos < encode_data_pos; pos++) {
    LZAH_EncodeSymbol(lzah, strm, encode_data[pos]);
  }
  LZAH_FinishEncode(lzah, strm);

  free(encode_data);
  LZAH_Destroy(lzah);
  AdaptiveHuffmanTree_Destroy(tree);
}

/* Mineo版Pack Bitsによる復号(LZAH) */
void MineoPackBitsLZAH_Decode(struct BitStream* strm,
    uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t length_bits)
{
  uint32_t      pos, i;
  PackBitsMode  mode;
  uint32_t      length;
  uint64_t      bitsbuf;
  uint32_t      headtailrun_bits, head_run, tail_run;
  struct AdaptiveHuffmanTree* tree = AdaptiveHuffmanTree_Create(length_bits);
  struct LZAH* lzah = LZAH_Create(9, 1UL << 18, 4, 259);
  uint32_t *decode_data = malloc(sizeof(uint32_t) * 20000);
  uint32_t decode_data_pos = 0;

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
  // printf("head:%d tail:%d \n", head_run, tail_run);

  /* 開始位置とデータ数の切り替え */
  pos = head_run;
  num_data = num_data - tail_run;

  /* 最初のモード取得 */
  mode = BitStream_GetBit(strm);

  /* 最初のモード切り替えで切り替わるので一回反転する */
  mode = (mode == PACKBITS_MODE_RUN) ?
    PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
  while (pos < num_data) {
    /* ラン/非ラン長を取得 */
    AdaptiveHuffman_DecodeSymbol(tree, strm, &length);
    if (length == 0) {
      /* モード継続: 長さを再取得 */
      AdaptiveHuffman_DecodeSymbol(tree, strm, &length);
    } else {
      /* モード切り替え */
      mode = (mode == PACKBITS_MODE_RUN) ?
        PACKBITS_MODE_NOT_RUN : PACKBITS_MODE_RUN;
    }
    switch (mode) {
      case PACKBITS_MODE_NOT_RUN:
        /* データは埋めずに設定位置だけ動かす */
        pos += length;
        decode_data_pos += length;
        break;
      case PACKBITS_MODE_RUN:
        for (i = 0; i < length; i++) {
          data[pos++] = 0;
        }
        break;
      default:
        assert(0);
    }
  } 

  /* LZAHデコード */
  for (pos = 0; pos < decode_data_pos; pos++) {
    LZAH_DecodeSymbol(lzah, strm, &decode_data[pos]);
  }

  decode_data_pos = 0;
  for (pos = 0; pos < num_data; pos++) {
    if (data[pos] != 0) {
      data[pos] = decode_data[decode_data_pos++];
    }
  }

  free(decode_data);
  LZAH_Destroy(lzah);
  AdaptiveHuffmanTree_Destroy(tree);

}

/* Mineo版Pack Bitsによる符号化(HORC) */
void MineoPackBitsHORC_Encode(struct BitStream* strm,
    const uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t threshould, uint32_t length_bits)
{
  uint32_t pos;
  uint32_t runlength;
  uint32_t head_run, tail_run;
  uint8_t  headtailrun_bits;
  const uint32_t MAXLENGTH = (1UL << length_bits) - 1;   /* 表現可能な最大長 */
  /* 
  struct AdaptiveHuffmanTree* run_tree = AdaptiveHuffmanTree_Create(length_bits);
  struct AdaptiveHuffmanTree* notrun_tree = AdaptiveHuffmanTree_Create(length_bits);
  */
  struct AdaptiveHuffmanTree* run_tree;
  struct AdaptiveHuffmanTree* notrun_tree;
  struct HORC* horc = HORC_Create(1, 9);
  uint32_t *encode_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t *run_data    = malloc(sizeof(uint32_t) * num_data);
  uint32_t *notrun_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t encode_data_pos = 0;
  uint32_t run_data_pos = 0, notrun_data_pos = 0;
  uint32_t max_runlength = 0, max_notrunlength = 0, runlength_bits, notrunlength_bits;
  int32_t  prev_run, run_pos;
  
  /* 先頭の0と末尾の0長さを記録 */
  /* 先頭 */
  head_run = 0;
  if (data[0] == 0) {
    head_run = PackBits_GetRunLength(data, 0, num_data);
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
    headtailrun_bits = log2ceil(MAX(head_run, tail_run));
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
  runlength = PackBits_GetRunLength(data, pos, num_data);
  if (runlength < threshould || data[pos] != 0) {
    BitStream_PutBit(strm, PACKBITS_MODE_NOT_RUN);
  } else {
    BitStream_PutBit(strm, PACKBITS_MODE_RUN);
  }

  while (pos < num_data) {
    /* ランレングス取得 */
    runlength = PackBits_GetRunLength(data, pos, num_data);
    if (data[pos] != 0 || runlength < threshould) {
      /* 非ラン出力モード */
      uint32_t notrunlength, notrun, tmp_pos;
      /* 非ランの出力回数を取得 */
      notrunlength = 0;
      tmp_pos = pos;
      while (((PackBits_GetRunLength(data, tmp_pos, num_data) < threshould)
              || (data[tmp_pos] != 0))
              && (tmp_pos < num_data)) {
        notrunlength++; tmp_pos++;
      }
      // printf("notrun:%d \n", notrunlength);

      notrun_data[notrun_data_pos++] = notrunlength;
      if (max_notrunlength < notrunlength) {
        max_notrunlength = notrunlength;
      }
      /* 非ラン分出力 */
      /*
      while (notrunlength >= MAXLENGTH) {
        AdaptiveHuffman_EncodeSymbol(notrun_tree, strm, MAXLENGTH);
        for (notrun = 0; notrun < MAXLENGTH; notrun++) {
          encode_data[encode_data_pos++] = data[pos++];
        }
        notrunlength -= MAXLENGTH;
      }
      AdaptiveHuffman_EncodeSymbol(notrun_tree, strm, notrunlength);
      */
      for (notrun = 0; notrun < notrunlength; notrun++) {
        encode_data[encode_data_pos++] = data[pos++];
      }
    } else {
      /* ラン出力モード */
      /* 実際に登録するラン長さはランレングス+1 */
      runlength++;
      // printf("run:%d \n", runlength);
      /* ランレングスだけ進むのは確定なので先に進めておく */
      pos += runlength;
      run_data[run_data_pos++] = runlength;
      if (max_runlength < runlength) {
        max_runlength = runlength;
      }
      /* ランレングス出力 */
    }
  }

  /* ラン/非ランの差分エンコーディング */
  runlength_bits = log2ceil(max_runlength);
  BitStream_PutBits(strm, 16, run_data_pos);
  BitStream_PutBits(strm, 5, runlength_bits);
  /* 差分を取るため1bit付加する */
  run_tree    = AdaptiveHuffmanTree_Create(runlength_bits + 1); 
  BitStream_PutBits(strm, runlength_bits, run_data[0]);
  for (run_pos = 1; run_pos < run_data_pos; run_pos++) {
    AdaptiveHuffman_EncodeSymbol(run_tree, strm,
        SINT32_TO_UINT32((int32_t)run_data[run_pos-1] - (int32_t)run_data[run_pos]));
  }

  notrunlength_bits = log2ceil(max_notrunlength);
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

/* Mineo版Pack Bitsによる復号(HORC) */
void MineoPackBitsHORC_Decode(struct BitStream* strm,
    uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t length_bits)
{
  uint32_t      pos, i;
  PackBitsMode  mode;
  uint32_t      length;
  uint64_t      bitsbuf;
  uint32_t      headtailrun_bits, head_run, tail_run;
  /*
  struct AdaptiveHuffmanTree* run_tree = AdaptiveHuffmanTree_Create(length_bits);
  struct AdaptiveHuffmanTree* notrun_tree = AdaptiveHuffmanTree_Create(length_bits);
  */
  const uint32_t MAXLENGTH = (1UL << length_bits) - 1;   /* 表現可能な最大長 */
  struct HORC* horc = HORC_Create(1, 9);
  uint32_t *decode_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t decode_data_pos = 0;

  struct AdaptiveHuffmanTree* run_tree;
  struct AdaptiveHuffmanTree* notrun_tree;
  uint32_t *run_data    = malloc(sizeof(uint32_t) * num_data);
  uint32_t *notrun_data = malloc(sizeof(uint32_t) * num_data);
  uint32_t run_data_pos = 0, notrun_data_pos = 0;
  uint32_t max_runlength = 0, max_notrunlength = 0, runlength_bits, notrunlength_bits;
  int32_t  prev_run, run_pos;

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
  // printf("head:%d tail:%d \n", head_run, tail_run);

  /* 開始位置とデータ数の切り替え */
  pos = head_run;
  num_data = num_data - tail_run;

  /* 最初のモード取得 */
  mode = BitStream_GetBit(strm);

  /* ラン/非ラン情報のデコード */
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

  /*
  for (run_pos = 100; run_pos < 110; run_pos++) {
    printf("D: %d Run(bits:%d num:%d):%d NotRun(bits:%d num:%d):%d \n", run_pos,
        runlength_bits, run_data_pos, run_data[run_pos],
        notrunlength_bits, notrun_data_pos, notrun_data[run_pos]);
  }
  */

  run_data_pos = notrun_data_pos = 0;
  while (pos < num_data) {
    switch (mode) {
      case PACKBITS_MODE_NOT_RUN:
        // do  {
          // AdaptiveHuffman_DecodeSymbol(run_tree, strm, &length);
          length = notrun_data[notrun_data_pos++];
          /* データは埋めずに設定位置だけ動かす */
          pos += length;
          decode_data_pos += length;
        // } while (length == MAXLENGTH);
        break;
      case PACKBITS_MODE_RUN:
        // do {
          // AdaptiveHuffman_DecodeSymbol(notrun_tree, strm, &length);
          length = run_data[run_data_pos++];
          /* ランは0が埋まっている */
          for (i = 0; i < length; i++) {
            data[pos++] = 0;
          }
        // } while (length == MAXLENGTH);
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

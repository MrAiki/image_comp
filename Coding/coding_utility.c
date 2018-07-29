#include "coding_utility.h"
#include "../../BitStream/bit_stream.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define GET_INT_SIGN(val) (((val) > 0) ? 1 : 0)
#define GET_INT_ABS(val)  (((val) > 0) ? (val) : (-(val)))

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
uint32_t CodingUtility_Log2ceil(uint32_t val)
{
  return 32 - nlz5(val - 1);
}

/* 符号部の長さを決定 */
static uint32_t SignedWyle_GetLength(uint32_t abs)
{
  if (abs < 8) {
    return 1;
  }
  
  return CodingUtility_Log2ceil(abs) - 1;
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
    BitStream_PutBits(strm, CodingUtility_Log2ceil(m), rest);
    return;
  }

  /* ゴロム符号化 */
  b = CodingUtility_Log2ceil(m);
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
    BitStream_GetBits(strm, CodingUtility_Log2ceil(m), &rest);
    return (uint32_t)(quot * m + rest);
  }

  /* ゴロム符号化 */
  b = CodingUtility_Log2ceil(m);
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
  const uint32_t MAXLENGTH = (1UL << length_bits) - 1;   /* 表現可能な最大長 */

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

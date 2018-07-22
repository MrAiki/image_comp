#include "naive_lzss.h"
#include "../../BitStream/bit_stream.h"

#include <stdlib.h>
#include <string.h>

/* 2の冪乗判定 */
#define IS_POWER_OF_2(val) (!((val) & ((val)-1)))

/* 窓サイズが2の冪乗であることを前提に、MOD */
#define SLIDEWINDOW_MOD(lzss, pos) ((pos) & (((lzss)->window_size) - 1))

/* baseとpos間の距離取得 */
#define SLIDEWINDOW_DISTANCE(lzss, base, pos) (((base) >= (pos)) ? ((base) - (pos)) : ((base) + ((lzss)->window_size) - (pos)))

/* 距離とベース位置からposを復帰 */
/* 実はSLIDEWINDOW_DISTANCEと同一だがわかりやすさのため分けた */
#define SLIDEWINDOW_DISTANCE_TO_POS(lzss, base, distance) ((base) >= (distance)) ? ((base) - (distance)) : ((base) + ((lzss)->window_size) - (distance))

/* 2つの値のうち最小値を取る */
#define MIN(a,b) ((a < b) ? (a) : (b))

/* ナイーブなLZSSハンドル */
struct NaiveLZSS {
  uint32_t symbol_bits;           /* 1シンボルのビット幅 */
  uint32_t matchlen_bits;         /* 先読み部のビット幅 */
  uint32_t position_bits;         /* スライド窓長のビット幅 */
  uint32_t *slide_window;         /* スライド窓 */
  uint32_t window_size;           /* スライド窓長 */
  uint32_t insert_pos;            /* 窓への挿入位置 */
  uint32_t search_begin_pos;      /* 探索開始位置 */
  uint32_t lookahead_begin_pos;   /* 符号化部先端位置 */
  uint32_t min_match_len;         /* 最小の一致長 */
  uint32_t max_match_len;         /* 最大の一致長 */
};

/* シンボルを挿入し、先読みバッファとスライド窓を更新 */
static void NaiveLZSS_InsertSymbol(
    struct NaiveLZSS* lzss, uint32_t symbol);

/* 最長一致位置と長さの探索 */
static void NaiveLZSS_SearchMatchPosition(
    const struct NaiveLZSS* lzss,
    uint32_t* match_pos, uint32_t* match_len);

/* ceil(log2(val)) を計算する */
static uint32_t log2ceil(uint32_t val);

/* デバッグ用表示関数 */
/* デバッグが死ぬほど辛かったので残しておく */
static void debug_print_window(struct NaiveLZSS* lzss);

/* マルチビットLZSSハンドル */
struct NaiveLZSS* NaiveLZSS_Create(
    uint32_t symbol_bits, uint32_t window_size,
    uint32_t min_match_len, uint32_t max_match_len)
{
  struct NaiveLZSS* lzss;

  if (symbol_bits == 0
      || !IS_POWER_OF_2(window_size)
      || min_match_len > max_match_len) {
    return NULL;
  }

  lzss = malloc(sizeof(struct NaiveLZSS));
  lzss->symbol_bits       = symbol_bits;
  lzss->window_size       = window_size;
  lzss->min_match_len     = min_match_len;
  lzss->max_match_len     = max_match_len;
  lzss->matchlen_bits     = log2ceil(max_match_len - min_match_len + 1);
  lzss->position_bits     = log2ceil(window_size);

  lzss->slide_window      = malloc(sizeof(uint32_t) * window_size);

  NaiveLZSS_Initialize(lzss);

  return lzss;
}

/* スライド窓初期化 */
void NaiveLZSS_Initialize(struct NaiveLZSS* lzss)
{
  uint32_t i;

  /* スライド窓のクリア */
  for (i = 0; i < lzss->window_size; i++) {
    lzss->slide_window[i] = 0;
  }

  /* 窓参照位置の初期化 */
  lzss->insert_pos          = 0;
  lzss->lookahead_begin_pos = 0;
  lzss->search_begin_pos    = 0;
}

/* マルチビットLZSSハンドル破棄 */
void NaiveLZSS_Destroy(struct NaiveLZSS* lzss)
{
  if (lzss != NULL) {
    if (lzss->slide_window != NULL) {
      free(lzss->slide_window);
    }
    free(lzss);
  }
}

/* エンコード処理 */
NaiveLZSSApiResult NaiveLZSS_Encode(
    struct NaiveLZSS* lzss, struct BitStream* strm,
    const uint32_t* data, uint32_t num_data)
{
  uint32_t data_pos, pos;
  uint32_t match_pos, match_len;

  if (lzss == NULL || strm == NULL) {
    return NAIVE_LZSS_APIRESULT_NG;
  }

  /* 符号化部が一杯になるまでデータを読み込む */
  for (data_pos = 0; data_pos < lzss->max_match_len; data_pos++) {
    NaiveLZSS_InsertSymbol(lzss, data[data_pos]);
  }
  /* この時点で先読み部を先頭から始めることで、符号化部一杯にデータが
   * 入っている状態になる */
  lzss->lookahead_begin_pos = 0;

  while (data_pos < num_data + lzss->max_match_len) {
    /* 一致位置と長さの探索 */
    NaiveLZSS_SearchMatchPosition(lzss, &match_pos, &match_len);

    if (match_len < lzss->min_match_len) {
      /* 最小一致長未満ならば、0の後にシンボルそのままの値を出力 */
      // printf("EncNotMatch: out:%d \n", lzss->slide_window[lzss->lookahead_begin_pos]); 
      BitStream_PutBit(strm, 0);
      BitStream_PutBits(strm,
          lzss->symbol_bits, lzss->slide_window[lzss->lookahead_begin_pos]);
      /* 一致長は1に */
      match_len = 1;
    } else {
      /* 最小一致長以上ならば、1, 一致長-最小一致長, 一致位置までの距離を出力 */
      // printf("EncMatch: pos:%d len:%d \n", match_pos, match_len); 
      BitStream_PutBit(strm, 1);
      BitStream_PutBits(strm, lzss->matchlen_bits, match_len - lzss->min_match_len);
      BitStream_PutBits(strm, lzss->position_bits, SLIDEWINDOW_DISTANCE(lzss, lzss->lookahead_begin_pos, match_pos));
    }

    /* 一致長だけシンボルを挿入 */
    for (pos = 0; pos < match_len; pos++) {
      NaiveLZSS_InsertSymbol(lzss, data[data_pos++]);
      /* 読み取り位置がデータ長を超えていたら戻し、
       * 符号化部がデータの外を見ないようにする */
      if (data_pos > num_data) {
        lzss->insert_pos--;
      }
    }

  }

  return NAIVE_LZSS_APIRESULT_OK;
}

/* デコード処理 */
NaiveLZSSApiResult NaiveLZSS_Decode(
    struct NaiveLZSS* lzss, struct BitStream* strm,
    uint32_t* data, uint32_t num_data)
{
  uint64_t bitsbuf;
  uint32_t pos, data_pos;
  uint32_t match_pos, match_len;

  if (lzss == NULL || strm == NULL || data == NULL) {
    return NAIVE_LZSS_APIRESULT_NG;
  }

  data_pos = 0;
  while (data_pos < num_data) {
    if (BitStream_GetBit(strm) == 1) {
      /* 一致長, 一致位置を順次デコード */
      BitStream_GetBits(strm, lzss->matchlen_bits, &bitsbuf);
      match_len = (uint32_t)bitsbuf + lzss->min_match_len;
      BitStream_GetBits(strm, lzss->position_bits, &bitsbuf);
      match_pos = SLIDEWINDOW_DISTANCE_TO_POS(lzss, lzss->lookahead_begin_pos, (uint32_t)bitsbuf);
      // printf("DecMatch: pos:%d len:%d \n", match_pos, match_len); 
      /* マッチ分だけ登録 */
      for (pos = 0; pos < match_len; pos++) {
        if (data_pos < num_data) {
          data[data_pos++] 
            = lzss->slide_window[SLIDEWINDOW_MOD(lzss, match_pos + pos)];
          // printf("DecMatch: get:%d \n", data[data_pos-1]); 
        }
        NaiveLZSS_InsertSymbol(lzss,
            lzss->slide_window[SLIDEWINDOW_MOD(lzss, match_pos + pos)]);
      }
    } else {
      /* そのままの値を読み出し */
      BitStream_GetBits(strm, lzss->symbol_bits, &bitsbuf);
      NaiveLZSS_InsertSymbol(lzss, (uint32_t)bitsbuf);
      data[data_pos++] = (uint32_t)bitsbuf;
      // printf("DecNotMatch: get:%d \n", data[data_pos-1]); 
    }
  }

  return NAIVE_LZSS_APIRESULT_OK;
}

/* シンボルを挿入 */
static void NaiveLZSS_InsertSymbol(
    struct NaiveLZSS* lzss, uint32_t symbol)
{
  /* スライド窓にシンボルを追加 */
  lzss->slide_window[lzss->insert_pos] = symbol;

  /* スライド窓の挿入位置更新 */
  lzss->insert_pos = SLIDEWINDOW_MOD(lzss, ++(lzss->insert_pos));

  /* 探索開始位置の更新 */
  /* 窓が一杯にならない限り動かさない */
  if (lzss->insert_pos == lzss->search_begin_pos) {
    lzss->search_begin_pos  = SLIDEWINDOW_MOD(lzss, ++(lzss->search_begin_pos));
  }

  /* 符号化部先端の更新 */
  lzss->lookahead_begin_pos = SLIDEWINDOW_MOD(lzss, ++(lzss->lookahead_begin_pos));
}

/* 最長一致位置と長さの探索 */
/* 超ナイーブ実装。 */
static void NaiveLZSS_SearchMatchPosition(
    const struct NaiveLZSS* lzss,
    uint32_t* match_pos, uint32_t* match_len)
{
  uint32_t pos, l_pos, end_pos;
  uint32_t tmp_len;
  uint32_t max_pos, max_len;

  /* 探索開始位置 */
  pos		  = lzss->search_begin_pos;
  /* 符号化部開始位置 */
  l_pos		= lzss->lookahead_begin_pos;
  /* 探索終了位置 */
  end_pos	= SLIDEWINDOW_MOD(lzss, l_pos + lzss->min_match_len - 1);

  max_len = 0;
  max_pos = pos;
  while (pos != end_pos) {
    /* 先頭がマッチしているかチェック */
    if (lzss->slide_window[l_pos] == lzss->slide_window[pos]) {
      /* 一致長の取得 */
      tmp_len = 1;
      while ((tmp_len < lzss->max_match_len)
          && (SLIDEWINDOW_MOD(lzss, pos + tmp_len) != end_pos)
          && (lzss->slide_window[SLIDEWINDOW_MOD(lzss, l_pos + tmp_len)] 
              == lzss->slide_window[SLIDEWINDOW_MOD(lzss, pos + tmp_len)])) {
        tmp_len++;
      }

      /* 最大長の更新 */
      if (max_len < tmp_len) {
        max_len = tmp_len;
        max_pos = pos;
        /* 最大一致長に達したら、探索を打ち切る */
        if (max_len == lzss->max_match_len) {
          break;
        }
      }
    }
    /* バッファ参照位置の移動 */
    pos++;
    pos = SLIDEWINDOW_MOD(lzss, pos);
  }

  *match_pos = max_pos;
  *match_len = max_len;
}

/* NLZ(上位ビットから1にぶつかるまでの0の数)を計算する(http://www.nminoru.jp/~nminoru/programming/bitcount.html) */
static uint32_t nlz5(uint32_t x)
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

static void debug_print_window(struct NaiveLZSS* lzss)
{
  uint32_t i;
  puts("-----------------------");
  for (i = lzss->search_begin_pos; 
      i != lzss->insert_pos;
      i++, i = SLIDEWINDOW_MOD(lzss, i)) {
    if (i == lzss->lookahead_begin_pos) {
      printf("| ");
    }
    printf("%d ", lzss->slide_window[i]);
  }
  printf("\nsearch_begin_pos:%d lookahead:%d insert_pos:%d \n",
      lzss->search_begin_pos, lzss->lookahead_begin_pos, lzss->insert_pos);
}

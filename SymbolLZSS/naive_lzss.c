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
  uint32_t *symbol_buffer;        /* エンコード/デコードデータの一時バッファ */
  uint32_t symbol_buffer_pos;     /* 一時バッファの参照位置 */
  uint32_t symbol_buffer_remain;  /* 一時バッファの残量 */
  uint32_t is_first_buffering;    /* 最初のバッファリングを終えたか？0:NO, 1:YES */
};

/* シンボルを挿入し、先読みバッファとスライド窓を更新 */
static void NaiveLZSS_InsertSymbol(
    struct NaiveLZSS* lzss, uint32_t symbol);

/* 最長一致位置と長さの探索 */
static void NaiveLZSS_SearchMatchPosition(
    const struct NaiveLZSS* lzss,
    uint32_t* match_pos, uint32_t* match_len);

/* 最長一致位置の探索とエンコード */
static void NaiveLZSS_SearchAndEncode(
    const struct NaiveLZSS* lzss,
    struct BitStream* strm,
    uint32_t* match_pos, uint32_t* match_len);

/* ceil(log2(val)) を計算する */
static uint32_t log2ceil(uint32_t val);

/* デバッグ用表示関数 */
/* デバッグが死ぬほど辛かったので残しておく */
static void debug_print_window(const struct NaiveLZSS* lzss);

/* マルチビットLZSSハンドルの作成 */
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
  lzss->symbol_buffer     = malloc(sizeof(uint32_t) * max_match_len);

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

  /* バッファのクリア */
  for (i = 0; i < lzss->max_match_len; i++) {
    lzss->symbol_buffer[i] = 0;
  }

  /* 窓参照位置の初期化 */
  lzss->insert_pos          = 0;
  lzss->lookahead_begin_pos = 0;
  lzss->search_begin_pos    = 0;

  /* バッファ参照位置の初期化 */
  lzss->symbol_buffer_pos     = 0;
  lzss->symbol_buffer_remain  = 0;
  lzss->is_first_buffering    = 1;
}

/* マルチビットLZSSハンドル破棄 */
void NaiveLZSS_Destroy(struct NaiveLZSS* lzss)
{
  if (lzss != NULL) {
    if (lzss->slide_window != NULL) {
      free(lzss->slide_window);
    }
    if (lzss->symbol_buffer != NULL) {
      free(lzss->symbol_buffer);
    }
    free(lzss);
  }
}

/* エンコード処理 */
NaiveLZSSApiResult NaiveLZSS_EncodeSymbol(
    struct NaiveLZSS* lzss, struct BitStream* strm, uint32_t symbol)
{
  uint32_t match_pos, match_len;

  if (lzss == NULL || strm == NULL) {
    return NAIVE_LZSS_APIRESULT_NG;
  }

  /* 符号化部が一杯になるまでシンボルを挿入 */
  if (lzss->symbol_buffer_pos < lzss->max_match_len) {
    // printf("Insert:%d BufferPos:%d \n", symbol, lzss->symbol_buffer_pos);
    NaiveLZSS_InsertSymbol(lzss, symbol);
    lzss->symbol_buffer_pos++;
    return NAIVE_LZSS_APIRESULT_OK;
  } 

  if (lzss->is_first_buffering == 1) {
    /* この時点で先読み部を先頭から始めることで、符号化部一杯にデータが
     * 入っている状態になる */
    lzss->lookahead_begin_pos = 0;
    lzss->is_first_buffering  = 0;
  }

  /* 一致部分の探索とエンコード */
  NaiveLZSS_SearchAndEncode(lzss, strm, &match_pos, &match_len);

  /* 一致長だけバッファを空ける */
  lzss->symbol_buffer_pos = lzss->max_match_len - match_len;

  /* 今受けとったシンボルを挿入 */
  NaiveLZSS_InsertSymbol(lzss, symbol);
  lzss->symbol_buffer_pos++;

  return NAIVE_LZSS_APIRESULT_OK;
}

/* エンコード終了 */
void NaiveLZSS_FinishEncode(
    struct NaiveLZSS* lzss, struct BitStream* strm)
{
  uint32_t pos;
  uint32_t match_pos, match_len;
  uint32_t remain, outcount;

  // printf("Call Finish BufferPos:%d \n", lzss->symbol_buffer_pos);

  /* 最後の一致情報がエンコード済みなので、その分符号化部を前に進める */
  for (; lzss->symbol_buffer_pos < lzss->max_match_len;
       lzss->symbol_buffer_pos++) {
    lzss->lookahead_begin_pos
      = SLIDEWINDOW_MOD(lzss, ++(lzss->lookahead_begin_pos));
  }

  /* 符号化部に残ったシンボルをエンコード */
  outcount = 0;
  remain = SLIDEWINDOW_DISTANCE(lzss, lzss->insert_pos, lzss->lookahead_begin_pos);
  while (outcount < remain) {
    /* エンコード */
    NaiveLZSS_SearchAndEncode(lzss, strm, &match_pos, &match_len);

    /* 一致分だけ符号化部を進めて符号化部に入っているシンボルを減らす */
    for (pos = 0; pos < match_len; pos++) {
      lzss->lookahead_begin_pos
        = SLIDEWINDOW_MOD(lzss, ++(lzss->lookahead_begin_pos));
      outcount++;
    }
  }
}

/* 最長一致位置の探索とエンコード */
static void NaiveLZSS_SearchAndEncode(
    const struct NaiveLZSS* lzss,
    struct BitStream* strm,
    uint32_t* match_pos, uint32_t* match_len)
{
  uint32_t tmp_match_pos, tmp_match_len;

  /* 一致位置と長さの探索 */
  NaiveLZSS_SearchMatchPosition(lzss, &tmp_match_pos, &tmp_match_len);

  if (tmp_match_len < lzss->min_match_len) {
    /* 最小一致長未満ならば、0の後にシンボルそのままの値を出力 */
    // printf("EncNotMatch: out:%d \n", lzss->slide_window[lzss->lookahead_begin_pos]); 
    BitStream_PutBit(strm, 0);
    BitStream_PutBits(strm,
        lzss->symbol_bits, lzss->slide_window[lzss->lookahead_begin_pos]);
    /* 一致長は1に */
    tmp_match_len = 1;
  } else {
    /* 最小一致長以上ならば、1, 一致長-最小一致長, 一致位置までの距離を出力 */
    // printf("EncMatch: pos:%d len:%d \n", tmp_match_pos, tmp_match_len); 
    BitStream_PutBit(strm, 1);
    BitStream_PutBits(strm, lzss->matchlen_bits, tmp_match_len - lzss->min_match_len);
    BitStream_PutBits(strm, lzss->position_bits, SLIDEWINDOW_DISTANCE(lzss, lzss->lookahead_begin_pos, tmp_match_pos));
  }

  *match_pos = tmp_match_pos;
  *match_len = tmp_match_len;
}

/* デコード処理 */
NaiveLZSSApiResult NaiveLZSS_DecodeSymbol(
    struct NaiveLZSS* lzss, struct BitStream* strm, uint32_t* symbol)
{
  uint64_t bitsbuf;
  uint32_t pos;
  uint32_t match_pos, match_len;

  if (lzss == NULL || strm == NULL || symbol == NULL) {
    return NAIVE_LZSS_APIRESULT_NG;
  }

  /* シンボルバッファから取り出し */
  if (lzss->symbol_buffer_pos < lzss->symbol_buffer_remain) {
    *symbol = lzss->symbol_buffer[lzss->symbol_buffer_pos];
    // printf("GetFromBuffer[%d]:%d \n", lzss->symbol_buffer_pos, *symbol);
    lzss->symbol_buffer_pos++;
    return NAIVE_LZSS_APIRESULT_OK;
  } 
  
  if (BitStream_GetBit(strm) == 1) {
    /* 一致長, 一致位置を順次デコード */
    BitStream_GetBits(strm, lzss->matchlen_bits, &bitsbuf);
    match_len = (uint32_t)bitsbuf + lzss->min_match_len;
    BitStream_GetBits(strm, lzss->position_bits, &bitsbuf);
    match_pos = SLIDEWINDOW_DISTANCE_TO_POS(lzss, lzss->lookahead_begin_pos, (uint32_t)bitsbuf);
    // printf("DecMatch: pos:%d len:%d \n", match_pos, match_len); 
    /* マッチ分だけバッファに貯める */
    for (pos = 0; pos < match_len; pos++) {
      lzss->symbol_buffer[pos]
        = lzss->slide_window[SLIDEWINDOW_MOD(lzss, match_pos + pos)];
      // printf("Buffered[%d]: %d \n", pos, lzss->symbol_buffer[pos]); 
      NaiveLZSS_InsertSymbol(lzss,
          lzss->slide_window[SLIDEWINDOW_MOD(lzss, match_pos + pos)]);
    }
    /* バッファ先頭を出力 */
    *symbol = lzss->symbol_buffer[0];
    lzss->symbol_buffer_pos     = 1;
    lzss->symbol_buffer_remain  = match_len;
  } else {
    /* そのままの値を読み出し。バッファリングしない */
    BitStream_GetBits(strm, lzss->symbol_bits, &bitsbuf);
    NaiveLZSS_InsertSymbol(lzss, (uint32_t)bitsbuf);
    *symbol = (uint32_t)bitsbuf;
    // printf("DecNotMatch: get:%d \n", *symbol); 
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

/* デバッグ用表示関数 */
static void debug_print_window(const struct NaiveLZSS* lzss)
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

#include "horc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define HORC_MAXIMUM_SCALE                ((int32_t)((1UL << 18) - 1))          /* 出現度数の最大値 */
#define HORC_MAX_COUNT                    ((1UL << 13) - 1)                     /* シンボルの出現度数の上限 */
#define HORC_DONE                         (-1)                                  /* ファイル終端を示す記号 */
#define HORC_FLUSH_MODEL                  (-2)                                  /* モデルの吐き出しを表す記号 */
#define HORC_COMPRESSION_RATIO_THRESHOLD  90                                    /* 圧縮率の閾値 */
#define HORC_NUM_SYMBOLS(n_bits)          (1UL << (n_bits))                     /* 通常のシンボル数 */
#define HORC_ESCAPE(n_bits)               ((int32_t)HORC_NUM_SYMBOLS(n_bits))   /* エスケープ文字 */
#define HORC_BOOL_TO_BIT(bool_exp)        ((bool_exp) ? 1 : 0)                  /* 条件式をビットに直す */

/* NULLチェックとfree */
#define NULLCHECK_AND_FREE(obj) { \
  if ((obj) != NULL) {            \
    free(obj);                    \
  }                               \
}

/* リークが多発したのでデバッグ用 */
/* #define MEMORY_DEBUG  1 */

#if defined(MEMORY_DEBUG) 

/* ログ付きfree */
#define HORC_FREE(ptr)  {                 \
  printf("free(%s(%p)); \n", #ptr, ptr);  \
  NULLCHECK_AND_FREE(ptr);                \
}

/* ログ付きcalloc */
#define HORC_CALLOC(ptr, size, num, type)  {  \
  ptr = (type)calloc(size, num);              \
  assert(ptr != NULL);                        \
  printf("%s(%p) <- calloc(%tu,%d); \n",      \
    #ptr, ptr, size, (int)num);               \
}

/* ログ付きrealloc */
#define HORC_REALLOC(ptr, size, type) {       \
  ptr = (type)realloc(ptr, size);             \
  assert(ptr != NULL);                        \
  printf("%s(%p) <- realloc(%p,%tu); \n",     \
    #ptr, ptr, ptr, size);                    \
}

#else

#define HORC_FREE(ptr) \
  NULLCHECK_AND_FREE(ptr)

#define HORC_CALLOC(ptr, size, num, type) \
  ptr = (type)calloc(size, num)

#define HORC_REALLOC(ptr, size, type) \
  ptr = (type)realloc(ptr, size)

#endif

/* レンジコーダの1シンボルの情報 */
struct RangeCoderSymbol {
  uint32_t  low_count;        /* 下限度数       */
  uint32_t  high_count;       /* 上限度数       */
  uint32_t  scale;            /* 度数のスケール */
};

/* 文脈のエントリ */
struct ContextEntry {
  int32_t   symbol;           /* 記号     */
  uint32_t  counts;           /* 出現度数 */
};

/* 上位次数の文脈のリンク */
struct ContextLinks {
  struct Context* next;
};

/* 文脈 */
struct Context {
  int32_t               max_index;        /* linksとentryの要素数-1 */
  struct ContextLinks*  links;            /* 上位文脈のリンク */
  struct ContextEntry*  stats;            /* シンボル出現情報 */
  struct Context*       lesser_context;   /* 下位の文脈       */
};

/* 高次文脈モデルを使用したレンジコーダハンドル */
struct HORC {
  /* 文脈モデル部 */
  uint32_t          symbol_bits;        /* シンボルのビット数 */
  int32_t           max_order;          /* 最大次数 */
  int32_t           current_order;      /* 現在のモデルの次数 */
  struct Context**  contexts;           /* 文脈情報 */
  int32_t*          totals;             /* 現在の文脈における累積度数表 */
  int32_t*          scoreboard;         /* 高次のモデルで出現した記号 */
  /* レンジコーダ部 */
  uint32_t          code;               /* 現在の入力符号 */
  uint32_t          low;                /* 現在の入力区間の下限 */
  uint32_t          high;               /* 現在の入力区間の上限 */
  uint32_t          underflow_bits;     /* アンダーフロービット数 */
  /* 圧縮率管理部 */
  int32_t           input_mark;         /* 入力の進捗 */
  int32_t           output_mark;        /* 出力の進捗 */
};

/* 圧縮率チェック */
static int32_t HORC_CheckCompression(
    struct HORC* horc, int32_t input_pos, struct BitStream* out_stream);

/* 新たな文脈表の作成 */
static struct Context* HORC_AllocateNextOrderTable(
    struct Context* table, int32_t symbol, struct Context* lesser_context);
/* モデルの更新 */
static void HORC_UpdateModel(struct HORC* horc, int32_t symbol);
/* 文脈表の更新 - シンボルの出現度数の更新 */
static void HORC_UpdateTable(struct HORC* horc, struct Context* table, int32_t symbol);
/* 復号器から得た出現度数から、その出現度数に一致する記号を探す */
static int32_t HORC_ConvertSymbolToInt(
    struct HORC* horc, int32_t count, struct RangeCoderSymbol* s);
/* 記号に対するスケール値の取得 */
static void HORC_GetSymbolScale(struct HORC* horc, struct RangeCoderSymbol* s);
/* 記号の符号化 */
static int32_t HORC_ConvertIntToSymbol(
    struct HORC* horc, int32_t c, struct RangeCoderSymbol* s);
/* 新しいシンボルをモデルに追加し、文脈を遷移する */
static void HORC_AddSymbolToModel(struct HORC* horc, int32_t c);
/* 新しいシンボルを追加するコア処理 */
static struct Context* HORC_ShiftToNextContext(
    struct HORC* horc, struct Context* table, int32_t c, int32_t order);
/* 文脈表のスケールダウン処理 */
static void HORC_RescaleTable(struct Context* table);
/* 累積度数表の生成 */
static void HORC_TotalizeTable(
    struct HORC* horc, struct Context* table);
/* モデルの再帰的掃き出し処理 */
static void HORC_RecursiveFlush(struct Context* table);
/* 全ての文脈表を掃き出す */
static void HORC_FlushModel(struct HORC* horc);

/* レンジコーダ初期化 */
static void HORCRangeCoder_InitializeEncode(struct HORC* horc);
/* レンジコーダのエンコード終了 */
static void HORCRangeCoder_FinishEncode(struct HORC* horc, struct BitStream* stream);
/* シンボルのエンコード */
static void HORCRangeCoder_EncodeSymbol(
    struct HORC* horc, struct BitStream* stream, struct RangeCoderSymbol* s);
/* 符号に対する出現度数を得る 返り値を符号のスケールで割ると符号が得られる */
static int32_t HORCRangeCoder_GetCurrentCount(struct HORC* horc, struct RangeCoderSymbol* s);
/* デコーダ初期化 */
static void HORCRangeCoder_InitializeDecode(struct HORC* horc, struct BitStream* stream);
/* シンボルを復号した後のビット読み込み処理 */
static void HORCRangeCoder_RemoveSymbolFromStream(
    struct HORC* horc, struct BitStream* stream, struct RangeCoderSymbol* s);

/* ハンドル作成 */
struct HORC* HORC_Create(int32_t max_order, uint32_t symbol_bits)
{
  int32_t i;
  struct Context* null_table;
  struct Context* control_table;
  struct HORC* horc;

  /* ハンドルの領域割当 */
  horc = (struct HORC *)malloc(sizeof(struct HORC));

  horc->max_order     = max_order;
  horc->current_order = max_order;
  horc->symbol_bits   = symbol_bits;

  /* 文脈の割当て */
  HORC_CALLOC(horc->contexts, sizeof(struct Context *), 10, struct Context **);
  horc->contexts += 2;  /* こうすることで-2次まで扱える */

  /* -1次の文脈作成 */
  HORC_CALLOC(null_table, sizeof(struct Context), 1, struct Context *);
  null_table->max_index = -1;
  horc->contexts[-1]    = null_table;

  for (i = 0; i <= max_order; i++) {
    horc->contexts[i] = HORC_AllocateNextOrderTable(horc->contexts[i-1], 0, horc->contexts[i-1]);
  }
  HORC_FREE(null_table->stats);
  HORC_CALLOC(null_table->stats, sizeof(struct ContextEntry), HORC_NUM_SYMBOLS(symbol_bits), struct ContextEntry *);
  null_table->max_index = HORC_NUM_SYMBOLS(symbol_bits)-1;

  /* 度数は全て1とする */
  for (i = 0; i < (int32_t)HORC_NUM_SYMBOLS(symbol_bits); i++) {
    null_table->stats[i].symbol = i;
    null_table->stats[i].counts = 1;
  }

  /* -2次の文脈作成 */
  HORC_CALLOC(control_table, sizeof(struct Context), 1, struct Context *);
  HORC_CALLOC(control_table->stats, sizeof(struct ContextEntry), 2, struct ContextEntry *);
  horc->contexts[-2]   = control_table;
  control_table->max_index = 1;
  control_table->stats[0].symbol = -HORC_FLUSH_MODEL;
  control_table->stats[0].counts = 1;
  control_table->stats[1].symbol = -HORC_DONE;
  control_table->stats[1].counts = 1;

  HORC_CALLOC(horc->totals, sizeof(int32_t), HORC_NUM_SYMBOLS(symbol_bits) + 2, int32_t *);

  /* 得点表の割当 */
  HORC_CALLOC(horc->scoreboard, sizeof(int32_t), HORC_NUM_SYMBOLS(symbol_bits), int32_t *);
  for (i = 0; i < (int32_t)HORC_NUM_SYMBOLS(symbol_bits); i++) {
    horc->scoreboard[i] = 0;
  }

  /* 圧縮率の初期化 */
  horc->input_mark  = 0;
  horc->output_mark = 0;

  return horc;
}

/* ハンドル破棄 */
void HORC_Destroy(struct HORC* horc)
{
  int32_t order;

  if (horc != NULL) {
    /* 文脈表は-2から始まる */
    for (order = -2; order <= horc->max_order; order++) {
      HORC_FREE(horc->contexts[order]->links);
      HORC_FREE(horc->contexts[order]->stats);
      HORC_FREE(horc->contexts[order]);
    }
    HORC_FREE(&(horc->contexts[-2]));
    HORC_FREE(horc->totals);
    HORC_FREE(horc->scoreboard);
    HORC_FREE(horc);
  }

}

/* エンコード */
HORCApiResult HORC_Encode(
    struct HORC* horc, struct BitStream* strm,
    const uint32_t* data, uint32_t num_data)
{
  struct RangeCoderSymbol s;
  int32_t   is_need_flush, c;
  int32_t   is_escaped;
  uint32_t  data_pos;

  /* 引数チェック */
  if (horc == NULL || strm == NULL || data == NULL) {
    return HORC_APIRESULT_NG;
  }

  /* レンジコーダの初期化 */
  HORCRangeCoder_InitializeEncode(horc);

  data_pos = 0;
  is_need_flush = 0;
  while (data_pos < num_data) {

    /* 定期間隔で圧縮率チェックを入れる */
    if ((c != HORC_FLUSH_MODEL)
        && (((data_pos+1) & (HORC_NUM_SYMBOLS(horc->symbol_bits)/2-1)) == 0)) {
      is_need_flush = HORC_CheckCompression(horc, data_pos, strm);
    }

    /* シンボルを取得
     * モデルのフラッシュが必要ならばシンボルを差し替える */
    if (is_need_flush == 0) {
      c = data[data_pos++];
    } else {
      c = HORC_FLUSH_MODEL;
    }

    /* エスケープが発生している間は下位の文脈に下りながら
     * エンコードを行う */
    do {
      is_escaped = HORC_ConvertIntToSymbol(horc, c, &s);
      HORCRangeCoder_EncodeSymbol(horc, strm, &s);
    } while (is_escaped == 1);

    /* モデルのフラッシュ */
    if (c == HORC_FLUSH_MODEL) {
      HORC_FlushModel(horc);
      is_need_flush = 0;
    }

    /* モデルの更新 */
    HORC_UpdateModel(horc, c);
    HORC_AddSymbolToModel(horc, c);
  }

  /* レンジコーダのエンコードを終了 */
  HORCRangeCoder_FinishEncode(horc, strm);

  return HORC_APIRESULT_OK;
}

/* デコード */
HORCApiResult HORC_Decode(
    struct HORC* horc, struct BitStream* strm,
    uint32_t* data, uint32_t num_data)
{
  struct RangeCoderSymbol s;
  int32_t   c;
  int32_t   count;
  uint32_t  data_pos;

  /* 引数チェック */
  if (horc == NULL || strm == NULL || data == NULL) {
    return HORC_APIRESULT_NG;
  }

  /* レンジコーダの初期化 */
  HORCRangeCoder_InitializeDecode(horc, strm);

  data_pos = 0;
  while (data_pos < num_data) {
    /* エスケープ記号から抜けるまで記号を取得 */
    do {
      HORC_GetSymbolScale(horc, &s);
      count = HORCRangeCoder_GetCurrentCount(horc, &s);
      c     = HORC_ConvertSymbolToInt(horc, count, &s);
      HORCRangeCoder_RemoveSymbolFromStream(horc, strm, &s);
    } while (c == HORC_ESCAPE(horc->symbol_bits));

    /* シンボルを取得
     * モデルのフラッシュ記号ならばフラッシュ */
    if (c != HORC_FLUSH_MODEL) {
      data[data_pos++] = c;
    } else {
      HORC_FlushModel(horc);
    }

    /* モデル更新 */
    HORC_UpdateModel(horc, c);
    HORC_AddSymbolToModel(horc, c);
  }

  return HORC_APIRESULT_OK;
}

/* 圧縮率チェック */
static int32_t HORC_CheckCompression(
    struct HORC* horc, int32_t input_pos, struct BitStream* out_stream)
{
  int32_t total_input_bits, total_output_bits;
  int32_t ratio;

  total_input_bits  = (input_pos - horc->input_mark) * horc->symbol_bits;
  total_output_bits = (BitStream_Tell(out_stream) - horc->output_mark) * 8;

  /* 入力が全く進んでいない。フラッシュの必要あり */
  if (total_input_bits == 0) {
    return 1;
  }

  /* 出力が進んでいないので圧縮率は最大になる */
  if (total_output_bits == 0) {
    total_output_bits = 1;
  }

  /* 圧縮率 */
  ratio = (total_output_bits * 100) / total_input_bits;

  /* 入力位置と出力位置の更新 */
  horc->input_mark  = input_pos;
  horc->output_mark = BitStream_Tell(out_stream);

  return ratio > HORC_COMPRESSION_RATIO_THRESHOLD;
}

/* 新たな文脈表の作成 */
static struct Context* HORC_AllocateNextOrderTable(
    struct Context* table, int32_t symbol, struct Context* lesser_context)
{
  struct Context* new_table;
  int32_t         i;
  uint32_t        new_size;

  for (i = 0; i <= table->max_index; i++) {
    if (table->stats[i].symbol == symbol) {
      break;
    }
  }

  /* 文脈表の拡大 */
  if (i > table->max_index) {
    table->max_index++;
    new_size = sizeof(struct ContextLinks) * (table->max_index + 1);
    if (table->links == NULL) {
      HORC_CALLOC(table->links, new_size, 1, struct ContextLinks *);
    } else {
      HORC_REALLOC(table->links, new_size, struct ContextLinks *);
    }

    new_size = sizeof(struct ContextEntry) * (table->max_index + 1);
    if (table->stats == NULL) {
      HORC_CALLOC(table->stats, new_size, 1, struct ContextEntry *);
    } else {
      HORC_REALLOC(table->stats, new_size, struct ContextEntry *);
    }

    table->stats[i].symbol = symbol;
    table->stats[i].counts = 0;
  }

  /* 新しい文脈表の作成 */
  HORC_CALLOC(new_table, sizeof(struct Context), 1, struct Context *);
  new_table->max_index      = -1;
  new_table->lesser_context = lesser_context;
  table->links[i].next      = new_table;

  return new_table;
}

/* モデルの更新 */
static void HORC_UpdateModel(struct HORC* horc, int32_t symbol)
{
  int32_t i, local_order;

  local_order = (horc->current_order < 0) ? 0 : horc->current_order;

  if (symbol >= 0) {
    /* 現在の次数から各次数の文脈に更新を行う */
    while (local_order <= horc->max_order) {
      if (symbol >= 0) {
        HORC_UpdateTable(horc, horc->contexts[local_order], symbol);
      }
      local_order++;
    }
  }

  horc->current_order = horc->max_order;

  /* 得点表の初期化 */
  for (i = 0; i < (int32_t)HORC_NUM_SYMBOLS(horc->symbol_bits); i++) {
    horc->scoreboard[i] = 0;
  }

}

/* 文脈表の更新 - シンボルの出現度数の更新 */
static void HORC_UpdateTable(struct HORC* horc, struct Context* table, int32_t symbol)
{
  int32_t         i, index;
  uint32_t        tmp;
  struct Context* tmp_ptr;
  uint32_t        new_size;

  index = 0;
  while (index <= table->max_index
          && table->stats[index].symbol != symbol) {
    index++;
  }

  /* 文脈表の拡大 */
  if (index > table->max_index) {
    table->max_index++;
    new_size = sizeof(struct ContextLinks) * (table->max_index + 1);
    if (horc->current_order < horc->max_order) {
      if (table->max_index == 0) {
        HORC_CALLOC(table->links, new_size, 1, struct ContextLinks *);
      } else {
        HORC_REALLOC(table->links, new_size, struct ContextLinks *);
      }
      table->links[index].next = NULL;
    }

    new_size = sizeof(struct ContextEntry) * (table->max_index + 1);
    if (table->max_index == 0) {
      HORC_CALLOC(table->stats, new_size, 1, struct ContextEntry *);
    } else {
      HORC_REALLOC(table->stats, new_size, struct ContextEntry *);
    }

    table->stats[index].symbol = symbol;
    table->stats[index].counts = 0;
  }

  /* シンボルをリストの前方の要素と入れ替え */
  i = index;
  while (i > 0 && table->stats[index].counts == table->stats[i-1].counts) {
    i--;
  }
  if (i != index) {
    tmp = table->stats[index].symbol;
    table->stats[index].symbol = table->stats[i].symbol;
    table->stats[i].symbol     = tmp;
    if (table->links != NULL) {
      tmp_ptr = table->links[index].next;
      table->links[index].next = table->links[i].next;
      table->links[i].next     = tmp_ptr;
    }
    index = i;
  }

  /* 出現度数を更新 */
  table->stats[index].counts++;
  if (table->stats[index].counts == HORC_MAX_COUNT) {
    HORC_RescaleTable(table);
  }

}

/* 記号の符号化 */
static int32_t HORC_ConvertIntToSymbol(
    struct HORC* horc, int32_t c, struct RangeCoderSymbol* s)
{
  int32_t i;
  struct Context* table;

  table = horc->contexts[horc->current_order];
  HORC_TotalizeTable(horc, table);
  s->scale = horc->totals[0];

  /* 次数-2の文字は負で表現されている */
  if (horc->current_order == -2) {
    c = -c;
  }

  for (i = 0; i <= table->max_index; i++) {
    if (c == table->stats[i].symbol) {
      /* 度数が0ならばループを抜けて次数を落とす */
      if (table->stats[i].counts == 0) {
        break;
      }

      /* 符号に対する下限度数と上限度数の取得 */
      s->low_count  = horc->totals[i + 2];
      s->high_count = horc->totals[i + 1];
      return 0;
    }
  }

  /* エスケープされている */
  s->low_count  = horc->totals[1];
  s->high_count = horc->totals[0];
  horc->current_order--;
  return 1;
}

/* 記号に対するスケール値の取得 */
static void HORC_GetSymbolScale(struct HORC* horc, struct RangeCoderSymbol* s)
{
  struct Context* table;

  table = horc->contexts[horc->current_order];
  /* 表を構成する */
  HORC_TotalizeTable(horc, table);
  s->scale = horc->totals[0];
}

/* 復号器から得た出現度数から、その出現度数に一致する記号を探す */
static int32_t HORC_ConvertSymbolToInt(
    struct HORC* horc,
    int32_t count, struct RangeCoderSymbol* s)
{
  int32_t c;
  struct Context* table;

  table = horc->contexts[horc->current_order];
  for (c = 0; count < horc->totals[c]; c++) {
    ;
  }
  s->high_count = horc->totals[c - 1];
  s->low_count  = horc->totals[c];

  if (c == 1) {
    horc->current_order--;
    return HORC_ESCAPE(horc->symbol_bits);
  }

  if (horc->current_order < -1) {
    return -table->stats[c - 2].symbol;
  } else {
    return table->stats[c - 2].symbol;
  }
}

/* 新しいシンボルをモデルに追加し、文脈を遷移する */
static void HORC_AddSymbolToModel(struct HORC* horc, int32_t c)
{
  int32_t i;

  /* 表が存在しない / 文字が-2次のものであれば何もしない */
  if (horc->max_order < 0 || c < 0) {
    return;
  }

  /* 文脈を移行 */
  horc->contexts[horc->max_order]
    = HORC_ShiftToNextContext(horc, horc->contexts[horc->max_order], c, horc->max_order);

  /* 最高次から低次の文脈を取得 */
  for (i = horc->max_order-1; i > 0; i--) {
    horc->contexts[i] = horc->contexts[i+1]->lesser_context;
  }
}

/* 新しいシンボルを追加するコア処理 */
static struct Context* HORC_ShiftToNextContext(
    struct HORC* horc, struct Context* table, int32_t c, int32_t order)
{
  int32_t i;
  struct Context* new_lesser;

  /* 低次の文脈表から目的の要素があるか探す */
  table = table->lesser_context;
  if (order == 0) {
    return table->links[0].next;
  }
  for (i = 0; i <= table->max_index; i++) {
    if (table->stats[i].symbol == c) {
      if (table->links[i].next != NULL) {
        /* あれば即時リターン */
        return table->links[i].next;
      } else {
        break;
      }
    }
  }

  /* ここまで来ている時は目的の文脈がなかったことになる
   * 新しい文脈表を作る */
  
  /* より低次の文脈から要素を探す(-1次で必ず見つかる) */
  new_lesser = HORC_ShiftToNextContext(horc, table, c, order-1);

  /* 新たな文脈表を割り当てる */
  table = HORC_AllocateNextOrderTable(table, c, new_lesser);
  return table;
}

/* 文脈表のスケールダウン処理 */
static void HORC_RescaleTable(struct Context* table)
{
  int32_t i;

  if (table->max_index == -1) {
    return;
  }

  /* 全ての出現度数を2で割る */
  for (i = 0; i <= table->max_index; i++) {
    table->stats[i].counts /= 2;
  }

  /* 2で割った結果出現度数が0になってしまったら、
   * その出現度数情報をstatsから取り除く */
  if (table->stats[table->max_index].counts == 0
      && table->links == NULL) {
    while (table->stats[table->max_index].counts == 0
            && table->max_index >= 0) {
      table->max_index--;
    }
    if (table->max_index == -1) {
      free(table->stats);
      table->stats = NULL;
    } else {
      HORC_REALLOC(table->stats, 
          sizeof(struct ContextEntry) * (table->max_index + 1),
          struct ContextEntry *);
    }
  }
}

/* 累積度数表の生成 */
static void HORC_TotalizeTable(
    struct HORC* horc, struct Context* table)
{
  int32_t i;
  uint32_t max;

  for (;;) {
    max = 0;
    i = table->max_index + 2;
    horc->totals[i] = 0;
    for ( ; i > 1; i--) {
      horc->totals[i-1] = horc->totals[i];
      if (table->stats[i-2].counts > 0) {
        if ((horc->current_order == -2)
            || (horc->scoreboard[table->stats[i-2].symbol] == 0)) {
          /* 得点表に出現していなければ度数を加算 */
          horc->totals[i-1] += table->stats[i-2].counts;
        }
      }
      /* 最大度数の更新 */
      if (table->stats[i-2].counts > max) {
        max = table->stats[i-2].counts;
      }
    }

    /* 要素0, 1のエスケープ記号に対する処理 */ 
    /* 最大度数に合わせて動的に変化させる */
    if (max == 0) {
      horc->totals[0] = 1;
    } else {
      horc->totals[0] =  HORC_NUM_SYMBOLS(horc->symbol_bits) - table->max_index;
      horc->totals[0] *= table->max_index;
      horc->totals[0] /= HORC_NUM_SYMBOLS(horc->symbol_bits);
      horc->totals[0] /= max;
      horc->totals[0]++;
      horc->totals[0] += horc->totals[1];
    }

    if (horc->totals[0] < HORC_MAXIMUM_SCALE) {
      break;
    }

    HORC_RescaleTable(table);
  }

  /* 得点表に出現した記号をマークする */
  for (i = 0; i < table->max_index; i++) {
    if (table->stats[i].counts != 0) {
      horc->scoreboard[table->stats[i].symbol] = 1;
    }
  }
}

/* モデルの再帰的掃き出し処理 */
static void HORC_RecursiveFlush(struct Context* table)
{
  int32_t i;

  if (table->links != NULL) {
    for (i = 0; i <= table->max_index; i++) {
      if (table->links[i].next != NULL) {
        HORC_RecursiveFlush(table->links[i].next);
      }
    }
  }

  /* スケールダウン処理 */
  HORC_RescaleTable(table);
}

/* 全ての文脈表を掃き出す */
static void HORC_FlushModel(struct HORC* horc)
{
  /* puts("Flush!"); */
  HORC_RecursiveFlush(horc->contexts[0]);
}

/* レンジコーダ初期化 */
static void HORCRangeCoder_InitializeEncode(struct HORC* horc)
{
  horc->low             = 0;
  horc->high            = 0xFFFFFFFF;
  horc->underflow_bits  = 0;
}

/* レンジコーダのエンコード終了 */
static void HORCRangeCoder_FinishEncode(
    struct HORC* horc, struct BitStream* stream)
{
  /* アンダーフロービット出力 */
  BitStream_PutBit(stream,
      HORC_BOOL_TO_BIT((horc->low & 0x40000000) != 0));
  horc->underflow_bits++;
  while (horc->underflow_bits > 0) {
    BitStream_PutBit(stream,
        HORC_BOOL_TO_BIT((~(horc->low) & 0x40000000) != 0));
    horc->underflow_bits--;
  }

  /* 余り分をフラッシュ */
  BitStream_Flush(stream);
}

/* シンボルのエンコード */
static void HORCRangeCoder_EncodeSymbol(struct HORC* horc,
    struct BitStream* stream, struct RangeCoderSymbol* s)
{
  uint64_t range;

  /* レンジと上限下限の更新 */
  range      = (uint64_t)(horc->high - horc->low) + 1;
  horc->high = horc->low + ((range * s->high_count) / s->scale - 1);
  horc->low  = horc->low + ((range * s->low_count) / s->scale);

  for (;;) {
    if ((horc->high & 0x80000000) == (horc->low & 0x80000000)) {
      /* 最上位桁が一致したら出力 */
      BitStream_PutBit(stream,
          HORC_BOOL_TO_BIT((horc->high & 0x80000000) != 0));
      /* 溜まったアンダーフロー分を出力 */
      while (horc->underflow_bits > 0) {
        BitStream_PutBit(stream, 
            HORC_BOOL_TO_BIT((~(horc->high) & 0x80000000) != 0));
        horc->underflow_bits--;
      }
    } else if ((horc->low & 0x40000000) && !(horc->high & 0x40000000)) {
      /* 上から2桁目が不一致ならば接近していると判定
       * アンダーフロー判定 */
      horc->underflow_bits++;
      horc->low   &= 0x3FFFFFFF;
      horc->high  |= 0x40000000;
    } else {
      return;
    }

    /* 下限と上限のシフト. 上限は下位ビットから1を補う */
    horc->low   <<= 1;
    horc->high  <<= 1;
    horc->high  |=  1;
  }
}

/* 符号に対する出現度数を得る 返り値を符号のスケールで割ると符号が得られる */
static int32_t HORCRangeCoder_GetCurrentCount(struct HORC* horc, struct RangeCoderSymbol* s)
{
  int64_t range, count;
  range = (int64_t)horc->high - horc->low + 1;
  count = ((int64_t)(horc->code - horc->low + 1) * s->scale - 1) / range;
  return count;
}

/* デコーダ初期化 */
static void HORCRangeCoder_InitializeDecode(
    struct HORC* horc, struct BitStream* stream)
{
  int32_t i;

  horc->code = 0;
  for (i = 0; i < 32; i++) {
    horc->code <<= 1;
    horc->code += BitStream_GetBit(stream);
  }

  horc->low   = 0;
  horc->high  = 0xFFFFFFFF;
}

/* シンボルを復号した後のビット読み込み処理 */
static void HORCRangeCoder_RemoveSymbolFromStream(
    struct HORC* horc, struct BitStream* stream, struct RangeCoderSymbol* s)
{
  int64_t range;

  assert(s->scale != 0);

  /* レンジ, 上限, 下限の更新 */
  range       = (int64_t)(horc->high - horc->low) + 1;
  horc->high  = horc->low + (s->high_count * range) / s->scale - 1;
  horc->low   = horc->low + (s->low_count  * range) / s->scale;

  /* ビットの読み込み */
  for (;;) {
    if ((horc->high & 0x80000000) == (horc->low & 0x80000000)) {
      /* 最上位ビットが一致したらそのまま取り除く */
    } else if (((horc->low & 0x40000000) == 0x40000000) && ((horc->high & 0x40000000) == 0)) {
      /* 上から2桁目が接近していれば、2桁目のビットを取り除く */
      horc->code  ^= 0x40000000;
      horc->low   &= 0x3FFFFFFF;
      horc->high  |= 0x40000000;
    } else {
      /* 取り除くことはできない */
      return;
    }

    /* ビットシフトと新たなビット読み込み */
    horc->low   <<= 1;
    horc->high  <<= 1;
    horc->high  |=  1;
    horc->code  <<= 1;
    horc->code  +=  BitStream_GetBit(stream);
  }
}

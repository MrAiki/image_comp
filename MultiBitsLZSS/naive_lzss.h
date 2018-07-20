#ifndef NAIVE_LZSS_H_INCLUDED 
#define NAIVE_LZSS_H_INCLUDED 

#include "../../BitStream/bit_stream.h"
#include <stdint.h>

/* ハンドル */
struct NaiveLZSS;

/* API結果型 */
typedef enum NaiveLZSSApiResultTag {
  NAIVE_LZSS_APIRESULT_OK = 0,
  NAIVE_LZSS_APIRESULT_NG,
} NaiveLZSSApiResult;

/* マルチビットLZSSハンドル */
struct NaiveLZSS* NaiveLZSS_Create(
    uint32_t symbol_bits, uint32_t window_size,
    uint32_t min_match_len, uint32_t max_match_len);

/* マルチビットLZSSハンドル破棄 */
void NaiveLZSS_Destroy(struct NaiveLZSS* lzss);

/* シンボルのエンコード */
NaiveLZSSApiResult NaiveLZSS_EncodeSymbol(
    struct NaiveLZSS* lzss, struct BitStream* strm,
    const uint32_t* data, uint32_t num_data);

/* シンボルのデコード */
NaiveLZSSApiResult NaiveLZSS_DecodeSymbol(
    struct NaiveLZSS* lzss, struct BitStream* strm,
    uint32_t* data, uint32_t num_data);

#endif /* NAIVE_LZSS_H_INCLUDED */

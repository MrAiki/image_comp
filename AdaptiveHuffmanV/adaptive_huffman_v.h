#ifndef ADAPTIVE_HUFFMAN_V_H_INCLUDED 
#define ADAPTIVE_HUFFMAN_V_H_INCLUDED 

#include "../BitStream/bit_stream.h"
#include <stdint.h>

/* 適応型ハフマン（Vアルゴリズムベース）木 */
struct AdaptiveHuffmanVTree;

/* API結果型 */
typedef enum AdaptiveHuffmanVApiResultTag {
  ADAPTIVE_HUFFMAN_V_APIRESULT_OK = 0,
  ADAPTIVE_HUFFMAN_V_APIRESULT_NG,
} AdaptiveHuffmanVApiResult;

/* 適応型ハフマン木の作成 */
struct AdaptiveHuffmanVTree* AdaptiveHuffmanVTree_Create(uint32_t symbol_bits);

/* 適応型ハフマン木の破棄 */
void AdaptiveHuffmanVTree_Destroy(struct AdaptiveHuffmanVTree* tree);

/* シンボルのエンコード */
AdaptiveHuffmanVApiResult AdaptiveHuffmanV_EncodeSymbol(
    struct AdaptiveHuffmanVTree* tree,
    struct BitStream* strm,
    int32_t symbol);

/* シンボルのデコード */
AdaptiveHuffmanVApiResult AdaptiveHuffmanV_DecodeSymbol(
    struct AdaptiveHuffmanVTree* tree,
    struct BitStream* strm,
    int32_t* symbol);

#endif /* ADAPTIVE_HUFFMAN_V_H_INCLUDED */

#ifndef ADAPTIVE_HUFFMAN_H_INCLUDED 
#define ADAPTIVE_HUFFMAN_H_INCLUDED 

#include "../../BitStream/bit_stream.h"
#include <stdint.h>

/* 適応型ハフマン木 */
struct AdaptiveHuffmanTree;

/* API結果型 */
typedef enum AdaptiveHuffmanApiResultTag {
  ADAPTIVE_HUFFMAN_APIRESULT_OK = 0,
  ADAPTIVE_HUFFMAN_APIRESULT_NG,
} AdaptiveHuffmanApiResult;

/* 適応型ハフマン木の作成 */
struct AdaptiveHuffmanTree* AdaptiveHuffmanTree_Create(uint32_t symbol_bits);

/* 適応型ハフマン木の破棄 */
void AdaptiveHuffmanTree_Destroy(struct AdaptiveHuffmanTree* tree);

/* シンボルのエンコード */
AdaptiveHuffmanApiResult AdaptiveHuffman_EncodeSymbol(
    struct AdaptiveHuffmanTree* tree,
    struct BitStream* strm,
    uint32_t symbol);

/* シンボルのデコード */
AdaptiveHuffmanApiResult AdaptiveHuffman_DecodeSymbol(
    struct AdaptiveHuffmanTree* tree,
    struct BitStream* strm,
    uint32_t* symbol);

#endif /* ADAPTIVE_HUFFMAN_H_INCLUDED */

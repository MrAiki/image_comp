#ifndef HORC_H_INCLUDED 
#define HORC_H_INCLUDED 

#include "../../BitStream/bit_stream.h"
#include <stdint.h>

/* 高次文脈モデルを使用したレンジコーダ */
/* Higher-Order Range Coder */
struct HORC;

/* API結果型 */
typedef enum HORCApiResultTag {
  HORC_APIRESULT_OK = 0,
  HORC_APIRESULT_NG,
} HORCApiResult;

/* ハンドル作成 */
struct HORC* HORC_Create(int32_t max_order, uint32_t symbol_bits);

/* ハンドル破棄 */
void HORC_Destroy(struct HORC* horc);

/* エンコード */
HORCApiResult HORC_Encode(
    struct HORC* horc, struct BitStream* strm,
    const uint32_t* data, uint32_t num_data);

/* デコード */
HORCApiResult HORC_Decode(
    struct HORC* horc, struct BitStream* strm,
    uint32_t* data, uint32_t num_data);

#endif /* HORC_H_INCLUDED */

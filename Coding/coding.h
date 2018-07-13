#ifndef CODING_UTIL_H_INCLUDED
#define CODING_UTIL_H_INCLUDED

#include <stdint.h>
#include "../../BitStream/bit_stream.h"

/* 符号付きワイル符号の出力 */
void SignedWyle_PutCode(struct BitStream* strm, int32_t val);

/* 符号付きワイル符号の取得 */
int32_t SignedWyle_GetCode(struct BitStream* strm);

/* ゴロム符号化の出力 mは符号化パラメータ */
void Golomb_PutCode(struct BitStream* strm, uint32_t m, uint32_t val);

/* ゴロム符号化の取得 mは符号化パラメータ */
uint32_t Golomb_GetCode(struct BitStream* strm, uint32_t m);

/* Pack Bitsによる符号化 */
void PackBits_Encode(struct BitStream* strm,
    const uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t threshould, uint32_t length_bits);

/* Pack Bitsによる復号 */
void PackBits_Decode(struct BitStream* strm,
    uint32_t* data, uint32_t num_data, uint32_t golomb_m, uint32_t length_bits);

/* Mineo版Pack Bitsによる復号 */
void MineoPackBits_Decode(struct BitStream* strm,
    uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t length_bits);

/* Mineo版Pack Bitsによる符号化 */
void MineoPackBits_Encode(struct BitStream* strm,
    const uint32_t* data, uint32_t num_data,
    uint32_t golomb_m, uint32_t threshould, uint32_t length_bits);

#endif /* CODING_UTIL_H_INCLUDED */

#ifndef SRC_H_INCLUDED
#define SRC_H_INCLUDED

#include "../BitStream/bit_stream.h"

/* SRC - Shibuki Ran-length Coding */

/* API結果型 */
typedef enum SRCApiResultTag {
  SRC_APIRESULT_OKOKOK = 0, /* OK */
  SRC_APIRESULT_NG = 0,
} SRCApiResult;

/* SRCによる符号化 */
SRCApiResult SRC_Encode(const char* in_filename, const char* out_filename);

/* SRCによる復号 */
SRCApiResult SRC_Decode(const char* in_filename, const char* out_filename);

#endif /* SRC_H_INCLUDED */

#include <stdio.h>
#include "pnm.h"

int main(int argc, char** argv)
{
  struct PNMImage *image;

  if (argc < 3) {
    puts("Usage: prog input output");
    return 1;
  }

  /* 画像オープン */
  image = PNM_ReadFile(argv[1]);
  if (image == NULL) {
    return 2;
  }

  /* 何かする */

  /* 画像書き出し */
  if (PNM_WriteFile(argv[2], image) != PNM_APIRESULT_OK) {
    return 3;
  }

  /* 画像の領域解放 */
  PNM_FreeImage(image);

  return 0;
}

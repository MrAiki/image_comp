#include <stdio.h>
#include <stdlib.h>
#include "pnm.h"

int main(int argc, char** argv)
{
  struct PNMPicture *picture;

  if (argc < 2) {
    puts("Usage: prog input");
    return 1;
  }

  /* 画像オープン */
  picture = PNM_CreatePictureFromFile(argv[1]);
  if (picture == NULL) {
    return 2;
  }

  /* 何かする */
  if (picture->header.format == PNM_FORMAT_P5) {
    int32_t i;
    uint32_t *hori_diff = (uint32_t *)calloc(sizeof(uint32_t), 512);
    uint32_t *vert_diff = (uint32_t *)calloc(sizeof(uint32_t), 512);
    uint32_t x, y;
    for (y = 0; y < picture->header.height; y++) {
      for (x = 0; x < picture->header.width; x++) {
        if (x >= 1 && y >= 1) {
          int16_t hdiff = PNMPict_GRAY(picture,x,y) - (int32_t)PNMPict_GRAY(picture,x-1,y);
          int16_t vdiff = PNMPict_GRAY(picture,x,y) - (int32_t)PNMPict_GRAY(picture,x,y-1);
          hori_diff[255 + hdiff]++;
          vert_diff[255 + vdiff]++;
        }
      }
    }

    puts("Diff, Horizontal, Vertival");
    for (i = 0; i < 511; i++) {
      printf("%d, %d, %d \n", i - 255, hori_diff[i], vert_diff[i]);
    }
  }
  

  /* 画像書き出し */
#if 0
  if (PNM_WriteFile(argv[2], picture) != PNM_APIRESULT_OK) {
    return 3;
  }
#endif

  /* 画像の領域解放 */
  PNM_DestroyPicture(picture);

  return 0;
}

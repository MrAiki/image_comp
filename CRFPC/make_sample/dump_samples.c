#include "../crfpc.h"
#include "../../pnm/pnm.h"

#include <stdlib.h>
#include <stdio.h>

#define MAX_DIM_SAMPLE 11

int main(int argc, char** argv)
{
  struct PNMPicture* pict;
  uint32_t i_dim, i_sample, x, y;
  uint8_t sample_data[MAX_DIM_SAMPLE];
  int16_t index_list[MAX_DIM_SAMPLE][2] = {
    {  0,  0 }, { -1,  0 }, { -1, -1 }, {  0, -1 },
    {  1, -1 }, { -2,  0 }, { -2, -1 }, { -2, -2 },
    { -1, -2 }, {  0, -2 }, {  1, -2 },
  };

  if (argc != 2) {
    fprintf(stderr, "Usage: prog picture.pgm \n");
    return 1;
  }

  if ((pict = PNM_CreatePictureFromFile(argv[1])) == NULL) {
    fprintf(stderr, "Failed to open file(%s). \n", argv[1]);
    return 1;
  }

  if (pict->header.format != PNM_FORMAT_P2
      && pict->header.format != PNM_FORMAT_P5) {
    fprintf(stderr, "Invalid pbm format. \n");
    return 1;
  }

  /* サンプル回収 */
  i_sample = 0;
  for (y = 2; y < pict->header.height; y++) {
    for (x = 2; x < pict->header.width-1; x++) {
      for (i_dim = 0; i_dim < MAX_DIM_SAMPLE; i_dim++) {
        fprintf(stdout, "%d,",
            PNMPict_GRAY(pict,
              x + index_list[i_dim][0],
              y + index_list[i_dim][1]));
      }
      fprintf(stdout, "\n");
      i_sample++;
    }
  }

  PNM_DestroyPicture(pict);

  return 0;
}

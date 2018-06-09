#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 読み出しバッファ */
static char read_buffer[256];

int is_useful_sample(uint8_t *sample_data, uint32_t dim_sample)
{
  uint32_t i_dim;
  uint32_t diff_count = 0;
  for (i_dim = 0; i_dim < dim_sample-1; i_dim++) {
    if (sample_data[i_dim] != sample_data[i_dim+1]) {
      diff_count++;
      if (diff_count > 2) {
        if (rand() % 100 <= 3) {
          return 1;
        } else {
          return 0;
        }
      }
    }
  }
  return 0;
}

int main(int argc, char** argv)
{
  uint8_t *sample_buffer;
  char    *token_ptr;
  uint32_t dim_sample, num_sample;
  uint32_t i_dim;

  if (argc != 3) {
    fprintf(stderr, "Usage: prog NSAMPLES NDIM \n");
    return 1;
  }

  num_sample = strtol(argv[1], NULL, 10);
  dim_sample = strtol(argv[2], NULL, 10);

  sample_buffer = malloc(sizeof(uint8_t) * dim_sample);

  while (fgets(read_buffer, sizeof(read_buffer), stdin) != NULL) {
    token_ptr = strtok(read_buffer, ",");
    for (i_dim = 0; (i_dim < dim_sample) && (token_ptr != NULL); i_dim++) {
      sample_buffer[i_dim] = strtol(token_ptr, NULL, 10);
      token_ptr = strtok(NULL, ",");
    }

    if (is_useful_sample(sample_buffer, dim_sample)) {
      for (i_dim = 0; i_dim < dim_sample; i_dim++) {
        fprintf(stdout, "%d,", sample_buffer[i_dim]);
      }
      fprintf(stdout, "\n");
    }
  }

  free(sample_buffer);

  return 0;
}

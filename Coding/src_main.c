#include "src.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
  if (argc != 4) {
    fprintf(stderr, "Usage: prog -[cd] infile outfile \n");
    return 1;
  }

  if (strcmp(argv[1], "-c") == 0) {
    if (SRC_Encode(argv[2], argv[3]) != SRC_APIRESULT_OKOKOK) {
      return 1;
    }
  } else if (strcmp(argv[1], "-d") == 0) {
    if (SRC_Decode(argv[2], argv[3]) != SRC_APIRESULT_OKOKOK) {
      return 1;
    }
  } else {
    return 1;
  }

  return 0;
}


#include "db.h"
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: gigamctl <command> ...\n");
  }
  return cli_dispatch(argc, argv);
}

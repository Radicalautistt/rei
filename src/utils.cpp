#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "utils.hpp"

namespace rei::utils {

void readFile (const char* relativePath, const char* flags, File& output) {
  FILE* file = fopen (relativePath, flags);
  assert (file);

  fseek (file, 0, SEEK_END);
  output.size = ftell (file);
  rewind (file);

  output.contents = malloc (output.size);
  fread (output.contents, 1, output.size, file);
  fclose (file);
}

}

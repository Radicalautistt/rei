#include <stdio.h>
#include <stdlib.h>

#include "utils.hpp"
#include "common.hpp"

namespace rei::utils {

timeval Timer::start;

void Timer::init () noexcept {
  gettimeofday (&Timer::start, nullptr);
}

float Timer::getCurrentTime () noexcept {
  timeval now;
  gettimeofday (&now, nullptr);
  return SCAST <float> ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000) / 1000.f;
}

Result readFile (const char* relativePath, bool binary, File& output) {
  FILE* file = fopen (relativePath, binary ? "rb" : "r");
  if (!file) return Result::FileDoesNotExist;

  fseek (file, 0, SEEK_END);
  output.size = ftell (file);
  rewind (file);

  output.contents = malloc (output.size);
  fread (output.contents, 1, output.size, file);
  fclose (file);

  return Result::Success;
}

}

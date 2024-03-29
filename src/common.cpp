#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include "common.hpp"

namespace rei {

timeval Timer::start;

void Timer::init () noexcept {
  gettimeofday (&Timer::start, nullptr);
}

f32 Timer::getCurrentTime () noexcept {
  timeval now;
  gettimeofday (&now, nullptr);
  return (f32) ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000) / 1000.f;
}

void logger (LogLevel level, const char* format, ...) {
  static const char colors[][10] {ANSI_GREEN, ANSI_RED, ANSI_YELLOW};
  static const char names[][11] {"[INFO]    ", "[ERROR]   ", "[WARNING] "};

  printf ("%s%s", colors[level], names[level]);

  va_list arguments;
  va_start (arguments, format);

  vprintf (format, arguments);
  puts (ANSI_RESET);

  va_end (arguments);
}

const char* getError (Result result) noexcept {
  switch (result) {
    case Result::FileIsEmpty: return "File is empty";
    case Result::FileDoesNotExist: return "File does not exist";
    default: return "Hmmmm";
  }
}

void writeFile (const char* relativePath, b8 binary, void* data, size_t size) {
  FILE* out = fopen (relativePath, binary ? "wb" : "w");
  fwrite (data, 1, size, out);
  fclose (out);
}

Result readFile (const char* relativePath, b8 binary, File* output) {
  FILE* file = fopen (relativePath, binary ? "rb" : "r");
  if (!file) return Result::FileDoesNotExist;

  fseek (file, 0, SEEK_END);
  output->size = ftell (file);
  if (!output->size) return Result::FileIsEmpty;
  rewind (file);

  output->contents = malloc (output->size);
  fread (output->contents, 1, output->size, file);
  fclose (file);

  return Result::Success;
}

}

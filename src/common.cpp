#include <stdio.h>
#include <stdarg.h>

#include "common.hpp"

namespace rei {

void logger (LogLevel level, const char* format, ...) {
  {
    static char colors[3][10] {ANSI_GREEN, ANSI_RED, ANSI_YELLOW};
    static char names[3][11] {"[INFO]    ", "[ERROR]   ", "[WARNING] "};

    printf ("%s%s", colors[level], names[level]);
  }

  va_list arguments;
  va_start (arguments, format);

  vprintf (format, arguments);
  puts (ANSI_RESET);

  va_end (arguments);
}

[[nodiscard]] const char* getError (Result result) noexcept {
  switch (result) {
    case Result::FileIsEmpty: return "File is empty";
    case Result::FileDoesNotExist: return "File does not exist";
    default: return "Hmmmm";
  }
}

}

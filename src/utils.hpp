#ifndef UTILS_HPP
#define UTILS_HPP

#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

#include "common.hpp"

#define CLAMP(value, min, max) (((value) > (max)) ? (max) : (((value) < (min)) ? (min) : (value)))

namespace rei::utils {

struct File {
  size_t size;
  void* contents;
};

struct Timer {
  static timeval start;

  static void init () noexcept;
  [[nodiscard]] static float getCurrentTime () noexcept;
};

[[nodiscard]] Result readFile (const char* relativePath, bool binary, File& output);

}

#endif /* UTILS_HPP */

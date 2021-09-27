#ifndef UTILS_HPP
#define UTILS_HPP

#include <stddef.h>

#define CLAMP(value, min, max) (((value) > (max)) ? (max) : (((value) < (min)) ? (min) : (value)))

namespace rei::utils {

struct File {
  size_t size;
  void* contents;
};

void readFile (const char* relativePath, const char* flags, File& output);

}

#endif /* UTILS_HPP */

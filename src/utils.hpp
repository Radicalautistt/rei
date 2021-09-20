#ifndef UTILS_HPP
#define UTILS_HPP

#include <stdint.h>
#include <stddef.h>

namespace rei::utils {

struct File {
  size_t size;
  void* contents;
};

template <typename Type>
[[nodiscard]] constexpr inline uint32_t clamp (Type value, Type min, Type max) noexcept {
  return value > max ? max : value < min ? min : value;
}

void readFile (const char* relativePath, const char* flags, File& output);

}

#endif /* UTILS_HPP */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <stdint.h>

namespace rei::utils {

template <typename Type>
[[nodiscard]] constexpr inline uint32_t clamp (Type value, Type min, Type max) noexcept {
  return value > max ? max : value < min ? min : value;
}

}

#endif /* UTILS_HPP */

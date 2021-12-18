#ifndef REI_MATH_TYPES_HPP
#define REI_MATH_TYPES_HPP

#include "rei_types.hpp"

namespace rei::math {

struct Vec2 {
  f32 x, y;
  Vec2 () = default;
  constexpr Vec2 (f32 x, f32 y) : x {x}, y {y} {}
  constexpr Vec2 (f32 scalar) : x {scalar}, y {scalar} {}
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct alignas (16) Vec3 {
  f32 x, y, z;

  Vec3 () = default;
  constexpr Vec3 (f32 x, f32 y, f32 z) : x {x}, y {y}, z {z} {}
  constexpr Vec3 (f32 scalar) : x {scalar}, y {scalar}, z {scalar} {}
};

#pragma GCC diagnostic pop

struct Vec4 {
  f32 x, y, z, w;
  Vec4 () = default;
  constexpr Vec4 (f32 scalar) :
    x {scalar}, y {scalar}, z {scalar}, w {scalar} {}

  constexpr Vec4 (f32 x, f32 y, f32 z, f32 w) :
    x {x}, y {y}, z {z}, w {w} {}
};

struct Mat4 {
  Vec4 rows[4];

  Mat4 () = default;
  Mat4 (f32 value) : rows {
    {value, 0.f, 0.f, 0.f},
    {0.f, value, 0.f, 0.f},
    {0.f, 0.f, value, 0.f},
    {0.f, 0.f, 0.f, value}
  } {}
};

}

#endif /* REI_MATH_TYPES_HPP */

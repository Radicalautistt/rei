#ifndef MATH_HPP
#define MATH_HPP

#include <math.h>
#include <immintrin.h>

#include "rei_math_types.hpp"

namespace rei::math {

[[nodiscard]] static inline f32 radians (f32 degrees) noexcept {
  return degrees * 0.01745329251994329576923690768489f;
}

// Set of functions that operate on/return a 128 bit simd register
namespace simd::m128 {

[[nodiscard]] static inline __m128 negate (__m128 value) noexcept {
  return _mm_sub_ps (_mm_setzero_ps (), value);
}

[[nodiscard]] static inline __m128 inverseSqrt (__m128 value) noexcept {
  // 1 / sqrt (value)
  return _mm_div_ps (_mm_set1_ps (1.f), _mm_sqrt_ps (value));
}

[[nodiscard]] static inline __m128 dot (__m128 a, __m128 b) noexcept {
  // a.x * b.x + a.y * b.y + a.z * b.z
  auto product = _mm_mul_ps (a, b);
  auto horizontalSum = _mm_hadd_ps (product, product);
  return _mm_hadd_ps (horizontalSum, horizontalSum);
}

[[nodiscard]] static inline __m128 normalize (__m128 value) noexcept {
  // value * (1 / sqrt (dot (value, value)))
  return _mm_mul_ps (value, inverseSqrt (dot (value, value)));
}

[[nodiscard]] static inline __m128 cross (__m128 a, __m128 b) noexcept {
  // [ a.y * b.z - a.z * b.y,
  //   a.z * b.x - a.x * b.z,
  //   a.x * b.y - a.y * b.x ]

  // Before performing any arithmetic operations, we must
  // reshuffle contents of both registers so that their order
  // is equal to argument order of the formula above.
  // NOTE _MM_SHUFFLE's argument order is a layout we
  // want our register to have. E.g. _MM_SHUFFLE (3, 0, 2, 1)
  // used with _mm_shuffle_ps returns a register
  // of form (w, x, z, y) (in this case w is ingnored since we are operating on 3d vectors),
  // just like the first column of the formula (in reverse order).
  const i32 leftMask = _MM_SHUFFLE (3, 0, 2, 1);
  const i32 rightMask = _MM_SHUFFLE (3, 1, 0, 2);

  __m128 columns[4];
  columns[0] = _mm_shuffle_ps (a, a, leftMask);
  columns[1] = _mm_shuffle_ps (b, b, rightMask);
  columns[2] = _mm_shuffle_ps (a, a, rightMask);
  columns[3] = _mm_shuffle_ps (b, b, leftMask);
  return _mm_sub_ps (_mm_mul_ps (columns[0], columns[1]), _mm_mul_ps (columns[2], columns[3]));
}

} /* simd::m128 */

namespace vec3 {

[[nodiscard]] static inline __m128 load (const Vec3* vector) noexcept {
  return _mm_set_ps (0.f, vector->z, vector->y, vector->x);
}

static inline void add (const Vec3* a, const Vec3* b, Vec3* out) noexcept {
  _mm_store_ps (&out->x, _mm_add_ps (load (a), load (b)));
}

static inline void sub (const Vec3* a, const Vec3* b, Vec3* out) noexcept {
  _mm_store_ps (&out->x, _mm_sub_ps (load (a), load (b)));
}

static inline void mulScalar (const Vec3* vector, f32 scalar, Vec3* out) noexcept {
  _mm_store_ps (&out->x, _mm_mul_ps (_mm_set1_ps (scalar), load (vector)));
}

static inline void normalize (Vec3* out) noexcept {
  _mm_store_ps (&out->x, simd::m128::normalize (load (out)));
}

[[nodiscard]] static inline f32 dot (const Vec3* a, const Vec3* b) noexcept {
  return _mm_cvtss_f32 (simd::m128::dot (load (a), load (b)));
}

static inline void cross (const Vec3* a, const Vec3* b, Vec3* out) noexcept {
  _mm_store_ps (&out->x, simd::m128::cross (load (a), load (b)));
}

} /* vec3 */

namespace vec4 {

[[nodiscard]] static inline __m128 load (const Vec4* vector) noexcept {
  return _mm_load_ps (&vector->x);
}

static inline void add (const Vec4* a, const Vec4* b, Vec4* out) noexcept {
  _mm_store_ps (&out->x, _mm_add_ps (load (a), load (b)));
}

static inline void mul (const Vec4* a, const Vec4* b, Vec4* out) noexcept {
  _mm_store_ps (&out->x, _mm_mul_ps (load (a), load (b)));
}

static inline void mulScalar (const Vec4* vector, f32 scalar, Vec4* out) noexcept {
  _mm_store_ps (&out->x, _mm_mul_ps (_mm_set1_ps (scalar), load (vector)));
}

} /* vec4 */

namespace mat4 {

static inline void scale (Mat4* matrix, const Vec3* vector) noexcept {
  Vec4 temp;
  vec4::mulScalar (&matrix->rows[0], vector->x, &temp);
  matrix->rows[0] = temp;
  vec4::mulScalar (&matrix->rows[1], vector->y, &temp);
  matrix->rows[1] = temp;
  vec4::mulScalar (&matrix->rows[2], vector->z, &temp);
  matrix->rows[2] = temp;
}

static inline void translate (Mat4* matrix, const Vec3* vector) noexcept {
  // matrix->rows[3] =
  //   matrix->rows[0] * vector->x +
  //   matrix->rows[1] * vector->y +
  //   matrix->rows[2] * vector->z + matrix->rows[3];

  Vec4 a, b, c;
  vec4::mulScalar (&matrix->rows[0], vector->x, &a);
  vec4::mulScalar (&matrix->rows[1], vector->y, &b);
  vec4::mulScalar (&matrix->rows[2], vector->z, &c);

  vec4::add (&a, &b, &b);
  vec4::add (&b, &c, &c);
  vec4::add (&c, &matrix->rows[3], &matrix->rows[3]);
}

static inline void mul (const Mat4* a, const Mat4* b, Mat4* out) noexcept {
  // out[0] = a[0] * b[0].x + a[1] * b[0].y + a[2] * b[0].z + a[3] * b[0].w;
  // out[1] = a[0] * b[1].x + a[1] * b[1].y + a[2] * b[1].z + a[3] * b[1].w;
  // out[2] = a[0] * b[2].x + a[1] * b[2].y + a[2] * b[2].z + a[3] * b[2].w;
  // out[3] = a[0] * b[3].x + a[1] * b[3].y + a[2] * b[3].z + a[3] * b[3].w;

  __m128 aRows[4];
  aRows[0] = vec4::load (&a->rows[0]);
  aRows[1] = vec4::load (&a->rows[1]);
  aRows[2] = vec4::load (&a->rows[2]);
  aRows[3] = vec4::load (&a->rows[3]);

  #define MUL_ROW(row) do {                                     \
    auto left = _mm_add_ps (                                    \
      _mm_mul_ps (aRows[0], _mm_set1_ps (b->rows[row].x)),      \
      _mm_mul_ps (aRows[1], _mm_set1_ps (b->rows[row].y))       \
    );                                                          \
                                                                \
    auto right = _mm_add_ps (                                   \
      _mm_mul_ps (aRows[2], _mm_set1_ps (b->rows[row].z)),      \
      _mm_mul_ps (aRows[3], _mm_set1_ps (b->rows[row].w))       \
    );                                                          \
                                                                \
    _mm_store_ps (&out->rows[row].x, _mm_add_ps (left, right)); \
  } while (0)

  MUL_ROW (0);
  MUL_ROW (1);
  MUL_ROW (2);
  MUL_ROW (3);
  #undef MUL_ROW
}

} /* mat4 */

static inline void lookAt (const Vec3* eye, const Vec3* center, const Vec3* up, Mat4* out) noexcept {
  using namespace simd;
  auto eyeM128 = vec3::load (eye);
  auto z = m128::normalize (_mm_sub_ps (vec3::load (center), eyeM128));
  auto x = m128::normalize (m128::cross (z, vec3::load (up)));
  auto y = m128::cross (x, z);

  f32 dotXEye = _mm_cvtss_f32 (m128::dot (x, eyeM128));
  f32 dotYEye = _mm_cvtss_f32 (m128::dot (y, eyeM128));
  f32 dotZEye = _mm_cvtss_f32 (m128::dot (z, eyeM128));

  z = m128::negate (z);

  Vec3 a, b, c;
  _mm_store_ps (&a.x, x);
  _mm_store_ps (&b.x, y);
  _mm_store_ps (&c.x, z);

  out->rows[0].x = a.x;
  out->rows[0].y = b.x;
  out->rows[0].z = c.x;
  out->rows[0].w = 0.f;

  out->rows[1].x = a.y;
  out->rows[1].y = b.y;
  out->rows[1].z = c.y;
  out->rows[1].w = 0.f;

  out->rows[2].x = a.z;
  out->rows[2].y = b.z;
  out->rows[2].z = c.z;
  out->rows[2].w = 0.f;

  out->rows[3].x = -dotXEye;
  out->rows[3].y = -dotYEye;
  out->rows[3].z = dotZEye;
  out->rows[3].w = 1.f;
}

// This function is basically stolen from GLM
static inline void perspective (f32 fov, f32 aspect, f32 zNear, f32 zFar, Mat4* out) noexcept {
  f32 zLength = zFar - zNear;
  f32 focalLength = 1.f / tanf (fov / 2.f);

  out->rows[0].x = focalLength / aspect;
  out->rows[0].y = 0.f;
  out->rows[0].z = 0.f;
  out->rows[0].w = 0.f;

  out->rows[1].x = 0.f;
  out->rows[1].y = -focalLength;
  out->rows[1].z = 0.f;
  out->rows[1].w = 0.f;

  out->rows[2].x = 0.f;
  out->rows[2].y = 0.f;
  out->rows[2].z = -(zFar + zNear) / zLength;
  out->rows[2].w = -1.f;

  out->rows[3].x = 0.f;
  out->rows[3].y = 0.f;
  out->rows[3].z = -(2 * zFar * zNear) / zLength;
  out->rows[3].w = 0.f;
}

}

#endif /* MATH_HPP */

#ifndef MATH_HPP
#define MATH_HPP

#include <immintrin.h>

#include "rei_types.hpp"

#ifndef PI
#  define PI 3.14159265359f
#endif

#ifndef PI_BY_180
// PI / 180. Used for to convert degrees to radians
#  define PI_BY_180 0.01745329251994329576923690768489f
#endif

namespace rei::math {

[[nodiscard]] constexpr inline Float32 radians (Float32 degrees) noexcept {
  return degrees * PI_BY_180;
}

// Set of functions that operate on/return a 128 bit simd register
namespace simd::m128 {

[[nodiscard]] inline __m128 negate (__m128 value) noexcept {
  return _mm_sub_ps (_mm_setzero_ps (), value);
}

[[nodiscard]] inline __m128 inverseSqrt (__m128 value) noexcept {
  // 1 / sqrt (value)
  return _mm_div_ps (_mm_set1_ps (1.f), _mm_sqrt_ps (value));
}

[[nodiscard]] inline __m128 dotProduct (__m128 a, __m128 b) noexcept {
  // a.x * b.x + a.y * b.y + a.z * b.z
  auto product = _mm_mul_ps (a, b);
  auto horizontalSum = _mm_hadd_ps (product, product);
  return _mm_hadd_ps (horizontalSum, horizontalSum);
}

[[nodiscard]] inline __m128 normalize (__m128 value) noexcept {
  // value * (1 / sqrt (dot (value, value)))
  return _mm_mul_ps (value, inverseSqrt (dotProduct (value, value)));
}

[[nodiscard]] inline __m128 crossProduct (__m128 a, __m128 b) noexcept {
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
  const Int32 leftMask = _MM_SHUFFLE (3, 0, 2, 1);
  const Int32 rightMask = _MM_SHUFFLE (3, 1, 0, 2);

  __m128 columns[4];
  columns[0] = _mm_shuffle_ps (a, a, leftMask);
  columns[1] = _mm_shuffle_ps (b, b, rightMask);
  columns[2] = _mm_shuffle_ps (a, a, rightMask);
  columns[3] = _mm_shuffle_ps (b, b, leftMask);
  return _mm_sub_ps (_mm_mul_ps (columns[0], columns[1]), _mm_mul_ps (columns[2], columns[3]));
}

}

struct Vector2 {
  Float32 x, y;

  Vector2 () = default;
  constexpr Vector2 (Float32 x, Float32 y) : x {x}, y {y} {}
  constexpr Vector2 (Float32 scalar) : x {scalar}, y {scalar} {}
};

struct alignas (16) Vector3 {
  Float32 x, y, z;

  Vector3 () = default;
  constexpr Vector3 (Float32 x, Float32 y, Float32 z) : x {x}, y {y}, z {z} {}
  constexpr Vector3 (Float32 scalar) : x {scalar}, y {scalar}, z {scalar} {}

  [[nodiscard]] inline __m128 load () const noexcept {
    return _mm_set_ps (0.f, z, y, x);
  }

  static inline void add (const Vector3* a, const Vector3* b, Vector3* out) {
    _mm_store_ps (&out->x, _mm_add_ps (a->load (), b->load ()));
  }

  static inline void sub (const Vector3* a, const Vector3* b, Vector3* out) {
    _mm_store_ps (&out->x, _mm_sub_ps (a->load (), b->load ()));
  }

  static inline void mulScalar (const Vector3* vector, Float32 scalar, Vector3* out) {
    _mm_store_ps (&out->x, _mm_mul_ps (_mm_set1_ps (scalar), vector->load ()));
  }

  static inline void normalize (Vector3* output) noexcept {
    _mm_store_ps (&output->x, simd::m128::normalize (output->load ()));
  }

  [[nodiscard]] static inline Float32 dotProduct (const Vector3* a, const Vector3* b) noexcept {
    return _mm_cvtss_f32 (simd::m128::dotProduct (a->load (), b->load ()));
  }

  static inline void crossProduct (const Vector3* a, const Vector3* b, Vector3* output) noexcept {
    _mm_store_ps (&output->x, simd::m128::crossProduct (a->load (), b->load ()));
  }
};

struct Vector4 {
  Float32 x, y, z, w;

  Vector4 () = default;
  constexpr Vector4 (Float32 scalar) :
    x {scalar}, y {scalar}, z {scalar}, w {scalar} {}

  constexpr Vector4 (Float32 x, Float32 y, Float32 z, Float32 w) :
    x {x}, y {y}, z {z}, w {w} {}

  [[nodiscard]] inline __m128 load () const noexcept {
    return _mm_load_ps (&this->x);
  }

  static inline void add (const Vector4* a, const Vector4* b, Vector4* out) {
    _mm_store_ps (&out->x, _mm_add_ps (a->load (), b->load ()));
  }

  static inline void mul (const Vector4* a, const Vector4* b, Vector4* out) {
    _mm_store_ps (&out->x, _mm_mul_ps (a->load (), b->load ()));
  }

  static inline void mulScalar (const Vector4* vector, Float32 scalar, Vector4* out) {
    _mm_store_ps (&out->x, _mm_mul_ps (_mm_set1_ps (scalar), vector->load ()));
  }
};

struct Matrix4 {
  Vector4 rows[4];

  Matrix4 () = default;
  Matrix4 (Float32 value) {
    rows[0].x = value;
    rows[0].y = 0.f;
    rows[0].z = 0.f;
    rows[0].w = 0.f;

    rows[1].x = 0.f;
    rows[1].y = value;
    rows[1].z = 0.f;
    rows[1].w = 0.f;

    rows[2].x = 0.f;
    rows[2].y = 0.f;
    rows[2].z = value;
    rows[2].w = 0.f;

    rows[3].x = 0.f;
    rows[3].y = 0.f;
    rows[3].z = 0.f;
    rows[3].w = value;
  }

  static inline void scale (Matrix4* matrix, const Vector3* vector) noexcept {
    Vector4 temp;
    Vector4::mulScalar (&matrix->rows[0], vector->x, &temp);
    matrix->rows[0] = temp;
    Vector4::mulScalar (&matrix->rows[1], vector->y, &temp);
    matrix->rows[1] = temp;
    Vector4::mulScalar (&matrix->rows[2], vector->z, &temp);
    matrix->rows[2] = temp;
  }

  static inline void translate (Matrix4* matrix, const Vector3* vector) noexcept {
    // matrix->rows[3] =
    //   matrix->rows[0] * vector->x +
    //   matrix->rows[1] * vector->y +
    //   matrix->rows[2] * vector->z + matrix->rows[3];

    Vector4 a, b, c;
    Vector4::mulScalar (&matrix->rows[0], vector->x, &a);
    Vector4::mulScalar (&matrix->rows[1], vector->y, &b);
    Vector4::mulScalar (&matrix->rows[2], vector->z, &c);

    Vector4::add (&a, &b, &b);
    Vector4::add (&b, &c, &c);
    Vector4::add (&c, &matrix->rows[3], &matrix->rows[3]);
  }

  static inline void mul (const Matrix4* a, const Matrix4* b, Matrix4* out) noexcept {
    // out[0] = a[0] * b[0].x + a[1] * b[0].y + a[2] * b[0].z + a[3] * b[0].w;
    // out[1] = a[0] * b[1].x + a[1] * b[1].y + a[2] * b[1].z + a[3] * b[1].w;
    // out[2] = a[0] * b[2].x + a[1] * b[2].y + a[2] * b[2].z + a[3] * b[2].w;
    // out[3] = a[0] * b[3].x + a[1] * b[3].y + a[2] * b[3].z + a[3] * b[3].w;

    __m128 aRows[4];
    aRows[0] = a->rows[0].load ();
    aRows[1] = a->rows[1].load ();
    aRows[2] = a->rows[2].load ();
    aRows[3] = a->rows[3].load ();

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
};

void lookAt (const Vector3* eye, const Vector3* center, const Vector3* up, Matrix4* out);
void perspective (Float32 fov, Float32 aspect, Float32 zNear, Float32 zFar, Matrix4* out);

}

#endif /* MATH_HPP */

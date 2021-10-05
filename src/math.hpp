#ifndef MATH_HPP
#define MATH_HPP

#include <math.h>
#include <immintrin.h>

#define PI 3.14159265359f
// PI / 180. Used for to convert degrees to radians
#define PI_BY_180 0.01745329251994329576923690768489f

namespace math {

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
  // just like the first row of the formula.
  const int leftMask = _MM_SHUFFLE (3, 0, 2, 1);
  const int rightMask = _MM_SHUFFLE (3, 1, 0, 2);

  __m128 rows[4];
  rows[0] = _mm_shuffle_ps (a, a, leftMask);
  rows[1] = _mm_shuffle_ps (b, b, rightMask);
  rows[2] = _mm_shuffle_ps (a, a, rightMask);
  rows[3] = _mm_shuffle_ps (b, b, leftMask);

  return _mm_sub_ps (_mm_mul_ps (rows[0], rows[2]), _mm_mul_ps (rows[1], rows[3]));
}

}

struct alignas (16) Vector3 {
  float x, y, z;

  Vector3 () = default;
  Vector3 (float x, float y, float z) : x {x}, y {y}, z {z} {}
  Vector3 (float scalar) : x {scalar}, y {scalar}, z {scalar} {}

  inline __m128 load () const noexcept {
    return _mm_set_ps (0.f, z, y, x);
  }

  inline Vector3& operator += (float scalar) noexcept {
    _mm_store_ps (&this->x, _mm_add_ps (_mm_set1_ps (scalar), this->load ()));
    return *this;
  }

  inline Vector3& operator += (const Vector3& other) noexcept {
    _mm_store_ps (&this->x, _mm_add_ps (this->load (), other.load ()));
    return *this;
  }

  inline Vector3& operator *= (float scalar) noexcept {
    _mm_store_ps (&this->x, _mm_mul_ps (_mm_set1_ps (scalar), this->load ()));
    return *this;
  }

  inline Vector3& operator *= (const Vector3& other) noexcept {
    _mm_store_ps (&this->x, _mm_mul_ps (this->load (), other.load ()));
    return *this;
  }

  static inline void normalize (Vector3& output) noexcept {
    _mm_store_ps (&output.x, simd::m128::normalize (output.load ()));
  }

  [[nodiscard]] static inline float dotProduct (const Vector3& a, const Vector3& b) noexcept {
    return _mm_cvtss_f32 (simd::m128::dotProduct (a.load (), b.load ()));
  }

  static inline void crossProduct (const Vector3& a, const Vector3& b, Vector3& output) noexcept {
    _mm_store_ps (&output.x, simd::m128::crossProduct (a.load (), b.load ()));
  }
};

inline Vector3 operator + (const Vector3& a, const Vector3& b) noexcept {
  Vector3 result;
  _mm_store_ps (&result.x, _mm_add_ps (a.load (), b.load ()));
  return result;
}

inline Vector3 operator * (const Vector3& a, const Vector3& b) noexcept {
  Vector3 result;
  _mm_store_ps (&result.x, _mm_mul_ps (a.load (), b.load ()));
  return result;
}

[[nodiscard]] constexpr inline float radians (float degrees) noexcept {
  return degrees * PI_BY_180;
}

}

#endif /* MATH_HPP */

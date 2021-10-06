#include "math.hpp"

namespace rei::math {

void lookAt (
  const Vector3& eye,
  const Vector3& center,
  const Vector3& up,
  Matrix4& output) noexcept {

  using namespace simd;
  auto eyeM128 = eye.load ();
  auto z = m128::normalize (_mm_sub_ps (center.load (), eyeM128));
  auto x = m128::normalize (m128::crossProduct (z, up.load ()));
  auto y = m128::crossProduct (x, z);

  float dotXEye = _mm_cvtss_f32 (m128::dotProduct (x, eyeM128));
  float dotYEye = _mm_cvtss_f32 (m128::dotProduct (y, eyeM128));
  float dotZEye = _mm_cvtss_f32 (m128::dotProduct (z, eyeM128));

  struct {
    Vector3 x, y, z;
  } result;

  z = m128::negate (z);

  _mm_store_ps (&result.x.x, x);
  _mm_store_ps (&result.y.x, y);
  _mm_store_ps (&result.z.x, z);

  output.rows[0].x = result.x.x;
  output.rows[0].y = result.y.x;
  output.rows[0].z = result.z.x;
  output.rows[0].w = 0.f;

  output.rows[1].x = result.x.y;
  output.rows[1].y = result.y.y;
  output.rows[1].z = result.z.y;
  output.rows[1].w = 0.f;

  output.rows[2].x = result.x.z;
  output.rows[2].y = result.y.z;
  output.rows[2].z = result.z.z;
  output.rows[2].w = 0.f;

  output.rows[3].x = -dotXEye;
  output.rows[3].y = -dotYEye;
  output.rows[3].z = dotZEye;
  output.rows[3].w = 1.f;
}

// This function is basically stolen from GLM
void perspective (
  float verticalFOV,
  float aspectRatio,
  float zNear,
  float zFar,
  Matrix4& output) noexcept {

  float zLength = zFar - zNear;
  float focalLength = 1.f / tanf (verticalFOV / 2.f);

  output[0] = {focalLength / aspectRatio, 0.f, 0.f, 0.f};
  output[1] = {0.f, -focalLength, 0.f, 0.f};
  output[2] = {0.f, 0.f, -(zFar + zNear) / zLength , -1.f};
  output[3] = {0.f, 0.f, -(2 * zFar * zNear) / zLength, 0.f};
}

}

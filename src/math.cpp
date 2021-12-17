#include <math.h>
#include "math.hpp"

namespace rei::math {

void lookAt (const Vector3* eye, const Vector3* center, const Vector3* up, Matrix4* out) {
  using namespace simd;
  auto eyeM128 = eye->load ();
  auto z = m128::normalize (_mm_sub_ps (center->load (), eyeM128));
  auto x = m128::normalize (m128::crossProduct (z, up->load ()));
  auto y = m128::crossProduct (x, z);

  f32 dotXEye = _mm_cvtss_f32 (m128::dotProduct (x, eyeM128));
  f32 dotYEye = _mm_cvtss_f32 (m128::dotProduct (y, eyeM128));
  f32 dotZEye = _mm_cvtss_f32 (m128::dotProduct (z, eyeM128));

  struct {
    Vector3 x, y, z;
  } result;

  z = m128::negate (z);

  _mm_store_ps (&result.x.x, x);
  _mm_store_ps (&result.y.x, y);
  _mm_store_ps (&result.z.x, z);

  out->rows[0].x = result.x.x;
  out->rows[0].y = result.y.x;
  out->rows[0].z = result.z.x;
  out->rows[0].w = 0.f;

  out->rows[1].x = result.x.y;
  out->rows[1].y = result.y.y;
  out->rows[1].z = result.z.y;
  out->rows[1].w = 0.f;

  out->rows[2].x = result.x.z;
  out->rows[2].y = result.y.z;
  out->rows[2].z = result.z.z;
  out->rows[2].w = 0.f;

  out->rows[3].x = -dotXEye;
  out->rows[3].y = -dotYEye;
  out->rows[3].z = dotZEye;
  out->rows[3].w = 1.f;
}

// This function is basically stolen from GLM
void perspective (f32 fov, f32 aspect, f32 zNear, f32 zFar, Matrix4* out) {
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

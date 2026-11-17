// Small vector helpers shared by host and device code.
#pragma once
#include <cmath>

#ifdef __CUDACC__
#define BH_HD __host__ __device__
#else
#define BH_HD
#endif

namespace bh {

BH_HD inline float  sq(float x)  { return sqrtf(x); }
BH_HD inline double sq(double x) { return sqrt(x); }
BH_HD inline float  at2(float y, float x)  { return atan2f(y, x); }
BH_HD inline double at2(double y, double x){ return atan2(y, x); }
BH_HD inline float  ab(float x)  { return fabsf(x); }
BH_HD inline double ab(double x) { return fabs(x); }
BH_HD inline float  cosf_like(float x)  { return cosf(x); }
BH_HD inline double cosf_like(double x) { return cos(x); }
BH_HD inline float  sinf_like(float x)  { return sinf(x); }
BH_HD inline double sinf_like(double x) { return sin(x); }
BH_HD inline float  acosf_like(float x)  { return acosf(x); }
BH_HD inline double acosf_like(double x) { return acos(x); }

template <class T>
BH_HD inline T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

template <class T>
struct V3 {
  T x, y, z;
  BH_HD V3() : x(0), y(0), z(0) {}
  BH_HD V3(T a, T b, T c) : x(a), y(b), z(c) {}
  BH_HD T dot(const V3& o) const { return x * o.x + y * o.y + z * o.z; }
  BH_HD V3 cross(const V3& o) const {
    return V3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
  }
  BH_HD T len() const { return sq(dot(*this)); }
  BH_HD V3 operator+(const V3& o) const { return V3(x + o.x, y + o.y, z + o.z); }
  BH_HD V3 operator-(const V3& o) const { return V3(x - o.x, y - o.y, z - o.z); }
  BH_HD V3 operator*(T s) const { return V3(x * s, y * s, z * s); }
  BH_HD V3 normalized() const {
    T l = len();
    return l > T(1e-30) ? V3(x / l, y / l, z / l) : V3(0, 0, 0);
  }
};

using V3d = V3<double>;
using V3f = V3<float>;

}  // namespace bh

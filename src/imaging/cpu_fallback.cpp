// CPU reference / fallback implementation of the ray tracer.
//
// It mirrors src/imaging/raytracer.cu step for step but uses double precision
// and std::thread, so the app still works without a CUDA toolkit and the GPU
// output can be cross-checked against it.
#include "imaging/render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

#include "core/metric.h"

namespace bh {
namespace {

struct C3 {
  double x, y, z;
};

inline double clampd(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline C3 blackbody(double t) {
  t = clampd(t, 0.0, 1.6);
  if (t < 0.5) {
    const double s = t / 0.5;
    return {0.55 + 0.45 * s, 0.06 + 0.36 * s * s, 0.02 + 0.06 * s * s};
  }
  if (t < 1.0) {
    const double s = (t - 0.5) / 0.5;
    return {1.0, 0.42 + 0.5 * s, 0.08 + 0.72 * s};
  }
  const double s = (t - 1.0) / 0.6;
  return {1.0 - 0.25 * s, 0.92 - 0.05 * s, 0.8 + 0.2 * s};
}

inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

inline double rand01(uint32_t h) {
  return double(h & 0xFFFFFFu) / 16777216.0;
}

C3 skyColor(const C3& d, uint64_t seed) {
  const double PI = 3.14159265358979323846;
  const double u = std::atan2(d.y, d.x) / (2.0 * PI) + 0.5;
  const double v = std::acos(clampd(d.z, -1.0, 1.0)) / PI;

  C3 col{0.004, 0.006, 0.013};
  const double band = std::exp(-std::pow((d.z - 0.18 * d.x) * 3.2, 2.0));
  col.x += 0.010 * band;
  col.y += 0.012 * band;
  col.z += 0.022 * band;

  const double N = 700.0, M = 350.0;
  const int iu = int(std::floor(u * N));
  const int iv = int(std::floor(v * M));
  const uint32_t s32 = uint32_t(seed);

  for (int dv = -1; dv <= 1; ++dv) {
    for (int du = -1; du <= 1; ++du) {
      const int cu = iu + du, cv = iv + dv;
      if (cv < 0 || cv >= int(M)) continue;
      const uint32_t h = hash32(uint32_t(cu * 73856093) ^ uint32_t(cv * 19349663) ^ s32);
      if ((h & 1023u) > 34u) continue;
      const double su = (double(cu) + rand01(hash32(h ^ 0x9e3779b9u))) / N;
      const double sv = (double(cv) + rand01(hash32(h ^ 0x85ebca6bu))) / M;
      const double th = sv * PI;
      const double ph = su * 2.0 * PI - PI;
      const C3 sd{std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph),
                  std::cos(th)};
      const double dotv = clampd(sd.x * d.x + sd.y * d.y + sd.z * d.z, -1.0, 1.0);
      const double ang = std::acos(dotv);
      const double mag = rand01(hash32(h ^ 0x27d4eb2fu));
      const double width = 0.00028 + 0.0006 * mag;
      const double fall = std::exp(-(ang * ang) / (2.0 * width * width));
      if (fall < 0.003) continue;
      const double bright = (0.15 + 1.6 * mag * mag) * fall;
      const double temp = 0.35 + 1.1 * rand01(hash32(h ^ 0x165667b1u));
      const C3 sc = blackbody(temp);
      col.x += sc.x * bright;
      col.y += sc.y * bright;
      col.z += sc.z * bright;
    }
  }
  return col;
}

inline void inv3(const double m[3][3], double out[3][3]) {
  const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                     m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                     m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  const double id = (std::fabs(det) > 1e-20) ? 1.0 / det : 0.0;
  out[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * id;
  out[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * id;
  out[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * id;
  out[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * id;
  out[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id;
  out[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * id;
  out[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * id;
  out[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * id;
  out[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * id;
}

inline void rk4Step(double s[6], double pt, double a, double M, double h) {
  double k1[6], k2[6], k3[6], k4[6], tmp[6];
  ksRhs(s, pt, a, M, k1);
  for (int i = 0; i < 6; ++i) tmp[i] = s[i] + 0.5 * h * k1[i];
  ksRhs(tmp, pt, a, M, k2);
  for (int i = 0; i < 6; ++i) tmp[i] = s[i] + 0.5 * h * k2[i];
  ksRhs(tmp, pt, a, M, k3);
  for (int i = 0; i < 6; ++i) tmp[i] = s[i] + h * k3[i];
  ksRhs(tmp, pt, a, M, k4);
  for (int i = 0; i < 6; ++i)
    s[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
}

C3 diskShade(double x, double y, double a, double M, const double p[4], double gCam,
             double rIn, double rOut, double tempK) {
  const double r = ksRadius(x, y, 0.0, a);
  if (r < rIn || r > rOut) return {-1.0, -1.0, -1.0};

  KSGeom<double> g;
  ksEval(x, y, 0.0, a, M, g);
  const double kphi = x * g.ky - y * g.kx;
  const double gtt = -1.0 + g.f;
  const double gtp = g.f * kphi;
  const double gpp = 1.0 + g.f * kphi * kphi;

  const double Om = 1.0 / (std::pow(r, 1.5) + a);
  const double den = -(gtt + 2.0 * Om * gtp + Om * Om * gpp);
  if (den <= 1e-6) return {0.0, 0.0, 0.0};
  const double ut = 1.0 / std::sqrt(den);

  const double psi = std::atan2(y, x);
  const double ph = psi - std::atan2(a, r);
  const double cp = std::cos(ph), sp = std::sin(ph);
  const double dxdph = -(r * sp + a * cp);
  const double dydph = (r * cp - a * sp);

  const double uphi = Om * ut;
  const double ux = dxdph * uphi;
  const double uy = dydph * uphi;

  const double pudot = p[0] * ut + p[1] * ux + p[2] * uy;
  double gg = (std::fabs(pudot) > 1e-9) ? gCam / pudot : 1.0;
  gg = clampd(gg, 0.02, 20.0);

  const double tnorm = std::pow(rIn / r, 0.75) * tempK;
  const C3 c = blackbody(0.35 + 1.05 * tnorm);
  double fade = (rOut - r) / (0.30 * (rOut - rIn) + 1e-3);
  fade = clampd(fade, 0.0, 1.0);
  const double bright = 2.4 * std::pow(rIn / r, 1.6) * std::pow(gg, 3.0) * fade * fade;
  return {c.x * bright, c.y * bright, c.z * bright};
}

void renderRowRange(const RenderParams& P, int y0, int y1, unsigned char* out) {
  const double aspect = double(P.width) / double(P.height);
  const double tanHalf = P.tanHalfFov;
  const double aD = double(P.a), MD = double(P.M);
  const double camX = double(P.camPos[0]);
  const double camY = double(P.camPos[1]);
  const double camZ = double(P.camPos[2]);

  Tetrad<double> T;
  ksZamoTetrad(camX, camY, camZ, aD, MD, T);
  const double E[3][3] = {{T.e[1][1], T.e[2][1], T.e[3][1]},
                          {T.e[1][2], T.e[2][2], T.e[3][2]},
                          {T.e[1][3], T.e[2][3], T.e[3][3]}};
  double Ei[3][3];
  inv3(E, Ei);

  const double rplus = ksHorizon(MD, aD);

  for (int py = y0; py < y1; ++py) {
    for (int px = 0; px < P.width; ++px) {
      const double u = (2.0 * (px + 0.5) / P.width - 1.0) * tanHalf * aspect;
      const double v = (1.0 - 2.0 * (py + 0.5) / P.height) * tanHalf;

      double d[3] = {P.camFwd[0] + u * P.camRight[0] + v * P.camUp[0],
                     P.camFwd[1] + u * P.camRight[1] + v * P.camUp[1],
                     P.camFwd[2] + u * P.camRight[2] + v * P.camUp[2]};
      const double dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      d[0] /= dl;
      d[1] /= dl;
      d[2] /= dl;

      double n[3] = {Ei[0][0] * d[0] + Ei[0][1] * d[1] + Ei[0][2] * d[2],
                     Ei[1][0] * d[0] + Ei[1][1] * d[1] + Ei[1][2] * d[2],
                     Ei[2][0] * d[0] + Ei[2][1] * d[1] + Ei[2][2] * d[2]};
      const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
      n[0] /= nl;
      n[1] /= nl;
      n[2] /= nl;

      double p4[4];
      ksPhotonMomentum(camX, camY, camZ, aD, MD, T, n, p4);
      const double pt = p4[0];
      const double gCam = p4[0] * T.e[0][0] + p4[1] * T.e[0][1] +
                          p4[2] * T.e[0][2] + p4[3] * T.e[0][3];

      double s[6] = {camX, camY, camZ, p4[1], p4[2], p4[3]};
      C3 col{0, 0, 0};
      bool done = false;

      for (int step = 0; step < P.maxSteps && !done; ++step) {
        const double r = ksRadius(s[0], s[1], s[2], aD);
        if (r <= rplus * 1.002) {
          col = {0, 0, 0};
          break;
        }
        if (r > double(P.escapeR)) {
          const double l = std::sqrt(s[3] * s[3] + s[4] * s[4] + s[5] * s[5]);
          col = skyColor({s[3] / l, s[4] / l, s[5] / l}, P.seed);
          break;
        }

        const double z0 = s[2], x0 = s[0], y0 = s[1];
        const double h = std::min(double(P.stepScale) * (1.0 + r), 1.5);
        rk4Step(s, pt, aD, MD, h);

        if (P.diskEnabled && z0 * s[2] < 0.0) {
          const double tt = z0 / (z0 - s[2]);
          const double cx = x0 + tt * (s[0] - x0);
          const double cy = y0 + tt * (s[1] - y0);
          double pd[4] = {pt, s[3], s[4], s[5]};
          const C3 dc = diskShade(cx, cy, aD, MD, pd, gCam, double(P.diskIn), double(P.diskOut),
                                  double(P.diskTemp));
          if (dc.x >= 0.0) {
            col = dc;
            done = true;
            break;
          }
        }

        for (int bi = 0; bi < P.bodyCount && !done; ++bi) {
          const RenderBody& b = P.bodies[bi];
          const double ax = b.x - x0, ay = b.y - y0, az = b.z - z0;
          const double bx = s[0] - x0, by = s[1] - y0, bz = s[2] - z0;
          const double bb = bx * bx + by * by + bz * bz;
          double t = (bb > 1e-12) ? (ax * bx + ay * by + az * bz) / bb : 0.0;
          t = clampd(t, 0.0, 1.0);
          const double qx = ax - t * bx, qy = ay - t * by, qz = az - t * bz;
          if (qx * qx + qy * qy + qz * qz < double(b.radius) * b.radius) {
            col = {b.r, b.g, b.b};
            done = true;
          }
        }
      }

      double o[3];
      for (int i = 0; i < 3; ++i) {
        const double* c = (i == 0) ? &col.x : (i == 1) ? &col.y : &col.z;
        double v2 = 1.0 - std::exp(-(*c) * 1.6);
        v2 = std::pow(v2, 1.0 / 2.2);
        o[i] = clampd(v2 * 255.0, 0.0, 255.0);
      }
      unsigned char* px4 = out + (size_t(py) * P.width + px) * 4;
      px4[0] = (unsigned char)o[0];
      px4[1] = (unsigned char)o[1];
      px4[2] = (unsigned char)o[2];
      px4[3] = 255;
    }
  }
}

}  // namespace

const char* renderLastError() { return "cpu fallback"; }

int renderDeviceCount() { return 0; }

bool renderDeviceInfo(char* out, int cap) {
  std::snprintf(out, cap, "CPU reference tracer (%u threads)",
                std::thread::hardware_concurrency());
  return true;
}

void renderShutdown() {}

bool renderImage(const RenderParams& p, unsigned char* rgbaOut) {
  const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
  const int threads = int(std::min<unsigned>(hw, 16));
  std::vector<std::thread> pool;
  const int rows = p.height;
  const int chunk = std::max(1, rows / threads);
  for (int y = 0; y < rows; y += chunk) {
    const int y1 = std::min(rows, y + chunk);
    pool.emplace_back(renderRowRange, std::cref(p), y, y1, rgbaOut);
  }
  for (auto& t : pool) t.join();
  return true;
}

}  // namespace bh




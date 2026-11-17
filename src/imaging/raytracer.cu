// GPU ray tracer: backwards null geodesics in Kerr-Schild Kerr spacetime.
#include <cuda_runtime.h>
#include <cstdint>
#include <cstdio>

#include "core/math.h"
#include "core/metric.h"
#include "imaging/render.h"

namespace {

__device__ __forceinline__ uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

__device__ __forceinline__ float rand01(uint32_t h) {
  return (float)(h & 0xFFFFFFu) / 16777216.0f;
}

__device__ __forceinline__ float3 blackbody(float t) {
  // t in [0, 1.6]; hand-tuned ramp from deep red to blue-white
  t = fminf(fmaxf(t, 0.0f), 1.6f);
  float3 c;
  if (t < 0.5f) {
    float s = t / 0.5f;
    c = make_float3(0.55f + 0.45f * s, 0.06f + 0.36f * s * s, 0.02f + 0.06f * s * s);
  } else if (t < 1.0f) {
    float s = (t - 0.5f) / 0.5f;
    c = make_float3(1.0f, 0.42f + 0.5f * s, 0.08f + 0.72f * s);
  } else {
    float s = (t - 1.0f) / 0.6f;
    c = make_float3(1.0f - 0.25f * s, 0.92f - 0.05f * s, 0.8f + 0.2f * s);
  }
  return c;
}

__device__ __forceinline__ float3 skyColor(float3 d, uint64_t seed) {
  const float PI = 3.14159265358979f;
  float u = atan2f(d.y, d.x) / (2.0f * PI) + 0.5f;
  float v = acosf(fminf(fmaxf(d.z, -1.0f), 1.0f)) / PI;

  float3 col = make_float3(0.004f, 0.006f, 0.013f);

  // faint galactic band
  float band = expf(-powf((d.z - 0.18f * d.x) * 3.2f, 2.0f));
  col.x += 0.010f * band;
  col.y += 0.012f * band;
  col.z += 0.022f * band;

  const float N = 700.0f, M = 350.0f;
  const int iu = (int)floorf(u * N);
  const int iv = (int)floorf(v * M);
  const uint32_t s32 = (uint32_t)seed;

  for (int dv = -1; dv <= 1; ++dv) {
    for (int du = -1; du <= 1; ++du) {
      int cu = iu + du, cv = iv + dv;
      if (cv < 0 || cv >= (int)M) continue;
      uint32_t h = hash32((uint32_t)(cu * 73856093) ^ (uint32_t)(cv * 19349663) ^ s32);
      if ((h & 1023u) > 34u) continue;  // star density
      float su = ((float)cu + rand01(hash32(h ^ 0x9e3779b9u))) / N;
      float sv = ((float)cv + rand01(hash32(h ^ 0x85ebca6bu))) / M;
      float th = sv * PI;
      float ph = su * 2.0f * PI - PI;
      float3 sd = make_float3(sinf(th) * cosf(ph), sinf(th) * sinf(ph), cosf(th));
      float dotv = fminf(fmaxf(sd.x * d.x + sd.y * d.y + sd.z * d.z, -1.0f), 1.0f);
      float ang = acosf(dotv);
      float mag = rand01(hash32(h ^ 0x27d4eb2fu));
      float width = 0.00028f + 0.0006f * mag;
      float fall = expf(-(ang * ang) / (2.0f * width * width));
      if (fall < 0.003f) continue;
      float bright = (0.15f + 1.6f * mag * mag) * fall;
      float temp = 0.35f + 1.1f * rand01(hash32(h ^ 0x165667b1u));
      float3 sc = blackbody(temp);
      col.x += sc.x * bright;
      col.y += sc.y * bright;
      col.z += sc.z * bright;
    }
  }
  return col;
}

__device__ __forceinline__ void inv3(const float m[3][3], float out[3][3]) {
  float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
              m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
              m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  float id = (fabsf(det) > 1e-20f) ? 1.0f / det : 0.0f;
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

__device__ __forceinline__ void rk4Step(float s[6], float pt, float a, float M,
                                        float h) {
  float k1[6], k2[6], k3[6], k4[6], tmp[6];
  bh::ksRhs(s, pt, a, M, k1);
  for (int i = 0; i < 6; ++i) tmp[i] = s[i] + 0.5f * h * k1[i];
  bh::ksRhs(tmp, pt, a, M, k2);
  for (int i = 0; i < 6; ++i) tmp[i] = s[i] + 0.5f * h * k2[i];
  bh::ksRhs(tmp, pt, a, M, k3);
  for (int i = 0; i < 6; ++i) tmp[i] = s[i] + h * k3[i];
  bh::ksRhs(tmp, pt, a, M, k4);
  for (int i = 0; i < 6; ++i)
    s[i] += h / 6.0f * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
}

// Redshift factor and disk colour at an equatorial crossing.
__device__ __forceinline__ float3 diskShade(float x, float y, float a, float M,
                                            const float p[4], float gCam,
                                            float rIn, float rOut, float tempK) {
  float r = bh::ksRadius(x, y, 0.0f, a);
  if (r < rIn || r > rOut) return make_float3(-1.0f, -1.0f, -1.0f);

  bh::KSGeom<float> g;
  bh::ksEval(x, y, 0.0f, a, M, g);
  const float kphi = x * g.ky - y * g.kx;
  const float gtt = -1.0f + g.f;
  const float gtp = g.f * kphi;
  const float gpp = 1.0f + g.f * kphi * kphi;

  const float Om = 1.0f / (powf(r, 1.5f) + a);
  float den = -(gtt + 2.0f * Om * gtp + Om * Om * gpp);
  if (den <= 1e-6f) return make_float3(0.0f, 0.0f, 0.0f);
  const float ut = 1.0f / sqrtf(den);

  const float r2 = r * r;
  const float q = r2 + a * a;
  const float ct = 0.0f, st = 1.0f;
  const float psi = atan2f(y, x);
  const float ph = psi - atan2f(a, r);
  const float cp = cosf(ph), sp = sinf(ph);
  const float dxdph = -(r * sp + a * cp) * st;
  const float dydph = (r * cp - a * sp) * st;

  const float uphi = Om * ut;
  const float ux = dxdph * uphi;
  const float uy = dydph * uphi;

  const float pudot = p[0] * ut + p[1] * ux + p[2] * uy;
  float gg = (fabsf(pudot) > 1e-9f) ? gCam / pudot : 1.0f;
  gg = fminf(fmaxf(gg, 0.02f), 20.0f);

  const float tnorm = powf(rIn / r, 0.75f) * tempK;
  float3 c = blackbody(0.35f + 1.05f * tnorm);
  float fade = (rOut - r) / (0.30f * (rOut - rIn) + 1e-3f);
  fade = fminf(1.0f, fmaxf(0.0f, fade));
  const float bright = 2.4f * powf(rIn / r, 1.6f) * powf(gg, 3.0f) * fade * fade;
  return make_float3(c.x * bright, c.y * bright, c.z * bright);
}

__global__ void renderKernel(bh::RenderParams P, uchar4* out) {
  const int px = P.tileX + blockIdx.x * blockDim.x + threadIdx.x;
  const int py = P.tileY + blockIdx.y * blockDim.y + threadIdx.y;
  if (px >= P.tileX + P.tileW || py >= P.tileY + P.tileH) return;
  if (px >= P.width || py >= P.height) return;

  const float aspect = (float)P.width / (float)P.height;
  const float u = (2.0f * (px + 0.5f) / (float)P.width - 1.0f) * P.tanHalfFov * aspect;
  const float v = (1.0f - 2.0f * (py + 0.5f) / (float)P.height) * P.tanHalfFov;

  float d[3] = {P.camFwd[0] + u * P.camRight[0] + v * P.camUp[0],
                P.camFwd[1] + u * P.camRight[1] + v * P.camUp[1],
                P.camFwd[2] + u * P.camRight[2] + v * P.camUp[2]};
  {
    float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    l = (l > 1e-9f) ? l : 1.0f;
    d[0] /= l; d[1] /= l; d[2] /= l;
  }

  bh::Tetrad<float> T;
  bh::ksZamoTetrad(P.camPos[0], P.camPos[1], P.camPos[2], P.a, P.M, T);

  float E[3][3] = {{T.e[1][1], T.e[2][1], T.e[3][1]},
                   {T.e[1][2], T.e[2][2], T.e[3][2]},
                   {T.e[1][3], T.e[2][3], T.e[3][3]}};
  float Ei[3][3];
  inv3(E, Ei);
  float n[3] = {Ei[0][0] * d[0] + Ei[0][1] * d[1] + Ei[0][2] * d[2],
                Ei[1][0] * d[0] + Ei[1][1] * d[1] + Ei[1][2] * d[2],
                Ei[2][0] * d[0] + Ei[2][1] * d[1] + Ei[2][2] * d[2]};
  // The local sky direction must be a unit vector in the orthonormal frame,
  // otherwise the constructed photon momentum would not be null.
  {
    float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    nl = (nl > 1e-9f) ? nl : 1.0f;
    n[0] /= nl;
    n[1] /= nl;
    n[2] /= nl;
  }

  float p4[4];
  bh::ksPhotonMomentum(P.camPos[0], P.camPos[1], P.camPos[2], P.a, P.M, T, n, p4);
  const float pt = p4[0];
  float gCam = p4[0] * T.e[0][0] + p4[1] * T.e[0][1] + p4[2] * T.e[0][2] +
               p4[3] * T.e[0][3];

  float s[6] = {P.camPos[0], P.camPos[1], P.camPos[2], p4[1], p4[2], p4[3]};
  const float rplus = bh::ksHorizon(P.M, P.a);

  float3 col = make_float3(0.0f, 0.0f, 0.0f);
  bool done = false;

  for (int step = 0; step < P.maxSteps; ++step) {
    const float r = bh::ksRadius(s[0], s[1], s[2], P.a);
    if (r <= rplus * 1.002f) {
      col = make_float3(0.0f, 0.0f, 0.0f);
      done = true;
      break;
    }
    if (r > P.escapeR) {
      float3 dd = make_float3(s[3], s[4], s[5]);
      float l = sqrtf(dd.x * dd.x + dd.y * dd.y + dd.z * dd.z);
      l = (l > 1e-9f) ? l : 1.0f;
      col = skyColor(make_float3(dd.x / l, dd.y / l, dd.z / l), P.seed);
      done = true;
      break;
    }

    const float z0 = s[2];
    const float x0 = s[0], y0 = s[1];
    const float h = fminf(P.stepScale * (1.0f + r), 1.5f);
    rk4Step(s, pt, P.a, P.M, h);

    if (P.diskEnabled && z0 * s[2] < 0.0f) {
      const float tt = z0 / (z0 - s[2]);
      const float cx = x0 + tt * (s[0] - x0);
      const float cy = y0 + tt * (s[1] - y0);
      float pd[4] = {pt, s[3], s[4], s[5]};
      float3 dc = diskShade(cx, cy, P.a, P.M, pd, gCam, P.diskIn, P.diskOut, P.diskTemp);
      if (dc.x >= 0.0f) {
        col = dc;
        done = true;
        break;
      }
    }

    for (int bi = 0; bi < P.bodyCount; ++bi) {
      const bh::RenderBody& b = P.bodies[bi];
      float ax = b.x - x0, ay = b.y - y0, az = b.z - z0;
      float bx = s[0] - x0, by = s[1] - y0, bz = s[2] - z0;
      float bb = bx * bx + by * by + bz * bz;
      float t = (bb > 1e-12f) ? (ax * bx + ay * by + az * bz) / bb : 0.0f;
      t = fminf(fmaxf(t, 0.0f), 1.0f);
      float qx = ax - t * bx, qy = ay - t * by, qz = az - t * bz;
      if (qx * qx + qy * qy + qz * qz < b.radius * b.radius) {
        col = make_float3(b.r, b.g, b.b);
        done = true;
        break;
      }
    }
    if (done) break;
  }

  // tone map + gamma
  float3 o;
  o.x = 1.0f - expf(-col.x * 1.6f);
  o.y = 1.0f - expf(-col.y * 1.6f);
  o.z = 1.0f - expf(-col.z * 1.6f);
  o.x = powf(o.x, 1.0f / 2.2f);
  o.y = powf(o.y, 1.0f / 2.2f);
  o.z = powf(o.z, 1.0f / 2.2f);

  uchar4 c;
  c.x = (unsigned char)fminf(255.0f, fmaxf(0.0f, o.x * 255.0f));
  c.y = (unsigned char)fminf(255.0f, fmaxf(0.0f, o.y * 255.0f));
  c.z = (unsigned char)fminf(255.0f, fmaxf(0.0f, o.z * 255.0f));
  c.w = 255;
  out[py * P.width + px] = c;
}

}  // namespace

namespace bh {

static char g_err[512] = {0};

const char* renderLastError() { return g_err; }

int renderDeviceCount() {
  int n = 0;
  if (cudaGetDeviceCount(&n) != cudaSuccess) return 0;
  return n;
}

void renderShutdown() { cudaDeviceReset(); }

bool renderImage(const RenderParams& p, unsigned char* rgbaOut) {
  static uchar4* d_buf = nullptr;
  static size_t d_cap = 0;
  const size_t need = (size_t)p.width * p.height * sizeof(uchar4);

  cudaError_t e = cudaSetDevice(0);
  if (e != cudaSuccess) {
    snprintf(g_err, sizeof(g_err), "cudaSetDevice: %s", cudaGetErrorString(e));
    return false;
  }
  if (!d_buf || d_cap < need) {
    if (d_buf) cudaFree(d_buf);
    d_buf = nullptr;
    e = cudaMalloc(&d_buf, need);
    if (e != cudaSuccess) {
      snprintf(g_err, sizeof(g_err), "cudaMalloc(%zu): %s", need, cudaGetErrorString(e));
      d_cap = 0;
      return false;
    }
    d_cap = need;
  }
  cudaMemset(d_buf, 0, need);

  const int TS = 64;
  for (int ty = 0; ty < p.height; ty += TS) {
    for (int tx = 0; tx < p.width; tx += TS) {
      RenderParams q = p;
      q.tileX = tx;
      q.tileY = ty;
      q.tileW = (tx + TS > p.width) ? (p.width - tx) : TS;
      q.tileH = (ty + TS > p.height) ? (p.height - ty) : TS;
      dim3 block(8, 8);
      dim3 grid((q.tileW + 7) / 8, (q.tileH + 7) / 8);
      renderKernel<<<grid, block>>>(q, d_buf);
      e = cudaGetLastError();
      if (e != cudaSuccess) {
        snprintf(g_err, sizeof(g_err), "kernel: %s", cudaGetErrorString(e));
        return false;
      }
    }
  }
  e = cudaDeviceSynchronize();
  if (e != cudaSuccess) {
    snprintf(g_err, sizeof(g_err), "sync: %s", cudaGetErrorString(e));
    return false;
  }
  e = cudaMemcpy(rgbaOut, d_buf, need, cudaMemcpyDeviceToHost);
  if (e != cudaSuccess) {
    snprintf(g_err, sizeof(g_err), "memcpy: %s", cudaGetErrorString(e));
    return false;
  }
  return true;
}

}  // namespace bh



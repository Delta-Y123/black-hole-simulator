// Kerr spacetime in Kerr-Schild Cartesian coordinates, shared by CPU and CUDA.
//
//   g_{mu nu} = eta_{mu nu} + f k_mu k_nu
//   f  = 2 M r^3 / (r^4 + a^2 z^2)
//   k  = (1, (r x + a y)/(r^2+a^2), (r y - a x)/(r^2+a^2), z/r)
//   r  solves  r^4 - (rho^2 - a^2) r^2 - a^2 z^2 = 0
//
// The metric is horizon-penetrating, so null geodesics can be integrated
// straight through r = r_+ without coordinate blow-up.
#pragma once
#include "core/math.h"

namespace bh {

template <class T>
struct KSGeom {
  T r;
  T f;
  T kx, ky, kz;
  T drx, dry, drz;
  T dfx, dfy, dfz;
  T dkx[3], dky[3], dkz[3];
};

template <class T>
BH_HD inline T ksRadius(T x, T y, T z, T a) {
  const T a2 = a * a;
  const T rho2 = x * x + y * y + z * z;
  const T b = rho2 - a2;
  T disc = b * b + T(4) * a2 * z * z;
  if (disc < T(0)) disc = T(0);
  T r2 = T(0.5) * (b + sq(disc));
  return sq(r2 > T(1e-12) ? r2 : T(1e-12));
}

template <class T>
BH_HD inline void ksEval(T x, T y, T z, T a, T M, KSGeom<T>& g) {
  const T a2 = a * a;
  const T r = ksRadius(x, y, z, a);
  const T r2 = r * r;
  const T r3 = r2 * r;
  const T r4 = r2 * r2;
  const T A = r4 + a2 * z * z;
  const T q = r2 + a2;

  g.r = r;
  g.f = T(2) * M * r3 / A;
  g.kx = (r * x + a * y) / q;
  g.ky = (r * y - a * x) / q;
  g.kz = z / r;

  g.drx = x * r3 / A;
  g.dry = y * r3 / A;
  g.drz = z * q * r / A;

  const T dr[3] = {g.drx, g.dry, g.drz};
  const T dA[3] = {T(4) * r3 * dr[0], T(4) * r3 * dr[1],
                   T(4) * r3 * dr[2] + T(2) * a2 * z};
  const T nx = r * x + a * y;
  const T ny = r * y - a * x;
  const T A2 = A * A;

  for (int i = 0; i < 3; ++i) {
    const T dix = (i == 0) ? T(1) : T(0);
    const T diy = (i == 1) ? T(1) : T(0);
    const T diz = (i == 2) ? T(1) : T(0);

    // d f / d x^i
    const T df = T(2) * M * (T(3) * r2 * dr[i] * A - r3 * dA[i]) / A2;
    if (i == 0) g.dfx = df;
    else if (i == 1) g.dfy = df;
    else g.dfz = df;

    // d k_x, d k_y, d k_z
    const T dq = T(2) * r * dr[i];
    g.dkx[i] = ((dr[i] * x + r * dix + a * diy) * q - nx * dq) / (q * q);
    g.dky[i] = ((dr[i] * y + r * diy - a * dix) * q - ny * dq) / (q * q);
    g.dkz[i] = (diz * r - z * dr[i]) / (r * r);
  }
}

template <class T>
BH_HD inline T ksKdotP(const KSGeom<T>& g, T pt, const T p[3]) {
  return -pt + g.kx * p[0] + g.ky * p[1] + g.kz * p[2];
}

// 2H = g^{mu nu} p_mu p_nu  (= 0 for null, = -m^2 for a massive particle)
template <class T>
BH_HD inline T ksTwoH(const KSGeom<T>& g, T pt, const T p[3]) {
  const T kp = ksKdotP(g, pt, p);
  const T p2 = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
  return p2 - pt * pt - g.f * kp * kp;
}

// State layout: {x, y, z, p_x, p_y, p_z}; p_t is a constant parameter.
template <class T>
BH_HD inline void ksRhs(const T s[6], T pt, T a, T M, T out[6]) {
  KSGeom<T> g;
  ksEval(s[0], s[1], s[2], a, M, g);

  const T px = s[3], py = s[4], pz = s[5];
  const T kp = -pt + g.kx * px + g.ky * py + g.kz * pz;
  const T df[3] = {g.dfx, g.dfy, g.dfz};
  const T dkp[3] = {px * g.dkx[0] + py * g.dky[0] + pz * g.dkz[0],
                    px * g.dkx[1] + py * g.dky[1] + pz * g.dkz[1],
                    px * g.dkx[2] + py * g.dky[2] + pz * g.dkz[2]};

  out[0] = px - g.f * kp * g.kx;
  out[1] = py - g.f * kp * g.ky;
  out[2] = pz - g.f * kp * g.kz;
  out[3] = T(0.5) * df[0] * kp * kp + g.f * kp * dkp[0];
  out[4] = T(0.5) * df[1] * kp * kp + g.f * kp * dkp[1];
  out[5] = T(0.5) * df[2] * kp * kp + g.f * kp * dkp[2];
}

// dt/dlambda for the coordinate time readout.
template <class T>
BH_HD inline T ksDtDlambda(const KSGeom<T>& g, T pt, const T p[3]) {
  return -pt + g.f * ksKdotP(g, pt, p);
}

template <class T>
BH_HD inline T ksHorizon(T M, T a) {
  T d = M * M - a * a;
  return M + sq(d > T(0) ? d : T(0));
}

// Boyer-Lindquist coordinates plus the covariant momentum components
//   p_mu^BL = p_nu^KS dX^nu / dx^mu,
// reusing the same Jacobian as ksZamoTetrad. This is what an external
// conserved-quantity check Q = K^{mu nu} p_mu p_nu needs: it works with the
// (t, r, theta, phi) components while the integrator lives in Kerr-Schild.
//
// NB: t_KS = t_BL + integral(2Mr/delta) dr, so dX^t/dr = 2Mr/delta != 0 and
// p_r picks up a p_t * (2Mr/delta) term. Dropping it leaves p_r off by O(M/r)
// and makes every BL momentum expression (including 2H) look non-conserved.
template <class T>
BH_HD inline void ksBlState(T x, T y, T z, T a, T M, T pt, const T p[3],
                            T& r, T& th, T& ph,
                            T& pr, T& pth, T& pph) {
  const T rr = ksRadius(x, y, z, a);
  const T ct = clampv(z / rr, T(-1), T(1));
  T st = sq(T(1) - ct * ct);
  if (st < T(1e-8)) st = T(1e-8);
  const T psi = at2(y, x);
  const T phi = psi - at2(a, rr);
  const T cp = cosf_like(phi), sp = sinf_like(phi);

  const T Jxr = st * cp, Jyr = st * sp, Jzr = ct;
  const T Jxt = (rr * cp - a * sp) * ct;
  const T Jyt = (rr * sp + a * cp) * ct;
  const T Jzt = -rr * st;
  const T Jxp = -(rr * sp + a * cp) * st;
  const T Jyp = (rr * cp - a * sp) * st;

  T del = rr * rr - T(2) * M * rr + a * a;
  if (del < T(1e-6)) del = T(1e-6);
  const T Jtr = T(2) * M * rr / del;

  r = rr;
  th = acosf_like(ct);
  ph = phi;
  pr = pt * Jtr + p[0] * Jxr + p[1] * Jyr + p[2] * Jzr;
  pth = p[0] * Jxt + p[1] * Jyt + p[2] * Jzt;
  pph = p[0] * Jxp + p[1] * Jyp;  // dX_z/dphi = 0
}

// Boyer-Lindquist angles from Kerr-Schild Cartesian.
template <class T>
BH_HD inline void ksAngles(T x, T y, T z, T a, T& r, T& th, T& ph) {
  r = ksRadius(x, y, z, a);
  T ct = clampv(z / r, T(-1), T(1));
  th = acosf_like(ct);
  ph = at2(y, x) - at2(a, r);
}

template <class T>
struct Tetrad {
  T e[4][4];  // e[a][mu]: orthonormal frame vector a, KS components mu=(t,x,y,z)
};

// ZAMO tetrad at a point outside the horizon.
template <class T>
BH_HD inline void ksZamoTetrad(T x, T y, T z, T a, T M, Tetrad<T>& Td) {
  const T r = ksRadius(x, y, z, a);
  T ct = clampv(z / r, T(-1), T(1));
  T st = sq(T(1) - ct * ct);
  if (st < T(1e-8)) st = T(1e-8);
  const T psi = at2(y, x);
  const T phi = psi - at2(a, r);
  const T cp = cosf_like(phi), sp = sinf_like(phi);

  const T a2 = a * a;
  const T r2 = r * r;
  const T sig = r2 + a2 * ct * ct;
  T del = r2 - T(2) * M * r + a2;
  if (del < T(1e-6)) del = T(1e-6);
  const T AA = (r2 + a2) * (r2 + a2) - a2 * del * st * st;
  const T alpha = sq(sig * del / AA);
  const T omega = T(2) * M * a * r / AA;

  // BL contravariant components
  const T e0[4] = {T(1) / alpha, T(0), T(0), omega / alpha};
  const T er[4] = {T(0), sq(del / sig), T(0), T(0)};
  const T eth[4] = {T(0), T(0), T(1) / sq(sig), T(0)};
  const T eph[4] = {T(0), T(0), T(0), sq(sig / AA) / st};

  const T Jxr = st * cp;
  const T Jyr = st * sp;
  const T Jzr = ct;
  const T Jxt = (r * cp - a * sp) * ct;
  const T Jyt = (r * sp + a * cp) * ct;
  const T Jzt = -r * st;
  const T Jxp = -(r * sp + a * cp) * st;
  const T Jyp = (r * cp - a * sp) * st;
  const T Jtr = T(2) * M * r / del;

  const T* src[4] = {e0, er, eth, eph};
  for (int i = 0; i < 4; ++i) {
    const T* v = src[i];
    Td.e[i][0] = v[0] + Jtr * v[1];
    Td.e[i][1] = Jxr * v[1] + Jxt * v[2] + Jxp * v[3];
    Td.e[i][2] = Jyr * v[1] + Jyt * v[2] + Jyp * v[3];
    Td.e[i][3] = Jzr * v[1] + Jzt * v[2];
  }
}

// p_mu = g_{mu nu} p^nu
template <class T>
BH_HD inline void ksLower(T x, T y, T z, T a, T M, const T pUp[4], T pDn[4]) {
  KSGeom<T> g;
  ksEval(x, y, z, a, M, g);
  const T kp = pUp[0] + g.kx * pUp[1] + g.ky * pUp[2] + g.kz * pUp[3];
  pDn[0] = -pUp[0] + g.f * kp;
  pDn[1] = pUp[1] + g.f * g.kx * kp;
  pDn[2] = pUp[2] + g.f * g.ky * kp;
  pDn[3] = pUp[3] + g.f * g.kz * kp;
}

// Convert a world-space (Kerr-Schild Cartesian) direction into components of
// the local orthonormal frame: n = E^{-1} d, with E's columns the spatial triad
// vectors e_1, e_2, e_3. The result is NOT normalised.
template <class T>
BH_HD inline void ksWorldToLocal(const Tetrad<T>& Td, const T d[3], T n[3]) {
  const T E[3][3] = {{Td.e[1][1], Td.e[2][1], Td.e[3][1]},
                     {Td.e[1][2], Td.e[2][2], Td.e[3][2]},
                     {Td.e[1][3], Td.e[2][3], Td.e[3][3]}};
  const T det = E[0][0] * (E[1][1] * E[2][2] - E[1][2] * E[2][1]) -
                E[0][1] * (E[1][0] * E[2][2] - E[1][2] * E[2][0]) +
                E[0][2] * (E[1][0] * E[2][1] - E[1][1] * E[2][0]);
  const T id = (ab(det) > T(1e-20)) ? T(1) / det : T(0);
  const T Ei[3][3] = {
      {(E[1][1] * E[2][2] - E[1][2] * E[2][1]) * id,
       (E[0][2] * E[2][1] - E[0][1] * E[2][2]) * id,
       (E[0][1] * E[1][2] - E[0][2] * E[1][1]) * id},
      {(E[1][2] * E[2][0] - E[1][0] * E[2][2]) * id,
       (E[0][0] * E[2][2] - E[0][2] * E[2][0]) * id,
       (E[0][2] * E[1][0] - E[0][0] * E[1][2]) * id},
      {(E[1][0] * E[2][1] - E[1][1] * E[2][0]) * id,
       (E[0][1] * E[2][0] - E[0][0] * E[2][1]) * id,
       (E[0][0] * E[1][1] - E[0][1] * E[1][0]) * id}};
  n[0] = Ei[0][0] * d[0] + Ei[0][1] * d[1] + Ei[0][2] * d[2];
  n[1] = Ei[1][0] * d[0] + Ei[1][1] * d[1] + Ei[1][2] * d[2];
  n[2] = Ei[2][0] * d[0] + Ei[2][1] * d[1] + Ei[2][2] * d[2];
}

// Build the covariant 4-momentum of a photon leaving the observer in local
// sky direction n (unit vector in the ZAMO frame).
template <class T>
BH_HD inline void ksPhotonMomentum(T x, T y, T z, T a, T M,
                                   const Tetrad<T>& Td, const T n[3], T p[4]) {
  T up[4];
  for (int mu = 0; mu < 4; ++mu)
    up[mu] = Td.e[0][mu] + n[0] * Td.e[1][mu] + n[1] * Td.e[2][mu] +
             n[2] * Td.e[3][mu];
  ksLower(x, y, z, a, M, up, p);
}

// Build the covariant 4-momentum of a massive particle with local 3-velocity v
// (units of c, |v| < 1) in the ZAMO frame.
template <class T>
BH_HD inline void ksParticleMomentum(T x, T y, T z, T a, T M,
                                     const Tetrad<T>& Td, const T v[3], T m,
                                     T p[4]) {
  const T v2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  T g = T(1);
  if (v2 > T(1e-12)) {
    T den = T(1) - v2;
    if (den < T(1e-6)) den = T(1e-6);
    g = T(1) / sq(den);
  }
  T up[4];
  for (int mu = 0; mu < 4; ++mu)
    up[mu] = m * g * (Td.e[0][mu] + v[0] * Td.e[1][mu] +
                      v[1] * Td.e[2][mu] + v[2] * Td.e[3][mu]);
  ksLower(x, y, z, a, M, up, p);
}

}  // namespace bh

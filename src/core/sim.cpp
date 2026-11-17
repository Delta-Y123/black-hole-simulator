#include "core/sim.h"

#include <algorithm>
#include <cmath>

#include "core/metric.h"

namespace bh {

QString stopText(StopCode c) {
  switch (c) {
    case StopCode::Running: return QStringLiteral("running");
    case StopCode::Captured: return QStringLiteral("captured by horizon");
    case StopCode::Escaped: return QStringLiteral("escaped the integration box");
    case StopCode::Timeout: return QStringLiteral("affine parameter budget exhausted");
    case StopCode::Failed: return QStringLiteral("integration failed");
  }
  return QStringLiteral("unknown");
}

double carterConstant(double x, double y, double z, double a, double m, double E,
                      double Lz, const double p[3]) {
  const double r = ksRadius(x, y, z, a);
  double ct = z / r;
  ct = std::max(-1.0, std::min(1.0, ct));
  const double st2 = std::max(1e-12, 1.0 - ct * ct);
  const double st = std::sqrt(st2);
  const double ph = std::atan2(y, x) - std::atan2(a, r);

  // p_theta = p . dX/dtheta
  const double dxdth = (r * std::cos(ph) - a * std::sin(ph)) * ct;
  const double dydth = (r * std::sin(ph) + a * std::cos(ph)) * ct;
  const double dzdth = -r * st;
  const double pth = p[0] * dxdth + p[1] * dydth + p[2] * dzdth;

  return pth * pth + ct * ct * (a * a * (m * m - E * E) + Lz * Lz / st2);
}

namespace {

struct DP54 {
  // Dormand-Prince 5(4) coefficients
  static constexpr double c2 = 1.0 / 5.0, c3 = 3.0 / 10.0, c4 = 4.0 / 5.0,
                        c5 = 8.0 / 9.0;
  static constexpr double a21 = 1.0 / 5.0;
  static constexpr double a31 = 3.0 / 40.0, a32 = 9.0 / 40.0;
  static constexpr double a41 = 44.0 / 45.0, a42 = -56.0 / 15.0, a43 = 32.0 / 9.0;
  static constexpr double a51 = 19372.0 / 6561.0, a52 = -25360.0 / 2187.0,
                        a53 = 64448.0 / 6561.0, a54 = -212.0 / 729.0;
  static constexpr double a61 = 9017.0 / 3168.0, a62 = -355.0 / 33.0,
                        a63 = 46732.0 / 5247.0, a64 = 49.0 / 176.0,
                        a65 = -5103.0 / 18656.0;
  static constexpr double b1 = 35.0 / 384.0, b3 = 500.0 / 1113.0,
                        b4 = 125.0 / 192.0, b5 = -2187.0 / 6784.0,
                        b6 = 11.0 / 84.0;
  static constexpr double d1 = 5179.0 / 57600.0, d3 = 7571.0 / 16695.0,
                        d4 = 393.0 / 640.0, d5 = -92097.0 / 339200.0,
                        d6 = 187.0 / 2100.0, d7 = 1.0 / 40.0;
};

inline void step(const double y[6], double pt, double a, double M, double h,
                 double yout[6], double err[6]) {
  double k1[6], k2[6], k3[6], k4[6], k5[6], k6[6], k7[6], tmp[6];

  ksRhs(y, pt, a, M, k1);

  for (int i = 0; i < 6; ++i) tmp[i] = y[i] + h * DP54::a21 * k1[i];
  ksRhs(tmp, pt, a, M, k2);

  for (int i = 0; i < 6; ++i)
    tmp[i] = y[i] + h * (DP54::a31 * k1[i] + DP54::a32 * k2[i]);
  ksRhs(tmp, pt, a, M, k3);

  for (int i = 0; i < 6; ++i)
    tmp[i] = y[i] + h * (DP54::a41 * k1[i] + DP54::a42 * k2[i] + DP54::a43 * k3[i]);
  ksRhs(tmp, pt, a, M, k4);

  for (int i = 0; i < 6; ++i)
    tmp[i] = y[i] + h * (DP54::a51 * k1[i] + DP54::a52 * k2[i] +
                         DP54::a53 * k3[i] + DP54::a54 * k4[i]);
  ksRhs(tmp, pt, a, M, k5);

  for (int i = 0; i < 6; ++i)
    tmp[i] = y[i] + h * (DP54::a61 * k1[i] + DP54::a62 * k2[i] +
                         DP54::a63 * k3[i] + DP54::a64 * k4[i] +
                         DP54::a65 * k5[i]);
  ksRhs(tmp, pt, a, M, k6);

  for (int i = 0; i < 6; ++i)
    yout[i] = y[i] + h * (DP54::b1 * k1[i] + DP54::b3 * k3[i] +
                          DP54::b4 * k4[i] + DP54::b5 * k5[i] +
                          DP54::b6 * k6[i]);
  ksRhs(yout, pt, a, M, k7);

  for (int i = 0; i < 6; ++i) {
    const double y4 = y[i] + h * (DP54::d1 * k1[i] + DP54::d3 * k3[i] +
                                  DP54::d4 * k4[i] + DP54::d5 * k5[i] +
                                  DP54::d6 * k6[i] + DP54::d7 * k7[i]);
    err[i] = yout[i] - y4;
  }
}

}  // namespace

Trajectory traceBody(const Experiment& ex, const Body& b,
                     const IntegratorOptions& opt) {
  Trajectory tr;
  tr.photon = (b.kind == BodyKind::Photon);
  tr.m = tr.photon ? 0.0 : (b.mass > 0.0 ? b.mass : 1.0);

  const double M = ex.M, a = ex.a;
  const double rplus = ex.horizon();
  const double x0 = b.pos.x, y0 = b.pos.y, z0 = b.pos.z;
  const double r0 = ksRadius(x0, y0, z0, a);

  if (r0 <= rplus * 1.001) {
    tr.stop = StopCode::Failed;
    tr.message = QStringLiteral("initial position is inside the horizon");
    return tr;
  }

  Tetrad<double> td;
  ksZamoTetrad(x0, y0, z0, a, M, td);

  double p4[4];
  if (tr.photon) {
    V3d n = b.vel.normalized();
    double nn[3] = {n.x, n.y, n.z};
    ksPhotonMomentum(x0, y0, z0, a, M, td, nn, p4);
  } else {
    double v[3] = {b.vel.x, b.vel.y, b.vel.z};
    ksParticleMomentum(x0, y0, z0, a, M, td, v, tr.m, p4);
  }

  const double pt = p4[0];
  const double E = -pt;
  double y[6] = {x0, y0, z0, p4[1], p4[2], p4[3]};
  double tcoord = 0.0;

  tr.samples.reserve(opt.maxSamples);
  const double sampleEvery = std::max(1e-9, ex.duration / std::max(1, opt.maxSamples - 1));
  double nextSample = 0.0;

  auto pushSample = [&](double lam) {
    Sample s;
    s.x = V3d(y[0], y[1], y[2]);
    s.t = tcoord;
    s.lam = lam;
    s.E = E;
    s.Lz = y[0] * y[4] - y[1] * y[3];
    s.Qc = carterConstant(y[0], y[1], y[2], a, tr.m, E, s.Lz, &y[3]);
    KSGeom<double> g;
    ksEval(y[0], y[1], y[2], a, M, g);
    s.H2 = ksTwoH(g, pt, &y[3]);
    s.p_t = pt;
    s.ksp[0] = y[3];
    s.ksp[1] = y[4];
    s.ksp[2] = y[5];
    ksBlState(y[0], y[1], y[2], a, M, pt, &y[3], s.r, s.theta, s.phi,
              s.p_r, s.p_th, s.p_ph);
    tr.samples.push_back(s);
  };

  pushSample(0.0);
  nextSample = sampleEvery;

  double h = std::min(opt.hInit, opt.hMax);
  double lam = 0.0;
  int steps = 0;
  tr.stop = StopCode::Timeout;

  while (steps < opt.maxSteps) {
    if (lam >= ex.duration) {
      tr.stop = StopCode::Timeout;
      tr.message = QStringLiteral("affine parameter budget exhausted");
      break;
    }
    if (lam + h > ex.duration) h = ex.duration - lam;

    double ynew[6], err[6];
    step(y, pt, a, M, h, ynew, err);

    double enorm = 0.0;
    for (int i = 0; i < 6; ++i) {
      const double scale = opt.atol + opt.rtol * std::max(std::fabs(y[i]), std::fabs(ynew[i]));
      enorm = std::max(enorm, std::fabs(err[i]) / scale);
    }

    if (!std::isfinite(enorm)) {
      tr.stop = StopCode::Failed;
      tr.message = QStringLiteral("non-finite state encountered");
      break;
    }

    if (enorm <= 1.0) {
      // accepted
      KSGeom<double> g0, g1;
      ksEval(y[0], y[1], y[2], a, M, g0);
      ksEval(ynew[0], ynew[1], ynew[2], a, M, g1);
      const double dt0 = ksDtDlambda(g0, pt, &y[3]);
      const double dt1 = ksDtDlambda(g1, pt, &ynew[3]);
      tcoord += 0.5 * (dt0 + dt1) * h;

      for (int i = 0; i < 6; ++i) y[i] = ynew[i];
      lam += h;
      ++steps;

      if (lam >= nextSample) {
        pushSample(lam);
        nextSample += sampleEvery;
      }

      const double r = ksRadius(y[0], y[1], y[2], a);
      if (r <= rplus * 1.002 + 1e-6) {
        tr.stop = StopCode::Captured;
        tr.message = QStringLiteral("crossed the event horizon");
        break;
      }
      if (r > ex.maxR) {
        KSGeom<double> g;
        ksEval(y[0], y[1], y[2], a, M, g);
        const double dr = (y[0] * y[3] + y[1] * y[4] + y[2] * y[5]) / r;
        if (dr > 0.0) {
          tr.stop = StopCode::Escaped;
          tr.message = QStringLiteral("escaped to r > %1 M").arg(ex.maxR);
          break;
        }
      }
      double fac;
      if (enorm <= 1e-16) {
        // Error estimate vanished (nearly linear motion): let the step grow.
        fac = 5.0;
      } else {
        fac = 0.9 * std::pow(enorm, -0.2);
      }
      fac = std::min(5.0, std::max(0.2, fac));
      h = std::min(opt.hMax, h * fac);
    } else {
      h *= std::max(0.1, 0.9 * std::pow(enorm, -0.2));
      if (h < opt.hMin) {
        tr.stop = StopCode::Failed;
        tr.message = QStringLiteral("step size underflow");
        break;
      }
    }
  }

  tr.lastLam = lam;
  if (tr.samples.empty()) pushSample(lam);
  return tr;
}

}  // namespace bh

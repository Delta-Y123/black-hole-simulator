// Physics acceptance tests for the B-tier core.
// Build: cmake --build . --target bh_tests
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/metric.h"
#include "core/presets.h"
#include "core/scene.h"
#include "core/sim.h"

using namespace bh;

static int g_fail = 0;
static int g_pass = 0;

static void check(bool ok, const std::string& name, const std::string& detail = "") {
  if (ok) {
    ++g_pass;
    std::printf("  PASS  %s %s\n", name.c_str(), detail.c_str());
  } else {
    ++g_fail;
    std::printf("  FAIL  %s %s\n", name.c_str(), detail.c_str());
  }
}

static std::string fmt(const char* f, double a) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), f, a);
  return buf;
}

// ---------------------------------------------------------------------------
static void testMetricInverse() {
  std::printf("[metric] g^{mu nu} g_{nu lambda} = delta\n");
  const double pts[3][3] = {{8.0, 2.0, 1.0}, {-3.0, 4.5, 0.5}, {0.7, -1.2, 3.3}};
  for (double a : {0.0, 0.5, 0.9}) {
    double worst = 0.0;
    for (auto& p : pts) {
      KSGeom<double> g;
      ksEval(p[0], p[1], p[2], a, 1.0, g);
      double gdn[4][4] = {};
      double gup[4][4] = {};
      const double kdn[4] = {1.0, g.kx, g.ky, g.kz};
      const double kup[4] = {-1.0, g.kx, g.ky, g.kz};
      for (int mu = 0; mu < 4; ++mu)
        for (int nu = 0; nu < 4; ++nu) {
          const double eta = (mu == nu) ? ((mu == 0) ? -1.0 : 1.0) : 0.0;
          gdn[mu][nu] = eta + g.f * kdn[mu] * kdn[nu];
          gup[mu][nu] = eta - g.f * kup[mu] * kup[nu];
        }
      for (int mu = 0; mu < 4; ++mu)
        for (int nu = 0; nu < 4; ++nu) {
          double s = 0.0;
          for (int l = 0; l < 4; ++l) s += gup[mu][l] * gdn[l][nu];
          const double expect = (mu == nu) ? 1.0 : 0.0;
          worst = std::max(worst, std::fabs(s - expect));
        }
    }
    check(worst < 1e-12, "a=" + std::to_string(a), fmt(" max|err|=%.3e", worst));
  }
}

// ---------------------------------------------------------------------------
static void testDerivatives() {
  std::printf("[metric] analytic dH/dx vs finite differences\n");
  const double x = 5.3, y = -2.1, z = 1.7, a = 0.7, M = 1.0;
  const double p[3] = {0.31, -0.42, 0.55};
  const double pt = -1.0;
  double rhs[6];

  double s[6] = {x, y, z, p[0], p[1], p[2]};
  ksRhs(s, pt, a, M, rhs);

  const double h = 1e-6;
  double worst = 0.0;
  for (int i = 0; i < 3; ++i) {
    double sp[6] = {x, y, z, p[0], p[1], p[2]};
    double sm[6] = {x, y, z, p[0], p[1], p[2]};
    sp[i] += h;
    sm[i] -= h;
    KSGeom<double> gp, gm;
    ksEval(sp[0], sp[1], sp[2], a, M, gp);
    ksEval(sm[0], sm[1], sm[2], a, M, gm);
    const double dH = (ksTwoH(gp, pt, &sp[3]) - ksTwoH(gm, pt, &sm[3])) / (2 * h);
    worst = std::max(worst, std::fabs(-0.5 * dH - rhs[3 + i]));
  }
  check(worst < 1e-6, "dp/dlambda", fmt(" max|err|=%.3e", worst));
}

// ---------------------------------------------------------------------------
static void testCircularOrbit() {
  std::printf("[orbit] Schwarzschild circular orbit r = 10M\n");
  Experiment e = makePreset("schw_circular");
  IntegratorOptions opt;
  opt.rtol = 1e-10;
  Trajectory tr = traceBody(e, e.bodies[0], opt);
  check(tr.stop != StopCode::Failed, "integrates", tr.message.toStdString());

  double rmin = 1e9, rmax = 0.0, E = 0, L = 0;
  for (auto& s : tr.samples) {
    const double r = ksRadius(s.x.x, s.x.y, s.x.z, 0.0);
    rmin = std::min(rmin, r);
    rmax = std::max(rmax, r);
    E = s.E;
    L = s.Lz;
  }
  check(rmax - rmin < 2e-4, "radius stays constant", fmt(" dr=%.3e", rmax - rmin));

  const double Eexpect = (1.0 - 2.0 / 10.0) / std::sqrt(1.0 - 3.0 / 10.0);
  const double Lexpect = std::sqrt(10.0) / std::sqrt(1.0 - 3.0 / 10.0);
  check(std::fabs(E - Eexpect) < 1e-6, "specific energy", fmt(" E=%.6f", E));
  check(std::fabs(L - Lexpect) < 1e-6, "specific angular momentum", fmt(" L=%.6f", L));
}

// ---------------------------------------------------------------------------
static void testCarter() {
  std::printf("[orbit] Kerr inclined orbit: Carter constant drift\n");
  Experiment e = makePreset("kerr_inclined");
  IntegratorOptions opt;
  opt.rtol = 1e-10;
  Trajectory tr = traceBody(e, e.bodies[0], opt);
  check(tr.stop != StopCode::Failed, "integrates", tr.message.toStdString());
  if (tr.samples.size() < 10) {
    check(false, "enough samples", "");
    return;
  }
  double q0 = tr.samples.front().Qc;
  double worst = 0.0, h2 = 0.0;
  for (auto& s : tr.samples) {
    worst = std::max(worst, std::fabs(s.Qc - q0) / std::max(1e-9, std::fabs(q0)));
    h2 = std::max(h2, std::fabs(s.H2 + 1.0));
  }
  check(worst < 1e-6, "relative Q drift", fmt(" %.3e", worst));
  check(h2 < 1e-6, "normalization 2H+1", fmt(" %.3e", h2));
}

// ---------------------------------------------------------------------------
struct PhotonResult {
  bool captured = false;
  V3d dir{0, 0, 0};
  int stop = -1;
  size_t n = 0;
};

// Convert a world-space direction into the observer's local (ZAMO) frame,
// exactly like the camera ray construction used by the renderer.
static V3d localFromWorld(double x, double y, double z, double a, double M,
                          const V3d& d) {
  Tetrad<double> T;
  ksZamoTetrad(x, y, z, a, M, T);
  const double E[3][3] = {{T.e[1][1], T.e[2][1], T.e[3][1]},
                          {T.e[1][2], T.e[2][2], T.e[3][2]},
                          {T.e[1][3], T.e[2][3], T.e[3][3]}};
  const double det = E[0][0] * (E[1][1] * E[2][2] - E[1][2] * E[2][1]) -
                     E[0][1] * (E[1][0] * E[2][2] - E[1][2] * E[2][0]) +
                     E[0][2] * (E[1][0] * E[2][1] - E[1][1] * E[2][0]);
  const double id = 1.0 / det;
  const double Ei[3][3] = {
      {(E[1][1] * E[2][2] - E[1][2] * E[2][1]) * id,
       (E[0][2] * E[2][1] - E[0][1] * E[2][2]) * id,
       (E[0][1] * E[1][2] - E[0][2] * E[1][1]) * id},
      {(E[1][2] * E[2][0] - E[1][0] * E[2][2]) * id,
       (E[0][0] * E[2][2] - E[0][2] * E[2][0]) * id,
       (E[0][2] * E[1][0] - E[0][0] * E[1][2]) * id},
      {(E[1][0] * E[2][1] - E[1][1] * E[2][0]) * id,
       (E[0][1] * E[2][0] - E[0][0] * E[2][1]) * id,
       (E[0][0] * E[1][1] - E[0][1] * E[1][0]) * id}};
  const V3d n(Ei[0][0] * d.x + Ei[0][1] * d.y + Ei[0][2] * d.z,
              Ei[1][0] * d.x + Ei[1][1] * d.y + Ei[1][2] * d.z,
              Ei[2][0] * d.x + Ei[2][1] * d.y + Ei[2][2] * d.z);
  return n.normalized();
}

static PhotonResult shootPhoton(double b, double a, double rStart,
                                double duration = 6000.0) {
  Experiment e;
  e.a = a;
  e.M = 1.0;
  e.maxR = rStart * 2.0;
  e.duration = duration;
  Body p;
  p.kind = BodyKind::Photon;
  p.pos = V3d(-rStart, b, 0.0);
  p.vel = localFromWorld(p.pos.x, p.pos.y, p.pos.z, a, 1.0, V3d(1.0, 0.0, 0.0));
  IntegratorOptions opt;
  opt.rtol = 1e-8;
  opt.atol = 1e-10;
  opt.hMax = 20.0;
  opt.maxSteps = 200000;
  Trajectory tr = traceBody(e, p, opt);
  PhotonResult r;
  r.stop = int(tr.stop);
  r.n = tr.samples.size();
  r.captured = (tr.stop == StopCode::Captured);
  if (tr.stop == StopCode::Timeout && !tr.samples.empty()) {
    const V3d& last = tr.samples.back().x;
    r.captured = ksRadius(last.x, last.y, last.z, a) < 30.0;
  }
  if (tr.samples.size() >= 2) {
    const V3d& p1 = tr.samples[tr.samples.size() - 2].x;
    const V3d& p2 = tr.samples.back().x;
    r.dir = (p2 - p1).normalized();
  }
  return r;
}

static void testShadow() {
  std::printf("[light] Schwarzschild critical impact parameter\n");
  double lo = 4.0, hi = 7.0;
  for (int i = 0; i < 28; ++i) {
    const double mid = 0.5 * (lo + hi);
    if (shootPhoton(mid, 0.0, 1000.0).captured)
      lo = mid;
    else
      hi = mid;
  }
  const double bc = 0.5 * (lo + hi);
  check(std::fabs(bc - 3.0 * std::sqrt(3.0)) < 5e-3, "b_crit = 3 sqrt(3) M",
        fmt(" b=%.6f", bc));
}

static void testDeflection() {
  std::printf("[light] weak-field deflection 4M/b\n");
  const double b = 50.0;
  PhotonResult r = shootPhoton(b, 0.0, 2000.0, 20000.0);
  check(!r.captured, "escapes", "");
  const double alpha = std::atan2(-r.dir.y, r.dir.x);
  // 4M/b plus the second-order Schwarzschild term (15 pi / 4)(M/b)^2.
  const double expect = 4.0 / b + (15.0 * M_PI / 4.0) / (b * b);
  std::printf("        dir=(%.6f, %.6f, %.6f) stop=%d samples=%zu\n", r.dir.x,
              r.dir.y, r.dir.z, r.stop, r.n);
  check(std::fabs(alpha - expect) / expect < 0.02, "deflection angle",
        fmt(" alpha=%.6f", alpha) + fmt(" expect=%.6f", expect));
}

// ---------------------------------------------------------------------------
static void testPhotonSphere() {
  std::printf("[light] photon sphere r = 3M\n");
  // Tangential photon launched at r = 3M should stay near the photon sphere.
  Experiment e;
  e.a = 0.0;
  e.M = 1.0;
  e.maxR = 100.0;
  e.duration = 30.0;
  Body p;
  p.kind = BodyKind::Photon;
  p.pos = V3d(0.0, 3.0, 0.0);
  p.vel = V3d(0.0, 0.0, 1.0);
  IntegratorOptions opt;
  opt.rtol = 1e-11;
  Trajectory tr = traceBody(e, p, opt);
  double rmin = 1e9, rmax = 0.0;
  for (auto& s : tr.samples) {
    const double r = ksRadius(s.x.x, s.x.y, s.x.z, 0.0);
    rmin = std::min(rmin, r);
    rmax = std::max(rmax, r);
  }
  check(tr.stop != StopCode::Failed, "integrates", tr.message.toStdString());
  check(rmax - rmin < 5e-3, "stays on photon sphere", fmt(" dr=%.4f", rmax - rmin));
}

// ---------------------------------------------------------------------------
static void testJsonRoundtrip() {
  std::printf("[io] experiment JSON round trip\n");
  Experiment e = makePreset("kerr_inclined");
  const QJsonObject o = e.toJson();
  bool ok = false;
  Experiment e2 = Experiment::fromJson(o, &ok);
  check(ok, "parses", "");
  check(std::fabs(e2.a - e.a) < 1e-12 && std::fabs(e2.M - e.M) < 1e-12, "parameters",
        "");
  check(e2.bodies.size() == e.bodies.size(), "body count", "");
  check(std::fabs(e2.bodies[0].pos.x - e.bodies[0].pos.x) < 1e-9, "positions", "");
}

int main() {
  std::printf("Black Hole Simulator - core acceptance tests\n");
  testMetricInverse();
  testDerivatives();
  testCircularOrbit();
  testCarter();
  testPhotonSphere();
  testShadow();
  testDeflection();
  testJsonRoundtrip();
  std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}

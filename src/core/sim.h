#pragma once
#include <QString>
#include <vector>

#include "core/scene.h"

namespace bh {

struct Sample {
  V3d x;
  double t = 0.0;    // coordinate time
  double lam = 0.0;  // affine parameter
  double E = 0.0;
  double Lz = 0.0;
  double Qc = 0.0;   // Carter constant
  double H2 = 0.0;   // 2H, should be -m^2 (particles) or 0 (photons)

  // Boyer-Lindquist coordinates and covariant momentum. Exported so that an
  // external tool can evaluate any candidate Q = K^{mu nu} p_mu p_nu along
  // the orbit without re-deriving the Kerr-Schild <-> BL transform.
  double r = 0.0;
  double theta = 0.0;
  double phi = 0.0;
  double p_t = 0.0;
  double p_r = 0.0;
  double p_th = 0.0;
  double p_ph = 0.0;
  // Raw Kerr-Schild covariant momentum, kept for cross-checks of the
  // transformation above.
  double ksp[3] = {0.0, 0.0, 0.0};
};

enum class StopCode { Running = 0, Captured = 1, Escaped = 2, Timeout = 3, Failed = 4 };
QString stopText(StopCode c);

struct Trajectory {
  std::vector<Sample> samples;
  StopCode stop = StopCode::Failed;
  QString message;
  double m = 1.0;
  bool photon = false;
  double lastLam = 0.0;
};

struct IntegratorOptions {
  double rtol = 1e-9;
  double atol = 1e-11;
  double hInit = 0.05;
  double hMax = 4.0;
  double hMin = 1e-7;
  int maxSteps = 3000000;
  int maxSamples = 4000;
};

Trajectory traceBody(const Experiment& ex, const Body& b,
                     const IntegratorOptions& opt = IntegratorOptions());

// Carter constant Q = p_theta^2 + cos^2(th) [ a^2 (m^2 - E^2) + Lz^2 / sin^2(th) ]
double carterConstant(double x, double y, double z, double a, double m, double E,
                      double Lz, const double p[3]);

}  // namespace bh

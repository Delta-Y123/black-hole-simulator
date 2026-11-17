#pragma once
#include <QJsonObject>
#include <QString>
#include <vector>

#include "core/math.h"

namespace bh {

enum class BodyKind { Particle = 0, Photon = 1 };

struct Body {
  QString name;
  BodyKind kind = BodyKind::Particle;
  V3d pos;          // Kerr-Schild Cartesian position (M)
  V3d vel;          // local ZAMO 3-velocity (c units); photons: unit direction
  double mass = 1.0;
  double radius = 0.25;  // visual radius (M)
  QString color = QStringLiteral("#ff9a3c");
};

struct CameraSpec {
  V3d pos{-24.0, 0.0, 6.0};
  double yaw = 0.0;    // degrees, 0 = looking towards -x
  double pitch = 0.0;  // degrees
  double fov = 55.0;   // degrees
};

struct DiskSpec {
  bool enabled = true;
  double rIn = 6.0;
  double rOut = 24.0;
  double tempK = 1.0;  // relative brightness
};

struct Experiment {
  QString name = QStringLiteral("Untitled");
  double M = 1.0;
  double a = 0.0;
  double Q = 0.0;  // reserved (Kerr-Newman charge, not used in B tier)
  std::vector<Body> bodies;
  CameraSpec camera;
  DiskSpec disk;
  double duration = 600.0;  // affine parameter budget
  double maxR = 400.0;      // escape radius

  QJsonObject toJson() const;
  static Experiment fromJson(const QJsonObject& o, bool* ok = nullptr);

  double horizon() const;
  double ergoEquator() const { return M + std::sqrt(std::max(0.0, M * M - a * a)); }
  bool valid() const { return a * a < M * M && M > 0.0; }
};

QString kindToString(BodyKind k);
BodyKind kindFromString(const QString& s);

}  // namespace bh

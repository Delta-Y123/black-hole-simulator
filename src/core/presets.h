#pragma once
#include <QString>
#include <QVector>

#include "core/scene.h"

namespace bh {

struct PresetInfo {
  QString id;
  QString title;
  QString detail;
};

QVector<PresetInfo> presetList();
Experiment makePreset(const QString& id);

// Boyer-Lindquist (r, theta, phi) -> Kerr-Schild Cartesian
V3d ksFromBL(double r, double th, double ph, double a);

// Local (ZAMO) orbital speed for a prograde equatorial circular geodesic.
double circularSpeed(double r, double a, double M);

}  // namespace bh

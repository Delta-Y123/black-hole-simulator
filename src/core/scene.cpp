#include "core/scene.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace bh {

QString kindToString(BodyKind k) {
  return k == BodyKind::Photon ? QStringLiteral("photon") : QStringLiteral("particle");
}

BodyKind kindFromString(const QString& s) {
  return s.compare(QStringLiteral("photon"), Qt::CaseInsensitive) == 0
             ? BodyKind::Photon
             : BodyKind::Particle;
}

double Experiment::horizon() const {
  const double d = M * M - a * a;
  return M + std::sqrt(d > 0.0 ? d : 0.0);
}

QJsonObject Experiment::toJson() const {
  QJsonObject o;
  o["version"] = 1;
  o["name"] = name;
  o["M"] = M;
  o["a"] = a;
  o["Q"] = Q;
  o["duration"] = duration;
  o["maxR"] = maxR;

  QJsonObject camObj;
  camObj["x"] = camera.pos.x;
  camObj["y"] = camera.pos.y;
  camObj["z"] = camera.pos.z;
  camObj["yaw"] = camera.yaw;
  camObj["pitch"] = camera.pitch;
  camObj["fov"] = camera.fov;
  o["camera"] = camObj;

  QJsonObject diskObj;
  diskObj["enabled"] = disk.enabled;
  diskObj["rIn"] = disk.rIn;
  diskObj["rOut"] = disk.rOut;
  diskObj["temp"] = disk.tempK;
  o["disk"] = diskObj;

  QJsonArray arr;
  for (const Body& b : bodies) {
    QJsonObject bo;
    bo["name"] = b.name;
    bo["kind"] = kindToString(b.kind);
    bo["x"] = b.pos.x;
    bo["y"] = b.pos.y;
    bo["z"] = b.pos.z;
    bo["vx"] = b.vel.x;
    bo["vy"] = b.vel.y;
    bo["vz"] = b.vel.z;
    bo["mass"] = b.mass;
    bo["radius"] = b.radius;
    bo["color"] = b.color;
    arr.append(bo);
  }
  o["bodies"] = arr;
  return o;
}

static double num(const QJsonObject& o, const char* key, double def) {
  const QJsonValue v = o.value(QLatin1String(key));
  return v.isDouble() ? v.toDouble() : def;
}

Experiment Experiment::fromJson(const QJsonObject& o, bool* ok) {
  Experiment e;
  bool good = o.contains("bodies") || o.contains("M");
  e.name = o.value("name").toString(QStringLiteral("Untitled"));
  e.M = num(o, "M", 1.0);
  e.a = num(o, "a", 0.0);
  e.Q = num(o, "Q", 0.0);
  e.duration = num(o, "duration", 600.0);
  e.maxR = num(o, "maxR", 400.0);

  const QJsonObject cam = o.value("camera").toObject();
  e.camera.pos = V3d(num(cam, "x", -24.0), num(cam, "y", 0.0), num(cam, "z", 6.0));
  e.camera.yaw = num(cam, "yaw", 0.0);
  e.camera.pitch = num(cam, "pitch", 0.0);
  e.camera.fov = num(cam, "fov", 55.0);

  const QJsonObject disk = o.value("disk").toObject();
  e.disk.enabled = disk.value("enabled").toBool(true);
  e.disk.rIn = num(disk, "rIn", 6.0);
  e.disk.rOut = num(disk, "rOut", 24.0);
  e.disk.tempK = num(disk, "temp", 1.0);

  for (const QJsonValue& v : o.value("bodies").toArray()) {
    const QJsonObject bo = v.toObject();
    Body b;
    b.name = bo.value("name").toString();
    b.kind = kindFromString(bo.value("kind").toString());
    b.pos = V3d(num(bo, "x", 10.0), num(bo, "y", 0.0), num(bo, "z", 0.0));
    b.vel = V3d(num(bo, "vx", 0.0), num(bo, "vy", 0.0), num(bo, "vz", 0.0));
    b.mass = num(bo, "mass", 1.0);
    b.radius = num(bo, "radius", 0.25);
    b.color = bo.value("color").toString(QStringLiteral("#ff9a3c"));
    e.bodies.push_back(b);
  }
  if (ok) *ok = good && e.valid();
  return e;
}

}  // namespace bh

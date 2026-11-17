#include "core/presets.h"

#include <cmath>

namespace bh {

V3d ksFromBL(double r, double th, double ph, double a) {
  const double st = std::sin(th), ct = std::cos(th);
  const double cp = std::cos(ph), sp = std::sin(ph);
  return V3d((r * cp - a * sp) * st, (r * sp + a * cp) * st, r * ct);
}

double circularSpeed(double r, double a, double M) {
  const double r2 = r * r;
  const double del = r2 - 2.0 * M * r + a * a;
  if (del <= 0.0) return 0.0;
  const double sig = r2;  // equatorial
  const double A = (r2 + a * a) * (r2 + a * a) - a * a * del;
  const double omega = 2.0 * M * a * r / A;
  const double Om = 1.0 / (std::pow(r, 1.5) + a);
  const double v = (Om - omega) * A / (sig * std::sqrt(del));
  return std::fabs(v);
}

namespace {

void aimAtOrigin(CameraSpec& c) {
  const V3d f = (V3d(0, 0, 0) - c.pos).normalized();
  c.yaw = std::atan2(f.y, f.x) * 180.0 / M_PI;
  c.pitch = std::asin(std::max(-1.0, std::min(1.0, f.z))) * 180.0 / M_PI;
}

Experiment base(double a, const QString& name) {
  Experiment e;
  e.name = name;
  e.a = a;
  e.M = 1.0;
  e.camera.pos = ksFromBL(42.0, 68.0 * M_PI / 180.0, 0.0, a);
  e.camera.fov = 50.0;
  aimAtOrigin(e.camera);
  e.disk.enabled = (a > 0.01);
  e.disk.rIn = (a > 0.01) ? 2.6 : 6.0;
  e.disk.rOut = 20.0;
  return e;
}

}  // namespace

QVector<PresetInfo> presetList() {
  return {
      {QStringLiteral("schw_circular"),
       QStringLiteral("Schwarzschild 圆轨道 r = 10M"),
       QStringLiteral("稳定圆轨道，用于检查角速度与守恒量。")},
      {QStringLiteral("schw_precess"),
       QStringLiteral("Schwarzschild 椭圆轨道（进动）"),
       QStringLiteral("偏心轨道，近星点逐圈进动。")},
      {QStringLiteral("schw_escape"),
       QStringLiteral("Schwarzschild 逃逸轨道"),
       QStringLiteral("局部速度高于逃逸速度，天体飞离。")},
      {QStringLiteral("schw_capture"),
       QStringLiteral("Schwarzschild 捕获轨道"),
       QStringLiteral("径向落入，穿过视界。")},
      {QStringLiteral("kerr_inclined"),
       QStringLiteral("Kerr 倾斜轨道 a = 0.9"),
       QStringLiteral("参考系拖曳使轨道面进动，Carter 常数守恒。")},
      {QStringLiteral("light_bend"),
       QStringLiteral("光线偏折 b = 5M"),
       QStringLiteral("单条零测地线从远处掠过黑洞。")},
      {QStringLiteral("light_beam"),
       QStringLiteral("平行光束（7 条）"),
       QStringLiteral("同一方向的平行光束，比较不同碰撞参数的偏折。")},
      {QStringLiteral("shadow"),
       QStringLiteral("黑洞阴影 a = 0.9"),
       QStringLiteral("观察者相机视图，含薄盘与背景星场。")},
  };
}

Experiment makePreset(const QString& id) {
  if (id == QLatin1String("schw_circular")) {
    Experiment e = base(0.0, QStringLiteral("Schwarzschild 圆轨道 r = 10M"));
    Body b;
    b.name = QStringLiteral("Star");
    b.kind = BodyKind::Particle;
    b.pos = ksFromBL(10.0, M_PI / 2.0, 0.0, 0.0);
    b.vel = V3d(0.0, 0.0, circularSpeed(10.0, 0.0, 1.0));
    b.radius = 0.35;
    b.color = QStringLiteral("#ffb347");
    e.bodies.push_back(b);
    e.duration = 400.0;
    e.disk.enabled = true;
    e.disk.rIn = 6.0;
    e.disk.rOut = 22.0;
    return e;
  }
  if (id == QLatin1String("schw_precess")) {
    Experiment e = base(0.0, QStringLiteral("Schwarzschild 椭圆轨道（进动）"));
    Body b;
    b.name = QStringLiteral("Comet");
    b.kind = BodyKind::Particle;
    b.pos = ksFromBL(14.0, M_PI / 2.0, 0.0, 0.0);
    b.vel = V3d(0.0, 0.0, 0.78 * circularSpeed(14.0, 0.0, 1.0));
    b.radius = 0.3;
    b.color = QStringLiteral("#7fd4ff");
    e.bodies.push_back(b);
    e.duration = 900.0;
    return e;
  }
  if (id == QLatin1String("schw_escape")) {
    Experiment e = base(0.0, QStringLiteral("Schwarzschild 逃逸轨道"));
    Body b;
    b.name = QStringLiteral("Probe");
    b.kind = BodyKind::Particle;
    b.pos = ksFromBL(10.0, M_PI / 2.0, 0.0, 0.0);
    b.vel = V3d(0.62, 0.0, 0.10);
    b.radius = 0.28;
    b.color = QStringLiteral("#8effa1");
    e.bodies.push_back(b);
    e.duration = 500.0;
    return e;
  }
  if (id == QLatin1String("schw_capture")) {
    Experiment e = base(0.0, QStringLiteral("Schwarzschild 捕获轨道"));
    Body b;
    b.name = QStringLiteral("Falling");
    b.kind = BodyKind::Particle;
    b.pos = ksFromBL(14.0, M_PI / 2.0, 0.0, 0.0);
    b.vel = V3d(-0.08, 0.0, 0.02);
    b.radius = 0.28;
    b.color = QStringLiteral("#ff7f7f");
    e.bodies.push_back(b);
    e.duration = 400.0;
    return e;
  }
  if (id == QLatin1String("kerr_inclined")) {
    Experiment e = base(0.9, QStringLiteral("Kerr 倾斜轨道 a = 0.9"));
    Body b;
    b.name = QStringLiteral("Star");
    b.kind = BodyKind::Particle;
    const double th = 62.0 * M_PI / 180.0;
    b.pos = ksFromBL(9.0, th, 0.0, 0.9);
    b.vel = V3d(0.0, 0.0, 0.52);
    b.radius = 0.32;
    b.color = QStringLiteral("#ffd166");
    e.bodies.push_back(b);
    e.duration = 700.0;
    e.disk.enabled = true;
    e.disk.rIn = 2.8;
    e.disk.rOut = 24.0;
    return e;
  }
  if (id == QLatin1String("light_bend")) {
    Experiment e = base(0.0, QStringLiteral("光线偏折 b = 5M"));
    Body b;
    b.name = QStringLiteral("Photon");
    b.kind = BodyKind::Photon;
    b.pos = V3d(-30.0, 0.0, 5.0);
    b.vel = V3d(1.0, 0.0, 0.0);
    b.radius = 0.12;
    b.color = QStringLiteral("#9fe8ff");
    e.bodies.push_back(b);
    e.duration = 200.0;
    return e;
  }
  if (id == QLatin1String("light_beam")) {
    Experiment e = base(0.0, QStringLiteral("平行光束（7 条）"));
    for (int i = -3; i <= 3; ++i) {
      Body b;
      b.name = QStringLiteral("Ray %1").arg(i + 4);
      b.kind = BodyKind::Photon;
      b.pos = V3d(-40.0, i * 2.0, 0.0);
      b.vel = V3d(1.0, 0.0, 0.0);
      b.radius = 0.06;
      b.color = QStringLiteral("#9fe8ff");
      e.bodies.push_back(b);
    }
    e.duration = 200.0;
    return e;
  }
  // shadow / default
  Experiment e = base(0.9, QStringLiteral("黑洞阴影 a = 0.9"));
  e.camera.pos = ksFromBL(34.0, 78.0 * M_PI / 180.0, 0.0, 0.9);
  e.camera.fov = 45.0;
  aimAtOrigin(e.camera);
  Body b;
  b.name = QStringLiteral("HotSpot");
  b.kind = BodyKind::Particle;
  b.pos = ksFromBL(7.5, M_PI / 2.0, 0.4, 0.9);
  b.vel = V3d(0.0, 0.0, 0.48);
  b.radius = 0.3;
  b.color = QStringLiteral("#ffd166");
  e.bodies.push_back(b);
  e.duration = 600.0;
  return e;
}

}  // namespace bh

#include "core/renderparams.h"

#include <QColor>

#include <algorithm>
#include <cmath>

namespace bh {

RenderParams buildRenderParams(const Experiment& ex, int w, int h, double lam,
                               const std::vector<Trajectory>& trajs) {
  RenderParams p;
  p.width = w;
  p.height = h;
  p.M = float(ex.M);
  p.a = float(ex.a);

  const double yr = ex.camera.yaw * M_PI / 180.0;
  const double pr = ex.camera.pitch * M_PI / 180.0;
  const V3d fwd(std::cos(pr) * std::cos(yr), std::cos(pr) * std::sin(yr),
                std::sin(pr));
  V3d right = fwd.cross(V3d(0, 0, 1)).normalized();
  if (right.len() < 1e-6) right = V3d(0, 1, 0);
  const V3d up = right.cross(fwd).normalized();

  p.camPos[0] = float(ex.camera.pos.x);
  p.camPos[1] = float(ex.camera.pos.y);
  p.camPos[2] = float(ex.camera.pos.z);
  p.camFwd[0] = float(fwd.x);
  p.camFwd[1] = float(fwd.y);
  p.camFwd[2] = float(fwd.z);
  p.camRight[0] = float(right.x);
  p.camRight[1] = float(right.y);
  p.camRight[2] = float(right.z);
  p.camUp[0] = float(up.x);
  p.camUp[1] = float(up.y);
  p.camUp[2] = float(up.z);
  p.tanHalfFov = float(std::tan(ex.camera.fov * M_PI / 360.0));

  p.diskEnabled = ex.disk.enabled ? 1 : 0;
  p.diskIn = float(ex.disk.rIn);
  p.diskOut = float(ex.disk.rOut);
  p.diskTemp = float(ex.disk.tempK);
  p.escapeR = float(std::max(4.0 * ex.maxR, 400.0));
  p.stepScale = 0.03f;
  p.maxSteps = 6000;

  p.bodyCount = 0;
  for (size_t i = 0; i < ex.bodies.size() && p.bodyCount < 24; ++i) {
    const Body& b = ex.bodies[i];
    V3d pos = b.pos;
    if (i < trajs.size() && !trajs[i].samples.empty()) {
      for (const auto& s : trajs[i].samples) {
        if (s.lam <= lam) pos = s.x;
        else break;
      }
    }
    const QColor c(b.color);
    RenderBody& rb = p.bodies[p.bodyCount++];
    rb.x = float(pos.x);
    rb.y = float(pos.y);
    rb.z = float(pos.z);
    rb.radius = float(std::max(0.02, b.radius));
    rb.r = float(c.redF());
    rb.g = float(c.greenF());
    rb.b = float(c.blueF());
  }
  return p;
}

}  // namespace bh

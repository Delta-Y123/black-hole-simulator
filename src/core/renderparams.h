#pragma once
#include <vector>

#include "core/scene.h"
#include "core/sim.h"
#include "imaging/render.h"

namespace bh {

// Builds the GPU/CPU render parameters from an experiment and (optional)
// pre-computed trajectories, evaluated at affine parameter `lam`.
RenderParams buildRenderParams(const Experiment& ex, int w, int h, double lam,
                               const std::vector<Trajectory>& trajs);

}  // namespace bh

#pragma once
#include <cstdint>

namespace bh {

struct RenderBody {
  float x, y, z;
  float radius;
  float r, g, b;
};

struct RenderParams {
  int width = 640;
  int height = 360;
  float M = 1.0f;
  float a = 0.0f;

  float camPos[3] = {-24.0f, 0.0f, 0.0f};
  float camFwd[3] = {1.0f, 0.0f, 0.0f};
  float camRight[3] = {0.0f, 1.0f, 0.0f};
  float camUp[3] = {0.0f, 0.0f, 1.0f};
  float tanHalfFov = 0.5f;

  int diskEnabled = 1;
  float diskIn = 6.0f;
  float diskOut = 22.0f;
  float diskTemp = 1.0f;

  int maxSteps = 4000;
  float escapeR = 1200.0f;
  float stepScale = 0.03f;

  int bodyCount = 0;
  RenderBody bodies[24];

  uint64_t seed = 12345;
  float time = 0.0f;

  int tileX = 0, tileY = 0, tileW = 0, tileH = 0;
};

// Renders the full image into host RGBA8 buffer (width*height*4).
// Returns false and fills lastError on failure.
bool renderImage(const RenderParams& p, unsigned char* rgbaOut);
const char* renderLastError();
int renderDeviceCount();
bool renderDeviceInfo(char* out, int cap);
void renderShutdown();

}  // namespace bh

#include "imaging/render.h"

#include <cuda_runtime.h>
#include <cstdio>

namespace bh {

bool renderWarmup() {
  int n = renderDeviceCount();
  if (n <= 0) return false;
  cudaError_t e = cudaFree(0);
  return e == cudaSuccess;
}

bool renderDeviceInfo(char* out, int cap) {
  cudaDeviceProp prop;
  if (cudaGetDeviceProperties(&prop, 0) != cudaSuccess) return false;
  int clockKHz = 0;
  cudaDeviceGetAttribute(&clockKHz, cudaDevAttrClockRate, 0);
  snprintf(out, cap, "%s (sm_%d%d, %.1f GB, %.0f MHz)", prop.name, prop.major,
           prop.minor, prop.totalGlobalMem / 1073741824.0, clockKHz / 1000.0);
  return true;
}

}  // namespace bh

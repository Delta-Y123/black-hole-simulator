# 构建说明

## 依赖

| 组件 | 最低版本 | 说明 |
|---|---|---|
| Windows | 10 / 11 x64 | 只在 Windows 上验证过 |
| Visual Studio Build Tools | 2022 (MSVC 14.4x) | 需要「使用 C++ 的桌面开发」工作负载 + Windows SDK |
| CMake | 3.28 | `cmake_minimum_required` 要求 |
| Ninja | 任意近期版本 | 也可用 MSBuild，但脚本默认 Ninja |
| Qt | 6.5（实测 6.8.3） | `msvc2022_64` 预编译包即可，需要 Core/Gui/Widgets/OpenGL/OpenGLWidgets |
| CUDA Toolkit | 12.6（实测 13.0） | 只需 nvcc + cudart；`sm_86` 及以上 |
| NVIDIA 驱动 | 支持上述 CUDA 版本 | 相机视图需要 |
| Inno Setup | 6.x | 仅打包安装程序时需要 |

可选：不装 CUDA 时用 `-DBH_ENABLE_CUDA=OFF`，会编译纯 CPU 的参考光追实现，功能完整但很慢。

## 构建

```powershell
# 自动探测 VS / Qt / CUDA 位置
powershell -ExecutionPolicy Bypass -File tools\build.ps1

# 只构建物理测试（不需要 CUDA）
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -TestsOnly

# 完全不用 CUDA
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -NoCuda
```

路径不在默认位置时，用环境变量覆盖：

```powershell
$env:BH_VCVARS     = 'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$env:BH_QT_PREFIX  = 'C:\Qt\6.8.3\msvc2022_64'
$env:BH_CUDA_ROOT  = 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0'
```

或者手动：

```bat
call "<vcvars64.bat>"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="<Qt>/6.8.3/msvc2022_64" ^
      -DCUDAToolkit_ROOT="<CUDA>"
cmake --build build --parallel
```

产物：

- `build\BlackHoleSimulator.exe` — 主程序
- `build\bh_tests.exe` — 物理验收测试
- `build\bh_cli.exe` — 无界面渲染器

## 打包

```powershell
# 1. 收集运行时（Qt DLL + 插件 + MSVC 运行库 + cudart）
#    见 docs/BUILD.md 下方的 dist 说明，或直接用 windeployqt
windeployqt --release --no-translations build\BlackHoleSimulator.exe

# 2. 编译安装包
"%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" installer.iss
```

## 运行

开发期需要 Qt 的 DLL 在 PATH 上：

```bat
set PATH=<Qt>\6.8.3\msvc2022_64\bin;%PATH%
build\BlackHoleSimulator.exe
```

## 物理验收

```bat
build\bh_tests.exe
```

覆盖项：

1. `g^{μν} g_{νλ} = δ` 数值恒等式（a = 0 / 0.5 / 0.9）
2. 解析导数与中心差分一致（`dp/dλ = -∂H/∂x`）
3. Schwarzschild 圆轨道 r = 10M：半径漂移 < 1e-4，E、L_z 与解析值一致
4. Kerr 倾斜轨道：Carter 常数相对漂移 < 1e-6，`2H = -m²`
5. 光子球 r = 3M：切向光线保持在该半径
6. 临界碰撞参数：二分法收敛到 `3√3 M = 5.196152`
7. 弱场偏折：b = 50M 时与 `4M/b + (15π/4)(M/b)²` 相差 < 2%
8. 场景 JSON 往返

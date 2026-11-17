# Black Hole Simulator — 实现说明（B 档核心版本）

## 1. 交付物

| 产物 | 位置 | 说明 |
|---|---|---|
| 安装包（单文件） | `installer_out\BlackHoleSimulator_Setup.exe` | 双击安装，开始菜单/桌面快捷方式 |
| 免安装目录 | `dist\` | 直接双击 `BlackHoleSimulator.exe` |
| 物理验收测试 | `build_cuda\bh_tests.exe` | 20 项，全部通过 |
| 命令行渲染器 | `dist\bh_cli.exe` | `--preset shadow --width 800 --height 450 --out a.png` |
| 源码 | `src\`、`tests\`、`tools\` | C++20 / Qt6 / OpenGL / CUDA |

## 2. 代码结构

```
src/core/          物理内核（无 Qt GUI 依赖）
  math.h           向量与设备/主机共用数学函数
  metric.h         Kerr-Schild 度规、ZAMO 标架、动量转换（host + device 共用）
  sim.h/.cpp       Dormand–Prince 5(4) 自适应积分、事件终止、守恒量采样
  scene.h/.cpp     场景模型 + JSON 存档
  presets.h/.cpp   8 个预设场景
  renderparams.*   场景 → 渲染参数
src/imaging/
  raytracer.cu     CUDA 反向零测地线追踪（星场、薄盘、多普勒、阴影）
  render_host.cpp  设备信息
  cpu_fallback.cpp 纯 CPU 参考实现（无 CUDA 时可用，也用于交叉验证）
src/ui/            Qt Widgets 界面、OpenGL 三维视图、相机视图、研究面板
src/app/main.cpp   入口（含 --screenshot 自检模式）
tests/             物理验收测试
tools/             构建脚本与 CLI 渲染器
```

## 3. 物理实现

- 坐标：Kerr-Schild 笛卡尔，视界处无坐标奇性，光线可直接穿过视界积分。
- 度规：`g_{μν} = η_{μν} + f k_μ k_ν`，`f = 2Mr³/(r⁴+a²z²)`，`r` 由四次方程解出。
- 运动方程：`H = ½ g^{μν} p_μ p_ν`，`dx/dλ = ∂H/∂p`，`dp/dλ = -∂H/∂x`，
  解析导数（含 `∂r/∂xⁱ` 闭式），已用中心差分交叉验证（误差 9e-11）。
- 初条件：在 ZAMO 正交标架中给局部速度/方向，经 Jacobian 变换到 Kerr-Schild 分量，
  再降指标得到协变动量。屏幕方向 → 局部天球方向同样走这条路径。
- 守恒量：`E = -p_t`、`L_z = x p_y - y p_x`、Carter `Q = p_θ² + cos²θ[a²(m²-E²) + L_z²/sin²θ]`，
  其中 `p_θ` 由 Kerr-Schild → Boyer-Lindquist 的解析 Jacobian 得到。
- 吸积盘：赤道薄盘，`T ∝ r^{-3/4}`，含 `g = (p·u_obs)/(p·u_emit)` 的红移/多普勒因子，
  强度 `∝ (r_in/r)^{1.6} g³`，外缘软衰减。

## 4. 验收结果（`bh_tests.exe`）

```
[metric] g^{μν} g_{νλ} = δ                       PASS  (a = 0 / 0.5 / 0.9, 误差 2e-16)
[metric] 解析导数 vs 中心差分                     PASS  (9e-11)
[orbit]  Schwarzschild 圆轨道 r=10M               PASS  (dr = 4e-9, E、L 与解析值一致)
[orbit]  Kerr 倾斜轨道 Carter 常数                PASS  (相对漂移 4e-11)
[light]  光子球 r=3M                              PASS
[light]  临界碰撞参数 b_c = 3√3 M                 PASS  (5.196152)
[light]  弱场偏折 4M/b + 二阶项                    PASS  (0.085083 vs 0.084712)
[io]     JSON 往返                                PASS
20 passed, 0 failed
```

## 5. 与 README 的差距（明确未做）

| README 要求 | 状态 |
|---|---|
| 三维场景、播放、预设、保存/导出 | 已实现 |
| Schwarzschild / Kerr 背景 | 已实现 |
| 中性测试天体 | 已实现 |
| 单条光线、平行光束 | 已实现（扇形光束未做） |
| 观察者成像：背景天球、发光物体、阴影、透镜 | 已实现（GPU） |
| 守恒量数值监测 + 导出 | 已实现 |
| 候选守恒量**导入**、符号残差 | **未实现**（README §7 预留接口） |
| 批量参数扫描 | **未实现** |
| 带电粒子（Kerr–Newman） | 未实现（扩展版本） |
| 偏振平行移动 | 未实现（扩展版本） |
| Maxwell 波动模块 | 未实现（独立扩展） |

## 6. 已知限制

- GPU 光线追踪用 FP32 + RK4 固定步长（`h ≈ 0.03(1+r)`），精度低于 CPU 轨迹；
  定量结论请以 CPU 的 DP5(4) 结果与守恒量漂移为准。
- 成像为单帧快照，没有运动模糊；相机不能进入 `1.35 r₊` 以内。
- 吸积盘是几何薄盘示意，不是辐射转移计算。
- 三维视图中的曲面与轨迹是坐标表示，不是等距嵌入。

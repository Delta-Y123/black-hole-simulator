#include "ui/mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "core/metric.h"
#include "core/presets.h"
#include "imaging/render.h"
#include "ui/addbodydialog.h"
#include "ui/glview.h"
#include "ui/imageview.h"
#include "ui/researchpanel.h"

namespace bh {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  buildUi();

  char gpu[256] = {0};
  QString gpuText;
  if (renderDeviceInfo(gpu, sizeof(gpu))) gpuText = QString::fromUtf8(gpu);
  statusBar()->showMessage(
      gpuText.isEmpty() ? QStringLiteral("CUDA device not available")
                        : QStringLiteral("CUDA: %1").arg(gpuText));

  connect(&playTimer_, &QTimer::timeout, this, &MainWindow::onTick);
  playTimer_.setInterval(30);
  connect(&renderTimer_, &QTimer::timeout, this, &MainWindow::doRender);
  renderTimer_.setSingleShot(true);

  applyExperiment(makePreset(QStringLiteral("schw_circular")));
}

void MainWindow::buildUi() {
  setWindowTitle(QStringLiteral("Black Hole Simulator"));
  resize(1480, 900);

  gl_ = new GLView(this);
  img_ = new ImageView(this);
  research_ = new ResearchPanel(this);

  auto* central = new QWidget(this);
  auto* hlay = new QHBoxLayout(central);
  hlay->setContentsMargins(0, 0, 0, 0);
  hlay->setSpacing(2);

  // ---- 场景设置 ----
  auto* sceneWidget = new QWidget(central);
  sceneWidget->setFixedWidth(370);
  auto* sl = new QVBoxLayout(sceneWidget);
  sl->setContentsMargins(8, 8, 8, 8);

  presetBox_ = new QComboBox(sceneWidget);
  for (const auto& p : presetList()) presetBox_->addItem(p.title, p.id);
  sl->addWidget(new QLabel(QStringLiteral("预设场景"), sceneWidget));
  sl->addWidget(presetBox_);

  auto* form = new QFormLayout();
  massSpin_ = new QDoubleSpinBox(sceneWidget);
  massSpin_->setRange(0.1, 10.0);
  massSpin_->setSingleStep(0.1);
  massSpin_->setDecimals(2);
  massSpin_->setValue(1.0);
  spinSpin_ = new QDoubleSpinBox(sceneWidget);
  spinSpin_->setRange(0.0, 0.99);
  spinSpin_->setSingleStep(0.05);
  spinSpin_->setDecimals(2);
  form->addRow(QStringLiteral("黑洞质量 M"), massSpin_);
  form->addRow(QStringLiteral("自旋 a/M"), spinSpin_);
  sl->addLayout(form);

  bodyList_ = new QListWidget(sceneWidget);
  bodyList_->setMaximumHeight(150);
  sl->addWidget(new QLabel(QStringLiteral("天体与光线"), sceneWidget));
  sl->addWidget(bodyList_);

  auto* btns = new QHBoxLayout();
  auto* addP = new QPushButton(QStringLiteral("+ 天体"), sceneWidget);
  auto* addPh = new QPushButton(QStringLiteral("+ 光线"), sceneWidget);
  auto* delB = new QPushButton(QStringLiteral("移除"), sceneWidget);
  btns->addWidget(addP);
  btns->addWidget(addPh);
  btns->addWidget(delB);
  sl->addLayout(btns);

  diskCheck_ = new QCheckBox(QStringLiteral("显示吸积盘"), sceneWidget);
  diskCheck_->setChecked(true);
  gridCheck_ = new QCheckBox(QStringLiteral("显示参考网格"), sceneWidget);
  gridCheck_->setChecked(true);
  trailCheck_ = new QCheckBox(QStringLiteral("显示轨迹"), sceneWidget);
  trailCheck_->setChecked(true);
  sl->addWidget(diskCheck_);
  sl->addWidget(gridCheck_);
  sl->addWidget(trailCheck_);

  auto* recomputeBtn = new QPushButton(QStringLiteral("重新计算轨迹"), sceneWidget);
  sl->addWidget(recomputeBtn);
  infoLabel_ = new QLabel(sceneWidget);
  infoLabel_->setWordWrap(true);
  infoLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  sl->addWidget(infoLabel_);
  sl->addStretch(1);

  sceneWidget->setObjectName(QStringLiteral("scenePanel"));
  sceneWidget->setStyleSheet(QStringLiteral(
      "#scenePanel{background:#1b1d22;border-right:1px solid #2a2d34;}"));

  // ---- 右侧：相机视图 / 物理与研究 ----
  tabs_ = new QTabWidget(central);
  tabs_->addTab(img_, QStringLiteral("观察者相机"));
  tabs_->addTab(research_, QStringLiteral("物理与研究"));
  tabs_->setFixedWidth(430);

  hlay->addWidget(sceneWidget);

  auto* glWrap = new QWidget(central);
  auto* glLay = new QVBoxLayout(glWrap);
  glLay->setContentsMargins(0, 0, 0, 0);
  glLay->setSpacing(0);
  glLay->addWidget(gl_, 1);
  auto* axisLegend = new QLabel(
      QStringLiteral("坐标轴：<span style='color:#e06a6a'>x</span> · "
                     "<span style='color:#5fd97a'>y</span> · "
                     "<span style='color:#5f9bff'>z</span>"
                     "　|　赤道网格每圈 10 M　|　单位 G=c=1"),
      glWrap);
  axisLegend->setTextFormat(Qt::RichText);
  axisLegend->setStyleSheet(
      QStringLiteral("color:#7d879b;background:#0f1116;padding:3px 10px;"));
  glLay->addWidget(axisLegend);

  hlay->addWidget(glWrap, 1);
  hlay->addWidget(tabs_);
  setCentralWidget(central);

  // ---- 播放控制 ----
  auto* bar = new QToolBar(QStringLiteral("播放"), this);
  bar->setMovable(false);
  addToolBar(Qt::BottomToolBarArea, bar);

  playBtn_ = new QPushButton(QStringLiteral("▶ 播放"), this);
  pauseBtn_ = new QPushButton(QStringLiteral("⏸ 暂停"), this);
  stepBtn_ = new QPushButton(QStringLiteral("单步"), this);
  resetBtn_ = new QPushButton(QStringLiteral("重置"), this);
  bar->addWidget(playBtn_);
  bar->addWidget(pauseBtn_);
  bar->addWidget(stepBtn_);
  bar->addWidget(resetBtn_);
  bar->addSeparator();

  bar->addWidget(new QLabel(QStringLiteral("速度"), this));
  speedSlider_ = new QSlider(Qt::Horizontal, this);
  speedSlider_->setRange(1, 200);
  speedSlider_->setValue(40);
  speedSlider_->setFixedWidth(150);
  bar->addWidget(speedSlider_);
  bar->addSeparator();
  timeLabel_ = new QLabel(QStringLiteral("λ = 0.0 M"), this);
  timeLabel_->setMinimumWidth(250);
  bar->addWidget(timeLabel_);
  bar->addSeparator();
  bar->addWidget(new QLabel(QStringLiteral("成像质量"), this));
  qualityBox_ = new QComboBox(this);
  qualityBox_->addItem(QStringLiteral("预览 480×270"), 0);
  qualityBox_->addItem(QStringLiteral("中等 800×450"), 1);
  qualityBox_->addItem(QStringLiteral("高清 1280×720"), 2);
  bar->addWidget(qualityBox_);

  // ---- 菜单 ----
  auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
  fileMenu->addAction(QStringLiteral("打开场景..."), this, &MainWindow::onLoad);
  fileMenu->addAction(QStringLiteral("保存场景..."), this, &MainWindow::onSave);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("导出轨迹 CSV..."), this, &MainWindow::onExportCsv);
  fileMenu->addAction(QStringLiteral("导出图像 PNG..."), this, &MainWindow::onExportPng);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);

  auto* helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
  helpMenu->addAction(QStringLiteral("关于"), this, [this] {
    QMessageBox::about(
        this, QStringLiteral("Black Hole Simulator"),
        QStringLiteral(
            "<b>Black Hole Simulator</b><br/>"
            "Kerr 背景中的测试粒子与光线测地线，观察者成像。<br/><br/>"
            "坐标：Kerr-Schild 笛卡尔，单位 G=c=1，长度以 M 计。<br/>"
            "粒子：H = ½ g^{μν} p_μ p_ν，DP5(4) 自适应积分；<br/>"
            "成像：GPU 反向追踪零测地线，阴影由光线命运决定。"));
  });

  connect(presetBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::onPresetChanged);
  connect(massSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onParamsChanged);
  connect(spinSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onParamsChanged);
  connect(recomputeBtn, &QPushButton::clicked, this, &MainWindow::onRecompute);
  connect(bodyList_, &QListWidget::currentRowChanged, this, &MainWindow::onBodySelected);
  connect(addP, &QPushButton::clicked, this, &MainWindow::onAddParticle);
  connect(addPh, &QPushButton::clicked, this, &MainWindow::onAddPhoton);
  connect(delB, &QPushButton::clicked, this, &MainWindow::onRemoveBody);
  connect(playBtn_, &QPushButton::clicked, this, &MainWindow::onPlay);
  connect(pauseBtn_, &QPushButton::clicked, this, &MainWindow::onPause);
  connect(stepBtn_, &QPushButton::clicked, this, &MainWindow::onStep);
  connect(resetBtn_, &QPushButton::clicked, this, &MainWindow::onReset);
  connect(speedSlider_, &QSlider::valueChanged, this,
          [this](int v) { speed_ = double(v); });
  connect(qualityBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { scheduleRender(); });
  connect(diskCheck_, &QCheckBox::toggled, this, [this](bool v) {
    ex_.disk.enabled = v;
    gl_->setExperiment(ex_);
    scheduleRender();
  });
  connect(gridCheck_, &QCheckBox::toggled, this, [this](bool v) { gl_->setShowGrid(v); });
  connect(trailCheck_, &QCheckBox::toggled, this, [this](bool v) { gl_->setShowTrails(v); });

  connect(img_, &ImageView::cameraDragged, this, &MainWindow::onCameraDragged);
  connect(img_, &ImageView::cameraMoved, this, &MainWindow::onCameraMoved);
  connect(img_, &ImageView::fovChanged, this, &MainWindow::onFovChanged);
}

void MainWindow::applyExperiment(const Experiment& ex) {
  ex_ = ex;
  massSpin_->blockSignals(true);
  spinSpin_->blockSignals(true);
  massSpin_->setValue(ex_.M);
  spinSpin_->setValue(ex_.a);
  massSpin_->blockSignals(false);
  spinSpin_->blockSignals(false);
  diskCheck_->blockSignals(true);
  diskCheck_->setChecked(ex_.disk.enabled);
  diskCheck_->blockSignals(false);

  refreshBodyList();
  recompute();
  gl_->setExperiment(ex_);
  gl_->setShowGrid(gridCheck_->isChecked());
  gl_->setShowTrails(trailCheck_->isChecked());
  lam_ = 0.0;
  gl_->setPlayback(0.0);
  updateCameraLabel();
  scheduleRender();
}

void MainWindow::refreshBodyList() {
  bodyList_->blockSignals(true);
  bodyList_->clear();
  for (const auto& b : ex_.bodies)
    bodyList_->addItem(QStringLiteral("%1  [%2]").arg(b.name,
        b.kind == BodyKind::Photon ? QStringLiteral("光子") : QStringLiteral("天体")));
  bodyList_->blockSignals(false);
  if (selected_ >= int(ex_.bodies.size())) selected_ = 0;
  if (!ex_.bodies.empty()) bodyList_->setCurrentRow(selected_);
}

void MainWindow::recompute() {
  trajs_.clear();
  trajs_.reserve(ex_.bodies.size());
  IntegratorOptions opt;
  for (const auto& b : ex_.bodies) trajs_.push_back(traceBody(ex_, b, opt));
  gl_->setTrajectories(trajs_);
  refreshResearch();

  if (!ex_.bodies.empty()) {
    const auto& t = trajs_[0];
    infoLabel_->setText(QStringLiteral(
                            "视界 r+ = %1 M<br/>赤道能层 rE = %2 M<br/>"
                            "轨迹采样 %3 点，终止：%4")
                            .arg(ex_.horizon(), 0, 'f', 3)
                            .arg(ex_.ergoEquator(), 0, 'f', 3)
                            .arg(t.samples.size())
                            .arg(t.message));
  }
}

void MainWindow::refreshResearch() {
  if (trajs_.empty() || selected_ < 0 || selected_ >= int(trajs_.size())) {
    research_->clear();
    return;
  }
  research_->setTrajectory(trajs_[selected_], ex_.bodies[selected_].name);
}

void MainWindow::onPresetChanged(int idx) {
  if (idx < 0) return;
  const QString id = presetBox_->itemData(idx).toString();
  applyExperiment(makePreset(id));
}

void MainWindow::onParamsChanged() {
  ex_.M = massSpin_->value();
  ex_.a = spinSpin_->value();
  if (!ex_.valid()) {
    statusBar()->showMessage(QStringLiteral("a 必须小于 M"), 3000);
    return;
  }
  recompute();
  gl_->setExperiment(ex_);
  scheduleRender();
}

void MainWindow::onRecompute() {
  recompute();
  scheduleRender();
}

void MainWindow::onPlay() {
  if (trajs_.empty()) return;
  playing_ = true;
  playTimer_.start();
}

void MainWindow::onPause() {
  playing_ = false;
  playTimer_.stop();
}

void MainWindow::onStep() {
  playing_ = false;
  playTimer_.stop();
  lam_ += 5.0;
  if (!trajs_.empty() && selected_ < int(trajs_.size()))
    lam_ = std::min(lam_, trajs_[selected_].lastLam);
  gl_->setPlayback(lam_);
  updateCameraLabel();
}

void MainWindow::onReset() {
  playing_ = false;
  playTimer_.stop();
  lam_ = 0.0;
  gl_->setPlayback(0.0);
  updateCameraLabel();
}

void MainWindow::onTick() {
  if (trajs_.empty() || selected_ >= int(trajs_.size())) return;
  const double maxLam = trajs_[selected_].lastLam;
  lam_ += speed_ * 0.03;
  if (lam_ >= maxLam) {
    lam_ = maxLam;
    playing_ = false;
    playTimer_.stop();
  }
  gl_->setPlayback(lam_);
  updateCameraLabel();
}

void MainWindow::updateCameraLabel() {
  double tc = 0.0;
  if (!trajs_.empty() && selected_ < int(trajs_.size())) {
    const auto& tr = trajs_[selected_];
    for (const auto& s : tr.samples) {
      if (s.lam <= lam_) tc = s.t;
      else break;
    }
  }
  timeLabel_->setText(QStringLiteral("λ = %1 M    t = %2 M")
                          .arg(lam_, 0, 'f', 1)
                          .arg(tc, 0, 'f', 1));
}

void MainWindow::onBodySelected(int row) {
  selected_ = std::max(0, row);
  gl_->setSelected(selected_);
  refreshResearch();
}

void MainWindow::onAddParticle() {
  AddBodyDialog dlg(ex_, BodyKind::Particle, this);
  if (dlg.exec() != QDialog::Accepted) return;
  ex_.bodies.push_back(dlg.body());
  selected_ = int(ex_.bodies.size()) - 1;
  refreshBodyList();
  recompute();
  scheduleRender();
}

void MainWindow::onAddPhoton() {
  AddBodyDialog dlg(ex_, BodyKind::Photon, this);
  if (dlg.exec() != QDialog::Accepted) return;
  ex_.bodies.push_back(dlg.body());
  selected_ = int(ex_.bodies.size()) - 1;
  refreshBodyList();
  recompute();
  scheduleRender();
}

void MainWindow::onRemoveBody() {
  if (ex_.bodies.empty()) return;
  const int row = bodyList_->currentRow();
  if (row < 0 || row >= int(ex_.bodies.size())) return;
  ex_.bodies.erase(ex_.bodies.begin() + row);
  refreshBodyList();
  recompute();
  scheduleRender();
}

void MainWindow::onSave() {
  const QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("保存场景"),
                                                  QStringLiteral("scene.json"),
                                                  QStringLiteral("JSON (*.json)"));
  if (fn.isEmpty()) return;
  QFile f(fn);
  if (!f.open(QIODevice::WriteOnly)) return;
  f.write(QJsonDocument(ex_.toJson()).toJson(QJsonDocument::Indented));
}

void MainWindow::onLoad() {
  const QString fn = QFileDialog::getOpenFileName(this, QStringLiteral("打开场景"),
                                                  QString(),
                                                  QStringLiteral("JSON (*.json)"));
  if (fn.isEmpty()) return;
  QFile f(fn);
  if (!f.open(QIODevice::ReadOnly)) return;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  bool ok = false;
  const Experiment e = Experiment::fromJson(doc.object(), &ok);
  if (!ok) {
    QMessageBox::warning(this, QStringLiteral("加载失败"),
                         QStringLiteral("不是有效的场景文件"));
    return;
  }
  applyExperiment(e);
}

void MainWindow::onExportCsv() {
  if (trajs_.empty() || selected_ >= int(trajs_.size())) return;
  const QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("导出轨迹"),
                                                  QStringLiteral("trajectory.csv"),
                                                  QStringLiteral("CSV (*.csv)"));
  if (fn.isEmpty()) return;
  QFile f(fn);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  f.write("lambda,t,x,y,z,E,Lz,Q,2H\n");
  for (const auto& s : trajs_[selected_].samples) {
    f.write(QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                .arg(s.lam, 0, 'g', 12)
                .arg(s.t, 0, 'g', 12)
                .arg(s.x.x, 0, 'g', 12)
                .arg(s.x.y, 0, 'g', 12)
                .arg(s.x.z, 0, 'g', 12)
                .arg(s.E, 0, 'g', 12)
                .arg(s.Lz, 0, 'g', 12)
                .arg(s.Qc, 0, 'g', 12)
                .arg(s.H2, 0, 'g', 12)
                .toUtf8());
  }
}

void MainWindow::onExportPng() {
  const QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("导出图像"),
                                                  QStringLiteral("image.png"),
                                                  QStringLiteral("PNG (*.png)"));
  if (fn.isEmpty()) return;
  const int q = qualityBox_->currentIndex();
  const int w = (q == 0) ? 480 : (q == 1 ? 800 : 1280);
  const int h = w * 9 / 16;
  std::vector<unsigned char> buf(size_t(w) * h * 4);
  RenderParams p = makeRenderParams(w, h);
  p.maxSteps = (q == 2) ? 12000 : 6000;
  if (!renderImage(p, buf.data())) {
    QMessageBox::warning(this, QStringLiteral("渲染失败"),
                         QString::fromUtf8(renderLastError()));
    return;
  }
  QImage out(buf.data(), w, h, w * 4, QImage::Format_RGBA8888);
  out.copy().save(fn);
}

RenderParams MainWindow::makeRenderParams(int w, int h) const {
  RenderParams p;
  p.width = w;
  p.height = h;
  p.M = float(ex_.M);
  p.a = float(ex_.a);

  const double yr = ex_.camera.yaw * M_PI / 180.0;
  const double pr = ex_.camera.pitch * M_PI / 180.0;
  V3d fwd(std::cos(pr) * std::cos(yr), std::cos(pr) * std::sin(yr), std::sin(pr));
  V3d worldUp(0, 0, 1);
  V3d right = fwd.cross(worldUp).normalized();
  if (right.len() < 1e-6) right = V3d(0, 1, 0);
  V3d up = right.cross(fwd).normalized();

  p.camPos[0] = float(ex_.camera.pos.x);
  p.camPos[1] = float(ex_.camera.pos.y);
  p.camPos[2] = float(ex_.camera.pos.z);
  p.camFwd[0] = float(fwd.x); p.camFwd[1] = float(fwd.y); p.camFwd[2] = float(fwd.z);
  p.camRight[0] = float(right.x); p.camRight[1] = float(right.y); p.camRight[2] = float(right.z);
  p.camUp[0] = float(up.x); p.camUp[1] = float(up.y); p.camUp[2] = float(up.z);
  p.tanHalfFov = float(std::tan(ex_.camera.fov * M_PI / 360.0));

  p.diskEnabled = ex_.disk.enabled ? 1 : 0;
  p.diskIn = float(ex_.disk.rIn);
  p.diskOut = float(ex_.disk.rOut);
  p.diskTemp = float(ex_.disk.tempK);
  p.escapeR = 1200.0f;
  p.stepScale = 0.03f;

  p.bodyCount = 0;
  for (size_t i = 0; i < ex_.bodies.size() && p.bodyCount < 24; ++i) {
    const Body& b = ex_.bodies[i];
    V3d pos = b.pos;
    if (i < trajs_.size() && !trajs_[i].samples.empty()) {
      for (const auto& s : trajs_[i].samples) {
        if (s.lam <= lam_) pos = s.x;
        else break;
      }
    }
    QColor c(b.color);
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

void MainWindow::scheduleRender() {
  if (!glReady_) glReady_ = true;
  renderTimer_.start(220);
}

void MainWindow::doRender() {
  if (rendering_) return;
  rendering_ = true;
  const int q = qualityBox_->currentIndex();
  const int w = (q == 0) ? 480 : (q == 1 ? 800 : 1280);
  const int h = w * 9 / 16;

  std::vector<unsigned char> buf(size_t(w) * h * 4);
  RenderParams p = makeRenderParams(w, h);
  p.maxSteps = (q == 2) ? 12000 : 6000;

  img_->setBusy(true);
  QElapsedTimer t;
  t.start();
  const bool ok = renderImage(p, buf.data());
  const double ms = t.nsecsElapsed() / 1e6;
  img_->setBusy(false);

  if (!ok) {
    statusBar()->showMessage(QString::fromUtf8(renderLastError()), 5000);
    rendering_ = false;
    return;
  }
  const QImage im(buf.data(), w, h, w * 4, QImage::Format_RGBA8888);
  img_->setImage(im.copy());
  img_->setCaption(QStringLiteral("%1×%2  %3 ms  a=%4")
                       .arg(w).arg(h).arg(ms, 0, 'f', 0)
                       .arg(ex_.a, 0, 'f', 2));
  rendering_ = false;
}

void MainWindow::onCameraDragged(double dyaw, double dpitch) {
  ex_.camera.yaw += dyaw;
  ex_.camera.pitch = std::clamp(ex_.camera.pitch + dpitch, -89.0, 89.0);
  updateCameraLabel();
  scheduleRender();
}

void MainWindow::onCameraMoved(double f, double r, double u) {
  const double yr = ex_.camera.yaw * M_PI / 180.0;
  const double pr = ex_.camera.pitch * M_PI / 180.0;
  const V3d fwd(std::cos(pr) * std::cos(yr), std::cos(pr) * std::sin(yr), std::sin(pr));
  const V3d worldUp(0, 0, 1);
  V3d right = fwd.cross(worldUp).normalized();
  if (right.len() < 1e-6) right = V3d(0, 1, 0);
  const V3d up = right.cross(fwd).normalized();

  V3d np = ex_.camera.pos + fwd * f + right * r + up * u;
  const double rn = ksRadius(np.x, np.y, np.z, ex_.a);
  if (rn > ex_.horizon() * 1.35) {
    ex_.camera.pos = np;
    gl_->setExperiment(ex_);
    scheduleRender();
  }
}

void MainWindow::onFovChanged(double factor) {
  ex_.camera.fov = std::clamp(ex_.camera.fov * factor, 8.0, 120.0);
  scheduleRender();
}

}  // namespace bh

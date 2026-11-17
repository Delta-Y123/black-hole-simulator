#include "ui/addbodydialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "core/metric.h"
#include "core/presets.h"
#include "core/sim.h"

namespace bh {

namespace {

QDoubleSpinBox* makeSpin(QWidget* parent, double lo, double hi, double step,
                         int decimals, double value) {
  auto* s = new QDoubleSpinBox(parent);
  s->setRange(lo, hi);
  s->setSingleStep(step);
  s->setDecimals(decimals);
  s->setValue(value);
  s->setKeyboardTracking(false);
  return s;
}

}  // namespace

AddBodyDialog::AddBodyDialog(const Experiment& ex, BodyKind kind, QWidget* parent)
    : QDialog(parent), ex_(ex), kind_(kind) {
  const bool photon = (kind == BodyKind::Photon);
  setWindowTitle(photon ? QStringLiteral("添加光子") : QStringLiteral("添加测试天体"));
  setMinimumWidth(520);

  auto* root = new QVBoxLayout(this);

  // ---- name ----
  auto* top = new QFormLayout();
  nameEdit_ = new QLineEdit(this);
  nameEdit_->setText(photon ? QStringLiteral("Ray")
                            : QStringLiteral("Body %1").arg(ex_.bodies.size() + 1));
  top->addRow(QStringLiteral("名称"), nameEdit_);
  root->addLayout(top);

  // ---- position ----
  auto* posBox = new QGroupBox(QStringLiteral("位置（Boyer–Lindquist 坐标，长度单位 M）"), this);
  auto* posForm = new QFormLayout(posBox);
  rSpin_ = makeSpin(posBox, 0.5, 500.0, 0.5, 3, photon ? 30.0 : 12.0);
  thetaSpin_ = makeSpin(posBox, 0.0, 180.0, 1.0, 2, 90.0);
  phiSpin_ = makeSpin(posBox, -360.0, 360.0, 5.0, 2, photon ? 180.0 : 0.0);
  rSpin_->setSuffix(QStringLiteral(" M"));
  thetaSpin_->setSuffix(QStringLiteral(" °"));
  phiSpin_->setSuffix(QStringLiteral(" °"));
  posForm->addRow(QStringLiteral("半径 r"), rSpin_);
  posForm->addRow(QStringLiteral("极角 θ（90° = 赤道）"), thetaSpin_);
  posForm->addRow(QStringLiteral("方位角 φ"), phiSpin_);
  auto* posHint = new QLabel(
      QStringLiteral("内部会换算成 Kerr-Schild 笛卡尔坐标；视界 r₊ = %1 M。")
          .arg(ex_.horizon(), 0, 'f', 3),
      posBox);
  posHint->setWordWrap(true);
  posHint->setStyleSheet(QStringLiteral("color:#8a93a6;"));
  posForm->addRow(posHint);
  root->addWidget(posBox);

  // ---- velocity / direction ----
  if (!photon) {
    auto* vBox = new QGroupBox(
        QStringLiteral("初速度（ZAMO 局部标架，单位 c，沿 r̂ / θ̂ / φ̂）"), this);
    auto* vForm = new QFormLayout(vBox);
    vrSpin_ = makeSpin(vBox, -0.999, 0.999, 0.01, 4, 0.0);
    vthSpin_ = makeSpin(vBox, -0.999, 0.999, 0.01, 4, 0.0);
    vphSpin_ = makeSpin(vBox, -0.999, 0.999, 0.01, 4,
                        circularSpeed(12.0, ex_.a, ex_.M));
    vForm->addRow(QStringLiteral("v_r"), vrSpin_);
    vForm->addRow(QStringLiteral("v_θ"), vthSpin_);
    vForm->addRow(QStringLiteral("v_φ"), vphSpin_);

    massSpin_ = makeSpin(vBox, 0.1, 10.0, 0.1, 2, 1.0);
    vForm->addRow(QStringLiteral("质量 m"), massSpin_);

    auto* row = new QHBoxLayout();
    auto* b1 = new QPushButton(QStringLiteral("圆轨道"), vBox);
    auto* b2 = new QPushButton(QStringLiteral("静止"), vBox);
    auto* b3 = new QPushButton(QStringLiteral("径向内落"), vBox);
    auto* b4 = new QPushButton(QStringLiteral("切向 0.5c"), vBox);
    row->addWidget(b1);
    row->addWidget(b2);
    row->addWidget(b3);
    row->addWidget(b4);
    vForm->addRow(row);

    connect(b1, &QPushButton::clicked, this, &AddBodyDialog::applyCircular);
    connect(b2, &QPushButton::clicked, this, &AddBodyDialog::applyStatic);
    connect(b3, &QPushButton::clicked, this, &AddBodyDialog::applyRadialInfall);
    connect(b4, &QPushButton::clicked, this, &AddBodyDialog::applyTangential);

    auto* vHint = new QLabel(
        QStringLiteral("圆轨道按钮使用赤道顺行速度；倾斜轨道请自行微调。"
                       "要求 v_r²+v_θ²+v_φ² < 1。"),
        vBox);
    vHint->setWordWrap(true);
    vHint->setStyleSheet(QStringLiteral("color:#8a93a6;"));
    vForm->addRow(vHint);
    root->addWidget(vBox);
  } else {
    auto* dBox = new QGroupBox(QStringLiteral("初始方向"), this);
    auto* dForm = new QFormLayout(dBox);

    modeBox_ = new QComboBox(dBox);
    modeBox_->addItem(QStringLiteral("瞄准（按碰撞参数 b）"), 0);
    modeBox_->addItem(QStringLiteral("手动方向分量"), 1);
    dForm->addRow(QStringLiteral("方式"), modeBox_);

    aimWidget_ = new QWidget(dBox);
    auto* aimForm = new QFormLayout(aimWidget_);
    aimForm->setContentsMargins(0, 0, 0, 0);
    bSpin_ = makeSpin(aimWidget_, 0.0, 60.0, 0.5, 2, 5.0);
    bSpin_->setSuffix(QStringLiteral(" M"));
    aimForm->addRow(QStringLiteral("碰撞参数 b"), bSpin_);
    auto* aimBtn = new QPushButton(QStringLiteral("按 b 重新瞄准"), aimWidget_);
    aimForm->addRow(aimBtn);
    connect(aimBtn, &QPushButton::clicked, this, &AddBodyDialog::applyAim);
    dForm->addRow(aimWidget_);

    manualWidget_ = new QWidget(dBox);
    auto* manForm = new QFormLayout(manualWidget_);
    manForm->setContentsMargins(0, 0, 0, 0);
    nrSpin_ = makeSpin(manualWidget_, -1.0, 1.0, 0.05, 3, 1.0);
    nthSpin_ = makeSpin(manualWidget_, -1.0, 1.0, 0.05, 3, 0.0);
    nphSpin_ = makeSpin(manualWidget_, -1.0, 1.0, 0.05, 3, 0.0);
    manForm->addRow(QStringLiteral("n_r"), nrSpin_);
    manForm->addRow(QStringLiteral("n_θ"), nthSpin_);
    manForm->addRow(QStringLiteral("n_φ"), nphSpin_);
    dForm->addRow(manualWidget_);

    auto* dHint = new QLabel(
        QStringLiteral("“瞄准”会从当前位置指向距黑洞 b 的偏移点；b = 0 表示正对黑洞。"
                       "方向会在内部归一化。"),
        dBox);
    dHint->setWordWrap(true);
    dHint->setStyleSheet(QStringLiteral("color:#8a93a6;"));
    dForm->addRow(dHint);
    root->addWidget(dBox);

    connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AddBodyDialog::onModeChanged);
    onModeChanged(0);
  }

  // ---- preview ----
  preview_ = new QLabel(this);
  preview_->setWordWrap(true);
  preview_->setTextFormat(Qt::RichText);
  preview_->setMinimumHeight(86);
  preview_->setStyleSheet(
      QStringLiteral("background:#15171c;border:1px solid #2a2d34;padding:6px;"));
  root->addWidget(preview_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                       this);
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &AddBodyDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &AddBodyDialog::reject);

  previewTimer_.setSingleShot(true);
  previewTimer_.setInterval(220);
  connect(&previewTimer_, &QTimer::timeout, this, &AddBodyDialog::updatePreview);

  for (QDoubleSpinBox* s : {rSpin_, thetaSpin_, phiSpin_, vrSpin_, vthSpin_, vphSpin_,
                            bSpin_, nrSpin_, nthSpin_, nphSpin_, massSpin_}) {
    if (!s) continue;
    connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &AddBodyDialog::schedulePreview);
  }

  // Photons start aimed at the hole with the default impact parameter.
  if (photon) applyAim();
  updatePreview();
}

V3d AddBodyDialog::positionKs() const {
  const double th = thetaSpin_->value() * M_PI / 180.0;
  const double ph = phiSpin_->value() * M_PI / 180.0;
  return ksFromBL(rSpin_->value(), th, ph, ex_.a);
}

void AddBodyDialog::onModeChanged(int idx) {
  if (!modeBox_) return;
  const bool aim = (idx == 0);
  if (aimWidget_) aimWidget_->setVisible(aim);
  if (manualWidget_) manualWidget_->setVisible(!aim);
  if (aim) applyAim();
}

void AddBodyDialog::applyAim() {
  if (!bSpin_ || !nrSpin_) return;
  const V3d pos = positionKs();
  V3d zhat(0, 0, 1);
  V3d perp = pos.cross(zhat);
  if (perp.len() < 1e-6) perp = pos.cross(V3d(1, 0, 0));
  perp = perp.normalized();
  const V3d target = perp * bSpin_->value();
  V3d d = (target - pos).normalized();
  if (d.len() < 1e-6) d = V3d(-1, 0, 0);

  Tetrad<double> td;
  ksZamoTetrad(pos.x, pos.y, pos.z, ex_.a, ex_.M, td);
  double dw[3] = {d.x, d.y, d.z};
  double n[3] = {0, 0, 0};
  ksWorldToLocal(td, dw, n);
  const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (len > 1e-9) {
    n[0] /= len;
    n[1] /= len;
    n[2] /= len;
  }

  updating_ = true;
  nrSpin_->setValue(n[0]);
  nthSpin_->setValue(n[1]);
  nphSpin_->setValue(n[2]);
  updating_ = false;
  schedulePreview();
}

void AddBodyDialog::applyCircular() {
  if (!vphSpin_) return;
  updating_ = true;
  vrSpin_->setValue(0.0);
  vthSpin_->setValue(0.0);
  vphSpin_->setValue(circularSpeed(rSpin_->value(), ex_.a, ex_.M));
  updating_ = false;
  schedulePreview();
}

void AddBodyDialog::applyStatic() {
  if (!vphSpin_) return;
  updating_ = true;
  vrSpin_->setValue(0.0);
  vthSpin_->setValue(0.0);
  vphSpin_->setValue(0.0);
  updating_ = false;
  schedulePreview();
}

void AddBodyDialog::applyRadialInfall() {
  if (!vphSpin_) return;
  updating_ = true;
  vrSpin_->setValue(-0.30);
  vthSpin_->setValue(0.0);
  vphSpin_->setValue(0.0);
  updating_ = false;
  schedulePreview();
}

void AddBodyDialog::applyTangential() {
  if (!vphSpin_) return;
  updating_ = true;
  vrSpin_->setValue(0.0);
  vthSpin_->setValue(0.0);
  vphSpin_->setValue(0.5);
  updating_ = false;
  schedulePreview();
}

void AddBodyDialog::schedulePreview() {
  if (updating_) return;
  previewTimer_.start();
}

Body AddBodyDialog::buildBody() const {
  Body b;
  b.name = nameEdit_->text().trimmed();
  if (b.name.isEmpty())
    b.name = (kind_ == BodyKind::Photon) ? QStringLiteral("Ray") : QStringLiteral("Body");
  b.kind = kind_;
  b.pos = positionKs();

  if (kind_ == BodyKind::Photon) {
    V3d n(nrSpin_->value(), nthSpin_->value(), nphSpin_->value());
    if (n.len() < 1e-9) n = V3d(1, 0, 0);
    b.vel = n.normalized();
    b.radius = 0.10;
    b.color = QStringLiteral("#b0f0ff");
    b.mass = 0.0;
  } else {
    b.vel = V3d(vrSpin_->value(), vthSpin_->value(), vphSpin_->value());
    b.mass = massSpin_->value();
    b.radius = 0.30;
    b.color = QStringLiteral("#9be7ff");
  }
  return b;
}

void AddBodyDialog::updatePreview() {
  const Body b = buildBody();
  body_ = b;

  const double r = ksRadius(b.pos.x, b.pos.y, b.pos.z, ex_.a);
  const double rplus = ex_.horizon();
  QString html;

  const double v2 = b.vel.dot(b.vel);
  if (r <= rplus * 1.01) {
    html = QStringLiteral("<span style='color:#ff7b72'><b>位置在视界内</b>：r = %1 M，"
                          "r₊ = %2 M，无法放置。</span>")
               .arg(r, 0, 'f', 3)
               .arg(rplus, 0, 'f', 3);
    preview_->setText(html);
    return;
  }
  if (kind_ == BodyKind::Particle && v2 >= 0.999 * 0.999) {
    html = QStringLiteral("<span style='color:#ff7b72'><b>速度达到或超过光速</b>："
                          "|v| = %1 c，请减小速度分量。</span>")
               .arg(std::sqrt(v2), 0, 'f', 4);
    preview_->setText(html);
    return;
  }

  Experiment ex = ex_;
  ex.duration = std::min(ex_.duration, 400.0);
  IntegratorOptions opt;
  opt.maxSamples = 400;
  opt.rtol = 1e-8;
  const Trajectory tr = traceBody(ex, b, opt);

  QString fate;
  QString color;
  switch (tr.stop) {
    case StopCode::Captured:
      fate = QStringLiteral("落入视界");
      color = QStringLiteral("#ff9a6b");
      break;
    case StopCode::Escaped:
      fate = QStringLiteral("逃逸出计算区域");
      color = QStringLiteral("#8bd48b");
      break;
    case StopCode::Timeout:
      fate = QStringLiteral("在积分时长内保持束缚");
      color = QStringLiteral("#8fb8ff");
      break;
    case StopCode::Failed:
      fate = QStringLiteral("积分失败");
      color = QStringLiteral("#ff7b72");
      break;
    default:
      fate = QStringLiteral("未知");
      color = QStringLiteral("#cccccc");
      break;
  }

  double q0 = 0.0, e0 = 0.0, lz0 = 0.0, h20 = 0.0;
  if (!tr.samples.empty()) {
    e0 = tr.samples.front().E;
    lz0 = tr.samples.front().Lz;
    q0 = tr.samples.front().Qc;
    h20 = tr.samples.front().H2;
  }

  html = QStringLiteral(
             "<b>r = %1 M</b>（r₊ = %2 M）&nbsp;&nbsp;"
             "<span style='color:%3'><b>%4</b></span><br/>"
             "E = %5 &nbsp; L<sub>z</sub> = %6 &nbsp; Carter Q = %7<br/>"
             "2H = %8（%9）&nbsp;&nbsp;采样 %10 点")
             .arg(r, 0, 'f', 3)
             .arg(rplus, 0, 'f', 3)
             .arg(color)
             .arg(fate)
             .arg(e0, 0, 'g', 7)
             .arg(lz0, 0, 'g', 7)
             .arg(q0, 0, 'g', 7)
             .arg(h20, 0, 'g', 7)
             .arg(kind_ == BodyKind::Photon ? QStringLiteral("应为 0")
                                            : QStringLiteral("应为 −m²"))
             .arg(tr.samples.size());
  preview_->setText(html);
}

void AddBodyDialog::accept() {
  const Body b = buildBody();
  const double r = ksRadius(b.pos.x, b.pos.y, b.pos.z, ex_.a);
  if (r <= ex_.horizon() * 1.01) {
    QMessageBox::warning(this, windowTitle(),
                         QStringLiteral("位置太靠近视界（r = %1 M），请增大半径。")
                             .arg(r, 0, 'f', 3));
    return;
  }
  if (kind_ == BodyKind::Particle) {
    const double v2 = b.vel.dot(b.vel);
    if (v2 >= 0.999 * 0.999) {
      QMessageBox::warning(
          this, windowTitle(),
          QStringLiteral("局部速度 |v| = %1 c 必须小于光速。").arg(std::sqrt(v2), 0, 'f', 4));
      return;
    }
  }
  body_ = b;
  QDialog::accept();
}

}  // namespace bh

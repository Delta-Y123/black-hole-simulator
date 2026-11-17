#include "ui/researchpanel.h"

#include <QLabel>
#include <QPainter>
#include <QPolygonF>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace bh {

DriftPlot::DriftPlot(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(180);
}

void DriftPlot::setData(const std::vector<double>& lam, const std::vector<double>& e,
                        const std::vector<double>& lz, const std::vector<double>& q,
                        const std::vector<double>& h) {
  lam_ = lam;
  e_ = e;
  lz_ = lz;
  q_ = q;
  h_ = h;
  update();
}

void DriftPlot::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor(14, 16, 22));
  p.setPen(QColor(50, 58, 74));
  p.drawRect(rect().adjusted(0, 0, -1, -1));

  if (lam_.size() < 2) {
    p.setPen(QColor(110, 120, 140));
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("no data"));
    return;
  }

  auto rel = [](const std::vector<double>& v) {
    std::vector<double> o(v.size(), 0.0);
    const double v0 = v.front();
    const double scale = std::max(std::fabs(v0), 1e-12);
    for (size_t i = 0; i < v.size(); ++i) o[i] = (v[i] - v0) / scale;
    return o;
  };
  const auto re = rel(e_), rl = rel(lz_), rq = rel(q_), rh = rel(h_);

  double lo = 0.0, hi = 0.0;
  for (const auto* v : {&re, &rl, &rq, &rh})
    for (double x : *v) {
      lo = std::min(lo, x);
      hi = std::max(hi, x);
    }
  const double pad = std::max(1e-9, 0.08 * (hi - lo));
  lo -= pad;
  hi += pad;
  const double l0 = lam_.front(), l1 = lam_.back();
  const double span = std::max(1e-12, l1 - l0);

  auto px = [&](double l) {
    return 46.0 + (width() - 60.0) * (l - l0) / span;
  };
  auto py = [&](double v) {
    return height() - 24.0 - (height() - 46.0) * (v - lo) / (hi - lo);
  };

  p.setPen(QColor(45, 52, 66));
  p.drawLine(QPointF(px(l0), py(0)), QPointF(px(l1), py(0)));

  auto curve = [&](const std::vector<double>& v, const QColor& c) {
    p.setPen(QPen(c, 1.6));
    QPolygonF poly;
    const size_t stride = std::max<size_t>(1, v.size() / 900);
    for (size_t i = 0; i < v.size(); i += stride)
      poly << QPointF(px(lam_[i]), py(v[i]));
    p.drawPolyline(poly);
  };
  curve(re, QColor(255, 170, 80));
  curve(rl, QColor(120, 220, 140));
  curve(rq, QColor(120, 170, 255));
  curve(rh, QColor(220, 120, 200));

  p.setPen(QColor(150, 160, 180));
  p.drawText(QRect(6, 6, width() - 12, 16), Qt::AlignLeft,
             QStringLiteral("relative drift vs affine parameter"));
  p.drawText(QRect(6, 6, width() - 12, 16), Qt::AlignRight,
             QStringLiteral("E  Lz  Carter Q  H"));
}

ResearchPanel::ResearchPanel(QWidget* parent) : QWidget(parent) {
  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(6, 6, 6, 6);
  summary_ = new QLabel(QStringLiteral("no trajectory"), this);
  summary_->setWordWrap(true);
  summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  status_ = new QLabel(this);
  status_->setWordWrap(true);
  plot_ = new DriftPlot(this);
  lay->addWidget(summary_);
  lay->addWidget(plot_, 1);
  lay->addWidget(status_);
}

void ResearchPanel::setTrajectory(const Trajectory& tr, const QString& title) {
  if (tr.samples.size() < 2) {
    clear();
    return;
  }
  std::vector<double> lam, e, lz, q, h;
  lam.reserve(tr.samples.size());
  for (const auto& s : tr.samples) {
    lam.push_back(s.lam);
    e.push_back(s.E);
    lz.push_back(s.Lz);
    q.push_back(s.Qc);
    h.push_back(s.H2);
  }
  plot_->setData(lam, e, lz, q, h);

  const auto& a = tr.samples.front();
  const auto& b = tr.samples.back();
  auto rel = [](double x, double y) {
    return std::fabs(y - x) / std::max(1e-12, std::fabs(x));
  };
  summary_->setText(QStringLiteral(
                        "<b>%1</b><br/>"
                        "E = %2<br/>L<sub>z</sub> = %3<br/>"
                        "Carter Q = %4<br/>2H = %5 (expect %6)")
                        .arg(title)
                        .arg(a.E, 0, 'g', 8)
                        .arg(a.Lz, 0, 'g', 8)
                        .arg(a.Qc, 0, 'g', 8)
                        .arg(a.H2, 0, 'g', 8)
                        .arg(tr.photon ? 0.0 : -tr.m * tr.m, 0, 'g', 4));
  status_->setText(QStringLiteral("drift  E: %1   Lz: %2   Q: %3   |  %4 samples  |  %5")
                       .arg(rel(a.E, b.E), 0, 'e', 2)
                       .arg(rel(a.Lz, b.Lz), 0, 'e', 2)
                       .arg(rel(a.Qc, b.Qc), 0, 'e', 2)
                       .arg(tr.samples.size())
                       .arg(tr.message));
}

void ResearchPanel::clear() {
  summary_->setText(QStringLiteral("no trajectory"));
  status_->clear();
  plot_->setData({}, {}, {}, {}, {});
}

}  // namespace bh

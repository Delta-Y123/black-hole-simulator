#pragma once
#include <QLabel>
#include <QWidget>
#include <vector>

#include "core/sim.h"

namespace bh {

class DriftPlot : public QWidget {
  Q_OBJECT
 public:
  explicit DriftPlot(QWidget* parent = nullptr);
  void setData(const std::vector<double>& lam, const std::vector<double>& e,
               const std::vector<double>& lz, const std::vector<double>& q,
               const std::vector<double>& h);

 protected:
  void paintEvent(QPaintEvent*) override;

 private:
  std::vector<double> lam_, e_, lz_, q_, h_;
};

class ResearchPanel : public QWidget {
  Q_OBJECT
 public:
  explicit ResearchPanel(QWidget* parent = nullptr);
  void setTrajectory(const Trajectory& tr, const QString& title);
  void clear();

 private:
  DriftPlot* plot_ = nullptr;
  QLabel* summary_ = nullptr;
  QLabel* status_ = nullptr;
};

}  // namespace bh

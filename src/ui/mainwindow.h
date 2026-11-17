#pragma once
#include <QMainWindow>
#include <QTimer>
#include <vector>

#include "core/scene.h"
#include "core/sim.h"
#include "imaging/render.h"

class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QCheckBox;
class QSplitter;
class QTabWidget;

namespace bh {

class GLView;
class ImageView;
class ResearchPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private slots:
  void onPresetChanged(int idx);
  void onParamsChanged();
  void onRecompute();
  void onPlay();
  void onPause();
  void onStep();
  void onReset();
  void onTick();
  void onBodySelected(int row);
  void onAddParticle();
  void onAddPhoton();
  void onRemoveBody();
  void onSave();
  void onLoad();
  void onExportCsv();
  void onExportPng();
  void onCameraDragged(double dyaw, double dpitch);
  void onCameraMoved(double f, double r, double u);
  void onFovChanged(double factor);
  void scheduleRender();
  void doRender();

 private:
  void buildUi();
  void applyExperiment(const Experiment& ex);
  void recompute();
  void refreshBodyList();
  void refreshResearch();
  void updateCameraLabel();
  RenderParams makeRenderParams(int w, int h) const;

  Experiment ex_;
  std::vector<Trajectory> trajs_;

  GLView* gl_ = nullptr;
  ImageView* img_ = nullptr;
  ResearchPanel* research_ = nullptr;
  QSplitter* split_ = nullptr;
  QTabWidget* tabs_ = nullptr;
  QDockWidget* sceneDock_ = nullptr;
  QDockWidget* resDock_ = nullptr;

  QComboBox* presetBox_ = nullptr;
  QDoubleSpinBox* massSpin_ = nullptr;
  QDoubleSpinBox* spinSpin_ = nullptr;
  QListWidget* bodyList_ = nullptr;
  QCheckBox* diskCheck_ = nullptr;
  QCheckBox* gridCheck_ = nullptr;
  QCheckBox* trailCheck_ = nullptr;
  QLabel* infoLabel_ = nullptr;

  QPushButton* playBtn_ = nullptr;
  QPushButton* pauseBtn_ = nullptr;
  QPushButton* stepBtn_ = nullptr;
  QPushButton* resetBtn_ = nullptr;
  QSlider* speedSlider_ = nullptr;
  QLabel* timeLabel_ = nullptr;
  QComboBox* qualityBox_ = nullptr;

  QTimer playTimer_;
  QTimer renderTimer_;
  double lam_ = 0.0;
  double speed_ = 40.0;
  bool playing_ = false;
  int selected_ = 0;
  bool rendering_ = false;
  bool glReady_ = false;
};

}  // namespace bh

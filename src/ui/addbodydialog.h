#pragma once
#include <QDialog>
#include <QTimer>

#include "core/scene.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace bh {

// Dialog for placing a test body or a photon with explicit initial conditions.
//
// Position is entered in Boyer-Lindquist (r, theta, phi) and converted to the
// Kerr-Schild Cartesian coordinates used internally. Velocities are given in the
// local ZAMO frame (r_hat, theta_hat, phi_hat) in units of c, which is the frame
// the physics code actually uses for initial conditions.
class AddBodyDialog : public QDialog {
  Q_OBJECT
 public:
  AddBodyDialog(const Experiment& ex, BodyKind kind, QWidget* parent = nullptr);

  Body body() const { return body_; }

 private slots:
  void schedulePreview();
  void updatePreview();
  void applyCircular();
  void applyStatic();
  void applyRadialInfall();
  void applyTangential();
  void applyAim();
  void onModeChanged(int idx);
  void accept() override;

 private:
  Body buildBody() const;
  void syncFromPosition();
  V3d positionKs() const;

  Experiment ex_;
  BodyKind kind_;
  Body body_;

  QLineEdit* nameEdit_ = nullptr;
  QDoubleSpinBox* rSpin_ = nullptr;
  QDoubleSpinBox* thetaSpin_ = nullptr;
  QDoubleSpinBox* phiSpin_ = nullptr;

  QDoubleSpinBox* vrSpin_ = nullptr;
  QDoubleSpinBox* vthSpin_ = nullptr;
  QDoubleSpinBox* vphSpin_ = nullptr;
  QDoubleSpinBox* massSpin_ = nullptr;

  QComboBox* modeBox_ = nullptr;
  QDoubleSpinBox* bSpin_ = nullptr;
  QDoubleSpinBox* nrSpin_ = nullptr;
  QDoubleSpinBox* nthSpin_ = nullptr;
  QDoubleSpinBox* nphSpin_ = nullptr;

  QWidget* manualWidget_ = nullptr;
  QWidget* aimWidget_ = nullptr;
  QLabel* preview_ = nullptr;
  QTimer previewTimer_;
  bool updating_ = false;
};

}  // namespace bh

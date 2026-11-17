#pragma once
#include <QImage>
#include <QWidget>

namespace bh {

class ImageView : public QWidget {
  Q_OBJECT
 public:
  explicit ImageView(QWidget* parent = nullptr);

  void setImage(const QImage& img);
  void setCaption(const QString& s);
  void setBusy(bool b);

 signals:
  void cameraDragged(double dyaw, double dpitch);
  void cameraMoved(double forward, double right, double up);
  void fovChanged(double factor);
  void renderRequested();

 protected:
  void paintEvent(QPaintEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;

 private:
  QImage img_;
  QString caption_;
  QPoint last_;
  bool dragging_ = false;
  bool busy_ = false;
};

}  // namespace bh

#include "ui/imageview.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace bh {

ImageView::ImageView(QWidget* parent) : QWidget(parent) {
  setMinimumSize(320, 240);
  setFocusPolicy(Qt::StrongFocus);
  setAutoFillBackground(false);
}

void ImageView::setImage(const QImage& img) {
  img_ = img;
  update();
}

void ImageView::setCaption(const QString& s) {
  caption_ = s;
  update();
}

void ImageView::setBusy(bool b) {
  busy_ = b;
  update();
}

void ImageView::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor(8, 9, 12));

  if (!img_.isNull()) {
    QSize target = img_.size();
    target.scale(size(), Qt::KeepAspectRatio);
    const QRect r(QPoint((width() - target.width()) / 2,
                         (height() - target.height()) / 2),
                  target);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(r, img_);
  } else {
    p.setPen(QColor(120, 130, 150));
    p.drawText(rect(), Qt::AlignCenter,
               QStringLiteral("相机视图\n拖动旋转 · WASD 移动 · 滚轮缩放"));
  }

  p.setPen(QColor(150, 170, 200));
  p.drawText(QRect(8, 6, width() - 16, 20), Qt::AlignLeft, caption_);
  if (busy_) {
    p.setPen(QColor(255, 200, 120));
    p.drawText(QRect(8, 6, width() - 16, 20), Qt::AlignRight,
               QStringLiteral("rendering..."));
  }
  p.setPen(QColor(60, 70, 90));
  p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ImageView::mousePressEvent(QMouseEvent* e) {
  if (e->button() == Qt::LeftButton) {
    dragging_ = true;
    last_ = e->pos();
    setFocus();
  }
}

void ImageView::mouseMoveEvent(QMouseEvent* e) {
  if (!dragging_) return;
  const QPoint d = e->pos() - last_;
  last_ = e->pos();
  emit cameraDragged(-d.x() * 0.25, -d.y() * 0.25);
}

void ImageView::wheelEvent(QWheelEvent* e) {
  emit fovChanged(std::pow(1.0015, double(e->angleDelta().y())));
}

void ImageView::keyPressEvent(QKeyEvent* e) {
  const double step = 1.0;
  switch (e->key()) {
    case Qt::Key_W: emit cameraMoved(step, 0, 0); break;
    case Qt::Key_S: emit cameraMoved(-step, 0, 0); break;
    case Qt::Key_A: emit cameraMoved(0, -step, 0); break;
    case Qt::Key_D: emit cameraMoved(0, step, 0); break;
    case Qt::Key_Q: emit cameraMoved(0, 0, step); break;
    case Qt::Key_E: emit cameraMoved(0, 0, -step); break;
    default: QWidget::keyPressEvent(e); return;
  }
}

}  // namespace bh

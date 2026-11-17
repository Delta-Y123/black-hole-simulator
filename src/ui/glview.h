#pragma once
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <vector>

#include "core/scene.h"
#include "core/sim.h"

namespace bh {

class GLView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
  Q_OBJECT
 public:
  explicit GLView(QWidget* parent = nullptr);
  ~GLView() override;

  void setExperiment(const Experiment& ex);
  void setTrajectories(const std::vector<Trajectory>& trs);
  void setPlayback(double lam);
  void setSelected(int idx) { selected_ = idx; update(); }
  void setShowGrid(bool v) { showGrid_ = v; update(); }
  void setShowTrails(bool v) { showTrails_ = v; update(); }
  void resetCamera();

 signals:
  void viewChanged();

 protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;

 private:
  void buildStaticMeshes();
  void rebuildScene();
  void drawLines(const std::vector<QVector3D>& pts, const QVector3D& color,
                 float width, unsigned mode = 0x0003 /*GL_LINE_STRIP*/);

  struct Mesh {
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer ibo{ QOpenGLBuffer::IndexBuffer };
    int count = 0;
    bool ready = false;
  };

  Mesh sphere_;
  Mesh horizon_;
  Mesh ergo_;
  Mesh disk_;
  Mesh trails_;
  Mesh grid_;

  QOpenGLShaderProgram meshProg_;
  QOpenGLShaderProgram lineProg_;

  Experiment ex_;
  std::vector<Trajectory> trajs_;
  double playLam_ = 0.0;
  int selected_ = 0;
  bool showGrid_ = true;
  bool showTrails_ = true;

  float yaw_ = 35.0f, pitch_ = 22.0f, dist_ = 115.0f;
  QMatrix4x4 vp_;
  QPoint lastPos_;
  bool dragging_ = false;
};

}  // namespace bh


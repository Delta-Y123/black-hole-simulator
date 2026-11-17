#include "ui/glview.h"

#include <QFont>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QVector4D>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "core/metric.h"
#include "core/presets.h"

namespace bh {
namespace {

const char* kMeshVert = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vN;
out vec3 vW;
void main(){
  vec4 w = uModel * vec4(aPos, 1.0);
  vW = w.xyz;
  vN = mat3(uModel) * aNrm;
  gl_Position = uViewProj * w;
})";

const char* kMeshFrag = R"(#version 330 core
in vec3 vN;
in vec3 vW;
uniform vec3 uColor;
uniform float uAlpha;
uniform vec3 uEye;
out vec4 fragColor;
void main(){
  vec3 n = normalize(vN);
  vec3 v = normalize(uEye - vW);
  float d = max(dot(n, normalize(vec3(0.45,0.55,0.7))), 0.0);
  float rim = pow(1.0 - max(dot(n, v), 0.0), 2.2);
  vec3 c = uColor * (0.20 + 0.80 * d) + uColor * rim * 0.55;
  fragColor = vec4(c, uAlpha);
})";

const char* kLineVert = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uViewProj;
void main(){ gl_Position = uViewProj * vec4(aPos,1.0); })";

const char* kLineFrag = R"(#version 330 core
uniform vec3 uColor;
uniform float uAlpha;
out vec4 fragColor;
void main(){ fragColor = vec4(uColor, uAlpha); })";

void appendMesh(std::vector<float>& v, std::vector<unsigned>& idx, float r,
                float theta, float phi, const V3d& p, const V3d& n) {
  v.push_back((float)p.x);
  v.push_back((float)p.y);
  v.push_back((float)p.z);
  v.push_back((float)n.x);
  v.push_back((float)n.y);
  v.push_back((float)n.z);
  (void)r; (void)theta; (void)phi;
}

}  // namespace

GLView::GLView(QWidget* parent) : QOpenGLWidget(parent) {
  setMinimumSize(420, 320);
  setFocusPolicy(Qt::StrongFocus);
}

GLView::~GLView() {
  makeCurrent();
  for (Mesh* m : {&sphere_, &horizon_, &ergo_, &disk_, &trails_, &grid_}) {
    if (m->vbo.isCreated()) m->vbo.destroy();
    if (m->ibo.isCreated()) m->ibo.destroy();
    if (m->vao.isCreated()) m->vao.destroy();
  }
  doneCurrent();
}

void GLView::initializeGL() {
  initializeOpenGLFunctions();
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  meshProg_.addShaderFromSourceCode(QOpenGLShader::Vertex, kMeshVert);
  meshProg_.addShaderFromSourceCode(QOpenGLShader::Fragment, kMeshFrag);
  meshProg_.link();

  lineProg_.addShaderFromSourceCode(QOpenGLShader::Vertex, kLineVert);
  lineProg_.addShaderFromSourceCode(QOpenGLShader::Fragment, kLineFrag);
  lineProg_.link();

  buildStaticMeshes();
  rebuildScene();
}

void GLView::buildStaticMeshes() {
  // unit sphere
  const int NU = 48, NV = 32;
  std::vector<float> v;
  std::vector<unsigned> idx;
  for (int j = 0; j <= NV; ++j) {
    const double th = M_PI * j / NV;
    for (int i = 0; i <= NU; ++i) {
      const double ph = 2.0 * M_PI * i / NU;
      const V3d p(std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph),
                  std::cos(th));
      appendMesh(v, idx, 1.0f, 0, 0, p, p);
    }
  }
  for (int j = 0; j < NV; ++j)
    for (int i = 0; i < NU; ++i) {
      const unsigned a = j * (NU + 1) + i;
      const unsigned b = a + NU + 1;
      idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
    }
  sphere_.vao.create();
  sphere_.vao.bind();
  sphere_.vbo.create();
  sphere_.vbo.bind();
  sphere_.vbo.allocate(v.data(), int(v.size() * sizeof(float)));
  sphere_.ibo.create();
  sphere_.ibo.bind();
  sphere_.ibo.allocate(idx.data(), int(idx.size() * sizeof(unsigned)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void*)(3 * sizeof(float)));
  sphere_.count = int(idx.size());
  sphere_.ready = true;
  sphere_.vao.release();
}

void GLView::rebuildScene() {
  const double a = ex_.a, M = ex_.M;
  const double rp = ex_.horizon();

  auto surface = [&](Mesh& mesh, auto rOfTheta) {
    const int NU = 64, NV = 40;
    std::vector<float> v;
    std::vector<unsigned> idx;
    for (int j = 0; j <= NV; ++j) {
      const double th = M_PI * j / NV;
      const double r = rOfTheta(th);
      for (int i = 0; i <= NU; ++i) {
        const double ph = 2.0 * M_PI * i / NU;
        const V3d p = ksFromBL(r, th, ph, a);
        V3d n = V3d(p.x, p.y, p.z).normalized();
        if (n.len() < 1e-9) n = V3d(0, 0, 1);
        appendMesh(v, idx, 0, 0, 0, p, n);
      }
    }
    for (int j = 0; j < NV; ++j)
      for (int i = 0; i < NU; ++i) {
        const unsigned p0 = j * (NU + 1) + i;
        const unsigned p1 = p0 + NU + 1;
        idx.insert(idx.end(), {p0, p1, p0 + 1, p0 + 1, p1, p1 + 1});
      }
    if (!mesh.vao.isCreated()) mesh.vao.create();
    mesh.vao.bind();
    if (!mesh.vbo.isCreated()) mesh.vbo.create();
    mesh.vbo.bind();
    mesh.vbo.allocate(v.data(), int(v.size() * sizeof(float)));
    if (!mesh.ibo.isCreated()) mesh.ibo.create();
    mesh.ibo.bind();
    mesh.ibo.allocate(idx.data(), int(idx.size() * sizeof(unsigned)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    mesh.count = int(idx.size());
    mesh.ready = true;
    mesh.vao.release();
  };

  surface(horizon_, [&](double) { return rp; });
  surface(ergo_, [&](double th) {
    return M + std::sqrt(std::max(0.0, M * M - a * a * std::cos(th) * std::cos(th)));
  });

  // disk annulus at z = 0
  {
    std::vector<float> v;
    std::vector<unsigned> idx;
    const int NU = 96;
    const double r0 = ex_.disk.rIn, r1 = ex_.disk.rOut;
    for (int i = 0; i <= NU; ++i) {
      const double ph = 2.0 * M_PI * i / NU;
      for (int k = 0; k < 2; ++k) {
        const double rr = (k == 0) ? r0 : r1;
        const double x = std::sqrt(std::max(0.0, rr * rr + a * a)) * std::cos(ph);
        const double y = std::sqrt(std::max(0.0, rr * rr + a * a)) * std::sin(ph);
        v.insert(v.end(), {float(x), float(y), 0.0f, 0, 0, 1});
      }
    }
    for (int i = 0; i < NU; ++i) {
      const unsigned b = i * 2;
      idx.insert(idx.end(), {b, b + 1, b + 2, b + 2, b + 1, b + 3});
    }
    if (!disk_.vao.isCreated()) disk_.vao.create();
    disk_.vao.bind();
    if (!disk_.vbo.isCreated()) disk_.vbo.create();
    disk_.vbo.bind();
    disk_.vbo.allocate(v.data(), int(v.size() * sizeof(float)));
    if (!disk_.ibo.isCreated()) disk_.ibo.create();
    disk_.ibo.bind();
    disk_.ibo.allocate(idx.data(), int(idx.size() * sizeof(unsigned)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    disk_.count = int(idx.size());
    disk_.ready = true;
    disk_.vao.release();
  }
  update();
}

void GLView::setExperiment(const Experiment& ex) {
  ex_ = ex;
  if (isValid()) {
    makeCurrent();
    rebuildScene();
    doneCurrent();
  }
  update();
}

void GLView::setTrajectories(const std::vector<Trajectory>& trs) {
  trajs_ = trs;
  update();
}

void GLView::setPlayback(double lam) {
  playLam_ = lam;
  update();
}

void GLView::resetCamera() {
  yaw_ = 35.0f;
  pitch_ = 22.0f;
  dist_ = 115.0f;
  update();
}

void GLView::drawLines(const std::vector<QVector3D>& pts, const QVector3D& color,
                       float width, unsigned mode) {
  if (pts.size() < 2 || !lineProg_.isLinked()) return;
  lineProg_.bind();
  lineProg_.setUniformValue("uViewProj", vp_);
  lineProg_.setUniformValue("uColor", color);
  lineProg_.setUniformValue("uAlpha", 1.0f);
  QOpenGLVertexArrayObject vao;
  vao.create();
  vao.bind();
  QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
  vbo.create();
  vbo.bind();
  vbo.allocate(pts.data(), int(pts.size() * sizeof(QVector3D)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
  glLineWidth(width);
  glDrawArrays(mode, 0, int(pts.size()));
  vbo.destroy();
  vao.release();
  vao.destroy();
}

void GLView::paintGL() {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_TRUE);
  glClearColor(0.015f, 0.018f, 0.03f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const float dpr = float(devicePixelRatioF());
  const float aspect = float(width()) / float(std::max(1, height()));

  QMatrix4x4 proj;
  proj.perspective(45.0f, aspect, 0.05f, 5000.0f);

  const float yr = qDegreesToRadians(yaw_);
  const float pr = qDegreesToRadians(pitch_);
  const QVector3D eye(dist_ * std::cos(pr) * std::cos(yr),
                      dist_ * std::cos(pr) * std::sin(yr),
                      dist_ * std::sin(pr));
  QMatrix4x4 view;
  view.lookAt(eye, QVector3D(0, 0, 0), QVector3D(0, 0, 1));
  const QMatrix4x4 vp = proj * view;
  vp_ = vp;

  meshProg_.bind();
  meshProg_.setUniformValue("uViewProj", vp);
  meshProg_.setUniformValue("uEye", eye);

  auto draw = [&](Mesh& m, const QMatrix4x4& model, const QVector3D& c, float alpha) {
    if (!m.ready) return;
    meshProg_.setUniformValue("uModel", model);
    meshProg_.setUniformValue("uColor", c);
    meshProg_.setUniformValue("uAlpha", alpha);
    m.vao.bind();
    glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
    m.vao.release();
  };

  QMatrix4x4 I;
  draw(horizon_, I, QVector3D(0.02f, 0.02f, 0.03f), 1.0f);

  glDepthMask(GL_FALSE);
  if (ex_.a > 0.02) draw(ergo_, I, QVector3D(0.25f, 0.12f, 0.45f), 0.14f);
  if (ex_.disk.enabled) draw(disk_, I, QVector3D(0.85f, 0.42f, 0.16f), 0.20f);
  glDepthMask(GL_TRUE);

  // test bodies
  for (size_t i = 0; i < ex_.bodies.size(); ++i) {
    const Body& b = ex_.bodies[i];
    QVector3D pos(float(b.pos.x), float(b.pos.y), float(b.pos.z));
    const auto& tr = (i < trajs_.size()) ? trajs_[i] : Trajectory();
    if (!tr.samples.empty()) {
      double best = 0;
      for (const auto& s : tr.samples) {
        if (playLam_ > 0 && s.lam > playLam_) break;
        best = s.lam;
        pos = QVector3D(float(s.x.x), float(s.x.y), float(s.x.z));
      }
      (void)best;
    }
    QMatrix4x4 m;
    m.translate(pos);
    m.scale(float(std::max(0.05, b.radius)));
    QColor c(b.color);
    draw(sphere_, m, QVector3D(c.redF(), c.greenF(), c.blueF()), 1.0f);
  }

  // camera marker
  {
    QMatrix4x4 m;
    m.translate(float(ex_.camera.pos.x), float(ex_.camera.pos.y),
                float(ex_.camera.pos.z));
    m.scale(0.35f);
    draw(sphere_, m, QVector3D(0.5f, 0.8f, 1.0f), 1.0f);
  }

  // trails
  if (showTrails_) {
    for (size_t i = 0; i < trajs_.size() && i < ex_.bodies.size(); ++i) {
      std::vector<QVector3D> pts;
      const auto& tr = trajs_[i];
      pts.reserve(tr.samples.size());
      for (const auto& s : tr.samples) {
        if (playLam_ > 0 && s.lam > playLam_) break;
        pts.push_back(QVector3D(float(s.x.x), float(s.x.y), float(s.x.z)));
      }
      if (pts.size() > 1) {
        QColor c(ex_.bodies[i].color);
        drawLines(pts, QVector3D(c.redF(), c.greenF(), c.blueF()),
                  i == size_t(selected_) ? 2.2f : 1.3f, GL_LINE_STRIP);
      }
    }
  }

  // reference grid: equatorial circles every 10 M plus 30-degree spokes
  if (showGrid_) {
    const float R = 40.0f;
    const QVector3D gridColor(0.16f, 0.21f, 0.30f);
    for (int ring = 1; ring <= 4; ++ring) {
      const float rad = ring * 10.0f;
      std::vector<QVector3D> circle;
      circle.reserve(97);
      for (int k = 0; k <= 96; ++k) {
        const double t = 2.0 * M_PI * k / 96.0;
        circle.push_back(QVector3D(rad * std::cos(t), rad * std::sin(t), 0.0f));
      }
      drawLines(circle, gridColor, ring == 4 ? 1.4f : 1.0f, GL_LINE_STRIP);
    }
    for (int k = 0; k < 12; ++k) {
      const double t = 2.0 * M_PI * k / 12.0;
      drawLines({QVector3D(0, 0, 0),
                 QVector3D(R * std::cos(t), R * std::sin(t), 0.0f)},
                gridColor, 1.0f, GL_LINES);
    }

    // axes with arrow heads and 10 M tick marks
    struct Axis {
      QVector3D dir;
      QVector3D color;
      float len;
    };
    const Axis axes[3] = {
        {QVector3D(1, 0, 0), QVector3D(0.85f, 0.30f, 0.30f), 30.0f},
        {QVector3D(0, 1, 0), QVector3D(0.35f, 0.85f, 0.42f), 30.0f},
        {QVector3D(0, 0, 1), QVector3D(0.35f, 0.58f, 1.00f), 30.0f}};

    for (const Axis& ax : axes) {
      const QVector3D tip = ax.dir * ax.len;
      drawLines({QVector3D(0, 0, 0), tip}, ax.color, 2.0f, GL_LINES);

      // arrow head: two short segments in a plane containing the axis
      QVector3D side = QVector3D::crossProduct(ax.dir, QVector3D(0, 0, 1));
      if (side.length() < 1e-4f) side = QVector3D(1, 0, 0);
      side.normalize();
      const QVector3D back = tip - ax.dir * 2.2f;
      drawLines({tip, back + side * 0.9f}, ax.color, 2.0f, GL_LINES);
      drawLines({tip, back - side * 0.9f}, ax.color, 2.0f, GL_LINES);

      // tick marks at 10 and 20 M
      for (float tick : {10.0f, 20.0f}) {
        const QVector3D p = ax.dir * tick;
        drawLines({p - side * 0.45f, p + side * 0.45f}, ax.color * 0.85f, 1.4f,
                  GL_LINES);
      }
    }
  }

  (void)dpr;
}

void GLView::resizeGL(int w, int h) { (void)w; (void)h; }

void GLView::mousePressEvent(QMouseEvent* e) {
  if (e->button() == Qt::LeftButton) {
    dragging_ = true;
    lastPos_ = e->pos();
  }
}

void GLView::mouseMoveEvent(QMouseEvent* e) {
  if (!dragging_) return;
  const QPoint d = e->pos() - lastPos_;
  lastPos_ = e->pos();
  yaw_ += d.x() * 0.4f;
  pitch_ = std::clamp(pitch_ + d.y() * 0.3f, -85.0f, 85.0f);
  update();
  emit viewChanged();
}

void GLView::wheelEvent(QWheelEvent* e) {
  const float f = std::pow(1.0012f, float(e->angleDelta().y()));
  dist_ = std::clamp(dist_ / f, 3.0f, 800.0f);
  update();
  emit viewChanged();
}

}  // namespace bh




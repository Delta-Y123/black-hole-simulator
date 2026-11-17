#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>

#include "ui/mainwindow.h"

int main(int argc, char** argv) {
  QSurfaceFormat fmt;
  fmt.setVersion(4, 6);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  fmt.setSamples(4);
  fmt.setSwapInterval(1);
  QSurfaceFormat::setDefaultFormat(fmt);

  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("Black Hole Simulator"));
  app.setOrganizationName(QStringLiteral("BlackHoleSim"));
  app.setApplicationVersion(QStringLiteral("0.1.0"));

  bh::MainWindow w;
  w.show();

  const QStringList args = app.arguments();
  const int shotIdx = args.indexOf(QStringLiteral("--screenshot"));
  const QString shotPath =
      (shotIdx >= 0 && shotIdx + 1 < args.size()) ? args.at(shotIdx + 1) : QString();
  const int dlgIdx = args.indexOf(QStringLiteral("--dialog"));

  if (dlgIdx >= 0 && dlgIdx + 1 < args.size()) {
    // Self-test hook: open the add-body dialog, grab it, then exit.
    const QString kind = args.at(dlgIdx + 1);
    QTimer::singleShot(1000, &w, [&w, kind]() {
      QMetaObject::invokeMethod(
          &w, kind == QLatin1String("photon") ? "onAddPhoton" : "onAddParticle");
    });
    QTimer::singleShot(2600, &app, [shotPath]() {
      if (QWidget* modal = QApplication::activeModalWidget()) {
        if (!shotPath.isEmpty()) modal->grab().save(shotPath);
        modal->close();
      }
      QCoreApplication::quit();
    });
  } else if (!shotPath.isEmpty()) {
    QTimer::singleShot(3000, &app, [&w, shotPath]() {
      w.grab().save(shotPath);
      QCoreApplication::quit();
    });
  }

  return app.exec();
}

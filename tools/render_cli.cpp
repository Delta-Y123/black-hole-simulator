// Headless renderer and trajectory dumper.
//
//   bh_cli --preset shadow --width 640 --height 360 --out shadow.png
//   bh_cli --scene my.json --no-render --trace traj.json
//   bh_cli --scene my.json --out preview.png --trace traj.json
//   bh_cli --list-presets
//
// The trace JSON carries the Boyer-Lindquist state and covariant momentum at
// every sample, so an external tool (e.g. the research Agent) can evaluate its
// own candidate Q = K^{mu nu} p_mu p_nu along the same orbit.
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/presets.h"
#include "core/renderparams.h"
#include "core/sim.h"
#include "imaging/render.h"

namespace {

bh::Experiment loadScene(const QString& path, bool* ok) {
  *ok = false;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    std::fprintf(stderr, "cannot open scene: %s\n", path.toUtf8().constData());
    return {};
  }
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    std::fprintf(stderr, "scene is not a JSON object: %s\n",
                 path.toUtf8().constData());
    return {};
  }
  bool good = false;
  bh::Experiment e = bh::Experiment::fromJson(doc.object(), &good);
  if (!good)
    std::fprintf(stderr, "invalid scene (need M > 0 and a^2 < M^2)\n");
  *ok = good;
  return e;
}

QJsonObject sampleJson(const bh::Sample& s) {
  QJsonObject o;
  o["lam"] = s.lam;
  o["t"] = s.t;
  o["x"] = s.x.x;
  o["y"] = s.x.y;
  o["z"] = s.x.z;
  o["r"] = s.r;
  o["theta"] = s.theta;
  o["phi"] = s.phi;
  o["pt"] = s.p_t;
  o["pr"] = s.p_r;
  o["pth"] = s.p_th;
  o["pph"] = s.p_ph;
  o["kspx"] = s.ksp[0];
  o["kspy"] = s.ksp[1];
  o["kspz"] = s.ksp[2];
  o["E"] = s.E;
  o["Lz"] = s.Lz;
  o["Qcarter"] = s.Qc;
  o["H2"] = s.H2;
  return o;
}

bool writeTrace(const bh::Experiment& ex, const QString& path) {
  QJsonObject root;
  root["name"] = ex.name;
  root["M"] = ex.M;
  root["a"] = ex.a;
  root["duration"] = ex.duration;

  QJsonArray bodies;
  for (const bh::Body& b : ex.bodies) {
    const bh::Trajectory tr = bh::traceBody(ex, b);
    QJsonObject bo;
    bo["name"] = b.name;
    bo["kind"] = bh::kindToString(b.kind);
    bo["mass"] = tr.m;
    bo["photon"] = tr.photon;
    bo["stop"] = bh::stopText(tr.stop);
    bo["stopCode"] = int(tr.stop);
    bo["message"] = tr.message;
    bo["lastLam"] = tr.lastLam;
    QJsonArray samples;
    for (const bh::Sample& s : tr.samples) samples.append(sampleJson(s));
    bo["samples"] = samples;
    bodies.append(bo);
    std::printf("traced %s: %d samples, %s\n", b.name.toUtf8().constData(),
                int(tr.samples.size()), tr.message.toUtf8().constData());
  }
  root["bodies"] = bodies;

  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    std::fprintf(stderr, "cannot write trace: %s\n", path.toUtf8().constData());
    return false;
  }
  f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  std::printf("wrote %s\n", path.toUtf8().constData());
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);

  QString preset = QStringLiteral("shadow");
  QString scenePath, tracePath, out = QStringLiteral("render.png");
  int w = 640, h = 360, steps = 6000;
  double lam = 0.0;
  bool noRender = false, listPresets = false, haveOut = false;

  // Use Qt's wide-character argument list: raw argv on Windows is decoded with
  // the ANSI code page, which mangles non-ASCII scene/output paths.
  const QStringList args = QCoreApplication::arguments();
  for (int i = 1; i < args.size(); ++i) {
    const QString a = args.at(i);
    auto need = [&]() -> QString {
      if (i + 1 >= args.size()) {
        std::fprintf(stderr, "%s needs a value\n", a.toUtf8().constData());
        std::exit(2);
      }
      return args.at(++i);
    };
    if (a == "--preset") preset = need();
    else if (a == "--scene") scenePath = need();
    else if (a == "--trace") tracePath = need();
    else if (a == "--out") { out = need(); haveOut = true; }
    else if (a == "--width") w = need().toInt();
    else if (a == "--height") h = need().toInt();
    else if (a == "--steps") steps = need().toInt();
    else if (a == "--lam") lam = need().toDouble();
    else if (a == "--no-render") noRender = true;
    else if (a == "--list-presets") listPresets = true;
    else {
      std::fprintf(stderr, "unknown option: %s\n", a.toUtf8().constData());
      return 2;
    }
  }

  if (listPresets) {
    for (const bh::PresetInfo& p : bh::presetList())
      std::printf("%-16s %s\n", p.id.toUtf8().constData(),
                  p.title.toUtf8().constData());
    return 0;
  }

  bh::Experiment ex;
  if (!scenePath.isEmpty()) {
    bool ok = false;
    ex = loadScene(scenePath, &ok);
    if (!ok) return 1;
  } else {
    bool known = false;
    for (const bh::PresetInfo& p : bh::presetList())
      if (p.id == preset) known = true;
    if (!known) {
      std::fprintf(stderr, "unknown preset: %s (try --list-presets)\n",
                   preset.toUtf8().constData());
      return 2;
    }
    ex = bh::makePreset(preset);
  }

  int rc = 0;
  if (!tracePath.isEmpty() && !writeTrace(ex, tracePath)) rc = 1;

  if (!noRender && (haveOut || tracePath.isEmpty())) {
    char info[256] = {0};
    if (bh::renderDeviceInfo(info, sizeof(info)))
      std::printf("device: %s\n", info);
    else
      std::printf("device: %s\n", bh::renderLastError());

    bh::RenderParams p = bh::buildRenderParams(ex, w, h, lam, {});
    p.maxSteps = steps;

    std::vector<unsigned char> buf(size_t(w) * h * 4, 0);
    if (!bh::renderImage(p, buf.data())) {
      std::printf("render failed: %s\n", bh::renderLastError());
      return 1;
    }

    QImage img(buf.data(), w, h, w * 4, QImage::Format_RGBA8888);
    if (!img.save(out)) {
      std::printf("save failed: %s\n", out.toUtf8().constData());
      return 1;
    }
    std::printf("wrote %s (%dx%d, a=%.2f, disk=%d)\n",
                out.toUtf8().constData(), w, h, ex.a, ex.disk.enabled ? 1 : 0);
  }
  return rc;
}

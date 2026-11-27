#include "demo_source.h"

#include "app_state.h"

using backhaul::SlotKind;
using backhaul::SlotSituation;

namespace {

SlotSituation situation(SlotKind kind, bool video, bool audio, bool unstable) {
  SlotSituation s;
  s.kind = kind;
  s.assigned = s.connected = s.hasLastFrame = true;
  s.videoOn = video;
  s.audioOn = audio;
  s.unstable = unstable;
  return s;
}

}  // namespace

void loadDemo(AppState& app) {
  app.demo_ = true;
  app.meetingName_ = QStringLiteral("サンプル発表会");
  app.connected_ = true;
  app.elapsed_ = QStringLiteral("00:42:15");
  app.audioDevice_ = QStringLiteral("サンプル音声機器（ASIO）");
  app.audioOk_ = true;
  app.returnVideoOk_ = app.returnAudioOk_ = true;

  QVector<SlotRow> rows;
  auto add = [&rows](const QString& no, const QString& label, const QString& zoom, SlotSituation s, int w, int h,
                     int fps, double level, double gain, int delay, int hdmi, int ch) {
    SlotRow r;
    r.number = no;
    r.label = label;
    r.zoomName = zoom;
    r.situation = s;
    r.width = w;
    r.height = h;
    r.fps = fps;
    r.level = level;
    r.gainDb = gain;
    r.delayMs = delay;
    r.display = QStringLiteral("HDMI %1").arg(hdmi);
    r.channel = ch;
    rows.push_back(r);
  };
  add(QStringLiteral("1"), QStringLiteral("架空 一郎"), QStringLiteral("Kakuu Ichiro"),
      situation(SlotKind::Person, true, true, false), 640, 360, 30, 0.62, 0.0, 120, 2, 1);
  add(QStringLiteral("2"), QStringLiteral("見本 花子"), QStringLiteral("Mihon Hanako"),
      situation(SlotKind::Person, false, true, false), 0, 0, 0, 0.38, 2.0, 120, 3, 2);
  add(QStringLiteral("3"), QStringLiteral("仮野 次郎"), QStringLiteral("Karino Jiro"),
      situation(SlotKind::Person, true, false, false), 640, 360, 30, 0.0, 0.0, 120, 4, 3);
  add(QStringLiteral("4"), QStringLiteral("例田 三奈"), QStringLiteral("Reida Mina"),
      situation(SlotKind::Person, true, true, true), 320, 180, 12, 0.47, -1.5, 140, 5, 4);
  add(QStringLiteral("S"), QStringLiteral("スライド"), QStringLiteral("共有：架空 一郎"),
      situation(SlotKind::Share, true, true, false), 1920, 1200, 0, 0.12, 0.0, 0, 6, 5);
  rows.last().cropTo169 = true;
  app.slotList()->setRows(rows);

  QVector<ParticipantRow> people;
  auto person = [&people](const QString& name, int slot, const QString& role = QString(), bool self = false) {
    ParticipantRow p;
    p.name = name;
    p.slot = slot;
    p.role = role;
    p.isSelf = self;
    people.push_back(p);
  };
  person(QStringLiteral("架空 一郎"), 0);
  person(QStringLiteral("見本 花子"), 1);
  person(QStringLiteral("仮野 次郎"), 2);
  person(QStringLiteral("例田 三奈"), 3);
  person(QStringLiteral("試験 五郎"), -1);
  person(QStringLiteral("ホストPC"), -1, QStringLiteral("主催"));
  person(QStringLiteral("共同ホストPC"), -1, QStringLiteral("共同"));
  person(QStringLiteral("会場（このアプリ）"), -1, QStringLiteral("自分"), true);
  app.participants()->setRows(people, {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                                       QStringLiteral("4"), QStringLiteral("S")});
  app.notifyChanged();
}

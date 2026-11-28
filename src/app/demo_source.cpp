#include "demo_source.h"
#include "dictionary_translator.h"

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
  app.meetingName_ = T("サンプル発表会");
  app.connected_ = true;
  app.elapsed_ = QStringLiteral("00:42:15");
  app.audioDeviceName_ = T("サンプル音声機器");
  app.setSpareDeviceName(T("サンプル予備機器"));
  app.audioAsio_ = false;
  app.audioChannels_ = 8;
  app.audioSampleRate_ = 48000;
  app.audioOk_ = true;
  app.returnVideoOk_ = app.returnAudioOk_ = true;

  QVector<SlotRow> rows;
  auto add = [&rows](const QString& no, const QString& label, const QString& zoom, SlotSituation s, int w, int h,
                     int fps, double level, double gain, int delay, int ch) {
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
    r.channels = quint32{1} << (ch - 1);
    rows.push_back(r);
  };
  add(QStringLiteral("1"), T("架空 一郎"), QStringLiteral("Kakuu Ichiro"),
      situation(SlotKind::Person, true, true, false), 640, 360, 30, 0.62, 0.0, 120, 1);
  add(QStringLiteral("2"), T("見本 花子"), QStringLiteral("Mihon Hanako"),
      situation(SlotKind::Person, false, true, false), 0, 0, 0, 0.38, 2.0, 120, 2);
  add(QStringLiteral("3"), T("仮野 次郎"), QStringLiteral("Karino Jiro"),
      situation(SlotKind::Person, true, false, false), 640, 360, 30, 0.0, 0.0, 120, 3);
  add(QStringLiteral("4"), T("例田 三奈"), QStringLiteral("Reida Mina"),
      situation(SlotKind::Person, true, true, true), 320, 180, 12, 0.47, -1.5, 140, 4);
  add(QStringLiteral("S"), T("スライド"), T("共有：架空 一郎"),
      situation(SlotKind::Share, true, true, false), 1920, 1200, 0, 0.12, 0.0, 0, 5);
  rows.last().cropTo169 = true;
  app.slotList()->setRows(rows);

  // Made-up monitors, so the pictures do not depend on the PC they are taken on.
  QVector<AppState::ScreenInfo> screens;
  screens.push_back({QStringLiteral("demo-1"), T("サンプルモニターA"), QStringLiteral("1920×1080・60Hz")});
  for (int i = 2; i <= 6; ++i) {
    screens.push_back({QStringLiteral("demo-%1").arg(i), QStringLiteral("HDMI %1").arg(i), QStringLiteral("1920×1080・59.94Hz")});
  }
  screens.push_back({QStringLiteral("demo-7"), QStringLiteral("HDMI 7"), QStringLiteral("3840×2160・60Hz")});
  app.setScreens(screens, QStringLiteral("demo-1"));
  for (int i = 0; i < 5; ++i) app.setDisplayChoice(i + 1, i + 1);
  app.setGridChoice(1);

  QVector<ParticipantRow> people;
  auto person = [&people](const QString& name, int slot, const QString& role = QString(), bool self = false) {
    ParticipantRow p;
    p.name = name;
    p.slot = slot;
    p.role = role;
    p.isSelf = self;
    people.push_back(p);
  };
  person(T("架空 一郎"), 0);
  person(T("見本 花子"), 1);
  person(T("仮野 次郎"), 2);
  person(T("例田 三奈"), 3);
  person(T("試験 五郎"), -1);
  person(T("ホストPC"), -1, T("主催"));
  person(T("共同ホストPC"), -1, T("共同"));
  person(T("会場（このアプリ）"), -1, T("自分"), true);
  app.participants()->setRows(people, {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                                       QStringLiteral("4"), QStringLiteral("S")});
  app.notifyChanged();
}

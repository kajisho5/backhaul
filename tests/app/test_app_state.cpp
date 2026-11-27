#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "app_state.h"
#include "check.h"
#include "demo_source.h"
#include "settings_saver.h"

namespace {

QVariant slotData(AppState& app, int row, int role) { return app.slotList()->data(app.slotList()->index(row), role); }
QVariant personData(AppState& app, int row, int role) {
  return app.participants()->data(app.participants()->index(row), role);
}

}  // namespace

int main(int argc, char* argv[]) {
  QGuiApplication qtApp(argc, argv);

  {  // an empty app is a valid state: nothing connected, no rows, nothing crashes
    AppState app;
    CHECK(app.slotList()->rowCount() == 0);
    CHECK(app.participantCount() == 0);
    app.nudgeGain(0, 1);
    app.nudgeDelay(3, -1);
    app.setFreezeOnLoss(-1, true);
    app.assignToSelected(0);
    app.setSelected(5);
    CHECK(app.selected() == 0);
    CHECK(!app.sansFamily().isEmpty() && !app.monoFamily().isEmpty());
  }

  AppState app;
  loadDemo(app);
  CHECK(app.slotList()->rowCount() == 5);
  CHECK(app.participantCount() == 8);

  // The screen states come from the core decision logic.
  CHECK(slotData(app, 0, SlotModel::StateRole).toString() == "ok");
  CHECK(slotData(app, 1, SlotModel::StateRole).toString() == "camoff");
  CHECK(slotData(app, 1, SlotModel::VideoOutRole).toString() == "card");
  CHECK(slotData(app, 2, SlotModel::StateRole).toString() == "mute");
  CHECK(slotData(app, 2, SlotModel::LevelRole).toDouble() == 0.0);  // muted: meter must not move
  CHECK(slotData(app, 3, SlotModel::StateRole).toString() == "unstable");
  CHECK(slotData(app, 4, SlotModel::IsShareRole).toBool());
  CHECK(slotData(app, 4, SlotModel::DetailTextRole).toString() == QStringLiteral("16:9に切り取り"));

  // Tile placement: people fill the two left columns, the slide sits top right,
  // and the return tile goes right below it.
  CHECK(slotData(app, 0, SlotModel::GridRowRole).toInt() == 0 && slotData(app, 0, SlotModel::GridColumnRole).toInt() == 0);
  CHECK(slotData(app, 1, SlotModel::GridRowRole).toInt() == 0 && slotData(app, 1, SlotModel::GridColumnRole).toInt() == 1);
  CHECK(slotData(app, 2, SlotModel::GridRowRole).toInt() == 1 && slotData(app, 2, SlotModel::GridColumnRole).toInt() == 0);
  CHECK(slotData(app, 3, SlotModel::GridRowRole).toInt() == 1 && slotData(app, 3, SlotModel::GridColumnRole).toInt() == 1);
  CHECK(slotData(app, 4, SlotModel::GridRowRole).toInt() == 0 && slotData(app, 4, SlotModel::GridColumnRole).toInt() == 2);
  CHECK(app.returnRow() == 1);

  // Level: 0.5 dB steps, limited to -60..+12 dB.
  app.nudgeGain(0, 1);
  CHECK_NEAR(slotData(app, 0, SlotModel::GainRole).toDouble(), 0.5, 1e-9);
  app.nudgeGain(0, 1000);
  CHECK_NEAR(slotData(app, 0, SlotModel::GainRole).toDouble(), 12.0, 1e-9);
  app.nudgeGain(0, -1000);
  CHECK_NEAR(slotData(app, 0, SlotModel::GainRole).toDouble(), -60.0, 1e-9);

  // Delay: 10 ms steps, limited to 0..2000 ms.
  app.nudgeDelay(0, -1000);
  CHECK(slotData(app, 0, SlotModel::DelayRole).toInt() == 0);
  app.nudgeDelay(0, 3);
  CHECK(slotData(app, 0, SlotModel::DelayRole).toInt() == 30);
  app.nudgeDelay(0, 1000);
  CHECK(slotData(app, 0, SlotModel::DelayRole).toInt() == 2000);

  // Line-drop behaviour switch.
  app.setFreezeOnLoss(0, false);
  CHECK(!slotData(app, 0, SlotModel::FreezeOnLossRole).toBool());
  app.setFreezeOnLoss(0, true);
  CHECK(slotData(app, 0, SlotModel::FreezeOnLossRole).toBool());

  // Selection ignores rows that do not exist.
  app.setSelected(3);
  CHECK(app.selected() == 3);
  app.setSelected(99);
  CHECK(app.selected() == 3);
  app.setSelected(-1);
  CHECK(app.selected() == 3);

  // Assigning a waiting participant to the selected slot replaces the previous person.
  app.setSelected(0);
  CHECK(personData(app, 4, ParticipantModel::CanAssignRole).toBool());   // waiting
  CHECK(!personData(app, 5, ParticipantModel::CanAssignRole).toBool());  // host PC is never offered
  CHECK(!personData(app, 7, ParticipantModel::CanAssignRole).toBool());  // nor this app itself
  app.assignToSelected(4);
  CHECK(slotData(app, 0, SlotModel::ZoomNameRole).toString() == personData(app, 4, ParticipantModel::NameRole).toString());
  CHECK(personData(app, 4, ParticipantModel::TagRole).toString() == QStringLiteral("枠1"));
  CHECK(personData(app, 0, ParticipantModel::CanAssignRole).toBool());  // the previous person is free again
  // The newcomer shows a name card and is silent until picture and sound actually arrive.
  CHECK(slotData(app, 0, SlotModel::VideoOutRole).toString() == "card");
  CHECK(slotData(app, 0, SlotModel::LevelRole).toDouble() == 0.0);
  app.assignToSelected(99);  // bad row: ignored
  CHECK(personData(app, 4, ParticipantModel::TagRole).toString() == QStringLiteral("枠1"));

  {  // settings screen: routes, names, adding a slot
    AppState a;
    loadDemo(a);
    int edits = 0;
    QObject::connect(&a, &AppState::settingsEdited, [&edits] { ++edits; });

    CHECK(slotData(a, 0, SlotModel::ChannelTextRole).toString() == QStringLiteral("ch 1"));
    a.toggleRoute(0, 3);
    CHECK(slotData(a, 0, SlotModel::ChannelsRole).toInt() == 0b101);
    CHECK(slotData(a, 0, SlotModel::ChannelTextRole).toString() == QStringLiteral("ch 1・3"));
    a.toggleRoute(0, 1);
    a.toggleRoute(0, 3);
    CHECK(slotData(a, 0, SlotModel::ChannelTextRole).toString().isEmpty());
    CHECK(edits == 3);
    a.toggleRoute(0, 0);   // outside the device's channels: ignored
    a.toggleRoute(0, 9);
    a.toggleRoute(99, 1);
    CHECK(edits == 3);

    a.setLabel(1, QStringLiteral("見本 花子（司会）"));
    CHECK(slotData(a, 1, SlotModel::LabelRole).toString() == QStringLiteral("見本 花子（司会）"));
    a.setLabel(1, QString(100, QLatin1Char('x')));
    CHECK(slotData(a, 1, SlotModel::LabelRole).toString().size() == AppState::kMaxLabelLength);
    a.setZoomName(1, QStringLiteral("Mihon H."));
    CHECK(slotData(a, 1, SlotModel::ZoomNameRole).toString() == QStringLiteral("Mihon H."));
    const int before = edits;
    a.setZoomName(1, QStringLiteral("Mihon H."));  // no change, no save
    CHECK(edits == before);

    CHECK(a.cropSlides());
    a.setCropSlides(false);
    CHECK(!a.cropSlides());
    CHECK(slotData(a, 4, SlotModel::DetailTextRole).toString() == QStringLiteral("全体を表示"));

    // A new person goes after the last person; the slide and the people in it keep their place.
    a.addPersonSlot();
    CHECK(a.slotList()->rowCount() == 6);
    CHECK(slotData(a, 4, SlotModel::NumberRole).toString() == QStringLiteral("5"));
    CHECK(!slotData(a, 4, SlotModel::IsShareRole).toBool());
    CHECK(slotData(a, 4, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
    CHECK(slotData(a, 5, SlotModel::IsShareRole).toBool());
    CHECK(a.selected() == 4);
    CHECK(personData(a, 3, ParticipantModel::TagRole).toString() == QStringLiteral("枠4"));
    for (int i = 0; i < 40; ++i) a.addPersonSlot();
    CHECK(a.slotList()->rowCount() == AppState::kMaxPersonSlots + 1);
    CHECK(!a.canAddSlot());
  }

  {  // what is saved comes back the same
    AppState a;
    a.loadDefaults();
    CHECK(a.slotList()->rowCount() == 5);
    CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
    a.setLabel(0, QStringLiteral("架空 一郎"));
    a.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
    a.nudgeGain(0, 3);
    a.nudgeDelay(0, 12);
    a.setFreezeOnLoss(0, true);
    a.toggleRoute(0, 6);
    a.setAudioAsio(false);
    a.setCropSlides(false);
    a.addPersonSlot();

    AppState b;
    CHECK(b.applySettings(a.settingsJson()));
    CHECK(b.settingsJson() == a.settingsJson());
    CHECK(b.slotList()->rowCount() == 6);
    CHECK(slotData(b, 0, SlotModel::LabelRole).toString() == QStringLiteral("架空 一郎"));
    CHECK_NEAR(slotData(b, 0, SlotModel::GainRole).toDouble(), 1.5, 1e-9);
    CHECK(slotData(b, 0, SlotModel::DelayRole).toInt() == 120);
    CHECK(slotData(b, 0, SlotModel::FreezeOnLossRole).toBool());
    CHECK(slotData(b, 0, SlotModel::ChannelTextRole).toString() == QStringLiteral("ch 1・6"));
    CHECK(!b.audioAsio());
    CHECK(!b.cropSlides());
    // Nobody is in a meeting after loading.
    CHECK(slotData(b, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));

    // Anything that is not a settings object is refused and changes nothing.
    const QJsonObject good = a.settingsJson();
    auto refused = [&b, &good](QJsonObject changed) {
      const QJsonObject keep = b.settingsJson();
      const bool ok = b.applySettings(changed);
      return !ok && b.settingsJson() == keep && keep == good;
    };
    CHECK(refused(QJsonObject{}));
    QJsonObject o = good;
    o[QStringLiteral("format")] = 2;
    CHECK(refused(o));
    o = good;
    o[QStringLiteral("slots")] = QJsonArray{};
    CHECK(refused(o));
    o = good;
    o.remove(QStringLiteral("audio"));
    CHECK(refused(o));
    auto withSlotField = [&good](const QString& key, const QJsonValue& value) {
      QJsonObject changed = good;
      QJsonArray slotArray = changed[QStringLiteral("slots")].toArray();
      QJsonObject first = slotArray[0].toObject();
      first[key] = value;
      slotArray[0] = first;
      changed[QStringLiteral("slots")] = slotArray;
      return changed;
    };
    CHECK(refused(withSlotField(QStringLiteral("gainDb"), 99.0)));
    CHECK(refused(withSlotField(QStringLiteral("delayMs"), -5)));
    CHECK(refused(withSlotField(QStringLiteral("channels"), QJsonArray{0})));
    CHECK(refused(withSlotField(QStringLiteral("channels"), QJsonArray{33})));
    CHECK(refused(withSlotField(QStringLiteral("kind"), QStringLiteral("other"))));
    CHECK(refused(withSlotField(QStringLiteral("label"), 5)));
  }

  {  // the settings file
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("sub/settings.json"));
    {
      AppState a;
      SettingsSaver saver(a, path);
      CHECK(saver.load() == SettingsSaver::LoadResult::NoFile);
      CHECK(a.slotList()->rowCount() == 5);
      CHECK(saver.flush());                 // nothing changed: nothing written
      CHECK(!QFile::exists(path));
      a.setLabel(0, QStringLiteral("架空 一郎"));
      CHECK(saver.flush());
      CHECK(QFile::exists(path));
    }
    {
      AppState a;
      SettingsSaver saver(a, path);
      CHECK(saver.load() == SettingsSaver::LoadResult::Loaded);
      CHECK(slotData(a, 0, SlotModel::LabelRole).toString() == QStringLiteral("架空 一郎"));
    }
    {  // a damaged file is kept under another name, never overwritten
      QFile f(path);
      CHECK(f.open(QIODevice::WriteOnly));
      f.write("{ not json");
      f.close();
      AppState a;
      SettingsSaver saver(a, path);
      CHECK(saver.load() == SettingsSaver::LoadResult::Unreadable);
      CHECK(!saver.asidePath().isEmpty());
      CHECK(!QFile::exists(path));
      QFile aside(saver.asidePath());
      CHECK(aside.open(QIODevice::ReadOnly));
      CHECK(aside.readAll() == QByteArray("{ not json"));
      CHECK(a.slotList()->rowCount() == 5);
      a.setLabel(0, QStringLiteral("仮野 次郎"));
      CHECK(saver.flush());
      CHECK(QFile::exists(path));
    }
  }

  if (testing::failures() == 0) {
    std::printf("all app state tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}

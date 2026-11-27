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

  {  // video output: monitors and the slots they show
    AppState a;
    a.loadDefaults();
    int edits = 0;
    QObject::connect(&a, &AppState::settingsEdited, [&edits] { ++edits; });
    auto display = [&a](int row, int role) { return a.displays()->data(a.displays()->index(row), role); };

    a.setScreens({{QStringLiteral("op"), QStringLiteral("サンプルモニターA"), QStringLiteral("1920×1080・60Hz")},
                  {QStringLiteral("out-a"), QStringLiteral("HDMI 2"), QStringLiteral("1920×1080・59.94Hz")},
                  {QStringLiteral("out-b"), QStringLiteral("HDMI 3"), QStringLiteral("1920×1080・59.94Hz")}},
                 QStringLiteral("op"));
    CHECK(a.displays()->rowCount() == 3);
    CHECK(display(0, DisplayModel::IsOperatorRole).toBool());
    CHECK(display(1, DisplayModel::SlotRole).toInt() == -1);
    CHECK(a.outputChoices().size() == 6);  // black + five slots
    CHECK(edits == 0);                     // plugging monitors in is not an edit

    a.setDisplayChoice(1, 1);  // HDMI 2 shows slot 1
    a.setDisplayChoice(2, 5);  // HDMI 3 shows the slide
    CHECK(display(1, DisplayModel::SlotRole).toInt() == 0);
    CHECK(display(1, DisplayModel::SlotTextRole).toString() == QStringLiteral("枠1　演者1"));
    CHECK(display(2, DisplayModel::SlotTextRole).toString() == QStringLiteral("S　スライド"));
    CHECK(slotData(a, 0, SlotModel::DisplayRole).toString() == QStringLiteral("HDMI 2"));
    CHECK(slotData(a, 4, SlotModel::DisplayRole).toString() == QStringLiteral("HDMI 3"));
    CHECK(edits == 2);
    a.setDisplayChoice(0, 2);   // the operator's monitor is never an output
    a.setDisplayChoice(1, 99);
    a.setDisplayChoice(9, 1);
    a.setDisplayChoice(1, 1);   // unchanged
    CHECK(edits == 2);
    CHECK(display(0, DisplayModel::SlotRole).toInt() == -1);

    // The label on the monitor follows the slot's name.
    a.setLabel(0, QStringLiteral("架空 一郎"));
    CHECK(display(1, DisplayModel::SlotTextRole).toString() == QStringLiteral("枠1　架空 一郎"));

    // Adding a person moves the slide down one row; its monitor keeps showing the slide.
    a.addPersonSlot();
    CHECK(display(2, DisplayModel::SlotTextRole).toString() == QStringLiteral("S　スライド"));
    CHECK(slotData(a, 5, SlotModel::DisplayRole).toString() == QStringLiteral("HDMI 3"));

    CHECK(a.gridChoices().size() == 3);  // none + the two outputs
    CHECK(a.gridChoice() == 0);
    a.setGridChoice(2);
    CHECK(a.gridChoice() == 2);
    a.setGridChoice(7);
    CHECK(a.gridChoice() == 2);

    // An unplugged monitor is remembered and comes back with its slot.
    a.setScreens({{QStringLiteral("op"), QStringLiteral("サンプルモニターA"), QString()}}, QStringLiteral("op"));
    CHECK(slotData(a, 0, SlotModel::DisplayRole).toString().isEmpty());
    AppState b;
    CHECK(b.applySettings(a.settingsJson()));
    CHECK(b.settingsJson() == a.settingsJson());
    b.setScreens({{QStringLiteral("out-a"), QStringLiteral("HDMI 2"), QString()},
                  {QStringLiteral("out-b"), QStringLiteral("HDMI 3"), QString()}},
                 QString());
    CHECK(slotData(b, 0, SlotModel::DisplayRole).toString() == QStringLiteral("HDMI 2"));
    CHECK(b.gridChoice() == 2);
    b.setDisplayChoice(0, 0);  // back to black
    CHECK(slotData(b, 0, SlotModel::DisplayRole).toString().isEmpty());

    // Files from before this page have no monitor entries and are still read.
    QJsonObject old = a.settingsJson();
    old.remove(QStringLiteral("outputs"));
    old.remove(QStringLiteral("gridScreen"));
    AppState c;
    CHECK(c.applySettings(old));
    // A monitor pointing at a slot that does not exist is refused.
    QJsonObject bad = a.settingsJson();
    bad[QStringLiteral("outputs")] = QJsonArray{QJsonObject{{QStringLiteral("screen"), QStringLiteral("x")}, {QStringLiteral("slot"), 50}}};
    CHECK(!c.applySettings(bad));
    bad[QStringLiteral("outputs")] = QStringLiteral("nope");
    CHECK(!c.applySettings(bad));
  }


  {  // roles: what this PC is for
    AppState a;
    a.loadDefaults();
    CHECK(a.role().isEmpty());          // a fresh PC has to be told
    CHECK(!a.handlesAudio());           // and stays silent until then
    a.setRole(QStringLiteral("everything")); // unknown names change nothing
    CHECK(a.role().isEmpty());
    a.setRole(QStringLiteral("all"));
    CHECK(a.role() == QStringLiteral("all") && a.handlesAudio());
    CHECK(a.slotActive(0) && a.slotActive(3) && a.slotActive(4) && !a.slotActive(5) && !a.slotActive(-1));

    a.setRole(QStringLiteral("camera"));
    CHECK(!a.handlesAudio());
    CHECK(a.slotActive(0) && !a.slotActive(1) && !a.slotActive(4));
    a.setRole(QStringLiteral("twoshot"));
    CHECK(a.slotActive(0) && a.slotActive(1) && !a.slotActive(2) && !a.slotActive(4));

    // The slide PC uses the slide slot and nobody else.
    a.setRole(QStringLiteral("slide"));
    CHECK(a.shareRow() == 4 && a.roleRow() == 4);
    CHECK(a.slotActive(4) && !a.slotActive(0) && !a.slotActive(3));
    CHECK(!a.handlesAudio() && a.receivesVideo());
    a.setRole(QStringLiteral("twoshot"));
    CHECK(a.roleRow() == 0);

    // The role's one picture goes to one monitor; the operator's own monitor is never offered.
    a.setScreens({{QStringLiteral("op"), QStringLiteral("サンプルモニターA"), QString()},
                  {QStringLiteral("out-a"), QStringLiteral("HDMI 2"), QString()},
                  {QStringLiteral("out-b"), QStringLiteral("HDMI 3"), QString()}},
                 QStringLiteral("op"));
    CHECK(a.gridChoices().size() == 3 && a.roleOutputChoice() == 0);
    a.setRoleOutputChoice(2);
    CHECK(a.roleOutputChoice() == 2);
    CHECK(a.displays()->rows()[0].slot == -1 && a.displays()->rows()[1].slot == -1 && a.displays()->rows()[2].slot == 0);
    a.setRoleOutputChoice(7);  // not a monitor: ignored
    CHECK(a.roleOutputChoice() == 2);

    // Two-shot options and the role survive a restart.
    a.setTwoShotFill(false);
    a.setTwoShotSolo(true);
    a.swapTwoShot();
    CHECK(a.twoShotLeft() == 1 && a.twoShotRight() == 0);
    AppState b;
    CHECK(b.applySettings(a.settingsJson()));
    CHECK(b.role() == QStringLiteral("twoshot") && !b.twoShotFill() && b.twoShotSolo() && b.twoShotLeft() == 1);
    b.setScreens({{QStringLiteral("out-b"), QStringLiteral("HDMI 3"), QString()}}, QString());
    CHECK(b.roleOutputChoice() == 1);

    // The slide PC sends the slide slot to that same monitor.
    a.setRole(QStringLiteral("slide"));
    CHECK(a.displays()->rows()[2].slot == 4 && a.displays()->rows()[1].slot == -1);

    // The slot-by-monitor table of the do-everything role is untouched by all this.
    a.setRole(QStringLiteral("all"));
    CHECK(a.displays()->rows()[2].slot == -1);

    // PCs that show no pictures open no output window, even on monitors set up for "everything".
    a.setDisplayChoice(1, 1);
    CHECK(a.displays()->rows()[1].slot == 0);
    a.setRole(QStringLiteral("audio"));
    CHECK(a.displays()->rows()[1].slot == -1 && a.displays()->rows()[2].slot == -1);
    a.setRole(QStringLiteral("return"));
    CHECK(a.displays()->rows()[1].slot == -1 && a.displays()->rows()[2].slot == -1);
    a.setRole(QStringLiteral("all"));
    CHECK(a.displays()->rows()[1].slot == 0);  // the set-up is still there
    a.setDisplayChoice(1, 0);

    // The word on the slide output while nobody shares: kept, held to a sane length, may be empty.
    CHECK(a.standbyText() == QStringLiteral("SLIDE"));
    a.setStandbyText(QStringLiteral("SAMPLE SLIDE PC 1"));
    a.setStandbyText(QString(100, QLatin1Char('x')));
    CHECK(a.standbyText().size() == AppState::kMaxStandbyLength);
    a.setStandbyText(QString());
    {
      AppState kept;
      CHECK(kept.applySettings(a.settingsJson()));
      CHECK(kept.standbyText().isEmpty());
      a.setStandbyText(QStringLiteral("SAMPLE SLIDE PC 1"));
      CHECK(kept.applySettings(a.settingsJson()));
      CHECK(kept.standbyText() == QStringLiteral("SAMPLE SLIDE PC 1"));
      QJsonObject before = a.settingsJson();
      before.remove(QStringLiteral("standbyText"));
      CHECK(kept.applySettings(before));
      CHECK(kept.standbyText() == QStringLiteral("SLIDE"));  // files from before this existed
      before[QStringLiteral("standbyText")] = 7;
      CHECK(!kept.applySettings(before));
      kept.loadDefaults();
      CHECK(kept.standbyText() == QStringLiteral("SLIDE"));
    }

    // Files from before roles existed belong to PCs that did everything.
    QJsonObject old = a.settingsJson();
    old.remove(QStringLiteral("role"));
    old.remove(QStringLiteral("roleScreen"));
    old.remove(QStringLiteral("twoShot"));
    AppState c;
    CHECK(c.applySettings(old));
    CHECK(c.role() == QStringLiteral("all"));
    // Damaged role entries are refused as a whole.
    QJsonObject bad = a.settingsJson();
    bad[QStringLiteral("role")] = QStringLiteral("everything");
    CHECK(!c.applySettings(bad));
    bad = a.settingsJson();
    bad[QStringLiteral("twoShot")] = QStringLiteral("yes");
    CHECK(!c.applySettings(bad));
    // A two-shot needs two person slots.
    bad = a.settingsJson();
    bad[QStringLiteral("role")] = QStringLiteral("twoshot");
    QJsonArray one;
    one.push_back(bad.value(QStringLiteral("slots")).toArray().at(0));
    bad[QStringLiteral("slots")] = one;
    bad[QStringLiteral("outputs")] = QJsonArray{};
    CHECK(!c.applySettings(bad));
    bad[QStringLiteral("role")] = QStringLiteral("camera");
    CHECK(c.applySettings(bad));
    c.setRole(QStringLiteral("twoshot"));  // chosen by hand: the second slot is added
    CHECK(c.slotList()->rowCount() == 2 && c.slotActive(1));
    CHECK(c.shareRow() == -1);
    c.setRole(QStringLiteral("slide"));    // no slide slot in that file: one is added
    CHECK(c.slotList()->rowCount() == 3 && c.shareRow() == 2 && c.slotActive(2));
    CHECK(slotData(c, 2, SlotModel::IsShareRole).toBool());
  }

  {  // two-shot: which halves are drawn, and who is where
    AppState a;
    loadDemo(a);  // slot 1 has a picture, slot 2's camera is off (name card)
    a.setRole(QStringLiteral("twoshot"));
    CHECK(a.twoShotSplit() == 0);
    a.setTwoShotSolo(true);
    CHECK(a.twoShotSplit() == 1);   // the left person alone
    a.swapTwoShot();
    CHECK(a.twoShotSplit() == 2);   // the same person, now on the right
    a.swapTwoShot();

    // Pickers: entry 0 is nobody; this PC itself is not offered.
    const QStringList names = a.pickNames();
    CHECK(names.size() == a.participantCount());  // everyone but self, plus "nobody"
    const int left = a.twoShotLeftPick();
    const int right = a.twoShotRightPick();
    CHECK(left > 0 && right > 0 && left != right);
    // Picking the right-hand person for the left side moves them: nobody is in two slots.
    a.pickForSlot(0, right);
    CHECK(a.twoShotLeftPick() == right && a.twoShotRightPick() == 0);
    CHECK(slotData(a, 1, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
    CHECK(slotData(a, 1, SlotModel::ZoomNameRole).toString().isEmpty());
    a.pickForSlot(0, 0);  // nobody
    CHECK(a.twoShotLeftPick() == 0);
    CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
    a.pickForSlot(3, 1);  // a slot the role does not use: refused
    CHECK(slotData(a, 3, SlotModel::ZoomNameRole).toString() == QStringLiteral("Reida Mina"));
    a.pickForSlot(0, 99);
    CHECK(a.twoShotLeftPick() == 0);

    // A role that does not use a slot empties it but keeps its Zoom name for later.
    AppState b;
    loadDemo(b);
    b.setRole(QStringLiteral("camera"));
    CHECK(b.twoShotSplit() == 0);
    CHECK(slotData(b, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));
    CHECK(slotData(b, 1, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
    CHECK(slotData(b, 1, SlotModel::ZoomNameRole).toString() == QStringLiteral("Mihon Hanako"));
    CHECK(personData(b, 1, ParticipantModel::InSlotRole).toBool() == false);
    CHECK(personData(b, 0, ParticipantModel::InSlotRole).toBool());
  }

  if (testing::failures() == 0) {
    std::printf("all app state tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}

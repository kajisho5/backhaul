#include <QGuiApplication>

#include "app_state.h"
#include "check.h"
#include "demo_source.h"

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

  if (testing::failures() == 0) {
    std::printf("all app state tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}

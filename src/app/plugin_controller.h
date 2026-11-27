// Plug-ins on the sound paths: a short row per person (slot) on the way to the venue,
// and one row on the way from the venue into the meeting. Knows which plug-in sits
// where, keeps their settings in the settings file, and guards against a plug-in
// that brought the program down last time.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <memory>
#include <string>
#include <vector>

#include "core/effect_chain.h"
#include "plugins/plugin_host.h"

class AppState;
namespace backhaul {
class AudioMixer;
}

class PluginController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available CONSTANT)  // false: this build or system has no plug-ins
  Q_PROPERTY(QStringList moduleNames READ moduleNames NOTIFY modulesChanged)  // installed plug-in files
  // One list per slot row, and the one towards the meeting. Each entry:
  // { name, bypassed, loaded, hasEditor, note } - `note` says why an entry is not running.
  Q_PROPERTY(QVariantList slotRacks READ slotRacks NOTIFY changed)
  Q_PROPERTY(QVariantList micRack READ micRack NOTIFY changed)
  Q_PROPERTY(QVariantList slotDelaysMs READ slotDelaysMs NOTIFY changed)  // added delay per slot row
  Q_PROPERTY(double micDelayMs READ micDelayMs NOTIFY changed)
  Q_PROPERTY(int maxPerRack READ maxPerRack CONSTANT)
  Q_PROPERTY(QString problem READ problem NOTIFY changed)
  // A file with several effects: their names, while the operator has to pick one.
  Q_PROPERTY(QStringList choices READ choices NOTIFY changed)
  Q_PROPERTY(int choiceTarget READ choiceTarget NOTIFY changed)

 public:
  static constexpr int kMic = -1;  // the `target` of the row towards the meeting; others are slot rows

  // `host` may be null. `guardFolder` is where the notes for the crash guard are kept
  // (empty: no guard, as in tests that do not need it).
  // `mixer` holds the rows of the slots, `micChain` is the row towards the meeting; both
  // belong to the sound side and must outlive this object.
  PluginController(AppState& app, backhaul::AudioMixer* mixer, backhaul::EffectChain* micChain,
                   std::unique_ptr<backhaul::PluginHost> host, QString guardFolder = QString(), QObject* parent = nullptr);
  ~PluginController() override;

  bool available() const { return host_ != nullptr; }
  QStringList moduleNames() const;
  QVariantList slotRacks() const;
  QVariantList micRack() const;
  QVariantList slotDelaysMs() const;
  double micDelayMs() const;
  int maxPerRack() const { return backhaul::EffectChain::kMaxEffects; }
  QString problem() const { return problem_; }
  QStringList choices() const;
  int choiceTarget() const { return choiceTarget_; }

  Q_INVOKABLE void refreshModules();
  // Adds the plug-in file with this index in moduleNames to a row. A file with several
  // effects fills `choices` instead; choose() then adds the picked one.
  Q_INVOKABLE void add(int target, int moduleIndex);
  Q_INVOKABLE void choose(int choiceIndex);
  Q_INVOKABLE void cancelChoice();
  Q_INVOKABLE void remove(int target, int index);
  Q_INVOKABLE void setBypassed(int target, int index, bool bypassed);
  Q_INVOKABLE void openEditor(int target, int index);
  // Tries again to load an entry that is not running (blocked by the guard, or its file was missing).
  Q_INVOKABLE void retry(int target, int index);

  // Builds the rows from what the settings hold (at start-up and when a project is opened).
  // Everything that was loaded before is let go first.
  void restore();
  // Puts the plug-ins' current settings into the application's settings (before saving).
  void capture();
  // Regular work, also callable from a test: plug-in housekeeping and noticing edits.
  void tick();

 signals:
  void changed();
  void modulesChanged();

 private:
  struct Entry {
    std::string path, classId;
    QString name;
    bool bypassed = false;
    bool blocked = false;  // the guard took it out; only retry() loads it again
    QString note;
    QByteArray saved;      // the plug-in's settings as last captured or as read from the file
    std::shared_ptr<backhaul::PluginInstance> instance;
  };
  using Rack = std::vector<Entry>;

  Rack* rack(int target);
  const Rack* rack(int target) const;
  backhaul::EffectChain* chain(int target);
  int channelsOf(int target) const { return target == kMic ? 2 : 1; }
  bool load(int target, Entry& entry);
  void addEntry(int target, const std::string& path, const backhaul::PluginClass& which);
  void rebuild(int target);
  void store();  // rows -> application settings (without asking the plug-ins again)
  QVariantList describe(const Rack& rack) const;
  double delayMs(int target) const;
  // Crash guard.
  void noteLoading(const std::string& path);
  void clearLoading();
  void noteRunning();
  QString guardFile(const char* name) const;

  AppState& app_;
  backhaul::AudioMixer* mixer_;
  backhaul::EffectChain* micChain_;
  std::unique_ptr<backhaul::PluginHost> host_;
  QString guardFolder_;
  std::vector<std::string> modules_;
  std::vector<Rack> slotRacks_;
  Rack micRack_;
  QString problem_;
  std::string choicePath_;
  std::vector<backhaul::PluginClass> choiceClasses_;
  int choiceTarget_ = -2;
  QTimer timer_;
  int quietTicks_ = -1;  // counts up after an edit; settings are captured once it is quiet
  bool running_ = false;
};

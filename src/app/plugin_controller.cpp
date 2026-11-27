#include "plugin_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <algorithm>

#include "app_state.h"
#include "core/audio_mixer.h"

namespace {

constexpr int kTickMs = 33;
constexpr int kQuietTicks = 30;  // about a second without edits before settings are captured
constexpr int kMaxStateBytes = 16 * 1024 * 1024;

QByteArray toBytes(const std::vector<std::uint8_t>& data) {
  return QByteArray(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
}

std::vector<std::uint8_t> fromBytes(const QByteArray& data) {
  const auto* begin = reinterpret_cast<const std::uint8_t*>(data.constData());
  return std::vector<std::uint8_t>(begin, begin + data.size());
}

}  // namespace

PluginController::PluginController(AppState& app, backhaul::AudioMixer* mixer, std::unique_ptr<backhaul::PluginHost> host,
                                   QString guardFolder, QObject* parent)
    : QObject(parent), app_(app), mixer_(mixer), host_(std::move(host)), guardFolder_(std::move(guardFolder)) {
  slotRacks_.resize(static_cast<std::size_t>(app_.slotList()->rows().size()));
  // A person slot added in the settings pushes the slide slots (and their rows here) down.
  connect(app_.slotList(), &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int first, int last) {
    const std::size_t at = std::min(static_cast<std::size_t>(first), slotRacks_.size());
    slotRacks_.insert(slotRacks_.begin() + static_cast<std::ptrdiff_t>(at), static_cast<std::size_t>(last - first + 1), Rack{});
    for (int row = first; row < static_cast<int>(slotRacks_.size()); ++row) rebuild(row);
    store();
    emit changed();
  });
  if (app_.demo_) {  // made-up rows for the demo picture
    Entry eq;
    eq.name = QStringLiteral("Sample EQ");
    Entry comp;
    comp.name = QStringLiteral("Sample Comp");
    comp.bypassed = true;
    if (!slotRacks_.empty()) slotRacks_[0] = {eq, comp};
    micRack_ = {eq};
  }
  refreshModules();
  timer_.setInterval(kTickMs);
  connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
  if (host_ != nullptr) timer_.start();
}

PluginController::~PluginController() {
  // The sound threads must let go of every plug-in before it is closed.
  micChain_.set({});
  for (int row = 0; row < static_cast<int>(slotRacks_.size()); ++row) {
    if (backhaul::EffectChain* c = chain(row)) c->set({});
  }
  micRack_.clear();
  slotRacks_.clear();
  if (running_) QFile::remove(guardFile("plugins-running"));  // a clean end
}

// ---- lists for the screens ----

QStringList PluginController::moduleNames() const {
  QStringList names;
  for (const std::string& path : modules_) names.push_back(QFileInfo(QString::fromStdString(path)).completeBaseName());
  return names;
}

void PluginController::refreshModules() {
  modules_ = host_ != nullptr ? host_->modules() : std::vector<std::string>{};
  emit modulesChanged();
}

QVariantList PluginController::describe(const Rack& rack) const {
  QVariantList list;
  for (const Entry& e : rack) {
    QVariantMap m;
    m[QStringLiteral("name")] = e.name;
    m[QStringLiteral("bypassed")] = e.bypassed;
    m[QStringLiteral("loaded")] = e.instance != nullptr || app_.demo_;
    m[QStringLiteral("hasEditor")] = e.instance != nullptr && e.instance->hasEditor();
    m[QStringLiteral("note")] = e.note;
    list.push_back(m);
  }
  return list;
}

QVariantList PluginController::slotRacks() const {
  QVariantList all;
  for (const Rack& rack : slotRacks_) all.push_back(QVariant(describe(rack)));
  return all;
}

QVariantList PluginController::micRack() const { return describe(micRack_); }

double PluginController::delayMs(int target) const {
  const Rack* r = rack(target);
  if (r == nullptr) return 0.0;
  int samples = 0;
  for (const Entry& e : *r) {
    if (e.instance != nullptr && !e.bypassed) samples += e.instance->latencySamples();
  }
  return samples * 1000.0 / backhaul::AudioMixer::kRate;
}

QVariantList PluginController::slotDelaysMs() const {
  QVariantList all;
  for (int row = 0; row < static_cast<int>(slotRacks_.size()); ++row) all.push_back(delayMs(row));
  return all;
}

double PluginController::micDelayMs() const { return delayMs(kMic); }

QStringList PluginController::choices() const {
  QStringList names;
  for (const backhaul::PluginClass& c : choiceClasses_) names.push_back(QString::fromStdString(c.name));
  return names;
}

// ---- rows ----

PluginController::Rack* PluginController::rack(int target) {
  if (target == kMic) return &micRack_;
  if (target < 0 || target >= static_cast<int>(slotRacks_.size())) return nullptr;
  return &slotRacks_[static_cast<std::size_t>(target)];
}

const PluginController::Rack* PluginController::rack(int target) const {
  return const_cast<PluginController*>(this)->rack(target);
}

backhaul::EffectChain* PluginController::chain(int target) {
  if (target == kMic) return &micChain_;
  return mixer_ != nullptr ? mixer_->effects(target) : nullptr;
}

// Hands the loaded plug-ins of a row to the sound thread, in order.
void PluginController::rebuild(int target) {
  backhaul::EffectChain* c = chain(target);
  const Rack* r = rack(target);
  if (c == nullptr || r == nullptr) return;
  std::vector<std::shared_ptr<backhaul::AudioEffect>> effects;
  std::vector<bool> bypass;
  for (const Entry& e : *r) {
    if (e.instance == nullptr) continue;
    effects.push_back(e.instance);
    bypass.push_back(e.bypassed);
  }
  // Bypass first for everything that stays in place, so nothing un-bypassed slips through.
  for (int i = 0; i < backhaul::EffectChain::kMaxEffects; ++i) {
    c->setBypassed(i, i < static_cast<int>(bypass.size()) ? bypass[static_cast<std::size_t>(i)] : false);
  }
  c->set(std::move(effects));
}

bool PluginController::load(int target, Entry& entry) {
  if (host_ == nullptr) {
    entry.note = QStringLiteral("このプログラムではプラグインを使えません");
    return false;
  }
  if (!QFileInfo::exists(QString::fromStdString(entry.path))) {
    entry.note = QStringLiteral("このPCに見つかりません");
    return false;
  }
  std::string error;
  noteLoading(entry.path);
  std::shared_ptr<backhaul::PluginInstance> instance = host_->create(entry.path, entry.classId, channelsOf(target), &error);
  if (instance != nullptr && !entry.saved.isEmpty() && !instance->setState(fromBytes(entry.saved))) {
    // It runs, with its own defaults. Say so instead of pretending the old settings are back.
    entry.note = QStringLiteral("保存してあった設定を戻せませんでした（初期状態で動いています）");
  } else {
    entry.note.clear();
  }
  clearLoading();
  if (instance == nullptr) {
    entry.note = error == "the plug-in only works in mono" ? QStringLiteral("モノラル専用のため、Zoomへ送る側では使えません")
                                                           : QStringLiteral("読み込めませんでした：%1").arg(QString::fromStdString(error));
    return false;
  }
  entry.instance = std::move(instance);
  entry.blocked = false;
  noteRunning();
  return true;
}

void PluginController::add(int target, int moduleIndex) {
  Rack* r = rack(target);
  if (host_ == nullptr || r == nullptr || moduleIndex < 0 || moduleIndex >= static_cast<int>(modules_.size())) return;
  if (static_cast<int>(r->size()) >= maxPerRack()) return;
  const std::string path = modules_[static_cast<std::size_t>(moduleIndex)];
  std::string error;
  noteLoading(path);
  const std::vector<backhaul::PluginClass> classes = host_->classes(path, &error);
  clearLoading();
  cancelChoice();
  if (classes.empty()) {
    problem_ = QStringLiteral("「%1」を読み込めませんでした：%2")
                   .arg(QFileInfo(QString::fromStdString(path)).completeBaseName(), QString::fromStdString(error));
    emit changed();
    return;
  }
  if (classes.size() == 1) {
    addEntry(target, path, classes.front());
    return;
  }
  choicePath_ = path;
  choiceClasses_ = classes;
  choiceTarget_ = target;
  problem_.clear();
  emit changed();
}

void PluginController::choose(int choiceIndex) {
  if (choiceIndex < 0 || choiceIndex >= static_cast<int>(choiceClasses_.size())) return;
  const std::string path = choicePath_;
  const backhaul::PluginClass which = choiceClasses_[static_cast<std::size_t>(choiceIndex)];
  const int target = choiceTarget_;
  choiceClasses_.clear();
  choiceTarget_ = -2;
  addEntry(target, path, which);
}

void PluginController::cancelChoice() {
  if (choiceClasses_.empty()) return;
  choiceClasses_.clear();
  choicePath_.clear();
  choiceTarget_ = -2;
  emit changed();
}

void PluginController::addEntry(int target, const std::string& path, const backhaul::PluginClass& which) {
  Rack* r = rack(target);
  if (r == nullptr || static_cast<int>(r->size()) >= maxPerRack()) return;
  Entry entry;
  entry.path = path;
  entry.classId = which.id;
  entry.name = QString::fromStdString(which.name);
  if (!load(target, entry)) {
    problem_ = QStringLiteral("「%1」：%2").arg(entry.name, entry.note);  // not kept in the row
    emit changed();
    return;
  }
  problem_.clear();
  r->push_back(std::move(entry));
  rebuild(target);
  store();
  emit changed();
}

void PluginController::remove(int target, int index) {
  Rack* r = rack(target);
  if (r == nullptr || index < 0 || index >= static_cast<int>(r->size())) return;
  Entry gone = std::move((*r)[static_cast<std::size_t>(index)]);
  r->erase(r->begin() + index);
  rebuild(target);  // the sound thread lets go of it here, before it is closed below
  if (gone.instance != nullptr) gone.instance->closeEditor();
  gone.instance.reset();
  store();
  emit changed();
}

void PluginController::setBypassed(int target, int index, bool bypassed) {
  Rack* r = rack(target);
  if (r == nullptr || index < 0 || index >= static_cast<int>(r->size())) return;
  Entry& entry = (*r)[static_cast<std::size_t>(index)];
  if (entry.bypassed == bypassed) return;
  entry.bypassed = bypassed;
  rebuild(target);
  store();
  emit changed();
}

void PluginController::openEditor(int target, int index) {
  Rack* r = rack(target);
  if (r == nullptr || index < 0 || index >= static_cast<int>(r->size())) return;
  Entry& entry = (*r)[static_cast<std::size_t>(index)];
  if (entry.instance == nullptr) return;
  const QString where = target == kMic ? QStringLiteral("マイク") : QStringLiteral("枠%1").arg(target + 1);
  if (!entry.instance->openEditor(QStringLiteral("%1 — %2").arg(entry.name, where).toStdString())) {
    problem_ = QStringLiteral("「%1」の画面を開けませんでした").arg(entry.name);
    emit changed();
  }
}

void PluginController::retry(int target, int index) {
  Rack* r = rack(target);
  if (r == nullptr || index < 0 || index >= static_cast<int>(r->size())) return;
  Entry& entry = (*r)[static_cast<std::size_t>(index)];
  if (entry.instance != nullptr) return;
  entry.blocked = false;
  store();  // if it brings the program down again, the guard finds it by its path
  load(target, entry);
  rebuild(target);
  store();
  emit changed();
}

// ---- settings ----

void PluginController::store() {
  if (app_.demo_) return;
  auto rowToJson = [](const Rack& rack) {
    QJsonArray list;
    for (const Entry& e : rack) {
      QJsonObject o;
      o[QStringLiteral("path")] = QString::fromStdString(e.path);
      o[QStringLiteral("class")] = QString::fromStdString(e.classId);
      o[QStringLiteral("name")] = e.name;
      o[QStringLiteral("bypassed")] = e.bypassed;
      if (e.blocked) o[QStringLiteral("blocked")] = true;
      if (!e.saved.isEmpty()) o[QStringLiteral("state")] = QString::fromLatin1(e.saved.toBase64());
      list.push_back(o);
    }
    return list;
  };
  bool any = !micRack_.empty();
  QJsonArray slotRows;
  for (const Rack& rack : slotRacks_) {
    any = any || !rack.empty();
    slotRows.push_back(rowToJson(rack));
  }
  QJsonObject root;
  if (any) {
    root[QStringLiteral("mic")] = rowToJson(micRack_);
    root[QStringLiteral("slots")] = slotRows;
  }
  app_.setPluginSettings(root);
}

void PluginController::capture() {
  auto take = [](Rack& rack) {
    for (Entry& e : rack) {
      if (e.instance == nullptr) continue;  // keep what was read from the file
      const std::vector<std::uint8_t> now = e.instance->state();
      if (!now.empty()) e.saved = toBytes(now);
    }
  };
  take(micRack_);
  for (Rack& rack : slotRacks_) take(rack);
  store();
}

void PluginController::restore() {
  // Let go of everything first.
  micChain_.set({});
  for (int row = 0; row < static_cast<int>(slotRacks_.size()); ++row) {
    if (backhaul::EffectChain* c = chain(row)) c->set({});
  }
  micRack_.clear();
  slotRacks_.assign(static_cast<std::size_t>(app_.slotList()->rows().size()), Rack{});
  cancelChoice();
  problem_.clear();
  if (app_.demo_) return;

  // What the guard knows about last time.
  QString crashedPath;
  QFile loading(guardFile("plugins-loading"));
  if (!guardFolder_.isEmpty() && loading.open(QIODevice::ReadOnly)) {
    crashedPath = QString::fromUtf8(loading.readAll()).trimmed();
    loading.close();
    loading.remove();
  }
  const bool endedBadly = !guardFolder_.isEmpty() && QFile::exists(guardFile("plugins-running"));
  if (endedBadly) QFile::remove(guardFile("plugins-running"));

  auto rowFromJson = [&](const QJsonValue& value) {
    Rack rack;
    const QJsonArray list = value.toArray();
    for (const QJsonValue& item : list) {
      if (!item.isObject() || static_cast<int>(rack.size()) >= maxPerRack()) continue;
      const QJsonObject o = item.toObject();
      Entry e;
      e.path = o.value(QStringLiteral("path")).toString().toStdString();
      e.classId = o.value(QStringLiteral("class")).toString().toStdString();
      e.name = o.value(QStringLiteral("name")).toString().left(120);
      if (e.path.empty() || e.classId.empty()) continue;
      if (e.name.isEmpty()) e.name = QFileInfo(QString::fromStdString(e.path)).completeBaseName();
      e.bypassed = o.value(QStringLiteral("bypassed")).toBool();
      e.blocked = o.value(QStringLiteral("blocked")).toBool();
      const QByteArray state = QByteArray::fromBase64(o.value(QStringLiteral("state")).toString().toLatin1());
      if (state.size() <= kMaxStateBytes) e.saved = state;
      rack.push_back(std::move(e));
    }
    return rack;
  };
  const QJsonObject saved = app_.pluginSettings();
  micRack_ = rowFromJson(saved.value(QStringLiteral("mic")));
  const QJsonArray slotRows = saved.value(QStringLiteral("slots")).toArray();
  for (int row = 0; row < slotRows.size() && row < static_cast<int>(slotRacks_.size()); ++row) {
    slotRacks_[static_cast<std::size_t>(row)] = rowFromJson(slotRows[row]);
  }

  bool any = false;
  auto bring = [&](int target, Rack& rack) {
    for (Entry& e : rack) {
      any = true;
      if (!crashedPath.isEmpty() && QString::fromStdString(e.path) == crashedPath) e.blocked = true;
      if (endedBadly) e.bypassed = true;
      if (e.blocked) {
        e.note = QStringLiteral("前回、読み込み中にアプリが落ちたため外してあります");
        continue;
      }
      load(target, e);
    }
    rebuild(target);
  };
  // Blocked entries are written down before anything is loaded: if another plug-in
  // brings the program down now, this one stays out all the same.
  if (!crashedPath.isEmpty()) {
    auto mark = [&](Rack& rack) {
      for (Entry& e : rack) {
        if (QString::fromStdString(e.path) == crashedPath) e.blocked = true;
      }
    };
    mark(micRack_);
    for (Rack& rack : slotRacks_) mark(rack);
    store();
  }
  bring(kMic, micRack_);
  for (int row = 0; row < static_cast<int>(slotRacks_.size()); ++row) bring(row, slotRacks_[static_cast<std::size_t>(row)]);

  if (!crashedPath.isEmpty()) {
    problem_ = QStringLiteral("前回「%1」の読み込み中にアプリが落ちました。このプラグインは外してあります（行の「もう一度試す」で戻せます）。")
                   .arg(QFileInfo(crashedPath).completeBaseName());
  } else if (endedBadly && any) {
    problem_ = QStringLiteral("前回、プラグインを使っている最中にアプリが正しく終わりませんでした。安全のため、すべてバイパスにして開いています。");
  }
  store();
  emit changed();
}

void PluginController::tick() {
  bool edited = false;
  auto visit = [&](Rack& rack) {
    for (Entry& e : rack) {
      if (e.instance == nullptr) continue;
      e.instance->idle();
      edited = e.instance->takeEdited() || edited;
    }
  };
  visit(micRack_);
  for (Rack& rack : slotRacks_) visit(rack);
  if (edited) {
    quietTicks_ = 0;
  } else if (quietTicks_ >= 0 && ++quietTicks_ >= kQuietTicks) {
    quietTicks_ = -1;
    capture();       // the operator stopped turning knobs: remember where they are
    emit changed();  // a plug-in's delay may have changed with its settings
  }
}

// ---- crash guard ----

QString PluginController::guardFile(const char* name) const {
  return QDir(guardFolder_).filePath(QString::fromLatin1(name));
}

void PluginController::noteLoading(const std::string& path) {
  if (guardFolder_.isEmpty()) return;
  QDir().mkpath(guardFolder_);
  QFile file(guardFile("plugins-loading"));
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    file.write(QString::fromStdString(path).toUtf8());
    file.flush();
  }
}

void PluginController::clearLoading() {
  if (!guardFolder_.isEmpty()) QFile::remove(guardFile("plugins-loading"));
}

void PluginController::noteRunning() {
  if (running_ || guardFolder_.isEmpty()) return;
  QDir().mkpath(guardFolder_);
  QFile file(guardFile("plugins-running"));
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) running_ = true;
}

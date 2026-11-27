// Plug-ins (VST 3) for the sound paths. Nothing outside this folder sees the VST 3 SDK,
// so an SDK update touches only this folder.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/audio_effect.h"

namespace backhaul {

// One effect inside a plug-in file (most files hold exactly one).
struct PluginClass {
  std::string id;      // stable handle for create()
  std::string name;    // what the operator reads (UTF-8)
  std::string vendor;
};

// A loaded plug-in. process() and latencySamples() may be called from a sound thread;
// everything else belongs to the thread that created it (the window thread).
class PluginInstance : public AudioEffect {
 public:
  virtual std::string name() const = 0;
  // Everything the plug-in wants remembered (its settings). Empty when it gives nothing.
  virtual std::vector<std::uint8_t> state() = 0;
  // Puts back what state() gave. False when the plug-in refuses it.
  virtual bool setState(const std::vector<std::uint8_t>& saved) = 0;
  // The plug-in's own window. Only where the system can show it (Windows).
  virtual bool hasEditor() const = 0;
  virtual bool openEditor(const std::string& title) = 0;
  virtual void closeEditor() = 0;
  virtual bool editorOpen() const = 0;
  // Call regularly (about 30 times a second): hands meter values and the like from the
  // sound thread to the plug-in's window, and picks up a changed delay.
  virtual void idle() = 0;
  // True once after the operator changed something in the plug-in's window.
  virtual bool takeEdited() = 0;
};

class PluginHost {
 public:
  virtual ~PluginHost() = default;
  // Paths of the plug-in files installed on this PC. Nothing is loaded for this.
  virtual std::vector<std::string> modules() = 0;
  // Loads the file (plug-in code runs from here on) and lists its effects.
  // Empty with a reason in `error` when the file cannot be used.
  virtual std::vector<PluginClass> classes(const std::string& path, std::string* error) = 0;
  // `channels` is 1 or 2: what process() will be given. A plug-in that only works in
  // stereo can still be used for one channel; one that only works in mono cannot be
  // used for two.
  virtual std::shared_ptr<PluginInstance> create(const std::string& path, const std::string& classId, int channels,
                                                 std::string* error) = 0;
};

// Null when the program was built without plug-in support.
std::unique_ptr<PluginHost> makePluginHost();

}  // namespace backhaul

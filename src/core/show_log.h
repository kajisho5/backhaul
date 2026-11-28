// What happened during a show, for looking at afterwards: when this PC went into the
// meeting and came out, who came and went, when a device dropped out. Kept in memory
// only; the operator can write it to a file (CSV, opens in a spreadsheet).
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>

namespace backhaul {

enum class LogKind {
  MeetingJoined,   // this PC is in the meeting
  MeetingLeft,     // out of it, for good
  MeetingDropped,  // out of it, coming back on its own
  PersonHere,      // was in the meeting when this PC came in
  PersonCame,
  PersonLeft,
  AudioLost,       // the audio output device stopped
  AudioBack,
  AudioOnSpare,    // the spare device took over
  Restarted,       // the program started again after a crash
};

struct LogEntry {
  std::int64_t atMs = 0;  // since 1970, UTC
  LogKind kind = LogKind::MeetingJoined;
  std::string who;        // a person, a meeting or a device; may be empty
};

class ShowLog {
 public:
  // The oldest entries are dropped beyond this many (a day of a big meeting fits easily).
  static constexpr std::size_t kMost = 20000;

  void add(std::int64_t atMs, LogKind kind, std::string who = {});
  const std::deque<LogEntry>& entries() const { return entries_; }
  bool empty() const { return entries_.empty(); }
  std::size_t size() const { return entries_.size(); }
  void clear() { entries_.clear(); }

  // One line per entry under a heading line: time, what happened, who. The caller says
  // how a time and a kind are written (they depend on the language and the time zone).
  std::string csv(const std::string& heading, const std::function<std::string(std::int64_t)>& time,
                  const std::function<std::string(LogKind)>& word) const;

 private:
  std::deque<LogEntry> entries_;
};

// One CSV field. Names come from other people (anyone can call themselves anything in a
// meeting), so: quotes when needed, no line breaks, and a field that a spreadsheet would
// take for a formula (= + - @ at the start) gets an apostrophe in front.
std::string csvField(const std::string& text);

}  // namespace backhaul

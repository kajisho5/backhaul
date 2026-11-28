#include "core/show_log.h"

#include <utility>

namespace backhaul {

void ShowLog::add(std::int64_t atMs, LogKind kind, std::string who) {
  entries_.push_back({atMs, kind, std::move(who)});
  while (entries_.size() > kMost) entries_.pop_front();
}

std::string csvField(const std::string& text) {
  std::string clean;
  clean.reserve(text.size() + 3);
  for (const char c : text) clean.push_back(c == '\r' || c == '\n' || c == '\t' ? ' ' : c);
  if (!clean.empty() && (clean[0] == '=' || clean[0] == '+' || clean[0] == '-' || clean[0] == '@')) clean.insert(clean.begin(), '\'');
  if (clean.find_first_of(",\"") == std::string::npos) return clean;
  std::string quoted = "\"";
  for (const char c : clean) {
    if (c == '"') quoted.push_back('"');
    quoted.push_back(c);
  }
  quoted.push_back('"');
  return quoted;
}

std::string ShowLog::csv(const std::string& heading, const std::function<std::string(std::int64_t)>& time,
                         const std::function<std::string(LogKind)>& word) const {
  std::string out = heading;
  out += "\r\n";
  for (const LogEntry& e : entries_) {
    out += csvField(time(e.atMs));
    out += ',';
    out += csvField(word(e.kind));
    out += ',';
    out += csvField(e.who);
    out += "\r\n";
  }
  return out;
}

}  // namespace backhaul

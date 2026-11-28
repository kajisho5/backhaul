#include <string>

#include "check.h"
#include "core/show_log.h"

using namespace backhaul;

void testShowLog() {
  // Fields: plain stays plain; commas and quotes are quoted; line breaks cannot split a line.
  CHECK(csvField("Alex Sample") == "Alex Sample");
  CHECK(csvField("Sample, Alex") == "\"Sample, Alex\"");
  CHECK(csvField("the \"otter\"") == "\"the \"\"otter\"\"\"");
  CHECK(csvField("two\r\nlines\there") == "two  lines here");
  CHECK(csvField("") == "");
  // A name that a spreadsheet would run as a formula is made harmless.
  CHECK(csvField("=1+1") == "'=1+1");
  CHECK(csvField("+otter") == "'+otter");
  CHECK(csvField("-otter") == "'-otter");
  CHECK(csvField("@otter") == "'@otter");
  CHECK(csvField("=HYPERLINK(\"x\",\"y\")") == "\"'=HYPERLINK(\"\"x\"\",\"\"y\"\")\"");

  ShowLog log;
  CHECK(log.empty());
  log.add(1000, LogKind::MeetingJoined, "Sample Conference");
  log.add(2000, LogKind::PersonCame, "Sample, Alex");
  log.add(3000, LogKind::AudioLost);
  CHECK(log.size() == 3);
  const auto time = [](std::int64_t at) { return "t" + std::to_string(at); };
  const auto word = [](LogKind kind) { return kind == LogKind::MeetingJoined ? "joined" : kind == LogKind::PersonCame ? "came" : "other"; };
  CHECK(log.csv("time,what,who", time, word) == "time,what,who\r\nt1000,joined,Sample Conference\r\nt2000,came,\"Sample, Alex\"\r\nt3000,other,\r\n");

  // Never grows without end: the oldest go first.
  log.clear();
  for (std::size_t i = 0; i < ShowLog::kMost + 5; ++i) log.add(static_cast<std::int64_t>(i), LogKind::PersonCame);
  CHECK(log.size() == ShowLog::kMost && log.entries().front().atMs == 5);
}

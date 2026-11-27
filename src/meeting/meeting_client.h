// What the application needs from a meeting, without naming Zoom or its SDK.
// The Zoom implementation lives in src/zoom; nothing outside that folder sees SDK types.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace backhaul {

struct JoinRequest {
  std::string clientId;      // Zoom app credentials (UTF-8)
  std::string clientSecret;
  std::uint64_t meetingNumber = 0;
  std::string displayName;
  std::string passcode;      // may be empty
  std::string zak;           // may be empty; needed for meetings of other accounts
};

enum class MeetingState {
  Idle,
  Authenticating,          // the app credentials are being checked
  Joining,
  WaitingForPermission,    // in the meeting, but not yet allowed to take picture and sound
  Receiving,
  Failed,                  // see the detail text
  Ended,
};

struct Participant {
  std::uint32_t id = 0;
  std::string name;
  bool isSelf = false;
  bool isHost = false;
  bool isCoHost = false;
  bool videoOn = false;
  bool audioMuted = true;

  bool operator==(const Participant&) const = default;
};

enum class VideoSize { P90, P180, P360, P720, P1080 };

struct VideoWant {
  std::uint32_t userId = 0;
  VideoSize size = VideoSize::P360;
  bool operator==(const VideoWant&) const = default;
};

// One picture. The planes are only valid during the call.
struct VideoFrame {
  const std::uint8_t* y = nullptr;
  const std::uint8_t* u = nullptr;
  const std::uint8_t* v = nullptr;
  int width = 0;
  int height = 0;
  bool limitedRange = true;
};

class MeetingEvents {
 public:
  virtual ~MeetingEvents() = default;
  // Called on the thread that called join() / poll().
  virtual void onMeetingState(MeetingState state, const std::string& detail) = 0;
  virtual void onParticipants(const std::vector<Participant>& everyone) = 0;
  virtual void onMeetingTopic(const std::string& topic) = 0;
  // Called on a thread of the meeting library, many times per second.
  virtual void onVideoFrame(std::uint32_t userId, const VideoFrame& frame) = 0;
  // One person's voice: 16-bit samples, `frames` per channel, channel-interleaved.
  virtual void onAudio(std::uint32_t userId, const std::int16_t* samples, std::size_t frames, int sampleRate,
                       int channels) = 0;
};

class MeetingClient {
 public:
  virtual ~MeetingClient() = default;
  // `events` must outlive the client. Returns false when the request cannot even be started.
  virtual bool join(const JoinRequest& request, MeetingEvents* events) = 0;
  virtual void leave() = 0;
  // Whose picture to receive, and how large. Replaces the previous list.
  virtual void setVideoWants(const std::vector<VideoWant>& wants) = 0;
  // Call a few times per second from the thread that called join(): reports
  // changes in the participant list (camera, mute, names, roles).
  virtual void poll() = 0;
};

// Null when this build has no meeting library.
std::unique_ptr<MeetingClient> makeMeetingClient();

}  // namespace backhaul

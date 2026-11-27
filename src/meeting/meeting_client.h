// What the application needs from a meeting, without naming Zoom or its SDK.
// The Zoom implementation lives in src/zoom; nothing outside that folder sees SDK types.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace backhaul {

// How the sound this PC sends is treated by the meeting service.
enum class SendQuality {
  Speech,    // the service's usual processing for talking
  Original,  // no processing
  Music,     // no processing, higher quality for music
};

// How a capture device's picture goes into the meeting.
enum class SendVideo {
  None,
  Camera,  // as this participant's camera picture
  Share,   // as a shared screen (what Zoom calls content from a second camera)
};

struct JoinRequest {
  std::string clientId;      // Zoom app credentials (UTF-8)
  std::string clientSecret;
  std::uint64_t meetingNumber = 0;
  std::string displayName;
  std::string passcode;      // may be empty
  std::string zak;           // may be empty; needed for meetings of other accounts
  bool audio = true;         // false: do not join the meeting's sound at all (another PC handles it)
  bool sendAudio = false;    // this PC sends a device's sound into the meeting (needs `audio`)
  SendQuality sendQuality = SendQuality::Speech;
  bool sendStereo = false;   // only with Original or Music
  SendVideo sendVideo = SendVideo::None;
  // false: this PC only sends. It then asks for no permission to take pictures and sound.
  bool receive = true;
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

// A screen someone is sharing. One person can share more than one.
struct ShareSource {
  std::uint32_t sourceId = 0;
  std::uint32_t userId = 0;
  bool operator==(const ShareSource&) const = default;
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
  // The screens being shared right now by others (empty when nobody shares).
  virtual void onShares(const std::vector<ShareSource>& shares) = 0;
  // Called on a thread of the meeting library, many times per second.
  virtual void onVideoFrame(std::uint32_t userId, const VideoFrame& frame) = 0;
  virtual void onShareFrame(std::uint32_t sourceId, const VideoFrame& frame) = 0;
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
  // Which shared screen to receive (a sourceId from onShares); 0 for none.
  virtual void setShareWant(std::uint32_t sourceId) = 0;
  // Sound for the meeting: 16-bit, 48 kHz, `frames` per channel (1 or 2 channels,
  // interleaved), at most 100 ms per call. May be called from any one thread.
  // False when it cannot be sent right now (not in a meeting, muted, not asked for).
  virtual bool sendAudio(const std::int16_t* samples, std::size_t frames, int channels) = 0;
  // Whether the meeting is taking the sound at this moment, and if not, why
  // (empty when there is nothing to say).
  virtual bool sendingAudio() const = 0;
  virtual std::string sendAudioNote() const = 0;
  // A picture for the meeting: I420, the three planes one after another without padding.
  // May be called from any one thread. False when it cannot be sent right now.
  virtual bool sendVideo(const std::uint8_t* i420, int width, int height) = 0;
  virtual bool sendingVideo() const = 0;
  virtual std::string sendVideoNote() const = 0;
  // Call a few times per second from the thread that called join(): reports
  // changes in the participant list (camera, mute, names, roles).
  virtual void poll() = 0;
};

// Null when this build has no meeting library.
std::unique_ptr<MeetingClient> makeMeetingClient();

}  // namespace backhaul

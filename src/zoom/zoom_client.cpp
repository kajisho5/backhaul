// MeetingClient on top of the Zoom Meeting SDK for Windows.
#include <windows.h>

#include <chrono>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "auth_service_interface.h"
#include "meeting_service_components/meeting_audio_interface.h"
#include "meeting_service_components/meeting_participants_ctrl_interface.h"
#include "meeting_service_components/meeting_recording_interface.h"
#include "meeting_service_interface.h"

// Needs types from meeting_service_interface.h, so it comes after it.
#include "meeting_service_components/meeting_sharing_interface.h"
#include "meeting_service_components/meeting_video_interface.h"
#include "rawdata/rawdata_audio_helper_interface.h"
#include "rawdata/rawdata_renderer_interface.h"
#include "rawdata/rawdata_share_source_helper_interface.h"
#include "rawdata/rawdata_video_source_helper_interface.h"
#include "rawdata/zoom_rawdata_api.h"
#include "setting_service_interface.h"
#include "zoom_sdk.h"
#include "zoom_sdk_raw_data_def.h"

#include "core/nv12.h"
#include "core/sdk_token.h"
#include "meeting/meeting_client.h"

namespace backhaul {
namespace {

namespace sdk = ZOOM_SDK_NAMESPACE;

std::string toUtf8(const wchar_t* text) {
  if (text == nullptr || *text == L'\0') return {};
  const int need = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (need <= 1) return {};
  std::string out(static_cast<std::size_t>(need), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), need, nullptr, nullptr);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

std::wstring toWide(const std::string& text) {
  if (text.empty()) return {};
  const int need = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (need <= 1) return {};
  std::wstring out(static_cast<std::size_t>(need), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), need);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

std::string code(const char* what, int value) { return std::string(what) + " (" + std::to_string(value) + ")"; }

sdk::ZoomSDKResolution toSdk(VideoSize size) {
  switch (size) {
    case VideoSize::P1080: return sdk::ZoomSDKResolution_1080P;
    case VideoSize::P720: return sdk::ZoomSDKResolution_720P;
    case VideoSize::P360: return sdk::ZoomSDKResolution_360P;
    case VideoSize::P180: return sdk::ZoomSDKResolution_180P;
    case VideoSize::P90: break;
  }
  return sdk::ZoomSDKResolution_90P;
}

// Receives one person's pictures and hands them on.
class VideoTap final : public sdk::IZoomSDKRendererDelegate {
 public:
  VideoTap(std::uint32_t userId, MeetingEvents* events) : userId_(userId), events_(events) {}

  void onRendererBeDestroyed() override {}
  void onRawDataFrameReceived(YUVRawDataI420* data) override {
    if (data == nullptr) return;
    VideoFrame frame;
    frame.y = reinterpret_cast<const std::uint8_t*>(data->GetYBuffer());
    frame.u = reinterpret_cast<const std::uint8_t*>(data->GetUBuffer());
    frame.v = reinterpret_cast<const std::uint8_t*>(data->GetVBuffer());
    frame.width = static_cast<int>(data->GetStreamWidth());
    frame.height = static_cast<int>(data->GetStreamHeight());
    frame.limitedRange = data->IsLimitedI420();
    if (frame.y == nullptr || frame.u == nullptr || frame.v == nullptr || frame.width <= 0 || frame.height <= 0) return;
    events_->onVideoFrame(userId_, frame);
  }
  void onRawDataStatusChanged(RawDataStatus) override {}
  void onShareCursorDataReceived(ZoomSDKShareCursorData) override {}

 private:
  std::uint32_t userId_;
  MeetingEvents* events_;
};

// Receives the pictures of one shared screen.
class ShareTap final : public sdk::IZoomSDKRendererDelegate {
 public:
  ShareTap(std::uint32_t sourceId, MeetingEvents* events) : sourceId_(sourceId), events_(events) {}
  void onRendererBeDestroyed() override {}
  void onRawDataFrameReceived(YUVRawDataI420* data) override {
    if (data == nullptr) return;
    VideoFrame frame;
    frame.y = reinterpret_cast<const std::uint8_t*>(data->GetYBuffer());
    frame.u = reinterpret_cast<const std::uint8_t*>(data->GetUBuffer());
    frame.v = reinterpret_cast<const std::uint8_t*>(data->GetVBuffer());
    frame.width = static_cast<int>(data->GetStreamWidth());
    frame.height = static_cast<int>(data->GetStreamHeight());
    frame.limitedRange = data->IsLimitedI420();
    if (frame.y == nullptr || frame.u == nullptr || frame.v == nullptr || frame.width <= 0 || frame.height <= 0) return;
    events_->onShareFrame(sourceId_, frame);
  }
  void onRawDataStatusChanged(RawDataStatus) override {}
  void onShareCursorDataReceived(ZoomSDKShareCursorData) override {}

 private:
  std::uint32_t sourceId_;
  MeetingEvents* events_;
};

// Receives everyone's voices, one person per call.
class AudioTap final : public sdk::IZoomSDKAudioRawDataDelegate {
 public:
  void setEvents(MeetingEvents* events) { events_ = events; }

  void onMixedAudioRawDataReceived(AudioRawData*) override {}
  void onOneWayAudioRawDataReceived(AudioRawData* data, uint32_t userId) override {
    if (data == nullptr || events_ == nullptr) return;
    const unsigned int channels = data->GetChannelNum();
    const char* buffer = data->GetBuffer();
    if (buffer == nullptr || channels < 1 || channels > 2) return;
    const std::size_t frames = data->GetBufferLen() / (2u * channels);
    events_->onAudio(userId, reinterpret_cast<const std::int16_t*>(buffer), frames,
                     static_cast<int>(data->GetSampleRate()), static_cast<int>(channels));
  }
  void onShareAudioRawDataReceived(AudioRawData*, uint32_t) override {}
  void onOneWayInterpreterAudioRawDataReceived(AudioRawData*, const zchar_t*) override {}

 private:
  MeetingEvents* events_ = nullptr;
};

// The meeting's "microphone" when this PC sends a device's sound. The meeting library
// says when it is ready and when it wants sound; sound arrives from another thread.
class MicSource final : public sdk::IZoomSDKVirtualAudioMicEvent {
 public:
  void onMicInitialize(sdk::IZoomSDKAudioRawDataSender* sender) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    sender_ = sender;
  }
  void onMicStartSend() override { sending_ = true; }
  void onMicStopSend() override { sending_ = false; }
  void onMicUninitialized() override {
    const std::lock_guard<std::mutex> lock(mutex_);
    sender_ = nullptr;
    sending_ = false;
  }

  bool sending() const { return sending_; }

  bool send(const std::int16_t* samples, std::size_t frames, int channels) {
    if (samples == nullptr || frames == 0 || frames > 4800 || channels < 1 || channels > 2) return false;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (sender_ == nullptr || !sending_) return false;
    const unsigned int bytes = static_cast<unsigned int>(frames) * 2u * static_cast<unsigned int>(channels);
    return sender_->send(const_cast<char*>(reinterpret_cast<const char*>(samples)), bytes, 48000,
                         channels == 2 ? sdk::ZoomSDKAudioChannel_Stereo : sdk::ZoomSDKAudioChannel_Mono) == sdk::SDKERR_SUCCESS;
  }

 private:
  std::mutex mutex_;
  sdk::IZoomSDKAudioRawDataSender* sender_ = nullptr;
  std::atomic<bool> sending_{false};
};

// This PC's "camera" when it sends a capture device's picture as a participant.
class CameraSource final : public sdk::IZoomSDKVideoSource {
 public:
  void onInitialize(sdk::IZoomSDKVideoSender* sender, sdk::IList<sdk::VideoSourceCapability>*,
                    sdk::VideoSourceCapability&) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    sender_ = sender;
  }
  void onPropertyChange(sdk::IList<sdk::VideoSourceCapability>*, sdk::VideoSourceCapability) override {}
  void onStartSend() override { sending_ = true; }
  void onStopSend() override { sending_ = false; }
  void onUninitialized() override {
    const std::lock_guard<std::mutex> lock(mutex_);
    sender_ = nullptr;
    sending_ = false;
  }
  bool sending() const { return sending_; }
  bool send(const std::uint8_t* i420, int width, int height) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (sender_ == nullptr || !sending_) return false;
    return sender_->sendVideoFrame(const_cast<char*>(reinterpret_cast<const char*>(i420)), width, height,
                                   static_cast<int>(i420Size(width, height)), 0) == sdk::SDKERR_SUCCESS;
  }

 private:
  std::mutex mutex_;
  sdk::IZoomSDKVideoSender* sender_ = nullptr;
  std::atomic<bool> sending_{false};
};

// The same picture sent as a shared screen.
class ScreenSource final : public sdk::IZoomSDKShareSource {
 public:
  void onStartSend(sdk::IZoomSDKShareSender* sender) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    sender_ = sender;
    sending_ = sender != nullptr;
  }
  void onStopSend() override {
    const std::lock_guard<std::mutex> lock(mutex_);
    sender_ = nullptr;
    sending_ = false;
  }
  bool sending() const { return sending_; }
  bool send(const std::uint8_t* i420, int width, int height) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (sender_ == nullptr) return false;
    return sender_->sendShareFrame(const_cast<char*>(reinterpret_cast<const char*>(i420)), width, height,
                                   static_cast<int>(i420Size(width, height)),
                                   sdk::FrameDataFormat_I420_LIMITED) == sdk::SDKERR_SUCCESS;
  }

 private:
  std::mutex mutex_;
  sdk::IZoomSDKShareSender* sender_ = nullptr;
  std::atomic<bool> sending_{false};
};

struct Subscription {
  std::unique_ptr<VideoTap> tap;
  sdk::IZoomSDKRenderer* renderer = nullptr;
  VideoSize size = VideoSize::P360;
};

class ZoomClient final : public MeetingClient,
                         public sdk::IAuthServiceEvent,
                         public sdk::IMeetingServiceEvent,
                         public sdk::IMeetingRecordingCtrlEvent {
 public:
  ~ZoomClient() override { shutDown(); }

  bool join(const JoinRequest& request, MeetingEvents* events) override {
    if (events == nullptr || busy_) return false;
    events_ = events;
    request_ = request;
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    // Issued a minute in the past so a slightly fast PC clock is not rejected.
    const std::string token = !request.sdkToken.empty()
                                  ? request.sdkToken
                                  : makeSdkToken(request.clientId, request.clientSecret, now - 60, 24 * 3600);
    if (token.empty() || request.meetingNumber == 0) {
      state(MeetingState::Failed, "Zoomアプリの鍵か会議IDが正しくありません");
      return false;
    }
    if (!sdkReady_) {
      sdk::InitParam init;
      init.strWebDomain = L"https://zoom.us";
      init.enableLogByDefault = true;
      // No Zoom windows: this program shows everything itself.
      init.obConfigOpts.optionalFeatures = ENABLE_CUSTOMIZED_UI_FLAG;
      const sdk::SDKError result = sdk::InitSDK(init);
      if (result != sdk::SDKERR_SUCCESS) {
        state(MeetingState::Failed, code("Zoomの部品を初期化できません", static_cast<int>(result)));
        return false;
      }
      sdkReady_ = true;
    }
    if (auth_ == nullptr) {
      const sdk::SDKError result = sdk::CreateAuthService(&auth_);
      if (result != sdk::SDKERR_SUCCESS || auth_ == nullptr) {
        state(MeetingState::Failed, code("Zoomの認証を始められません", static_cast<int>(result)));
        return false;
      }
      auth_->SetEvent(this);
    }
    busy_ = true;
    token_ = toWide(token);
    sdk::AuthContext context;
    context.jwt_token = token_.c_str();
    const sdk::SDKError result = auth_->SDKAuth(context);
    if (result != sdk::SDKERR_SUCCESS) {
      busy_ = false;
      state(MeetingState::Failed, code("Zoomの認証を始められません", static_cast<int>(result)));
      return false;
    }
    state(MeetingState::Authenticating, {});
    return true;
  }

  void leave() override {
    stopRaw();
    // Also on the way in (connecting, waiting room, reconnecting): otherwise the meeting
    // library carries on and enters the meeting after this program said it had left.
    if (meeting_ != nullptr && onTheWayInOrIn(meeting_->GetMeetingStatus())) meeting_->Leave(sdk::LEAVE_MEETING);
    endOfMeeting();
    if (events_ != nullptr) state(MeetingState::Idle, {});
  }

  void setVideoWants(const std::vector<VideoWant>& wants) override {
    wants_ = wants;
    applyWants();
  }

  bool sendVideo(const std::uint8_t* i420, int width, int height) override {
    if (i420 == nullptr || width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    // Set before joining and unchanged during the meeting, so safe to read from the sending thread.
    switch (request_.sendVideo) {
      case SendVideo::Camera: return camera_.send(i420, width, height);
      case SendVideo::Share: return screen_.send(i420, width, height);
      case SendVideo::None: break;
    }
    return false;
  }
  bool sendingVideo() const override { return camera_.sending() || screen_.sending(); }
  std::string sendVideoNote() const override {
    const std::lock_guard<std::mutex> lock(noteMutex_);
    return videoNote_;
  }

  void setShareWant(std::uint32_t sourceId) override {
    shareWant_ = sourceId;
    applyShareWant();
  }

  bool sendAudio(const std::int16_t* samples, std::size_t frames, int channels) override {
    return mic_.send(samples, frames, channels);
  }
  bool sendingAudio() const override { return mic_.sending(); }
  std::string sendAudioNote() const override {
    const std::lock_guard<std::mutex> lock(noteMutex_);
    return sendNote_;
  }

  void poll() override {
    if (participants_ == nullptr || events_ == nullptr) return;
    std::vector<Participant> everyone;
    sdk::IList<unsigned int>* ids = participants_->GetParticipantsList();
    if (ids != nullptr) {
      for (int i = 0; i < ids->GetCount(); ++i) {
        sdk::IUserInfo* info = participants_->GetUserByUserID(ids->GetItem(i));
        if (info == nullptr || info->IsInWaitingRoom()) continue;
        Participant p;
        p.id = info->GetUserID();
        p.name = toUtf8(info->GetUserName());
        p.isSelf = info->IsMySelf();
        p.isHost = info->IsHost();
        p.isCoHost = info->GetUserRole() == sdk::USERROLE_COHOST;
        p.videoOn = info->IsVideoOn();
        p.audioMuted = info->IsAudioMuted();
        everyone.push_back(std::move(p));
      }
    }
    if (inMeeting_ && request_.audio && request_.sendAudio) keepSending(everyone);
    if (inMeeting_ && request_.audio && !request_.sendAudio) keepMuted(everyone);
    if (inMeeting_ && request_.sendVideo != SendVideo::None) keepSendingVideo();
    if (everyone != lastParticipants_) {
      lastParticipants_ = everyone;
      events_->onParticipants(everyone);
      applyWants();  // someone who was wanted may have just arrived
    }
    pollShares();
    // Permission can arrive without a notice (for example when made co-host).
    if (inMeeting_ && !rawStarted_ && request_.receive) tryStartRaw();
  }

  // IAuthServiceEvent
  void onAuthenticationReturn(sdk::AuthResult result) override {
    if (!busy_) return;
    if (result != sdk::AUTHRET_SUCCESS) {
      busy_ = false;
      state(MeetingState::Failed, code("Zoomアプリの鍵が通りませんでした", static_cast<int>(result)));
      return;
    }
    startJoin();
  }
  void onLoginReturnWithReason(sdk::LOGINSTATUS, sdk::IAccountInfo*, sdk::LoginFailReason) override {}
  void onLogout() override {}
  void onZoomIdentityExpired() override {}
  void onZoomAuthIdentityExpired() override {}
  void onNotificationServiceStatus(sdk::SDKNotificationServiceStatus, sdk::SDKNotificationServiceError) override {}

  // IMeetingServiceEvent
  void onMeetingStatusChanged(sdk::MeetingStatus status, int result) override {
    switch (status) {
      case sdk::MEETING_STATUS_INMEETING:
        if (!busy_) {
          // Nobody asked to be here any more (left while this was on its way): go out again.
          if (meeting_ != nullptr) meeting_->Leave(sdk::LEAVE_MEETING);
          break;
        }
        enterMeeting();
        break;
      case sdk::MEETING_STATUS_FAILED:
        endOfMeeting();
        state(MeetingState::Failed, code("会議に入れませんでした", result));
        break;
      case sdk::MEETING_STATUS_ENDED:
        stopRaw();
        endOfMeeting();
        state(MeetingState::Ended, {});
        break;
      case sdk::MEETING_STATUS_IN_WAITING_ROOM:
        state(MeetingState::Joining, "待機室にいます。ホストの入室許可を待っています");
        break;
      case sdk::MEETING_STATUS_RECONNECTING:
        state(MeetingState::Joining, "つなぎ直しています");
        break;
      default:
        break;
    }
  }
  void onMeetingStatisticsWarningNotification(sdk::StatisticsWarningType) override {}
  void onMeetingParameterNotification(const sdk::MeetingParameter*) override {}
  void onSuspendParticipantsActivities() override {}
  void onAICompanionActiveChangeNotice(bool) override {}
  void onMeetingTopicChanged(const zchar_t* topic) override {
    if (events_ != nullptr) events_->onMeetingTopic(toUtf8(topic));
  }
  void onMeetingFullToWatchLiveStream(const zchar_t*) override {}
  void onUserNetworkStatusChanged(sdk::MeetingComponentType, sdk::ConnectionQuality, unsigned int, bool) override {}
  void onAppSignalPanelUpdated(sdk::IMeetingAppSignalHandler*) override {}

  // IMeetingRecordingCtrlEvent
  void onRecordingStatus(sdk::RecordingStatus) override {}
  void onCloudRecordingStatus(sdk::RecordingStatus) override {}
  void onRecordPrivilegeChanged(bool canRecord) override {
    if (canRecord) tryStartRaw();
  }
  void onLocalRecordingPrivilegeRequestStatus(sdk::RequestLocalRecordingStatus status) override {
    if (status == sdk::RequestLocalRecording_Granted) tryStartRaw();
  }
  void onRequestCloudRecordingResponse(sdk::RequestStartCloudRecordingStatus) override {}
  void onLocalRecordingPrivilegeRequested(sdk::IRequestLocalRecordingPrivilegeHandler*) override {}
  void onStartCloudRecordingRequested(sdk::IRequestStartCloudRecordingHandler*) override {}
  void onRecording2MP4Done(bool, int, const zchar_t*) override {}
  void onRecording2MP4Processing(int) override {}
  void onCustomizedLocalRecordingSourceNotification(sdk::ICustomizedLocalRecordingLayoutHelper*) override {}
  void onCloudRecordingStorageFull(time_t) override {}
  void onEnableAndStartSmartRecordingRequested(sdk::IRequestEnableAndStartSmartRecordingHandler*) override {}
  void onSmartRecordingEnableActionCallback(sdk::ISmartRecordingEnableActionHandler*) override {}

 private:
  // True from the moment a join was started until the meeting is over for this PC.
  static bool onTheWayInOrIn(sdk::MeetingStatus status) {
    switch (status) {
      case sdk::MEETING_STATUS_CONNECTING:
      case sdk::MEETING_STATUS_WAITINGFORHOST:
      case sdk::MEETING_STATUS_INMEETING:
      case sdk::MEETING_STATUS_RECONNECTING:
      case sdk::MEETING_STATUS_IN_WAITING_ROOM:
        return true;
      default:
        return false;
    }
  }

  void state(MeetingState s, const std::string& detail) {
    if (events_ != nullptr) events_->onMeetingState(s, detail);
  }

  void startJoin() {
    if (meeting_ == nullptr) {
      const sdk::SDKError result = sdk::CreateMeetingService(&meeting_);
      if (result != sdk::SDKERR_SUCCESS || meeting_ == nullptr) {
        busy_ = false;
        state(MeetingState::Failed, code("会議に入る準備ができません", static_cast<int>(result)));
        return;
      }
      meeting_->SetEvent(this);
    }
    name_ = toWide(request_.displayName);
    passcode_ = toWide(request_.passcode);
    zak_ = toWide(request_.zak);

    sdk::JoinParam param;
    param.userType = sdk::SDK_UT_WITHOUT_LOGIN;
    sdk::JoinParam4WithoutLogin& p = param.param.withoutloginuserJoin;
    p.meetingNumber = request_.meetingNumber;
    p.userName = name_.c_str();
    p.psw = passcode_.empty() ? nullptr : passcode_.c_str();
    p.userZAK = zak_.empty() ? nullptr : zak_.c_str();
    p.isVideoOff = true;
    p.isAudioOff = !request_.audio;
    p.eAudioRawdataSamplingRate = sdk::AudioRawdataSamplingRate_48K;
    p.eVideoRawdataColorspace = sdk::VideoRawdataColorspace_BT709_L;
    const sdk::SDKError result = meeting_->Join(param);
    if (result != sdk::SDKERR_SUCCESS) {
      busy_ = false;
      state(MeetingState::Failed, code("会議に入れませんでした", static_cast<int>(result)));
      return;
    }
    state(MeetingState::Joining, {});
  }

  void enterMeeting() {
    if (inMeeting_) {  // back from a reconnect
      // Say where things stand again: the screen showed "reconnecting" until now.
      if (!request_.receive || rawStarted_) {
        state(MeetingState::Receiving, {});
      } else {
        state(MeetingState::WaitingForPermission, {});
      }
      return;
    }
    inMeeting_ = true;
    participants_ = meeting_->GetMeetingParticipantsController();
    recording_ = meeting_->GetMeetingRecordingController();
    sharing_ = meeting_->GetMeetingShareController();
    if (recording_ != nullptr) recording_->SetEvent(this);
    sdk::IMeetingInfo* info = meeting_->GetMeetingInfo();
    if (info != nullptr && events_ != nullptr) events_->onMeetingTopic(toUtf8(info->GetMeetingTopic()));
    // Without the Zoom window nobody presses "join audio", so do it here.
    // A PC that only shows pictures stays out of the sound, so that it can never
    // play or send any.
    sdk::IMeetingAudioController* audio = meeting_->GetMeetingAudioController();
    if (request_.audio && audio != nullptr) audio->JoinVoip();
    if (request_.audio && request_.sendAudio) applySendQuality();
    if (request_.receive) {
      state(MeetingState::WaitingForPermission, {});
      tryStartRaw();
    } else {
      // A PC that only sends needs no permission to take pictures and sound from the meeting.
      state(MeetingState::Receiving, {});
    }
    poll();
  }

  // Who is sharing a screen right now (not this PC itself).
  void pollShares() {
    if (sharing_ == nullptr || events_ == nullptr) return;
    std::vector<ShareSource> shares;
    sdk::IList<unsigned int>* users = sharing_->GetViewableSharingUserList();
    if (users != nullptr) {
      for (int i = 0; i < users->GetCount(); ++i) {
        const unsigned int userId = users->GetItem(i);
        sdk::IList<sdk::ZoomSDKSharingSourceInfo>* sources = sharing_->GetSharingSourceInfoList(userId);
        if (sources == nullptr) continue;
        for (int k = 0; k < sources->GetCount(); ++k) {
          const sdk::ZoomSDKSharingSourceInfo info = sources->GetItem(k);
          if (info.shareSourceID != 0) shares.push_back({info.shareSourceID, info.userid});
        }
      }
    }
    if (shares != lastShares_) {
      lastShares_ = shares;
      events_->onShares(shares);
      applyShareWant();  // the wanted screen may have just started or ended
    }
  }

  void dropShare() {
    if (share_.renderer != nullptr) {
      share_.renderer->unSubscribe();
      sdk::destroyRenderer(share_.renderer);
    }
    share_.renderer = nullptr;
    share_.tap.reset();
    shareGot_ = 0;
  }

  void applyShareWant() {
    bool present = false;
    for (const ShareSource& source : lastShares_) present = present || source.sourceId == shareWant_;
    const std::uint32_t want = rawStarted_ && present ? shareWant_ : 0;
    if (want == shareGot_) return;
    dropShare();
    if (want == 0) return;
    share_.tap = std::make_unique<ShareTap>(want, events_);
    if (sdk::createRenderer(&share_.renderer, share_.tap.get()) != sdk::SDKERR_SUCCESS || share_.renderer == nullptr) {
      share_.renderer = nullptr;
      share_.tap.reset();
      return;  // tried again at the next change or poll
    }
    share_.renderer->setRawDataResolution(sdk::ZoomSDKResolution_1080P);
    if (share_.renderer->subscribe(want, sdk::RAW_DATA_TYPE_SHARE) != sdk::SDKERR_SUCCESS) {
      dropShare();
      return;
    }
    shareGot_ = want;
  }

  void setSendNote(const std::string& note) {
    const std::lock_guard<std::mutex> lock(noteMutex_);
    sendNote_ = note;
  }

  // "Original sound", music mode and stereo are settings of the meeting library.
  void applySendQuality() {
    if (settings_ == nullptr && sdk::CreateSettingService(&settings_) != sdk::SDKERR_SUCCESS) settings_ = nullptr;
    sdk::IAudioSettingContext* audio = settings_ != nullptr ? settings_->GetAudioSettings() : nullptr;
    if (audio == nullptr) {
      qualityNote_ = "音質の設定を反映できませんでした";
      return;
    }
    const bool original = request_.sendQuality != SendQuality::Speech;
    bool ok = audio->EnableMicOriginalInput(original) == sdk::SDKERR_SUCCESS;
    if (original) {
      // Both are only valid while "original sound" is on.
      ok = audio->EnableHighFidelityMusicMode(request_.sendQuality == SendQuality::Music) == sdk::SDKERR_SUCCESS && ok;
      ok = audio->EnableStereoAudio(request_.sendStereo) == sdk::SDKERR_SUCCESS && ok;
    }
    qualityNote_ = ok ? std::string() : std::string("音質の設定を一部反映できませんでした");
  }

  // Becomes the meeting's microphone and unmutes once. Called from poll() until both
  // have worked: right after joining, the meeting is often not ready for either.
  void keepSending(const std::vector<Participant>& everyone) {
    if (!micSet_) {
      sdk::IZoomSDKAudioRawDataHelper* helper = sdk::GetAudioRawdataHelper();
      const sdk::SDKError result =
          helper != nullptr ? helper->setExternalAudioSource(&mic_) : sdk::SDKERR_UNINITIALIZE;
      micSet_ = result == sdk::SDKERR_SUCCESS;
      if (!micSet_) {
        setSendNote(code("Zoomへ音を送る準備ができていません", static_cast<int>(result)));
        return;
      }
    }
    if (!unmuted_) {
      // Once only: if the host mutes this PC later, that is the host's decision.
      for (const Participant& p : everyone) {
        if (!p.isSelf) continue;
        if (!p.audioMuted) {
          unmuted_ = true;
        } else {
          sdk::IMeetingAudioController* audio = meeting_->GetMeetingAudioController();
          unmuted_ = audio != nullptr && audio->UnMuteAudio(p.id) == sdk::SDKERR_SUCCESS;
        }
      }
      if (!unmuted_) {
        setSendNote("ミュートを解除できていません");
        return;
      }
    }
    bool mutedNow = false;
    for (const Participant& p : everyone) {
      if (p.isSelf) mutedNow = p.audioMuted;
    }
    setSendNote(mutedNow ? std::string("ミュートされています（ホストに解除を頼んでください）") : qualityNote_);
  }

  void setVideoNote(const std::string& note) {
    const std::lock_guard<std::mutex> lock(noteMutex_);
    videoNote_ = note;
  }

  // Registers this PC as the source of its camera picture or of a shared screen.
  // Called from poll() until it has worked.
  void keepSendingVideo() {
    if (videoSet_) {
      setVideoNote(sendingVideo() ? shareRateNote_ : std::string("Zoomがまだ映像を受け取っていません"));
      return;
    }
    if (request_.sendVideo == SendVideo::Camera) {
      sdk::IZoomSDKVideoSourceHelper* helper = sdk::GetRawdataVideoSourceHelper();
      const sdk::SDKError result = helper != nullptr
                                       ? helper->setExternalVideoSource(&camera_, sdk::FrameDataFormat_I420_LIMITED)
                                       : sdk::SDKERR_UNINITIALIZE;
      if (result != sdk::SDKERR_SUCCESS) {
        setVideoNote(code("Zoomへ映像を送る準備ができていません", static_cast<int>(result)));
        return;
      }
      sdk::IMeetingVideoController* video = meeting_->GetMeetingVideoController();
      const sdk::SDKError on = video != nullptr ? video->UnmuteVideo() : sdk::SDKERR_UNINITIALIZE;
      if (on != sdk::SDKERR_SUCCESS) {
        setVideoNote(code("カメラをオンにできていません（ホストに止められていませんか）", static_cast<int>(on)));
        return;  // the source stays registered; only the unmute is tried again
      }
    } else {
      // A shared screen may run at 60 pictures per second when "optimise for video" is on
      // and the quality is asked for; without both the meeting keeps its lower default.
      sdk::IMeetingShareController* sharing = meeting_->GetMeetingShareController();
      shareRateNote_.clear();
      if (sharing == nullptr || sharing->EnableOptimizeForFullScreenVideoClip(true) != sdk::SDKERR_SUCCESS) {
        shareRateNote_ = "動画向けの共有にできませんでした（毎秒60コマにならない場合があります）";
      } else {
        sdk::SDKError quality = sharing->SetVideoSharingQuality(sdk::ZoomSDKVideoSharingQuality_1080p_60fps);
        if (quality != sdk::SDKERR_SUCCESS) {
          quality = sharing->SetVideoSharingQuality(sdk::ZoomSDKVideoSharingQuality_720p_60fps);
          shareRateNote_ = quality == sdk::SDKERR_SUCCESS
                               ? std::string("1080p・60コマにできなかったため、720p・60コマで共有しています")
                               : code("毎秒60コマの共有にできませんでした", static_cast<int>(quality));
        }
      }
      sdk::IZoomSDKShareSourceHelper* helper = sdk::GetRawdataShareSourceHelper();
      const sdk::SDKError result =
          helper != nullptr ? helper->setExternalShareSource(&screen_) : sdk::SDKERR_UNINITIALIZE;
      if (result != sdk::SDKERR_SUCCESS) {
        setVideoNote(code("画面共有を始められていません（ほかの人が共有中か、共有が許可されていません）",
                          static_cast<int>(result)));
        return;
      }
    }
    videoSet_ = true;
    setVideoNote(shareRateNote_);
  }

  void stopSendingVideo() {
    if (videoSet_ && inMeeting_ && meeting_ != nullptr) {
      if (request_.sendVideo == SendVideo::Share) {
        sdk::IMeetingShareController* sharing = meeting_->GetMeetingShareController();
        if (sharing != nullptr) sharing->StopShare();
      } else {
        sdk::IMeetingVideoController* video = meeting_->GetMeetingVideoController();
        if (video != nullptr) video->MuteVideo();
      }
    }
    videoSet_ = false;
    shareRateNote_.clear();
    setVideoNote({});
  }

  // A PC that only plays the meeting's voices must never send this PC's own microphone:
  // joining the sound opens the default microphone, and the meeting may let people in unmuted.
  void keepMuted(const std::vector<Participant>& everyone) {
    for (const Participant& p : everyone) {
      if (!p.isSelf || p.audioMuted) continue;
      sdk::IMeetingAudioController* audio = meeting_->GetMeetingAudioController();
      if (audio != nullptr) audio->MuteAudio(p.id, true);
    }
  }

  void stopSending() {
    if (micSet_) {
      sdk::IZoomSDKAudioRawDataHelper* helper = sdk::GetAudioRawdataHelper();
      if (helper != nullptr) helper->setExternalAudioSource(nullptr);
    }
    micSet_ = false;
    unmuted_ = false;
    setSendNote({});
  }

  void tryStartRaw() {
    if (rawStarted_ || recording_ == nullptr) return;
    if (recording_->CanStartRawRecording() != sdk::SDKERR_SUCCESS) {
      if (!askedPermission_) {
        askedPermission_ = true;
        if (recording_->IsSupportRequestLocalRecordingPrivilege() == sdk::SDKERR_SUCCESS) {
          recording_->RequestLocalRecordingPrivilege();
        }
      }
      return;
    }
    if (recording_->StartRawRecording() != sdk::SDKERR_SUCCESS) return;
    rawStarted_ = true;
    audioTap_.setEvents(events_);
    if (request_.audio) {
      sdk::IZoomSDKAudioRawDataHelper* audio = sdk::GetAudioRawdataHelper();
      audioSubscribed_ = audio != nullptr && audio->subscribe(&audioTap_) == sdk::SDKERR_SUCCESS;
    }
    state(MeetingState::Receiving,
          audioSubscribed_ || !request_.audio ? std::string() : std::string("音声を受け取れませんでした（映像のみ）"));
    applyWants();
    applyShareWant();
  }

  bool present(std::uint32_t userId) const {
    for (const Participant& p : lastParticipants_) {
      if (p.id == userId) return !p.isSelf;
    }
    return false;
  }

  void applyWants() {
    if (!rawStarted_) return;
    // Drop what is no longer wanted, or wanted at another size, or gone.
    for (auto it = video_.begin(); it != video_.end();) {
      bool keep = false;
      for (const VideoWant& want : wants_) {
        if (want.userId == it->first && want.size == it->second.size && present(want.userId)) keep = true;
      }
      if (keep) {
        ++it;
      } else {
        it->second.renderer->unSubscribe();
        sdk::destroyRenderer(it->second.renderer);
        it = video_.erase(it);
      }
    }
    for (const VideoWant& want : wants_) {
      if (video_.count(want.userId) != 0 || !present(want.userId)) continue;
      Subscription sub;
      sub.size = want.size;
      sub.tap = std::make_unique<VideoTap>(want.userId, events_);
      if (sdk::createRenderer(&sub.renderer, sub.tap.get()) != sdk::SDKERR_SUCCESS || sub.renderer == nullptr) continue;
      sub.renderer->setRawDataResolution(toSdk(want.size));
      if (sub.renderer->subscribe(want.userId, sdk::RAW_DATA_TYPE_VIDEO) != sdk::SDKERR_SUCCESS) {
        sdk::destroyRenderer(sub.renderer);
        continue;
      }
      video_.emplace(want.userId, std::move(sub));
    }
  }

  void stopRaw() {
    for (auto& entry : video_) {
      entry.second.renderer->unSubscribe();
      sdk::destroyRenderer(entry.second.renderer);
    }
    video_.clear();
    dropShare();
    if (audioSubscribed_) {
      sdk::IZoomSDKAudioRawDataHelper* audio = sdk::GetAudioRawdataHelper();
      if (audio != nullptr) audio->unSubscribe();
      audioSubscribed_ = false;
    }
    audioTap_.setEvents(nullptr);
    if (rawStarted_ && recording_ != nullptr) recording_->StopRawRecording();
    rawStarted_ = false;
  }

  void endOfMeeting() {
    stopSending();
    stopSendingVideo();
    inMeeting_ = false;
    busy_ = false;
    askedPermission_ = false;
    participants_ = nullptr;
    recording_ = nullptr;
    sharing_ = nullptr;
    if (!lastShares_.empty()) {
      lastShares_.clear();
      if (events_ != nullptr) events_->onShares({});
    }
    if (!lastParticipants_.empty()) {
      lastParticipants_.clear();
      if (events_ != nullptr) events_->onParticipants({});
    }
  }

  void shutDown() {
    stopRaw();
    events_ = nullptr;
    if (meeting_ != nullptr) {
      if (onTheWayInOrIn(meeting_->GetMeetingStatus())) meeting_->Leave(sdk::LEAVE_MEETING);
      sdk::DestroyMeetingService(meeting_);
      meeting_ = nullptr;
    }
    if (settings_ != nullptr) {
      sdk::DestroySettingService(settings_);
      settings_ = nullptr;
    }
    if (auth_ != nullptr) {
      sdk::DestroyAuthService(auth_);
      auth_ = nullptr;
    }
    if (sdkReady_) {
      sdk::CleanUPSDK();
      sdkReady_ = false;
    }
  }

  MeetingEvents* events_ = nullptr;
  JoinRequest request_;
  std::wstring token_, name_, passcode_, zak_;
  sdk::IAuthService* auth_ = nullptr;
  sdk::IMeetingService* meeting_ = nullptr;
  sdk::IMeetingParticipantsController* participants_ = nullptr;
  sdk::IMeetingRecordingController* recording_ = nullptr;
  std::vector<VideoWant> wants_;
  std::vector<Participant> lastParticipants_;
  std::map<std::uint32_t, Subscription> video_;
  bool sdkReady_ = false;
  bool busy_ = false;
  bool inMeeting_ = false;
  bool rawStarted_ = false;
  bool askedPermission_ = false;
  bool audioSubscribed_ = false;
  AudioTap audioTap_;
  sdk::IMeetingShareController* sharing_ = nullptr;
  std::vector<ShareSource> lastShares_;
  struct {
    std::unique_ptr<ShareTap> tap;
    sdk::IZoomSDKRenderer* renderer = nullptr;
  } share_;
  std::uint32_t shareWant_ = 0;
  std::uint32_t shareGot_ = 0;
  MicSource mic_;
  CameraSource camera_;
  ScreenSource screen_;
  bool videoSet_ = false;
  std::string videoNote_;
  std::string shareRateNote_;
  sdk::ISettingService* settings_ = nullptr;
  bool micSet_ = false;
  bool unmuted_ = false;
  std::string qualityNote_;
  mutable std::mutex noteMutex_;
  std::string sendNote_;
};

}  // namespace

std::unique_ptr<MeetingClient> makeMeetingClient() { return std::make_unique<ZoomClient>(); }

}  // namespace backhaul

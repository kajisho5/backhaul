// MeetingClient on top of the Zoom Meeting SDK for Windows.
#include <windows.h>

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "auth_service_interface.h"
#include "meeting_service_components/meeting_audio_interface.h"
#include "meeting_service_components/meeting_participants_ctrl_interface.h"
#include "meeting_service_components/meeting_recording_interface.h"
#include "meeting_service_interface.h"
#include "rawdata/rawdata_audio_helper_interface.h"
#include "rawdata/rawdata_renderer_interface.h"
#include "rawdata/zoom_rawdata_api.h"
#include "zoom_sdk.h"
#include "zoom_sdk_raw_data_def.h"

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
    const std::string token = makeSdkToken(request.clientId, request.clientSecret, now - 60, 24 * 3600);
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
    if (meeting_ != nullptr && meeting_->GetMeetingStatus() == sdk::MEETING_STATUS_INMEETING) {
      meeting_->Leave(sdk::LEAVE_MEETING);
    }
    endOfMeeting();
    if (events_ != nullptr) state(MeetingState::Idle, {});
  }

  void setVideoWants(const std::vector<VideoWant>& wants) override {
    wants_ = wants;
    applyWants();
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
    if (everyone != lastParticipants_) {
      lastParticipants_ = everyone;
      events_->onParticipants(everyone);
      applyWants();  // someone who was wanted may have just arrived
    }
    // Permission can arrive without a notice (for example when made co-host).
    if (inMeeting_ && !rawStarted_) tryStartRaw();
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
      if (!rawStarted_) state(MeetingState::WaitingForPermission, {});
      return;
    }
    inMeeting_ = true;
    participants_ = meeting_->GetMeetingParticipantsController();
    recording_ = meeting_->GetMeetingRecordingController();
    if (recording_ != nullptr) recording_->SetEvent(this);
    sdk::IMeetingInfo* info = meeting_->GetMeetingInfo();
    if (info != nullptr && events_ != nullptr) events_->onMeetingTopic(toUtf8(info->GetMeetingTopic()));
    // Without the Zoom window nobody presses "join audio", so do it here.
    // A PC that only shows pictures stays out of the sound, so that it can never
    // play or send any.
    sdk::IMeetingAudioController* audio = meeting_->GetMeetingAudioController();
    if (request_.audio && audio != nullptr) audio->JoinVoip();
    state(MeetingState::WaitingForPermission, {});
    tryStartRaw();
    poll();
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
    inMeeting_ = false;
    busy_ = false;
    askedPermission_ = false;
    participants_ = nullptr;
    recording_ = nullptr;
    if (!lastParticipants_.empty()) {
      lastParticipants_.clear();
      if (events_ != nullptr) events_->onParticipants({});
    }
  }

  void shutDown() {
    stopRaw();
    events_ = nullptr;
    if (meeting_ != nullptr) {
      if (meeting_->GetMeetingStatus() == sdk::MEETING_STATUS_INMEETING) meeting_->Leave(sdk::LEAVE_MEETING);
      sdk::DestroyMeetingService(meeting_);
      meeting_ = nullptr;
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
};

}  // namespace

std::unique_ptr<MeetingClient> makeMeetingClient() { return std::make_unique<ZoomClient>(); }

}  // namespace backhaul

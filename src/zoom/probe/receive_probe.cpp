// Verification tool 1: join a meeting and measure what the Zoom Meeting SDK really
// delivers — per-person video size and frame rate, per-person audio format, and the
// network quality notices. Nothing is rendered or recorded; only numbers are logged.
//
// Secrets are read from environment variables so they never appear on the command line:
//   BACKHAUL_ZOOM_CLIENT_ID, BACKHAUL_ZOOM_CLIENT_SECRET   (required)
//   BACKHAUL_ZOOM_PASSCODE, BACKHAUL_ZOOM_ZAK               (optional)
//
// Usage: backhaul_probe_receive --meeting <number> [--name <display name>]
//                               [--resolution 1080|720|360] [--seconds <n>]

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <map>
#include <memory>
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
#include "setting_service_interface.h"
#include "zoom_sdk.h"
#include "zoom_sdk_raw_data_def.h"

#include "core/sdk_token.h"

namespace {

namespace sdk = ZOOM_SDK_NAMESPACE;

constexpr UINT_PTR kReportTimer = 1;
constexpr UINT kReportIntervalMs = 5000;
constexpr UINT kQuitMessage = WM_APP + 1;

DWORD g_mainThread = 0;

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

std::wstring envVar(const wchar_t* name) {
  const DWORD need = GetEnvironmentVariableW(name, nullptr, 0);
  if (need == 0) return {};
  std::wstring value(need, L'\0');
  const DWORD got = GetEnvironmentVariableW(name, value.data(), need);
  value.resize(got);
  return value;
}

// One line per event, to the console and to a UTF-8 log file next to the program.
class Log {
 public:
  Log() {
    SetConsoleOutputCP(CP_UTF8);
    if (_wfopen_s(&file_, L"backhaul-probe-receive.log", L"ab") != 0) file_ = nullptr;
  }
  ~Log() {
    if (file_ != nullptr) std::fclose(file_);
  }
  Log(const Log&) = delete;
  Log& operator=(const Log&) = delete;

  void line(const std::string& text) {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    char stamp[16];
    std::snprintf(stamp, sizeof(stamp), "%02u:%02u:%02u ", static_cast<unsigned>(t.wHour),
                  static_cast<unsigned>(t.wMinute), static_cast<unsigned>(t.wSecond));
    const std::lock_guard<std::mutex> lock(mutex_);
    std::printf("%s%s\n", stamp, text.c_str());
    std::fflush(stdout);
    if (file_ != nullptr) {
      std::fprintf(file_, "%s%s\n", stamp, text.c_str());
      std::fflush(file_);
    }
  }

 private:
  std::mutex mutex_;
  FILE* file_ = nullptr;
};

Log& logger() {
  static Log log;
  return log;
}

std::string fmt(const char* format, ...) {
  char buffer[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return buffer;
}

struct Options {
  std::uint64_t meeting = 0;
  std::wstring name = L"backhaul probe";
  sdk::ZoomSDKResolution resolution = sdk::ZoomSDKResolution_1080P;
  int resolutionLabel = 1080;
  int seconds = 0;  // 0: run until Ctrl+C
};

bool parseOptions(int argc, wchar_t** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    const std::wstring key = argv[i];
    if (i + 1 >= argc) return false;
    const std::wstring value = argv[++i];
    if (key == L"--meeting") {
      std::wstring digits;
      for (const wchar_t c : value) {
        if (c >= L'0' && c <= L'9') digits.push_back(c);
        else if (c != L' ' && c != L'-') return false;
      }
      if (digits.empty() || digits.size() > 12) return false;
      out.meeting = std::wcstoull(digits.c_str(), nullptr, 10);
    } else if (key == L"--name") {
      if (value.empty()) return false;
      out.name = value;
    } else if (key == L"--resolution") {
      if (value == L"1080") {
        out.resolution = sdk::ZoomSDKResolution_1080P;
        out.resolutionLabel = 1080;
      } else if (value == L"720") {
        out.resolution = sdk::ZoomSDKResolution_720P;
        out.resolutionLabel = 720;
      } else if (value == L"360") {
        out.resolution = sdk::ZoomSDKResolution_360P;
        out.resolutionLabel = 360;
      } else {
        return false;
      }
    } else if (key == L"--seconds") {
      wchar_t* end = nullptr;
      const long n = std::wcstol(value.c_str(), &end, 10);
      if (end == value.c_str() || *end != L'\0' || n <= 0 || n > 24 * 3600) return false;
      out.seconds = static_cast<int>(n);
    } else {
      return false;
    }
  }
  return out.meeting != 0;
}

// Counters filled on the SDK's data threads and read by the report timer.
struct VideoCount {
  unsigned frames = 0;
  unsigned width = 0;
  unsigned height = 0;
  unsigned rotation = 0;
  bool limitedRange = false;
};

struct AudioCount {
  unsigned callbacks = 0;
  unsigned long long bytes = 0;
  unsigned sampleRate = 0;
  unsigned channels = 0;
};

class VideoTap final : public sdk::IZoomSDKRendererDelegate {
 public:
  void onRendererBeDestroyed() override {}
  void onRawDataFrameReceived(YUVRawDataI420* data) override {
    if (data == nullptr) return;
    const std::lock_guard<std::mutex> lock(mutex_);
    ++count_.frames;
    count_.width = data->GetStreamWidth();
    count_.height = data->GetStreamHeight();
    count_.rotation = data->GetRotation();
    count_.limitedRange = data->IsLimitedI420();
  }
  void onRawDataStatusChanged(RawDataStatus status) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    on_ = (status == RawData_On);
  }
  void onShareCursorDataReceived(ZoomSDKShareCursorData) override {}

  VideoCount take(bool& on) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const VideoCount c = count_;
    count_.frames = 0;
    on = on_;
    return c;
  }

 private:
  std::mutex mutex_;
  VideoCount count_;
  bool on_ = false;
};

class AudioTap final : public sdk::IZoomSDKAudioRawDataDelegate {
 public:
  void onMixedAudioRawDataReceived(AudioRawData* data) override { add(mixed_, data); }
  void onOneWayAudioRawDataReceived(AudioRawData* data, uint32_t userId) override {
    if (data == nullptr) return;
    const std::lock_guard<std::mutex> lock(mutex_);
    addLocked(perUser_[userId], data);
  }
  void onShareAudioRawDataReceived(AudioRawData* data, uint32_t) override { add(share_, data); }
  void onOneWayInterpreterAudioRawDataReceived(AudioRawData*, const zchar_t*) override {}

  void take(AudioCount& mixed, AudioCount& share, std::map<uint32_t, AudioCount>& perUser) {
    const std::lock_guard<std::mutex> lock(mutex_);
    mixed = mixed_;
    share = share_;
    perUser = perUser_;
    mixed_ = AudioCount{};
    share_ = AudioCount{};
    perUser_.clear();
  }

 private:
  void add(AudioCount& target, AudioRawData* data) {
    if (data == nullptr) return;
    const std::lock_guard<std::mutex> lock(mutex_);
    addLocked(target, data);
  }
  static void addLocked(AudioCount& target, AudioRawData* data) {
    ++target.callbacks;
    target.bytes += data->GetBufferLen();
    target.sampleRate = data->GetSampleRate();
    target.channels = data->GetChannelNum();
  }

  std::mutex mutex_;
  AudioCount mixed_;
  AudioCount share_;
  std::map<uint32_t, AudioCount> perUser_;
};

struct Subscription {
  std::unique_ptr<VideoTap> tap;
  sdk::IZoomSDKRenderer* renderer = nullptr;
};

class Probe final : public sdk::IAuthServiceEvent,
                    public sdk::IMeetingServiceEvent,
                    public sdk::IMeetingParticipantsCtrlEvent,
                    public sdk::IMeetingRecordingCtrlEvent {
 public:
  explicit Probe(Options options) : options_(std::move(options)) {}

  bool start(const std::wstring& token) {
    sdk::InitParam init;
    init.strWebDomain = L"https://zoom.us";
    init.enableLogByDefault = true;
    const sdk::SDKError initResult = sdk::InitSDK(init);
    if (initResult != sdk::SDKERR_SUCCESS) {
      logger().line(fmt("InitSDK failed: %d", static_cast<int>(initResult)));
      return false;
    }
    sdkReady_ = true;
    logger().line("Zoom SDK version: " + toUtf8(sdk::GetSDKVersion()));

    sdk::SDKError result = sdk::CreateAuthService(&auth_);
    if (result != sdk::SDKERR_SUCCESS || auth_ == nullptr) {
      logger().line(fmt("CreateAuthService failed: %d", static_cast<int>(result)));
      return false;
    }
    auth_->SetEvent(this);
    sdk::AuthContext context;
    context.jwt_token = token.c_str();
    result = auth_->SDKAuth(context);
    if (result != sdk::SDKERR_SUCCESS) {
      logger().line(fmt("SDKAuth call failed: %d", static_cast<int>(result)));
      return false;
    }
    logger().line("authenticating with Zoom...");
    return true;
  }

  void report() {
    if (!rawStarted_) return;
    const double seconds = kReportIntervalMs / 1000.0;
    for (auto& [userId, sub] : video_) {
      bool on = false;
      const VideoCount c = sub.tap->take(on);
      logger().line(fmt("video  user %u  %s  %ux%u  %.1f fps  rotation %u  %s range  asked %dp  [%s]",
                        userId, on ? "on " : "off", c.width, c.height, c.frames / seconds,
                        c.rotation, c.limitedRange ? "limited" : "full", options_.resolutionLabel,
                        userName(userId).c_str()));
    }
    AudioCount mixed;
    AudioCount share;
    std::map<uint32_t, AudioCount> perUser;
    audioTap_.take(mixed, share, perUser);
    logAudio("audio  mixed      ", mixed, seconds);
    if (share.callbacks != 0) logAudio("audio  share      ", share, seconds);
    for (const auto& [userId, c] : perUser) {
      logAudio(fmt("audio  user %u", userId).c_str(), c, seconds);
    }
  }

  void leaveAndCleanUp() {
    stopRaw();
    if (meeting_ != nullptr) {
      if (meeting_->GetMeetingStatus() == sdk::MEETING_STATUS_INMEETING) {
        meeting_->Leave(sdk::LEAVE_MEETING);
      }
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

  bool failed() const { return failed_; }

  // IAuthServiceEvent
  void onAuthenticationReturn(sdk::AuthResult result) override {
    if (result != sdk::AUTHRET_SUCCESS) {
      logger().line(fmt("authentication failed: AuthResult %d (11 = token rejected, 2 = wrong id/secret)",
                        static_cast<int>(result)));
      fail();
      return;
    }
    logger().line("authenticated");
    join();
  }
  void onLoginReturnWithReason(sdk::LOGINSTATUS, sdk::IAccountInfo*, sdk::LoginFailReason) override {}
  void onLogout() override {}
  void onZoomIdentityExpired() override {}
  void onZoomAuthIdentityExpired() override { logger().line("the SDK token expired"); }
  void onNotificationServiceStatus(sdk::SDKNotificationServiceStatus,
                                   sdk::SDKNotificationServiceError) override {}

  // IMeetingServiceEvent
  void onMeetingStatusChanged(sdk::MeetingStatus status, int result) override {
    logger().line(fmt("meeting status %d (result %d)", static_cast<int>(status), result));
    if (status == sdk::MEETING_STATUS_INMEETING) {
      onInMeeting();
    } else if (status == sdk::MEETING_STATUS_FAILED) {
      logger().line("could not join the meeting");
      fail();
    } else if (status == sdk::MEETING_STATUS_ENDED) {
      stopRaw();
      PostThreadMessageW(g_mainThread, kQuitMessage, 0, 0);
    }
  }
  void onMeetingStatisticsWarningNotification(sdk::StatisticsWarningType type) override {
    logger().line(fmt("statistics warning %d", static_cast<int>(type)));
  }
  void onMeetingParameterNotification(const sdk::MeetingParameter*) override {}
  void onSuspendParticipantsActivities() override {}
  void onAICompanionActiveChangeNotice(bool) override {}
  void onMeetingTopicChanged(const zchar_t*) override {}
  void onMeetingFullToWatchLiveStream(const zchar_t*) override {}
  void onUserNetworkStatusChanged(sdk::MeetingComponentType type, sdk::ConnectionQuality level,
                                  unsigned int userId, bool uplink) override {
    logger().line(fmt("network  user %u  component %d  quality %d  %s", userId,
                      static_cast<int>(type), static_cast<int>(level), uplink ? "uplink" : "downlink"));
  }
  void onAppSignalPanelUpdated(sdk::IMeetingAppSignalHandler*) override {}

  // IMeetingParticipantsCtrlEvent
  void onUserJoin(sdk::IList<unsigned int>* users, const zchar_t*) override {
    if (users == nullptr) return;
    for (int i = 0; i < users->GetCount(); ++i) {
      const unsigned int id = users->GetItem(i);
      logger().line(fmt("joined: user %u [%s]", id, userName(id).c_str()));
      if (rawStarted_) subscribeVideo(id);
    }
  }
  void onUserLeft(sdk::IList<unsigned int>* users, const zchar_t*) override {
    if (users == nullptr) return;
    for (int i = 0; i < users->GetCount(); ++i) {
      const unsigned int id = users->GetItem(i);
      logger().line(fmt("left: user %u", id));
      unsubscribeVideo(id);
    }
  }
  void onHostChangeNotification(unsigned int) override { tryStartRaw("host changed"); }
  void onLowOrRaiseHandStatusChanged(bool, unsigned int) override {}
  void onUserNamesChanged(sdk::IList<unsigned int>*) override {}
  void onCoHostChangeNotification(unsigned int userId, bool isCoHost) override {
    logger().line(fmt("co-host change: user %u -> %s", userId, isCoHost ? "co-host" : "not co-host"));
    tryStartRaw("co-host changed");
  }
  void onInvalidReclaimHostkey() override {}
  void onAllHandsLowered() override {}
  void onLocalRecordingStatusChanged(unsigned int, sdk::RecordingStatus) override {}
  void onAllowParticipantsRenameNotification(bool) override {}
  void onAllowParticipantsUnmuteSelfNotification(bool) override {}
  void onAllowParticipantsStartVideoNotification(bool) override {}
  void onAllowParticipantsShareWhiteBoardNotification(bool) override {}
  void onRequestLocalRecordingPrivilegeChanged(sdk::LocalRecordingRequestPrivilegeStatus) override {}
  void onAllowParticipantsRequestCloudRecording(bool) override {}
  void onInMeetingUserAvatarPathUpdated(unsigned int) override {}
  void onParticipantProfilePictureStatusChange(bool) override {}
  void onFocusModeStateChanged(bool) override {}
  void onFocusModeShareTypeChanged(sdk::FocusModeShareType) override {}
  void onBotAuthorizerRelationChanged(unsigned int) override {}
  void onVirtualNameTagStatusChanged(bool, unsigned int) override {}
  void onVirtualNameTagRosterInfoUpdated(unsigned int) override {}
  void onCreateCompanionRelation(unsigned int, unsigned int) override {}
  void onRemoveCompanionRelation(unsigned int) override {}
  void onGrantCoOwnerPrivilegeChanged(bool) override {}

  // IMeetingRecordingCtrlEvent
  void onRecordingStatus(sdk::RecordingStatus status) override {
    logger().line(fmt("recording status %d", static_cast<int>(status)));
  }
  void onCloudRecordingStatus(sdk::RecordingStatus) override {}
  void onRecordPrivilegeChanged(bool canRecord) override {
    logger().line(fmt("recording permission: %s", canRecord ? "granted" : "not granted"));
    if (canRecord) tryStartRaw("permission granted");
  }
  void onLocalRecordingPrivilegeRequestStatus(sdk::RequestLocalRecordingStatus status) override {
    logger().line(fmt("answer to the permission request: %d (0 = granted, 1 = denied, 2 = timed out)",
                      static_cast<int>(status)));
    if (status == sdk::RequestLocalRecording_Granted) tryStartRaw("request granted");
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
  void fail() {
    failed_ = true;
    PostThreadMessageW(g_mainThread, kQuitMessage, 0, 0);
  }

  void join() {
    sdk::SDKError result = sdk::CreateSettingService(&settings_);
    if (result == sdk::SDKERR_SUCCESS && settings_ != nullptr) {
      sdk::IAudioSettingContext* audio = settings_->GetAudioSettings();
      if (audio != nullptr) {
        result = audio->EnableAutoJoinAudio(true);
        if (result != sdk::SDKERR_SUCCESS) {
          logger().line(fmt("could not turn on automatic audio join: %d", static_cast<int>(result)));
        }
      }
    } else {
      logger().line(fmt("CreateSettingService failed: %d", static_cast<int>(result)));
    }

    result = sdk::CreateMeetingService(&meeting_);
    if (result != sdk::SDKERR_SUCCESS || meeting_ == nullptr) {
      logger().line(fmt("CreateMeetingService failed: %d", static_cast<int>(result)));
      fail();
      return;
    }
    meeting_->SetEvent(this);

    passcode_ = envVar(L"BACKHAUL_ZOOM_PASSCODE");
    zak_ = envVar(L"BACKHAUL_ZOOM_ZAK");

    sdk::JoinParam param;
    param.userType = sdk::SDK_UT_WITHOUT_LOGIN;
    sdk::JoinParam4WithoutLogin& p = param.param.withoutloginuserJoin;
    p.meetingNumber = options_.meeting;
    p.userName = options_.name.c_str();
    p.psw = passcode_.empty() ? nullptr : passcode_.c_str();
    p.userZAK = zak_.empty() ? nullptr : zak_.c_str();
    p.isVideoOff = true;
    p.isAudioOff = false;
    p.eAudioRawdataSamplingRate = sdk::AudioRawdataSamplingRate_48K;
    p.eVideoRawdataColorspace = sdk::VideoRawdataColorspace_BT709_L;
    result = meeting_->Join(param);
    if (result != sdk::SDKERR_SUCCESS) {
      logger().line(fmt("Join call failed: %d", static_cast<int>(result)));
      fail();
      return;
    }
    logger().line("joining the meeting (asked for 48 kHz audio, BT.709 limited-range video)...");
  }

  void onInMeeting() {
    participants_ = meeting_->GetMeetingParticipantsController();
    recording_ = meeting_->GetMeetingRecordingController();
    if (participants_ == nullptr || recording_ == nullptr) {
      logger().line("the SDK did not give the participant or recording controller");
      fail();
      return;
    }
    participants_->SetEvent(this);
    recording_->SetEvent(this);
    logger().line(fmt("in the meeting. raw data licence: %s", sdk::HasRawdataLicense() ? "yes" : "no"));
    tryStartRaw("joined");
  }

  void tryStartRaw(const char* reason) {
    if (rawStarted_ || recording_ == nullptr) return;
    const sdk::SDKError can = recording_->CanStartRawRecording();
    if (can != sdk::SDKERR_SUCCESS) {
      logger().line(fmt("cannot take raw data yet (%s): code %d", reason, static_cast<int>(can)));
      if (!askedPermission_) {
        askedPermission_ = true;
        if (recording_->IsSupportRequestLocalRecordingPrivilege() == sdk::SDKERR_SUCCESS) {
          const sdk::SDKError asked = recording_->RequestLocalRecordingPrivilege();
          logger().line(fmt("asked the host for recording permission: code %d", static_cast<int>(asked)));
        }
        logger().line("-> make this participant a co-host, or allow its recording request");
      }
      return;
    }
    const sdk::SDKError started = recording_->StartRawRecording();
    if (started != sdk::SDKERR_SUCCESS) {
      logger().line(fmt("StartRawRecording failed: %d", static_cast<int>(started)));
      return;
    }
    rawStarted_ = true;
    logger().line(fmt("raw data started (%s)", reason));

    sdk::IZoomSDKAudioRawDataHelper* audio = sdk::GetAudioRawdataHelper();
    if (audio == nullptr) {
      logger().line("no audio raw data helper");
    } else {
      const sdk::SDKError subscribed = audio->subscribe(&audioTap_);
      logger().line(fmt("audio subscribe: code %d", static_cast<int>(subscribed)));
      audioSubscribed_ = (subscribed == sdk::SDKERR_SUCCESS);
    }

    sdk::IList<unsigned int>* users = participants_->GetParticipantsList();
    if (users != nullptr) {
      for (int i = 0; i < users->GetCount(); ++i) subscribeVideo(users->GetItem(i));
    }
    SetTimer(nullptr, kReportTimer, kReportIntervalMs, nullptr);
  }

  void subscribeVideo(unsigned int userId) {
    if (video_.count(userId) != 0) return;
    sdk::IUserInfo* info = participants_->GetUserByUserID(userId);
    if (info == nullptr || info->IsMySelf()) return;

    Subscription sub;
    sub.tap = std::make_unique<VideoTap>();
    sdk::SDKError result = sdk::createRenderer(&sub.renderer, sub.tap.get());
    if (result != sdk::SDKERR_SUCCESS || sub.renderer == nullptr) {
      logger().line(fmt("createRenderer for user %u failed: %d", userId, static_cast<int>(result)));
      return;
    }
    const sdk::SDKError sized = sub.renderer->setRawDataResolution(options_.resolution);
    result = sub.renderer->subscribe(userId, sdk::RAW_DATA_TYPE_VIDEO);
    logger().line(fmt("video subscribe user %u at %dp: resolution code %d, subscribe code %d  [%s]",
                      userId, options_.resolutionLabel, static_cast<int>(sized),
                      static_cast<int>(result), userName(userId).c_str()));
    if (result != sdk::SDKERR_SUCCESS) {
      sdk::destroyRenderer(sub.renderer);
      return;
    }
    video_.emplace(userId, std::move(sub));
  }

  void unsubscribeVideo(unsigned int userId) {
    const auto it = video_.find(userId);
    if (it == video_.end()) return;
    it->second.renderer->unSubscribe();
    sdk::destroyRenderer(it->second.renderer);
    video_.erase(it);
  }

  void stopRaw() {
    if (!rawStarted_) return;
    KillTimer(nullptr, kReportTimer);
    std::vector<unsigned int> ids;
    for (const auto& entry : video_) ids.push_back(entry.first);
    for (const unsigned int id : ids) unsubscribeVideo(id);
    if (audioSubscribed_) {
      sdk::IZoomSDKAudioRawDataHelper* audio = sdk::GetAudioRawdataHelper();
      if (audio != nullptr) audio->unSubscribe();
      audioSubscribed_ = false;
    }
    if (recording_ != nullptr) recording_->StopRawRecording();
    rawStarted_ = false;
  }

  std::string userName(unsigned int userId) {
    if (participants_ == nullptr) return {};
    sdk::IUserInfo* info = participants_->GetUserByUserID(userId);
    if (info == nullptr) return {};
    return toUtf8(info->GetUserName());
  }

  static void logAudio(const char* label, const AudioCount& c, double seconds) {
    const double bytesPerSecond = static_cast<double>(c.bytes) / seconds;
    logger().line(fmt("%s  %u Hz  %u ch  %.0f blocks/s  %.0f bytes/s", label, c.sampleRate, c.channels,
                      c.callbacks / seconds, bytesPerSecond));
  }

  Options options_;
  std::wstring passcode_;
  std::wstring zak_;
  sdk::IAuthService* auth_ = nullptr;
  sdk::IMeetingService* meeting_ = nullptr;
  sdk::ISettingService* settings_ = nullptr;
  sdk::IMeetingParticipantsController* participants_ = nullptr;
  sdk::IMeetingRecordingController* recording_ = nullptr;
  AudioTap audioTap_;
  std::map<unsigned int, Subscription> video_;
  bool sdkReady_ = false;
  bool rawStarted_ = false;
  bool audioSubscribed_ = false;
  bool askedPermission_ = false;
  bool failed_ = false;
};

BOOL WINAPI onConsoleSignal(DWORD) {
  PostThreadMessageW(g_mainThread, kQuitMessage, 0, 0);
  return TRUE;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  g_mainThread = GetCurrentThreadId();

  Options options;
  if (!parseOptions(argc, argv, options)) {
    logger().line("usage: backhaul_probe_receive --meeting <number> [--name <display name>] "
                  "[--resolution 1080|720|360] [--seconds <n>]");
    return 2;
  }
  const std::string clientId = toUtf8(envVar(L"BACKHAUL_ZOOM_CLIENT_ID").c_str());
  const std::string clientSecret = toUtf8(envVar(L"BACKHAUL_ZOOM_CLIENT_SECRET").c_str());
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  // Issued 60 seconds in the past so a slightly fast PC clock is not rejected.
  const std::string token = backhaul::makeSdkToken(clientId, clientSecret, now - 60, 4 * 3600);
  if (token.empty()) {
    logger().line("set BACKHAUL_ZOOM_CLIENT_ID and BACKHAUL_ZOOM_CLIENT_SECRET (Zoom app credentials) first");
    return 2;
  }

  // Make sure this thread has a message queue before anything posts to it.
  MSG msg{};
  PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
  SetConsoleCtrlHandler(onConsoleSignal, TRUE);

  Probe probe(options);
  if (!probe.start(toWide(token))) {
    probe.leaveAndCleanUp();
    return 1;
  }

  const ULONGLONG startedAt = GetTickCount64();
  const UINT_PTR deadlineTimer = SetTimer(nullptr, 0, 1000, nullptr);
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (msg.hwnd == nullptr && msg.message == kQuitMessage) break;
    if (msg.hwnd == nullptr && msg.message == WM_TIMER) {
      if (msg.wParam == deadlineTimer) {
        if (options.seconds > 0 &&
            GetTickCount64() - startedAt >= static_cast<ULONGLONG>(options.seconds) * 1000u) {
          logger().line("time is up");
          break;
        }
      } else {
        probe.report();
      }
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  KillTimer(nullptr, deadlineTimer);

  probe.leaveAndCleanUp();
  logger().line("finished");
  return probe.failed() ? 1 : 0;
}

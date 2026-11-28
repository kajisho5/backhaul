#include "credential_store.h"

#ifdef _WIN32
#include <windows.h>

#include <wincred.h>

#include <string>

namespace {

class WindowsCredentialStore final : public CredentialStore {
 public:
  explicit WindowsCredentialStore(const QString& name) : target_(name.toStdWString()) {}

  bool load(ZoomCredentials& out) override {
    PCREDENTIALW found = nullptr;
    if (!CredReadW(target_.c_str(), CRED_TYPE_GENERIC, 0, &found) || found == nullptr) return false;
    const QString id = found->UserName != nullptr ? QString::fromWCharArray(found->UserName) : QString();
    const QString secret =
        QString::fromUtf16(reinterpret_cast<const char16_t*>(found->CredentialBlob),
                           static_cast<qsizetype>(found->CredentialBlobSize / sizeof(char16_t)));
    CredFree(found);
    if (id.isEmpty() || secret.isEmpty()) return false;
    out.clientId = id;
    out.clientSecret = secret;
    return true;
  }

  bool save(const ZoomCredentials& credentials) override {
    if (credentials.clientId.isEmpty() || credentials.clientSecret.isEmpty()) return false;
    std::wstring id = credentials.clientId.toStdWString();
    std::u16string secret = credentials.clientSecret.toStdU16String();
    CREDENTIALW entry{};
    entry.Type = CRED_TYPE_GENERIC;
    entry.TargetName = target_.data();
    entry.UserName = id.data();
    entry.CredentialBlob = reinterpret_cast<LPBYTE>(secret.data());
    entry.CredentialBlobSize = static_cast<DWORD>(secret.size() * sizeof(char16_t));
    entry.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&entry, 0) != FALSE;
  }

  bool clear() override {
    if (CredDeleteW(target_.c_str(), CRED_TYPE_GENERIC, 0)) return true;
    return GetLastError() == ERROR_NOT_FOUND;
  }

 private:
  std::wstring target_;
};

class WindowsSecretStore final : public SecretStore {
 public:
  explicit WindowsSecretStore(const QString& name) : target_(name.toStdWString()) {}

  QByteArray load() override {
    PCREDENTIALW found = nullptr;
    if (!CredReadW(target_.c_str(), CRED_TYPE_GENERIC, 0, &found) || found == nullptr) return {};
    const QByteArray secret(reinterpret_cast<const char*>(found->CredentialBlob), static_cast<qsizetype>(found->CredentialBlobSize));
    CredFree(found);
    return secret;
  }

  bool save(const QByteArray& secret) override {
    if (secret.isEmpty() || secret.size() > kMaxBytes) return false;
    QByteArray copy = secret;
    std::wstring user = L"backhaul";
    CREDENTIALW entry{};
    entry.Type = CRED_TYPE_GENERIC;
    entry.TargetName = target_.data();
    entry.UserName = user.data();
    entry.CredentialBlob = reinterpret_cast<LPBYTE>(copy.data());
    entry.CredentialBlobSize = static_cast<DWORD>(copy.size());
    entry.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&entry, 0) != FALSE;
  }

  bool clear() override {
    if (CredDeleteW(target_.c_str(), CRED_TYPE_GENERIC, 0)) return true;
    return GetLastError() == ERROR_NOT_FOUND;
  }

 private:
  std::wstring target_;
};

}  // namespace

std::unique_ptr<CredentialStore> makeCredentialStore(const QString& name) {
  return std::make_unique<WindowsCredentialStore>(name);
}

std::unique_ptr<SecretStore> makeSecretStore(const QString& name) { return std::make_unique<WindowsSecretStore>(name); }

#else

std::unique_ptr<CredentialStore> makeCredentialStore(const QString&) { return nullptr; }
std::unique_ptr<SecretStore> makeSecretStore(const QString&) { return nullptr; }

#endif

// Where the Zoom app credentials are kept: the operating system's own protected
// store, never the settings file.
#pragma once

#include <QByteArray>
#include <QString>
#include <memory>

struct ZoomCredentials {
  QString clientId;
  QString clientSecret;
};

class CredentialStore {
 public:
  virtual ~CredentialStore() = default;
  // False when nothing is saved or it cannot be read.
  virtual bool load(ZoomCredentials& out) = 0;
  virtual bool save(const ZoomCredentials& credentials) = 0;
  virtual bool clear() = 0;
};

// Windows: the Credential Manager. Null on systems without a store this program supports.
// `name` is the entry's name in the store; tests pass their own.
std::unique_ptr<CredentialStore> makeCredentialStore(const QString& name = QStringLiteral("backhaul/zoom-app"));

// One secret text under a name in the same protected store (a licence token, a sign-in).
// At most 2500 bytes: what the Windows store takes.
class SecretStore {
 public:
  static constexpr int kMaxBytes = 2500;
  virtual ~SecretStore() = default;
  // Empty when nothing is saved or it cannot be read.
  virtual QByteArray load() = 0;
  virtual bool save(const QByteArray& secret) = 0;
  virtual bool clear() = 0;
};

// Null on systems without a store this program supports.
std::unique_ptr<SecretStore> makeSecretStore(const QString& name);

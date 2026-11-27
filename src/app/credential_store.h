// Where the Zoom app credentials are kept: the operating system's own protected
// store, never the settings file.
#pragma once

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

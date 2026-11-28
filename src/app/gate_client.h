// Talking to the licence server ("gate"). Only the transport: what is asked and what the
// answers mean is in LicenceController.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

struct GateReply {
  int status = 0;       // HTTP status; 0 when the server was not reached
  QJsonObject body;     // empty when the answer was not a JSON object
  QString transport;    // why the server was not reached (empty when it was)
};

class GateClient {
 public:
  using Done = std::function<void(const GateReply&)>;
  virtual ~GateClient() = default;
  // POSTs `body` as JSON to `path` ("/v1/..."), with `bearer` as the PC's token when not
  // empty. `done` is called once, later, on the caller's thread - never from inside post().
  virtual void post(const QString& path, const QJsonObject& body, const QString& bearer, Done done) = 0;
};

// The real thing, over HTTPS. `baseUrl` without a slash at the end.
std::unique_ptr<GateClient> makeGateClient(const QString& baseUrl, QObject* parent = nullptr);

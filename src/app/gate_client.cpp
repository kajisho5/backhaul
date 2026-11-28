#include "gate_client.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

constexpr int kTimeoutMs = 15000;
constexpr qint64 kMaxAnswerBytes = 256 * 1024;

class HttpGateClient final : public GateClient {
 public:
  HttpGateClient(QString baseUrl, QObject* parent) : base_(std::move(baseUrl)), network_(parent) {}

  void post(const QString& path, const QJsonObject& body, const QString& bearer, Done done) override {
    QNetworkRequest request{QUrl(base_ + path)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("backhaul"));
    if (!bearer.isEmpty()) request.setRawHeader("Authorization", "Bearer " + bearer.toUtf8());
    request.setTransferTimeout(kTimeoutMs);
    // An answer from somewhere else must never be taken for the server's.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done = std::move(done)] {
      GateReply result;
      result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (result.status == 0) {
        result.transport = reply->errorString();
      } else {
        const QJsonDocument document = QJsonDocument::fromJson(reply->read(kMaxAnswerBytes));
        if (document.isObject()) result.body = document.object();
      }
      reply->deleteLater();
      done(result);
    });
  }

 private:
  QString base_;
  QNetworkAccessManager network_;
};

}  // namespace

std::unique_ptr<GateClient> makeGateClient(const QString& baseUrl, QObject* parent) {
  return std::make_unique<HttpGateClient>(baseUrl, parent);
}

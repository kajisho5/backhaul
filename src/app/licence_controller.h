// The licence of this PC and the licence's Zoom sign-in. With both, joining a meeting
// needs no Zoom keys on the PC: the licence server signs the Meeting SDK token, and
// turns the sign-in (made once, on any PC of the licence) into the ZAK that meetings of
// other accounts ask for. Nothing of the sign-in is kept on the PC.
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <functional>
#include <memory>

#include "credential_store.h"
#include "gate_client.h"

class LicenceController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available CONSTANT)   // false: no server connection or no protected store here
  Q_PROPERTY(bool active READ active NOTIFY changed)   // this PC is registered with a licence
  Q_PROPERTY(bool busy READ busy NOTIFY changed)       // an answer of the server is awaited
  Q_PROPERTY(QString keyTail READ keyTail NOTIFY changed)
  Q_PROPERTY(int seats READ seats NOTIFY changed)
  Q_PROPERTY(int used READ used NOTIFY changed)
  Q_PROPERTY(QString expiresText READ expiresText NOTIFY changed)  // "2027-11-30 まで" or "期限なし"
  Q_PROPERTY(QString deviceName READ deviceName NOTIFY changed)
  Q_PROPERTY(QString message READ message NOTIFY changed)
  Q_PROPERTY(bool messageBad READ messageBad NOTIFY changed)
  // Zoom sign-in.
  Q_PROPERTY(bool zoomSignedIn READ zoomSignedIn NOTIFY changed)
  Q_PROPERTY(bool zoomWaiting READ zoomWaiting NOTIFY changed)  // the browser is open, the answer is awaited
  Q_PROPERTY(QString zoomMessage READ zoomMessage NOTIFY changed)
  Q_PROPERTY(bool zoomMessageBad READ zoomMessageBad NOTIFY changed)

 public:
  using Opener = std::function<bool(const QUrl&)>;  // opens the browser
  using Clock = std::function<qint64()>;            // unix seconds

  // Either of the first two may be null: the feature is then switched off (available() is false).
  LicenceController(std::unique_ptr<GateClient> gate, std::unique_ptr<SecretStore> licenceStore, QString deviceName,
                    Opener openUrl, Clock clock = {}, QObject* parent = nullptr);

  bool available() const { return gate_ != nullptr && licenceStore_ != nullptr; }
  bool active() const { return !deviceToken_.isEmpty(); }
  bool busy() const { return pending_ > 0; }
  QString keyTail() const { return keyTail_; }
  int seats() const { return seats_; }
  int used() const { return used_; }
  QString expiresText() const;
  QString deviceName() const { return shownName_; }
  QString message() const { return message_; }
  bool messageBad() const { return messageBad_; }
  // One sign-in serves every PC of the licence; the server keeps it and says whether there is one.
  bool zoomSignedIn() const { return zoomSignedIn_; }
  bool zoomWaiting() const { return !loginState_.isEmpty(); }
  QString zoomMessage() const { return zoomMessage_; }
  bool zoomMessageBad() const { return zoomMessageBad_; }

  Q_INVOKABLE void activate(const QString& key);
  Q_INVOKABLE void deactivate();
  // Asks the server where the licence stands (seats in use, end, stopped).
  Q_INVOKABLE void refresh();
  // The same question with a stopwatch, for the check before the show: whether the server
  // answered at all (any answer counts), and after how many milliseconds. Also asked when
  // this PC has no licence. `done` is called once, later.
  void probe(std::function<void(bool reachable, int ms)> done);
  // The licence stopped working (ended or stopped by the owner), as far as this PC knows.
  bool stopped() const;
  // Whole days until the licence ends; -1 when it has no end or renews by itself.
  qint64 daysLeft() const;
  Q_INVOKABLE void signIn();
  Q_INVOKABLE void cancelSignIn();
  Q_INVOKABLE void signOut();
  // The sign-in's clock, also callable from a test: asks whether the browser part is done.
  void pollSignIn();

  // What a join needs. `ok` false: do not join, `error` says why. `notice` is something the
  // operator should know although the join goes ahead (for example: no ZAK this time).
  struct Entry {
    bool ok = false;
    bool unreachable = false;  // not ok because the licence server could not be reached
    QString sdkToken, zak, error, notice;
  };
  void prepareJoin(std::function<void(const Entry&)> done);

 signals:
  void changed();

 private:
  void post(const QString& path, const QJsonObject& body, GateClient::Done done);
  void takeStatus(const GateReply& reply);
  QString explain(const GateReply& reply);
  void takeLicence(const QJsonObject& licence);
  void say(const QString& message, bool bad);
  void sayZoom(const QString& message, bool bad);
  void saveLicence();
  void forgetLicence();
  void addZak(Entry entry, std::function<void(const Entry&)> done);
  qint64 now() const;

  std::unique_ptr<GateClient> gate_;
  std::unique_ptr<SecretStore> licenceStore_;
  QString machineName_;
  Opener openUrl_;
  Clock clock_;

  QString deviceId_, deviceToken_, keyTail_, shownName_;
  bool stoppedByOwner_ = false;  // as the server last said
  int seats_ = 0, used_ = 0;
  qint64 expiresAt_ = 0;  // 0 = no end
  bool renews_ = false;   // paid by a subscription that renews by itself (as the server last said)
  QString message_, zoomMessage_;
  bool messageBad_ = false, zoomMessageBad_ = false;
  int pending_ = 0;

  QString sdkToken_;
  qint64 sdkTokenExpiresAt_ = 0;

  bool zoomSignedIn_ = false;
  QString loginState_, loginVerifier_;
  qint64 loginDeadline_ = 0;
  QTimer loginTimer_;
};

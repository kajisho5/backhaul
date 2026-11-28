#pragma once

#include <QHash>
#include <QString>
#include <QTranslator>

#include <cstddef>

struct DictionaryEntry {
  const char* japanese;
  const char* english;
};
struct DictionaryOverride {
  const char* screen;
  const char* japanese;
  const char* english;
};
extern const DictionaryOverride kEnglishByScreen[];
extern const std::size_t kEnglishByScreenCount;
extern const DictionaryEntry kEnglish[];
extern const std::size_t kEnglishCount;

// Turns the app's Japanese texts into English by looking the whole text up in one table.
// Installed while the language is English; taken out for Japanese.
class DictionaryTranslator final : public QTranslator {
 public:
  explicit DictionaryTranslator(QObject* parent = nullptr);
  QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override;
  bool isEmpty() const override { return table_.isEmpty(); }
  // For tests: how many texts are known.
  qsizetype size() const { return table_.size(); }

 private:
  QHash<QByteArray, QString> table_;
};

// Text made in C++: Japanese in the code, English when the dictionary is installed.
QString T(const char* japanese);
// The same for a text that arrives from a layer without Qt (the Zoom, sound and picture
// parts say things in Japanese). A text the table does not know is shown as it is.
QString T(const std::string& japanese);

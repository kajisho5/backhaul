#include "dictionary_translator.h"

#include <QCoreApplication>

#include <string>

DictionaryTranslator::DictionaryTranslator(QObject* parent) : QTranslator(parent) {
  table_.reserve(static_cast<qsizetype>(kEnglishCount));
  for (std::size_t i = 0; i < kEnglishCount; ++i) {
    table_.insert(QByteArray(kEnglish[i].japanese), QString::fromUtf8(kEnglish[i].english));
  }
  for (std::size_t i = 0; i < kEnglishByScreenCount; ++i) {
    const DictionaryOverride& o = kEnglishByScreen[i];
    table_.insert(QByteArray(o.screen) + '\n' + o.japanese, QString::fromUtf8(o.english));
  }
}

QString DictionaryTranslator::translate(const char* context, const char* sourceText, const char*, int) const {
  // An empty answer tells Qt "not known": the Japanese text is then shown as it is.
  if (sourceText == nullptr) return QString();
  if (context != nullptr) {
    const auto special = table_.constFind(QByteArray(context) + '\n' + sourceText);
    if (special != table_.constEnd()) return special.value();
  }
  return table_.value(QByteArray(sourceText));
}

QString T(const char* japanese) { return QCoreApplication::translate("backhaul", japanese); }

QString T(const std::string& japanese) { return QCoreApplication::translate("backhaul", japanese.c_str()); }

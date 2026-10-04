#pragma once

#include <QDir>
#include <QUrl>

#include <optional>

// Clipboard text is one strictly escaped path, never shell input.
inline std::optional<QUrl> parsePastedPath(const QString& text) {
  if (!text.startsWith('/') && !text.startsWith(QStringLiteral("~/"))) {
    return std::nullopt;
  }
  QString path;
  for (qsizetype index = 0; index < text.size(); ++index) {
    auto character = text.at(index);
    if (character == QLatin1Char('\\')) {
      if (++index == text.size()) {
        return std::nullopt;
      }
      character = text.at(index);
      if (character != QLatin1Char(' ') && character != QLatin1Char('\\')) {
        return std::nullopt;
      }
    } else if (character.isSpace() || character.isNull() || character == QLatin1Char('\'') ||
               character == QLatin1Char('"')) {
      return std::nullopt;
    }
    path += character;
  }
  if (path.startsWith(QStringLiteral("~/"))) {
    path.replace(0, 1, QDir::homePath());
  }
  return QUrl::fromLocalFile(path);
}

#include "local_url.h"

#include <QDir>
#include <QFileInfo>

QUrl normalizedLocalUrl(const QUrl& url) {
  return QUrl::fromLocalFile(QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath()));
}

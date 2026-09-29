// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>

#pragma once

#include "thumbnail_provider.h"
#include "thumbnail_size.h"

#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// The only place QML learns the thumbnail box size and builds thumbnail sources.
class ThumbnailMetrics : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int boxSize READ boxSize CONSTANT)

 public:
  explicit ThumbnailMetrics(QObject* parent = nullptr) : QObject(parent) {}

  // Logical box side in device-independent pixels.
  [[nodiscard]] static constexpr int boxSize() { return kThumbnailBoxLogical; }

  Q_INVOKABLE static constexpr int devicePixels(qreal dpr) { return thumbnailBoxPixels(dpr); }

  // Empty for a URL that is not a local file. The generation changes on a rescan so thumbnails decode again.
  Q_INVOKABLE static QString sourceFor(const QUrl& fileUrl, qreal dpr, int generation) {
    if (!fileUrl.isLocalFile()) {
      return {};
    }
    return QStringLiteral("image://thumbnail/") +
           ThumbnailProvider::idFor(fileUrl.toLocalFile(), thumbnailBoxPixels(dpr), generation);
  }
};

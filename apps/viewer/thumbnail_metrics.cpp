// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "thumbnail_metrics.h"

#include "thumbnail_provider.h"

QString ThumbnailMetrics::sourceFor(const QUrl& fileUrl, qreal dpr, int generation) {
  if (!fileUrl.isLocalFile()) {
    return {};
  }
  return QStringLiteral("image://thumbnail/") +
         ThumbnailProvider::idFor(fileUrl.toLocalFile(), thumbnailBoxPixels(dpr), generation);
}
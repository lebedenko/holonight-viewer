#pragma once
#include <QStringList>

// Viewer policy, independent of host Qt image plugins. Explicit opens still validate content.
inline QStringList viewerImageSuffixes() {
  return {QStringLiteral("png"), QStringLiteral("jpeg"), QStringLiteral("jpg"),
          QStringLiteral("jpe"), QStringLiteral("bmp"),  QStringLiteral("tiff"),
          QStringLiteral("tif"), QStringLiteral("webp"), QStringLiteral("gif")};
}

#pragma once

#include "decoded_image_cache.h"

#include <QDir>
#include <QVariantList>

// Image Information presentation helpers; pure so tests need no decoded image.
// Replaces a leading home directory with "~"; a sibling such as "/home/alice2" is left unchanged.
QString abbreviateHomePath(const QString& absolutePath, const QString& home = QDir::homePath());
// "JPEG · 3072 × 4080 · 12.5 MP · 2.5 MB" without missing parts, or "Details unavailable" when all are missing.
QString formatSummaryLine(const QString& format, QSize decodedSize, qint64 encodedSize);
// "Rotated view W × H" only when the transform swaps the decoded width and height.
QString formatTransformedLine(QSize decodedSize, QSize transformedSize);
// Locale short date and time, or empty for an unknown time.
QString formatModifiedText(const QDateTime& modified);
QString joinNonEmpty(const QStringList& parts, QStringView separator = u" · ");

QVariantList formatInformationSections(const ImageInformation& information, const QString& path);
QString formatFileSize(qint64 size);

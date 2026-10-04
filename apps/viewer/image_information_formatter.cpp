#include "image_information_formatter.h"

#include <QCoreApplication>
#include <QLocale>
#include <QVariantMap>

#include <array>

QString abbreviateHomePath(const QString& absolutePath, const QString& home) {
  if (home.isEmpty()) {
    return absolutePath;
  }
  const auto normalizedHome = QDir::cleanPath(home);
  const auto normalizedPath = QDir::cleanPath(absolutePath);
  if (normalizedPath == normalizedHome) {
    return QStringLiteral("~");
  }
  // The separator check keeps "/home/alice2" from matching home "/home/alice".
  if (normalizedHome != u"/" && normalizedPath.startsWith(normalizedHome + u'/')) {
    return u'~' + normalizedPath.sliced(normalizedHome.size());
  }
  return absolutePath;
}

QString formatSummaryLine(const QString& format, QSize decodedSize, qint64 encodedSize) {
  QStringList parts{format};
  if (decodedSize.isValid() && !decodedSize.isEmpty()) {
    const auto megapixels = static_cast<double>(decodedSize.width()) * decodedSize.height() / 1'000'000.0;
    parts << QCoreApplication::translate("ImageDocument", "%1 × %2").arg(decodedSize.width()).arg(decodedSize.height())
          << QCoreApplication::translate("ImageDocument", "%1 MP").arg(QLocale().toString(megapixels, 'f', 1));
  }
  if (encodedSize >= 0) {
    parts << QLocale().formattedDataSize(encodedSize, 1, QLocale::DataSizeSIFormat);
  }
  const auto summary = joinNonEmpty(parts);
  return summary.isEmpty() ? QCoreApplication::translate("ImageDocument", "Details unavailable") : summary;
}

QString formatTransformedLine(QSize decodedSize, QSize transformedSize) {
  if (!decodedSize.isValid() || transformedSize != decodedSize.transposed() || transformedSize == decodedSize) {
    return {};
  }
  return QCoreApplication::translate("ImageDocument", "Rotated view %1 × %2")
      .arg(transformedSize.width())
      .arg(transformedSize.height());
}

QString formatModifiedText(const QDateTime& modified) {
  return modified.isValid() ? QLocale().toString(modified.toLocalTime(), QLocale::ShortFormat) : QString{};
}

QString joinNonEmpty(const QStringList& parts, QStringView separator) {
  QStringList present;
  for (const auto& part : parts) {
    if (!part.isEmpty()) {
      present << part;
    }
  }
  return present.join(separator);
}

QVariantList formatInformationSections(const ImageInformation& information, const QString& path) {
  struct Section {
    const char* key;
    QString label;
    QStringList lines;
  };
  const auto& exif = information.exif;
  // Keys are stable identifiers for QML; labels are translated for display.
  const std::array<Section, 3> sections{{
      {.key = "Camera",
       .label = QCoreApplication::translate("ImageDocument", "Camera"),
       .lines = {exif.camera, joinNonEmpty({exif.lens, exif.focal_length, exif.aperture}),
                 joinNonEmpty({exif.shutter, exif.iso})}},
      {.key = "Location",
       .label = QCoreApplication::translate("ImageDocument", "Location"),
       .lines = {joinNonEmpty({exif.location, exif.altitude})}},
      {.key = "File",
       .label = QCoreApplication::translate("ImageDocument", "File"),
       .lines = {path.isEmpty() ? QString{} : abbreviateHomePath(path)}},
  }};
  QVariantList result;
  for (const auto& section : sections) {
    auto present = section.lines;
    present.removeAll(QString{});
    if (!present.isEmpty()) {
      result.append(QVariantMap{{QStringLiteral("key"), QString::fromLatin1(section.key)},
                                {QStringLiteral("label"), section.label},
                                {QStringLiteral("lines"), present}});
    }
  }
  return result;
}

QString formatFileSize(qint64 size) {
  return size < 0 ? QCoreApplication::translate("ImageDocument", "Unavailable")
                  : QLocale().formattedDataSize(size, 1, QLocale::DataSizeSIFormat);
}

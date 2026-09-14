#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>

#include <atomic>

// Display-ready EXIF facts. Missing or malformed tags leave their field empty.
struct ExifDetails {
  QString camera;
  QString lens;
  QString aperture;
  QString shutter;
  QString iso;
  QString focalLength;
  QString location;
  QString altitude;
  [[nodiscard]] bool hasCamera() const {
    return !camera.isEmpty() || !lens.isEmpty() || !aperture.isEmpty() || !shutter.isEmpty() || !iso.isEmpty() ||
           !focalLength.isEmpty();
  }
  [[nodiscard]] bool hasLocation() const { return !location.isEmpty() || !altitude.isEmpty(); }
  bool operator==(const ExifDetails&) const = default;
};

namespace ExifMetadata {
// Locates the EXIF block in a JPEG, PNG or WebP stream with bounded seeks and reads.
QByteArray payload(QIODevice& source, const std::atomic_bool& cancelled);
// Parses an "Exif\0\0"-prefixed TIFF block with libexif.
ExifDetails parse(const QByteArray& payload);
ExifDetails read(QIODevice& source, const std::atomic_bool& cancelled);
}  // namespace ExifMetadata

#pragma once

#include <QString>

#include <holonight_images/image.h>
#include <optional>

// Display-ready EXIF facts. Missing or malformed tags leave their field empty.
struct ExifDetails {
  std::optional<HolonightImages::Outcome> outcome;
  QString camera;
  QString lens;
  QString aperture;
  QString shutter;
  QString iso;
  QString focal_length;
  QString location;
  QString altitude;
  [[nodiscard]] bool hasCamera() const {
    return !camera.isEmpty() || !lens.isEmpty() || !aperture.isEmpty() || !shutter.isEmpty() || !iso.isEmpty() ||
           !focal_length.isEmpty();
  }
  [[nodiscard]] bool hasLocation() const { return !location.isEmpty() || !altitude.isEmpty(); }
  bool operator==(const ExifDetails&) const = default;
};

namespace ExifMetadata {
// Formats structured provider facts for Viewer presentation.
ExifDetails format(const HolonightImages::ExifFacts& facts,
                   std::optional<HolonightImages::Outcome> outcome = std::nullopt);
}  // namespace ExifMetadata

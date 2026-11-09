#include "exif_metadata.h"

#include <cmath>

namespace ExifMetadata {
namespace {
QString decimal(double value, int precision) {
  auto text = QString::number(value, 'f', precision);
  while (text.contains(u'.') && (text.endsWith(u'0') || text.endsWith(u'.'))) {
    text.chop(1);
  }
  return text;
}
}  // namespace
ExifDetails format(const HolonightImages::ExifFacts& facts, std::optional<HolonightImages::Outcome> outcome) {
  ExifDetails details;
  details.outcome = outcome;
  details.camera = facts.model;
  if (facts.model.isEmpty()) {
    details.camera = facts.make;
  } else if (!facts.make.isEmpty() && !facts.model.startsWith(facts.make, Qt::CaseInsensitive)) {
    details.camera = facts.make + u' ' + facts.model;
  }
  details.lens = facts.lens;
  if (facts.aperture && *facts.aperture > 0) {
    details.aperture = QStringLiteral("f/%1").arg(*facts.aperture, 0, 'f', 1);
  }
  if (facts.exposureSeconds && *facts.exposureSeconds > 0) {
    const auto time = *facts.exposureSeconds;
    details.shutter = time < 1 ? QStringLiteral("1/%1 s").arg(std::round(1 / time), 0, 'f', 0)
                               : QStringLiteral("%1 s").arg(decimal(time, 1));
  }
  if (facts.iso && *facts.iso > 0) {
    details.iso = QStringLiteral("ISO %1").arg(*facts.iso);
  }
  if (facts.focalLengthMm && *facts.focalLengthMm > 0) {
    details.focal_length = QStringLiteral("%1 mm").arg(decimal(*facts.focalLengthMm, 1));
  }
  if (facts.latitude && facts.longitude) {
    details.location = QStringLiteral("%1° %2, %3° %4")
                           .arg(std::abs(*facts.latitude), 0, 'f', 5)
                           .arg(std::signbit(*facts.latitude) ? "S" : "N")
                           .arg(std::abs(*facts.longitude), 0, 'f', 5)
                           .arg(std::signbit(*facts.longitude) ? "W" : "E");
  }
  if (facts.altitudeMeters) {
    details.altitude = QStringLiteral("%1 m").arg(std::round(*facts.altitudeMeters), 0, 'f', 0);
  }
  return details;
}
}  // namespace ExifMetadata

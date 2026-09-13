#include "exif_metadata.h"

#include "image_document.h"

#include <QBuffer>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

#include <gtest/gtest.h>
#include <utility>

namespace {
// Minimal little-endian TIFF writer; offsets are relative to the TIFF header.
struct Field {
  quint16 tag;
  quint16 type;
  quint32 count;
  QByteArray data;
};

QByteArray le16(quint16 value) {
  QByteArray bytes(2, '\0');
  qToLittleEndian(value, bytes.data());
  return bytes;
}
QByteArray le32(quint32 value) {
  QByteArray bytes(4, '\0');
  qToLittleEndian(value, bytes.data());
  return bytes;
}
Field text(quint16 tag, const QByteArray& value) {
  return {.tag = tag, .type = 2, .count = static_cast<quint32>(value.size() + 1), .data = value + '\0'};
}
Field byte(quint16 tag, quint8 value) {
  return {.tag = tag, .type = 1, .count = 1, .data = QByteArray(1, static_cast<char>(value))};
}
Field shortValue(quint16 tag, quint16 value) { return {.tag = tag, .type = 3, .count = 1, .data = le16(value)}; }
Field longValue(quint16 tag, quint32 value) { return {.tag = tag, .type = 4, .count = 1, .data = le32(value)}; }
Field rationals(quint16 tag, std::initializer_list<std::pair<quint32, quint32>> values) {
  QByteArray data;
  for (const auto& [numerator, denominator] : values) {
    data += le32(numerator) + le32(denominator);
  }
  return {.tag = tag, .type = 5, .count = static_cast<quint32>(values.size()), .data = data};
}
qsizetype outOfLine(const Field& field) {
  return field.data.size() > 4 ? field.data.size() + (field.data.size() & 1) : 0;
}
quint32 ifdSize(const QList<Field>& fields) {
  qsizetype size = 2 + (12 * fields.size()) + 4;
  for (const auto& field : fields) {
    size += outOfLine(field);
  }
  return static_cast<quint32>(size);
}
QByteArray ifd(quint32 offset, const QList<Field>& fields) {
  QByteArray entries = le16(static_cast<quint16>(fields.size()));
  QByteArray values;
  auto valueOffset = offset + 2 + (12 * fields.size()) + 4;
  for (const auto& field : fields) {
    entries += le16(field.tag) + le16(field.type) + le32(field.count);
    if (field.data.size() <= 4) {
      entries += field.data + QByteArray(4 - field.data.size(), '\0');
    } else {
      entries += le32(static_cast<quint32>(valueOffset + values.size()));
      values += field.data + QByteArray(field.data.size() & 1, '\0');
    }
  }
  return entries + le32(0) + values;
}

QByteArray tiff(const QList<Field>& primary, const QList<Field>& exif, const QList<Field>& gps) {
  const quint32 exifOffset = 8 + ifdSize(primary) + 24;
  const quint32 gpsOffset = exifOffset + ifdSize(exif);
  auto zero = primary;
  zero << longValue(0x8769, exifOffset) << longValue(0x8825, gpsOffset);
  return QByteArray("II*\0", 4) + le32(8) + ifd(8, zero) + ifd(exifOffset, exif) + ifd(gpsOffset, gps);
}

QByteArray sonyPayload() {
  return QByteArray("Exif\0\0", 6) +
         tiff({text(0x010f, "SONY"), text(0x0110, "ILCE-7M4")},
              {rationals(0x829a, {{1, 100}}), rationals(0x829d, {{8, 1}}), shortValue(0x8827, 100),
               rationals(0x920a, {{32, 1}}), text(0xa434, "FE 24-70mm F2.8 GM II")},
              {text(0x0001, "N"), rationals(0x0002, {{50, 1}, {27, 1}, {0, 1}}), text(0x0003, "W"),
               rationals(0x0004, {{30, 1}, {31, 1}, {12, 1}}), byte(0x0005, 0), rationals(0x0006, {{1790, 10}})});
}

QByteArray jpegWithApp1(const QByteArray& exif) {
  QImage image(4, 3, QImage::Format_RGB32);
  image.fill(Qt::blue);
  QByteArray jpeg;
  QBuffer buffer(&jpeg);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "JPEG");
  const auto segment = [](const QByteArray& body) {
    QByteArray length(2, '\0');
    qToBigEndian(static_cast<quint16>(body.size() + 2), length.data());
    return QByteArray("\xff\xe1", 2) + length + body;
  };
  // An XMP APP1 segment first proves non-EXIF APP1 segments are skipped.
  return jpeg.first(2) + segment("http://ns.adobe.com/xap/1.0/\0<x/>") + segment(exif) + jpeg.sliced(2);
}

ExifDetails readBytes(QByteArray bytes) {
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::ReadOnly);
  const std::atomic_bool cancelled{false};
  return ExifMetadata::read(buffer, cancelled);
}
}  // namespace

TEST(ExifMetadata, FormatsCameraExposureAndLocation) {
  const auto details = ExifMetadata::parse(sonyPayload());
  EXPECT_EQ(details.camera, "SONY ILCE-7M4");
  EXPECT_EQ(details.lens, "FE 24-70mm F2.8 GM II");
  EXPECT_EQ(details.exposure, "f/8.0  1/100 s  ISO 100  32 mm");
  EXPECT_EQ(details.location, QStringLiteral("50.45000° N, 30.52000° W"));
  EXPECT_EQ(details.altitude, "179 m");
}

TEST(ExifMetadata, OmitsRepeatedMakeInvalidGpsAndZeroDenominators) {
  const auto details = ExifMetadata::parse(
      QByteArray("Exif\0\0", 6) +
      tiff({text(0x010f, "Canon"), text(0x0110, "Canon EOS R5")},
           {rationals(0x829d, {{8, 0}}), shortValue(0x8827, 400)},
           {text(0x0001, "N"), rationals(0x0002, {{95, 1}, {0, 1}, {0, 1}}), text(0x0003, "E"),
            rationals(0x0004, {{10, 1}, {0, 1}, {0, 1}}), byte(0x0005, 1), rationals(0x0006, {{12, 1}})}));
  EXPECT_EQ(details.camera, "Canon EOS R5");
  EXPECT_EQ(details.exposure, "ISO 400");
  EXPECT_TRUE(details.location.isEmpty());
  EXPECT_EQ(details.altitude, "-12 m");
}

TEST(ExifMetadata, ExtractsJpegPngAndWebPBlocks) {
  const auto expected = ExifMetadata::parse(sonyPayload());
  ASSERT_TRUE(expected.hasCamera() && expected.hasLocation());
  EXPECT_EQ(readBytes(jpegWithApp1(sonyPayload())), expected);

  QImage image(2, 2, QImage::Format_RGB32);
  image.fill(Qt::red);
  QByteArray png;
  QBuffer pngBuffer(&png);
  pngBuffer.open(QIODevice::WriteOnly);
  image.save(&pngBuffer, "PNG");
  const auto tiffBlock = sonyPayload().sliced(6);
  QByteArray chunkLength(4, '\0');
  qToBigEndian(static_cast<quint32>(tiffBlock.size()), chunkLength.data());
  png.insert(png.size() - 12, chunkLength + "eXIf" + tiffBlock + QByteArray(4, '\0'));
  EXPECT_EQ(readBytes(png), expected);

  // WebP: an odd-sized chunk is padded, and EXIF may omit the JPEG signature.
  QByteArray chunks = QByteArray("ICCP") + le32(3) + "abc" + '\0' + "EXIF" + le32(tiffBlock.size()) + tiffBlock;
  EXPECT_EQ(readBytes(QByteArray("RIFF") + le32(chunks.size() + 4) + "WEBP" + chunks), expected);
}

TEST(ExifMetadata, IgnoresMissingOversizedAndDamagedBlocks) {
  EXPECT_EQ(readBytes("not an image"), ExifDetails{});
  EXPECT_EQ(ExifMetadata::parse(QByteArray("Exif\0\0II*\0\xff\xff\xff\xff", 14)), ExifDetails{});
  EXPECT_EQ(ExifMetadata::parse(sonyPayload().first(40)).location, QString{});

  QByteArray png("\x89PNG\r\n\x1a\n", 8);
  png += QByteArray("\0\x20\0\0eXIf", 8) + QByteArray(2 * 1024 * 1024, 'a') + QByteArray(4, '\0');
  EXPECT_EQ(readBytes(png), ExifDetails{});

  auto cancelledBytes = jpegWithApp1(sonyPayload());
  QBuffer buffer(&cancelledBytes);
  buffer.open(QIODevice::ReadOnly);
  const std::atomic_bool cancelled{true};
  EXPECT_EQ(ExifMetadata::read(buffer, cancelled), ExifDetails{});
}

TEST(ExifMetadata, InformationTextShowsExifSections) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto path = dir.filePath("camera.jpg");
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(jpegWithApp1(sonyPayload()));
  file.close();
  ImageDocument document;
  document.open({QUrl::fromLocalFile(path)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  EXPECT_EQ(document.information().exif, ExifMetadata::parse(sonyPayload()));
  const auto sections = document.informationText().split(QStringLiteral("\n\n"));
  ASSERT_EQ(sections.size(), 4);
  EXPECT_TRUE(sections[0].startsWith("JPEG image\n4 × 3"));
  EXPECT_EQ(sections[1], "SONY ILCE-7M4\nFE 24-70mm F2.8 GM II\nf/8.0  1/100 s  ISO 100  32 mm");
  EXPECT_EQ(sections[2], QStringLiteral("50.45000° N, 30.52000° W\nAltitude 179 m"));
  EXPECT_EQ(sections[3], path);

  const auto plain = dir.filePath("plain.png");
  QImage image(2, 2, QImage::Format_RGB32);
  image.fill(Qt::green);
  ASSERT_TRUE(image.save(plain));
  document.open({QUrl::fromLocalFile(plain)});
  ASSERT_TRUE(
      QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready && document.fileName() == "plain.png"; }));
  EXPECT_EQ(document.informationText().count(QStringLiteral("\n\n")), 1);
}

TEST(ExifMetadata, InformationTextShowsRotatedTransformedDimensions) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto path = dir.filePath("portrait.png");
  QImage image(4, 3, QImage::Format_RGB32);
  image.fill(Qt::blue);
  ASSERT_TRUE(image.save(path));

  ImageDocument document;
  document.open({QUrl::fromLocalFile(path)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));

  const auto decoded = document.information().decodedSize;
  const auto initialTransformed = document.transformedDimensions();
  EXPECT_EQ(initialTransformed, decoded);
  EXPECT_TRUE(document.informationText().contains(
      QStringLiteral("Transformed dimensions: %1 × %2").arg(decoded.width()).arg(decoded.height())));

  document.transform(1);
  ASSERT_TRUE(
      QTest::qWaitFor([&] { return document.transformedDimensions() == QSize(decoded.height(), decoded.width()); }));
  const auto rotated = document.transformedDimensions();
  EXPECT_NE(rotated, decoded);
  EXPECT_TRUE(document.informationText().contains(
      QStringLiteral("Transformed dimensions: %1 × %2").arg(rotated.width()).arg(rotated.height())));
}

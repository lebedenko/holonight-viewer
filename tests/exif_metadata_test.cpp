#include "exif_metadata.h"

#include "exif_fixture.h"
#include "image_document.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QtEndian>

#include <gtest/gtest.h>
#include <utility>

namespace {
using namespace ExifFixture;

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
  EXPECT_EQ(details.aperture, "f/8.0");
  EXPECT_EQ(details.shutter, "1/100 s");
  EXPECT_EQ(details.iso, "ISO 100");
  EXPECT_EQ(details.focalLength, "32 mm");
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
  EXPECT_TRUE(details.lens.isEmpty());
  EXPECT_TRUE(details.aperture.isEmpty());
  EXPECT_TRUE(details.shutter.isEmpty());
  EXPECT_EQ(details.iso, "ISO 400");
  EXPECT_TRUE(details.focalLength.isEmpty());
  EXPECT_TRUE(details.hasCamera());
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

TEST(ImageInformationFormat, JoinsOnlyNonEmptyParts) {
  EXPECT_EQ(joinNonEmpty({}), QString{});
  EXPECT_EQ(joinNonEmpty({{}, {}}), QString{});
  EXPECT_EQ(joinNonEmpty({"6.9 mm", {}, "f/2.2"}), QStringLiteral("6.9 mm · f/2.2"));
  EXPECT_EQ(joinNonEmpty({"a", "b"}, u", "), "a, b");
}

TEST(ImageInformationFormat, AbbreviatesOnlyPathsInsideHome) {
  const QString home = "/home/alice";
  EXPECT_EQ(abbreviateHomePath("/home/alice/Pictures/image.jpg", home), "~/Pictures/image.jpg");
  EXPECT_EQ(abbreviateHomePath("/home/alice/Pictures/image.jpg", "/home/alice/"), "~/Pictures/image.jpg");
  EXPECT_EQ(abbreviateHomePath("/home/alice", home), "~");
  EXPECT_EQ(abbreviateHomePath("/srv/photos/image.jpg", home), "/srv/photos/image.jpg");
  EXPECT_EQ(abbreviateHomePath("/home/alice2/x", home), "/home/alice2/x");
  EXPECT_EQ(abbreviateHomePath("/home/alice/x", {}), "/home/alice/x");
  EXPECT_EQ(abbreviateHomePath("/etc/x", "/"), "/etc/x");
}

TEST(ImageInformationFormat, SummaryOmitsMissingParts) {
  QLocale::setDefault(QLocale::c());
  EXPECT_EQ(formatSummaryLine("JPEG", {3072, 4080}, 2'500'000),
            QStringLiteral("JPEG · 3072 × 4080 · 12.5 MP · 2.5 MB"));
  EXPECT_EQ(formatSummaryLine("JPEG", {}, 2'500'000), QStringLiteral("JPEG · 2.5 MB"));
  EXPECT_EQ(formatSummaryLine("PNG", {4, 3}, -1), QStringLiteral("PNG · 4 × 3 · 0.0 MP"));
  EXPECT_EQ(formatSummaryLine({}, {}, 2'500'000), "2.5 MB");
  EXPECT_EQ(formatSummaryLine({}, {}, -1), "Details unavailable");
  QLocale::setDefault(QLocale::system());
}

TEST(ImageInformationFormat, TransformedLineOnlyForSwappedDimensions) {
  const QSize decoded(4, 3);
  EXPECT_EQ(formatTransformedLine(decoded, decoded), QString{});
  EXPECT_EQ(formatTransformedLine(decoded, ImageOrientation::dimensions(ImageOrientation::compose(0, 4), decoded)),
            QString{});
  const auto rotated = ImageOrientation::dimensions(ImageOrientation::compose(0, 1), decoded);
  EXPECT_EQ(formatTransformedLine(decoded, rotated), QStringLiteral("Rotated view 3 × 4"));
  EXPECT_EQ(formatTransformedLine({5, 5}, {5, 5}), QString{});
  EXPECT_EQ(formatTransformedLine({}, {}), QString{});
}

TEST(ImageInformationFormat, ModifiedTextFollowsDefaultLocale) {
  const auto modified = QDateTime(QDate(2026, 9, 14), QTime(15, 45), QTimeZone::UTC);
  for (const auto& locale : {QLocale(QLocale::English, QLocale::UnitedStates), QLocale(QLocale::German)}) {
    QLocale::setDefault(locale);
    EXPECT_EQ(formatModifiedText(modified), QLocale().toString(modified.toLocalTime(), QLocale::ShortFormat));
  }
  EXPECT_NE(QLocale(QLocale::English, QLocale::UnitedStates).toString(modified.toLocalTime(), QLocale::ShortFormat),
            QLocale(QLocale::German).toString(modified.toLocalTime(), QLocale::ShortFormat));
  QLocale::setDefault(QLocale::system());
  EXPECT_EQ(formatModifiedText({}), QString{});
}

namespace {
QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& bytes) {
  auto path = dir.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
    return {};
  }
  return path;
}

bool openAndSettle(ImageDocument& document, const QUrl& url) {
  document.open({url});
  return QTest::qWaitFor(
      [&] { return document.state() == ImageDocument::Ready || document.state() == ImageDocument::Error; });
}

QList<std::pair<QString, QStringList>> sections(const ImageDocument& document) {
  QList<std::pair<QString, QStringList>> result;
  for (const auto& section : document.informationSections()) {
    const auto map = section.toMap();
    EXPECT_EQ(map.value("key").toString(), map.value("label").toString()) << "untranslated tests use matching keys";
    result.append({map.value("label").toString(), map.value("lines").toStringList()});
  }
  return result;
}
}  // namespace

TEST(ImageInformationProperties, ExifRichJpegHasCameraLocationAndFileSections) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto path = writeFile(dir, "camera.jpg", jpegWithApp1(sonyPayload()));
  ImageDocument document;
  ASSERT_TRUE(openAndSettle(document, QUrl::fromLocalFile(path)));
  ASSERT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_EQ(document.information().exif, ExifMetadata::parse(sonyPayload()));

  EXPECT_EQ(document.summaryLine(), formatSummaryLine("JPEG", {4, 3}, QFileInfo(path).size()));
  EXPECT_EQ(document.transformedLine(), QString{});
  EXPECT_EQ(document.modifiedText(), formatModifiedText(QFileInfo(path).lastModified()));
  EXPECT_FALSE(document.modifiedText().isEmpty());
  EXPECT_EQ(document.displayPath(), abbreviateHomePath(path));

  const QList<std::pair<QString, QStringList>> expected{
      {"Camera",
       {"SONY ILCE-7M4", QStringLiteral("FE 24-70mm F2.8 GM II · 32 mm · f/8.0"), QStringLiteral("1/100 s · ISO 100")}},
      {"Location", {QStringLiteral("50.45000° N, 30.52000° W · 179 m")}},
      {"File", {abbreviateHomePath(path)}},
  };
  EXPECT_EQ(sections(document), expected);
}

TEST(ImageInformationProperties, CameraLinesOmitMissingLensAndIso) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto payload =
      QByteArray("Exif\0\0", 6) +
      tiff({text(0x010f, "Google"), text(0x0110, "Pixel 7 Pro")},
           {rationals(0x829a, {{1, 20}}), rationals(0x829d, {{22, 10}}), rationals(0x920a, {{69, 10}})}, {});
  const auto path = writeFile(dir, "pixel.jpg", jpegWithApp1(payload));
  ImageDocument document;
  ASSERT_TRUE(openAndSettle(document, QUrl::fromLocalFile(path)));

  const auto result = sections(document);
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].first, "Camera");
  EXPECT_EQ(result[0].second,
            QStringList({"Google Pixel 7 Pro", QStringLiteral("6.9 mm · f/2.2"), QStringLiteral("1/20 s")}));
  EXPECT_EQ(result[1].first, "File");
}

TEST(ImageInformationProperties, ImageWithoutExifHasOnlyFileSection) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto path = dir.filePath("plain.png");
  QImage image(4, 3, QImage::Format_RGB32);
  image.fill(Qt::green);
  ASSERT_TRUE(image.save(path));
  ImageDocument document;
  ASSERT_TRUE(openAndSettle(document, QUrl::fromLocalFile(path)));

  EXPECT_EQ(document.summaryLine(), formatSummaryLine("PNG", {4, 3}, QFileInfo(path).size()));
  EXPECT_EQ(sections(document), (QList<std::pair<QString, QStringList>>{{"File", {abbreviateHomePath(path)}}}));

  document.transform(1);
  EXPECT_EQ(document.transformedLine(), QStringLiteral("Rotated view 3 × 4"));
  document.transform(4);
  EXPECT_EQ(document.transformedLine(), QStringLiteral("Rotated view 3 × 4"));
  document.resetTransform();
  EXPECT_EQ(document.transformedLine(), QString{});
}

TEST(ImageInformationProperties, FailedLocalLoadKeepsSizeAndFileSection) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto path = writeFile(dir, "broken.jpg", QByteArray(10, 'x'));
  ImageDocument document;
  ASSERT_TRUE(openAndSettle(document, QUrl::fromLocalFile(path)));
  ASSERT_EQ(document.state(), ImageDocument::Error);

  EXPECT_NE(document.summaryLine(), "Details unavailable");
  EXPECT_TRUE(document.summaryLine().endsWith(QLocale().formattedDataSize(10, 1, QLocale::DataSizeSIFormat)));
  EXPECT_EQ(document.transformedLine(), QString{});
  EXPECT_EQ(sections(document), (QList<std::pair<QString, QStringList>>{{"File", {abbreviateHomePath(path)}}}));
}

TEST(ImageInformationProperties, NonLocalDocumentHasNoDetailsOrSections) {
  ImageDocument document;
  ASSERT_TRUE(openAndSettle(document, QUrl("https://example.com/image.jpg")));
  ASSERT_EQ(document.state(), ImageDocument::Error);

  EXPECT_EQ(document.summaryLine(), "Details unavailable");
  EXPECT_EQ(document.transformedLine(), QString{});
  EXPECT_EQ(document.modifiedText(), QString{});
  EXPECT_EQ(document.displayPath(), QString{});
  EXPECT_TRUE(document.informationSections().isEmpty());
}

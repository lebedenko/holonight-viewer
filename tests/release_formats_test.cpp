#include "image_document.h"

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

#include <array>
#include <gtest/gtest.h>

TEST(Release, RequiredIndependentFormats) {
  ImageDocument document;
  for (const auto* format : {"png", "jpeg", "bmp", "webp", "gif", "tif", "tiff"}) {
    ASSERT_TRUE(QImageReader::supportedImageFormats().contains(format)) << "Required decoder: " << format;
  }
  for (const auto* extension : {"png", "jpg", "bmp", "webp", "gif", "tif"}) {
    const auto path = QStringLiteral(RELEASE_FIXTURE_DIR) + "/sample." + extension;
    QFile original(path);
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    const auto bytes = original.readAll();
    original.close();
    document.open({QUrl::fromLocalFile(path)});
    ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() != ImageDocument::Loading; }));
    ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
    if (QByteArray(extension) == "jpg") {
      EXPECT_EQ(document.image().size(), QSize(20, 40));
      // Fixed JPEG: stored red left half, green upper right, blue lower right.
      // Orientation 6 rotates clockwise; tolerate lossy compression noise.
      const auto red = document.image().pixelColor(15, 5);
      const auto green = document.image().pixelColor(15, 30);
      const auto blue = document.image().pixelColor(5, 30);
      EXPECT_GT(red.red(), 220);
      EXPECT_LT(red.green(), 40);
      EXPECT_LT(red.blue(), 40);
      EXPECT_GT(green.green(), 220);
      EXPECT_LT(green.red(), 40);
      EXPECT_LT(green.blue(), 40);
      EXPECT_GT(blue.blue(), 220);
      EXPECT_LT(blue.red(), 40);
      EXPECT_LT(blue.green(), 40);
    } else {
      EXPECT_EQ(document.image().size(), QSize(1, 1));
      EXPECT_EQ(document.image().pixelColor(0, 0),
                QColor(255, 0, 0, QByteArray(extension) == "bmp" || QByteArray(extension) == "gif" ? 255 : 128));
    }
    document.transform(1);
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    EXPECT_EQ(original.readAll(), bytes);
    // Content detection must survive a misleading extension.
    const auto mismatch = QStringLiteral(RELEASE_FIXTURE_DIR) + "/mismatch-" + extension +
                          (QByteArray(extension) == "png" ? ".jpg" : ".png");
    QFile copy(mismatch);
    ASSERT_TRUE(copy.open(QIODevice::WriteOnly));
    ASSERT_EQ(copy.write(bytes), bytes.size());
    copy.close();
    document.open({QUrl::fromLocalFile(mismatch)});
    ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() != ImageDocument::Loading; }));
    EXPECT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  }
}

TEST(Release, CompactWebPDecoderGate) {
  ImageDocument document;
  document.open({QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/compact.webp")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() != ImageDocument::Loading; }));
  ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  EXPECT_EQ(document.image().pixelColor(0, 0), QColor(255, 0, 0, 128));
}

TEST(Release, SimpleWebPContentCorruptionAndLimits) {
  QTemporaryDir directory(QStringLiteral(RELEASE_FIXTURE_DIR) + "/webp-XXXXXX");
  ASSERT_TRUE(directory.isValid());
  QFile source(QStringLiteral(RELEASE_FIXTURE_DIR) + "/compact.webp");
  ASSERT_TRUE(source.open(QIODevice::ReadOnly));
  const auto original = source.readAll();
  std::atomic_bool cancelled{false};
  const auto decode = [&](const QByteArray& bytes) {
    const auto path = directory.filePath("misleading.jpg");
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly));
    EXPECT_EQ(file.write(bytes), bytes.size());
    file.close();
    auto result = decodeImage(QUrl::fromLocalFile(path), cancelled);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), bytes);
    return result;
  };
  auto result = decode(original);
  ASSERT_FALSE(result.image.isNull());
  EXPECT_EQ(result.information.format, "WEBP");
  EXPECT_EQ(result.image.size(), QSize(1, 1));
  EXPECT_EQ(result.image.pixelColor(0, 0), QColor(255, 0, 0, 128));
  EXPECT_EQ(result.image.format(), QImage::Format_ARGB32_Premultiplied);
  for (qsizetype size = 0; size < original.size(); ++size) {
    EXPECT_TRUE(decode(original.first(size)).image.isNull()) << size;
  }
  auto broken = original;
  broken[20] = 0;  // Invalid VP8L signature.
  EXPECT_TRUE(decode(broken).image.isNull());
  broken = original;
  broken.replace(16, 4, QByteArray::fromHex("ffffffff"));
  EXPECT_TRUE(decode(broken).image.isNull());
  // VP8L packs 14-bit width/height minus one into the four signature-following bytes.
  broken = original;
  broken.replace(21, 4, QByteArray::fromHex("ffffff1f"));
  result = decode(broken);
  EXPECT_TRUE(result.image.isNull());
  EXPECT_EQ(result.outcome, HolonightImages::Outcome::ResourceLimit);
  cancelled.store(true);
  EXPECT_TRUE(decode(original).image.isNull());
}

TEST(Release, QtWebPContainersAndAnimatedFirstFrame) {
  std::atomic_bool cancelled{false};
  for (const auto* name : {"sample.webp", "extended.webp", "animated.webp", "lossy.webp"}) {
    const auto path = QStringLiteral(RELEASE_FIXTURE_DIR) + "/" + name;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto expected = reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    ASSERT_FALSE(expected.isNull()) << name;
    const auto result = decodeImage(QUrl::fromLocalFile(path), cancelled);
    EXPECT_EQ(result.image, expected) << name;
    EXPECT_EQ(result.image.size(), QSize(1, 1));
    if (QByteArray(name) == "animated.webp") {
      EXPECT_EQ(reader.imageCount(), 2);
      EXPECT_EQ(result.image.pixelColor(0, 0), QColor(255, 0, 0, 128));
    }
  }
}

TEST(Release, GifFixturesAreSequencesWithTheirDelaysAndLoops) {
  struct Expected {
    const char* name;
    std::array<int, 2> delays;
    int loopCount;
  };
  for (const auto& [name, delays, loopCount] :
       {Expected{.name = "sample.gif", .delays = {100, 200}, .loopCount = -1},
        Expected{.name = "gif87a.gif", .delays = {100, 100}, .loopCount = 0},
        Expected{.name = "transparent.gif", .delays = {100, 200}, .loopCount = 2}}) {
    QImageReader reader(QStringLiteral(RELEASE_FIXTURE_DIR) + "/" + name);
    ASSERT_TRUE(reader.canRead()) << name;
    EXPECT_EQ(reader.imageCount(), 2) << name;
    EXPECT_EQ(reader.loopCount(), loopCount) << name;
    for (const int delay : delays) {
      ASSERT_FALSE(reader.read().isNull()) << name;
      EXPECT_EQ(reader.nextImageDelay(), delay) << name;
    }
  }
  // Both revisions are opened and played by the viewer itself.
  ImageDocument document;
  for (const auto* name : {"sample.gif", "gif87a.gif", "transparent.gif"}) {
    document.open({QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/" + name)});
    ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() != ImageDocument::Loading; })) << name;
    ASSERT_EQ(document.state(), ImageDocument::Ready) << name;
    EXPECT_TRUE(QTest::qWaitFor([&] { return document.animation()->animated(); })) << name;
    EXPECT_EQ(document.animation()->frameCount(), 2) << name;
  }
}

TEST(Release, TiffGuaranteedVariantsDecode) {
  struct Expected {
    const char* name = nullptr;
    QColor color;
  };
  std::atomic_bool cancelled{false};
  for (const auto& [name, color] : {Expected{.name = "tiff-rgb8.tif", .color = QColor(255, 0, 0, 255)},
                                    Expected{.name = "sample.tif", .color = QColor(255, 0, 0, 128)},
                                    Expected{.name = "tiff-gray8.tif", .color = QColor(128, 128, 128, 255)},
                                    Expected{.name = "tiff-palette8.tif", .color = QColor(255, 0, 0, 255)}}) {
    const auto result = decodeImage(QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/" + name), cancelled);
    ASSERT_FALSE(result.image.isNull()) << name << ": " << result.error.toStdString();
    EXPECT_TRUE(result.error.isEmpty()) << name;
    EXPECT_EQ(result.image.size(), QSize(1, 1)) << name;
    EXPECT_EQ(result.image.pixelColor(0, 0), color) << name;
    EXPECT_EQ(result.information.format, "TIFF") << name;
    // These fixtures carry no EXIF tags: that is an empty summary, not an error.
    ExifDetails empty;
    empty.outcome = HolonightImages::Outcome::Success;
    EXPECT_EQ(result.information.exif, empty) << name;
  }
}

TEST(Release, TiffMultiPageShowsFirstPage) {
  std::atomic_bool cancelled{false};
  const auto result =
      decodeImage(QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/tiff-two-page.tif"), cancelled);
  ASSERT_FALSE(result.image.isNull()) << result.error.toStdString();
  EXPECT_EQ(result.image.pixelColor(0, 0), QColor(255, 0, 0, 255));
  EXPECT_NE(result.image.pixelColor(0, 0), QColor(0, 255, 0, 255));
}

namespace {
QByteArray readFixture(const char* name) {
  QFile file(QStringLiteral(RELEASE_FIXTURE_DIR) + "/" + name);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << name;
  return file.readAll();
}

DecodeResult decodeBytes(QTemporaryDir& directory, const QByteArray& bytes) {
  const auto path = directory.filePath("copy.tif");
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::WriteOnly));
  EXPECT_EQ(file.write(bytes), bytes.size());
  file.close();
  const std::atomic_bool cancelled{false};
  return decodeImage(QUrl::fromLocalFile(path), cancelled);
}
}  // namespace

TEST(Release, TiffLimitsApply) {
  QTemporaryDir directory(QStringLiteral(RELEASE_FIXTURE_DIR) + "/tiff-XXXXXX");
  ASSERT_TRUE(directory.isValid());
  const auto original = readFixture("tiff-rgb8.tif");
  // The fixture writer puts ImageWidth's value word at offset 18 and ImageLength's at offset 30.
  const auto patched = [&](quint32 width, quint32 height) {
    const auto word = [](quint32 value) {
      QByteArray bytes(4, '\0');
      qToLittleEndian(value, bytes.data());
      return bytes;
    };
    auto bytes = original;
    bytes.replace(18, 4, word(width));
    bytes.replace(30, 4, word(height));
    return decodeBytes(directory, bytes);
  };
  const auto accepted = patched(1, 1);
  ASSERT_FALSE(accepted.image.isNull()) << accepted.error.toStdString();
  for (const auto& [width, height] : {std::pair<quint32, quint32>{32769, 1}, {1, 32769}, {6000, 6000}}) {
    const auto result = patched(width, height);
    EXPECT_TRUE(result.image.isNull()) << width << "x" << height;
    EXPECT_EQ(result.outcome, HolonightImages::Outcome::ResourceLimit) << width << "x" << height;
  }
}

TEST(Release, TiffTruncatedIsAnError) {
  QTemporaryDir directory(QStringLiteral(RELEASE_FIXTURE_DIR) + "/tiff-XXXXXX");
  ASSERT_TRUE(directory.isValid());
  const auto truncated = decodeBytes(directory, readFixture("tiff-truncated.tif"));
  EXPECT_TRUE(truncated.image.isNull());
  ASSERT_TRUE(truncated.outcome);
  EXPECT_NE(truncated.outcome, HolonightImages::Outcome::Success);
  EXPECT_NE(truncated.outcome, HolonightImages::Outcome::Cancelled);
  // Every proper prefix stops at a different stage: header, IFD, tag data or strip.
  const auto original = readFixture("tiff-rgb8.tif");
  ASSERT_FALSE(decodeBytes(directory, original).image.isNull());
  for (qsizetype size = 0; size < original.size(); ++size) {
    const auto result = decodeBytes(directory, original.first(size));
    EXPECT_TRUE(result.image.isNull()) << size;
    ASSERT_TRUE(result.outcome) << size;
    EXPECT_NE(result.outcome, HolonightImages::Outcome::Success) << size;
    EXPECT_NE(result.outcome, HolonightImages::Outcome::Cancelled) << size;
  }
}

TEST(Release, TiffBestEffortVariantsNeverCrashOrHang) {
  const std::atomic_bool cancelled{false};
  for (const auto* name :
       {"tiff-be-rgb16.tif", "tiff-be-float.tif", "tiff-be-cmyk.tif", "tiff-be-lab.tif", "tiff-be-tiled.tif",
        "tiff-be-bigtiff.tif", "tiff-be-lzw.tif", "tiff-be-packbits.tif", "tiff-be-deflate.tif"}) {
    const auto result = decodeImage(QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/" + name), cancelled);
    // Best effort: either the variant opens or the user gets an error; never both, never neither.
    ASSERT_TRUE(result.outcome) << name;
    EXPECT_NE(result.outcome, HolonightImages::Outcome::Cancelled) << name;
    EXPECT_NE(result.image.isNull(), result.outcome == HolonightImages::Outcome::Success) << name;
    RecordProperty(name, static_cast<int>(*result.outcome));
  }
}

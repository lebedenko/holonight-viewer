#include "image_document.h"

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

TEST(Release, RequiredIndependentFormats) {
  ImageDocument document;
  for (const auto* format : {"png", "jpeg", "bmp", "webp"}) {
    ASSERT_TRUE(QImageReader::supportedImageFormats().contains(format)) << "Required decoder: " << format;
  }
  for (const auto* extension : {"png", "jpg", "bmp", "webp"}) {
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
      EXPECT_EQ(document.image().pixelColor(0, 0), QColor(255, 0, 0, QByteArray(extension) == "bmp" ? 255 : 128));
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
  EXPECT_TRUE(result.error.contains("viewing limit"));
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

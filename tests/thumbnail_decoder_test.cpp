#include "thumbnail_decoder.h"

#include "exif_fixture.h"
#include "gif_fixture.h"
#include "thumbnail_size.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtEndian>

#include <array>
#include <gtest/gtest.h>
#include <holonight_thumbnails/cache.h>

namespace {
const std::atomic_bool kNotCancelled{false};

QString writeBytes(const QTemporaryDir& dir, const QString& name, const QByteArray& bytes) {
  auto path = dir.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
    return {};
  }
  return path;
}

QString writeImage(const QTemporaryDir& dir, const QString& name, QSize size, const char* format,
                   QColor color = Qt::red) {
  QImage image(size, QImage::Format_RGB32);
  image.fill(color);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, format);
  return writeBytes(dir, name, bytes);
}

// ExifFixture::jpegWithApp1 puts an XMP segment before the EXIF one, and Qt's JPEG handler then reports no
// orientation, so the EXIF segment goes first here.
QByteArray jpegWithExif(const QByteArray& payload, QSize size = {4, 3}) {
  QImage image(size, QImage::Format_RGB32);
  image.fill(Qt::blue);
  QByteArray jpeg;
  QBuffer buffer(&jpeg);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "JPEG");
  QByteArray length(2, '\0');
  qToBigEndian(static_cast<quint16>(payload.size() + 2), length.data());
  return jpeg.first(2) + QByteArray("\xff\xe1", 2) + length + payload + jpeg.sliced(2);
}

ThumbnailResult decode(const QString& path, qreal dpr, const std::atomic_bool& cancelled = kNotCancelled) {
  return decodeThumbnail({.path = path, .box_pixels = thumbnailBoxPixels(dpr)}, cancelled);
}

quint32 crc32(const QByteArray& bytes) {
  quint32 crc = 0xffffffffU;
  for (const char byte : bytes) {
    crc ^= static_cast<quint8>(byte);
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb88320U : 0U);
    }
  }
  return ~crc;
}

QByteArray pngChunk(const QByteArray& type, const QByteArray& data) {
  QByteArray length(4, '\0');
  qToBigEndian<quint32>(static_cast<quint32>(data.size()), length.data());
  QByteArray checksum(4, '\0');
  qToBigEndian<quint32>(crc32(type + data), checksum.data());
  return length + type + data + checksum;
}

// A well-formed header followed by a bogus pixel chunk: a decoder that reaches the pixels reports damage.
QByteArray pngDeclaring(quint32 width, quint32 height) {
  const auto bigEndian = [](quint32 value) {
    QByteArray bytes(4, '\0');
    qToBigEndian<quint32>(value, bytes.data());
    return bytes;
  };
  // Bit depth 8, truecolor, then the default compression, filter and interlace.
  const auto header = bigEndian(width) + bigEndian(height) + QByteArray::fromHex("0802000000");
  return QByteArray("\x89PNG\r\n\x1a\n", 8) + pngChunk("IHDR", header) + pngChunk("IDAT", "x") + pngChunk("IEND", {});
}

// One solid 4x4 frame per colour index. Every 3-bit code group restarts the LZW table with a clear code,
// so the stream needs no dictionary and stays valid at a fixed code width.
QByteArray solidFrame(int colorIndex) {
  QByteArray packed;
  quint32 bits = 0;
  int count = 0;
  const auto put = [&](int code) {
    bits |= static_cast<quint32>(code) << count;
    count += 3;
    while (count >= 8) {
      packed.append(static_cast<char>(bits & 0xff));
      bits >>= 8;
      count -= 8;
    }
  };
  for (int pair = 0; pair < 8; ++pair) {
    put(4);  // clear
    put(colorIndex);
    put(colorIndex);
  }
  put(5);  // end of information
  if (count > 0) {
    packed.append(static_cast<char>(bits & 0xff));
  }
  return QByteArray::fromHex("21f9040000000000") + QByteArray::fromHex("2c000000000400040000") + QByteArray(1, '\x02') +
         QByteArray(1, static_cast<char>(packed.size())) + packed + QByteArray(1, '\0');
}

QByteArray redGreenBlueGif() {
  // Screen 4x4 with a four-colour global table: red, green, blue, black.
  return QByteArray("GIF89a") +
         QByteArray::fromHex(
             "040004008100"
             "00"
             "ff0000"
             "00ff00"
             "0000ff"
             "000000") +
         QByteArray::fromHex("21ff0b") + "NETSCAPE2.0" +
         QByteArray::fromHex(
             "03010000"
             "00") +
         solidFrame(0) + solidFrame(1) + solidFrame(2) + QByteArray::fromHex("3b");
}

QByteArray svgWithViewBox() {
  return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 200 100'>"
         "<rect width='200' height='100' fill='#3366cc'/></svg>";
}

struct CacheHome {
  explicit CacheHome(const QString& path)
      : wasSet(qEnvironmentVariableIsSet("XDG_CACHE_HOME")), old(qgetenv("XDG_CACHE_HOME")) {
    qputenv("XDG_CACHE_HOME", path.toUtf8());
  }
  CacheHome(const CacheHome&) = delete;
  CacheHome& operator=(const CacheHome&) = delete;
  CacheHome(CacheHome&&) = delete;
  CacheHome& operator=(CacheHome&&) = delete;
  ~CacheHome() {
    if (wasSet) {
      qputenv("XDG_CACHE_HOME", old);
    } else {
      qunsetenv("XDG_CACHE_HOME");
    }
  }
  bool wasSet;
  QByteArray old;
};

HolonightThumbnails::Request cacheRequest(const QString& path, QSize required, HolonightThumbnails::Kind kind) {
  const QFileInfo info(path);
  return {.uri = QUrl::fromLocalFile(info.absoluteFilePath()),
          .modified = info.lastModified(),
          .size = info.size(),
          .required = required,
          .kind = kind,
          .revision = {}};
}
}  // namespace

TEST(ThumbnailDecoder, LargeSourceFitsTheDeviceBox) {
  QTemporaryDir dir;
  QImage source(4000, 3000, QImage::Format_Grayscale8);
  source.fill(Qt::gray);
  ASSERT_TRUE(source.save(dir.filePath("big.png")));
  for (const auto [dpr, width, height] :
       {std::array{1.0, 256.0, 192.0}, std::array{1.25, 320.0, 240.0}, std::array{2.0, 512.0, 384.0}}) {
    const auto result = decode(dir.filePath("big.png"), dpr);
    ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
    EXPECT_EQ(result.image.size(), QSize(static_cast<int>(width), static_cast<int>(height))) << dpr;
    EXPECT_EQ(result.source_size, QSize(4000, 3000));
    EXPECT_EQ(result.image.format(), QImage::Format_ARGB32_Premultiplied);
  }
}

TEST(ThumbnailDecoder, SmallSourceIsNeverEnlarged) {
  QTemporaryDir dir;
  const auto path = writeImage(dir, "small.png", {100, 100}, "PNG");
  for (const qreal dpr : {1.0, 2.0}) {
    const auto result = decode(path, dpr);
    ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
    EXPECT_EQ(result.image.size(), QSize(100, 100)) << dpr;
    EXPECT_EQ(result.source_size, QSize(100, 100));
  }
}

TEST(ThumbnailDecoder, PortraitAndLandscapeKeepTheirAspect) {
  QTemporaryDir dir;
  EXPECT_EQ(decode(writeImage(dir, "wide.png", {1024, 768}, "PNG"), 1).image.size(), QSize(256, 192));
  EXPECT_EQ(decode(writeImage(dir, "tall.png", {768, 1024}, "PNG"), 1).image.size(), QSize(192, 256));
}

TEST(ThumbnailDecoder, ExifOrientationIsApplied) {
  QTemporaryDir dir;
  const auto payload = QByteArray("Exif\0\0", 6) + ExifFixture::tiff({ExifFixture::shortValue(0x0112, 6)}, {}, {});
  const auto path = writeBytes(dir, "rotated.jpg", jpegWithExif(payload));
  const auto result = decode(path, 1);
  ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
  // The fixture is 4x3 and orientation 6 turns it a quarter.
  EXPECT_EQ(result.image.size(), QSize(3, 4));
  EXPECT_EQ(result.source_size, QSize(3, 4));
}

TEST(ThumbnailDecoder, OverLimitHeaderIsRejectedBeforeAnyPixelRead) {
  QTemporaryDir dir;
  const auto path = writeBytes(dir, "huge.png", pngDeclaring(40000, 40000));
  const auto result = decode(path, 1);
  EXPECT_TRUE(result.image.isNull());
  // With no pixel data, a decoder that started reading pixels would report damage instead.
  EXPECT_TRUE(result.error.contains(QLatin1String("limit"))) << qPrintable(result.error);
}

TEST(ThumbnailDecoder, CorruptAndMissingFilesFail) {
  QTemporaryDir dir;
  const auto corrupt = writeBytes(dir, "broken.jpg", QByteArray("\xff\xd8\xff\xe0garbage that is not a jpeg"));
  const auto result = decode(corrupt, 1);
  EXPECT_TRUE(result.image.isNull());
  EXPECT_FALSE(result.error.isEmpty());
  EXPECT_TRUE(decode(dir.filePath("absent.png"), 1).image.isNull());
  EXPECT_TRUE(decode(dir.path(), 1).image.isNull());
  EXPECT_TRUE(decode(writeBytes(dir, "text.png", "plain text"), 1).image.isNull());
}

TEST(ThumbnailDecoder, AnimatedImageYieldsItsFirstFrame) {
  if (!gif::available()) {
    GTEST_SKIP() << "The GIF plugin is unavailable";
  }
  QTemporaryDir dir;
  const auto path = writeBytes(dir, "rgb.gif", redGreenBlueGif());
  const auto result = decode(path, 1);
  ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
  EXPECT_EQ(result.image.size(), QSize(4, 4));
  EXPECT_EQ(result.image.pixel(2, 2), QColor(Qt::red).rgba());
}

TEST(ThumbnailDecoder, SvgScalesUpToTheBoxKeepingAspect) {
  QTemporaryDir dir;
  const auto path = writeBytes(dir, "wide.svg", svgWithViewBox());
  const auto one = decode(path, 1);
  ASSERT_FALSE(one.image.isNull()) << qPrintable(one.error);
  EXPECT_EQ(one.image.size(), QSize(256, 128));
  EXPECT_EQ(one.source_size, QSize(256, 128));
  EXPECT_EQ(qAlpha(one.image.pixel(128, 64)), 255);
  const auto two = decode(path, 2);
  ASSERT_FALSE(two.image.isNull()) << qPrintable(two.error);
  EXPECT_EQ(two.image.size(), QSize(512, 256));
  EXPECT_EQ(two.source_size, QSize(256, 128));
}

TEST(ThumbnailDecoder, SvgWithExternalResourcesFails) {
  QTemporaryDir dir;
  const auto path = writeBytes(dir, "linked.svg",
                               "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' "
                               "viewBox='0 0 10 10'><image xlink:href='https://example.com/a.png'/></svg>");
  EXPECT_TRUE(decode(path, 1).image.isNull());
}

TEST(ThumbnailDecoder, ReusesSharedRasterPixelsAndPreservesLogicalSourceSize) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  CacheHome cacheHome(dir.filePath("cache"));
  const auto path = writeImage(dir, "red.png", {1024, 1024}, "PNG");
  QImage shared({256, 256}, QImage::Format_ARGB32_Premultiplied);
  shared.fill(Qt::blue);
  ASSERT_TRUE(HolonightThumbnails::store(cacheRequest(path, {256, 256}, HolonightThumbnails::Kind::Raster), shared,
                                         kNotCancelled));

  const auto result = decode(path, 1);
  ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
  EXPECT_EQ(result.image.pixelColor(128, 128), QColor(Qt::blue));
  EXPECT_EQ(result.source_size, QSize(1024, 1024));
}

TEST(ThumbnailDecoder, WritesAspectAwareAndOrientedThumbnailEntries) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  CacheHome cacheHome(dir.filePath("cache"));
  const auto landscape = writeImage(dir, "landscape.png", {1024, 768}, "PNG");
  const auto portrait = writeImage(dir, "portrait.png", {768, 1024}, "PNG");
  const auto payload = QByteArray("Exif\0\0", 6) + ExifFixture::tiff({ExifFixture::shortValue(0x0112, 6)}, {}, {});
  const auto oriented = writeBytes(dir, "oriented.jpg", jpegWithExif(payload));
  const auto largeOriented = writeBytes(dir, "large-oriented.jpg", jpegWithExif(payload, {400, 300}));

  for (const auto& [path, pixels] : {std::pair{landscape, QSize(256, 192)}, std::pair{portrait, QSize(192, 256)},
                                     std::pair{oriented, QSize(3, 4)}, std::pair{largeOriented, QSize(192, 256)}}) {
    const auto result = decode(path, 1);
    ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
    const auto cached =
        HolonightThumbnails::lookup(cacheRequest(path, pixels, HolonightThumbnails::Kind::Raster), kNotCancelled);
    ASSERT_TRUE(cached.has_value()) << qPrintable(path);
    EXPECT_EQ(cached->size(), pixels);
  }
}

TEST(ThumbnailDecoder, RequestsAboveLargestTierDecodeTheSource) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  CacheHome cacheHome(dir.filePath("cache"));
  const auto path = writeImage(dir, "red.png", {1600, 1600}, "PNG");
  QImage cached({1024, 1024}, QImage::Format_ARGB32_Premultiplied);
  cached.fill(Qt::blue);
  ASSERT_TRUE(HolonightThumbnails::store(cacheRequest(path, {1024, 1024}, HolonightThumbnails::Kind::Raster), cached,
                                         kNotCancelled));

  const auto result = decodeThumbnail({.path = path, .box_pixels = 1100}, kNotCancelled);
  ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
  EXPECT_EQ(result.image.size(), QSize(1100, 1100));
  EXPECT_EQ(result.image.pixelColor(550, 550), QColor(Qt::red));
}

TEST(ThumbnailDecoder, CacheWriteFailureDoesNotPreventSourceDecode) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto blocked = writeBytes(dir, "blocked-cache-home", "regular file");
  ASSERT_FALSE(blocked.isEmpty());
  CacheHome cacheHome(blocked);
  const auto path = writeImage(dir, "red.png", {1024, 1024}, "PNG");

  const auto result = decode(path, 1);
  ASSERT_FALSE(result.image.isNull()) << qPrintable(result.error);
  EXPECT_EQ(result.image.size(), QSize(256, 256));
  EXPECT_EQ(result.image.pixelColor(128, 128), QColor(Qt::red));
}

TEST(ThumbnailDecoder, SvgResourceValidationPrecedesDiskLookup) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  CacheHome cacheHome(dir.filePath("cache"));
  const auto path = writeBytes(dir, "linked.svg",
                               "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' "
                               "viewBox='0 0 10 10'><image xlink:href='https://example.com/a.png'/></svg>");
  QImage cached({256, 256}, QImage::Format_ARGB32_Premultiplied);
  cached.fill(Qt::blue);
  ASSERT_TRUE(HolonightThumbnails::store(cacheRequest(path, {256, 256}, HolonightThumbnails::Kind::Svg), cached,
                                         kNotCancelled));

  EXPECT_TRUE(decode(path, 1).image.isNull());
}

TEST(ThumbnailDecoder, CancelledBeforeStartDecodesNothing) {
  QTemporaryDir dir;
  const auto path = writeImage(dir, "a.png", {10, 10}, "PNG");
  const std::atomic_bool cancelled{true};
  const auto result = decode(path, 1, cancelled);
  EXPECT_TRUE(result.image.isNull());
  EXPECT_EQ(result.error, QLatin1String("Cancelled"));
}

TEST(ThumbnailDecoder, LocalImageSvgBypassesCachesAndReflectsLinkedEdits) {
  QTemporaryDir directory;
  const auto linked = writeImage(directory, "linked.png", {16, 8}, "PNG", Qt::red);
  ASSERT_FALSE(linked.isEmpty());
  const auto svg = writeBytes(
      directory, "linked.svg",
      "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' width='16' height='8'>"
      "<image xlink:href='linked.png' width='16' height='8'/></svg>");
  for (const int box : {256, 320}) {
    const auto first = decodeThumbnail({.path = svg, .box_pixels = box}, kNotCancelled);
    ASSERT_FALSE(first.image.isNull()) << qPrintable(first.error);
    EXPECT_FALSE(first.cache_eligible);
    EXPECT_EQ(first.image.size(), QSize(box, box / 2));
    EXPECT_EQ(first.source_size, QSize(256, 128));
    EXPECT_EQ(first.image.pixelColor(box / 2, box / 4), QColor(Qt::red));
  }
  ASSERT_FALSE(writeImage(directory, "linked.png", {16, 8}, "PNG", Qt::blue).isEmpty());
  const auto changed = decodeThumbnail({.path = svg, .box_pixels = 320}, kNotCancelled);
  ASSERT_FALSE(changed.image.isNull());
  EXPECT_EQ(changed.image.pixelColor(160, 80), QColor(Qt::blue));
}

TEST(ThumbnailDecoder, MixedUnsupportedSvgReferencesRemainRejected) {
  QTemporaryDir directory;
  const auto svg = writeBytes(directory, "mixed.svg",
                              "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='8'>"
                              "<image href='linked.png'/><image href='https://example.com/image.png'/></svg>");
  EXPECT_TRUE(decodeThumbnail({.path = svg, .box_pixels = 256}, kNotCancelled).image.isNull());
}

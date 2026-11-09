#include "image_document.h"
#include "image_formats.h"
#include "image_runtime.h"

#include <QColorSpace>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace {
using HolonightImages::Outcome;
using HolonightImages::RuntimeStatus;
using MetadataReply = HolonightImages::IsolatedResult<HolonightImages::MetadataResult>;
using RasterReply = HolonightImages::IsolatedResult<HolonightImages::DecodeResult>;
QUrl fixture(QTemporaryDir& dir) {
  const auto path = dir.filePath("image.png");
  QImage image(4, 3, QImage::Format_ARGB32);
  image.fill(QColor(255, 0, 0, 128));
  EXPECT_TRUE(image.save(path));
  return QUrl::fromLocalFile(path);
}
RasterReply success() {
  HolonightImages::DecodeResult result;
  result.image = QImage(4, 3, QImage::Format_ARGB32);
  result.image.fill(Qt::red);
  result.inspection.format = "png";
  return {.runtime = RuntimeStatus::Success, .result = result, .diagnostic = {}};
}
}  // namespace

TEST(IsolatedDecoder, InfrastructureFailuresNeverBecomeProviderOutcomesOrFallback) {
  QTemporaryDir dir;
  const auto url = fixture(dir);
  const std::atomic_bool running{false};
  for (auto status : {RuntimeStatus::Unavailable, RuntimeStatus::ProtocolFailure, RuntimeStatus::Timeout,
                      RuntimeStatus::CleanupFailure, RuntimeStatus::Cancelled}) {
    for (bool metadataFailure : {false, true}) {
      int decodes = 0;
      RasterOperations operations{
          .metadata = [&](QIODevice&, const std::atomic_bool&) -> MetadataReply {
            if (metadataFailure) {
              return {.runtime = status, .result = std::nullopt, .diagnostic = {}};
            }
            return {.runtime = RuntimeStatus::Success, .result = HolonightImages::MetadataResult{}, .diagnostic = {}};
          },
          .decode = [&](QIODevice&, const std::atomic_bool&) -> RasterReply {
            ++decodes;
            return {.runtime = status, .result = std::nullopt, .diagnostic = {}};
          }};
      const auto result = decodeImageWithOperations(url, running, operations);
      EXPECT_EQ(result.runtime, status);
      EXPECT_FALSE(result.outcome);
      EXPECT_TRUE(result.image.isNull());
      EXPECT_EQ(result.error, HolonightFileBrowser::imageRuntimeError(status));
      EXPECT_EQ(decodes, metadataFailure ? 0 : 1);
    }
  }
}

TEST(IsolatedDecoder, CancellationBetweenOperationsDoesNotDecode) {
  for (const bool signal : {false, true}) {
    QTemporaryDir dir;
    const auto url = fixture(dir);
    std::atomic_bool cancelled{false};
    RasterOperations operations{
        .metadata = [&](QIODevice&, const std::atomic_bool&) -> MetadataReply {
          cancelled.store(signal);
          return {.runtime = RuntimeStatus::Success,
                  .result = HolonightImages::MetadataResult{.outcome = signal ? Outcome::Success : Outcome::Cancelled,
                                                            .facts = {}},
                  .diagnostic = {}};
        },
        .decode = [](QIODevice&, const std::atomic_bool&) -> RasterReply {
          ADD_FAILURE() << "decode after cancellation";
          return success();
        }};
    const auto result = decodeImageWithOperations(url, cancelled, operations);
    EXPECT_EQ(result.outcome, Outcome::Cancelled);
    EXPECT_TRUE(result.image.isNull());
  }
}

TEST(IsolatedDecoder, ReusesOpenDeviceAndPreservesIncompleteStructuredMetadata) {
  QTemporaryDir dir;
  const auto url = fixture(dir);
  const std::atomic_bool running{false};
  QIODevice* source = nullptr;
  RasterOperations operations{.metadata = [&](QIODevice& device, const std::atomic_bool&) -> MetadataReply {
                                source = &device;
                                EXPECT_TRUE(device.isOpen());
                                HolonightImages::MetadataResult result;
                                result.outcome = Outcome::ResourceLimit;
                                result.facts.make = "SONY";
                                result.facts.model = "ILCE-7M4";
                                result.facts.aperture = 8;
                                result.facts.exposureSeconds = .01;
                                return {.runtime = RuntimeStatus::Success, .result = result, .diagnostic = {}};
                              },
                              .decode =
                                  [&](QIODevice& device, const std::atomic_bool&) {
                                    EXPECT_EQ(&device, source);
                                    EXPECT_TRUE(device.isOpen());
                                    return success();
                                  }};
  const auto result = decodeImageWithOperations(url, running, operations);
  EXPECT_EQ(result.outcome, Outcome::Success);
  EXPECT_EQ(result.information.exif.outcome, Outcome::ResourceLimit);
  EXPECT_EQ(result.information.exif.camera, "SONY ILCE-7M4");
  EXPECT_EQ(result.information.exif.aperture, "f/8.0");
  EXPECT_EQ(result.information.exif.shutter, "1/100 s");
  EXPECT_EQ(result.image.format(), QImage::Format_ARGB32_Premultiplied);
  EXPECT_EQ(result.image.size(), QSize(4, 3));
}

TEST(IsolatedDecoder, PreservesProviderFailures) {
  QTemporaryDir dir;
  const auto url = fixture(dir);
  const std::atomic_bool running{false};
  for (auto outcome :
       {Outcome::Unsupported, Outcome::Damaged, Outcome::ResourceLimit, Outcome::IoFailure, Outcome::Cancelled}) {
    RasterOperations operations{
        .metadata = [](QIODevice&, const std::atomic_bool&) -> MetadataReply {
          return {.runtime = RuntimeStatus::Success, .result = HolonightImages::MetadataResult{}, .diagnostic = {}};
        },
        .decode = [&](QIODevice&, const std::atomic_bool&) -> RasterReply {
          HolonightImages::DecodeResult result;
          result.outcome = outcome;
          return {.runtime = RuntimeStatus::Success, .result = result, .diagnostic = {}};
        }};
    const auto result = decodeImageWithOperations(url, running, operations);
    EXPECT_EQ(result.runtime, RuntimeStatus::Success);
    EXPECT_EQ(result.outcome, outcome);
    EXPECT_TRUE(result.image.isNull());
  }
}

TEST(IsolatedDecoder, MissingOrInvalidBundleRejectsAHostReadablePng) {
  QTemporaryDir dir;
  const auto url = fixture(dir);
  const std::atomic_bool running{false};
  for (const auto& path : {dir.filePath("missing"), dir.path()}) {
    const auto result = decodeImageWithRuntime(url, running, {path});
    EXPECT_EQ(result.runtime, RuntimeStatus::Unavailable);
    EXPECT_FALSE(result.outcome);
    EXPECT_TRUE(result.image.isNull());
  }
}

TEST(IsolatedDecoder, FormatPolicyDoesNotAdvertiseHostOnlyPlugins) {
  EXPECT_EQ(viewerImageSuffixes(), (QStringList{"png", "jpeg", "jpg", "jpe", "bmp", "tiff", "tif", "webp", "gif"}));
}

TEST(IsolatedHost, NamedAndUntaggedColorsAndAlpha) {
  QTemporaryDir dir;
  const auto path = dir.filePath("color.png");
  for (int color = 0; color <= QColorSpace::Bt2100Hlg; ++color) {
    QImage image(4, 3, QImage::Format_ARGB32);
    image.fill(QColor(12, 34, 56, 128));
    image.setColorSpace(color != 0 ? QColorSpace(QColorSpace::NamedColorSpace(color)) : QColorSpace());
    ASSERT_TRUE(image.save(path));
    const auto result = viewer_test::decode(QUrl::fromLocalFile(path), std::atomic_bool{false});
    ASSERT_EQ(result.runtime, RuntimeStatus::Success) << result.error.toStdString();
    ASSERT_EQ(result.outcome, Outcome::Success) << color;
    EXPECT_EQ(result.image.size(), image.size());
    EXPECT_EQ(result.image.colorSpace(), image.colorSpace());
    EXPECT_EQ(result.image.pixelColor(0, 0).alpha(), 128);
  }
}

TEST(IsolatedHost, CustomProfileRejectedWithoutDroppingOrConverting) {
  QTemporaryDir dir;
  const auto path = dir.filePath("custom.png");
  QImage image(4, 3, QImage::Format_ARGB32);
  image.fill(Qt::red);
  image.setColorSpace(QColorSpace(QColorSpace::Primaries::SRgb, QColorSpace::TransferFunction::Gamma, 1.7));
  ASSERT_TRUE(image.save(path));
  const auto result = viewer_test::decode(QUrl::fromLocalFile(path), std::atomic_bool{false});
  ASSERT_EQ(result.runtime, RuntimeStatus::Success) << result.error.toStdString();
  EXPECT_EQ(result.outcome, Outcome::Unsupported);
  EXPECT_TRUE(result.image.isNull());
}

TEST(IsolatedDocument, TentativeRuntimeFailurePreservesDisplayAndRefreshRetries) {
  QTemporaryDir dir;
  const auto first = fixture(dir);
  const auto second = QUrl::fromLocalFile(dir.filePath("second.png"));
  ASSERT_TRUE(QFile::copy(first.toLocalFile(), second.toLocalFile()));
  std::atomic_bool fail{false};
  std::atomic_int calls{0};
  ImageDocument document(
      [&](const QUrl&, const std::atomic_bool&) {
        ++calls;
        DecodeResult result;
        if (fail.load()) {
          result.runtime = RuntimeStatus::Unavailable;
          result.error = HolonightFileBrowser::imageRuntimeError(result.runtime);
        } else {
          result.image = success().result->image;
          result.information.format = "PNG";
          result.information.decoded_size = result.image.size();
          result.outcome = Outcome::Success;
        }
        return result;
      },
      [](const QUrl& url, const std::atomic_bool&) {
        return DirectoryResult{.urls = {url}, .error = {}, .missing = {}};
      });
  document.open({first});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready && !document.scanning(); }));
  const auto displayed = document.image();
  fail.store(true);
  const auto before = calls.load();
  document.tryOpenPastedPath(second);
  ASSERT_TRUE(QTest::qWaitFor([&] { return calls.load() > before; }));
  // Queue another tentative request: a failed result must not enter the retained cache.
  document.tryOpenPastedPath(second);
  ASSERT_TRUE(QTest::qWaitFor([&] { return calls.load() > before + 1; }));
  EXPECT_EQ(document.url(), first);
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_EQ(document.image(), displayed);
  document.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Error; }));
  EXPECT_EQ(document.error(), HolonightFileBrowser::imageRuntimeError(RuntimeStatus::Unavailable));
  fail.store(false);
  document.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  EXPECT_EQ(document.image(), displayed);
}

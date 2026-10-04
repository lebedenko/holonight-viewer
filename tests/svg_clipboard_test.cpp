#include "image_document.h"

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>
#include <limits>

namespace {
QUrl writeSvg(const QTemporaryDir& directory, const QString& name, const QByteArray& bytes) {
  QFile file(directory.filePath(name));
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
    return {};
  }
  return QUrl::fromLocalFile(file.fileName());
}
}  // namespace

TEST(SvgClipboard, CopiesIntrinsicPixelsAlphaAndCapturedOrientation) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const auto url = writeSvg(directory, "copy.svg",
                            "<svg xmlns='http://www.w3.org/2000/svg' width='40' height='20'>"
                            "<rect width='20' height='20' fill='#ff0000' fill-opacity='0.5'/></svg>");
  ImageDocument document;
  document.open({url});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  document.transform(1);
  QGuiApplication::clipboard()->setText("previous clipboard");
  document.copyImage();
  ASSERT_TRUE(document.clipboard()->busy());
  document.open({QUrl::fromLocalFile(directory.filePath("missing.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  const auto image = QImage::fromData(QGuiApplication::clipboard()->mimeData()->data("image/png"), "PNG");
  ASSERT_FALSE(image.isNull());
  EXPECT_EQ(image.size(), QSize(20, 40));
  EXPECT_NEAR(image.pixelColor(10, 10).alpha(), 128, 1);
  EXPECT_EQ(image.pixelColor(10, 30).alpha(), 0);
}

TEST(SvgClipboard, AnimatedMarkupRemainsStaticAndStopsAfterNavigation) {
  QTemporaryDir directory;
  const auto url = writeSvg(directory, "animated.svg",
                            "<svg xmlns='http://www.w3.org/2000/svg' width='40' height='20'>"
                            "<rect width='40' height='20' fill='red'>"
                            "<animate attributeName='fill' values='red;blue;red' dur='0.1s' repeatCount='indefinite'/>"
                            "</rect></svg>");
  ImageDocument document;
  document.open({url});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  auto* renderer = document.svgRenderer();
  ASSERT_NE(renderer, nullptr);
  EXPECT_TRUE(renderer->options().testFlag(QtSvg::DisableAnimations));
  QSignalSpy repaint(renderer, &QSvgRenderer::repaintNeeded);
  QTest::qWait(150);
  EXPECT_TRUE(repaint.isEmpty());
  document.open({QUrl::fromLocalFile(directory.filePath("missing.png"))});
  QTest::qWait(150);
  EXPECT_TRUE(repaint.isEmpty());
}

TEST(SvgClipboard, IntrinsicSizeIsRoundedAndOversizedDimensionsAreBounded) {
  EXPECT_EQ(svgClipboardSize({40, 20}), QSize(40, 20));
  EXPECT_EQ(svgClipboardSize({0.25, 0.5}), QSize(1, 1));
  EXPECT_EQ(svgClipboardSize({2.4, 3.6}), QSize(2, 4));
  EXPECT_EQ(svgClipboardSize({40000, 1}), QSize(32768, 1));
  const auto large = svgClipboardSize({40000, 80000});
  EXPECT_LE(static_cast<qint64>(large.width()) * large.height(), 32000000);
  EXPECT_NEAR(static_cast<double>(large.height()) / large.width(), 2, 0.001);
  EXPECT_TRUE(svgClipboardSize({0, 10}).isEmpty());
  EXPECT_TRUE(svgClipboardSize({std::numeric_limits<double>::infinity(), 10}).isEmpty());
}

TEST(SvgClipboard, OversizedThinDocumentCopiesWithoutAllocatingItsIntrinsicExtent) {
  QTemporaryDir directory;
  ImageDocument document;
  document.open({writeSvg(directory, "wide.svg",
                          "<svg xmlns='http://www.w3.org/2000/svg' width='40000' height='1'>"
                          "<rect width='40000' height='1' fill='red'/></svg>")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  document.copyImage();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  const auto image = QImage::fromData(QGuiApplication::clipboard()->mimeData()->data("image/png"), "PNG");
  EXPECT_EQ(image.size(), QSize(32768, 1));
  EXPECT_EQ(image.pixelColor(100, 0), QColor(Qt::red));
}

TEST(SvgClipboard, ResolvesLocalImagesAndKeepsClipboardOnPreparationFailure) {
  QTemporaryDir directory;
  QImage linked(4, 2, QImage::Format_ARGB32_Premultiplied);
  linked.fill(QColor(255, 0, 0, 128));
  ASSERT_TRUE(linked.save(directory.filePath("linked.png")));
  ImageDocument document;
  document.open({writeSvg(directory, "linked.svg",
                          "<svg xmlns='http://www.w3.org/2000/svg' width='4' height='2'>"
                          "<image href='linked.png' width='4' height='2'/></svg>")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  document.copyImage();
  ASSERT_TRUE(document.clipboard()->busy());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  const auto image = QImage::fromData(QGuiApplication::clipboard()->mimeData()->data("image/png"), "PNG");
  ASSERT_EQ(image.size(), QSize(4, 2));
  EXPECT_EQ(image.pixelColor(0, 0), linked.pixelColor(0, 0));
  QGuiApplication::clipboard()->setText("preserve me");
  document.clipboard()->copySvg({.bytes = "invalid SVG", .local_path = {}, .intrinsic_size = {4, 2}}, 0, "invalid.svg");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  EXPECT_EQ(QGuiApplication::clipboard()->text(), "preserve me");
  EXPECT_TRUE(document.clipboard()->feedback().startsWith("Could not prepare"));
}

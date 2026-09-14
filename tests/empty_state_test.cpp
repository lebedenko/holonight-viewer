#include "image_document.h"

#include <QAccessible>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <gtest/gtest.h>

namespace {
constexpr int kBodyRole = 4;     // HnTypographyRole.Body
constexpr int kCaptionRole = 5;  // HnTypographyRole.Caption

struct EmptyFixture {
  ImageDocument document;
  QQmlApplicationEngine engine;
  QQuickWindow* window = nullptr;
  QQuickItem* glyph = nullptr;
  QQuickItem* primary = nullptr;
  QQuickItem* secondary = nullptr;
  QQuickItem* area = nullptr;

  ::testing::AssertionResult load() {
    engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
    engine.loadFromModule("HolonightViewer", "Main");
    if (engine.rootObjects().size() != 1) {
      return ::testing::AssertionFailure() << "Main failed to load";
    }
    window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window == nullptr) {
      return ::testing::AssertionFailure() << "no window";
    }
    window->requestActivate();
    if (!QTest::qWaitForWindowActive(window)) {
      return ::testing::AssertionFailure() << "window not active";
    }
    glyph = window->findChild<QQuickItem*>("emptyState");
    primary = window->findChild<QQuickItem*>("emptyStateHintPrimary");
    secondary = window->findChild<QQuickItem*>("emptyStateHintSecondary");
    if (glyph == nullptr || primary == nullptr || secondary == nullptr || glyph->parentItem() == nullptr) {
      return ::testing::AssertionFailure() << "empty-state items not found";
    }
    area = glyph->parentItem()->parentItem();
    if (area == nullptr) {
      return ::testing::AssertionFailure() << "canvas area not found";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] ::testing::AssertionResult resize(QSize size) const {
    window->resize(size);
    if (!QTest::qWaitFor([&] { return window->size() == size; })) {
      return ::testing::AssertionFailure() << "window did not resize";
    }
    QTest::qWait(50);
    return ::testing::AssertionSuccess();
  }
  [[nodiscard]] QRectF inArea(QQuickItem* item) const {
    return {item->mapToItem(area, {0, 0}), QSizeF(item->width(), item->height())};
  }
};
}  // namespace

TEST(EmptyState, HintWordingStyleAndPlacement) {
  EmptyFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.resize({1000, 700}));
  auto* palette = fixture.engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  const auto muted = palette->property("textMuted").value<QColor>();

  EXPECT_EQ(fixture.primary->property("text").toString(), "No image open");
  EXPECT_EQ(fixture.secondary->property("text").toString(),
            QString::fromUtf8("Ctrl+O to open · or drop an image here"));
  EXPECT_EQ(fixture.primary->property("role").toInt(), kBodyRole);
  EXPECT_EQ(fixture.secondary->property("role").toInt(), kCaptionRole);
  EXPECT_EQ(fixture.primary->property("color").value<QColor>(), muted);
  EXPECT_EQ(fixture.secondary->property("color").value<QColor>(), muted);
  EXPECT_EQ(fixture.secondary->property("textFormat").toInt(), Qt::PlainText);
  // Plain text: no keycap inside the hint.
  for (auto* child : fixture.secondary->parentItem()->childItems()) {
    EXPECT_FALSE(child->inherits("HnKeyHint_QMLTYPE") || child->metaObject()->className() == QByteArray("HnKeyHint"));
  }

  ASSERT_TRUE(fixture.glyph->isVisible());
  ASSERT_TRUE(fixture.primary->isVisible());
  ASSERT_TRUE(fixture.secondary->isVisible());
  const auto glyph = fixture.inArea(fixture.glyph);
  const auto primary = fixture.inArea(fixture.primary);
  const auto secondary = fixture.inArea(fixture.secondary);
  EXPECT_LT(glyph.bottom(), primary.top());
  EXPECT_LE(primary.bottom(), secondary.top());
  const QRectF bounds(0, 0, fixture.area->width(), fixture.area->height());
  for (const auto& rect : {glyph, primary, secondary}) {
    EXPECT_TRUE(bounds.contains(rect)) << rect.x() << "," << rect.y() << " " << rect.width() << "x" << rect.height();
  }
}

TEST(EmptyState, TextKeepsSizeWhileGlyphGivesWay) {
  EmptyFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.resize({1000, 700}));
  const auto primaryFont = fixture.primary->property("font").value<QFont>();
  const auto secondaryFont = fixture.secondary->property("font").value<QFont>();
  const auto fullGlyph = fixture.glyph->width();
  const auto primaryHeight = fixture.primary->height();

  // Progressively shorter windows: the glyph shrinks, then hides, while the text never changes size.
  qreal previousGlyph = fullGlyph;
  bool hidden = false;
  for (const auto height : {600, 500, 420, 360, 320, 280}) {
    ASSERT_TRUE(fixture.resize({1000, height}));
    EXPECT_EQ(fixture.primary->property("font").value<QFont>(), primaryFont) << height;
    EXPECT_EQ(fixture.secondary->property("font").value<QFont>(), secondaryFont) << height;
    EXPECT_DOUBLE_EQ(fixture.primary->height(), primaryHeight) << height;
    EXPECT_TRUE(fixture.primary->isVisible() && fixture.secondary->isVisible()) << height;
    const QRectF bounds(0, 0, fixture.area->width(), fixture.area->height());
    EXPECT_TRUE(bounds.contains(fixture.inArea(fixture.primary))) << height;
    EXPECT_TRUE(bounds.contains(fixture.inArea(fixture.secondary))) << height;
    if (fixture.glyph->isVisible()) {
      EXPECT_FALSE(hidden) << "glyph reappeared at " << height;
      EXPECT_LE(fixture.glyph->width(), previousGlyph) << height;
      EXPECT_GE(fixture.glyph->width(), 48) << height;
      EXPECT_LT(fixture.inArea(fixture.glyph).bottom(), fixture.inArea(fixture.primary).top()) << height;
      EXPECT_TRUE(bounds.contains(fixture.inArea(fixture.glyph))) << height;
      previousGlyph = fixture.glyph->width();
    } else {
      hidden = true;
    }
  }
  EXPECT_LT(previousGlyph, fullGlyph);

  // At the minimum window the footer wraps, leaving too little height for glyph and hint together.
  ASSERT_TRUE(fixture.resize({420, 280}));
  EXPECT_FALSE(fixture.glyph->isVisible());
  EXPECT_TRUE(fixture.primary->isVisible() && fixture.secondary->isVisible());
  EXPECT_EQ(fixture.secondary->property("font").value<QFont>(), secondaryFont);
  EXPECT_EQ(fixture.primary->property("font").value<QFont>(), primaryFont);
  const auto secondary = fixture.inArea(fixture.secondary);
  EXPECT_GE(secondary.left(), 0);
  EXPECT_LE(secondary.right(), fixture.area->width());
  if (fixture.secondary->implicitWidth() > fixture.area->width() - 32) {
    EXPECT_GT(fixture.secondary->property("lineCount").toInt(), 1);
  }
}

TEST(EmptyState, HiddenOutsideEmptyAndAccessible) {
  QAccessible::setActive(true);
  EmptyFixture fixture;
  ASSERT_TRUE(fixture.load());
  auto* primary = QAccessible::queryAccessibleInterface(fixture.primary);
  auto* secondary = QAccessible::queryAccessibleInterface(fixture.secondary);
  ASSERT_TRUE(primary && secondary);
  EXPECT_EQ(primary->role(), QAccessible::StaticText);
  EXPECT_EQ(primary->text(QAccessible::Name), "No image open");
  EXPECT_EQ(secondary->role(), QAccessible::StaticText);
  EXPECT_EQ(secondary->text(QAccessible::Name), QString::fromUtf8("Ctrl+O to open · or drop an image here"));
  // The glyph is decoration only: not a child of the canvas area's accessible node.
  auto* areaNode = QAccessible::queryAccessibleInterface(fixture.window);
  ASSERT_NE(areaNode, nullptr);
  std::function<bool(QAccessibleInterface*)> containsGlyph = [&](QAccessibleInterface* node) {
    if (node == nullptr) {
      return false;
    }
    if (node->object() == fixture.glyph) {
      return true;
    }
    for (int index = 0; index < node->childCount(); ++index) {
      if (containsGlyph(node->child(index))) {
        return true;
      }
    }
    return false;
  };
  EXPECT_FALSE(containsGlyph(areaNode));

  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/empty-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  QImage image(8, 6, QImage::Format_RGB32);
  image.fill(Qt::darkCyan);
  ASSERT_TRUE(image.save(dir.filePath("ready.png")));
  fixture.document.open({QUrl::fromLocalFile(dir.filePath("ready.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }));
  EXPECT_FALSE(fixture.glyph->isVisible());
  EXPECT_FALSE(fixture.primary->isVisible());
  EXPECT_FALSE(fixture.secondary->isVisible());

  QFile broken(dir.filePath("broken.png"));
  ASSERT_TRUE(broken.open(QIODevice::WriteOnly));
  broken.write("not an image");
  broken.close();
  fixture.document.open({QUrl::fromLocalFile(dir.filePath("broken.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Error; }));
  EXPECT_FALSE(fixture.primary->isVisible());
  EXPECT_FALSE(fixture.secondary->isVisible());
  auto* feedback = fixture.window->findChild<QQuickItem*>("documentFeedback");
  ASSERT_NE(feedback, nullptr);
  EXPECT_TRUE(feedback->isVisible());
}

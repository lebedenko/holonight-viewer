#include "image_document.h"

#include <QAccessible>
#include <QFontInfo>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <array>
#include <cmath>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <set>

namespace {
struct Hint {
  const char* name;
  const char* key;
  const char* label;
};

constexpr std::array<Hint, 7> kHints{{{.name = "Navigate", .key = "[ or ]", .label = "Navigate"},
                                      {.name = "Zoom", .key = "Ctrl plus + or Ctrl plus -", .label = "Zoom"},
                                      {.name = "Fit", .key = "Ctrl plus 0", .label = "Fit"},
                                      {.name = "ActualSize", .key = "1", .label = "100%"},
                                      {.name = "Rotate", .key = "R", .label = "Rotate"},
                                      {.name = "Fullscreen", .key = "F", .label = "Fullscreen"},
                                      {.name = "Help", .key = "?", .label = "Help"}}};

// Hint rows created by the Repeater inside the footer row, in layout order.
QList<QQuickItem*> hintRows(QQuickItem* footer) {
  QList<QQuickItem*> rows;
  const std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
    for (auto* child : item->childItems()) {
      const auto name = child->objectName();
      if (name.startsWith(QStringLiteral("footerHint"))) {
        rows.append(child);
      } else {
        walk(child);
      }
    }
  };
  walk(footer);
  return rows;
}

QQuickItem* part(QQuickItem* row, const char* suffix) {
  return row->findChild<QQuickItem*>(row->objectName() + QLatin1String(suffix));
}

qreal sceneCenterY(QQuickItem* item) { return item->mapToScene(QPointF(0, item->height() / 2)).y(); }

void collectTexts(QQuickItem* item, QStringList& texts) {
  for (const auto* property : {"text", "rawText"}) {
    const auto value = item->property(property);
    if (value.isValid()) {
      texts.append(value.toString());
    }
  }
  for (auto* child : item->childItems()) {
    collectTexts(child, texts);
  }
}

// The footer Flow has no accessible role, so walk the window tree and keep its descendants.
void collectAccessibleNames(QAccessibleInterface* node, QQuickItem* footer, QStringList& names) {
  if (node == nullptr) {
    return;
  }
  auto* item = qobject_cast<QQuickItem*>(node->object());
  const auto name = node->text(QAccessible::Name);
  if (item != nullptr && footer->isAncestorOf(item) && !name.isEmpty()) {
    names.append(name);
  }
  for (int index = 0; index < node->childCount(); ++index) {
    collectAccessibleNames(node->child(index), footer, names);
  }
}

struct ViewerFixture {
  ImageDocument document;
  QQmlApplicationEngine engine;
  QQuickWindow* window = nullptr;
  QQuickItem* footer = nullptr;

  ::testing::AssertionResult load() {
    engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
    engine.loadFromModule("HolonightViewer", "Main");
    if (engine.rootObjects().size() != 1) {
      return ::testing::AssertionFailure() << "Main failed to load";
    }
    window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window == nullptr || !QTest::qWaitForWindowExposed(window)) {
      return ::testing::AssertionFailure() << "window not exposed";
    }
    footer = window->findChild<QQuickItem*>(QStringLiteral("footer"));
    if (footer == nullptr) {
      return ::testing::AssertionFailure() << "footer not found";
    }
    return ::testing::AssertionSuccess();
  }
};

// Standalone footer has no window minimum width, so it can be laid out at 400 px.
struct StandaloneFooter {
  QQmlEngine engine;
  QQuickWindow window;
  std::unique_ptr<QQuickItem> footer;

  ::testing::AssertionResult load(qreal width) {
    QQmlComponent component(&engine);
    component.loadFromModule("HolonightViewer", "FooterKeyHints");
    footer.reset(qobject_cast<QQuickItem*>(component.create()));
    if (!footer) {
      return ::testing::AssertionFailure() << component.errorString().toStdString();
    }
    // Paint the themed ground so captures show the footer as it appears in the viewer.
    if (auto* palette = engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette")) {
      window.setColor(palette->property("background").value<QColor>());
    }
    footer->setParentItem(window.contentItem());
    footer->setWidth(width);
    window.resize(static_cast<int>(width), 200);
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
      return ::testing::AssertionFailure() << "standalone window not exposed";
    }
    window.resize(static_cast<int>(width), static_cast<int>(footer->height()) + 1);
    QTest::qWait(50);
    return ::testing::AssertionSuccess();
  }
};
}  // namespace

// REQ-F-001, REQ-F-003, REQ-F-004: seven hints, authoritative keys and labels, in order.
TEST(FooterKeyHints, ContentOrderAndCount) {
  ViewerFixture viewer;
  ASSERT_TRUE(viewer.load());
  const auto rows = hintRows(viewer.footer);
  ASSERT_EQ(rows.size(), static_cast<qsizetype>(kHints.size()));
  for (std::size_t index = 0; index < kHints.size(); ++index) {
    const auto& hint = kHints.at(index);
    auto* row = rows.at(static_cast<qsizetype>(index));
    EXPECT_EQ(row->objectName(), QStringLiteral("footerHint") + QLatin1String(hint.name));
    auto* keycap = part(row, "Keycap");
    auto* label = part(row, "Label");
    ASSERT_NE(keycap, nullptr) << hint.name;
    ASSERT_NE(label, nullptr) << hint.name;
    EXPECT_EQ(keycap->property("accessibleText").toString(), QString::fromUtf8(hint.key));
    EXPECT_EQ(label->property("rawText").toString(), QString::fromUtf8(hint.label));
  }
  QStringList texts;
  collectTexts(viewer.footer, texts);
  for (const auto* removed : {"Q", "Quit", "I", "Information"}) {
    EXPECT_FALSE(texts.contains(QString::fromLatin1(removed), Qt::CaseInsensitive)) << removed;
  }
  viewer.window->close();
}

// REQ-F-002: keycap precedes label, both share a vertical centre, pairs sit tighter than hints.
TEST(FooterKeyHints, KeycapLabelPairingAndSpacing) {
  ViewerFixture viewer;
  ASSERT_TRUE(viewer.load());
  const auto rows = hintRows(viewer.footer);
  ASSERT_EQ(rows.size(), static_cast<qsizetype>(kHints.size()));
  const auto hintSpacing = QQmlProperty::read(viewer.footer, QStringLiteral("spacing")).toReal();
  for (auto* row : rows) {
    auto* keycap = part(row, "Keycap");
    auto* label = part(row, "Label");
    ASSERT_NE(keycap, nullptr);
    ASSERT_NE(label, nullptr);
    EXPECT_GT(label->x(), keycap->x()) << row->objectName().toStdString();
    EXPECT_LE(std::abs(sceneCenterY(keycap) - sceneCenterY(label)), 1.0) << row->objectName().toStdString();
    const auto pairSpacing = QQmlProperty::read(row, QStringLiteral("spacing")).toReal();
    EXPECT_GT(pairSpacing, 0);
    EXPECT_GT(hintSpacing, pairSpacing);
  }
  viewer.window->close();
}

// Narrow footer stays on one line, hides whole hints from the right, and always keeps Help.
TEST(FooterKeyHints, NarrowWidthHidesRightmostWholeHints) {
  StandaloneFooter standalone;
  ASSERT_TRUE(standalone.load(1000));
  auto* footer = standalone.footer.get();
  const auto visibleNames = [&] {
    QStringList names;
    for (auto* row : hintRows(footer)) {
      if (row->isVisible()) {
        names.append(row->objectName());
      }
    }
    return names;
  };
  EXPECT_EQ(visibleNames().size(), static_cast<qsizetype>(kHints.size()));
  qsizetype previous = kHints.size();
  for (int width = 1000; width >= 60; width -= 20) {
    footer->setWidth(width);
    QTest::qWait(50);
    const auto names = visibleNames();
    EXPECT_TRUE(names.contains(QStringLiteral("footerHintHelp"))) << width;
    EXPECT_LE(names.size(), previous) << width;
    previous = names.size();
    std::set<int> lines;
    for (auto* row : hintRows(footer)) {
      if (!row->isVisible()) {
        continue;
      }
      lines.insert(qRound(row->y()));
      if (width >= 180) {
        EXPECT_LE(row->mapToItem(footer, QPointF(row->width(), 0)).x(), width) << row->objectName().toStdString();
      }
    }
    EXPECT_EQ(lines.size(), 1U) << width;
    // Hidden hints form a suffix (before Help): visible names keep layout order.
    const QStringList expected{"footerHintNavigate",   "footerHintZoom",   "footerHintFit",
                               "footerHintActualSize", "footerHintRotate", "footerHintFullscreen"};
    QStringList withoutHelp = names;
    withoutHelp.removeAll(QStringLiteral("footerHintHelp"));
    EXPECT_EQ(withoutHelp, expected.mid(0, withoutHelp.size())) << width;
  }
  EXPECT_LT(previous, static_cast<qsizetype>(kHints.size()));
  footer->setWidth(1000);
  QTest::qWait(10);
  EXPECT_EQ(visibleNames().size(), static_cast<qsizetype>(kHints.size()));
}

// The footer content is centred horizontally.
TEST(FooterKeyHints, ContentIsCentred) {
  StandaloneFooter standalone;
  ASSERT_TRUE(standalone.load(1000));
  const auto rows = hintRows(standalone.footer.get());
  ASSERT_FALSE(rows.isEmpty());
  const auto left = rows.first()->mapToItem(standalone.footer.get(), QPointF(0, 0)).x();
  const auto right = rows.last()->mapToItem(standalone.footer.get(), QPointF(rows.last()->width(), 0)).x();
  EXPECT_NEAR(left, 1000 - right, 1.0);
}

// REQ-NF-001: the footer stays visible in empty, loaded, and idle-after-pointer states.
TEST(FooterKeyHints, PersistentVisibility) {
  ViewerFixture viewer;
  ASSERT_TRUE(viewer.load());
  EXPECT_TRUE(viewer.footer->isVisible());
  EXPECT_DOUBLE_EQ(viewer.footer->opacity(), 1.0);
  auto* arrows = viewer.window->findChild<QObject*>(QStringLiteral("arrowTimer"));
  auto* details = viewer.window->findChild<QObject*>(QStringLiteral("detailsTimer"));
  ASSERT_NE(arrows, nullptr);
  ASSERT_NE(details, nullptr);
  // Shorter intervals exercise the overlay timers without a six-second suite delay.
  arrows->setProperty("interval", 200);
  details->setProperty("interval", 200);
  viewer.document.open({QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/sample.png")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return viewer.document.state() == ImageDocument::Ready; }));
  EXPECT_TRUE(viewer.footer->isVisible());
  QTest::mouseMove(viewer.window, QPoint(40, 40));
  QTest::mouseMove(viewer.window, QPoint(60, 60));
  auto* next = viewer.window->findChild<QQuickItem*>(QStringLiteral("nextButton"));
  auto* strip = viewer.window->findChild<QQuickItem*>(QStringLiteral("detailsStrip"));
  ASSERT_TRUE(next && strip);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !next->isVisible() && !strip->isVisible(); }));
  QTest::qWait(100);
  EXPECT_TRUE(viewer.footer->isVisible());
  EXPECT_DOUBLE_EQ(viewer.footer->opacity(), 1.0);
  for (auto* row : hintRows(viewer.footer)) {
    EXPECT_TRUE(row->isVisible()) << row->objectName().toStdString();
  }
  viewer.window->close();
}

// REQ-NF-002: one StaticText node per hint, named "<key> <label>"; parts are not exposed.
TEST(FooterKeyHints, AccessibleNaming) {
  QAccessible::setActive(true);
  ViewerFixture viewer;
  ASSERT_TRUE(viewer.load());
  const auto rows = hintRows(viewer.footer);
  ASSERT_EQ(rows.size(), static_cast<qsizetype>(kHints.size()));
  for (std::size_t index = 0; index < kHints.size(); ++index) {
    const auto& hint = kHints.at(index);
    auto* row = rows.at(static_cast<qsizetype>(index));
    auto* accessible = QAccessible::queryAccessibleInterface(row);
    ASSERT_NE(accessible, nullptr) << hint.name;
    EXPECT_EQ(accessible->role(), QAccessible::StaticText) << hint.name;
    EXPECT_EQ(accessible->text(QAccessible::Name),
              QString::fromUtf8(hint.key) + QLatin1Char(' ') + QString::fromUtf8(hint.label));
  }
  QStringList names;
  collectAccessibleNames(QAccessible::queryAccessibleInterface(viewer.window), viewer.footer, names);
  EXPECT_EQ(names.size(), static_cast<qsizetype>(kHints.size())) << names.join(" | ").toStdString();
  for (const auto& hint : kHints) {
    EXPECT_FALSE(names.contains(QString::fromUtf8(hint.key))) << hint.key;
    EXPECT_FALSE(names.contains(QString::fromUtf8(hint.label))) << hint.label;
  }
  viewer.window->close();
}

// REQ-NF-003: no footer item accepts keyboard focus; Tab never lands in the footer.
TEST(FooterKeyHints, NotFocusable) {
  ViewerFixture viewer;
  ASSERT_TRUE(viewer.load());
  viewer.window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(viewer.window));
  const std::function<void(QQuickItem*)> check = [&](QQuickItem* item) {
    EXPECT_FALSE(item->activeFocusOnTab()) << item->objectName().toStdString();
    const auto policy = item->property("focusPolicy");
    if (policy.isValid()) {
      EXPECT_EQ(policy.toInt(), static_cast<int>(Qt::NoFocus)) << item->objectName().toStdString();
    }
    for (auto* child : item->childItems()) {
      check(child);
    }
  };
  check(viewer.footer);
  for (int step = 0; step < 8; ++step) {
    QTest::keyClick(viewer.window, step % 2 == 0 ? Qt::Key_Tab : Qt::Key_Backtab);
    auto* focused = viewer.window->activeFocusItem();
    EXPECT_FALSE(focused != nullptr && viewer.footer->isAncestorOf(focused));
  }
  viewer.window->close();
}

// Captures for scripts/check-visual.sh: full window at minimum and default size, footer at 400 px.
TEST(FooterKeyHints, VisualCapture) {
  const auto capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (capture.isEmpty()) {
    GTEST_SKIP() << "VIEWER_CAPTURE_PREFIX not set";
  }
  {
    ViewerFixture viewer;
    ASSERT_TRUE(viewer.load());
    for (const auto size : {QSize(420, 280), QSize(1000, 700)}) {
      viewer.window->resize(size);
      QTest::qWait(150);
      EXPECT_TRUE(viewer.window->grabWindow().save(capture + QStringLiteral("-footer-%1.png").arg(size.width())));
    }
    viewer.window->close();
  }
  StandaloneFooter standalone;
  ASSERT_TRUE(standalone.load(400));
  EXPECT_TRUE(standalone.window.grabWindow().save(capture + QStringLiteral("-footer-standalone-400.png")));
}

TEST(FooterKeyHints, TracksDescriptionPointAndPixelSizesWithMonospaceFamily) {
  StandaloneFooter standalone;
  ASSERT_TRUE(standalone.load(1000));
  for (auto* row : hintRows(standalone.footer.get())) {
    auto* keycap = part(row, "Keycap");
    auto* label = part(row, "Label");
    ASSERT_NE(keycap, nullptr);
    ASSERT_NE(label, nullptr);
    const auto family = keycap->property("font").value<QFont>().family();
    EXPECT_EQ(QFontInfo(keycap->property("font").value<QFont>()).pixelSize(),
              QFontInfo(label->property("font").value<QFont>()).pixelSize());
    qreal smallHeight = 0;
    for (int size : {8, 18, -32, 12}) {
      QFont description(QStringLiteral("DejaVu Sans"));
      if (size > 0) {
        description.setPointSize(size);
      } else {
        description.setPixelSize(-size);
      }
      ASSERT_TRUE(label->setProperty("font", description));
      const auto badgeFont = keycap->property("font").value<QFont>();
      EXPECT_EQ(badgeFont.family(), family);
      EXPECT_EQ(QFontInfo(badgeFont).pixelSize(), QFontInfo(description).pixelSize());
      if (size == 8) {
        smallHeight = keycap->implicitHeight();
      } else {
        EXPECT_GT(keycap->implicitHeight(), smallHeight);
      }
    }
  }
}

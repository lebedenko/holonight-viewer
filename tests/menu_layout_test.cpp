#include "image_document.h"

#include <QAccessible>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <string_view>

namespace {
struct Entry {
  std::string_view label;  // empty marks a separator
  std::string_view shortcut;
  [[nodiscard]] bool separator() const { return label.empty(); }
};

// REQ-F-005: order, separators, labels and shortcut spellings.
constexpr std::array kMenu = {
    Entry{.label = "Open…", .shortcut = "Ctrl+O"},
    Entry{.label = "Refresh", .shortcut = "Ctrl+R"},
    Entry{},
    Entry{.label = "Previous", .shortcut = "["},
    Entry{.label = "Next", .shortcut = "]"},
    Entry{},
    Entry{.label = "Fit", .shortcut = "Ctrl+0"},
    Entry{.label = "Actual Size", .shortcut = "1"},
    Entry{.label = "Zoom In", .shortcut = "Ctrl++"},
    Entry{.label = "Zoom Out", .shortcut = "Ctrl+−"},
    Entry{},
    Entry{.label = "Rotate Clockwise", .shortcut = "R"},
    Entry{.label = "Rotate Counterclockwise", .shortcut = "Shift+R"},
    Entry{.label = "Flip Horizontally", .shortcut = "X"},
    Entry{.label = "Flip Vertically", .shortcut = "Shift+X"},
    Entry{.label = "Reset Transform", .shortcut = ""},
    Entry{},
    Entry{.label = "Copy Image", .shortcut = "Ctrl+C"},
    Entry{.label = "Copy Path", .shortcut = "Ctrl+Shift+C"},
    Entry{},
    Entry{.label = "Image Information", .shortcut = "I"},
    Entry{.label = "Shortcut Help", .shortcut = "?"},
    Entry{},
    Entry{.label = "Fullscreen", .shortcut = "F"},
    Entry{.label = "Quit", .shortcut = "Q"},
};
constexpr int kEntries = kMenu.size();

QString text(std::string_view view) { return QString::fromUtf8(view.data(), static_cast<qsizetype>(view.size())); }

struct MenuFixture {
  ImageDocument document;
  QQmlApplicationEngine engine;
  QQuickWindow* window = nullptr;
  QObject* menu = nullptr;

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
    menu = window->findChild<QObject*>("actionsMenu");
    if (menu == nullptr) {
      return ::testing::AssertionFailure() << "actionsMenu not found";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] ::testing::AssertionResult openMenu() const {
    auto* button = window->findChild<QQuickItem*>("actionsButton");
    if (button == nullptr) {
      return ::testing::AssertionFailure() << "actionsButton not found";
    }
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      button->mapToScene({button->width() / 2, button->height() / 2}).toPoint());
    if (!QTest::qWaitFor([&] { return menu->property("opened").toBool(); })) {
      return ::testing::AssertionFailure() << "menu did not open";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] QQuickItem* entry(int index) const {
    QQuickItem* item = nullptr;
    QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, index));
    return item;
  }

  [[nodiscard]] int count() const { return menu->property("count").toInt(); }
  [[nodiscard]] int current() const { return menu->property("currentIndex").toInt(); }
};

bool isSeparator(const QQuickItem* item) { return item != nullptr && item->inherits("QQuickMenuSeparator"); }
bool isMenuItem(const QQuickItem* item) { return item != nullptr && item->inherits("QQuickMenuItem"); }

qreal textRight(QQuickItem* label, QQuickItem* target) {
  return label->mapToItem(target, {std::min(label->implicitWidth(), label->width()), 0}).x();
}

void expectHairline(const QImage& capture, QQuickItem* content, qreal dpr, const QColor& color) {
  const auto origin = content->mapToScene({0, 0});
  const int top = qFloor((origin.y() - 3) * dpr);
  const int bottom = qCeil((origin.y() + content->height() + 3) * dpr);
  ASSERT_GE(top, 0);
  ASSERT_LT(bottom, capture.height());
  for (const qreal fraction : {0.25, 0.5, 0.75}) {
    const int column = qRound((origin.x() + (content->width() * fraction)) * dpr);
    ASSERT_GE(column, 0);
    ASSERT_LT(column, capture.width());
    const auto background = capture.pixelColor(column, top);
    ASSERT_NE(background, color);
    int rows = 0;
    int paintedRows = 0;
    for (int row = top; row <= bottom; ++row) {
      const auto pixel = capture.pixelColor(column, row);
      const bool matches = std::abs(pixel.red() - color.red()) <= 1 && std::abs(pixel.green() - color.green()) <= 1 &&
                           std::abs(pixel.blue() - color.blue()) <= 1;
      rows += matches ? 1 : 0;
      paintedRows += capture.pixelColor(column, row) != background ? 1 : 0;
    }
    EXPECT_EQ(paintedRows, 1) << "Extra solid or blended rows";
    EXPECT_EQ(rows, 1) << "sample=" << fraction;
  }
}
}  // namespace

TEST(MenuLayout, OrderLabelsShortcutsAndDisabledColors) {
  MenuFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openMenu());
  ASSERT_EQ(fixture.count(), kEntries);
  int commands = 0;
  int separators = 0;
  for (int index = 0; index < kEntries; ++index) {
    auto* item = fixture.entry(index);
    ASSERT_NE(item, nullptr) << index;
    const auto& expected = kMenu.at(index);
    if (expected.separator()) {
      EXPECT_TRUE(isSeparator(item)) << index;
      ++separators;
      continue;
    }
    ASSERT_TRUE(isMenuItem(item)) << index;
    ++commands;
    EXPECT_EQ(item->property("text").toString(), text(expected.label)) << index;
    EXPECT_EQ(item->property("shortcutText").toString(), text(expected.shortcut)) << index;
    auto* shortcut = item->findChild<QQuickItem*>("menuItemShortcut");
    ASSERT_NE(shortcut, nullptr) << index;
    EXPECT_EQ(shortcut->isVisible(), !expected.shortcut.empty()) << index;
  }
  EXPECT_EQ(commands, 19);
  EXPECT_EQ(separators, 6);
  EXPECT_FALSE(isSeparator(fixture.entry(0)));
  EXPECT_FALSE(isSeparator(fixture.entry(kEntries - 1)));
  for (int index = 1; index < kEntries; ++index) {
    EXPECT_FALSE(isSeparator(fixture.entry(index - 1)) && isSeparator(fixture.entry(index))) << index;
  }

  auto* palette = fixture.engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  const auto disabled = palette->property("textDisabled").value<QColor>();
  auto* fit = fixture.window->findChild<QQuickItem*>("fitButton");
  ASSERT_NE(fit, nullptr);
  EXPECT_FALSE(fit->isEnabled());
  for (const auto* name : {"menuItemLabel", "menuItemShortcut"}) {
    auto* label = fit->findChild<QQuickItem*>(name);
    ASSERT_NE(label, nullptr) << name;
    EXPECT_EQ(label->property("color").value<QColor>(), disabled) << name;
  }
  auto* open = fixture.window->findChild<QQuickItem*>("openButton");
  ASSERT_NE(open, nullptr);
  EXPECT_EQ(open->findChild<QQuickItem*>("menuItemShortcut")->property("color").value<QColor>(),
            palette->property("textMuted").value<QColor>());

  // Separators must not surface as actionable nodes.
  QAccessible::setActive(true);
  for (int index = 0; index < kEntries; ++index) {
    if (!kMenu.at(index).separator()) {
      continue;
    }
    if (auto* accessible = QAccessible::queryAccessibleInterface(fixture.entry(index))) {
      EXPECT_NE(accessible->role(), QAccessible::MenuItem) << index;
      EXPECT_NE(accessible->role(), QAccessible::Button) << index;
    }
  }
}

TEST(MenuLayout, ShortcutColumnGeometry) {
  MenuFixture fixture;
  ASSERT_TRUE(fixture.load());
  for (const auto size : {QSize(1000, 700), QSize(420, 280)}) {
    fixture.window->resize(size);
    ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.window->size() == size; }));
    ASSERT_TRUE(fixture.openMenu());
    auto* list = fixture.window->findChild<QQuickItem*>("actionsMenuList");
    ASSERT_NE(list, nullptr);
    const bool scrollable = list->property("interactive").toBool();
    EXPECT_EQ(scrollable, size.height() < 700) << size.height();
    for (int index = 0; index < kEntries; ++index) {
      auto* item = fixture.entry(index);
      if (!isMenuItem(item) || item->property("shortcutText").toString().isEmpty()) {
        continue;
      }
      auto* label = item->findChild<QQuickItem*>("menuItemLabel");
      auto* shortcut = item->findChild<QQuickItem*>("menuItemShortcut");
      ASSERT_TRUE(label && shortcut) << index;
      const qreal contentRight = item->width() - item->property("rightPadding").toReal();
      const qreal shortcutRight = shortcut->mapToItem(item, {shortcut->width(), 0}).x();
      EXPECT_LE(shortcutRight, contentRight + 0.5) << index;
      EXPECT_GE(shortcutRight, contentRight - item->property("padding").toReal()) << index;
      EXPECT_GT(shortcut->mapToItem(item, {0, 0}).x(), textRight(label, item)) << index;
      if (scrollable) {
        // The overlay scroll bar sits at the list's right edge; the shortcut must stay left of it.
        const qreal barLeft = list->width() - list->property("scrollBarReserve").toReal();
        EXPECT_GT(list->property("scrollBarReserve").toReal(), 0);
        EXPECT_LE(shortcut->mapToItem(list, {shortcut->width(), 0}).x(), barLeft) << index;
      }
    }
    QTest::keyClick(fixture.window, Qt::Key_Escape);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.menu->property("visible").toBool(); }));
  }
}

TEST(MenuLayout, KeyboardNavigationSkipsSeparators) {
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/menu-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  QImage image(4, 3, QImage::Format_RGB32);
  image.fill(Qt::darkCyan);
  for (const auto* name : {"a.png", "b.png", "c.png"}) {
    ASSERT_TRUE(image.save(dir.filePath(QString::fromLatin1(name))));
  }
  MenuFixture fixture;
  ASSERT_TRUE(fixture.load());
  fixture.document.open({QUrl::fromLocalFile(dir.filePath("b.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] {
    return fixture.document.state() == ImageDocument::Ready && fixture.document.canPrevious() &&
           fixture.document.canNext();
  }));
  ASSERT_TRUE(fixture.openMenu());

  const auto labelAt = [&](int index) {
    auto* item = fixture.entry(index);
    return item ? item->property("text").toString() : QString();
  };
  for (const auto [down, upward] : {std::pair{Qt::Key_Down, Qt::Key_Up}, std::pair{Qt::Key_J, Qt::Key_K}}) {
    fixture.menu->setProperty("currentIndex", 1);
    ASSERT_EQ(labelAt(fixture.current()), "Refresh");
    QTest::keyClick(fixture.window, down);
    EXPECT_EQ(labelAt(fixture.current()), "Previous");
    QTest::keyClick(fixture.window, upward);
    EXPECT_EQ(labelAt(fixture.current()), "Refresh");
  }

  fixture.menu->setProperty("currentIndex", 0);
  QStringList visited{labelAt(0)};
  for (int step = 0; step < 18; ++step) {
    QTest::keyClick(fixture.window, Qt::Key_Down);
    ASSERT_FALSE(isSeparator(fixture.entry(fixture.current()))) << step;
    visited.append(labelAt(fixture.current()));
  }
  QStringList expected;
  for (const auto& entry : kMenu) {
    if (!entry.separator()) {
      expected.append(text(entry.label));
    }
  }
  EXPECT_EQ(visited, expected);
}

TEST(MenuLayout, SeparatorsRenderAsPhysicalHairlines) {
  MenuFixture fixture;
  ASSERT_TRUE(fixture.load());
  auto* palette = fixture.engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  const auto color = palette->property("borderPassive").value<QColor>();
  ASSERT_EQ(color.alpha(), 255);
  auto* list = fixture.window->findChild<QQuickItem*>("actionsMenuList");
  ASSERT_NE(list, nullptr);
  const auto capturePrefix = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  for (const bool scroll : {false, true}) {
    fixture.window->resize(1000, scroll ? 400 : 1200);
    ASSERT_TRUE(fixture.openMenu());
    EXPECT_EQ(list->property("interactive").toBool(), scroll);
    const auto menuY = fixture.menu->property("y").toReal();
    for (const qreal offset : {0.0, 0.25, 0.5, 0.75}) {
      ASSERT_TRUE(fixture.menu->setProperty("y", menuY + offset));
      int checked = 0;
      for (int index = 0; index < kEntries; ++index) {
        if (!kMenu.at(index).separator()) {
          continue;
        }
        auto* item = fixture.entry(index);
        ASSERT_NE(item, nullptr);
        auto* content = item->property("contentItem").value<QQuickItem*>();
        ASSERT_NE(content, nullptr);
        if (scroll) {
          // Center each separator, then move through subpixel scroll positions.
          const auto position = item->mapToItem(list, {0, 0}).y() + list->property("contentY").toReal();
          const auto maximum = list->property("contentHeight").toReal() - list->height();
          ASSERT_GT(maximum, 0);
          ASSERT_TRUE(list->setProperty("contentY", std::clamp(position - list->height() / 2, 0.0, maximum) + offset));
        }
        // Allow the scene-position observer and render loop to settle after movement.
        QTest::qWait(50);
        const qreal dpr = fixture.window->devicePixelRatio();
        const auto grab = fixture.window->contentItem()->grabToImage(fixture.window->size());
        ASSERT_FALSE(grab.isNull());
        ASSERT_TRUE(QTest::qWaitFor([&] { return !grab->image().isNull(); }));
        const auto capture = grab->image();
        ASSERT_FALSE(capture.isNull());
        ASSERT_EQ(capture.width(), qRound(fixture.window->width() * dpr));
        SCOPED_TRACE(::testing::Message()
                     << "separator=" << index << " dpr=" << dpr << " offset=" << offset << " scroll=" << scroll);
        ASSERT_NO_FATAL_FAILURE(expectHairline(capture, content, dpr, color));
        if (!capturePrefix.isEmpty() && offset == 0.5 && (!scroll || index == 16)) {
          ASSERT_TRUE(capture.save(capturePrefix + (scroll ? "-menu-scrolled.png" : "-menu.png")));
        }
        ++checked;
      }
      EXPECT_EQ(checked, 6);
    }
    QTest::keyClick(fixture.window, Qt::Key_Escape);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.menu->property("visible").toBool(); }));
  }
}

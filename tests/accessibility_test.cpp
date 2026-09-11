#include "image_canvas.h"
#include "image_document.h"

#include <QAccessible>
#include <QClipboard>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QTest>

#include <gtest/gtest.h>

namespace {
QStringList& announcements() {
  static QStringList messages;
  return messages;
}
void recordEvent(QAccessibleEvent* event) {
  if (event->type() == QAccessible::Announcement) {
    announcements().append(dynamic_cast<QAccessibleAnnouncementEvent*>(event)->message());
  }
}
}  // namespace

TEST(Accessibility, NamesRolesEnabledFocusAndDialogs) {
  announcements().clear();
  const auto previousHandler = QAccessible::installUpdateHandler(recordEvent);
  const auto restore = qScopeGuard([&] { QAccessible::installUpdateHandler(previousHandler); });
  QAccessible::setActive(true);
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  for (const auto* name :
       {"actionsButton", "informationButton", "fullscreenButton", "previousButton", "nextButton", "imageCanvas"}) {
    auto* item = window->findChild<QQuickItem*>(QString::fromLatin1(name));
    ASSERT_NE(item, nullptr) << name;
    auto* accessible = QAccessible::queryAccessibleInterface(item);
    ASSERT_NE(accessible, nullptr) << name;
    EXPECT_FALSE(accessible->text(QAccessible::Name).isEmpty()) << name;
    EXPECT_EQ(accessible->role(), QByteArray(name) == "imageCanvas" ? QAccessible::Graphic : QAccessible::Button);
    EXPECT_EQ(accessible->state().disabled, !item->isEnabled());
  }
  auto* canvas = window->findChild<ImageCanvas*>(QStringLiteral("imageCanvas"));
  ASSERT_NE(canvas, nullptr);
  canvas->forceActiveFocus(Qt::TabFocusReason);
  EXPECT_TRUE(QAccessible::queryAccessibleInterface(canvas)->state().focused);
  QTest::keyClick(window, Qt::Key_Tab);
  EXPECT_NE(window->activeFocusItem(), canvas);
  auto* actions = window->findChild<QQuickItem*>("actionsButton");
  ASSERT_NE(actions, nullptr);
  actions->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(window, Qt::Key_Space);
  auto* menu = window->findChild<QObject*>("actionsMenu");
  ASSERT_NE(menu, nullptr);
  ASSERT_TRUE(QTest::qWaitFor([&] { return menu->property("visible").toBool(); }));
  auto* information = QAccessible::queryAccessibleInterface(window->findChild<QObject*>("informationMenuItem"));
  ASSERT_NE(information, nullptr);
  EXPECT_EQ(information->role(), QAccessible::MenuItem);
  EXPECT_EQ(information->text(QAccessible::Name), QStringLiteral("Image Information"));
  EXPECT_TRUE(information->state().disabled);
  auto* disabledItem = window->findChild<QQuickItem*>("informationMenuItem");
  auto* disabledBackground = disabledItem->property("background").value<QQuickItem*>();
  ASSERT_NE(disabledBackground, nullptr);
  EXPECT_EQ(QQmlProperty::read(disabledBackground, "border.width").toDouble(), 0);
  QTest::keyClick(window, Qt::Key_Escape);
  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->findChild<QQuickItem*>("detailsText") != nullptr; }));
  auto* text = window->findChild<QQuickItem*>("detailsText");
  auto* accessible = QAccessible::queryAccessibleInterface(text);
  ASSERT_NE(accessible, nullptr);
  EXPECT_EQ(accessible->text(QAccessible::Name), QStringLiteral("Shortcut Help"));
  EXPECT_TRUE(text->property("readOnly").toBool());
  EXPECT_TRUE(text->property("selectByMouse").toBool());
  text->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
  QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
  EXPECT_EQ(QGuiApplication::clipboard()->text(), text->property("text").toString());
  auto* closeButton = window->findChild<QQuickItem*>("closeDetailsButton");
  ASSERT_NE(closeButton, nullptr);
  ASSERT_TRUE(closeButton->isVisible());
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                    closeButton->mapToScene(QPointF(closeButton->width() / 2, closeButton->height() / 2)).toPoint());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
  EXPECT_TRUE(canvas->hasActiveFocus());
  announcements().clear();
  document.open({QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/sample.png")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  ASSERT_EQ(announcements().size(), 1);
  EXPECT_TRUE(announcements().last().contains("Loaded sample.png"));
  document.transform(1);
  canvas->zoomSteps(1, QPointF(10, 10));
  EXPECT_EQ(announcements().size(), 1);
  document.copyPath();
  ASSERT_EQ(announcements().size(), 2);
  EXPECT_TRUE(announcements().last().contains("Copied path"));
  document.copyImage();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  ASSERT_EQ(announcements().size(), 3);
  EXPECT_TRUE(announcements().last().contains("Copied sample.png"));
  document.open({QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/missing.png")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Error; }));
  ASSERT_EQ(announcements().size(), 4);
  EXPECT_TRUE(announcements().last().contains("Error opening"));
  window->close();
}

// Exercise real key transitions and pixels: focus state alone missed this regression.
TEST(Accessibility, VisibleKeyboardFocus) {
  ImageDocument document;
  document.open({QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/sample.png")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  auto* canvas = window->findChild<QQuickItem*>("imageCanvas");
  auto* actions = window->findChild<QQuickItem*>("actionsButton");
  ASSERT_NE(canvas, nullptr);
  ASSERT_NE(actions, nullptr);
  auto background = [](QQuickItem* item) { return item->property("background").value<QQuickItem*>(); };
  auto capture = [&](const QString& label) {
    QTest::qWait(30);
    const auto pixels = window->grabWindow();
    EXPECT_FALSE(pixels.isNull());
    const auto prefix = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
    if (!prefix.isEmpty()) {
      EXPECT_TRUE(pixels.save(prefix + "-keyboard-" + QString::number(window->width()) + "-" + label + ".png"));
    }
    return pixels;
  };
  auto borderPixel = [&](QQuickItem* item) {
    const auto pixels = capture("current");
    const auto point = item->mapToScene(QPointF(item->width() / 2, 0));
    const auto ratio = static_cast<qreal>(pixels.width()) / window->width();
    return pixels.pixelColor(qRound(point.x() * ratio), qRound(point.y() * ratio));
  };
  auto expectBorder = [&](QQuickItem* item) {
    ASSERT_NE(item, nullptr);
    const auto color = QQmlProperty::read(item, "border.color").value<QColor>();
    EXPECT_GT(QQmlProperty::read(item, "border.width").toDouble(), 0);
    const auto rendered = borderPixel(item);
    // Native surfaces may quantize channels (e.g. RGB565); retain a tight tolerance.
    EXPECT_NEAR(rendered.red(), color.red(), 5);
    EXPECT_NEAR(rendered.green(), color.green(), 5);
    EXPECT_NEAR(rendered.blue(), color.blue(), 5);
  };
  auto tabTo = [&](QQuickItem* item, bool backwards = false) {
    for (int step = 0; step < 30; ++step) {
      QTest::keyClick(window, Qt::Key_Tab, backwards ? Qt::ShiftModifier : Qt::NoModifier);
      if (item->hasActiveFocus()) {
        return true;
      }
    }
    return false;
  };
  for (const QSize size : {QSize(1000, 700), QSize(420, 280)}) {
    window->resize(size);
    ASSERT_TRUE(tabTo(canvas));
    auto* outline = window->findChild<QQuickItem*>("canvasFocusOutline");
    ASSERT_NE(outline, nullptr);
    EXPECT_TRUE(outline->isVisible());
    expectBorder(outline);
    capture("canvas");
    ASSERT_TRUE(tabTo(actions));
    EXPECT_FALSE(outline->isVisible());
    EXPECT_TRUE(actions->property("visualFocus").toBool());
    expectBorder(background(actions));
    capture("button");
    const auto focusedColor = borderPixel(background(actions));
    QTest::keyClick(window, Qt::Key_Tab);
    EXPECT_FALSE(actions->property("visualFocus").toBool());
    EXPECT_NE(borderPixel(background(actions)), focusedColor);
    QTest::keyClick(window, Qt::Key_Tab, Qt::ShiftModifier);
    ASSERT_TRUE(actions->hasActiveFocus());
    expectBorder(background(actions));
    ASSERT_TRUE(tabTo(canvas, true));
    expectBorder(outline);
    ASSERT_TRUE(tabTo(actions));
    QTest::keyClick(window, Qt::Key_Space);
    auto* menu = window->findChild<QObject*>("actionsMenu");
    ASSERT_NE(menu, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu->property("visible").toBool(); }));
    QTest::keyClick(window, Qt::Key_Down);
    auto* first = window->activeFocusItem();
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(first->property("highlighted").toBool());
    auto* firstBackground = background(first);
    ASSERT_NE(firstBackground, nullptr);
    expectBorder(firstBackground);
    const auto selected = borderPixel(firstBackground);
    capture("menu");
    QTest::keyClick(window, Qt::Key_Down);
    EXPECT_FALSE(first->property("highlighted").toBool());
    EXPECT_NE(borderPixel(firstBackground), selected);
    QTest::keyClick(window, Qt::Key_Up);
    EXPECT_EQ(window->activeFocusItem(), first);
    expectBorder(firstBackground);
    QTest::keyClick(window, Qt::Key_Escape);
    for (const auto key : {Qt::Key_Question, Qt::Key_I}) {
      QTest::keyClick(window, key);
      ASSERT_TRUE(QTest::qWaitFor([&] { return window->findChild<QQuickItem*>("detailsText") != nullptr; }));
      auto* text = window->findChild<QQuickItem*>("detailsText");
      auto* close = window->findChild<QQuickItem*>("closeDetailsButton");
      ASSERT_NE(close, nullptr);
      ASSERT_TRUE(tabTo(text));
      expectBorder(background(text));
      capture(key == Qt::Key_Question ? "help-text" : "information-text");
      ASSERT_TRUE(tabTo(close));
      EXPECT_FALSE(text->hasActiveFocus());
      EXPECT_NE(QQmlProperty::read(background(text), "border.color").value<QColor>(), focusedColor);
      expectBorder(background(close));
      capture("close");
      ASSERT_TRUE(tabTo(text, true));
      expectBorder(background(text));
      ASSERT_TRUE(tabTo(close));
      QTest::keyClick(window, Qt::Key_Space);
      ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
      EXPECT_TRUE(canvas->hasActiveFocus());
      expectBorder(outline);
      QTest::keyClick(window, key);
      ASSERT_TRUE(QTest::qWaitFor([&] { return window->property("modalActive").toBool(); }));
      QTest::keyClick(window, Qt::Key_Escape);
      ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
      EXPECT_TRUE(canvas->hasActiveFocus());
    }
  }
  window->close();
}

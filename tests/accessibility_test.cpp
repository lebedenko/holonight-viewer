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
  EXPECT_FALSE(canvas->activeFocusOnTab());
  EXPECT_FALSE(QAccessible::queryAccessibleInterface(canvas)->state().focused);
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
  QTest::keyClick(window, Qt::Key_J);
  EXPECT_EQ(menu->property("currentIndex").toInt(), 0);
  QTest::keyClick(window, Qt::Key_J);
  EXPECT_EQ(menu->property("currentIndex").toInt(), 8);
  QTest::keyClick(window, Qt::Key_K);
  EXPECT_EQ(menu->property("currentIndex").toInt(), 0);
  QTest::keyClick(window, Qt::Key_Down);
  EXPECT_EQ(menu->property("currentIndex").toInt(), 8);
  QTest::keyClick(window, Qt::Key_Up);
  EXPECT_EQ(menu->property("currentIndex").toInt(), 0);
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
  EXPECT_FALSE(canvas->hasActiveFocus());
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

// Focus is restricted to enabled header controls; image actions clear its ring.
TEST(Accessibility, VisibleKeyboardFocus) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  auto* canvas = window->findChild<QQuickItem*>("imageCanvas");
  auto* neutral = window->findChild<QQuickItem*>("neutralFocus");
  auto* information = window->findChild<QQuickItem*>("informationButton");
  auto* fullscreen = window->findChild<QQuickItem*>("fullscreenButton");
  auto* actions = window->findChild<QQuickItem*>("actionsButton");
  ASSERT_NE(canvas, nullptr);
  ASSERT_NE(neutral, nullptr);
  ASSERT_NE(information, nullptr);
  ASSERT_NE(fullscreen, nullptr);
  ASSERT_NE(actions, nullptr);
  EXPECT_FALSE(canvas->activeFocusOnTab());
  EXPECT_EQ(window->findChild<QQuickItem*>("canvasFocusOutline"), nullptr);
  EXPECT_TRUE(neutral->hasActiveFocus());
  EXPECT_EQ(window->focusObject(), neutral);
  auto* router = window->findChild<QObject*>("windowKeyRouter");
  ASSERT_NE(router, nullptr);
  EXPECT_EQ(router->property("target").value<QQuickWindow*>(), window);
  EXPECT_FALSE(information->isEnabled());
  auto capture = [&](const QString& label) {
    QTest::qWait(30);
    const auto pixels = window->grabWindow();
    EXPECT_FALSE(pixels.isNull());
    const auto prefix = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
    if (!prefix.isEmpty()) {
      EXPECT_TRUE(pixels.save(prefix + "-header-" + label + ".png"));
    }
    return pixels;
  };
  auto verifyRing = [&](QQuickItem* button, const QString& label) {
    auto* background = button->property("background").value<QQuickItem*>();
    ASSERT_NE(background, nullptr);
    EXPECT_GT(QQmlProperty::read(background, "border.width").toDouble(), 0);
    const auto pixels = capture(label);
    const auto point = background->mapToScene(QPointF(background->width() / 2, 0));
    const auto ratio = static_cast<qreal>(pixels.width()) / window->width();
    const auto rendered = pixels.pixelColor(qRound(point.x() * ratio), qRound(point.y() * ratio));
    const auto expected = QQmlProperty::read(background, "border.color").value<QColor>();
    EXPECT_NEAR(rendered.red(), expected.red(), 5);
    EXPECT_NEAR(rendered.green(), expected.green(), 5);
    EXPECT_NEAR(rendered.blue(), expected.blue(), 5);
  };
  capture("empty-neutral");
  auto tab = [&](QQuickItem* expected, bool reverse = false) {
    QTest::keyClick(window, Qt::Key_Tab, reverse ? Qt::ShiftModifier : Qt::NoModifier);
    EXPECT_EQ(window->activeFocusItem(), expected) << window->activeFocusItem()->objectName().toStdString();
    EXPECT_TRUE(expected->property("visualFocus").toBool());
    EXPECT_FALSE(canvas->hasActiveFocus());
  };
  tab(fullscreen);
  verifyRing(fullscreen, "empty-fullscreen");
  tab(actions);
  verifyRing(actions, "empty-actions");
  tab(fullscreen);
  tab(actions, true);
  tab(fullscreen, true);
  tab(actions, true);
  document.open({QUrl::fromLocalFile(QStringLiteral(RELEASE_FIXTURE_DIR) + "/sample.png")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  EXPECT_TRUE(information->isEnabled());
  tab(information);
  verifyRing(information, "ready-information");
  tab(fullscreen);
  tab(actions);
  tab(information);
  tab(actions, true);
  tab(fullscreen, true);
  tab(information, true);
  QTest::keyClick(window, Qt::Key_1);
  EXPECT_TRUE(neutral->hasActiveFocus());
  EXPECT_FALSE(information->property("visualFocus").toBool());
  capture("ready-neutral");
  tab(information);
  verifyRing(information, "ready-return");
  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->property("modalActive").toBool(); }));
  QTest::keyClick(window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
  EXPECT_TRUE(neutral->hasActiveFocus());
  tab(information);
  window->close();
}

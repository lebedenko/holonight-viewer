#include "image_canvas.h"
#include "image_document.h"

#include <QAccessible>
#include <QClipboard>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
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
  for (const auto* name : {"openButton", "actionsButton", "fitButton", "actualSizeButton", "zoomOutButton",
                           "zoomInButton", "previousButton", "nextButton", "imageCanvas"}) {
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
  QTest::keyClick(window, Qt::Key_Escape);
  QTest::keyClick(window, Qt::Key_F1);
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
  QTest::keyClick(window, Qt::Key_Escape);
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

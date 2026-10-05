#include "image_document.h"

#include <QQmlApplicationEngine>
#include <QQmlListReference>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <gtest/gtest.h>

TEST(ViewerHeader, HiddenTitlesPreserveGeometryActionsNavigationAndMenuAnchor) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  QObject* decoration = nullptr;
  for (auto* child : window->findChildren<QObject*>()) {
    if (child->inherits("HnWindowDecoration")) {
      decoration = child;
      break;
    }
  }
  ASSERT_NE(decoration, nullptr);
  EXPECT_EQ(decoration->property("window").value<QObject*>(), window);
  EXPECT_FALSE(decoration->property("externalDecorationPresent").toBool());
  auto* header = window->findChild<QQuickItem*>("viewerHeader");
  auto* title = window->findChild<QQuickItem*>("headerTitle");
  auto* information = window->findChild<QQuickItem*>("informationButton");
  auto* fullscreen = window->findChild<QQuickItem*>("fullscreenButton");
  auto* menu = window->findChild<QQuickItem*>("actionsButton");
  ASSERT_NE(header, nullptr);
  ASSERT_NE(title, nullptr);
  ASSERT_NE(information, nullptr);
  ASSERT_NE(fullscreen, nullptr);
  ASSERT_NE(menu, nullptr);
  ASSERT_TRUE(QTest::qWaitFor([&] { return header->height() > 0 && header->width() > 900 && menu->width() > 0; }));
  EXPECT_TRUE(header->property("titleVisible").toBool());
  const auto height = header->height();
  const auto menuPosition = menu->mapToItem(header, QPointF());
  const auto nativeTitle = window->title();
  const auto anchor = header->property("menuAnchor");
  ASSERT_EQ(anchor.value<QQuickItem*>(), menu);
  const auto targets = header->property("focusTargets");
  const QQmlListReference focusTargets(header, "focusTargets");
  ASSERT_EQ(focusTargets.count(), 3);
  EXPECT_EQ(focusTargets.at(0), information);
  EXPECT_EQ(focusTargets.at(1), fullscreen);
  EXPECT_EQ(focusTargets.at(2), menu);
  const auto informationGeometry = QRectF(information->position(), information->size());
  const auto fullscreenGeometry = QRectF(fullscreen->position(), fullscreen->size());
  const auto titleWidth = title->width();
  for (const auto& text : {QString(), QStringLiteral("image.png"), QStringLiteral("Pictures — 12 images")}) {
    ASSERT_TRUE(header->setProperty("title", text));
    ASSERT_TRUE(header->setProperty("titleVisible", true));
    EXPECT_TRUE(title->isVisible());
    ASSERT_TRUE(header->setProperty("titleVisible", false));
    EXPECT_FALSE(title->isVisible());
    EXPECT_EQ(title->property("rawText").toString(), text);
    EXPECT_EQ(title->width(), titleWidth);
    EXPECT_EQ(QRectF(information->position(), information->size()), informationGeometry);
    EXPECT_EQ(QRectF(fullscreen->position(), fullscreen->size()), fullscreenGeometry);
    EXPECT_EQ(header->height(), height);
    EXPECT_EQ(menu->mapToItem(header, QPointF()), menuPosition);
    EXPECT_TRUE(information->isVisible());
    EXPECT_TRUE(fullscreen->isVisible());
    EXPECT_TRUE(menu->isVisible());
    EXPECT_EQ(header->property("menuAnchor"), anchor);
    EXPECT_EQ(header->property("focusTargets"), targets);
    EXPECT_EQ(window->title(), nativeTitle);
    EXPECT_EQ(QQmlProperty::read(information, "KeyNavigation.tab", qmlContext(information)).value<QQuickItem*>(),
              fullscreen);
    EXPECT_EQ(QQmlProperty::read(fullscreen, "KeyNavigation.tab", qmlContext(fullscreen)).value<QQuickItem*>(), menu);
    EXPECT_EQ(QQmlProperty::read(menu, "KeyNavigation.tab", qmlContext(menu)).value<QQuickItem*>(), information);
    EXPECT_EQ(QQmlProperty::read(information, "KeyNavigation.backtab", qmlContext(information)).value<QQuickItem*>(),
              menu);
    EXPECT_EQ(QQmlProperty::read(fullscreen, "KeyNavigation.backtab", qmlContext(fullscreen)).value<QQuickItem*>(),
              information);
    EXPECT_EQ(QQmlProperty::read(menu, "KeyNavigation.backtab", qmlContext(menu)).value<QQuickItem*>(), fullscreen);
  }
}

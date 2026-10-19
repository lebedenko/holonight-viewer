#include "image_document.h"
#include "viewer_preferences.h"

#include <QAccessible>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlListReference>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

TEST(ViewerHeader, DefaultsAndAllPreferenceCombinationsInBothModes) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  auto* header = window->findChild<QQuickItem*>("viewerHeader");
  auto* title = window->findChild<QQuickItem*>("headerTitle");
  auto* information = window->findChild<QQuickItem*>("informationButton");
  auto* fullscreen = window->findChild<QQuickItem*>("fullscreenButton");
  auto* menu = window->findChild<QQuickItem*>("actionsButton");
  auto* close = window->findChild<QQuickItem*>("closeButton");
  ASSERT_NE(header, nullptr);
  ASSERT_NE(title, nullptr);
  ASSERT_NE(close, nullptr);
  ASSERT_NE(menu, nullptr);
  ASSERT_NE(information, nullptr);
  ASSERT_NE(fullscreen, nullptr);
  ASSERT_TRUE(QTest::qWaitFor([&] { return menu->width() > 0; }));
  EXPECT_TRUE(header->property("titleVisible").toBool());
  EXPECT_TRUE(close->isVisible());
  EXPECT_EQ(title->property("text").toString(), "HoloNight Viewer");
  auto* accessible = QAccessible::queryAccessibleInterface(close);
  ASSERT_NE(accessible, nullptr);
  EXPECT_EQ(accessible->text(QAccessible::Name), "Close");
  const auto nativeTitle = window->title();
  const auto height = header->height();
  header->setProperty("title", QString(300, 'x') + "<b>plain</b>.png");
  window->resize(420, 280);
  for (const auto mode : {QWindow::Windowed, QWindow::FullScreen}) {
    window->setVisibility(mode);
    for (bool showTitle : {false, true}) {
      for (bool showClose : {false, true}) {
        header->setProperty("titleVisible", showTitle);
        header->setProperty("closeVisible", showClose);
        QMetaObject::invokeMethod(window, "showArrows");
        QCoreApplication::processEvents();
        ASSERT_TRUE(header->isVisible());
        EXPECT_EQ(title->isVisible(), showTitle);
        EXPECT_EQ(close->isVisible(), showClose);
        EXPECT_EQ(header->height(), height);
        EXPECT_EQ(window->title(), nativeTitle);
        EXPECT_EQ(header->property("menuAnchor").value<QQuickItem*>(), menu);
        const QQmlListReference targets(header, "focusTargets");
        ASSERT_EQ(targets.count(), showClose ? 4 : 3);
        EXPECT_EQ(targets.at(0), information);
        EXPECT_EQ(targets.at(1), fullscreen);
        EXPECT_EQ(targets.at(2), menu);
        if (showClose) {
          EXPECT_EQ(targets.at(3), close);
        }
        EXPECT_GE(title->width(), 0);
        EXPECT_LT(title->mapToItem(header, QPointF(title->width(), 0)).x(),
                  information->mapToItem(header, QPointF()).x());
        EXPECT_EQ(title->property("textFormat").toInt(), 0);  // Text.PlainText
        EXPECT_EQ(title->property("elide").toInt(), Qt::ElideMiddle);
        EXPECT_LT(title->mapToItem(header, QPointF()).x(), 30);
        EXPECT_EQ(QQmlProperty::read(menu, "KeyNavigation.tab", qmlContext(menu)).value<QQuickItem*>(),
                  showClose ? close : information);
        EXPECT_EQ(
            QQmlProperty::read(information, "KeyNavigation.backtab", qmlContext(information)).value<QQuickItem*>(),
            showClose ? close : menu);
        if (showClose) {
          EXPECT_GT(close->mapToItem(header, QPointF()).x(), menu->mapToItem(header, QPointF()).x());
        }
      }
    }
  }
}

TEST(ViewerHeader, ReloadTransfersFocusedCloseToMenuAndHonorsModalGating) {
  QTemporaryDir directory;
  ViewerPreferences preferences(directory.path(), {}, {});
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({
      {QStringLiteral("document"), QVariant::fromValue(&document)},
      {QStringLiteral("preferences"), QVariant::fromValue(&preferences)},
  });
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  auto* close = window->findChild<QQuickItem*>("closeButton");
  auto* menu = window->findChild<QQuickItem*>("actionsButton");
  ASSERT_NE(close, nullptr);
  window->setVisibility(QWindow::FullScreen);
  close->forceActiveFocus(Qt::TabFocusReason);
  ASSERT_TRUE(close->hasActiveFocus());
  ASSERT_TRUE(QDir().mkpath(directory.filePath("holonight-viewer")));
  QSaveFile file(directory.filePath("holonight-viewer/config.toml"));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  ASSERT_GT(file.write("[header]\nshow_title = false\nshow_close_button = false\n"), 0);
  ASSERT_TRUE(file.commit());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !preferences.showCloseButton(); }));
  EXPECT_FALSE(close->isVisible());
  EXPECT_FALSE(window->findChild<QQuickItem*>("headerTitle")->isVisible());
  EXPECT_TRUE(menu->hasActiveFocus());
  EXPECT_TRUE(window->findChild<QQuickItem*>("viewerHeader")->isVisible());
  window->setProperty("helpOpen", true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->property("modalActive").toBool(); }));
  EXPECT_FALSE(close->isEnabled());
  const auto* focus = window->activeFocusItem();
  ASSERT_TRUE(QFile::remove(file.fileName()));
  ASSERT_TRUE(QTest::qWaitFor([&] { return preferences.showCloseButton(); }));
  EXPECT_EQ(window->activeFocusItem(), focus);
}

TEST(ViewerHeader, CloseActivatesOnEnterAndSpaceThroughNormalWindowClose) {
  for (const auto key : {Qt::Key_Return, Qt::Key_Space}) {
    ImageDocument document;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
    engine.loadFromModule("HolonightViewer", "Main");
    ASSERT_EQ(engine.rootObjects().size(), 1);
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* close = window->findChild<QQuickItem*>("closeButton");
    ASSERT_NE(close, nullptr);
    close->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(window, key);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !window->isVisible(); }));
  }
}

TEST(ViewerHeader, GlyphsPreserveIntendedExtents) {
  QRect closeBounds;
  for (const auto* name : {"close", "information", "fullscreen", "menu"}) {
    QSvgRenderer renderer(QStringLiteral(":/qt/qml/HolonightViewer/icons/%1.svg").arg(QString::fromLatin1(name)));
    ASSERT_TRUE(renderer.isValid()) << name;
    QImage pixels(24, 24, QImage::Format_ARGB32_Premultiplied);
    pixels.fill(Qt::transparent);
    QPainter painter(&pixels);
    renderer.render(&painter);
    painter.end();
    QRect bounds;
    for (int row = 0; row < pixels.height(); ++row) {
      for (int column = 0; column < pixels.width(); ++column) {
        if (qAlpha(pixels.pixel(column, row)) != 0) {
          bounds = bounds.united(QRect(column, row, 1, 1));
        }
      }
    }
    if (closeBounds.isEmpty()) {
      closeBounds = bounds;
      EXPECT_FALSE(bounds.isEmpty());
    } else {
      // The information glyph is optically enlarged while retaining the same center.
      const auto expected = QString::fromLatin1(name) == QStringLiteral("information")
                                ? closeBounds.adjusted(-1, -1, 1, 1)
                                : closeBounds;
      EXPECT_EQ(bounds, expected) << name;
    }
  }
}

TEST(ViewerHeader, CloseHoverUsesErrorSurfaceAndContrastingForeground) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  auto* close = window->findChild<QQuickItem*>("closeButton");
  auto* menu = window->findChild<QQuickItem*>("actionsButton");
  ASSERT_NE(close, nullptr);
  ASSERT_NE(menu, nullptr);
  auto* background = close->property("background").value<QQuickItem*>();
  ASSERT_NE(background, nullptr);
  auto* palette = engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  ASSERT_TRUE(QTest::qWaitForWindowExposed(window));
  ASSERT_TRUE(QTest::qWaitFor([&] { return close->width() > 0 && menu->width() > 0; }));
  const auto center = [](QQuickItem* item) {
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
  };
  QTest::mouseMove(window, QPoint(200, 200));
  QTest::mouseMove(window, center(close));
  ASSERT_TRUE(QTest::qWaitFor([&] { return close->property("hovered").toBool(); }));
  EXPECT_EQ(background->property("color").value<QColor>(), palette->property("error").value<QColor>());
  EXPECT_EQ(QQmlProperty::read(close, "icon.color").value<QColor>(), palette->property("onError").value<QColor>());
  // Keep the error surface while the button is pressed.
  close->setProperty("down", true);
  EXPECT_EQ(background->property("color").value<QColor>(), palette->property("error").value<QColor>());
  close->setProperty("down", false);
  QTest::mouseMove(window, center(menu));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !close->property("hovered").toBool(); }));
  EXPECT_EQ(background->property("color").value<QColor>(), QColor(Qt::transparent));
  auto* menuBackground = menu->property("background").value<QQuickItem*>();
  ASSERT_NE(menuBackground, nullptr);
  EXPECT_EQ(menuBackground->property("color").value<QColor>(), palette->property("surfaceHover").value<QColor>());
  window->setProperty("helpOpen", true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !close->isEnabled(); }));
  EXPECT_EQ(QQmlProperty::read(close, "icon.color").value<QColor>(), palette->property("textDisabled").value<QColor>());
}

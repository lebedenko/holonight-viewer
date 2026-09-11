#include "image_canvas.h"
#include "image_document.h"

#include <QAccessible>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>

Q_IMPORT_QML_PLUGIN(HolonightViewerPlugin)

TEST(Viewer, WindowAndKeyboard) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  QStringList iconWarnings;
  QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError>& warnings) {
    for (const auto& warning : warnings) {
      if (warning.description().contains("Failed to get image from provider")) {
        iconWarnings.append(warning.toString());
      }
    }
  });
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  EXPECT_TRUE(window->isVisible());
  EXPECT_FALSE(window->flags().testFlag(Qt::FramelessWindowHint));
  auto* empty = window->findChild<QObject*>(QStringLiteral("emptyState"));
  ASSERT_NE(empty, nullptr);
  auto* empty_item = qobject_cast<QQuickItem*>(empty);
  ASSERT_NE(empty_item, nullptr);
  auto* canvas = window->findChild<ImageCanvas*>(QStringLiteral("imageCanvas"));
  ASSERT_NE(canvas, nullptr);
  auto* accessible = QAccessible::queryAccessibleInterface(canvas);
  ASSERT_NE(accessible, nullptr);
  EXPECT_EQ(accessible->text(QAccessible::Name), QStringLiteral("No image open"));
  QQmlExpression ignored(qmlContext(empty), empty, "Accessible.ignored");
  EXPECT_TRUE(ignored.evaluate().toBool());
  EXPECT_FALSE(ignored.hasError());
  EXPECT_FALSE(empty_item->activeFocusOnTab());
  EXPECT_TRUE(empty_item->isVisible());
  EXPECT_FALSE(empty->property("hasError").toBool());
  const QString capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  const auto checkDecoration = [&] {
    QTest::qWait(100);
    const auto* area = empty_item->parentItem();
    const auto shorter = std::min(area->width(), area->height());
    const auto padding = std::clamp(shorter * 0.08, 32.0, 64.0);
    EXPECT_EQ(empty_item->width(), std::max(0.0, std::floor(shorter - (2 * padding))));
    EXPECT_EQ(empty_item->height(), empty_item->width());
    EXPECT_NEAR(empty_item->x() + (empty_item->width() / 2), area->width() / 2, 0.5);
    EXPECT_NEAR(empty_item->y() + (empty_item->height() / 2), area->height() / 2, 0.5);
    EXPECT_GE(empty_item->x(), padding);
    EXPECT_GE(empty_item->y(), padding);
    EXPECT_LE(empty_item->x() + empty_item->width(), area->width() - padding);
    EXPECT_LE(empty_item->y() + empty_item->height(), area->height() - padding);
    EXPECT_FALSE(empty->property("hasError").toBool());
    ASSERT_FALSE(empty_item->childItems().isEmpty());
    const auto rasterLimit = std::max(1, static_cast<int>(std::floor(1024 / window->devicePixelRatio())));
    EXPECT_EQ(QQmlProperty::read(empty_item->childItems().first(), "sourceSize").toSize(),
              QSize(std::min(rasterLimit, static_cast<int>(empty_item->width())),
                    std::min(rasterLimit, static_cast<int>(empty_item->height()))));
  };
  for (const auto& size : {QSize(420, 280), QSize(1000, 700), QSize(480, 900), QSize(1600, 500), QSize(2000, 1600)}) {
    window->resize(size);
    checkDecoration();
    if (!capture.isEmpty()) {
      EXPECT_TRUE(
          window->grabWindow().save(capture + QStringLiteral("-empty-%1x%2.png").arg(size.width()).arg(size.height())));
    }
  }
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  // Qt updates visibility synchronously; let the compositor apply each request
  // before issuing the next one or checking restoration.
  const auto settle = [] { QTest::qWait(250); };
  settle();
  const auto initial_geometry = window->geometry();
  const auto initial_visibility = window->visibility();
  QTest::keyClick(window, Qt::Key_F);
  settle();
  EXPECT_TRUE(QTest::qWaitFor([window] { return window->visibility() == QWindow::FullScreen; }));
  checkDecoration();
  if (!capture.isEmpty()) {
    EXPECT_TRUE(window->grabWindow().save(capture + QStringLiteral("-empty-fullscreen.png")));
  }
  QTest::keyClick(window, Qt::Key_F);
  settle();
  EXPECT_TRUE(QTest::qWaitFor([window, initial_visibility] { return window->visibility() == initial_visibility; }));
  QTest::keyClick(window, Qt::Key_F);
  settle();
  QTest::keyClick(window, Qt::Key_Escape);
  settle();
  EXPECT_TRUE(QTest::qWaitFor([window, initial_visibility] { return window->visibility() == initial_visibility; }));
  QTest::keyClick(window, Qt::Key_Escape);
  settle();
  EXPECT_TRUE(QTest::qWaitFor([window, initial_visibility] { return window->visibility() == initial_visibility; }));
  EXPECT_EQ(window->geometry(), initial_geometry);
  window->showMaximized();
  settle();
  ASSERT_TRUE(QTest::qWaitFor([window] { return window->visibility() == QWindow::Maximized; }));
  QTest::keyClick(window, Qt::Key_F);
  settle();
  QTest::keyClick(window, Qt::Key_Escape);
  settle();
  EXPECT_EQ(window->visibility(), QWindow::Maximized);
  QTest::keyClick(window, Qt::Key_Q);
  EXPECT_FALSE(window->isVisible());
  EXPECT_TRUE(iconWarnings.isEmpty()) << iconWarnings.join(QLatin1Char('\n')).toStdString();
}

TEST(Viewer, EmptyDecorationLiveTheme) {
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  QTemporaryDir directory(QStringLiteral(VIEWER_FIXTURE_DIR) + "/empty-theme-XXXXXX");
  ASSERT_TRUE(directory.isValid());
  const auto childPath = qEnvironmentVariable("VIEWER_TEST_APPEARANCE_FILE");
  const auto path = childPath.isEmpty() ? directory.filePath("appearance.toml") : childPath;
  const auto writeTheme = [&](const QByteArray& scheme) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    const auto content = "version = 1\n[theme]\nscheme = \"" + scheme + R"("
accent = "blue"
[typography]
ui_family = "Inter"
ui_size = 12
monospace_family = "JetBrains Mono"
monospace_size = 12
title_family = "Audiowide"
title_size = 10
display_family = "Rajdhani"
display_size = 24
[icons]
theme = "HoloNight"
fallback = "Papirus"
cursor = "default"
[layout]
scale = 1.0
[shape]
style = "inherit"
scale = 1.0
)";
    return file.write(content) == content.size();
  };
  ASSERT_TRUE(writeTheme("holonight-dark"));
  if (childPath.isEmpty()) {
    QProcess child;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("HOLONIGHT_APPEARANCE_FILE", path);
    environment.insert("VIEWER_TEST_APPEARANCE_FILE", path);
    child.setProcessEnvironment(environment);
    child.start(QCoreApplication::applicationFilePath(), {"--gtest_filter=Viewer.EmptyDecorationLiveTheme"});
    ASSERT_TRUE(child.waitForFinished(15000));
    EXPECT_EQ(child.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(child.exitCode(), 0) << child.readAllStandardOutput().toStdString()
                                   << child.readAllStandardError().toStdString();
    return;
  }
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  auto* decoration = window->findChild<QQuickItem*>("emptyState");
  ASSERT_NE(decoration, nullptr);
  auto* palette = engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  auto* appearance = engine.singletonInstance<QObject*>("Holonight.Core", "HnAppearance");
  ASSERT_NE(appearance, nullptr);
  ASSERT_EQ(appearance->property("configFile").toString(), path);
  QTest::qWait(200);
  const auto darkColor = decoration->property("normalColor").value<QColor>();
  EXPECT_EQ(darkColor, palette->property("surface").value<QColor>());
  const auto darkSource = decoration->property("_renderSource").toUrl();
  const auto darkImage = window->grabWindow();
  ASSERT_FALSE(darkImage.isNull());
  ASSERT_TRUE(writeTheme("holonight-light"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return decoration->property("normalColor").value<QColor>() != darkColor; }));
  EXPECT_EQ(decoration->property("normalColor"), palette->property("surface"));
  EXPECT_NE(decoration->property("_renderSource").toUrl(), darkSource);
  QTest::qWait(200);
  EXPECT_FALSE(decoration->property("hasError").toBool());
  const auto lightImage = window->grabWindow();
  EXPECT_NE(lightImage, darkImage);
  const QString capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (!capture.isEmpty()) {
    EXPECT_TRUE(darkImage.save(capture + "-live-dark.png"));
    EXPECT_TRUE(lightImage.save(capture + "-live-light.png"));
  }
  ASSERT_TRUE(writeTheme("holonight-dark"));
  EXPECT_TRUE(QTest::qWaitFor([&] { return decoration->property("normalColor").value<QColor>() == darkColor; }));
}

namespace {
QQuickItem* findFileDelegate(QQuickItem* root, const QUrl& url) {
  if (root->property("fileUrl").toUrl() == url) {
    return root;
  }
  for (auto* child : root->childItems()) {
    if (auto* match = findFileDelegate(child, url)) {
      return match;
    }
  }
  return nullptr;
}
}  // namespace

TEST(Viewer, OpeningCanvasAndAdapters) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR) + "/dialog");
  const auto path = QStringLiteral(VIEWER_FIXTURE_DIR) + QString::fromUtf8("/dialog/вікно space.png");
  QImage fixture(80, 40, QImage::Format_ARGB32);
  fixture.fill(Qt::transparent);
  ASSERT_TRUE(fixture.save(path));
  const auto url = QUrl::fromLocalFile(path);
  auto* canvas = window->findChild<ImageCanvas*>(QStringLiteral("imageCanvas"));
  ASSERT_NE(canvas, nullptr);

  QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->findChild<QObject*>(QStringLiteral("openDialog")) != nullptr; }));
  auto* dialog = window->findChild<QObject*>(QStringLiteral("openDialog"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }));
  ASSERT_TRUE(dialog->setProperty("currentFolder", QUrl::fromLocalFile(QFileInfo(path).absolutePath())));
  QQuickItem* delegate = nullptr;
  ASSERT_TRUE(QTest::qWaitFor([&] {
    for (auto* top : QGuiApplication::allWindows()) {
      if (auto* quick = qobject_cast<QQuickWindow*>(top)) {
        delegate = findFileDelegate(quick->contentItem(), url);
        if (delegate) {
          return true;
        }
      }
    }
    return false;
  }));
  QTest::mouseDClick(delegate->window(), Qt::LeftButton, Qt::NoModifier,
                     delegate->mapToScene(QPointF(delegate->width() / 2, delegate->height() / 2)).toPoint());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }))
      << document.error().toStdString() << " state=" << document.state();
  EXPECT_EQ(canvas->image().size(), fixture.size());
  EXPECT_FALSE(window->property("dialogRequested").toBool());
  QTest::qWait(100);
  const QString capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (!capture.isEmpty()) {
    EXPECT_TRUE(window->grabWindow().save(capture + QStringLiteral("-image-large.png")));
    window->resize(420, 280);
    QTest::qWait(100);
    EXPECT_TRUE(window->grabWindow().save(capture + QStringLiteral("-image-small.png")));
  }
  const auto fit = ImageCanvas::fitRect(fixture.size(), canvas->size());
  EXPECT_LE(fit.bottom(), canvas->height());
  EXPECT_LE(fit.right(), canvas->width());

  QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->findChild<QObject*>(QStringLiteral("openDialog")) != nullptr; }));
  dialog = window->findChild<QObject*>(QStringLiteral("openDialog"));
  ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "reject"));
  EXPECT_EQ(document.state(), ImageDocument::Ready);

  auto* decoration = window->findChild<QQuickItem*>(QStringLiteral("emptyState"));
  ASSERT_NE(decoration, nullptr);
  EXPECT_FALSE(decoration->isVisible());
  const auto dropUrls = [&](const QList<QUrl>& urls) {
    QMimeData mime;
    mime.setUrls(urls);
    const QPoint point(window->width() / 2, window->height() / 2);
    QDragEnterEvent enter(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &enter);
    QDropEvent drop(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &drop);
    EXPECT_TRUE(drop.isAccepted());
  };
  dropUrls({url, url});
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_FALSE(decoration->isVisible());
  EXPECT_TRUE(canvas->image().isNull());
  dropUrls({QUrl("https://example.org/a.png")});
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_FALSE(decoration->isVisible());
  dropUrls({url});
  EXPECT_EQ(document.state(), ImageDocument::Loading);
  EXPECT_FALSE(decoration->isVisible());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  EXPECT_EQ(canvas->image().size(), fixture.size());
  QTest::keyClick(window, Qt::Key_F);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->visibility() == QWindow::FullScreen; }));
  EXPECT_EQ(canvas->image().size(), fixture.size());
  QTest::keyClick(window, Qt::Key_Escape);
  QTest::keyClick(window, Qt::Key_Q);
  EXPECT_FALSE(window->isVisible());
}

TEST(Viewer, EmbeddedStyleSelection) {
  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.setData("import QtQuick.Controls\nButton {}", QUrl());
  const std::unique_ptr<QObject> button(component.create());
  ASSERT_NE(button, nullptr) << component.errorString().toStdString();
  EXPECT_EQ(QQuickStyle::name(), QStringLiteral("Holonight"));
  EXPECT_TRUE(button->property("foregroundColor").isValid());
}

int main(int argc, char* argv[]) {
  qunsetenv("QT_QUICK_CONTROLS_STYLE");
  qunsetenv("QT_QUICK_CONTROLS_FALLBACK_STYLE");
  qunsetenv("QT_QUICK_CONTROLS_CONF");
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  const QGuiApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

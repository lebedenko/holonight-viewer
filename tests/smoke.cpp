#include "image_canvas.h"
#include "image_document.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

#include <gtest/gtest.h>
#include <memory>

Q_IMPORT_QML_PLUGIN(HolonightViewerPlugin)

TEST(Viewer, WindowAndKeyboard) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  EXPECT_TRUE(window->isVisible());
  EXPECT_FALSE(window->flags().testFlag(Qt::FramelessWindowHint));
  auto* empty = window->findChild<QObject*>(QStringLiteral("emptyState"));
  ASSERT_NE(empty, nullptr);
  EXPECT_EQ(empty->property("titleText").toString(), QStringLiteral("No image open"));
  window->resize(420, 280);
  auto* empty_item = qobject_cast<QQuickItem*>(empty);
  ASSERT_NE(empty_item, nullptr);
  QCoreApplication::processEvents();
  EXPECT_GE(empty_item->x(), 0);
  EXPECT_GE(empty_item->y(), 0);
  EXPECT_LE(empty_item->x() + empty_item->width(), window->width());
  EXPECT_LE(empty_item->y() + empty_item->height(), window->height());
  const QString capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (!capture.isEmpty()) {
    QTest::qWait(200);
    EXPECT_TRUE(window->grabWindow().save(capture + QStringLiteral("-small.png")));
    window->resize(1000, 700);
    QTest::qWait(200);
    EXPECT_TRUE(window->grabWindow().save(capture + QStringLiteral("-large.png")));
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
  fixture.fill(Qt::red);
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
  EXPECT_TRUE(canvas->image().isNull());
  dropUrls({QUrl("https://example.org/a.png")});
  EXPECT_EQ(document.state(), ImageDocument::Error);
  dropUrls({url});
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

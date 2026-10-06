#include "image_document.h"
#include "viewer_controller.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>
#include <memory>

namespace {
struct IndicatorFixture {
  QQuickWindow window;
  QQmlEngine engine;
  QQmlComponent component{&engine, QUrl("qrc:/qt/qml/HolonightViewer/overlays/ViewerLoadingIndicator.qml")};
  std::unique_ptr<QObject> object;
  QQuickItem* item = nullptr;

  bool load(bool loading = false) {
    object.reset(component.createWithInitialProperties({{"loading", loading}}));
    item = qobject_cast<QQuickItem*>(object.get());
    if (item != nullptr) {
      item->setParentItem(window.contentItem());
      window.show();
    }
    return item != nullptr;
  }
  void loading(bool value) const { object->setProperty("loading", value); }
  [[nodiscard]] bool running() const { return object->property("running").toBool(); }
};
}  // namespace

TEST(LoadingIndicator, ShortLoadAndCancellationNeverReveal) {
  IndicatorFixture fixture;
  ASSERT_TRUE(fixture.load());
  fixture.loading(true);
  QTest::qWait(100);
  EXPECT_FALSE(fixture.item->isVisible());
  fixture.loading(false);
  QTest::qWait(250);
  EXPECT_FALSE(fixture.item->isVisible());
  EXPECT_FALSE(fixture.running());
  EXPECT_EQ(fixture.item->opacity(), 0);
}

TEST(LoadingIndicator, ExistingLoadRevealsAndCompletionFadesOut) {
  IndicatorFixture fixture;
  ASSERT_TRUE(fixture.load(true));
  QTest::qWait(100);
  EXPECT_FALSE(fixture.item->isVisible());
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.item->opacity() == 1; }, 600));
  EXPECT_TRUE(fixture.running());
  EXPECT_FALSE(fixture.item->activeFocusOnTab());
  QQmlExpression name(qmlContext(fixture.object.get()), fixture.object.get(), "Accessible.name");
  EXPECT_EQ(name.evaluate().toString(), "Loading image");
  fixture.loading(false);
  EXPECT_TRUE(fixture.item->isVisible());
  EXPECT_TRUE(fixture.running());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.item->isVisible(); }, 300));
  EXPECT_FALSE(fixture.running());
  QQmlExpression ignored(qmlContext(fixture.object.get()), fixture.object.get(), "Accessible.ignored");
  EXPECT_TRUE(ignored.evaluate().toBool());
}

TEST(LoadingIndicator, NewEpisodeClearsFadeAndGetsFreshDelay) {
  IndicatorFixture fixture;
  ASSERT_TRUE(fixture.load(true));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.item->opacity() == 1; }, 600));
  fixture.loading(false);
  fixture.loading(true);
  EXPECT_EQ(fixture.item->opacity(), 0);
  EXPECT_FALSE(fixture.running());
  QTest::qWait(100);
  EXPECT_FALSE(fixture.item->isVisible());
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.item->opacity() == 1; }, 600));
}

TEST(LoadingIndicator, CompletionDuringFadeInSettlesHidden) {
  IndicatorFixture fixture;
  ASSERT_TRUE(fixture.load(true));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.item->opacity() > 0; }, 400));
  ASSERT_LT(fixture.item->opacity(), 1);
  fixture.loading(false);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.item->isVisible(); }, 300));
  QTest::qWait(150);
  EXPECT_EQ(fixture.item->opacity(), 0);
  EXPECT_FALSE(fixture.running());
}

TEST(LoadingIndicator, MainBindingNotificationsGridAndImmediateError) {
  // The folder scan must find an image so grid entry does not depend on /tmp contents.
  QTemporaryDir folder;
  ASSERT_TRUE(folder.isValid());
  QFile image(folder.filePath(QStringLiteral("loading-indicator.png")));
  ASSERT_TRUE(image.open(QIODevice::WriteOnly));
  image.close();
  // Keep decoding pending until the test cancels it; no image-size timing dependency.
  ImageDocument document([](const QUrl&, const std::atomic_bool& cancelled) {
    while (!cancelled.load()) {
      QThread::msleep(1);
    }
    return DecodeResult{};
  });
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{"document", QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  auto* indicator = window->findChild<QQuickItem*>("viewerLoadingIndicator");
  auto* feedback = window->findChild<QQuickItem*>("documentFeedback");
  ASSERT_TRUE(indicator && feedback);
  EXPECT_FALSE(indicator->property("loading").toBool());
  document.open({QUrl::fromLocalFile(image.fileName())});
  ASSERT_EQ(document.state(), ImageDocument::Loading);
  EXPECT_TRUE(indicator->property("loading").toBool());
  EXPECT_FALSE(feedback->isVisible());
  // Re-evaluating the document binding must leave the original deadline intact.
  for (int notification = 0; notification < 6; ++notification) {
    QTest::qWait(50);
    document.changed();
  }
  ASSERT_TRUE(QTest::qWaitFor([&] { return indicator->opacity() == 1; }, 150));
  auto* controller = window->findChild<ViewerController*>();
  ASSERT_NE(controller, nullptr);
  controller->toggleGrid();
  EXPECT_FALSE(indicator->property("loading").toBool());
  EXPECT_FALSE(indicator->isVisible());
  EXPECT_FALSE(indicator->property("running").toBool());
  controller->toggleGrid();
  EXPECT_TRUE(indicator->property("loading").toBool());
  EXPECT_EQ(indicator->opacity(), 0);
  ASSERT_TRUE(QTest::qWaitFor([&] { return indicator->opacity() == 1; }, 600));
  document.open({QUrl("https://example.org/image.png")});
  ASSERT_EQ(document.state(), ImageDocument::Error);
  EXPECT_FALSE(indicator->isVisible());
  EXPECT_FALSE(indicator->property("running").toBool());
  EXPECT_TRUE(feedback->isVisible());
  EXPECT_EQ(feedback->property("text").toString(), document.error());
  EXPECT_EQ(feedback->property("textFormat").toInt(), Qt::PlainText);
  QQmlExpression name(qmlContext(feedback), feedback, "Accessible.name");
  EXPECT_EQ(name.evaluate().toString(), document.error());
}

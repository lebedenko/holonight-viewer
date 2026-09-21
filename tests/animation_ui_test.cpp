#include "gif_fixture.h"
#include "image_canvas.h"
#include "image_document.h"

#include <QAccessible>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace {
struct AnimationFixture {
  QTemporaryDir dir{QStringLiteral(VIEWER_FIXTURE_DIR) + "/animation-XXXXXX"};
  ImageDocument document;
  QQmlApplicationEngine engine;
  QQuickWindow* window = nullptr;
  ImageCanvas* canvas = nullptr;
  QQuickItem* button = nullptr;

  ::testing::AssertionResult load() {
    if (!dir.isValid()) {
      return ::testing::AssertionFailure() << "temporary directory unavailable";
    }
    engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
    engine.loadFromModule("HolonightViewer", "Main");
    if (engine.rootObjects().size() != 1) {
      return ::testing::AssertionFailure() << "Main failed to load";
    }
    window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    window->requestActivate();
    if (!QTest::qWaitForWindowActive(window)) {
      return ::testing::AssertionFailure() << "window not active";
    }
    canvas = window->findChild<ImageCanvas*>("imageCanvas");
    button = window->findChild<QQuickItem*>("playPauseButton");
    return canvas != nullptr && button != nullptr ? ::testing::AssertionSuccess()
                                                  : ::testing::AssertionFailure() << "items not found";
  }

  [[nodiscard]] QUrl writeGif(const QString& name, int frames, int delayCs, int loopField = 0) const {
    QList<gif::Frame> list;
    for (int i = 0; i < frames; ++i) {
      list.append({.delayCs = delayCs, .second = (i % 2) != 0});
    }
    QFile file(dir.filePath(name));
    if (!file.open(QIODevice::WriteOnly)) {
      return {};
    }
    file.write(gif::bytes("GIF89a", list, loopField));
    return QUrl::fromLocalFile(file.fileName());
  }

  ::testing::AssertionResult openAnimated(const QUrl& url) {
    QSignalSpy rendered(canvas, &ImageCanvas::firstRendered);
    document.open({url});
    if (!QTest::qWaitFor([&] {
          return document.state() == ImageDocument::Ready && document.animation()->animated() && !rendered.isEmpty();
        })) {
      return ::testing::AssertionFailure() << "animated document did not open";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] AnimationController* animation() { return document.animation(); }
  // Alternates the position: a move to the current position sends no event.
  void showButton() const {
    const auto offset = (moves_++ % 2) * 5;
    QTest::mouseMove(window, canvas->mapToScene({(canvas->width() / 2) + offset, canvas->height() / 4}).toPoint());
  }
  [[nodiscard]] QQuickItem* item(const QString& name) const { return find(window->contentItem(), name); }
  [[nodiscard]] QStringList stripTexts() const {
    QStringList texts;
    collect(window->findChild<QQuickItem*>("detailsStrip"), texts);
    return texts;
  }
  void reveal() const { QMetaObject::invokeMethod(window, "revealDetails"); }

 private:
  static QQuickItem* find(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) {
      return root;
    }
    for (auto* child : root->childItems()) {
      if (auto* found = find(child, name)) {
        return found;
      }
    }
    return nullptr;
  }
  inline static int moves_ = 0;  // shared: the cursor position outlives a fixture
  static void collect(QQuickItem* item, QStringList& texts) {
    if (item == nullptr) {
      return;
    }
    const auto text = item->property("rawText");
    if (text.isValid()) {
      texts.append(text.toString());
    }
    for (auto* child : item->childItems()) {
      collect(child, texts);
    }
  }
};

bool hasPerceivableNode(QAccessibleInterface* node, const QString& name) {
  if (node == nullptr) {
    return false;
  }
  if (node->text(QAccessible::Name) == name && !node->state().invisible) {
    return true;
  }
  for (int index = 0; index < node->childCount(); ++index) {
    if (hasPerceivableNode(node->child(index), name)) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(AnimationUi, ButtonIsCentredSizedAndFadesWithTheArrows) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is missing";
  }
  QAccessible::setActive(true);
  AnimationFixture fixture;
  ASSERT_TRUE(fixture.load());
  EXPECT_FALSE(fixture.button->isVisible());  // nothing is open
  ASSERT_TRUE(fixture.openAnimated(fixture.writeGif("loop.gif", 3, 50)));
  EXPECT_EQ(fixture.button->width(), 48);
  EXPECT_EQ(fixture.button->height(), 48);
  fixture.showButton();
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.button->opacity() == 1.0 && fixture.button->isVisible(); }));
  EXPECT_TRUE(fixture.button->isEnabled());
  const auto centre = fixture.button->mapToItem(fixture.canvas, {24, 24});
  EXPECT_NEAR(centre.x(), fixture.canvas->width() / 2, 1);
  EXPECT_NEAR(centre.y(), fixture.canvas->height() / 2, 1);
  auto* root = QAccessible::queryAccessibleInterface(fixture.window);
  EXPECT_TRUE(hasPerceivableNode(root, "Pause"));
  EXPECT_FALSE(hasPerceivableNode(root, "Play"));
  // Faded away with the arrows: neither visible nor perceivable nor clickable.
  QTest::keyClick(fixture.window, Qt::Key_Left);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.button->isVisible(); }));
  EXPECT_FALSE(hasPerceivableNode(root, "Pause"));
}

TEST(AnimationUi, ClickTogglesAndTheNameFollowsTheAction) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is missing";
  }
  QAccessible::setActive(true);
  AnimationFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openAnimated(fixture.writeGif("loop.gif", 3, 50)));
  fixture.showButton();
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.button->opacity() == 1.0; }));
  const auto point = fixture.button->mapToScene({24, 24}).toPoint();
  QTest::mouseClick(fixture.window, Qt::LeftButton, Qt::NoModifier, point);
  EXPECT_TRUE(fixture.animation()->userPaused());
  EXPECT_FALSE(fixture.animation()->playing());
  auto* root = QAccessible::queryAccessibleInterface(fixture.window);
  fixture.showButton();
  EXPECT_TRUE(QTest::qWaitFor([&] { return hasPerceivableNode(root, "Play"); }));
  QTest::mouseClick(fixture.window, Qt::LeftButton, Qt::NoModifier, point);
  EXPECT_TRUE(fixture.animation()->playing());
}

TEST(AnimationUi, SpaceTogglesOnlyWhenNothingElseOwnsIt) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is missing";
  }
  AnimationFixture fixture;
  ASSERT_TRUE(fixture.load());
  // Static image: Space is not consumed and nothing toggles.
  QImage still(8, 8, QImage::Format_RGB32);
  still.fill(Qt::red);
  ASSERT_TRUE(still.save(fixture.dir.filePath("still.png")));
  fixture.document.open({QUrl::fromLocalFile(fixture.dir.filePath("still.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }));
  QTest::keyClick(fixture.window, Qt::Key_Space);
  EXPECT_FALSE(fixture.animation()->userPaused());
  EXPECT_FALSE(fixture.animation()->canToggle());

  ASSERT_TRUE(fixture.openAnimated(fixture.writeGif("loop.gif", 3, 50)));
  QTest::keyClick(fixture.window, Qt::Key_Space);
  EXPECT_TRUE(fixture.animation()->userPaused());
  QTest::keyClick(fixture.window, Qt::Key_Space);
  EXPECT_FALSE(fixture.animation()->userPaused());
  // Held-down auto-repeat does not flicker the state.
  QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QString(), true);
  QCoreApplication::sendEvent(fixture.window, &repeat);
  EXPECT_FALSE(fixture.animation()->userPaused());

  // A focused header button keeps Space for itself (it opens Image Information) and playback is untouched.
  auto* information = fixture.window->findChild<QQuickItem*>("informationButton");
  ASSERT_NE(information, nullptr);
  information->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(fixture.window, Qt::Key_Space);
  EXPECT_FALSE(fixture.animation()->userPaused());
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.window->property("informationOpen").toBool(); }));
  // With that dialog open playback is suspended and Space does not toggle it.
  EXPECT_TRUE(fixture.animation()->suspendedByModal());
  EXPECT_FALSE(fixture.animation()->playing());
  QTest::keyClick(fixture.window, Qt::Key_Space);
  EXPECT_FALSE(fixture.animation()->userPaused());
  QTest::keyClick(fixture.window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.window->property("informationOpen").toBool(); }));
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.animation()->playing(); }));
}

TEST(AnimationUi, StripShowsStateFrameCountAndDamageNotice) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is missing";
  }
  AnimationFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openAnimated(fixture.writeGif("loop.gif", 3, 50)));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.animation()->frameCount() == 3; }));
  fixture.reveal();
  auto texts = fixture.stripTexts();
  EXPECT_TRUE(texts.contains("Animated")) << texts.join('|').toStdString();
  EXPECT_TRUE(texts.contains("Playing"));
  EXPECT_TRUE(texts.contains("3 frames"));
  QTest::keyClick(fixture.window, Qt::Key_Space);
  texts = fixture.stripTexts();
  EXPECT_TRUE(texts.contains("Paused"));
  EXPECT_FALSE(texts.contains("Playing"));
  EXPECT_TRUE(fixture.window->findChild<QQuickItem*>("detailsStrip")->property("shown").toBool());

  // A file cut inside its last frame keeps the last good frame and explains itself.
  auto bytes = gif::bytes(
      "GIF89a", {{.delayCs = 3, .second = false}, {.delayCs = 3, .second = true}, {.delayCs = 3, .second = false}}, 0);
  bytes.chop(6);
  QFile damaged(fixture.dir.filePath("damaged.gif"));
  ASSERT_TRUE(damaged.open(QIODevice::WriteOnly));
  damaged.write(bytes);
  damaged.close();
  fixture.document.open({QUrl::fromLocalFile(damaged.fileName())});
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.animation()->failureNotice().isEmpty(); }, 3000));
  auto* notice = fixture.window->findChild<QQuickItem*>("playbackNotice");
  ASSERT_NE(notice, nullptr);
  EXPECT_TRUE(notice->isVisible());
  EXPECT_EQ(notice->property("rawText").toString(), "Playback stopped: damaged frame");
  EXPECT_FALSE(fixture.button->isEnabled());
  EXPECT_EQ(fixture.document.state(), ImageDocument::Ready);
  // Navigation clears it.
  fixture.document.open({fixture.writeGif("next.gif", 2, 50)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.animation()->animated(); }));
  EXPECT_TRUE(fixture.animation()->failureNotice().isEmpty());
  EXPECT_FALSE(notice->isVisible());
}

TEST(AnimationUi, ZoomPanAndTransformsDoNotInterruptPlayback) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is missing";
  }
  AnimationFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openAnimated(fixture.writeGif("fast.gif", 3, 3)));
  fixture.canvas->actualSize();
  fixture.canvas->zoom(2, {10, 10});
  ASSERT_FALSE(fixture.canvas->fitting());
  const auto rect = fixture.canvas->imageRect();
  QSignalSpy frames(fixture.animation(), &AnimationController::frameReady);
  ASSERT_TRUE(QTest::qWaitFor([&] { return frames.count() >= 3; }, 3000));
  // Frames replaced the picture in place: the view is exactly where the user left it.
  EXPECT_FALSE(fixture.canvas->fitting());
  EXPECT_EQ(fixture.canvas->imageRect(), rect);
  EXPECT_TRUE(fixture.animation()->playing());
  EXPECT_EQ(fixture.canvas->image(), fixture.document.image());
  // A transform emits changed() but neither pauses playback nor snaps the canvas back to frame 0.
  QMetaObject::invokeMethod(fixture.window, "transformImage", Q_ARG(int, 1));
  const auto before = frames.count();
  ASSERT_TRUE(QTest::qWaitFor([&] { return frames.count() > before + 1; }, 3000));
  EXPECT_TRUE(fixture.animation()->playing());
  EXPECT_EQ(fixture.canvas->image().cacheKey(), fixture.document.image().cacheKey());
}

TEST(AnimationUi, HiddenWindowSuspendsAndFooterAndHelpAdvertiseSpace) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is missing";
  }
  AnimationFixture fixture;
  ASSERT_TRUE(fixture.load());
  EXPECT_EQ(fixture.item("footerHintPlayPause"), nullptr);
  ASSERT_TRUE(fixture.openAnimated(fixture.writeGif("loop.gif", 3, 50)));
  EXPECT_NE(fixture.item("footerHintPlayPause"), nullptr);
  EXPECT_NE(fixture.item("footerHintPlayPauseKeycap"), nullptr);
  fixture.window->hide();
  EXPECT_TRUE(fixture.animation()->suspendedByWindow());
  EXPECT_FALSE(fixture.animation()->playing());
  fixture.window->show();
  EXPECT_FALSE(fixture.animation()->suspendedByWindow());
  EXPECT_TRUE(fixture.animation()->playing());
  fixture.document.open({fixture.dir.filePath("missing.gif")});
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.item("footerHintPlayPause") == nullptr; }));
}

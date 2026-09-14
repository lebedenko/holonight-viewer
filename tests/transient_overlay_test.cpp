#include "image_canvas.h"
#include "image_document.h"

#include <QAccessible>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>

#include <array>
#include <functional>
#include <gtest/gtest.h>

namespace {
struct OverlayFixture {
  QTemporaryDir dir{QStringLiteral(VIEWER_FIXTURE_DIR) + "/overlay-XXXXXX"};
  ImageDocument document;
  QQmlApplicationEngine engine;
  QQuickWindow* window = nullptr;
  ImageCanvas* canvas = nullptr;
  QQuickItem* previous = nullptr;
  QQuickItem* next = nullptr;
  QQuickItem* strip = nullptr;
  QObject* arrowTimer = nullptr;
  QObject* detailsTimer = nullptr;

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
    if (window == nullptr) {
      return ::testing::AssertionFailure() << "no window";
    }
    window->requestActivate();
    if (!QTest::qWaitForWindowActive(window)) {
      return ::testing::AssertionFailure() << "window not active";
    }
    canvas = window->findChild<ImageCanvas*>("imageCanvas");
    previous = window->findChild<QQuickItem*>("previousButton");
    next = window->findChild<QQuickItem*>("nextButton");
    strip = window->findChild<QQuickItem*>("detailsStrip");
    arrowTimer = window->findChild<QObject*>("arrowTimer");
    detailsTimer = window->findChild<QObject*>("detailsTimer");
    if (canvas == nullptr || previous == nullptr || next == nullptr || strip == nullptr || arrowTimer == nullptr ||
        detailsTimer == nullptr) {
      return ::testing::AssertionFailure() << "overlay items not found";
    }
    return ::testing::AssertionSuccess();
  }

  // Three large images so the middle one can browse both ways and pan at actual size.
  ::testing::AssertionResult openFolder() {
    QImage image(3000, 2000, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    for (const auto* name : {"a.png", "b.png", "c.png"}) {
      if (!image.save(dir.filePath(QString::fromLatin1(name)))) {
        return ::testing::AssertionFailure() << "fixture image not written";
      }
    }
    QSignalSpy rendered(canvas, &ImageCanvas::firstRendered);
    document.open({QUrl::fromLocalFile(dir.filePath("b.png"))});
    if (!QTest::qWaitFor([&] {
          return document.state() == ImageDocument::Ready && !document.scanning() && document.canPrevious() &&
                 document.canNext() && !rendered.isEmpty();
        })) {
      return ::testing::AssertionFailure() << "folder did not open on the middle image";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] bool arrowsShown() const {
    return next->property("shown").toBool() && previous->property("shown").toBool();
  }
  [[nodiscard]] bool arrowsHidden() const {
    return !next->property("shown").toBool() && !previous->property("shown").toBool();
  }
  [[nodiscard]] bool hudShown() const { return strip->property("shown").toBool(); }

  [[nodiscard]] ::testing::AssertionResult hideHud() const {
    window->setProperty("detailsShown", false);
    if (!QTest::qWaitFor([&] { return !strip->isVisible(); })) {
      return ::testing::AssertionFailure() << "HUD did not fade out";
    }
    return ::testing::AssertionSuccess();
  }

  // A pointer move over the canvas away from the arrows and the HUD.
  void pointerOverCanvas(int offset = 0) const {
    const auto point = canvas->mapToScene({(canvas->width() / 2) + offset, canvas->height() / 3}).toPoint();
    QTest::mouseMove(window, point);
  }

  [[nodiscard]] static QPoint centre(const QQuickItem* item) {
    return item == nullptr ? QPoint() : item->mapToScene({item->width() / 2, item->height() / 2}).toPoint();
  }
};

void waitUntil(const QElapsedTimer& clock, qint64 milliseconds) {
  const auto remaining = milliseconds - clock.elapsed();
  if (remaining > 0) {
    QTest::qWait(static_cast<int>(remaining));
  }
}

// Qt keeps hidden items in the tree flagged invisible, which assistive technology skips.
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

// REQ-F-018/023: production countdowns measured against their spec bounds.
TEST(TransientOverlay, ProductionCountdownBounds) {
  OverlayFixture fixture;
  ASSERT_TRUE(fixture.load());
  EXPECT_EQ(fixture.arrowTimer->property("interval").toInt(), 2000);
  EXPECT_EQ(fixture.detailsTimer->property("interval").toInt(), 3000);
  ASSERT_TRUE(fixture.openFolder());
  EXPECT_TRUE(fixture.hudShown());
  EXPECT_TRUE(fixture.arrowsHidden());

  QElapsedTimer clock;
  fixture.pointerOverCanvas();
  clock.start();
  EXPECT_TRUE(fixture.arrowsShown());
  EXPECT_TRUE(fixture.hudShown());
  waitUntil(clock, 1750);
  EXPECT_TRUE(fixture.arrowsShown());
  waitUntil(clock, 2250);
  EXPECT_TRUE(fixture.arrowsHidden());
  waitUntil(clock, 2750);
  EXPECT_TRUE(fixture.hudShown());
  waitUntil(clock, 3250);
  EXPECT_FALSE(fixture.hudShown());
}

// REQ-F-018/023: a second trigger restarts the countdown (scaled intervals, same proportions).
TEST(TransientOverlay, SecondTriggerRestartsCountdown) {
  OverlayFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openFolder());
  fixture.arrowTimer->setProperty("interval", 400);
  fixture.detailsTimer->setProperty("interval", 600);

  QElapsedTimer clock;
  fixture.pointerOverCanvas();
  clock.start();
  waitUntil(clock, 300);
  fixture.pointerOverCanvas(10);
  waitUntil(clock, 640);
  EXPECT_TRUE(fixture.arrowsShown());
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.arrowsHidden(); }, 400));

  ASSERT_TRUE(fixture.hideHud());
  QTest::keyClick(fixture.window, Qt::Key_Plus, Qt::ControlModifier);
  clock.restart();
  EXPECT_TRUE(fixture.hudShown());
  waitUntil(clock, 400);
  QTest::keyClick(fixture.window, Qt::Key_Minus, Qt::ControlModifier);
  waitUntil(clock, 960);
  EXPECT_TRUE(fixture.hudShown());
  EXPECT_TRUE(QTest::qWaitFor([&] { return !fixture.hudShown(); }, 600));
}

// REQ-F-019: hovering an arrow holds the countdown; leaving restarts it.
TEST(TransientOverlay, HoverPausesArrows) {
  OverlayFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openFolder());
  fixture.arrowTimer->setProperty("interval", 400);
  fixture.pointerOverCanvas();
  ASSERT_TRUE(fixture.arrowsShown());
  QTest::mouseMove(fixture.window, OverlayFixture::centre(fixture.next));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.next->property("hovered").toBool(); }));
  QTest::qWait(1200);
  EXPECT_TRUE(fixture.arrowsShown());

  QElapsedTimer clock;
  fixture.pointerOverCanvas();
  clock.start();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.next->property("hovered").toBool(); }));
  waitUntil(clock, 350);
  EXPECT_TRUE(fixture.arrowsShown());
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.arrowsHidden(); }, 300));
}

// REQ-F-020/021/025: keyboard hides at once, the fade runs after `shown`, and a faded arrow is inert.
TEST(TransientOverlay, KeyboardHidesAndFadedArrowsAreInert) {
  QAccessible::setActive(true);
  OverlayFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openFolder());
  // Rest the pointer on the arrow so the later press needs no pointer move to reach it.
  const auto nextPoint = OverlayFixture::centre(fixture.next);
  QTest::mouseMove(fixture.window, nextPoint);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.next->opacity() == 1.0 && fixture.arrowsShown(); }));
  auto* root = QAccessible::queryAccessibleInterface(fixture.window);
  EXPECT_TRUE(hasPerceivableNode(root, "Next image"));

  QElapsedTimer clock;
  QTest::keyClick(fixture.window, Qt::Key_Left);
  clock.start();
  EXPECT_TRUE(fixture.arrowsHidden());
  waitUntil(clock, 60);
  EXPECT_GT(fixture.next->opacity(), 0.0);
  EXPECT_LT(fixture.next->opacity(), 1.0);
  EXPECT_TRUE(fixture.next->isVisible());
  waitUntil(clock, 300);
  EXPECT_EQ(fixture.next->opacity(), 0.0);
  EXPECT_FALSE(fixture.next->isVisible());
  EXPECT_FALSE(fixture.previous->isVisible());
  EXPECT_FALSE(hasPerceivableNode(root, "Next image"));
  EXPECT_FALSE(hasPerceivableNode(root, "Previous image"));

  // Pressing where the arrow was must not browse.
  const auto position = fixture.document.position();
  QTest::mousePress(fixture.window, Qt::LeftButton, Qt::NoModifier, nextPoint);
  QTest::mouseRelease(fixture.window, Qt::LeftButton, Qt::NoModifier, nextPoint);
  QTest::qWait(100);
  EXPECT_EQ(fixture.document.position(), position);
  EXPECT_TRUE(fixture.arrowsHidden());

  // Keyboard navigation does not bring the arrows back.
  QTest::keyClick(fixture.window, Qt::Key_Escape);
  ASSERT_TRUE(fixture.arrowsHidden());
  QTest::keyClick(fixture.window, Qt::Key_BracketRight);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.position() == position + 1; }));
  QElapsedTimer hold;
  hold.start();
  while (hold.elapsed() < 2500) {
    ASSERT_TRUE(fixture.arrowsHidden()) << hold.elapsed();
    QTest::qWait(100);
  }
}

// REQ-F-023: every view command reveals the HUD without pointer movement; resizing does not.
TEST(TransientOverlay, CommandsRevealHudButResizeDoesNot) {
  OverlayFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openFolder());
  struct Command {
    const char* name = nullptr;
    Qt::Key key = Qt::Key_unknown;
    Qt::KeyboardModifiers modifiers;
  };
  const std::array commands = {
      Command{.name = "Ctrl++", .key = Qt::Key_Plus, .modifiers = Qt::ControlModifier},
      Command{.name = "Ctrl+-", .key = Qt::Key_Minus, .modifiers = Qt::ControlModifier},
      Command{.name = "Ctrl+0", .key = Qt::Key_0, .modifiers = Qt::ControlModifier},
      Command{.name = "1", .key = Qt::Key_1, .modifiers = Qt::NoModifier},
      Command{.name = "R", .key = Qt::Key_R, .modifiers = Qt::NoModifier},
      Command{.name = "Shift+R", .key = Qt::Key_R, .modifiers = Qt::ShiftModifier},
      Command{.name = "X", .key = Qt::Key_X, .modifiers = Qt::NoModifier},
      Command{.name = "Shift+X", .key = Qt::Key_X, .modifiers = Qt::ShiftModifier},
  };
  for (const auto& command : commands) {
    ASSERT_TRUE(fixture.hideHud());
    QTest::keyClick(fixture.window, command.key, command.modifiers);
    EXPECT_TRUE(fixture.hudShown()) << command.name;
  }

  // Menu Zoom In, triggered after the menu is already open so opening it cannot count as the trigger.
  auto* button = fixture.window->findChild<QQuickItem*>("actionsButton");
  auto* menu = fixture.window->findChild<QObject*>("actionsMenu");
  auto* zoomIn = fixture.window->findChild<QQuickItem*>("zoomInButton");
  ASSERT_TRUE(button && menu && zoomIn);
  QTest::mouseClick(fixture.window, Qt::LeftButton, Qt::NoModifier, OverlayFixture::centre(button));
  ASSERT_TRUE(QTest::qWaitFor([&] { return menu->property("opened").toBool(); }));
  ASSERT_TRUE(fixture.hideHud());
  const auto magnification = fixture.canvas->magnification();
  ASSERT_TRUE(QMetaObject::invokeMethod(zoomIn, "click"));
  EXPECT_GT(fixture.canvas->magnification(), magnification);
  EXPECT_TRUE(fixture.hudShown()) << "menu Zoom In";
  ASSERT_TRUE(QTest::qWaitFor([&] { return !menu->property("visible").toBool(); }));

  // Wheel zoom.
  ASSERT_TRUE(fixture.hideHud());
  const auto position = fixture.canvas->mapToScene({fixture.canvas->width() / 3, fixture.canvas->height() / 3});
  QWheelEvent wheel(position, fixture.window->mapToGlobal(position), {}, {0, 120}, Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QCoreApplication::sendEvent(fixture.window, &wheel);
  EXPECT_TRUE(fixture.hudShown()) << "wheel";

  // Navigation hides the HUD while leaving the Ready image, then reveals it on the next first render.
  ASSERT_TRUE(fixture.hideHud());
  QSignalSpy shownChanges(fixture.strip, SIGNAL(shownChanged()));
  fixture.window->setProperty("detailsShown", true);
  QSignalSpy rendered(fixture.canvas, &ImageCanvas::firstRendered);
  QTest::keyClick(fixture.window, Qt::Key_BracketRight);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !rendered.isEmpty(); }));
  EXPECT_TRUE(fixture.hudShown()) << "]";
  // shown went true (setup) → false (leaving) → true (first render).
  EXPECT_GE(shownChanges.count(), 3);

  // Resizing re-lays out the canvas but is not a trigger.
  ASSERT_TRUE(fixture.hideHud());
  for (const auto size : {QSize(900, 600), QSize(640, 480), QSize(1000, 700)}) {
    fixture.window->resize(size);
    ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.window->size() == size; }));
    QTest::qWait(50);
    EXPECT_FALSE(fixture.hudShown()) << size.width();
  }
}

// REQ-F-024: a drag that starts on the fading or faded HUD still pans the canvas.
TEST(TransientOverlay, DragThroughHudPans) {
  OverlayFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_TRUE(fixture.openFolder());
  QTest::keyClick(fixture.window, Qt::Key_1);
  ASSERT_TRUE(fixture.canvas->canPan());
  for (const int delay : {60, 300}) {
    QTest::keyClick(fixture.window, Qt::Key_0, Qt::ControlModifier);
    QTest::keyClick(fixture.window, Qt::Key_1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.strip->opacity() == 1.0; }));
    const auto start = OverlayFixture::centre(fixture.strip);
    fixture.window->setProperty("detailsShown", false);
    QTest::qWait(delay);
    if (delay < 150) {
      EXPECT_GT(fixture.strip->opacity(), 0.0);
      EXPECT_LT(fixture.strip->opacity(), 1.0);
    } else {
      EXPECT_FALSE(fixture.strip->isVisible());
    }
    const auto before = fixture.canvas->imageRect();
    QTest::mousePress(fixture.window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(fixture.window, start + QPoint(-20, -20));
    QTest::mouseMove(fixture.window, start + QPoint(-60, -50));
    QTest::mouseRelease(fixture.window, Qt::LeftButton, Qt::NoModifier, start + QPoint(-60, -50));
    EXPECT_NE(fixture.canvas->imageRect(), before) << delay;
  }
}

// REQ-F-022/026: arrows keep working in Error; the HUD never shows outside Ready.
TEST(TransientOverlay, EmptyAndErrorStates) {
  OverlayFixture fixture;
  ASSERT_TRUE(fixture.load());
  fixture.pointerOverCanvas();
  EXPECT_FALSE(fixture.hudShown());
  EXPECT_TRUE(fixture.arrowsShown());

  QImage image(64, 48, QImage::Format_RGB32);
  image.fill(Qt::darkCyan);
  ASSERT_TRUE(image.save(fixture.dir.filePath("a.png")));
  QFile broken(fixture.dir.filePath("b.png"));
  ASSERT_TRUE(broken.open(QIODevice::WriteOnly));
  broken.write("not an image");
  broken.close();
  ASSERT_TRUE(image.save(fixture.dir.filePath("c.png")));
  fixture.document.open({QUrl::fromLocalFile(fixture.dir.filePath("b.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] {
    return fixture.document.state() == ImageDocument::Error && !fixture.document.scanning() &&
           fixture.document.canNext();
  }));
  QTest::keyClick(fixture.window, Qt::Key_Plus, Qt::ControlModifier);
  fixture.pointerOverCanvas(12);
  EXPECT_FALSE(fixture.hudShown());
  ASSERT_TRUE(fixture.arrowsShown());
  const auto position = fixture.document.position();
  QTest::mouseClick(fixture.window, Qt::LeftButton, Qt::NoModifier, OverlayFixture::centre(fixture.next));
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.document.position() == position + 1; }));
}

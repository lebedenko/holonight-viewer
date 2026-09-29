#include "synthetic_thumbnails.h"
#include "thumbnail_provider.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QMimeData>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QTranslator>
#include <QWaitCondition>

#include <array>
#include <gtest/gtest.h>
#include <memory>

namespace {
// Main over a document whose folder listing is a fixed list, with synthetic thumbnails.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct GridModeFixture {
  GridModeFixture()
      : urls(std::make_shared<QList<QUrl>>()),
        document(
            [this](const QUrl&, const std::atomic_bool&) {
              if (decodeFails) {
                DecodeResult result;
                result.error = QStringLiteral("Broken image");
                return result;
              }
              return solidDecodeSized(imageSize);
            },
            [this](const QUrl&, const std::atomic_bool&) {
              {
                QMutexLocker lock(&scanMutex);
                while (scanBlocked) {
                  scanGate.wait(&scanMutex);
                }
              }
              return DirectoryResult{.urls = *urls, .error = {}, .missing = missingUrl};
            }) {}
  ~GridModeFixture() { blockScans(false); }

  QMutex scanMutex;
  QWaitCondition scanGate;
  bool scanBlocked = false;
  void blockScans(bool value) {
    QMutexLocker lock(&scanMutex);
    scanBlocked = value;
    if (!value) {
      scanGate.wakeAll();
    }
  }

  std::shared_ptr<QList<QUrl>> urls;
  QSize imageSize{20, 10};
  QUrl missingUrl;
  bool decodeFails = false;
  ImageDocument document;
  QQmlApplicationEngine engine;
  SyntheticDecoder decoder;
  QQuickWindow* window = nullptr;

  ::testing::AssertionResult load(int fileCount) {
    *urls = photos(fileCount);
    engine.addImageProvider(
        QStringLiteral("thumbnail"),
        new ThumbnailProvider(
            [this](const auto& request, const auto& cancelled) { return decoder(request, cancelled); }, 2));
    engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
    engine.loadFromModule("HolonightViewer", "Main");
    if (engine.rootObjects().size() != 1) {
      return ::testing::AssertionFailure() << "Main failed to load";
    }
    window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window == nullptr || !QTest::qWaitForWindowExposed(window)) {
      return ::testing::AssertionFailure() << "window not exposed";
    }
    return ::testing::AssertionSuccess();
  }

  bool open(int fileNumber) {
    document.open({photo(fileNumber)});
    return QTest::qWaitFor([&] { return !document.scanning() && document.state() == ImageDocument::Ready; }, 5000);
  }
  template <typename T = QQuickItem>
  T* find(const char* name) const {
    return window->findChild<T*>(QString::fromLatin1(name));
  }
  [[nodiscard]] bool gridMode() const { return window->property("gridMode").toBool(); }
  [[nodiscard]] QString title() const { return find("headerTitle")->property("text").toString(); }
  void key(Qt::Key key, Qt::KeyboardModifiers modifiers = {}) const {
    QTest::keyClick(window, key, modifiers);
    QTest::qWait(20);
  }
  bool actionEnabled(const char* name) const { return find<QObject>(name)->property("enabled").toBool(); }
};
}  // namespace

TEST(GridMode, CtrlGTogglesBetweenSingleViewAndGridKeepingTheImage) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  EXPECT_FALSE(fixture.gridMode());
  EXPECT_TRUE(fixture.find("imageCanvas")->isVisible());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_TRUE(fixture.gridMode());
  EXPECT_FALSE(fixture.find("imageCanvas")->isVisible());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_FALSE(fixture.gridMode());
  EXPECT_TRUE(fixture.find("imageCanvas")->isVisible());
  EXPECT_EQ(fixture.document.url(), photo(5));
  EXPECT_EQ(fixture.document.state(), ImageDocument::Ready);
}

TEST(GridMode, ShiftedCtrlGDoesNothing) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  fixture.key(Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier);
  EXPECT_FALSE(fixture.gridMode());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  fixture.key(Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier);
  EXPECT_TRUE(fixture.gridMode());
}

TEST(GridMode, CannotEnterWithoutADocument) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  EXPECT_FALSE(fixture.window->property("canEnterGrid").toBool());
  EXPECT_FALSE(fixture.window->property("canToggleGrid").toBool());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_FALSE(fixture.gridMode());
}

TEST(GridMode, HeaderNamesTheFolderAndCountOnlyInGrid) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(42));
  ASSERT_TRUE(fixture.open(3));
  EXPECT_EQ(fixture.title(), "photo_0003.png");
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_EQ(fixture.title(), QStringLiteral("photos — 42 images"));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_EQ(fixture.title(), "photo_0003.png");

  GridModeFixture single;
  ASSERT_TRUE(single.load(1));
  ASSERT_TRUE(single.open(1));
  single.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_EQ(single.title(), QStringLiteral("photos — 1 image"));
}

TEST(GridMode, HeaderPassesCountToPluralTranslation) {
  class PluralTranslator : public QTranslator {
   public:
    [[nodiscard]] bool isEmpty() const override { return false; }
    QString translate(const char* /*context*/, const char* source, const char* /*disambiguation*/,
                      int n) const override {
      if (QString::fromUtf8(source).startsWith(QStringLiteral("%1 — %n image"))) {
        return n == 2 ? QStringLiteral("%1 — %n translated dual") : QString();
      }
      return {};
    }
  } translator;
  QCoreApplication::installTranslator(&translator);
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(2));
  ASSERT_TRUE(fixture.open(1));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_EQ(fixture.title(), QStringLiteral("photos — 2 translated dual"));
  QCoreApplication::removeTranslator(&translator);
}

TEST(GridMode, MissingCurrentFileIsExcludedFromCountAndEmptyEntry) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(2));
  ASSERT_TRUE(fixture.open(1));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  fixture.missingUrl = photo(1);
  fixture.key(Qt::Key_R, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.document.scanning(); }, 5000));
  EXPECT_EQ(fixture.title(), QStringLiteral("photos — 1 image"));
  *fixture.urls = {photo(1)};
  fixture.key(Qt::Key_R, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.document.scanning(); }, 5000));
  EXPECT_EQ(fixture.title(), QStringLiteral("photos — 0 images"));
  EXPECT_TRUE(fixture.find("emptyStateGroup")->isVisible());
  fixture.key(Qt::Key_Escape);
  EXPECT_FALSE(fixture.window->property("canEnterGrid").toBool());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_FALSE(fixture.gridMode());
}

TEST(GridMode, DocumentErrorFeedbackIsHiddenUntilLeavingGrid) {
  GridModeFixture fixture;
  fixture.decodeFails = true;
  ASSERT_TRUE(fixture.load(2));
  fixture.document.open({photo(1)});
  ASSERT_TRUE(QTest::qWaitFor(
      [&] { return !fixture.document.scanning() && fixture.document.state() == ImageDocument::Error; }, 5000));
  EXPECT_TRUE(fixture.find("documentFeedback")->isVisible());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  EXPECT_FALSE(fixture.find("documentFeedback")->isVisible());
  fixture.key(Qt::Key_Escape);
  EXPECT_TRUE(fixture.find("documentFeedback")->isVisible());
}

TEST(GridMode, PictureActionsAreDisabledInGridAndReturnInSingleView) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  const std::array<const char*, 7> actions = {"rotateClockwiseAction", "rotateCounterclockwiseAction",
                                              "flipHorizontalAction",  "flipVerticalAction",
                                              "resetTransformAction",  "copyImageAction",
                                              "imageInformationAction"};
  EXPECT_TRUE(fixture.window->property("canInspect").toBool());
  for (const auto* name : actions) {
    EXPECT_TRUE(fixture.actionEnabled(name)) << name;
  }
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  EXPECT_FALSE(fixture.window->property("canInspect").toBool());
  for (const auto* name : actions) {
    EXPECT_FALSE(fixture.actionEnabled(name)) << name;
  }
  EXPECT_TRUE(fixture.find<QObject>("informationButton") != nullptr);
  EXPECT_FALSE(fixture.find<QObject>("informationButton")->property("enabled").toBool());
  // Copy Path is not a picture action and stays available.
  EXPECT_TRUE(fixture.actionEnabled("copyPathAction"));

  auto* canvas = fixture.find<QObject>("imageCanvas");
  const auto magnification = canvas->property("magnification").toReal();
  const auto orientation = fixture.document.orientation();
  fixture.key(Qt::Key_Plus, Qt::ControlModifier);
  fixture.key(Qt::Key_Minus, Qt::ControlModifier);
  fixture.key(Qt::Key_0, Qt::ControlModifier);
  fixture.key(Qt::Key_1);
  fixture.key(Qt::Key_R);
  fixture.key(Qt::Key_X);
  fixture.key(Qt::Key_I);
  EXPECT_EQ(canvas->property("magnification").toReal(), magnification);
  EXPECT_EQ(fixture.document.orientation(), orientation);
  EXPECT_FALSE(fixture.window->property("informationOpen").toBool());

  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_TRUE(fixture.window->property("canInspect").toBool());
  for (const auto* name : actions) {
    EXPECT_TRUE(fixture.actionEnabled(name)) << name;
  }
  // The tiny fixture image already sits at the maximum zoom, so only zooming out changes it.
  fixture.key(Qt::Key_Minus, Qt::ControlModifier);
  EXPECT_LT(canvas->property("magnification").toReal(), magnification);
}

TEST(GridMode, OpenFileToggleFullscreenAndRescanStayAvailable) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  const auto generation = fixture.window->property("thumbnailGeneration").toInt();
  fixture.key(Qt::Key_R, Qt::ControlModifier);
  EXPECT_EQ(fixture.window->property("thumbnailGeneration").toInt(), generation + 1);
  EXPECT_TRUE(fixture.gridMode());
  EXPECT_FALSE(fixture.window->property("modalActive").toBool());
  fixture.key(Qt::Key_Question);
  EXPECT_TRUE(fixture.window->property("helpOpen").toBool());
}

namespace {
bool waitVisibility(QQuickWindow* window, QWindow::Visibility visibility) {
  return QTest::qWaitFor([&] { return window->visibility() == visibility; }, 5000);
}
}  // namespace

TEST(GridMode, EscapeClosesTheGridBeforeLeavingFullscreen) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  const auto windowed = fixture.window->visibility();
  fixture.key(Qt::Key_F);
  ASSERT_TRUE(waitVisibility(fixture.window, QWindow::FullScreen));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  fixture.key(Qt::Key_Escape);
  EXPECT_FALSE(fixture.gridMode());
  QTest::qWait(150);
  EXPECT_EQ(fixture.window->visibility(), QWindow::FullScreen);
  // With the grid closed, Escape leaves fullscreen as before.
  fixture.key(Qt::Key_Escape);
  EXPECT_TRUE(waitVisibility(fixture.window, windowed));
  EXPECT_EQ(fixture.document.url(), photo(5));
}

TEST(GridMode, EscapeInAWindowedGridOnlyClosesTheGridAndKeepsTheOpenImage) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  QMetaObject::invokeMethod(fixture.find<QObject>("thumbnailGrid"), "select", Q_ARG(int, 9));
  QTest::qWait(30);
  fixture.key(Qt::Key_Escape);
  EXPECT_FALSE(fixture.gridMode());
  // The selection moved, but the document did not.
  EXPECT_EQ(fixture.document.url(), photo(5));
}

TEST(GridMode, HeaderAndFooterStayVisibleInFullscreenGrid) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  fixture.key(Qt::Key_F);
  ASSERT_TRUE(waitVisibility(fixture.window, QWindow::FullScreen));
  // Without pointer movement the transient reveal times out and hides them in single view.
  fixture.window->setProperty("arrowsShown", false);
  fixture.window->setProperty("countdownActive", false);
  QTest::qWait(50);
  EXPECT_FALSE(fixture.find("viewerHeader")->isVisible());
  EXPECT_FALSE(fixture.find("viewerFooter")->isVisible());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  auto* header = fixture.find("viewerHeader");
  auto* footer = fixture.find("viewerFooter");
  EXPECT_TRUE(header->isVisible());
  EXPECT_TRUE(footer->isVisible());
  // The grid sits between them instead of underneath.
  auto* view = fixture.find("thumbnailGridView");
  ASSERT_NE(view, nullptr);
  const auto top = view->mapToItem(nullptr, QPointF(0, 0)).y();
  const auto bottom = top + view->height();
  EXPECT_GE(top, header->mapToItem(nullptr, QPointF(0, header->height())).y() - 1);
  EXPECT_LE(bottom, footer->mapToItem(nullptr, QPointF(0, 0)).y() + 1);
}

TEST(GridMode, CanvasOverlaysAndAnimationAreSuspendedInGrid) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  fixture.window->setProperty("arrowsShown", true);
  fixture.window->setProperty("detailsShown", true);
  QTest::qWait(250);
  EXPECT_TRUE(fixture.find("previousButton")->isVisible());
  EXPECT_TRUE(fixture.find("nextButton")->isVisible());
  EXPECT_TRUE(fixture.find("detailsStrip")->isVisible());
  EXPECT_FALSE(fixture.document.animation()->property("suspendedByModal").toBool());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  QTest::qWait(300);
  EXPECT_FALSE(fixture.find("previousButton")->isVisible());
  EXPECT_FALSE(fixture.find("nextButton")->isVisible());
  EXPECT_FALSE(fixture.find("playPauseButton")->isVisible());
  EXPECT_FALSE(fixture.find("detailsStrip")->isVisible());
  EXPECT_TRUE(fixture.document.animation()->property("suspendedByModal").toBool());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_FALSE(fixture.document.animation()->property("suspendedByModal").toBool());
}

TEST(GridMode, OpeningAFileLeavesTheGridAndACancelledDialogDoesNot) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  auto* chooser = fixture.find<QObject>("portalFileChooser");
  ASSERT_NE(chooser, nullptr);

  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  QMetaObject::invokeMethod(chooser, "cancelled");
  QTest::qWait(30);
  EXPECT_TRUE(fixture.gridMode());

  QMetaObject::invokeMethod(chooser, "finished", Q_ARG(QList<QUrl>, (QList<QUrl>{photo(8)})));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }, 5000));
  EXPECT_FALSE(fixture.gridMode());
  EXPECT_EQ(fixture.document.url(), photo(8));
}

TEST(GridMode, DroppingAFileLeavesTheGrid) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  QMimeData mime;
  mime.setUrls({photo(9)});
  const QPoint point(fixture.window->width() / 2, fixture.window->height() / 2);
  QDragEnterEvent enter(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(fixture.window, &enter);
  QDropEvent drop(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(fixture.window, &drop);
  EXPECT_TRUE(drop.isAccepted());
  EXPECT_FALSE(fixture.gridMode());
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }, 5000));
  EXPECT_EQ(fixture.document.url(), photo(9));
}

TEST(GridMode, EmptyListingShowsTheEmptyStateInsteadOfCells) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(5));
  ASSERT_TRUE(fixture.open(1));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  EXPECT_FALSE(fixture.find("emptyStateGroup")->isVisible());
  fixture.urls->clear();
  fixture.document.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.document.scanning(); }, 5000));
  QTest::qWait(60);
  EXPECT_TRUE(fixture.find("emptyStateGroup")->isVisible());
}

namespace {
QQuickItem* gridItem(GridModeFixture& fixture) { return fixture.find("thumbnailGrid"); }
int selected(GridModeFixture& fixture) { return gridItem(fixture)->property("selectedIndex").toInt(); }
int columnCount(GridModeFixture& fixture) { return gridItem(fixture)->property("columns").toInt(); }
int pageRows(GridModeFixture& fixture) { return std::max(1, gridItem(fixture)->property("visibleRows").toInt() / 2); }

// Enters grid mode with the given (one-based) file selected.
bool enterGridAt(GridModeFixture& fixture, int fileNumber) {
  if (!fixture.open(fileNumber)) {
    return false;
  }
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  return fixture.gridMode() && selected(fixture) == fileNumber - 1;
}
}  // namespace

TEST(GridKeys, HorizontalKeysStepOneItemWithoutWrapping) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 1));
  fixture.key(Qt::Key_H);
  EXPECT_EQ(selected(fixture), 0);
  fixture.key(Qt::Key_L);
  EXPECT_EQ(selected(fixture), 1);
  fixture.key(Qt::Key_Right);
  EXPECT_EQ(selected(fixture), 2);
  fixture.key(Qt::Key_Left);
  EXPECT_EQ(selected(fixture), 1);
  fixture.key(Qt::Key_Left);
  fixture.key(Qt::Key_Left);
  EXPECT_EQ(selected(fixture), 0);
}

TEST(GridKeys, VerticalKeysMoveOneRowAndClampAtTheEnds) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(10));
  ASSERT_TRUE(enterGridAt(fixture, 5));
  const int width = columnCount(fixture);
  ASSERT_GE(width, 2);
  fixture.key(Qt::Key_J);
  EXPECT_EQ(selected(fixture), std::min(4 + width, 9));
  fixture.key(Qt::Key_K);
  EXPECT_EQ(selected(fixture), std::max(std::min(4 + width, 9) - width, 0));
  for (int i = 0; i < 6; ++i) {
    fixture.key(Qt::Key_Down);
  }
  EXPECT_GE(selected(fixture), 9 - width + 1);
  const auto last = selected(fixture);
  fixture.key(Qt::Key_Down);
  EXPECT_EQ(selected(fixture), last);
  for (int i = 0; i < 6; ++i) {
    fixture.key(Qt::Key_Up);
  }
  EXPECT_LT(selected(fixture), width);
}

TEST(GridKeys, CtrlDAndCtrlUPageByHalfTheVisibleRows) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(200));
  ASSERT_TRUE(enterGridAt(fixture, 1));
  const int step = pageRows(fixture) * columnCount(fixture);
  fixture.key(Qt::Key_D, Qt::ControlModifier);
  EXPECT_EQ(selected(fixture), step);
  fixture.key(Qt::Key_D, Qt::ControlModifier);
  EXPECT_EQ(selected(fixture), 2 * step);
  fixture.key(Qt::Key_U, Qt::ControlModifier);
  EXPECT_EQ(selected(fixture), step);
  // Ctrl+Shift+D, a bare D and Ctrl+Shift+U do nothing.
  fixture.key(Qt::Key_D, Qt::ControlModifier | Qt::ShiftModifier);
  fixture.key(Qt::Key_D);
  fixture.key(Qt::Key_U, Qt::ControlModifier | Qt::ShiftModifier);
  EXPECT_EQ(selected(fixture), step);
  for (int i = 0; i < 5; ++i) {
    fixture.key(Qt::Key_U, Qt::ControlModifier);
  }
  EXPECT_EQ(selected(fixture), 0);
}

TEST(GridKeys, GridKeysAreInertInSingleView) {
  GridModeFixture fixture;
  fixture.imageSize = {4000, 3000};
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  auto* canvas = fixture.find<QObject>("imageCanvas");
  for (const auto key : {Qt::Key_H, Qt::Key_J, Qt::Key_K, Qt::Key_L}) {
    fixture.key(key);
  }
  fixture.key(Qt::Key_D, Qt::ControlModifier);
  fixture.key(Qt::Key_U, Qt::ControlModifier);
  EXPECT_FALSE(fixture.gridMode());
  EXPECT_EQ(fixture.document.url(), photo(5));
  // The arrow keys still pan a zoomed-in image.
  fixture.key(Qt::Key_1);
  ASSERT_TRUE(canvas->property("canPan").toBool());
  const auto before = canvas->property("imageRect").toRectF();
  fixture.key(Qt::Key_Left);
  EXPECT_NE(canvas->property("imageRect").toRectF(), before);
}

TEST(GridKeys, EnterOpensTheSelectedFileInSingleView) {
  for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
    GridModeFixture fixture;
    ASSERT_TRUE(fixture.load(12));
    ASSERT_TRUE(enterGridAt(fixture, 1));
    for (int i = 0; i < 6; ++i) {
      fixture.key(Qt::Key_L);
    }
    ASSERT_EQ(selected(fixture), 6);
    fixture.key(key);
    EXPECT_FALSE(fixture.gridMode());
    ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }, 5000));
    EXPECT_EQ(fixture.document.url(), photo(7));
    EXPECT_EQ(fixture.document.position(), 7);
  }
}

TEST(GridKeys, FocusedHeaderButtonKeepsEnterAndSpace) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 3));
  auto* fullscreen = fixture.find("fullscreenButton");
  auto* actions = fixture.find("actionsButton");
  ASSERT_NE(fullscreen, nullptr);
  fullscreen->forceActiveFocus(Qt::TabFocusReason);
  ASSERT_TRUE(fullscreen->hasActiveFocus());
  const auto windowed = fixture.window->visibility();
  fixture.key(Qt::Key_Return);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.window->visibility() != windowed; }, 5000));
  EXPECT_TRUE(fixture.gridMode());
  EXPECT_EQ(fixture.document.url(), photo(3));
  fixture.key(Qt::Key_F);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.window->visibility() == windowed; }, 5000));

  actions->forceActiveFocus(Qt::TabFocusReason);
  ASSERT_TRUE(actions->hasActiveFocus());
  fixture.key(Qt::Key_Space);
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.window->property("actionsMenuOpen").toBool(); }, 5000));
  EXPECT_TRUE(fixture.gridMode());
  EXPECT_EQ(fixture.document.url(), photo(3));
}

TEST(GridKeys, TabReachesTheHeaderButtonsInGridMode) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 1));
  fixture.key(Qt::Key_Tab);
  // The information button is disabled in grid mode, so Tab skips it.
  EXPECT_TRUE(fixture.find("fullscreenButton")->hasActiveFocus());
  fixture.key(Qt::Key_Tab);
  EXPECT_TRUE(fixture.find("actionsButton")->hasActiveFocus());
  EXPECT_TRUE(fixture.gridMode());
}

TEST(GridKeys, KeyPressDropsButtonFocusSoALaterEnterOpensTheFile) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 1));
  fixture.key(Qt::Key_Tab);
  ASSERT_TRUE(fixture.find("fullscreenButton")->hasActiveFocus());
  fixture.key(Qt::Key_L);
  EXPECT_FALSE(fixture.find("fullscreenButton")->hasActiveFocus());
  EXPECT_EQ(selected(fixture), 1);
  fixture.key(Qt::Key_Return);
  EXPECT_FALSE(fixture.gridMode());
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }, 5000));
  EXPECT_EQ(fixture.document.url(), photo(2));
}

TEST(GridKeys, HoldingJThroughTwoThousandFilesStaysResponsiveAndNeverDecodesOnTheGuiThread) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(2000));
  ASSERT_TRUE(enterGridAt(fixture, 1));
  qint64 slowest = 0;
  for (int i = 0; i < 200; ++i) {
    QKeyEvent press(QEvent::KeyPress, Qt::Key_J, Qt::NoModifier, QStringLiteral("j"), true);
    QElapsedTimer timer;
    timer.start();
    QCoreApplication::sendEvent(fixture.window, &press);
    slowest = std::max(slowest, timer.nsecsElapsed());
    // Deliver queued work between presses the way the event loop does between real key repeats.
    QCoreApplication::processEvents();
  }
  EXPECT_LT(slowest, 16'000'000) << static_cast<double>(slowest) / 1000000.0 << " ms";
  EXPECT_EQ(selected(fixture), std::min(200 * columnCount(fixture), 1999));
  QTest::qWait(100);
  EXPECT_EQ(fixture.decoder.guiThreadCalls.load(), 0);
  EXPECT_GT(fixture.decoder.calls.load(), 0);
}

TEST(GridBrowse, BracketKeysMoveTheSelectionAndLeaveTheDocumentAlone) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 5));
  fixture.key(Qt::Key_BracketRight);
  EXPECT_EQ(selected(fixture), 5);
  fixture.key(Qt::Key_BracketRight);
  EXPECT_EQ(selected(fixture), 6);
  fixture.key(Qt::Key_BracketLeft);
  EXPECT_EQ(selected(fixture), 5);
  EXPECT_EQ(fixture.document.url(), photo(5));
  EXPECT_EQ(fixture.document.position(), 5);
  // No wrapping past the ends.
  for (int i = 0; i < 12; ++i) {
    fixture.key(Qt::Key_BracketLeft);
  }
  EXPECT_EQ(selected(fixture), 0);
  for (int i = 0; i < 20; ++i) {
    fixture.key(Qt::Key_BracketRight);
  }
  EXPECT_EQ(selected(fixture), 11);
  EXPECT_EQ(fixture.document.url(), photo(5));
}

TEST(GridBrowse, BracketKeysStillBrowseTheDocumentInSingleView) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  fixture.key(Qt::Key_BracketRight);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }, 5000));
  EXPECT_EQ(fixture.document.url(), photo(6));
  fixture.key(Qt::Key_BracketLeft);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }, 5000));
  EXPECT_EQ(fixture.document.url(), photo(5));
}

TEST(GridBrowse, PreviousAndNextMenuItemsMoveTheSelection) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 5));
  auto* previous = fixture.find("previousMenuItem");
  auto* next = fixture.find("nextMenuItem");
  ASSERT_NE(previous, nullptr);
  ASSERT_NE(next, nullptr);
  EXPECT_TRUE(next->isEnabled());
  QMetaObject::invokeMethod(next, "click");
  QTest::qWait(30);
  EXPECT_EQ(selected(fixture), 5);
  EXPECT_EQ(fixture.document.position(), 5);
  EXPECT_EQ(fixture.document.url(), photo(5));
  QMetaObject::invokeMethod(previous, "click");
  QMetaObject::invokeMethod(previous, "click");
  QTest::qWait(30);
  EXPECT_EQ(selected(fixture), 3);
  EXPECT_EQ(fixture.document.position(), 5);
}

TEST(GridBrowse, MenuItemsAndShortcutsFollowTheSelectionAtTheEnds) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 1));
  // The open document is the first file, but what matters here is the selection.
  QMetaObject::invokeMethod(gridItem(fixture), "select", Q_ARG(int, 11));
  QTest::qWait(30);
  EXPECT_FALSE(fixture.find("nextMenuItem")->isEnabled());
  EXPECT_TRUE(fixture.find("previousMenuItem")->isEnabled());
  QMetaObject::invokeMethod(gridItem(fixture), "select", Q_ARG(int, 0));
  QTest::qWait(30);
  EXPECT_TRUE(fixture.find("nextMenuItem")->isEnabled());
  EXPECT_FALSE(fixture.find("previousMenuItem")->isEnabled());
  // The document is at its first file, which would disable Previous in single view; in grid the selection decides.
  QMetaObject::invokeMethod(gridItem(fixture), "select", Q_ARG(int, 5));
  QTest::qWait(30);
  EXPECT_TRUE(fixture.find("previousMenuItem")->isEnabled());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_FALSE(fixture.find("previousMenuItem")->isEnabled());
  EXPECT_TRUE(fixture.find("nextMenuItem")->isEnabled());
}

TEST(GridMenu, GridViewItemIsCheckableAndFollowsTheMode) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  auto* item = fixture.find("gridToggleItem");
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->property("text").toString(), "Grid View");
  EXPECT_TRUE(item->property("checkable").toBool());
  // Nothing is open yet, so the item cannot be used.
  EXPECT_FALSE(item->isEnabled());
  EXPECT_FALSE(item->property("checked").toBool());

  ASSERT_TRUE(fixture.open(5));
  EXPECT_TRUE(item->isEnabled());
  EXPECT_FALSE(item->property("checked").toBool());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_TRUE(item->property("checked").toBool());
  EXPECT_TRUE(item->isEnabled());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_FALSE(item->property("checked").toBool());
}

TEST(GridMenu, TriggeringTheItemTogglesTheGridAndTheCheckStaysInStep) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  auto* item = fixture.find("gridToggleItem");
  ASSERT_NE(item, nullptr);
  QMetaObject::invokeMethod(item, "click");
  QTest::qWait(30);
  EXPECT_TRUE(fixture.gridMode());
  EXPECT_TRUE(item->property("checked").toBool());
  QMetaObject::invokeMethod(item, "click");
  QTest::qWait(30);
  EXPECT_FALSE(fixture.gridMode());
  EXPECT_FALSE(item->property("checked").toBool());
  // The binding survives repeated toggling, including from the keyboard.
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_TRUE(item->property("checked").toBool());
  QMetaObject::invokeMethod(item, "click");
  QTest::qWait(30);
  EXPECT_FALSE(item->property("checked").toBool());
  EXPECT_FALSE(fixture.gridMode());
}

TEST(GridMenu, ItemIsEnabledWhileTheGridIsShowing) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(3));
  ASSERT_TRUE(fixture.open(2));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  EXPECT_TRUE(fixture.find("gridToggleItem")->isEnabled());
}

TEST(GridMenu, CheckMarkAppearsOnlyWhileTheGridShows) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  auto* menu = fixture.find<QObject>("actionsMenu");
  ASSERT_TRUE(QMetaObject::invokeMethod(menu, "open"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return menu->property("opened").toBool(); }, 5000));
  auto* item = fixture.find("gridToggleItem");
  auto* check = item->findChild<QQuickItem*>("menuItemCheck");
  ASSERT_NE(check, nullptr);
  EXPECT_TRUE(check->isVisible());
  EXPECT_EQ(check->opacity(), 0);
  fixture.window->setProperty("gridMode", true);
  EXPECT_EQ(check->opacity(), 1);
  fixture.window->setProperty("gridMode", false);
  EXPECT_EQ(check->opacity(), 0);
  // Items that cannot be checked carry no mark.
  auto* open = fixture.find("openButton");
  EXPECT_FALSE(open->findChild<QQuickItem*>("menuItemCheck")->isVisible());
}

namespace {
QQuickItem* descendant(QQuickItem* root, const QString& name) {
  if (root == nullptr || root->objectName() == name) {
    return root;
  }
  for (auto* child : root->childItems()) {
    if (auto* found = descendant(child, name)) {
      return found;
    }
  }
  return nullptr;
}

// The help popup lists the grid shortcuts in a Grid section, and Esc mentions the grid.
void expectGridHelp(GridModeFixture& fixture) {
  fixture.key(Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.find("shortcutHelpContent") != nullptr; }, 5000));
  auto* content = fixture.find("shortcutHelpContent");
  const auto keycap = [&](int row) {
    auto* item = descendant(content, QStringLiteral("shortcutHelpRowGrid%1Keycap").arg(row));
    return item == nullptr ? QString() : item->property("accessibleText").toString();
  };
  EXPECT_EQ(keycap(0), "Ctrl plus G");
  EXPECT_EQ(keycap(1), "H or J or K or L");
  EXPECT_EQ(keycap(2), "Ctrl plus U");
  EXPECT_EQ(keycap(3), "Ctrl plus D");
  EXPECT_EQ(keycap(4), "Enter");
  EXPECT_NE(descendant(content, "shortcutHelpSectionLabelGrid"), nullptr);
  fixture.key(Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.window->property("helpOpen").toBool(); }, 5000));
}
}  // namespace

TEST(GridHelp, ListsTheGridShortcutsInSingleViewAndInGrid) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  ASSERT_NO_FATAL_FAILURE(expectGridHelp(fixture));
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  ASSERT_NO_FATAL_FAILURE(expectGridHelp(fixture));
  // Closing help leaves the grid as it was.
  EXPECT_TRUE(fixture.gridMode());
}

TEST(GridScan, CtrlGDuringAScanShowsBusyThenTheOpenFileIsSelected) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  fixture.blockScans(true);
  fixture.document.open({photo(9)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }, 5000));
  ASSERT_TRUE(fixture.document.scanning());
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  EXPECT_TRUE(fixture.find("gridBusy")->isVisible());
  EXPECT_FALSE(fixture.find("thumbnailGridView")->isVisible());
  // While scanning the title names only the folder.
  EXPECT_EQ(fixture.title(), "photos");
  fixture.blockScans(false);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.document.scanning(); }, 5000));
  QTest::qWait(80);
  EXPECT_FALSE(fixture.find("gridBusy")->isVisible());
  EXPECT_TRUE(fixture.find("thumbnailGridView")->isVisible());
  EXPECT_EQ(selected(fixture), 8);
  EXPECT_EQ(fixture.title(), QStringLiteral("photos — 30 images"));
}

TEST(GridScan, CtrlRInGridShowsBusyUntilTheScanCompletes) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  ASSERT_TRUE(enterGridAt(fixture, 4));
  EXPECT_FALSE(fixture.find("gridBusy")->isVisible());
  fixture.key(Qt::Key_L);
  ASSERT_EQ(selected(fixture), 4);
  fixture.blockScans(true);
  fixture.key(Qt::Key_R, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.scanning(); }, 5000));
  QTest::qWait(50);
  EXPECT_TRUE(fixture.find("gridBusy")->isVisible());
  EXPECT_TRUE(fixture.gridMode());
  for (const auto key : {Qt::Key_H, Qt::Key_J, Qt::Key_K, Qt::Key_L, Qt::Key_BracketRight, Qt::Key_Return}) {
    fixture.key(key);
    EXPECT_EQ(selected(fixture), 4);
    EXPECT_TRUE(fixture.gridMode());
  }
  fixture.key(Qt::Key_D, Qt::ControlModifier);
  EXPECT_EQ(selected(fixture), 4);
  fixture.blockScans(false);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.document.scanning(); }, 5000));
  QTest::qWait(80);
  EXPECT_FALSE(fixture.find("gridBusy")->isVisible());
  EXPECT_EQ(selected(fixture), 4);
}

TEST(GridFocus, KeyboardFocusStaysInTheWindowAcrossNavigationAndModeChanges) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(40));
  ASSERT_TRUE(fixture.open(1));
  const auto focusName = [&] {
    auto* item = fixture.window->activeFocusItem();
    return item == nullptr ? QString() : item->objectName();
  };
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  EXPECT_EQ(focusName(), "neutralFocus");
  for (const auto key : {Qt::Key_L, Qt::Key_J, Qt::Key_Down, Qt::Key_H, Qt::Key_K, Qt::Key_BracketRight}) {
    fixture.key(key);
    EXPECT_EQ(focusName(), "neutralFocus") << key;
  }
  fixture.key(Qt::Key_D, Qt::ControlModifier);
  EXPECT_EQ(focusName(), "neutralFocus");
  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_EQ(focusName(), "neutralFocus");
}

TEST(GridFeatures, PanAndPlaybackKeysAreOffInGridAndBackInSingleView) {
  GridModeFixture fixture;
  fixture.imageSize = {4000, 3000};
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(5));
  auto* router = fixture.find<QObject>("windowKeyRouter");
  ASSERT_NE(router, nullptr);
  auto* canvas = fixture.find<QObject>("imageCanvas");
  EXPECT_TRUE(router->property("imageReady").toBool());
  EXPECT_FALSE(router->property("gridActive").toBool());
  fixture.key(Qt::Key_1);
  ASSERT_TRUE(canvas->property("canPan").toBool());
  const auto before = canvas->property("imageRect").toRectF();

  fixture.key(Qt::Key_G, Qt::ControlModifier);
  ASSERT_TRUE(fixture.gridMode());
  EXPECT_FALSE(router->property("imageReady").toBool());
  EXPECT_FALSE(router->property("playbackAvailable").toBool());
  EXPECT_TRUE(router->property("gridActive").toBool());
  // The arrows move the selection, Space does nothing, and the picture underneath is untouched.
  fixture.key(Qt::Key_Right);
  fixture.key(Qt::Key_Space);
  fixture.key(Qt::Key_Left);
  EXPECT_EQ(canvas->property("imageRect").toRectF(), before);

  fixture.key(Qt::Key_G, Qt::ControlModifier);
  EXPECT_TRUE(router->property("imageReady").toBool());
  EXPECT_FALSE(router->property("gridActive").toBool());
  fixture.key(Qt::Key_Left);
  EXPECT_NE(canvas->property("imageRect").toRectF(), before);
}

TEST(GridFeatures, OpenAndQuitStillWorkInGrid) {
  GridModeFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(enterGridAt(fixture, 3));
  fixture.key(Qt::Key_O, Qt::ControlModifier);
  EXPECT_TRUE(fixture.window->property("dialogRequested").toBool());
  EXPECT_TRUE(fixture.gridMode());
  // Cancelling leaves the grid as it was.
  fixture.window->setProperty("dialogRequested", false);
  QTest::qWait(50);
  EXPECT_TRUE(fixture.gridMode());
  EXPECT_EQ(selected(fixture), 2);

  QSignalSpy closing(fixture.window, &QQuickWindow::closing);
  fixture.key(Qt::Key_Q);
  EXPECT_GE(closing.count(), 1);
}

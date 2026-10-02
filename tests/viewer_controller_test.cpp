// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "viewer_controller.h"

#include "synthetic_thumbnails.h"
#include "viewer_shortcuts.h"
#include "window_key_router.h"

#include <QKeySequence>
#include <QSignalSpy>
#include <QTest>

#include <functional>
#include <gtest/gtest.h>

TEST(ViewerController, SelectionSurvivesRefreshAndFallsBackWhenRemoved) {
  auto urls = photos(5);
  ImageDocument document(solidDecode, [&urls](const QUrl&, const std::atomic_bool&) {
    return DirectoryResult{.urls = urls, .error = {}, .missing = {}};
  });
  ViewerController controller;
  controller.setDocument(&document);
  controller.open({photo(3)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  controller.enterGrid();
  EXPECT_TRUE(controller.gridMode());
  EXPECT_EQ(controller.selection()->selectedUrl(), photo(3));
  controller.moveSelection(static_cast<int>(GridNavigation::Move::Next));
  EXPECT_EQ(controller.selection()->selectedUrl(), photo(4));
  controller.refresh();
  EXPECT_FALSE(controller.selection()->canActivate());
  controller.activateSelection();
  EXPECT_TRUE(controller.gridMode());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  EXPECT_EQ(controller.selection()->selectedUrl(), photo(4));
  urls.removeAt(3);
  controller.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  EXPECT_EQ(controller.selection()->selectedUrl(), photo(5));
  controller.activateSelection();
  EXPECT_FALSE(controller.gridMode());
  EXPECT_EQ(document.url(), photo(5));
}

TEST(ViewerController, GridAndModalSuspendPlaybackAndGateCommands) {
  ImageDocument document(solidDecode, [](const QUrl&, const std::atomic_bool&) {
    return DirectoryResult{.urls = photos(3), .error = {}, .missing = {}};
  });
  ViewerController controller;
  controller.setDocument(&document);
  controller.open({photo(2)});
  controller.enterGrid();
  EXPECT_TRUE(controller.gridMode());
  EXPECT_TRUE(document.animation()->suspendedByModal());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  controller.setModalActive(true);
  EXPECT_FALSE(controller.canNext());
  EXPECT_FALSE(controller.canToggleGrid());
  controller.moveSelection(static_cast<int>(GridNavigation::Move::Next));
  controller.activateSelection();
  controller.leaveGrid();
  EXPECT_TRUE(controller.gridMode());
  EXPECT_EQ(controller.selection()->selectedUrl(), photo(2));
  controller.setModalActive(false);
  controller.leaveGrid();
  EXPECT_EQ(document.url(), photo(2));
  EXPECT_FALSE(document.animation()->suspendedByModal());
  controller.setWindowSuspended(true);
  EXPECT_TRUE(document.animation()->suspendedByWindow());
  controller.setWindowSuspended(false);
  EXPECT_FALSE(document.animation()->suspendedByWindow());
}

TEST(ViewerShortcuts, StandardOpenAliasesAndDisplayGroups) {
  EXPECT_EQ(ViewerShortcuts::sequences(ViewerShortcuts::Open), QVariantList{static_cast<int>(QKeySequence::Open)});
  EXPECT_EQ(ViewerShortcuts::sequences(ViewerShortcuts::ZoomIn),
            (QVariantList{QStringLiteral("Ctrl++"), QStringLiteral("Ctrl+="), QStringLiteral("+")}));
  EXPECT_EQ(ViewerShortcuts::keyGroups(ViewerShortcuts::RotateCounterclockwise),
            (QVariantList{QVariantList{static_cast<int>(Qt::Key_Shift), static_cast<int>(Qt::Key_R)}}));
}

TEST(WindowKeyRouter, ExplicitTargetsIgnoreNamesSkipDisabledAndSurviveDestruction) {
  QQuickWindow window;
  WindowKeyRouter router;
  router.setWindow(&window);
  QQuickItem first(window.contentItem());
  QQuickItem disabled(window.contentItem());
  auto destroyed = std::make_unique<QQuickItem>(window.contentItem());
  QQuickItem last(window.contentItem());
  first.setObjectName("renamed");
  disabled.setEnabled(false);
  router.setHeaderFocusTargets({&first, &disabled, destroyed.get(), &last});
  destroyed.reset();
  window.show();
  ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
  first.forceActiveFocus();
  QTest::keyClick(&window, Qt::Key_Tab);
  EXPECT_EQ(window.activeFocusItem(), &last);
  QTest::keyClick(&window, Qt::Key_Backtab);
  EXPECT_EQ(window.activeFocusItem(), &first);
  router.setProperty("imageReady", true);
  router.setProperty("playbackAvailable", true);
  QSignalSpy toggle(&router, &WindowKeyRouter::playbackToggleRequested);
  QTest::keyClick(&window, Qt::Key_Space);
  EXPECT_TRUE(toggle.isEmpty());
  router.setHeaderFocusTargets({});
  router.setPlaybackButton(&first);
  QTest::keyClick(&window, Qt::Key_Space);
  EXPECT_TRUE(toggle.isEmpty());
}

TEST(ViewerController, OverlappingScansPublishOnlyScanningUntilTheLatestListing) {
  std::atomic_bool blocked{true};
  std::atomic_int scans{0};
  ImageDocument document(solidDecode, [&](const QUrl&, const std::atomic_bool& cancelled) {
    ++scans;
    while (blocked.load() && !cancelled.load()) {
      QThread::yieldCurrentThread();
    }
    return DirectoryResult{.urls = photos(4), .error = {}, .missing = {}};
  });
  ViewerController controller;
  controller.setDocument(&document);
  controller.open({photo(3)});
  controller.enterGrid();
  EXPECT_TRUE(controller.gridMode());
  EXPECT_EQ(controller.selection()->selectedUrl(), photo(3));
  bool announcedComplete = false;
  const auto connection = QObject::connect(&document, &ImageDocument::changed, [&] {
    if (!document.scanning()) {
      announcedComplete = true;
    }
  });
  controller.refresh();
  controller.refresh();
  EXPECT_FALSE(announcedComplete);
  EXPECT_FALSE(controller.selection()->canMove(static_cast<int>(GridNavigation::Move::Next)));
  EXPECT_FALSE(controller.selection()->canActivate());
  QObject::disconnect(connection);
  blocked.store(false);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  EXPECT_EQ(controller.selection()->selectedUrl(), photo(3));
  EXPECT_EQ(controller.selection()->selectedIndex(), 2);
}

TEST(ViewerController, EmptyAndErrorListingsDisableActivationAndAcceptedOpenLeavesGrid) {
  bool empty = false;
  ImageDocument document(solidDecode, [&](const QUrl&, const std::atomic_bool&) {
    return DirectoryResult{
        .urls = empty ? QList<QUrl>{} : photos(2),
        .error = empty ? QStringLiteral("Unreadable") : QString{},
        .missing = {},
    };
  });
  ViewerController controller;
  controller.setDocument(&document);
  controller.open({photo(1)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  controller.enterGrid();
  empty = true;
  controller.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  EXPECT_EQ(controller.selection()->selectedIndex(), -1);
  EXPECT_FALSE(controller.canNext());
  controller.activateSelection();
  EXPECT_TRUE(controller.gridMode());
  EXPECT_EQ(document.folderError(), "Unreadable");
  controller.open({photo(2)});
  EXPECT_FALSE(controller.gridMode());
  EXPECT_EQ(document.url(), photo(2));
}

TEST(ViewerController, NamedTransformsKeepCompositionAndRejectInvalidIntegers) {
  ImageDocument document(solidDecode, [](const QUrl&, const std::atomic_bool&) { return DirectoryResult{}; });
  document.open({photo(1)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  document.transform(ImageDocument::RotateClockwise);
  document.transform(ImageDocument::FlipHorizontal);
  EXPECT_EQ(document.orientation(),
            ImageOrientation::compose(ImageDocument::RotateClockwise, ImageDocument::FlipHorizontal));
  document.transform(8);
  EXPECT_EQ(document.orientation(),
            ImageOrientation::compose(ImageDocument::RotateClockwise, ImageDocument::FlipHorizontal));
  document.resetTransform();
  document.transform(ImageDocument::RotateClockwise);
  document.transform(ImageDocument::RotateCounterclockwise);
  EXPECT_EQ(document.orientation(), 0);
}

TEST(ViewerShortcuts, MainBindingsAndAliasesHaveSeparateDisplays) {
  EXPECT_EQ(ViewerShortcuts::sequence(ViewerShortcuts::Previous), "[");
  EXPECT_EQ(ViewerShortcuts::aliasSequences(ViewerShortcuts::Previous), QVariantList{QStringLiteral("H")});
  EXPECT_EQ(ViewerShortcuts::sequences(ViewerShortcuts::Fit),
            (QVariantList{QStringLiteral("Ctrl+0"), QStringLiteral("0")}));
  EXPECT_EQ(ViewerShortcuts::sequence(ViewerShortcuts::GridFirst), "G, G");
  EXPECT_TRUE(ViewerShortcuts::keyGroups(ViewerShortcuts::GridFirst).isEmpty());
  EXPECT_TRUE(ViewerShortcuts::aliasSequences(ViewerShortcuts::GridFirst).isEmpty());
  EXPECT_EQ(ViewerShortcuts::aliasKeyGroups(ViewerShortcuts::MenuDown),
            ViewerShortcuts::keyGroups(ViewerShortcuts::PanDown));
}

TEST(WindowKeyRouter, FirstSequenceRejectsRepeatAndCancelsOnInputFocusAndContextChanges) {
  QQuickWindow window;
  WindowKeyRouter router;
  router.setWindow(&window);
  router.setGridActive(true);
  QSignalSpy moves(&router, &WindowKeyRouter::gridMoveRequested);
  const auto press = [&](int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, bool repeat = false) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, QString(), repeat);
    QCoreApplication::sendEvent(&window, &event);
  };
  press(Qt::Key_G);
  press(Qt::Key_G, Qt::NoModifier, true);
  EXPECT_TRUE(moves.isEmpty());
  press(Qt::Key_G);
  ASSERT_EQ(moves.size(), 1);
  EXPECT_EQ(moves.takeFirst().first().toInt(), static_cast<int>(GridNavigation::Move::First));
  press(Qt::Key_G, Qt::ShiftModifier);
  ASSERT_EQ(moves.size(), 1);
  EXPECT_EQ(moves.takeFirst().first().toInt(), static_cast<int>(GridNavigation::Move::Last));
  press(Qt::Key_G, Qt::ShiftModifier, true);
  EXPECT_TRUE(moves.isEmpty());
  const auto expectCancelled = [&](const std::function<void()>& cancel) {
    press(Qt::Key_G);
    cancel();
    moves.clear();
    press(Qt::Key_G);
    EXPECT_TRUE(moves.isEmpty());
    press(Qt::Key_G);
    EXPECT_EQ(moves.size(), 1);
    moves.clear();
  };
  expectCancelled([&] { press(Qt::Key_J); });
  expectCancelled([&] {
    QKeyEvent event(QEvent::ShortcutOverride, Qt::Key_Plus, Qt::ControlModifier);
    QCoreApplication::sendEvent(&window, &event);
  });
  expectCancelled([&] {
    QEvent event(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&window, &event);
  });
  expectCancelled([&] {
    router.setModalActive(true);
    router.setModalActive(false);
  });
  expectCancelled([&] {
    router.setMenuOpen(true);
    router.setMenuOpen(false);
  });
  expectCancelled([&] {
    router.setGridActive(false);
    router.setGridActive(true);
  });
  expectCancelled([&] { QTest::mouseClick(&window, Qt::LeftButton); });
  press(Qt::Key_G);
  QTest::qWait(1050);
  press(Qt::Key_G);
  EXPECT_TRUE(moves.isEmpty());
  press(Qt::Key_G);
  EXPECT_EQ(moves.size(), 1);
}

TEST(WindowKeyRouter, MouseButtonsEmitOncePerPressAndLeaveOtherButtonsAlone) {
  QQuickWindow window;
  WindowKeyRouter router;
  router.setWindow(&window);
  window.show();
  ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
  QSignalSpy browsing(&router, &WindowKeyRouter::browseRequested);
  QTest::mouseClick(&window, Qt::BackButton);
  ASSERT_EQ(browsing.size(), 1);
  EXPECT_EQ(browsing.takeFirst().first().toInt(), -1);
  QTest::mouseClick(&window, Qt::ForwardButton);
  ASSERT_EQ(browsing.size(), 1);
  EXPECT_EQ(browsing.takeFirst().first().toInt(), 1);
  QTest::mouseClick(&window, Qt::LeftButton);
  QTest::mouseClick(&window, Qt::RightButton);
  EXPECT_TRUE(browsing.isEmpty());
  router.setModalActive(true);
  QTest::mouseClick(&window, Qt::BackButton);
  QTest::mouseClick(&window, Qt::ForwardButton);
  EXPECT_TRUE(browsing.isEmpty());
}

#include "grid_navigation.h"
#include "grid_selection_controller.h"
#include "image_document.h"
#include "synthetic_thumbnails.h"
#include "thumbnail_provider.h"
#include "thumbnail_size.h"

#include <QAccessible>
#include <QFile>
#include <QMutex>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWaitCondition>

#include <algorithm>
#include <atomic>
#include <functional>
#include <gtest/gtest.h>
#include <memory>

namespace {
// The cell has no window minimum width, so it loads standalone at any width.
struct StandaloneCell {
  explicit StandaloneCell(int width = 300) : windowWidth(width) {}

  QQmlEngine engine;
  QQuickWindow window;
  SyntheticDecoder decoder;
  std::unique_ptr<QQuickItem> cell;
  int windowWidth;

  ::testing::AssertionResult load(const QString& fileName = QStringLiteral("photo.png")) {
    engine.addImageProvider(
        QStringLiteral("thumbnail"),
        new ThumbnailProvider(
            [this](const auto& request, const auto& cancelled) { return decoder(request, cancelled); }, 2));
    QQmlComponent component(&engine);
    component.loadFromModule("HolonightViewer", "ThumbnailCell");
    cell.reset(qobject_cast<QQuickItem*>(component.createWithInitialProperties(
        {{"index", 0}, {"fileName", fileName}, {"url", QUrl::fromLocalFile("/photos/" + fileName)}})));
    if (!cell) {
      return ::testing::AssertionFailure() << component.errorString().toStdString();
    }
    if (auto* palette = engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette")) {
      window.setColor(palette->property("background").value<QColor>());
    }
    cell->setParentItem(window.contentItem());
    cell->setSize({cell->implicitWidth(), cell->implicitHeight()});
    window.resize(windowWidth, static_cast<int>(cell->height()) + 20);
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
      return ::testing::AssertionFailure() << "standalone window not exposed";
    }
    return ::testing::AssertionSuccess();
  }
  QQuickItem* part(const char* name) const { return cell->findChild<QQuickItem*>(name); }
  void setSource(const QString& path, int box = 256) const {
    cell->setProperty("source", QUrl(QStringLiteral("image://thumbnail/") + ThumbnailProvider::idFor(path, box, 0)));
  }
  [[nodiscard]] int status() const { return part("cellThumbnail")->property("status").toInt(); }
  QColor palette(const char* name) {
    return engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette")->property(name).value<QColor>();
  }
};

constexpr int kLoading = 2;
constexpr int kReady = 1;
constexpr int kError = 3;
}  // namespace

TEST(ThumbnailCell, LoadsStandaloneBelowTheWindowMinimumWidth) {
  StandaloneCell standalone(300);
  ASSERT_TRUE(standalone.load());
  EXPECT_LT(standalone.cell->width(), 420);
  for (const char* name :
       {"cellBackground", "cellFocusRing", "cellThumbnail", "cellPlaceholder", "cellBrokenGlyph", "cellLabel"}) {
    EXPECT_NE(standalone.part(name), nullptr) << name;
  }
  EXPECT_EQ(standalone.cell->implicitWidth(), 256 + (2 * standalone.cell->property("padding").toReal()));
}

TEST(ThumbnailCell, PlaceholderShowsWhileLoadingThenTheThumbnailAtItsLogicalSize) {
  StandaloneCell standalone;
  ASSERT_TRUE(standalone.load());
  standalone.decoder.block(true);
  standalone.setSource("/photos/a.png");
  ASSERT_TRUE(QTest::qWaitFor([&] { return standalone.status() == kLoading; }, 5000));
  EXPECT_TRUE(standalone.part("cellPlaceholder")->isVisible());
  EXPECT_FALSE(standalone.part("cellThumbnail")->isVisible());
  EXPECT_FALSE(standalone.part("cellBrokenGlyph")->isVisible());
  standalone.decoder.block(false);
  ASSERT_TRUE(QTest::qWaitFor([&] { return standalone.status() == kReady; }, 5000));
  EXPECT_FALSE(standalone.part("cellPlaceholder")->isVisible());
  EXPECT_TRUE(standalone.part("cellThumbnail")->isVisible());
  // A 100x50 source is not enlarged.
  EXPECT_EQ(standalone.part("cellThumbnail")->width(), 100);
  EXPECT_EQ(standalone.part("cellThumbnail")->height(), 50);
}

TEST(ThumbnailCell, LargeSourceFillsTheBoxAtLogicalSizeWhateverTheDeviceResolution) {
  StandaloneCell standalone;
  standalone.decoder.decodedSize = {320, 240};
  standalone.decoder.sourceSize = {4000, 3000};
  ASSERT_TRUE(standalone.load());
  standalone.setSource("/photos/big.png", 320);
  ASSERT_TRUE(QTest::qWaitFor([&] { return standalone.status() == kReady; }, 5000));
  EXPECT_EQ(standalone.part("cellThumbnail")->width(), 256);
  EXPECT_EQ(standalone.part("cellThumbnail")->height(), 192);
}

TEST(ThumbnailCell, BrokenGlyphReplacesTheThumbnailOnError) {
  StandaloneCell standalone;
  standalone.decoder.failing = true;
  ASSERT_TRUE(standalone.load());
  standalone.setSource("/photos/bad.png");
  ASSERT_TRUE(QTest::qWaitFor([&] { return standalone.status() == kError; }, 5000));
  EXPECT_TRUE(standalone.part("cellBrokenGlyph")->isVisible());
  EXPECT_FALSE(standalone.part("cellPlaceholder")->isVisible());
  EXPECT_FALSE(standalone.part("cellThumbnail")->isVisible());
  // The cell stays a normal item: it is still labelled and can still be selected.
  standalone.cell->setProperty("isSelected", true);
  EXPECT_TRUE(standalone.part("cellFocusRing")->isVisible());
}

TEST(ThumbnailCell, SelectionTintsBehindTheThumbnailAndDrawsTheRing) {
  StandaloneCell standalone;
  ASSERT_TRUE(standalone.load());
  standalone.setSource("/photos/a.png");
  ASSERT_TRUE(QTest::qWaitFor([&] { return standalone.status() == kReady; }, 5000));
  auto* ring = standalone.part("cellFocusRing");
  auto* background = standalone.part("cellBackground");
  EXPECT_FALSE(ring->isVisible());
  const auto padding = standalone.cell->property("padding").toReal();
  // A point inside the box but outside the centred 100x50 thumbnail shows the background.
  const QPoint corner(static_cast<int>(padding) + 4, static_cast<int>(padding) + 4);
  const QPoint center(static_cast<int>(standalone.cell->width() / 2), static_cast<int>(padding + 128));
  QTest::qWait(50);
  const auto unselected = standalone.window.grabWindow();
  EXPECT_EQ(unselected.pixelColor(center), QColor(Qt::red));

  standalone.cell->setProperty("isSelected", true);
  QTest::qWait(50);
  EXPECT_TRUE(ring->isVisible());
  EXPECT_EQ(ring->property("border").value<QObject*>()->property("width").toReal(),
            standalone.engine.singletonInstance<QObject*>("Holonight.Core", "HnMetrics")
                ->property("focusBorderWidth")
                .toReal());
  EXPECT_EQ(ring->property("border").value<QObject*>()->property("color").value<QColor>(),
            standalone.palette("borderFocus"));
  EXPECT_EQ(background->property("color").value<QColor>(), standalone.palette("surfaceSelected"));
  const auto selected = standalone.window.grabWindow();
  EXPECT_NE(selected.pixelColor(corner), unselected.pixelColor(corner));
  // The tint sits behind the picture, which is not obscured.
  EXPECT_EQ(selected.pixelColor(center), QColor(Qt::red));
}

TEST(ThumbnailCell, LongNameElidesInTheMiddleOnOneLine) {
  StandaloneCell standalone;
  ASSERT_TRUE(
      standalone.load(QStringLiteral("a-very-long-photograph-file-name-taken-on-holiday-2026-final-edit.jpeg")));
  auto* label = standalone.part("cellLabel");
  EXPECT_TRUE(label->property("truncated").toBool());
  EXPECT_EQ(label->property("elide").toInt(), Qt::ElideMiddle);
  EXPECT_EQ(label->property("maximumLineCount").toInt(), 1);
  EXPECT_EQ(label->width(), 256);
  EXPECT_GE(label->y(), standalone.cell->property("padding").toReal() + 256);
  EXPECT_EQ(label->property("lineCount").toInt(), 1);

  StandaloneCell shortName;
  ASSERT_TRUE(shortName.load(QStringLiteral("a.png")));
  EXPECT_FALSE(shortName.part("cellLabel")->property("truncated").toBool());
}

TEST(ThumbnailCell, ExposesOneListItemNamedAfterTheFile) {
  QAccessible::setActive(true);
  StandaloneCell standalone;
  ASSERT_TRUE(standalone.load(QStringLiteral("holiday.png")));
  auto* accessible = QAccessible::queryAccessibleInterface(standalone.cell.get());
  ASSERT_NE(accessible, nullptr);
  EXPECT_EQ(accessible->role(), QAccessible::ListItem);
  EXPECT_EQ(accessible->text(QAccessible::Name), QStringLiteral("holiday.png"));
  EXPECT_FALSE(accessible->state().selected);
  standalone.cell->setProperty("isSelected", true);
  EXPECT_TRUE(QAccessible::queryAccessibleInterface(standalone.cell.get())->state().selected);
  // The image, label, ring and placeholder are ignored, so the cell is the only exposed node.
  EXPECT_EQ(accessible->childCount(), 0);
}

TEST(ThumbnailCell, ClicksAndDoubleClicksAreReported) {
  StandaloneCell standalone;
  ASSERT_TRUE(standalone.load());
  QSignalSpy clicked(standalone.cell.get(), SIGNAL(clicked()));
  QSignalSpy doubleClicked(standalone.cell.get(), SIGNAL(doubleClicked()));
  const QPoint middle(static_cast<int>(standalone.cell->width() / 2), static_cast<int>(standalone.cell->height() / 2));
  QTest::mouseClick(&standalone.window, Qt::LeftButton, {}, middle);
  EXPECT_EQ(clicked.count(), 1);
  EXPECT_EQ(doubleClicked.count(), 0);
  QTest::mouseDClick(&standalone.window, Qt::LeftButton, {}, middle);
  EXPECT_EQ(doubleClicked.count(), 1);
}

namespace {
using Move = GridNavigation::Move;

void settle() {
  QTest::qWait(30);
  QCoreApplication::processEvents();
}

// A grid over a document whose folder listing is a mutable list and whose scan can be held closed.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct GridFixture {
  GridFixture()
      : urls(std::make_shared<QList<QUrl>>()), document(solidDecode, [this](const QUrl&, const std::atomic_bool&) {
          {
            QMutexLocker lock(&scanMutex);
            while (scanBlocked) {
              scanGate.wait(&scanMutex);
            }
          }
          return DirectoryResult{.urls = *urls, .error = {}, .missing = {}};
        }) {}
  ~GridFixture() { blockScans(false); }

  std::shared_ptr<QList<QUrl>> urls;
  ImageDocument document;
  GridSelectionController selection{&document};
  QQmlEngine engine;
  QQuickWindow window;
  SyntheticDecoder decoder;
  std::unique_ptr<QQuickItem> root;
  QQuickItem* grid = nullptr;
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

  ::testing::AssertionResult load(int fileCount, qreal width = 0, int rows = 4) {
    *urls = photos(fileCount);
    engine.addImageProvider(
        QStringLiteral("thumbnail"),
        new ThumbnailProvider(
            [this](const auto& request, const auto& cancelled) { return decoder(request, cancelled); }, 2));
    QQmlComponent component(&engine);
    component.setData(R"(import QtQuick
import HolonightViewer
Item {
    required property ImageDocument document
    required property GridSelectionController selection
    property alias grid: thumbnailGrid
    ThumbnailGrid {
        id: thumbnailGrid
        anchors.fill: parent
        model: document.folder
        selectionController: selection
        scanning: document.scanning
    }
})",
                      QUrl());
    root.reset(qobject_cast<QQuickItem*>(component.createWithInitialProperties(
        {{"document", QVariant::fromValue(&document)}, {"selection", QVariant::fromValue(&selection)}})));
    if (!root) {
      return ::testing::AssertionFailure() << component.errorString().toStdString();
    }
    grid = root->property("grid").value<QQuickItem*>();
    root->setParentItem(window.contentItem());
    const auto cellWidth = grid->property("cellWidth").toReal();
    const auto cellHeight = grid->property("cellHeight").toReal();
    const auto side = width > 0 ? width : (3 * cellWidth) + (cellWidth / 2);
    root->setSize({side, (rows * cellHeight) + (cellHeight / 3)});
    window.resize(static_cast<int>(root->width()), static_cast<int>(root->height()));
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
      return ::testing::AssertionFailure() << "standalone window not exposed";
    }
    return ::testing::AssertionSuccess();
  }

  bool open(int fileNumber) {
    document.open({photo(fileNumber)});
    return QTest::qWaitFor([&] { return !document.scanning() && document.state() != ImageDocument::Loading; }, 5000);
  }
  bool rescan() {
    document.refresh();
    return QTest::qWaitFor([&] { return !document.scanning() && document.state() != ImageDocument::Loading; }, 5000);
  }

  [[nodiscard]] QQuickItem* view() const { return grid->findChild<QQuickItem*>("thumbnailGridView"); }
  // Delegates are visual children of the content item, not QObject children, so findChild cannot see them.
  [[nodiscard]] QList<QQuickItem*> cells() const {
    QList<QQuickItem*> found;
    for (auto* item : view()->property("contentItem").value<QQuickItem*>()->childItems()) {
      if (item->objectName() == QLatin1String("thumbnailCell")) {
        found.append(item);
      }
    }
    return found;
  }
  [[nodiscard]] QQuickItem* cellAt(int index) const {
    for (auto* item : cells()) {
      if (item->property("index").toInt() == index) {
        return item;
      }
    }
    return nullptr;
  }
  [[nodiscard]] int selectedIndex() const { return grid->property("selectedIndex").toInt(); }
  [[nodiscard]] QUrl selectedUrl() const { return grid->property("selectedUrl").toUrl(); }
  void call(const char* method, int argument) const {
    QMetaObject::invokeMethod(grid, method, Q_ARG(int, argument));
    settle();
  }
  void enter(const QUrl& url) {
    selection.enter(url);
    QMetaObject::invokeMethod(grid, "scrollIntoView");
    settle();
  }
  // The selected cell's rectangle in the grid's coordinates.
  [[nodiscard]] QRectF selectedRect() const {
    auto* cell = cellAt(selectedIndex());
    return cell != nullptr ? QRectF(cell->mapToItem(grid, QPointF(0, 0)), QSizeF(cell->width(), cell->height()))
                           : QRectF();
  }
  [[nodiscard]] bool selectedFullyVisible() const {
    const auto rect = selectedRect();
    return !rect.isNull() && QRectF(0, 0, grid->width(), grid->height()).contains(rect);
  }
};
}  // namespace

TEST(ThumbnailGrid, ColumnsAndCentringFollowTheWidth) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  ASSERT_TRUE(fixture.open(1));
  settle();
  const auto cellWidth = fixture.grid->property("cellWidth").toReal();
  fixture.root->setWidth((3 * cellWidth) + (cellWidth / 2));
  settle();
  EXPECT_EQ(fixture.grid->property("columns").toInt(), 3);
  auto* first = fixture.cellAt(0);
  auto* third = fixture.cellAt(2);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(third, nullptr);
  const auto left = first->mapToItem(fixture.grid, QPointF(0, 0)).x();
  const auto right = fixture.grid->width() - (third->mapToItem(fixture.grid, QPointF(0, 0)).x() + third->width());
  EXPECT_NEAR(left, right, 1.0);
  EXPECT_GT(left, 0);
  // Cells are exactly one column apart.
  EXPECT_NEAR(fixture.cellAt(1)->x() - first->x(), cellWidth, 0.5);
  EXPECT_NEAR(first->width(), cellWidth, 0.5);

  fixture.root->setWidth(2 * cellWidth);
  settle();
  EXPECT_EQ(fixture.grid->property("columns").toInt(), 2);
  fixture.root->setWidth(cellWidth / 2);
  settle();
  EXPECT_EQ(fixture.grid->property("columns").toInt(), 1);
}

TEST(ThumbnailGrid, ListsEveryFileInFolderOrder) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(50));
  ASSERT_TRUE(fixture.open(1));
  settle();
  EXPECT_EQ(fixture.grid->property("count").toInt(), 50);
  const auto* model = fixture.document.folder();
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(model->urlAt(i), photo(i + 1));
  }
  auto* cell = fixture.cellAt(0);
  ASSERT_NE(cell, nullptr);
  EXPECT_EQ(cell->property("fileName").toString(), "photo_0001.png");
}

TEST(ThumbnailGrid, EnteringSelectsAndScrollsTheOpenFileIntoView) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(2000));
  ASSERT_TRUE(fixture.open(500));
  fixture.enter(photo(500));
  EXPECT_EQ(fixture.selectedIndex(), 499);
  EXPECT_EQ(fixture.selectedUrl(), photo(500));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.selectedFullyVisible(); }, 5000));
  int selected = 0;
  for (auto* cell : fixture.cells()) {
    selected += cell->property("isSelected").toBool() ? 1 : 0;
  }
  EXPECT_EQ(selected, 1);
  EXPECT_TRUE(fixture.cellAt(499)->property("isSelected").toBool());
}

TEST(ThumbnailGrid, SelectionStaysFullyVisibleWhileNavigating) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(2000));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  struct Step {
    Move move;
    int times;
  };
  for (const auto& step : {Step{.move = Move::RowDown, .times = 20}, Step{.move = Move::PageDown, .times = 5},
                           Step{.move = Move::RowUp, .times = 40}, Step{.move = Move::PageUp, .times = 5},
                           Step{.move = Move::Next, .times = 7}}) {
    for (int i = 0; i < step.times; ++i) {
      fixture.call("move", static_cast<int>(step.move));
      ASSERT_TRUE(fixture.selectedFullyVisible())
          << static_cast<int>(step.move) << " #" << i << " index " << fixture.selectedIndex();
    }
  }
  EXPECT_EQ(fixture.selectedUrl(), fixture.document.folder()->urlAt(fixture.selectedIndex()));
}

TEST(ThumbnailGrid, MovesFollowTheGridNavigationRules) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  const auto columns = fixture.grid->property("columns").toInt();
  ASSERT_EQ(columns, 3);
  const auto canMove = [&](Move move) {
    bool result = false;
    QMetaObject::invokeMethod(fixture.grid, "canMove", Q_RETURN_ARG(bool, result), Q_ARG(int, static_cast<int>(move)));
    return result;
  };
  EXPECT_FALSE(canMove(Move::Previous));
  EXPECT_FALSE(canMove(Move::RowUp));
  EXPECT_TRUE(canMove(Move::Next));
  EXPECT_TRUE(canMove(Move::PageDown));
  fixture.call("move", static_cast<int>(Move::Previous));
  EXPECT_EQ(fixture.selectedIndex(), 0);
  fixture.call("move", static_cast<int>(Move::Next));
  EXPECT_EQ(fixture.selectedIndex(), 1);
  fixture.call("move", static_cast<int>(Move::RowDown));
  EXPECT_EQ(fixture.selectedIndex(), 4);
  fixture.call("move", static_cast<int>(Move::PageDown));
  // Four visible rows move two rows.
  EXPECT_EQ(fixture.selectedIndex(), 10);
  EXPECT_EQ(fixture.selectedUrl(), photo(11));
}

TEST(ThumbnailGrid, IsAnAccessibleListOfNamedItems) {
  QAccessible::setActive(true);
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(12));
  ASSERT_TRUE(fixture.open(2));
  fixture.enter(photo(2));
  auto* list = QAccessible::queryAccessibleInterface(fixture.view());
  ASSERT_NE(list, nullptr);
  EXPECT_EQ(list->role(), QAccessible::List);
  EXPECT_EQ(list->text(QAccessible::Name), QStringLiteral("Images"));
  int selected = 0;
  ASSERT_FALSE(fixture.cells().isEmpty());
  for (auto* cell : fixture.cells()) {
    auto* accessible = QAccessible::queryAccessibleInterface(cell);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->role(), QAccessible::ListItem);
    EXPECT_EQ(accessible->text(QAccessible::Name), cell->property("fileName").toString());
    selected += accessible->state().selected ? 1 : 0;
    EXPECT_EQ(accessible->state().selected, cell->property("index").toInt() == 1);
  }
  EXPECT_EQ(selected, 1);
}

TEST(ThumbnailGrid, ShowsBusyDuringAScanThenRestoresTheSelection) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  ASSERT_TRUE(fixture.open(7));
  fixture.enter(photo(7));
  ASSERT_EQ(fixture.selectedIndex(), 6);

  fixture.blockScans(true);
  fixture.document.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.scanning(); }, 5000));
  settle();
  auto* busy = fixture.grid->findChild<QQuickItem*>("gridBusy");
  ASSERT_NE(busy, nullptr);
  EXPECT_TRUE(busy->isVisible());
  EXPECT_FALSE(fixture.view()->isVisible());
  EXPECT_FALSE(fixture.grid->property("showsEmpty").toBool());
  // The transient one-row model does not disturb the remembered selection.
  EXPECT_EQ(fixture.selectedUrl(), photo(7));

  fixture.blockScans(false);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.document.scanning(); }, 5000));
  settle();
  EXPECT_FALSE(busy->isVisible());
  EXPECT_TRUE(fixture.view()->isVisible());
  EXPECT_EQ(fixture.selectedUrl(), photo(7));
  EXPECT_EQ(fixture.selectedIndex(), 6);
  EXPECT_TRUE(fixture.selectedFullyVisible());
}

TEST(ThumbnailGrid, RescanKeepsTheSameFileAndFallsBackToTheOldIndex) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(2000));
  ASSERT_TRUE(fixture.open(10));
  fixture.enter(photo(10));
  ASSERT_EQ(fixture.selectedIndex(), 9);

  // A file that sorts before it shifts the index but not the selection.
  fixture.urls->prepend(QUrl::fromLocalFile("/photos/photo_0000.png"));
  ASSERT_TRUE(fixture.rescan());
  settle();
  EXPECT_EQ(fixture.selectedUrl(), photo(10));
  EXPECT_EQ(fixture.selectedIndex(), 10);

  // Deleting it selects the item now at the old index.
  fixture.urls->removeAll(photo(10));
  ASSERT_TRUE(fixture.rescan());
  settle();
  EXPECT_EQ(fixture.selectedIndex(), 10);
  EXPECT_EQ(fixture.selectedUrl(), fixture.document.folder()->urlAt(10));

  // With only a few files left, the last one is selected.
  *fixture.urls = {photo(1), photo(2), photo(3)};
  ASSERT_TRUE(fixture.rescan());
  settle();
  EXPECT_EQ(fixture.selectedIndex(), 2);
  EXPECT_EQ(fixture.selectedUrl(), photo(3));
}

TEST(ThumbnailGrid, EmptyFolderReportsTheEmptyState) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(5));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  EXPECT_FALSE(fixture.grid->property("showsEmpty").toBool());
  fixture.urls->clear();
  // The listing keeps the open file, so hide it the way a deleted file is hidden: nothing else is listed.
  ASSERT_TRUE(fixture.rescan());
  settle();
  EXPECT_EQ(fixture.grid->property("count").toInt(), 0);
  EXPECT_TRUE(fixture.grid->property("showsEmpty").toBool());
}

TEST(ThumbnailGrid, ClickSelectsAndDoubleClickActivates) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  QSignalSpy activated(fixture.grid, SIGNAL(activated(QUrl)));
  QSignalSpy interaction(fixture.grid, SIGNAL(selectionInteraction()));
  auto* target = fixture.cellAt(4);
  ASSERT_NE(target, nullptr);
  const auto center = target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
  QTest::mouseClick(&fixture.window, Qt::LeftButton, {}, center);
  settle();
  EXPECT_EQ(fixture.selectedIndex(), 4);
  EXPECT_EQ(activated.count(), 0);
  EXPECT_GE(interaction.count(), 1);
  QTest::mouseDClick(&fixture.window, Qt::LeftButton, {}, center);
  settle();
  ASSERT_EQ(activated.count(), 1);
  EXPECT_EQ(activated.first().first().toUrl(), photo(5));
}

TEST(ThumbnailGrid, ActivateSelectionEmitsTheSelectedUrl) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(10));
  ASSERT_TRUE(fixture.open(3));
  fixture.enter(photo(3));
  QSignalSpy activated(fixture.grid, SIGNAL(activated(QUrl)));
  QMetaObject::invokeMethod(fixture.grid, "activateSelection");
  ASSERT_EQ(activated.count(), 1);
  EXPECT_EQ(activated.first().first().toUrl(), photo(3));
}

namespace {
int indexOfPath(const QString& path) {
  // "/photos/photo_NNNN.png" is file number NNNN, at index NNNN - 1.
  return path.mid(path.lastIndexOf('_') + 1, 4).toInt() - 1;
}
}  // namespace

TEST(ThumbnailGrid, EntryDecodesOnlyTheViewportPlusAboutOnePage) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(2000));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  QTest::qWait(300);
  const auto columns = fixture.grid->property("columns").toInt();
  const auto rows = fixture.grid->property("visibleRows").toInt();
  // Partial rows count, so the bound is the instantiated cells: visible plus one page.
  const auto bound = 2 * (rows + 1) * columns;
  EXPECT_GT(fixture.decoder.calls.load(), 0);
  EXPECT_LE(fixture.decoder.calls.load(), bound);
  EXPECT_EQ(fixture.decoder.guiThreadCalls.load(), 0);
}

TEST(ThumbnailGrid, JumpingToTheEndDropsRequestsForCellsThatLeftTheLookahead) {
  GridFixture fixture;
  // Two pool threads block on the first two requests; everything else queues behind them.
  fixture.decoder.block(true);
  ASSERT_TRUE(fixture.load(2000));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.decoder.calls.load() >= 2; }, 5000));
  const auto columns = fixture.grid->property("columns").toInt();
  const auto rows = fixture.grid->property("visibleRows").toInt();
  for (int i = 0; i < 40; ++i) {
    fixture.call("select", std::min(1999, (i + 1) * 100));
  }
  ASSERT_EQ(fixture.selectedIndex(), 1999);
  fixture.decoder.block(false);
  QTest::qWait(400);
  // Cells more than the viewport plus a page above the final viewport must not have been decoded.
  const auto lowestAllowed = 2000 - (((2 * (rows + 1)) + 1) * columns);
  int stale = 0;
  const auto decoded = fixture.decoder.decodedPaths();
  for (const auto& path : decoded) {
    if (indexOfPath(path) < lowestAllowed) {
      ++stale;
    }
  }
  // Only the two requests that were already running when the jump began may still have decoded.
  EXPECT_LE(stale, 2);
  EXPECT_LE(decoded.size(), 2 + (2 * (rows + 1) * columns) + columns);
  EXPECT_EQ(fixture.decoder.guiThreadCalls.load(), 0);
}

TEST(ThumbnailGrid, RingMarksOnlyTheSelectedCellAfterNavigation) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(60));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  for (const auto move : {Move::Next, Move::RowDown, Move::RowDown, Move::PageDown}) {
    fixture.call("move", static_cast<int>(move));
    ASSERT_TRUE(fixture.selectedFullyVisible());
    int rings = 0;
    for (auto* cell : fixture.cells()) {
      auto* ring = cell->findChild<QQuickItem*>("cellFocusRing");
      ASSERT_NE(ring, nullptr);
      const bool selected = cell->property("index").toInt() == fixture.selectedIndex();
      EXPECT_EQ(ring->isVisible(), selected) << cell->property("index").toInt();
      rings += ring->isVisible() ? 1 : 0;
    }
    EXPECT_EQ(rings, 1);
  }
}

TEST(ThumbnailGrid, ChangingTheScaleFactorRequestsTheNewBoxSize) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  ASSERT_TRUE(fixture.open(1));
  fixture.enter(photo(1));
  QTest::qWait(150);
  ASSERT_TRUE(fixture.decoder.decodedBoxes().contains(256));
  const auto atOne = fixture.decoder.calls.load();
  ASSERT_TRUE(fixture.grid->setProperty("devicePixelRatio", 1.25));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.decoder.calls.load() > atOne; }, 5000));
  QTest::qWait(100);
  EXPECT_TRUE(fixture.decoder.decodedBoxes().contains(320));
  const auto atOneAndAQuarter = fixture.decoder.calls.load();
  ASSERT_TRUE(fixture.grid->setProperty("devicePixelRatio", 2.0));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.decoder.calls.load() > atOneAndAQuarter; }, 5000));
  QTest::qWait(100);
  EXPECT_TRUE(fixture.decoder.decodedBoxes().contains(512));
  // Cell sizes are logical, so they do not change with the scale factor.
  EXPECT_EQ(fixture.grid->property("cellWidth").toReal(), 256 + (2 * fixture.grid->property("cellPadding").toReal()) +
                                                              fixture.grid->property("cellSpacing").toReal());
}

TEST(ThumbnailGrid, RescanGenerationDecodesAgainWithoutTouchingTheFiles) {
  QTemporaryDir directory;
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(30));
  fixture.urls->clear();
  for (int index = 0; index < 30; ++index) {
    const auto path = directory.filePath(QStringLiteral("photo_%1.png").arg(index));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write("fixture"), 7);
    fixture.urls->append(QUrl::fromLocalFile(path));
  }
  fixture.document.open({fixture.urls->first()});
  ASSERT_TRUE(QTest::qWaitFor(
      [&] { return !fixture.document.scanning() && fixture.document.state() == ImageDocument::Ready; }));
  fixture.enter(fixture.urls->first());
  QTest::qWait(200);
  const auto first = fixture.decoder.calls.load();
  ASSERT_GT(first, 0);
  ASSERT_TRUE(fixture.grid->setProperty("generation", 1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.decoder.calls.load() >= 2 * first; }, 5000));
}

TEST(ThumbnailGrid, TheBoxSizeIsTheSharedConstantAndTheFolderIsNotWatched) {
  GridFixture fixture;
  ASSERT_TRUE(fixture.load(10));
  ASSERT_TRUE(fixture.open(1));
  settle();
  auto* metrics = fixture.engine.singletonInstance<QObject*>("HolonightViewer", "ThumbnailMetrics");
  ASSERT_NE(metrics, nullptr);
  EXPECT_EQ(metrics->property("boxSize").toInt(), kThumbnailBoxLogical);
  EXPECT_EQ(fixture.grid->property("cellWidth").toReal(), kThumbnailBoxLogical +
                                                              (2 * fixture.grid->property("cellPadding").toReal()) +
                                                              fixture.grid->property("cellSpacing").toReal());
  // The listing changes on disk, and nothing rescans until the user asks.
  fixture.urls->append(photo(11));
  fixture.urls->append(photo(12));
  QTest::qWait(300);
  EXPECT_EQ(fixture.grid->property("count").toInt(), 10);
}

TEST(ThumbnailGrid, CacheBufferStaysNonnegativeDuringTransientLayout) {
  GridFixture fixture;
  QStringList cacheWarnings;
  QObject::connect(&fixture.engine, &QQmlEngine::warnings, &fixture.engine, [&](const QList<QQmlError>& warnings) {
    for (const auto& warning : warnings) {
      if (warning.description().contains("negative cache buffer")) {
        cacheWarnings.append(warning.toString());
      }
    }
  });
  ASSERT_TRUE(fixture.load(0));
  EXPECT_GT(fixture.view()->property("cacheBuffer").toInt(), 0);
  for (const int height : {-40, 0, 1, 200}) {
    fixture.root->setHeight(height);
    QCoreApplication::processEvents();
    EXPECT_EQ(fixture.view()->property("cacheBuffer").toInt(), std::max(0, height));
  }
  EXPECT_TRUE(cacheWarnings.isEmpty()) << qPrintable(cacheWarnings.join('\n'));
}

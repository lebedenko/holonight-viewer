#include "exif_fixture.h"
#include "image_document.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <gtest/gtest.h>
#include <memory>

namespace {
// Collects QML warnings so layout tests can assert a clean load.
struct WarningCollector {
  QStringList warnings;
  QtMessageHandler previous = nullptr;
  static WarningCollector*& active() {
    static WarningCollector* collector = nullptr;
    return collector;
  }
  WarningCollector() {
    active() = this;
    previous = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& context, const QString& message) {
      if (type == QtWarningMsg || type == QtCriticalMsg) {
        active()->warnings.append(message);
      }
      active()->previous(type, context, message);
    });
  }
  WarningCollector(const WarningCollector&) = delete;
  WarningCollector& operator=(const WarningCollector&) = delete;
  WarningCollector(WarningCollector&&) = delete;
  WarningCollector& operator=(WarningCollector&&) = delete;
  ~WarningCollector() {
    qInstallMessageHandler(previous);
    active() = nullptr;
  }
};

// Standalone popup has no window minimum width, so it can be laid out below 420 px.
struct StandalonePopup {
  QTemporaryDir dir;
  ImageDocument document;
  QQmlEngine engine;
  QQuickWindow window;
  // Sized explicitly: compositors may not honor window resizes, and the popup only reads its parent.
  QQuickItem host;
  std::unique_ptr<QObject> popup;

  ::testing::AssertionResult openImage() {
    QImage image(40, 30, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return openFile(QStringLiteral("landscape.png"), png);
  }

  ::testing::AssertionResult openFile(const QString& name, const QByteArray& bytes) {
    if (!dir.isValid()) {
      return ::testing::AssertionFailure() << "temporary directory unavailable";
    }
    const auto path = dir.filePath(name);
    if (!QFileInfo(path).dir().mkpath(QStringLiteral("."))) {
      return ::testing::AssertionFailure() << "fixture directory could not be created for " << path.toStdString();
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
      return ::testing::AssertionFailure() << "fixture image not written";
    }
    file.close();
    document.open({QUrl::fromLocalFile(path)});
    if (!QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; })) {
      return ::testing::AssertionFailure() << "document did not become Ready";
    }
    return ::testing::AssertionSuccess();
  }

  ::testing::AssertionResult load(int width, int height) {
    window.resize(qMax(width, 480), qMax(height, 480));
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
      return ::testing::AssertionFailure() << "standalone window not exposed";
    }
    window.requestActivate();
    if (!QTest::qWaitForWindowActive(&window)) {
      return ::testing::AssertionFailure() << "standalone window not active";
    }
    host.setParentItem(window.contentItem());
    host.setSize(QSizeF(width, height));
    QQmlComponent component(&engine);
    component.loadFromModule("HolonightViewer", "ImageInformationPopup");
    popup.reset(component.createWithInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)},
                                                       {QStringLiteral("parent"), QVariant::fromValue(&host)}}));
    if (!popup) {
      return ::testing::AssertionFailure() << component.errorString().toStdString();
    }
    QMetaObject::invokeMethod(popup.get(), "open");
    if (!QTest::qWaitFor([&] { return popup->property("opened").toBool(); })) {
      return ::testing::AssertionFailure() << "popup did not open";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] QQuickItem* item(const char* name) const {
    auto* content = popup->property("contentItem").value<QQuickItem*>();
    return content == nullptr ? nullptr : content->findChild<QQuickItem*>(QString::fromLatin1(name));
  }
};
}  // namespace

// REQ-NF-001: width is min(480, window width − 24).
TEST(ImageInformationPopup, WidthFollowsWindow) {
  for (const auto& [windowWidth, expected] : {std::pair{400, 376}, std::pair{500, 476}, std::pair{1920, 480}}) {
    StandalonePopup fixture;
    ASSERT_TRUE(fixture.openImage());
    ASSERT_TRUE(fixture.load(windowWidth, 600));
    EXPECT_DOUBLE_EQ(fixture.popup->property("width").toReal(), expected) << windowWidth;
  }
}

// REQ-F-007, REQ-NF-003: preview shows only for a Ready document in a window at least 360 px wide.
TEST(ImageInformationPopup, PreviewNeedsReadyDocumentAndWidth) {
  StandalonePopup narrow;
  ASSERT_TRUE(narrow.openImage());
  ASSERT_TRUE(narrow.load(340, 500));
  ASSERT_NE(narrow.item("informationPreview"), nullptr);
  EXPECT_FALSE(narrow.item("informationPreview")->isVisible());

  StandalonePopup wide;
  ASSERT_TRUE(wide.openImage());
  ASSERT_TRUE(wide.load(420, 500));
  EXPECT_TRUE(wide.item("informationPreview")->isVisible());

  StandalonePopup empty;
  ASSERT_TRUE(empty.load(420, 500));
  EXPECT_FALSE(empty.item("informationPreview")->isVisible());
}

// REQ-C-007, REQ-NF-004: loads cleanly at 400 px and every text stays inside the content width.
TEST(ImageInformationPopup, StandaloneNarrowLayoutFits) {
  const WarningCollector collector;
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openImage());
  ASSERT_TRUE(fixture.load(400, 600));
  QTest::qWait(50);
  auto* content = fixture.popup->property("contentItem").value<QQuickItem*>();
  ASSERT_NE(content, nullptr);
  EXPECT_DOUBLE_EQ(content->width(), 376 - 32);
  int checked = 0;
  for (auto* item : content->findChildren<QQuickItem*>()) {
    if (!item->isVisible() || !item->property("text").isValid()) {
      continue;
    }
    const auto right = item->mapToItem(content, QPointF(item->width(), 0)).x();
    EXPECT_LE(right, content->width() + 0.5) << item->objectName().toStdString();
    ++checked;
  }
  EXPECT_GE(checked, 4);
  EXPECT_TRUE(collector.warnings.isEmpty()) << collector.warnings.join('\n').toStdString();
}

namespace {
QQuickItem* findItem(QQuickItem* root, const QString& name) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == name) {
    return root;
  }
  for (auto* child : root->childItems()) {
    if (auto* found = findItem(child, name)) {
      return found;
    }
  }
  return nullptr;
}

QPoint popupCenter(const StandalonePopup& fixture) {
  auto* content = fixture.popup->property("contentItem").value<QQuickItem*>();
  return content->mapToScene(QPointF(content->width() / 2, content->height() / 2)).toPoint();
}
}  // namespace

// REQ-F-015: the × button closes the popup.
TEST(ImageInformationPopup, CloseButtonCloses) {
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openImage());
  ASSERT_TRUE(fixture.load(480, 480));
  auto* button = fixture.item("informationCloseButton");
  ASSERT_NE(button, nullptr);
  EXPECT_EQ(button->property("display").toInt(), 0);  // AbstractButton.IconOnly
  QTest::mouseClick(&fixture.window, Qt::LeftButton, {},
                    button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
  EXPECT_TRUE(QTest::qWaitFor([&] { return !fixture.popup->property("visible").toBool(); }));
}

// REQ-F-016: Escape closes the popup.
TEST(ImageInformationPopup, EscapeCloses) {
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openImage());
  ASSERT_TRUE(fixture.load(480, 480));
  QTest::keyClick(&fixture.window, Qt::Key_Escape);
  EXPECT_TRUE(QTest::qWaitFor([&] { return !fixture.popup->property("visible").toBool(); }));
}

// REQ-F-017: pressing the dimmed area closes the popup; pressing its content does not.
TEST(ImageInformationPopup, PressOutsideClosesInsideDoesNot) {
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openImage());
  ASSERT_TRUE(fixture.load(480, 480));
  QTest::mouseClick(&fixture.window, Qt::LeftButton, {}, popupCenter(fixture));
  QTest::qWait(50);
  EXPECT_TRUE(fixture.popup->property("visible").toBool());
  QTest::mouseClick(&fixture.window, Qt::LeftButton, {}, QPoint(3, 3));
  EXPECT_TRUE(QTest::qWaitFor([&] { return !fixture.popup->property("visible").toBool(); }));
}

// REQ-NF-005: the popup dims the window more lightly than the Basic style default Help uses.
TEST(ImageInformationPopup, ModalDimmingIsLight) {
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openImage());
  ASSERT_TRUE(fixture.load(480, 480));
  EXPECT_TRUE(fixture.popup->property("modal").toBool());
  auto* dimmer = findItem(fixture.window.contentItem(), QStringLiteral("informationDimmer"));
  ASSERT_NE(dimmer, nullptr);
  EXPECT_TRUE(dimmer->isVisible());
  EXPECT_NEAR(dimmer->property("color").value<QColor>().alphaF(), 0.22, 0.01);
}

// REQ-NF-002: at 400 × 300 with a long path, Tab + Ctrl+End/Home keep the cursor visible while header stays fixed.
TEST(ImageInformationPopup, PathCursorRemainsVisibleWithKeyboardScroll) {
  StandalonePopup fixture;
  const auto longPath =
      QStringLiteral("alpha_%1/beta_%1/gamma_%1/image-with-a-very-long-encoded-path-for-scroll-behavior-check.jpg")
          .arg(QString(80, u'd'));
  ASSERT_TRUE(fixture.openFile(longPath, ExifFixture::jpegWithApp1(ExifFixture::sonyPayload())));
  ASSERT_TRUE(fixture.load(400, 300));
  QTest::qWait(50);
  EXPECT_LE(fixture.popup->property("height").toReal(), 276);
  auto* scroll = fixture.item("informationScroll");
  auto* header = fixture.item("informationHeaderRow");
  auto* pathText = fixture.item("informationPathText");
  ASSERT_NE(scroll, nullptr);
  ASSERT_NE(header, nullptr);
  ASSERT_NE(pathText, nullptr);
  auto* flickable = scroll->property("contentItem").value<QQuickItem*>();
  ASSERT_NE(flickable, nullptr);
  const auto contentHeight = flickable->property("contentHeight").toReal();
  ASSERT_GT(contentHeight, flickable->height());

  // The initial path cursor is clipped, so focus must scroll the body to reveal it.
  const auto initialCursor = pathText->property("cursorRectangle").toRectF();
  ASSERT_GT(pathText->mapToItem(flickable, initialCursor.bottomLeft()).y(), flickable->height());
  const qreal headerY = header->mapToScene(QPointF(0, 0)).y();

  // Tab reaches the path text so keyboard commands are routed through the active text edit.
  for (int step = 0; step < 4 && !pathText->hasActiveFocus(); ++step) {
    QTest::keyClick(&fixture.window, Qt::Key_Tab);
  }
  ASSERT_TRUE(pathText->hasActiveFocus());
  QTest::qWait(50);

  auto assertCursorInView = [&](const QString& phase) {
    const auto cursorRect = pathText->property("cursorRectangle").toRectF();
    const auto cursorTop = pathText->mapToItem(flickable, cursorRect.topLeft()).y();
    const auto cursorBottom = pathText->mapToItem(flickable, cursorRect.bottomLeft()).y();
    EXPECT_GE(cursorTop, 0) << phase.toStdString();
    EXPECT_LE(cursorBottom, flickable->height()) << phase.toStdString();
    EXPECT_NEAR(header->mapToScene(QPointF(0, 0)).y(), headerY, 0.5);
  };

  assertCursorInView(QStringLiteral("Tab"));
  QTest::keyClick(&fixture.window, Qt::Key_End, Qt::ControlModifier);
  QTest::qWait(50);
  EXPECT_EQ(pathText->property("cursorPosition").toInt(), pathText->property("text").toString().size());
  assertCursorInView(QStringLiteral("Ctrl+End"));
  EXPECT_GT(flickable->property("contentY").toReal(), 0);

  QTest::keyClick(&fixture.window, Qt::Key_Home, Qt::ControlModifier);
  QTest::qWait(50);
  EXPECT_EQ(pathText->property("cursorPosition").toInt(), 0);
  assertCursorInView(QStringLiteral("Ctrl+Home"));
}

// REQ-NF-002: at 400 × 300 the sections scroll while the header row stays put.
TEST(ImageInformationPopup, BodyScrollsUnderFixedHeader) {
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openFile(QStringLiteral("camera.jpg"), ExifFixture::jpegWithApp1(ExifFixture::sonyPayload())));
  ASSERT_TRUE(fixture.load(400, 300));
  QTest::qWait(50);
  EXPECT_LE(fixture.popup->property("height").toReal(), 276);
  auto* scroll = fixture.item("informationScroll");
  auto* header = fixture.item("informationHeaderRow");
  ASSERT_NE(scroll, nullptr);
  ASSERT_NE(header, nullptr);
  auto* flickable = scroll->property("contentItem").value<QQuickItem*>();
  ASSERT_NE(flickable, nullptr);
  const auto contentHeight = flickable->property("contentHeight").toReal();
  ASSERT_GT(contentHeight, flickable->height());
  const auto headerY = header->mapToScene(QPointF(0, 0)).y();
  flickable->setProperty("contentY", contentHeight - flickable->height());
  QTest::qWait(50);
  EXPECT_GT(flickable->property("contentY").toReal(), 0);
  EXPECT_DOUBLE_EQ(header->mapToScene(QPointF(0, 0)).y(), headerY);
}

// REQ-F-003: a long file name elides in the middle instead of wrapping.
TEST(ImageInformationPopup, LongFileNameElidesInMiddle) {
  StandalonePopup fixture;
  QImage image(4, 3, QImage::Format_RGB32);
  image.fill(Qt::gray);
  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  const auto name = QString(80, u'n') + QStringLiteral("_tail.png");
  ASSERT_TRUE(fixture.openFile(name, png));
  ASSERT_TRUE(fixture.load(400, 600));
  QTest::qWait(50);
  auto* label = fixture.item("informationFileName");
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->property("elide").toInt(), Qt::ElideMiddle);
  EXPECT_TRUE(label->property("truncated").toBool());
  EXPECT_EQ(label->property("lineCount").toInt(), 1);
}

// REQ-F-008: section labels are upper case and use the secondary text color, unlike values.
TEST(ImageInformationPopup, SectionLabelsUseSecondaryColor) {
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openFile(QStringLiteral("camera.jpg"), ExifFixture::jpegWithApp1(ExifFixture::sonyPayload())));
  ASSERT_TRUE(fixture.load(480, 600));
  auto* palette = fixture.engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  const auto secondary = palette->property("textSecondary").value<QColor>();
  auto* content = fixture.popup->property("contentItem").value<QQuickItem*>();
  for (const auto& [key, text] :
       {std::pair{"Camera", "CAMERA"}, std::pair{"Location", "LOCATION"}, std::pair{"File", "FILE"}}) {
    auto* label = findItem(content, QStringLiteral("informationSectionLabel") + QString::fromLatin1(key));
    ASSERT_NE(label, nullptr) << key;
    EXPECT_EQ(label->property("text").toString(), QString::fromLatin1(text));
    EXPECT_EQ(label->property("color").value<QColor>(), secondary) << key;
  }
  auto* camera = findItem(content, QStringLiteral("informationSectionCamera"));
  ASSERT_NE(camera, nullptr);
  QQuickItem* value = nullptr;
  for (auto* child : camera->childItems()) {
    if (child->property("text").toString() == QStringLiteral("SONY ILCE-7M4")) {
      value = child;
    }
  }
  ASSERT_NE(value, nullptr);
  EXPECT_NE(value->property("color").value<QColor>(), secondary);
}

// REQ-F-020: an open popup follows document changes without warnings.
TEST(ImageInformationPopup, LiveUpdatesWhileOpen) {
  const WarningCollector collector;
  StandalonePopup fixture;
  ASSERT_TRUE(fixture.openImage());
  ASSERT_TRUE(fixture.load(480, 600));
  auto* transformed = fixture.item("informationTransformed");
  ASSERT_NE(transformed, nullptr);
  EXPECT_FALSE(transformed->isVisible());
  fixture.document.transform(1);
  ASSERT_TRUE(QTest::qWaitFor([&] { return transformed->isVisible(); }));
  EXPECT_EQ(transformed->property("text").toString(), QStringLiteral("Rotated view 30 × 40"));
  fixture.document.resetTransform();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !transformed->isVisible(); }));

  fixture.document.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }));
  EXPECT_EQ(fixture.item("informationSummary")->property("text").toString(), fixture.document.summaryLine());

  // Switching images passes through Loading, hiding and restoring the preview and sections.
  auto* const popupContent = fixture.popup->property("contentItem").value<QQuickItem*>();
  EXPECT_EQ(findItem(popupContent, QStringLiteral("informationSectionCamera")), nullptr);
  ASSERT_TRUE(fixture.openFile(QStringLiteral("camera.jpg"), ExifFixture::jpegWithApp1(ExifFixture::sonyPayload())));
  ASSERT_TRUE(
      QTest::qWaitFor([&] { return findItem(popupContent, QStringLiteral("informationSectionCamera")) != nullptr; }));
  EXPECT_EQ(fixture.item("informationFileName")->property("text").toString(), QStringLiteral("camera.jpg"));
  EXPECT_TRUE(fixture.item("informationPreview")->isVisible());
  fixture.document.open({QUrl::fromLocalFile(fixture.dir.filePath(QStringLiteral("landscape.png")))});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }));
  ASSERT_TRUE(
      QTest::qWaitFor([&] { return findItem(popupContent, QStringLiteral("informationSectionCamera")) == nullptr; }));
  EXPECT_EQ(fixture.item("informationFileName")->property("text").toString(), QStringLiteral("landscape.png"));
  EXPECT_TRUE(fixture.popup->property("visible").toBool());
  EXPECT_TRUE(collector.warnings.isEmpty()) << collector.warnings.join('\n').toStdString();
}

// REQ-NF-005: in the viewer, Image Information dims less than Shortcut Help.
TEST(ImageInformationPopup, DimsLessThanShortcutHelp) {
  StandalonePopup files;
  ASSERT_TRUE(files.openImage());
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&files.document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  // The dimmer is the visible translucent rectangle covering the whole window.
  const auto dimmerAlpha = [&]() -> qreal {
    std::function<qreal(QQuickItem*)> find = [&](QQuickItem* item) -> qreal {
      const auto color = item->property("color").value<QColor>();
      if (item->isVisible() && item->inherits("QQuickRectangle") && item->width() >= window->width() &&
          item->height() >= window->height() && color.alphaF() > 0 && color.alphaF() < 1) {
        return color.alphaF();
      }
      for (auto* child : item->childItems()) {
        if (const auto alpha = find(child); alpha >= 0) {
          return alpha;
        }
      }
      return -1;
    };
    return find(window->contentItem());
  };

  QTest::keyClick(window, Qt::Key_I);
  ASSERT_TRUE(QTest::qWaitFor([&] { return dimmerAlpha() > 0; }));
  const auto information = dimmerAlpha();
  QTest::keyClick(window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
  ASSERT_TRUE(QTest::qWaitFor([&] { return dimmerAlpha() < 0; }));

  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return dimmerAlpha() > 0; }));
  const auto help = dimmerAlpha();
  EXPECT_LT(information, help);
  EXPECT_NEAR(information, 0.22, 0.01);
  EXPECT_NEAR(help, 0.5, 0.01);
  QTest::keyClick(window, Qt::Key_Escape);
  window->close();
}

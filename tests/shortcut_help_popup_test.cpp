#include "image_document.h"

#include <QAccessible>
#include <QColor>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <gtest/gtest.h>
#include <memory>

namespace {
struct Row {
  const char* key;
  const char* description;
  bool keycap;
};
struct Section {
  const char* key;
  const char* label;
  std::vector<Row> rows;
};

// REQ-F-012/013 as committed in DESIGN §3.4.
const std::vector<Section>& expectedSections() {
  static const std::vector<Section> sections = {
      Section{.key = "Navigation",
              .label = "Navigation",
              .rows = {Row{.key = "Ctrl+O", .description = "Open image", .keycap = true},
                       Row{.key = "[ / ]", .description = "Previous / next image", .keycap = true},
                       Row{.key = "Ctrl+R", .description = "Refresh folder and image", .keycap = true}}},
      Section{.key = "View",
              .label = "View",
              .rows = {Row{.key = "Ctrl+0", .description = "Fit", .keycap = true},
                       Row{.key = "1", .description = "Actual size", .keycap = true},
                       Row{.key = "Ctrl++ / Ctrl+−", .description = "Zoom", .keycap = true},
                       Row{.key = "F", .description = "Fullscreen", .keycap = true},
                       Row{.key = "Esc", .description = "Close dialog or leave fullscreen", .keycap = true}}},
      Section{.key = "Transform",
              .label = "Transform",
              .rows = {Row{.key = "R / Shift+R", .description = "Rotate clockwise/counterclockwise", .keycap = true},
                       Row{.key = "X / Shift+X", .description = "Flip horizontally/vertically", .keycap = true}}},
      Section{.key = "Image",
              .label = "Image",
              .rows = {Row{.key = "I", .description = "Image information", .keycap = true},
                       Row{.key = "Ctrl+C", .description = "Copy image", .keycap = true},
                       Row{.key = "Ctrl+Shift+C", .description = "Copy path", .keycap = true}}},
      Section{.key = "Application",
              .label = "Application",
              .rows = {Row{.key = "?", .description = "Toggle this help", .keycap = true},
                       Row{.key = "Q", .description = "Quit", .keycap = true}}},
      Section{.key = "Mouse",
              .label = "Mouse",
              .rows = {Row{.key = "Wheel/touchpad scroll", .description = "Zoom at pointer", .keycap = false},
                       Row{.key = "Left-drag", .description = "Pan", .keycap = false},
                       Row{.key = "Arrow keys", .description = "Pan", .keycap = true},
                       Row{.key = "Drop an image", .description = "Open image", .keycap = false}}},
  };
  return sections;
}

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

void collectItems(QQuickItem* root, const std::function<void(QQuickItem*)>& visit) {
  visit(root);
  for (auto* child : root->childItems()) {
    collectItems(child, visit);
  }
}

void collectAccessibleNames(QAccessibleInterface* node, QStringList& names, QList<QAccessible::Role>& roles) {
  if (node == nullptr) {
    return;
  }
  names.append(node->text(QAccessible::Name));
  roles.append(node->role());
  for (int index = 0; index < node->childCount(); ++index) {
    collectAccessibleNames(node->child(index), names, roles);
  }
}

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

// The popup only reads its parent's size, so a sized host stands in for windows of any width.
struct StandaloneHelp {
  QQmlEngine engine;
  QQuickWindow window;
  QQuickItem host;
  std::unique_ptr<QObject> popup;

  ::testing::AssertionResult load(int width, int height) {
    window.resize(qMax(width, 480), qMax(height, 480));
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
      return ::testing::AssertionFailure() << "window not exposed";
    }
    window.requestActivate();
    if (!QTest::qWaitForWindowActive(&window)) {
      return ::testing::AssertionFailure() << "window not active";
    }
    host.setParentItem(window.contentItem());
    host.setSize(QSizeF(width, height));
    QQmlComponent component(&engine);
    component.loadFromModule("HolonightViewer", "ShortcutHelpPopup");
    popup.reset(component.createWithInitialProperties({{QStringLiteral("parent"), QVariant::fromValue(&host)}}));
    if (!popup) {
      return ::testing::AssertionFailure() << component.errorString().toStdString();
    }
    QMetaObject::invokeMethod(popup.get(), "open");
    if (!QTest::qWaitFor([&] { return popup->property("opened").toBool(); })) {
      return ::testing::AssertionFailure() << "popup did not open";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] QQuickItem* content() const { return popup->property("contentItem").value<QQuickItem*>(); }
  [[nodiscard]] QQuickItem* item(const QString& name) const { return findItem(content(), name); }
};

struct ViewerFixture {
  ImageDocument document;
  QQmlApplicationEngine engine;
  QQuickWindow* window = nullptr;

  void load() {
    engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
    engine.loadFromModule("HolonightViewer", "Main");
    ASSERT_EQ(engine.rootObjects().size(), 1);
    window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    ASSERT_NE(window, nullptr);
    window->requestActivate();
    ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  }

  [[nodiscard]] QQuickItem* help() const { return window->findChild<QQuickItem*>("shortcutHelpContent"); }
  [[nodiscard]] bool helpOpen() const { return window->property("helpOpen").toBool(); }
  [[nodiscard]] QQuickItem* find(const char* name) const {
    return findItem(window->contentItem(), QString::fromLatin1(name));
  }
};
}  // namespace

TEST(ShortcutHelpPopup, SectionsRowsAndRendering) {
  StandaloneHelp fixture;
  ASSERT_TRUE(fixture.load(400, 600));
  EXPECT_DOUBLE_EQ(fixture.popup->property("width").toReal(), 376);

  const auto sections = fixture.popup->property("sections").toList();
  const auto& expected = expectedSections();
  ASSERT_EQ(sections.size(), static_cast<qsizetype>(expected.size()));
  int rowCount = 0;
  auto* palette = fixture.engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  for (size_t sectionIndex = 0; sectionIndex < expected.size(); ++sectionIndex) {
    const auto section = sections[static_cast<qsizetype>(sectionIndex)].toMap();
    EXPECT_EQ(section.value("key").toString(), expected[sectionIndex].key);
    EXPECT_EQ(section.value("label").toString(), expected[sectionIndex].label);
    auto* label = fixture.item(QStringLiteral("shortcutHelpSectionLabel") + expected[sectionIndex].key);
    ASSERT_NE(label, nullptr) << expected[sectionIndex].key;
    EXPECT_EQ(label->property("text").toString(), QString::fromLatin1(expected[sectionIndex].label).toUpper());
    EXPECT_EQ(label->property("color").value<QColor>(), palette->property("textSecondary").value<QColor>());
    const auto rows = section.value("rows").toList();
    ASSERT_EQ(rows.size(), static_cast<qsizetype>(expected[sectionIndex].rows.size())) << expected[sectionIndex].key;
    for (size_t rowIndex = 0; rowIndex < expected[sectionIndex].rows.size(); ++rowIndex) {
      const auto row = rows[static_cast<qsizetype>(rowIndex)].toMap();
      const auto& want = expected[sectionIndex].rows[rowIndex];
      const auto key = row.value("key").toString();
      const auto description = row.value("description").toString();
      EXPECT_EQ(key, QString::fromUtf8(want.key));
      EXPECT_EQ(description, QString::fromUtf8(want.description));
      EXPECT_EQ(row.value("keycap").toBool(), want.keycap) << want.key;
      // REQ-F-014: no widget navigation in the help.
      EXPECT_FALSE(key.contains("Tab") || key.split(QRegularExpression("[^A-Za-z]+")).contains("J") ||
                   key.split(QRegularExpression("[^A-Za-z]+")).contains("K"))
          << want.key;
      EXPECT_FALSE(description.contains("header", Qt::CaseInsensitive) ||
                   description.contains("menu", Qt::CaseInsensitive))
          << want.description;
      const auto rowName = QStringLiteral("shortcutHelpRow%1%2").arg(expected[sectionIndex].key).arg(rowIndex);
      auto* keycap = fixture.item(rowName + "Keycap");
      auto* gesture = fixture.item(rowName + "Gesture");
      ASSERT_TRUE(keycap && gesture) << rowName.toStdString();
      EXPECT_EQ(keycap->isVisible(), want.keycap) << rowName.toStdString();
      EXPECT_EQ(gesture->isVisible(), !want.keycap) << rowName.toStdString();
      ++rowCount;
    }
  }
  EXPECT_EQ(rowCount, 19);

  auto* title = fixture.item("shortcutHelpTitle");
  ASSERT_NE(title, nullptr);
  EXPECT_EQ(title->property("text").toString(), "Shortcuts");
  // No footer and no text "Close" button; the only close affordance is the header's icon button.
  EXPECT_EQ(fixture.popup->property("footer").value<QQuickItem*>(), nullptr);
  collectItems(fixture.content(), [](QQuickItem* item) {
    if (item->inherits("QQuickAbstractButton")) {
      EXPECT_NE(item->property("text").toString(), "Close");
    }
  });
  auto* header = fixture.item("shortcutHelpHeaderRow");
  auto* close = fixture.item("shortcutHelpCloseButton");
  ASSERT_TRUE(header && close);
  EXPECT_NEAR(close->mapToItem(header, {close->width(), 0}).x(), header->width(), 0.5);
  EXPECT_NEAR(close->mapToItem(header, {0, 0}).y(), 0, 0.5);
  EXPECT_EQ(close->property("display").toInt(), 0);  // AbstractButton.IconOnly
  QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                    close->mapToScene({close->width() / 2, close->height() / 2}).toPoint());
  EXPECT_TRUE(QTest::qWaitFor([&] { return !fixture.popup->property("visible").toBool(); }));
}

TEST(ShortcutHelpPopup, WidthsAndScrollingWithFixedHeader) {
  for (const auto [host, expectedWidth] :
       {std::pair{QSize(1000, 700), 480.0}, std::pair{QSize(420, 280), 396.0}, std::pair{QSize(400, 280), 376.0}}) {
    StandaloneHelp fixture;
    ASSERT_TRUE(fixture.load(host.width(), host.height()));
    EXPECT_DOUBLE_EQ(fixture.popup->property("width").toReal(), expectedWidth) << host.width();
    if (host.height() > 280) {
      continue;
    }
    EXPECT_LE(fixture.popup->property("height").toReal(), host.height() - 24);
    auto* scroll = fixture.item("shortcutHelpScroll");
    auto* header = fixture.item("shortcutHelpHeaderRow");
    auto* body = fixture.item("shortcutHelpBody");
    ASSERT_TRUE(scroll && header && body);
    auto* flickable = scroll->property("contentItem").value<QQuickItem*>();
    ASSERT_NE(flickable, nullptr);
    EXPECT_DOUBLE_EQ(scroll->property("contentWidth").toReal(), scroll->property("availableWidth").toReal());
    EXPECT_GT(flickable->property("contentHeight").toReal(), flickable->height());
    for (auto* row : body->findChildren<QQuickItem*>()) {
      if (row->objectName().startsWith("shortcutHelpRow") &&
          !row->objectName().contains(QRegularExpression("(Key|Keycap|Gesture|Description)$"))) {
        EXPECT_LE(row->mapToItem(body, {row->width(), 0}).x(), scroll->property("availableWidth").toReal() + 0.5)
            << row->objectName().toStdString();
      }
    }
    const auto headerY = header->mapToScene({0, 0}).y();
    flickable->setProperty("contentY", flickable->property("contentHeight").toReal() - flickable->height());
    QTest::qWait(30);
    EXPECT_GT(flickable->property("contentY").toReal(), 0);
    EXPECT_DOUBLE_EQ(header->mapToScene({0, 0}).y(), headerY);
  }
}

// Check the rendered children, not just the row bounds: a naturally sized keycap can
// overflow its layout slot even when the containing row fits the popup.
TEST(ShortcutHelpPopup, KeycapsStayWithinColumns) {
  WarningCollector collector;
  for (const auto size : {QSize(1000, 700), QSize(420, 700), QSize(420, 280), QSize(400, 280)}) {
    StandaloneHelp fixture;
    ASSERT_TRUE(fixture.load(size.width(), size.height()));
    for (const int pointSize : {12, 18}) {
      collectItems(fixture.content(), [pointSize](QQuickItem* item) {
        if (item->objectName().endsWith("Keycap")) {
          auto font = item->property("font").value<QFont>();
          font.setPointSize(pointSize);
          item->setProperty("font", font);
        }
      });
      QTest::qWait(50);
      for (const auto& section : expectedSections()) {
        for (size_t index = 0; index < section.rows.size(); ++index) {
          if (!section.rows[index].keycap) {
            continue;
          }
          const auto name = QStringLiteral("shortcutHelpRow%1%2").arg(section.key).arg(index);
          SCOPED_TRACE(name.toStdString() + " at " + std::to_string(size.width()) + "px / " +
                       std::to_string(pointSize) + "pt");
          auto* row = fixture.item(name);
          auto* key = fixture.item(name + "Keycap");
          auto* description = fixture.item(name + "Description");
          ASSERT_TRUE(row && key && description);
          auto* text = key->property("contentItem").value<QQuickItem*>();
          ASSERT_NE(text, nullptr);
          EXPECT_LE(key->width(), key->parentItem()->width() + 0.5);
          EXPECT_LE(key->mapToItem(row, {key->width(), 0}).x(), description->x());
          EXPECT_LE(text->property("contentWidth").toReal(), text->width() + 0.5);
          EXPECT_LE(text->property("contentHeight").toReal(), text->height() + 0.5);
          EXPECT_GE(key->mapToItem(row, {0, 0}).y(), -0.5);
          EXPECT_LE(key->mapToItem(row, {0, key->height()}).y(), row->height() + 0.5);
        }
      }
      const auto capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
      if (!capture.isEmpty() && size == QSize(420, 700)) {
        ASSERT_TRUE(fixture.window.grabWindow().save(capture + QStringLiteral("-help-%1pt.png").arg(pointSize)));
      }
    }
  }
  EXPECT_TRUE(collector.warnings.isEmpty()) << collector.warnings.join('\n').toStdString();
}

TEST(ShortcutHelpPopup, RowsAreExposedOnceToAccessibility) {
  QAccessible::setActive(true);
  StandaloneHelp fixture;
  ASSERT_TRUE(fixture.load(480, 700));
  auto* dialog = QAccessible::queryAccessibleInterface(fixture.content());
  ASSERT_NE(dialog, nullptr);
  EXPECT_EQ(dialog->role(), QAccessible::Dialog);
  EXPECT_EQ(dialog->text(QAccessible::Name), "Shortcut Help");
  auto* close = QAccessible::queryAccessibleInterface(fixture.item("shortcutHelpCloseButton"));
  ASSERT_NE(close, nullptr);
  EXPECT_EQ(close->text(QAccessible::Name), "Close Shortcut Help");

  QStringList names;
  QList<QAccessible::Role> roles;
  collectAccessibleNames(dialog, names, roles);
  for (const auto& section : expectedSections()) {
    const auto index = names.indexOf(QString::fromLatin1(section.label));
    ASSERT_GE(index, 0) << section.key;
    EXPECT_EQ(roles[index], QAccessible::Grouping) << section.key;
    for (const auto& row : section.rows) {
      const auto combined = QString::fromUtf8(row.key) + " " + QString::fromUtf8(row.description);
      EXPECT_EQ(names.count(combined), 1) << combined.toStdString();
      EXPECT_EQ(roles[names.indexOf(combined)], QAccessible::StaticText) << combined.toStdString();
      // Neither the keycap text nor the description label is a node of its own.
      EXPECT_FALSE(names.contains(QString::fromUtf8(row.key))) << row.key;
      EXPECT_FALSE(names.contains(QString::fromUtf8(row.description))) << row.description;
    }
  }
}

TEST(ShortcutHelpPopup, ToggleCloseDimmerAndModality) {
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/help-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  QImage image(3000, 2000, QImage::Format_RGB32);
  image.fill(Qt::darkCyan);
  for (const auto* name : {"a.png", "b.png", "c.png"}) {
    ASSERT_TRUE(image.save(dir.filePath(QString::fromLatin1(name))));
  }
  ViewerFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.load());
  fixture.document.open({QUrl::fromLocalFile(dir.filePath("b.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] {
    return fixture.document.state() == ImageDocument::Ready && fixture.document.canPrevious() &&
           fixture.document.canNext();
  }));
  auto* window = fixture.window;

  // ? toggles.
  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.help() && fixture.help()->isVisible(); }));
  EXPECT_TRUE(window->property("modalActive").toBool());
  auto* popup = window->findChild<QObject*>("shortcutHelpPopup");
  ASSERT_NE(popup, nullptr);
  EXPECT_DOUBLE_EQ(popup->property("width").toReal(), 480);
  auto* dimmer = fixture.find("shortcutHelpDimmer");
  ASSERT_NE(dimmer, nullptr);
  auto* palette = fixture.engine.singletonInstance<QObject*>("Holonight.Core", "HoloniightPalette");
  ASSERT_NE(palette, nullptr);
  auto shadow = palette->property("shadow").value<QColor>();
  shadow.setAlphaF(0.22F);
  EXPECT_EQ(dimmer->property("color").value<QColor>(), shadow);
  QTest::qWait(100);
  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.helpOpen(); }));
  EXPECT_FALSE(window->property("modalActive").toBool());

  // ? then Escape.
  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.helpOpen(); }));
  QTest::qWait(100);
  QTest::keyClick(window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.helpOpen(); }));

  // Press outside on the dimmer.
  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.helpOpen(); }));
  QTest::qWait(100);
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(8, window->height() / 2));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.helpOpen(); }));

  // While open, window navigation and panning are blocked; afterwards they resume.
  auto* canvas = fixture.find("imageCanvas");
  ASSERT_NE(canvas, nullptr);
  ASSERT_TRUE(QMetaObject::invokeMethod(canvas, "actualSize"));
  ASSERT_TRUE(canvas->property("canPan").toBool());
  const auto position = fixture.document.position();
  QTest::keyClick(window, Qt::Key_Question);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.helpOpen(); }));
  QTest::qWait(100);
  const auto rect = canvas->property("imageRect").toRectF();
  QTest::keyClick(window, Qt::Key_BracketLeft);
  QTest::keyClick(window, Qt::Key_Left);
  QTest::qWait(50);
  EXPECT_EQ(fixture.document.position(), position);
  EXPECT_EQ(canvas->property("imageRect").toRectF(), rect);
  QTest::keyClick(window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
  QTest::keyClick(window, Qt::Key_Left);
  EXPECT_NE(canvas->property("imageRect").toRectF(), rect);
  QTest::keyClick(window, Qt::Key_BracketLeft);
  EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.document.position() == position - 1; }));
}

// Row layouts must settle at every card width without recursive rearrange warnings.
TEST(ShortcutHelpPopup, LayoutSettlesAcrossWidths) {
  WarningCollector collector;
  StandaloneHelp fixture;
  ASSERT_TRUE(fixture.load(1000, 700));
  auto* host = &fixture.host;
  for (int width = 1000; width >= 320; width -= 3) {
    host->setWidth(width);
    QCoreApplication::processEvents();
  }
  for (int width = 320; width <= 1000; width += 5) {
    host->setWidth(width);
    QCoreApplication::processEvents();
  }
  QTest::qWait(50);
  EXPECT_TRUE(collector.warnings.isEmpty()) << collector.warnings.join('\n').toStdString();
}

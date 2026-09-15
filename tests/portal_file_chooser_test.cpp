#include "portal_file_chooser.h"

#include "image_canvas.h"
#include "image_document.h"
#include "mock_portal.h"
#include "portal_request_builder.h"
#include "wayland_foreign_export.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>
#include <memory>

namespace {
[[nodiscard]] QDBusPendingCall callOpenFile(const QString& handleToken) {
  auto call = QDBusMessage::createMethodCall(MockPortal::service, MockPortal::path,
                                             "org.freedesktop.portal.FileChooser", "OpenFile");
  call << QString() << QStringLiteral("Open image") << QVariantMap{{"handle_token", handleToken}};
  return QDBusConnection::sessionBus().asyncCall(call);
}
}  // namespace

TEST(MockPortal, ReceivesOpenFileFromDefaultConnection) {
  MockPortal portal;
  ASSERT_TRUE(portal.start());
  const auto token = PortalRequest::newHandleToken();
  QDBusPendingReply<QDBusObjectPath> reply = callOpenFile(token);
  ASSERT_TRUE(QTest::qWaitFor([&] { return reply.isFinished(); }));
  ASSERT_FALSE(reply.isError()) << reply.error().message().toStdString();
  const auto sender = QDBusConnection::sessionBus().baseService();
  EXPECT_EQ(reply.value().path(), PortalRequest::requestPath(sender, token));
  ASSERT_EQ(portal.calls().size(), 1);
  EXPECT_EQ(portal.calls().front().sender, sender);
  EXPECT_EQ(portal.calls().front().title, "Open image");
  EXPECT_TRUE(portal.unexpectedMessages().isEmpty());
}

TEST(MockPortal, StoppedPortalIsUnknownService) {
  MockPortal portal;
  ASSERT_TRUE(portal.start());
  portal.stop();
  QDBusPendingReply<QDBusObjectPath> reply = callOpenFile(PortalRequest::newHandleToken());
  ASSERT_TRUE(QTest::qWaitFor([&] { return reply.isFinished(); }));
  EXPECT_TRUE(reply.isError());
  EXPECT_TRUE(portal.calls().isEmpty());
}

TEST(MockPortal, GuardRejectsBusWithPortalName) {
  EXPECT_TRUE(MockPortal::sessionBusIsPrivate(QDBusConnection::sessionBus()));
  MockPortal portal;
  ASSERT_TRUE(portal.start());
  const auto guard = MockPortal::sessionBusIsPrivate(QDBusConnection::sessionBus());
  EXPECT_FALSE(guard);
  EXPECT_NE(std::string(guard.message()).find("dbus-run-session"), std::string::npos);
  MockPortal second;
  EXPECT_FALSE(second.start());
}

namespace {
constexpr auto requestSettleMs = 150;

class PortalChooserFixture {
 public:
  [[nodiscard]] ::testing::AssertionResult start() {
    if (auto started = portal.start(); !started) {
      return started;
    }
    chooser.setNameFilters({"Images (*.jpg *.png)", "All files (*)"});
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] ::testing::AssertionResult requestAndAwait() {
    const auto before = portal.calls().size();
    chooser.requestOpen();
    if (!QTest::qWaitFor([&] { return chooser.awaitingResponseForTesting(); })) {
      return ::testing::AssertionFailure() << "chooser never reached AwaitingResponse";
    }
    if (portal.calls().size() != before + 1) {
      return ::testing::AssertionFailure() << "expected one OpenFile call, got " << portal.calls().size() - before;
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] int outcomes() const { return static_cast<int>(finished.size() + cancelled.size() + fallback.size()); }

  MockPortal portal;
  PortalFileChooser chooser;
  QSignalSpy finished{&chooser, &PortalFileChooser::finished};
  QSignalSpy cancelled{&chooser, &PortalFileChooser::cancelled};
  QSignalSpy fallback{&chooser, &PortalFileChooser::fallbackShownChanged};
};
}  // namespace

TEST(PortalFileChooser, SendsOneOpenFileWithRequestOptions) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  fixture.chooser.setCurrentLocalPath("/tmp/test/image.jpg");
  ASSERT_TRUE(fixture.requestAndAwait());
  ASSERT_EQ(fixture.portal.calls().size(), 1);
  const auto& call = fixture.portal.calls().front();
  EXPECT_EQ(call.parentWindow, "");
  EXPECT_EQ(call.title, "Open image");
  EXPECT_EQ(call.options.value("modal").metaType(), QMetaType::fromType<bool>());
  EXPECT_TRUE(call.options.value("modal").toBool());
  EXPECT_EQ(call.options.value("multiple").metaType(), QMetaType::fromType<bool>());
  EXPECT_FALSE(call.options.value("multiple").toBool());
  const auto token = call.options.value("handle_token").toString();
  EXPECT_EQ(call.handle, PortalRequest::requestPath(QDBusConnection::sessionBus().baseService(), token));
  const auto filters = qdbus_cast<QList<PortalFileFilter>>(call.options.value("filters"));
  EXPECT_EQ(filters, PortalRequest::filters(fixture.chooser.nameFilters()));
  ASSERT_FALSE(filters.isEmpty());
  EXPECT_EQ(qdbus_cast<PortalFileFilter>(call.options.value("current_filter")), filters.front());
  EXPECT_EQ(call.options.value("current_folder").toByteArray(), QByteArray("/tmp/test\0", 10));
  EXPECT_TRUE(fixture.portal.unexpectedMessages().isEmpty());

  fixture.chooser.requestOpen();
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.portal.calls().size(), 1);
  fixture.portal.emitResponse(call.handle, 1);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.cancelled.size() == 1; }));

  fixture.chooser.setCurrentLocalPath({});
  ASSERT_TRUE(fixture.requestAndAwait());
  const auto& second = fixture.portal.calls().back();
  EXPECT_FALSE(second.options.contains("current_folder"));
  EXPECT_NE(second.options.value("handle_token"), token);
}

TEST(PortalFileChooser, SelectionFinishesWithPortalUris) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  ASSERT_TRUE(fixture.requestAndAwait());
  const auto handle = fixture.portal.calls().front().handle;
  fixture.portal.emitResponse(handle, 0, {{"uris", QStringList{"file:///tmp/caf%C3%A9%20%2525.jpg"}}});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.finished.size() == 1; }));
  const auto urls = fixture.finished.front().front().value<QList<QUrl>>();
  ASSERT_EQ(urls.size(), 1);
  EXPECT_EQ(urls.front().toLocalFile(), "/tmp/café %25.jpg");
  EXPECT_FALSE(fixture.chooser.fallbackShown());

  // The request is over, so a repeated Response on its handle is ignored.
  fixture.portal.emitResponse(handle, 0, {{"uris", QStringList{"file:///tmp/other.jpg"}}});
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.outcomes(), 1);
}

TEST(PortalFileChooser, ResponseBeforeMethodReplyIsReceived) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  fixture.portal.setResponseBeforeReply(MockPortal::Response{.code = 1, .results = {}});
  fixture.chooser.requestOpen();
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.cancelled.size() == 1; }));
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.outcomes(), 1);
  EXPECT_FALSE(fixture.chooser.awaitingResponseForTesting());
  fixture.portal.setResponseBeforeReply(std::nullopt);
  ASSERT_TRUE(fixture.requestAndAwait());
}

TEST(PortalFileChooser, DifferingHandleIsResubscribed) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  const QString returned = "/org/freedesktop/portal/desktop/request/legacy/hn_returned";
  fixture.portal.setReturnedHandle(returned);
  ASSERT_TRUE(fixture.requestAndAwait());
  const auto& call = fixture.portal.calls().front();
  const auto predicted = PortalRequest::requestPath(QDBusConnection::sessionBus().baseService(),
                                                    call.options.value("handle_token").toString());
  ASSERT_NE(predicted, returned);
  fixture.portal.emitResponse(predicted, 0, {{"uris", QStringList{"file:///tmp/stale.jpg"}}});
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.outcomes(), 0);
  EXPECT_TRUE(fixture.chooser.awaitingResponseForTesting());
  fixture.portal.emitResponse(returned, 0, {{"uris", QStringList{"file:///tmp/fresh.jpg"}}});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.finished.size() == 1; }));
  EXPECT_EQ(fixture.finished.front().front().value<QList<QUrl>>(), QList<QUrl>{QUrl("file:///tmp/fresh.jpg")});
}

TEST(PortalFileChooser, EmptyAndNonFileUrisFinishWithoutFallback) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  ASSERT_TRUE(fixture.requestAndAwait());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 0, {{"uris", QStringList{"http://example.com"}}});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.finished.size() == 1; }));
  EXPECT_EQ(fixture.finished.back().front().value<QList<QUrl>>(), QList<QUrl>{QUrl("http://example.com")});
  ASSERT_TRUE(fixture.requestAndAwait());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 0);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.finished.size() == 2; }));
  EXPECT_TRUE(fixture.finished.back().front().value<QList<QUrl>>().isEmpty());
  EXPECT_TRUE(fixture.fallback.isEmpty());
  EXPECT_FALSE(fixture.chooser.fallbackShown());
}

TEST(PortalFileChooser, OutstandingResponseHasNoTimeout) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  fixture.chooser.setCallTimeoutMsForTesting(100);
  ASSERT_TRUE(fixture.requestAndAwait());
  QTest::qWait(400);
  EXPECT_TRUE(fixture.chooser.awaitingResponseForTesting());
  EXPECT_EQ(fixture.outcomes(), 0);
  fixture.chooser.requestOpen();
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.portal.calls().size(), 1);
}

TEST(PortalFileChooser, EndedResponseIsCancellation) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  // xdg-desktop-portal-gtk sends code 2 with empty uris when the picker is dismissed by Escape.
  ASSERT_TRUE(fixture.requestAndAwait());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 2, {{"uris", QStringList{}}});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.cancelled.size() == 1; }));
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.outcomes(), 1);
  EXPECT_FALSE(fixture.chooser.fallbackShown());
  fixture.chooser.fallbackRejected();
  EXPECT_EQ(fixture.cancelled.size(), 1);
  // Any other non-zero code is treated the same way, and the chooser stays usable.
  ASSERT_TRUE(fixture.requestAndAwait());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 7);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.cancelled.size() == 2; }));
  EXPECT_TRUE(fixture.fallback.isEmpty());
  EXPECT_TRUE(fixture.finished.isEmpty());
  EXPECT_EQ(fixture.portal.calls().size(), 2);
}

TEST(PortalFileChooser, CallFailuresShowFallbackOnce) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  const QUrl selected("file:///tmp/fallback.jpg");
  const auto expectFallback = [&](int calls) {
    fixture.chooser.requestOpen();
    EXPECT_TRUE(QTest::qWaitFor([&] { return fixture.chooser.fallbackShown(); }));
    QTest::qWait(requestSettleMs);
    EXPECT_EQ(fixture.portal.calls().size(), calls);
    EXPECT_EQ(fixture.outcomes(), 1);
    fixture.chooser.fallbackAccepted(selected);
    EXPECT_FALSE(fixture.chooser.fallbackShown());
    ASSERT_EQ(fixture.finished.size(), 1);
    EXPECT_EQ(fixture.finished.front().front().value<QList<QUrl>>(), QList<QUrl>{selected});
    fixture.chooser.fallbackAccepted(selected);
    EXPECT_EQ(fixture.finished.size(), 1);
    fixture.finished.clear();
    fixture.cancelled.clear();
    fixture.fallback.clear();
  };

  fixture.portal.setReply(MockPortal::Reply::Error);
  expectFallback(1);

  fixture.chooser.setCallTimeoutMsForTesting(200);
  fixture.portal.setReply(MockPortal::Reply::Never);
  expectFallback(2);

  // Availability is probed per request: the portal disappearing between Opens is noticed.
  fixture.chooser.setCallTimeoutMsForTesting(PortalFileChooser::defaultCallTimeoutMs);
  fixture.portal.setReply(MockPortal::Reply::Handle);
  ASSERT_TRUE(fixture.requestAndAwait());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 1);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.cancelled.size() == 1; }));
  fixture.cancelled.clear();
  fixture.portal.stop();
  expectFallback(3);
}

TEST(PortalFileChooser, MissingServiceShowsFallback) {
  PortalFileChooser chooser;
  QSignalSpy fallback(&chooser, &PortalFileChooser::fallbackShownChanged);
  chooser.requestOpen();
  ASSERT_TRUE(QTest::qWaitFor([&] { return chooser.fallbackShown(); }));
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fallback.size(), 1);
  chooser.fallbackRejected();
  EXPECT_FALSE(chooser.fallbackShown());
}

TEST(PortalFileChooser, DestructionClosesOutstandingRequest) {
  MockPortal portal;
  ASSERT_TRUE(portal.start());
  auto chooser = std::make_unique<PortalFileChooser>();
  QSignalSpy finished(chooser.get(), &PortalFileChooser::finished);
  chooser->requestOpen();
  ASSERT_TRUE(QTest::qWaitFor([&] { return chooser->awaitingResponseForTesting(); }));
  ASSERT_EQ(portal.calls().size(), 1);
  const auto handle = portal.calls().front().handle;
  chooser.reset();
  ASSERT_TRUE(QTest::qWaitFor([&] { return portal.closedHandles().size() == 1; }));
  EXPECT_EQ(portal.closedHandles().front(), handle);
  portal.emitResponse(handle, 0, {{"uris", QStringList{"file:///tmp/late.jpg"}}});
  QTest::qWait(requestSettleMs);
  EXPECT_TRUE(finished.isEmpty());
}

TEST(PortalFileChooser, WindowDestructionClosesRequestWhileChooserSurvives) {
  for (const bool awaitReply : {false, true}) {
    PortalChooserFixture fixture;
    ASSERT_TRUE(fixture.start());
    auto window = std::make_unique<QQuickWindow>();
    fixture.chooser.setWindow(window.get());
    if (awaitReply) {
      ASSERT_TRUE(fixture.requestAndAwait());
    } else {
      fixture.chooser.requestOpen();
    }
    window.reset();
    EXPECT_EQ(fixture.chooser.window(), nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.portal.closedHandles().size() == 1; }));
    ASSERT_EQ(fixture.portal.calls().size(), 1);
    const auto handle = fixture.portal.calls().back().handle;
    EXPECT_EQ(fixture.portal.closedHandles().front(), handle);
    fixture.portal.emitResponse(handle, 0, {{"uris", QStringList{"file:///tmp/late.jpg"}}});
    QTest::qWait(requestSettleMs);
    EXPECT_EQ(fixture.outcomes(), 0);
    EXPECT_EQ(fixture.portal.closedHandles().size(), 1);
  }
}

TEST(PortalFileChooser, ReplacedWindowDestructionDoesNotCancelRequest) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  auto oldWindow = std::make_unique<QQuickWindow>();
  QQuickWindow window;
  fixture.chooser.setWindow(oldWindow.get());
  fixture.chooser.setWindow(&window);
  ASSERT_TRUE(fixture.requestAndAwait());
  oldWindow.reset();
  QTest::qWait(requestSettleMs);
  EXPECT_TRUE(fixture.chooser.awaitingResponseForTesting());
  EXPECT_TRUE(fixture.portal.closedHandles().isEmpty());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 1);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.cancelled.size() == 1; }));
}

TEST(PortalFileChooser, CancelBeforeReplyClosesReturnedHandle) {
  PortalChooserFixture fixture;
  ASSERT_TRUE(fixture.start());
  fixture.chooser.requestOpen();
  fixture.chooser.cancel();
  EXPECT_FALSE(fixture.chooser.awaitingResponseForTesting());
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.portal.closedHandles().size() == 1; }));
  ASSERT_EQ(fixture.portal.calls().size(), 1);
  const auto handle = fixture.portal.calls().front().handle;
  EXPECT_EQ(fixture.portal.closedHandles().front(), handle);
  fixture.portal.emitResponse(handle, 0, {{"uris", QStringList{"file:///tmp/late.jpg"}}});
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.outcomes(), 0);
  // A cancelled request leaves the chooser ready for the next Open.
  ASSERT_TRUE(fixture.requestAndAwait());
  fixture.chooser.cancel();
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.portal.closedHandles().size() == 2; }));
  EXPECT_EQ(fixture.portal.closedHandles().back(), fixture.portal.calls().back().handle);
}

namespace {
// Captures warnings so export fallbacks can be asserted silent.
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
}  // namespace

namespace {
struct PortalViewerFixture {
  MockPortal portal;
  ImageDocument document;
  QQmlApplicationEngine engine;
  QQuickWindow* window = nullptr;
  PortalFileChooser* chooser = nullptr;
  ImageCanvas* canvas = nullptr;

  [[nodiscard]] ::testing::AssertionResult load() {
    if (auto started = portal.start(); !started) {
      return started;
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
    chooser = window->findChild<PortalFileChooser*>(QStringLiteral("portalFileChooser"));
    canvas = window->findChild<ImageCanvas*>(QStringLiteral("imageCanvas"));
    if (chooser == nullptr || canvas == nullptr) {
      return ::testing::AssertionFailure() << "portal chooser or canvas not found";
    }
    window->requestActivate();
    if (!QTest::qWaitForWindowActive(window)) {
      return ::testing::AssertionFailure() << "window not active";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] ::testing::AssertionResult pressOpen() const {
    const auto before = portal.calls().size();
    QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier);
    if (!QTest::qWaitFor([&] { return chooser->awaitingResponseForTesting(); })) {
      return ::testing::AssertionFailure() << "Ctrl+O did not reach the portal";
    }
    if (portal.calls().size() != before + 1) {
      return ::testing::AssertionFailure() << "expected one OpenFile call per Ctrl+O";
    }
    return ::testing::AssertionSuccess();
  }

  [[nodiscard]] bool modalActive() const { return window->property("modalActive").toBool(); }
  [[nodiscard]] bool dialogExists() const {
    return window->findChild<QObject*>(QStringLiteral("openDialog")) != nullptr;
  }
};
}  // namespace

TEST(PortalViewer, CtrlOUsesPortalForSelectionCancelAndErrors) {
  PortalViewerFixture fixture;
  ASSERT_TRUE(fixture.load());
  const auto dir = QStringLiteral(VIEWER_FIXTURE_DIR) + "/portal";
  ASSERT_TRUE(QDir().mkpath(dir));
  const auto path = dir + QString::fromUtf8("/café 100%.png");
  QImage image(64, 32, QImage::Format_ARGB32);
  image.fill(Qt::darkCyan);
  ASSERT_TRUE(image.save(path));

  ASSERT_TRUE(fixture.pressOpen());
  EXPECT_FALSE(fixture.dialogExists());
  EXPECT_TRUE(fixture.modalActive());
  EXPECT_FALSE(fixture.portal.calls().back().options.contains("current_folder"));
  const auto uri = QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
  ASSERT_TRUE(uri.contains("caf%C3%A9%20100%25.png")) << uri.toStdString();
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 0, {{"uris", QStringList{uri}}});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Ready; }))
      << fixture.document.error().toStdString();
  EXPECT_EQ(fixture.document.localPath(), path);
  EXPECT_EQ(fixture.canvas->image().size(), image.size());
  EXPECT_FALSE(fixture.modalActive());

  QTest::qWait(requestSettleMs);
  const auto magnification = fixture.canvas->magnification();
  const auto imageRect = fixture.canvas->imageRect();
  const auto orientation = fixture.document.orientation();
  const auto* focus = fixture.window->activeFocusItem();
  ASSERT_TRUE(fixture.pressOpen());
  const auto folder = fixture.portal.calls().back().options.value("current_folder").toByteArray();
  EXPECT_EQ(folder, QFile::encodeName(dir) + '\0');
  EXPECT_TRUE(fixture.modalActive());
  QTest::keyClick(fixture.window, Qt::Key_O, Qt::ControlModifier);
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.portal.calls().size(), 2);
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 1);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.modalActive(); }));
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.document.state(), ImageDocument::Ready);
  EXPECT_EQ(fixture.document.localPath(), path);
  EXPECT_DOUBLE_EQ(fixture.canvas->magnification(), magnification);
  EXPECT_EQ(fixture.canvas->imageRect(), imageRect);
  EXPECT_EQ(fixture.document.orientation(), orientation);
  EXPECT_EQ(fixture.window->activeFocusItem(), focus);
  EXPECT_FALSE(fixture.dialogExists());

  ASSERT_TRUE(fixture.pressOpen());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 0, {{"uris", QStringList{"http://example.com"}}});
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Error; }));
  EXPECT_FALSE(fixture.document.error().isEmpty());
  EXPECT_FALSE(fixture.modalActive());
  QTest::qWait(requestSettleMs);
  EXPECT_FALSE(fixture.dialogExists());
  EXPECT_TRUE(fixture.portal.unexpectedMessages().isEmpty());
}

TEST(PortalViewer, CancellationRestoresFocusedControl) {
  PortalViewerFixture fixture;
  ASSERT_TRUE(fixture.load());
  auto* control = fixture.window->findChild<QQuickItem*>(QStringLiteral("actionsButton"));
  ASSERT_NE(control, nullptr);
  for (const uint code : {1U, 2U}) {
    control->forceActiveFocus(Qt::TabFocusReason);
    ASSERT_EQ(fixture.window->activeFocusItem(), control);
    ASSERT_TRUE(fixture.pressOpen());
    fixture.portal.emitResponse(fixture.portal.calls().back().handle, code);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.modalActive(); }));
    EXPECT_EQ(fixture.window->activeFocusItem(), control);
    EXPECT_FALSE(fixture.dialogExists());
  }
}

TEST(PortalViewer, ExternalDialogReleaseResetsFallback) {
  PortalViewerFixture fixture;
  ASSERT_TRUE(fixture.load());
  fixture.portal.stop();
  fixture.window->setProperty("dialogRequested", true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.dialogExists(); }));
  fixture.window->setProperty("dialogRequested", false);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.dialogExists(); }));
  EXPECT_FALSE(fixture.chooser->fallbackShown());
  fixture.window->setProperty("dialogRequested", true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.dialogExists(); }));
  auto* dialog = fixture.window->findChild<QObject*>(QStringLiteral("openDialog"));
  ASSERT_NE(dialog, nullptr);
  ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "reject"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.modalActive(); }));
  EXPECT_FALSE(fixture.chooser->fallbackShown());
}

TEST(PortalViewer, WindowCloseClosesRequestAndIgnoresLateResponse) {
  PortalViewerFixture fixture;
  ASSERT_TRUE(fixture.load());
  const auto dir = QStringLiteral(VIEWER_FIXTURE_DIR) + "/portal";
  ASSERT_TRUE(QDir().mkpath(dir));
  const auto path = dir + "/late.png";
  QImage image(16, 16, QImage::Format_ARGB32);
  image.fill(Qt::red);
  ASSERT_TRUE(image.save(path));
  const QVariantMap selection{{"uris", QStringList{QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded)}}};

  ASSERT_TRUE(fixture.pressOpen());
  const auto handle = fixture.portal.calls().back().handle;
  fixture.window->close();
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.portal.closedHandles().size() == 1; }));
  EXPECT_EQ(fixture.portal.closedHandles().front(), handle);
  fixture.portal.emitResponse(handle, 0, selection);
  QTest::qWait(requestSettleMs);
  EXPECT_NE(fixture.document.state(), ImageDocument::Ready);
  EXPECT_TRUE(fixture.document.localPath().isEmpty());

  delete fixture.window;
  fixture.portal.emitResponse(handle, 0, selection);
  QTest::qWait(requestSettleMs);
  EXPECT_TRUE(fixture.document.localPath().isEmpty());
  EXPECT_EQ(fixture.portal.closedHandles().size(), 1);
}

TEST(PortalViewer, OffscreenSendsEmptyParentWindowWithoutWarnings) {
  PortalViewerFixture fixture;
  ASSERT_TRUE(fixture.load());
  ASSERT_EQ(QGuiApplication::platformName(), "offscreen");
  const WarningCollector collector;
  {
    WaylandForeignExporter exporter;
    EXPECT_FALSE(exporter.exportWindow(nullptr).isValid());
    const auto exported = exporter.exportWindow(fixture.window);
    EXPECT_FALSE(exported.isValid());
    EXPECT_TRUE(exported.handle().isEmpty());
  }
  ASSERT_TRUE(fixture.pressOpen());
  EXPECT_EQ(fixture.portal.calls().back().parentWindow, "");
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 1);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.modalActive(); }));
  EXPECT_TRUE(collector.warnings.isEmpty()) << collector.warnings.join('\n').toStdString();
  EXPECT_FALSE(fixture.dialogExists());
}

TEST(PortalViewer, FallbackOnlyWhenPortalCannotServe) {
  PortalViewerFixture fixture;
  ASSERT_TRUE(fixture.load());
  QSignalSpy fallback(fixture.chooser, &PortalFileChooser::fallbackShownChanged);

  ASSERT_TRUE(fixture.pressOpen());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 0);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.document.state() == ImageDocument::Error; }));
  EXPECT_FALSE(fixture.modalActive());
  EXPECT_FALSE(fixture.dialogExists());
  EXPECT_TRUE(fallback.isEmpty());

  // Escape in the GTK picker (code 2) is a cancellation, not a reason for the FileDialog.
  ASSERT_TRUE(fixture.pressOpen());
  fixture.portal.emitResponse(fixture.portal.calls().back().handle, 2, {{"uris", QStringList{}}});
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.modalActive(); }));
  QTest::qWait(requestSettleMs);
  EXPECT_FALSE(fixture.dialogExists());
  EXPECT_TRUE(fallback.isEmpty());
  EXPECT_EQ(fixture.document.state(), ImageDocument::Error);

  fixture.portal.setReply(MockPortal::Reply::Error);
  QTest::keyClick(fixture.window, Qt::Key_O, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.dialogExists(); }));
  EXPECT_TRUE(fixture.modalActive());
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fallback.size(), 1);
  EXPECT_EQ(fixture.portal.calls().size(), 3);
  auto* dialog = fixture.window->findChild<QObject*>(QStringLiteral("openDialog"));
  ASSERT_NE(dialog, nullptr);
  ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "reject"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.modalActive(); }));
  QTest::qWait(requestSettleMs);
  EXPECT_FALSE(fixture.dialogExists());
  EXPECT_EQ(fallback.size(), 2);
  EXPECT_EQ(fixture.portal.calls().size(), 3);

  // Availability is not cached: the next Open notices the portal is gone.
  fixture.portal.stop();
  QTest::keyClick(fixture.window, Qt::Key_O, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return fixture.dialogExists(); }));
  dialog = fixture.window->findChild<QObject*>(QStringLiteral("openDialog"));
  ASSERT_NE(dialog, nullptr);
  ASSERT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }));
  // Accepting through the dialog's file list is covered by Viewer.OpeningCanvasAndAdapters.
  ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "reject"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !fixture.modalActive(); }));
  QTest::qWait(requestSettleMs);
  EXPECT_EQ(fixture.document.state(), ImageDocument::Error);
  EXPECT_FALSE(fixture.dialogExists());
  EXPECT_EQ(fallback.size(), 4);
  EXPECT_EQ(fixture.portal.calls().size(), 3);
  EXPECT_TRUE(fixture.portal.unexpectedMessages().isEmpty());
}

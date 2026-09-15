#include "mock_portal.h"

#include "portal_request_builder.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>

namespace {
constexpr auto fileChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr auto requestInterface = "org.freedesktop.portal.Request";
}  // namespace

MockPortal::MockPortal() : connection_(QString()) { PortalRequest::registerMetaTypes(); }

MockPortal::~MockPortal() { stop(); }

::testing::AssertionResult MockPortal::start() {
  static int instance = 0;
  const auto name = QStringLiteral("mockPortalConnection%1").arg(++instance);
  connection_ = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
  if (!connection_.isConnected()) {
    return ::testing::AssertionFailure() << "mock portal cannot reach the session bus: "
                                         << connection_.lastError().message().toStdString();
  }
  started_ = true;
  if (auto guard = sessionBusIsPrivate(connection_); !guard) {
    return guard;
  }
  if (!connection_.registerVirtualObject(path, this, QDBusConnection::SubPath)) {
    return ::testing::AssertionFailure() << "mock portal cannot register " << path;
  }
  const auto reply = connection_.interface()->registerService(service, QDBusConnectionInterface::DontQueueService,
                                                              QDBusConnectionInterface::DontAllowReplacement);
  if (!reply.isValid() || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
    return ::testing::AssertionFailure() << "mock portal cannot own " << service;
  }
  return ::testing::AssertionSuccess();
}

void MockPortal::stop() {
  if (!started_) {
    return;
  }
  started_ = false;
  connection_.unregisterObject(path, QDBusConnection::UnregisterTree);
  connection_.interface()->unregisterService(service);
  const auto name = connection_.name();
  connection_ = QDBusConnection(QString());
  QDBusConnection::disconnectFromBus(name);
}

void MockPortal::emitResponse(const QString& handle, uint code, const QVariantMap& results) const {
  auto signal = QDBusMessage::createTargetedSignal(last_sender_, handle, requestInterface, "Response");
  signal << code << results;
  connection_.send(signal);
}

bool MockPortal::handleMessage(const QDBusMessage& message, const QDBusConnection& connection) {
  if (message.type() != QDBusMessage::MethodCallMessage) {
    return false;
  }
  if (message.path() == path && message.interface() == fileChooserInterface && message.member() == "OpenFile" &&
      message.signature() == "ssa{sv}") {
    const auto arguments = message.arguments();
    const auto options = qdbus_cast<QVariantMap>(arguments.at(2));
    const auto predicted = PortalRequest::requestPath(message.service(), options.value("handle_token").toString());
    last_sender_ = message.service();
    const auto handle = returned_handle_.isEmpty() ? predicted : returned_handle_;
    calls_.append({.sender = message.service(),
                   .parentWindow = arguments.at(0).toString(),
                   .title = arguments.at(1).toString(),
                   .options = options,
                   .handle = handle});
    if (response_before_reply_) {
      emitResponse(predicted, response_before_reply_->code, response_before_reply_->results);
    }
    if (reply_ == Reply::Error) {
      connection.send(message.createErrorReply(QDBusError::AccessDenied, "mock portal rejected OpenFile"));
    } else if (reply_ == Reply::Handle) {
      connection.send(message.createReply(QVariant::fromValue(QDBusObjectPath(handle))));
    }
    return true;
  }
  if (message.interface() == requestInterface && message.member() == "Close") {
    closed_handles_.append(message.path());
    connection.send(message.createReply());
    return true;
  }
  unexpected_messages_.append(message.interface() + '.' + message.member());
  connection.send(message.createErrorReply(QDBusError::UnknownMethod, "mock portal does not implement this"));
  return true;
}

QString MockPortal::introspect(const QString& /*path*/) const { return {}; }

::testing::AssertionResult MockPortal::sessionBusIsPrivate(const QDBusConnection& bus) {
  const auto* interface = bus.interface();
  if (interface == nullptr) {
    return ::testing::AssertionSuccess();
  }
  const auto activatable = interface->activatableServiceNames();
  if (interface->isServiceRegistered(service).value() ||
      (activatable.isValid() && activatable.value().contains(service))) {
    return ::testing::AssertionFailure()
           << service << " is already owned or activatable on this session bus; run viewer-smoke through "
           << "ctest, which starts it under dbus-run-session with tests/fixtures/dbus-session.conf";
  }
  return ::testing::AssertionSuccess();
}

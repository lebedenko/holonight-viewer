#include "portal_file_chooser.h"

#include "portal_request_builder.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>

namespace {
Q_LOGGING_CATEGORY(portalLog, "holonight.viewer.portal")

constexpr auto portalService = "org.freedesktop.portal.Desktop";
constexpr auto portalPath = "/org/freedesktop/portal/desktop";
constexpr auto fileChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr auto requestInterface = "org.freedesktop.portal.Request";

constexpr uint responseSelected = 0;
constexpr int shutdownCloseTimeoutMs = 500;

void closeRequest(const QString& handle, QDBus::CallMode mode) {
  const auto close = QDBusMessage::createMethodCall(portalService, handle, requestInterface, "Close");
  QDBusConnection::sessionBus().call(close, mode, shutdownCloseTimeoutMs);
}
}  // namespace

PortalFileChooser::PortalFileChooser(QObject* parent) : QObject(parent) { PortalRequest::registerMetaTypes(); }

// Destruction may be the last chance before process exit, so Close is written synchronously.
PortalFileChooser::~PortalFileChooser() { abandonRequest(QDBus::Block); }

void PortalFileChooser::setWindow(QQuickWindow* window) {
  if (window_ == window) {
    return;
  }
  disconnect(window_closing_);
  disconnect(window_destroyed_);
  window_ = window;
  if (window_ != nullptr) {
    // Closing still has a live event loop and bus, unlike engine teardown at exit.
    window_closing_ = connect(window_, &QQuickWindow::closing, this, &PortalFileChooser::cancel);
    window_destroyed_ = connect(window_, &QObject::destroyed, this, [this] {
      cancel();
      emit windowChanged();
    });
  }
  emit windowChanged();
}

void PortalFileChooser::setNameFilters(const QStringList& nameFilters) {
  if (name_filters_ == nameFilters) {
    return;
  }
  name_filters_ = nameFilters;
  emit nameFiltersChanged();
}

void PortalFileChooser::setCurrentLocalPath(const QString& path) {
  if (current_local_path_ == path) {
    return;
  }
  current_local_path_ = path;
  emit currentLocalPathChanged();
}

void PortalFileChooser::requestOpen() {
  if (state_ != State::Idle) {
    return;
  }
  // Nothing about portal availability is remembered: every Open issues its own call.
  auto bus = QDBusConnection::sessionBus();
  const auto token = PortalRequest::newHandleToken();

  QVariantMap options{
      {"handle_token", token},
      {"modal", true},
      {"multiple", false},
  };
  const auto filters = PortalRequest::filters(name_filters_);
  if (!filters.isEmpty()) {
    options.insert("filters", QVariant::fromValue(filters));
    options.insert("current_filter", QVariant::fromValue(filters.first()));
  }
  if (const auto folder = PortalRequest::currentFolder(current_local_path_); !folder.isEmpty()) {
    options.insert("current_folder", folder);
  }

  // Subscribe on the predicted request path before calling: the portal may emit Response
  // before its OpenFile reply reaches us, and waiting for the returned handle would miss it.
  subscribeResponse(PortalRequest::requestPath(bus.baseService(), token));

  // A fresh export per request parents the picker to this window; it lives until the request
  // ends. Without one (not Wayland, no xdg-foreign) the portal still works, just unparented.
  active_export_ = exporter_.exportWindow(window_);
  auto call = QDBusMessage::createMethodCall(portalService, portalPath, fileChooserInterface, "OpenFile");
  call << PortalRequest::parentWindow(active_export_.handle()) << tr("Open image") << options;
  state_ = State::Calling;
  watcher_ = new QDBusPendingCallWatcher(bus.asyncCall(call, call_timeout_ms_), this);
  connect(watcher_, &QDBusPendingCallWatcher::finished, this, &PortalFileChooser::onOpenFileFinished);
}

void PortalFileChooser::fallbackAccepted(const QUrl& selectedFile) {
  if (state_ != State::FallbackShown) {
    return;
  }
  state_ = State::Idle;
  emit finished({selectedFile});
  emit fallbackShownChanged();
}

void PortalFileChooser::fallbackRejected() {
  if (state_ != State::FallbackShown) {
    return;
  }
  state_ = State::Idle;
  emit cancelled();
  emit fallbackShownChanged();
}

void PortalFileChooser::cancel() {
  const auto wasFallback = state_ == State::FallbackShown;
  abandonRequest(QDBus::NoBlock);
  if (wasFallback) {
    emit fallbackShownChanged();
  }
}

void PortalFileChooser::onOpenFileFinished(QDBusPendingCallWatcher* watcher) {
  if (watcher != watcher_ || state_ != State::Calling) {
    return;
  }
  const QDBusPendingReply<QDBusObjectPath> reply(*watcher);
  watcher_ = nullptr;
  watcher->deleteLater();
  // Any call failure (no portal on the bus, missing interface, error reply or timeout) means
  // the portal cannot serve this Open, so the FileDialog takes over exactly once.
  if (reply.isError()) {
    qCWarning(portalLog) << "FileChooser.OpenFile failed, using the fallback dialog:" << reply.error().name()
                         << reply.error().message();
    finishRequest();
    showFallback();
    return;
  }
  // Older portals return a handle that differs from the predicted path.
  if (const auto handle = reply.value().path(); handle != subscribed_path_) {
    unsubscribeResponse();
    subscribeResponse(handle);
  }
  // No timeout from here on: a picker may legitimately stay open indefinitely.
  state_ = State::AwaitingResponse;
}

void PortalFileChooser::abandonRequest(QDBus::CallMode closeMode) {
  if (state_ == State::AwaitingResponse) {
    closeRequest(subscribed_path_, closeMode);
  } else if (state_ == State::Calling && watcher_ != nullptr) {
    // The handle is not known yet; close it when the reply arrives if the process still runs.
    // Unsubscribing below means the abandoned request can never open a file.
    disconnect(watcher_, nullptr, this, nullptr);
    watcher_->setParent(QCoreApplication::instance());
    connect(watcher_, &QDBusPendingCallWatcher::finished, watcher_, [](QDBusPendingCallWatcher* watcher) {
      const QDBusPendingReply<QDBusObjectPath> reply(*watcher);
      if (!reply.isError()) {
        closeRequest(reply.value().path(), QDBus::NoBlock);
      }
      watcher->deleteLater();
    });
    watcher_ = nullptr;
  }
  finishRequest();
}

void PortalFileChooser::onResponse(uint code, const QVariantMap& results, const QDBusMessage& message) {
  if ((state_ != State::Calling && state_ != State::AwaitingResponse) || message.path() != subscribed_path_) {
    return;
  }
  finishRequest();
  if (code == responseSelected) {
    QList<QUrl> urls;
    for (const auto& uri : results.value("uris").toStringList()) {
      urls.append(QUrl(uri, QUrl::StrictMode));
    }
    emit finished(urls);
  } else {
    // Code 1 is the Cancel button; GTK reports Escape and closing the picker as code 2.
    // Both are a user decision, so the FileDialog is reserved for OpenFile call failures.
    emit cancelled();
  }
}

void PortalFileChooser::subscribeResponse(const QString& path) {
  subscribed_path_ = path;
  QDBusConnection::sessionBus().connect(portalService, path, requestInterface, "Response", this,
                                        SLOT(onResponse(uint, QVariantMap, QDBusMessage)));
}

void PortalFileChooser::unsubscribeResponse() {
  if (subscribed_path_.isEmpty()) {
    return;
  }
  QDBusConnection::sessionBus().disconnect(portalService, subscribed_path_, requestInterface, "Response", this,
                                           SLOT(onResponse(uint, QVariantMap, QDBusMessage)));
  subscribed_path_.clear();
}

void PortalFileChooser::finishRequest() {
  unsubscribeResponse();
  delete watcher_;
  watcher_ = nullptr;
  active_export_ = {};
  state_ = State::Idle;
}

void PortalFileChooser::showFallback() {
  state_ = State::FallbackShown;
  emit fallbackShownChanged();
}

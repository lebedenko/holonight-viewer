#pragma once

#include "wayland_foreign_export.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <cstdint>

class QDBusPendingCallWatcher;

// Runs one Open flow through the xdg-desktop-portal FileChooser, falling back to the
// in-window FileDialog (shown by QML while fallbackShown is true) only when the portal fails.
// QObject owns identity and disables copying/moving.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class PortalFileChooser : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
  Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters NOTIFY nameFiltersChanged)
  Q_PROPERTY(QString currentLocalPath READ currentLocalPath WRITE setCurrentLocalPath NOTIFY currentLocalPathChanged)
  Q_PROPERTY(bool fallbackShown READ fallbackShown NOTIFY fallbackShownChanged)
 public:
  static constexpr int defaultCallTimeoutMs = 25000;

  explicit PortalFileChooser(QObject* parent = nullptr);
  ~PortalFileChooser() override;

  [[nodiscard]] QQuickWindow* window() const { return window_; }
  void setWindow(QQuickWindow* window);
  [[nodiscard]] QStringList nameFilters() const { return name_filters_; }
  void setNameFilters(const QStringList& nameFilters);
  [[nodiscard]] QString currentLocalPath() const { return current_local_path_; }
  void setCurrentLocalPath(const QString& path);
  [[nodiscard]] bool fallbackShown() const { return state_ == State::FallbackShown; }

  // Starts a request unless one (portal or fallback) is already outstanding.
  Q_INVOKABLE void requestOpen();
  Q_INVOKABLE void fallbackAccepted(const QUrl& selectedFile);
  Q_INVOKABLE void fallbackRejected();
  // Abandons an outstanding request without emitting finished() or cancelled().
  Q_INVOKABLE void cancel();

  void setCallTimeoutMsForTesting(int milliseconds) { call_timeout_ms_ = milliseconds; }
  [[nodiscard]] bool awaitingResponseForTesting() const { return state_ == State::AwaitingResponse; }

 signals:
  void windowChanged();
  void nameFiltersChanged();
  void currentLocalPathChanged();
  void fallbackShownChanged();
  // Code 0: the portal's uris as given; ImageDocument::open() rejects anything but one local file.
  void finished(const QList<QUrl>& urls);
  void cancelled();

 private:
  enum class State : std::uint8_t { Idle, Calling, AwaitingResponse, FallbackShown };

  void onOpenFileFinished(QDBusPendingCallWatcher* watcher);
  void abandonRequest(QDBus::CallMode closeMode);
  Q_SLOT void onResponse(uint code, const QVariantMap& results, const QDBusMessage& message);
  void subscribeResponse(const QString& path);
  void unsubscribeResponse();
  void finishRequest();
  void showFallback();

  QPointer<QQuickWindow> window_;
  QMetaObject::Connection window_closing_;
  QMetaObject::Connection window_destroyed_;
  QStringList name_filters_;
  QString current_local_path_;
  State state_ = State::Idle;
  QString subscribed_path_;
  QDBusPendingCallWatcher* watcher_ = nullptr;
  // Declared after the exporter so an outstanding export is destroyed first.
  WaylandForeignExporter exporter_;
  WaylandForeignExport active_export_;
  int call_timeout_ms_ = defaultCallTimeoutMs;
};

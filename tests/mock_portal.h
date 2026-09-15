#pragma once

#include <QDBusConnection>
#include <QDBusVirtualObject>
#include <QStringList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <optional>

// Stand-in for org.freedesktop.portal.Desktop on the private ctest bus. It uses its own
// connection so Response routing and request-path prediction cross a real bus hop.
class MockPortal : public QDBusVirtualObject {
 public:
  enum class Reply { Handle, Error, Never };
  struct Response {
    uint code = 0;
    QVariantMap results;
  };
  struct OpenFileCall {
    QString sender;
    QString parentWindow;
    QString title;
    QVariantMap options;
    QString handle;
  };

  MockPortal();
  ~MockPortal() override;
  MockPortal(const MockPortal&) = delete;
  MockPortal& operator=(const MockPortal&) = delete;
  MockPortal(MockPortal&&) = delete;
  MockPortal& operator=(MockPortal&&) = delete;

  [[nodiscard]] ::testing::AssertionResult start();
  void stop();

  void setReply(Reply reply) { reply_ = reply; }
  // Replies with this handle instead of the one the caller can predict.
  void setReturnedHandle(const QString& handle) { returned_handle_ = handle; }
  // Emitted on the predicted request path before the OpenFile reply is sent.
  void setResponseBeforeReply(std::optional<Response> response) { response_before_reply_ = std::move(response); }
  void emitResponse(const QString& handle, uint code, const QVariantMap& results = {}) const;

  [[nodiscard]] const QList<OpenFileCall>& calls() const { return calls_; }
  [[nodiscard]] const QStringList& closedHandles() const { return closed_handles_; }
  [[nodiscard]] const QStringList& unexpectedMessages() const { return unexpected_messages_; }

  bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override;
  [[nodiscard]] QString introspect(const QString& path) const override;

  // The smoke binary must never reach a real portal: fail when the name is owned or activatable.
  [[nodiscard]] static ::testing::AssertionResult sessionBusIsPrivate(const QDBusConnection& bus);

  static constexpr auto service = "org.freedesktop.portal.Desktop";
  static constexpr auto path = "/org/freedesktop/portal/desktop";

 private:
  QDBusConnection connection_;
  bool started_ = false;
  Reply reply_ = Reply::Handle;
  QString returned_handle_;
  std::optional<Response> response_before_reply_;
  QString last_sender_;
  QList<OpenFileCall> calls_;
  QStringList closed_handles_;
  QStringList unexpected_messages_;
};

#pragma once

#include "decoded_image_cache.h"
#include "image_decoder.h"

#include <QObject>
#include <QThread>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

// Application-private GUI-thread control surface; decode and cache storage belong to the worker.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class ImageLoader : public QObject {
  Q_OBJECT
 public:
  using Decoder = std::function<DecodeResult(const QUrl&, const std::atomic_bool&)>;
  enum class Kind : std::uint8_t { Foreground, Tentative, Prefetch };
  struct Request {
    quint64 request_id;
    QUrl url;
    quint64 cache_epoch;
    Kind kind = Kind::Foreground;
  };
  explicit ImageLoader(Decoder decoder, qint64 retainedBytes, QObject* parent = nullptr);
  ~ImageLoader() override;
  void submit(Request request, bool cancelActive = true);
  void cancel();
  bool cancelTentative();
  void retainTentative(const Request& request, const DecodeResult& result, quint64 epoch);
  void shutdown();
  [[nodiscard]] bool busy() const { return busy_; }
  [[nodiscard]] bool hasPending() const { return pending_.has_value(); }
  [[nodiscard]] bool hasPendingForeground() const { return pending_ && pending_->kind == Kind::Foreground; }
 signals:
  void completed(ImageLoader::Request request, DecodeResult result);
  void shutdownFinished();

 private:
  friend struct ImageLoaderTestAccess;
  void startPending();
  void decodeRequest(const Request& request, const std::shared_ptr<std::atomic_bool>& cancel);
  void complete(const Request& request, DecodeResult result, bool cancelled);
  QThread thread_;
  QObject* worker_;
  Decoder decoder_;
  std::shared_ptr<std::atomic_bool> cancellation_;
  std::optional<Request> pending_;
  bool busy_ = false;
  bool tentative_active_ = false;
  bool delivering_ = false;
  bool stopping_ = false;
  // Accessed exclusively on the worker, including tentative adoption.
  quint64 worker_cache_epoch_ = 0;
  DecodedImageCache cache_;
  qint64 retained_budget_;
  std::optional<CachedImage> displayed_;
};

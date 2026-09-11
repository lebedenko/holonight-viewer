#pragma once

#include <QDateTime>
#include <QImage>
#include <QUrl>

#include <list>
#include <optional>

struct ImageInformation {
  QString format;
  qint64 encodedSize = -1;
  QDateTime modified;
  QSize decodedSize;
};

struct CachedImage {
  QUrl url;
  qint64 size = -1;
  QDateTime modified;
  QImage image;
  ImageInformation information;
};

// Used only on the decode worker. The front is the most recently used entry.
class DecodedImageCache {
 public:
  static constexpr qint64 byteLimit = 128 * 1024 * 1024;
  static CachedImage metadata(const QUrl& url);
  static bool valid(const CachedImage& entry);
  std::optional<CachedImage> take(const QUrl& url);
  void put(CachedImage entry);
  void clear();
  [[nodiscard]] qsizetype count() const { return static_cast<qsizetype>(entries_.size()); }
  [[nodiscard]] qint64 bytes() const { return bytes_; }

 private:
  std::list<CachedImage> entries_;
  qint64 bytes_ = 0;
};

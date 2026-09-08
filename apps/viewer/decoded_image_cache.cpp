#include "decoded_image_cache.h"

#include "directory_model.h"

#include <QFileInfo>

CachedImage DecodedImageCache::metadata(const QUrl& url) {
  const auto normalized = normalizedLocalUrl(url);
  const QFileInfo info(normalized.toLocalFile());
  return {.url = normalized, .size = info.isFile() ? info.size() : -1, .modified = info.lastModified(), .image = {}};
}
bool DecodedImageCache::valid(const CachedImage& entry) {
  const auto current = metadata(entry.url);
  return current.size >= 0 && current.size == entry.size && current.modified == entry.modified;
}
std::optional<CachedImage> DecodedImageCache::take(const QUrl& url) {
  const auto normalized = normalizedLocalUrl(url);
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->url == normalized) {
      bytes_ -= it->image.sizeInBytes();
      auto entry = std::move(*it);
      entries_.erase(it);
      if (valid(entry)) {
        return entry;
      }
      return {};
    }
  }
  return {};
}
void DecodedImageCache::put(CachedImage entry) {
  const auto length = entry.image.sizeInBytes();
  if (entry.image.isNull() || length > byteLimit || !valid(entry)) {
    return;
  }
  // Remove a duplicate without retaining a second copy of its storage.
  take(entry.url);
  while (!entries_.empty() && (entries_.size() >= 2 || bytes_ + length > byteLimit)) {
    bytes_ -= entries_.back().image.sizeInBytes();
    entries_.pop_back();
  }
  bytes_ += length;
  entries_.push_front(std::move(entry));
}
void DecodedImageCache::clear() {
  entries_.clear();
  bytes_ = 0;
}

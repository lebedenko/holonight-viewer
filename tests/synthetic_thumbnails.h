#pragma once

#include "image_document.h"
#include "thumbnail_decoder.h"

#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

// Decodes to a synthetic solid image; can be held closed or made to fail.
struct SyntheticDecoder {
  QSize decodedSize{100, 50};
  QSize sourceSize{100, 50};
  std::atomic_bool failing{false};
  QMutex mutex;
  QWaitCondition gate;
  bool blocked = false;
  QStringList paths;
  QList<int> boxes;
  // Every decode on the thread that built the decoder, which is the GUI thread, counts here.
  QThread* guiThread = QThread::currentThread();
  std::atomic_int guiThreadCalls{0};
  std::atomic_int calls{0};

  ThumbnailResult operator()(const ThumbnailRequest& request, const std::atomic_bool& /*cancelled*/) {
    ++calls;
    {
      QMutexLocker lock(&mutex);
      paths.append(request.path);
      boxes.append(request.boxPixels);
    }
    if (QThread::currentThread() == guiThread) {
      ++guiThreadCalls;
    }
    {
      QMutexLocker lock(&mutex);
      while (blocked) {
        gate.wait(&mutex);
      }
    }
    if (failing) {
      return {.image = {}, .sourceSize = {}, .error = QStringLiteral("boom")};
    }
    QImage image(decodedSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    return {.image = image, .sourceSize = sourceSize, .error = {}};
  }
  QStringList decodedPaths() {
    QMutexLocker lock(&mutex);
    return paths;
  }
  QList<int> decodedBoxes() {
    QMutexLocker lock(&mutex);
    return boxes;
  }
  void block(bool value) {
    QMutexLocker lock(&mutex);
    blocked = value;
    if (!value) {
      gate.wakeAll();
    }
  }
};

inline QUrl photo(int number) {
  return QUrl::fromLocalFile(QStringLiteral("/photos/photo_%1.png").arg(number, 4, 10, QChar('0')));
}

inline QList<QUrl> photos(int count) {
  QList<QUrl> urls;
  for (int i = 1; i <= count; ++i) {
    urls.append(photo(i));
  }
  return urls;
}

inline DecodeResult solidDecodeSized(QSize size) {
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::blue);
  return {.image = image, .error = {}, .information = {}, .svgData = {}};
}

inline DecodeResult solidDecode(const QUrl&, const std::atomic_bool&) { return solidDecodeSized({20, 10}); }

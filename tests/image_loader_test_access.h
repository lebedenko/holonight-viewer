#pragma once

#include "image_loader.h"

#include <QMetaObject>

#include <utility>

// Test-only observations are marshalled to the worker; no cache internals are public API.
struct ImageLoaderTestAccess {
  static std::pair<qint64, qint64> cacheBudget(ImageLoader& loader) {
    std::pair<qint64, qint64> result;
    QMetaObject::invokeMethod(
        loader.worker_, [&] { result = {loader.cache_.bytes(), loader.cache_.limit()}; }, Qt::BlockingQueuedConnection);
    return result;
  }
};

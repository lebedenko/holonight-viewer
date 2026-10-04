#pragma once

#include "frame_source.h"

#include <QImage>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

// Scripted FrameSource. Frame i is a 2x2 image whose pixel value is i, so tests identify frames by pixel.
struct FakeScript {
  std::vector<int> delaysMs;  // raw delay of each frame
  SequenceInfo info{.frame_count = 0, .loop_count = 0};
  // Frame index at which reads report damage (-1: never); reads past delaysMs report EndOfSequence.
  int damagedAt = -1;
  std::atomic<int> reads{0};
  std::atomic<int> rewinds{0};
  std::atomic<int> outstanding{0};  // live QImage buffers created by the fake
  std::atomic<int> peak{0};
  std::function<void()> onRead;  // runs at the start of every readFrame()
};

inline int frameId(const QImage& image) { return static_cast<int>(image.pixel(0, 0) & 0xffU); }

struct FakeBuffer {
  std::array<uchar, 16> pixels;
  FakeScript* script;
};

inline QImage fakeImage(FakeScript& script, int frameIndex) {
  auto* owner = new FakeBuffer{.pixels = {}, .script = &script};
  const int now = ++script.outstanding;
  int peak = script.peak.load();
  while (now > peak && !script.peak.compare_exchange_weak(peak, now)) {
  }
  QImage image(
      owner->pixels.data(), 2, 2, 8, QImage::Format_ARGB32_Premultiplied,
      [](void* info) {
        auto* released = static_cast<FakeBuffer*>(info);
        --released->script->outstanding;
        delete released;
      },
      owner);
  image.fill(0xff000000U | static_cast<quint32>(frameIndex));
  return image;
}

class FakeFrameSource final : public FrameSource {
 public:
  explicit FakeFrameSource(FakeScript& script) : script_(script) {}
  bool open(const QString& /*path*/) override {
    position_ = 0;
    return true;
  }
  FrameResult readFrame() override {
    ++script_.reads;
    if (script_.onRead) {
      script_.onRead();
    }
    if (position_ == script_.damagedAt) {
      return {.status = FrameResult::Status::Damaged};
    }
    if (std::cmp_greater_equal(position_, script_.delaysMs.size())) {
      return {.status = FrameResult::Status::EndOfSequence};
    }
    const int frameIndex = position_++;
    return {.status = FrameResult::Status::Ok,
            .image = fakeImage(script_, frameIndex),
            .delay_ms = script_.delaysMs[static_cast<size_t>(frameIndex)]};
  }
  bool rewind() override {
    ++script_.rewinds;
    position_ = 0;
    return true;
  }
  SequenceInfo scan() override { return script_.info; }
  void close() override {}

 private:
  FakeScript& script_;
  int position_ = 0;
};

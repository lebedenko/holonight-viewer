#pragma once

#include "frame_source.h"

#include <QImage>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// Scripted FrameSource. Frame i is a 2x2 image whose pixel value is i, so tests identify frames by pixel.
struct FakeScript {
  std::vector<int> delaysMs;  // raw delay of each frame
  SequenceInfo info{.frameCount = 0, .loopCount = 0};
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
  uchar pixels[16];
  FakeScript* script;
};

inline QImage fakeImage(FakeScript& script, int id) {
  auto* owner = new FakeBuffer{.pixels = {}, .script = &script};
  auto* buffer = owner->pixels;
  for (int i = 0; i < 4; ++i) {
    reinterpret_cast<quint32*>(buffer)[i] = 0xff000000U | static_cast<quint32>(id);
  }
  const int now = ++script.outstanding;
  int peak = script.peak.load();
  while (now > peak && !script.peak.compare_exchange_weak(peak, now)) {
  }
  return {buffer,
          2,
          2,
          8,
          QImage::Format_ARGB32_Premultiplied,
          [](void* info) {
            auto* released = static_cast<FakeBuffer*>(info);
            --released->script->outstanding;
            delete released;
          },
          owner};
}

class FakeFrameSource final : public FrameSource {
 public:
  explicit FakeFrameSource(FakeScript& script) : script_(script) {}
  bool open(const QString&) override {
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
    if (position_ >= static_cast<int>(script_.delaysMs.size())) {
      return {.status = FrameResult::Status::EndOfSequence};
    }
    const int id = position_++;
    return {.status = FrameResult::Status::Ok,
            .image = fakeImage(script_, id),
            .delayMs = script_.delaysMs[static_cast<size_t>(id)]};
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

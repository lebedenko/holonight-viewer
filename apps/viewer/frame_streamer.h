#pragma once

#include "frame_source.h"

#include <QImage>
#include <QSize>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

struct StreamEvent {
  enum class Kind : std::uint8_t { Opened, Frame, End, Failed, Info };
  Kind kind = Kind::Failed;
  quint64 generation = 0;
  bool ok = false;    // Opened only: the file is readable and frame 0 matches the displayed size
  QImage image = {};  // Frame
  int delayMs = 0;    // Opened (frame 0) and Frame: raw delay
  SequenceInfo info = {};
};

// Executes commands against one FrameSource. Lives on the animation thread; every command and result carries the
// generation it belongs to, and work for a stale generation is dropped without being delivered.
class FrameStreamer {
 public:
  using Deliver = std::function<void(StreamEvent)>;
  FrameStreamer(FrameSourceFactory factory, std::shared_ptr<std::atomic<quint64>> current, Deliver deliver);
  // Opens the file, reads frame 0 (checked against firstFrameSize), then frame 1 and the sequence info.
  void open(quint64 generation, const QString& path, QSize firstFrameSize);
  void readNext(quint64 generation);
  // Restarts at frame 0 and reads it.
  void rewind(quint64 generation);
  void close(quint64 generation);

 private:
  [[nodiscard]] bool stale(quint64 generation) const { return current_->load() != generation; }
  void readOne(quint64 generation);
  void post(StreamEvent event);
  FrameSourceFactory factory_;
  std::shared_ptr<std::atomic<quint64>> current_;
  Deliver deliver_;
  std::unique_ptr<FrameSource> source_;
};

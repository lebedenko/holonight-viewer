#include "frame_streamer.h"

#include <utility>

FrameStreamer::FrameStreamer(FrameSourceFactory factory, std::shared_ptr<std::atomic<quint64>> current, Deliver deliver)
    : factory_(std::move(factory)), current_(std::move(current)), deliver_(std::move(deliver)) {}

void FrameStreamer::post(StreamEvent event) { deliver_(std::move(event)); }

void FrameStreamer::open(quint64 generation, const QString& path, QSize firstFrameSize) {
  source_.reset();
  if (stale(generation)) {
    return;
  }
  source_ = factory_();
  bool opened = source_->open(path);
  FrameResult first;
  if (opened) {
    first = source_->readFrame();
    opened = first.status == FrameResult::Status::Ok && first.image.size() == firstFrameSize;
  }
  if (stale(generation)) {
    return;
  }
  post({.kind = StreamEvent::Kind::Opened, .generation = generation, .ok = opened, .delayMs = first.delayMs});
  if (!opened) {
    source_.reset();
    return;
  }
  readOne(generation);
  if (stale(generation) || !source_) {
    return;
  }
  post({.kind = StreamEvent::Kind::Info, .generation = generation, .info = source_->scan()});
}

void FrameStreamer::readNext(quint64 generation) {
  if (source_ && !stale(generation)) {
    readOne(generation);
  }
}

void FrameStreamer::rewind(quint64 generation) {
  if (!source_ || stale(generation)) {
    return;
  }
  if (!source_->rewind()) {
    post({.kind = StreamEvent::Kind::Failed, .generation = generation});
    return;
  }
  readOne(generation);
}

void FrameStreamer::close(quint64 generation) {
  Q_UNUSED(generation)
  source_.reset();
}

void FrameStreamer::readOne(quint64 generation) {
  auto frame = source_->readFrame();
  if (stale(generation)) {
    return;
  }
  switch (frame.status) {
    case FrameResult::Status::Ok:
      post({.kind = StreamEvent::Kind::Frame,
            .generation = generation,
            .image = std::move(frame.image),
            .delayMs = frame.delayMs});
      break;
    case FrameResult::Status::EndOfSequence:
      post({.kind = StreamEvent::Kind::End, .generation = generation});
      break;
    case FrameResult::Status::Damaged:
      post({.kind = StreamEvent::Kind::Failed, .generation = generation});
      break;
  }
}

#include "animation_controller.h"

#include <QMetaObject>

#include <algorithm>

AnimationController::AnimationController(QObject* parent)
    : AnimationController(std::make_unique<QtPlaybackClock>(), makeQtGifFrameSource, Execution::Threaded, parent) {}

AnimationController::AnimationController(std::unique_ptr<PlaybackClock> clock, FrameSourceFactory factory,
                                         Execution execution, QObject* parent)
    : QObject(parent), clock_(std::move(clock)), execution_(execution) {
  clock_->setWakeHandler([this] { tryAdvance(); });
  streamer_ = std::make_unique<FrameStreamer>(std::move(factory), generation_, [this](StreamEvent event) {
    if (execution_ == Execution::Inline) {
      handle(std::move(event));
      return;
    }
    QMetaObject::invokeMethod(
        this, [this, event = std::move(event)]() mutable { handle(std::move(event)); }, Qt::QueuedConnection);
  });
  if (execution_ == Execution::Threaded) {
    context_ = new QObject;
    context_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, context_, &QObject::deleteLater);
  }
}

AnimationController::~AnimationController() {
  generation_->fetch_add(1);
  thread_.quit();
  thread_.wait();
  // A thread that never ran never fired the deleteLater connection.
  delete context_.data();
}

void AnimationController::command(std::function<void(FrameStreamer&, quint64)> action) {
  const auto generation = generation_->load();
  if (execution_ == Execution::Inline) {
    action(*streamer_, generation);
    return;
  }
  if (!thread_.isRunning()) {
    thread_.start();
  }
  QMetaObject::invokeMethod(
      context_, [streamer = streamer_.get(), action = std::move(action), generation] { action(*streamer, generation); },
      Qt::QueuedConnection);
}

void AnimationController::resetSession() {
  clock_->disarm();
  const bool changed = active_ || animated_ || failed_ || finished_ || user_paused_ || !notice_.isEmpty() ||
                       frame_count_ != 0 || frame_index_ != 0;
  active_ = false;
  animated_ = false;
  failed_ = false;
  finished_ = false;
  user_paused_ = false;
  running_ = false;
  end_pending_ = false;
  loop_ = {};
  frame_count_ = 0;
  frame_index_ = 0;
  read_index_ = 1;
  plays_done_ = 1;
  remaining_ = 0;
  next_.reset();
  notice_.clear();
  if (changed) {
    emit stateChanged();
  }
}

void AnimationController::start(const QString& path, QSize firstFrameSize) {
  if (shut_down_) {
    return;
  }
  generation_->fetch_add(1);
  resetSession();
  active_ = true;
  start_ms_ = clock_->nowMs();
  command([path, firstFrameSize](FrameStreamer& streamer, quint64 generation) {
    streamer.open(generation, path, firstFrameSize);
  });
}

void AnimationController::stop() {
  generation_->fetch_add(1);
  const bool was_active = active_;
  resetSession();
  if (was_active) {
    command([](FrameStreamer& streamer, quint64 generation) { streamer.close(generation); });
  }
}

void AnimationController::shutdown() {
  if (shut_down_) {
    return;
  }
  shut_down_ = true;
  generation_->fetch_add(1);
  resetSession();
  if (execution_ == Execution::Inline || !thread_.isRunning()) {
    QMetaObject::invokeMethod(this, &AnimationController::shutdownFinished, Qt::QueuedConnection);
    return;
  }
  connect(&thread_, &QThread::finished, this, &AnimationController::shutdownFinished, Qt::QueuedConnection);
  thread_.quit();
}

void AnimationController::setSuspendedByModal(bool suspended) {
  if (suspended_by_modal_ == suspended) {
    return;
  }
  suspended_by_modal_ = suspended;
  recompute();
  emit stateChanged();
}

void AnimationController::setSuspendedByWindow(bool suspended) {
  if (suspended_by_window_ == suspended) {
    return;
  }
  suspended_by_window_ = suspended;
  recompute();
  emit stateChanged();
}

void AnimationController::toggle() {
  if (!canToggle()) {
    return;
  }
  if (finished_) {
    // Replay from frame 0; the last frame's delay has already elapsed.
    finished_ = false;
    end_pending_ = false;
    user_paused_ = false;
    plays_done_ = 1;
    read_index_ = 0;
    remaining_ = 0;
    next_.reset();
    command([](FrameStreamer& streamer, quint64 generation) { streamer.rewind(generation); });
  } else {
    user_paused_ = !user_paused_;
  }
  recompute();
  emit stateChanged();
}

void AnimationController::arm() { clock_->arm(std::max<qint64>(0, deadline_ - clock_->nowMs())); }

void AnimationController::recompute() {
  const bool run = shouldRun();
  if (run == running_) {
    return;
  }
  running_ = run;
  if (run) {
    deadline_ = clock_->nowMs() + remaining_;
    arm();
    tryAdvance();
  } else {
    remaining_ = std::max<qint64>(0, deadline_ - clock_->nowMs());
    clock_->disarm();
  }
}

void AnimationController::fail() {
  failed_ = true;
  notice_ = tr("Playback stopped: damaged frame");
  next_.reset();
  running_ = false;
  clock_->disarm();
  emit failureNoticeChanged(notice_);
}

void AnimationController::handle(StreamEvent event) {
  if (event.generation != generation_->load() || !active_) {
    return;
  }
  using Kind = StreamEvent::Kind;
  switch (event.kind) {
    case Kind::Opened:
      if (!event.ok) {
        active_ = false;  // not readable as a sequence: remains an ordinary still image
        return;
      }
      current_delay_ms_ = clampFrameDelay(event.delayMs);
      prime_deadline_ = start_ms_ + current_delay_ms_;
      return;
    case Kind::Info:
      loop_ = LoopPolicy::fromQt(event.info.loopCount);
      if (event.info.frameCount > 1 && frame_count_ == 0) {
        frame_count_ = event.info.frameCount;
        emit stateChanged();
      }
      return;
    case Kind::Frame: {
      next_ = Pending{.image = std::move(event.image),
                      .delayMs = clampFrameDelay(event.delayMs),
                      .index = read_index_++,
                      .readyAt = clock_->nowMs()};
      if (!animated_) {
        animated_ = true;
        remaining_ = std::max<qint64>(0, prime_deadline_ - clock_->nowMs());
        recompute();
        emit stateChanged();
      }
      tryAdvance();
      return;
    }
    case Kind::End: {
      // The displayed frame is the last of the sequence.
      if (frame_count_ == 0 && animated_) {
        frame_count_ = frame_index_ + 1;
      }
      if (!animated_) {
        // Single-frame file: it stays an ordinary still image and the reader is released.
        active_ = false;
        command([](FrameStreamer& streamer, quint64 generation) { streamer.close(generation); });
        return;
      }
      if (loop_.morePlays(plays_done_)) {
        ++plays_done_;
        read_index_ = 0;
        command([](FrameStreamer& streamer, quint64 generation) { streamer.rewind(generation); });
      } else {
        end_pending_ = true;
      }
      emit stateChanged();
      tryAdvance();
      return;
    }
    case Kind::Failed:
      fail();
      emit stateChanged();
      return;
  }
}

void AnimationController::tryAdvance() {
  if (advancing_) {
    advance_again_ = true;
    return;
  }
  advancing_ = true;
  advance_again_ = true;
  while (advance_again_) {
    advance_again_ = false;
    step();
  }
  advancing_ = false;
}

void AnimationController::step() {
  if (!running_ || clock_->nowMs() < deadline_) {
    return;
  }
  const auto now = clock_->nowMs();
  if (next_) {
    auto frame = std::move(*next_);
    next_.reset();
    auto next_deadline = deadline_ + frame.delayMs;
    if (frame.readyAt > deadline_ || next_deadline < now) {
      next_deadline = now + frame.delayMs;
    }
    deadline_ = next_deadline;
    frame_index_ = frame.index;
    current_delay_ms_ = frame.delayMs;
    arm();
    emit frameReady(frame.image);
    emit stateChanged();
    command([](FrameStreamer& streamer, quint64 generation) { streamer.readNext(generation); });
    return;
  }
  if (end_pending_) {
    finished_ = true;
    end_pending_ = false;
    running_ = false;
    clock_->disarm();
    emit stateChanged();
  }
}

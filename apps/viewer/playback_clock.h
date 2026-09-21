#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QtTypes>

#include <functional>
#include <utility>

// Time source and single-shot wake-up for playback; the seam that makes timing testable.
class PlaybackClock {
 public:
  PlaybackClock() = default;
  PlaybackClock(const PlaybackClock&) = delete;
  PlaybackClock& operator=(const PlaybackClock&) = delete;
  PlaybackClock(PlaybackClock&&) = delete;
  PlaybackClock& operator=(PlaybackClock&&) = delete;
  virtual ~PlaybackClock() = default;
  [[nodiscard]] virtual qint64 nowMs() const = 0;
  // Replaces any armed wake-up with one after delayMs.
  virtual void arm(qint64 delayMs) = 0;
  virtual void disarm() = 0;
  void setWakeHandler(std::function<void()> handler) { wake_ = std::move(handler); }

 protected:
  void wake() const {
    if (wake_) {
      wake_();
    }
  }

 private:
  std::function<void()> wake_;
};

class QtPlaybackClock final : public PlaybackClock {
 public:
  QtPlaybackClock() {
    elapsed_.start();
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer_, &QTimer::timeout, &timer_, [this] { wake(); });
  }
  [[nodiscard]] qint64 nowMs() const override { return elapsed_.elapsed(); }
  void arm(qint64 delayMs) override { timer_.start(static_cast<int>(delayMs < 0 ? 0 : delayMs)); }
  void disarm() override { timer_.stop(); }

 private:
  QElapsedTimer elapsed_;
  QTimer timer_;
};

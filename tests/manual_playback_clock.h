#pragma once

#include "playback_clock.h"

#include <optional>

// Deterministic clock: time only moves through advance(), which fires each armed wake-up when it is reached.
class ManualPlaybackClock final : public PlaybackClock {
 public:
  [[nodiscard]] qint64 nowMs() const override { return now_; }
  void arm(qint64 delayMs) override { deadline_ = now_ + (delayMs < 0 ? 0 : delayMs); }
  void disarm() override { deadline_.reset(); }
  [[nodiscard]] bool armed() const { return deadline_.has_value(); }
  void advance(qint64 ms) {
    const auto target = now_ + ms;
    while (deadline_ && *deadline_ <= target) {
      now_ = *deadline_;
      deadline_.reset();
      wake();
    }
    now_ = target;
  }

 private:
  qint64 now_ = 0;
  std::optional<qint64> deadline_;
};

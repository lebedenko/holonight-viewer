#pragma once

#include "frame_source.h"
#include "frame_streamer.h"
#include "playback_clock.h"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

// Playback state machine for one animated document. All state lives on the GUI thread; frames are decoded one at a
// time on a dedicated animation thread and paired with a single look-ahead frame, so memory never depends on the
// frame count. Every session carries a generation token; results from an older session are dropped.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class AnimationController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Owned by ImageDocument")
  Q_PROPERTY(bool animated READ animated NOTIFY stateChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
  Q_PROPERTY(bool userPaused READ userPaused NOTIFY stateChanged)
  Q_PROPERTY(bool canToggle READ canToggle NOTIFY stateChanged)
  Q_PROPERTY(int frameCount READ frameCount NOTIFY stateChanged)
  Q_PROPERTY(int frameIndex READ frameIndex NOTIFY stateChanged)
  Q_PROPERTY(QString failureNotice READ failureNotice NOTIFY stateChanged)
  Q_PROPERTY(bool suspendedByModal READ suspendedByModal WRITE setSuspendedByModal NOTIFY stateChanged)
  Q_PROPERTY(bool suspendedByWindow READ suspendedByWindow WRITE setSuspendedByWindow NOTIFY stateChanged)

 public:
  enum class Execution : std::uint8_t { Threaded, Inline };
  explicit AnimationController(QObject* parent = nullptr);
  AnimationController(std::unique_ptr<PlaybackClock> clock, FrameSourceFactory factory, Execution execution,
                      QObject* parent = nullptr);
  ~AnimationController() override;

  // True once a second frame has been decoded, i.e. the file really is multi-frame.
  [[nodiscard]] bool animated() const { return animated_; }
  // Effective state: false while user-paused, suspended by a modal or the window, finished or failed.
  [[nodiscard]] bool playing() const { return shouldRun(); }
  [[nodiscard]] bool userPaused() const { return user_paused_; }
  [[nodiscard]] bool canToggle() const { return animated_ && !failed_; }
  // 0 while unknown.
  [[nodiscard]] int frameCount() const { return frame_count_; }
  [[nodiscard]] int frameIndex() const { return frame_index_; }
  [[nodiscard]] QString failureNotice() const { return notice_; }
  [[nodiscard]] bool suspendedByModal() const { return suspended_by_modal_; }
  [[nodiscard]] bool suspendedByWindow() const { return suspended_by_window_; }
  void setSuspendedByModal(bool suspended);
  void setSuspendedByWindow(bool suspended);

  Q_INVOKABLE void toggle();
  // Begins a session for a file whose frame 0 (of the given size) is already displayed.
  void start(const QString& path, QSize firstFrameSize);
  void stop();
  void shutdown();

 signals:
  void stateChanged();
  void frameReady(QImage frame);
  void failureNoticeChanged(QString notice);
  void shutdownFinished();

 private:
  struct Pending {
    QImage image;
    int delayMs;
    int index;
    qint64 readyAt;
  };
  [[nodiscard]] bool shouldRun() const {
    return animated_ && !failed_ && !finished_ && !user_paused_ && !suspended_by_modal_ && !suspended_by_window_;
  }
  void command(std::function<void(FrameStreamer&, quint64)> action);
  void handle(StreamEvent event);
  void recompute();
  void step();
  void tryAdvance();
  void resetSession();
  void fail();
  void arm();
  std::unique_ptr<PlaybackClock> clock_;
  Execution execution_;
  std::shared_ptr<std::atomic<quint64>> generation_ = std::make_shared<std::atomic<quint64>>(0);
  std::unique_ptr<FrameStreamer> streamer_;
  QThread thread_;
  QPointer<QObject> context_;
  bool shut_down_ = false;

  bool active_ = false;
  bool animated_ = false;
  bool failed_ = false;
  bool finished_ = false;
  bool user_paused_ = false;
  bool suspended_by_modal_ = false;
  bool suspended_by_window_ = false;
  bool running_ = false;
  bool end_pending_ = false;  // the source ended after the displayed frame and no more plays remain
  int frame_count_ = 0;
  int frame_index_ = 0;
  int read_index_ = 1;  // index the next frame delivered by the source will have
  int plays_done_ = 1;
  int current_delay_ms_ = 100;
  LoopPolicy loop_;
  qint64 start_ms_ = 0;
  qint64 prime_deadline_ = 0;
  qint64 deadline_ = 0;
  qint64 remaining_ = 0;
  std::optional<Pending> next_;
  QString notice_;
  bool advancing_ = false;
  bool advance_again_ = false;
};

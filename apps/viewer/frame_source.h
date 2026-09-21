#pragma once

#include <QImage>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

struct FrameResult {
  enum class Status : std::uint8_t { Ok, EndOfSequence, Damaged };
  Status status = Status::Damaged;
  QImage image = {};  // ARGB32_Premultiplied, DPR 1, fresh per frame and within the viewing limits
  int delayMs = 0;    // raw QImageReader::nextImageDelay() read right after the frame; not clamped
};

// Raw Qt values; a frameCount <= 0 means unknown. loopCount follows Qt: -1 infinite, N repeats after the first play.
struct SequenceInfo {
  int frameCount = 0;
  int loopCount = 0;
};

// Sequential access to the frames of one file. Used from a single thread; one reader per source.
class FrameSource {
 public:
  FrameSource() = default;
  FrameSource(const FrameSource&) = delete;
  FrameSource& operator=(const FrameSource&) = delete;
  FrameSource(FrameSource&&) = delete;
  FrameSource& operator=(FrameSource&&) = delete;
  virtual ~FrameSource() = default;
  virtual bool open(const QString& path) = 0;
  virtual FrameResult readFrame() = 0;
  // Restarts at frame 0. Qt's GIF handler refuses jumpToImage(0), so implementations reopen the same file.
  virtual bool rewind() = 0;
  virtual SequenceInfo scan() = 0;
  virtual void close() = 0;
};

using FrameSourceFactory = std::function<std::unique_ptr<FrameSource>()>;
std::unique_ptr<FrameSource> makeQtGifFrameSource();

// Browsers treat delays of 10 ms or less as 100 ms.
constexpr int clampFrameDelay(int rawMs) { return rawMs <= 10 ? 100 : rawMs; }

struct LoopPolicy {
  static LoopPolicy fromQt(int rawLoopCount) {
    if (rawLoopCount < 0) {
      return {.infinite = true, .totalPlays = 1};
    }
    return {.infinite = false, .totalPlays = rawLoopCount + 1};
  }
  bool infinite = false;
  int totalPlays = 1;
  [[nodiscard]] bool morePlays(int playsDone) const { return infinite || playsDone < totalPlays; }
};

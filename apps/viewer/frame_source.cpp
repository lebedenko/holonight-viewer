#include "frame_source.h"

#include "image_limits.h"

#include <QFile>
#include <QImageReader>

namespace {
class QtGifFrameSource final : public FrameSource {
 public:
  bool open(const QString& path) override {
    close();
    file_.setFileName(path);
    if (!file_.open(QIODevice::ReadOnly) || file_.size() > kFileLimitBytes) {
      file_.close();
      return false;
    }
    return startReader();
  }
  FrameResult readFrame() override {
    if (!reader_) {
      return {};
    }
    auto image = reader_->read();
    const int delay = reader_->nextImageDelay();
    if (image.isNull()) {
      // A null read is a clean end only when the file declared no further frames.
      if (info_.frame_count <= 0) {
        info_.frame_count = reader_->imageCount();
      }
      const bool ended = info_.frame_count > 0 && frames_read_ >= info_.frame_count;
      return {.status = ended ? FrameResult::Status::EndOfSequence : FrameResult::Status::Damaged};
    }
    image.convertTo(QImage::Format_ARGB32_Premultiplied);
    if (!acceptableImage(image)) {
      return {};
    }
    image.setDevicePixelRatio(1);
    ++frames_read_;
    return {.status = FrameResult::Status::Ok, .image = std::move(image), .delay_ms = delay};
  }
  bool rewind() override {
    if (!file_.isOpen() || !file_.seek(0)) {
      return false;
    }
    return startReader();
  }
  SequenceInfo scan() override {
    if (!reader_) {
      return {};
    }
    info_ = {.frame_count = reader_->imageCount(), .loop_count = reader_->loopCount()};
    return info_;
  }
  void close() override {
    reader_.reset();
    file_.close();
    frames_read_ = 0;
    info_ = {};
  }

 private:
  bool startReader() {
    reader_.reset();
    frames_read_ = 0;
    reader_ = std::make_unique<QImageReader>(&file_);
    if (!reader_->canRead()) {
      reader_.reset();
      return false;
    }
    return true;
  }
  QFile file_;
  std::unique_ptr<QImageReader> reader_;
  int frames_read_ = 0;
  SequenceInfo info_;
};
}  // namespace

std::unique_ptr<FrameSource> makeQtGifFrameSource() { return std::make_unique<QtGifFrameSource>(); }

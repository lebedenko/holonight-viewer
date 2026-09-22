#pragma once

#include "animation_controller.h"
#include "clipboard_controller.h"
#include "decoded_image_cache.h"
#include "directory_model.h"
#include "image_orientation.h"

#include <QDir>
#include <QImage>
#include <QObject>
#include <QSvgRenderer>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

struct DecodeResult {
  QImage image;
  QString error;
  ImageInformation information;
  QByteArray svgData;  // Non-empty only for a successfully validated SVG.
};

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled);
QUrl commandLineUrl(const QString& argument);
// viewBox is authoritative when present; otherwise Qt's own width/height-or-300x150 resolution.
QSize svgIntrinsicSize(const QSvgRenderer& renderer);

// Exposes QSvgRenderer as a property type to QML/qmllint without making it constructible there.
struct QSvgRendererForeign {
  Q_GADGET
  QML_FOREIGN(QSvgRenderer)
  QML_ANONYMOUS
};

// Image Information presentation helpers; pure so tests need no decoded image.
// Replaces a leading home directory with "~"; a sibling such as "/home/alice2" is left unchanged.
QString abbreviateHomePath(const QString& absolutePath, const QString& home = QDir::homePath());
// "JPEG · 3072 × 4080 · 12.5 MP · 2.5 MB" without missing parts, or "Details unavailable" when all are missing.
QString formatSummaryLine(const QString& format, QSize decodedSize, qint64 encodedSize);
// "Rotated view W × H" only when the transform swaps the decoded width and height.
QString formatTransformedLine(QSize decodedSize, QSize transformedSize);
// Locale short date and time, or empty for an unknown time.
QString formatModifiedText(const QDateTime& modified);
QString joinNonEmpty(const QStringList& parts, QStringView separator = u" · ");

// QObject owns identity and disables copying/moving.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class ImageDocument : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Created by the application")
  Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged)
  Q_PROPERTY(QSize transformedDimensions READ transformedDimensions NOTIFY changed)
  Q_PROPERTY(QString localPath READ localPath NOTIFY changed)
  Q_PROPERTY(QString formattedFileSize READ formattedFileSize NOTIFY changed)
  Q_PROPERTY(QString summaryLine READ summaryLine NOTIFY changed)
  Q_PROPERTY(QString transformedLine READ transformedLine NOTIFY changed)
  Q_PROPERTY(QString modifiedText READ modifiedText NOTIFY changed)
  Q_PROPERTY(QString displayPath READ displayPath NOTIFY changed)
  Q_PROPERTY(QVariantList informationSections READ informationSections NOTIFY changed)
  Q_PROPERTY(ClipboardController* clipboard READ clipboard CONSTANT)
  Q_PROPERTY(AnimationController* animation READ animation CONSTANT)
  Q_PROPERTY(State state READ state NOTIFY changed)
  Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QImage image READ image NOTIFY changed)
  Q_PROPERTY(QSvgRenderer* svgRenderer READ svgRenderer NOTIFY changed)
  Q_PROPERTY(QImage previewImage READ previewImage NOTIFY changed)
  Q_PROPERTY(int position READ position NOTIFY changed)
  Q_PROPERTY(int count READ count NOTIFY changed)
  Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
  Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY changed)
  Q_PROPERTY(bool canNext READ canNext NOTIFY changed)
  Q_PROPERTY(QString folderError READ folderError NOTIFY changed)
  Q_PROPERTY(QStringList nameFilters READ nameFilters CONSTANT)

 public:
  // Preserve the existing Qt-facing enum type and values.
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size)
  enum State { Empty, Loading, Ready, Error };
  Q_ENUM(State)
  using Decoder = std::function<DecodeResult(const QUrl&, const std::atomic_bool&)>;
  explicit ImageDocument(QObject* parent = nullptr);
  explicit ImageDocument(Decoder decoder, QObject* parent = nullptr);
  ImageDocument(Decoder decoder, DirectoryModel::Scanner scanner, QObject* parent = nullptr);
  ~ImageDocument() override;
  [[nodiscard]] int orientation() const { return orientation_; }
  [[nodiscard]] QSize transformedDimensions() const {
    return ImageOrientation::dimensions(orientation_, information_.decodedSize);
  }
  [[nodiscard]] QString localPath() const { return selected_url_.toLocalFile(); }
  [[nodiscard]] QString summaryLine() const {
    return formatSummaryLine(information_.format, information_.decodedSize, information_.encodedSize);
  }
  [[nodiscard]] QString transformedLine() const {
    return formatTransformedLine(information_.decodedSize, transformedDimensions());
  }
  [[nodiscard]] QString modifiedText() const { return formatModifiedText(information_.modified); }
  [[nodiscard]] QString displayPath() const { return abbreviateHomePath(localPath()); }
  // Ordered {key, label, lines} maps for Camera, Location and File; sections without lines are omitted.
  [[nodiscard]] QVariantList informationSections() const;
  [[nodiscard]] QString formattedFileSize() const;
  [[nodiscard]] const ImageInformation& information() const { return information_; }
  ClipboardController* clipboard() { return &clipboard_; }
  AnimationController* animation() { return &animation_; }
  Q_INVOKABLE void transform(int operation);
  Q_INVOKABLE void resetTransform();
  Q_INVOKABLE void copyImage();
  Q_INVOKABLE void copyPath();
  [[nodiscard]] State state() const { return state_; }
  [[nodiscard]] QString fileName() const { return file_name_; }
  [[nodiscard]] QString error() const { return error_; }
  [[nodiscard]] QImage image() const { return image_; }
  // Non-null only once a document has finished loading as SVG; the discriminator ImageCanvas paints against.
  [[nodiscard]] QSvgRenderer* svgRenderer() {
    return state_ == Ready && information_.format == QLatin1String("SVG") ? &svg_renderer_ : nullptr;
  }
  // Always a raster QImage: the decoded image for raster formats, or an on-demand bounded rasterization for SVG.
  [[nodiscard]] QImage previewImage();
  [[nodiscard]] int position() const { return selected_index_ + 1; }
  [[nodiscard]] int count() const { return directory_.rowCount(); }
  [[nodiscard]] bool scanning() const { return directory_.scanning(); }
  [[nodiscard]] bool canPrevious() const { return !stopping_ && !scanning() && selected_index_ > 0; }
  [[nodiscard]] bool canNext() const {
    return !stopping_ && !scanning() && selected_index_ >= 0 && selected_index_ + 1 < count();
  }
  [[nodiscard]] QString folderError() const { return directory_.error(); }
  Q_INVOKABLE void previous();
  Q_INVOKABLE void next();
  Q_INVOKABLE void refresh();
  static QStringList nameFilters();
  static bool isLocalUrl(const QUrl& url);
  Q_INVOKABLE void open(const QList<QUrl>& urls);
  Q_INVOKABLE void shutdown();

 signals:
  void openingFailed(QString fileName, QString error);
  void changed();
  // A new animation frame is now image(); changed() is not emitted.
  void frameChanged();
  void orientationChanged();
  void shutdownFinished();

 private:
  struct Request {
    quint64 requestId;
    QUrl url;
    quint64 cacheEpoch;
    bool prefetch = false;
  };
  void startPending();
  void complete(const Request& request, DecodeResult result);
  void select(const QUrl& url);
  void navigate(int direction);
  void maybePrefetch();
  void workerFinished();
  ClipboardController clipboard_;
  AnimationController animation_;
  int orientation_ = 0;
  ImageInformation information_;
  DirectoryModel directory_;
  QUrl selected_url_;
  int selected_index_ = -1;
  int direction_ = 1;
  quint64 prefetched_selection_ = 0;
  quint64 cache_epoch_ = 0;
  int finished_workers_ = 0;
  // Accessed exclusively by the decode worker.
  quint64 worker_cache_epoch_ = 0;
  DecodedImageCache cache_;
  std::optional<CachedImage> displayed_;
  QThread thread_;
  QObject* worker_;
  Decoder decoder_;
  std::shared_ptr<std::atomic_bool> cancellation_;
  std::optional<Request> pending_;
  quint64 request_id_ = 0;
  bool busy_ = false;
  bool stopping_ = false;
  State state_ = Empty;
  QString file_name_;
  QString error_;
  QImage image_;
  QByteArray svg_data_;
  QSvgRenderer svg_renderer_;
};

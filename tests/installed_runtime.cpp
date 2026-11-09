#include "image_canvas.h"
#include "image_document.h"

#include <QGuiApplication>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>

#include <HolonightFileBrowser/preview/image_limits.h>
#include <HolonightFileBrowser/preview_quick/thumbnail_provider.h>
#include <algorithm>
#include <array>

namespace {
bool requiredDecodersAvailable() {
  const std::array required{"png", "jpeg", "bmp", "webp", "gif", "tif", "tiff"};
  return std::ranges::all_of(required, [](const char* format) {
    if (!QImageReader::supportedImageFormats().contains(format)) {
      QTextStream(stderr) << "Missing required decoder: " << format << '\n';
      return false;
    }
    return true;
  });
}
// The GIF fixtures must be readable as sequences with the delays and loop counts they were generated with.
bool gifFixturesReadable(const QString& directory) {
  struct Expected {
    const char* name;
    std::array<int, 2> delays;
    int loopCount;
  };
  for (const auto& [name, delays, loopCount] :
       {Expected{.name = "sample.gif", .delays = {100, 200}, .loopCount = -1},
        Expected{.name = "gif87a.gif", .delays = {100, 100}, .loopCount = 0},
        Expected{.name = "transparent.gif", .delays = {100, 200}, .loopCount = 2}}) {
    QImageReader reader(directory + "/" + name);
    if (!reader.canRead() || reader.imageCount() != 2 || reader.loopCount() != loopCount) {
      QTextStream(stderr) << "Unreadable GIF sequence: " << name << '\n';
      return false;
    }
    for (const int delay : delays) {
      if (reader.read().isNull() || reader.nextImageDelay() != delay) {
        QTextStream(stderr) << "Unexpected GIF frame or delay: " << name << '\n';
        return false;
      }
    }
  }
  return true;
}
bool matchesFixture(const QImage& image, const QString& extension) {
  if (extension == "jpg") {
    return image.size() == QSize(20, 40);
  }
  const bool opaque = extension == "bmp" || extension == "gif";
  return image.size() == QSize(1, 1) && image.pixelColor(0, 0) == QColor(255, 0, 0, opaque ? 255 : 128);
}
bool renderedFixture(QQmlApplicationEngine& engine, bool first) {
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  auto* canvas = window != nullptr ? window->findChild<ImageCanvas*>(QStringLiteral("imageCanvas")) : nullptr;
  const auto rendered = window != nullptr ? window->grabWindow() : QImage{};
  if (canvas == nullptr || rendered.isNull() || canvas->image().isNull()) {
    QTextStream(stderr) << "Missing rendered Viewer output\n";
    return false;
  }
  if (first) {
    const auto center = canvas->mapToScene(canvas->imageRect().center()) * window->devicePixelRatio();
    const auto pixel = rendered.pixelColor(center.toPoint());
    if (pixel.red() < pixel.green() + 50) {
      QTextStream(stderr) << "PNG was not painted on the Viewer canvas\n";
      return false;
    }
  }

  return true;
}
int verifyFormats(ImageDocument& document, QQmlApplicationEngine& engine, const QString& fixtures,
                  QGuiApplication& app) {
  const QStringList extensions{"png", "jpg", "bmp", "webp", "gif", "tif"};
  int index = 0;
  QObject::connect(&document, &ImageDocument::changed, &app, [&] {
    // Directory and clipboard updates share this signal; consume each file once.
    if (index >= extensions.size() || document.fileName() != "sample." + extensions.at(index)) {
      return;
    }
    if (document.state() == ImageDocument::Error) {
      QTextStream(stderr) << document.error() << '\n';
      QCoreApplication::exit(1);
    } else if (document.state() == ImageDocument::Ready) {
      const auto& extension = extensions.at(index);
      const auto image = document.image();
      if (!matchesFixture(image, extension)) {
        QCoreApplication::exit(1);
        return;
      }
      QTextStream(stdout) << "Decoded " << extension << ": " << image.width() << 'x' << image.height() << '\n';
      ++index;
      QTimer::singleShot(100, &app, [&] {
        if (!renderedFixture(engine, index == 1)) {
          QCoreApplication::exit(1);
          return;
        }
        if (index == extensions.size()) {
          QCoreApplication::quit();
        } else {
          document.open({QUrl::fromLocalFile(fixtures + "/sample." + extensions.at(index))});
        }
      });
    }
  });
  QTimer::singleShot(100, &app, [&] { document.open({QUrl::fromLocalFile(fixtures + "/sample.png")}); });
  QTimer::singleShot(120000, &app, [&] { QCoreApplication::exit(1); });
  return QGuiApplication::exec();
}
}  // namespace

int main(int argc, char* argv[]) {
  configureDecodeLimits();
  QGuiApplication app(argc, argv);
  const auto args = QCoreApplication::arguments();
  const bool portable = args.size() == 2 && args.at(1) == "--portable";
  if (!portable && args.size() != 3) {
    return 2;
  }
  if (!requiredDecodersAvailable()) {
    return 1;
  }
  if (!portable && !gifFixturesReadable(args.at(1))) {
    return 1;
  }
  const HolonightFileBrowser::ImageRuntimeOptions runtime{portable ? QString{} : args.at(2)};
  ImageDocument document(runtime);
  QQmlApplicationEngine engine;
  engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider(runtime));
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  if (engine.rootObjects().isEmpty() || document.state() != ImageDocument::Empty) {
    return 1;
  }
  if (portable) {
    return 0;
  }
  return verifyFormats(document, engine, args.at(1), app);
}

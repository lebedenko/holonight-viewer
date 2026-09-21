#include "image_document.h"

#include <QGuiApplication>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QTextStream>
#include <QTimer>

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
}  // namespace

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  const auto args = QCoreApplication::arguments();
  if (args.size() != 2) {
    return 2;
  }
  if (!requiredDecodersAvailable()) {
    return 1;
  }
  if (!gifFixturesReadable(args.at(1))) {
    return 1;
  }
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  if (engine.rootObjects().isEmpty() || document.state() != ImageDocument::Empty) {
    return 1;
  }
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
      QTimer::singleShot(0, &app, [&] {
        if (index == extensions.size()) {
          QCoreApplication::quit();
        } else {
          document.open({QUrl::fromLocalFile(args.at(1) + "/sample." + extensions.at(index))});
        }
      });
    }
  });
  QTimer::singleShot(100, &app, [&] { document.open({QUrl::fromLocalFile(args.at(1) + "/sample.png")}); });
  QTimer::singleShot(10000, &app, [&] { QCoreApplication::exit(1); });
  return QGuiApplication::exec();
}

#include "image_document.h"

#include <QGuiApplication>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QTextStream>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include <algorithm>
#include <array>

Q_IMPORT_QML_PLUGIN(HolonightViewerPlugin)

namespace {
bool requiredDecodersAvailable() {
  const std::array required{"png", "jpeg", "bmp", "webp"};
  return std::ranges::all_of(required, [](const char* format) {
    if (!QImageReader::supportedImageFormats().contains(format)) {
      QTextStream(stderr) << "Missing required decoder: " << format << '\n';
      return false;
    }
    return true;
  });
}
bool matchesFixture(const QImage& image, const QString& extension) {
  if (extension == "jpg") {
    return image.size() == QSize(20, 40);
  }
  return image.size() == QSize(1, 1) && image.pixelColor(0, 0) == QColor(255, 0, 0, extension == "bmp" ? 255 : 128);
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
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  if (engine.rootObjects().isEmpty() || document.state() != ImageDocument::Empty) {
    return 1;
  }
  const QStringList extensions{"png", "jpg", "bmp", "webp"};
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

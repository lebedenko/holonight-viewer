#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QTimer>

// Separate receiver: exercise platform MIME transport, not in-process QMimeData.
int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  const auto args = QCoreApplication::arguments();
  if (args.size() != 3) {
    return 2;
  }
  QQuickWindow window;
  window.resize(160, 80);
  window.show();
  window.requestActivate();
  QTimer::singleShot(500, &app, [&] {
    auto* clipboard = QGuiApplication::clipboard();
    bool success = false;
    if (args.at(1) == "image") {
      const auto image = clipboard->image();
      success = !image.isNull() && image.save(args.at(2));
    } else {
      QFile file(args.at(2));
      const auto bytes = clipboard->text().toUtf8();
      success = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }
    QCoreApplication::exit(success ? 0 : 1);
  });
  return QGuiApplication::exec();
}
